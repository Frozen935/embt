/*
 * Copyright (c) 2019, NXP
 * Copyright (c) 2025, HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <base/base.h>
#include <errno.h>
#include <embt/random/random.h>


#include <psa/crypto.h>

int sys_csrand_get(void *dst, size_t outlen)
{
	return psa_generate_random((uint8_t *)dst, outlen) == PSA_SUCCESS ? 0 : -EIO;
}

void sys_rand_get(void *dst, size_t len)
{
	if (sys_csrand_get(dst, len) != 0) {
		osi_fatal("PSA random generator failed");
	}
}
