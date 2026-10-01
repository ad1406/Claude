/* Thin OS layer: one window with an OpenGL context, input, time, fonts.
   Implemented in platform_win32.c and platform_x11.c. */
#ifndef PLATFORM_H
#define PLATFORM_H

enum {
    KEY_ESC, KEY_ENTER, KEY_BACKSPACE, KEY_DELETE, KEY_LEFT, KEY_RIGHT,
    KEY_UP, KEY_DOWN, KEY_HOME, KEY_END, KEY_TAB, KEY_SPACE, KEY_B, KEY_COUNT
};
enum { CURSOR_ARROW, CURSOR_HAND, CURSOR_MOVE, CURSOR_TEXT, CURSOR_COUNT };
enum { MOUSE_L, MOUSE_R, MOUSE_M };

typedef struct {
    int   win_w, win_h;         /* client area in pixels */
    float dpi;                  /* 1.0 = 96 dpi */
    int   mx, my, mdx, mdy;     /* mouse, pixels, top-left origin */
    int   down[3], pressed[3], released[3];
    int   dbl;                  /* left double-click this frame */
    float wheel;                /* notches, + = away from user */
    int   key_down[KEY_COUNT], key_pressed[KEY_COUNT];
    unsigned text[32]; int ntext;  /* typed characters (Unicode) */
    int   ctrl, shift;
    int   resized, quit, focused;
} Input;

int    plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa, int *stencil);
void   plat_shutdown(void);
void   plat_poll(Input *in);
void   plat_swap(void);
double plat_time(void);
void   plat_sleep(double sec);
void   plat_cursor(int c);
int    plat_vsync(void);
int    plat_window_alive(void);
int    plat_font_path(int bold, char *out, int cap);
int    app_main(int argc, char **argv);
#endif
