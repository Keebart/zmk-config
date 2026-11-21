/*
 * Generated 1-bpp Sharp MiP font for lv_font_montserrat_16 (4 glyphs, 124 B bitmap).
 * Derived from upstream LVGL Montserrat 4-bpp source using the exact LV_COLOR_DEPTH == 1
 * LV_OPA_50 threshold (nibble >= 8). Regenerate with scripts/generate-mip-fonts.py.
 */

#include <lvgl.h>

#ifndef LV_FONT_MONTSERRAT_16
    #define LV_FONT_MONTSERRAT_16 1
#endif

#if LV_FONT_MONTSERRAT_16

static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    0x00, 0x1c, 0x1f, 0xc7, 0xbd, 0xe3, 0xf8, 0x3e, 0x07, 0xc1, 0xfc, 0x7f,
    0xde, 0x3f, 0x83, 0xa0, 0x20, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x3f,
    0xfc, 0x7f, 0xfe, 0x7f, 0xfe, 0x7c, 0x3e, 0x3c, 0x3c, 0x3c, 0x3c, 0x7c,
    0x3e, 0x7f, 0xfe, 0x7f, 0xfe, 0x3f, 0xfc, 0x03, 0xc0, 0x03, 0xc0, 0x03,
    0xc0, 0x00, 0x00, 0x00, 0x3f, 0xc0, 0x0f, 0xff, 0x03, 0xff, 0xfc, 0x78,
    0x01, 0xee, 0x00, 0x07, 0x41, 0xf8, 0x20, 0x7f, 0xe0, 0x0f, 0xff, 0x00,
    0xc0, 0x30, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0xf0, 0x00, 0x0f, 0x00,
    0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x3e, 0x00, 0x03,
    0x70, 0x00, 0x10, 0x00, 0x39, 0x80, 0x03, 0xc8, 0x03, 0x9f, 0xff, 0xfe,
    0xf0, 0xc0, 0x40, 0x02, 0x00, 0x00, 0x1b, 0xc0, 0x00, 0x7e, 0x00, 0x00,
    0xf0, 0x00, 0x00, 0x00
};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 176, .box_w = 11, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 17, .adv_w = 256, .box_w = 16, .box_h = 16, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 49, .adv_w = 320, .box_w = 20, .box_h = 15, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 87, .adv_w = 320, .box_w = 21, .box_h = 14, .ofs_x = 0, .ofs_y = -1}
};

static const uint16_t unicode_list_0[] = {
    0x0, 0x6, 0x1de, 0x27a
};

static const lv_font_fmt_txt_cmap_t cmaps[] = {
    {
        .range_start = 61453, .range_length = 635, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
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
const lv_font_t lv_font_montserrat_16 = {
#else
lv_font_t lv_font_montserrat_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = 18,
    .base_line = 3,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc
};

#endif /* LV_FONT_MONTSERRAT_16 */
