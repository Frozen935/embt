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

/** @file @brief Static PCM slots with per-stream synchronization and bounded copied queues. */
#include "driver.h"
#include <errno.h>
#include <string.h>

static struct bt_audio_stream streams[CONFIG_EMBT_AUDIO_STREAMS];
static const struct bt_audio_driver *drivers[CONFIG_EMBT_AUDIO_DRIVERS];
static bool initialized, registration_closed;
static size_t driver_count;

int bt_audio_driver_init(void)
{
	int err;
	if (initialized) {
		return -EALREADY;
	}
	memset(streams, 0, sizeof(streams));
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		err = osi_mutex_init(&streams[i].lock, OS_MUTEX_NORMAL);
		if (err) {
			while (i) {
				osi_mutex_deinit(&streams[--i].lock);
			}
			return err;
		}
	}
	driver_count = 0;
	registration_closed = false;
	initialized = true;
	return 0;
}

int bt_audio_driver_register(const struct bt_audio_driver *driver)
{
	if (!initialized || !driver || !driver->name || !driver->name[0] ||
	    !driver->wake || !driver->shutdown) {
		return -EINVAL;
	}
	if (registration_closed) {
		return -EBUSY;
	}
	for (size_t i = 0; i < driver_count; i++) {
		if (!strcmp(drivers[i]->name, driver->name)) {
			return -EEXIST;
		}
	}
	if (driver_count == CONFIG_EMBT_AUDIO_DRIVERS) {
		return -ENOSPC;
	}
	drivers[driver_count++] = driver;
	return 0;
}

struct bt_audio_stream *bt_audio_slot(size_t index)
{
	return initialized && index < CONFIG_EMBT_AUDIO_STREAMS ? &streams[index] : NULL;
}

int bt_audio_open(const char *backend, const char *device,
		  const struct bt_audio_format *format, bt_audio_notify_t notify,
		  void *user, struct bt_audio_stream **output)
{
	const struct bt_audio_driver *driver = NULL;
	if (!initialized || !backend || !format || !output || !format->rate ||
	    format->rate > 192000 || (format->channels != 1 && format->channels != 2) ||
	    (device && strlen(device) >= sizeof(streams[0].device))) {
		return -EINVAL;
	}
	for (size_t i = 0; i < driver_count; i++) {
		if (!strcmp(backend, drivers[i]->name)) {
			driver = drivers[i];
			break;
		}
	}
	if (!driver) {
		return -ENODEV;
	}
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		struct bt_audio_stream *stream = &streams[i];
		osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
		if (stream->allocated) {
			osi_mutex_unlock(&stream->lock);
			continue;
		}
		stream->allocated = true;
		stream->driver = driver;
		stream->format = *format;
		strcpy(stream->device, device ? device : "");
		stream->notify = notify;
		stream->user = user;
		stream->head = stream->count = 0;
		stream->status = (struct bt_audio_status){.state = BT_AUDIO_OPENING};
		registration_closed = true;
		*output = stream;
		osi_mutex_unlock(&stream->lock);
		driver->wake();
		return 0;
	}
	return -ENOSPC;
}

int bt_audio_submit(struct bt_audio_stream *stream, const int16_t *pcm, size_t frames)
{
	const struct bt_audio_driver *driver;
	size_t bytes;
	int err = 0;
	if (!initialized || !stream || !pcm || !frames) {
		return -EINVAL;
	}
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	if (!stream->allocated || stream->status.state != BT_AUDIO_READY) {
		err = stream->status.state == BT_AUDIO_OPENING ? -EAGAIN : -ESHUTDOWN;
	} else if (frames > CONFIG_EMBT_AUDIO_BLOCK_BYTES / (stream->format.channels * 2U)) {
		err = -EMSGSIZE;
	} else if (stream->count == CONFIG_EMBT_AUDIO_QUEUE_BLOCKS) {
		err = -EAGAIN;
	} else {
		struct bt_audio_block *block =
			&stream->blocks[(stream->head + stream->count) % CONFIG_EMBT_AUDIO_QUEUE_BLOCKS];
		bytes = frames * stream->format.channels * sizeof(int16_t);
		memcpy(block->data, pcm, bytes);
		block->length = bytes;
		block->offset = 0;
		stream->count++;
		stream->status.submitted_frames += frames;
		stream->status.queued_frames += frames;
	}
	driver = stream->driver;
	osi_mutex_unlock(&stream->lock);
	if (!err) {
		driver->wake();
	}
	return err;
}

