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

/** @file @brief Repository-internal audio backend registration and queue contract. */
#ifndef EMBT_AUDIO_DRIVER_H_
#define EMBT_AUDIO_DRIVER_H_

#include <embt/drivers/audio.h>
#include <embt/osi/sync.h>

/** @brief Persistent backend identity and execution scheduling operations. */
struct bt_audio_driver {
	const char *name;
	void (*wake)(void); /**< Thread-safe; schedule processing without device I/O. */
	void (*shutdown)(void); /**< Join execution and release native resources. */
};

/** @brief One statically allocated copied PCM block and its consumed byte offset. */
struct bt_audio_block {
	size_t length, offset;
	int16_t data[CONFIG_EMBT_AUDIO_BLOCK_BYTES / sizeof(int16_t)];
};

/** @brief Private stream storage; native providers own consumption and state completion. */
struct bt_audio_stream {
	os_mutex_t lock;
	const struct bt_audio_driver *driver;
	struct bt_audio_format format;
	char device[256];
	bt_audio_notify_t notify;
	void *user;
	struct bt_audio_status status;
	bool drain, allocated;
	size_t head, count;
	struct bt_audio_block blocks[CONFIG_EMBT_AUDIO_QUEUE_BLOCKS];
};

/**
 * @brief Register a persistent backend descriptor before the first open.
 * @param[in] driver Descriptor and name valid until driver shutdown; wake and shutdown are
 *   required.
 * @return Zero on success, -EEXIST for duplicates, -ENOSPC for a full table, or another negative
 *   errno.
 */
int bt_audio_driver_register(const struct bt_audio_driver *driver);
/**
 * @brief Access stable private PCM slot storage.
 * @param[in] index Slot index below CONFIG_EMBT_AUDIO_STREAMS.
 * @return The slot pointer, or NULL for an invalid index or uninitialized driver.
 * @note Check allocation and driver ownership while holding the slot lock.
 */
struct bt_audio_stream *bt_audio_slot(size_t index);
/**
 * @brief Publish native completion and notify outside the slot lock.
 * @param[in,out] stream Slot owned by the calling backend.
 * @param[in] state Completed lifecycle state.
 * @param[in] error Zero for success or a negative native error.
 * @note Close supersedes late readiness. CLOSED relinquishes the slot after the callback returns.
 */
void bt_audio_complete(struct bt_audio_stream *stream, enum bt_audio_state state, int error);
/**
 * @brief Copy frame-aligned bytes from the front PCM block.
 * @param[in] stream Valid backend-owned slot.
 * @param[out] data Destination storage of at least capacity bytes.
 * @param[in] capacity Maximum byte count to copy.
 * @return Copied byte count, or zero for an empty queue or insufficient frame capacity.
 * @note One backend owner serializes peek, native write, consume and cancellation.
 */
size_t bt_audio_peek(struct bt_audio_stream *stream, void *data, size_t capacity);
/**
 * @brief Retire bytes copied by the backend.
 * @param[in,out] stream Valid backend-owned slot.
 * @param[in] bytes Frame-aligned byte count no greater than the preceding peek result.
 * @note Consumption confirms buffer reuse, not physical speaker completion.
 */
void bt_audio_consume(struct bt_audio_stream *stream, size_t bytes);

#endif /* EMBT_AUDIO_DRIVER_H_ */
