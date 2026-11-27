#include "native_internal.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define TOAST_MAGIC 0x54534f54u

struct Toast {
    uint32_t magic;
    int32_t duration;
    char message[256];
};

static int native_toast_make_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;
    struct Toast *toast;

    if (argc < 3 || result == NULL) {
        return -1;
    }
    text = runtime_resource_string(rt, (uint32_t)args[1].i);
    fprintf(stderr, "Toast.makeText id=%#x -> %s\n", (unsigned)args[1].i,
            text != NULL ? text : "(missing)");
    if (text == NULL) {
        fprintf(stderr, "resource string %#x not found\n", (unsigned)args[1].i);
        return -1;
    }
    toast = runtime_alloc(rt, sizeof(*toast));
    if (toast == NULL) {
        return -1;
    }
    toast->magic = TOAST_MAGIC;
    toast->duration = args[2].i;
    snprintf(toast->message, sizeof(toast->message), "%s", text);
    memset(result, 0, sizeof(*result));
    result->l = toast;
    return 0;
}

static int native_toast_show(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    struct Toast *toast;

    (void)rt;
    (void)result;
    if (argc < 1 || args[0].l == NULL) {
        return -1;
    }
    toast = args[0].l;
    if (toast->magic != TOAST_MAGIC) {
        return -1;
    }
    fprintf(stderr, "Toast.show: %s\n", toast->message);
    if (ui_show_toast(toast->message, toast->duration) != 0) {
        fprintf(stderr, "toast window failed\n");
        return -1;
    }
    return 0;
}

static int native_get_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JavaObject *text;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    text = native_new_string(rt, java_object_text(args[0].l));
    if (text == NULL) {
        return -1;
    }
    result->l = text;
    return 0;
}

static int native_set_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    const char *text;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    text = java_object_check(args[1].l) ? java_object_text(args[1].l) : "";
    java_object_set_text(args[0].l, text);
    ui_content_set_text(java_object_widget(args[0].l), text);
    return 0;
}

static int native_surface_view_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "SurfaceView.<init>\n");
    return 0;
}

static int native_surface_view_get_holder(Runtime *rt, const JValue *args, uint32_t argc,
                                          JValue *result)
{
    JavaObject *view;
    JavaObject *holder;
    JValue stored;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    view = args[0].l;
    memset(&stored, 0, sizeof(stored));
    java_object_get_field(view, "holder", &stored);
    if (!java_object_check(stored.l)) {
        holder = java_object_new(rt, "Landroid/view/SurfaceHolder;");
        if (holder == NULL) {
            return -1;
        }
        stored.l = holder;
        if (java_object_set_field(view, "holder", stored) != 0) {
            return -1;
        }
    }
    fprintf(stderr, "SurfaceView.getHolder\n");
    result->l = stored.l;
    return 0;
}

static int native_surface_holder_get_surface(Runtime *rt, const JValue *args, uint32_t argc,
                                             JValue *result)
{
    JavaObject *holder;
    JavaObject *surface;
    JValue stored;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    holder = args[0].l;
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(holder, "surface", &stored) != 0) {
        return -1;
    }
    if (!java_object_check(stored.l)) {
        surface = java_object_new(rt, "Landroid/view/Surface;");
        if (surface == NULL) {
            return -1;
        }
        stored.l = surface;
        if (java_object_set_field(holder, "surface", stored) != 0) {
            return -1;
        }
    }
    fprintf(stderr, "SurfaceHolder.getSurface\n");
    result->l = stored.l;
    return 0;
}

static int native_surface_holder_lock_canvas(Runtime *rt, const JValue *args, uint32_t argc,
                                             JValue *result)
{
    JavaObject *holder;
    JavaObject *canvas;
    JValue stored;
    JValue size;

    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    holder = args[0].l;
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(holder, "canvas", &stored) != 0) {
        return -1;
    }
    if (!java_object_check(stored.l)) {
        canvas = java_object_new(rt, "Landroid/graphics/Canvas;");
        if (canvas == NULL) {
            return -1;
        }
        memset(&size, 0, sizeof(size));
        size.i = 480;
        if (java_object_set_field(canvas, "width", size) != 0) {
            return -1;
        }
        size.i = 800;
        if (java_object_set_field(canvas, "height", size) != 0) {
            return -1;
        }
        stored.l = canvas;
        if (java_object_set_field(holder, "canvas", stored) != 0) {
            return -1;
        }
    }
    fprintf(stderr, "SurfaceHolder.lockCanvas\n");
    result->l = stored.l;
    ui_canvas_begin();
    return 0;
}

