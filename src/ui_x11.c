#include "ui.h"
#include "segments.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    Display *dpy;
    int      screen;
    Window   win;
    Pixmap   buf;
    GC       gc;
    XFontStruct *font;
    Atom     wm_delete;
    int      w, h;
    int      wx, wy;          /* position courante de la fenetre */
    bool     dragging;
    int      drag_dx, drag_dy;
    unsigned long c_bg, c_panel, c_dim, c_text, c_accent, c_flash;
} X11Ui;

static unsigned long alloc_color(Display *dpy, int screen, const char *spec)
{
    Colormap cm = DefaultColormap(dpy, screen);
    XColor c;
    if (XParseColor(dpy, cm, spec, &c) && XAllocColor(dpy, cm, &c))
        return c.pixel;
    return WhitePixel(dpy, screen);
}

static XFontStruct *load_any_font(Display *dpy)
{
    static const char *candidates[] = {
        "-*-fixed-medium-r-normal--13-*-*-*-*-*-iso8859-1",
        "-*-fixed-medium-r-*--13-*",
        "9x15", "8x13", "6x13", "fixed", NULL
    };
    for (int i = 0; candidates[i]; i++) {
        XFontStruct *f = XLoadQueryFont(dpy, candidates[i]);
        if (f)
            return f;
    }
    /* Dernier recours : n'importe quelle police servie par le serveur. */
    int n = 0;
    char **names = XListFonts(dpy, "*", 1, &n);
    if (names && n > 0) {
        XFontStruct *f = XLoadQueryFont(dpy, names[0]);
        XFreeFontNames(names);
        return f;
    }
    if (names)
        XFreeFontNames(names);
    return NULL;
}

static void set_window_hints(X11Ui *u, const UiOptions *o)
{
    Display *dpy = u->dpy;

    if (!o->decorated) {
        /* _MOTIF_WM_HINTS : la facon portable de retirer les decorations. */
        Atom motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        struct {
            unsigned long flags, functions, decorations;
            long input_mode;
            unsigned long status;
        } hints = { 2, 0, 0, 0, 0 };   /* flags = MWM_HINTS_DECORATIONS */
        XChangeProperty(dpy, u->win, motif, motif, 32, PropModeReplace,
                        (unsigned char *)&hints, 5);
    }

    Atom type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    XChangeProperty(dpy, u->win, type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&utility, 1);

    Atom states[3];
    int ns = 0;
    if (o->above)
        states[ns++] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    states[ns++] = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    states[ns++] = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom st = XInternAtom(dpy, "_NET_WM_STATE", False);
    XChangeProperty(dpy, u->win, st, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)states, ns);

    if (o->opacity < 1.0) {
        Atom op = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long value = (unsigned long)(o->opacity * 0xffffffffUL);
        XChangeProperty(dpy, u->win, op, XA_CARDINAL, 32, PropModeReplace,
                        (unsigned char *)&value, 1);
    }

    XClassHint ch = { (char *)"clock", (char *)"Clock" };
    XSetClassHint(dpy, u->win, &ch);
    XStoreName(dpy, u->win, "clock");
}

static void fill(X11Ui *u, unsigned long color, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    XSetForeground(u->dpy, u->gc, color);
    XFillRectangle(u->dpy, u->buf, u->gc, x, y, w, h);
}

