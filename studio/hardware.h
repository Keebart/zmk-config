#pragma once
#if defined(CONFIG_BOARD_SOFLE_CHOC_PRO_LEFT) || defined(CONFIG_BOARD_SOFLE_CHOC_PRO_RIGHT) ||     \
    defined(KS_MODEL_SOFLE)
#define KS_SOFLE 1
#define KS_KEYS 60
#define KS_ENCODERS 2
#define KS_UID "KBSFBT01"
#define KS_UNLOCK_RIGHT 11
#define KS_BOOTMAGIC_LEFT 0
#define KS_BOOTMAGIC_RIGHT 11
#elif defined(CONFIG_BOARD_PIANTOR_PRO_BT_LEFT) || defined(CONFIG_BOARD_PIANTOR_PRO_BT_RIGHT) ||   \
    defined(KS_MODEL_PIANTOR)
#define KS_PIANTOR 1
#define KS_KEYS 42
#define KS_ENCODERS 0
#define KS_UID "KBPNBT01"
#define KS_UNLOCK_RIGHT 11
#define KS_BOOTMAGIC_LEFT 1
#define KS_BOOTMAGIC_RIGHT 10
#else
#define KS_CORNE 1
#define KS_KEYS 46
#define KS_ENCODERS 4
#define KS_UID "KBCCBT01"
#define KS_UNLOCK_RIGHT 13
#define KS_BOOTMAGIC_LEFT 1
#define KS_BOOTMAGIC_RIGHT 12
#endif
#define KS_HALF_LEDS (KS_KEYS / 2)
#define KS_ENCODER_SLOTS (KS_ENCODERS ? KS_ENCODERS : 1)

#if defined(KS_SOFLE)
#define KS_MODEL_ID 0x0305
#elif defined(KS_PIANTOR)
#define KS_MODEL_ID 0x0306
#else
#define KS_MODEL_ID 0x0304
#endif
