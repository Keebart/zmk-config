/* Keebart Horizon — Default Sharp MiP (68x160 portrait) display preset.
 *
 * Left (Central):  Symbol-first wireless status totem blending nice!view clarity
 *                  with OLED Keebart Horizon's sculptural geometry — bold battery
 *                  icon + charging bolt, bold USB/BLE link symbol, iconic
 *                  5-circle Bluetooth profile constellation, inverted rounded layer
 *                  hero pill + geometric layer pips, 2x2 live modifier keycaps,
 *                  whisper-quiet lock indicators, and a minimal last-key chip.
 * Right (Periph):  Matching bold battery + split-link header over a full-bleed
 *                  silver-paper pen-and-ink coastal lighthouse illustration
 *                  (crescent moon, soaring gulls, layered woodblock clouds,
 *                  delicate stippled light beam when linked, nautical striped
 *                  lighthouse, keeper's cottage, shaded headland, sailboat, and
 *                  shimmering ocean ripples).
 */
#include "mip_canvas.h"

/* 9x9 / 9x7 modifier pixel icons (MSB-first, 2 bytes per row — matches OLED Horizon) */
static const uint8_t KSM_ICON_SHIFT_9X9[18] = {
    0x08, 0x00, /* ....#.... */
    0x1c, 0x00, /* ...###... */
    0x36, 0x00, /* ..##.##.. */
    0x63, 0x00, /* .##...##. */
    0xc1, 0x80, /* ##.....## */
    0xf7, 0x80, /* ####.#### */
    0x14, 0x00, /* ...#.#... */
    0x14, 0x00, /* ...#.#... */
    0x1c, 0x00, /* ...###... */
};

static const uint8_t KSM_ICON_CTRL_9X7[14] = {
    0x08, 0x00, /* ....#.... */
    0x1c, 0x00, /* ...###... */
    0x36, 0x00, /* ..##.##.. */
    0x63, 0x00, /* .##...##. */
    0xc1, 0x80, /* ##.....## */
    0x80, 0x80, /* #.......# */
    0x00, 0x00,
};

static const uint8_t KSM_ICON_ALT_9X7[14] = {
    0xe0, 0x80, /* ###.....# */
    0x30, 0x00, /* ..##..... */
    0x18, 0x00, /* ...##.... */
    0x0c, 0x00, /* ....##... */
    0x06, 0x00, /* .....##.. */
    0x03, 0x80, /* ......### */
    0x00, 0x00,
};

static const uint8_t KSM_ICON_GUI_9X9[18] = {
    0x63, 0x00, /* .##...##. */
    0x94, 0x80, /* #..#.#..# */
    0x7f, 0x00, /* .#######. */
    0x14, 0x00, /* ...#.#... */
    0x14, 0x00, /* ...#.#... */
    0x14, 0x00, /* ...#.#... */
    0x7f, 0x00, /* .#######. */
    0x94, 0x80, /* #..#.#..# */
    0x63, 0x00, /* .##...##. */
};

/* 7x7 lock icons (1 byte per row, bits 7..1) */
static const uint8_t KSM_ICON_CAPS_7X7[7] = {
    0x10, /* ...#... */
    0x38, /* ..###.. */
    0x6c, /* .##.##. */
    0xc6, /* ##...## */
    0x28, /* ..#.#.. */
    0x38, /* ..###.. */
    0x7c, /* .#####. */
};

static const uint8_t KSM_ICON_NUM_7X7[7] = {
    0x28, /* ..#.#.. */
    0xfe, /* ####### */
    0x28, /* ..#.#.. */
    0x28, /* ..#.#.. */
    0xfe, /* ####### */
    0x28, /* ..#.#.. */
    0x00,
};

static const uint8_t KSM_ICON_SCRL_7X7[7] = {
    0x10, /* ...#... */
    0x38, /* ..###.. */
    0x7c, /* .#####. */
    0x10, /* ...#... */
    0x7c, /* .#####. */
    0x38, /* ..###.. */
    0x10, /* ...#... */
};

/* 13x11 bold wireless signal icon (MSB-first, 2 bytes per row) */
static const uint8_t KSM_ICON_WIFI_13X11[22] = {
    0x1f, 0xc0, /* ...#######... */
    0x78, 0xf0, /* .####...####. */
    0xc0, 0x18, /* ##.........## */
    0x00, 0x00, /* ............. */
    0x0f, 0x80, /* ....#####.... */
    0x38, 0xe0, /* ..###...###.. */
    0x20, 0x20, /* ..#.......#.. */
    0x00, 0x00, /* ............. */
    0x02, 0x00, /* ......#...... */
    0x07, 0x00, /* .....###..... */
    0x02, 0x00, /* ......#...... */
};

