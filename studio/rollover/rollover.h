#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Runtime-only: zero initialization and a power cycle always select 6KRO. */
#define KSN_ON 0x7011u
#define KSN_OFF 0x7012u
#define KSN_TOGGLE 0x7013u
struct ksn_mode { bool active, requested; };
static inline bool ksn_action(struct ksn_mode *s, uint16_t code, bool down) {
    if (code < KSN_ON || code > KSN_TOGGLE) return false;
    if (down) s->requested = code == KSN_TOGGLE ? !s->requested : code == KSN_ON;
    return true;
}
/* Only after a neutral report has been accepted by the selected transport.
 * This retains the old report ID until every held key/modifier is released. */
static inline bool ksn_commit(struct ksn_mode *s, bool idle) {
    if (!idle || s->active == s->requested) return false;
    s->active = s->requested;
    return true;
}