static int native_surface_holder_unlock_canvas(Runtime *rt, const JValue *args, uint32_t argc,
                                               JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    fprintf(stderr, "SurfaceHolder.unlockCanvasAndPost\n");
    ui_canvas_post();
    return 0;
}

static int native_canvas_get_width(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;

    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "width", &stored) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->i = stored.i;
    fprintf(stderr, "Canvas.getWidth %d\n", stored.i);
    return 0;
}

static int native_canvas_get_height(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;

    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "height", &stored) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->i = stored.i;
    fprintf(stderr, "Canvas.getHeight %d\n", stored.i);
    return 0;
}

static uint32_t paint_argb(JavaObject *paint, uint32_t *second)
{
    JValue field;
    uint32_t color = 0xffffffffu;
    uint32_t other = color;

    memset(&field, 0, sizeof(field));
    if (java_object_get_field(paint, "shader", &field) == 0 && java_object_check(field.l)) {
        JValue first;
        JValue next;

        memset(&first, 0, sizeof(first));
        memset(&next, 0, sizeof(next));
        java_object_get_field(field.l, "color0", &first);
        java_object_get_field(field.l, "color1", &next);
        color = (uint32_t)first.i | 0xff000000u;
        other = (uint32_t)next.i | 0xff000000u;
    } else {
        memset(&field, 0, sizeof(field));
        java_object_get_field(paint, "color", &field);
        color = (uint32_t)field.i;
        if ((color & 0xff000000u) == 0) {
            color |= 0xff000000u;
        }
        other = color;
    }
    memset(&field, 0, sizeof(field));
    java_object_get_field(paint, "alpha", &field);
    if (field.i != 0) {
        uint32_t alpha = ((uint32_t)field.i & 0xffu) << 24;
        color = (color & 0x00ffffffu) | alpha;
        other = (other & 0x00ffffffu) | alpha;
    }
    if (second != NULL) {
        *second = other;
    }
    return color;
}

