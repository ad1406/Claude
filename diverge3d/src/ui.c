#include "ui.h"
#include "gl_inc.h"
#include "mathx.h"
#include <string.h>
#include <stdio.h>

UI ui;
Color C_BG, C_PANEL, C_INK, C_MUTED, C_FAINT, C_LINE, C_SOFT, C_HOVER, C_ACCENT, C_ACCENT_SOFT, C_ACCENT_INK,
      C_OK, C_OK_SOFT, C_WARN, C_WARN_SOFT, C_BAD, C_BAD_SOFT, C_TEAL, C_TEAL_SOFT, C_SHADOW;

Color rgba(unsigned h, float a) { Color c; c.r = ((h >> 16) & 255) / 255.f; c.g = ((h >> 8) & 255) / 255.f; c.b = (h & 255) / 255.f; c.a = a; return c; }
Color rgb(unsigned h) { return rgba(h, 1); }
Color cmix(Color a, Color b, float t) { Color c; c.r = lerpf(a.r, b.r, t); c.g = lerpf(a.g, b.g, t); c.b = lerpf(a.b, b.b, t); c.a = lerpf(a.a, b.a, t); return c; }
Color calpha(Color c, float a) { c.a *= a; return c; }

static int g_w, g_h;
static Rect g_clip[16]; static int g_nclip;
static int g_theme_done;

static void theme(void) {
    C_BG = rgb(0xFFFFFF); C_PANEL = rgb(0xFFFFFF); C_INK = rgb(0x1A2124); C_MUTED = rgb(0x66737A);
    C_FAINT = rgb(0x9AA5AA); C_LINE = rgb(0xE4E8EA); C_SOFT = rgb(0xF4F6F7); C_HOVER = rgb(0xEEF1F3);
    C_ACCENT = rgb(0xE0571F); C_ACCENT_SOFT = rgb(0xFCE9DF); C_ACCENT_INK = rgb(0xFFFFFF);
    C_OK = rgb(0x2B8150); C_OK_SOFT = rgb(0xE1F2E7); C_WARN = rgb(0xA8680E); C_WARN_SOFT = rgb(0xFBEFD9);
    C_BAD = rgb(0xC5372A); C_BAD_SOFT = rgb(0xFBE3DF); C_TEAL = rgb(0x1B7C8E); C_TEAL_SOFT = rgb(0xDDEFF2);
    C_SHADOW = rgba(0x0B1A22, 1);
}

void ui_begin(const Input *in, float dpi, double t, int w, int h) {
    if (!g_theme_done) { theme(); g_theme_done = 1; }
    ui.in = in; ui.s = dpi; ui.time = t; g_w = w; g_h = h;
    ui.last_hot = ui.hot; ui.hot = 0; ui.mouse_used = 0; ui.want_cursor = CURSOR_ARROW;
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glViewport(0, 0, w, h);
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    g_nclip = 0; glDisable(GL_SCISSOR_TEST);
}
void ui_end(void) {
    if (!ui.in->down[MOUSE_L]) ui.active = 0;
    if (ui.in->pressed[MOUSE_L] && ui.focus && ui.hot != ui.focus) ui.focus = 0;
}
int ui_mouse_in(Rect r) {
    if (g_nclip && !in_rect(g_clip[g_nclip - 1], (float)ui.in->mx, (float)ui.in->my)) return 0;
    return in_rect(r, (float)ui.in->mx, (float)ui.in->my);
}