static void draw_digits(X11Ui *u, const char *s, int ax, int ay, int aw, int ah,
                        unsigned long lit, unsigned long dim)
{
    size_t len = strlen(s);
    if (len == 0)
        return;

    /* Largeur exprimee en "unites de chiffre" pour trouver l'echelle. */
    double units = 0.0;
    for (size_t i = 0; i < len; i++)
        units += seg_is_colon(s[i]) ? 0.40 : 1.0;
    units += (double)(len - 1) * 0.16;   /* espacement inter-caracteres */

    int dw = (int)(aw / units);
    int dw_by_h = (int)(ah / 1.85);
    if (dw_by_h < dw)
        dw = dw_by_h;
    if (dw < 6)
        dw = 6;
    int dh = (int)(dw * 1.85);
    int t = dw / 5;
    if (t < 2)
        t = 2;

    int total_w = (int)(dw * units);
    int x = ax + (aw - total_w) / 2;
    int y = ay + (ah - dh) / 2;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (seg_is_colon(c)) {
            int cw = (int)(dw * 0.40);
            SegRect dots[2];
            seg_colon_rects(x, y, cw, dh, t, dots);
            for (int k = 0; k < 2; k++)
                fill(u, lit, dots[k].x, dots[k].y, dots[k].w, dots[k].h);
            x += cw + (int)(dw * 0.16);
            continue;
        }

        SegRect r[7];
        seg_rects(x, y, dw, dh, t, r);
        unsigned char mask = seg_mask(c);
        for (int k = 0; k < 7; k++) {
            unsigned long col = (mask & (1 << k)) ? lit : dim;
            fill(u, col, r[k].x, r[k].y, r[k].w, r[k].h);
        }
        x += dw + (int)(dw * 0.16);
    }
}

static unsigned long phase_color(X11Ui *u, const App *a)
{
    if (a->mode == MODE_CLOCK)
        return u->c_accent;
    switch (a->phase) {
    case PH_WORK:  return u->c_flash;
    case PH_SHORT:
    case PH_LONG:  return u->c_dim;
    default:       return u->c_accent;
    }
}

static void redraw(X11Ui *u, App *a, time_t now, int frame)
{
    int pad = 14;
    unsigned long bg = u->c_bg;

    /* Clignotement discret quand une alarme ou une phase vient de tomber. */
    if (a->flash && (frame / 3) % 2 == 0)
        bg = u->c_panel;

    fill(u, bg, 0, 0, u->w, u->h);

    int font_h = u->font ? (u->font->ascent + u->font->descent) : 0;
    int bar_h = 5;
    int bottom = font_h ? font_h + 6 : 0;
    int bar_y = u->h - pad - bottom - bar_h;
    int digits_h = bar_y - pad - 4;
    if (digits_h < 20)
        digits_h = u->h - 2 * pad;

    unsigned long lit = phase_color(u, a);

    char text[32];
    app_display(a, now, text, sizeof(text));
    draw_digits(u, text, pad, pad, u->w - 2 * pad, digits_h, lit, u->c_panel);

    /* Barre d'avancement : pleine largeur en pomodoro, rien en horloge. */
    if (a->mode == MODE_POMODORO && a->phase != PH_IDLE) {
        int bw = u->w - 2 * pad;
        fill(u, u->c_panel, pad, bar_y, bw, bar_h);
        fill(u, lit, pad, bar_y, (int)(bw * app_progress(a)), bar_h);
    }

    if (u->font) {
        char sub[96];
        app_subtitle(a, now, sub, sizeof(sub));
        int tw = XTextWidth(u->font, sub, (int)strlen(sub));
        XSetForeground(u->dpy, u->gc, u->c_text);
        XSetFont(u->dpy, u->gc, u->font->fid);
        XDrawString(u->dpy, u->buf, u->gc,
                    (u->w - tw) / 2, u->h - pad - u->font->descent,
                    sub, (int)strlen(sub));
    }

    XCopyArea(u->dpy, u->buf, u->win, u->gc, 0, 0, u->w, u->h, 0, 0);
    XFlush(u->dpy);
}

static void resize_buffer(X11Ui *u, int w, int h)
{
    if (u->buf)
        XFreePixmap(u->dpy, u->buf);
    u->w = w;
    u->h = h;
    u->buf = XCreatePixmap(u->dpy, u->win, w, h,
                           DefaultDepth(u->dpy, u->screen));
}

