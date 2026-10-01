#ifndef FONT_H
#define FONT_H

typedef struct { float r, g, b, a; } Color;

/* Faces. Text faces first, then the math faces (roman and italic at a
   ladder of sizes, used by the typesetter in tex.c). */
enum {
    F_XS, F_S, F_UI, F_UIB, F_CAP,            /* sans: 11, 12.5, 14, 14 bold, 10.5 bold caps */
    F_TX, F_TXI, F_TXB, F_TXS, F_TXSI,        /* serif prose: 16, 16 italic, 16 bold, 14, 14 italic */
    F_H3, F_H2, F_H2I, F_H1, F_H1I,           /* serif display: 19, 25, 25 italic, 42, 42 italic */
    F_MATH0                                   /* first math face */
};
#define MATH_SIZES 12
#define F_COUNT (F_MATH0 + 2 * MATH_SIZES)

typedef struct { float adv, x0, y0, x1, y1; } GlyphM;   /* ink box, pixels, y down from baseline */

int   font_init(float dpi);
void  font_free(void);
float font_px(int f);
float font_width(int f, const char *s, int n);     /* n < 0: whole string */
float font_line(int f);                            /* line height in px */
float font_ascent(int f);
float font_descent(int f);                         /* positive */
float font_draw(int f, float x, float y_top, const char *s, int n, Color c);   /* returns end x */
float font_draw_base(int f, float x, float base, const char *s, int n, Color c);
/* Word-wrapped plain text. Returns height used. Draws only if draw != 0. */
float font_wrap(int f, float x, float y_top, float maxw, const char *s, Color c, int draw);
unsigned utf8_next(const char **s);

/* Glyph level access for the math typesetter. */
int   font_has(int f, unsigned cp);
void  font_glyph(int f, unsigned cp, GlyphM *m);
void  font_glyph_draw(int f, unsigned cp, float x, float base, float scale, Color c);
int   font_math(float px, int italic);             /* nearest math face */
float font_math_px(int i);                         /* size of math ladder step i (unscaled by dpi) */
#endif