static int native_canvas_draw_rect(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 6 || !java_object_check(args[0].l) || !java_object_check(args[5].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[1].f;
    if (java_object_set_field(args[0].l, "left", field) != 0) {
        return -1;
    }
    field.f = args[2].f;
    if (java_object_set_field(args[0].l, "top", field) != 0) {
        return -1;
    }
    field.f = args[3].f;
    if (java_object_set_field(args[0].l, "right", field) != 0) {
        return -1;
    }
    field.f = args[4].f;
    if (java_object_set_field(args[0].l, "bottom", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[5].l;
    if (java_object_set_field(args[0].l, "paint", field) != 0) {
        return -1;
    }
    {
        uint32_t second = 0;
        uint32_t color = paint_argb(args[5].l, &second);
        ui_canvas_rect(args[1].f, args[2].f, args[3].f, args[4].f, color, second);
    }
    fprintf(stderr, "Canvas.drawRect %g %g %g %g\n", args[1].f, args[2].f, args[3].f, args[4].f);
    return 0;
}

static int native_canvas_draw_round_rect(Runtime *rt, const JValue *args, uint32_t argc,
                                        JValue *result)
{
    JValue edge;
    float left;
    float top;
    float right;
    float bottom;
    float radius;

    (void)rt;
    (void)result;
    if (argc < 5 || !java_object_check(args[0].l) || !java_object_check(args[1].l) ||
        !java_object_check(args[4].l)) {
        return -1;
    }
    memset(&edge, 0, sizeof(edge));
    if (java_object_get_field(args[1].l, "left", &edge) != 0) {
        return -1;
    }
    left = edge.f;
    if (java_object_get_field(args[1].l, "top", &edge) != 0) {
        return -1;
    }
    top = edge.f;
    if (java_object_get_field(args[1].l, "right", &edge) != 0) {
        return -1;
    }
    right = edge.f;
    if (java_object_get_field(args[1].l, "bottom", &edge) != 0) {
        return -1;
    }
    bottom = edge.f;
    radius = args[2].f < args[3].f ? args[2].f : args[3].f;
    {
        uint32_t second = 0;
        uint32_t color = paint_argb(args[4].l, &second);
        ui_canvas_round_rect(left, top, right, bottom, radius, color, second);
    }
    fprintf(stderr, "Canvas.drawRoundRect %g %g %g %g r=%g\n", left, top, right, bottom, radius);
    return 0;
}

static int native_canvas_draw_line(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;
    float stroke = 1.0f;

    (void)rt;
    (void)result;
    if (argc < 6 || !java_object_check(args[0].l) || !java_object_check(args[5].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    if (java_object_get_field(args[5].l, "strokeWidth", &field) == 0 && field.f > 0.0f) {
        stroke = field.f;
    }
    {
        uint32_t color = paint_argb(args[5].l, NULL);
        ui_canvas_line(args[1].f, args[2].f, args[3].f, args[4].f, stroke, color);
    }
    fprintf(stderr, "Canvas.drawLine %g %g %g %g\n", args[1].f, args[2].f, args[3].f, args[4].f);
    return 0;
}

static int native_canvas_draw_text(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 5 || !java_object_check(args[0].l) || !java_object_check(args[1].l) ||
        !java_object_check(args[4].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[1].l;
    if (java_object_set_field(args[0].l, "text", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[2].f;
    if (java_object_set_field(args[0].l, "textX", field) != 0) {
        return -1;
    }
    field.f = args[3].f;
    if (java_object_set_field(args[0].l, "textY", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[4].l;
    if (java_object_set_field(args[0].l, "textPaint", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    java_object_get_field(args[4].l, "textSize", &field);
    ui_canvas_text(java_object_text(args[1].l), args[2].f, args[3].f, field.f,
                   paint_argb(args[4].l, NULL));
    fprintf(stderr, "Canvas.drawText %s %g %g size %g\n", java_object_text(args[1].l), args[2].f,
            args[3].f, field.f);
    return 0;
}

static int native_surface_is_valid(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    result->i = 1;
    fprintf(stderr, "Surface.isValid 1\n");
    return 0;
}

static int native_motion_get_x(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;

    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "x", &stored) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->f = stored.f;
    return 0;
}

static int native_motion_get_y(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;

    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "y", &stored) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->f = stored.f;
    return 0;
}

static int native_motion_get_action(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;

    (void)rt;
    if (argc < 1 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "action", &stored) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->i = stored.i;
    return 0;
}

static int native_view_set_focusable(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "View.setFocusable %d\n", args[1].i != 0);
    return 0;
}

static int native_surface_holder_add_callback(Runtime *rt, const JValue *args, uint32_t argc,
                                              JValue *result)
{
    const char *callback_class;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    java_object_set_listener(args[0].l, args[1].l);
    callback_class = java_object_class(args[1].l);
    fprintf(stderr, "SurfaceHolder.addCallback %s\n",
            callback_class != NULL ? callback_class : "?");
    return 0;
}

static int native_paint_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "Paint.<init> %#x\n", (unsigned)args[1].i);
    return 0;
}

static int native_paint_set_shader(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;
    JValue field;

    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    if (args[1].l != NULL && !java_object_check(args[1].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "shader", &stored) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[1].l;
    if (java_object_set_field(args[0].l, "shader", field) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->l = stored.l;
    fprintf(stderr, "Paint.setShader %p\n", args[1].l);
    return 0;
}

static int native_paint_set_text_align(Runtime *rt, const JValue *args, uint32_t argc,
                                       JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[1].l;
    if (java_object_set_field(args[0].l, "textAlign", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setTextAlign %p\n", args[1].l);
    return 0;
}

static int native_paint_set_style(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l) || !java_object_check(args[1].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[1].l;
    if (java_object_set_field(args[0].l, "style", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setStyle %p\n", args[1].l);
    return 0;
}

static int native_paint_set_stroke_width(Runtime *rt, const JValue *args, uint32_t argc,
                                        JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[1].f;
    if (java_object_set_field(args[0].l, "strokeWidth", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setStrokeWidth %g\n", args[1].f);
    return 0;
}

static int native_paint_set_text_size(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[1].f;
    if (java_object_set_field(args[0].l, "textSize", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setTextSize %g\n", args[1].f);
    return 0;
}

static int native_paint_set_color(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.i = args[1].i;
    if (java_object_set_field(args[0].l, "color", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setColor %#x\n", (unsigned)args[1].i);
    return 0;
}

static int native_paint_set_alpha(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.i = args[1].i;
    if (java_object_set_field(args[0].l, "alpha", field) != 0) {
        return -1;
    }
    fprintf(stderr, "Paint.setAlpha %d\n", args[1].i);
    return 0;
}

static int native_paint_set_typeface(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue stored;
    JValue field;

    (void)rt;
    if (argc < 2 || result == NULL || !java_object_check(args[0].l)) {
        return -1;
    }
    if (args[1].l != NULL && !java_object_check(args[1].l)) {
        return -1;
    }
    memset(&stored, 0, sizeof(stored));
    if (java_object_get_field(args[0].l, "typeface", &stored) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[1].l;
    if (java_object_set_field(args[0].l, "typeface", field) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->l = stored.l;
    fprintf(stderr, "Paint.setTypeface %p\n", args[1].l);
    return 0;
}

static int native_linear_gradient_init(Runtime *rt, const JValue *args, uint32_t argc,
                                       JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 8 || !java_object_check(args[0].l) || !java_object_check(args[7].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[1].f;
    if (java_object_set_field(args[0].l, "x0", field) != 0) {
        return -1;
    }
    field.f = args[2].f;
    if (java_object_set_field(args[0].l, "y0", field) != 0) {
        return -1;
    }
    field.f = args[3].f;
    if (java_object_set_field(args[0].l, "x1", field) != 0) {
        return -1;
    }
    field.f = args[4].f;
    if (java_object_set_field(args[0].l, "y1", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.i = args[5].i;
    if (java_object_set_field(args[0].l, "color0", field) != 0) {
        return -1;
    }
    field.i = args[6].i;
    if (java_object_set_field(args[0].l, "color1", field) != 0) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.l = args[7].l;
    if (java_object_set_field(args[0].l, "tileMode", field) != 0) {
        return -1;
    }
    fprintf(stderr, "LinearGradient.<init>\n");
    return 0;
}

static int native_rectf_init(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 1 || !java_object_check(args[0].l)) {
        return -1;
    }
    fprintf(stderr, "RectF.<init>\n");
    return 0;
}

static int native_rectf_set(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    JValue field;

    (void)rt;
    (void)result;
    if (argc < 5 || !java_object_check(args[0].l)) {
        return -1;
    }
    memset(&field, 0, sizeof(field));
    field.f = args[1].f;
    if (java_object_set_field(args[0].l, "left", field) != 0) {
        return -1;
    }
    field.f = args[2].f;
    if (java_object_set_field(args[0].l, "top", field) != 0) {
        return -1;
    }
    field.f = args[3].f;
    if (java_object_set_field(args[0].l, "right", field) != 0) {
        return -1;
    }
    field.f = args[4].f;
    if (java_object_set_field(args[0].l, "bottom", field) != 0) {
        return -1;
    }
    fprintf(stderr, "RectF.set %g %g %g %g\n", args[1].f, args[2].f, args[3].f, args[4].f);
    return 0;
}

static int native_set_click_listener(Runtime *rt, const JValue *args, uint32_t argc, JValue *result)
{
    (void)rt;
    (void)result;
    if (argc < 2 || !java_object_check(args[0].l)) {
        return -1;
    }
    java_object_set_listener(args[0].l, args[1].l);
    return 0;
}

int native_register_widget(Runtime *rt)
{
    static const struct NativeBinding rows[] = {
        {"Landroid/widget/Toast;", "makeText", "(Landroid/content/Context;II)Landroid/widget/Toast;",
         native_toast_make_text},
        {"Landroid/widget/Toast;", "show", "()V", native_toast_show},
        {"Landroid/widget/TextView;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/TextView;", "setText", "(Ljava/lang/CharSequence;)V", native_set_text},
        {"Landroid/widget/Button;", "getText", "()Ljava/lang/CharSequence;", native_get_text},
        {"Landroid/widget/Button;", "setOnClickListener", "(Landroid/view/View$OnClickListener;)V",
         native_set_click_listener},
        {"Landroid/view/SurfaceView;", "<init>", "(Landroid/content/Context;)V",
         native_surface_view_init},
        {"Landroid/view/SurfaceView;", "getHolder", "()Landroid/view/SurfaceHolder;",
         native_surface_view_get_holder},
        {"Landroid/view/SurfaceHolder;", "addCallback", "(Landroid/view/SurfaceHolder$Callback;)V",
         native_surface_holder_add_callback},
        {"Landroid/view/SurfaceHolder;", "getSurface", "()Landroid/view/Surface;",
         native_surface_holder_get_surface},
        {"Landroid/view/SurfaceHolder;", "lockCanvas", "()Landroid/graphics/Canvas;",
         native_surface_holder_lock_canvas},
        {"Landroid/view/SurfaceHolder;", "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V",
         native_surface_holder_unlock_canvas},
        {"Landroid/graphics/Canvas;", "getWidth", "()I", native_canvas_get_width},
        {"Landroid/graphics/Canvas;", "getHeight", "()I", native_canvas_get_height},
        {"Landroid/graphics/Canvas;", "drawRect", "(FFFFLandroid/graphics/Paint;)V",
         native_canvas_draw_rect},
        {"Landroid/graphics/Canvas;", "drawRoundRect",
         "(Landroid/graphics/RectF;FFLandroid/graphics/Paint;)V", native_canvas_draw_round_rect},
        {"Landroid/graphics/Canvas;", "drawLine", "(FFFFLandroid/graphics/Paint;)V",
         native_canvas_draw_line},
        {"Landroid/graphics/Canvas;", "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V",
         native_canvas_draw_text},
        {"Landroid/view/Surface;", "isValid", "()Z", native_surface_is_valid},
        {"Landroid/view/MotionEvent;", "getX", "()F", native_motion_get_x},
        {"Landroid/view/MotionEvent;", "getY", "()F", native_motion_get_y},
        {"Landroid/view/MotionEvent;", "getAction", "()I", native_motion_get_action},
        {"Landroid/view/View;", "setFocusable", "(Z)V", native_view_set_focusable},
        {"Landroid/graphics/Paint;", "<init>", "(I)V", native_paint_init},
        {"Landroid/graphics/Paint;", "setShader", "(Landroid/graphics/Shader;)Landroid/graphics/Shader;",
         native_paint_set_shader},
        {"Landroid/graphics/Paint;", "setTextAlign", "(Landroid/graphics/Paint$Align;)V",
         native_paint_set_text_align},
        {"Landroid/graphics/Paint;", "setStyle", "(Landroid/graphics/Paint$Style;)V",
         native_paint_set_style},
        {"Landroid/graphics/Paint;", "setStrokeWidth", "(F)V", native_paint_set_stroke_width},
        {"Landroid/graphics/Paint;", "setTextSize", "(F)V", native_paint_set_text_size},
        {"Landroid/graphics/Paint;", "setColor", "(I)V", native_paint_set_color},
        {"Landroid/graphics/Paint;", "setAlpha", "(I)V", native_paint_set_alpha},
        {"Landroid/graphics/Paint;", "setTypeface",
         "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;", native_paint_set_typeface},
        {"Landroid/graphics/LinearGradient;", "<init>",
         "(FFFFIILandroid/graphics/Shader$TileMode;)V", native_linear_gradient_init},
        {"Landroid/graphics/RectF;", "<init>", "()V", native_rectf_init},
        {"Landroid/graphics/RectF;", "set", "(FFFF)V", native_rectf_set},
    };

    return native_bind(rt, rows, sizeof(rows) / sizeof(rows[0]));
}
