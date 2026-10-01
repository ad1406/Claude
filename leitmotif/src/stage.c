/* The stage. Every scene shares one visual language: a paper plane, thin ink
   axes, and the motif colours for whatever each motif produced (mirror images
   in vermilion, turns in blue, middles and half-gaps in ochre, lanes in teal,
   loops in green, held lines in violet, shadows in slate). */
#include "stage.h"
#include "tex.h"
#include "synth.h"
#include "gl_inc.h"
#include <stdio.h>
#include <string.h>

typedef struct { Rect r; float cx, cy, s; } Pl;

#define MC(m) MOTIF_COL[m]
#define LPX S(15)

static Pl pl_fit(Rect r, float x0, float x1, float y0, float y1) {
    Pl p; float sx = r.w / (x1 - x0), sy = r.h / (y1 - y0);
    p.r = r; p.s = sx < sy ? sx : sy;
    p.cx = r.x + r.w / 2 - (x0 + x1) / 2 * p.s;
    p.cy = r.y + r.h / 2 + (y0 + y1) / 2 * p.s;
    return p;
}
static float X(const Pl *p, float x) { return p->cx + x * p->s; }
static float Y(const Pl *p, float y) { return p->cy - y * p->s; }
static float IX(const Pl *p, float sx) { return (sx - p->cx) / p->s; }
static float IY(const Pl *p, float sy) { return (p->cy - sy) / p->s; }

static void lbl(float x, float base, const char *t, float px, Color c, int align) {
    TexBox b = tex_measure(t, px);
    if (align == 1) x -= b.w / 2; else if (align == 2) x -= b.w;
    tex_draw(t, x, base, px, c);
}
static void plbl(const Pl *p, float x, float y, const char *t, Color c, float dx, float dy, int align) {
    TexBox b = tex_measure(t, LPX); float sx = X(p, x) + S(dx);
    if (align == 1) sx -= b.w / 2; else if (align == 2) sx -= b.w;
    if (sx + b.w > p->r.x + p->r.w - S(4)) sx = p->r.x + p->r.w - S(4) - b.w;
    if (sx < p->r.x + S(4)) sx = p->r.x + S(4);
    tex_draw(t, sx, Y(p, y) + S(dy), LPX, c);
}
static void note(float x, float y, const char *s, Color c) { font_draw(F_TXSI, x, y, s, -1, c); }

static void pl_line(const Pl *p, float x0, float y0, float x1, float y1, float w, Color c) { d_line(X(p, x0), Y(p, y0), X(p, x1), Y(p, y1), w, c); }
static void pl_dash(const Pl *p, float x0, float y0, float x1, float y1, float w, Color c) { d_dash(X(p, x0), Y(p, y0), X(p, x1), Y(p, y1), w, S(5), S(4), c); }
static void pl_vec(const Pl *p, float x0, float y0, float x1, float y1, float w, Color c) { d_arrow(X(p, x0), Y(p, y0), X(p, x1), Y(p, y1), w, S(9), c); }
static void pl_dot(const Pl *p, float x, float y, float r, Color c) { d_circle(X(p, x), Y(p, y), r, c); }
static void pl_circle(const Pl *p, float x, float y, float rad, float w, Color c) { d_ring(X(p, x), Y(p, y), rad * p->s, w, c); }
static void pl_arc(const Pl *p, float x, float y, float rad, float a0, float a1, float w, Color c) { d_arc(X(p, x), Y(p, y), rad * p->s, a0, a1, w, c); }

static void pl_axes(const Pl *p, const char *xl, const char *yl) {
    Rect r = p->r; Color c = calpha(C_INK, 0.55f);
    float ox = X(p, 0), oy = Y(p, 0);
    if (oy > r.y && oy < r.y + r.h) { d_line(r.x, oy, r.x + r.w - S(4), oy, 1.0f, c); d_tri(r.x + r.w, oy, r.x + r.w - S(7), oy - S(3), r.x + r.w - S(7), oy + S(3), c); }
    if (ox > r.x && ox < r.x + r.w) { d_line(ox, r.y + r.h, ox, r.y + S(4), 1.0f, c); d_tri(ox, r.y, ox - S(3), r.y + S(7), ox + S(3), r.y + S(7), c); }
    if (xl) lbl(r.x + r.w - S(4), oy + S(18), xl, LPX * 0.95f, C_MUTED, 2);
    if (yl) lbl(ox + S(8), r.y + S(16), yl, LPX * 0.95f, C_MUTED, 0);
}
static void pl_grid(const Pl *p, float step, Color c) {
    float x0 = IX(p, p->r.x), x1 = IX(p, p->r.x + p->r.w), y0 = IY(p, p->r.y + p->r.h), y1 = IY(p, p->r.y), v;
    for (v = floorf(x0 / step) * step; v <= x1; v += step) d_line(X(p, v), p->r.y, X(p, v), p->r.y + p->r.h, 1, c);
    for (v = floorf(y0 / step) * step; v <= y1; v += step) d_line(p->r.x, Y(p, v), p->r.x + p->r.w, Y(p, v), 1, c);
}
static void pl_curve(const Pl *p, float (*f)(float, const float *), const float *par, float x0, float x1, int n, float w, Color c, int dashed) {
    static float pts[2 * 801]; int i;
    if (n > 800) n = 800;
    for (i = 0; i <= n; i++) { float x = x0 + (x1 - x0) * i / n; pts[2 * i] = X(p, x); pts[2 * i + 1] = Y(p, f(x, par)); }
    if (!dashed) d_polyline(pts, n + 1, w, c);
    else for (i = 0; i < n; i += 2) d_line(pts[2 * i], pts[2 * i + 1], pts[2 * i + 2], pts[2 * i + 3], w, c);
}

static float smooth01(float x) { if (x < 0) x = 0; if (x > 1) x = 1; return x * x * (3 - 2 * x); }
static float wrapf(float x, float p) { x = fmodf(x, p); return x < 0 ? x + p : x; }
static float approachf(float v, float target, float k) { return v + (target - v) * k; }

/* -------------------------------------------------------------- handles */
static int g_over;
static void handle(Stage *st, int i, const Pl *p, const Input *in, Color c, int interactive) {
    float sx = X(p, st->h[i][0]), sy = Y(p, st->h[i][1]);
    float dx = in->mx - sx, dy = in->my - sy, is_near = dx * dx + dy * dy < S(12) * S(12);
    if (interactive) {
        if (is_near) g_over = 1;
        if (in->pressed[MOUSE_L] && is_near && st->drag < 0) st->drag = i;
        if (st->drag == i) {
            if (in->down[MOUSE_L]) { st->h[i][0] = IX(p, (float)in->mx); st->h[i][1] = IY(p, (float)in->my); g_over = 1; }
            else st->drag = -1;
        }
    }
    sx = X(p, st->h[i][0]); sy = Y(p, st->h[i][1]);
    d_circle(sx, sy, S(is_near || st->drag == i ? 9 : 7), calpha(c, 0.16f));
    d_circle(sx, sy, S(4), c);
    d_ring(sx, sy, S(4), 1.2f, C_CARD);
}

/* ---------------------------------------------------------------- slider */
static void slider(Stage *st, Rect r, const Input *in, int interactive) {
    float x0 = r.x + S(150), x1 = r.x + r.w - S(70), y = r.y + r.h / 2;
    float t = (st->slider - st->smin) / (st->smax - st->smin), kx;
    static int dragging;
    char buf[32];
    if (t < 0) t = 0; if (t > 1) t = 1;
    kx = x0 + (x1 - x0) * t;
    if (st->slider_label) lbl(r.x + S(14), y + S(5), st->slider_label, S(15), C_INK2, 0);
    d_line(x0, y, x1, y, S(1.5f), C_HAIR);
    d_line(x0, y, kx, y, S(2), calpha(C_INK, 0.6f));
    if (st->sstep >= 1) {
        float v;
        for (v = st->smin; v <= st->smax + 1e-3f; v += st->sstep) { float tx = x0 + (x1 - x0) * (v - st->smin) / (st->smax - st->smin); d_line(tx, y - S(4), tx, y + S(4), 1, C_FAINT); }
    }
    if (interactive) {
        Rect hit = rect(x0 - S(10), y - S(12), x1 - x0 + S(20), S(24));
        if (in_rect(hit, (float)in->mx, (float)in->my)) g_over = 1;
        if (in->pressed[MOUSE_L] && in_rect(hit, (float)in->mx, (float)in->my) && st->drag < 0) { dragging = 1; st->drag = 99; }
        if (dragging && st->drag == 99) {
            if (in->down[MOUSE_L]) {
                float v = st->smin + (st->smax - st->smin) * ((in->mx - x0) / (x1 - x0));
                if (v < st->smin) v = st->smin; if (v > st->smax) v = st->smax;
                if (st->sstep > 0) v = st->smin + floorf((v - st->smin) / st->sstep + 0.5f) * st->sstep;
                st->slider = v;
            } else { dragging = 0; st->drag = -1; }
        }
    }
    d_circle(kx, y, S(7), C_INK); d_circle(kx, y, S(4.5f), C_CARD);
    if (st->sstep >= 1) snprintf(buf, sizeof buf, "%d", (int)floorf(st->slider + 0.5f)); else snprintf(buf, sizeof buf, "%.2f", st->slider);
    font_draw(F_S, x1 + S(14), y - font_line(F_S) / 2, buf, -1, C_INK2);
}

/* --------------------------------------------------------------- set-up */
static void want_slider(Stage *st, const char *label, float mn, float mx, float step, float v) {
    st->has_slider = 1; st->slider_label = label; st->smin = mn; st->smax = mx; st->sstep = step; st->slider = v;
}

void stage_set(Stage *st, int scene, const float *p) {
    int i; int same = st->scene == scene;
    for (i = 0; i < 4; i++) if (st->p[i] != (p ? p[i] : 0)) same = 0;
    if (same) return;
    st->scene = scene;
    for (i = 0; i < 4; i++) st->p[i] = p ? p[i] : 0;
    st->has_slider = 0; st->nh = 0; st->drag = -1; st->t = 0;
    switch (scene) {
    case SC_STANDING:
        if (st->p[1] > 0) want_slider(st, "finger at \\ell/N", 2, 6, 1, st->p[1]);
        else want_slider(st, "mode n", 1, 8, 1, st->p[0] > 0 ? st->p[0] : 1);
        break;
    case SC_PHASORS:
        if ((int)st->p[0] == 0) want_slider(st, "\\nu_1-\\nu_0\\ \\text{(Hz)}", 0, 8, 0.5f, st->p[1] > 0 ? st->p[1] : 2);
        st->nh = 2; st->h[0][0] = cosf(1.25f); st->h[0][1] = sinf(1.25f); st->h[1][0] = cosf(0.35f); st->h[1][1] = sinf(0.35f);
        break;
    case SC_PLANE:
        st->nh = 2; st->h[0][0] = 1.3f; st->h[0][1] = 0.9f; st->h[1][0] = 0.7f; st->h[1][1] = -0.5f;
        if ((int)st->p[0] == PL_PRODUCT) { st->h[1][0] = 0.8f; st->h[1][1] = 0.85f; }
        if ((int)st->p[0] == PL_TRIANGLE) { st->h[0][0] = 1.2f; st->h[0][1] = 0.5f; st->h[1][0] = 0.4f; st->h[1][1] = 1.0f; }
        if ((int)st->p[0] == PL_INVERSE) { st->h[0][0] = 1.5f; st->h[0][1] = 0.8f; }
        if ((int)st->p[0] == PL_DIVIDE) { st->h[0][0] = 1.2f; st->h[0][1] = 1.1f; st->h[1][0] = 1.1f; st->h[1][1] = 0.45f; }
        break;
    case SC_EXP:
        st->nh = 2; st->h[0][0] = -0.25f; st->h[0][1] = 1.6f; st->h[1][0] = 0; st->h[1][1] = 0;
        if ((int)st->p[0] == 3) { st->h[0][0] = cosf(0.9f); st->h[0][1] = sinf(0.9f); st->h[1][0] = cosf(0.6f); st->h[1][1] = sinf(0.6f); }
        break;
    case SC_WINDING:
        if ((int)st->p[0] == 0) want_slider(st, "m-n", -4, 4, 1, st->p[1]);
        else if ((int)st->p[0] == 1) { want_slider(st, "\\text{terms}\\ n", 1, 30, 1, st->p[1] > 0 ? st->p[1] : 6); st->nh = 1; st->h[0][0] = 0.62f; st->h[0][1] = 0.55f; }
        else want_slider(st, "t", 0.02f, 0.98f, 0.01f, 0.09f);
        break;
    case SC_RIEMANN:
        if ((int)st->p[0] == 0) want_slider(st, "\\text{pieces}", 2, 24, 1, 6);
        if ((int)st->p[0] == 1) want_slider(st, "\\delta", 0.05f, 0.8f, 0.01f, 0.3f);
        break;
    case SC_SEPARATION:
        want_slider(st, "k\\ /\\ (\\pi/\\ell)^2", -1, 10, 0.01f, st->p[0]);
        break;
    case SC_PERIODS:
        if ((int)st->p[0] == 0) want_slider(st, "\\text{shift}\\ s", -2.2f, 2.2f, 0.01f, 0.37f);
        break;
    case SC_SPACETIME:
        if ((int)st->p[0] == 4) { st->nh = 1; st->h[0][0] = 1.1f; st->h[0][1] = 0.45f; }
        break;
    case SC_SPLIT:
        st->nh = 2; st->h[0][0] = 1.6f; st->h[0][1] = 0; st->h[1][0] = 0.4f; st->h[1][1] = 0;
        break;
    }
}

/* ============================================================== scenes */

static float bump(float s, float w) { return expf(-(s * s) / (w * w)); }

/* plucked string: odd, 2l-periodic extension of a triangle (l = 1) */
static float pluck_ext(float x, float pk) {
    float u = wrapf(x + 1, 2) - 1, s = 1, h = 0.6f;     /* u in [-1,1) */
    if (u < 0) { u = -u; s = -1; }
    return s * (u <= pk ? h * u / pk : h * (1 - u) / (1 - pk));
}

static Color field_col(float v);

/* the history of the string as a space-time picture: each row is the string
   at one moment, newest at the top */
