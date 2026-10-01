#include "draw.h"
#include "gl_inc.h"
#include <string.h>

float g_s = 1;
static int g_wh;

Color rgb(unsigned h) { Color c; c.r = ((h >> 16) & 255) / 255.f; c.g = ((h >> 8) & 255) / 255.f; c.b = (h & 255) / 255.f; c.a = 1; return c; }
Color rgba(unsigned h, float a) { Color c = rgb(h); c.a = a; return c; }
Color cmix(Color a, Color b, float t) { Color c; c.r = a.r + (b.r - a.r) * t; c.g = a.g + (b.g - a.g) * t; c.b = a.b + (b.b - a.b) * t; c.a = a.a + (b.a - a.a) * t; return c; }
Color calpha(Color c, float a) { c.a *= a; return c; }

Color C_PAPER, C_CARD, C_INK, C_INK2, C_MUTED, C_FAINT, C_HAIR, C_WASH, C_WHITE;
Color MOTIF_COL[MO_COUNT];

void d_begin_frame(int w, int h) {
    static int init;
    if (!init) {
        C_PAPER = rgb(0xF3F0E8); C_CARD = rgb(0xFAF8F3); C_INK = rgb(0x1D1C1A); C_INK2 = rgb(0x3E3C37);
        C_MUTED = rgb(0x6F6B62); C_FAINT = rgb(0xA9A496); C_HAIR = rgb(0xD8D2C4); C_WASH = rgb(0xE9E4D8); C_WHITE = rgb(0xFFFFFF);
        MOTIF_COL[MO_GROUND] = rgb(0x4B4842);
        MOTIF_COL[MO_SPLIT]  = rgb(0xB97A12);
        MOTIF_COL[MO_TURN]   = rgb(0x2D5A9B);
        MOTIF_COL[MO_MIRROR] = rgb(0xBF4129);
        MOTIF_COL[MO_LOOP]   = rgb(0x3A7A4A);
        MOTIF_COL[MO_HOLD]   = rgb(0x7B4E98);
        MOTIF_COL[MO_LANES]  = rgb(0x1F7F86);
        MOTIF_COL[MO_SHADOW] = rgb(0x5C6878);
        init = 1;
    }
    g_wh = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void col(Color c, float a) { glColor4f(c.r, c.g, c.b, c.a * a); }

void d_rect(Rect r, Color c) {
    col(c, 1); glBegin(GL_QUADS);
    glVertex2f(r.x, r.y); glVertex2f(r.x + r.w, r.y); glVertex2f(r.x + r.w, r.y + r.h); glVertex2f(r.x, r.y + r.h);
    glEnd();
}
void d_rect_line(Rect r, float w, Color c) {
    d_rect(rect(r.x, r.y, r.w, w), c); d_rect(rect(r.x, r.y + r.h - w, r.w, w), c);
    d_rect(rect(r.x, r.y + w, w, r.h - 2 * w), c); d_rect(rect(r.x + r.w - w, r.y + w, w, r.h - 2 * w), c);
}
void d_vgrad(Rect r, Color t, Color b) {
    glBegin(GL_QUADS);
    col(t, 1); glVertex2f(r.x, r.y); glVertex2f(r.x + r.w, r.y);
    col(b, 1); glVertex2f(r.x + r.w, r.y + r.h); glVertex2f(r.x, r.y + r.h);
    glEnd();
}
void d_quad_colors(float x0, float y0, float x1, float y1, Color c00, Color c10, Color c11, Color c01) {
    glBegin(GL_QUADS);
    col(c00, 1); glVertex2f(x0, y0); col(c10, 1); glVertex2f(x1, y0);
    col(c11, 1); glVertex2f(x1, y1); col(c01, 1); glVertex2f(x0, y1);
    glEnd();
}

/* -------- feathered geometry, emitted inside glBegin(GL_QUADS) -------- */
static void line_q(float x0, float y0, float x1, float y1, float w, Color c) {
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), nx, ny, hi, ho, a = 1;
    if (len < 1e-4f) return;
    nx = -dy / len; ny = dx / len;
    if (w < 1) { a = w; w = 1; }
    hi = w * 0.5f - 0.5f; ho = w * 0.5f + 0.5f; if (hi < 0) hi = 0;
    /* left feather */
    col(c, 0); glVertex2f(x0 + nx * ho, y0 + ny * ho); glVertex2f(x1 + nx * ho, y1 + ny * ho);
    col(c, a); glVertex2f(x1 + nx * hi, y1 + ny * hi); glVertex2f(x0 + nx * hi, y0 + ny * hi);
    /* core */
    if (hi > 0) {
        glVertex2f(x0 + nx * hi, y0 + ny * hi); glVertex2f(x1 + nx * hi, y1 + ny * hi);
        glVertex2f(x1 - nx * hi, y1 - ny * hi); glVertex2f(x0 - nx * hi, y0 - ny * hi);
    }
    /* right feather */
    glVertex2f(x0 - nx * hi, y0 - ny * hi); glVertex2f(x1 - nx * hi, y1 - ny * hi);
    col(c, 0); glVertex2f(x1 - nx * ho, y1 - ny * ho); glVertex2f(x0 - nx * ho, y0 - ny * ho);
}

