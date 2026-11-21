/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/** @file @brief Native asynchronous libpulse playback with one provider-owned event loop. */
#include "../driver.h"
#include <embt/osi/thread.h>
#include <embt/osi/time.h>
#include <embt/base/atomic.h>
#include <pulse/pulseaudio.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

struct pulse_stream {
	struct bt_audio_stream *audio;
	pa_stream *native;
	pa_operation *operation;
	uint64_t deadline_ms;
	bool stopping;
};

static struct pulse_stream slots[CONFIG_EMBT_AUDIO_STREAMS];
static pa_mainloop *mainloop;
static pa_context *context;
static os_thread_t thread;
static bt_atomic_t quitting;
static const struct bt_audio_driver pulse_driver;

/** @brief Wake the event loop without locking libpulse or performing device I/O. */
static void pulse_wake(void)
{
	pa_mainloop_wakeup(mainloop);
}

/** @brief Publish terminal native stream errors. */
static void stream_state(pa_stream *stream, void *user)
{
	struct pulse_stream *slot = user;
	pa_stream_state_t state = pa_stream_get_state(stream);
	if (state == PA_STREAM_READY) {
		const char *device = pa_stream_get_device_name(stream);
		if (slot->audio->device[0] &&
		    (!device || strcmp(slot->audio->device, device))) {
			bt_audio_complete(slot->audio, BT_AUDIO_ERROR, -ENODEV);
		} else {
			bt_audio_complete(slot->audio, BT_AUDIO_READY, 0);
		}
	} else if (state == PA_STREAM_FAILED) {
		bt_audio_complete(slot->audio, BT_AUDIO_ERROR, -EIO);
	}
}

/** @brief The outer event-loop pass writes pending blocks after this notification. */
static void stream_writable(pa_stream *stream, size_t length, void *user)
{
	(void)stream;
	(void)length;
	(void)user;
}

/** @brief Complete asynchronous drain or flush before admitting a restart. */
static void stop_done(pa_stream *stream, int success, void *user)
{
	struct pulse_stream *slot = user;
	(void)stream;
	bt_audio_complete(slot->audio, success ? BT_AUDIO_STOPPED : BT_AUDIO_ERROR,
			  success ? 0 : -EIO);
}

/** @brief Release all native callbacks before making a slot reusable. */
static void native_close(struct pulse_stream *slot)
{
	if (slot->operation) {
		pa_operation_cancel(slot->operation);
		pa_operation_unref(slot->operation);
		slot->operation = NULL;
	}
	if (slot->native) {
		pa_stream_set_state_callback(slot->native, NULL, NULL);
		pa_stream_set_write_callback(slot->native, NULL, NULL);
		pa_stream_disconnect(slot->native);
		pa_stream_unref(slot->native);
		slot->native = NULL;
	}
}

/** @brief Connect lazily; registration alone never connects to an audio server. */
static int ensure_context(void)
{
	if (!context) {
		context = pa_context_new(pa_mainloop_get_api(mainloop), "EmBT");
		if (!context) {
			return -ENOMEM;
		}
		if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
			pa_context_unref(context);
			context = NULL;
			return -ECONNREFUSED;
		}
	}
	return 0;
}