static void col(Color c) { glColor4f(c.r, c.g, c.b, c.a); }
void d_rect(Rect r, Color c) {
    col(c); glBegin(GL_QUADS);
    glVertex2f(r.x, r.y); glVertex2f(r.x + r.w, r.y); glVertex2f(r.x + r.w, r.y + r.h); glVertex2f(r.x, r.y + r.h);
    glEnd();
}
void d_vgrad(Rect r, Color a, Color b) {
    glBegin(GL_QUADS);
    col(a); glVertex2f(r.x, r.y); glVertex2f(r.x + r.w, r.y);
    col(b); glVertex2f(r.x + r.w, r.y + r.h); glVertex2f(r.x, r.y + r.h);
    glEnd();
}
static void rr_path(Rect r, float rad, int fan) {
    int k, i; float cx[4], cy[4];
    if (rad > r.w / 2) rad = r.w / 2;
    if (rad > r.h / 2) rad = r.h / 2;
    cx[0] = r.x + r.w - rad; cy[0] = r.y + rad;
    cx[1] = r.x + rad;       cy[1] = r.y + rad;
    cx[2] = r.x + rad;       cy[2] = r.y + r.h - rad;
    cx[3] = r.x + r.w - rad; cy[3] = r.y + r.h - rad;
    if (fan) glVertex2f(r.x + r.w / 2, r.y + r.h / 2);
    for (k = 0; k < 4; k++)
        for (i = 0; i <= 6; i++) {
            float a = (k * 90 + i * 15) * PI_F / 180.0f;
            glVertex2f(cx[k] + cosf(a) * rad, cy[k] - sinf(a) * rad);
        }
    if (fan) glVertex2f(cx[0] + rad, cy[0]);
}
void d_rrect(Rect r, float rad, Color c) { col(c); glBegin(GL_TRIANGLE_FAN); rr_path(r, rad, 1); glEnd(); }
void d_rrect_border(Rect r, float rad, float bw, Color fill, Color border) {
    d_rrect(r, rad, border);
    d_rrect(rect(r.x + bw, r.y + bw, r.w - 2 * bw, r.h - 2 * bw), rad - bw, fill);
}
void d_shadow(Rect r, float rad, float spread, float alpha) {
    int i, n = 6;
    for (i = n; i >= 1; i--) {
        float e = spread * i / n;
        Rect q = rect(r.x - e, r.y - e + spread * 0.35f, r.w + 2 * e, r.h + 2 * e);
        d_rrect(q, rad + e, calpha(C_SHADOW, alpha / n));
    }
}
void d_line(float x0, float y0, float x1, float y1, float w, Color c) {
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy), nx, ny;
    if (l < 1e-4f) return;
    nx = -dy / l * w * 0.5f; ny = dx / l * w * 0.5f;
    col(c); glBegin(GL_QUADS);
    glVertex2f(x0 + nx, y0 + ny); glVertex2f(x1 + nx, y1 + ny); glVertex2f(x1 - nx, y1 - ny); glVertex2f(x0 - nx, y0 - ny);
    glEnd();
}
void d_circle(float cx, float cy, float r, Color c) {
    int i, n = r < 6 ? 16 : r < 30 ? 32 : 64;
    col(c); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (i = 0; i <= n; i++) { float a = (float)i / n * 2 * PI_F; glVertex2f(cx + cosf(a) * r, cy + sinf(a) * r); }
    glEnd();
}
void d_ring(float cx, float cy, float r0, float r1, Color c) {
    int i, n = 64;
    col(c); glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= n; i++) { float a = (float)i / n * 2 * PI_F; glVertex2f(cx + cosf(a) * r0, cy + sinf(a) * r0); glVertex2f(cx + cosf(a) * r1, cy + sinf(a) * r1); }
    glEnd();
}
void d_poly(const float *p, int n, Color c) {
    int i; col(c); glBegin(GL_POLYGON); for (i = 0; i < n; i++) glVertex2f(p[i * 2], p[i * 2 + 1]); glEnd();
}
void d_tri(float x0, float y0, float x1, float y1, float x2, float y2, Color c) {
    col(c); glBegin(GL_TRIANGLES); glVertex2f(x0, y0); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
}