static int segs_for(float r) { int n = (int)(r * 0.9f); if (n < 14) n = 14; if (n > 160) n = 160; return n; }

static void disc_q(float cx, float cy, float r, Color c) {
    int i, n = segs_for(r); float ri = r - 0.5f, ro = r + 0.5f;
    if (ri < 0) ri = 0;
    for (i = 0; i < n; i++) {
        float a0 = TAU_F * i / n, a1 = TAU_F * (i + 1) / n;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        col(c, 1);
        glVertex2f(cx, cy); glVertex2f(cx, cy); glVertex2f(cx + c1 * ri, cy + s1 * ri); glVertex2f(cx + c0 * ri, cy + s0 * ri);
        glVertex2f(cx + c0 * ri, cy + s0 * ri); glVertex2f(cx + c1 * ri, cy + s1 * ri);
        col(c, 0); glVertex2f(cx + c1 * ro, cy + s1 * ro); glVertex2f(cx + c0 * ro, cy + s0 * ro);
    }
}

static void arc_q(float cx, float cy, float r, float a0, float a1, float w, Color c) {
    int i, n = (int)(segs_for(r) * fabsf(a1 - a0) / TAU_F) + 2; float a = 1, hi, ho;
    if (w < 1) { a = w; w = 1; }
    hi = w * 0.5f - 0.5f; ho = w * 0.5f + 0.5f; if (hi < 0) hi = 0;
    for (i = 0; i < n; i++) {
        float t0 = a0 + (a1 - a0) * i / n, t1 = a0 + (a1 - a0) * (i + 1) / n;
        float c0 = cosf(t0), s0 = -sinf(t0), c1 = cosf(t1), s1 = -sinf(t1);
        float rr[4]; float aa[4]; int k;
        rr[0] = r + ho; rr[1] = r + hi; rr[2] = r - hi; rr[3] = r - ho;
        aa[0] = 0; aa[1] = a; aa[2] = a; aa[3] = 0;
        for (k = 0; k < 3; k++) {
            if (k == 1 && hi <= 0) continue;
            col(c, aa[k]); glVertex2f(cx + c0 * rr[k], cy + s0 * rr[k]); glVertex2f(cx + c1 * rr[k], cy + s1 * rr[k]);
            col(c, aa[k + 1]); glVertex2f(cx + c1 * rr[k + 1], cy + s1 * rr[k + 1]); glVertex2f(cx + c0 * rr[k + 1], cy + s0 * rr[k + 1]);
        }
    }
}

