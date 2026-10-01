#ifndef FONT_H
#define FONT_H
enum { F_SMALL, F_BODY, F_BOLD, F_H3, F_H2, F_TITLE, F_COUNT };
typedef struct { float r, g, b, a; } Color;

int   font_init(float dpi);
void  font_free(void);
float font_width(int f, const char *s, int n);     /* n < 0: whole string */
float font_line(int f);                            /* line height in px */
float font_ascent(int f);
float font_draw(int f, float x, float y_top, const char *s, int n, Color c);   /* returns end x */
/* Word-wrapped text. Returns height used. Draws only if draw != 0. */
float font_wrap(int f, float x, float y_top, float maxw, const char *s, Color c, int draw);
unsigned utf8_next(const char **s);
#endif
