#include "support/test_support.h"

/* main.c - Application main entry point */

/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <embt/bluetooth/att.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/pacs.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/uuid.h>
#include <fff.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>
DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(mock_pacs_location_cb, struct bt_conn *, enum bt_audio_dir,
	       enum bt_audio_location);

static struct bt_pacs_cb mock_pacs_cb = {
	.location = mock_pacs_location_cb,
};

static void self_unregistering_location_cb(struct bt_conn *conn, enum bt_audio_dir dir,
					   enum bt_audio_location location);

static struct bt_pacs_cb self_unregistering_pacs_cb = {
	.location = self_unregistering_location_cb,
};

static void self_unregistering_location_cb(struct bt_conn *conn, enum bt_audio_dir dir,
					   enum bt_audio_location location)
{
	int err;

	err = bt_pacs_unregister_cb(&self_unregistering_pacs_cb);
	assert_int_equal(err, 0);
}

static void pacs_test_suite_before(void *f)
{
	ARG_UNUSED(f);

	RESET_FAKE(mock_pacs_location_cb);
}

static void pacs_test_suite_after(void *f)
{
	int err;

	ARG_UNUSED(f);

	/* attempt to clean up after any failures */
	err = bt_pacs_unregister_cb(&mock_pacs_cb);
	assert_true(err == 0 || err == -ENOENT);

	err = bt_pacs_unregister_cb(&self_unregistering_pacs_cb);
	assert_true(err == 0 || err == -ENOENT);

	err = bt_pacs_unregister();
	assert_true(err == 0 || err == -EALREADY);
}



/* Helper macro to define parameters ignoring unsupported features */
#define PACS_REGISTER_PARAM(_snk_pac, _snk_loc, _src_pac, _src_loc)                                \
	(struct bt_pacs_register_param)                                                            \
	{                                                                                          \
		IF_ENABLED(CONFIG_BT_PAC_SNK, (.snk_pac = (_snk_pac),))                            \
		IF_ENABLED(CONFIG_BT_PAC_SNK_LOC, (.snk_loc = (_snk_loc),))                        \
		IF_ENABLED(CONFIG_BT_PAC_SRC, (.src_pac = (_src_pac),))                            \
		IF_ENABLED(CONFIG_BT_PAC_SRC_LOC, (.src_loc = (_src_loc),))                        \
	}

static void pacs_test_suite__test_pacs_register(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_params[] = {
#if defined(CONFIG_BT_PAC_SNK)
		/* valid snk_pac combinations */
		PACS_REGISTER_PARAM(true, true, true, true),
		PACS_REGISTER_PARAM(true, true, true, false),
		PACS_REGISTER_PARAM(true, true, false, false),
		PACS_REGISTER_PARAM(true, false, true, true),
		PACS_REGISTER_PARAM(true, false, true, false),
		PACS_REGISTER_PARAM(true, false, false, false),
#endif /* CONFIG_BT_PAC_SNK */

#if defined(CONFIG_BT_PAC_SRC)
		/* valid src_pac combinations */
		PACS_REGISTER_PARAM(true, true, true, true),
		PACS_REGISTER_PARAM(true, false, true, true),
		PACS_REGISTER_PARAM(false, false, true, true),
		PACS_REGISTER_PARAM(true, true, true, false),
		PACS_REGISTER_PARAM(true, false, true, false),
		PACS_REGISTER_PARAM(false, false, true, false),
#endif /* CONFIG_BT_PAC_SRC */
	};

	for (size_t i = 0U; i < ARRAY_SIZE(pacs_params); i++) {
		struct bt_gatt_attr *attr;
		int err;

		err = bt_pacs_register(&pacs_params[i]);
		assert_int_equal(err, 0);

#if defined(CONFIG_BT_PAC_SNK)
		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SNK);
		if (pacs_params[i].snk_pac) {
			assert_non_null(attr);
		} else {
			assert_null(attr);
		}
#endif /*CONFIG_BT_PAC_SNK */
#if defined(CONFIG_BT_PAC_SNK_LOC)
		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SNK_LOC);
		if (pacs_params[i].snk_loc) {
			assert_non_null(attr);
		} else {
			assert_null(attr);
		}
