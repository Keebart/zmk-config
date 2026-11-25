/* Keebart Studio Sharp MiP 68x160 portrait drawing primitives & wireless icons.
 * Freestanding C11 (zero libc/OS headers required).
 */
#ifndef KEEBART_STUDIO_MIP_CANVAS_H
#define KEEBART_STUDIO_MIP_CANVAS_H

#include "mip_preset.h"
#include <stddef.h>
#include <stdint.h>

static inline void ksm_clear(uint8_t fb[KSM_FB_BYTES]) {
    for (size_t i = 0; i < KSM_FB_BYTES; i++) {
        fb[i] = 0;
    }
}

static inline void ksm_px(uint8_t fb[KSM_FB_BYTES], int x, int y, uint8_t on) {
    if (x < 0 || x >= (int)KSM_WIDTH || y < 0 || y >= (int)KSM_HEIGHT) {
        return;
    }
    uint32_t bit_index = (uint32_t)y * KSM_WIDTH + (uint32_t)x;
    uint32_t byte_index = bit_index >> 3;
    uint8_t mask = (uint8_t)(1u << (bit_index & 7u));
    if (on) {
        fb[byte_index] |= mask;
    } else {
        fb[byte_index] &= (uint8_t)~mask;
    }
}

static inline uint8_t ksm_get_px(const uint8_t fb[KSM_FB_BYTES], int x, int y) {
    if (x < 0 || x >= (int)KSM_WIDTH || y < 0 || y >= (int)KSM_HEIGHT) {
        return 0;
    }
    uint32_t bit_index = (uint32_t)y * KSM_WIDTH + (uint32_t)x;
    return (uint8_t)((fb[bit_index >> 3] >> (bit_index & 7u)) & 1u);
}

static inline void ksm_hline(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, uint8_t on) {
    for (int i = 0; i < w; i++) {
        ksm_px(fb, x + i, y, on);
    }
}

static inline void ksm_vline(uint8_t fb[KSM_FB_BYTES], int x, int y, int h, uint8_t on) {
    for (int i = 0; i < h; i++) {
        ksm_px(fb, x, y + i, on);
    }
}

static inline void ksm_dotted_hline(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int step,
                                    uint8_t on) {
    if (step < 1) {
        step = 2;
    }
    for (int i = 0; i < w; i += step) {
        ksm_px(fb, x + i, y, on);
    }
}

static inline void ksm_fill_rect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h, uint8_t on) {
    for (int row = 0; row < h; row++) {
        ksm_hline(fb, x, y + row, w, on);
    }
}

static inline void ksm_rect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h, uint8_t on) {
    if (w <= 0 || h <= 0) {
        return;
    }
    ksm_hline(fb, x, y, w, on);
    ksm_hline(fb, x, y + h - 1, w, on);
    ksm_vline(fb, x, y, h, on);
    ksm_vline(fb, x + w - 1, y, h, on);
}

static inline void ksm_round_rect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h,
                                  uint8_t on) {
    if (w < 3 || h < 3) {
        ksm_rect(fb, x, y, w, h, on);
        return;
    }
    ksm_hline(fb, x + 1, y, w - 2, on);
    ksm_hline(fb, x + 1, y + h - 1, w - 2, on);
    ksm_vline(fb, x, y + 1, h - 2, on);
    ksm_vline(fb, x + w - 1, y + 1, h - 2, on);
}

static inline void ksm_fill_round_rect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h,
                                       uint8_t on) {
    if (w < 3 || h < 3) {
        ksm_fill_rect(fb, x, y, w, h, on);
        return;
    }
    ksm_hline(fb, x + 1, y, w - 2, on);
    ksm_fill_rect(fb, x, y + 1, w, h - 2, on);
    ksm_hline(fb, x + 1, y + h - 1, w - 2, on);
}

static inline void ksm_line(uint8_t fb[KSM_FB_BYTES], int x0, int y0, int x1, int y1, uint8_t on) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = dx >= 0 ? 1 : -1;
    int sy = dy >= 0 ? 1 : -1;
    dx = dx >= 0 ? dx : -dx;
    dy = dy >= 0 ? -dy : dy;
    int err = dx + dy;
    for (;;) {
        ksm_px(fb, x0, y0, on);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static inline void ksm_fill_circle(uint8_t fb[KSM_FB_BYTES], int cx, int cy, int r, uint8_t on) {
    int rr = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= rr) {
                ksm_px(fb, cx + dx, cy + dy, on);
            }
        }
    }
}