static void apply_clip(void) {
    if (!g_nclip) { glDisable(GL_SCISSOR_TEST); return; }
    {
        Rect r = g_clip[g_nclip - 1];
        int x = (int)r.x, y = (int)r.y, w = (int)(r.w + 0.5f), h = (int)(r.h + 0.5f);
        if (w < 0) w = 0;
        if (h < 0) h = 0;
        glEnable(GL_SCISSOR_TEST); glScissor(x, g_h - (y + h), w, h);
    }
}
void clip_push(Rect r) {
    if (g_nclip) {   /* intersect with parent */
        Rect p = g_clip[g_nclip - 1];
        float x0 = r.x > p.x ? r.x : p.x, y0 = r.y > p.y ? r.y : p.y;
        float x1 = (r.x + r.w) < (p.x + p.w) ? r.x + r.w : p.x + p.w, y1 = (r.y + r.h) < (p.y + p.h) ? r.y + r.h : p.y + p.h;
        r = rect(x0, y0, x1 - x0, y1 - y0);
    }
    if (g_nclip < 16) g_clip[g_nclip++] = r;
    apply_clip();
}
void clip_pop(void) { if (g_nclip) g_nclip--; apply_clip(); }

float d_text(int f, float x, float y, const char *s, Color c) { return font_draw(f, (float)(int)x, y, s, -1, c); }
float d_text_center(int f, Rect r, const char *s, Color c) {
    float w = font_width(f, s, -1);
    return font_draw(f, (float)(int)(r.x + (r.w - w) / 2), (float)(int)(r.y + (r.h - font_line(f)) / 2 + S(0.5f)), s, -1, c);
}
float d_text_right(int f, float right, float y, const char *s, Color c) { return font_draw(f, (float)(int)(right - font_width(f, s, -1)), y, s, -1, c); }

/* ---- widgets ---- */
static int interact(unsigned id, Rect r, int *hover) {
    int clicked = 0;
    *hover = ui_mouse_in(r) && (!ui.active || ui.active == id);
    if (*hover) { ui.hot = id; ui.mouse_used = 1; }
    if (*hover && ui.in->pressed[MOUSE_L]) ui.active = id;
    if (ui.active == id && ui.in->released[MOUSE_L]) { if (*hover) clicked = 1; ui.active = 0; }
    return clicked;
}

int ui_button(unsigned id, Rect r, const char *label, int style) {
    int hover, click = interact(id, r, &hover), pressed = ui.active == id && hover;
    Color fill = C_PANEL, border = C_LINE, ink = C_INK; float rad = S(8);
    switch (style) {
    case BTN_PRIMARY: fill = hover ? cmix(C_ACCENT, rgb(0x000000), 0.08f) : C_ACCENT; border = fill; ink = C_ACCENT_INK; break;
    case BTN_SECONDARY: fill = hover ? C_HOVER : C_PANEL; break;
    case BTN_GHOST: fill = hover ? C_HOVER : calpha(C_PANEL, 0); border = fill; ink = hover ? C_INK : C_MUTED; break;
    case BTN_SEG: fill = hover ? C_HOVER : calpha(C_PANEL, 0); border = fill; ink = C_MUTED; rad = S(6); break;
    case BTN_SEG_ON: fill = C_PANEL; border = C_LINE; ink = C_INK; rad = S(6); break;
    case BTN_CHIP: fill = hover ? C_HOVER : C_PANEL; ink = C_MUTED; rad = r.h / 2; break;
    case BTN_CHIP_ON: fill = C_INK; border = C_INK; ink = C_PANEL; rad = r.h / 2; break;
    case BTN_DANGER_ON: fill = C_BAD; border = C_BAD; ink = C_ACCENT_INK; break;
    }
    if (pressed) fill = cmix(fill, rgb(0x000000), 0.06f);
    if (style == BTN_SEG_ON) d_shadow(r, rad, S(2), 0.10f);
    d_rrect_border(r, rad, S(1), fill, border);
    d_text_center(style == BTN_PRIMARY || style == BTN_SEG_ON || style == BTN_CHIP_ON || style == BTN_DANGER_ON ? F_BOLD : F_BODY, r, label, ink);
    if (hover) ui.want_cursor = CURSOR_HAND;
    return click;
}

int ui_hold(unsigned id, Rect r, const char *label, int extern_on) {
    int hover, on;
    interact(id, r, &hover);
    on = ui.active == id || extern_on;
    d_rrect_border(r, S(8), S(1), on ? C_BAD : (hover ? C_HOVER : C_PANEL), on ? C_BAD : C_LINE);
    d_text_center(F_BOLD, r, label, on ? C_ACCENT_INK : C_INK);
    if (hover) ui.want_cursor = CURSOR_HAND;
    return ui.active == id;
}

