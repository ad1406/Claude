/* A small TeX-like math typesetter, drawn with the font atlas and vector
   strokes, plus a rich-text layout for prose with inline $math$.

   Math syntax: letters are italic, digits upright; ^ _ { }; \frac \dfrac
   \tfrac \sqrt \overline \bar \hat \left \right (with ( [ \{ | \| \langle .)
   \bigl \bigr; \int \sum \lim \sup \inf \max \min; \sin \cos \exp \Re \Im ...;
   \text{} \mathrm{} \mathbb{}; \R \C \N \Z; Greek; \pmat{a}{b} (column vector);
   \under{expr}{label} and \over{expr}{label} (brace with a small label);
   \col{k}{...} (motif colour k) and \hl{k}{...} (tinted highlight);
   spaces \, \: \; \! \quad \qquad ~.

   Rich text: words wrap; $...$ is inline math; *...* is italic;
   [k|...] colours the words with motif colour k; a blank line or \n breaks. */
#ifndef TEX_H
#define TEX_H
#include "font.h"

typedef struct { float w, asc, desc; } TexBox;
extern int tex_unknown;     /* count of unknown commands seen (self-test) */

void   tex_set_palette(const Color *cols, int n);
TexBox tex_measure(const char *src, float px);
TexBox tex_draw(const char *src, float x, float base, float px, Color c);
/* scale px down so the formula fits maxw; returns the px used */
float  tex_fit(const char *src, float px, float maxw);

typedef struct { int face, iface; float mathpx; float line_gap; } RichStyle;
float  rich(const char *s, float x, float y_top, float maxw, RichStyle st, Color c, int draw);
#endif
