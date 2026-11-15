/* buf.c - Buffer management */

/*
 * Copyright (c) 2015-2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME net_buf
#define LOG_LEVEL CONFIG_NET_BUF_LOG_LEVEL
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define NET_BUF_DBG(...) LOG_DBG(__VA_ARGS__)
#define NET_BUF_ERR(...) LOG_ERR(__VA_ARGS__)
#define NET_BUF_WARN(...) LOG_WRN(__VA_ARGS__)
#define NET_BUF_INFO(...) LOG_INF(__VA_ARGS__)
#include <base/base.h>
#include <base/fifo.h>
#include <base/lifo.h>
#include <base/spinlock.h>
#include <base/time.h>
#include <embt/types/timeout.h>
#include <base/atomic.h>
#include <osi/memory.h>
#include <osi/io.h>
#include <base/assert.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <embt/sys/byteorder.h>
#include <embt/sys/minmax.h>

#include <embt/net_buf.h>

/* The struct net_buf::ref_word layout assumes bt_atomic_t is `long` -- the
 * conditional padding in the byte-struct view (see net_buf.h) keys off
 * __SIZEOF_LONG__ to keep `ref` at the LSB of ref_word on big-endian
 * 64-bit. If bt_atomic_t is ever redefined to a different integer type,
 * the layout has to be revisited.
 */
BUILD_ASSERT(sizeof(bt_atomic_t) == sizeof(long),
	     "struct net_buf::ref_word layout assumes bt_atomic_t == long");

#define GET_ALIGN(pool) MAX(sizeof(void *), pool->alloc->alignment)

void net_buf_pool_init(struct net_buf_pool *pool)
{
	if (!pool->free._queue.initialized) {
		bt_lifo_init(&pool->free);
	}
}

int net_buf_id(const struct net_buf *buf)
{
	struct net_buf_pool *pool = buf->pool;
	size_t struct_size = ROUND_UP(sizeof(struct net_buf) + pool->user_data_size,
				__alignof__(struct net_buf));
	ptrdiff_t offset = (uint8_t *)buf - (uint8_t *)pool->__bufs;

	return offset / struct_size;
}

static inline struct net_buf *pool_get_uninit(struct net_buf_pool *pool,
					      uint16_t uninit_count)
{
	size_t struct_size = ROUND_UP(sizeof(struct net_buf) + pool->user_data_size,
				__alignof__(struct net_buf));
	size_t byte_offset = (pool->buf_count - uninit_count) * struct_size;
	struct net_buf *buf;

	buf = (struct net_buf *)(((uint8_t *)pool->__bufs) + byte_offset);

	buf->pool = pool;
	buf->user_data_size = pool->user_data_size;

	return buf;
}

void net_buf_reset(struct net_buf *buf)
{
	__ASSERT_NO_MSG(buf->frags == NULL);

	net_buf_simple_reset(&buf->b);
}

static uint8_t *generic_data_ref(struct net_buf *buf, uint8_t *data)
{
	struct net_buf_pool *buf_pool = buf->pool;
	bt_atomic_t *ref_count;

	ref_count = (bt_atomic_t *)(data - GET_ALIGN(buf_pool));
	bt_atomic_inc(ref_count);

	return data;
}

static uint8_t *fixed_data_alloc(struct net_buf *buf, size_t *size)
{
	struct net_buf_pool *pool = buf->pool;
	const struct net_buf_pool_fixed *fixed = pool->alloc->alloc_data;

	*size = pool->alloc->max_alloc_size;

	return fixed->data_pool + *size * net_buf_id(buf);
}

static void fixed_data_unref(struct net_buf *buf, uint8_t *data)
{
	/* Nothing needed for fixed-size data pools */
}

const struct net_buf_data_cb net_buf_fixed_cb = {
	.alloc = fixed_data_alloc,
	.unref = fixed_data_unref,
};

static uint8_t *heap_data_alloc(struct net_buf *buf, size_t *size)
{
	struct net_buf_pool *pool = buf->pool;
	size_t prefix = GET_ALIGN(pool);
	bt_atomic_t *ref_count;

	if (*size > SIZE_MAX - prefix) {
		__ASSERT(false, "Dynamic net_buf size overflow");
		osi_fatal("Dynamic net_buf size overflow");
	}
	if (pool->alloc->alignment) {
		ref_count = osi_aligned_alloc(prefix, prefix + *size);
	} else {
		ref_count = osi_malloc(prefix + *size);
	}
	if (!ref_count) {
		__ASSERT(false, "Dynamic net_buf allocation failed");
		osi_fatal("Dynamic net_buf allocation failed");
	}
	bt_atomic_set(ref_count, 1);

	return (uint8_t *)ref_count + prefix;
}

