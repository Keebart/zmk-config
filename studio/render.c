#include "render.h"
#include <math.h>
#include <string.h>
#include "coordinates.h"
#define TAU 6.28318530718f
static float clamp(float v) { return fminf(1, fmaxf(0, v)); }
static float wave(float v) { return (sinf(v * TAU) + 1) * .5f; }
static uint8_t hue(float v) {
    v = fmodf(v, 256);
    if (v < 0)
        v += 256;
    return v;
}
static void hsv(uint8_t h, uint8_t s, uint8_t v, uint8_t out[3]) {
    if (!s) {
        memset(out, v, 3);
        return;
    }
    unsigned region = h * 6 / 255, rem = (h * 2 - region * 85) * 3;
    uint8_t p = (v * (255 - s)) >> 8, q = (v * (255 - ((s * rem) >> 8))) >> 8,
            t = (v * (255 - ((s * (255 - rem)) >> 8))) >> 8;
    switch (region) {
    case 0:
    case 6:
        out[0] = v;
        out[1] = t;
        out[2] = p;
        break;
    case 1:
        out[0] = q;
        out[1] = v;
        out[2] = p;
        break;
    case 2:
        out[0] = p;
        out[1] = v;
        out[2] = t;
        break;
    case 3:
        out[0] = p;
        out[1] = q;
        out[2] = v;
        break;
    case 4:
        out[0] = t;
        out[1] = p;
        out[2] = v;
        break;
    default:
        out[0] = v;
        out[1] = p;
        out[2] = q;
    }
}
void ks_render_press(struct ks_renderer *s, unsigned p, int64_t now) {
    if (p >= KS_KEYS)
        return;
    s->hits[s->next++ % 32] = (struct ks_hit){p, now + 1};
    s->heat[p] = s->heat[p] > 175 ? 255 : s->heat[p] + 80;
}
static void effect(struct ks_renderer *r, const struct ks_config *c, unsigned p, int64_t now,
                   uint8_t out[3]) {
    unsigned id = c->lighting[0];
    float speed = c->lighting[2], h = c->lighting[3], v = c->lighting[5];
    uint8_t s = c->lighting[4];
    float x = xy[p][0] / 224.f, y = xy[p][1] / 224.f, t = now * .001f * (.18f + speed / 180.f);
    float angle = atan2f(y - .14f, x - .5f) / TAU, radius = hypotf(x - .5f, y - .14f);
    if (id < 2) {
        memset(out, 0, 3);
        return;
    }
    if (id == 29 || (id >= 31 && id <= 42)) {
        float strength = 0, chosen_dx = 0, chosen_dy = 0, chosen_age = 0, chosen_distance = 0;
        bool multi = id == 29 || id == 34 || id == 36 || id == 38 || id == 40 || id == 42;
        for (unsigned n = 0; n < (multi ? 32U : 1U); n++) {
            struct ks_hit hit = r->hits[(r->next + 31 - n) % 32];
            if (!hit.at)
                continue;
            float age = (now - (hit.at - 1)) * .001f * (.65f + speed / 200.f);
            if (age < 0 || age > 4)
                continue;
            float dx = x - xy[hit.position][0] / 224.f, dy = y - xy[hit.position][1] / 224.f,
                  d = hypotf(dx, dy), fade = clamp(1 - age / 1.8f), a = 0;
            switch (id) {
            case 29:
                a = expf(-age / 1.2f) * expf(-d * d / .006f);
                break;
            case 31:
            case 32:
                a = p == hit.position ? fade : 0;
                break;
            case 33:
            case 34:
                a = clamp(1 - d / .32f) * fade;
                break;
            case 35:
            case 36:
                a = clamp(1 - fminf(fabsf(dx), fabsf(dy)) / .035f) * clamp(1 - d / .65f) * fade;
                break;
            case 37:
            case 38:
                a = clamp(1 - fminf(fabsf(dx), fabsf(dy)) / .035f) *
                    clamp(1 - fabsf(d - age * .45f) / .12f) * fade;
                break;
            default:
                a = clamp(1 - fabsf(d - age * .45f) / .07f) * fade;
            }
            if (id == 29)
                strength += a;
            else if (a > strength) {
                strength = a;
                chosen_dx = dx;
                chosen_dy = dy;
                chosen_age = age;
                chosen_distance = d;
            }
        }
        (void)chosen_dx;
        if (id == 29) {
            strength = clamp(strength);
            h = fmaxf(0, 85 - strength * 85);
            s = 255;
        }
        if (id == 32)
            h += chosen_age * 25;
        if (id == 37 || id == 38)
            h += chosen_dy * 200;
        if (id == 39 || id == 40) {
            h = chosen_distance * 510 - chosen_age * 90;
            s = 255;
        }
        v *= clamp(strength);
    } else
        switch (id) {
        case 2:
            break;
        case 3:
            if (p >= 40 || (c->keys[p] >= 0xe0 && c->keys[p] <= 0xe7))
                h += speed;
            break;
        case 4:
            h += y * speed * 3.5f;
            break;
        case 5:
            h += x * speed;
            break;
        case 6:
            v *= wave(t);
            break;
        case 7:
            s = 255 * wave(x - t);
            break;
        case 8:
            v *= wave(x - t);
            break;
        case 9:
            s = 255 * wave(angle - t);
            break;
        case 10:
            v *= wave(angle - t);
            break;
        case 11:
            s = 255 * wave(angle + radius * 3 - t);
            break;
        case 12:
            v *= wave(angle + radius * 3 - t);
            break;
        case 13:
            h = t * 80;
            break;
        case 14:
            h = (x - t) * 255;
            break;
        case 15:
            h = (y * 3.5f - t) * 255;
            break;
        case 16:
            h = (fabsf(x - .5f) + y - t) * 255;
            break;
        case 17:
            h = (radius - t) * 255;
            break;
        case 18:
            h = (fminf(hypotf(x - .25f, y - .14f), hypotf(x - .75f, y - .14f)) - t) * 255;
            break;
        case 19:
            h = (angle - t) * 255;
            break;
        case 20:
            h = (angle + radius * 3 - t) * 255;
            break;
        case 21:
            v *= powf(wave(angle * 2 - t), 4);
            break;
        case 22:
            h += (angle - t) * 255;
            v *= powf(wave(angle * 2 - t), 4);
            break;
        case 23:
            h = (atan2f(y - .14f, x < (float).5 ? x - .25f : x - .75f) / TAU - t) * 255;
            break;
        case 24:
        case 25:
        case 43:
        case 44: {
            float cell = floorf(x * 13) + 17 * floorf(y * 14), phase = floorf(t * 2 + cell * .618f);
            v *= powf(wave(cell * .381f + phase * .173f), 8);
            if (id == 25 || id == 43) {
                h = phase * 73 + cell * 29;
                if (id == 43)
                    s = 127 + (unsigned)fabsf(phase * 37 + cell * 11) % 128;
            }
            if (id == 44)
                v *= wave(x * 8 + y * 12 - t);
            break;
        }
        case 26:
            h += wave(t) * 80;
            break;
        case 27:
            h += wave(t + x) * 100;
            break;
        case 28:
            h += (x - t) * 100;
            break;
        case 30:
            h = 85;
            s = 255;
            v *= powf(wave(y * 3.5f - t + x * 3), 8);
            break;
        default:
            memset(out, 0, 3);
            return;
        }
    unsigned brightness = fminf(30, fmaxf(0, v));
    hsv(hue(h), s, cie[brightness], out);
}
void ks_render(struct ks_renderer *r, const struct ks_config *c, unsigned layer, uint8_t locks,
               bool awake, int64_t now, uint8_t out[KS_KEYS][3]) {
    memset(out, 0, KS_KEYS * 3);
    if (!awake || layer >= KS_LAYERS)
        return;
    unsigned brightness = c->lighting[5] > 30 ? 30 : c->lighting[5];
    for (unsigned p = 0; p < KS_KEYS; p++) {
        if (c->mode && !(((unsigned)c->animation_layers[0] | (unsigned)c->animation_layers[1] << 8) & (1u << layer)))
            for (unsigned ch = 0; ch < 3; ch++)
                out[p][ch] = c->palette[(layer * KS_KEYS + p) * 3 + ch] * brightness / 255;
        else
            effect(r, c, p, now, out[p]);
        const uint8_t *rule = c->palette + KS_KEY_COUNT * 3 + (layer * KS_KEYS + p) * 4;
        if (locks & rule[0])
            for (unsigned ch = 0; ch < 3; ch++)
                out[p][ch] = rule[ch + 1] * brightness / 255;
    }
}