int ui_x11_run(App *a, const UiOptions *o)
{
    X11Ui u;
    memset(&u, 0, sizeof(u));

    u.dpy = XOpenDisplay(NULL);
    if (!u.dpy) {
        fprintf(stderr, "clock: impossible d'ouvrir l'affichage X "
                        "(essaie --tui pour le mode terminal)\n");
        return 1;
    }
    u.screen = DefaultScreen(u.dpy);

    u.c_bg     = alloc_color(u.dpy, u.screen, "#12141c");
    u.c_panel  = alloc_color(u.dpy, u.screen, "#232839");
    u.c_dim    = alloc_color(u.dpy, u.screen, "#9ece6a");
    u.c_text   = alloc_color(u.dpy, u.screen, "#7f88a3");
    u.c_accent = alloc_color(u.dpy, u.screen, "#7aa2f7");
    u.c_flash  = alloc_color(u.dpy, u.screen, "#f7768e");

    int x = o->has_pos ? o->x : 60;
    int y = o->has_pos ? o->y : 60;

    XSetWindowAttributes swa;
    swa.background_pixel = u.c_bg;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
    u.win = XCreateWindow(u.dpy, RootWindow(u.dpy, u.screen),
                          x, y, o->width, o->height, 0,
                          CopyFromParent, InputOutput, CopyFromParent,
                          CWBackPixel | CWEventMask, &swa);

    u.wx = x;
    u.wy = y;

    set_window_hints(&u, o);

    XSizeHints sh;
    sh.flags = PMinSize;
    sh.min_width = 140;
    sh.min_height = 70;
    if (o->has_pos) {
        sh.flags |= USPosition;
        sh.x = x;
        sh.y = y;
    }
    XSetWMNormalHints(u.dpy, u.win, &sh);

    u.wm_delete = XInternAtom(u.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(u.dpy, u.win, &u.wm_delete, 1);

    u.gc = XCreateGC(u.dpy, u.win, 0, NULL);
    u.font = load_any_font(u.dpy);

    XMapWindow(u.dpy, u.win);
    resize_buffer(&u, o->width, o->height);

    int fd = ConnectionNumber(u.dpy);
    int frame = 0;
    time_t last_tick = 0;

    while (!a->quit) {
        while (XPending(u.dpy)) {
            XEvent ev;
            XNextEvent(u.dpy, &ev);

            switch (ev.type) {
            case Expose:
                break;
            case ConfigureNotify:
                u.wx = ev.xconfigure.x;
                u.wy = ev.xconfigure.y;
                if (ev.xconfigure.width != u.w || ev.xconfigure.height != u.h)
                    resize_buffer(&u, ev.xconfigure.width, ev.xconfigure.height);
                break;
            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape)
                    a->quit = true;
                else if (ks == XK_space)
                    app_key(a, ' ');
                else if (ks < 128)
                    app_key(a, (int)ks);
                break;
            }
            case ButtonPress:
                if (ev.xbutton.button == Button1) {
                    u.dragging = true;
                    u.drag_dx = ev.xbutton.x_root - u.wx;
                    u.drag_dy = ev.xbutton.y_root - u.wy;
                } else if (ev.xbutton.button == Button3) {
                    app_key(a, 'm');
                }
                break;
            case ButtonRelease:
                if (ev.xbutton.button == Button1)
                    u.dragging = false;
                break;
            case MotionNotify:
                if (u.dragging) {
                    u.wx = ev.xmotion.x_root - u.drag_dx;
                    u.wy = ev.xmotion.y_root - u.drag_dy;
                    XMoveWindow(u.dpy, u.win, u.wx, u.wy);
                }
                break;
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == u.wm_delete)
                    a->quit = true;
                break;
            default:
                break;
            }
        }

        if (a->quit)
            break;

        time_t now = time(NULL);
        if (now != last_tick) {
            app_tick(a, now);
            last_tick = now;
        }
        redraw(&u, a, now, frame++);

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = { 0, 150000 };   /* 150 ms : assez fluide */
        select(fd + 1, &rfds, NULL, NULL, &tv);
    }

    if (u.font)
        XFreeFont(u.dpy, u.font);
    if (u.buf)
        XFreePixmap(u.dpy, u.buf);
    XFreeGC(u.dpy, u.gc);
    XDestroyWindow(u.dpy, u.win);
    XCloseDisplay(u.dpy);
    return 0;
}
