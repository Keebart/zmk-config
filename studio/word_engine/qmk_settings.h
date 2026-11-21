#pragma once
#include <stdint.h>
const uint8_t *ks_typing_config(void);
#define QMK_SETTINGS
#define QS_auto_shift_enable ((ks_typing_config()[0]&0x80)!=0)
#define QS_auto_shift_modifiers 0
#define QS_auto_shift_no_auto_shift_special ((ks_typing_config()[0]&12)==0)
#define QS_auto_shift_no_auto_shift_numeric ((ks_typing_config()[0]&2)==0)
#define QS_auto_shift_no_auto_shift_alpha ((ks_typing_config()[0]&1)==0)
#define QS_auto_shift_repeat 0
#define QS_auto_shift_no_auto_repeat 1
#define QS_tap_code_delay ((ks_typing_config()[14]&2)?(ks_typing_config()[22]|(uint16_t)ks_typing_config()[23]<<8):0)