static float str_y(int mode, float x, float tau) {
    if (mode == 0) {
        float L = 2.2f, c = 0.45f, xf = L - wrapf(c * tau + 0.5f, 2 * L), xg = -L + wrapf(c * tau + 0.9f, 2 * L);
        return 0.75f * bump(x - xf, 0.32f) + 0.55f * bump(x - xg, 0.22f);
    }
    if (mode == 1) {
        float L = 2.0f, c = 0.45f, xf = L - wrapf(c * tau, 2 * L + 0.6f) + 0.3f;
        return 0.8f * bump(x - xf, 0.3f) - 0.8f * bump(-x - xf, 0.3f);
    }
    return 0.5f * pluck_ext(x + 0.22f * tau, 0.3f) + 0.5f * pluck_ext(x - 0.22f * tau, 0.3f);
}
static void spacetime_strip(const Pl *p, Rect b, int mode, float t, float x0, float x1, float span) {
    int nx = 110, ny = 44, i, j;
    float cw = (X(p, x1) - X(p, x0)) / nx, ch = b.h / ny;
    for (j = 0; j < ny; j++) for (i = 0; i < nx; i++) {
        float xa = x0 + (x1 - x0) * i / nx, xb = x0 + (x1 - x0) * (i + 1) / nx;
        float ta = t - span * j / ny, tb = t - span * (j + 1) / ny;
        float sx = X(p, x0) + i * cw, sy = b.y + j * ch;
        float dim = 1;
        if ((mode == 1 && xb <= 0) || (mode == 2 && (xb <= 0 || xa >= 1))) dim = 0.5f;
        {   float k = mode == 2 ? 2.4f : 1.3f;
            Color c00 = field_col(str_y(mode, xa, ta) * k), c10 = field_col(str_y(mode, xb, ta) * k);
            Color c11 = field_col(str_y(mode, xb, tb) * k), c01 = field_col(str_y(mode, xa, tb) * k);
            c00.a *= dim; c10.a *= dim; c11.a *= dim; c01.a *= dim;
            d_quad_colors(sx, sy, sx + cw, sy + ch, c00, c10, c11, c01); }
    }
    d_rect_line(rect(X(p, x0), b.y, X(p, x1) - X(p, x0), b.h), 1, C_HAIR);
    d_line(X(p, x0), b.y, X(p, x1), b.y, S(1.4f), calpha(C_INK, 0.6f));
    lbl(X(p, x0) + S(4), b.y + b.h + S(16), "x", S(14), C_MUTED, 0);
    d_arrow(X(p, x0) - S(10), b.y + b.h, X(p, x0) - S(10), b.y + S(4), 1, S(6), C_MUTED);
    lbl(X(p, x0) - S(14), b.y + S(12), "t", S(14), C_MUTED, 2);
    d_text(F_XS, X(p, x1) - S(4) - font_width(F_XS, "now", -1), b.y - S(16), "now", C_MUTED);
    d_text(F_XS, X(p, x0) + S(14), b.y + b.h + S(4), "space-time: each row is the string at one moment; warm is up, cool is down", C_MUTED);
}

static void shade_world(const Pl *p, float x0, float x1, Color c) {
    float a = X(p, x0), b = X(p, x1);
    if (a < p->r.x) a = p->r.x; if (b > p->r.x + p->r.w) b = p->r.x + p->r.w;
    if (b > a) d_rect(rect(a, p->r.y, b - a, p->r.h), c);
}
static void mirror_wall(const Pl *p, float x, float y0, float y1) {
    d_dash(X(p, x), Y(p, y0), X(p, x), Y(p, y1), S(1.6f), S(6), S(4), MC(MO_MIRROR));
}
static void nail(const Pl *p, float x) { d_circle(X(p, x), Y(p, 0), S(4.5f), C_INK); d_circle(X(p, x), Y(p, 0), S(2), C_CARD); }

static void sc_string(Stage *st, Rect r) {
    int mode = (int)st->p[0], i, n = 360;
    float t = st->t;
    static float A[2 * 361], B[2 * 361], Sm[2 * 361];
    Rect top = rect(r.x, r.y + S(10), r.w, r.h * 0.5f), bot = rect(r.x, r.y + r.h * 0.58f, r.w, r.h * 0.34f);
    if (mode == 0) {
        Pl p = pl_fit(top, -2.3f, 2.3f, -0.6f, 1.25f);
        float L = 2.2f, c = 0.45f;
        float xf = L - wrapf(c * t + 0.5f, 2 * L) , xg = -L + wrapf(c * t + 0.9f, 2 * L);
        pl_line(&p, -L, 0, L, 0, 1, C_HAIR);
        for (i = 0; i <= n; i++) {
            float x = -L + 2 * L * i / n, f = 0.75f * bump(x - xf, 0.32f), g = 0.55f * bump(x - xg, 0.22f);
            A[2 * i] = B[2 * i] = Sm[2 * i] = X(&p, x);
            A[2 * i + 1] = Y(&p, f); B[2 * i + 1] = Y(&p, g); Sm[2 * i + 1] = Y(&p, f + g);
        }
        d_polyline(A, n + 1, S(1.4f), calpha(MC(MO_SPLIT), 0.8f));
        d_polyline(B, n + 1, S(1.4f), calpha(MC(MO_TURN), 0.8f));
        d_polyline(Sm, n + 1, S(2.4f), C_INK);
        plbl(&p, xf, 0.85f, "f(x+ct)", MC(MO_SPLIT), 0, 0, 1);
        plbl(&p, xg, 0.66f, "g(x-ct)", MC(MO_TURN), 0, 0, 1);
        pl_vec(&p, xf - 0.05f, 0.98f, xf - 0.45f, 0.98f, S(1.3f), MC(MO_SPLIT));
        pl_vec(&p, xg + 0.05f, 0.8f, xg + 0.45f, 0.8f, S(1.3f), MC(MO_TURN));
        spacetime_strip(&p, bot, 0, t, -2.2f, 2.2f, 9);
        note(r.x + S(16), r.y + S(12), "two shapes, sliding through each other at speed c: in space-time, two diagonal ridges", C_MUTED);
    } else if (mode == 1) {
        Pl p = pl_fit(top, -2.1f, 2.1f, -1.05f, 1.25f);
        float L = 2.0f, c = 0.45f, xf = L - wrapf(c * t, 2 * L + 0.6f) + 0.3f;
        shade_world(&p, -L - 1, 0, calpha(MC(MO_MIRROR), 0.05f));
        pl_line(&p, -L, 0, L, 0, 1, C_HAIR);
        for (i = 0; i <= n; i++) {
            float x = -L + 2 * L * i / n, f = 0.8f * bump(x - xf, 0.3f), g = -0.8f * bump(-x - xf, 0.3f);
            A[2 * i] = B[2 * i] = Sm[2 * i] = X(&p, x);
            A[2 * i + 1] = Y(&p, f); B[2 * i + 1] = Y(&p, g); Sm[2 * i + 1] = Y(&p, f + g);
        }
        for (i = 0; i < n; i += 2) {
            d_line(A[2 * i], A[2 * i + 1], A[2 * i + 2], A[2 * i + 3], S(1.3f), calpha(MC(MO_SPLIT), 0.85f));
            d_line(B[2 * i], B[2 * i + 1], B[2 * i + 2], B[2 * i + 3], S(1.3f), calpha(MC(MO_MIRROR), 0.85f));
        }
        d_polyline(Sm + n, n / 2 + 1, S(2.6f), C_INK);
        mirror_wall(&p, 0, -1.05f, 1.2f); nail(&p, 0);
        plbl(&p, 0.05f, 1.0f, "x=0", MC(MO_MIRROR), 4, 0, 0);
        note(X(&p, -L) + S(6), r.y + S(12), "mirror world", calpha(MC(MO_MIRROR), 0.8f));
        note(X(&p, 0.9f), r.y + S(12), "the string", C_MUTED);
        plbl(&p, xf, 0.95f, "f(x+ct)", MC(MO_SPLIT), 0, 0, 1);
        plbl(&p, -xf, -1.0f, "-f(-x+ct)", MC(MO_MIRROR), 0, 0, 1);
        spacetime_strip(&p, bot, 1, t, -2.0f, 2.0f, 9);
        d_dash(X(&p, 0), bot.y, X(&p, 0), bot.y + bot.h, S(1.6f), S(6), S(4), MC(MO_MIRROR));
    } else if (mode == 2) {
        Pl p = pl_fit(top, -1.05f, 2.05f, -0.75f, 0.85f);
        float c = 0.22f, pk = 0.3f;
        shade_world(&p, -2, 0, calpha(MC(MO_MIRROR), 0.05f));
        shade_world(&p, 1, 3, calpha(MC(MO_MIRROR), 0.05f));
        pl_line(&p, -1.05f, 0, 2.05f, 0, 1, C_HAIR);
        for (i = 0; i <= n; i++) {
            float x = -1.05f + 3.1f * i / n, a = 0.5f * pluck_ext(x + c * t, pk), b = 0.5f * pluck_ext(x - c * t, pk);
            A[2 * i] = B[2 * i] = Sm[2 * i] = X(&p, x);
            A[2 * i + 1] = Y(&p, a); B[2 * i + 1] = Y(&p, b); Sm[2 * i + 1] = Y(&p, a + b);
        }
        for (i = 0; i < n; i += 2) {
            d_line(A[2 * i], A[2 * i + 1], A[2 * i + 2], A[2 * i + 3], S(1.2f), calpha(MC(MO_SPLIT), 0.75f));
            d_line(B[2 * i], B[2 * i + 1], B[2 * i + 2], B[2 * i + 3], S(1.2f), calpha(MC(MO_TURN), 0.75f));
        }
        d_polyline(Sm, n + 1, S(1.2f), calpha(C_INK, 0.25f));
        {   int i0 = (int)(n * (1.05f / 3.1f) + 0.5f), i1 = (int)(n * (2.05f / 3.1f) + 0.5f);
            d_polyline(Sm + 2 * i0, i1 - i0 + 1, S(2.8f), C_INK); }
        mirror_wall(&p, 0, -0.9f, 0.95f); mirror_wall(&p, 1, -0.9f, 0.95f);
        nail(&p, 0); nail(&p, 1);
        plbl(&p, 0, -0.92f, "0", C_MUTED, 0, 0, 1); plbl(&p, 1, -0.92f, "\\ell", C_MUTED, 0, 0, 1);
        note(X(&p, -1) + S(6), r.y + S(12), "mirror world", calpha(MC(MO_MIRROR), 0.8f));
        note(X(&p, 1.04f), r.y + S(12), "mirror world", calpha(MC(MO_MIRROR), 0.8f));
        note(X(&p, 0.05f), r.y + S(12), "plucked string", C_MUTED);
        spacetime_strip(&p, bot, 2, t, -1.05f, 2.05f, 9);
        d_dash(X(&p, 0), bot.y, X(&p, 0), bot.y + bot.h, S(1.6f), S(6), S(4), MC(MO_MIRROR));
        d_dash(X(&p, 1), bot.y, X(&p, 1), bot.y + bot.h, S(1.6f), S(6), S(4), MC(MO_MIRROR));
    } else {   /* two mirrors make one shift */
        Pl p = pl_fit(inset(r, S(10)), -1.6f, 3.6f, -1.35f, 1.35f);
        float lam = 0.35f + 0.25f * sinf(t * 0.6f), ph = wrapf(t * 0.35f, 3.0f);
        float k1 = smooth01(ph / 0.8f), k2 = smooth01((ph - 1.0f) / 0.8f);
        float ytop = 0.45f, flag = 0.3f;
        /* number line and mirrors */
        pl_line(&p, -1.6f, ytop, 3.6f, ytop, 1, C_HAIR);
        mirror_wall(&p, 0, ytop - 0.3f, ytop + 0.8f); mirror_wall(&p, 1, ytop - 0.3f, ytop + 0.8f);
        plbl(&p, 0, ytop - 0.5f, "0", C_MUTED, 0, 0, 1); plbl(&p, 1, ytop - 0.5f, "\\ell", C_MUTED, 0, 0, 1);
        plbl(&p, 2, ytop - 0.5f, "2\\ell", C_MUTED, 0, 0, 1);
        {   /* a flag at lam; its image in 0 (at -lam, flipped); that image's image in l (at 2l+lam, upright again) */
            float xs[3]; int k; float ori[3] = { 1, -1, 1 };
            xs[0] = lam; xs[1] = -lam; xs[2] = 2 + lam;
            for (k = 0; k < 3; k++) {
                float a = k == 0 ? 1 : k == 1 ? k1 : k2; float x = xs[k], o = ori[k];
                Color c = k == 0 ? C_INK : k == 1 ? MC(MO_MIRROR) : MC(MO_LOOP);
                if (a <= 0.01f) continue;
                pl_line(&p, x, ytop, x, ytop + 0.62f, S(2), calpha(c, a));
                d_tri(X(&p, x), Y(&p, ytop + 0.62f), X(&p, x + o * flag), Y(&p, ytop + 0.5f), X(&p, x), Y(&p, ytop + 0.36f), calpha(c, a));
            }
            if (k1 > 0.02f) pl_arc(&p, 0, ytop, lam, 0.15f, PI_F - 0.15f, S(1.1f), calpha(MC(MO_MIRROR), 0.6f * k1));
            if (k2 > 0.02f) pl_arc(&p, 1, ytop, 1 + lam, 0.05f, PI_F - 0.05f, S(1.1f), calpha(MC(MO_MIRROR), 0.6f * k2));
            if (k2 > 0.5f) {
                float a = (k2 - 0.5f) * 2;
                pl_vec(&p, lam, ytop - 0.18f, 2 + lam, ytop - 0.18f, S(1.8f), calpha(MC(MO_LOOP), a));
                plbl(&p, 1 + lam, ytop - 0.24f, "\\text{shift by }2\\ell", calpha(MC(MO_LOOP), a), 0, 14, 1);
            }
        }
        /* the 2l-periodic f below */
        {   float base = -0.75f; static float pts[2 * 301];
            for (i = 0; i <= 300; i++) { float x = -1.6f + 5.2f * i / 300; pts[2 * i] = X(&p, x); pts[2 * i + 1] = Y(&p, base + 0.35f * pluck_ext(x - 0.15f, 0.3f) / 0.6f); }
            pl_line(&p, -1.6f, base, 3.6f, base, 1, C_HAIR);
            d_polyline(pts, 301, S(1.8f), MC(MO_LOOP));
            plbl(&p, -1.5f, base + 0.45f, "f(\\lambda+2\\ell)=f(\\lambda)", MC(MO_LOOP), 0, 0, 0);
            for (i = -1; i <= 3; i += 2) pl_dash(&p, (float)i, base - 0.4f, (float)i, base + 0.4f, 1, calpha(MC(MO_LOOP), 0.4f));
        }
        note(r.x + S(16), r.y + S(12), "reflect in 0, then in \u2113: the flag is upright again, moved by 2\u2113", C_MUTED);
    }
}

