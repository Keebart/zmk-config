"""Studio-only readiness bridge for the pinned ZMK split transport.

Lighting must not start its own ATT requests before native discovery and CCC
acknowledgements finish. Callbacks publish atomics only; no new Bluetooth work.
"""
import pathlib,sys
source=pathlib.Path(sys.argv[1]).read_text()
def replace(old,new):
 global source
 if source.count(old)!=1: raise SystemExit('Pinned ZMK split source changed: '+old[:70])
 source=source.replace(old,new,1)
replace('struct peripheral_slot {\n', '''struct peripheral_slot {
    atomic_t keebart_ready;
''')
needle='int release_peripheral_slot(int index) {'
bridge='''// Private Studio readiness, separate from merely connected/encrypted.
// Native discovery can enqueue several CCC operations from Bluetooth RX.
// Keep the lighting client silent until all of those operations acknowledge.
#define KS_SPLIT_DISCOVERED BIT(0)
#define KS_SPLIT_POSITIONS BIT(1)
#define KS_SPLIT_SENSORS BIT(2)
static void keebart_split_subscribed(struct bt_conn *conn, uint8_t err,
                                    struct bt_gatt_subscribe_params *params) {
    struct peripheral_slot *slot = peripheral_slot_for_conn(conn);
    if (!slot) return;
    unsigned bit;
    if (params == &slot->subscribe_params) bit = 1;
#if ZMK_KEYMAP_HAS_SENSORS
    else if (params == &slot->sensor_subscribe_params) bit = 2;
#endif
    else return;
    if (err) atomic_clear_bit(&slot->keebart_ready, bit);
    else atomic_set_bit(&slot->keebart_ready, bit);
}
bool keebart_split_ready(struct bt_conn *conn) {
    struct peripheral_slot *slot = peripheral_slot_for_conn(conn);
    unsigned required = KS_SPLIT_DISCOVERED | KS_SPLIT_POSITIONS;
#if ZMK_KEYMAP_HAS_SENSORS
    required |= KS_SPLIT_SENSORS;
#endif
    return slot && (atomic_get(&slot->keebart_ready) & required) == required;
}

'''
replace(needle,bridge+needle)
replace('    LOG_DBG("Releasing peripheral slot at %d", index);','''    atomic_clear(&slot->keebart_ready);
    LOG_DBG("Releasing peripheral slot at %d", index);''')
replace('''static int split_central_subscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params) {
''','''static int split_central_subscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params) {
    params->subscribe = keebart_split_subscribed;
''')
replace('    int err = bt_gatt_subscribe(conn, params);', '    int err = bt_gatt_subscribe(conn, params);\n    if (err == -EALREADY && params->ccc_handle &&\n        !atomic_test_bit(params->flags, BT_GATT_SUBSCRIBE_FLAG_WRITE_PENDING))\n        keebart_split_subscribed(conn, 0, params);')
# Zephyr does not deliver a final NULL callback after BT_GATT_ITER_STOP.
# Mark the successful early-stop path, not exhaustion/error with missing handles.
replace('    return subscribed ? BT_GATT_ITER_STOP : BT_GATT_ITER_CONTINUE;',
        '    if (subscribed) atomic_set_bit(&slot->keebart_ready, 0);\n'
        '    return subscribed ? BT_GATT_ITER_STOP : BT_GATT_ITER_CONTINUE;')

# Prevent permanent scan deadlock on transient HCI errors or stop_scanning() failures.
replace(
    'static int stop_scanning(void) {\n'
    '    LOG_DBG("Stopping peripheral scanning");\n'
    '    is_scanning = false;',
    'static void ks_scan_retry_cb(struct k_work *work) {\n'
    '    ARG_UNUSED(work);\n'
    '    start_scanning();\n'
    '}\n'
    '\n'
    'static K_WORK_DELAYABLE_DEFINE(ks_scan_retry_work, ks_scan_retry_cb);\n'
    '\n'
    'static int stop_scanning(void) {\n'
    '    LOG_DBG("Stopping peripheral scanning");\n'
    '    k_work_cancel_delayable(&ks_scan_retry_work);\n'
    '    is_scanning = false;',
)
replace(
    '    // Stop scanning so we can connect to the peripheral device.\n'
    '    int err = stop_scanning();\n'
    '    if (err < 0) {\n'
    '        return false;\n'
    '    }',
    '    // Stop scanning so we can connect to the peripheral device.\n'
    '    int err = stop_scanning();\n'
    '    if (err < 0) {\n'
    '        release_peripheral_slot(slot_idx);\n'
    '        start_scanning();\n'
    '        return false;\n'
    '    }',
)
replace(
    '    // Start scanning otherwise.\n'
    '    is_scanning = true;\n'
    '    int err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, split_central_device_found);\n'
    '    if (err < 0) {\n'
    '        LOG_ERR("Scanning failed to start (err %d)", err);\n'
    '        return err;\n'
    '    }',
    '    // Start scanning otherwise.\n'
    '    k_work_cancel_delayable(&ks_scan_retry_work);\n'
    '    is_scanning = true;\n'
    '    int err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, split_central_device_found);\n'
    '    if (err < 0) {\n'
    '        LOG_ERR("Scanning failed to start (err %d)", err);\n'
    '        if (err != -EALREADY) {\n'
    '            is_scanning = false;\n'
    '            k_work_reschedule(&ks_scan_retry_work, K_MSEC(500));\n'
    '        }\n'
    '        return err;\n'
    '    }',
)
pathlib.Path(sys.argv[2]).write_text(source)