/* 13x11 bold USB trident icon (MSB-first, 2 bytes per row) */
static const uint8_t KSM_ICON_USB_13X11[22] = {
    0x00, 0x00, /* ............. */
    0x03, 0x80, /* ......###.... */
    0x07, 0x80, /* .....####.... */
    0x0c, 0x00, /* ....##....... */
    0x78, 0x20, /* .####.....#.. */
    0xff, 0xf8, /* ############# */
    0x74, 0x20, /* .###.#....#.. */
    0x03, 0x00, /* ......##..... */
    0x01, 0xe0, /* .......####.. */
    0x00, 0xc0, /* ........##... */
    0x00, 0x00, /* ............. */
};

/* 13x11 bold disconnected X icon (MSB-first, 2 bytes per row) */
static const uint8_t KSM_ICON_OFF_13X11[22] = {
    0x00, 0x00, /* ............. */
    0x30, 0x60, /* ..##.....##.. */
    0x38, 0xe0, /* ..###...###.. */
    0x1d, 0xc0, /* ...###.###... */
    0x0f, 0x80, /* ....#####.... */
    0x07, 0x00, /* .....###..... */
    0x0f, 0x80, /* ....#####.... */
    0x1d, 0xc0, /* ...###.###... */
    0x38, 0xe0, /* ..###...###.. */
    0x30, 0x60, /* ..##.....##.. */
    0x00, 0x00, /* ............. */
};

/* 13x11 open/pairing beacon icon (dashed ring + solid center dot) */
static const uint8_t KSM_ICON_OPEN_13X11[22] = {
    0x0f, 0x80, /* ....#####.... */
    0x20, 0x20, /* ..#.......#.. */
    0x40, 0x10, /* .#.........#. */
    0x00, 0x00, /* ............. */
    0x87, 0x08, /* #....###....# */
    0x87, 0x08, /* #....###....# */
    0x87, 0x08, /* #....###....# */
    0x00, 0x00, /* ............. */
    0x40, 0x10, /* .#.........#. */
    0x20, 0x20, /* ..#.......#.. */
    0x0f, 0x80, /* ....#####.... */
};

static void ksm_blit_mask(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h,
                          const uint8_t *mask, uint8_t on) {
    int stride = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t byte = mask[row * stride + (col >> 3)];
            if (byte & (0x80u >> (col & 7))) {
                ksm_px(fb, x + col, y + row, on);
            }
        }
    }
}

static void ksm_fill_rrect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h, int r,
                           uint8_t on) {
    for (int row = 0; row < h; row++) {
        int inset = 0;
        if (r >= 4) {
            if (row == 0 || row == h - 1) inset = 3;
            else if (row == 1 || row == h - 2) inset = 2;
            else if (row == 2 || row == h - 3) inset = 1;
        } else if (r == 3) {
            if (row == 0 || row == h - 1) inset = 2;
            else if (row == 1 || row == h - 2) inset = 1;
        } else if (r >= 1) {
            if (row == 0 || row == h - 1) inset = 1;
        }
        ksm_hline(fb, x + inset, y + row, w - 2 * inset, on);
    }
}

static void ksm_stroke_rrect(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h, int r,
                             uint8_t on) {
    if (r <= 1 || w < 6 || h < 6) {
        ksm_round_rect(fb, x, y, w, h, on);
        return;
    }
    int x1 = x + w - 1;
    int y1 = y + h - 1;
    if (r >= 3) {
        ksm_hline(fb, x + 2, y, w - 4, on);
        ksm_hline(fb, x + 2, y1, w - 4, on);
        ksm_vline(fb, x, y + 2, h - 4, on);
        ksm_vline(fb, x1, y + 2, h - 4, on);
        ksm_px(fb, x + 1, y + 1, on);
        ksm_px(fb, x1 - 1, y + 1, on);
        ksm_px(fb, x + 1, y1 - 1, on);
        ksm_px(fb, x1 - 1, y1 - 1, on);
    } else {
        ksm_round_rect(fb, x, y, w, h, on);
    }
}

/* Minimal corner-bracket keycap outline for calm inactive modifiers / idle chip. */
static void draw_subtle_keycap(uint8_t fb[KSM_FB_BYTES], int x, int y, int w, int h) {
    int x1 = x + w - 1;
    int y1 = y + h - 1;
    ksm_hline(fb, x + 2, y, 4, 1);
    ksm_px(fb, x + 1, y + 1, 1);
    ksm_vline(fb, x, y + 2, 4, 1);
    ksm_hline(fb, x1 - 5, y, 4, 1);
    ksm_px(fb, x1 - 1, y + 1, 1);
    ksm_vline(fb, x1, y + 2, 4, 1);
    ksm_hline(fb, x + 2, y1, 4, 1);
    ksm_px(fb, x + 1, y1 - 1, 1);
    ksm_vline(fb, x, y1 - 5, 4, 1);
    ksm_hline(fb, x1 - 5, y1, 4, 1);
    ksm_px(fb, x1 - 1, y1 - 1, 1);
    ksm_vline(fb, x1, y1 - 5, 4, 1);
}