/* field colours: positive warm, negative cool */
static Color field_col(float v) {
    Color pos = MC(MO_SPLIT), neg = MC(MO_TURN);
    float a = fabsf(v); if (a > 1) a = 1;
    return calpha(v >= 0 ? pos : neg, a * 0.55f);
}

static void sc_spacetime(Stage *st, Rect r, const Input *in, int interactive) {
    int mode = (int)st->p[0];
    Pl p = pl_fit(rect(r.x + S(10), r.y + S(10), r.w - S(20), r.h - S(20)), -2.2f, 2.2f, -1.6f, 1.6f);
    float t = st->t;
    if (mode == 2 || mode == 3) {   /* heat field */
        int nx = 70, ny = 52, i, j;
        float x0 = IX(&p, p.r.x), x1 = IX(&p, p.r.x + p.r.w), y0 = IY(&p, p.r.y + p.r.h), y1 = IY(&p, p.r.y);
        for (j = 0; j < ny; j++) for (i = 0; i < nx; i++) {
            float xa = x0 + (x1 - x0) * i / nx, xb = x0 + (x1 - x0) * (i + 1) / nx;
            float ya = y0 + (y1 - y0) * j / ny, yb = y0 + (y1 - y0) * (j + 1) / ny;
            float vv[4]; int k; float xs[4] = { xa, xb, xb, xa }, ys[4] = { ya, ya, yb, yb };
            for (k = 0; k < 4; k++) {
                float u = xs[k] + ys[k], v = xs[k] - ys[k];
                vv[k] = mode == 2 ? 0.8f * sinf(1.7f * v) * bump(v, 2.2f) + 0.3f * cosf(3.1f * v)
                                  : 0.9f * bump(u - 0.6f, 0.35f) + 0.9f * bump(v + 0.4f, 0.3f);
            }
            d_quad_colors(X(&p, xa), Y(&p, ya), X(&p, xb), Y(&p, yb), field_col(vv[0]), field_col(vv[1]), field_col(vv[2]), field_col(vv[3]));
        }
    }
    pl_axes(&p, "x", "ct");
    if (mode == 0 || mode == 1 || mode == 4) {
        float a = mode == 0 ? (PI_F / 4) * smooth01(0.5f + 0.5f * sinf(t * 0.7f) * 1.4f) : PI_F / 4;
        int k;
        for (k = -6; k <= 6; k++) {   /* lines u = const and v = const */
            float d = k * 0.5f, ca = cosf(a), sa = sinf(a), L = 4;
            /* direction of the u-axis: (sin a, cos a); v-axis: (cos a, -sin a) */
            float ux = sa, uy = ca, vx = ca, vy = -sa;
            pl_line(&p, d * vx - L * ux, d * vy - L * uy, d * vx + L * ux, d * vy + L * uy, 1, calpha(MC(MO_SPLIT), k == 0 ? 0.9f : 0.18f));
            pl_line(&p, d * ux - L * vx, d * uy - L * vy, d * ux + L * vx, d * uy + L * vy, 1, calpha(MC(MO_TURN), k == 0 ? 0.9f : 0.18f));
        }
        {   float ca = cosf(a), sa = sinf(a);
            plbl(&p, 1.45f * sa, 1.45f * ca, "u", MC(MO_SPLIT), 6, 0, 0);
            plbl(&p, 1.9f * ca, -1.9f * sa, "v", MC(MO_TURN), 6, 0, 0);
        }
        if (mode == 0) note(r.x + S(16), r.y + S(12), "the u- and v-axes are the ct- and x-axes turned by \u03c0/4", C_MUTED);
    }
    if (mode == 1) {
        float px = -0.3f, py = -0.2f, s = 0.9f, k = wrapf(t * 0.35f, 1.0f);
        pl_vec(&p, px, py, px + s, py + s, S(2), MC(MO_SPLIT));
        pl_vec(&p, px, py, px + s, py - s, S(2), MC(MO_TURN));
        pl_dot(&p, px + s * k, py + s * k, S(4), MC(MO_SPLIT));
        pl_dot(&p, px + s * k, py - s * k, S(4), MC(MO_TURN));
        pl_dot(&p, px, py, S(3), C_INK);
        plbl(&p, px + s, py + s, "\\partial_x+\\tfrac1c\\partial_t=2\\partial_u", MC(MO_SPLIT), 8, -2, 0);
        plbl(&p, px + s, py - s, "\\partial_x-\\tfrac1c\\partial_t=2\\partial_v", MC(MO_TURN), 8, 14, 0);
        note(r.x + S(16), r.y + S(12), "each factor of \u25a1 differentiates along one diagonal", C_MUTED);
    }
    if (mode == 2) {
        float v0 = 0.6f, k = wrapf(t * 0.25f, 1.0f) * 3.2f - 1.6f;
        /* the held line v = v0 runs in direction (1,1) */
        pl_line(&p, v0 / 2 - 2, -v0 / 2 - 2, v0 / 2 + 2, -v0 / 2 + 2, S(2), MC(MO_HOLD));
        pl_dot(&p, v0 / 2 + k, -v0 / 2 + k, S(6), MC(MO_HOLD));
        plbl(&p, v0 / 2 + 1.25f, -v0 / 2 + 1.25f, "v=v_0\\ \\text{held}", MC(MO_HOLD), 10, 0, 0);
        {   char buf[64]; float val = 0.8f * sinf(1.7f * v0) * bump(v0, 2.2f) + 0.3f * cosf(3.1f * v0);
            snprintf(buf, sizeof buf, "\\partial_v y=h(v_0)=%.3f", val);
            plbl(&p, v0 / 2 + k, -v0 / 2 + k, buf, C_INK, 12, 18, 0); }
        note(r.x + S(16), r.y + S(12), "walk along u with v held: \u2202y/\u2202v never changes, so it is a function of v alone", C_MUTED);
    }
    if (mode == 3) {
        float ct = wrapf(t * 0.25f, 3.0f) - 1.5f; static float pts[2 * 201]; int i;
        pl_line(&p, -2.2f, ct, 2.2f, ct, S(1.2f), calpha(C_INK, 0.5f));
        for (i = 0; i <= 200; i++) { float x = -2.2f + 4.4f * i / 200, u = x + ct, v = x - ct;
            pts[2 * i] = X(&p, x); pts[2 * i + 1] = Y(&p, ct) - S(38) * (0.9f * bump(u - 0.6f, 0.35f) + 0.9f * bump(v + 0.4f, 0.3f)); }
        d_polyline(pts, 201, S(2), C_INK);
        plbl(&p, 2.1f, ct, "y(\\cdot,t)", C_INK, 0, -6, 2);
        plbl(&p, -1.9f, 1.4f, "f(u)", MC(MO_SPLIT), 0, 0, 0);
        plbl(&p, 1.5f, 1.4f, "g(v)", MC(MO_TURN), 0, 0, 0);
        note(r.x + S(16), r.y + S(12), "y = f(u) + g(v): two ridges along the diagonals. Each time-slice is the string.", C_MUTED);
    }
    if (mode == 4) {
        float zx, zy, wx_, wy_;
        handle(st, 0, &p, in, C_INK, interactive);
        zx = st->h[0][0]; zy = st->h[0][1];
        wx_ = zx - zy; wy_ = zx + zy;           /* (1+i)(x+i ct) = (x-ct) + i(x+ct) = v + iu */
        pl_vec(&p, 0, 0, zx, zy, S(1.6f), C_INK);
        pl_vec(&p, 0, 0, wx_, wy_, S(2), MC(MO_TURN));
        {   float a0 = atan2f(zy, zx), rr = 0.45f; pl_arc(&p, 0, 0, rr, a0, a0 + PI_F / 4, S(1.4f), MC(MO_TURN)); }
        plbl(&p, zx, zy, "x+ict", C_INK, 8, 4, 0);
        plbl(&p, wx_, wy_, "v+iu=(1+i)(x+ict)", MC(MO_TURN), 8, 4, 0);
        note(r.x + S(16), r.y + S(12), "drag the point: d'Alembert's coordinates are a turn by \u03c0/4 and a stretch by \u221a2", C_MUTED);
    }
}

static void sc_standing(Stage *st, Rect r) {
    int finger = st->p[1] > 0, n = finger ? 0 : (int)(st->slider + 0.5f), N = finger ? (int)(st->slider + 0.5f) : 0, i, k;
    int parts = st->p[2] > 0;
    Rect top = rect(r.x, r.y, r.w, r.h * 0.64f), bot = rect(r.x + S(30), r.y + r.h * 0.66f, r.w - S(60), r.h * 0.3f);
    Pl p = pl_fit(rect(top.x + S(30), top.y + S(26), top.w - S(60), top.h - S(40)), -0.05f, 1.05f, -0.75f, 0.75f);
    float t = st->t, w0 = 1.4f;
    static float A[2 * 401], B[2 * 401], Sm[2 * 401];
    pl_line(&p, 0, 0, 1, 0, 1, C_HAIR);
    for (i = 0; i <= 400; i++) {
        float x = (float)i / 400, y = 0, a = 0, b = 0;
        if (!finger) {
            float kx = n * PI_F * x, wt = n * w0 * t * 0.5f, K = 0.27f;
            y = 2 * K * sinf(kx) * cosf(wt);
            a = K * sinf(kx + wt); b = -K * sinf(-kx + wt);
        } else {
            for (k = N; k <= 12; k += N) y += 0.5f / (float)(k / N) * sinf(k * PI_F * x) * cosf(k * w0 * t * 0.25f);
        }
        A[2 * i] = B[2 * i] = Sm[2 * i] = X(&p, x);
        A[2 * i + 1] = Y(&p, a); B[2 * i + 1] = Y(&p, b); Sm[2 * i + 1] = Y(&p, y);
    }
    if (!finger) {
        static float E1[2 * 401], E2[2 * 401];
        for (i = 0; i <= 400; i++) { float x = (float)i / 400, e = 0.54f * fabsf(sinf(n * PI_F * x)); E1[2 * i] = E2[2 * i] = X(&p, x); E1[2 * i + 1] = Y(&p, e); E2[2 * i + 1] = Y(&p, -e); }
        d_polyline(E1, 401, 1, calpha(C_INK, 0.12f)); d_polyline(E2, 401, 1, calpha(C_INK, 0.12f));
    }
    if (parts && !finger) {
        for (i = 0; i < 400; i += 2) {
            d_line(A[2 * i], A[2 * i + 1], A[2 * i + 2], A[2 * i + 3], S(1.2f), calpha(MC(MO_SPLIT), 0.8f));
            d_line(B[2 * i], B[2 * i + 1], B[2 * i + 2], B[2 * i + 3], S(1.2f), calpha(MC(MO_MIRROR), 0.8f));
        }
        plbl(&p, 0.02f, 0.62f, "f(x+ct)", MC(MO_SPLIT), 0, 0, 0);
        plbl(&p, 0.3f, 0.62f, "-f(-x+ct)", MC(MO_MIRROR), 0, 0, 0);
    }
    d_polyline(Sm, 401, S(2.6f), C_INK);
    nail(&p, 0); nail(&p, 1);
    if (!finger) for (k = 1; k < n; k++) d_ring(X(&p, (float)k / n), Y(&p, 0), S(4), S(1.3f), MC(MO_LOOP));
    if (finger) {
        float fx = 1.0f / N;
        d_line(X(&p, fx), Y(&p, 0) - S(46), X(&p, fx), Y(&p, 0) - S(8), S(3), MC(MO_HOLD));
        d_circle(X(&p, fx), Y(&p, 0) - S(50), S(5), MC(MO_HOLD));
        plbl(&p, fx, 0, "\\tfrac{\\ell}{N}", MC(MO_HOLD), 8, -30, 0);
    }
    {   char buf[96];
        if (!finger) snprintf(buf, sizeof buf, "\\nu_{%d}=%d\\,\\nu_1,\\quad \\lambda_{%d}=\\tfrac{2\\ell}{%d}", n, n, n, n);
        else snprintf(buf, sizeof buf, "\\text{survivors: }n\\in%d\\N,\\quad \\text{pitch }\\nu_{%d}=%d\\nu_1", N, N, N);
        lbl(top.x + S(16), top.y + S(22), buf, S(16), C_INK2, 0);
    }
    /* spectrum */
    {   float bw = bot.w / 12, base = bot.y + bot.h - S(16);
        for (k = 1; k <= 12; k++) {
            float h = (bot.h - S(30)) / sqrtf((float)k), x = bot.x + (k - 0.5f) * bw;
            int alive = finger ? (k % N == 0) : 1, cur = !finger && k == n;
            Color c = cur ? C_INK : alive ? calpha(MC(MO_LOOP), finger ? 0.85f : 0.45f) : calpha(C_FAINT, 0.5f);
            char nb[8];
            d_rect(rect(x - bw * 0.18f, base - h, bw * 0.36f, h), c);
            if (finger && !alive) d_line(x - bw * 0.25f, base - h * 0.5f - S(6), x + bw * 0.25f, base - h * 0.5f + S(6), S(1.5f), MC(MO_MIRROR));
            snprintf(nb, sizeof nb, "%d", k);
            d_text_center(F_XS, x, base + S(3), nb, C_MUTED);
        }
        lbl(bot.x, bot.y - S(4), "\\text{harmonics}\\quad\\nu_n=n\\,\\nu_1", S(14), C_MUTED, 0);
    }
    if (st->sound) {
        if (!finger) synth_hold(0, 110.0f * n, 0.09f), synth_hold(1, 0, 0);
        else synth_hold(0, 110.0f * N, 0.08f), synth_hold(1, 220.0f * N, 0.03f);
        synth_hold(2, 0, 0); synth_hold(3, 0, 0);
    }
}

static float per_f(float t, const float *par) { float T = par[0]; return 0.42f * sinf(TAU_F * t / T) + 0.2f * sinf(2 * TAU_F * t / T + 1.1f) + 0.08f * cosf(3 * TAU_F * t / T); }

