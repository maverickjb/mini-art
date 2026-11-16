#include "ui_internal.h"
#include "ui.h"

#include "lvgl.h"
#include "SDL2/SDL.h"

#include <stdio.h>

static lv_display_t *display;

int ui_open_display(void)
{
    SDL_Window *window;

    if (display != NULL) {
        return 0;
    }
    lv_init();
    display = lv_sdl_window_create(360, 640);
    if (display == NULL) {
        fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
        return -1;
    }
    lv_sdl_window_set_title(display, "mini-art");
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101014), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_sdl_mouse_create();
    window = lv_sdl_window_get_window(display);
    if (window != NULL) {
        SDL_SetWindowPosition(window, 80, 80);
        SDL_SetWindowAlwaysOnTop(window, SDL_TRUE);
        SDL_ShowWindow(window);
        SDL_RaiseWindow(window);
    }
    return 0;
}

void ui_pump(uint32_t duration_ms)
{
    uint32_t start = lv_tick_get();

    lv_refr_now(display);
    while (lv_tick_elaps(start) < duration_ms) {
        lv_timer_handler();
        SDL_Delay(5);
    }
}

static void (*content_click)(void *user);

static void display_text(const char *text, char *out, size_t cap)
{
    size_t n = 0;
    const unsigned char *s = (const unsigned char *)(text != NULL ? text : "");

    while (*s != 0 && n + 5 < cap) {
        if (s[0] == 0xc2 && s[1] == 0xb2) {
            out[n++] = '^';
            out[n++] = '2';
            s += 2;
        } else if (s[0] == 0xc3 && s[1] == 0x97) {
            out[n++] = '*';
            s += 2;
        } else if (s[0] == 0xc3 && s[1] == 0xb7) {
            out[n++] = '/';
            s += 2;
        } else if (s[0] == 0xe2 && s[1] == 0x88 && s[2] == 0x9a) {
            out[n++] = 's';
            out[n++] = 'q';
            out[n++] = 'r';
            out[n++] = 't';
            s += 3;
        } else if (s[0] == 0xcf && s[1] == 0x80) {
            out[n++] = 'p';
            out[n++] = 'i';
            s += 2;
        } else {
            out[n++] = (char)*s++;
        }
    }
    out[n] = '\0';
}

static void on_content_clicked(lv_event_t *event)
{
    if (content_click != NULL) {
        content_click(lv_event_get_user_data(event));
    }
}

static void box_style(lv_obj_t *box, int horizontal)
{
    lv_obj_set_width(box, lv_pct(100));
    lv_obj_set_height(box, lv_pct(100));
    lv_obj_set_flex_flow(box, horizontal ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_pad_gap(box, 2, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
}

void *ui_content_begin(void)
{
    lv_obj_t *screen;
    SDL_Window *window;

    if (ui_open_display() != 0) {
        return NULL;
    }
    screen = lv_screen_active();
    lv_obj_clean(screen);
    box_style(screen, 0);
    lv_obj_set_flex_grow(screen, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    window = lv_sdl_window_get_window(display);
    if (window != NULL) {
        SDL_SetWindowSize(window, 480, 800);
        SDL_SetWindowAlwaysOnTop(window, SDL_FALSE);
        lv_sdl_window_set_title(display, "Calculator");
    }
    return screen;
}

void *ui_content_box(void *parent, int horizontal)
{
    lv_obj_t *box;

    if (parent == NULL) {
        return NULL;
    }
    box = lv_obj_create(parent);
    box_style(box, horizontal);
    return box;
}

void *ui_content_label(void *parent, const char *text)
{
    lv_obj_t *label;

    if (parent == NULL) {
        return NULL;
    }
    char shown[64];

    display_text(text, shown, sizeof shown);
    label = lv_label_create(parent);
    lv_label_set_text(label, shown);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_min_height(label, 36, 0);
    lv_obj_set_style_pad_right(label, 12, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    return label;
}

void *ui_content_button(void *parent, const char *text, void *user)
{
    lv_obj_t *button;
    lv_obj_t *label;

    if (parent == NULL) {
        return NULL;
    }
    button = lv_button_create(parent);
    lv_obj_set_flex_grow(button, 1);
    lv_obj_set_height(button, lv_pct(100));
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1c1c1c), 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    char shown[64];

    display_text(text, shown, sizeof shown);
    label = lv_label_create(button);
    lv_label_set_text(label, shown);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffa500), 0);
    lv_obj_center(label);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(button, on_content_clicked, LV_EVENT_CLICKED, user);
    return button;
}

void ui_content_set_text(void *widget, const char *text)
{
    lv_obj_t *obj = widget;
    lv_obj_t *child;

    if (obj == NULL) {
        return;
    }
    child = lv_obj_get_child(obj, 0);
    if (child != NULL && lv_obj_check_type(child, &lv_label_class)) {
        obj = child;
    }
    char shown[256];

    display_text(text, shown, sizeof shown);
    lv_label_set_text(obj, shown);
}

void ui_content_on_click(void (*fn)(void *user))
{
    content_click = fn;
}

void ui_run(void)
{
    SDL_Window *window;
    Uint32 window_id;

    if (display == NULL) {
        return;
    }
    window = lv_sdl_window_get_window(display);
    window_id = window != NULL ? SDL_GetWindowID(window) : 0;
    lv_refr_now(display);
    while (window_id != 0 && SDL_GetWindowFromID(window_id) != NULL) {
        lv_timer_handler();
        SDL_Delay(5);
    }
    display = NULL;
}
