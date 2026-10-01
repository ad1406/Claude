/* Leitmotif: Chapters 1-2 of a harmonic analysis text, read as a score.

   Three views share one vocabulary of seven motifs (plus the ground):
     Score   - the whole two chapters at once: every result and exercise is a
               column, every motif a staff, so recurring ideas read as
               recurring lines. A fisheye lens magnifies around the cursor.
     Piece   - one result or exercise: its derivation as a sequence of moves.
               In Perform mode you choose each move on the motif keyboard
               before it is revealed; references open the earlier piece, so
               any formula can be followed back to the ground.
     Etudes  - each motif alone: its canonical picture and every place it is
               used. */
#include "gl_inc.h"
#include "platform.h"
#include "mem.h"
#include "font.h"
#include "draw.h"
#include "tex.h"
#include "synth.h"
#include "content.h"
#include "stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { V_SCORE, V_PIECE, V_ETUDE };
#define MC_DUMMY_PLAY MOTIF_COL[MO_LOOP]

static struct {
    Input in;
    int view, help;
    int msaa;
    double t; float dt;
    /* score */
    float focus, focus_amt; int hov_col, hov_staff, sel_filter;
    int playing; float play_x; int play_last;
    /* piece */
    int stack[24], depth;          /* breadcrumb of pieces */
    int perform;
    int sel_step;
    float scroll, scroll_target, content_h;
    float shake; int shake_key; int wrong_key;
    int flash_key; float flash;
    int hint_shown;
    int lineage_open;
    /* etude */
    int etude;
    float escroll, escroll_target, econtent_h;
    Stage st;
    /* keyboard */
    int kbd, kcol, ecur, scroll_to_step;
    int palette, pal_sel; char pal_q[64];
    int help_was_open, nav_jumped;
    /* mouse */
    int clicked, consumed, want_hand;
    /* self-test */
    int selftest, frames; const char *shot_dir;
} A;

static int g_revealed[160];
static int g_wrong[160];

#define CUR (A.depth > 0 ? A.stack[A.depth - 1] : -1)

/* ------------------------------------------------------------- helpers */
static int hover(Rect r) { return in_rect(r, (float)A.in.mx, (float)A.in.my); }
static int click(Rect r) {
    if (hover(r)) { A.want_hand = 1; if (A.clicked && !A.consumed) { A.consumed = 1; return 1; } }
    return 0;
}
static void play_motif(int m, float amp) { synth_pluck(110.0f * MOTIFS[m].harmonic, amp, 0.9f); }
static void play_chord(int p) {
    int m;
    synth_pluck(110.0f, 0.10f, 1.6f);
    for (m = 1; m < MO_COUNT; m++) if (PIECE_MOTIFS[p][m]) synth_pluck(110.0f * MOTIFS[m].harmonic, 0.07f + 0.02f * PIECE_MOTIFS[p][m], 1.3f);
}
static const char *kind_name(int k) { return k == K_GROUND ? "GROUND" : k == K_DEF ? "DEFINITION" : k == K_PROBLEM ? "EXERCISE" : "RESULT"; }
static const char *short_label(int p) {
    static char buf[8][32]; static int k;
    const Piece *pc = &PIECES[p]; char *b = buf[k = (k + 1) % 8];
    if (pc->kind == K_PROBLEM && pc->id[0] == 'P') snprintf(b, 32, "P%s", pc->id + 1);
    else snprintf(b, 32, "%s", pc->label);
    return b;
}

static RichStyle rs(int face, int iface, float mpx) { RichStyle r; r.face = face; r.iface = iface; r.mathpx = mpx; r.line_gap = S(3); return r; }

/* a chip: small rounded label; returns width; sets *hit when clicked */
static float chip(float x, float y, const char *text, Color c, int filled, int *hit) {
    float w = font_width(F_S, text, -1) + S(16), h = S(22);
    Rect r = rect(x, y, w, h); int hv = hover(r);
    d_rrect(r, S(11), filled ? calpha(c, hv ? 0.22f : 0.13f) : (hv ? calpha(c, 0.10f) : C_CARD));
    d_rrect_line(r, S(11), 1, calpha(c, hv ? 0.9f : 0.45f));
    font_draw(F_S, x + S(8), y + (h - font_line(F_S)) / 2, text, -1, cmix(c, C_INK, 0.25f));
    if (hit) *hit = click(r);
    return w;
}

static int button(Rect r, const char *label, int on) {
    int hv = hover(r);
    d_rrect(r, S(6), on ? C_INK : hv ? C_WASH : C_CARD);
    if (!on) d_rrect_line(r, S(6), 1, C_HAIR);
    d_text_center(F_UI, r.x + r.w / 2, r.y + (r.h - font_line(F_UI)) / 2, label, on ? C_PAPER : C_INK2);
    return click(r);
}

/* ------------------------------------------------------------ navigation */
static void stage_for_piece(void);
static void open_piece(int p, int push) {
    if (p < 0) return;
    if (!push) A.depth = 0;
    if (A.depth > 0 && A.stack[A.depth - 1] == p) { A.view = V_PIECE; return; }
    plat_log("open piece %s", PIECES[p].id);
    if (A.depth < 24) A.stack[A.depth++] = p;
    else { memmove(A.stack, A.stack + 1, sizeof(int) * 23); A.stack[23] = p; }
    A.view = V_PIECE; A.scroll = A.scroll_target = 0; A.hint_shown = 0;
    A.sel_step = A.perform ? g_revealed[p] - 1 : 0;
    if (!PIECES[p].nsteps) A.sel_step = -1;
    synth_release_all();
    stage_for_piece();
    play_chord(p);
}

static void stage_for_piece(void) {
    int p = CUR, s;
    const Piece *pc;
    if (p < 0) return;
    pc = &PIECES[p];
    s = A.sel_step;
    if (A.perform && s >= g_revealed[p]) s = g_revealed[p] - 1;
    for (; s >= 0; s--) if (pc->steps[s].scene >= 0) { stage_set(&A.st, pc->steps[s].scene, pc->steps[s].p); return; }
    stage_set(&A.st, pc->scene, pc->p);
}

/* ------------------------------------------------------------- history
   Every change of place (view, piece, breadcrumb, etude) is recorded, so
   Back returns exactly where you were: same step, same scroll. */
typedef struct { int view, depth, stack[24], sel_step, etude, kcol, ecur; float scroll, escroll; } Loc;
#define NHIST 96
static Loc g_back[NHIST], g_fwd[NHIST];
static int g_nb, g_nf;

static Loc loc_now(void) {
    Loc l; memset(&l, 0, sizeof l);
    l.view = A.view; l.depth = A.depth; memcpy(l.stack, A.stack, sizeof l.stack);
    l.sel_step = A.sel_step; l.etude = A.etude; l.kcol = A.kcol; l.ecur = A.ecur; l.scroll = A.scroll_target; l.escroll = A.escroll_target;
    return l;
}
static int same_place(const Loc *a, const Loc *b) {
    if (a->view != b->view) return 0;
    if (a->view == V_PIECE) return a->depth == b->depth && !memcmp(a->stack, b->stack, sizeof(int) * (size_t)a->depth);
    if (a->view == V_ETUDE) return a->etude == b->etude;
    return 1;
}
static void loc_restore(const Loc *l) {
    A.view = l->view; A.depth = l->depth; memcpy(A.stack, l->stack, sizeof A.stack);
    A.etude = l->etude; A.kcol = l->kcol;
    synth_release_all();
    if (A.view == V_PIECE && CUR >= 0) {
        int p = CUR;
        A.sel_step = l->sel_step;
        if (A.perform && A.sel_step > g_revealed[p] - 1) A.sel_step = g_revealed[p] - 1;
        if (!PIECES[p].nsteps) A.sel_step = -1;
        A.scroll = A.scroll_target = l->scroll; A.hint_shown = 0; A.wrong_key = -1;
        stage_for_piece();
    } else if (A.view == V_PIECE) A.view = V_SCORE;
    if (A.view == V_ETUDE) { A.escroll = A.escroll_target = l->escroll; A.ecur = l->ecur; stage_set(&A.st, MOTIFS[A.etude].scene, MOTIFS[A.etude].p); }
}
static void push_hist(Loc *st, int *n, const Loc *l) {
    if (*n == NHIST) { memmove(st, st + 1, sizeof(Loc) * (NHIST - 1)); (*n)--; }
    st[(*n)++] = *l;
}
static void nav_back(void) {
    Loc here;
    if (!g_nb) return;
    here = loc_now(); push_hist(g_fwd, &g_nf, &here);
    loc_restore(&g_back[--g_nb]); A.nav_jumped = 1;
}
static void nav_forward(void) {
    Loc here;
    if (!g_nf) return;
    here = loc_now(); push_hist(g_back, &g_nb, &here);
    loc_restore(&g_fwd[--g_nf]); A.nav_jumped = 1;
}
static void open_etude(int m) {
    A.etude = m; A.view = V_ETUDE; A.ecur = 0; A.escroll = A.escroll_target = 0;
    synth_release_all(); stage_set(&A.st, MOTIFS[m].scene, MOTIFS[m].p);
}
static void go_score(void) { A.view = V_SCORE; synth_release_all(); if (CUR >= 0) { A.kcol = CUR; } }

