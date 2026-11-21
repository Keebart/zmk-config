// Persistent geometry is an ABI: moving it strands Bluetooth identities/bonds.
// Keep this in BOTH normal Studio roles, independent of the USB adapter.
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#if defined(CONFIG_ZMK_SETTINGS_RESET_ON_START) || defined(CONFIG_ZMK_BLE_CLEAR_BONDS_ON_START)
#error "Normal Studio firmware must never erase settings or bonds on boot"
#endif
#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
#error "A full bond table must not evict an existing host or split pairing"
#endif
#if !defined(CONFIG_SETTINGS_NVS) || !defined(CONFIG_BT_SETTINGS)
#error "Studio must retain Bluetooth settings in NVS"
#endif
#if defined(CONFIG_KEEBART_STUDIO) == defined(CONFIG_KEEBART_STUDIO_PERIPHERAL)
#error "Select exactly one Studio half role"
#endif

#if defined(CONFIG_KEEBART_STUDIO)
#define KS_STORAGE_START 0xe4000
#define KS_STORAGE_SIZE 0x10000
#define KS_STORAGE_SECTORS 16
#if !defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "Studio USB configuration belongs on the central half"
#endif
BUILD_ASSERT(CONFIG_BT_MAX_PAIRED >= 6, "Retain space for five hosts and the split peer");
#else
#define KS_STORAGE_START 0xec000
#define KS_STORAGE_SIZE 0x8000
#define KS_STORAGE_SECTORS 8
#if defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#error "Studio peripheral firmware must retain its peripheral role"
#endif
BUILD_ASSERT(CONFIG_BT_MAX_PAIRED == 1, "Peripheral retains exactly one central bond");
#endif

BUILD_ASSERT(DT_REG_ADDR(DT_NODELABEL(storage_partition)) == KS_STORAGE_START &&
             DT_REG_SIZE(DT_NODELABEL(storage_partition)) == KS_STORAGE_SIZE,
             "Do not relocate Studio storage: it contains Bluetooth identities and bonds");
BUILD_ASSERT(CONFIG_SETTINGS_NVS_SECTOR_COUNT == KS_STORAGE_SECTORS &&
             CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT == 1 &&
             DT_PROP(DT_NODELABEL(flash0), erase_block_size) == 4096,
             "Do not change the persistent NVS sector geometry");
BUILD_ASSERT(DT_REG_ADDR(DT_NODELABEL(code_partition)) == 0x26000 &&
             DT_REG_SIZE(DT_NODELABEL(code_partition)) == KS_STORAGE_START - 0x26000,
             "Application must not overlap SoftDevice or persistent storage");
BUILD_ASSERT(DT_REG_ADDR(DT_NODELABEL(boot_partition)) == 0xf4000,
             "Keep the recovery bootloader outside application and settings writes");
BUILD_ASSERT(CONFIG_BT_ID_MAX == 1, "Preserve the existing Bluetooth identity index");
BUILD_ASSERT(DT_SAME_NODE(DT_CHOSEN(zephyr_code_partition), DT_NODELABEL(code_partition)),
             "The linker must use the guarded code partition");
#if DT_HAS_CHOSEN(zephyr_settings_partition)
BUILD_ASSERT(DT_SAME_NODE(DT_CHOSEN(zephyr_settings_partition), DT_NODELABEL(storage_partition)),
             "The settings backend must use the guarded storage partition");
#endif
#if !defined(CONFIG_SOC_FLASH_NRF_RADIO_SYNC_TICKER) || !defined(CONFIG_SOC_FLASH_NRF_PARTIAL_ERASE)
#error "Keep radio-synchronized partial flash erase enabled"
#endif
#if defined(CONFIG_BT_CTLR_ASSERT_OVERHEAD_START)
#error "Disable controller assert overhead start to preserve radio timing"
#endif
BUILD_ASSERT(CONFIG_SOC_FLASH_NRF_PARTIAL_ERASE_MS == 2, "Keep partial flash erase window bounded to 2 ms");