int ui_slider(unsigned id, Rect r, float *v, float mn, float mx, float step) {
    int hover, changed = 0; float t, kx, old = *v;
    Rect hit = rect(r.x - S(6), r.y, r.w + S(12), r.h);
    interact(id, hit, &hover);
    if (ui.active == id) {
        float nv = mn + clampf((ui.in->mx - r.x) / r.w, 0, 1) * (mx - mn);
        if (step > 0) nv = mn + floorf((nv - mn) / step + 0.5f) * step;
        *v = clampf(nv, mn, mx);
    }
    if (hover && ui.in->wheel != 0 && !ui.active) {
        float st = step > 0 ? step : (mx - mn) / 50;
        *v = clampf(*v + st * (ui.in->wheel > 0 ? 1 : -1), mn, mx);
    }
    changed = *v != old;
    t = (mx > mn) ? (*v - mn) / (mx - mn) : 0;
    {
        float cy = r.y + r.h / 2, th = S(4);
        d_rrect(rect(r.x, cy - th / 2, r.w, th), th / 2, C_LINE);
        d_rrect(rect(r.x, cy - th / 2, r.w * t, th), th / 2, C_ACCENT);
        kx = r.x + r.w * t;
        d_circle(kx, cy + S(1), S(9), calpha(C_SHADOW, 0.12f));
        d_circle(kx, cy, S(8), (hover || ui.active == id) ? C_ACCENT : C_PANEL);
        d_ring(kx, cy, S(7), S(8.5f), C_ACCENT);
    }
    if (hover) ui.want_cursor = CURSOR_HAND;
    return changed;
}

int ui_textbox(unsigned id, Rect r, char *buf, int cap, const char *ph) {
    int hover, changed = 0, i; size_t len;
    interact(id, r, &hover);
    if (hover && ui.in->pressed[MOUSE_L]) ui.focus = id;
    if (ui.focus == id) {
        len = strlen(buf);
        for (i = 0; i < ui.in->ntext; i++) {
            unsigned c = ui.in->text[i];
            char enc[4]; int n = 0;
            if (c < 0x80) { enc[0] = (char)c; n = 1; }
            else if (c < 0x800) { enc[0] = (char)(0xC0 | (c >> 6)); enc[1] = (char)(0x80 | (c & 63)); n = 2; }
            else { enc[0] = (char)(0xE0 | (c >> 12)); enc[1] = (char)(0x80 | ((c >> 6) & 63)); enc[2] = (char)(0x80 | (c & 63)); n = 3; }
            if (len + (size_t)n < (size_t)cap) { memcpy(buf + len, enc, (size_t)n); len += (size_t)n; buf[len] = 0; changed = 1; }
        }
        if (ui.in->key_pressed[KEY_BACKSPACE] && len > 0) {
            do { len--; } while (len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80);
            buf[len] = 0; changed = 1;
        }
        if (ui.in->key_pressed[KEY_ESC]) { if (len) { buf[0] = 0; changed = 1; } else ui.focus = 0; }
        if (ui.in->key_pressed[KEY_ENTER]) ui.focus = 0;
    }
    d_rrect_border(r, S(8), ui.focus == id ? S(1.5f) : S(1), C_SOFT, ui.focus == id ? C_ACCENT : C_LINE);
    {
        float tx = r.x + S(32), ty = r.y + (r.h - font_line(F_BODY)) / 2;
        /* magnifier icon */
        d_ring(r.x + S(16), r.y + r.h / 2 - S(1), S(4.5f), S(6), C_MUTED);
        d_line(r.x + S(19.5f), r.y + r.h / 2 + S(2.5f), r.x + S(23), r.y + r.h / 2 + S(6), S(1.6f), C_MUTED);
        clip_push(rect(r.x + S(28), r.y, r.w - S(36), r.h));
        if (buf[0]) {
            float ex = d_text(F_BODY, tx, ty, buf, C_INK);
            if (ui.focus == id && fmod(ui.time, 1.0) < 0.55) d_rect(rect(ex + S(1), ty + S(2), S(1.5f), font_line(F_BODY) - S(4)), C_INK);
        } else {
            d_text(F_BODY, tx, ty, ph, C_FAINT);
            if (ui.focus == id && fmod(ui.time, 1.0) < 0.55) d_rect(rect(tx, ty + S(2), S(1.5f), font_line(F_BODY) - S(4)), C_INK);
        }
        clip_pop();
    }
    if (hover) ui.want_cursor = CURSOR_TEXT;
    return changed;
}

