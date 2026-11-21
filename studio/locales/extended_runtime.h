#pragma once
struct kse_edge {unsigned pos;uint16_t code;bool used,release;};
struct kse_runtime {
    struct ksl_stroke unicode[20];
    uint8_t unicode_count,unicode_at,unicode_mode[6],unicode_restore_locks;
    int64_t unicode_due,leader_due,last_text;
    uint16_t leader_keys[5],locked[16];
    uint8_t leader_count,program_steps;
    uint8_t shifts[2];
    uint16_t program_at,text_remaining;
    int64_t program_due;
    struct kse_edge edges[32];
    char text[32];uint8_t text_len;
    bool program_active;
    unsigned keyboard_lock_position;
    bool lock_key_down;
    bool emitting,leader,arm_lock,keyboard_lock,sentence,capitalize,terminal,both_shifts;
    bool swap,swap_once,swap_temporary,swap_previous,dim_soon;
};