/* ---------------------------------------------------------- go-to palette */
typedef struct { int kind, idx; char text[160]; } PalItem;   /* kind 0 piece, 1 etude, 2 view */
static PalItem g_pal[128];
static int pal_lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int pal_find(const char *hay, const char *q) {   /* -1 none, 0 prefix, 1 inside */
    int i, j, n = (int)strlen(q);
    if (!n) return 1;
    for (i = 0; hay[i]; i++) {
        for (j = 0; j < n && hay[i + j] && pal_lower((unsigned char)hay[i + j]) == pal_lower((unsigned char)q[j]); j++) ;
        if (j == n) return i == 0 ? 0 : 1;
    }
    return -1;
}
static int pal_build(PalItem *out, int cap) {
    int n = 0, pass, i;
    const char *q = A.pal_q;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {   /* views and etudes */
            static const char *views[2] = { "Score", "Etudes" };
            for (i = 0; i < 2 && n < cap; i++) if (pal_find(views[i], q) >= 0) { out[n].kind = 2; out[n].idx = i; snprintf(out[n].text, 160, "%s  (view)", views[i]); n++; }
            for (i = 0; i < MO_COUNT && n < cap; i++) if (pal_find(MOTIFS[i].name, q) >= 0) { out[n].kind = 1; out[n].idx = i; snprintf(out[n].text, 160, "%s  (etude)", MOTIFS[i].name); n++; }
        }
        for (i = 0; i < NPIECES && n < cap; i++) {
            const Piece *pc = &PIECES[i];
            int m = pal_find(pc->id, q), m2 = pal_find(pc->label, q), m3 = pal_find(pc->name, q), m4 = pal_find(short_label(i), q), best = 9;
            if (m >= 0 && m < best) best = m; if (m2 >= 0 && m2 < best) best = m2; if (m3 >= 0 && m3 < best) best = m3; if (m4 >= 0 && m4 < best) best = m4;
            if (best == 9 || best != pass) continue;
            out[n].kind = 0; out[n].idx = i; snprintf(out[n].text, 160, "%s   %s", pc->label, pc->name); n++;
        }
    }
    return n;
}
static void pal_open_item(const PalItem *it) {
    A.palette = 0;
    if (it->kind == 0) open_piece(it->idx, 0);
    else if (it->kind == 1) open_etude(it->idx);
    else if (it->idx == 0) go_score(); else open_etude(A.etude);
}
static void palette_overlay(Rect r) {
    Rect b = rect(r.x + r.w / 2 - S(320), r.y + S(40), S(640), S(470));
    int n = pal_build(g_pal, 128), i, k, first, show = 12;
    float y;
    /* typing */
    for (k = 0; k < A.in.ntext; k++) {
        unsigned c = A.in.text[k]; size_t L = strlen(A.pal_q);
        if (c >= 32 && c < 127 && c != '/' && L < sizeof A.pal_q - 1) { A.pal_q[L] = (char)c; A.pal_q[L + 1] = 0; A.pal_sel = 0; }
    }
    if (A.in.key_pressed[KEY_BACKSPACE]) { size_t L = strlen(A.pal_q); if (L) A.pal_q[L - 1] = 0; A.pal_sel = 0; }
    n = pal_build(g_pal, 128);
    if (A.in.key_pressed[KEY_DOWN]) A.pal_sel++;
    if (A.in.key_pressed[KEY_UP]) A.pal_sel--;
    if (A.in.key_pressed[KEY_PGDN]) A.pal_sel += show;
    if (A.in.key_pressed[KEY_PGUP]) A.pal_sel -= show;
    if (A.pal_sel >= n) A.pal_sel = n - 1; if (A.pal_sel < 0) A.pal_sel = 0;
    d_rect(r, calpha(C_PAPER, 0.7f));
    d_rrect(b, S(12), C_CARD); d_rrect_line(b, S(12), 1, C_HAIR);
    y = b.y + S(18);
    {   Rect tb = rect(b.x + S(18), y, b.w - S(36), S(40)); float tx;
        d_rrect(tb, S(8), C_PAPER); d_rrect_line(tb, S(8), S(1.5f), C_INK2);
        tx = font_draw(F_TX, tb.x + S(12), tb.y + (tb.h - font_line(F_TX)) / 2, A.pal_q[0] ? A.pal_q : "", -1, C_INK);
        if (!A.pal_q[0]) font_draw(F_TXI, tb.x + S(12), tb.y + (tb.h - font_line(F_TXI)) / 2, "Go to a result, exercise or motif: type 2.38, beats, P1.4, mirror ...", -1, C_FAINT);
        else if (fmodf((float)A.t, 1.0f) < 0.6f) d_rect(rect(tx + S(2), tb.y + S(9), S(1.5f), tb.h - S(18)), C_INK);
        y += S(54); }
    first = A.pal_sel >= show ? A.pal_sel - show + 1 : 0;
    for (i = first; i < n && i < first + show; i++) {
        Rect row = rect(b.x + S(10), y, b.w - S(20), S(30));
        int on = i == A.pal_sel;
        if (hover(row) && (A.in.mdx || A.in.mdy)) A.pal_sel = i;
        if (on) d_rrect(row, S(6), calpha(C_INK, 0.08f));
        if (g_pal[i].kind == 0) {
            int m, gx = 0;
            for (m = 1; m < MO_COUNT; m++) if (PIECE_MOTIFS[g_pal[i].idx][m]) { motif_glyph(m, row.x + row.w - S(16) - gx, row.y + row.h / 2, S(14), S(1.3f), MOTIF_COL[m]); gx += S(20); }
            d_text(F_UI, row.x + S(10), row.y + (row.h - font_line(F_UI)) / 2, g_pal[i].text, on ? C_INK : C_INK2);
        } else {
            if (g_pal[i].kind == 1) motif_glyph(g_pal[i].idx, row.x + S(20), row.y + row.h / 2, S(16), S(1.4f), MOTIF_COL[g_pal[i].idx]);
            d_text(F_UIB, row.x + S(40), row.y + (row.h - font_line(F_UIB)) / 2, g_pal[i].text, on ? C_INK : C_INK2);
        }
        if (A.clicked && hover(row)) { A.clicked = 0; pal_open_item(&g_pal[i]); return; }
        y += S(32);
    }
    if (!n) d_text(F_TXI, b.x + S(24), y + S(6), "Nothing matches.", C_MUTED);
    d_text(F_XS, b.x + S(20), b.y + b.h - S(24), "↑ ↓ choose     Enter open     Esc close", C_MUTED);
    if (A.in.key_pressed[KEY_ENTER] && n > 0) { pal_open_item(&g_pal[A.pal_sel]); return; }
    if (A.in.key_pressed[KEY_ESC]) A.palette = 0;
    if (A.clicked && !hover(b)) { A.palette = 0; A.clicked = 0; }
}

/* ============================================================== top bar */
static void top_bar(Rect r) {
    float x = r.x + S(24);
    d_rect(r, C_PAPER);
    d_line(r.x, r.y + r.h - 0.5f, r.x + r.w, r.y + r.h - 0.5f, 1, C_HAIR);
    x = font_draw(F_H2I, x, r.y + (r.h - font_line(F_H2I)) / 2 + S(1), "Leitmotif", -1, C_INK) + S(14);
    d_text_spaced(F_CAP, x, r.y + (r.h - font_line(F_CAP)) / 2 + S(2), "CHAPTERS 1–2 AS A SCORE", S(1.6f), C_MUTED);
    x += text_spaced_w(F_CAP, "CHAPTERS 1–2 AS A SCORE", S(1.6f)) + S(30);
    {   const char *tabs[3] = { "Score", "Etudes", "Piece" }; int i;
        for (i = 0; i < 3; i++) {
            float w = font_width(F_UIB, tabs[i], -1) + S(22); Rect b = rect(x, r.y + S(12), w, r.h - S(24));
            int on = (i == 0 && A.view == V_SCORE) || (i == 1 && A.view == V_ETUDE) || (i == 2 && A.view == V_PIECE);
            if (i == 2 && A.depth == 0) continue;
            if (on) d_rrect(b, S(6), C_INK);
            else if (hover(b)) d_rrect(b, S(6), C_WASH);
            d_text_center(F_UIB, b.x + b.w / 2, b.y + (b.h - font_line(F_UIB)) / 2, tabs[i], on ? C_PAPER : C_INK2);
            if (click(b)) { if (i == 0) go_score(); else if (i == 1) open_etude(A.etude); else { A.view = V_PIECE; stage_for_piece(); } }
            x += w + S(6);
        }
    }
    if (A.view == V_PIECE && A.depth > 0) {   /* breadcrumb */
        int i; x += S(14);
        for (i = 0; i < A.depth; i++) {
            const char *s = short_label(A.stack[i]); float w = font_width(F_S, s, -1);
            Rect b = rect(x - S(4), r.y + S(14), w + S(8), r.h - S(28));
            if (i > 0) { d_text(F_S, x - S(14), r.y + (r.h - font_line(F_S)) / 2, "›", C_FAINT); }
            if (hover(b) && i < A.depth - 1) d_rrect(b, S(4), C_WASH);
            d_text(F_S, x, r.y + (r.h - font_line(F_S)) / 2, s, i == A.depth - 1 ? C_INK : C_MUTED);
            if (i < A.depth - 1 && click(b)) { A.depth = i + 1; A.sel_step = A.perform ? g_revealed[CUR] - 1 : 0; A.scroll = A.scroll_target = 0; stage_for_piece(); }
            x += w + S(20);
            if (x > r.x + r.w - S(640)) break;
        }
    }
    {   float rx = r.x + r.w - S(24);
        Rect b = rect(rx - S(34), r.y + S(12), S(34), r.h - S(24));
        if (button(b, "?", A.help)) A.help = !A.help;
        rx -= S(42);
        b = rect(rx - S(118), r.y + S(12), S(118), r.h - S(24));
        if (button(b, "Go to\u2026  Ctrl+K", 0)) { A.palette = 1; A.pal_q[0] = 0; A.pal_sel = 0; }
        rx -= S(126);
        b = rect(rx - S(34), r.y + S(12), S(34), r.h - S(24));
        if (g_nf) { if (button(b, "\u2192", 0)) nav_forward(); } else { d_rrect_line(b, S(6), 1, calpha(C_HAIR, 0.6f)); d_text_center(F_UI, b.x + b.w / 2, b.y + (b.h - font_line(F_UI)) / 2, "\u2192", C_FAINT); }
        rx -= S(40);
        b = rect(rx - S(34), r.y + S(12), S(34), r.h - S(24));
        if (g_nb) { if (button(b, "\u2190", 0)) nav_back(); } else { d_rrect_line(b, S(6), 1, calpha(C_HAIR, 0.6f)); d_text_center(F_UI, b.x + b.w / 2, b.y + (b.h - font_line(F_UI)) / 2, "\u2190", C_FAINT); }
        rx -= S(42);
        b = rect(rx - S(76), r.y + S(12), S(76), r.h - S(24));
        if (button(b, synth_muted() ? "sound off" : "sound on", 0)) synth_mute(!synth_muted());
        rx -= S(84);
        if (A.view == V_PIECE) {
            b = rect(rx - S(64), r.y + S(12), S(64), r.h - S(24));
            if (button(b, "Read", !A.perform)) { A.perform = 0; }
            rx -= S(68);
            b = rect(rx - S(80), r.y + S(12), S(80), r.h - S(24));
            if (button(b, "Perform", A.perform)) { A.perform = 1; if (CUR >= 0) A.sel_step = g_revealed[CUR] - 1; stage_for_piece(); }
        }
    }
}

/* ================================================================ score */
typedef struct { float x, w; } Col;
static Col g_cols[160];
static float g_staff_y[MO_COUNT];   /* y of each staff line */
static Rect g_staff_area;

/* order of staves top to bottom: motifs 1..7, then the ground */
static int staff_of(int m) { return m == MO_GROUND ? MO_COUNT - 1 : m - 1; }
static int motif_of_staff(int s) { return s == MO_COUNT - 1 ? MO_GROUND : s + 1; }

static void layout_columns(Rect r) {
    int i; float total = 0, x, wts[160];
    for (i = 0; i < NPIECES; i++) {
        float d = (float)i - A.focus, w = 1 + A.focus_amt * 5.0f * expf(-d * d / (2 * 2.6f * 2.6f));
        if (i > 0 && PIECES[i].section != PIECES[i - 1].section) w += 0.7f;
        wts[i] = w; total += w;
    }
    x = r.x;
    for (i = 0; i < NPIECES; i++) {
        float w = wts[i] / total * r.w, gap = (i > 0 && PIECES[i].section != PIECES[i - 1].section) ? 0.7f / total * r.w : 0;
        g_cols[i].x = x + gap + (w - gap) / 2; g_cols[i].w = w - gap;
        x += w;
    }
}

static void notehead(float x, float y, float r, int kind, Color c, int hollow) {
    if (kind == K_GROUND || kind == K_DEF) {
        float pts[8] = { x, y - r * 1.1f, x + r * 1.1f, y, x, y + r * 1.1f, x - r * 1.1f, y };
        if (hollow) { d_line(pts[0], pts[1], pts[2], pts[3], S(1.4f), c); d_line(pts[2], pts[3], pts[4], pts[5], S(1.4f), c); d_line(pts[4], pts[5], pts[6], pts[7], S(1.4f), c); d_line(pts[6], pts[7], pts[0], pts[1], S(1.4f), c); }
        else d_poly(pts, 4, c);
        return;
    }
    if (hollow) { d_ring(x, y, r * 0.92f, S(1.6f), c); }
    else d_circle(x, y, r, c);
}