static int request_state(struct bt_audio_stream *stream, enum bt_audio_state state, bool drain)
{
	const struct bt_audio_driver *driver;
	int err = 0;
	if (!initialized || !stream) {
		return -EINVAL;
	}
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	if (!stream->allocated || stream->status.state == BT_AUDIO_CLOSING ||
	    stream->status.state == BT_AUDIO_CLOSED) {
		err = -ESHUTDOWN;
	} else if (state == BT_AUDIO_READY && stream->status.state != BT_AUDIO_STOPPED) {
		err = -EBUSY;
	} else if (state == BT_AUDIO_STOPPING && stream->status.state != BT_AUDIO_READY) {
		err = -EBUSY;
	} else {
		stream->status.state = state;
		stream->drain = drain;
	}
	driver = stream->driver;
	osi_mutex_unlock(&stream->lock);
	if (!err) {
		driver->wake();
	}
	return err;
}

int bt_audio_stop(struct bt_audio_stream *stream, bool drain)
{
	return request_state(stream, BT_AUDIO_STOPPING, drain);
}

int bt_audio_start(struct bt_audio_stream *stream)
{
	return request_state(stream, BT_AUDIO_READY, false);
}

int bt_audio_close(struct bt_audio_stream *stream)
{
	return request_state(stream, BT_AUDIO_CLOSING, false);
}

int bt_audio_get_status(struct bt_audio_stream *stream, struct bt_audio_status *status)
{
	if (!initialized || !stream || !status) {
		return -EINVAL;
	}
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	*status = stream->status;
	osi_mutex_unlock(&stream->lock);
	return 0;
}

void bt_audio_complete(struct bt_audio_stream *stream, enum bt_audio_state state, int error)
{
	bt_audio_notify_t notify;
	void *user;
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	/* A pending close or stop wins over delayed native readiness. */
	if (!stream->allocated ||
	    (state == BT_AUDIO_READY && stream->status.state != BT_AUDIO_OPENING) ||
	    (stream->status.state == BT_AUDIO_CLOSING && state != BT_AUDIO_CLOSED)) {
		osi_mutex_unlock(&stream->lock);
		return;
	}
	stream->status.state = state;
	stream->status.error = error;
	if (state == BT_AUDIO_CLOSED || state == BT_AUDIO_ERROR ||
	    (state == BT_AUDIO_STOPPED && !stream->drain)) {
		stream->count = 0;
		stream->status.queued_frames = 0;
	}
	notify = stream->notify;
	user = stream->user;
	osi_mutex_unlock(&stream->lock);
	if (notify) {
		notify(stream, state, error, user);
	}
	if (state == BT_AUDIO_CLOSED) {
		osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
		stream->allocated = false;
		stream->driver = NULL;
		osi_mutex_unlock(&stream->lock);
	}
}

size_t bt_audio_peek(struct bt_audio_stream *stream, void *data, size_t capacity)
{
	size_t bytes = 0;
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	if (stream->count) {
		struct bt_audio_block *block = &stream->blocks[stream->head];
		bytes = block->length - block->offset;
		if (bytes > capacity) {
			bytes = capacity;
		}
		bytes -= bytes % (stream->format.channels * sizeof(int16_t));
		memcpy(data, (char *)block->data + block->offset, bytes);
	}
	osi_mutex_unlock(&stream->lock);
	return bytes;
}

void bt_audio_consume(struct bt_audio_stream *stream, size_t bytes)
{
	osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
	if (stream->count) {
		struct bt_audio_block *block = &stream->blocks[stream->head];
		block->offset += bytes;
		stream->status.queued_frames -= bytes / (stream->format.channels * 2U);
		stream->status.consumed_frames += bytes / (stream->format.channels * 2U);
		if (block->offset == block->length) {
			stream->head = (stream->head + 1) % CONFIG_EMBT_AUDIO_QUEUE_BLOCKS;
			stream->count--;
		}
	}
	osi_mutex_unlock(&stream->lock);
}

void bt_audio_driver_shutdown(void)
{
	if (!initialized) {
		return;
	}
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		bt_audio_close(&streams[i]);
	}
	for (size_t i = 0; i < driver_count; i++) {
		drivers[i]->shutdown();
	}
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		osi_mutex_deinit(&streams[i].lock);
	}
	initialized = false;
}
