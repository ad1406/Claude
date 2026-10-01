/* Thin OS layer: one window with an OpenGL context, input, time, fonts and
   a mono audio stream. Implemented in platform_win32.c and platform_x11.c. */
#ifndef PLATFORM_H
#define PLATFORM_H

enum {
    KEY_ESC, KEY_ENTER, KEY_BACKSPACE, KEY_DELETE, KEY_LEFT, KEY_RIGHT,
    KEY_UP, KEY_DOWN, KEY_HOME, KEY_END, KEY_TAB, KEY_SPACE, KEY_PGUP, KEY_PGDN, KEY_F1, KEY_COUNT
};
enum { CURSOR_ARROW, CURSOR_HAND, CURSOR_MOVE, CURSOR_COUNT };
enum { MOUSE_L, MOUSE_R, MOUSE_M, MOUSE_BACK, MOUSE_FWD };

typedef struct {
    int   win_w, win_h;         /* client area in pixels */
    float dpi;                  /* 1.0 = 96 dpi */
    int   mx, my, mdx, mdy;     /* mouse, pixels, top-left origin */
    int   down[5], pressed[5], released[5];   /* incl. the side (back/forward) buttons */
    float wheel;                /* notches, + = away from user */
    int   key_down[KEY_COUNT], key_pressed[KEY_COUNT];
    unsigned text[32]; int ntext;  /* typed characters (Unicode) */
    int   ctrl, shift, alt;
    int   resized, quit, focused;
} Input;

int    plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa);
void   plat_shutdown(void);
void   plat_poll(Input *in);
void   plat_swap(void);
double plat_time(void);
void   plat_sleep(double sec);
void   plat_cursor(int c);
/* Fonts: the directory that holds system fonts (with trailing separator),
   or "" when fonts are listed by absolute path (Linux). */
int    plat_font_dir(char *out, int cap);

/* Audio: 16-bit mono. fill() is called from plat_audio_pump on the main
   thread whenever the device has room. Returns 0 when there is no device. */
typedef void (*AudioFill)(float *out, int frames, int rate);
int    plat_audio_open(int rate);
void   plat_audio_pump(AudioFill fill);
void   plat_audio_close(void);

/* Diagnostics: a plain-text log in the temp folder, and a message box for
   fatal errors (stderr is invisible in a Windows GUI program). */
void   plat_log(const char *fmt, ...);
const char *plat_log_path(void);
void   plat_fatal(const char *msg);

int    app_main(int argc, char **argv);
#endif