static void heap_data_unref(struct net_buf *buf, uint8_t *data)
{
	struct net_buf_pool *buf_pool = buf->pool;
	bt_atomic_t *ref_count;

	ref_count = (bt_atomic_t *)(data - GET_ALIGN(buf_pool));
	if (bt_atomic_dec(ref_count) != 1) {
		return;
	}

	osi_free(ref_count);
}

const struct net_buf_data_cb net_buf_heap_cb = {
	.alloc = heap_data_alloc,
	.ref   = generic_data_ref,
	.unref = heap_data_unref,
};

const struct net_buf_data_alloc net_buf_heap_alloc = {
	.cb = &net_buf_heap_cb,
	.max_alloc_size = 0,
	.alignment = CONFIG_NET_BUF_ALIGNMENT,
};

static uint8_t *data_alloc(struct net_buf *buf, size_t *size)
{
	struct net_buf_pool *pool = buf->pool;

	return pool->alloc->cb->alloc(buf, size);
}

static uint8_t *data_ref(struct net_buf *buf, uint8_t *data)
{
	struct net_buf_pool *pool = buf->pool;

	return pool->alloc->cb->ref(buf, data);
}

struct net_buf *net_buf_alloc_len(struct net_buf_pool *pool, size_t size,
				  bt_timeout_t timeout)
{
	bt_timepoint_t end = bt_timepoint_calc(timeout);
	struct net_buf *buf;
	bt_spinlock_key_t key;

	__ASSERT_NO_MSG(pool);
	bt_base_require(pool->free._queue.initialized ? 0 : -EINVAL);


	NET_BUF_DBG("%s():%d: pool %p size %zu", __func__, __LINE__, pool, size);
	/* We need to prevent race conditions
	 * when accessing pool->uninit_count.
	 */
	key = bt_spin_lock(&pool->lock);

	/* If there are uninitialized buffers we're guaranteed to succeed
	 * with the allocation one way or another.
	 */
	if (pool->uninit_count) {
		uint16_t uninit_count;

		/* If this is not the first access to the pool, we can
		 * be opportunistic and try to fetch a previously used
		 * buffer from the LIFO with BT_NO_WAIT.
		 */
		if (pool->uninit_count < pool->buf_count) {
			buf = bt_lifo_get(&pool->free, BT_NO_WAIT);
			if (buf) {
				bt_spin_unlock(&pool->lock, key);
				goto success;
			}
		}

		uninit_count = pool->uninit_count--;
		bt_spin_unlock(&pool->lock, key);

		buf = pool_get_uninit(pool, uninit_count);
		goto success;
	}

	bt_spin_unlock(&pool->lock, key);

	buf = bt_lifo_get(&pool->free, BT_NO_WAIT);
	if (buf) {
		goto success;
	}

	if (!BT_TIMEOUT_EQ(timeout, BT_NO_WAIT)) {
		uint64_t wait_start = (uint64_t)bt_uptime_get_ms();

		NET_BUF_WARN("Pool %p low on buffers.", (void *)pool);
		buf = bt_lifo_get(&pool->free, bt_timepoint_timeout(end));
		NET_BUF_WARN("Pool %p blocked for %llu ms", (void *)pool,
			     (unsigned long long)((uint64_t)bt_uptime_get_ms() - wait_start));
	}
	if (!buf) {
		NET_BUF_ERR("%s():%d: Failed to get free buffer", __func__, __LINE__);
		return NULL;
	}

success:
	NET_BUF_DBG("allocated buf %p", buf);

	if (size) {
		__maybe_unused size_t req_size = size;

		buf->__buf = data_alloc(buf, &size);
		if (!buf->__buf) {
			NET_BUF_ERR("%s():%d: Failed to allocate data",
				    __func__, __LINE__);
			net_buf_destroy(buf);
			return NULL;
		}

		__ASSERT_NO_MSG(req_size <= size);
	} else {
		buf->__buf = NULL;
	}

	buf->ref   = 1U;
	buf->flags = 0U;
	buf->frags = NULL;
	buf->size  = size;
	memset(buf->user_data, 0, buf->user_data_size);
	net_buf_reset(buf);

#if defined(CONFIG_NET_BUF_POOL_USAGE)
	bt_atomic_val_t avail = bt_atomic_dec(&pool->avail_count) - 1;

	__ASSERT_NO_MSG(avail >= 0);

	/* A concurrent allocation must not overwrite a higher value with its own */
	key = bt_spin_lock(&pool->lock);
	pool->max_used = max(pool->max_used, pool->buf_count - avail);
	bt_spin_unlock(&pool->lock, key);
#endif
	return buf;
}