static void score_view(Rect r) {
    int i, s, m;
    float gut = S(176), top = r.y + S(150), staff_gap, bottom_notes = S(262);
    float sy0, ground_gap = S(16);
    Rect sa;
    float avail = r.h - (top - r.y) - bottom_notes - S(70);
    staff_gap = (avail - ground_gap) / 7.5f;
    if (staff_gap < S(20)) staff_gap = S(20);
    if (staff_gap > S(46)) staff_gap = S(46);
    sy0 = top + S(30);
    for (s = 0; s < MO_COUNT; s++) g_staff_y[s] = sy0 + s * staff_gap + (s == MO_COUNT - 1 ? ground_gap : 0);
    sa = rect(r.x + gut, sy0 - staff_gap * 0.5f, r.w - gut - S(28), g_staff_y[MO_COUNT - 1] - sy0 + staff_gap);
    g_staff_area = sa;

    /* header: the thesis */
    {   float hx = r.x + S(28), hy = r.y + S(22);
        font_draw(F_H2I, hx, hy, "Two mirrors make a loop; a sum of turns is a product.", -1, C_INK);
        font_wrap(F_TXS, hx, hy + S(42), r.w * 0.62f,
                  "Each column is a result or exercise, in book order. Each staff is a motif: a move that recurs. A note means the derivation makes that move, and larger notes mean more often. Hover or use \u2190 \u2192 to read, click or Enter to derive. With a motif chosen (\u2191 \u2193), \u2190 \u2192 follow it from column to column. Space plays the score.", C_MUTED, 1);
        {   /* legend */
            float lx = r.x + r.w * 0.70f, ly = hy + S(6);
            notehead(lx, ly + S(8), S(5), K_RESULT, C_INK, 0); d_text(F_S, lx + S(12), ly, "result", C_INK2);
            notehead(lx + S(80), ly + S(8), S(5), K_PROBLEM, C_INK, 1); d_text(F_S, lx + S(92), ly, "exercise", C_INK2);
            notehead(lx + S(170), ly + S(8), S(5), K_DEF, C_INK, 0); d_text(F_S, lx + S(182), ly, "given / definition", C_INK2);
            d_text(F_S, lx, ly + S(24), "♣  essential exercise", C_INK2);
            d_circle(lx + S(134), ly + S(32), S(3), MOTIF_COL[MO_LOOP]); d_text(F_S, lx + S(144), ly + S(24), "performed by you", C_INK2);
            d_text(F_S, lx, ly + S(48), "keys 1\u20137 (0 = ground) highlight one motif", C_INK2);
        }
    }

    /* fisheye focus */
    {   int inside = in_rect(rect(sa.x, sa.y - S(40), sa.w, sa.h + S(60)), (float)A.in.mx, (float)A.in.my);
        float target_amt = inside ? 1.0f : 0.0f;
        if (inside) {
            /* invert the current layout to find the column under the mouse */
            float best = 1e9f; int bi = 0;
            for (i = 0; i < NPIECES; i++) { float d = fabsf(g_cols[i].x - A.in.mx); if (d < best) { best = d; bi = i; } }
            {   float frac = (float)bi;
                if (bi + 1 < NPIECES && A.in.mx > g_cols[bi].x) frac += (A.in.mx - g_cols[bi].x) / (g_cols[bi + 1].x - g_cols[bi].x + 1e-3f);
                else if (bi > 0 && A.in.mx < g_cols[bi].x) frac -= (g_cols[bi].x - A.in.mx) / (g_cols[bi].x - g_cols[bi - 1].x + 1e-3f);
                A.focus += (frac - A.focus) * 0.25f; }
        }
        if (A.playing) { A.focus += (A.play_x - A.focus) * 0.2f; target_amt = 1; }
        else if (A.kbd && A.kcol >= 0) { A.focus += ((float)A.kcol - A.focus) * 0.25f; target_amt = 1; }
        A.focus_amt += (target_amt - A.focus_amt) * 0.12f;
        layout_columns(sa);
    }

    /* movements and sections */
    {   int sec = -1; float ymv = g_staff_y[0] - S(76), ysec = g_staff_y[MO_COUNT - 1] + S(14);
        for (i = 0; i < NPIECES; i++) {
            if (PIECES[i].section != sec) {
                float bx = g_cols[i].x - g_cols[i].w / 2 - S(2);
                sec = PIECES[i].section;
                if (i > 0) {
                    int newch = SECTIONS[sec].chapter != SECTIONS[PIECES[i - 1].section].chapter;
                    d_line(bx, g_staff_y[0] - S(8), bx, g_staff_y[MO_COUNT - 1] + S(8), newch ? S(2) : 1, newch ? C_INK : C_HAIR);
                    if (newch) d_line(bx - S(4), g_staff_y[0] - S(8), bx - S(4), g_staff_y[MO_COUNT - 1] + S(8), 1, C_INK);
                }
                if (i == 0 || SECTIONS[sec].chapter != SECTIONS[PIECES[i - 1].section].chapter) {
                    const char *mv = SECTIONS[sec].chapter == 1 ? "I.  Tones \u2014 the signature of periodicity" : "II.  Beats \u2014 the complex plane";
                    font_draw(F_H3, bx + S(6), ymv, mv, -1, C_INK);
                }
                {   char buf[64]; snprintf(buf, sizeof buf, "\u00a7%s %s", SECTIONS[sec].num, g_cols[i].w > S(30) || 1 ? "" : "");
                    font_draw(F_XS, bx + S(5), ysec, buf, -1, C_MUTED); }
            }
        }
        {   float ex = sa.x + sa.w + S(4);
            d_line(ex, g_staff_y[0] - S(8), ex, g_staff_y[MO_COUNT - 1] + S(8), S(3), C_INK);
            d_line(ex - S(5), g_staff_y[0] - S(8), ex - S(5), g_staff_y[MO_COUNT - 1] + S(8), 1, C_INK); }
    }

    /* hover detection */
    A.hov_col = -1; A.hov_staff = -1;
    if (A.kbd && A.kcol >= 0) A.hov_col = A.kcol;
    else if (in_rect(rect(sa.x, sa.y - S(50), sa.w, sa.h + S(60)), (float)A.in.mx, (float)A.in.my)) {
        for (i = 0; i < NPIECES; i++) if (fabsf(A.in.mx - g_cols[i].x) <= g_cols[i].w / 2 + 0.5f) { A.hov_col = i; break; }
    }
    for (s = 0; s < MO_COUNT; s++) {
        Rect lr = rect(r.x, g_staff_y[s] - staff_gap / 2, gut, staff_gap);
        if (hover(lr) && !A.kbd) { A.hov_staff = s; A.want_hand = 1; if (click(lr)) open_etude(motif_of_staff(s)); }
    }
    {   int hl_m = A.hov_staff >= 0 ? motif_of_staff(A.hov_staff) : A.sel_filter;

        /* hovered column band */
        if (A.hov_col >= 0) d_rect(rect(g_cols[A.hov_col].x - g_cols[A.hov_col].w / 2, sa.y - S(50), g_cols[A.hov_col].w, sa.h + S(92)), calpha(C_WASH, 0.9f));
        if (A.playing) { float px = sa.x + 0; int pc = (int)A.play_x; if (pc >= 0 && pc < NPIECES) px = g_cols[pc].x; d_line(px, sa.y - S(50), px, sa.y + sa.h + S(30), S(1.5f), calpha(MC_DUMMY_PLAY, 1)); }

        /* staff lines and labels */
        for (s = 0; s < MO_COUNT; s++) {
            int mm = motif_of_staff(s); Color c = MOTIF_COL[mm];
            int dim = hl_m >= 0 && hl_m != mm;
            float y = g_staff_y[s];
            d_line(sa.x - S(10), y, sa.x + sa.w + S(4), y, mm == MO_GROUND ? S(1.8f) : 1, calpha(mm == MO_GROUND ? C_INK2 : C_HAIR, dim ? 0.5f : 1));
            motif_glyph(mm, r.x + S(34), y, S(20), S(1.6f), calpha(c, dim ? 0.35f : 1));
            d_text_spaced(F_CAP, r.x + S(56), y - font_line(F_CAP) / 2, mm == MO_GROUND ? "GROUND" : MOTIFS[mm].name, S(1.5f), calpha(cmix(c, C_INK, 0.2f), dim ? 0.35f : 1));
            {   char nb[8]; int cnt = 0; for (i = 0; i < NPIECES; i++) if (PIECE_MOTIFS[i][mm]) cnt++;
                snprintf(nb, sizeof nb, "%d", cnt); d_text_right(F_XS, r.x + gut - S(12), y - font_line(F_XS) / 2, nb, calpha(C_FAINT, dim ? 0.4f : 1)); }
        }

        /* slurs for the hovered column: what it needs (above), who uses it (below) */
        if (A.hov_col >= 0) {
            int users[64], nu = piece_users(A.hov_col, users, 64), k;
            float x0 = g_cols[A.hov_col].x, ytop = g_staff_y[0] - S(54), ybot = g_staff_y[MO_COUNT - 1] + S(26);
            for (k = 0; k < 6; k++) {
                int q = piece_need(A.hov_col, k); float x1, mx_, h; static float pts[2 * 41]; int j;
                if (q < 0) continue;
                x1 = g_cols[q].x; mx_ = (x0 + x1) / 2; h = S(14) + fabsf(x1 - x0) * 0.12f; if (h > S(60)) h = S(60);
                for (j = 0; j <= 40; j++) { float t = j / 40.0f; pts[2 * j] = x0 + (x1 - x0) * t; pts[2 * j + 1] = ytop - h * 4 * t * (1 - t); }
                d_polyline(pts, 41, S(1.4f), calpha(C_INK, 0.55f));
                d_circle(x1, ytop, S(2.5f), C_INK); (void)mx_;
            }
            for (k = 0; k < nu; k++) {
                float x1 = g_cols[users[k]].x, h = S(12) + fabsf(x1 - x0) * 0.1f; static float pts[2 * 41]; int j;
                if (h > S(50)) h = S(50);
                for (j = 0; j <= 40; j++) { float t = j / 40.0f; pts[2 * j] = x0 + (x1 - x0) * t; pts[2 * j + 1] = ybot + h * 4 * t * (1 - t); }
                d_polyline(pts, 41, S(1.2f), calpha(MOTIF_COL[MO_TURN], 0.45f));
                d_circle(x1, ybot, S(2.2f), MOTIF_COL[MO_TURN]);
            }
        }

        /* the notes */
        for (i = 0; i < NPIECES; i++) {
            const Piece *pc = &PIECES[i];
            float x = g_cols[i].x, ymin = 1e9f, ymax = -1e9f, scale = 0.75f + 0.5f * fminf(1, (g_cols[i].w - S(10)) / S(40));
            int hollow = pc->kind == K_PROBLEM, any_hl = 0;
            float flash = (A.playing && (int)A.play_x == i) ? 1 : 0;
            for (m = 0; m < MO_COUNT; m++) if (PIECE_MOTIFS[i][m]) {
                float y = g_staff_y[staff_of(m)]; if (y < ymin) ymin = y; if (y > ymax) ymax = y;
                if (m == hl_m) any_hl = 1;
            }
            if (ymin < 1e8f && ymax > ymin) d_line(x, ymin, x, ymax, 1, calpha(C_INK, hl_m >= 0 && !any_hl ? 0.15f : 0.45f));
            for (m = 0; m < MO_COUNT; m++) if (PIECE_MOTIFS[i][m]) {
                int cnt = PIECE_MOTIFS[i][m]; float rr = S(4.2f) * scale * (cnt >= 3 ? 1.45f : cnt == 2 ? 1.22f : 1);
                Color c = MOTIF_COL[m];
                if (hl_m >= 0 && m != hl_m) c = calpha(cmix(c, C_PAPER, 0.4f), any_hl ? 0.5f : 0.25f);
                if (flash > 0) d_circle(x, g_staff_y[staff_of(m)], rr * 2.2f, calpha(MOTIF_COL[m], 0.25f));
                notehead(x, g_staff_y[staff_of(m)], rr, pc->kind, c, hollow && m != MO_GROUND);
            }
            /* column label, essential mark, and your progress */
            {   const char *lb = short_label(i); float lw = font_width(F_XS, lb, -1);
                float ly = g_staff_y[0] - S(42);
                if (pc->essential) d_text_center(F_XS, x, g_staff_y[0] - S(24), "\u2663", calpha(C_INK, 0.6f));
                if (g_cols[i].w > lw + S(4) || i == A.hov_col) d_text_center(F_XS, x, ly, lb, i == A.hov_col ? C_INK : C_MUTED);
                else d_line(x, ly + S(7), x, ly + S(11), 1, C_FAINT);
                if (pc->nsteps && g_revealed[i] >= pc->nsteps) d_circle(x, g_staff_y[MO_COUNT - 1] + S(36), S(3), MOTIF_COL[MO_LOOP]);
                else if (pc->nsteps && g_revealed[i] > 0) d_ring(x, g_staff_y[MO_COUNT - 1] + S(36), S(3), 1, MOTIF_COL[MO_LOOP]);
            }
        }
        if (A.hov_col >= 0 && !A.kbd) { A.want_hand = 1; if (click(rect(sa.x, sa.y - S(50), sa.w, sa.h + S(60)))) { A.kcol = A.hov_col; open_piece(A.hov_col, 0); } }
        if (A.kbd && A.kcol >= 0) d_rrect_line(rect(g_cols[A.kcol].x - g_cols[A.kcol].w / 2 + 1, sa.y - S(50), g_cols[A.kcol].w - 2, sa.h + S(92)), S(4), S(1.5f), calpha(C_INK, 0.55f));
    }

    /* programme notes */
    {   Rect nb = rect(r.x + S(24), r.y + r.h - bottom_notes, r.w - S(48), bottom_notes - S(18));
        d_rrect(nb, S(10), C_CARD); d_rrect_line(nb, S(10), 1, C_HAIR);
        if (A.hov_col >= 0) {
            const Piece *pc = &PIECES[A.hov_col]; float x = nb.x + S(24), y = nb.y + S(18), mw = nb.w * 0.62f, px;
            char head[160];
            snprintf(head, sizeof head, "%s%s   ·   %s", kind_name(pc->kind), pc->essential ? " ♣" : "", pc->label);
            d_text_spaced(F_CAP, x, y, head, S(1.2f), C_MUTED);
            font_draw(F_H2, x, y + S(18), pc->name, -1, C_INK);
            px = tex_fit(pc->statement, S(26), mw);
            {   TexBox b = tex_measure(pc->statement, px); tex_draw(pc->statement, x, y + S(66) + b.asc, px, C_INK); y += S(66) + b.asc + b.desc + S(14); }
            rich(pc->gist, x, y, mw, rs(F_TXS, F_TXSI, S(15)), C_INK2, 1);
            {   float cx = nb.x + nb.w * 0.70f, cy = nb.y + S(22); int k, n = 0;
                d_text_spaced(F_CAP, cx, cy, "MOVES", S(1.4f), C_MUTED); cy += S(22);
                for (m = 1; m < MO_COUNT; m++) if (PIECE_MOTIFS[A.hov_col][m]) {
                    char t[48]; snprintf(t, sizeof t, "%s ×%d", MOTIFS[m].name, PIECE_MOTIFS[A.hov_col][m]);
                    motif_glyph(m, cx + S(9) + (n % 2) * S(150), cy + S(9) + (n / 2) * S(26), S(16), S(1.4f), MOTIF_COL[m]);
                    d_text(F_UI, cx + S(24) + (n % 2) * S(150), cy + (n / 2) * S(26), t, cmix(MOTIF_COL[m], C_INK, 0.3f));
                    n++;
                }
                if (!pc->nsteps) { motif_glyph(MO_GROUND, cx + S(9), cy + S(9), S(16), S(1.4f), MOTIF_COL[MO_GROUND]); d_text(F_UI, cx + S(24), cy, pc->kind == K_DEF ? "a definition: taken as given" : "ground: taken as given", C_INK2); n = 2; }
                cy += ((n + 1) / 2) * S(26) + S(10);
                {   int any = 0; float xx = cx;
                    for (k = 0; k < 6; k++) { int q = piece_need(A.hov_col, k); if (q < 0) continue;
                        if (!any) { d_text(F_XS, cx, cy, "needs", C_MUTED); cy += S(16); any = 1; }
                        xx += chip(xx, cy, short_label(q), C_INK, 0, NULL) + S(6); }
                    if (any) cy += S(30);
                }
                d_text(F_TXSI, cx, nb.y + nb.h - S(30), pc->nsteps ? "click or Enter to derive it \u2192" : "click or Enter to see it \u2192", MOTIF_COL[MO_TURN]);
            }
        } else if (A.hov_staff >= 0) {
            int mm = motif_of_staff(A.hov_staff); float x = nb.x + S(24), y = nb.y + S(18), cnt = 0;
            for (i = 0; i < NPIECES; i++) if (PIECE_MOTIFS[i][mm]) cnt++;
            motif_glyph(mm, x + S(14), y + S(20), S(30), S(2.2f), MOTIF_COL[mm]);
            font_draw(F_H2, x + S(40), y + S(4), MOTIFS[mm].name, -1, MOTIF_COL[mm]);
            {   char buf[256]; snprintf(buf, sizeof buf, "to %s", MOTIFS[mm].verb);
                rich(buf, x + S(40), y + S(40), nb.w * 0.6f, rs(F_TXI, F_TXI, S(16)), C_INK2, 1); }
            rich(MOTIFS[mm].essence, x, y + S(84), nb.w * 0.62f, rs(F_TXS, F_TXSI, S(15)), C_INK2, 1);
            {   char buf[160]; snprintf(buf, sizeof buf, "Heard in %d of %d pieces.  Its question: %s", (int)cnt, NPIECES, MOTIFS[mm].question);
                rich(buf, nb.x + nb.w * 0.68f, y + S(6), nb.w * 0.29f, rs(F_TXS, F_TXSI, S(15)), C_INK2, 1); }
            d_text(F_TXSI, nb.x + nb.w * 0.68f, nb.y + nb.h - S(30), "click the label for its etude →", MOTIF_COL[mm]);
        } else {
            /* the coda: the one picture behind both chapters */
            float x = nb.x + S(24), y = nb.y + S(18), px = S(22);
            d_text_spaced(F_CAP, x, y, "THE BIG PICTURE IN THREE LINES", S(1.4f), C_MUTED);
            y += S(30);
            {   const char *L[3] = {
                    "f(x+ct)-f(-x+ct)\\;=\\;2K\\,\\col{1}{\\sin\\delta}\\,\\col{1}{\\cos\\gamma}\\qquad\\text{the string (1.18): nodes in space}",
                    "\\sin\\alpha+\\sin\\beta\\;=\\;2\\,\\col{1}{\\cos\\delta}\\,\\col{1}{\\sin\\gamma}\\qquad\\text{beats (2.1): swells in time}",
                    "e^{i\\alpha}+e^{i\\beta}\\;=\\;\\col{2}{e^{i\\gamma}}\\cdot\\col{3}{\\left(e^{i\\delta}+e^{-i\\delta}\\right)}\\qquad\\text{the one picture: a turn times a mirror pair}" };
                int k;
                for (k = 0; k < 3; k++) { float fp = tex_fit(L[k], px, nb.w * 0.64f); TexBox b = tex_measure(L[k], fp); tex_draw(L[k], x, y + b.asc, fp, C_INK); y += b.asc + b.desc + S(16); }
            }
            rich("[1|$\\gamma$] is the middle angle and [1|$\\delta$] the half-gap: *Split*. Factoring out [2|$e^{i\\gamma}$] is a *Turn*; a vector plus its [3|mirror image] is real. The nails of Chapter 1 are the same mirrors, and two of them make the *Loop* that makes a tone.",
                 x, y + S(2), nb.w * 0.64f, rs(F_TXS, F_TXSI, S(15)), C_INK2, 1);
            {   float cx = nb.x + nb.w * 0.70f, cy = nb.y + S(18);
                d_text_spaced(F_CAP, cx, cy, "HOW TO USE IT", S(1.4f), C_MUTED); cy += S(26);
                cy += font_wrap(F_TXS, cx, cy, nb.w * 0.27f, "Pick any column: its derivation is a short phrase of moves. In Perform mode you name each move before it is shown.", C_INK2, 1) + S(8);
                font_wrap(F_TXS, cx, cy, nb.w * 0.27f, "Every cited result is a link, so you can follow any formula back to the ground.", C_INK2, 1);
            }
        }
    }
}