#endif /*CONFIG_BT_PAC_SNK_LOC */
#if defined(CONFIG_BT_PAC_SRC)
		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SRC);
		if (pacs_params[i].src_pac) {
			assert_non_null(attr);
		} else {
			assert_null(attr);
		}
#endif /*CONFIG_BT_PAC_SRC */
#if defined(CONFIG_BT_PAC_SRC_LOC)
		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SRC_LOC);
		if (pacs_params[i].src_loc) {
			assert_non_null(attr);
		} else {
			assert_null(attr);
		}
#endif /*CONFIG_BT_PAC_SRC_LOC */

		err = bt_pacs_unregister();
		assert_int_equal(err, 0);

		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SNK);
		assert_null(attr);

		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SNK_LOC);
		assert_null(attr);

		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SRC);
		assert_null(attr);

		attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_PACS_SRC_LOC);
		assert_null(attr);
	}
}

struct chrc_search {
	const struct bt_uuid *uuid;
	const struct bt_gatt_attr *decl;
};

static uint8_t chrc_search_cb(const struct bt_gatt_attr *attr, uint16_t handle, void *user_data)
{
	struct chrc_search *search = user_data;
	const struct bt_gatt_chrc *chrc = attr->user_data;

	if (bt_uuid_cmp(chrc->uuid, search->uuid) == 0) {
		search->decl = attr;

		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

static void check_notify_property(const struct bt_uuid *uuid, bool notifiable)
{
	struct chrc_search search = {.uuid = uuid};
	const struct bt_gatt_attr *value;
	const struct bt_gatt_attr *next;
	const struct bt_gatt_chrc *chrc;
	bool has_cccd;

	bt_gatt_foreach_attr_type(BT_ATT_FIRST_ATTRIBUTE_HANDLE, BT_ATT_LAST_ATTRIBUTE_HANDLE,
				  BT_UUID_GATT_CHRC, NULL, 0, chrc_search_cb, &search);
	assert_non_null(search.decl);

	chrc = search.decl->user_data;
	value = bt_gatt_attr_next(search.decl);
	assert_non_null(value);
	next = bt_gatt_attr_next(value);
	has_cccd = next != NULL && bt_uuid_cmp(next->uuid, BT_UUID_GATT_CCC) == 0;

	assert_int_equal(has_cccd, notifiable);
	assert_int_equal((chrc->properties & BT_GATT_CHRC_NOTIFY) != 0U, notifiable);
}

static void pacs_test_suite__test_pacs_pac_notify_property(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	if (IS_ENABLED(CONFIG_BT_PAC_SNK)) {
		check_notify_property(BT_UUID_PACS_SNK, IS_ENABLED(CONFIG_BT_PAC_SNK_NOTIFIABLE));
	}

	if (IS_ENABLED(CONFIG_BT_PAC_SRC)) {
		check_notify_property(BT_UUID_PACS_SRC, IS_ENABLED(CONFIG_BT_PAC_SRC_NOTIFIABLE));
	}
}

static void pacs_test_suite__test_pacs_register_inval_null_param(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_register(NULL);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_register_inval_double_register(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, -EALREADY);
}

static void pacs_test_suite__test_pacs_register_inval_snk_loc_without_snk_pac(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(false, true, true, true);
	int err;

	if (!(IS_ENABLED(CONFIG_BT_PAC_SNK) && IS_ENABLED(CONFIG_BT_PAC_SNK_LOC))) {
		skip();
	}

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_register_inval_src_loc_without_src_pac(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, false, true);
	int err;

	if (!(IS_ENABLED(CONFIG_BT_PAC_SRC) && IS_ENABLED(CONFIG_BT_PAC_SRC_LOC))) {
		skip();
	}

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_register_inval_no_pac(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(false, false, false, false);
	int err;

	if (!(IS_ENABLED(CONFIG_BT_PAC_SNK) && IS_ENABLED(CONFIG_BT_PAC_SNK_LOC))) {
		skip();
	}

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_register_cb(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_register_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);

	err = bt_pacs_unregister_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);
}

static void pacs_test_suite__test_pacs_register_cb_inval_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_register_cb(NULL);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_register_cb_inval_double_register(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_register_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);

	err = bt_pacs_register_cb(&mock_pacs_cb);
	assert_int_equal(err, -EEXIST);
}

static void pacs_test_suite__test_pacs_unregister_cb_inval_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_unregister_cb(NULL);
	assert_int_equal(err, -EINVAL);
}