void d_line(float x0, float y0, float x1, float y1, float w, Color c) {
    glBegin(GL_QUADS); line_q(x0, y0, x1, y1, w, c); glEnd();
}
void d_dash(float x0, float y0, float x1, float y1, float w, float on, float off, Color c) {
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), t = 0;
    if (len < 1e-3f) return;
    dx /= len; dy /= len;
    glBegin(GL_QUADS);
    while (t < len) {
        float e = t + on; if (e > len) e = len;
        line_q(x0 + dx * t, y0 + dy * t, x0 + dx * e, y0 + dy * e, w, c);
        t += on + off;
    }
    glEnd();
}
void d_polyline(const float *p, int n, float w, Color c) {
    int i;
    if (n < 2) return;
    glBegin(GL_QUADS);
    for (i = 0; i + 1 < n; i++) line_q(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], w, c);
    if (w > 1.8f) for (i = 1; i + 1 < n; i++) disc_q(p[2 * i], p[2 * i + 1], w * 0.5f, c);
    glEnd();
}
void d_circle(float cx, float cy, float r, Color c) { glBegin(GL_QUADS); disc_q(cx, cy, r, c); glEnd(); }
void d_ring(float cx, float cy, float r, float w, Color c) { glBegin(GL_QUADS); arc_q(cx, cy, r, 0, TAU_F, w, c); glEnd(); }
void d_arc(float cx, float cy, float r, float a0, float a1, float w, Color c) { glBegin(GL_QUADS); arc_q(cx, cy, r, a0, a1, w, c); glEnd(); }
void d_dash_arc(float cx, float cy, float r, float a0, float a1, float w, float on, float off, Color c) {
    float step = (on + off) / (r > 1 ? r : 1), a, dir = a1 >= a0 ? 1.f : -1.f, onr = on / (r > 1 ? r : 1);
    glBegin(GL_QUADS);
    for (a = a0; dir * (a1 - a) > 0; a += dir * step) {
        float e = a + dir * onr; if (dir * (e - a1) > 0) e = a1;
        arc_q(cx, cy, r, a, e, w, c);
    }
    glEnd();
}
void d_tri(float x0, float y0, float x1, float y1, float x2, float y2, Color c) {
    col(c, 1); glBegin(GL_TRIANGLES); glVertex2f(x0, y0); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
    glBegin(GL_QUADS);
    line_q(x0, y0, x1, y1, 0.8f, c); line_q(x1, y1, x2, y2, 0.8f, c); line_q(x2, y2, x0, y0, 0.8f, c);
    glEnd();
}
void d_poly(const float *p, int n, Color c) {
    int i;
    col(c, 1); glBegin(GL_TRIANGLE_FAN); for (i = 0; i < n; i++) glVertex2f(p[2 * i], p[2 * i + 1]); glEnd();
}
void d_arrow(float x0, float y0, float x1, float y1, float w, float head, Color c) {
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), ux, uy, bx, by;
    if (len < 1e-3f) return;
    ux = dx / len; uy = dy / len;
    if (head > len * 0.6f) head = len * 0.6f;
    bx = x1 - ux * head; by = y1 - uy * head;
    d_line(x0, y0, bx + ux * 0.5f, by + uy * 0.5f, w, c);
    d_tri(x1, y1, bx - uy * head * 0.42f, by + ux * head * 0.42f, bx + uy * head * 0.42f, by - ux * head * 0.42f, c);
}
void d_rrect(Rect r, float rad, Color c) {
    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h; int i, n = 6;
    if (rad > r.w / 2) rad = r.w / 2;
    if (rad > r.h / 2) rad = r.h / 2;
    if (rad < 0.5f) { d_rect(r, c); return; }
    col(c, 1);
    glBegin(GL_QUADS);
    glVertex2f(x0 + rad, y0); glVertex2f(x1 - rad, y0); glVertex2f(x1 - rad, y1); glVertex2f(x0 + rad, y1);
    glVertex2f(x0, y0 + rad); glVertex2f(x0 + rad, y0 + rad); glVertex2f(x0 + rad, y1 - rad); glVertex2f(x0, y1 - rad);
    glVertex2f(x1 - rad, y0 + rad); glVertex2f(x1, y0 + rad); glVertex2f(x1, y1 - rad); glVertex2f(x1 - rad, y1 - rad);
    glEnd();
    {   float cx[4] = { x1 - rad, x0 + rad, x0 + rad, x1 - rad }, cy[4] = { y0 + rad, y0 + rad, y1 - rad, y1 - rad };
        int k;
        glBegin(GL_TRIANGLES);
        for (k = 0; k < 4; k++) for (i = 0; i < n; i++) {
            float a0 = -PI_F / 2 * k - PI_F / 2 * i / n, a1 = -PI_F / 2 * k - PI_F / 2 * (i + 1) / n;
            /* quadrant k: 0 top-right, 1 top-left, 2 bottom-left, 3 bottom-right (screen) */
            float b0 = (k == 0 ? 0 : k == 1 ? PI_F / 2 : k == 2 ? PI_F : 1.5f * PI_F);
            a0 = b0 + PI_F / 2 * i / n; a1 = b0 + PI_F / 2 * (i + 1) / n;
            glVertex2f(cx[k], cy[k]);
            glVertex2f(cx[k] + cosf(a0) * rad, cy[k] - sinf(a0) * rad);
            glVertex2f(cx[k] + cosf(a1) * rad, cy[k] - sinf(a1) * rad);
        }
        glEnd();
    }
}
void d_rrect_line(Rect r, float rad, float w, Color c) {
    float x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;
    if (rad > r.w / 2) rad = r.w / 2;
    if (rad > r.h / 2) rad = r.h / 2;
    glBegin(GL_QUADS);
    line_q(x0 + rad, y0, x1 - rad, y0, w, c); line_q(x0 + rad, y1, x1 - rad, y1, w, c);
    line_q(x0, y0 + rad, x0, y1 - rad, w, c); line_q(x1, y0 + rad, x1, y1 - rad, w, c);
    arc_q(x1 - rad, y0 + rad, rad, 0, PI_F / 2, w, c); arc_q(x0 + rad, y0 + rad, rad, PI_F / 2, PI_F, w, c);
    arc_q(x0 + rad, y1 - rad, rad, PI_F, 1.5f * PI_F, w, c); arc_q(x1 - rad, y1 - rad, rad, 1.5f * PI_F, TAU_F, w, c);
    glEnd();
}