/* =========================================================== motif keys */
static int keyboard(Rect r, int mode_perform, int correct, int *pressed) {
    int m, hit = -1; float kw = (r.w - S(8) * (MO_COUNT - 1)) / MO_COUNT;
    *pressed = -1;
    for (m = 0; m < MO_COUNT; m++) {
        int k = m == MO_GROUND ? 0 : m;       /* key digit: 0 ground, 1..7 motifs */
        float x = r.x + m * (kw + S(8));
        Rect b = rect(x, r.y, kw, r.h); int hv = hover(b);
        float shake = (A.shake > 0 && A.shake_key == m) ? sinf(A.shake * 60) * A.shake * S(10) : 0;
        Color c = MOTIF_COL[m];
        float fl = (A.flash > 0 && A.flash_key == m) ? A.flash : 0;
        b.x += shake;
        d_rrect(b, S(8), fl > 0 ? cmix(C_CARD, c, 0.35f * fl) : hv ? cmix(C_CARD, c, 0.08f) : C_CARD);
        d_rrect_line(b, S(8), hv ? S(1.6f) : 1, calpha(c, hv ? 0.9f : 0.35f));
        if (mode_perform && correct == m && A.hint_shown >= 2) d_rrect_line(inset(b, -S(3)), S(10), S(2), calpha(c, 0.5f + 0.5f * sinf((float)A.t * 6)));
        if (A.wrong_key == m && A.shake <= 0 && mode_perform) d_line(b.x + S(10), b.y + b.h - S(6), b.x + b.w - S(10), b.y + b.h - S(6), S(1.5f), calpha(MOTIF_COL[MO_MIRROR], 0.5f));
        motif_glyph(m, b.x + S(26), b.y + b.h / 2, S(26), S(2), c);
        d_text(F_UIB, b.x + S(48), b.y + S(12), m == MO_GROUND ? "Ground" : MOTIFS[m].name, cmix(c, C_INK, 0.3f));
        {   char kb[4]; snprintf(kb, sizeof kb, "%d", k); d_text_right(F_XS, b.x + b.w - S(10), b.y + S(10), kb, C_FAINT); }
        {   static const char *tag[MO_COUNT] = { "what is given", "middle \u00b1 half-gap", "rotate and stretch", "reflect; pair with image",
                                                  "once around", "freeze one variable", "Re and Im apart", "compare with length" };
            if (font_width(F_XS, tag[m], -1) < b.w - S(56)) d_text(F_XS, b.x + S(48), b.y + S(36), tag[m], C_MUTED); }
        if (click(b)) *pressed = m;
        if (hv) hit = m;
    }
    {   int i; for (i = 0; i < A.in.ntext; i++) { unsigned ch = A.in.text[i]; if (ch >= '0' && ch <= '7') *pressed = ch == '0' ? MO_GROUND : (int)(ch - '0'); } }
    return hit;
}

