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

/* Copyright (c) 2018 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file @brief Actual Mesh settings read helper for isolated replay and bridge tests. */
#include "support/test_support.h"
#include <embt/base/log.h>
#include <embt/bluetooth/mesh.h>
#include <embt/settings/settings.h>
#include "mesh.h"
#include "net.h"
#include "settings.h"
LOG_MODULE_REGISTER(mesh_test_settings_reader, LOG_LEVEL_ERR);
struct bt_mesh_net bt_mesh;
int bt_mesh_settings_set(settings_read_cb read_cb, void *cb_arg,
			 void *out, size_t read_len)
{
	ssize_t len;

	len = read_cb(cb_arg, out, read_len);
	if (len < 0) {
		LOG_ERR("Failed to read value (err %zd)", len);
		return len;
	}

	LOG_HEXDUMP_DBG(out, len, "val");

	if (len != read_len) {
		LOG_ERR("Unexpected value length (%zd != %zu)", len, read_len);
		return -EINVAL;
	}

	return 0;
}