/** @brief Process one stream's requested state and frame-aligned writable capacity. */
static void process_stream(size_t index, uint64_t now)
{
	struct bt_audio_stream *audio = bt_audio_slot(index);
	struct pulse_stream *slot = &slots[index];
	struct bt_audio_status status;
	bool owned, drain;
	int err;

	osi_mutex_lock(&audio->lock, OS_TIMEOUT_FOREVER);
	owned = audio->allocated && audio->driver == &pulse_driver;
	status = audio->status;
	drain = audio->drain;
	osi_mutex_unlock(&audio->lock);
	if (!owned) {
		return;
	}
	slot->audio = audio;
	if (slot->operation && pa_operation_get_state(slot->operation) != PA_OPERATION_RUNNING) {
		pa_operation_unref(slot->operation);
		slot->operation = NULL;
	}
	if (status.state == BT_AUDIO_CLOSING) {
		native_close(slot);
		slot->deadline_ms = 0;
		slot->stopping = false;
		bt_audio_complete(audio, BT_AUDIO_CLOSED, 0);
		return;
	}
	if (status.state == BT_AUDIO_ERROR || status.state == BT_AUDIO_STOPPED) {
		return;
	}
	if (status.state == BT_AUDIO_STOPPING) {
		if (!slot->stopping) {
			slot->stopping = true;
			slot->deadline_ms = now + 10000;
		}
		if (now >= slot->deadline_ms) {
			native_close(slot);
			bt_audio_complete(audio, BT_AUDIO_ERROR, -ETIMEDOUT);
			return;
		}
	} else {
		slot->stopping = false;
	}
	if (status.state == BT_AUDIO_OPENING) {
		if (!slot->deadline_ms) {
			slot->deadline_ms = now + 10000;
		}
		if (now >= slot->deadline_ms) {
			native_close(slot);
			bt_audio_complete(audio, BT_AUDIO_ERROR, -ETIMEDOUT);
			return;
		}
		err = ensure_context();
		if (err) {
			bt_audio_complete(audio, BT_AUDIO_ERROR, err);
			return;
		}
		if (pa_context_get_state(context) != PA_CONTEXT_READY) {
			return;
		}
		if (!slot->native) {
			pa_sample_spec spec = {PA_SAMPLE_S16NE, audio->format.rate,
					       audio->format.channels};
			pa_buffer_attr attr = {.maxlength = (uint32_t)-1,
				.tlength = audio->format.rate * audio->format.channels * 2U / 20U,
				.prebuf = (uint32_t)-1, .minreq = (uint32_t)-1,
				.fragsize = (uint32_t)-1};
			slot->native = pa_stream_new(context, "EmBT PCM", &spec, NULL);
			if (!slot->native) {
				bt_audio_complete(audio, BT_AUDIO_ERROR, -ENOMEM);
				return;
			}
			pa_stream_set_state_callback(slot->native, stream_state, slot);
			pa_stream_set_write_callback(slot->native, stream_writable, slot);
			if (pa_stream_connect_playback(slot->native,
					audio->device[0] ? audio->device : NULL, &attr,
					PA_STREAM_ADJUST_LATENCY |
					(audio->device[0] ? PA_STREAM_DONT_MOVE : 0), NULL, NULL) < 0) {
				native_close(slot);
				bt_audio_complete(audio, BT_AUDIO_ERROR, -ENODEV);
			}
		}
		return;
	}
	if (!slot->native || pa_stream_get_state(slot->native) != PA_STREAM_READY) {
		bt_audio_complete(audio, BT_AUDIO_ERROR, -EIO);
		return;
	}
	if (status.state == BT_AUDIO_READY || (status.state == BT_AUDIO_STOPPING && drain)) {
		int16_t pcm[CONFIG_EMBT_AUDIO_BLOCK_BYTES / sizeof(int16_t)];
		for (;;) {
			size_t available = pa_stream_writable_size(slot->native);
			size_t bytes;
			if (available == (size_t)-1) {
				bt_audio_complete(audio, BT_AUDIO_ERROR, -EIO);
				return;
			}
			bytes = bt_audio_peek(audio, pcm,
				available < sizeof(pcm) ? available : sizeof(pcm));
			if (!bytes) {
				break;
			}
			if (pa_stream_write(slot->native, pcm, bytes, NULL, 0, PA_SEEK_RELATIVE) < 0) {
				bt_audio_complete(audio, BT_AUDIO_ERROR, -EIO);
				return;
			}
			bt_audio_consume(audio, bytes);
		}
	}
	bt_audio_get_status(audio, &status);
	if (status.state == BT_AUDIO_STOPPING && !slot->operation &&
	    (!drain || !status.queued_frames)) {
		slot->operation = drain ? pa_stream_drain(slot->native, stop_done, slot) :
			pa_stream_flush(slot->native, stop_done, slot);
		if (!slot->operation) {
			bt_audio_complete(audio, BT_AUDIO_ERROR, -EIO);
		}
	}
}

