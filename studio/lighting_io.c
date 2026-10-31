#include "release.h"
#include "runtime.h"
#include "render.h"
#include "frames.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>
#include <drivers/ext_power.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/atomic.h>
#include <string.h>
#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#define KS_LED_UUID BT_UUID_128_ENCODE(0xa56f8b22, 0x1605, 0x4f65, 0x9479, 0x6b6565626172)
#define KS_SERVICE_UUID BT_UUID_128_ENCODE(0xa56f8b21, 0x1605, 0x4f65, 0x9479, 0x6b6565626172)
static const struct device *const strip = DEVICE_DT_GET(DT_CHOSEN(zmk_underglow));
static const struct device *const power = DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
K_SEM_DEFINE(lighting_wake, 0, 1);
static void show(const uint8_t colors[KS_HALF_LEDS][3]) {
    BUILD_ASSERT(DT_PROP(DT_CHOSEN(zmk_underglow), chain_length) == KS_HALF_LEDS);
    struct led_rgb pixels[KS_HALF_LEDS];
    for (unsigned i = 0; i < KS_HALF_LEDS; i++)
        pixels[i] = (struct led_rgb){.r = colors[i][0], .g = colors[i][1], .b = colors[i][2]};
    led_strip_update_rgb(strip, pixels, KS_HALF_LEDS);
}
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
static struct bt_uuid_128 led_uuid = BT_UUID_INIT_128(KS_LED_UUID);
/* This existing ZMK symbol identifies the bonded split peripheral, excluding host BLE links. */
extern int peripheral_slot_index_for_conn(struct bt_conn *);
extern bool keebart_split_ready(struct bt_conn *);
// Only the lighting thread owns/refcounts peer and starts GATT operations.
// Bluetooth callbacks publish bounded results and never allocate TX buffers.
static struct bt_conn *peer;
static atomic_ptr_t callback_peer;
static atomic_t candidate_handle, reading_release;
static int64_t release_retry, release_deadline, discovery_deadline, discovery_retry, mtu_retry;
static atomic_t handle;
static atomic_t peer_version;
static atomic_t peer_schema;
static struct bt_gatt_read_params release_read;
void ks_peer_release(uint8_t out[12]) {
    uint32_t version=(uint32_t)atomic_get(&peer_version);
    memset(out,0,12);
    if(!version) return;
    keebart_release(out,KS_MODEL_ID,2,(uint8_t)atomic_get(&peer_schema));
    for(unsigned i=0;i<4;i++)out[4+i]=(uint8_t)(version>>(8*i));
    if(version!=(uint32_t)atomic_get(&peer_version)) memset(out,0,12);
}
static uint8_t release_received(struct bt_conn *conn,uint8_t err,struct bt_gatt_read_params *params,
                               const void *data,uint16_t length) {
    if(conn!=atomic_ptr_get(&callback_peer)) return BT_GATT_ITER_STOP;
    if(!err && data && keebart_release_matches(data,length,KS_MODEL_ID,2)) {
        const uint8_t *p=data;
        uint32_t version=(uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16)|((uint32_t)p[7]<<24);
        atomic_set(&peer_schema,p[11]);atomic_set(&peer_version,version);
    }
    atomic_clear(&reading_release);
    k_sem_give(&lighting_wake);
    return BT_GATT_ITER_STOP;
}