static void draw_centered_fitted(uint8_t fb[KSM_FB_BYTES], int box_x, int box_w, int y,
                                 const char *src, uint8_t on) {
    char buf[16];
    int max_chars = (box_w + 1) / 6;
    if (max_chars > 14) max_chars = 14;
    if (max_chars < 1) return;

    int len = 0;
    while (src && src[len] && len < 32) len++;
    if (len <= max_chars) {
        for (int i = 0; i < len; i++) buf[i] = src[i];
        buf[len] = '\0';
    } else if (max_chars >= 3) {
        for (int i = 0; i < max_chars - 2; i++) buf[i] = src[i];
        buf[max_chars - 2] = '.';
        buf[max_chars - 1] = '.';
        buf[max_chars] = '\0';
    } else {
        buf[0] = src[0];
        buf[1] = '\0';
    }
    int w = ksm_text5x7_width(buf);
    ksm_text5x7(fb, box_x + (box_w - w) / 2, y, buf, on);
}

/* Bold nice!view-proportioned battery glyph (28x11 body + 2x5 cap). */
static void draw_bold_battery(uint8_t fb[KSM_FB_BYTES], int x, int y, uint8_t level,
                              uint8_t charging) {
    const int body_w = 28;
    const int body_h = 11;
    ksm_round_rect(fb, x, y, body_w, body_h, 1);
    ksm_fill_rect(fb, x + body_w, y + 3, 2, 5, 1);

    const int inner_w = body_w - 4;
    const int inner_h = body_h - 4;
    if (level <= 100u) {
        int fill = (int)(((uint32_t)level * (uint32_t)inner_w + 50u) / 100u);
        if (level > 0u && fill == 0) fill = 1;
        if (fill > inner_w) fill = inner_w;
        if (fill > 0) {
            ksm_fill_rect(fb, x + 2, y + 2, fill, inner_h, 1);
        }
    } else {
        /* Unknown level: calm horizontal dash */
        ksm_hline(fb, x + (body_w - 6) / 2, y + body_h / 2, 6, 1);
    }

    if (charging) {
        /* Crisp 180-deg symmetric 4x7 lightning bolt with tight 1px silver halo */
        int cx = x + body_w / 2;
        int cy = y + body_h / 2;
        ksm_hline(fb, cx, cy - 3, 3, 0);
        ksm_hline(fb, cx - 1, cy - 2, 4, 0);
        ksm_hline(fb, cx - 2, cy - 1, 4, 0);
        ksm_hline(fb, cx - 2, cy, 6, 0);
        ksm_hline(fb, cx, cy + 1, 4, 0);
        ksm_hline(fb, cx - 1, cy + 2, 4, 0);
        ksm_hline(fb, cx - 1, cy + 3, 3, 0);
        ksm_px(fb, cx + 1, cy - 3, 1);
        ksm_hline(fb, cx, cy - 2, 2, 1);
        ksm_hline(fb, cx - 1, cy - 1, 2, 1);
        ksm_hline(fb, cx - 1, cy, 4, 1);
        ksm_hline(fb, cx + 1, cy + 1, 2, 1);
        ksm_hline(fb, cx, cy + 2, 2, 1);
        ksm_px(fb, cx, cy + 3, 1);
    }
}

static void draw_host_link_icon(uint8_t fb[KSM_FB_BYTES], int x, int y, const ksm_state_t *s) {
    if (s->transport == KSM_TRANSPORT_USB) {
        ksm_blit_mask(fb, x, y, 13, 11, KSM_ICON_USB_13X11, 1);
        return;
    }
    uint8_t active_bit = (uint8_t)(1u << (s->active_profile < 5u ? s->active_profile : 0u));
    if (s->profiles_connected & active_bit) {
        ksm_blit_mask(fb, x, y, 13, 11, KSM_ICON_WIFI_13X11, 1);
    } else if (s->profiles_bonded & active_bit) {
        ksm_blit_mask(fb, x, y, 13, 11, KSM_ICON_OFF_13X11, 1);
    } else {
        ksm_blit_mask(fb, x, y, 13, 11, KSM_ICON_OPEN_13X11, 1);
    }
}

/* Crisp 17x17 (r=8) Bluetooth constellation ring, matching nice!view's iconic geometry.
 * Solid ring = connected; 8-segment dashed ring = bonded; no ring = open.
 */
