#include "support/test_support.h"
/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stddef.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/gatt.h>

#include "gatt_internal.h"

/* Custom Service Variables */
static const struct bt_uuid_128 test_uuid = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);
static const struct bt_uuid_128 test_chrc_uuid = BT_UUID_INIT_128(
	0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static uint8_t test_value[] = { 'T', 'e', 's', 't', '\0' };

static const struct bt_uuid_128 test1_uuid = BT_UUID_INIT_128(
	0xf4, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const struct bt_uuid_128 test1_nfy_uuid = BT_UUID_INIT_128(
	0xf5, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static uint8_t nfy_enabled;

static void test1_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	nfy_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t test1_ccc_cfg_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				      uint16_t value)
{
	return sizeof(value);
}

static ssize_t read_test(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, uint16_t len, uint16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t write_test(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, uint16_t len, uint16_t offset,
			 uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(test_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

static struct bt_gatt_attr test_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&test_uuid),

	BT_GATT_CHARACTERISTIC(&test_chrc_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ_AUTHEN |
			       BT_GATT_PERM_WRITE_AUTHEN,
			       read_test, write_test, test_value),
};

static struct bt_gatt_service test_svc = BT_GATT_SERVICE(test_attrs);

static struct bt_gatt_attr test1_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&test1_uuid),

	BT_GATT_CHARACTERISTIC(&test1_nfy_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
			       NULL, NULL, &nfy_enabled),
	BT_GATT_CCC(test1_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service test1_svc = BT_GATT_SERVICE(test1_attrs);



static void test_gatt__test_gatt_register(void **cm_state)
{
	(void)cm_state;
	/* Ensure our test services are not already registered */
	bt_gatt_service_unregister(&test_svc);
	bt_gatt_service_unregister(&test1_svc);

	/* Attempt to register services */
	assert_false(bt_gatt_service_register(&test_svc));
	assert_false(bt_gatt_service_register(&test1_svc));

	/* Attempt to register already registered services */
	assert_true(bt_gatt_service_register(&test_svc));
	assert_true(bt_gatt_service_register(&test1_svc));
}

static void test_gatt__test_gatt_unregister(void **cm_state)
{
	(void)cm_state;
	/* Attempt to unregister last */
	assert_false(bt_gatt_service_unregister(&test1_svc));
	assert_false(bt_gatt_service_register(&test1_svc));

	/* Attempt to unregister first/middle */
	assert_false(bt_gatt_service_unregister(&test_svc));
	assert_false(bt_gatt_service_register(&test_svc));

	/* Attempt to unregister all reverse order */
	assert_false(bt_gatt_service_unregister(&test1_svc));
	assert_false(bt_gatt_service_unregister(&test_svc));

	assert_false(bt_gatt_service_register(&test_svc));
	assert_false(bt_gatt_service_register(&test1_svc));

	/* Attempt to unregister all same order */
	assert_false(bt_gatt_service_unregister(&test_svc));
	assert_false(bt_gatt_service_unregister(&test1_svc));
}

/* Test that a service A can be re-registered after registering it once, unregistering it, and then
 * registering another service B.
 * No pre-allocated handles. Repeat the process multiple times.
 */
static void test_gatt__test_gatt_reregister(void **cm_state)
{
	(void)cm_state;
	struct bt_gatt_attr local_test_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test_uuid),

		BT_GATT_CHARACTERISTIC(&test_chrc_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
				       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
				       read_test, write_test, test_value),
	};

	struct bt_gatt_attr local_test1_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test1_uuid),

		BT_GATT_CHARACTERISTIC(&test1_nfy_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
				       NULL, NULL, &nfy_enabled),
		BT_GATT_CCC(test1_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	};
	struct bt_gatt_service local_test_svc = BT_GATT_SERVICE(local_test_attrs);
	struct bt_gatt_service local_test1_svc = BT_GATT_SERVICE(local_test1_attrs);

	/* Check that the procedure is successful for a few iterations to verify stability and
	 * detect residual state or memory issues.
	 */
	for (int i = 0; i < 10; i++) {

		/* Check that the handles are initially 0x0000 before registering the service */
		for (int j = 0; j < local_test_svc.attr_count; j++) {
			assert_int_equal(local_test_svc.attrs[j].handle, 0x0000);
		}

		assert_false(bt_gatt_service_register(&local_test_svc));

		assert_false(bt_gatt_service_unregister(&local_test_svc));

		/* Check that the handles are the same as before registering the service */
		for (int j = 0; j < local_test_svc.attr_count; j++) {
			assert_int_equal(local_test_svc.attrs[j].handle, 0x0000);
		}

		assert_false(bt_gatt_service_register(&local_test1_svc));

		assert_false(bt_gatt_service_register(&local_test_svc));

		/* Clean up */
		assert_false(bt_gatt_service_unregister(&local_test_svc));
		assert_false(bt_gatt_service_unregister(&local_test1_svc));
	}
}

