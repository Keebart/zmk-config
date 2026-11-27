"""Studio-only advertising retry bridge for the pinned ZMK split peripheral.

On Zephyr 3.5 with CONFIG_BT_MAX_CONN=1, disconnect cleanup on sysworkq can
attempt to restart directed advertising before service_work_q releases its
temporary bt_conn reference, returning -EINVAL/-ENOMEM. Upstream ZMK v0.3 logs
the error and never retries, leaving the peripheral silent until power-cycled.
"""
import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()


def replace(old, new):
    global source
    if source.count(old) != 1:
        raise SystemExit("Pinned ZMK split peripheral source changed: " + old[:70])
    source = source.replace(old, new, 1)


replace(
    """static void advertising_cb(struct k_work *work) {
    const int err = start_advertising(low_duty_advertising);
    if (err < 0) {
        LOG_ERR("Failed to start advertising (%d)", err);
    }
}

K_WORK_DEFINE(advertising_work, advertising_cb);""",
    """static void advertising_cb(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(advertising_work, advertising_cb);

static void advertising_cb(struct k_work *work) {
    ARG_UNUSED(work);
    if (!enabled || is_connected) {
        return;
    }

    const int err = start_advertising(low_duty_advertising);
    if (err < 0) {
        LOG_ERR("Failed to start advertising (%d)", err);
        if (err != -EALREADY) {
            k_work_reschedule(&advertising_work, K_MSEC(200));
        }
    }
}""",
)

replace(
    """static void connected(struct bt_conn *conn, uint8_t err) {
    is_connected = (err == 0);

    raise_zmk_split_peripheral_status_changed(
        (struct zmk_split_peripheral_status_changed){.connected = is_connected});

    if (err == BT_HCI_ERR_ADV_TIMEOUT && enabled) {
        low_duty_advertising = true;
        k_work_submit(&advertising_work);
    }
}""",
    """static void connected(struct bt_conn *conn, uint8_t err) {
    is_connected = (err == 0);
    if (is_connected) {
        k_work_cancel_delayable(&advertising_work);
    }

    raise_zmk_split_peripheral_status_changed(
        (struct zmk_split_peripheral_status_changed){.connected = is_connected});

    if (err == BT_HCI_ERR_ADV_TIMEOUT && enabled) {
        low_duty_advertising = true;
        k_work_reschedule(&advertising_work, K_NO_WAIT);
    }
}""",
)

replace(
    """    if (enabled) {
        low_duty_advertising = false;
        k_work_submit(&advertising_work);
    }""",
    """    if (enabled) {
        low_duty_advertising = false;
        k_work_reschedule(&advertising_work, K_NO_WAIT);
    }""",
)

replace(
    """    enabled = en;
    if (en) {
        k_work_submit(&advertising_work);
        return 0;
    } else {
        struct bt_conn *conn = NULL;""",
    """    enabled = en;
    if (en) {
        k_work_reschedule(&advertising_work, K_NO_WAIT);
        return 0;
    } else {
        k_work_cancel_delayable(&advertising_work);
        struct bt_conn *conn = NULL;""",
)

pathlib.Path(sys.argv[2]).write_text(source)
