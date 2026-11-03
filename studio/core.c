#include "lighting_defaults.h"
#include "macro_restore.h"
#include "release.h"
#include "core.h"
#include <stddef.h>
#include <string.h>
_Static_assert(offsetof(struct ks_config, crc) == 8 + KS_DATA_SIZE, "wire layout");
static uint16_t rd16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static void wr32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = v >> (i * 8);
}
uint32_t ks_crc(const void *data, size_t n) {
    const uint8_t *p = data;
    uint32_t c = ~0U;
    while (n--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((c & 1) ? 0xedb88320U : 0);
    }
    return ~c;
}
bool ks_supported(uint16_t c) {
    return (c>=KSL_BINDING&&c<KSL_BINDING+KSL_BINDINGS) || ksl_supported(c) || (c >= KD_BASE && c < KD_BASE + KD_COUNT) || (c >= 0x7700 && c < 0x7700 + KM_COUNT) || c <= 0x51ff || (c >= 0x5240 && c <= 0x524f) || (c >= 0x52c0 && c <= 0x52cf) ||
           (c >= 0x52e0 && c <= 0x52ef) || (c >= 0x7c77 && c <= 0x7c7a) || c == 0x7c7b ||
           (c >= 0x7c5d && c <= 0x7c5f) || (c >= 0x7f40 && c <= 0x7f5f) || (c >= 0x7840 && c <= 0x7841) || (c >= 0x7820 && c <= 0x782a) || (c >= 0x5200 && c <= 0x520f) ||
           (c >= 0x5220 && c <= 0x522f) || (c >= 0x5260 && c <= 0x526f) ||
           (c >= 0x5280 && c <= 0x528f) || (c >= 0x7c00 && c <= 0x7c01) ||
           (c >= 0x7e00 && c <= 0x7e04) || (c >= 0x7e10 && c <= 0x7e13) ||
           (c >= 0x7e20 && c <= 0x7e22);
}
bool ks_mapping_changed(const struct ks_config *a, const struct ks_config *b) {
    return memcmp(a->repeats,b->repeats,sizeof(a->repeats)) || memcmp(a->locales,b->locales,KSL_BYTES) || memcmp(a->keys, b->keys, sizeof(a->keys)) || a->default_layer != b->default_layer || memcmp(a->macros, b->macros, KM_BYTES) || memcmp(a->dances,b->dances,sizeof(a->dances)) || memcmp(a->overrides,b->overrides,sizeof(a->overrides));
}
static void defaults(struct ks_config *c, const uint16_t *stock) {
    memset(c, 0, sizeof(*c));
    c->version = 8;
    ksl_stock(c->locales);
    for(unsigned i=0;i<KS_KEY_COUNT;i++)c->keys[i]=ksl_stock_code(stock[i]);
    for (unsigned i = KS_ENCODER_AT; i < KS_COMBO_AT; i++)
        c->keys[i] = 1;
#if KS_ENCODERS
#if KS_SOFLE
    const uint16_t encoders[4] = {170, 169, 172, 171};
#else
    const uint16_t encoders[8] = {170, 169, 75, 78, 172, 171, 170, 169};
#endif
    memcpy(c->keys + KS_ENCODER_AT, encoders, sizeof(encoders));
#if KS_SOFLE
    for (unsigned l = 1; l < 3; l++)
        memcpy(c->keys + KS_ENCODER_AT + l * KS_ENCODERS * 2, encoders, sizeof(encoders));
#endif
#endif
    c->keys[KS_TERM_AT] = 50;
    const uint8_t lighting[6] = {6, 0, 128, 143, 255, KS_DEFAULT_BRIGHTNESS};
    memcpy(c->lighting, lighting, 6);
    for (unsigned i = 0; i < KS_KEY_COUNT; i++) {
        c->palette[i * 3] = KS_DEFAULT_RED;
        c->palette[i * 3 + 1] = KS_DEFAULT_GREEN;
        c->palette[i * 3 + 2] = KS_DEFAULT_BLUE;
    }
    for(unsigned i=0;i<KD_COUNT;i++)c->dances[i].term=200;
    c->mode = 1;
    c->crc = ks_crc(c, offsetof(struct ks_config, crc));
}
void ks_init(struct ks_state *s, const uint16_t *stock) {
    memset(s, 0, sizeof(*s));
    defaults(&s->active, stock);
    s->hold_since = -1;
}
bool ks_config_valid(const struct ks_config *c) {
    if (c->version != 8 || !ksl_valid(c->locales) || c->crc != ks_crc(c, offsetof(struct ks_config, crc)) ||
        c->keys[KS_TERM_AT] < 1 || c->keys[KS_TERM_AT] > 1000 || c->mode > 1 || c->lighting[1] ||
        c->lighting[0] > 44 || c->lighting[5] > 30 || c->default_layer >= KS_LAYERS ||
        c->reserved[0] || c->reserved[1] || !km_valid(c->macros, KM_BYTES, KM_COUNT))
        return false;
    unsigned bindings=c->locales[480]|(unsigned)c->locales[481]<<8;
    for(unsigned i=0;i<bindings;i++)for(unsigned computer=0;computer<c->locales[4];computer++){
        const uint8_t *p=c->locales+KSL_BINDING_AT+i*12+computer*2;uint16_t code=rd16(p);
        if(!ks_supported(code))return false;
    }
    for(unsigned i=0;i<KS_COUNT;i++)if(c->keys[i]>=KSL_BINDING&&c->keys[i]<KSL_BINDING+KSL_BINDINGS&&c->keys[i]>=KSL_BINDING+bindings)return false;
    for(unsigned i=0;i<KAR_COUNT;i++)if(!kar_valid(&c->repeats[i]))return false;
    for(unsigned i=0;i<KS_OVERRIDE_COUNT;i++)if(!ks_override_valid(&c->overrides[i]))return false;
    for(unsigned i=0;i<KD_COUNT;i++) {
        const struct kd_entry *e=&c->dances[i];
        const uint16_t *outputs=(const uint16_t *)e;
        if(e->term>1000 || (!e->term && (e->tap || e->hold || e->twice || e->tap_hold))) return false;
        for(unsigned j=0;j<4;j++)if(outputs[j] && (outputs[j]==1 || !ks_supported(outputs[j]) ||
            (outputs[j]>=KD_BASE && outputs[j]<KD_BASE+KD_COUNT) || (outputs[j]>=0x2000 && outputs[j]<=0x51ff))) return false;
    }
    for (unsigned i = 0; i < KS_TERM_AT; i++)
        if (!ks_supported(c->keys[i]))
            return false;
    for (unsigned i = KS_KEY_COUNT * 3; i < KS_PALETTE_SIZE; i += 4) {
        uint8_t mask = c->palette[i];
        if (mask && (mask > 16 || (mask & (mask - 1))))
            return false;
    }
    for (unsigned i = 0; i < KS_COMBOS; i++) {
        const uint16_t *combo = c->keys + KS_COMBO_AT + i * 5;
        unsigned n = 0;
        while (n < 4 && combo[n])
            n++;
        if (!n && !combo[1] && !combo[2] && !combo[3] && !combo[4])
            continue;
        if (n < 2 || combo[4] <= 1)
            return false;
        for (unsigned j = 0; j < 4; j++) {
            if ((j < n && combo[j] <= 1) || (j >= n && combo[j]))
                return false;
            for (unsigned k = j + 1; k < n; k++)
                if (combo[j] == combo[k])
                    return false;
        }
        for (unsigned k = 0; k < i; k++) {
            const uint16_t *other = c->keys + KS_COMBO_AT + k * 5;
            unsigned same = 0, count = 0;
            for (unsigned j = 0; j < 4 && other[j]; j++) {
                count++;
                for (unsigned l = 0; l < n; l++)
                    if (other[j] == combo[l])
                        same++;
            }
            if (same == n && count == n)
                return false;
        }
    }
    return true;
}
bool ks_restore(struct ks_state *s, const struct ks_config *config) {
    if (!ks_config_valid(config))
        return false;
    s->active = *config;
    return true;
}
uint16_t ks_binding(const struct ks_state *s, unsigned layer, unsigned position) {
    return layer < KS_LAYERS && position < KS_KEYS ? s->active.keys[layer * KS_KEYS + position] : 0;
}
void ks_rotate(struct ks_state *s, unsigned encoder, int32_t degrees, int32_t micro,
               unsigned triggers) {
    if (encoder >= (unsigned)(KS_ENCODER_SLOTS) || !KS_ENCODERS || !triggers || triggers > 360)
        return;
    int64_t angle = (int64_t)degrees * 1000000 + micro + s->rotation_remainder[encoder];
    int64_t step = 360000000 / triggers;
    int64_t steps = angle / step;
    s->rotation_remainder[encoder] = angle % step;
    if (steps > 32)
        steps = 32;
    if (steps < -32)
        steps = -32;
    s->rotation_steps[encoder] = steps;
    if (steps)
        s->rotations[encoder][steps < 0] += steps < 0 ? -steps : steps;
}
void ks_lock(struct ks_state *s) {
    s->unlocked = false;
    s->unlocking = false;
    s->staging = false;
    s->hold_since = -1;
}
static bool chord(struct ks_state *s) {
    return (s->pressed & ((1ULL << 0) | (1ULL << KS_UNLOCK_RIGHT))) ==
           ((1ULL << 0) | (1ULL << KS_UNLOCK_RIGHT));
}
void ks_position(struct ks_state *s, unsigned pos, bool down, int64_t now) {
    if (pos >= KS_KEYS)
        return;
    if (down)
        s->pressed |= 1ULL << pos;
    else
        s->pressed &= ~(1ULL << pos);
    if (!chord(s))
        s->hold_since = -1;
    else if (s->unlocking && s->hold_since < 0)
        s->hold_since = now;
}
static bool zeros(const uint8_t *p, unsigned n) {
    while (n--)
        if (*p++)
            return false;
    return true;
}
void ks_handle(struct ks_state *s, const uint8_t *in, size_t len, uint8_t out[32], int64_t now,
               ks_persist save, const uint16_t *stock) {
    memset(out, 0, 32);
    if (len != 32) {
        out[5] = 2;
        return;
    }
    memcpy(out, in, 5);
    if (in[0] != 'K' || in[1] != 'S' || in[2] != 3 || in[5] != 0) {
        out[5] = 2;
        return;
    }
    uint8_t op = in[3];
    const uint8_t *p = in + 6;
    uint8_t *r = out + 6;
    if (now - s->last_request > 60000)
        ks_lock(s);
    s->last_request = now;
    if (s->unlocking) {
        if (now - s->unlock_since > 30000)
            ks_lock(s);
        else if (chord(s) && s->hold_since >= 0 && now - s->hold_since >= 2500) {
            s->unlocked = true;
            s->unlocking = false;
        }
    }
    unsigned used;
    switch (op) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 9:
    case 12:
    case 15:
        used = 0;
        break;
    case 13:
        used = 7;
        break;
    case 6:
    case 10:
    case 11:
        used = 4;
        break;
    case 14:
        used = 7 + (unsigned)p[6];
        break;
    case 8:
        used = 8;
        break;
    default:
        out[5] = 1;
        return;
    }
    if (used > 26 || !zeros(p + used, 26 - used)) {
        out[5] = 2;
        return;
    }
    // Ordinary saves stay unprotected; macro content changes are checked at
    // commit against the active configuration. Never mutate during unlock.
    bool protected = op == 9 || op == 10 || op == 12;
    bool transaction = op == 6 || op == 8 || op == 14;
    if(s->storage_error && (transaction || op==10 || op==11)){out[5]=6;return;}
    if ((protected && !s->unlocked) || ((protected || transaction) && s->unlocking)) {
        out[5] = 3;
        return;
    }
    switch (op) {
    case 15:
        keebart_release(r,KS_MODEL_ID,1,8);
        memcpy(r+12,s->peer_release,12);
        break;
    case 0:
        memcpy(r, KS_UID, 8);
        r[8] = KS_LAYERS;
        r[9] = KS_KEYS;
        r[10] = KS_CAPABILITIES & 255;
        r[11] = KS_CAPABILITIES >> 8;
        wr32(r+16,(uint32_t)s->storage_error);
        wr32(r + 12, s->active.generation);
        break;
    case 1:
        r[3] = 0;
        r[4] = KS_UNLOCK_RIGHT; /* fall through */
    case 3:
        r[0] = s->unlocked;
        r[1] = s->unlocking;
        r[2] = s->unlocked         ? 0
               : s->hold_since < 0 ? 25
                                   : (uint8_t)((2500 - (now - s->hold_since) + 99) / 100);
        break;
    case 2:
        ks_lock(s);
        s->unlocking = true;
        s->unlock_since = now;
        s->hold_since = chord(s) ? now : -1;
        break;
    case 4:
        ks_lock(s);
        break;
    case 6:
        if (rd32(p) != s->active.generation) {
            out[5] = 4;
            break;
        }
        s->staged = s->active;
        memset(s->received, 0, sizeof(s->received));
        s->staging = true;
        s->transaction++;
        if (!s->transaction)
            s->transaction++;
        wr32(r, s->transaction);
        break;
    case 13: {
        unsigned at = rd16(p), n = p[2];
        if (n < 1 || n > 19 || at + n > KS_DATA_SIZE) {
            out[5] = 2;
            break;
        }
        if (rd32(p + 3) != s->active.generation) {
            out[5] = 4;
            break;
        }
        memcpy(r, p, 7);
        memcpy(r + 7, (const uint8_t *)s->active.keys + at, n);
        break;
    }
    case 14: {
        unsigned at = rd16(p + 4), n = p[6];
        if (!s->staging || rd32(p) != s->transaction) {
            out[5] = 4;
            break;
        }
        if (n < 1 || n > 19 || at + n > KS_DATA_SIZE) {
            out[5] = 2;
            break;
        }
        memcpy((uint8_t *)s->staged.keys + at, p + 7, n);
        for (unsigned i = at; i < at + n; i++)
            s->received[i / 8] |= 1 << (i % 8);
        break;
    }
    case 12:
        r[0] = KS_ENCODERS;
#if KS_ENCODERS
        for (unsigned i = 0; i < KS_ENCODERS; i++)
            for (unsigned d = 0; d < 2; d++) {
                unsigned at = 1 + i * 4 + d * 2;
                r[at] = s->rotations[i][d];
                r[at + 1] = s->rotations[i][d] >> 8;
            }
#endif
        break;
    case 8:
        if (!s->staging || rd32(p) != s->transaction) {
            out[5] = 4;
            break;
        }
        if (!s->unlocked && memcmp(s->active.macros, s->staged.macros, KM_BYTES) && !ks_restore_matches(s->staged.macros, KM_BYTES)) {
            out[5] = 3;
            break;
        }
        bool mapping = ks_mapping_changed(&s->active, &s->staged);
        if (mapping && s->pressed) {
            out[5] = 5;
            break;
        }
        for (unsigned i = 0; i < KS_DATA_SIZE; i++)
            if (!(s->received[i / 8] & (1 << (i % 8)))) {
                out[5] = 2;
                return;
            }
        if (rd32(p + 4) != ks_crc(s->staged.keys, KS_DATA_SIZE)) {
            out[5] = 2;
            break;
        }
        s->staged.generation = s->active.generation + 1;
        s->staged.crc = ks_crc(&s->staged, offsetof(struct ks_config, crc));
        if (!ks_config_valid(&s->staged)) {
            out[5] = 2;
            break;
        }
        if (save(&s->staged)) {
            out[5] = 6;
            break;
        }
        if (memcmp(s->active.macros,s->staged.macros,KM_BYTES)) ks_restore_consumed();
        s->active = s->staged;
        s->staging = false;
        wr32(r, s->active.generation);
        break;
    case 9:
        for (unsigned i = 0; i < (KS_KEYS + 7) / 8; i++)
            r[i] = s->pressed >> (8 * i);
        break;
    case 10:
        if (rd32(p) != s->active.generation) {
            out[5] = 4;
            break;
        }
        if (s->pressed) {
            out[5] = 5;
            break;
        }
        defaults(&s->staged, stock);
        s->staged.generation = s->active.generation + 1;
        s->staged.crc = ks_crc(&s->staged, offsetof(struct ks_config, crc));
        if (!ks_config_valid(&s->staged)) {
            out[5] = 2;
            break;
        }
        if (save(&s->staged)) {
            out[5] = 6;
            break;
        }
        s->active = s->staged;
        ks_lock(s);
        break;
    case 11:
        if (rd32(p) == s->transaction)
            s->staging = false;
        break;
    }
}
