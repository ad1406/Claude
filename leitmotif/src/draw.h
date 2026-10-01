/* 2D drawing in window pixels: feathered (anti-aliased) strokes and fills,
   text helpers, clipping, the palette and the motif glyphs. */
#ifndef DRAW_H
#define DRAW_H
#include "font.h"
#include <math.h>
#if defined(_MSC_VER) && !defined(__cplusplus) && !defined(inline)
#define inline __inline
#endif

typedef struct { float x, y, w, h; } Rect;
static inline Rect rect(float x, float y, float w, float h) { Rect r; r.x = x; r.y = y; r.w = w; r.h = h; return r; }
static inline int in_rect(Rect r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
static inline Rect inset(Rect r, float d) { return rect(r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d); }

#define PI_F 3.14159265358979f
#define TAU_F 6.28318530717959f

/* palette */
extern Color C_PAPER, C_CARD, C_INK, C_INK2, C_MUTED, C_FAINT, C_HAIR, C_WASH, C_WHITE;
Color rgb(unsigned hex);
Color rgba(unsigned hex, float a);
Color cmix(Color a, Color b, float t);
Color calpha(Color c, float a);

extern float g_s;          /* dpi scale */
#define S(v) ((v) * g_s)

void d_begin_frame(int w, int h);
void d_rect(Rect r, Color c);
void d_rect_line(Rect r, float w, Color c);
void d_rrect(Rect r, float rad, Color c);
void d_rrect_line(Rect r, float rad, float w, Color c);
void d_vgrad(Rect r, Color top, Color bottom);
void d_line(float x0, float y0, float x1, float y1, float w, Color c);
void d_dash(float x0, float y0, float x1, float y1, float w, float on, float off, Color c);
void d_polyline(const float *xy, int n, float w, Color c);
void d_circle(float cx, float cy, float r, Color c);
void d_ring(float cx, float cy, float r, float w, Color c);
void d_arc(float cx, float cy, float r, float a0, float a1, float w, Color c);   /* radians, y up (math) */
void d_dash_arc(float cx, float cy, float r, float a0, float a1, float w, float on, float off, Color c);
void d_arrow(float x0, float y0, float x1, float y1, float w, float head, Color c);
void d_tri(float x0, float y0, float x1, float y1, float x2, float y2, Color c);
void d_poly(const float *xy, int n, Color c);   /* convex fill */
void d_quad_colors(float x0, float y0, float x1, float y1, Color c00, Color c10, Color c11, Color c01);
void clip_push(Rect r);
void clip_pop(void);

float d_text(int f, float x, float y, const char *s, Color c);
float d_text_center(int f, float cx, float y, const char *s, Color c);
float d_text_right(int f, float right, float y, const char *s, Color c);
float d_text_spaced(int f, float x, float y, const char *s, float track, Color c);   /* letter-spaced caps */
float text_spaced_w(int f, const char *s, float track);

/* motifs */
enum { MO_GROUND, MO_SPLIT, MO_TURN, MO_MIRROR, MO_LOOP, MO_HOLD, MO_LANES, MO_SHADOW, MO_COUNT };
extern Color MOTIF_COL[MO_COUNT];
void motif_glyph(int m, float cx, float cy, float size, float w, Color c);
#endif