/* Test that a service A can be re-registered after registering it once, unregistering it, and then
 * registering another service B.
 * Service A and B both have pre-allocated handles for their attributes.
 * Check that pre-allocated handles are the same after unregistering as they were before
 * registering the service.
 */
static void test_gatt__test_gatt_reregister_pre_allocated_handles(void **cm_state)
{
	(void)cm_state;
	struct bt_gatt_attr local_test_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test_uuid),

		BT_GATT_CHARACTERISTIC(&test_chrc_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
				       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
				       read_test, write_test, test_value),
	};

	struct bt_gatt_attr local_test1_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test1_uuid),

		BT_GATT_CHARACTERISTIC(&test1_nfy_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
				       NULL, NULL, &nfy_enabled),
		BT_GATT_CCC(test1_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	};

	struct bt_gatt_service prealloc_test_svc = BT_GATT_SERVICE(local_test_attrs);
	struct bt_gatt_service prealloc_test1_svc = BT_GATT_SERVICE(local_test1_attrs);

	/* Pre-allocate handles for both services */
	for (int i = 0; i < prealloc_test_svc.attr_count; i++) {
		prealloc_test_svc.attrs[i].handle = 0x0100 + i;
	}
	for (int i = 0; i < prealloc_test1_svc.attr_count; i++) {
		prealloc_test1_svc.attrs[i].handle = 0x0200 + i;
	}

	assert_false(bt_gatt_service_register(&prealloc_test_svc));

	assert_false(bt_gatt_service_unregister(&prealloc_test_svc));

	/* Check that the handles are the same as before registering the service */
	for (int i = 0; i < prealloc_test_svc.attr_count; i++) {
		assert_int_equal(prealloc_test_svc.attrs[i].handle, 0x0100 + i);
	}

	assert_false(bt_gatt_service_register(&prealloc_test1_svc));

	assert_false(bt_gatt_service_register(&prealloc_test_svc));

	/* Clean up */
	assert_false(bt_gatt_service_unregister(&prealloc_test_svc));
	assert_false(bt_gatt_service_unregister(&prealloc_test1_svc));
}

/* Test that a service A can be re-registered after registering it once, unregistering it, and then
 * registering another service B.
 * Service A has pre-allocated handles for its attributes, while Service B has handles assigned by
 * the stack when registered.
 * Check that pre-allocated handles are the same after unregistering as they were before
 * registering the service.
 */