struct net_buf *net_buf_alloc_fixed(struct net_buf_pool *pool,
				    bt_timeout_t timeout)
{
	return net_buf_alloc_len(pool, pool->alloc->max_alloc_size, timeout);
}

struct net_buf *net_buf_alloc_with_data(struct net_buf_pool *pool,
					void *data, size_t size,
					bt_timeout_t timeout)
{
	struct net_buf *buf;

	buf = net_buf_alloc_len(pool, 0, timeout);
	if (!buf) {
		return NULL;
	}

	net_buf_simple_init_with_data(&buf->b, data, size);
	buf->flags = NET_BUF_EXTERNAL_DATA;

	return buf;
}

static struct bt_spinlock net_buf_slist_lock;

void net_buf_slist_put(sys_slist_t *list, struct net_buf *buf)
{
	bt_spinlock_key_t key;

	__ASSERT_NO_MSG(list);
	__ASSERT_NO_MSG(buf);

	key = bt_spin_lock(&net_buf_slist_lock);
	sys_slist_append(list, &buf->node);
	bt_spin_unlock(&net_buf_slist_lock, key);
}

struct net_buf *net_buf_slist_get(sys_slist_t *list)
{
	struct net_buf *buf;
	bt_spinlock_key_t key;

	__ASSERT_NO_MSG(list);

	key = bt_spin_lock(&net_buf_slist_lock);

	buf = (void *)sys_slist_get(list);

	bt_spin_unlock(&net_buf_slist_lock, key);

	return buf;
}

void net_buf_slist_purge(sys_slist_t *list)
{
	bt_spinlock_key_t key;
	sys_snode_t *node;

	__ASSERT_NO_MSG(list != NULL);

	/* Destroy callbacks may use the list functions, so they must not run under the lock */
	key = bt_spin_lock(&net_buf_slist_lock);
	node = sys_slist_peek_head(list);
	sys_slist_init(list);
	bt_spin_unlock(&net_buf_slist_lock, key);

	while (node != NULL) {
		struct net_buf *buf = CONTAINER_OF(node, struct net_buf, node);

		/* Read the link before the buffer may go back to its pool */
		node = sys_slist_peek_next_no_check(node);
		net_buf_unref(buf);
	}
}

void net_buf_fifo_purge(struct bt_fifo *fifo)
{
	struct net_buf *buf;

	__ASSERT_NO_MSG(fifo != NULL);

	buf = bt_fifo_get(fifo, BT_NO_WAIT);
	while (buf != NULL) {
		net_buf_unref(buf);
		buf = bt_fifo_get(fifo, BT_NO_WAIT);
	}
}

void net_buf_unref(struct net_buf *buf)
{
	__ASSERT_NO_MSG(buf);

	while (buf) {
		/* Capture fields that may be needed for logging *before* the
		 * decrement: once our reference is dropped, another CPU may
		 * free the buffer and we must not read it again. The
		 * decrement is performed on `ref_word` (the bt_atomic_t view of
		 * the slot shared with flags/_reserved/user_data_size) and the
		 * uint8_t narrowing extracts just the ref byte from the
		 * prior word value.
		 */
		struct net_buf *frags = buf->frags;
		struct net_buf_pool *pool;
		bt_atomic_val_t old_word;
		uint8_t old_ref;

		/* A zero count must not be decremented, not even by an unref
		 * racing with the last one: the borrow would go into the other
		 * bytes of ref_word of a buffer that has already been freed.
		 * A saturated count stays as it is, see net_buf_ref().
		 */
		do {
			old_word = bt_atomic_get(&buf->ref_word);
			old_ref = (uint8_t)old_word;
			if ((old_ref == 0U) || (old_ref == UINT8_MAX)) {
				break;
			}
		} while (!bt_atomic_cas(&buf->ref_word, old_word, old_word - 1));


		NET_BUF_DBG("buf %p ref %u pool %p frags %p", buf, old_ref,
			    (void *)buf->pool, frags);
		__ASSERT(old_ref != 0, "buf %p double free", buf);
		if (old_ref == 0) {
			NET_BUF_ERR("buf %p double free", (void *)buf);
			return;
		}

		if (old_ref != 1) {
			return;
		}

		buf->data = NULL;
		buf->frags = NULL;

		pool = buf->pool;

#if defined(CONFIG_NET_BUF_POOL_USAGE)
		__maybe_unused bt_atomic_val_t old_avail =
			bt_atomic_inc(&pool->avail_count);

		__ASSERT_NO_MSG(old_avail + 1 <= pool->buf_count);
#endif

		if (pool->destroy) {
			pool->destroy(buf);
		} else {
			net_buf_destroy(buf);
		}

		buf = frags;
	}
}