static void draw_profile_ring(uint8_t fb[KSM_FB_BYTES], int cx, int cy, uint8_t dashed) {
    for (int dy = -8; dy <= 8; dy++) {
        for (int dx = -8; dx <= 8; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 >= 56 && d2 <= 70) {
                if (dashed) {
                    int ax = dx < 0 ? -dx : dx;
                    int ay = dy < 0 ? -dy : dy;
                    /* Clean 8-segment breaks along diagonals and mid-octants */
                    if (ax == 0 || ay == 0) continue;
                    if (ax == ay || ax == ay - 1 || ay == ax - 1) continue;
                }
                ksm_px(fb, cx + dx, cy + dy, 1);
            }
        }
    }
}

static void draw_bt_constellation_node(uint8_t fb[KSM_FB_BYTES], int cx, int cy, uint8_t idx,
                                       uint8_t selected, uint8_t connected, uint8_t bonded,
                                       uint8_t is_usb) {
    char digit[2] = {(char)('1' + idx), '\0'};

    /* Outer status ring (connected = solid, bonded = dashed, open = none) */
    if (connected) {
        draw_profile_ring(fb, cx, cy, 0);
    } else if (bonded) {
        draw_profile_ring(fb, cx, cy, 1);
    }

    /* Inner selection disc (always clean filled disc like nice!view; USB icon in header shows host) */
    (void)is_usb;
    if (selected) {
        for (int dy = -5; dy <= 5; dy++) {
            for (int dx = -5; dx <= 5; dx++) {
                if (dx * dx + dy * dy <= 29) {
                    ksm_px(fb, cx + dx, cy + dy, 1);
                }
            }
        }
        ksm_text5x7(fb, cx - 2, cy - 3, digit, 0);
    } else {
        ksm_text5x7(fb, cx - 2, cy - 3, digit, 1);
    }
}

static void draw_mod_tile(uint8_t fb[KSM_FB_BYTES], int x, int y, uint8_t active,
                          const uint8_t *icon, int iw, int ih) {
    const int w = 28, h = 17;
    if (active) {
        ksm_fill_rrect(fb, x, y, w, h, 3, 1);
        ksm_blit_mask(fb, x + (w - iw) / 2, y + (h - ih) / 2, iw, ih, icon, 0);
    } else {
        draw_subtle_keycap(fb, x, y, w, h);
        ksm_blit_mask(fb, x + (w - iw) / 2, y + (h - ih) / 2, iw, ih, icon, 1);
    }
}

static void draw_lock_slot(uint8_t fb[KSM_FB_BYTES], int cx, int cy, uint8_t active,
                           uint8_t outline_only, const uint8_t *icon) {
    if (active) {
        ksm_fill_rrect(fb, cx - 8, cy - 5, 17, 11, 3, 1);
        ksm_blit_mask(fb, cx - 3, cy - 3, 7, 7, icon, 0);
    } else if (outline_only) {
        ksm_stroke_rrect(fb, cx - 8, cy - 5, 17, 11, 3, 1);
        ksm_blit_mask(fb, cx - 3, cy - 3, 7, 7, icon, 1);
    } else {
        ksm_fill_rect(fb, cx - 1, cy - 1, 2, 2, 1);
    }
}

