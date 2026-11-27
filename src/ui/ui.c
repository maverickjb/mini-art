#include "ui_internal.h"
#include "ui.h"

#include "lvgl.h"
#include "SDL2/SDL.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static lv_display_t *display;
static lv_indev_t *pointer;
static lv_obj_t *canvas_layer;
static char window_title[256];
static int window_title_set;
static void (*touch_handler)(float x, float y, int action);
static int touch_down;
static lv_point_t touch_point;

void ui_set_touch_handler(void (*fn)(float x, float y, int action))
{
    touch_handler = fn;
}

static void poll_touch(void)
{
    lv_point_t point;
    int down;

    if (pointer == NULL || touch_handler == NULL) {
        return;
    }
    lv_indev_get_point(pointer, &point);
    down = lv_indev_get_state(pointer) == LV_INDEV_STATE_PRESSED;
    if (down && !touch_down) {
        touch_handler((float)point.x, (float)point.y, 0);
    } else if (down && (point.x != touch_point.x || point.y != touch_point.y)) {
        touch_handler((float)point.x, (float)point.y, 2);
    } else if (!down && touch_down) {
        touch_handler((float)touch_point.x, (float)touch_point.y, 1);
    }
    touch_down = down;
    touch_point = point;
}

void ui_set_title(const char *title)
{
    if (title == NULL) {
        return;
    }
    snprintf(window_title, sizeof(window_title), "%s", title);
    window_title_set = 1;
    if (display != NULL) {
        lv_sdl_window_set_title(display, window_title);
    }
}