static inline void ksm_circle(uint8_t fb[KSM_FB_BYTES], int cx, int cy, int r, uint8_t on) {
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        ksm_px(fb, cx + x, cy + y, on);
        ksm_px(fb, cx + y, cy + x, on);
        ksm_px(fb, cx - y, cy + x, on);
        ksm_px(fb, cx - x, cy + y, on);
        ksm_px(fb, cx - x, cy - y, on);
        ksm_px(fb, cx - y, cy - x, on);
        ksm_px(fb, cx + y, cy - x, on);
        ksm_px(fb, cx + x, cy - y, on);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x + 1);
        }
    }
}

/* Crisp 5x7 ASCII font (column-major, 5 bytes per char, bit 0 = top row). */
static inline const uint8_t *ksm_font5x7(char c) {
    static const uint8_t glyphs[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, /* ' ' */
        {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! */
        {0x00, 0x07, 0x00, 0x07, 0x00}, /* " */
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* # */
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* $ */
        {0x23, 0x13, 0x08, 0x64, 0x62}, /* % */
        {0x36, 0x49, 0x55, 0x22, 0x50}, /* & */
        {0x00, 0x05, 0x03, 0x00, 0x00}, /* ' */
        {0x00, 0x1C, 0x22, 0x41, 0x00}, /* ( */
        {0x00, 0x41, 0x22, 0x1C, 0x00}, /* ) */
        {0x14, 0x08, 0x3E, 0x08, 0x14}, /* * */
        {0x08, 0x08, 0x3E, 0x08, 0x08}, /* + */
        {0x00, 0x50, 0x30, 0x00, 0x00}, /* , */
        {0x08, 0x08, 0x08, 0x08, 0x08}, /* - */
        {0x00, 0x60, 0x60, 0x00, 0x00}, /* . */
        {0x20, 0x10, 0x08, 0x04, 0x02}, /* / */
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
        {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
        {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
        {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
        {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
        {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
        {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
        {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
        {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
        {0x00, 0x36, 0x36, 0x00, 0x00}, /* : */
        {0x00, 0x56, 0x36, 0x00, 0x00}, /* ; */
        {0x08, 0x14, 0x22, 0x41, 0x00}, /* < */
        {0x14, 0x14, 0x14, 0x14, 0x14}, /* = */
        {0x00, 0x41, 0x22, 0x14, 0x08}, /* > */
        {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
        {0x32, 0x49, 0x79, 0x41, 0x3E}, /* @ */
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
        {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
        {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
        {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
        {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, /* G */
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
        {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
        {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
        {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
        {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* M */
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
        {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
        {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
        {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
        {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* W */
        {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
        {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
        {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
        {0x00, 0x7F, 0x41, 0x41, 0x00}, /* [ */
        {0x02, 0x04, 0x08, 0x10, 0x20}, /* \ */
        {0x00, 0x41, 0x41, 0x7F, 0x00}, /* ] */
        {0x04, 0x02, 0x01, 0x02, 0x04}, /* ^ */
        {0x40, 0x40, 0x40, 0x40, 0x40}, /* _ */
        {0x00, 0x01, 0x02, 0x04, 0x00}, /* ` */
        {0x20, 0x54, 0x54, 0x54, 0x78}, /* a */
        {0x7F, 0x48, 0x44, 0x44, 0x38}, /* b */
        {0x38, 0x44, 0x44, 0x44, 0x20}, /* c */
        {0x38, 0x44, 0x44, 0x48, 0x7F}, /* d */
        {0x38, 0x54, 0x54, 0x54, 0x18}, /* e */
        {0x08, 0x7E, 0x09, 0x01, 0x02}, /* f */
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, /* g */
        {0x7F, 0x08, 0x04, 0x04, 0x78}, /* h */
        {0x00, 0x44, 0x7D, 0x40, 0x00}, /* i */
        {0x20, 0x40, 0x44, 0x3D, 0x00}, /* j */
        {0x7F, 0x10, 0x28, 0x44, 0x00}, /* k */
        {0x00, 0x41, 0x7F, 0x40, 0x00}, /* l */
        {0x7C, 0x04, 0x18, 0x04, 0x78}, /* m */
        {0x7C, 0x08, 0x04, 0x04, 0x78}, /* n */
        {0x38, 0x44, 0x44, 0x44, 0x38}, /* o */
        {0x7C, 0x14, 0x14, 0x14, 0x08}, /* p */
        {0x08, 0x14, 0x14, 0x18, 0x7C}, /* q */
        {0x7C, 0x08, 0x04, 0x04, 0x08}, /* r */
        {0x48, 0x54, 0x54, 0x54, 0x20}, /* s */
        {0x04, 0x3F, 0x44, 0x40, 0x20}, /* t */
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* u */
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* v */
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* w */
        {0x44, 0x28, 0x10, 0x28, 0x44}, /* x */
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* y */
        {0x44, 0x64, 0x54, 0x4C, 0x44}, /* z */
        {0x00, 0x08, 0x36, 0x41, 0x00}, /* { */
        {0x00, 0x00, 0x7F, 0x00, 0x00}, /* | */
        {0x00, 0x41, 0x36, 0x08, 0x00}, /* } */
        {0x08, 0x04, 0x08, 0x10, 0x08}, /* ~ */
    };
    if (c < 32 || c > 126) {
        return glyphs[0];
    }
    return glyphs[(int)c - 32];
}

static inline int ksm_text5x7_width(const char *text) {
    int n = 0;
    while (text && text[n]) {
        n++;
    }
    return n > 0 ? n * 6 - 1 : 0;
}

static inline void ksm_text5x7(uint8_t fb[KSM_FB_BYTES], int x, int y, const char *text,
                               uint8_t on) {
    if (!text) {
        return;
    }
    for (int i = 0; text[i]; i++) {
        const uint8_t *g = ksm_font5x7(text[i]);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    ksm_px(fb, x + i * 6 + col, y + row, on);
                }
            }
        }
    }
}

static inline void ksm_text5x7_centered(uint8_t fb[KSM_FB_BYTES], int y, const char *text,
                                        uint8_t on) {
    int w = ksm_text5x7_width(text);
    ksm_text5x7(fb, ((int)KSM_WIDTH - w) / 2, y, text, on);
}

static inline void ksm_text5x7_clipped(uint8_t fb[KSM_FB_BYTES], int x, int y, int min_x, int max_x,
                                       const char *text, uint8_t on) {
    if (!text) {
        return;
    }
    for (int i = 0; text[i]; i++) {
        const uint8_t *g = ksm_font5x7(text[i]);
        for (int col = 0; col < 5; col++) {
            int px = x + i * 6 + col;
            if (px < min_x || px > max_x) {
                continue;
            }
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    ksm_px(fb, px, y + row, on);
                }
            }
        }
    }
}

/* Crisp 3x5 uppercase/digit/symbol micro font for compact wireless badges.
 * Each glyph is 5 rows of 3 bits (MSB = left pixel).
 */
static inline void ksm_glyph3x5(uint8_t fb[KSM_FB_BYTES], int x, int y, char c, uint8_t on) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    uint8_t rows[5] = {0, 0, 0, 0, 0};
    switch (c) {
        case '0': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case '1': rows[0]=2; rows[1]=6; rows[2]=2; rows[3]=2; rows[4]=7; break;
        case '2': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=4; rows[4]=7; break;
        case '3': rows[0]=7; rows[1]=1; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case '4': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=1; break;
        case '5': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case '6': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=5; rows[4]=7; break;
        case '7': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case '8': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=7; break;
        case '9': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case 'A': rows[0]=2; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'B': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=6; break;
        case 'C': rows[0]=7; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
        case 'D': rows[0]=6; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=6; break;
        case 'E': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=7; break;
        case 'F': rows[0]=7; rows[1]=4; rows[2]=6; rows[3]=4; rows[4]=4; break;
        case 'G': rows[0]=7; rows[1]=4; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'H': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'I': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=7; break;
        case 'J': rows[0]=1; rows[1]=1; rows[2]=1; rows[3]=5; rows[4]=7; break;
        case 'K': rows[0]=5; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; break;
        case 'L': rows[0]=4; rows[1]=4; rows[2]=4; rows[3]=4; rows[4]=7; break;
        case 'M': rows[0]=5; rows[1]=7; rows[2]=7; rows[3]=5; rows[4]=5; break;
        case 'N': rows[0]=6; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=5; break;
        case 'O': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'P': rows[0]=7; rows[1]=5; rows[2]=7; rows[3]=4; rows[4]=4; break;
        case 'Q': rows[0]=7; rows[1]=5; rows[2]=5; rows[3]=7; rows[4]=1; break;
        case 'R': rows[0]=6; rows[1]=5; rows[2]=6; rows[3]=5; rows[4]=5; break;
        case 'S': rows[0]=7; rows[1]=4; rows[2]=7; rows[3]=1; rows[4]=7; break;
        case 'T': rows[0]=7; rows[1]=2; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case 'U': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=7; break;
        case 'V': rows[0]=5; rows[1]=5; rows[2]=5; rows[3]=5; rows[4]=2; break;
        case 'W': rows[0]=5; rows[1]=5; rows[2]=7; rows[3]=7; rows[4]=5; break;
        case 'X': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=5; rows[4]=5; break;
        case 'Y': rows[0]=5; rows[1]=5; rows[2]=2; rows[3]=2; rows[4]=2; break;
        case 'Z': rows[0]=7; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=7; break;
        case '%': rows[0]=5; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=5; break;
        case '-': rows[0]=0; rows[1]=0; rows[2]=7; rows[3]=0; rows[4]=0; break;
        case '.': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=2; break;
        case ':': rows[0]=0; rows[1]=2; rows[2]=0; rows[3]=2; rows[4]=0; break;
        case '+': rows[0]=0; rows[1]=2; rows[2]=7; rows[3]=2; rows[4]=0; break;
        case '/': rows[0]=1; rows[1]=1; rows[2]=2; rows[3]=4; rows[4]=4; break;
        case '&': rows[0]=2; rows[1]=5; rows[2]=2; rows[3]=5; rows[4]=3; break;
        case '_': rows[0]=0; rows[1]=0; rows[2]=0; rows[3]=0; rows[4]=7; break;
        default: return;
    }
    for (int r = 0; r < 5; r++) {
        for (int col = 0; col < 3; col++) {
            if (rows[r] & (4u >> col)) {
                ksm_px(fb, x + col, y + r, on);
            }
        }
    }
}

static inline int ksm_text3x5_width(const char *text) {
    int n = 0;
    while (text && text[n]) {
        n++;
    }
    return n > 0 ? n * 4 - 1 : 0;
}

static inline void ksm_text3x5(uint8_t fb[KSM_FB_BYTES], int x, int y, const char *text,
                               uint8_t on) {
    if (!text) {
        return;
    }
    for (int i = 0; text[i]; i++) {
        ksm_glyph3x5(fb, x + i * 4, y, text[i], on);
    }
}

static inline void ksm_text3x5_centered_in(uint8_t fb[KSM_FB_BYTES], int box_x, int box_w, int y,
                                           const char *text, uint8_t on) {
    int w = ksm_text3x5_width(text);
    ksm_text3x5(fb, box_x + (box_w - w) / 2, y, text, on);
}

static inline void ksm_text3x5_right(uint8_t fb[KSM_FB_BYTES], int right_x, int y, const char *text,
                                     uint8_t on) {
    int w = ksm_text3x5_width(text);
    ksm_text3x5(fb, right_x - w + 1, y, text, on);
}

/* Format unsigned integer (0..999) into buf without stdio. */
static inline void ksm_format_u8(char out[6], uint8_t val, const char *suffix) {
    int p = 0;
    if (val >= 100u) {
        out[p++] = (char)('0' + (val / 100u));
        out[p++] = (char)('0' + ((val / 10u) % 10u));
        out[p++] = (char)('0' + (val % 10u));
    } else if (val >= 10u) {
        out[p++] = (char)('0' + (val / 10u));
        out[p++] = (char)('0' + (val % 10u));
    } else {
        out[p++] = (char)('0' + val);
    }
    if (suffix) {
        while (*suffix && p < 5) {
            out[p++] = *suffix++;
        }
    }
    out[p] = '\0';
}

/* Reusable 18x9 battery glyph (16x9 body + 2x5 terminal nub) with optional charging bolt. */
static inline void ksm_icon_battery(uint8_t fb[KSM_FB_BYTES], int x, int y, uint8_t level,
                                    uint8_t charging) {
    ksm_round_rect(fb, x, y, 16, 9, 1);
    ksm_fill_rect(fb, x + 16, y + 2, 2, 5, 1);
    if (level <= 100u) {
        int fill = (int)(((uint32_t)level * 12u + 50u) / 100u);
        if (level > 0u && fill == 0) {
            fill = 1;
        }
        if (fill > 12) {
            fill = 12;
        }
        if (fill > 0) {
            ksm_fill_rect(fb, x + 2, y + 2, fill, 5, 1);
        }
    } else {
        /* Unknown battery: subtle center dash */
        ksm_hline(fb, x + 5, y + 4, 6, 1);
    }
    if (charging) {
        /* High-contrast lightning bolt centered over the battery body */
        static const int8_t bx[9] = {9, 8, 7, 6, 7, 8, 9, 8, 7};
        static const int8_t by[9] = {1, 2, 3, 4, 4, 4, 4, 5, 6};
        /* Halo cutout so the bolt reads clearly over both empty and full cells */
        ksm_fill_rect(fb, x + 5, y + 2, 6, 5, 0);
        for (int i = 0; i < 9; i++) {
            ksm_px(fb, x + bx[i], y + by[i], 1);
        }
        ksm_px(fb, x + 6, y + 7, 1);
    }
}

/* Reusable 11x9 USB connector icon. */
static inline void ksm_icon_usb(uint8_t fb[KSM_FB_BYTES], int x, int y) {
    /* Central horizontal bus */
    ksm_hline(fb, x + 1, y + 4, 8, 1);
    /* Left circle root */
    ksm_fill_rect(fb, x, y + 3, 2, 3, 1);
    /* Right arrowhead */
    ksm_px(fb, x + 9, y + 4, 1);
    ksm_px(fb, x + 8, y + 3, 1);
    ksm_px(fb, x + 8, y + 5, 1);
    /* Upper branch to square node */
    ksm_px(fb, x + 3, y + 3, 1);
    ksm_px(fb, x + 4, y + 2, 1);
    ksm_hline(fb, x + 5, y + 1, 2, 1);
    ksm_fill_rect(fb, x + 6, y, 2, 2, 1);
    /* Lower branch to dot node */
    ksm_px(fb, x + 4, y + 5, 1);
    ksm_px(fb, x + 5, y + 6, 1);
    ksm_hline(fb, x + 6, y + 7, 2, 1);
    ksm_px(fb, x + 7, y + 8, 1);
}

/* Reusable 11x9 wireless connected signal-waves icon. */
static inline void ksm_icon_wireless_ok(uint8_t fb[KSM_FB_BYTES], int x, int y) {
    /* Outer arc */
    ksm_hline(fb, x + 2, y + 1, 7, 1);
    ksm_px(fb, x + 1, y + 2, 1);
    ksm_px(fb, x + 9, y + 2, 1);
    ksm_px(fb, x, y + 3, 1);
    ksm_px(fb, x + 10, y + 3, 1);
    /* Middle arc */
    ksm_hline(fb, x + 3, y + 4, 5, 1);
    ksm_px(fb, x + 2, y + 5, 1);
    ksm_px(fb, x + 8, y + 5, 1);
    /* Base node */
    ksm_fill_rect(fb, x + 4, y + 7, 3, 2, 1);
}

/* Reusable 11x9 disconnected X icon. */
static inline void ksm_icon_wireless_off(uint8_t fb[KSM_FB_BYTES], int x, int y) {
    for (int i = 0; i < 7; i++) {
        ksm_px(fb, x + 2 + i, y + 1 + i, 1);
        ksm_px(fb, x + 8 - i, y + 1 + i, 1);
    }
    ksm_px(fb, x + 5, y + 4, 1);
}

/* Reusable 11x9 open/advertising pairing beacon icon. */
static inline void ksm_icon_wireless_open(uint8_t fb[KSM_FB_BYTES], int x, int y) {
    /* Dashed/pulsing ring around center beacon dot */
    ksm_hline(fb, x + 4, y, 3, 1);
    ksm_hline(fb, x + 4, y + 8, 3, 1);
    ksm_vline(fb, x + 1, y + 3, 3, 1);
    ksm_vline(fb, x + 9, y + 3, 3, 1);
    ksm_px(fb, x + 2, y + 1, 1);
    ksm_px(fb, x + 8, y + 1, 1);
    ksm_px(fb, x + 2, y + 7, 1);
    ksm_px(fb, x + 8, y + 7, 1);
    ksm_fill_rect(fb, x + 4, y + 3, 3, 3, 1);
}

#endif /* KEEBART_STUDIO_MIP_CANVAS_H */