static atomic_t discovering;
static struct bt_gatt_discover_params discovery;
static atomic_t mtu_requested;
static int64_t parameter_retry;
static void mtu_exchanged(struct bt_conn *conn, uint8_t err,
                          struct bt_gatt_exchange_params *params) {
    if (conn == atomic_ptr_get(&callback_peer)) {
        if(err)atomic_clear(&mtu_requested);
        k_sem_give(&lighting_wake);
    }
}
static struct bt_gatt_exchange_params mtu_exchange = {.func = mtu_exchanged};
static uint8_t found(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                     struct bt_gatt_discover_params *params) {
    if (conn != atomic_ptr_get(&callback_peer))
        return BT_GATT_ITER_STOP;
    if (!attr) {
        atomic_clear(&discovering);
        return BT_GATT_ITER_STOP;
    }
    const struct bt_gatt_chrc *ch = attr->user_data;
    if (bt_uuid_cmp(ch->uuid, &led_uuid.uuid) == 0) {
        // Do not issue a read from the Bluetooth RX callback: allocating an
        // ATT TX buffer here can wait for this same thread to release buffers.
        atomic_set(&candidate_handle, ch->value_handle);
        k_sem_give(&lighting_wake);
        atomic_clear(&discovering);
        return BT_GATT_ITER_STOP;
    }
    return BT_GATT_ITER_CONTINUE;
}
static void find_peer(struct bt_conn *conn, void *arg) {
    struct bt_conn_info info;
    if (!*(struct bt_conn **)arg && !bt_conn_get_info(conn, &info) &&
        info.state == BT_CONN_STATE_CONNECTED && bt_conn_get_security(conn) >= BT_SECURITY_L2 &&
        peripheral_slot_index_for_conn(conn) >= 0 && keebart_split_ready(conn))
        *(struct bt_conn **)arg = bt_conn_ref(conn);
}
static bool maintain_peer(bool streaming) {
    struct bt_conn *next = NULL;
    bt_conn_foreach(BT_CONN_TYPE_LE, find_peer, &next);
    bool changed = next != peer;
    if (changed) {
        atomic_ptr_set(&callback_peer,NULL);
        atomic_clear(&peer_version);atomic_clear(&peer_schema);
        if (peer) {
            if (atomic_get(&discovering))
                bt_gatt_cancel(peer, &discovery);
            bt_gatt_cancel(peer, &mtu_exchange);
            bt_gatt_cancel(peer, &release_read);
            bt_conn_unref(peer);
        }
        peer = next;
        atomic_clear(&candidate_handle);atomic_clear(&reading_release);
        release_retry=release_deadline=discovery_deadline=discovery_retry=mtu_retry=0;
        atomic_ptr_set(&callback_peer,peer);
        atomic_clear(&mtu_requested);
        parameter_retry = 0;
        atomic_clear(&handle);
        atomic_clear(&discovering);
        memset(&discovery, 0, sizeof(discovery));
    } else if (next)
        bt_conn_unref(next);
    if (!peer)
        return changed;
    int64_t now=k_uptime_get();
    if (!atomic_get(&mtu_requested) && now>=mtu_retry) {
        int err = bt_gatt_exchange_mtu(peer, &mtu_exchange);
        atomic_set(&mtu_requested,!err || err==-EALREADY);
        mtu_retry=now+1000;
    }
    // Serialize our own startup as well: discovery starts only after the MTU
    // transaction has completed, never alongside native split subscriptions.
    if (bt_gatt_get_mtu(peer)<7+KS_HALF_LEDS*3) return changed;
    // Keep the split radio responsive while LEDs are running, then restore
    // ZMK's power-saving latency when lighting is off or the keyboard sleeps.
    struct bt_conn_info info;
    unsigned latency = streaming ? 0 : CONFIG_ZMK_SPLIT_BLE_PREF_LATENCY;
    if (k_uptime_get() >= parameter_retry && !bt_conn_get_info(peer, &info) &&
        info.le.latency != latency) {
        bt_conn_le_param_update(peer, BT_LE_CONN_PARAM(CONFIG_ZMK_SPLIT_BLE_PREF_INT,
                                                       CONFIG_ZMK_SPLIT_BLE_PREF_INT, latency,
                                                       CONFIG_ZMK_SPLIT_BLE_PREF_TIMEOUT));
        parameter_retry = k_uptime_get() + 2000;
    }
    if (atomic_get(&discovering) && now>=discovery_deadline) {
        bt_gatt_cancel(peer,&discovery);atomic_clear(&discovering);
        discovery_retry=now+1000;
    }
    if (!atomic_get(&candidate_handle) && now>=discovery_retry) {
        if (atomic_cas(&discovering, 0, 1)) {
            discovery_deadline=now+3000;discovery_retry=now+1000;
            discovery = (struct bt_gatt_discover_params){.uuid = &led_uuid.uuid,
                                                         .func = found,
                                                         .start_handle = 1,
                                                         .end_handle = 0xffff,
                                                         .type = BT_GATT_DISCOVER_CHARACTERISTIC};
            if (bt_gatt_discover(peer, &discovery))
                atomic_clear(&discovering);
        }
    }
    if (atomic_get(&reading_release) && now>=release_deadline) {
        bt_gatt_cancel(peer,&release_read);atomic_clear(&reading_release);
        release_retry=now+250;
    }
    if (atomic_get(&candidate_handle) && !atomic_get(&peer_version)
        && !atomic_get(&reading_release) && now>=release_retry) {
        memset(&release_read,0,sizeof(release_read));
        release_read.func=release_received;release_read.handle_count=1;
        release_read.single.handle=atomic_get(&candidate_handle);
        atomic_set(&reading_release,1);release_deadline=now+3000;release_retry=now+250;
        if (bt_gatt_read(peer,&release_read))atomic_clear(&reading_release);
    }
    if ((uint32_t)atomic_get(&peer_version)==KEEBART_RELEASE_ID && atomic_get(&peer_schema)==7)
        atomic_set(&handle,atomic_get(&candidate_handle));
    return changed;
}
static bool send_frame(const uint8_t colors[KS_HALF_LEDS][3], uint16_t sequence) {
    if (!peer || !atomic_get(&handle) || bt_gatt_get_mtu(peer)<7+KS_HALF_LEDS*3)
        return false;
    // Wait for the full-frame MTU. Sending six startup fragments can exhaust
    // the ATT pool while ZMK is still establishing its key subscriptions.
    // One ATT write carries all KS_HALF_LEDS LEDs after MTU negotiation. Respect the
    // negotiated MTU even during connection setup; never overrun the link.
    for (unsigned start = 0; start < KS_HALF_LEDS;) {
        uint8_t packet[KS_FRAME_PACKET_SIZE];
        size_t len = ks_frame_packet(colors, sequence, start, bt_gatt_get_mtu(peer), packet);
        if (!len || bt_gatt_write_without_response(peer, atomic_get(&handle), packet, len, false))
            return false;
        start += packet[3];
    }
    return true;
}
#else
static struct ks_frames frames;
K_MUTEX_DEFINE(frame_mutex);
static void disconnected(struct bt_conn *conn, uint8_t reason) {
    // A restarted central begins a new sequence. Do not reject its first frames
    // as older than the previous connection's counter.
    k_mutex_lock(&frame_mutex, K_FOREVER);
    ks_frames_disconnect(&frames);
    k_mutex_unlock(&frame_mutex);
    k_sem_give(&lighting_wake);
}
BT_CONN_CB_DEFINE(keebart_lighting_connection) = {.disconnected = disconnected};
static int peripheral_activity(const zmk_event_t *event) {
    k_sem_give(&lighting_wake);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(keebart_lighting_activity, peripheral_activity);
ZMK_SUBSCRIPTION(keebart_lighting_activity, zmk_activity_state_changed);
static ssize_t receive(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                       uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset || (flags & BT_GATT_WRITE_FLAG_PREPARE) ||
        bt_conn_get_security(conn) < BT_SECURITY_L2)
        return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
    k_mutex_lock(&frame_mutex, K_FOREVER);
    int result = ks_frame_receive(&frames, buf, len, k_uptime_get());
    k_mutex_unlock(&frame_mutex);
    if (result == 1)
        k_sem_give(&lighting_wake);
    return result < 0 ? BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED) : len;
}
static ssize_t read_release(struct bt_conn *conn,const struct bt_gatt_attr *attr,void *buf,
                            uint16_t length,uint16_t offset) {
    uint8_t record[12];keebart_release(record,KS_MODEL_ID,2,8);
    return bt_gatt_attr_read(conn,attr,buf,length,offset,record,sizeof(record));
}
BT_GATT_SERVICE_DEFINE(keebart_led_service,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(KS_SERVICE_UUID)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(KS_LED_UUID),
                                              BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_READ,
                                              BT_GATT_PERM_WRITE_ENCRYPT | BT_GATT_PERM_READ_ENCRYPT, read_release, receive, NULL));
