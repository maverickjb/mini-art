#include "ui_internal.h"

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
