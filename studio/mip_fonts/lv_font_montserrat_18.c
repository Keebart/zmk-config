/*
 * Generated 1-bpp Sharp MiP font for lv_font_montserrat_18 (5 glyphs, 80 B bitmap).
 * Derived from upstream LVGL Montserrat 4-bpp source using the exact LV_COLOR_DEPTH == 1
 * LV_OPA_50 threshold (nibble >= 8). Regenerate with scripts/generate-mip-fonts.py.
 */

#include <lvgl.h>

#ifndef LV_FONT_MONTSERRAT_18
    #define LV_FONT_MONTSERRAT_18 1
#endif

#if LV_FONT_MONTSERRAT_18

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    0xff, 0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc6, 0x31, 0x80, 0x3e, 0x1f, 0xc4,
    0x18, 0x06, 0x01, 0x80, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0xfe,
    0x7f, 0xc0, 0x7f, 0x9f, 0xe0, 0x30, 0x18, 0x0c, 0x03, 0x00, 0xf0, 0x06,
    0x00, 0x80, 0x24, 0x1b, 0xfe, 0x3e, 0x00, 0x01, 0x80, 0x30, 0x06, 0x00,
    0xc0, 0x0c, 0x01, 0x88, 0x30, 0x86, 0x08, 0x7f, 0xf7, 0xff, 0x00, 0x80,
    0x08, 0x00, 0x80, 0x3f, 0x8f, 0xe2, 0x00, 0x80, 0x60, 0x1f, 0x83, 0xf8,
    0x06, 0x00, 0xc0, 0x34, 0x19, 0xfe, 0x3e, 0x00
};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 107, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 9, .adv_w = 165, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 165, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 43, .adv_w = 193, .box_w = 12, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 165, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = 0}
};

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 49, .range_length = 5, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

#if LV_VERSION_CHECK(8, 0, 0)
static lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};

#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t lv_font_montserrat_18 = {
#else
lv_font_t lv_font_montserrat_18 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 21,
    .base_line = 4,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc
};

#endif /* LV_FONT_MONTSERRAT_18 */
