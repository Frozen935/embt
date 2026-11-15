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
 * Derived compiler attributes retain the notices of their original providers:
 * Copyright (c) 2010-2014, Wind River Systems, Inc.
 * Copyright (c) 2010-2014,2017 Wind River Systems, Inc.
 * Copyright (c) 2025 Siemens AG
 * Copyright (c) 2021 Intel Corporation.
 */

/**
 * @file
 * @brief Compiler capabilities needed by EmBT declarations and utilities.
 *
 * C11 is required for C consumers, and C++11 for C++ consumers. Current packed
 * declarations require GCC-compatible attributes, supported by GCC and Clang.
 * This header provides no OS, assembly, linker-script, or startup framework.
 */

#ifndef EMBT_COMPILER_H_
#define EMBT_COMPILER_H_

#include <stdbool.h>

#if !defined(__GNUC__) && !defined(__clang__)
#error "Provide EmBT packed and alignment attributes for this compiler"
#endif

/** @brief Report a builtin capability, or select the caller's portable fallback. */
#ifdef __has_builtin
#define HAS_BUILTIN(name) __has_builtin(name)
#else
#define HAS_BUILTIN(name) 0
#endif

/** @brief Require a constant expression at compile time; the message is optional. */
#ifdef __cplusplus
#define BUILD_ASSERT(expr, ...) static_assert((expr), "" __VA_ARGS__)
#else
#define BUILD_ASSERT(expr, ...) _Static_assert((expr), "" __VA_ARGS__)
#endif

/** @brief Keep wire-format structures free of implicit padding. */
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

/** @brief Preserve explicit object and type alignment at existing declaration sites. */
#ifndef __aligned
#define __aligned(bytes) __attribute__((__aligned__(bytes)))
#endif

/** @brief Mark compiler-retained storage without creating a runtime registry. */
#define __used __attribute__((__used__))

/** @brief Allow declarations used only by selected configurations. */
#define __unused __attribute__((__unused__))
#define __maybe_unused __attribute__((__unused__))

/** @brief Keep the existing optional link-time override contract. */
#ifndef __weak
#define __weak __attribute__((__weak__))
#endif

/** @brief Preserve existing public API deprecation attributes. */
#ifndef __deprecated
#define __deprecated __attribute__((deprecated))
#endif

/** @brief Diagnose ignored results where the existing API requires a check. */
#ifndef __must_check
#define __must_check __attribute__((warn_unused_result))
#endif

/** @brief Mark a function whose result depends only on its arguments. */
#ifndef __attribute_const__
#define __attribute_const__ __attribute__((__const__))
#endif

/**
 * @brief Keep current printf declaration annotations without adding diagnostics.
 *
 * These annotations were inactive in the supported configurations before this
 * extraction. Enabling format checking is a separate diagnostic change.
 */
#define __printf_like(format_index, first_arg)

/** @brief Retain source alias annotations without restoring a retired Kconfig. */
#define __DEPRECATED_MACRO

/** @brief Retain the existing inline requirement. */
#define ALWAYS_INLINE inline __attribute__((always_inline))

/** @brief Mark functions and paths that do not return. */
#define FUNC_NORETURN __attribute__((__noreturn__))
#define CODE_UNREACHABLE __builtin_unreachable()

/** @brief Retain branch prediction and population-count hints. */
#define unlikely(value) (__builtin_expect((bool)!!(value), false) != 0L)
#define POPCOUNT(value) __builtin_popcount(value)

/** @brief Place explicitly declared metadata in its requested section. */
#define __section(name) __attribute__((section(name)))

/** @brief Prevent compiler reordering across this point; not a CPU memory fence. */
#define compiler_barrier() \
	do { \
		__asm__ __volatile__("" ::: "memory"); \
	} while (0)

/** @brief Annotate an intentional fall-through when supported by the compiler. */
#ifndef __fallthrough
#if defined(__clang__)
#if __has_attribute(fallthrough)
#define __fallthrough __attribute__((fallthrough))
#else
#define __fallthrough
#endif
#elif __GNUC__ >= 7
#define __fallthrough __attribute__((fallthrough))
#else
#define __fallthrough
#endif
#endif

/** @brief Suppress one Clang diagnostic around an existing declaration. */
#if defined(__clang__)
#define EMBT_PRAGMA(value) _Pragma(#value)
#define EMBT_CLANG_DIAGNOSTIC_IGNORE(warning) \
	EMBT_PRAGMA(clang diagnostic push) \
	EMBT_PRAGMA(clang diagnostic ignored warning)
#define EMBT_CLANG_DIAGNOSTIC_POP() EMBT_PRAGMA(clang diagnostic pop)
#else
#define EMBT_CLANG_DIAGNOSTIC_IGNORE(warning)
#define EMBT_CLANG_DIAGNOSTIC_POP()
#endif

#endif /* EMBT_COMPILER_H_ */
