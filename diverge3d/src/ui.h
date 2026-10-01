/* Immediate-mode UI: 2D drawing in window pixels plus a small widget set. */
#ifndef UI_H
#define UI_H
#include "font.h"
#include "platform.h"

typedef struct { float x, y, w, h; } Rect;

/* palette */
extern Color C_BG, C_PANEL, C_INK, C_MUTED, C_FAINT, C_LINE, C_SOFT, C_HOVER, C_ACCENT, C_ACCENT_SOFT,
             C_ACCENT_INK, C_OK, C_OK_SOFT, C_WARN, C_WARN_SOFT, C_BAD, C_BAD_SOFT, C_TEAL, C_TEAL_SOFT, C_SHADOW;
Color rgb(unsigned hex);
Color rgba(unsigned hex, float a);
Color cmix(Color a, Color b, float t);
Color calpha(Color c, float a);

typedef struct {
    const Input *in;
    float s;                 /* dpi scale */
    unsigned hot, active, focus, last_hot;
    int  mouse_used;         /* a widget consumed the mouse this frame */
    int  want_cursor;
    double time;
    unsigned text_cursor;
} UI;
extern UI ui;

static inline Rect rect(float x, float y, float w, float h) { Rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }
static inline int in_rect(Rect r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
#define S(v) ((v) * ui.s)

void ui_begin(const Input *in, float dpi, double t, int w, int h);
void ui_end(void);
int  ui_mouse_in(Rect r);

/* drawing */
void d_rect(Rect r, Color c);
void d_rrect(Rect r, float rad, Color c);
void d_rrect_border(Rect r, float rad, float bw, Color fill, Color border);
void d_shadow(Rect r, float rad, float spread, float alpha);
void d_line(float x0, float y0, float x1, float y1, float w, Color c);
void d_circle(float cx, float cy, float r, Color c);
void d_ring(float cx, float cy, float r0, float r1, Color c);
void d_poly(const float *xy, int n, Color c);
void d_tri(float x0, float y0, float x1, float y1, float x2, float y2, Color c);
void d_vgrad(Rect r, Color top, Color bottom);
void clip_push(Rect r);
void clip_pop(void);
float d_text(int f, float x, float y, const char *s, Color c);
float d_text_center(int f, Rect r, const char *s, Color c);
float d_text_right(int f, float right, float y, const char *s, Color c);

/* widgets: id must be unique and stable per frame */
enum { BTN_PRIMARY, BTN_SECONDARY, BTN_GHOST, BTN_SEG, BTN_SEG_ON, BTN_CHIP, BTN_CHIP_ON, BTN_DANGER_ON };
int  ui_button(unsigned id, Rect r, const char *label, int style);
int  ui_hold(unsigned id, Rect r, const char *label, int extern_on);
int  ui_slider(unsigned id, Rect r, float *v, float mn, float mx, float step);
int  ui_textbox(unsigned id, Rect r, char *buf, int cap, const char *placeholder);
int  ui_row(unsigned id, Rect r, int selected);           /* clickable list row; caller draws content */
float ui_pill(float x, float y, const char *text, int kind);   /* kind: 0 info,1 ok,2 warn,3 bad; returns width */
int  ui_segmented(unsigned id, Rect r, const char **labels, int n, int *sel);
void ui_tooltip(float x, float y, const char *text);
float ui_pill_w(const char *text);

/* vertical scroll container */
typedef struct { float off, content_h, target; } Scroll;
float scroll_begin(unsigned id, Scroll *sc, Rect r);   /* returns y origin to draw from */
void  scroll_end(Scroll *sc, Rect r, float used_h);

#define UID(n) ((unsigned)(__LINE__) * 4096u + (unsigned)(n))
#endif
