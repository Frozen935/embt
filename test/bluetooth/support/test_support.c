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

/** @file @brief Explicit cmocka groups with process-level failure isolation. */
#include "test_support.h"
#include <embt/base/base.h>
#include <embt/bluetooth/bluetooth.h>
#if defined(CONFIG_BT_HCI_RAW)
#include "host/hci_raw_internal.h"
#endif
#include <psa/crypto.h>
void bt_test_require(int result)
{
	if (result) {
		fprintf(stderr, "Bluetooth test fixture failed: %d\n", result);
		exit(1);
	}
}

int bt_test_initialize(void)
{
	bt_test_runtime_initialize();
	int result = bt_base_init();
#if defined(CONFIG_PSA_CRYPTO)
	if (!result && psa_crypto_init() != PSA_SUCCESS) {
		result = -EIO;
	}
#endif
	return result;
}

int bt_test_base_setup(void **state)
{
	(void)state;
	return bt_test_initialize();
}

int bt_test_host_setup(void **state)
{
	int result = bt_test_base_setup(state);

#if defined(CONFIG_BT_HCI_RAW)
	return result ? result : -ENOTSUP;
#else
	return result ? result : bt_stack_init();
#endif
}

int bt_test_buffer_setup(void **state)
{
#if defined(CONFIG_BT_HCI_RAW)
	int result = bt_test_base_setup(state);

	if (!result) { bt_hci_raw_init(); }
	return result;
#else
	return bt_test_host_setup(state);
#endif
}

/* Preserve the failed operation without a platform-specific stack unwinder. */
void __real_bt_base_require(int result);
void __wrap_bt_base_require(int result)
{
	if (result != 0) {
		fprintf(stderr, "Base operation failed: %d\n", result);
	}
	__real_bt_base_require(result);
}