struct net_buf *net_buf_ref(struct net_buf *buf)
{
	bt_atomic_val_t old_word;
	uint8_t old_ref;

	__ASSERT_NO_MSG(buf);

	/* A count that reaches UINT8_MAX saturates: it is neither incremented
	 * nor decremented any more, so the buffer is never freed. Incrementing
	 * it would carry into the other bytes of ref_word and let the buffer be
	 * freed while it is still referenced.
	 */
	do {
		old_word = bt_atomic_get(&buf->ref_word);
		old_ref = (uint8_t)old_word;
		if (old_ref == UINT8_MAX) {
			break;
		}
	} while (!bt_atomic_cas(&buf->ref_word, old_word, old_word + 1));

	__ASSERT(old_ref < (UINT8_MAX - 1U), "buf %p ref count saturated", buf);
	if (old_ref >= (UINT8_MAX - 1U)) {
		NET_BUF_ERR("buf %p ref count saturated", buf);
	}

	NET_BUF_DBG("buf %p (old) ref %u pool %p",
		    buf, old_ref, (void *)buf->pool);
	return buf;
}

struct net_buf *net_buf_clone(struct net_buf *buf, bt_timeout_t timeout)
{
	struct net_buf_pool *pool;
	struct net_buf *clone;

	__ASSERT_NO_MSG(buf);

	pool = buf->pool;

	clone = net_buf_alloc_len(pool, 0, timeout);
	if (!clone) {
		return NULL;
	}

	/* If the pool supports data referencing use that. Otherwise
	 * we need to allocate new data and make a copy.
	 */
	if (pool->alloc->cb->ref && !(buf->flags & NET_BUF_EXTERNAL_DATA)) {
		clone->__buf = buf->__buf ? data_ref(buf, buf->__buf) : NULL;
		clone->data = buf->data;
		clone->len = buf->len;
		clone->size = buf->size;
	} else {
		size_t size = buf->size;

		clone->__buf = data_alloc(clone, &size);
		if (!clone->__buf || size < buf->size) {
			net_buf_destroy(clone);
			return NULL;
		}

		clone->size = size;
		clone->data = clone->__buf + net_buf_headroom(buf);
		net_buf_add_mem(clone, buf->data, buf->len);
	}

	/* user_data_size should be the same for buffers from the same pool */
	__ASSERT(buf->user_data_size == clone->user_data_size, "Unexpected user data size");

	memcpy(clone->user_data, buf->user_data, clone->user_data_size);

	return clone;
}

int net_buf_user_data_copy(struct net_buf *dst, const struct net_buf *src)
{
	__ASSERT_NO_MSG(dst);
	__ASSERT_NO_MSG(src);

	if (dst == src) {
		return 0;
	}

	if (dst->user_data_size < src->user_data_size) {
		return -EINVAL;
	}

	memcpy(dst->user_data, src->user_data, src->user_data_size);

	return 0;
}

struct net_buf *net_buf_frag_last(struct net_buf *buf)
{
	__ASSERT_NO_MSG(buf);

	while (buf->frags) {
		buf = buf->frags;
	}

	return buf;
}

void net_buf_frag_insert(struct net_buf *parent, struct net_buf *frag)
{
	__ASSERT_NO_MSG(parent);
	__ASSERT_NO_MSG(frag);

	if (parent->frags) {
		net_buf_frag_last(frag)->frags = parent->frags;
	}
	/* Take ownership of the fragment reference */
	parent->frags = frag;
}

struct net_buf *net_buf_frag_add(struct net_buf *head, struct net_buf *frag)
{
	__ASSERT_NO_MSG(frag);

	if (!head) {
		return net_buf_ref(frag);
	}

	net_buf_frag_insert(net_buf_frag_last(head), frag);

	return head;
}

struct net_buf *net_buf_frag_del(struct net_buf *parent, struct net_buf *frag)
{
	struct net_buf *next_frag;

	__ASSERT_NO_MSG(frag);