/* =========================================================== piece view */
static float draw_step(const Piece *pc, int p, int i, float x, float y, float w, int draw, int *ref_click) {
    const Step *s = &pc->steps[i];
    int revealed = !A.perform || i < g_revealed[p];
    int current = A.perform && i == g_revealed[p];
    float y0 = y, tx = x + S(42), tw = w - S(46);
    Color mc = MOTIF_COL[s->motif];
    *ref_click = 0;
    if (!revealed && !current) {
        if (draw) {
            d_ring(x + S(16), y + S(14), S(12), 1, C_HAIR);
            d_dash(tx, y + S(16), tx + tw * 0.6f, y + S(16), 1, S(4), S(5), C_HAIR);
        }
        return S(36);
    }
    y += S(4);
    if (current) {
        float h;
        if (draw) {
            d_ring(x + S(16), y + S(12), S(13), S(1.6f), calpha(C_INK, 0.5f + 0.3f * sinf((float)A.t * 3)));
            d_text_center(F_UIB, x + S(16), y + S(12) - font_line(F_UIB) / 2, "?", C_INK);
        }
        h = rich(s->cue, tx, y, tw, rs(F_TX, F_TXI, S(17)), C_INK, draw);
        y += h + S(6);
        if (draw) d_text(F_TXSI, tx, y, "Which move? Press its key below (0–7), or Enter to reveal.", C_MUTED);
        y += S(22);
        if (A.hint_shown >= 1 && A.wrong_key >= 0) {
            char buf[512];
            snprintf(buf, sizeof buf, "Not *%s*: that would %s. Ask instead: %s", A.wrong_key == MO_GROUND ? "Ground" : MOTIFS[A.wrong_key].name, MOTIFS[A.wrong_key].verb, MOTIFS[s->motif].question);
            y += rich(buf, tx, y, tw, rs(F_TXS, F_TXSI, S(15)), MOTIF_COL[MO_MIRROR], draw) + S(6);
        }
        return y - y0 + S(10);
    }
    if (draw) {
        d_circle(x + S(16), y + S(14), S(14), calpha(mc, 0.12f));
        motif_glyph(s->motif, x + S(16), y + S(14), S(20), S(1.6f), mc);
    }
    {   float h = rich(s->cue, tx, y, tw, rs(F_TXS, F_TXSI, S(14.5f)), C_MUTED, draw);
        y += h + S(4); }
    {   float px = tex_fit(s->tex, S(21), tw); TexBox b = tex_measure(s->tex, px);
        if (draw) tex_draw(s->tex, tx, y + b.asc + S(2), px, C_INK);
        y += b.asc + b.desc + S(10); }
    if (s->why) y += rich(s->why, tx, y, tw, rs(F_TXS, F_TXSI, S(14.5f)), C_INK2, draw) + S(4);
    {   int q = step_ref(p, i);
        if (q >= 0) {
            char buf[80]; int hit = 0;
            snprintf(buf, sizeof buf, "uses %s →", short_label(q));
            if (draw) chip(tx, y + S(2), buf, MOTIF_COL[MO_TURN], 0, &hit);
            if (hit) *ref_click = q + 1;
            y += S(30);
        }
    }
    return y - y0 + S(12);
}

/* every ancestor of p, deepest first: the piece built from scratch */
static int lineage(int p, int *out, int cap) {
    static int mark[160]; int stack[160], sp = 0, n = 0, k, i;
    memset(mark, 0, sizeof mark);
    /* depth-first post-order over needs and step refs */
    {   int it[160]; memset(it, 0, sizeof it);
        stack[sp++] = p; mark[p] = 1;
        while (sp > 0) {
            int c = stack[sp - 1], next = -1;
            while (it[c] < 6 + 12 && next < 0) {
                int j = it[c]++, q = j < 6 ? piece_need(c, j) : step_ref(c, j - 6);
                if (q >= 0 && !mark[q]) next = q;
            }
            if (next >= 0) { mark[next] = 1; stack[sp++] = next; }
            else { sp--; if (c != p && n < cap) out[n++] = c; }
        }
    }
    (void)k; (void)i;
    return n;
}

static void piece_view(Rect r) {
    int p = CUR, i, pressed, hit, refc;
    const Piece *pc;
    Rect stage_r, right, kb;
    float x, y, w;
    if (p < 0) { A.view = V_SCORE; return; }
    pc = &PIECES[p];
    kb = rect(r.x + S(24), r.y + r.h - S(86), r.w - S(48), S(68));
    stage_r = rect(r.x + S(24), r.y + S(20), r.w * 0.5f - S(30), r.h - S(130));
    right = rect(stage_r.x + stage_r.w + S(28), r.y, r.x + r.w - (stage_r.x + stage_r.w + S(28)) - S(24), r.h - S(106));

    /* stage */
    d_rrect(stage_r, S(10), C_CARD); d_rrect_line(stage_r, S(10), 1, C_HAIR);
    A.st.sound = !synth_muted();
    if (stage_draw(&A.st, inset(stage_r, S(2)), &A.in, A.dt, 1)) A.want_hand = 1;

    /* right column, scrolled */
    clip_push(right);
    x = right.x; w = right.w; y = right.y + S(20) - A.scroll;
    {   char head[160];
        snprintf(head, sizeof head, "%s%s   ·   %s   ·   §%s %s", kind_name(pc->kind), pc->essential ? " ♣" : "", pc->label, SECTIONS[pc->section].num, SECTIONS[pc->section].title);
        d_text_spaced(F_CAP, x, y, head, S(1.2f), C_MUTED); y += S(20);
        font_draw(F_H2, x, y, pc->name, -1, C_INK); y += S(42);
        {   float px = tex_fit(pc->statement, S(27), w - S(10)); TexBox b = tex_measure(pc->statement, px);
            d_rrect(rect(x - S(10), y - S(6), w + S(10), b.asc + b.desc + S(22)), S(8), calpha(C_WASH, 0.6f));
            tex_draw(pc->statement, x + S(4), y + S(4) + b.asc, px, C_INK); y += b.asc + b.desc + S(30); }
        y += rich(pc->gist, x, y, w, rs(F_TX, F_TXI, S(17)), C_INK2, 1) + S(14);
    }
    /* from scratch: the lineage, folded by default */
    {   int lin[96], n = lineage(p, lin, 96), k, ng = 0;
        for (k = 0; k < n; k++) if (PIECES[lin[k]].kind == K_GROUND || PIECES[lin[k]].kind == K_DEF) ng++;
        if (n > 0) {
            char buf[160]; Rect tb;
            snprintf(buf, sizeof buf, "%s  built on %d earlier piece%s, down to %d given fact%s", A.lineage_open ? "\u25be" : "\u25b8", n, n == 1 ? "" : "s", ng, ng == 1 ? "" : "s");
            tb = rect(x - S(4), y - S(2), font_width(F_S, buf, -1) + S(12), S(22));
            if (hover(tb)) d_rrect(tb, S(5), C_WASH);
            d_text(F_S, x, y, buf, C_MUTED);
            if (click(tb)) A.lineage_open = !A.lineage_open;
            y += S(26);
            if (A.lineage_open) {
                float xx = x, yy = y;
                for (k = 0; k < n; k++) {
                    const char *lb = short_label(lin[k]); float cw = font_width(F_S, lb, -1) + S(16);
                    int h2 = 0, gr = PIECES[lin[k]].kind == K_GROUND || PIECES[lin[k]].kind == K_DEF;
                    if (xx + cw > x + w) { xx = x; yy += S(28); }
                    chip(xx, yy, lb, gr ? MOTIF_COL[MO_GROUND] : MOTIF_COL[MO_TURN], gr, &h2);
                    if (h2) { open_piece(lin[k], 1); clip_pop(); return; }
                    xx += cw + S(6);
                }
                y = yy + S(34);
                d_text(F_XS, x, y - S(4), "deepest first; filled chips are the ground", C_FAINT); y += S(16);
            }
        }
    }
    d_line(x, y, x + w, y, 1, C_HAIR); y += S(16);

    if (!pc->nsteps) {
        motif_glyph(MO_GROUND, x + S(16), y + S(14), S(22), S(1.8f), MOTIF_COL[MO_GROUND]);
        rich(pc->kind == K_DEF ? "A *definition*: nothing to derive. It is part of the ground the other pieces stand on." :
             "*Ground*: taken as given. Every derivation rests on a few such facts, the drone under the music.",
             x + S(42), y + S(4), w - S(42), rs(F_TX, F_TXI, S(17)), C_INK2, 1);
        y += S(70);
    } else {
        for (i = 0; i < pc->nsteps; i++) {
            float h = draw_step(pc, p, i, x, y, w, 0, &refc);
            if (A.scroll_to_step && i == A.sel_step) {   /* keep the chosen step in view */
                float top = y + A.scroll - right.y, bot = top + h;
                if (top < A.scroll_target + S(20)) A.scroll_target = top - S(20);
                if (bot > A.scroll_target + right.h - S(20)) A.scroll_target = bot - right.h + S(20);
                A.scroll_to_step = 0;
            }
            Rect sr = rect(x - S(8), y, w + S(16), h - S(4));
            int revealed = !A.perform || i < g_revealed[p];
            if (revealed && i == A.sel_step) { d_rrect(sr, S(8), calpha(MOTIF_COL[pc->steps[i].motif], 0.07f)); d_rect(rect(sr.x, sr.y + S(6), S(3), sr.h - S(12)), MOTIF_COL[pc->steps[i].motif]); }
            draw_step(pc, p, i, x, y, w, 1, &refc);
            if (refc) { open_piece(refc - 1, 1); clip_pop(); return; }
            if (revealed && click(sr)) { A.sel_step = i; stage_for_piece(); }
            if (revealed && i == A.sel_step && A.kbd) d_rect(rect(sr.x - S(6), sr.y + S(4), S(2), sr.h - S(8)), C_INK);
            y += h;
            if (A.perform && i == g_revealed[p]) break;
        }
        if (A.perform && g_revealed[p] < pc->nsteps) {
            for (i = g_revealed[p] + 1; i < pc->nsteps; i++) { d_ring(x + S(16), y + S(14), S(12), 1, C_HAIR); d_dash(x + S(42), y + S(16), x + w * 0.55f, y + S(16), 1, S(4), S(5), C_HAIR); y += S(34); }
        }
        if (!A.perform || g_revealed[p] >= pc->nsteps) {   /* coda: the chord and its relatives */
            int m, k, n = 0, same[24], ns = 0;
            y += S(6);
            d_line(x, y, x + w, y, 1, C_HAIR); y += S(14);
            d_text_spaced(F_CAP, x, y, "THE CHORD OF THIS PIECE", S(1.3f), C_MUTED); y += S(22);
            for (m = 0; m < MO_COUNT; m++) if (PIECE_MOTIFS[p][m] && pc->nsteps) {
                char t[40]; snprintf(t, sizeof t, "%s ×%d", m == MO_GROUND ? "Ground" : MOTIFS[m].name, PIECE_MOTIFS[p][m]);
                motif_glyph(m, x + S(9) + n * S(124), y + S(9), S(16), S(1.4f), MOTIF_COL[m]);
                d_text(F_S, x + S(22) + n * S(124), y + S(1), t, cmix(MOTIF_COL[m], C_INK, 0.3f));
                n++;
                if (n == 4 && m < MO_COUNT - 1) { n = 0; y += S(24); }
            }
            y += S(30);
            /* pieces that share at least the same set of non-ground motifs */
            for (k = 0; k < NPIECES && ns < 24; k++) {
                int ok = k != p && PIECES[k].nsteps > 0, any = 0;
                for (m = 1; m < MO_COUNT && ok; m++) { if ((PIECE_MOTIFS[k][m] > 0) != (PIECE_MOTIFS[p][m] > 0)) ok = 0; if (PIECE_MOTIFS[p][m]) any = 1; }
                if (ok && any && PIECES[k].steps != pc->steps) same[ns++] = k;
            }
            if (ns) {
                float xx = x + S(128);
                d_text(F_XS, x, y + S(4), "same chord as", C_MUTED);
                for (k = 0; k < ns; k++) { int h2 = 0; float cw = font_width(F_S, short_label(same[k]), -1) + S(16);
                    if (xx + cw > x + w) { xx = x + S(128); y += S(28); }
                    chip(xx, y, short_label(same[k]), MOTIF_COL[MO_LOOP], 0, &h2); if (h2) { open_piece(same[k], 1); clip_pop(); return; } xx += cw + S(6); }
                y += S(36);
            }
            {   int users[64], nu = piece_users(p, users, 64);
                if (nu) {
                    float xx = x + S(128);
                    d_text(F_XS, x, y + S(4), "used later by", C_MUTED);
                    for (k = 0; k < nu; k++) { int h2 = 0; float cw = font_width(F_S, short_label(users[k]), -1) + S(16);
                        if (xx + cw > x + w) { xx = x + S(128); y += S(28); }
                        chip(xx, y, short_label(users[k]), MOTIF_COL[MO_TURN], 0, &h2); if (h2) { open_piece(users[k], 1); clip_pop(); return; } xx += cw + S(6); }
                    y += S(36);
                }
            }
        }
    }
    if (pc->echo && (!A.perform || g_revealed[p] >= pc->nsteps || !pc->nsteps)) {
        float h = rich(pc->echo, x + S(42), y + S(10), w - S(52), rs(F_TXS, F_TXSI, S(15)), C_INK2, 0);
        d_rrect(rect(x - S(4), y, w + S(4), h + S(20)), S(8), calpha(MOTIF_COL[MO_LOOP], 0.07f));
        d_text(F_H3, x + S(12), y + S(6), "♪", MOTIF_COL[MO_LOOP]);
        rich(pc->echo, x + S(42), y + S(10), w - S(52), rs(F_TXS, F_TXSI, S(15)), C_INK2, 1);
        y += h + S(30);
    }
    if (pc->note) {
        float h = rich(pc->note, x + S(42), y + S(10), w - S(52), rs(F_TXS, F_TXSI, S(15)), C_INK2, 0);
        int err = !strncmp(pc->note, "Erratum", 7) || !strncmp(pc->note, "Typo", 4);
        Color c = err ? MOTIF_COL[MO_MIRROR] : MOTIF_COL[MO_GROUND];
        d_rrect(rect(x - S(4), y, w + S(4), h + S(20)), S(8), calpha(c, 0.07f));
        d_text(F_H3, x + S(12), y + S(4), err ? "*" : "§", c);
        rich(pc->note, x + S(42), y + S(10), w - S(52), rs(F_TXS, F_TXSI, S(15)), C_INK2, 1);
        y += h + S(30);
    }
    A.content_h = y + A.scroll - right.y + S(40);
    clip_pop();
    if (A.content_h > right.h) {   /* scroll indicator */
        float th = right.h * right.h / A.content_h, ty = right.y + (right.h - th) * (A.scroll / (A.content_h - right.h));
        d_rrect(rect(right.x + right.w + S(10), ty, S(3), th), S(2), C_HAIR);
    }
    if (hover(right) && A.in.wheel != 0) A.scroll_target -= A.in.wheel * S(70);
    {   float mx = A.content_h - right.h; if (mx < 0) mx = 0;
        if (A.scroll_target > mx) A.scroll_target = mx; if (A.scroll_target < 0) A.scroll_target = 0; }
    A.scroll += (A.scroll_target - A.scroll) * 0.25f;

    /* keyboard */
    {   int correct = (A.perform && pc->nsteps && g_revealed[p] < pc->nsteps) ? pc->steps[g_revealed[p]].motif : -1;
        hit = keyboard(kb, A.perform, correct, &pressed);
        if (hit >= 0) {
            const char *q = MOTIFS[hit].question; float tw = font_width(F_TXSI, q, -1) + S(20);
            Rect tip = rect(A.in.mx - tw / 2, kb.y - S(36), tw, S(28));
            if (tip.x < r.x + S(8)) tip.x = r.x + S(8); if (tip.x + tip.w > r.x + r.w - S(8)) tip.x = r.x + r.w - S(8) - tip.w;
            d_rrect(tip, S(6), C_INK); d_text(F_TXSI, tip.x + S(10), tip.y + (tip.h - font_line(F_TXSI)) / 2, q, C_PAPER);
        }
        if (pressed >= 0) {
            if (correct >= 0) {
                if (pressed == correct) {
                    g_revealed[p]++; A.sel_step = g_revealed[p] - 1; A.flash_key = pressed; A.flash = 1; A.hint_shown = 0; A.wrong_key = -1;
                    play_motif(pressed, 0.16f); stage_for_piece();
                    if (g_revealed[p] >= pc->nsteps) play_chord(p);
                    {   float target = A.content_h - right.h * 0.5f; if (target > A.scroll_target) A.scroll_target = target; }
                } else {
                    A.shake = 0.35f; A.shake_key = pressed; A.wrong_key = pressed; A.hint_shown++; g_wrong[p]++;
                    synth_pluck(110.0f * MOTIFS[pressed].harmonic * 1.06f, 0.08f, 0.4f);
                }
            } else {
                /* read mode: jump to the next step that uses this motif */
                int k, start = A.sel_step;
                for (k = 1; k <= pc->nsteps; k++) { int s2 = (start + k) % (pc->nsteps ? pc->nsteps : 1); if (pc->nsteps && pc->steps[s2].motif == pressed) { A.sel_step = s2; stage_for_piece(); break; } }
                play_motif(pressed, 0.12f);
            }
        }
        if (A.perform && correct >= 0 && A.in.key_pressed[KEY_ENTER]) {
            A.flash_key = correct; A.flash = 1; g_revealed[p]++; A.sel_step = g_revealed[p] - 1; A.hint_shown = 0; A.wrong_key = -1;
            play_motif(correct, 0.12f); stage_for_piece();
        }
    }
    /* keys: walk the steps, follow references, move between pieces */
    if (pc->nsteps) {
        int lim = A.perform ? g_revealed[p] - 1 : pc->nsteps - 1, old = A.sel_step;
        if (A.in.key_pressed[KEY_DOWN] || A.in.key_pressed[KEY_RIGHT] || (!A.perform && A.in.key_pressed[KEY_SPACE])) { if (A.sel_step < lim) A.sel_step++; }
        if (A.in.key_pressed[KEY_UP] || A.in.key_pressed[KEY_LEFT]) { if (A.sel_step > 0) A.sel_step--; }
        if (A.in.key_pressed[KEY_HOME] && lim >= 0) A.sel_step = 0;
        if (A.in.key_pressed[KEY_END] && lim >= 0) A.sel_step = lim;
        if (A.sel_step != old) { A.kbd = 1; A.scroll_to_step = 1; stage_for_piece(); }
    }
    if (A.in.key_pressed[KEY_PGDN]) A.scroll_target += right.h * 0.8f;
    if (A.in.key_pressed[KEY_PGUP]) A.scroll_target -= right.h * 0.8f;
    {   int k2;
        for (k2 = 0; k2 < A.in.ntext; k2++) {
            unsigned c = A.in.text[k2];
            if (c == ']' && p + 1 < NPIECES) { open_piece(p + 1, 0); return; }
            if (c == '[' && p > 0) { open_piece(p - 1, 0); return; }
            if ((c == 'u' || c == 'U') && A.sel_step >= 0) { int q = step_ref(p, A.sel_step); if (q >= 0 && (!A.perform || A.sel_step < g_revealed[p])) { open_piece(q, 1); return; } }
            if (c == 'l' || c == 'L') A.lineage_open = !A.lineage_open;
            if ((c == 'r' || c == 'R') && A.perform && pc->nsteps) { g_revealed[p] = 0; A.sel_step = -1; A.hint_shown = 0; A.wrong_key = -1; A.scroll_target = 0; stage_for_piece(); }
        }
        if (!A.perform && A.in.key_pressed[KEY_ENTER] && A.sel_step >= 0) { int q = step_ref(p, A.sel_step); if (q >= 0) { open_piece(q, 1); return; } }
    }
}