/* ----------------------------------------------------------------- clip */
static Rect g_clip[16]; static int g_nclip;
static void apply_clip(void) {
    if (!g_nclip) { glDisable(GL_SCISSOR_TEST); return; }
    {   Rect r = g_clip[g_nclip - 1]; int x = (int)floorf(r.x), y = (int)floorf(r.y);
        int w = (int)ceilf(r.x + r.w) - x, h = (int)ceilf(r.y + r.h) - y;
        if (w < 0) w = 0; if (h < 0) h = 0;
        glEnable(GL_SCISSOR_TEST); glScissor(x, g_wh - (y + h), w, h); }
}
void clip_push(Rect r) {
    if (g_nclip) {
        Rect p = g_clip[g_nclip - 1];
        float x0 = r.x > p.x ? r.x : p.x, y0 = r.y > p.y ? r.y : p.y;
        float x1 = (r.x + r.w) < (p.x + p.w) ? r.x + r.w : p.x + p.w, y1 = (r.y + r.h) < (p.y + p.h) ? r.y + r.h : p.y + p.h;
        r = rect(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
    }
    if (g_nclip < 16) g_clip[g_nclip++] = r;
    apply_clip();
}
void clip_pop(void) { if (g_nclip) g_nclip--; apply_clip(); }

/* ----------------------------------------------------------------- text */
float d_text(int f, float x, float y, const char *s, Color c) { return font_draw(f, x, y, s, -1, c); }
float d_text_center(int f, float cx, float y, const char *s, Color c) { return font_draw(f, cx - font_width(f, s, -1) / 2, y, s, -1, c); }
float d_text_right(int f, float right, float y, const char *s, Color c) { return font_draw(f, right - font_width(f, s, -1), y, s, -1, c); }
float text_spaced_w(int f, const char *s, float track) {
    float w = 0; const char *p = s; int n = 0;
    while (*p) { const char *q = p; utf8_next(&q); w += font_width(f, p, (int)(q - p)); p = q; n++; }
    return w + track * (n > 0 ? n - 1 : 0);
}
float d_text_spaced(int f, float x, float y, const char *s, float track, Color c) {
    const char *p = s;
    while (*p) { const char *q = p; utf8_next(&q); x = font_draw(f, x, y, p, (int)(q - p), c) + track; p = q; }
    return x - track;
}

/* ------------------------------------------------------------ motif glyphs
   Each motif has a small drawn sign that recurs wherever the idea does. */
void motif_glyph(int m, float cx, float cy, float sz, float w, Color c) {
    float h = sz * 0.5f;
    switch (m) {
    case MO_GROUND:   /* a ground symbol: the drone everything stands on */
        d_line(cx, cy - h * 0.75f, cx, cy - h * 0.05f, w, c);
        d_line(cx - h * 0.8f, cy, cx + h * 0.8f, cy, w, c);
        d_line(cx - h * 0.52f, cy + h * 0.32f, cx + h * 0.52f, cy + h * 0.32f, w, c);
        d_line(cx - h * 0.24f, cy + h * 0.64f, cx + h * 0.24f, cy + h * 0.64f, w, c);
        break;
    case MO_SPLIT: {  /* two values, their middle, and the half-gap either side */
        float y0 = cy + h * 0.15f;
        d_line(cx - h * 0.85f, y0, cx + h * 0.85f, y0, w * 0.8f, calpha(c, 0.55f));
        d_circle(cx - h * 0.7f, y0, w * 1.35f, c); d_circle(cx + h * 0.7f, y0, w * 1.35f, c);
        d_line(cx, cy - h * 0.75f, cx, cy + h * 0.6f, w, c);
        d_arc(cx - h * 0.35f, y0, h * 0.35f, 0.15f, PI_F - 0.15f, w * 0.9f, c);
        d_arc(cx + h * 0.35f, y0, h * 0.35f, 0.15f, PI_F - 0.15f, w * 0.9f, c);
        break; }
    case MO_TURN: {   /* a quarter-and-more turn with its arrow */
        float r = h * 0.72f, a1 = 1.85f * PI_F / 1.2f;
        d_arc(cx, cy, r, 0.2f, a1, w, c);
        {   float ex = cx + cosf(a1) * r, ey = cy - sinf(a1) * r, tx = -sinf(a1), ty = -cosf(a1);
            float hl = h * 0.45f;
            d_tri(ex + tx * hl * 0.9f, ey + ty * hl * 0.9f, ex - ty * hl * 0.45f, ey + tx * hl * 0.45f, ex + ty * hl * 0.45f, ey - tx * hl * 0.45f, c); }
        d_circle(cx, cy, w * 1.1f, c);
        break; }
    case MO_MIRROR: { /* a mirror line with a flag and its reflection */
        d_dash(cx, cy - h * 0.9f, cx, cy + h * 0.9f, w * 0.8f, h * 0.22f, h * 0.14f, c);
        d_tri(cx - h * 0.18f, cy - h * 0.45f, cx - h * 0.85f, cy - h * 0.1f, cx - h * 0.18f, cy + h * 0.25f, c);
        d_tri(cx + h * 0.18f, cy - h * 0.45f, cx + h * 0.85f, cy - h * 0.1f, cx + h * 0.18f, cy + h * 0.25f, calpha(c, 0.45f));
        break; }
    case MO_LOOP:     /* a full turn: the same point again */
        d_ring(cx, cy, h * 0.68f, w, c);
        d_circle(cx + h * 0.68f, cy, w * 1.6f, c);
        d_line(cx + h * 0.42f, cy, cx + h * 0.94f, cy, w * 0.8f, c);
        break;
    case MO_HOLD:     /* one direction pinned while the other varies */
        d_line(cx - h * 0.9f, cy + h * 0.45f, cx + h * 0.9f, cy + h * 0.45f, w, c);
        d_line(cx, cy + h * 0.45f, cx, cy - h * 0.55f, w, c);
        d_circle(cx, cy - h * 0.62f, w * 1.9f, c);
        d_line(cx - h * 0.6f, cy + h * 0.45f, cx - h * 0.78f, cy + h * 0.8f, w * 0.7f, c);
        d_line(cx + h * 0.6f, cy + h * 0.45f, cx + h * 0.42f, cy + h * 0.8f, w * 0.7f, c);
        break;
    case MO_LANES:    /* two parallel lanes travelled side by side */
        d_line(cx - h * 0.85f, cy - h * 0.3f, cx + h * 0.55f, cy - h * 0.3f, w, c);
        d_line(cx - h * 0.85f, cy + h * 0.3f, cx + h * 0.55f, cy + h * 0.3f, w, c);
        d_tri(cx + h * 0.9f, cy - h * 0.3f, cx + h * 0.5f, cy - h * 0.52f, cx + h * 0.5f, cy - h * 0.08f, c);
        d_tri(cx + h * 0.9f, cy + h * 0.3f, cx + h * 0.5f, cy + h * 0.08f, cx + h * 0.5f, cy + h * 0.52f, c);
        break;
    case MO_SHADOW:   /* a vector and its shadow on the real line */
        d_line(cx - h * 0.85f, cy + h * 0.6f, cx + h * 0.85f, cy + h * 0.6f, w * 0.7f, calpha(c, 0.6f));
        d_line(cx - h * 0.7f, cy + h * 0.6f, cx + h * 0.45f, cy - h * 0.7f, w, c);
        d_dash(cx + h * 0.45f, cy - h * 0.7f, cx + h * 0.45f, cy + h * 0.6f, w * 0.7f, h * 0.16f, h * 0.12f, c);
        d_line(cx - h * 0.7f, cy + h * 0.6f, cx + h * 0.45f, cy + h * 0.6f, w * 2.0f, c);
        break;
    }
}