	if (parent) {
		__ASSERT_NO_MSG(parent->frags);
		__ASSERT_NO_MSG(parent->frags == frag);
		parent->frags = frag->frags;
	}

	next_frag = frag->frags;

	frag->frags = NULL;

	net_buf_unref(frag);

	return next_frag;
}

size_t net_buf_linearize(void *dst, size_t dst_len, const struct net_buf *src,
			 size_t offset, size_t len)
{
	const struct net_buf *frag;
	size_t to_copy;
	size_t copied;

	len = min(len, dst_len);

	frag = src;

	/* find the right fragment to start copying from */
	while (frag && offset >= frag->len) {
		offset -= frag->len;
		frag = frag->frags;
	}

	/* traverse the fragment chain until len bytes are copied */
	copied = 0;
	while (frag && len > 0) {
		to_copy = min(len, frag->len - offset);
		memcpy((uint8_t *)dst + copied, frag->data + offset, to_copy);

		copied += to_copy;

		/* to_copy is always <= len */
		len -= to_copy;
		frag = frag->frags;

		/* after the first iteration, this value will be 0 */
		offset = 0;
	}

	return copied;
}

/* This helper routine will append multiple bytes, if there is no place for
 * the data in current fragment then create new fragment and add it to
 * the buffer. It assumes that the buffer has at least one fragment.
 */
size_t net_buf_append_bytes(struct net_buf *buf, size_t len,
			    const void *value, bt_timeout_t timeout,
			    net_buf_allocator_cb allocate_cb, void *user_data)
{
	struct net_buf *tail = net_buf_frag_last(buf);
	struct net_buf *frag = tail;
	size_t added_len = 0;
	const uint8_t *value8 = value;
	size_t max_size;

	do {
		uint16_t count = min(len, net_buf_tailroom(frag));

		net_buf_add_mem(frag, value8, count);
		len -= count;
		added_len += count;
		value8 += count;

		if (len == 0) {
			return added_len;
		}

		if (allocate_cb) {
			frag = allocate_cb(timeout, user_data);
		} else {
			struct net_buf_pool *pool;

			/* Allocate from the original pool if no callback has
			 * been provided.
			 */
			pool = buf->pool;
			max_size = pool->alloc->max_alloc_size;
			frag = net_buf_alloc_len(pool,
						 max_size ? min(len, max_size) : len,
						 timeout);
		}

		if (!frag) {
			return added_len;
		}

		/* The tail is already known, so link the new fragment behind
		 * it instead of walking the chain from the head again: a
		 * single append spanning n fragments would otherwise cost
		 * O(n^2) traversal steps. Re-derive the tail from the
		 * returned fragment, in case the allocator handed back a
		 * chain rather than a single buffer.
		 */
		net_buf_frag_insert(tail, frag);
		tail = net_buf_frag_last(frag);
	} while (1);

	/* Unreachable */
	return 0;
}

size_t net_buf_data_match(const struct net_buf *buf, size_t offset, const void *data, size_t len)
{
	const uint8_t *dptr = data;
	const uint8_t *bptr;
	size_t compared = 0;
	size_t to_compare;

	if (!buf || !data) {
		return compared;
	}

	/* find the right fragment to start comparison */
	while (buf && offset >= buf->len) {
		offset -= buf->len;
		buf = buf->frags;
	}

	while (buf && len > 0) {
		bptr = buf->data + offset;
		to_compare = min(len, buf->len - offset);

		for (size_t i = 0; i < to_compare; ++i) {
			if (dptr[compared] != bptr[i]) {
				return compared;
			}
			compared++;
		}

		len -= to_compare;
		buf = buf->frags;
		offset = 0;
	}

	return compared;
}

struct net_buf *__must_check net_buf_take(struct net_buf **orig)
{
	return (struct net_buf *)bt_atomic_ptr_clear((bt_atomic_ptr_t *)orig);
}

void net_buf_destroy(struct net_buf *buf)
{
	struct net_buf_pool *pool = buf->pool;

	if (buf->__buf) {
		if (!(buf->flags & NET_BUF_EXTERNAL_DATA)) {
			pool->alloc->cb->unref(buf, buf->__buf);
		}
		buf->__buf = NULL;
	}

	bt_lifo_put(&pool->free, buf);
}

#if defined(CONFIG_NET_BUF_POOL_USAGE)
size_t net_buf_get_available(struct net_buf_pool *pool)
{
	return (size_t)bt_atomic_get(&pool->avail_count);
}
#endif