static void test_gatt__test_gatt_reregister_pre_allocated_handle_single(void **cm_state)
{
	(void)cm_state;
	struct bt_gatt_attr local_test_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test_uuid),

		BT_GATT_CHARACTERISTIC(&test_chrc_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
				       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
				       read_test, write_test, test_value),
	};

	struct bt_gatt_attr local_test1_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test1_uuid),

		BT_GATT_CHARACTERISTIC(&test1_nfy_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
				       NULL, NULL, &nfy_enabled),
		BT_GATT_CCC(test1_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	};

	struct bt_gatt_service prealloc_test_svc = BT_GATT_SERVICE(local_test_attrs);
	struct bt_gatt_service auto_test_svc = BT_GATT_SERVICE(local_test1_attrs);

	/* Pre-allocate handles for one service only */
	for (int j = 0; j < prealloc_test_svc.attr_count; j++) {
		prealloc_test_svc.attrs[j].handle = 0x0100 + j;
	}

	assert_false(bt_gatt_service_register(&prealloc_test_svc));

	assert_false(bt_gatt_service_unregister(&prealloc_test_svc));

	/* Check that the handles are the same as before registering the service */
	for (int i = 0; i < prealloc_test_svc.attr_count; i++) {
		assert_int_equal(prealloc_test_svc.attrs[i].handle, 0x0100 + i);
	}

	assert_false(bt_gatt_service_register(&auto_test_svc));

	assert_false(bt_gatt_service_register(&prealloc_test_svc));

	/* Clean up */
	assert_false(bt_gatt_service_unregister(&prealloc_test_svc));
	assert_false(bt_gatt_service_unregister(&auto_test_svc));
}

static uint8_t count_attr(const struct bt_gatt_attr *attr, uint16_t handle,
			  void *user_data)
{
	uint16_t *count = user_data;

	(*count)++;

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t find_attr(const struct bt_gatt_attr *attr, uint16_t handle,
			 void *user_data)
{
	const struct bt_gatt_attr **tmp = user_data;

	*tmp = attr;

	return BT_GATT_ITER_CONTINUE;
}

static void test_gatt__test_gatt_foreach(void **cm_state)
{
	(void)cm_state;
	const struct bt_gatt_attr *attr;
	uint16_t num = 0;

	/* Attempt to register services */
	assert_false(bt_gatt_service_register(&test_svc));
	assert_false(bt_gatt_service_register(&test1_svc));

	/* Iterate attributes */
	bt_gatt_foreach_attr(test_attrs[0].handle, 0xffff, count_attr, &num);
	assert_int_equal(num, 7);

	/* Iterate 1 attribute */
	num = 0;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff, NULL, NULL, 1,
				  count_attr, &num);
	assert_int_equal(num, 1);

	/* Find attribute by UUID */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  &test_chrc_uuid.uuid, NULL, 0, find_attr,
				  &attr);
	assert_non_null(attr);
	if (attr) {
		assert_int_equal(attr->uuid, &test_chrc_uuid.uuid);
	}

	/* Find attribute by DATA */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff, NULL,
				  test_value, 0, find_attr, &attr);
	assert_non_null(attr);
	if (attr) {
		assert_int_equal(attr->user_data, test_value);
	}

	/* Find all characteristics */
	num = 0;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  BT_UUID_GATT_CHRC, NULL, 0, count_attr, &num);
	assert_int_equal(num, 2);

	/* Find 1 characteristic */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  BT_UUID_GATT_CHRC, NULL, 1, find_attr, &attr);
	assert_non_null(attr);

	/* Find attribute by UUID and DATA */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  &test1_nfy_uuid.uuid, &nfy_enabled, 1,
				  find_attr, &attr);
	assert_non_null(attr);
	if (attr) {
		assert_int_equal(attr->uuid, &test1_nfy_uuid.uuid);
		assert_int_equal(attr->user_data, &nfy_enabled);
	}
}

static void test_gatt__test_gatt_read(void **cm_state)
{
	(void)cm_state;
	const struct bt_gatt_attr *attr;
	uint8_t buf[256];
	ssize_t ret;

	/* Find attribute by UUID */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  &test_chrc_uuid.uuid, NULL, 0, find_attr,
				  &attr);
	assert_non_null(attr);
	assert_int_equal(attr->uuid, &test_chrc_uuid.uuid);

	ret = attr->read(NULL, attr, (void *)buf, sizeof(buf), 0);
	assert_int_equal(ret, strlen(test_value));
	assert_memory_equal(buf, test_value, ret);
}