#endif
static void run(void *a, void *b, void *c) {
    k_sleep(K_MSEC(1500));
    if (!device_is_ready(strip) || !device_is_ready(power))
        return;
    ext_power_enable(power);
    uint8_t previous[KS_HALF_LEDS][3] = {0};
    show(previous);
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
    uint8_t previous_right[KS_HALF_LEDS][3] = {0};
    int64_t sent = 0;
    uint16_t sequence = 0;
    bool right_sent = false;
#else
    k_mutex_lock(&frame_mutex, K_FOREVER);
    // A frame may already have arrived during device startup.
    if (!frames.has_display) ks_frames_init(&frames);
    k_mutex_unlock(&frame_mutex);
#endif
    for (;;) {
        uint8_t colors[KS_HALF_LEDS][3];
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
        uint8_t all[KS_KEYS][3], right[KS_HALF_LEDS][3];
        bool streaming = ks_frame(all);
        if (maintain_peer(streaming))
            right_sent = false;
        for (unsigned i = 0; i < KS_HALF_LEDS; i++) {
            memcpy(colors[i], all[ks_led_positions[i]], 3);
            memcpy(right[i], all[ks_led_positions[i + KS_HALF_LEDS]], 3);
        }
        if ((!right_sent || memcmp(previous_right, right, sizeof(right)) ||
             k_uptime_get() - sent >= 500) &&
            send_frame(right, ++sequence)) {
            // Failed/discovery-pending transmissions retry on the next frame,
            // rather than falsely caching success and waiting 500 ms.
            memcpy(previous_right, right, sizeof(right));
            sent = k_uptime_get();
            right_sent = true;
        }
#else
        k_mutex_lock(&frame_mutex, K_FOREVER);
        ks_frames_output(&frames, zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE,
                         k_uptime_get(), colors);
        k_mutex_unlock(&frame_mutex);
#endif
        if (memcmp(previous, colors, sizeof(colors))) {
            show(colors);
            memcpy(previous, colors, sizeof(colors));
        }
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
        k_sem_take(&lighting_wake, K_MSEC(50));
#else
        // Render immediately when a complete frame arrives. The timeout only
        // checks the standalone idle state when central frames stop arriving.
        k_sem_take(&lighting_wake, K_MSEC(1500));
#endif
    }
}
K_THREAD_DEFINE(keebart_lighting_thread, 4096, run, NULL, NULL, NULL, 8, 0, 0);