static void sc_periods(Stage *st, Rect r) {
    int mode = (int)st->p[0], k;
    Pl p = pl_fit(rect(r.x + S(16), r.y + S(30), r.w - S(32), r.h - S(40)), -2.6f, 2.6f, -1.2f, 1.0f);
    float T = 0.8f; float t = st->t;
    if (mode == 0) {
        float s = st->slider, base = -0.85f, off = fabsf(s / T - floorf(s / T + 0.5f));
        int match = off < 0.012f;
        pl_line(&p, -2.6f, 0.2f, 2.6f, 0.2f, 1, C_HAIR);
        {   float pp[1]; pp[0] = T;
            Pl q = p; q.cy = Y(&p, 0.2f);
            pl_curve(&q, per_f, pp, -2.6f, 2.6f, 400, S(2.2f), C_INK, 0);
            {   static float pts[2 * 401]; int i;
                for (i = 0; i <= 400; i++) { float x = -2.6f + 5.2f * i / 400; pts[2 * i] = X(&q, x); pts[2 * i + 1] = Y(&q, per_f(x + s, pp)); }
                d_polyline(pts, 401, S(1.6f), calpha(match ? MC(MO_LOOP) : MC(MO_MIRROR), 0.75f)); }
        }
        plbl(&p, -2.5f, 0.85f, "f(t)", C_INK, 0, 0, 0);
        plbl(&p, -2.0f, 0.85f, match ? "f(t+s)=f(t)" : "f(t+s)", match ? MC(MO_LOOP) : MC(MO_MIRROR), 0, 0, 0);
        pl_line(&p, -2.6f, base, 2.6f, base, 1, calpha(C_INK, 0.5f));
        for (k = -3; k <= 3; k++) { pl_dot(&p, k * T, base, S(4), MC(MO_LOOP)); }
        plbl(&p, T, base, "T", MC(MO_LOOP), 0, 18, 1); plbl(&p, 2 * T, base, "2T", MC(MO_LOOP), 0, 18, 1);
        plbl(&p, -T, base, "-T", MC(MO_LOOP), 0, 18, 1); plbl(&p, 0, base, "0", C_MUTED, 0, 18, 1);
        d_line(X(&p, s), Y(&p, base) - S(12), X(&p, s), Y(&p, base) + S(12), S(2.4f), match ? MC(MO_LOOP) : MC(MO_MIRROR));
        note(r.x + S(16), r.y + S(10), match ? "s is a period: the shifted copy is the same function" : "slide s: only at the periods \u03a0f does the copy coincide", C_MUTED);
    } else if (mode == 1) {
        int a = (int)wrapf(floorf(t / 2.5f), 5) - 2, b = (int)wrapf(floorf(t / 2.5f) * 3 + 1, 5) - 2;
        float base = -0.1f, y1 = 0.35f, y2 = 0.65f;
        if (a == 0) a = 2; if (b == 0) b = -1;
        pl_line(&p, -2.6f, base, 2.6f, base, 1, calpha(C_INK, 0.5f));
        for (k = -3; k <= 3; k++) pl_dot(&p, k * T, base, S(4), MC(MO_LOOP));
        pl_vec(&p, 0, y1, a * T, y1, S(2), MC(MO_LOOP));
        pl_vec(&p, a * T, y2, (a + b) * T, y2, S(2), calpha(MC(MO_LOOP), 0.7f));
        pl_dash(&p, (a + b) * T, y2, (a + b) * T, base, 1, MC(MO_LOOP));
        plbl(&p, a * T / 2, y1, "T_1", MC(MO_LOOP), 0, -6, 1);
        plbl(&p, a * T + b * T / 2, y2, "T_2", MC(MO_LOOP), 0, -6, 1);
        plbl(&p, (a + b) * T, base, "T_1+T_2\\in\\Pi_f", MC(MO_LOOP), 0, 22, 1);
        note(r.x + S(16), r.y + S(10), "two loops in a row are a loop; periods form a group", C_MUTED);
    } else {
        int kk = 2 + (int)wrapf(floorf(t / 1.6f), 9);
        float Tk = 2.0f / kk, base = 0.0f, t0 = -1.2f, tt = 1.7f;
        float r_ = wrapf(tt - t0, Tk);
        pl_line(&p, -2.6f, base, 2.6f, base, 1, calpha(C_INK, 0.5f));
        for (k = -40; k <= 40; k++) { float x = t0 + k * Tk; if (x > -2.6f && x < 2.6f) d_line(X(&p, x), Y(&p, base) - S(5), X(&p, x), Y(&p, base) + S(5), 1, calpha(MC(MO_LOOP), 0.7f)); }
        d_rect(rect(X(&p, t0 - 0.12f), Y(&p, base) - S(16), 0.24f * p.s, S(32)), calpha(MC(MO_SHADOW), 0.15f));
        pl_dot(&p, t0, base, S(4), C_INK); plbl(&p, t0, base, "t_0", C_INK, 0, 26, 1);
        pl_dot(&p, tt, base, S(4), C_INK); plbl(&p, tt, base, "t", C_INK, 0, 26, 1);
        pl_arc(&p, (tt + t0 + r_) / 2, base, (tt - t0 - r_) / 2, 0.05f, PI_F - 0.05f, S(1.4f), MC(MO_LOOP));
        pl_dot(&p, t0 + r_, base, S(4), MC(MO_LOOP));
        {   char buf[80]; snprintf(buf, sizeof buf, "T_k=%.3f,\\quad r_k=%.3f\\to0", Tk, r_); plbl(&p, -2.5f, 0.75f, buf, C_INK2, 0, 0, 0); }
        note(r.x + S(16), r.y + S(10), "whole loops carry t back to within r\u2096 of t\u2080; continuity at t\u2080 does the rest", C_MUTED);
    }
}

static void sc_phasors(Stage *st, Rect r, const Input *in, int interactive) {
    int mode = (int)st->p[0];
    Rect left = rect(r.x, r.y, mode == 0 ? r.w * 0.45f : r.w, r.h);
    Pl p = pl_fit(inset(left, S(24)), -2.2f, 2.2f, -2.2f, 2.2f);
    float al, be, t = st->t;
    if (mode == 0) {
        float d = st->slider, w0 = TAU_F * 0.22f, w1 = w0 + TAU_F * d * 0.035f;
        al = w1 * t + 0.6f; be = w0 * t + 0.6f;
    } else {
        handle(st, 0, &p, in, MC(MO_TURN), interactive);
        handle(st, 1, &p, in, MC(MO_TURN), interactive);
        al = atan2f(st->h[0][1], st->h[0][0]); be = atan2f(st->h[1][1], st->h[1][0]);
    }
    {
        float ax = cosf(al), ay = sinf(al), bx = cosf(be), by = sinf(be), sx = ax + bx, sy = ay + by;
        float g = (al + be) / 2, dl = (al - be) / 2, len = 2 * cosf(dl);
        if (mode != 0) { st->h[0][0] = ax; st->h[0][1] = ay; st->h[1][0] = bx; st->h[1][1] = by; }
        pl_axes(&p, "\\Re", "\\Im");
        pl_circle(&p, 0, 0, 1, 1, C_HAIR);
        pl_dash(&p, 0, 0, 2.15f * cosf(g), 2.15f * sinf(g), S(1.2f), MC(MO_SPLIT));
        pl_line(&p, ax, ay, sx, sy, 1, calpha(MC(MO_TURN), 0.4f));
        pl_line(&p, bx, by, sx, sy, 1, calpha(MC(MO_TURN), 0.4f));
        pl_vec(&p, 0, 0, ax, ay, S(2), MC(MO_TURN));
        pl_vec(&p, 0, 0, bx, by, S(2), calpha(MC(MO_TURN), 0.65f));
        pl_vec(&p, 0, 0, sx, sy, S(2.6f), C_INK);
        pl_arc(&p, 0, 0, 0.42f, g, al, S(1.4f), MC(MO_SPLIT));
        pl_arc(&p, 0, 0, 0.36f, be, g, S(1.4f), MC(MO_SPLIT));
        plbl(&p, ax, ay, "e^{i\\alpha}", MC(MO_TURN), 6, -4, 0);
        plbl(&p, bx, by, "e^{i\\beta}", MC(MO_TURN), 6, 14, 0);
        plbl(&p, 0.6f * cosf((g + al) / 2), 0.6f * sinf((g + al) / 2), "\\delta", MC(MO_SPLIT), 0, 5, 1);
        plbl(&p, 2.15f * cosf(g), 2.15f * sinf(g), "\\gamma", MC(MO_SPLIT), 6, 0, 0);
        {   char buf[64]; snprintf(buf, sizeof buf, "2\\cos\\delta=%.2f", len);
            plbl(&p, sx, sy, buf, C_INK, 8, -6, 0); }
        /* the Im lane: heights add */
        pl_dash(&p, sx, sy, -1.95f, sy, 1, calpha(MC(MO_LANES), 0.7f));
        pl_line(&p, -1.95f, 0, -1.95f, sy, S(3), MC(MO_LANES));
        plbl(&p, -1.95f, sy, "\\sin\\alpha+\\sin\\beta", MC(MO_LANES), -4, sy > 0 ? -6 : 18, 0);
    }
    if (mode == 0) {
        Rect wr = rect(r.x + r.w * 0.47f, r.y + S(40), r.w * 0.5f, r.h - S(80));
        float d = st->slider, f0 = 9, f1 = 9 + d, Tw = 2.0f, cur = wrapf(t * 0.25f, Tw);
        static float pts[2 * 801], e1[2 * 201], e2[2 * 201]; int i;
        float mid = wr.y + wr.h / 2, amp = wr.h * 0.22f;
        d_line(wr.x, mid, wr.x + wr.w, mid, 1, C_HAIR);
        for (i = 0; i <= 800; i++) { float tt = Tw * i / 800; pts[2 * i] = wr.x + wr.w * i / 800; pts[2 * i + 1] = mid - amp * (sinf(TAU_F * f0 * tt) + sinf(TAU_F * f1 * tt)); }
        for (i = 0; i <= 200; i++) { float tt = Tw * i / 200, e = 2 * fabsf(cosf(PI_F * d * tt)); e1[2 * i] = e2[2 * i] = wr.x + wr.w * i / 200; e1[2 * i + 1] = mid - amp * e; e2[2 * i + 1] = mid + amp * e; }
        d_polyline(pts, 801, S(1.1f), C_INK);
        for (i = 0; i < 200; i += 2) { d_line(e1[2 * i], e1[2 * i + 1], e1[2 * i + 2], e1[2 * i + 3], S(1.6f), MC(MO_SPLIT)); d_line(e2[2 * i], e2[2 * i + 1], e2[2 * i + 2], e2[2 * i + 3], S(1.6f), MC(MO_SPLIT)); }
        d_line(wr.x + wr.w * cur / Tw, wr.y, wr.x + wr.w * cur / Tw, wr.y + wr.h, 1, calpha(C_INK, 0.3f));
        lbl(wr.x, wr.y - S(8), "\\sin\\alpha+\\sin\\beta\\quad\\text{with envelope}\\ \\pm2\\cos\\delta", S(15), C_INK2, 0);
        {   char buf[96]; snprintf(buf, sizeof buf, "\\text{2 seconds:}\\ %d\\ \\text{beats}\\quad(\\nu_1-\\nu_0=%.1f\\ \\text{Hz})", (int)floorf(2 * d + 0.5f), d);
            lbl(wr.x, wr.y + wr.h + S(26), buf, S(15), MC(MO_LOOP), 0); }
        if (st->sound) { synth_hold(0, 440, 0.07f); synth_hold(1, 440 + d, 0.07f); synth_hold(2, 0, 0); synth_hold(3, 0, 0); }
        note(r.x + S(16), r.y + S(10), "the phasors turn slowly here; you hear 440 Hz and 440 + \u0394 Hz", C_MUTED);
    } else note(r.x + S(16), r.y + S(10), "drag the two tips: the sum always points along the middle angle \u03b3", C_MUTED);
}