static void test_gatt__test_gatt_write(void **cm_state)
{
	(void)cm_state;
	const struct bt_gatt_attr *attr;
	char *value = "    ";
	ssize_t ret;

	/* The preceding ordered cases leave the service registered. Recreate the
	 * original write-case precondition explicitly.
	 */
	(void)bt_gatt_service_unregister(&test_svc);
	assert_false(bt_gatt_service_register(&test_svc));

	/* Find attribute by UUID */
	attr = NULL;
	bt_gatt_foreach_attr_type(test_attrs[0].handle, 0xffff,
				  &test_chrc_uuid.uuid, NULL, 0, find_attr,
				  &attr);
	assert_non_null(attr);

	ret = attr->write(NULL, attr, (void *)value, strlen(value), 0, 0);
	assert_int_equal(ret, strlen(value));
	assert_memory_equal(value, test_value, ret);
}

static void test_gatt__test_bt_att_err_to_str(void **cm_state)
{
	(void)cm_state;
	/* Test a couple of entries */
	assert_string_equal(bt_att_err_to_str(BT_ATT_ERR_SUCCESS), "BT_ATT_ERR_SUCCESS");
	assert_string_equal(bt_att_err_to_str(BT_ATT_ERR_INSUFFICIENT_ENCRYPTION), "BT_ATT_ERR_INSUFFICIENT_ENCRYPTION");
	assert_string_equal(bt_att_err_to_str(BT_ATT_ERR_OUT_OF_RANGE), "BT_ATT_ERR_OUT_OF_RANGE");

	/* Test a entries that is not used */
	assert_memory_equal(bt_att_err_to_str(0x14), "(unknown)", strlen("(unknown)"));
	assert_memory_equal(bt_att_err_to_str(0xFB), "(unknown)", strlen("(unknown)"));

	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_att_err_to_str(i));
	}
}

static void test_gatt__test_bt_gatt_err_to_str(void **cm_state)
{
	(void)cm_state;
	/* Test a couple of entries */
	assert_string_equal(bt_gatt_err_to_str(BT_GATT_ERR(BT_ATT_ERR_SUCCESS)), "BT_ATT_ERR_SUCCESS");
	assert_string_equal(bt_gatt_err_to_str(BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_ENCRYPTION)), "BT_ATT_ERR_INSUFFICIENT_ENCRYPTION");
	assert_string_equal(bt_gatt_err_to_str(BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE)), "BT_ATT_ERR_OUT_OF_RANGE");

	/* Test entries that are not used */
	assert_memory_equal(bt_gatt_err_to_str(BT_GATT_ERR(0x14)), "(unknown)", strlen("(unknown)"));
	assert_memory_equal(bt_gatt_err_to_str(BT_GATT_ERR(0xFB)), "(unknown)", strlen("(unknown)"));

	/* Test positive values */
	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_gatt_err_to_str(i));
	}

	/* Test negative values */
	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_gatt_err_to_str(-i));
	}
}