/* ============================================================== etudes */
static void etude_view(Rect r) {
    int m, i, k;
    Rect list = rect(r.x + S(24), r.y + S(20), S(250), r.h - S(40));
    Rect stage_r = rect(list.x + list.w + S(24), r.y + S(20), (r.w - list.w - S(96)) * 0.55f, r.h - S(40));
    Rect right = rect(stage_r.x + stage_r.w + S(24), r.y + S(20), r.x + r.w - (stage_r.x + stage_r.w + S(24)) - S(24), r.h - S(40));
    float y = list.y;
    for (m = 0; m < MO_COUNT; m++) {
        int mm = m == 0 ? MO_GROUND : m;
        Rect b = rect(list.x, y, list.w, S(58)); int on = A.etude == mm, hv = hover(b);
        if (on || hv) d_rrect(b, S(8), on ? calpha(MOTIF_COL[mm], 0.12f) : C_WASH);
        motif_glyph(mm, b.x + S(26), b.y + b.h / 2, S(28), S(2), MOTIF_COL[mm]);
        d_text(F_UIB, b.x + S(54), b.y + S(10), mm == MO_GROUND ? "Ground" : MOTIFS[mm].name, cmix(MOTIF_COL[mm], C_INK, 0.3f));
        {   int cnt = 0; char t[32]; for (i = 0; i < NPIECES; i++) for (k = 0; k < PIECES[i].nsteps; k++) if (PIECES[i].steps[k].motif == mm) cnt++;
            snprintf(t, sizeof t, "%d moves", cnt); d_text(F_XS, b.x + S(54), b.y + S(32), t, C_MUTED); }
        if (click(b)) { open_etude(mm); play_motif(mm, 0.14f); }
        y += S(64);
    }
    d_rrect(stage_r, S(10), C_CARD); d_rrect_line(stage_r, S(10), 1, C_HAIR);
    A.st.sound = !synth_muted();
    if (stage_draw(&A.st, inset(stage_r, S(2)), &A.in, A.dt, 1)) A.want_hand = 1;

    clip_push(right);
    {   const MotifInfo *mi = &MOTIFS[A.etude]; float x = right.x, w = right.w;
        y = right.y + S(6) - A.escroll;
        motif_glyph(A.etude, x + S(20), y + S(22), S(36), S(2.6f), MOTIF_COL[A.etude]);
        font_draw(F_H1I, x + S(52), y - S(8), A.etude == MO_GROUND ? "Ground" : mi->name, -1, MOTIF_COL[A.etude]);
        y += S(56);
        {   char buf[300]; snprintf(buf, sizeof buf, "to %s", mi->verb); y += rich(buf, x, y, w, rs(F_TXI, F_TXI, S(17)), C_INK2, 1) + S(12); }
        y += rich(mi->essence, x, y, w, rs(F_TX, F_TXI, S(17)), C_INK, 1) + S(14);
        d_rrect(rect(x - S(4), y, w + S(4), S(10) + rich(mi->question, x + S(12), y + S(8), w - S(20), rs(F_TXI, F_TXI, S(17)), C_INK, 0) + S(10)), S(8), calpha(MOTIF_COL[A.etude], 0.08f));
        y += rich(mi->question, x + S(12), y + S(8), w - S(20), rs(F_TXI, F_TXI, S(17)), C_INK, 1) + S(34);
        d_text_spaced(F_CAP, x, y, "EVERY TIME IT SOUNDS", S(1.3f), C_MUTED); y += S(22);
        {   int ch, occ = 0, nocc = 0;
            for (i = 0; i < NPIECES; i++) for (k = 0; k < PIECES[i].nsteps; k++) if (PIECES[i].steps[k].motif == A.etude) nocc++;
            if (A.etude == MO_GROUND) for (i = 0; i < NPIECES; i++) if (PIECES[i].kind == K_GROUND || PIECES[i].kind == K_DEF) nocc++;
            if (A.in.key_pressed[KEY_RIGHT]) { A.ecur++; A.kbd = 1; }
            if (A.in.key_pressed[KEY_LEFT]) { A.ecur--; A.kbd = 1; }
            if (A.ecur >= nocc) A.ecur = nocc - 1; if (A.ecur < 0) A.ecur = 0;
            for (ch = 1; ch <= 2; ch++) {
                float xx = x; int any = 0;
                for (i = 0; i < NPIECES; i++) {
                    if (PIECES[i].chapter != ch) continue;
                    for (k = 0; k < PIECES[i].nsteps; k++) if (PIECES[i].steps[k].motif == A.etude) {
                        char lb[48]; float cw; int h2 = 0;
                        if (!any) { d_text(F_XS, x, y, ch == 1 ? "Chapter 1" : "Chapter 2", C_MUTED); y += S(18); any = 1; }
                        snprintf(lb, sizeof lb, "%s · %d", short_label(i), k + 1);
                        cw = font_width(F_S, lb, -1) + S(16);
                        if (xx + cw > x + w) { xx = x; y += S(28); }
                        chip(xx, y, lb, MOTIF_COL[A.etude], PIECES[i].kind == K_PROBLEM ? 0 : 1, &h2);
                        if (A.kbd && occ == A.ecur) {
                            d_rrect_line(rect(xx - S(3), y - S(3), cw + S(6), S(28)), S(13), S(1.8f), C_INK);
                            if (y < right.y + S(20) - 0) A.escroll_target -= S(60); else if (y > right.y + right.h - S(40)) A.escroll_target += S(60);
                            if (A.in.key_pressed[KEY_ENTER]) h2 = 1;
                        }
                        occ++;
                        if (h2) { clip_pop(); A.perform = 0; open_piece(i, 0); A.sel_step = k; A.scroll_to_step = 1; stage_for_piece(); return; }
                        xx += cw + S(6);
                    }
                }
                if (any) y += S(38);
            }
            if (A.etude == MO_GROUND) {
                float xx = x;
                d_text(F_XS, x, y, "the given facts", C_MUTED); y += S(18);
                for (i = 0; i < NPIECES; i++) if (PIECES[i].kind == K_GROUND || PIECES[i].kind == K_DEF) {
                    int h2 = 0; float cw = font_width(F_S, short_label(i), -1) + S(16);
                    if (xx + cw > x + w) { xx = x; y += S(28); }
                    chip(xx, y, short_label(i), MOTIF_COL[MO_GROUND], 1, &h2);
                    if (A.kbd && occ == A.ecur) { d_rrect_line(rect(xx - S(3), y - S(3), cw + S(6), S(28)), S(13), S(1.8f), C_INK); if (A.in.key_pressed[KEY_ENTER]) h2 = 1; }
                    occ++;
                    if (h2) { clip_pop(); open_piece(i, 0); return; }
                    xx += cw + S(6);
                }
                y += S(38);
            }
        }
        A.econtent_h = y + A.escroll - right.y;
    }
    clip_pop();
    if (hover(right) && A.in.wheel != 0) A.escroll_target -= A.in.wheel * S(70);
    {   float mx = A.econtent_h - right.h; if (mx < 0) mx = 0; if (A.escroll_target > mx) A.escroll_target = mx; if (A.escroll_target < 0) A.escroll_target = 0; }
    A.escroll += (A.escroll_target - A.escroll) * 0.25f;
}

