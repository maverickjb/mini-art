#include "ui.h"

#include "ui_internal.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>

int ui_show_toast(const char *text, int32_t duration)
{
    uint32_t ms = duration == 1 ? 3500u : 2000u;
    lv_obj_t *label;

    if (text == NULL || ui_open_display() != 0) {
        return -1;
    }
    label = lv_label_create(lv_screen_active());
    if (label == NULL) {
        return -1;
    }
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(label, 18, 0);
    lv_obj_set_style_radius(label, 16, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -48);
    fprintf(stderr, "toast window: %s (%u ms)\n", text, ms);
    ui_pump(ms);
    lv_obj_delete(label);
    return 0;
}