void ksm_preset_render_left(uint8_t fb[KSM_FB_BYTES], const ksm_state_t *s) {
    ksm_clear(fb);

    /* 1. Pure-Symbol Top Power & Connectivity Header (y = 4..14) */
    draw_bold_battery(fb, 4, 4, s->central_battery, s->central_charging);
    draw_host_link_icon(fb, 51, 4, s);

    /* 2. Iconic 5-Circle Bluetooth Profile Constellation (y = 20..58) */
    static const int8_t NODES[5][2] = {
        {13, 28}, {54, 28}, {34, 39}, {13, 50}, {54, 50},
    };
    uint8_t is_usb = (s->transport == KSM_TRANSPORT_USB) ? 1u : 0u;
    for (uint8_t i = 0; i < 5u; i++) {
        uint8_t bit = (uint8_t)(1u << i);
        draw_bt_constellation_node(fb, NODES[i][0], NODES[i][1], i, i == s->active_profile,
                                   (s->profiles_connected & bit) != 0,
                                   (s->profiles_bonded & bit) != 0, is_usb);
    }

    /* 3. Active Layer Hero Pill + Geometric Layer Pips (y = 65..87) */
    ksm_fill_rrect(fb, 4, 65, 60, 17, 4, 1);
    const char *lname = s->layer_name[0] ? s->layer_name : "BASE";
    draw_centered_fitted(fb, 6, 56, 70, lname, 0);

    uint8_t base = s->layer >= 8u ? (uint8_t)(s->layer - 7u) : 0u;
    int px = 9;
    for (uint8_t i = 0; i < 8u; i++) {
        uint8_t idx = (uint8_t)(base + i);
        if (idx == s->layer) {
            ksm_fill_round_rect(fb, px, 85, 9, 3, 1);
            px += 12;
        } else {
            ksm_fill_rect(fb, px, 86, 2, 2, 1);
            px += 6;
        }
    }

    /* 4. Sculptural 2x2 Live Modifier Keycaps (y = 94..130) */
    draw_mod_tile(fb, 4, 94, (s->mods & KSM_MOD_SHIFT) != 0, KSM_ICON_SHIFT_9X9, 9, 9);
    draw_mod_tile(fb, 36, 94, (s->mods & KSM_MOD_CTRL) != 0, KSM_ICON_CTRL_9X7, 9, 7);
    draw_mod_tile(fb, 4, 114, (s->mods & KSM_MOD_ALT) != 0, KSM_ICON_ALT_9X7, 9, 7);
    draw_mod_tile(fb, 36, 114, (s->mods & KSM_MOD_GUI) != 0, KSM_ICON_GUI_9X9, 9, 9);

    /* 5. Whisper-Quiet Lock Trio (cy = 138) */
    uint8_t caps = (s->locks & KSM_LOCK_CAPS) != 0;
    uint8_t caps_word = (s->locks & KSM_LOCK_CAPS_WORD) != 0;
    uint8_t num = (s->locks & KSM_LOCK_NUM) != 0;
    uint8_t scrl = (s->locks & KSM_LOCK_SCROLL) != 0;
    draw_lock_slot(fb, 16, 138, caps, !caps && caps_word, KSM_ICON_CAPS_7X7);
    draw_lock_slot(fb, 34, 138, num, 0, KSM_ICON_NUM_7X7);
    draw_lock_slot(fb, 52, 138, scrl, 0, KSM_ICON_SCRL_7X7);

    /* 6. Minimal Last-Key Chip (y = 145..158) */
    if (s->last_key[0] != '\0') {
        ksm_stroke_rrect(fb, 4, 145, 60, 14, 3, 1);
        draw_centered_fitted(fb, 6, 56, 148, s->last_key, 1);
    } else {
        draw_subtle_keycap(fb, 4, 145, 60, 14);
        ksm_fill_rect(fb, 27, 151, 2, 2, 1);
        ksm_fill_rect(fb, 33, 151, 2, 2, 1);
        ksm_fill_rect(fb, 39, 151, 2, 2, 1);
    }
}

/* ============================================================================
 * Right (Peripheral): Silver-Paper Pen-and-Ink Coastal Lighthouse Horizon
 * ============================================================================
 * Full-bleed ink engraving (1 = dark ink, 0 = luminous reflective silver paper)
 * beneath the exact same symbol-only wireless header as the Left half.
 */

