/** @file @brief Explicit initialization of selected standalone GATT services. */

#include <errno.h>

int bt_services_init(void)
{
	int err = 0;

#if defined(CONFIG_BT_ANS)
	{
		extern int bt_ans_base_init(void);

		err = bt_ans_base_init();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_HRS)
	{
		extern int bt_hrs_base_init(void);

		err = bt_hrs_base_init();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_BAS)
	{
		extern int bt_bas_base_init(void);

		err = bt_bas_base_init();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_HIDS)
	{
		extern int bt_hids_base_init(void);

		err = bt_hids_base_init();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_OTS)
	{
		extern int bt_gatt_ots_instances_prepare(void);

		err = bt_gatt_ots_instances_prepare();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_OTS) || defined(CONFIG_BT_OTS_CLIENT)
	{
		extern int bt_gatt_ots_l2cap_init(void);

		err = bt_gatt_ots_l2cap_init();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_ANS)
	{
		extern int bt_service_ans_register(void);

		err = bt_service_ans_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_BAS)
	{
		extern int bt_service_bas_bas_register(void);

		err = bt_service_bas_bas_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_CTS)
	{
		extern int bt_service_cts_register(void);

		err = bt_service_cts_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_ETS)
	{
		extern int bt_service_ets_register(void);

		err = bt_service_ets_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_HRS)
	{
		extern int bt_service_hrs_register(void);

		err = bt_service_hrs_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_TPS)
	{
		extern int bt_service_tps_register(void);

		err = bt_service_tps_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_IAS)
	{
		extern int bt_service_ias_ias_register(void);

		err = bt_service_ias_ias_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_DIS)
	{
		extern int bt_service_dis_register(void);

		err = bt_service_dis_register();
		if (err) {
			return err;
		}
	}
#endif
#if defined(CONFIG_BT_ZEPHYR_NUS)
	{
		extern int bt_nus_base_init(void);

		err = bt_nus_base_init();
	}
#endif
#if defined(CONFIG_BT_OTS) || defined(CONFIG_BT_OTS_CLIENT)
	{
		extern void bt_services_ots_ots_l2cap_storage_init(void);

		bt_services_ots_ots_l2cap_storage_init();
	}
#endif
#if defined(CONFIG_BT_OTS)
	{
		extern int bt_services_ots_ots_callbacks_init(void);

		err = bt_services_ots_ots_callbacks_init();
		if (err) {
			return err;
		}
	}
#endif
	return err;
}