/* ---------------------------------------------------------------- plane */
typedef struct { float x, y; } Cx;
static Cx cx(float x, float y) { Cx c; c.x = x; c.y = y; return c; }
static Cx cmul(Cx a, Cx b) { return cx(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }
static Cx cconj(Cx a) { return cx(a.x, -a.y); }
static float cabs_(Cx a) { return sqrtf(a.x * a.x + a.y * a.y); }
static Cx cdiv(Cx a, Cx b) { float d = b.x * b.x + b.y * b.y; Cx n = cmul(a, cconj(b)); if (d < 1e-6f) d = 1e-6f; return cx(n.x / d, n.y / d); }

static void sc_plane(Stage *st, Rect r, const Input *in, int interactive) {
    int op = (int)st->p[0];
    Pl p = pl_fit(inset(r, S(22)), -2.6f, 2.6f, -2.0f, 2.0f);
    Cx z, w;
    pl_grid(&p, 0.5f, calpha(C_HAIR, 0.5f));
    pl_axes(&p, "\\Re", "\\Im");
    pl_circle(&p, 0, 0, 1, 1, calpha(C_INK, 0.18f));
    {   int nh = (op == PL_SUM || op == PL_PRODUCT || op == PL_DIVIDE || op == PL_TRIANGLE) ? 2 : (op == PL_ITURN ? 1 : 1), k;
        for (k = 0; k < nh; k++) handle(st, k, &p, in, k == 0 ? C_INK : MC(MO_TURN), interactive);
    }
    z = cx(st->h[0][0], st->h[0][1]); w = cx(st->h[1][0], st->h[1][1]);
    switch (op) {
    case PL_SUM: {
        Cx s = cx(z.x + w.x, z.y + w.y);
        pl_line(&p, z.x, z.y, s.x, s.y, 1, calpha(MC(MO_TURN), 0.5f)); pl_line(&p, w.x, w.y, s.x, s.y, 1, calpha(C_INK, 0.4f));
        pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, w.x, w.y, S(1.8f), MC(MO_TURN)); pl_vec(&p, 0, 0, s.x, s.y, S(2.4f), MC(MO_LANES));
        plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, w.x, w.y, "w", MC(MO_TURN), 8, 14, 0); plbl(&p, s.x, s.y, "z+w", MC(MO_LANES), 8, 0, 0);
        note(r.x + S(16), r.y + S(10), "addition is lane by lane: the parallelogram", C_MUTED);
        break; }
    case PL_PRODUCT: {
        Cx m = cmul(z, w); float tz = atan2f(z.y, z.x), tw = atan2f(w.y, w.x);
        float tri[6], tri2[6];
        tri[0] = X(&p, 0); tri[1] = Y(&p, 0); tri[2] = X(&p, 1); tri[3] = Y(&p, 0); tri[4] = X(&p, z.x); tri[5] = Y(&p, z.y);
        tri2[0] = X(&p, 0); tri2[1] = Y(&p, 0); tri2[2] = X(&p, w.x); tri2[3] = Y(&p, w.y); tri2[4] = X(&p, m.x); tri2[5] = Y(&p, m.y);
        d_poly(tri, 3, calpha(C_INK, 0.07f)); d_poly(tri2, 3, calpha(MC(MO_TURN), 0.1f));
        pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, w.x, w.y, S(1.8f), MC(MO_TURN)); pl_vec(&p, 0, 0, m.x, m.y, S(2.4f), MC(MO_TURN));
        pl_line(&p, 1, 0, z.x, z.y, 1, calpha(C_INK, 0.4f)); pl_line(&p, w.x, w.y, m.x, m.y, 1, calpha(MC(MO_TURN), 0.5f));
        pl_arc(&p, 0, 0, 0.3f, 0, tz, S(1.3f), C_INK);
        pl_arc(&p, 0, 0, 0.42f, 0, tw, S(1.3f), MC(MO_TURN));
        pl_arc(&p, 0, 0, 0.55f, 0, tz + tw, S(1.3f), calpha(MC(MO_TURN), 0.6f));
        plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, w.x, w.y, "w", MC(MO_TURN), 8, 14, 0); plbl(&p, m.x, m.y, "zw", MC(MO_TURN), 8, 0, 0);
        {   char buf[96]; snprintf(buf, sizeof buf, "|zw|=%.2f=%.2f\\cdot%.2f", cabs_(m), cabs_(z), cabs_(w)); lbl(r.x + S(16), r.y + r.h - S(16), buf, S(15), C_INK2, 0); }
        note(r.x + S(16), r.y + S(10), "multiply lengths, add angles: the shaded triangles are similar", C_MUTED);
        break; }
    case PL_CONJ: case PL_REIM: case PL_MODSQ: {
        Cx c = cconj(z);
        pl_line(&p, -2.6f, 0, 2.6f, 0, S(1.6f), calpha(MC(MO_MIRROR), 0.55f));
        pl_dash(&p, z.x, z.y, c.x, c.y, 1, calpha(MC(MO_MIRROR), 0.6f));
        pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, c.x, c.y, S(1.8f), MC(MO_MIRROR));
        plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, c.x, c.y, "\\overline z", MC(MO_MIRROR), 8, 14, 0);
        if (op == PL_CONJ) {
            float a = atan2f(z.y, z.x);
            pl_arc(&p, 0, 0, cabs_(z), -fabsf(a), fabsf(a), 1, calpha(MC(MO_MIRROR), 0.35f));
            note(r.x + S(16), r.y + S(10), "the mirror passes through 0, so the mirror image is just as long", C_MUTED);
        } else if (op == PL_REIM) {
            pl_vec(&p, 0, 0, 2 * z.x, 0, S(2.2f), MC(MO_SPLIT)); pl_dot(&p, z.x, 0, S(4.5f), MC(MO_SPLIT));
            pl_vec(&p, 0, 0, 0, 2 * z.y, S(2.2f), calpha(MC(MO_SPLIT), 0.6f));
            plbl(&p, 2 * z.x, 0, "z+\\overline z", MC(MO_SPLIT), 6, 18, 0);
            plbl(&p, z.x, 0, "\\Re z", MC(MO_SPLIT), 0, -8, 1);
            plbl(&p, 0, 2 * z.y, "z-\\overline z", MC(MO_SPLIT), 8, 4, 0);
            note(r.x + S(16), r.y + S(10), "the middle of z and its mirror image lies on the mirror", C_MUTED);
        } else {
            float a = atan2f(z.y, z.x), m2 = z.x * z.x + z.y * z.y;
            pl_arc(&p, 0, 0, 0.35f, 0, a, S(1.4f), MC(MO_TURN)); pl_arc(&p, 0, 0, 0.35f, -a, 0, S(1.4f), MC(MO_MIRROR));
            pl_vec(&p, 0, 0, m2 > 2.5f ? 2.5f : m2, 0, S(2.6f), MC(MO_SPLIT));
            plbl(&p, m2 > 2.5f ? 2.5f : m2, 0, "z\\overline z=|z|^2", MC(MO_SPLIT), 0, 20, 2);
            note(r.x + S(16), r.y + S(10), "a turn and its mirror image cancel, so z times its mirror image is real", C_MUTED);
        }
        break; }
    case PL_INVERSE: {
        float m2 = z.x * z.x + z.y * z.y; Cx c = cconj(z), iz = cx(c.x / m2, c.y / m2);
        pl_line(&p, -2.6f, 0, 2.6f, 0, S(1.2f), calpha(MC(MO_MIRROR), 0.4f));
        pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, c.x, c.y, S(1.2f), calpha(MC(MO_MIRROR), 0.6f));
        pl_vec(&p, 0, 0, iz.x, iz.y, S(2.4f), MC(MO_TURN));
        plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, c.x, c.y, "\\overline z", MC(MO_MIRROR), 8, 14, 0);
        plbl(&p, iz.x, iz.y, "z^{-1}=\\overline z/|z|^2", MC(MO_TURN), 8, 16, 0);
        note(r.x + S(16), r.y + S(10), "mirror the angle, invert the length", C_MUTED);
        break; }
    case PL_DIVIDE: {
        Cx q = cdiv(z, w), c = cconj(w), zc = cmul(z, c);
        pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, w.x, w.y, S(1.8f), MC(MO_TURN));
        pl_vec(&p, 0, 0, c.x, c.y, S(1.2f), calpha(MC(MO_MIRROR), 0.7f));
        if (cabs_(zc) < 4) pl_vec(&p, 0, 0, zc.x, zc.y, S(1.2f), calpha(MC(MO_MIRROR), 0.45f));
        pl_vec(&p, 0, 0, q.x, q.y, S(2.4f), MC(MO_LANES));
        plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, w.x, w.y, "w", MC(MO_TURN), 8, 0, 0);
        plbl(&p, c.x, c.y, "\\overline w", MC(MO_MIRROR), 8, 14, 0); plbl(&p, q.x, q.y, "z/w", MC(MO_LANES), 8, 14, 0);
        note(r.x + S(16), r.y + S(10), "to divide by w: multiply by its mirror image, then shrink by |w|\u00b2", C_MUTED);
        break; }
    case PL_TRIANGLE: {
        int rev = st->p[1] > 0;
        if (!rev) {
            Cx s = cx(z.x + w.x, z.y + w.y);
            float lz = cabs_(z), lw = cabs_(w), ls = cabs_(s), bx = r.x + r.w - S(200), by = r.y + r.h - S(60), k = S(50);
            pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, z.x, z.y, s.x, s.y, S(1.8f), MC(MO_TURN));
            pl_vec(&p, 0, 0, s.x, s.y, S(2.4f), MC(MO_SHADOW));
            plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, s.x, s.y, "z+w", MC(MO_SHADOW), 8, 0, 0);
            d_rect(rect(bx, by, lz * k, S(6)), C_INK); d_rect(rect(bx + lz * k, by, lw * k, S(6)), MC(MO_TURN));
            d_rect(rect(bx, by + S(16), ls * k, S(6)), MC(MO_SHADOW));
            d_text(F_XS, bx, by - S(16), "|z| + |w|", C_MUTED); d_text(F_XS, bx, by + S(26), "|z + w|", MC(MO_SHADOW));
            note(r.x + S(16), r.y + S(10), "the straight path is never longer than the detour", C_MUTED);
        } else {
            float lz = cabs_(z), lw = cabs_(w);
            pl_circle(&p, 0, 0, lz, 1, calpha(C_INK, 0.25f)); pl_circle(&p, 0, 0, lw, 1, calpha(MC(MO_TURN), 0.35f));
            pl_vec(&p, 0, 0, z.x, z.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, w.x, w.y, S(1.8f), MC(MO_TURN));
            pl_line(&p, z.x, z.y, w.x, w.y, S(2.4f), MC(MO_SHADOW));
            plbl(&p, z.x, z.y, "z", C_INK, 8, 0, 0); plbl(&p, w.x, w.y, "w", MC(MO_TURN), 8, 0, 0);
            plbl(&p, (z.x + w.x) / 2, (z.y + w.y) / 2, "|z-w|", MC(MO_SHADOW), 8, 0, 0);
            note(r.x + S(16), r.y + S(10), "the gap between the circles, ||z| \u2212 |w||, is at most the distance |z \u2212 w|", C_MUTED);
        }
        break; }
    case PL_SHADOW: {
        float L = cabs_(z), ang = atan2f(z.y, z.x), ph = smooth01(0.5f + 0.5f * sinf(st->t * 0.6f));
        Cx rz = cx(L * cosf(ang * (1 - ph)), L * sinf(ang * (1 - ph)));
        pl_vec(&p, 0, 0, rz.x, rz.y, S(2.2f), C_INK);
        pl_dash(&p, rz.x, rz.y, rz.x, 0, 1, MC(MO_SHADOW));
        pl_line(&p, 0, 0, rz.x, 0, S(4), calpha(MC(MO_SHADOW), 0.8f));
        if (ph < 0.95f) pl_arc(&p, 0, 0, L, 0, ang, 1, calpha(MC(MO_TURN), 0.35f));
        plbl(&p, rz.x, rz.y, "w", C_INK, 8, 0, 0);
        plbl(&p, rz.x / 2, 0, "\\Re w", MC(MO_SHADOW), 0, 20, 1);
        {   char buf[80]; snprintf(buf, sizeof buf, "\\Re w=%.2f\\ \\le\\ |w|=%.2f", rz.x, L); lbl(r.x + S(16), r.y + r.h - S(16), buf, S(16), C_INK2, 0); }
        note(r.x + S(16), r.y + S(10), "a shadow is never longer than the vector; turn it onto the axis and they agree", C_MUTED);
        break; }
    case PL_POLAR: {
        float L = cabs_(z), a = atan2f(z.y, z.x);
        pl_dash(&p, z.x, z.y, z.x, 0, 1, MC(MO_LANES)); pl_dash(&p, z.x, z.y, 0, z.y, 1, MC(MO_LANES));
        pl_vec(&p, 0, 0, z.x, z.y, S(2.2f), C_INK);
        pl_vec(&p, 0, 0, cosf(a), sinf(a), S(2), MC(MO_TURN));
        pl_arc(&p, 0, 0, 0.35f, 0, a, S(1.4f), MC(MO_TURN));
        plbl(&p, z.x, z.y, "z=re^{i\\theta}", C_INK, 8, 0, 0);
        plbl(&p, cosf(a), sinf(a), "e^{i\\theta}", MC(MO_TURN), -10, -6, 2);
        plbl(&p, 0.5f * cosf(a / 2), 0.5f * sinf(a / 2), "\\theta", MC(MO_TURN), 4, 4, 0);
        plbl(&p, z.x, 0, "r\\cos\\theta", MC(MO_LANES), 0, 20, 1);
        plbl(&p, 0, z.y, "r\\sin\\theta", MC(MO_LANES), -6, 4, 2);
        {   char buf[64]; snprintf(buf, sizeof buf, "r=%.2f,\\ \\theta=%.2f", L, a); lbl(r.x + S(16), r.y + r.h - S(16), buf, S(15), C_INK2, 0); }
        note(r.x + S(16), r.y + S(10), "length times direction", C_MUTED);
        break; }
    case PL_ITURN: {
        int k; float ph = st->t * 0.5f, q = smooth01(wrapf(ph, 1.0f) * 1.6f), base = floorf(ph);
        /* a flag shape, and its images under multiplication by i^k */
        static const float F[10] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.55f, 0.55f, 0.32f, 1.0f, 0.12f };
        for (k = 0; k < 4; k++) {
            float a = PI_F / 2 * k, ca = cosf(a), sa = sinf(a); Color c = k == 0 ? C_INK : calpha(MC(MO_TURN), 0.35f);
            pl_line(&p, 0, 0, F[2] * ca, F[2] * sa, S(1.6f), c);
            pl_line(&p, F[2] * ca - 0 * sa, F[2] * sa, (F[4] * ca - F[5] * sa), (F[4] * sa + F[5] * ca), S(1.6f), c);
            pl_line(&p, (F[4] * ca - F[5] * sa), (F[4] * sa + F[5] * ca), (F[6] * ca - F[7] * sa), (F[6] * sa + F[7] * ca), S(1.6f), c);
        }
        {   float a = PI_F / 2 * (base + q), ca = cosf(a), sa = sinf(a);
            pl_line(&p, 0, 0, F[2] * ca, F[2] * sa, S(2.6f), MC(MO_TURN));
            pl_line(&p, F[2] * ca, F[2] * sa, (F[4] * ca - F[5] * sa), (F[4] * sa + F[5] * ca), S(2.6f), MC(MO_TURN));
            pl_line(&p, (F[4] * ca - F[5] * sa), (F[4] * sa + F[5] * ca), (F[6] * ca - F[7] * sa), (F[6] * sa + F[7] * ca), S(2.6f), MC(MO_TURN));
        }
        plbl(&p, 1.05f, 0, "1", C_INK, 4, 16, 0); plbl(&p, 0, 1.05f, "i", MC(MO_TURN), 8, 0, 0);
        plbl(&p, -1.05f, 0, "i^2=-1", MC(MO_TURN), -4, 16, 2); plbl(&p, 0, -1.05f, "i^3=-i", MC(MO_TURN), 8, 10, 0);
        {   Cx iz = cx(-z.y, z.x);
            pl_vec(&p, 0, 0, z.x, z.y, S(1.6f), C_INK); pl_vec(&p, 0, 0, iz.x, iz.y, S(1.6f), MC(MO_TURN));
            plbl(&p, z.x, z.y, "(c,d)", C_INK, 8, 0, 0); plbl(&p, iz.x, iz.y, "i(c,d)=(-d,c)", MC(MO_TURN), 8, 0, 0); }
        note(r.x + S(16), r.y + S(10), "multiplying by i is a quarter turn; two quarter turns point backwards", C_MUTED);
        break; }
    }
}