static uint8_t bayer4(int x, int y) {
    static const uint8_t M[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    return M[y & 3][x & 3];
}

/* Crisp woodblock cloud silhouette: open silver body, clean dark contour, light underside hatch. */
static void draw_ink_cloud(uint8_t fb[KSM_FB_BYTES], int x, int y, uint8_t large) {
    if (large) {
        static const char *const ROWS[6] = {
            ".....#####.........",
            "...##.....##.......",
            "..#.........#.###..",
            ".#...........#...#.",
            "#.................#",
            ".#################.",
        };
        for (int r = 0; r < 6; r++) {
            for (int dx = 0; ROWS[r][dx]; dx++) {
                char ch = ROWS[r][dx];
                if (ch == '#') {
                    ksm_px(fb, x + dx, y + r, 1);
                } else if (ch == '.' && r == 4 && dx >= 2 && dx <= 16 && ((dx & 1) == 0)) {
                    ksm_px(fb, x + dx, y + r, 1);
                }
            }
        }
    } else {
        static const char *const ROWS[4] = {
            "...####.....",
            ".##....##...",
            "#........##.",
            ".##########.",
        };
        for (int r = 0; r < 4; r++) {
            for (int dx = 0; ROWS[r][dx]; dx++) {
                if (ROWS[r][dx] == '#') {
                    ksm_px(fb, x + dx, y + r, 1);
                }
            }
        }
    }
}

/* Tiny soaring coastal gull silhouette. */
static void draw_gull(uint8_t fb[KSM_FB_BYTES], int x, int y) {
    ksm_px(fb, x, y, 1);
    ksm_px(fb, x + 1, y, 1);
    ksm_px(fb, x + 2, y + 1, 1);
    ksm_px(fb, x + 3, y, 1);
    ksm_px(fb, x + 4, y, 1);
}

/* Crisp dark ink evergreen silhouette standing on the coastal bluff. */
static void draw_ink_pine(uint8_t fb[KSM_FB_BYTES], int cx, int base_y, int h) {
    for (int row = 0; row < h; row++) {
        int y = base_y - h + row;
        int tier = row % 3;
        int half = (row / 3) + (tier == 2 ? 1 : 0);
        if (row == 0 || row == h - 1) half = 0;
        ksm_hline(fb, cx - half, y, half * 2 + 1, 1);
    }
}

/* Distant coastal mountain ridge across the left bay (x = 0..36). */
static int island_ridge_y(int x) {
    if (x < 0 || x > 36) return 115;
    if (x < 12) return 109 - (x * 11) / 12;        /* climbs to main peak (12, 98) */
    if (x < 21) return 98 + ((x - 12) * 11) / 9;   /* dips to saddle (21, 109) */
    if (x < 27) return 109 - ((x - 21) * 6) / 6;   /* rises to east shoulder (27, 103) */
    return 103 + ((x - 27) * 12) / 9;              /* slopes down into bay */
}

/* Foreground rocky headland cliff profile (x = 24..67). */
static int headland_y(int x) {
    if (x < 24) return 119;
    if (x < 31) return 117 - (x - 24) * 2;         /* steep rocky slope into bay */
    if (x < 43) return 105 - (x - 31) / 6;         /* cottage terrace */
    if (x < 58) return 103;                        /* lighthouse plateau */
    return 104 + (x - 58) / 2;                     /* outer cliff shoulder */
}

void ksm_preset_render_right(uint8_t fb[KSM_FB_BYTES], const ksm_state_t *s) {
    ksm_clear(fb);

    /* 1. Matching Pure-Symbol Top Wireless Header (y = 4..14) */
    draw_bold_battery(fb, 4, 4, s->peripheral_battery, s->peripheral_charging);
    if (s->peripheral_connected) {
        ksm_blit_mask(fb, 51, 4, 13, 11, KSM_ICON_WIFI_13X11, 1);
    } else {
        ksm_blit_mask(fb, 51, 4, 13, 11, KSM_ICON_OFF_13X11, 1);
    }

    /* 2. Crisp Crescent Moon (upper-left sky, cx = 14, cy = 30) */
    const int mx = 14, my = 30;
    for (int dy = -7; dy <= 7; dy++) {
        for (int dx = -7; dx <= 7; dx++) {
            int r_outer = dx * dx + dy * dy;
            int ox = dx - 3;
            int oy = dy + 2;
            int r_inner = ox * ox + oy * oy;
            if (r_outer <= 48 && r_inner > 34) {
                ksm_px(fb, mx + dx, my + dy, 1);
            }
        }
    }

    /* 3. Graceful Soaring Seabirds & Minimal Sky Glints */
    draw_gull(fb, 28, 25);
    draw_gull(fb, 36, 30);
    ksm_px(fb, 54, 24, 1);
    ksm_px(fb, 60, 29, 1);
    ksm_px(fb, 22, 41, 1);

    /* 4. Airy Woodblock Clouds */
    draw_ink_cloud(fb, 42, 35, 1);
    draw_ink_cloud(fb, 3, 66, 0);

    /* 5. Delicate Stippled Lighthouse Beam (ONLY when peripheral link is active) */
    const int lx = 50;
    const int lan_y = 58;
    if (s->peripheral_connected) {
        for (int x = 3; x <= 44; x++) {
            int dist = 44 - x; /* 0 at lantern .. 41 at left edge */
            int center_y = lan_y - (dist * 3) / 14;
            int half_h = 2 + (dist * 7) / 30;
            /* Delicate tapering ray hairlines near the lantern */
            if ((dist < 14 && ((dist & 1) == 0)) || (dist >= 14 && dist < 24 && (dist % 3) == 0)) {
                ksm_px(fb, x, center_y - half_h, 1);
                ksm_px(fb, x, center_y + half_h, 1);
            }
            for (int y = center_y - half_h + 1; y <= center_y + half_h - 1; y++) {
                int dy = y - center_y;
                if (dy < 0) dy = -dy;
                int edge = half_h > 0 ? ((half_h - dy) * 8) / (half_h + 1) : 6;
                int fade = 6 - (dist * 5) / 42;
                int density = (edge * fade) / 7; /* airy 0..5 out of 16 Bayer levels */
                if (density > 5) density = 5;
                if (density > (int)bayer4(x, y)) {
                    ksm_px(fb, x, y, 1);
                }
            }
        }
        /* Subtle short counter-ray glint to the right */
        for (int x = 56; x <= 65; x++) {
            int dist = x - 56;
            int center_y = lan_y + (dist * 2) / 9;
            int half_h = 2 + dist / 4;
            for (int y = center_y - half_h; y <= center_y + half_h; y++) {
                int dy = y - center_y;
                if (dy < 0) dy = -dy;
                int density = ((half_h - dy) * (4 - dist / 3)) / (half_h + 1);
                if (density > (int)bayer4(x, y)) {
                    ksm_px(fb, x, y, 1);
                }
            }
        }
    }

    /* 6. Distant Mountain / Island Ridge with nice!view Halftone Shadow Slopes */
    for (int x = 0; x <= 36; x++) {
        int ry = island_ridge_y(x);
        if (ry >= 115) continue;
        int ny = x < 36 ? island_ridge_y(x + 1) : ry;
        uint8_t shadow_slope = (ny > ry) ? 1u : 0u;
        /* Clear sky behind mountain body */
        for (int y = ry; y <= 114; y++) {
            ksm_px(fb, x, y, 0);
        }
        ksm_px(fb, x, ry, 1);
        for (int y = ry + 1; y <= 114; y++) {
            int depth = y - ry;
            if (shadow_slope) {
                int tone = depth < 6 ? 9 : 6;
                if (tone > (int)bayer4(x, y)) ksm_px(fb, x, y, 1);
            } else if (depth >= 4 && ((x + y) & 7) == 0) {
                ksm_px(fb, x, y, 1);
            }
        }
    }

    /* 7. Architectural Nautical Striped Lighthouse (cx = 50, y = 46..104) */
    /* Finial & dark dome cupola (y = 46..53) */
    ksm_px(fb, lx, 46, 1);
    ksm_px(fb, lx, 47, 1);
    ksm_hline(fb, lx - 1, 48, 3, 1);
    ksm_hline(fb, lx - 2, 49, 5, 1);
    ksm_hline(fb, lx - 3, 50, 7, 1);
    ksm_hline(fb, lx - 4, 51, 9, 1);
    ksm_hline(fb, lx - 5, 52, 11, 1);

    /* Lantern glass room (y = 53..60) */
    ksm_fill_rect(fb, lx - 4, 53, 9, 8, 0);
    ksm_vline(fb, lx - 4, 53, 8, 1);
    ksm_vline(fb, lx + 4, 53, 8, 1);
    if (s->peripheral_connected) {
        /* Bright glowing open glass with delicate vertical mullions & radiant core */
        ksm_vline(fb, lx - 2, 53, 8, 1);
        ksm_vline(fb, lx + 2, 53, 8, 1);
        ksm_px(fb, lx, 56, 1);
        ksm_hline(fb, lx - 1, 57, 3, 1);
        ksm_px(fb, lx, 58, 1);
    } else {
        /* Shuttered dark lantern when split link is off */
        ksm_fill_rect(fb, lx - 3, 54, 7, 6, 1);
    }

    /* Gallery balcony deck & railing (y = 61..63) */
    ksm_hline(fb, lx - 6, 61, 13, 1);
    ksm_hline(fb, lx - 5, 62, 11, 0);
    ksm_px(fb, lx - 5, 62, 1);
    ksm_px(fb, lx - 2, 62, 1);
    ksm_px(fb, lx + 2, 62, 1);
    ksm_px(fb, lx + 5, 62, 1);
    ksm_hline(fb, lx - 6, 63, 13, 1);

    /* Tapered nautical striped tower (y = 64..103): crisp alternating ink & silver bands */
    for (int row = 0; row <= 39; row++) {
        int y = 64 + row;
        int half = 4 + (row * 3) / 39; /* 9px wide at top -> 15px wide at base */
        uint8_t dark_band = (row >= 9 && row <= 16) || (row >= 25 && row <= 32);
        for (int dx = -half; dx <= half; dx++) {
            uint8_t ink;
            if (dx == -half || dx == half) {
                ink = 1;
            } else if (dark_band) {
                ink = 1;
            } else if (dx >= half - 2) {
                /* Delicate cylindrical shadow along the right wall */
                ink = ((dx + y) & 1) == 0 ? 1u : 0u;
            } else {
                ink = 0;
            }
            ksm_px(fb, lx + dx, y, ink);
        }
    }
    /* Crisp tiny window slits & arched doorway on the silver tower bands */
    ksm_vline(fb, lx, 67, 3, 1);
    ksm_vline(fb, lx, 83, 3, 1);
    ksm_fill_rect(fb, lx - 1, 99, 3, 5, 1);
    ksm_px(fb, lx - 1, 99, 0);
    ksm_px(fb, lx + 1, 99, 0);

    /* 8. Keeper's Cottage beside the tower (x = 29..42, y = 93..105) */
    /* Tiny chimney & gentle curling smoke wisp */
    ksm_fill_rect(fb, 31, 92, 2, 4, 1);
    ksm_px(fb, 30, 90, 1);
    ksm_px(fb, 29, 88, 1);
    ksm_px(fb, 30, 86, 1);
    /* Pitched dark roof */
    for (int r = 0; r < 5; r++) {
        int y = 93 + r;
        int x0 = 35 - r * 3 / 2;
        int x1 = 37 + r * 3 / 2;
        if (x0 < 29) x0 = 29;
        if (x1 > 43) x1 = 43;
        ksm_hline(fb, x0, y, x1 - x0 + 1, 1);
    }
    /* Bright whitewashed cottage walls with dark outline & cozy window */
    ksm_fill_rect(fb, 30, 98, 13, 8, 0);
    ksm_vline(fb, 30, 98, 8, 1);
    ksm_hline(fb, 30, 98, 13, 1);
    ksm_fill_rect(fb, 33, 100, 3, 3, 1);
    ksm_fill_rect(fb, 38, 101, 2, 4, 1);

    /* 9. Foreground Rocky Headland Cliff (x = 24..67, y = 103..119) */
    for (int x = 24; x < (int)KSM_WIDTH; x++) {
        int cy = headland_y(x);
        for (int y = cy; y <= 118; y++) {
            ksm_px(fb, x, y, 0);
        }
        ksm_px(fb, x, cy, 1);
        ksm_px(fb, x, cy + 1, 1);
        for (int y = cy + 2; y <= 118; y++) {
            int depth = y - cy;
            /* Bold shaded cliff face with crisp rock striations */
            int tone = depth <= 4 ? 12 : (depth <= 9 ? 9 : 5);
            if (tone > (int)bayer4(x, y)) {
                ksm_px(fb, x, y, 1);
            }
        }
    }

    /* Coastal evergreen pines on the headland */
    draw_ink_pine(fb, 27, 109, 8);
    draw_ink_pine(fb, 62, 105, 9);

    /* 10. Calm Water Horizon Line (y = 115) */
    for (int x = 0; x <= 25; x++) {
        if ((x % 9) != 5) ksm_px(fb, x, 115, 1);
    }

    /* 11. Graceful Sailboat Gliding in the Bay (x = 9..22, y = 110..124) */
    ksm_fill_rect(fb, 9, 111, 13, 13, 0);
    ksm_vline(fb, 15, 110, 11, 1); /* Mast */
    /* Dark triangular mainsail (right of mast) */
    ksm_px(fb, 16, 112, 1);
    ksm_hline(fb, 16, 113, 2, 1);
    ksm_hline(fb, 16, 114, 2, 1);
    ksm_hline(fb, 16, 115, 3, 1);
    ksm_hline(fb, 16, 116, 4, 1);
    ksm_hline(fb, 16, 117, 4, 1);
    ksm_hline(fb, 16, 118, 5, 1);
    ksm_hline(fb, 16, 119, 6, 1);
    /* Crisp outlined jib sail (left of mast) */
    ksm_px(fb, 14, 113, 1);
    ksm_px(fb, 13, 114, 1);
    ksm_px(fb, 13, 115, 1);
    ksm_px(fb, 12, 116, 1);
    ksm_px(fb, 12, 117, 1);
    ksm_px(fb, 11, 118, 1);
    ksm_hline(fb, 11, 119, 4, 1);
    /* Sleek dark hull */
    ksm_hline(fb, 10, 121, 12, 1);
    ksm_hline(fb, 11, 122, 10, 1);
    ksm_hline(fb, 13, 123, 6, 1);

    /* 12. Shimmering Open Ocean Ripples (y = 121..157) — fading softly into silver paper */
    static const uint8_t BOAT_RIPPLES[8][3] = {
        {10, 126, 11}, {12, 129, 8}, {7, 133, 13}, {11, 137, 9},
        {9, 141, 10},  {13, 145, 7}, {10, 149, 7}, {14, 154, 4},
    };
    for (int i = 0; i < 8; i++) {
        ksm_hline(fb, BOAT_RIPPLES[i][0], BOAT_RIPPLES[i][1], BOAT_RIPPLES[i][2], 1);
    }

    static const uint8_t CLIFF_RIPPLES[10][3] = {
        {34, 121, 26}, {39, 124, 18}, {32, 128, 24}, {41, 132, 15}, {37, 136, 19},
        {43, 140, 13}, {40, 144, 15}, {45, 148, 10}, {43, 152, 11}, {47, 156, 6},
    };
    for (int i = 0; i < 10; i++) {
        if (!s->peripheral_connected && (i & 1) != 0) continue;
        ksm_hline(fb, CLIFF_RIPPLES[i][0], CLIFF_RIPPLES[i][1], CLIFF_RIPPLES[i][2], 1);
    }

    /* Quiet mid-bay water glints */
    ksm_hline(fb, 25, 125, 5, 1);
    ksm_hline(fb, 27, 131, 4, 1);
    ksm_hline(fb, 24, 138, 5, 1);
    ksm_hline(fb, 28, 146, 4, 1);
    ksm_hline(fb, 26, 153, 3, 1);
}