static void pacs_test_suite__test_pacs_unregister_cb_inval_not_registered(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_pacs_unregister_cb(&mock_pacs_cb);
	assert_int_equal(err, -ENOENT);
}

static void pacs_test_suite__test_pacs_get_location_inval(void **cm_state)
{
	(void)cm_state;
	enum bt_audio_location location;
	int err;

	err = bt_pacs_get_location(BT_AUDIO_DIR_SINK, NULL);
	assert_int_equal(err, -EINVAL);

	err = bt_pacs_get_location((enum bt_audio_dir)0, &location);
	assert_int_equal(err, -EINVAL);

	if (!IS_ENABLED(CONFIG_BT_PAC_SNK_LOC)) {
		err = bt_pacs_get_location(BT_AUDIO_DIR_SINK, &location);
		assert_int_equal(err, -EINVAL);
	}

	if (!IS_ENABLED(CONFIG_BT_PAC_SRC_LOC)) {
		err = bt_pacs_get_location(BT_AUDIO_DIR_SOURCE, &location);
		assert_int_equal(err, -EINVAL);
	}
}

static void test_get_location(enum bt_audio_dir dir)
{
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	enum bt_audio_location location;
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	err = bt_pacs_set_location(dir, BT_AUDIO_LOCATION_FRONT_RIGHT);
	assert_int_equal(err, 0);

	err = bt_pacs_get_location(dir, &location);
	assert_int_equal(err, 0);
	assert_int_equal(location, BT_AUDIO_LOCATION_FRONT_RIGHT);

	assert_int_equal(mock_pacs_location_cb_fake.call_count, 0);
}

static void pacs_test_suite__test_pacs_get_location_snk(void **cm_state)
{
	(void)cm_state;
	if (!IS_ENABLED(CONFIG_BT_PAC_SNK_LOC)) {
		skip();
	}

	assert_int_equal(bt_pacs_register_cb(&mock_pacs_cb), 0);
	test_get_location(BT_AUDIO_DIR_SINK);
}

static void pacs_test_suite__test_pacs_get_location_src(void **cm_state)
{
	(void)cm_state;
	if (!IS_ENABLED(CONFIG_BT_PAC_SRC_LOC)) {
		skip();
	}

	assert_int_equal(bt_pacs_register_cb(&mock_pacs_cb), 0);
	test_get_location(BT_AUDIO_DIR_SOURCE);
}

static void pacs_test_suite__test_pacs_get_location_inval_not_registered(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, false, true, false);
	enum bt_audio_location location = BT_AUDIO_LOCATION_FRONT_LEFT;
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	err = bt_pacs_get_location(BT_AUDIO_DIR_SINK, &location);
	assert_int_equal(err, IS_ENABLED(CONFIG_BT_PAC_SNK_LOC) ? -ENOENT : -EINVAL);

	err = bt_pacs_get_location(BT_AUDIO_DIR_SOURCE, &location);
	assert_int_equal(err, IS_ENABLED(CONFIG_BT_PAC_SRC_LOC) ? -ENOENT : -EINVAL);
}

