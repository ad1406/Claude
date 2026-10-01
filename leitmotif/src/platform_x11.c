#ifndef _WIN32
#define _POSIX_C_SOURCE 199309L
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <dlfcn.h>
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
static struct timespec g_t0;
static int g_gone;

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
    case XK_Prior: return KEY_PGUP; case XK_Next: return KEY_PGDN;
    }
    return -1;
}

int plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa) {
    int attr[] = { GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                   GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                   GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, True,
                   GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None };
    GLXFBConfig *fbc = NULL; int n = 0, sb = 0, sm = 0;
    XVisualInfo *vi; XSetWindowAttributes swa; XSizeHints *hints;

    clock_gettime(CLOCK_MONOTONIC, &g_t0);
    g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) { fprintf(stderr, "cannot open X display\n"); return 0; }
    XSetErrorHandler(on_xerror);
    g_st.dpi = 1.0f;
    {   const char *s = getenv("LEITMOTIF_SCALE"); if (s) { float v = (float)atof(s); if (v >= 0.75f && v <= 3) g_st.dpi = v; } }
    fbc = glXChooseFBConfig(g_dpy, DefaultScreen(g_dpy), attr, &n);
    if (!fbc || n == 0) {
        if (fbc) XFree(fbc);
        attr[18] = None;
        fbc = glXChooseFBConfig(g_dpy, DefaultScreen(g_dpy), attr, &n);
    }
    if (!fbc || n == 0) { fprintf(stderr, "no suitable GLX framebuffer config\n"); return 0; }
    glXGetFBConfigAttrib(g_dpy, fbc[0], GLX_SAMPLE_BUFFERS, &sb);
    glXGetFBConfigAttrib(g_dpy, fbc[0], GLX_SAMPLES, &sm);
    *msaa = sb ? sm : 0;
    vi = glXGetVisualFromFBConfig(g_dpy, fbc[0]);
    g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, vi->screen), vi->visual, AllocNone);
    memset(&swa, 0, sizeof swa);
    swa.colormap = g_cmap; swa.background_pixel = WhitePixel(g_dpy, vi->screen); swa.border_pixel = 0;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | StructureNotifyMask | FocusChangeMask;
    {   const char *ws = getenv("LEITMOTIF_WIN"); int a, b;   /* testing aid: logical window size */
        if (ws && sscanf(ws, "%dx%d", &a, &b) == 2 && a >= 640 && b >= 480) { w = a; h = b; } }
    w = (int)(w * g_st.dpi); h = (int)(h * g_st.dpi);
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

static void add_text(XKeyEvent *e) {
    char buf[16]; KeySym ks = 0; int nb = XLookupString(e, buf, sizeof buf, &ks, NULL), i;
    for (i = 0; i < nb; i++) { unsigned char c = (unsigned char)buf[i]; if (c >= 32 && c != 127 && g_st.ntext < 32) g_st.text[g_st.ntext++] = c; }
}