/* ------------------------------------------------------------------- exp */
static void sc_exp(Stage *st, Rect r, const Input *in, int interactive) {
    int mode = (int)st->p[0]; float t = st->t;
    if (mode == 0) {
        Rect a = rect(r.x, r.y, r.w * 0.5f, r.h), b = rect(r.x + r.w * 0.5f, r.y + S(40), r.w * 0.48f, r.h - S(80));
        Pl p = pl_fit(inset(a, S(30)), -1.5f, 1.5f, -1.5f, 1.5f);
        float th = wrapf(t * 0.8f, TAU_F + 2.0f); int i; static float pts[2 * 201];
        if (th > TAU_F) th = TAU_F;
        pl_axes(&p, "\\Re", "\\Im");
        pl_circle(&p, 0, 0, 1, 1, C_HAIR);
        pl_arc(&p, 0, 0, 1, 0, th, S(2.4f), MC(MO_LOOP));
        pl_vec(&p, 0, 0, cosf(th), sinf(th), S(2), C_INK);
        pl_dash(&p, cosf(th), sinf(th), cosf(th), 0, 1, MC(MO_LANES)); pl_dash(&p, cosf(th), sinf(th), 0, sinf(th), 1, MC(MO_LANES));
        plbl(&p, cosf(th), sinf(th), "e^{it}", C_INK, 8, -4, 0);
        /* lanes */
        for (i = 0; i <= 200; i++) { float tt = TAU_F * i / 200; pts[2 * i] = b.x + b.w * i / 200; pts[2 * i + 1] = b.y + b.h * 0.25f - b.h * 0.2f * cosf(tt); }
        d_polyline(pts, 201, S(1.6f), MC(MO_LANES));
        for (i = 0; i <= 200; i++) { float tt = TAU_F * i / 200; pts[2 * i + 1] = b.y + b.h * 0.75f - b.h * 0.2f * sinf(tt); }
        d_polyline(pts, 201, S(1.6f), MC(MO_LANES));
        d_line(b.x + b.w * th / TAU_F, b.y, b.x + b.w * th / TAU_F, b.y + b.h, 1, calpha(C_INK, 0.35f));
        lbl(b.x, b.y - S(6), "\\Re\\,e^{it}=\\cos t", S(15), MC(MO_LANES), 0);
        lbl(b.x, b.y + b.h * 0.5f - S(4), "\\Im\\,e^{it}=\\sin t", S(15), MC(MO_LANES), 0);
        lbl(b.x + b.w, b.y + b.h + S(20), "t=2\\pi", S(14), MC(MO_LOOP), 2);
        note(r.x + S(16), r.y + S(10), "once round the circle every 2\u03c0: the model of every periodic thing", C_MUTED);
    } else if (mode == 1) {
        Pl p = pl_fit(inset(r, S(24)), -2.6f, 2.6f, -2.0f, 2.0f);
        Rect ir = rect(r.x + r.w - S(170), r.y + S(36), S(150), S(150));
        Pl ip = pl_fit(inset(ir, S(8)), -1, 1, -2, 2);
        float la, lb, tt = wrapf(t * 0.5f, 6.0f) - 3.0f; int i; static float pts[2 * 301];
        Cx e, v;
        pl_axes(&p, "\\Re", "\\Im");
        pl_circle(&p, 0, 0, 1, 1, C_HAIR);
        d_rrect(ir, S(6), C_CARD); d_rrect_line(ir, S(6), 1, C_HAIR);
        d_line(ir.x + S(6), Y(&ip, 0), ir.x + ir.w - S(6), Y(&ip, 0), 1, C_HAIR); d_line(X(&ip, 0), ir.y + S(6), X(&ip, 0), ir.y + ir.h - S(6), 1, C_HAIR);
        handle(st, 0, &ip, in, MC(MO_TURN), interactive);
        if (st->h[0][0] < -0.9f) st->h[0][0] = -0.9f; if (st->h[0][0] > 0.9f) st->h[0][0] = 0.9f;
        if (st->h[0][1] < -1.9f) st->h[0][1] = -1.9f; if (st->h[0][1] > 1.9f) st->h[0][1] = 1.9f;
        la = st->h[0][0]; lb = st->h[0][1];
        lbl(ir.x + S(8), ir.y + S(18), "\\lambda", S(15), MC(MO_TURN), 0);
        for (i = 0; i <= 300; i++) { float s = -3 + 6.0f * i / 300, m = expf(la * s); pts[2 * i] = X(&p, m * cosf(lb * s)); pts[2 * i + 1] = Y(&p, m * sinf(lb * s)); }
        clip_push(r); d_polyline(pts, 301, S(1.6f), calpha(C_INK, 0.6f)); clip_pop();
        e = cx(expf(la * tt) * cosf(lb * tt), expf(la * tt) * sinf(lb * tt)); v = cmul(cx(la, lb), e);
        if (cabs_(e) < 3) {
            pl_vec(&p, 0, 0, e.x, e.y, S(1.6f), C_INK);
            pl_vec(&p, e.x, e.y, e.x + 0.5f * v.x, e.y + 0.5f * v.y, S(2.2f), MC(MO_TURN));
            plbl(&p, e.x, e.y, "e^{\\lambda t}", C_INK, -10, -6, 2);
            plbl(&p, e.x + 0.5f * v.x, e.y + 0.5f * v.y, "\\tfrac{d}{dt}e^{\\lambda t}=\\lambda e^{\\lambda t}", MC(MO_TURN), 8, 0, 0);
        }
        {   char buf[64]; snprintf(buf, sizeof buf, "\\lambda=%.2f%+.2fi", la, lb); lbl(r.x + S(16), r.y + r.h - S(16), buf, S(15), C_INK2, 0); }
        note(r.x + S(16), r.y + S(10), "drag \u03bb: velocity is the position turned by arg \u03bb and stretched by |\u03bb|", C_MUTED);
    } else if (mode == 2) {
        Rect a = rect(r.x, r.y + S(20), r.w * 0.42f, r.h - S(30)), b = rect(r.x + r.w * 0.45f, r.y + S(20), r.w * 0.55f, r.h - S(30));
        Pl pa = pl_fit(inset(a, S(20)), -1.3f, 1.3f, -3.4f, 3.4f), pb = pl_fit(inset(b, S(20)), -3.8f, 3.8f, -3.8f, 3.8f);
        int i, k; static float pts[2 * 121];
        pl_axes(&pa, "a", "b"); pl_axes(&pb, "\\Re", "\\Im");
        for (k = -4; k <= 4; k++) {   /* vertical lines a = const -> circles */
            float av = k * 0.3f; Color c = calpha(MC(MO_SHADOW), k == 0 ? 0.9f : 0.45f);
            pl_line(&pa, av, -PI_F, av, PI_F, S(1.2f), c);
            pl_circle(&pb, 0, 0, expf(av), S(1.2f), c);
        }
        for (k = -6; k <= 6; k++) {   /* horizontal lines b = const -> rays */
            float bv = k * PI_F / 6; Color c = calpha(MC(MO_TURN), k == 0 ? 0.9f : 0.45f);
            pl_line(&pa, -1.2f, bv, 1.2f, bv, S(1.2f), c);
            for (i = 0; i <= 120; i++) { float av = -1.2f + 2.4f * i / 120, m = expf(av); pts[2 * i] = X(&pb, m * cosf(bv)); pts[2 * i + 1] = Y(&pb, m * sinf(bv)); }
            d_polyline(pts, 121, S(1.2f), c);
        }
        lbl(a.x + S(10), a.y + S(4), "z=a+ib", S(15), C_INK2, 0);
        lbl(b.x + S(10), b.y + S(4), "e^z=e^a\\,e^{ib}", S(15), C_INK2, 0);
        note(r.x + S(16), r.y + r.h - S(24), "the real part stretches (circles), the imaginary part turns (rays)", C_MUTED);
    } else {
        Pl p = pl_fit(inset(r, S(24)), -1.6f, 1.6f, -1.4f, 1.6f);
        float b, d; Cx e1, e2, m;
        pl_axes(&p, "\\Re", "\\Im");
        pl_circle(&p, 0, 0, 1, 1, C_HAIR);
        handle(st, 0, &p, in, C_INK, interactive); handle(st, 1, &p, in, MC(MO_TURN), interactive);
        b = atan2f(st->h[0][1], st->h[0][0]); d = atan2f(st->h[1][1], st->h[1][0]);
        st->h[0][0] = cosf(b); st->h[0][1] = sinf(b); st->h[1][0] = cosf(d); st->h[1][1] = sinf(d);
        e1 = cx(cosf(b), sinf(b)); e2 = cx(cosf(d), sinf(d)); m = cmul(e1, e2);
        pl_vec(&p, 0, 0, e1.x, e1.y, S(1.8f), C_INK); pl_vec(&p, 0, 0, e2.x, e2.y, S(1.8f), MC(MO_TURN)); pl_vec(&p, 0, 0, m.x, m.y, S(2.6f), MC(MO_TURN));
        pl_arc(&p, 0, 0, 0.25f, 0, b, S(1.4f), C_INK); pl_arc(&p, 0, 0, 0.38f, 0, d, S(1.4f), MC(MO_TURN)); pl_arc(&p, 0, 0, 0.52f, 0, b + d, S(1.4f), calpha(MC(MO_TURN), 0.6f));
        pl_dash(&p, m.x, m.y, m.x, 0, 1, MC(MO_LANES)); pl_dash(&p, m.x, m.y, 0, m.y, 1, MC(MO_LANES));
        plbl(&p, e1.x, e1.y, "e^{ib}", C_INK, 8, 0, 0); plbl(&p, e2.x, e2.y, "e^{id}", MC(MO_TURN), 8, 0, 0);
        plbl(&p, m.x, m.y, "e^{ib}e^{id}=e^{i(b+d)}", MC(MO_TURN), 8, -4, 0);
        plbl(&p, m.x, 0, "\\cos(b+d)", MC(MO_LANES), 0, 20, 1);
        plbl(&p, 0, m.y, "\\sin(b+d)", MC(MO_LANES), -6, 4, 2);
        note(r.x + S(16), r.y + S(10), "drag the two turns: angles add, and the lanes are the sum-angle formulas", C_MUTED);
    }
}

/* ---------------------------------------------------------------- winding */
static void sc_winding(Stage *st, Rect r, const Input *in, int interactive) {
    int mode = (int)st->p[0], i;
    if (mode == 0) {
        int k = (int)floorf(st->slider + 0.5f), N = 48;
        Rect a = rect(r.x, r.y + S(20), r.w * 0.5f, r.h - S(30)), b = rect(r.x + r.w * 0.5f, r.y + S(20), r.w * 0.5f, r.h - S(30));
        Pl pa = pl_fit(inset(a, S(26)), -1.4f, 1.4f, -1.4f, 1.4f), pb = pl_fit(inset(b, S(26)), -0.7f, 1.2f, -0.95f, 0.95f);
        float t = st->t, xm = wrapf(t * 0.15f, 1.0f); float cxs = 0, cys = 0;
        pl_axes(&pa, NULL, NULL); pl_circle(&pa, 0, 0, 1, 1, C_HAIR);
        for (i = 0; i < N; i++) {
            float x = (i + 0.5f) / N, a2 = TAU_F * k * x;
            pl_dot(&pa, cosf(a2), sinf(a2), S(2.6f), calpha(MC(MO_LOOP), 0.7f));
        }
        pl_vec(&pa, 0, 0, cosf(TAU_F * k * xm), sinf(TAU_F * k * xm), S(2), C_INK);
        pl_dot(&pa, 0, 0, S(4), MC(MO_LOOP));
        lbl(a.x + S(10), a.y + S(4), "e^{2\\pi i(m-n)x},\\ x\\in[0,1]", S(15), C_INK2, 0);
        /* the integral as a chain of little steps */
        pl_axes(&pb, NULL, NULL);
        {   float px = 0, py = 0; static float pts[2 * 49];
            pts[0] = X(&pb, 0); pts[1] = Y(&pb, 0);
            for (i = 0; i < N; i++) { float x = (i + 0.5f) / N, a2 = TAU_F * k * x; px += cosf(a2) / N * (k ? 3.0f : 1.0f); py += sinf(a2) / N * (k ? 3.0f : 1.0f); pts[2 * i + 2] = X(&pb, px); pts[2 * i + 3] = Y(&pb, py); }
            d_polyline(pts, N + 1, S(1.8f), MC(MO_LOOP));
            cxs = px; cys = py;
        }
        pl_dot(&pb, cxs, cys, S(4.5f), C_INK);
        lbl(b.x + S(10), b.y + S(4), k ? "\\text{steps } e^{2\\pi i(m-n)x}\\,dx\\ \\text{(enlarged)}" : "\\text{steps } 1\\cdot dx", S(15), C_INK2, 0);
        lbl(b.x + S(10), b.y + b.h - S(4), k ? "\\int_0^1e^{2\\pi i(m-n)x}dx=0" : "\\int_0^1 1\\,dx=1", S(17), k ? MC(MO_LOOP) : C_INK, 0);
        note(r.x + S(16), r.y + S(4), "whole turns close up: the steps of the integral return to where they began", C_MUTED);
    } else if (mode == 1) {
        Pl p = pl_fit(inset(r, S(24)), -1.2f, 3.4f, -1.6f, 2.2f);
        int n = (int)floorf(st->slider + 0.5f); Cx z, pw = cx(1, 0), s = cx(0, 0), lim;
        static float pts[2 * 32];
        pl_axes(&p, "\\Re", "\\Im"); pl_circle(&p, 0, 0, 1, 1, C_HAIR);
        handle(st, 0, &p, in, MC(MO_TURN), interactive);
        z = cx(st->h[0][0], st->h[0][1]);
        pts[0] = X(&p, 0); pts[1] = Y(&p, 0);
        for (i = 0; i <= n && i < 31; i++) { s.x += pw.x; s.y += pw.y; pts[2 * i + 2] = X(&p, s.x); pts[2 * i + 3] = Y(&p, s.y); pw = cmul(pw, z); }
        d_polyline(pts, (n < 31 ? n : 30) + 2, S(1.8f), MC(MO_TURN));
        for (i = 1; i <= n + 1 && i < 32; i++) d_circle(pts[2 * i], pts[2 * i + 1], S(2.4f), MC(MO_TURN));
        pl_vec(&p, 0, 0, z.x, z.y, S(1.4f), calpha(C_INK, 0.6f)); plbl(&p, z.x, z.y, "z", C_INK, 8, 14, 0);
        if (cabs_(z) < 1) { lim = cdiv(cx(1, 0), cx(1 - z.x, -z.y)); pl_circle(&p, lim.x, lim.y, 0.05f, 1.5f, MC(MO_SPLIT)); plbl(&p, lim.x, lim.y, "\\tfrac{1}{1-z}", MC(MO_SPLIT), 10, 0, 0); }
        lbl(r.x + S(16), r.y + r.h - S(16), "1+z+\\dots+z^n=\\frac{1-z^{n+1}}{1-z}", S(16), C_INK2, 0);
        note(r.x + S(16), r.y + S(10), "each step is the last one turned by arg z and scaled by |z|", C_MUTED);
    } else {
        Pl p = pl_fit(inset(r, S(24)), -3.0f, 5.0f, -1.0f, 5.0f);
        int N = st->p[1] > 0 ? (int)st->p[1] : 7; float t = st->slider, px = 0, py = 0;
        static float pts[2 * 40]; float g = PI_F * N * t, chord = sinf(PI_F * (N + 1) * t) / sinf(PI_F * t);
        pl_axes(&p, "\\Re", "\\Im");
        pts[0] = X(&p, 0); pts[1] = Y(&p, 0);
        for (i = 0; i <= N && i < 38; i++) { float a = TAU_F * i * t; px += cosf(a); py += sinf(a); pts[2 * i + 2] = X(&p, px); pts[2 * i + 3] = Y(&p, py); }
        d_polyline(pts, N + 2, S(1.8f), MC(MO_TURN));
        for (i = 1; i <= N + 1; i++) d_circle(pts[2 * i], pts[2 * i + 1], S(2.4f), MC(MO_TURN));
        pl_dash(&p, 0, 0, 6 * cosf(g), 6 * sinf(g), S(1.2f), MC(MO_SPLIT));
        pl_vec(&p, 0, 0, px, py, S(2.4f), C_INK);
        plbl(&p, px, py, "\\sum_{k=0}^{N}e^{2\\pi ikt}", C_INK, 10, 0, 0);
        {   char buf[160]; snprintf(buf, sizeof buf, "\\text{length}\\ \\frac{\\sin(\\pi(N+1)t)}{\\sin(\\pi t)}=%.2f,\\quad \\text{direction}\\ \\pi Nt", chord);
            lbl(r.x + S(16), r.y + r.h - S(16), buf, S(15), C_INK2, 0); }
        note(r.x + S(16), r.y + S(10), "unit steps bend into an arc; the chord points along the middle angle \u03c0Nt", C_MUTED);
    }
}

