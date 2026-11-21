#include "word_bridge.h"
#include "override_bridge.h"
#include "locales/locale.h"
#include "word_engine/caps_word.h"
#include "word_engine/process_caps_word.h"
#include "word_engine/process_auto_shift.h"
#include <limits.h>
#include <zephyr/kernel.h>

extern const uint8_t *ks_locale_record(void);
extern unsigned ks_locale_computer(void);
extern bool ks_locale_resolved_action(void);
extern uint8_t ks_locale_mods(void);
extern bool ks_locale_character_press(uint16_t, uint16_t, uint8_t);
extern void ks_locale_character_release(uint16_t);
extern void ks_locale_character_emit(uint16_t, uint8_t);
extern void ks_override_prepare(void);
extern void ks_word_begin(void);
extern void ks_word_schedule(void);
extern void register_code16(uint16_t);
extern void unregister_code(uint8_t);

static int64_t due;

const uint8_t *ks_typing_config(void) {
    return ksl_typing(ks_locale_record());
}

void unregister_code16(uint16_t code) {
    unregister_code(code & 0xff);
}

bool ks_word_process(uint16_t code, bool down) {
    ks_override_prepare();
    keyrecord_t record = {.event = {.pressed = down, .time = (uint16_t)k_uptime_get()}};
    bool pass = process_caps_word(code, &record);
    due = is_caps_word_on() ? k_uptime_get() + CAPS_WORD_IDLE_TIMEOUT : 0;
    return pass;
}

bool ks_auto_shift_process(uint16_t code, bool down) {
    ks_override_prepare();
    keyrecord_t record = {.event = {.pressed = down, .time = (uint16_t)k_uptime_get()}};
    return process_auto_shift(code, &record);
}

void ks_word_tick(void) {
    if (!ks_override_ready()) return;
    caps_word_task();
    autoshift_matrix_scan();
    if (!is_caps_word_on()) due = 0;
}

int64_t ks_word_deadline(void) {
    if (ks_autoshift_pending()) return k_uptime_get() + 1;
    if (is_caps_word_on() && due) return due;
    return INT64_MAX;
}

void ks_word_cancel(void) {
    ks_autoshift_cancel();
    caps_word_off();
    if (ks_override_ready()) {
        clear_weak_mods();
        send_keyboard_report();
    }
}

#define TYPING_RECORD() ks_locale_record()
#define TYPING_COMPUTER() ks_locale_computer()
#define TYPING_RESOLVED() ks_locale_resolved_action()
#define TYPING_MODS() ks_locale_mods()
#define TYPING_PRESS(source, code, mods) ks_locale_character_press(source, code, mods)
#define TYPING_RELEASE(code) ks_locale_character_release(code)
#define TYPING_EMIT(code, mods) ks_locale_character_emit(code, mods)
#define TYPING_BEGIN() ks_word_begin()
#define TYPING_WORD(code, record) ks_word_process(code, (record)->event.pressed)
#define TYPING_SCHEDULE() ks_word_schedule()
#include "locales/typing_port.inc"

bool ks_typing_character(uint16_t code, bool down, bool resolved) {
    return typing_character(NULL, code, down, resolved);
}

bool get_auto_shifted_key(uint16_t code, keyrecord_t *record) {
    (void)record;
    return typing_auto_key(code);
}

bool get_auto_shift_repeat(uint16_t code, keyrecord_t *record) {
    (void)code;
    (void)record;
    return false;
}

bool get_auto_shift_no_auto_repeat(uint16_t code, keyrecord_t *record) {
    (void)code;
    (void)record;
    return true;
}

uint16_t get_autoshift_timeout(uint16_t code, keyrecord_t *record) {
    (void)code;
    (void)record;
    const uint8_t *t = ks_typing_config();
    return t[2] | ((uint16_t)t[3] << 8);
}