static void test_gatt__test_gatt_ccc_write_cb(void **cm_state)
{
	(void)cm_state;
	struct bt_gatt_attr test_write_cb_attrs[] = {
		/* Vendor Primary Service Declaration */
		BT_GATT_PRIMARY_SERVICE(&test1_uuid),

		BT_GATT_CHARACTERISTIC(&test1_nfy_uuid.uuid,
				       BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE,
				       NULL, NULL, &nfy_enabled),
		BT_GATT_CCC_WITH_WRITE_CB(test1_ccc_cfg_changed,
			test1_ccc_cfg_write_cb,
			BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
	};

	struct bt_gatt_service test_write_cb_svc = BT_GATT_SERVICE(test_write_cb_attrs);

	assert_false(bt_gatt_service_register(&test_write_cb_svc));
	assert_false(bt_gatt_service_unregister(&test_write_cb_svc));
}

/*
 * Only Mbed TLS exposes the PSA key slot count. With TF-M providing the PSA
 * core the count is not visible to the non-secure image.
 */
#if defined(CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT)
#define TEST_PSA_KEY_SLOT_COUNT CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT
#else
#define TEST_PSA_KEY_SLOT_COUNT 0
#endif

/*
 * Test for PSA key memory leak
 *
 * Using dynamic registration/deregistration to test for PSA key memory leak.
 * Each dynamic registration/deregistration clears DB_HASH_VALID and causes the db_hash work to be
 * rescheduled; if db_hash_gen fails, the flag stays clear; otherwise the flag is set. If PSA
 * memory is not released, either dynamic registration/deregistration fails or the flag check
 * asserts.
 * osi_sleep_ms(BT_MSEC(settle_ms)) is used to wait for the db_hash work to complete
 * The presence of memory leak does not depend on the PSA memory type (heap or static slot).
 * CONFIG_MBEDTLS_PSA_STATIC_KEY_SLOTS=y is used to fail quickly with fewer iterations in case of a
 * memory leak. The iteration count is set to CONFIG_MBEDTLS_PSA_KEY_SLOT_COUNT + 1: slightly
 * larger than slot_count, assuming each iteration leaks at least one key.
 */
static void test_gatt__test_gatt_db_hash_psa_dynamic_register_no_leak(void **cm_state)
{
	(void)cm_state;

	if (!IS_ENABLED(CONFIG_BT_GATT_CACHING) || TEST_PSA_KEY_SLOT_COUNT == 0) {
		skip();
	}

	/*
	 * Using only SLOT_COUNT+1 iterations to test for a memory leak, assuming each iteration
	 * leaks at least one key, which guarantees that we will hit PSA_ERROR_INSUFFICIENT_MEMORY
	 * with +1 after all slots are occupied by leakages.
	 */
	int iterations = TEST_PSA_KEY_SLOT_COUNT + 1;

	/*
	 * Wait for the db_hash work to complete; DB_HASH_TIMEOUT is 10ms so 30ms should be enough
	 * as a very conservative wait time.
	 */
	int settle_ms = 30;

	/* Same layout as test_attrs / test_svc (reuse UUIDs and callbacks). */
	struct bt_gatt_attr dyn_attrs[] = {
		BT_GATT_PRIMARY_SERVICE(&test_uuid),
		BT_GATT_CHARACTERISTIC(&test_chrc_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
				       BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN,
				       read_test, write_test, test_value),
	};
	struct bt_gatt_service dyn_svc = BT_GATT_SERVICE(dyn_attrs);

	/* We need to call bt_gatt_init() to set GATT_INITIALIZED which locks DB hash behaviour */
	bt_gatt_init();

	/* Ensure our test services are not already registered */
	int err = bt_gatt_service_unregister(&dyn_svc);

	assert_true(err == 0 || err == -ENOENT);

	for (int i = 0; i < iterations; i++) {
		err = bt_gatt_service_register(&dyn_svc);
		assert_true(err == 0);
		osi_sleep_ms(BT_MSEC(settle_ms));
		assert_true(bt_gatt_is_db_hash_valid());

		err = bt_gatt_service_unregister(&dyn_svc);
		assert_true(err == 0);
		osi_sleep_ms(BT_MSEC(settle_ms));
		assert_true(bt_gatt_is_db_hash_valid());
	}
}





static const struct CMUnitTest test_cases_test_gatt[] = {
	cmocka_unit_test(test_gatt__test_gatt_register),
	cmocka_unit_test(test_gatt__test_gatt_unregister),
	cmocka_unit_test(test_gatt__test_gatt_reregister),
	cmocka_unit_test(test_gatt__test_gatt_reregister_pre_allocated_handles),
	cmocka_unit_test(test_gatt__test_gatt_reregister_pre_allocated_handle_single),
	cmocka_unit_test(test_gatt__test_gatt_foreach),
	cmocka_unit_test(test_gatt__test_gatt_read),
	cmocka_unit_test(test_gatt__test_gatt_write),
	cmocka_unit_test(test_gatt__test_bt_att_err_to_str),
	cmocka_unit_test(test_gatt__test_bt_gatt_err_to_str),
	cmocka_unit_test(test_gatt__test_gatt_ccc_write_cb),
	cmocka_unit_test(test_gatt__test_gatt_db_hash_psa_dynamic_register_no_leak),
};
BT_TEST_SUITE(test_gatt, test_cases_test_gatt, bt_test_host_setup, NULL);