/* -------------------------------------------------------------- polarize */
static Cx pol_f(float t) { float m = 1 + 0.35f * cosf(3 * t); float a = 1.2f * t + 0.6f * sinf(2 * t) + 0.3f; return cx(m * cosf(a), m * sinf(a)); }
static void sc_polarize(Stage *st, Rect r) {
    int phase = (int)st->p[0], N = 22, i;
    Pl p = pl_fit(inset(r, S(28)), -0.6f, 2.6f, -1.0f, 2.2f);
    float T = 2.0f, dt = T / N, px = 0, py = 0, total = 0, th, rot;
    static float pts[2 * 23]; static float ang;
    Cx I = cx(0, 0);
    for (i = 0; i < N; i++) { Cx f = pol_f((i + 0.5f) * dt); I.x += f.x * dt; I.y += f.y * dt; total += cabs_(f) * dt; }
    th = atan2f(I.y, I.x);
    rot = phase == 0 ? 0 : -th;
    ang = approachf(ang, rot, 0.06f);
    pl_axes(&p, "\\Re", "\\Im");
    pts[0] = X(&p, 0); pts[1] = Y(&p, 0);
    for (i = 0; i < N; i++) {
        Cx f = pol_f((i + 0.5f) * dt), s = cmul(cx(f.x * dt, f.y * dt), cx(cosf(ang), sinf(ang)));
        float qx = px + s.x, qy = py + s.y;
        if (phase == 2) {
            pl_line(&p, px, -0.35f, qx, -0.35f, S(4), calpha(MC(MO_SHADOW), i % 2 ? 0.55f : 0.85f));
            pl_dash(&p, qx, qy, qx, -0.35f, 1, calpha(MC(MO_SHADOW), 0.25f));
        }
        px = qx; py = qy; pts[2 * i + 2] = X(&p, px); pts[2 * i + 3] = Y(&p, py);
    }
    d_polyline(pts, N + 1, S(2), C_INK);
    for (i = 1; i <= N; i++) d_circle(pts[2 * i], pts[2 * i + 1], S(2), C_INK);
    pl_vec(&p, 0, 0, px, py, S(2.4f), MC(MO_TURN));
    plbl(&p, px, py, phase == 0 ? "\\int f=re^{i\\theta}" : "e^{-i\\theta}\\int f=r", MC(MO_TURN), 8, 0, 0);
    if (phase >= 1) pl_arc(&p, 0, 0, 0.5f, ang, 0, S(1.4f), MC(MO_TURN));
    {   float bx = r.x + S(18), by = r.y + r.h - S(52), k = S(90);
        d_rect(rect(bx, by, cabs_(I) * k, S(6)), MC(MO_TURN));
        d_rect(rect(bx, by + S(20), total * k, S(6)), C_INK);
        d_text(F_XS, bx, by - S(15), "|\u222b f| = r", MC(MO_TURN));
        d_text(F_XS, bx, by + S(29), "\u222b |f|  (length of the path)", C_INK);
    }
    note(r.x + S(16), r.y + S(10), phase == 0 ? "the integral is a path of small steps f(t) dt" :
         phase == 1 ? "turn the whole path by e^(\u2212i\u03b8): the endpoint lands on the real axis" :
                      "now the endpoint is a sum of shadows, and each shadow is shorter than its step", C_MUTED);
}

/* --------------------------------------------------------------- riemann */
static float rg(float x, const float *par) { (void)par; return 0.55f + 0.35f * sinf(2.2f * x) + 0.22f * tanhf(8 * (x - 1.6f)) + 0.08f * sinf(7 * x); }
static void sc_riemann(Stage *st, Rect r) {
    int mode = (int)st->p[0], i;
    Pl p = pl_fit(rect(r.x + S(20), r.y + S(36), r.w - S(40), r.h - S(56)), -0.2f, 3.3f, -0.3f, 1.4f);
    float a = 0, b = 3.0f;
    if (mode == 0 || mode == 1) {
        int n = mode == 0 ? (int)floorf(st->slider + 0.5f) : 12; float delta = mode == 1 ? st->slider : 0, U = 0, L = 0, bad = 0;
        pl_axes(&p, "x", NULL);
        for (i = 0; i < n; i++) {
            float x0 = a + (b - a) * i / n, x1 = a + (b - a) * (i + 1) / n, M = -1e9f, m = 1e9f; int k;
            for (k = 0; k <= 30; k++) { float v = rg(x0 + (x1 - x0) * k / 30, NULL); if (v > M) M = v; if (v < m) m = v; }
            U += M * (x1 - x0); L += m * (x1 - x0);
            if (mode == 0) {
                d_rect(rect(X(&p, x0), Y(&p, M), (x1 - x0) * p.s, (M - m) * p.s), calpha(MC(MO_SPLIT), 0.3f));
                d_rect(rect(X(&p, x0), Y(&p, m), (x1 - x0) * p.s, m * p.s), calpha(MC(MO_TURN), 0.12f));
            } else {
                int isbad = M - m >= delta;
                d_rect(rect(X(&p, x0), Y(&p, M), (x1 - x0) * p.s, (M - m) * p.s), calpha(isbad ? MC(MO_MIRROR) : MC(MO_LOOP), 0.3f));
                if (isbad) { bad += x1 - x0; d_rect(rect(X(&p, x0), Y(&p, 0) + S(4), (x1 - x0) * p.s, S(5)), MC(MO_MIRROR)); }
            }
            d_line(X(&p, x0), Y(&p, 0), X(&p, x0), Y(&p, 1.3f), 1, calpha(C_HAIR, 0.8f));
        }
        pl_curve(&p, rg, NULL, a, b, 300, S(2.2f), C_INK, 0);
        {   char buf[128];
            if (mode == 0) snprintf(buf, sizeof buf, "U-L=%.3f", U - L);
            else snprintf(buf, sizeof buf, "\\text{bad length}=%.2f\\ \\le\\ \\frac{U-L}{\\delta}=%.2f", bad, (U - L) / delta);
            lbl(r.x + S(16), r.y + S(30), buf, S(16), C_INK2, 0); }
        note(r.x + S(16), r.y + S(8), mode == 0 ? "more pieces, smaller gap between upper and lower sums" : "intervals where g fluctuates by \u2265 \u03b4 (red) are few: their total length is at most \u03b7/\u03b4", C_MUTED);
    } else if (mode == 2) {
        float M = 0; int k;
        pl_axes(&p, "t", "|f|");
        for (k = 0; k <= 300; k++) { float v = rg(a + (b - a) * k / 300, NULL); if (v > M) M = v; }
        d_rect(rect(X(&p, a), Y(&p, M), (b - a) * p.s, M * p.s), calpha(MC(MO_SHADOW), 0.12f));
        d_rect_line(rect(X(&p, a), Y(&p, M), (b - a) * p.s, M * p.s), 1, MC(MO_SHADOW));
        {   static float pts[2 * 303]; pts[0] = X(&p, a); pts[1] = Y(&p, 0);
            for (k = 0; k <= 300; k++) { float x = a + (b - a) * k / 300; pts[2 * k + 2] = X(&p, x); pts[2 * k + 3] = Y(&p, rg(x, NULL)); }
            for (k = 0; k < 300; k++) d_quad_colors(pts[2 * k + 2], pts[2 * k + 3], pts[2 * k + 4], Y(&p, 0), calpha(C_INK, 0.12f), calpha(C_INK, 0.12f), calpha(C_INK, 0.12f), calpha(C_INK, 0.12f));
            d_polyline(pts + 2, 301, S(2.2f), C_INK); }
        plbl(&p, b, M, "M=\\|f\\|_\\infty", MC(MO_SHADOW), 6, 4, 0);
        plbl(&p, (a + b) / 2, 0, "L=|S|", MC(MO_SHADOW), 0, 22, 1);
        note(r.x + S(16), r.y + S(8), "the area under |f| fits in the box of height M and width L", C_MUTED);
    } else if (mode == 3) {
        Pl q = pl_fit(rect(r.x + S(30), r.y + S(40), r.w - S(60), r.h - S(70)), -0.4f, 2.6f, -0.4f, 1.8f);
        static float pts[2 * 121]; float umin = 9, umax = -9, vmin = 9, vmax = -9, d = 0; int k, j;
        float t0 = 0.4f + 0.3f * sinf(st->t * 0.3f);
        Cx fs[121];
        pl_axes(&q, "\\Re", "\\Im");
        for (k = 0; k <= 120; k++) { float s = t0 + 1.4f * k / 120; fs[k] = cx(1 + 0.8f * cosf(2.4f * s) + 0.2f * s, 0.8f + 0.6f * sinf(3.1f * s)); pts[2 * k] = X(&q, fs[k].x); pts[2 * k + 1] = Y(&q, fs[k].y);
            if (fs[k].x < umin) umin = fs[k].x; if (fs[k].x > umax) umax = fs[k].x; if (fs[k].y < vmin) vmin = fs[k].y; if (fs[k].y > vmax) vmax = fs[k].y; }
        for (k = 0; k <= 120; k += 4) for (j = k; j <= 120; j += 4) { float dd = cabs_(cx(fs[k].x - fs[j].x, fs[k].y - fs[j].y)); if (dd > d) d = dd; }
        d_polyline(pts, 121, S(2.2f), C_INK);
        pl_line(&q, umin, -0.25f, umax, -0.25f, S(4), MC(MO_LANES));
        pl_line(&q, -0.25f, vmin, -0.25f, vmax, S(4), MC(MO_LANES));
        {   char buf[128]; snprintf(buf, sizeof buf, "\\omega(u)=%.2f\\le\\omega(f)\\approx%.2f\\le\\omega(u)+\\omega(v)=%.2f", umax - umin, d, umax - umin + vmax - vmin);
            lbl(r.x + S(16), r.y + S(30), buf, S(15), C_INK2, 0); }
        note(r.x + S(16), r.y + S(8), "f on a small interval, and its shadows in the two lanes", C_MUTED);
    } else {
        int k;
        pl_axes(&p, "t", NULL);
        {   float br[4] = { 0, 0.9f, 1.9f, 3.0f }; int j;
            for (j = 0; j < 3; j++) {
                static float pts[2 * 101];
                for (k = 0; k <= 100; k++) { float x = br[j] + (br[j + 1] - br[j]) * k / 100, v = 0.4f + 0.25f * j + 0.2f * sinf(3 * x + j); pts[2 * k] = X(&p, x); pts[2 * k + 1] = Y(&p, v);
                    if (k < 100) d_quad_colors(pts[2 * k], pts[2 * k + 1], X(&p, br[j] + (br[j + 1] - br[j]) * (k + 1) / 100), Y(&p, 0), calpha(j % 2 ? MC(MO_LANES) : MC(MO_SHADOW), 0.12f), calpha(j % 2 ? MC(MO_LANES) : MC(MO_SHADOW), 0.12f), calpha(j % 2 ? MC(MO_LANES) : MC(MO_SHADOW), 0.12f), calpha(j % 2 ? MC(MO_LANES) : MC(MO_SHADOW), 0.12f)); }
                d_polyline(pts, 101, S(2.2f), C_INK);
                d_ring(pts[0], pts[1], S(3.5f), S(1.4f), C_INK); d_ring(pts[200], pts[201], S(3.5f), S(1.4f), C_INK);
            }
        }
        note(r.x + S(16), r.y + S(8), "finitely many jumps with one-sided limits: integrate piece by piece", C_MUTED);
    }
}

