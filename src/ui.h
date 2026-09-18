#ifndef UI_H
#define UI_H

#include "core.h"

typedef struct {
    int   width, height;
    int   x, y;
    bool  has_pos;
    bool  decorated;
    bool  above;
    double opacity;
} UiOptions;

int ui_x11_run(App *a, const UiOptions *o);
int ui_tui_run(App *a);

#endif