/** @brief Keep finite opening/drain deadlines observable while the server is silent. */
static void deadline_event(pa_mainloop_api *api, pa_time_event *event,
			   const struct timeval *time, void *user)
{
	struct timeval next;
	(void)time;
	(void)user;
	gettimeofday(&next, NULL);
	next.tv_usec += 100000;
	if (next.tv_usec >= 1000000) {
		next.tv_sec++;
		next.tv_usec -= 1000000;
	}
	api->time_restart(event, &next);
}

/** @brief Own all context/stream calls and cancel outstanding work before shutdown. */
static void pulse_worker(void *user)
{
	pa_mainloop_api *api = pa_mainloop_get_api(mainloop);
	struct timeval time;
	pa_time_event *timer;
	(void)user;
	gettimeofday(&time, NULL);
	timer = api->time_new(api, &time, deadline_event, NULL);
	for (;;) {
		uint64_t now = 0;
		osi_time_get_ms(&now);
		if (context && (pa_context_get_state(context) == PA_CONTEXT_FAILED ||
				pa_context_get_state(context) == PA_CONTEXT_TERMINATED)) {
			for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
				struct bt_audio_stream *audio = bt_audio_slot(i);
				bool owned;
				osi_mutex_lock(&audio->lock, OS_TIMEOUT_FOREVER);
				owned = audio->allocated && audio->driver == &pulse_driver;
				osi_mutex_unlock(&audio->lock);
				if (slots[i].native) {
					native_close(&slots[i]);
				}
				if (owned) {
					bt_audio_complete(audio, BT_AUDIO_ERROR, -ECONNRESET);
				}
			}
			pa_context_unref(context);
			context = NULL;
		}
		for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
			process_stream(i, now);
		}
		if (bt_atomic_get(&quitting)) {
			break;
		}
		if (pa_mainloop_iterate(mainloop, 1, NULL) < 0) {
			break;
		}
	}
	if (timer) {
		api->time_free(timer);
	}
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		struct bt_audio_stream *audio = bt_audio_slot(i);
		bool owned;
		osi_mutex_lock(&audio->lock, OS_TIMEOUT_FOREVER);
		owned = audio->allocated && audio->driver == &pulse_driver;
		osi_mutex_unlock(&audio->lock);
		native_close(&slots[i]);
		if (owned) {
			bt_audio_complete(audio, BT_AUDIO_CLOSED, 0);
		}
	}
	if (context) {
		pa_context_disconnect(context);
		pa_context_unref(context);
		context = NULL;
	}
}

/** @brief Stop the event-loop owner before releasing its wakeup destination. */
static void pulse_shutdown(void)
{
	bt_atomic_set(&quitting, 1);
	pulse_wake();
	osi_thread_join(&thread);
	pa_mainloop_free(mainloop);
	mainloop = NULL;
	memset(slots, 0, sizeof(slots));
}

static const struct bt_audio_driver pulse_driver = {
	.name = "pulse", .wake = pulse_wake, .shutdown = pulse_shutdown,
};

int bt_audio_pulse_register(void)
{
	int err;
	if (!bt_audio_slot(0)) {
		return -EINVAL;
	}
	if (mainloop) {
		return -EEXIST;
	}
	mainloop = pa_mainloop_new();
	if (!mainloop) {
		return -ENOMEM;
	}
	bt_atomic_set(&quitting, 0);
	err = osi_thread_create(&thread, pulse_worker, NULL, "bt-audio-pulse", 0, 0);
	if (err) {
		pa_mainloop_free(mainloop);
		mainloop = NULL;
		return err;
	}
	err = bt_audio_driver_register(&pulse_driver);
	if (err) {
		pulse_shutdown();
	}
	return err;
}