void plat_poll(Input *in) {
    memset(g_st.pressed, 0, sizeof g_st.pressed); memset(g_st.released, 0, sizeof g_st.released);
    memset(g_st.key_pressed, 0, sizeof g_st.key_pressed);
    g_st.wheel = 0; g_st.ntext = 0; g_st.resized = 0;
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
            }
            break; }
        case ButtonRelease: {
            int b = e.xbutton.button;
            if (b >= 1 && b <= 3) { int idx = b == 1 ? MOUSE_L : b == 3 ? MOUSE_R : MOUSE_M; g_st.down[idx] = 0; g_st.released[idx] = 1; }
            break; }
        case KeyPress: {
            KeySym ks = XLookupKeysym(&e.xkey, 0); int k = map_key(ks);
            if (k >= 0) { if (!g_st.key_down[k]) g_st.key_pressed[k] = 1; g_st.key_down[k] = 1; }
            add_text(&e.xkey);
            break; }
        case KeyRelease: {
            if (XEventsQueued(g_dpy, QueuedAfterReading)) {
                XEvent nx; XPeekEvent(g_dpy, &nx);
                if (nx.type == KeyPress && nx.xkey.time == e.xkey.time && nx.xkey.keycode == e.xkey.keycode) {
                    KeySym ks; int k;
                    XNextEvent(g_dpy, &nx);
                    ks = XLookupKeysym(&nx.xkey, 0); k = map_key(ks);
                    if (k >= 0) g_st.key_pressed[k] = 1;
                    add_text(&nx.xkey);
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
int plat_font_dir(char *out, int cap) { if (cap > 0) out[0] = 0; return 0; }

/* ------------------------------------------------------------------ audio
   ALSA is loaded at run time so the program builds and runs without it. */
typedef struct _snd_pcm snd_pcm_t;
static void *g_alsa;
static snd_pcm_t *g_pcm;
static int g_rate;
static int (*p_open)(snd_pcm_t **, const char *, int, int);
static int (*p_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*p_avail)(snd_pcm_t *);
static long (*p_writei)(snd_pcm_t *, const void *, unsigned long);
static int (*p_recover)(snd_pcm_t *, int, int);
static int (*p_close)(snd_pcm_t *);

int plat_audio_open(int rate) {
    if (getenv("LEITMOTIF_NOAUDIO")) return 0;
    g_alsa = dlopen("libasound.so.2", RTLD_NOW);
    if (!g_alsa) return 0;
    *(void **)&p_open = dlsym(g_alsa, "snd_pcm_open");
    *(void **)&p_set_params = dlsym(g_alsa, "snd_pcm_set_params");
    *(void **)&p_avail = dlsym(g_alsa, "snd_pcm_avail_update");
    *(void **)&p_writei = dlsym(g_alsa, "snd_pcm_writei");
    *(void **)&p_recover = dlsym(g_alsa, "snd_pcm_recover");
    *(void **)&p_close = dlsym(g_alsa, "snd_pcm_close");
    if (!p_open || !p_set_params || !p_avail || !p_writei || !p_recover || !p_close) { dlclose(g_alsa); g_alsa = NULL; return 0; }
    /* stream 0 = playback, mode 1 = non-blocking; format 2 = S16_LE, access 3 = RW interleaved */
    if (p_open(&g_pcm, "default", 0, 1) < 0) { g_pcm = NULL; dlclose(g_alsa); g_alsa = NULL; return 0; }
    if (p_set_params(g_pcm, 2, 3, 1, (unsigned)rate, 1, 120000) < 0) { p_close(g_pcm); g_pcm = NULL; dlclose(g_alsa); g_alsa = NULL; return 0; }
    g_rate = rate;
    return 1;
}

void plat_audio_pump(AudioFill fill) {
    static float mix[1024]; static short pcm[1024];
    long room; int k;
    if (!g_pcm) return;
    room = p_avail(g_pcm);
    if (room < 0) { p_recover(g_pcm, (int)room, 1); return; }
    while (room >= 256) {
        int n = room > 1024 ? 1024 : (int)room;
        fill(mix, n, g_rate);
        for (k = 0; k < n; k++) { float v = mix[k]; if (v > 1) v = 1; if (v < -1) v = -1; pcm[k] = (short)(v * 32000.0f); }
        if (p_writei(g_pcm, pcm, (unsigned long)n) < 0) break;
        room -= n;
    }
}

void plat_audio_close(void) {
    if (g_pcm) { p_close(g_pcm); g_pcm = NULL; }
    if (g_alsa) { dlclose(g_alsa); g_alsa = NULL; }
}

#include <stdarg.h>
static FILE *g_log;
const char *plat_log_path(void) { return "/tmp/leitmotif-log.txt"; }
void plat_log(const char *fmt, ...) {
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}
void plat_fatal(const char *msg) { plat_log("FATAL: %s", msg); fprintf(stderr, "%s\n", msg); }

int main(int argc, char **argv) {
    int rc;
    g_log = fopen(plat_log_path(), "w");
    rc = app_main(argc, argv);
    plat_log("exit code %d", rc);
    if (g_log) fclose(g_log);
    return rc;
}
#endif
