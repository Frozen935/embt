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

/*
 * Copyright (c) 1997-2015, Wind River Systems, Inc.
 * Copyright (c) 2021 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Header file for the Atomic operations API.
 * @ingroup bt_atomic_apis
 */

#ifndef EMBT_BASE_BT_ATOMIC_H_
#define EMBT_BASE_BT_ATOMIC_H_
#include <stdbool.h>
#include <stddef.h>
#include <embt/sys/util.h>
typedef long bt_atomic_t;
typedef long bt_atomic_val_t;
typedef void *bt_atomic_ptr_t;
typedef void *bt_atomic_ptr_val_t;
BUILD_ASSERT(__atomic_always_lock_free(sizeof(long), 0), "Native long atomics required");
BUILD_ASSERT(__atomic_always_lock_free(sizeof(void *), 0), "Native pointer atomics required");

#ifdef __cplusplus
extern "C" {
#endif

static inline bool bt_atomic_cas(bt_atomic_t *target, bt_atomic_val_t old_value,
				 bt_atomic_val_t new_value)
{
	return __atomic_compare_exchange_n(target, &old_value, new_value, 0, __ATOMIC_SEQ_CST,
					   __ATOMIC_SEQ_CST);
}

static inline bool bt_atomic_ptr_cas(bt_atomic_ptr_t *target, bt_atomic_ptr_val_t old_value,
				     bt_atomic_ptr_val_t new_value)
{
	return __atomic_compare_exchange_n(target, &old_value, new_value, 0, __ATOMIC_SEQ_CST,
					   __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_add(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_sub(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_sub(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_inc(bt_atomic_t *target)
{
	return bt_atomic_add(target, 1);
}

static inline bt_atomic_val_t bt_atomic_dec(bt_atomic_t *target)
{
	return bt_atomic_sub(target, 1);
}

static inline bt_atomic_val_t bt_atomic_get(const bt_atomic_t *target)
{
	return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_ptr_val_t bt_atomic_ptr_get(const bt_atomic_ptr_t *target)
{
	return __atomic_load_n(target, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_set(bt_atomic_t *target, bt_atomic_val_t value)
{
	/* This builtin, as described by Intel, is not a traditional
	 * test-and-set operation, but rather an atomic exchange operation. It
	 * writes value into *ptr, and returns the previous contents of *ptr.
	 */
	return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_ptr_val_t bt_atomic_ptr_set(bt_atomic_ptr_t *target,
						    bt_atomic_ptr_val_t value)
{
	return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_clear(bt_atomic_t *target)
{
	return bt_atomic_set(target, 0);
}

static inline bt_atomic_ptr_val_t bt_atomic_ptr_clear(bt_atomic_ptr_t *target)
{
	return bt_atomic_ptr_set(target, NULL);
}

static inline bt_atomic_val_t bt_atomic_or(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_xor(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_and(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST);
}

static inline bt_atomic_val_t bt_atomic_nand(bt_atomic_t *target, bt_atomic_val_t value)
{
	return __atomic_fetch_nand(target, value, __ATOMIC_SEQ_CST);
}

/* Portable higher-level utilities: */

/**
 * @defgroup bt_atomic_apis Atomic Services APIs
 * @ingroup kernel_apis
 * @{
 */

/**
 * @brief Initialize an atomic variable.
 *
 * This macro can be used to initialize an atomic variable. For example,
 * @code bt_atomic_t my_var = BT_ATOMIC_INIT(75); @endcode
 *
 * @param i Value to assign to atomic variable.
 */
#define BT_ATOMIC_INIT(i) (i)

/**
 * @brief Initialize an atomic pointer variable.
 *
 * This macro can be used to initialize an atomic pointer variable. For
 * example,
 * @code bt_atomic_ptr_t my_ptr = BT_ATOMIC_PTR_INIT(&data); @endcode
 *
 * @param p Pointer value to assign to atomic pointer variable.
 */
#define BT_ATOMIC_PTR_INIT(p) (p)

/**
 * @cond INTERNAL_HIDDEN
 */

#define BT_ATOMIC_BITS            (sizeof(bt_atomic_val_t) * BITS_PER_BYTE)
#define BT_ATOMIC_MASK(bit)       BIT((unsigned long)(bit) & (BT_ATOMIC_BITS - 1U))
#define BT_ATOMIC_ELEM(addr, bit) ((addr) + ((bit) / BT_ATOMIC_BITS))

/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @brief This macro computes the number of atomic variables necessary to
 * represent a bitmap with @a num_bits.
 *
 * @param num_bits Number of bits.
 */
#define BT_ATOMIC_BITMAP_SIZE(num_bits) (ROUND_UP(num_bits, BT_ATOMIC_BITS) / BT_ATOMIC_BITS)

/**
 * @brief Define an array of atomic variables.
 *
 * This macro defines an array of atomic variables containing at least
 * @a num_bits bits.
 *
 * @note
 * If used from file scope, the bits of the array are initialized to zero;
 * if used from within a function, the bits are left uninitialized.
 *
 * @cond INTERNAL_HIDDEN
 * @note
 * This macro should be replicated in the PREDEFINED field of the documentation
 * Doxyfile.
 * @endcond
 *
 * @param name Name of array of atomic variables.
 * @param num_bits Number of bits needed.
 */
#define BT_ATOMIC_DEFINE(name, num_bits) bt_atomic_t name[BT_ATOMIC_BITMAP_SIZE(num_bits)]

/**
 * @brief Atomically get and test a bit.
 *
 * Atomically get a value and then test whether bit number @a bit of @a target is set or not.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return true if the bit was set, false if it wasn't.
 */
static inline bool bt_atomic_test_bit(const bt_atomic_t *target, int bit)
{
	bt_atomic_val_t val = bt_atomic_get(BT_ATOMIC_ELEM(target, bit));

	return (1 & (val >> (bit & (BT_ATOMIC_BITS - 1)))) != 0;
}

/**
 * @brief Atomically clear a bit and test it.
 *
 * Atomically clear bit number @a bit of @a target and return its old value.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return false if the bit was already cleared, true if it wasn't.
 */
static inline bool bt_atomic_test_and_clear_bit(bt_atomic_t *target, int bit)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);
	bt_atomic_val_t old;

	old = bt_atomic_and(BT_ATOMIC_ELEM(target, bit), ~mask);

	return (old & mask) != 0;
}

/**
 * @brief Atomically set a bit and test it.
 *
 * Atomically set bit number @a bit of @a target and return its old value.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 *
 * @return true if the bit was already set, false if it wasn't.
 */
static inline bool bt_atomic_test_and_set_bit(bt_atomic_t *target, int bit)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);
	bt_atomic_val_t old;

	old = bt_atomic_or(BT_ATOMIC_ELEM(target, bit), mask);

	return (old & mask) != 0;
}

/**
 * @brief Atomically set a bit to a given value and report if it was changed.
 *
 * Atomically set bit number @a bit of @a target to value @a val.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 * @param val true for 1, false for 0.
 *
 * @return true if the bit was changed, false if it wasn't.
 */
static inline bool bt_atomic_test_and_set_bit_to(bt_atomic_t *target, int bit, bool val)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);

	if (val) {
		return (bt_atomic_or(BT_ATOMIC_ELEM(target, bit), mask) & mask) == 0;
	}

	return (bt_atomic_and(BT_ATOMIC_ELEM(target, bit), ~mask) & mask) != 0;
}

/**
 * @brief Atomically clear a bit.
 *
 * Atomically clear bit number @a bit of @a target.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 */
static inline void bt_atomic_clear_bit(bt_atomic_t *target, int bit)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);

	(void)bt_atomic_and(BT_ATOMIC_ELEM(target, bit), ~mask);
}

/**
 * @brief Atomically set a bit.
 *
 * Atomically set bit number @a bit of @a target.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 */
static inline void bt_atomic_set_bit(bt_atomic_t *target, int bit)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);

	(void)bt_atomic_or(BT_ATOMIC_ELEM(target, bit), mask);
}

/**
 * @brief Atomically set a bit to a given value.
 *
 * Atomically set bit number @a bit of @a target to value @a val.
 * The target may be a single atomic variable or an array of them.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable or array.
 * @param bit Bit number (starting from 0).
 * @param val true for 1, false for 0.
 */
static inline void bt_atomic_set_bit_to(bt_atomic_t *target, int bit, bool val)
{
	bt_atomic_val_t mask = BT_ATOMIC_MASK(bit);

	if (val) {
		(void)bt_atomic_or(BT_ATOMIC_ELEM(target, bit), mask);
	} else {
		(void)bt_atomic_and(BT_ATOMIC_ELEM(target, bit), ~mask);
	}
}

/*
 * The declarations below exist only so that Doxygen has a single place to
 * document the low-level atomic API.  Every backend selected above already
 * declares these functions, and does so with internal linkage when they are
 * static inline, so repeating them here unqualified would violate MISRA
 * C:2012 Rule 8.8.
 */
#ifdef __DOXYGEN__

/**
 * @brief Atomic compare-and-set.
 *
 * This routine performs an atomic compare-and-set on @a target. If the current
 * value of @a target equals @a old_value, @a target is set to @a new_value.
 * If the current value of @a target does not equal @a old_value, @a target
 * is left unchanged.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param old_value Original value to compare against.
 * @param new_value New value to store.
 * @return true if @a new_value is written, false otherwise.
 */
bool bt_atomic_cas(bt_atomic_t *target, bt_atomic_val_t old_value, bt_atomic_val_t new_value);

/**
 * @brief Atomic compare-and-set with pointer values
 *
 * This routine performs an atomic compare-and-set on @a target. If the current
 * value of @a target equals @a old_value, @a target is set to @a new_value.
 * If the current value of @a target does not equal @a old_value, @a target
 * is left unchanged.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param old_value Original value to compare against.
 * @param new_value New value to store.
 * @return true if @a new_value is written, false otherwise.
 */
bool bt_atomic_ptr_cas(bt_atomic_ptr_t *target, bt_atomic_ptr_val_t old_value,
		       bt_atomic_ptr_val_t new_value);

/**
 * @brief Atomic addition.
 *
 * This routine performs an atomic addition on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to add.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_add(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic subtraction.
 *
 * This routine performs an atomic subtraction on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to subtract.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_sub(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic increment.
 *
 * This routine performs an atomic increment by 1 on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_inc(bt_atomic_t *target);

/**
 * @brief Atomic decrement.
 *
 * This routine performs an atomic decrement by 1 on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_dec(bt_atomic_t *target);

/**
 * @brief Atomic get.
 *
 * This routine performs an atomic read on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 *
 * @return Value of @a target.
 */
bt_atomic_val_t bt_atomic_get(const bt_atomic_t *target);

/**
 * @brief Atomic get a pointer value
 *
 * This routine performs an atomic read on @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of pointer variable.
 *
 * @return Value of @a target.
 */
bt_atomic_ptr_val_t bt_atomic_ptr_get(const bt_atomic_ptr_t *target);

/**
 * @brief Atomic get-and-set.
 *
 * This routine atomically sets @a target to @a value and returns
 * the previous value of @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to write to @a target.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_set(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic get-and-set for pointer values
 *
 * This routine atomically sets @a target to @a value and returns
 * the previous value of @a target.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to write to @a target.
 *
 * @return Previous value of @a target.
 */
bt_atomic_ptr_val_t bt_atomic_ptr_set(bt_atomic_ptr_t *target, bt_atomic_ptr_val_t value);

/**
 * @brief Atomic clear.
 *
 * This routine atomically sets @a target to zero and returns its previous
 * value. (Hence, it is equivalent to bt_atomic_set(target, 0).)
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_clear(bt_atomic_t *target);

/**
 * @brief Atomic clear of a pointer value
 *
 * This routine atomically sets @a target to zero and returns its previous
 * value. (Hence, it is equivalent to bt_atomic_set(target, 0).)
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 *
 * @return Previous value of @a target.
 */
bt_atomic_ptr_val_t bt_atomic_ptr_clear(bt_atomic_ptr_t *target);

/**
 * @brief Atomic bitwise inclusive OR.
 *
 * This routine atomically sets @a target to the bitwise inclusive OR of
 * @a target and @a value.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to OR.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_or(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic bitwise exclusive OR (XOR).
 *
 * @note @bt_atomic_api
 *
 * This routine atomically sets @a target to the bitwise exclusive OR (XOR) of
 * @a target and @a value.
 *
 * @param target Address of atomic variable.
 * @param value Value to XOR
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_xor(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic bitwise AND.
 *
 * This routine atomically sets @a target to the bitwise AND of @a target
 * and @a value.
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to AND.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_and(bt_atomic_t *target, bt_atomic_val_t value);

/**
 * @brief Atomic bitwise NAND.
 *
 * This routine atomically sets @a target to the bitwise NAND of @a target
 * and @a value. (This operation is equivalent to target = ~(target & value).)
 *
 * @note @bt_atomic_api
 *
 * @param target Address of atomic variable.
 * @param value Value to NAND.
 *
 * @return Previous value of @a target.
 */
bt_atomic_val_t bt_atomic_nand(bt_atomic_t *target, bt_atomic_val_t value);

#endif /* __DOXYGEN__ */

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* EMBT_BASE_BT_ATOMIC_H_ */