/* ------------------------------------------------------------ separation */
static void sc_separation(Stage *st, Rect r) {
    Pl p = pl_fit(rect(r.x + S(30), r.y + S(50), r.w - S(60), r.h - S(80)), -0.05f, 1.1f, -0.7f, 0.7f);
    float kk = st->slider, k = kk * PI_F * PI_F, end; int i; static float pts[2 * 301];
    float scale = 0;
    for (i = 0; i <= 300; i++) {
        float x = (float)i / 300, y;
        if (k > 1e-4f) y = sinf(sqrtf(k) * x) / sqrtf(k);
        else if (k < -1e-4f) y = sinhf(sqrtf(-k) * x) / sqrtf(-k);
        else y = x;
        if (fabsf(y) > scale) scale = fabsf(y);
        pts[2 * i] = X(&p, x); pts[2 * i + 1] = y;
    }
    if (scale < 1e-4f) scale = 1;
    for (i = 0; i <= 300; i++) pts[2 * i + 1] = Y(&p, 0.55f * pts[2 * i + 1] / scale);
    end = (pts[601] - Y(&p, 0)) / p.s;
    pl_line(&p, 0, 0, 1, 0, 1, C_HAIR);
    {   int hit = fabsf(end) < 0.02f && kk > 0.5f;
        d_polyline(pts, 301, S(2.4f), hit ? MC(MO_LOOP) : C_INK);
        nail(&p, 0); nail(&p, 1);
        if (hit) { char buf[128]; snprintf(buf, sizeof buf, "k=%d^2\\left(\\tfrac{\\pi}{\\ell}\\right)^2:\\ \\text{lands on the nail}", (int)floorf(sqrtf(kk) + 0.5f)); lbl(r.x + S(16), r.y + S(38), buf, S(16), MC(MO_LOOP), 0); }
        else lbl(r.x + S(16), r.y + S(38), kk < 0 ? "k<0:\\ \\text{grows, never returns}" : kk < 0.01f && kk > -0.01f ? "k=0:\\ \\text{a straight line}" : "f(\\ell)\\ne0:\\ \\text{misses the nail}", S(16), C_INK2, 0);
    }
    plbl(&p, 0, 0, "0", C_MUTED, 0, 22, 1); plbl(&p, 1, 0, "\\ell", C_MUTED, 0, 22, 1);
    note(r.x + S(16), r.y + S(8), "f\u2033 = \u2212k f with f(0) = 0: slide k until the curve lands on the far nail", C_MUTED);
}

/* ----------------------------------------------------------------- lanes */
static Cx lane_f(float t) { return cx(0.9f * cosf(t) + 0.35f * cosf(2.3f * t), 0.7f * sinf(t * 1.3f) + 0.25f * sinf(3 * t)); }
static void sc_lanes(Stage *st, Rect r) {
    int mode = (int)st->p[0], i; float t = st->t;
    if (mode == 1) {
        Pl p = pl_fit(inset(r, S(30)), -0.6f, 2.6f, -0.6f, 2.0f);
        Cx z = cx(1.3f, 0.9f); int n, N = 1 + (int)wrapf(t * 3, 40);
        float eps = 0.18f;
        pl_axes(&p, "\\Re", "\\Im");
        pl_circle(&p, z.x, z.y, eps, S(1.2f), MC(MO_SHADOW));
        pl_line(&p, z.x - eps, -0.45f, z.x + eps, -0.45f, S(4), calpha(MC(MO_LANES), 0.4f));
        pl_line(&p, -0.45f, z.y - eps, -0.45f, z.y + eps, S(4), calpha(MC(MO_LANES), 0.4f));
        for (n = 1; n <= N; n++) {
            float rr = 1.2f / n, a = n * 1.1f; Cx zn = cx(z.x + rr * cosf(a), z.y + rr * sinf(a));
            float al = n == N ? 1 : 0.35f;
            pl_dot(&p, zn.x, zn.y, S(n == N ? 4 : 2.5f), calpha(C_INK, al));
            pl_dot(&p, zn.x, -0.45f, S(n == N ? 3.5f : 2), calpha(MC(MO_LANES), al));
            pl_dot(&p, -0.45f, zn.y, S(n == N ? 3.5f : 2), calpha(MC(MO_LANES), al));
            if (n == N) { pl_dash(&p, zn.x, zn.y, zn.x, -0.45f, 1, calpha(MC(MO_LANES), 0.4f)); pl_dash(&p, zn.x, zn.y, -0.45f, zn.y, 1, calpha(MC(MO_LANES), 0.4f)); }
        }
        pl_dot(&p, z.x, z.y, S(3), MC(MO_SHADOW)); plbl(&p, z.x, z.y, "z", MC(MO_SHADOW), 8, -6, 0);
        note(r.x + S(16), r.y + S(10), "z\u2099 \u2192 z exactly when both shadows converge", C_MUTED);
        return;
    }
    {
        Rect a = rect(r.x, r.y + S(20), r.w * 0.5f, r.h - S(20)), b = rect(r.x + r.w * 0.52f, r.y + S(40), r.w * 0.45f, r.h - S(70));
        Pl p = pl_fit(inset(a, S(24)), -1.5f, 1.5f, -1.3f, 1.3f);
        float tt = wrapf(t * 0.5f, TAU_F); Cx f = lane_f(tt), fp; static float pts[2 * 201];
        float h = b.h / 2 - S(14);
        pl_axes(&p, "u", "v");
        for (i = 0; i <= 200; i++) { Cx q = lane_f(TAU_F * i / 200); pts[2 * i] = X(&p, q.x); pts[2 * i + 1] = Y(&p, q.y); }
        d_polyline(pts, 201, S(1.6f), calpha(C_INK, 0.5f));
        pl_dot(&p, f.x, f.y, S(4.5f), C_INK);
        pl_dash(&p, f.x, f.y, f.x, 0, 1, MC(MO_LANES)); pl_dash(&p, f.x, f.y, 0, f.y, 1, MC(MO_LANES));
        pl_dot(&p, f.x, 0, S(3.5f), MC(MO_LANES)); pl_dot(&p, 0, f.y, S(3.5f), MC(MO_LANES));
        plbl(&p, f.x, f.y, "f(t)", C_INK, 8, -4, 0);
        if (mode == 2) {
            Cx f2 = lane_f(tt + 0.01f); fp = cx((f2.x - f.x) / 0.01f * 0.35f, (f2.y - f.y) / 0.01f * 0.35f);
            pl_vec(&p, f.x, f.y, f.x + fp.x, f.y + fp.y, S(2.2f), MC(MO_TURN));
            pl_vec(&p, f.x, f.y, f.x + fp.x, f.y, S(1.4f), MC(MO_LANES));
            pl_vec(&p, f.x + fp.x, f.y, f.x + fp.x, f.y + fp.y, S(1.4f), MC(MO_LANES));
            plbl(&p, f.x + fp.x, f.y + fp.y, "f'=u'+iv'", MC(MO_TURN), 8, 0, 0);
        }
        /* the two lanes as graphs */
        {
            int lane;
            for (lane = 0; lane < 2; lane++) {
                float mid = b.y + (lane == 0 ? h : 2 * h + S(28)) - h / 2 + S(4);
                d_line(b.x, mid, b.x + b.w, mid, 1, C_HAIR);
                if (mode == 3) {
                    for (i = 0; i < 200; i++) { Cx q0 = lane_f(TAU_F * i / 200), q1 = lane_f(TAU_F * (i + 1) / 200); float v0 = lane ? q0.y : q0.x, v1 = lane ? q1.y : q1.x;
                        d_quad_colors(b.x + b.w * i / 200, mid - v0 * h * 0.45f, b.x + b.w * (i + 1) / 200, mid, calpha(MC(MO_LANES), 0.15f), calpha(MC(MO_LANES), 0.15f), calpha(MC(MO_LANES), 0.15f), calpha(MC(MO_LANES), 0.15f)); (void)v1; }
                }
                for (i = 0; i <= 200; i++) { Cx q = lane_f(TAU_F * i / 200); pts[2 * i] = b.x + b.w * i / 200; pts[2 * i + 1] = mid - (lane ? q.y : q.x) * h * 0.45f; }
                d_polyline(pts, 201, S(1.8f), MC(MO_LANES));
                d_line(b.x + b.w * tt / TAU_F, mid - h / 2, b.x + b.w * tt / TAU_F, mid + h / 2, 1, calpha(C_INK, 0.35f));
                lbl(b.x, mid - h / 2 - S(4), lane ? "v(t)=\\Im f(t)" : "u(t)=\\Re f(t)", S(15), MC(MO_LANES), 0);
            }
        }
        note(r.x + S(16), r.y + S(4), mode == 2 ? "differentiate each lane; the arrows add up to f\u2032" : mode == 3 ? "integrate each lane; the two areas are the coordinates of \u222b f" : "one point in the plane, two numbers in two lanes", C_MUTED);
    }
}

/* ---------------------------------------------------------------- etudes */
static void sc_split(Stage *st, Rect r, const Input *in, int interactive) {
    Rect top = rect(r.x, r.y + S(30), r.w, r.h * 0.3f), bot = rect(r.x, r.y + r.h * 0.36f, r.w, r.h * 0.62f);
    Pl p = pl_fit(inset(top, S(20)), -2.4f, 2.4f, -0.5f, 0.6f);
    float a, b, m, h;
    pl_line(&p, -2.4f, 0, 2.4f, 0, 1, calpha(C_INK, 0.5f));
    handle(st, 0, &p, in, C_INK, interactive); handle(st, 1, &p, in, MC(MO_TURN), interactive);
    st->h[0][1] = 0; st->h[1][1] = 0;
    a = st->h[0][0]; b = st->h[1][0]; m = (a + b) / 2; h = (a - b) / 2;
    pl_dot(&p, m, 0, S(5), MC(MO_SPLIT));
    pl_vec(&p, m, 0.25f, a, 0.25f, S(1.6f), MC(MO_SPLIT)); pl_vec(&p, m, 0.25f, b, 0.25f, S(1.6f), MC(MO_SPLIT));
    plbl(&p, a, 0, "a", C_INK, 0, 22, 1); plbl(&p, b, 0, "b", MC(MO_TURN), 0, 22, 1);
    plbl(&p, m, 0, "m=\\tfrac{a+b}{2}", MC(MO_SPLIT), 0, -14, 1);
    plbl(&p, (m + a) / 2, 0.25f, "h", MC(MO_SPLIT), 0, -6, 1);
    {   Pl q = pl_fit(inset(bot, S(20)), -2.4f, 2.4f, -2.4f, 2.4f);
        float L = 3.4f;
        pl_axes(&q, "a", "b");
        pl_line(&q, -L, -L, L, L, S(1.2f), calpha(MC(MO_SPLIT), 0.7f)); pl_line(&q, -L, L, L, -L, S(1.2f), calpha(MC(MO_SPLIT), 0.7f));
        pl_dash(&q, a, b, m, m, 1, MC(MO_SPLIT)); pl_dash(&q, a, b, h, -h, 1, MC(MO_SPLIT));
        pl_dot(&q, a, b, S(5), C_INK);
        plbl(&q, a, b, "(a,b)", C_INK, 8, 0, 0);
        plbl(&q, 2.0f, 2.0f, "\\text{middle axis}", MC(MO_SPLIT), 6, 0, 0);
        plbl(&q, 2.0f, -2.0f, "\\text{half-gap axis}", MC(MO_SPLIT), 6, 14, 0);
        lbl(r.x + S(16), r.y + r.h - S(14), "a=m+h,\\quad b=m-h\\qquad (u,v)\\ \\text{in Ch. 1},\\ (\\gamma,\\delta)\\ \\text{in Ch. 2}", S(15), C_INK2, 0);
    }
    note(r.x + S(16), r.y + S(8), "drag a and b; the pair is the same point read on turned axes", C_MUTED);
}

static void sc_ground(Stage *st, Rect r) {
    Pl p = pl_fit(inset(r, S(40)), -0.05f, 1.05f, -0.6f, 0.6f);
    static float pts[2 * 201]; int i; float t = st->t;
    for (i = 0; i <= 200; i++) { float x = i / 200.0f; pts[2 * i] = X(&p, x); pts[2 * i + 1] = Y(&p, 0.32f * sinf(PI_F * x) * cosf(t * 1.2f)); }
    d_polyline(pts, 201, S(2.6f), MOTIF_COL[MO_GROUND]);
    nail(&p, 0); nail(&p, 1);
    lbl(r.x + r.w / 2, Y(&p, -0.45f), "\\nu_1=\\frac{c}{2\\ell}", S(18), MOTIF_COL[MO_GROUND], 1);
    note(r.x + S(16), r.y + S(10), "the fundamental: the drone the whole score stands on", C_MUTED);
}

/* ================================================================= stage */
int stage_draw(Stage *st, Rect r, const Input *in, float dt, int interactive) {
    Rect sr = r;
    g_over = 0;
    st->t += dt;
    if (st->has_slider) sr.h -= S(44);
    clip_push(r);
    switch (st->scene) {
    case SC_STRING: sc_string(st, sr); break;
    case SC_SPACETIME: sc_spacetime(st, sr, in, interactive); break;
    case SC_HOLD: { float sv = st->p[0]; st->p[0] = 2; sc_spacetime(st, sr, in, interactive); st->p[0] = sv; break; }
    case SC_STANDING: sc_standing(st, sr); break;
    case SC_PERIODS: sc_periods(st, sr); break;
    case SC_PHASORS: sc_phasors(st, sr, in, interactive); break;
    case SC_PLANE: sc_plane(st, sr, in, interactive); break;
    case SC_EXP: sc_exp(st, sr, in, interactive); break;
    case SC_WINDING: sc_winding(st, sr, in, interactive); break;
    case SC_POLARIZE: sc_polarize(st, sr); break;
    case SC_RIEMANN: sc_riemann(st, sr); break;
    case SC_SEPARATION: sc_separation(st, sr); break;
    case SC_LANES: sc_lanes(st, sr); break;
    case SC_SPLIT: sc_split(st, sr, in, interactive); break;
    case SC_GROUND: sc_ground(st, sr); break;
    default: break;
    }
    if (st->has_slider) {
        Rect s = rect(r.x, r.y + r.h - S(44), r.w, S(44));
        d_line(s.x + S(12), s.y, s.x + s.w - S(12), s.y, 1, C_HAIR);
        slider(st, s, in, interactive);
    }
    clip_pop();
    if (st->scene != SC_PHASORS && st->scene != SC_STANDING && st->sound) synth_release_all();
    if (!st->sound) synth_release_all();
    return g_over;
}

const char *stage_caption(const Stage *st) { (void)st; return NULL; }
