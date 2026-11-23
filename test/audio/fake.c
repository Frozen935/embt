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

/** @file @brief A real copied-PCM consumer with controllable readiness and failure. */
#include "fake.h"
#include <string.h>
#include <errno.h>

bool fake_hold;
int fake_error;
unsigned char fake_pcm[262144];
size_t fake_length;

void fake_pump(void)
{
	if (fake_hold) {
		return;
	}
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		struct bt_audio_stream *stream = bt_audio_slot(i);
		struct bt_audio_status status;
		bool owned;
		osi_mutex_lock(&stream->lock, OS_TIMEOUT_FOREVER);
		owned = stream->allocated &&
			(stream->driver == &fake_driver || stream->driver == &other_driver);
		osi_mutex_unlock(&stream->lock);
		if (!owned) {
			continue;
		}
		bt_audio_get_status(stream, &status);
		if (status.state == BT_AUDIO_CLOSING) {
			bt_audio_complete(stream, BT_AUDIO_CLOSED, 0);
			continue;
		}
		if (status.state == BT_AUDIO_OPENING) {
			bt_audio_complete(stream, fake_error ? BT_AUDIO_ERROR : BT_AUDIO_READY, fake_error);
		}
		bt_audio_get_status(stream, &status);
		if (status.state == BT_AUDIO_READY ||
		    (status.state == BT_AUDIO_STOPPING && stream->drain)) {
			size_t bytes;
			while ((bytes = bt_audio_peek(stream, fake_pcm + fake_length,
						 sizeof(fake_pcm) - fake_length)) != 0) {
				fake_length += bytes;
				bt_audio_consume(stream, bytes);
			}
		}
		if (status.state == BT_AUDIO_STOPPING) {
			bt_audio_complete(stream, BT_AUDIO_STOPPED, 0);
		}
	}
}

/** @brief Reclaim all test-owned slots before common synchronization is destroyed. */
static void fake_shutdown(void)
{
	fake_hold = false;
	fake_pump();
}

const struct bt_audio_driver fake_driver = {
	.name = "memory", .wake = fake_pump, .shutdown = fake_shutdown,
};
const struct bt_audio_driver other_driver = {
	.name = "other", .wake = fake_pump, .shutdown = fake_shutdown,
};

int audio_setup(void **state)
{
	int err;
	(void)state;
	fake_hold = false;
	fake_error = 0;
	fake_length = 0;
	err = bt_audio_driver_init();
	if (!err) {
		err = bt_audio_driver_register(&fake_driver);
	}
	if (!err) {
		err = bt_audio_driver_register(&other_driver);
	}
	return err;
}

int audio_teardown(void **state)
{
	(void)state;
	bt_audio_driver_shutdown();
	return 0;
}
