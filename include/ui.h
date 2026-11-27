#ifndef MINI_ART_UI_H
#define MINI_ART_UI_H

#include <stdint.h>

/* duration is Toast.LENGTH_SHORT (0) or Toast.LENGTH_LONG (1). */
int ui_show_toast(const char *text, int32_t duration);

void *ui_content_begin(void);
void *ui_content_box(void *parent, int horizontal);
void *ui_content_label(void *parent, const char *text);
void *ui_content_button(void *parent, const char *text, void *user);
void ui_content_set_text(void *widget, const char *text);
void ui_content_on_click(void (*fn)(void *user));
void ui_canvas_begin(void);
void ui_canvas_rect(float left, float top, float right, float bottom, uint32_t color,
                    uint32_t color1);
void ui_canvas_text(const char *text, float x, float y, uint32_t color);
void ui_canvas_post(void);
void ui_set_title(const char *title);
void ui_set_touch_handler(void (*fn)(float x, float y, int action));
void ui_run(void);

#endif