/* ================================================================= help */
static void help_overlay(Rect r) {
    static const char *rows[][2] = {
        { "#", "Anywhere" },
        { "Alt+←  /  Alt+→", "back / forward to where you were (also Backspace, the mouse's side buttons, and ← → in the top bar)" },
        { "Ctrl+K  or  /", "go to any result, exercise or motif by typing its number or name" },
        { "S   E", "the Score / the Etudes" },
        { "Esc", "close this, or go up to the Score" },
        { "M", "sound on / off" },
        { "F1  or  ?", "this help" },
        { "#", "Score" },
        { "← →   Home End", "move along the columns (Shift: jump a section)" },
        { "↑ ↓   or 0–7", "choose a motif; then ← → jump to the next column that uses it" },
        { "Enter", "derive the chosen column" },
        { "Space", "play the score" },
        { "#", "Piece" },
        { "0–7", "Perform: name the next move (0 = Ground). Read: next step with that motif" },
        { "Enter", "Perform: reveal the step. Read: follow the step's reference" },
        { "↑ ↓   Home End", "walk through the steps" },
        { "U", "follow the chosen step's reference (\"uses …\")" },
        { "[   ]", "previous / next piece in book order" },
        { "Tab", "switch Perform and Read" },
        { "L   R", "show the lineage / reset your progress on this piece" },
        { "PgUp PgDn", "scroll the text" },
        { "#", "Etudes" },
        { "↑ ↓   or 0–7", "choose a motif" },
        { "← →   Enter", "walk through its occurrences / open one" },
    };
    int n = (int)(sizeof rows / sizeof rows[0]), i;
    float colw = S(450), bw = colw * 2 + S(70), bh = S(560);
    Rect b = rect(r.x + r.w / 2 - bw / 2, r.y + S(24), bw, bh);
    float x = b.x + S(30), y = b.y + S(24), y0;
    if (b.x < r.x + S(10)) { b.x = r.x + S(10); b.w = r.w - S(20); colw = (b.w - S(70)) / 2; x = b.x + S(30); }
    d_rect(r, calpha(C_PAPER, 0.8f));
    d_rrect(b, S(12), C_CARD); d_rrect_line(b, S(12), 1, C_HAIR);
    font_draw(F_H2I, x, y, "Keys", -1, C_INK);
    d_text(F_XS, b.x + b.w - S(30) - font_width(F_XS, "Esc, F1 or a click closes this", -1), y + S(10), "Esc, F1 or a click closes this", C_MUTED);
    y += S(46); y0 = y;
    for (i = 0; i < n; i++) {
        if (i == 12) { x += colw + S(30); y = y0; }
        if (rows[i][0][0] == '#') { if (y > y0) y += S(6); d_text_spaced(F_CAP, x, y, rows[i][1], S(1.4f), C_MUTED); y += S(20); continue; }
        d_text(F_UIB, x, y, rows[i][0], C_INK);
        y += font_wrap(F_TXS, x + S(150), y, colw - S(150), rows[i][1], C_INK2, 1) + S(5);
    }
    font_wrap(F_TXSI, b.x + S(30), b.y + b.h - S(46), b.w - S(60), "Each motif sounds one harmonic of the string from Chapter 1: Ground is the fundamental, Split the 2nd harmonic, and so on up to Shadow, the 8th.", C_MUTED, 1);
    if (A.help_was_open && A.clicked && !A.consumed) { A.help = 0; A.consumed = 1; }
}

/* ============================================================= progress
   How far you have performed each piece, kept in a small text file. */
static void progress_path(char *out, int cap) {
    const char *d = getenv("APPDATA");
#ifdef _WIN32
    if (d) { snprintf(out, (size_t)cap, "%s\\leitmotif-progress.txt", d); return; }
#else
    d = getenv("HOME");
    if (d) { snprintf(out, (size_t)cap, "%s/.leitmotif-progress", d); return; }
#endif
    snprintf(out, (size_t)cap, "leitmotif-progress.txt");
}
static void progress_load(void) {
    char path[600], id[64]; int n; FILE *f;
    progress_path(path, sizeof path);
    f = fopen(path, "r");
    if (!f) return;
    while (fscanf(f, "%63s %d", id, &n) == 2) {
        int p = piece_find(id);
        if (p >= 0 && n >= 0 && n <= PIECES[p].nsteps) g_revealed[p] = n;
    }
    fclose(f);
}
static void progress_save(void) {
    char path[600]; int i; FILE *f;
    progress_path(path, sizeof path);
    f = fopen(path, "w");
    if (!f) return;
    for (i = 0; i < NPIECES; i++) if (g_revealed[i] > 0) fprintf(f, "%s %d\n", PIECES[i].id, g_revealed[i]);
    fclose(f);
}