int ui_row(unsigned id, Rect r, int selected) {
    int hover, click = interact(id, r, &hover);
    if (selected) d_rrect(r, S(8), C_ACCENT_SOFT);
    else if (hover) d_rrect(r, S(8), C_HOVER);
    if (hover) ui.want_cursor = CURSOR_HAND;
    return click;
}

float ui_pill_w(const char *text) { return font_width(F_SMALL, text, -1) + S(14); }
float ui_pill(float x, float y, const char *text, int kind) {
    Color bg = C_TEAL_SOFT, fg = C_TEAL; float w = ui_pill_w(text), h = font_line(F_SMALL) + S(4);
    if (kind == 1) { bg = C_OK_SOFT; fg = C_OK; } else if (kind == 2) { bg = C_WARN_SOFT; fg = C_WARN; } else if (kind == 3) { bg = C_BAD_SOFT; fg = C_BAD; }
    else if (kind == 4) { bg = C_SOFT; fg = C_MUTED; }
    d_rrect(rect(x, y, w, h), h / 2, bg);
    d_text(F_SMALL, x + S(7), y + S(2), text, fg);
    return w;
}

int ui_segmented(unsigned id, Rect r, const char **labels, int n, int *sel) {
    int i, changed = 0; float w = (r.w - S(6)) / n;
    d_rrect(r, S(8), C_SOFT);
    for (i = 0; i < n; i++) {
        Rect b = rect(r.x + S(3) + w * i, r.y + S(3), w, r.h - S(6));
        if (ui_button(id * 31u + (unsigned)i, b, labels[i], *sel == i ? BTN_SEG_ON : BTN_SEG) && *sel != i) { *sel = i; changed = 1; }
    }
    return changed;
}

void ui_tooltip(float x, float y, const char *text) {
    float w = font_width(F_BOLD, text, -1) + S(18), h = font_line(F_BOLD) + S(10);
    Rect r = rect(x + S(14), y + S(14), w, h);
    if (r.x + r.w > g_w - S(4)) r.x = g_w - S(4) - r.w;
    if (r.y + r.h > g_h - S(4)) r.y = y - h - S(8);
    d_shadow(r, S(6), S(6), 0.18f);
    d_rrect(r, S(6), C_INK);
    d_text(F_BOLD, r.x + S(9), r.y + S(5), text, C_PANEL);
}

float scroll_begin(unsigned id, Scroll *sc, Rect r) {
    (void)id;
    if (ui_mouse_in(r) && ui.in->wheel != 0) sc->target -= ui.in->wheel * S(70);
    {
        float maxoff = sc->content_h - r.h; if (maxoff < 0) maxoff = 0;
        sc->target = clampf(sc->target, 0, maxoff);
        sc->off += (sc->target - sc->off) * 0.35f;
        if (fabsf(sc->target - sc->off) < 0.5f) sc->off = sc->target;
    }
    clip_push(r);
    return r.y - sc->off;
}
void scroll_end(Scroll *sc, Rect r, float used_h) {
    sc->content_h = used_h;
    clip_pop();
    if (used_h > r.h + 1) {   /* thin scrollbar */
        float frac = r.h / used_h, th = r.h * frac, ty = r.y + (r.h - th) * (sc->off / (used_h - r.h));
        d_rrect(rect(r.x + r.w - S(5), ty, S(3), th), S(1.5f), rgba(0x1A2124, 0.18f));
    }
}