static void attach_cursor(void)
{
    lv_obj_t *cursor;
    SDL_Cursor *arrow;

    if (pointer == NULL) {
        return;
    }
    arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    if (arrow != NULL) {
        SDL_SetCursor(arrow);
    }
    SDL_ShowCursor(SDL_ENABLE);
    cursor = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cursor, 16, 16);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(0xffa500), 0);
    lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(cursor, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(cursor, 2, 0);
    lv_obj_set_style_pad_all(cursor, 0, 0);
    /* LVGL pins the object's top-left to the pointer. Shift by half so the
     * circle sits on the system cursor hotspot. */
    lv_obj_set_style_translate_x(cursor, -8, 0);
    lv_obj_set_style_translate_y(cursor, -8, 0);
    lv_indev_set_display(pointer, display);
    lv_indev_set_cursor(pointer, cursor);
}

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
    lv_sdl_window_set_title(display, window_title_set ? window_title : "mini-art");
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101014), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    pointer = lv_sdl_mouse_create();
    attach_cursor();
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
    canvas_layer = NULL;
    box_style(screen, 0);
    lv_obj_set_flex_grow(screen, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    window = lv_sdl_window_get_window(display);
    /* SDL_SetWindowSize alone leaves the LVGL display at 360x640, so pointer
     * events (window pixels) and the drawn cursor (display pixels) diverge. */
    lv_display_set_resolution(display, 480, 800);
    if (window != NULL) {
        SDL_SetWindowAlwaysOnTop(window, SDL_FALSE);
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

enum {
    CANVAS_RECT = 1,
    CANVAS_TEXT = 2,
    CANVAS_LINE = 3
};

#define CANVAS_CMD_MAX 1024

struct CanvasCmd {
    int kind;
    float left;
    float top;
    float right;
    float bottom;
    uint32_t color;
    uint32_t color1;
    float radius;
    float size;
    char text[96];
};

struct CanvasFrame {
    struct CanvasCmd cmds[CANVAS_CMD_MAX];
    int count;
};

static pthread_mutex_t canvas_mu = PTHREAD_MUTEX_INITIALIZER;
static struct CanvasFrame canvas_building;
static struct CanvasFrame canvas_pending;
static struct CanvasFrame canvas_shown;
static int canvas_pending_ready;

static void ui_canvas_apply(void);

static struct CanvasCmd *canvas_push(int kind)
{
    struct CanvasCmd *cmd;

    if (canvas_building.count >= CANVAS_CMD_MAX) {
        return NULL;
    }
    cmd = &canvas_building.cmds[canvas_building.count++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->kind = kind;
    return cmd;
}

void ui_canvas_begin(void)
{
    pthread_mutex_lock(&canvas_mu);
    canvas_building.count = 0;
    pthread_mutex_unlock(&canvas_mu);
}

void ui_canvas_rect(float left, float top, float right, float bottom, uint32_t color,
                    uint32_t color1)
{
    struct CanvasCmd *cmd;

    pthread_mutex_lock(&canvas_mu);
    cmd = canvas_push(CANVAS_RECT);
    if (cmd != NULL) {
        cmd->left = left;
        cmd->top = top;
        cmd->right = right;
        cmd->bottom = bottom;
        cmd->color = color;
        cmd->color1 = color1;
    }
    pthread_mutex_unlock(&canvas_mu);
}

void ui_canvas_round_rect(float left, float top, float right, float bottom, float radius,
                          uint32_t color, uint32_t color1)
{
    struct CanvasCmd *cmd;

    pthread_mutex_lock(&canvas_mu);
    cmd = canvas_push(CANVAS_RECT);
    if (cmd != NULL) {
        cmd->left = left;
        cmd->top = top;
        cmd->right = right;
        cmd->bottom = bottom;
        cmd->color = color;
        cmd->color1 = color1;
        cmd->radius = radius;
    }
    pthread_mutex_unlock(&canvas_mu);
}

void ui_canvas_line(float x1, float y1, float x2, float y2, float stroke, uint32_t color)
{
    struct CanvasCmd *cmd;

    pthread_mutex_lock(&canvas_mu);
    cmd = canvas_push(CANVAS_LINE);
    if (cmd != NULL) {
        cmd->left = x1;
        cmd->top = y1;
        cmd->right = x2;
        cmd->bottom = y2;
        cmd->color = color;
        cmd->color1 = color;
        cmd->radius = stroke;
    }
    pthread_mutex_unlock(&canvas_mu);
}

void ui_canvas_text(const char *text, float x, float y, float size, uint32_t color)
{
    struct CanvasCmd *cmd;

    pthread_mutex_lock(&canvas_mu);
    cmd = canvas_push(CANVAS_TEXT);
    if (cmd != NULL) {
        cmd->left = x;
        cmd->top = y;
        cmd->color = color;
        cmd->color1 = color;
        cmd->size = size;
        if (text != NULL) {
            strncpy(cmd->text, text, sizeof(cmd->text) - 1);
        }
    }
    pthread_mutex_unlock(&canvas_mu);
}

void ui_canvas_post(void)
{
    pthread_mutex_lock(&canvas_mu);
    canvas_pending = canvas_building;
    canvas_pending_ready = 1;
    canvas_building.count = 0;
    pthread_mutex_unlock(&canvas_mu);
}

static uint32_t mix_color(uint32_t start, uint32_t end, int step, int steps)
{
    uint32_t mixed = 0;
    int shift;

    if (steps <= 1) {
        return start;
    }
    for (shift = 0; shift < 32; shift += 8) {
        int from = (int)((start >> shift) & 0xff);
        int to = (int)((end >> shift) & 0xff);
        int channel = from + (to - from) * step / (steps - 1);
        mixed |= (uint32_t)channel << shift;
    }
    return mixed;
}

static void paint_rect(float left, float top, float right, float bottom, uint32_t color,
                       float radius)
{
    lv_obj_t *box;
    int x = (int)left;
    int y = (int)top;
    int width = (int)(right - left);
    int height = (int)(bottom - top);

    if (width < 1 || height < 1) {
        return;
    }
    box = lv_obj_create(canvas_layer);
    lv_obj_set_flag(box, LV_OBJ_FLAG_IGNORE_LAYOUT, true);
    lv_obj_set_flag(box, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_radius(box, (int32_t)radius, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(color & 0x00ffffffu), 0);
    lv_obj_set_style_bg_opa(box, (lv_opa_t)((color >> 24) & 0xffu), 0);
}

static void paint_line(float x1, float y1, float x2, float y2, float stroke, uint32_t color)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float half;

    if (stroke < 1.0f) {
        return;
    }
    half = stroke / 2.0f;

    if (dx < 0.0f) {
        dx = -dx;
    }
    if (dy < 0.0f) {
        dy = -dy;
    }
    if (dx >= dy) {
        float left = x1 < x2 ? x1 : x2;
        float right = x1 < x2 ? x2 : x1;
        float mid = (y1 + y2) / 2.0f;
        paint_rect(left, mid - half, right, mid + half, color, 0.0f);
    } else {
        float top = y1 < y2 ? y1 : y2;
        float bottom = y1 < y2 ? y2 : y1;
        float mid = (x1 + x2) / 2.0f;
        paint_rect(mid - half, top, mid + half, bottom, color, 0.0f);
    }
}

static const lv_font_t *font_for_size(float size)
{
    if (size > 0.0f && size <= 13.0f) {
        return &lv_font_montserrat_12;
    }
    if (size > 0.0f && size <= 16.0f) {
        return &lv_font_montserrat_14;
    }
    if (size > 0.0f && size <= 24.0f) {
        return &lv_font_montserrat_20;
    }
    return &lv_font_montserrat_28;
}

static void paint_command(const struct CanvasCmd *cmd)
{
    if (cmd->kind == CANVAS_TEXT) {
        lv_obj_t *label;
        const lv_font_t *font = font_for_size(cmd->size);
        int y = (int)cmd->top - (int)font->line_height;

        label = lv_label_create(canvas_layer);
        lv_label_set_text(label, cmd->text);
        lv_obj_set_flag(label, LV_OBJ_FLAG_IGNORE_LAYOUT, true);
        lv_obj_set_pos(label, (int)cmd->left, y < 0 ? 0 : y);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(cmd->color & 0x00ffffffu), 0);
        return;
    }
    if (cmd->kind == CANVAS_LINE) {
        paint_line(cmd->left, cmd->top, cmd->right, cmd->bottom, cmd->radius, cmd->color);
        return;
    }
    if (cmd->color == cmd->color1 || cmd->radius > 0.0f) {
        paint_rect(cmd->left, cmd->top, cmd->right, cmd->bottom, cmd->color, cmd->radius);
        return;
    }
    {
        enum { BANDS = 8 };
        float span = cmd->bottom - cmd->top;
        int band;

        for (band = 0; band < BANDS; band++) {
            float band_top = cmd->top + span * (float)band / (float)BANDS;
            float band_bottom = cmd->top + span * (float)(band + 1) / (float)BANDS;
            paint_rect(cmd->left, band_top, cmd->right, band_bottom,
                       mix_color(cmd->color, cmd->color1, band, BANDS), 0.0f);
        }
    }
}

static void ensure_canvas_layer(void)
{
    if (canvas_layer != NULL) {
        return;
    }
    canvas_layer = lv_obj_create(lv_screen_active());
    lv_obj_set_flag(canvas_layer, LV_OBJ_FLAG_IGNORE_LAYOUT, true);
    lv_obj_set_flag(canvas_layer, LV_OBJ_FLAG_SCROLLABLE, false);
    lv_obj_set_pos(canvas_layer, 0, 0);
    lv_obj_set_size(canvas_layer, 480, 800);
    lv_obj_set_style_bg_opa(canvas_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(canvas_layer, 0, 0);
    lv_obj_set_style_pad_all(canvas_layer, 0, 0);
    lv_obj_set_style_radius(canvas_layer, 0, 0);
}

static void ui_canvas_apply(void)
{
    int ready = 0;
    int i;

    if (display == NULL) {
        return;
    }
    pthread_mutex_lock(&canvas_mu);
    if (canvas_pending_ready) {
        canvas_shown = canvas_pending;
        canvas_pending_ready = 0;
        ready = 1;
    }
    pthread_mutex_unlock(&canvas_mu);
    if (!ready) {
        return;
    }
    ensure_canvas_layer();
    lv_obj_clean(canvas_layer);
    for (i = 0; i < canvas_shown.count; i++) {
        paint_command(&canvas_shown.cmds[i]);
    }
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
        ui_canvas_apply();
        lv_timer_handler();
        poll_touch();
        SDL_Delay(5);
    }
    display = NULL;
}
