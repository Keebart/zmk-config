/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_status {
    sys_snode_t node;
    lv_obj_t *obj;
#if !IS_ENABLED(CONFIG_KEEBART_STUDIO) && !IS_ENABLED(CONFIG_KEEBART_STUDIO_PERIPHERAL)
    lv_color_t cbuf[CANVAS_SIZE * CANVAS_SIZE];
    lv_color_t cbuf2[CANVAS_SIZE * CANVAS_SIZE];
    lv_color_t cbuf3[CANVAS_SIZE * CANVAS_SIZE];
    struct status_state state;
#if IS_ENABLED(CONFIG_KEEBART_STUDIO)
    char studio_layer_label[129];
#endif
#endif
};

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget);