static void pacs_test_suite__test_pacs_get_location_inval_unregistered(void **cm_state)
{
	(void)cm_state;
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	enum bt_audio_location location;
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	if (IS_ENABLED(CONFIG_BT_PAC_SNK_LOC)) {
		err = bt_pacs_get_location(BT_AUDIO_DIR_SINK, &location);
		assert_int_equal(err, 0);
	}

	if (IS_ENABLED(CONFIG_BT_PAC_SRC_LOC)) {
		err = bt_pacs_get_location(BT_AUDIO_DIR_SOURCE, &location);
		assert_int_equal(err, 0);
	}

	err = bt_pacs_unregister();
	assert_int_equal(err, 0);

	err = bt_pacs_get_location(BT_AUDIO_DIR_SINK, &location);
	assert_int_equal(err, IS_ENABLED(CONFIG_BT_PAC_SNK_LOC) ? -ENOENT : -EINVAL);

	err = bt_pacs_get_location(BT_AUDIO_DIR_SOURCE, &location);
	assert_int_equal(err, IS_ENABLED(CONFIG_BT_PAC_SRC_LOC) ? -ENOENT : -EINVAL);
}

#if defined(CONFIG_BT_PAC_SNK_LOC_WRITEABLE) || defined(CONFIG_BT_PAC_SRC_LOC_WRITEABLE)
static ssize_t write_location(const struct bt_uuid *uuid, uint32_t location)
{
	const struct bt_gatt_attr *attr = bt_gatt_find_by_uuid(NULL, 0, uuid);
	uint8_t buf[sizeof(location)];

	assert_non_null(attr);
	assert_non_null(attr->write);

	sys_put_le32(location, buf);

	return attr->write(NULL, attr, buf, sizeof(buf), 0, 0);
}

static void test_location_write(enum bt_audio_dir dir, const struct bt_uuid *uuid)
{
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	enum bt_audio_location location;
	ssize_t ret;
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	err = bt_pacs_set_location(dir, BT_AUDIO_LOCATION_FRONT_LEFT);
	assert_int_equal(err, 0);

	err = bt_pacs_register_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);

	ret = write_location(uuid, BT_AUDIO_LOCATION_FRONT_RIGHT);
	assert_int_equal(ret, sizeof(uint32_t));

	assert_int_equal(mock_pacs_location_cb_fake.call_count, 1);
	assert_null(mock_pacs_location_cb_fake.arg0_val);
	assert_int_equal(mock_pacs_location_cb_fake.arg1_val, dir);
	assert_int_equal(mock_pacs_location_cb_fake.arg2_val, BT_AUDIO_LOCATION_FRONT_RIGHT);

	err = bt_pacs_get_location(dir, &location);
	assert_int_equal(err, 0);
	assert_int_equal(location, BT_AUDIO_LOCATION_FRONT_RIGHT);

	ret = write_location(uuid, BT_AUDIO_LOCATION_FRONT_RIGHT);
	assert_int_equal(ret, sizeof(uint32_t));
	assert_int_equal(mock_pacs_location_cb_fake.call_count, 1);

	ret = write_location(uuid, BIT(28));
	assert_int_equal(ret, BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED));
	assert_int_equal(mock_pacs_location_cb_fake.call_count, 1);

	err = bt_pacs_unregister_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);

	ret = write_location(uuid, BT_AUDIO_LOCATION_FRONT_LEFT);
	assert_int_equal(ret, sizeof(uint32_t));
	assert_int_equal(mock_pacs_location_cb_fake.call_count, 1);
}

