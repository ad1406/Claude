#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "platform.h"

static Display *g_dpy;
static Window g_win;
static Colormap g_cmap;
static GLXContext g_ctx;
static Atom g_wm_delete;
static Input g_st;
static Cursor g_cur[CURSOR_COUNT];
static int g_cursor = -1, g_lastx, g_lasty;
static Time g_last_click; static int g_click_x, g_click_y;
static struct timespec g_t0;
static int g_gone;   /* window destroyed from outside */

/* Report X protocol errors instead of letting Xlib terminate the program */
static int on_xerror(Display *d, XErrorEvent *e) {
    char msg[128]; XGetErrorText(d, e->error_code, msg, sizeof msg);
    if (!g_gone) fprintf(stderr, "X error: %s\n", msg);
    return 0;
}

static int map_key(KeySym k) {
    switch (k) {
    case XK_Escape: return KEY_ESC; case XK_Return: case XK_KP_Enter: return KEY_ENTER;
    case XK_BackSpace: return KEY_BACKSPACE; case XK_Delete: return KEY_DELETE;
    case XK_Left: return KEY_LEFT; case XK_Right: return KEY_RIGHT; case XK_Up: return KEY_UP; case XK_Down: return KEY_DOWN;
    case XK_Home: return KEY_HOME; case XK_End: return KEY_END; case XK_Tab: return KEY_TAB; case XK_space: return KEY_SPACE;
    case XK_b: case XK_B: return KEY_B;
    }
    return -1;
}

int plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa, int *stencil) {
    int attr[] = { GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                   GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                   GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_DOUBLEBUFFER, True,
                   GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None };
    GLXFBConfig *fbc = NULL; int n = 0, sb = 0, sm = 0, st = 0;
    XVisualInfo *vi; XSetWindowAttributes swa; XSizeHints *hints;

    clock_gettime(CLOCK_MONOTONIC, &g_t0);
    g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) { fprintf(stderr, "cannot open X display\n"); return 0; }
    XSetErrorHandler(on_xerror);
    g_st.dpi = 1.0f;
    fbc = glXChooseFBConfig(g_dpy, DefaultScreen(g_dpy), attr, &n);
    if (!fbc || n == 0) {
        if (fbc) XFree(fbc);
        attr[20] = None;   /* retry without multisampling */
        fbc = glXChooseFBConfig(g_dpy, DefaultScreen(g_dpy), attr, &n);
    }
    if (!fbc || n == 0) { fprintf(stderr, "no suitable GLX framebuffer config\n"); return 0; }
    glXGetFBConfigAttrib(g_dpy, fbc[0], GLX_SAMPLE_BUFFERS, &sb);
    glXGetFBConfigAttrib(g_dpy, fbc[0], GLX_SAMPLES, &sm);
    glXGetFBConfigAttrib(g_dpy, fbc[0], GLX_STENCIL_SIZE, &st);
    *msaa = sb ? sm : 0; *stencil = st >= 8;
    vi = glXGetVisualFromFBConfig(g_dpy, fbc[0]);
    g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, vi->screen), vi->visual, AllocNone);
    memset(&swa, 0, sizeof swa);
    swa.colormap = g_cmap; swa.background_pixel = WhitePixel(g_dpy, vi->screen); swa.border_pixel = 0;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | StructureNotifyMask | FocusChangeMask;
    g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, vi->screen), 0, 0, (unsigned)w, (unsigned)h, 0, vi->depth,
                          InputOutput, vi->visual, CWColormap | CWBorderPixel | CWEventMask | CWBackPixel, &swa);
    g_ctx = glXCreateNewContext(g_dpy, fbc[0], GLX_RGBA_TYPE, NULL, True);
    XFree(vi); XFree(fbc);
    if (!g_ctx) return 0;
    hints = XAllocSizeHints();
    if (hints) { hints->flags = PMinSize; hints->min_width = min_w; hints->min_height = min_h;
                 XSetWMNormalHints(g_dpy, g_win, hints); XFree(hints); }
    XStoreName(g_dpy, g_win, title);
    g_wm_delete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &g_wm_delete, 1);
    XMapWindow(g_dpy, g_win);
    glXMakeCurrent(g_dpy, g_win, g_ctx);
    g_cur[CURSOR_ARROW] = XCreateFontCursor(g_dpy, XC_left_ptr);
    g_cur[CURSOR_HAND] = XCreateFontCursor(g_dpy, XC_hand2);
    g_cur[CURSOR_MOVE] = XCreateFontCursor(g_dpy, XC_fleur);
    g_cur[CURSOR_TEXT] = XCreateFontCursor(g_dpy, XC_xterm);
    g_st.win_w = w; g_st.win_h = h; g_st.focused = 1;
    return 1;
}

void plat_shutdown(void) {
    int i;
    if (!g_dpy) return;
    glXMakeCurrent(g_dpy, None, NULL);
    if (g_ctx) glXDestroyContext(g_dpy, g_ctx);
    for (i = 0; i < CURSOR_COUNT; i++) if (g_cur[i]) XFreeCursor(g_dpy, g_cur[i]);
    if (g_win && !g_gone) XDestroyWindow(g_dpy, g_win);
    if (g_cmap) XFreeColormap(g_dpy, g_cmap);
    XCloseDisplay(g_dpy); g_dpy = NULL;
}

