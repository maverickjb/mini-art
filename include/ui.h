#ifndef MINI_ART_UI_H
#define MINI_ART_UI_H

#include <stdint.h>

/* duration is Toast.LENGTH_SHORT (0) or Toast.LENGTH_LONG (1). */
int ui_show_toast(const char *text, int32_t duration);

#endif