/* ============================================================ self-test */
static void screenshot(const char *name) {
    int w = A.in.win_w, h = A.in.win_h, y; char path[512]; FILE *f;
    unsigned char *px = (unsigned char *)mem_alloc((size_t)w * h * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    snprintf(path, sizeof path, "%s/%s.ppm", A.shot_dir, name);
    f = fopen(path, "wb");
    if (f) { fprintf(f, "P6\n%d %d\n255\n", w, h); for (y = h - 1; y >= 0; y--) fwrite(px + (size_t)y * w * 3, 1, (size_t)w * 3, f); fclose(f); }
    mem_free(px);
}

static int g_fail;
static void check_content(void) {
    int i, k, bad = content_init();
    if (bad) { printf("FAIL: %d unresolved references\n", bad); g_fail = 1; }
    tex_unknown = 0;
    for (i = 0; i < NPIECES; i++) {
        const Piece *pc = &PIECES[i]; int before = tex_unknown;
        tex_measure(pc->statement, 20);
        for (k = 0; k < pc->nsteps; k++) tex_measure(pc->steps[k].tex, 20);
        if (pc->nsteps > 12) { printf("FAIL: %s has more than 12 steps\n", pc->id); g_fail = 1; }
        if (tex_unknown != before) { printf("FAIL: unknown TeX command in %s\n", pc->id); g_fail = 1; }
        for (k = i + 1; k < NPIECES; k++) if (!strcmp(pc->id, PIECES[k].id)) { printf("FAIL: duplicate id %s\n", pc->id); g_fail = 1; }
    }
    {   int nsteps = 0, cnt[MO_COUNT] = {0}; for (i = 0; i < NPIECES; i++) for (k = 0; k < PIECES[i].nsteps; k++) { nsteps++; cnt[PIECES[i].steps[k].motif]++; }
        printf("pieces=%d steps=%d  ground=%d split=%d turn=%d mirror=%d loop=%d hold=%d lanes=%d shadow=%d\n", NPIECES, nsteps,
               cnt[0], cnt[1], cnt[2], cnt[3], cnt[4], cnt[5], cnt[6], cnt[7]); }
}

/* drive every view and scene; returns a screenshot name for this frame or NULL */
static const char *selftest_script(int f) {
    static char name[64];
    int phase = f / 8, sub = f % 8;
    A.in.mx = -100; A.in.my = -100;
    if (phase == 0) { if (sub == 7) return "01_score"; return NULL; }
    if (phase == 1) { int c = piece_find("1.13"); A.in.mx = (int)g_cols[c].x; A.in.my = (int)g_staff_y[3]; if (sub == 7) return "02_score_hover"; return NULL; }
    if (phase == 2) { A.in.mx = (int)S(60); A.in.my = (int)g_staff_y[2]; if (sub == 7) return "03_score_motif"; return NULL; }
    if (phase == 3) { if (sub == 0) { A.perform = 1; open_piece(piece_find("2.1"), 0); } if (sub == 7) return "04_piece_perform"; return NULL; }
    if (phase == 4) { if (sub == 0) { A.in.ntext = 1; A.in.text[0] = '3'; } if (sub == 7) return "05_piece_wrong"; return NULL; }
    if (phase == 5) { if (sub == 0) { A.in.ntext = 1; A.in.text[0] = '6'; } if (sub == 2) { A.in.ntext = 1; A.in.text[0] = '1'; } if (sub == 4) { A.in.ntext = 1; A.in.text[0] = '2'; } if (sub == 7) return "06_piece_progress"; return NULL; }
    if (phase == 6) { if (sub == 0) { A.perform = 0; open_piece(piece_find("1.13"), 0); A.sel_step = 2; stage_for_piece(); } if (sub == 7) return "07_piece_read"; return NULL; }
    if (phase == 7) { if (sub == 0) { A.view = V_ETUDE; A.etude = MO_MIRROR; stage_set(&A.st, MOTIFS[A.etude].scene, MOTIFS[A.etude].p); } if (sub == 7) return "08_etude"; return NULL; }
    if (phase == 8) { if (sub == 0) { A.help = 1; A.view = V_SCORE; } if (sub == 7) { return "09_help"; } return NULL; }
    if (phase == 9) { if (sub == 0) A.help = 0; return NULL; }
    /* then every piece in read mode, each with its last step selected */
    {   int i = phase - 10;
        if (i < NPIECES) {
            if (sub == 0) { A.perform = 0; open_piece(i, 0); A.sel_step = PIECES[i].nsteps ? PIECES[i].nsteps - 1 : -1; stage_for_piece(); }
            if (sub == 1) A.scroll_target = 0;
            if (sub == 7) { snprintf(name, sizeof name, "p%02d_%s", i, PIECES[i].id); return name; }
            return NULL;
        }
        i -= NPIECES;
        if (i < MO_COUNT) {
            if (sub == 0) { A.view = V_ETUDE; A.etude = i; stage_set(&A.st, MOTIFS[i].scene, MOTIFS[i].p); }
            if (sub == 7) { snprintf(name, sizeof name, "e%d_%s", i, MOTIFS[i].name); return name; }
            return NULL;
        }
    }
    A.in.quit = 1;
    return NULL;
}

/* =================================================================== main */
static void frame(void) {
    Rect top, body;
    int W = A.in.win_w, H = A.in.win_h;
    g_s = A.in.dpi;
    d_begin_frame(W, H);
    glClearColor(C_PAPER.r, C_PAPER.g, C_PAPER.b, 1); glClear(GL_COLOR_BUFFER_BIT);
    A.clicked = A.in.pressed[MOUSE_L]; A.consumed = 0; A.want_hand = 0;
    if (A.help) A.consumed = 0;
    top = rect(0, 0, (float)W, S(56)); body = rect(0, S(56), (float)W, (float)H - S(56));

    Loc before = loc_now();
    A.help_was_open = A.help; A.nav_jumped = 0;
    if (A.in.mdx || A.in.mdy) { if (A.kbd && (abs(A.in.mdx) + abs(A.in.mdy) > 2)) A.kbd = 0; }
    {   int i, alt = A.in.alt;
        /* overlays take the keyboard */
        if (A.palette) {
            /* handled when drawn, below */
        } else if (A.help) {
            int close = A.in.key_pressed[KEY_ESC] || A.in.key_pressed[KEY_F1];
            for (i = 0; i < A.in.ntext; i++) if (A.in.text[i] == '?') close = 1;
            if (close) A.help = 0;
        } else {
            int back = (alt && A.in.key_pressed[KEY_LEFT]) || A.in.key_pressed[KEY_BACKSPACE] || A.in.pressed[MOUSE_BACK];
            int fwd = (alt && A.in.key_pressed[KEY_RIGHT]) || A.in.pressed[MOUSE_FWD];
            if (A.in.key_pressed[KEY_F1]) A.help = 1;
            if (A.in.ctrl && !alt) for (i = 0; i < A.in.ntext; i++) if (A.in.text[i] == 'k' || A.in.text[i] == 'K' || A.in.text[i] == 11) { A.palette = 1; A.pal_q[0] = 0; A.pal_sel = 0; A.in.ntext = 0; }
            if (back) nav_back();
            else if (fwd) nav_forward();
            else if (A.in.key_pressed[KEY_ESC] && A.view != V_SCORE) go_score();
            if (alt) { A.in.key_pressed[KEY_LEFT] = A.in.key_pressed[KEY_RIGHT] = 0; A.in.ntext = 0; }
            A.in.key_pressed[KEY_BACKSPACE] = 0;
            for (i = 0; i < A.in.ntext; i++) {
                unsigned c = A.in.text[i];
                if (c == '?') A.help = 1;
                else if (c == '/') { A.palette = 1; A.pal_q[0] = 0; A.pal_sel = 0; }
                else if (c == 's' || c == 'S') { if (A.view != V_SCORE) go_score(); }
                else if (c == 'e' || c == 'E') { if (A.view != V_ETUDE) open_etude(A.etude); }
                else if (c == 'm' || c == 'M') synth_mute(!synth_muted());
            }
            if (A.palette || A.help) A.in.ntext = 0;
            if (A.view == V_PIECE && A.in.key_pressed[KEY_TAB]) { A.perform = !A.perform; if (CUR >= 0) A.sel_step = A.perform ? g_revealed[CUR] - 1 : (A.sel_step < 0 ? 0 : A.sel_step); stage_for_piece(); }
            if (A.view == V_SCORE) {
                int col = A.kcol >= 0 ? A.kcol : (A.hov_col >= 0 ? A.hov_col : -1), moved = 0;
                if (A.in.key_pressed[KEY_SPACE]) { A.playing = !A.playing; A.play_x = 0; A.play_last = -1; }
                for (i = 0; i < A.in.ntext; i++) { unsigned c = A.in.text[i];
                    if (c >= '1' && c <= '7') A.sel_filter = A.sel_filter == (int)(c - '0') ? -1 : (int)(c - '0');
                    if (c == '0') A.sel_filter = A.sel_filter == MO_GROUND ? -1 : MO_GROUND; }
                if (A.in.key_pressed[KEY_UP] || A.in.key_pressed[KEY_DOWN]) {
                    /* walk the staves: -1 (none), Split ... Shadow, Ground */
                    static const int order[MO_COUNT + 1] = { -1, MO_SPLIT, MO_TURN, MO_MIRROR, MO_LOOP, MO_HOLD, MO_LANES, MO_SHADOW, MO_GROUND };
                    int k, at = 0;
                    for (k = 0; k <= MO_COUNT; k++) if (order[k] == A.sel_filter) at = k;
                    at += A.in.key_pressed[KEY_DOWN] ? 1 : -1;
                    if (at < 0) at = MO_COUNT; if (at > MO_COUNT) at = 0;
                    A.sel_filter = order[at];
                }
                if (A.in.key_pressed[KEY_RIGHT] || A.in.key_pressed[KEY_LEFT]) {
                    int dir = A.in.key_pressed[KEY_RIGHT] ? 1 : -1, c = col < 0 ? (dir > 0 ? -1 : NPIECES) : col;
                    if (A.in.shift) {   /* to the next section */
                        int sec = c >= 0 && c < NPIECES ? PIECES[c].section : -1;
                        do c += dir; while (c >= 0 && c < NPIECES && PIECES[c].section == sec);
                        if (dir < 0 && c >= 0) { int s2 = PIECES[c].section; while (c > 0 && PIECES[c - 1].section == s2) c--; }
                    } else if (A.sel_filter >= 0) {
                        do c += dir; while (c >= 0 && c < NPIECES && !PIECE_MOTIFS[c][A.sel_filter]);
                    } else c += dir;
                    if (c >= 0 && c < NPIECES) col = c;
                    moved = 1;
                }
                if (A.in.key_pressed[KEY_HOME]) { col = 0; moved = 1; }
                if (A.in.key_pressed[KEY_END]) { col = NPIECES - 1; moved = 1; }
                if (moved && col >= 0) { A.kcol = col; A.kbd = 1; play_chord(col); }
                if (A.in.key_pressed[KEY_ENTER]) { if (col < 0) col = 0; A.kcol = col; open_piece(col, 0); }
            }
            if (A.view == V_ETUDE) {
                static const int order[MO_COUNT] = { MO_GROUND, MO_SPLIT, MO_TURN, MO_MIRROR, MO_LOOP, MO_HOLD, MO_LANES, MO_SHADOW };
                int k, at = 0;
                for (k = 0; k < MO_COUNT; k++) if (order[k] == A.etude) at = k;
                if (A.in.key_pressed[KEY_DOWN] && at < MO_COUNT - 1) { open_etude(order[at + 1]); A.kbd = 1; }
                if (A.in.key_pressed[KEY_UP] && at > 0) { open_etude(order[at - 1]); A.kbd = 1; }
                for (i = 0; i < A.in.ntext; i++) { unsigned c = A.in.text[i]; if (c >= '0' && c <= '7') { open_etude(c == '0' ? MO_GROUND : (int)(c - '0')); A.kbd = 1; } }
            }
        }
    }

    if (A.playing) {
        A.play_x += A.dt * 3.2f;
        if ((int)A.play_x != A.play_last && (int)A.play_x < NPIECES) { A.play_last = (int)A.play_x; play_chord(A.play_last); }
        if (A.play_x >= NPIECES) A.playing = 0;
    }
    if (A.shake > 0) A.shake -= A.dt; if (A.flash > 0) A.flash -= A.dt * 2.5f;

    {   int saved_clicked = A.clicked;
        Input keep = A.in;
        if (A.help || A.palette) {   /* an overlay takes the clicks and the keys */
            A.clicked = 0; A.in.ntext = 0; memset(A.in.key_pressed, 0, sizeof A.in.key_pressed); A.in.wheel = 0;
        }
        top_bar(top);
        if (A.view == V_SCORE) score_view(body);
        else if (A.view == V_PIECE) piece_view(body);
        else etude_view(body);
        A.clicked = saved_clicked;
        if (A.help || A.palette) { A.in.ntext = keep.ntext; memcpy(A.in.text, keep.text, sizeof A.in.text); memcpy(A.in.key_pressed, keep.key_pressed, sizeof A.in.key_pressed); }
        if (A.palette) { int had = A.palette; A.consumed = 0; palette_overlay(body); (void)had; }
        else if (A.help) { A.consumed = 0; if (!A.help_was_open) A.consumed = 1; help_overlay(body); }
        {   Loc after = loc_now();   /* record every change of place */
            if (!A.nav_jumped && !same_place(&before, &after)) { push_hist(g_back, &g_nb, &before); g_nf = 0; }
        }
    }
    if (A.view != V_PIECE && A.view != V_ETUDE) synth_release_all();
    plat_cursor(A.want_hand ? CURSOR_HAND : CURSOR_ARROW);
}

int app_main(int argc, char **argv) {
    int i, max_frames = 0; double last;
    memset(&A, 0, sizeof A);
    A.hov_col = -1; A.hov_staff = -1; A.kcol = -1; A.sel_filter = -1; A.perform = 1; A.wrong_key = -1; A.etude = MO_SPLIT; A.st.drag = -1; A.st.scene = -1;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) { A.selftest = 1; A.shot_dir = i + 1 < argc ? argv[++i] : "."; }
    }
    if (!plat_init("Leitmotif \u2014 Chapters 1\u20132 as a score", 1440, 900, 1100, 700, &A.msaa)) { plat_fatal("Could not open an OpenGL window."); return 1; }
    plat_poll(&A.in);
    g_s = A.in.dpi;
    d_begin_frame(A.in.win_w, A.in.win_h);
    {   GLint mt = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mt);
        plat_log("window %dx%d px, dpi %.2f, msaa %d", A.in.win_w, A.in.win_h, A.in.dpi, A.msaa);
        plat_log("GL: %s | %s | %s | max texture %d", (const char *)glGetString(GL_VENDOR), (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), (int)mt); }
    /* paint the paper colour at once, so the window is never blank while fonts load */
    glClearColor(0.953f, 0.941f, 0.910f, 1); glClear(GL_COLOR_BUFFER_BIT); plat_swap();
    if (!font_init(A.in.dpi)) { plat_fatal("Could not load system fonts for the display."); plat_shutdown(); return 1; }
    plat_log("fonts ready");
    tex_set_palette(MOTIF_COL, MO_COUNT);
    synth_init();
    if (!A.selftest) { int ok = plat_audio_open(44100); plat_log("audio %s", ok ? "on" : "unavailable"); }
    if (A.selftest) synth_mute(1);
    check_content();
    plat_log("content checked");
    if (!A.selftest) progress_load();
    plat_log("entering main loop");
    if (A.selftest) { printf("msaa=%d dpi=%.2f GL_RENDERER=%s\n", A.msaa, A.in.dpi, (const char *)glGetString(GL_RENDERER)); max_frames = (10 + NPIECES + MO_COUNT) * 8 + 8; }
    last = plat_time();
    while (!A.in.quit) {
        double now = plat_time(); const char *shot = NULL;
        plat_poll(&A.in);
        A.dt = (float)(now - last); if (A.dt > 0.1f) A.dt = 0.1f; last = now;
        if (A.selftest) { A.dt = 1.0f / 30; shot = selftest_script(A.frames); }
        A.t += A.dt;
        frame();
        if (shot) { glFinish(); screenshot(shot); }
        plat_swap();
        plat_audio_pump(synth_fill);
        A.frames++;
        if (A.frames == 1) plat_log("first frame drawn");
        if (A.selftest && A.frames > max_frames) A.in.quit = 1;
        if (!A.selftest) { double spent = plat_time() - now; if (spent < 1.0 / 60) plat_sleep(1.0 / 60 - spent); }
        if (!A.in.focused && !A.selftest) plat_sleep(0.03);
    }
    if (!A.selftest) progress_save();
    plat_audio_close();
    font_free();
    plat_shutdown();
    if (A.selftest) {
        printf("frames=%d live_blocks=%ld peak_bytes=%ld\n", A.frames, mem_live_blocks(), mem_peak_bytes());
        if (mem_live_blocks() != 0) { printf("FAIL: leaked blocks\n"); return 2; }
        if (g_fail) return 3;
        printf("selftest OK\n");
    }
    return 0;
}