void plat_poll(Input *in) {
    memset(g_st.pressed, 0, sizeof g_st.pressed); memset(g_st.released, 0, sizeof g_st.released);
    memset(g_st.key_pressed, 0, sizeof g_st.key_pressed);
    g_st.dbl = 0; g_st.wheel = 0; g_st.ntext = 0; g_st.resized = 0;
    while (XPending(g_dpy)) {
        XEvent e; XNextEvent(g_dpy, &e);
        switch (e.type) {
        case ConfigureNotify:
            if (e.xconfigure.width != g_st.win_w || e.xconfigure.height != g_st.win_h) {
                g_st.win_w = e.xconfigure.width; g_st.win_h = e.xconfigure.height; g_st.resized = 1; }
            break;
        case MotionNotify: g_st.mx = e.xmotion.x; g_st.my = e.xmotion.y; break;
        case ButtonPress: {
            int b = e.xbutton.button;
            g_st.mx = e.xbutton.x; g_st.my = e.xbutton.y;
            if (b == 4) g_st.wheel += 1; else if (b == 5) g_st.wheel -= 1;
            else if (b >= 1 && b <= 3) {
                int idx = b == 1 ? MOUSE_L : b == 3 ? MOUSE_R : MOUSE_M;
                g_st.down[idx] = 1; g_st.pressed[idx] = 1;
                if (idx == MOUSE_L) {
                    if (e.xbutton.time - g_last_click < 400 && abs(e.xbutton.x - g_click_x) < 5 && abs(e.xbutton.y - g_click_y) < 5) {
                        g_st.dbl = 1; g_last_click = 0;
                    } else { g_last_click = e.xbutton.time; g_click_x = e.xbutton.x; g_click_y = e.xbutton.y; }
                }
            }
            break; }
        case ButtonRelease: {
            int b = e.xbutton.button;
            if (b >= 1 && b <= 3) { int idx = b == 1 ? MOUSE_L : b == 3 ? MOUSE_R : MOUSE_M; g_st.down[idx] = 0; g_st.released[idx] = 1; }
            break; }
        case KeyPress: {
            char buf[16]; KeySym ks = 0; int k, nb = XLookupString(&e.xkey, buf, sizeof buf, &ks, NULL), i;
            k = map_key(ks);
            if (k >= 0) { if (!g_st.key_down[k] || k == KEY_BACKSPACE || k == KEY_DELETE || k == KEY_LEFT || k == KEY_RIGHT) g_st.key_pressed[k] = 1; g_st.key_down[k] = 1; }
            for (i = 0; i < nb; i++) { unsigned char c = (unsigned char)buf[i]; if (c >= 32 && c != 127 && g_st.ntext < 32) g_st.text[g_st.ntext++] = c; }
            break; }
        case KeyRelease: {
            /* X sends release+press pairs for auto-repeat; ignore those */
            if (XEventsQueued(g_dpy, QueuedAfterReading)) {
                XEvent nx; XPeekEvent(g_dpy, &nx);
                if (nx.type == KeyPress && nx.xkey.time == e.xkey.time && nx.xkey.keycode == e.xkey.keycode) {
                    char buf[16]; KeySym ks = 0; int k, nb, i;
                    XNextEvent(g_dpy, &nx);
                    nb = XLookupString(&nx.xkey, buf, sizeof buf, &ks, NULL); k = map_key(ks);
                    if (k == KEY_BACKSPACE || k == KEY_DELETE || k == KEY_LEFT || k == KEY_RIGHT) g_st.key_pressed[k] = 1;
                    for (i = 0; i < nb; i++) { unsigned char c = (unsigned char)buf[i]; if (c >= 32 && c != 127 && g_st.ntext < 32) g_st.text[g_st.ntext++] = c; }
                    break;
                }
            }
            { KeySym ks = XLookupKeysym(&e.xkey, 0); int k = map_key(ks); if (k >= 0) g_st.key_down[k] = 0; }
            break; }
        case ClientMessage:
            if ((Atom)e.xclient.data.l[0] == g_wm_delete) g_st.quit = 1;
            break;
        case DestroyNotify: if (e.xdestroywindow.window == g_win) { g_gone = 1; g_st.quit = 1; } break;
        case FocusIn: g_st.focused = 1; break;
        case FocusOut: g_st.focused = 0; memset(g_st.down, 0, sizeof g_st.down); memset(g_st.key_down, 0, sizeof g_st.key_down); break;
        }
    }
    {   Window rr, cr; int rx, ry, wx, wy; unsigned mask;
        if (XQueryPointer(g_dpy, g_win, &rr, &cr, &rx, &ry, &wx, &wy, &mask)) {
            g_st.ctrl = (mask & ControlMask) != 0; g_st.shift = (mask & ShiftMask) != 0; }
    }
    g_st.mdx = g_st.mx - g_lastx; g_st.mdy = g_st.my - g_lasty;
    g_lastx = g_st.mx; g_lasty = g_st.my;
    *in = g_st;
}

void plat_swap(void) { if (!g_gone) glXSwapBuffers(g_dpy, g_win); }
double plat_time(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)(t.tv_sec - g_t0.tv_sec) + (double)(t.tv_nsec - g_t0.tv_nsec) * 1e-9;
}
void plat_sleep(double s) {
    struct timespec t; if (s <= 0) return;
    t.tv_sec = (time_t)s; t.tv_nsec = (long)((s - (double)t.tv_sec) * 1e9);
    nanosleep(&t, NULL);
}
void plat_cursor(int c) { if (c != g_cursor) { g_cursor = c; XDefineCursor(g_dpy, g_win, g_cur[c]); } }
int plat_vsync(void) { return 0; }
int plat_window_alive(void) { return !g_gone; }

int plat_font_path(int bold, char *out, int cap) {
    static const char *regular[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf" };
    static const char *boldf[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf" };
    int i;
    for (i = 0; i < 3; i++) {
        const char *p = bold ? boldf[i] : regular[i];
        if (access(p, R_OK) == 0) { snprintf(out, (size_t)cap, "%s", p); return 1; }
    }
    return 0;
}

int main(int argc, char **argv) { return app_main(argc, argv); }
#endif