static void test_location_write_unregister_in_cb(enum bt_audio_dir dir, const struct bt_uuid *uuid)
{
	const struct bt_pacs_register_param pacs_param =
		PACS_REGISTER_PARAM(true, true, true, true);
	ssize_t ret;
	int err;

	err = bt_pacs_register(&pacs_param);
	assert_int_equal(err, 0);

	err = bt_pacs_set_location(dir, BT_AUDIO_LOCATION_FRONT_LEFT);
	assert_int_equal(err, 0);

	err = bt_pacs_register_cb(&self_unregistering_pacs_cb);
	assert_int_equal(err, 0);

	err = bt_pacs_register_cb(&mock_pacs_cb);
	assert_int_equal(err, 0);

	ret = write_location(uuid, BT_AUDIO_LOCATION_FRONT_RIGHT);
	assert_int_equal(ret, sizeof(uint32_t));
	assert_int_equal(mock_pacs_location_cb_fake.call_count, 1);

	err = bt_pacs_unregister_cb(&self_unregistering_pacs_cb);
	assert_int_equal(err, -ENOENT);
}
#endif /* CONFIG_BT_PAC_SNK_LOC_WRITEABLE || CONFIG_BT_PAC_SRC_LOC_WRITEABLE */

#if defined(CONFIG_BT_PAC_SNK_LOC_WRITEABLE)
static void pacs_test_suite__test_pacs_snk_loc_write_calls_cb(void **cm_state)
{
	(void)cm_state;
	test_location_write(BT_AUDIO_DIR_SINK, BT_UUID_PACS_SNK_LOC);
}

static void pacs_test_suite__test_pacs_snk_loc_write_cb_unregisters_itself(void **cm_state)
{
	(void)cm_state;
	test_location_write_unregister_in_cb(BT_AUDIO_DIR_SINK, BT_UUID_PACS_SNK_LOC);
}
#endif /* CONFIG_BT_PAC_SNK_LOC_WRITEABLE */

#if defined(CONFIG_BT_PAC_SRC_LOC_WRITEABLE)
static void pacs_test_suite__test_pacs_src_loc_write_calls_cb(void **cm_state)
{
	(void)cm_state;
	test_location_write(BT_AUDIO_DIR_SOURCE, BT_UUID_PACS_SRC_LOC);
}

static void pacs_test_suite__test_pacs_src_loc_write_cb_unregisters_itself(void **cm_state)
{
	(void)cm_state;
	test_location_write_unregister_in_cb(BT_AUDIO_DIR_SOURCE, BT_UUID_PACS_SRC_LOC);
}
#endif /* CONFIG_BT_PAC_SRC_LOC_WRITEABLE */


static int bt_test_pacs_test_suite_before(void **state)
{
	(void)state;
	pacs_test_suite_before(*state);
	return 0;
}
static int bt_test_pacs_test_suite_after(void **state)
{
	(void)state;
	pacs_test_suite_after(*state);
	return 0;
}

static const struct CMUnitTest test_cases_pacs_test_suite[] = {
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_pac_notify_property, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_inval_null_param, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_inval_double_register, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_inval_snk_loc_without_snk_pac, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_inval_src_loc_without_src_pac, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_inval_no_pac, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_cb, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_cb_inval_null, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_register_cb_inval_double_register, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_unregister_cb_inval_null, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_unregister_cb_inval_not_registered, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_get_location_inval, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_get_location_snk, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_get_location_src, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_get_location_inval_not_registered, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_get_location_inval_unregistered, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
#if (defined(CONFIG_BT_PAC_SNK_LOC_WRITEABLE))
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_snk_loc_write_calls_cb, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
#endif
#if (defined(CONFIG_BT_PAC_SNK_LOC_WRITEABLE))
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_snk_loc_write_cb_unregisters_itself, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
#endif
#if (defined(CONFIG_BT_PAC_SRC_LOC_WRITEABLE))
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_src_loc_write_calls_cb, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
#endif
#if (defined(CONFIG_BT_PAC_SRC_LOC_WRITEABLE))
	cmocka_unit_test_setup_teardown(pacs_test_suite__test_pacs_src_loc_write_cb_unregisters_itself, bt_test_pacs_test_suite_before, bt_test_pacs_test_suite_after),
#endif
};
BT_TEST_SUITE(pacs_test_suite, test_cases_pacs_test_suite, bt_test_host_setup, NULL);
