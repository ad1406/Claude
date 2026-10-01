/* Diverge E5 Workshop: application shell. Layout, camera, picking, the four
   modes (Explore, Systems, Benches, Fix it) and the self-test harness. */
#include "gl_inc.h"
#include "platform.h"
#include "mem.h"
#include "mathx.h"
#include "font.h"
#include "ui.h"
#include "sim.h"
#include "bike.h"
#include "content.h"
#include "diagrams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { M_EXPLORE, M_SYSTEMS, M_BENCHES, M_FIX, M_COUNT };
typedef struct { V3 target; float dist, yaw, pitch; } Cam;

static struct {
    Input in;
    int mode, sel, hover, labels;
    int bench, run, step, playing; float step_t;
    int prob; char search[64]; int sysf; int groupf;
    Scroll sc_left, sc_right;
    Cam cam, goal; int cam_anim;
    Rect vp;                    /* 3D viewport, window pixels */
    Rect blockers[8]; int nblock;
    M4 proj, view, mvp;
    int msaa, stencil;
    int drag, drag_btn, drag_moved; int press_x, press_y, press_hover;
    float squeeze_hold;
    int fix_hl[P_COUNT]; int fix_hl_on; float fix_hl_t;
    int selftest, frames; const char *shot_dir;
    int hide_schematic; float lens_ty; Rect schem;
    double t;
} A;

static const char *MODE_NAMES[M_COUNT] = { "Explore", "Systems", "Benches", "Fix it" };

/* ------------------------------------------------------------------ camera */
static Cam cam_default(void) { Cam c; c.target = v3(540, 470, 0); c.dist = 3250; c.yaw = DEG(28); c.pitch = DEG(12); return c; }
static void cam_fly(V3 t, float dist, float yaw_deg, float pitch_deg) {
    A.goal.target = t; A.goal.dist = dist; A.goal.yaw = DEG(yaw_deg); A.goal.pitch = DEG(pitch_deg); A.cam_anim = 1;
    /* take the short way round */
    while (A.goal.yaw - A.cam.yaw > PI_F) A.goal.yaw -= 2 * PI_F;
    while (A.goal.yaw - A.cam.yaw < -PI_F) A.goal.yaw += 2 * PI_F;
}
static void cam_focus_part(int p) {
    const PartInfo *pi = &PARTS[p];
    cam_fly(bike_anchor(p), pi->dist * 1.9f, pi->yaw, pi->pitch);
}
static void cam_bench(int b) { const BenchInfo *bi = &BENCHES[b]; cam_fly(bi->target, bi->dist, bi->yaw, bi->pitch); }
static void cam_update(float dt) {
    if (A.cam_anim) {
        float k = 1 - expf(-dt * 4.5f);
        A.cam.target = v3lerp(A.cam.target, A.goal.target, k);
        A.cam.dist = lerpf(A.cam.dist, A.goal.dist, k);
        A.cam.yaw = lerpf(A.cam.yaw, A.goal.yaw, k);
        A.cam.pitch = lerpf(A.cam.pitch, A.goal.pitch, k);
        if (v3len(v3sub(A.cam.target, A.goal.target)) < 0.5f && fabsf(A.cam.dist - A.goal.dist) < 0.5f &&
            fabsf(A.cam.yaw - A.goal.yaw) < 0.001f && fabsf(A.cam.pitch - A.goal.pitch) < 0.001f) A.cam_anim = 0;
    }
}
static V3 cam_eye(void) {
    Cam *c = &A.cam;
    return v3add(c->target, v3mul(v3(sinf(c->yaw) * cosf(c->pitch), sinf(c->pitch), cosf(c->yaw) * cosf(c->pitch)), c->dist));
}
static void cam_matrices(void) {
    float aspect = A.vp.w / (A.vp.h > 1 ? A.vp.h : 1);
    A.proj = m4_perspective(DEG(30), aspect, 20, 40000);
    {   /* lens shift: centre the bike in the part of the view not covered by cards */
        int c; for (c = 0; c < 4; c++) A.proj.m[c * 4 + 1] += A.lens_ty * A.proj.m[c * 4 + 3];
    }
    A.view = m4_lookat(cam_eye(), A.cam.target, v3(0, 1, 0));
    A.mvp = m4_mul(A.proj, A.view);
}

/* ------------------------------------------------------------- highlights */
static void bench_parts(int b, unsigned char *hl) {
    static const int sets[B_COUNT][6] = {
        { P_CHAIN, P_CASSETTE, P_CRANKSET, -1 },
        { P_RD, P_HANGER, P_CASSETTE, P_CHAIN, -1 },
        { P_FD, P_CHAIN, P_CRANKSET, -1 },
        { P_CALIPERS, P_ROTORS, P_LEVERS, -1 },
        { P_TIRES, P_WHEELS, P_HUBS, P_THRUAXLE, -1 },
        { P_HEADSET, P_COCKPIT, P_BB, P_FORK, -1 } };
    int i;
    for (i = 0; i < 6 && sets[b][i] >= 0; i++) hl[sets[b][i]] = 1;
}
static void build_draw_opts(DrawOpts *o) {
    int i;
    memset(o, 0, sizeof *o);
    o->pulse = 0.55f + 0.45f * sinf((float)A.t * 3.2f);
    if (A.mode == M_SYSTEMS && A.run >= 0) { o->hl[RUNS[A.run].steps[A.step].part] = 1; }
    else if (A.mode == M_BENCHES) { bench_parts(A.bench, o->hl); o->pulse *= 0.35f; }
    else if (A.mode == M_FIX && A.fix_hl_on) { for (i = 0; i < P_COUNT; i++) o->hl[i] = (unsigned char)A.fix_hl[i]; }
    else if (A.sel >= 0) o->hl[A.sel] = 1;
    for (i = 0; i < P_COUNT; i++) if (o->hl[i]) o->any_hl = 1;
    o->show_patch = A.mode == M_BENCHES && A.bench == B_WHEELS;
}

/* ---------------------------------------------------------------- actions */
static void set_mode(int m) {
    if (A.mode == m) return;
    A.mode = m; A.sc_right.target = A.sc_right.off = 0;
    A.playing = 0;
    if (m == M_BENCHES) cam_bench(A.bench);
    if (m == M_SYSTEMS) { A.step = 0; A.step_t = 0; }
    if (m == M_EXPLORE && A.sel >= 0) cam_focus_part(A.sel);
}
static void select_part(int p, int focus) {
    A.sel = p; A.sc_right.target = A.sc_right.off = 0;
    if (A.mode != M_EXPLORE) { A.mode = M_EXPLORE; A.playing = 0; }
    if (p >= 0 && focus) cam_focus_part(p);
}
static void open_bench(int b) { A.bench = b; A.mode = M_BENCHES; A.sc_right.target = A.sc_right.off = 0; A.playing = 0; cam_bench(b); }
static void open_problem(int i) { A.prob = i; A.mode = M_FIX; A.sc_right.target = A.sc_right.off = 0; A.fix_hl_on = 0; A.playing = 0; }

static void do_action(int a) {
    switch (a) {
    case A_PEDAL_ON: sim.r.pedaling = 1; if (sim.r.v < 0.5f) sim.r.v = 0.5f; break;
    case A_PEDAL_OFF: sim.r.pedaling = 0; break;
    case A_SHIFT_EASIER: if (sim.r.rear < ncogs() - 1) sim.r.rear++; break;
    case A_BRAKE_ON: sim.r.pedaling = 0; sim.r.brake = 1; break;
    case A_BRAKE_OFF: sim.r.brake = 0; break;
    }
}
static void run_goto(int step) {
    const Run *r = &RUNS[A.run];
    if (step < 0 || step >= r->n) return;
    A.step = step; A.step_t = 0;
    do_action(r->steps[step].action);
    {
        int p = r->steps[step].part; const PartInfo *pi = &PARTS[p];
        cam_fly(bike_anchor(p), pi->dist * 2.1f, pi->yaw, pi->pitch);
    }
}
static void start_run(int i) {
    A.mode = M_SYSTEMS; A.run = i; A.playing = 1; A.sc_right.target = A.sc_right.off = 0;
    sim.r.brake = 0;
    run_goto(0);
}

static void reset_bench_state(int b) {
    switch (b) {
    case B_RD: memset(&sim.rd, 0, sizeof sim.rd); break;
    case B_FD: memset(&sim.fd, 0, sizeof sim.fd); break;
    case B_BRAKES: { int h = sim.br.hydro; memset(&sim.br, 0, sizeof sim.br); sim.br.pad = 3; sim.br.hydro = h; } break;
    case B_WHEELS: sim.wh.psi = 40; sim.wh.kg = 85; sim.wh.tubeless = 0; sim.wh.terrain = 1; break;
    case B_STEER: sim.st.preload = 50; break;
    }
}
static void recreate_fault(int pi) {
    const Problem *p = &PROBS[pi]; int maxc = ncogs() - 1;
    if (p->bench < 0) return;
    reset_bench_state(p->bench);
    switch (pi) {
    case 0: sim.rd.barrel = -5; sim.r.rear = 1; break;
    case 1: sim.rd.barrel = -2; sim.r.rear = 1; break;
    case 2: sim.rd.barrel = 2; sim.r.rear = 1; break;
    case 3: sim.rd.L = 3; sim.r.rear = maxc; break;
    case 4: sim.rd.H = -3; sim.r.rear = 0; break;
    case 5: sim.rd.hanger = 4; sim.r.rear = maxc; break;
    case 6: sim.r.front = 1; sim.r.rear = maxc; break;
    case 7: sim.fd.L = -3; sim.r.front = 0; sim.r.rear = maxc; break;
    case 8: sim.r.front = 1; sim.r.rear = maxc; break;
    case 9: sim.br.hydro = 0; sim.br.pad = 2.6f; sim.br.stretch = 0.8f; break;
    case 10: sim.br.hydro = 1; sim.br.air = 0.75f; break;
    case 11: sim.br.contam = 1; break;
    case 12: sim.br.warp = 1; sim.r.pedaling = 1; break;
    case 13: sim.wh.psi = 24; sim.wh.terrain = 2; break;
    case 15: if (sim.r.v < 6) sim.r.v = 6; sim.r.pedaling = 0; break;
    case 16: sim.st.preload = 12; break;
    case 17: sim.st.preload = 92; break;
    case 20: break;
    }
    open_bench(p->bench);
}

static void set_ringset(int i) { if (i != sim.r.ringset) { sim.r.ringset = i; bike_rebuild_drive(); } }
static void set_cassette(int i) {
    if (i != sim.r.cassette) { sim.r.cassette = i; if (sim.r.rear >= ncogs()) sim.r.rear = ncogs() - 1; bike_rebuild_drive(); }
}
static void set_build(int b) {
    int r = sim.r.ringset, c = sim.r.cassette;
    sim_apply_build(b);
    if (r != sim.r.ringset || c != sim.r.cassette) bike_rebuild_drive();
}

/* ----------------------------------------------------------------- picking */
static int pick_at(int mx, int my) {
    DrawOpts o; unsigned char px[4] = {0, 0, 0, 0}; int H = A.in.win_h;
    if (!in_rect(A.vp, (float)mx, (float)my)) return -1;
    memset(&o, 0, sizeof o); o.mode = DRAW_PICK;
    glViewport((int)A.vp.x, H - (int)(A.vp.y + A.vp.h), (int)A.vp.w, (int)A.vp.h);
    glEnable(GL_SCISSOR_TEST); glScissor(mx, H - my - 1, 1, 1);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING); glDisable(GL_BLEND); glDisable(GL_DITHER); glDisable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(A.proj.m);
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(A.view.m);
    bike_draw(&o);
    glReadPixels(mx, H - my - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glDisable(GL_SCISSOR_TEST); glEnable(GL_DITHER);
    if (A.msaa) glEnable(GL_MULTISAMPLE);
    return pick_decode(px);
}

/* --------------------------------------------------------------- rendering */
static void setup_lights(void) {
    float amb[4] = {0.40f, 0.41f, 0.42f, 1}, l0p[4] = {-0.45f, 1.0f, 0.75f, 0}, l0d[4] = {0.72f, 0.71f, 0.69f, 1}, l0s[4] = {0.55f, 0.55f, 0.55f, 1};
    float l1p[4] = {0.7f, 0.35f, -0.8f, 0}, l1d[4] = {0.30f, 0.32f, 0.34f, 1}, zero[4] = {0, 0, 0, 1};
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); glEnable(GL_LIGHT1);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    glLightfv(GL_LIGHT0, GL_POSITION, l0p); glLightfv(GL_LIGHT0, GL_DIFFUSE, l0d); glLightfv(GL_LIGHT0, GL_SPECULAR, l0s); glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
    glLightfv(GL_LIGHT1, GL_POSITION, l1p); glLightfv(GL_LIGHT1, GL_DIFFUSE, l1d); glLightfv(GL_LIGHT1, GL_SPECULAR, zero); glLightfv(GL_LIGHT1, GL_AMBIENT, zero);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
}

static void draw_background(void) {
    /* white void: a faint radial falloff centred on the viewport */
    int i, n = 64; float cx = A.vp.x + A.vp.w / 2, cy = A.vp.y + A.vp.h * 0.55f;
    float r = sqrtf((float)A.in.win_w * A.in.win_w + (float)A.in.win_h * A.in.win_h);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1, 1, 1, 1); glVertex2f(cx, cy);
    for (i = 0; i <= n; i++) {
        float a = (float)i / n * 2 * PI_F;
        glColor4f(0.925f, 0.937f, 0.945f, 1); glVertex2f(cx + cosf(a) * r * 0.75f, cy + sinf(a) * r * 0.75f);
    }
    glEnd();
}

static void render_3d(void) {
    DrawOpts o; int H = A.in.win_h;
    build_draw_opts(&o);
    glViewport((int)A.vp.x, H - (int)(A.vp.y + A.vp.h), (int)A.vp.w, (int)A.vp.h);
    glMatrixMode(GL_PROJECTION); glLoadMatrixf(A.proj.m);
    glMatrixMode(GL_MODELVIEW); glLoadMatrixf(A.view.m);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    setup_lights();
    bike_draw_ground(A.stencil, &o);
    glDisable(GL_BLEND);
    bike_draw(&o);
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST);
}

/* ------------------------------------------------------------------- panel */
static Rect panel(Rect r) {
    d_shadow(r, S(14), S(14), 0.10f);
    d_rrect_border(r, S(14), S(1), C_PANEL, C_LINE);
    return rect(r.x + S(18), r.y + S(16), r.w - S(36), r.h - S(32));
}
static float para(int f, float x, float y, float w, const char *s, Color c) { return font_wrap(f, x, y, w, s, c, 1); }
static float label(float x, float y, const char *s) { d_text(F_SMALL, x, y, s, C_MUTED); return font_line(F_SMALL) + S(4); }
static void block(Rect r) { if (A.nblock < 8) A.blockers[A.nblock++] = r; }

static float msg_list(float x, float y, float w, int bench) {
    Msg m[6]; int n = bench_messages(bench, m, 6), i; float yy = y;
    static const char *k[] = { "NOTE", "OK", "WATCH", "FAULT" };
    for (i = 0; i < n; i++) {
        float pw = S(60), h;
        ui_pill(x, yy + S(1), k[m[i].kind], m[i].kind == 0 ? 0 : m[i].kind);
        h = para(F_BODY, x + pw, yy, w - pw, m[i].text, C_INK);
        yy += h + S(8);
    }
    return yy - y;
}

static float slider_row(unsigned id, float x, float y, float w, const char *name, float *v, float mn, float mx, float step, const char *valfmt) {
    char buf[48];
    snprintf(buf, sizeof buf, valfmt, *v);
    d_text(F_BODY, x, y, name, C_INK);
    d_text_right(F_BOLD, x + w, y, buf, C_INK);
    ui_slider(id, rect(x + S(4), y + font_line(F_BODY) + S(4), w - S(8), S(22)), v, mn, mx, step);
    return font_line(F_BODY) + S(34);
}

static float card_list(float x, float y, float w, const char *title, const char *const *items, int numbered) {
    float yy = y; int i; char num[16];
    d_text(F_BOLD, x, yy, title, C_INK); yy += font_line(F_BOLD) + S(6);
    for (i = 0; items[i]; i++) {
        if (numbered) { snprintf(num, sizeof num, "%d", i + 1); d_circle(x + S(9), yy + font_line(F_BODY) / 2, S(9), C_SOFT); d_text_center(F_SMALL, rect(x, yy + font_line(F_BODY) / 2 - S(9), S(18), S(18)), num, C_MUTED); }
        else d_circle(x + S(6), yy + font_line(F_BODY) / 2, S(2.5f), C_MUTED);
        yy += para(F_BODY, x + S(26), yy, w - S(26), items[i], C_INK) + S(6);
    }
    return yy - y;
}

/* --- left panel per mode --- */
static void left_explore(Rect r) {
    Rect in = panel(r); float y, setup_h = S(286); int g, p;
    Rect list;
    d_text(F_H2, in.x, in.y, "Parts", C_INK);
    y = in.y + font_line(F_H2) + S(2);
    y += para(F_SMALL, in.x, y, in.w, "Choose a part here or click it on the bike.", C_MUTED) + S(10);
    list = rect(in.x - S(6), y, in.w + S(12), in.y + in.h - setup_h - y);
    {
        float yy = scroll_begin(UID(0), &A.sc_left, list) , y0 = yy;
        for (g = 0; g < G_COUNT; g++) {
            d_text(F_SMALL, list.x + S(8), yy + S(6), GROUP_NAMES[g], C_FAINT); yy += font_line(F_SMALL) + S(10);
            for (p = 0; p < P_COUNT; p++) if (PARTS[p].group == g) {
                Rect row = rect(list.x, yy, list.w - S(8), S(32));
                if (ui_row(UID(p), row, A.sel == p)) select_part(A.sel == p ? -1 : p, 1);
                if (A.hover == p && A.sel != p) d_rrect(row, S(8), C_HOVER);
                d_text(A.sel == p ? F_BOLD : F_BODY, row.x + S(10), row.y + (row.h - font_line(F_BODY)) / 2, PARTS[p].name, A.sel == p ? C_ACCENT : C_INK);
                yy += S(33);
            }
            yy += S(6);
        }
        scroll_end(&A.sc_left, list, yy - y0);
    }
    /* bike setup */
    y = in.y + in.h - setup_h + S(10);
    d_rect(rect(in.x, y - S(10), in.w, S(1)), C_LINE);
    d_text(F_H3, in.x, y, "Bike setup", C_INK); y += font_line(F_H3) + S(6);
    {
        static const char *builds[] = { "Claris 2×8", "GRX 2×10" };
        static const char *rings[] = { "50/34", "46/34", "46/32", "46/30" };
        static const char *cassA[] = { "8-speed 11-32", "8-speed 11-34" }, *cassB[] = { "10-speed 11-34", "10-speed 11-36" };
        int b = sim.r.build, rs = sim.r.ringset, ca = sim.r.cassette < 2 ? sim.r.cassette : -1, cb = sim.r.cassette >= 2 ? sim.r.cassette - 2 : -1;
        y += label(in.x, y, "Build");
        if (ui_segmented(UID(0), rect(in.x, y, in.w, S(32)), builds, 2, &b)) set_build(b);
        y += S(38); y += label(in.x, y, "Chainrings");
        if (ui_segmented(UID(0), rect(in.x, y, in.w, S(32)), rings, 4, &rs)) set_ringset(rs);
        y += S(38); y += label(in.x, y, "Cassette");
        if (ui_segmented(UID(0), rect(in.x, y, in.w, S(32)), cassA, 2, &ca)) set_cassette(ca);
        if (ui_segmented(UID(0), rect(in.x, y + S(36), in.w, S(32)), cassB, 2, &cb)) set_cassette(cb + 2);
    }
}

static void left_systems(Rect r) {
    Rect in = panel(r); float y; int i;
    d_text(F_H2, in.x, in.y, "Systems", C_INK);
    y = in.y + font_line(F_H2) + S(2);
    y += para(F_SMALL, in.x, y, in.w, "Each run traces one job through the bike, part by part. The camera follows along.", C_MUTED) + S(12);
    for (i = 0; i < NRUNS; i++) {
        float h = S(12) + font_line(F_BOLD) + font_wrap(F_SMALL, 0, 0, in.w - S(20), RUNS[i].sub, C_MUTED, 0) + S(10);
        Rect row = rect(in.x - S(6), y, in.w + S(12), h);
        if (ui_row(UID(i), row, A.run == i && A.mode == M_SYSTEMS)) start_run(i);
        d_text(F_BOLD, row.x + S(12), row.y + S(10), RUNS[i].name, A.run == i ? C_ACCENT : C_INK);
        para(F_SMALL, row.x + S(12), row.y + S(12) + font_line(F_BOLD), in.w - S(20), RUNS[i].sub, C_MUTED);
        y += h + S(4);
    }
}

static void left_benches(Rect r) {
    Rect in = panel(r); float y; int i;
    d_text(F_H2, in.x, in.y, "Benches", C_INK);
    y = in.y + font_line(F_H2) + S(2);
    y += para(F_SMALL, in.x, y, in.w, "Each bench isolates one mechanism. Move the adjusters and watch the bike respond.", C_MUTED) + S(12);
    for (i = 0; i < B_COUNT; i++) {
        float h = S(12) + font_line(F_BOLD) + font_line(F_SMALL) + S(10);
        Rect row = rect(in.x - S(6), y, in.w + S(12), h);
        if (ui_row(UID(i), row, A.bench == i)) open_bench(i);
        d_text(F_BOLD, row.x + S(12), row.y + S(10), BENCHES[i].name, A.bench == i ? C_ACCENT : C_INK);
        d_text(F_SMALL, row.x + S(12), row.y + S(12) + font_line(F_BOLD), BENCHES[i].sub, C_MUTED);
        y += h + S(4);
    }
}

static int str_icontains(const char *h, const char *n) {
    size_t i, j, ln = strlen(n);
    if (!ln) return 1;
    for (i = 0; h[i]; i++) {
        for (j = 0; j < ln && h[i + j]; j++) {
            char a = h[i + j], b = n[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
        }
        if (j == ln) return 1;
    }
    return 0;
}
static int prob_matches(int i) {
    const Problem *p = &PROBS[i]; int k;
    if (A.sysf >= 0 && !(p->sys & (1 << A.sysf))) return 0;
    if (!A.search[0]) return 1;
    if (str_icontains(p->title, A.search) || str_icontains(p->sub, A.search)) return 1;
    for (k = 0; p->causes[k]; k++) if (str_icontains(p->causes[k], A.search)) return 1;
    return 0;
}

static void left_fix(Rect r) {
    Rect in = panel(r), list; float y, x; int i, shown = 0;
    d_text(F_H2, in.x, in.y, "Fix it", C_INK);
    y = in.y + font_line(F_H2) + S(8);
    ui_textbox(UID(0), rect(in.x, y, in.w, S(38)), A.search, sizeof A.search, "Describe it: skips, squeal, creak…");
    y += S(48);
    x = in.x;
    for (i = -1; i < SYS_N; i++) {   /* filter chips, wrapping */
        const char *n = i < 0 ? "All" : SYS_NAMES[i];
        float w = font_width(F_BODY, n, -1) + S(22);
        if (x + w > in.x + in.w) { x = in.x; y += S(34); }
        if (ui_button(UID(i + 1), rect(x, y, w, S(28)), n, A.sysf == i ? BTN_CHIP_ON : BTN_CHIP)) A.sysf = i;
        x += w + S(6);
    }
    y += S(40);
    list = rect(in.x - S(6), y, in.w + S(12), in.y + in.h - y);
    {
        float yy = scroll_begin(UID(0), &A.sc_left, list), y0 = yy;
        for (i = 0; i < NPROBS; i++) {
            const Problem *p = &PROBS[i]; float h; Rect row; Color sev;
            if (!prob_matches(i)) continue;
            shown++;
            h = S(10) + font_wrap(F_BOLD, 0, 0, list.w - S(40), p->title, C_INK, 0) + S(10);
            row = rect(list.x, yy, list.w - S(8), h);
            if (ui_row(UID(i), row, A.prob == i)) open_problem(i);
            sev = p->sev == 3 ? C_BAD : p->sev == 2 ? C_WARN : C_OK;
            d_rrect(rect(row.x + S(12), row.y + S(15), S(9), S(9)), S(2), sev);
            para(F_BOLD, row.x + S(30), row.y + S(10), list.w - S(40), p->title, A.prob == i ? C_ACCENT : C_INK);
            yy += h + S(2);
        }
        if (!shown) yy += para(F_BODY, list.x + S(8), yy + S(6), list.w - S(16), "No match. Try a simpler word like noise, brake or gear, or choose All.", C_MUTED) + S(12);
        scroll_end(&A.sc_left, list, yy - y0);
    }
}

/* --- right panel per mode --- */
static void right_explore(Rect r) {
    Rect in = panel(r); Rect area = rect(in.x - S(4), in.y, in.w + S(8), in.h);
    float y = scroll_begin(UID(0), &A.sc_right, area), y0 = y, x = in.x, w = in.w;
    if (A.sel < 0) {
        static const char *mouse[] = { "Drag to orbit around the bike.", "Right-drag (or middle-drag) to pan.", "Scroll to zoom.", "Click a part to inspect it; double-click to fly to it.", NULL };
        static const char *keys[] = { "Space: start or stop pedalling.", "B (hold): brake.", "[ and ]: shift to an easier or harder rear cog.", "1 and 2: small or big chainring.", "L: labels.  R: reset view.  Esc: clear selection.", NULL };
        d_text(F_SMALL, x, y, "INSPECTOR", C_ACCENT); y += font_line(F_SMALL) + S(2);
        d_text(F_H2, x, y, "Pick a part", C_INK); y += font_line(F_H2) + S(6);
        y += para(F_BODY, x, y, w, "Every part of the bike is clickable. The inspector explains what it does, what it works with, how to check it, and the problems it causes.", C_MUTED) + S(18);
        y += card_list(x, y, w, "Mouse", mouse, 0) + S(14);
        y += card_list(x, y, w, "Keyboard", keys, 0) + S(14);
        y += para(F_SMALL, x, y, w, "The animation runs at a quarter of real speed so you can follow the chain and wheels. Toggle it in the control bar.", C_FAINT);
    } else {
        const PartInfo *p = &PARTS[A.sel]; int i; float cx;
        d_text(F_SMALL, x, y, GROUP_NAMES[p->group], C_ACCENT); y += font_line(F_SMALL) + S(2);
        y += para(F_H2, x, y, w, p->name, C_INK) + S(10);
        y += label(x, y, "What it does"); y += para(F_BODY, x, y, w, p->does, C_INK) + S(14);
        y += label(x, y, "On this bike"); y += para(F_BODY, x, y, w, p->spec, C_INK) + S(14);
        y += label(x, y, "Quick check"); y += para(F_BODY, x, y, w, p->check, C_INK) + S(14);
        y += label(x, y, "Works with");
        cx = x;
        for (i = 0; p->links[i] >= 0; i++) {
            const char *n = PARTS[p->links[i]].name; float bw = font_width(F_BODY, n, -1) + S(20);
            if (cx + bw > x + w) { cx = x; y += S(34); }
            if (ui_button(UID(i), rect(cx, y, bw, S(28)), n, BTN_CHIP)) { select_part(p->links[i], 1); break; }
            cx += bw + S(6);
        }
        y += S(42);
        if (A.sel >= 0 && p->probs[0] >= 0) {
            y += label(x, y, "Problems it causes");
            for (i = 0; p->probs[i] >= 0; i++) {
                const Problem *pr = &PROBS[p->probs[i]];
                float h = font_wrap(F_BODY, 0, 0, w - S(40), pr->title, C_INK, 0) + S(16);
                Rect row = rect(x - S(6), y, w + S(12), h);
                if (ui_row(UID(i), row, 0)) { open_problem(p->probs[i]); break; }
                d_rrect(rect(row.x + S(10), row.y + h / 2 - S(4), S(8), S(8)), S(2), pr->sev == 3 ? C_BAD : pr->sev == 2 ? C_WARN : C_OK);
                para(F_BODY, row.x + S(26), row.y + S(8), w - S(40), pr->title, C_INK);
                y += h + S(2);
            }
            y += S(12);
        }
        if (A.sel >= 0) {
            if (ui_button(UID(0), rect(x, y, w, S(38)), "Fly to this part", BTN_SECONDARY)) cam_focus_part(A.sel);
            y += S(46);
            if (A.sel >= 0 && p->bench >= 0) {
                char b[64]; snprintf(b, sizeof b, "Open the %s bench", BENCHES[p->bench].name);
                if (ui_button(UID(0), rect(x, y, w, S(38)), b, BTN_PRIMARY)) open_bench(p->bench);
                y += S(46);
            }
        }
    }
    scroll_end(&A.sc_right, area, y - y0 + S(8));
}

static void right_systems(Rect r) {
    Rect in = panel(r); Rect area; float y, x = in.x, w = in.w, y0; int i;
    if (A.run < 0) A.run = 0;
    d_text(F_SMALL, x, in.y, "RUN", C_ACCENT);
    y = in.y + font_line(F_SMALL) + S(2);
    y += para(F_H2, x, y, w, RUNS[A.run].name, C_INK) + S(10);
    {
        float bw = (w - S(12)) / 3;
        if (ui_button(UID(0), rect(x, y, bw, S(36)), A.playing ? "Pause" : "Play", BTN_PRIMARY)) { A.playing = !A.playing; A.step_t = 0; }
        if (ui_button(UID(0), rect(x + bw + S(6), y, bw, S(36)), "Back", BTN_SECONDARY)) { A.playing = 0; run_goto(A.step - 1); }
        if (ui_button(UID(0), rect(x + 2 * (bw + S(6)), y, bw, S(36)), "Next", BTN_SECONDARY)) { A.playing = 0; run_goto(A.step + 1); }
        y += S(50);
    }
    area = rect(in.x - S(6), y, in.w + S(12), in.y + in.h - y);
    y = scroll_begin(UID(0), &A.sc_right, area); y0 = y;
    for (i = 0; i < RUNS[A.run].n; i++) {
        const Step *st = &RUNS[A.run].steps[i]; char num[16];
        float th = font_wrap(F_BODY, 0, 0, w - S(42), st->text, C_INK, 0), h = S(12) + font_line(F_BOLD) + th + S(10);
        Rect row = rect(area.x, y, area.w - S(8), h);
        if (ui_row(UID(i), row, i == A.step)) { A.playing = 0; run_goto(i); }
        snprintf(num, sizeof num, "%d", i + 1);
        d_circle(row.x + S(20), row.y + S(10) + font_line(F_BOLD) / 2, S(10), i == A.step ? C_ACCENT : C_SOFT);
        d_text_center(F_SMALL, rect(row.x + S(10), row.y + S(10) + font_line(F_BOLD) / 2 - S(10), S(20), S(20)), num, i == A.step ? C_ACCENT_INK : C_MUTED);
        d_text(F_BOLD, row.x + S(38), row.y + S(10), PARTS[st->part].name, i == A.step ? C_ACCENT : C_INK);
        para(F_BODY, row.x + S(38), row.y + S(12) + font_line(F_BOLD), w - S(42), st->text, i == A.step ? C_INK : C_MUTED);
        y += h + S(2);
    }
    scroll_end(&A.sc_right, area, y - y0 + S(8));
}

static void bench_controls(float x, float *py, float w) {
    float y = *py; char buf[64];
    switch (A.bench) {
    case B_GEARING:
        y += para(F_SMALL, x, y, w, "Metres travelled per crank turn. Click a gear to select it. Hatched gears are cross-chained.", C_MUTED) + S(4);
        break;
    case B_RD:
        d_text(F_BODY, x, y, "Barrel adjuster", C_INK);
        snprintf(buf, sizeof buf, "%+.2f turns", sim.rd.barrel / 4); d_text_right(F_BOLD, x + w, y, buf, C_INK);
        y += font_line(F_BODY) + S(6);
        if (ui_button(UID(0), rect(x, y, w / 2 - S(3), S(34)), "¼ turn out (+)", BTN_SECONDARY) && sim.rd.barrel < 8) sim.rd.barrel += 1;
        if (ui_button(UID(0), rect(x + w / 2 + S(3), y, w / 2 - S(3), S(34)), "¼ turn in (−)", BTN_SECONDARY) && sim.rd.barrel > -8) sim.rd.barrel -= 1;
        y += S(40);
        y += para(F_SMALL, x, y, w, "Out is anticlockwise as seen from where the cable enters. It lengthens the housing, which adds cable tension.", C_MUTED) + S(12);
        y += slider_row(UID(0), x, y, w, "H limit screw (smallest-cog stop)", &sim.rd.H, -3, 3, 0.25f, "%+.2f mm");
        y += slider_row(UID(0), x, y, w, "L limit screw (biggest-cog stop)", &sim.rd.L, -3, 3, 0.25f, "%+.2f mm");
        y += para(F_SMALL, x, y, w, "0 is set correctly. Negative H or positive L is too loose (can overshoot); the other way is too tight (can't reach).", C_MUTED) + S(12);
        y += slider_row(UID(0), x, y, w, "Hanger bent inward", &sim.rd.hanger, 0, 6, 0.5f, "%.1f°");
        if (ui_button(UID(0), rect(x, y, w, S(34)), "Reset to correct setup", BTN_SECONDARY)) reset_bench_state(B_RD);
        y += S(42);
        break;
    case B_FD:
        if (ui_button(UID(0), rect(x, y, w, S(34)), sim.fd.trim ? "Trim click: on" : "Trim click: off", sim.fd.trim ? BTN_CHIP_ON : BTN_SECONDARY)) sim.fd.trim = !sim.fd.trim;
        y += S(40);
        y += para(F_SMALL, x, y, w, "Trim works on the big ring only. It nudges the cage inward a little.", C_MUTED) + S(12);
        y += slider_row(UID(0), x, y, w, "Cable tension (inline adjuster)", &sim.fd.tension, -2, 2, 0.25f, "%+.2f mm");
        y += slider_row(UID(0), x, y, w, "L limit (inner stop)", &sim.fd.L, -3, 2, 0.25f, "%+.2f mm");
        y += slider_row(UID(0), x, y, w, "H limit (outer stop)", &sim.fd.H, -2, 3, 0.25f, "%+.2f mm");
        if (ui_button(UID(0), rect(x, y, w, S(34)), "Reset to correct setup", BTN_SECONDARY)) reset_bench_state(B_FD);
        y += S(42);
        break;
    case B_BRAKES: {
        static const char *types[] = { "Mechanical (cable)", "Hydraulic" };
        int t = sim.br.hydro;
        if (ui_segmented(UID(0), rect(x, y, w, S(34)), types, 2, &t)) sim.br.hydro = t;
        y += S(44);
        y += slider_row(UID(0), x, y, w, "Lever pull", &sim.br.lever, 0, 1, 0.01f, "%.2f");
        A.squeeze_hold = ui_hold(UID(0), rect(x, y, w, S(36)), "Hold to squeeze", 0) ? 1.0f : 0.0f;
        y += S(46);
        y += slider_row(UID(0), x, y, w, "Pad material left (each pad)", &sim.br.pad, 0, 3, 0.1f, "%.1f mm");
        if (!sim.br.hydro) {
            y += slider_row(UID(0), x, y, w, "Cable stretch", &sim.br.stretch, 0, 3, 0.1f, "%.1f mm");
            y += slider_row(UID(0), x, y, w, "Barrel adjuster (turns out)", &sim.br.barrel, 0, 8, 1, "%.0f");
            y += slider_row(UID(0), x, y, w, "Inner pad dial (clicks in)", &sim.br.inner, 0, 8, 1, "%.0f");
        } else y += slider_row(UID(0), x, y, w, "Air in the system", &sim.br.air, 0, 1, 0.05f, "%.2f");
        {
            float bw = (w - S(12)) / 3;
            if (ui_button(UID(0), rect(x, y, bw, S(34)), "Oil on pads", sim.br.contam ? BTN_DANGER_ON : BTN_SECONDARY)) sim.br.contam = !sim.br.contam;
            if (ui_button(UID(0), rect(x + bw + S(6), y, bw, S(34)), "Bent rotor", sim.br.warp ? BTN_DANGER_ON : BTN_SECONDARY)) sim.br.warp = !sim.br.warp;
            if (ui_button(UID(0), rect(x + 2 * (bw + S(6)), y, bw, S(34)), "Reset", BTN_SECONDARY)) reset_bench_state(B_BRAKES);
            y += S(42);
        }
        break; }
    case B_WHEELS: {
        static const char *tt[] = { "Inner tubes", "Tubeless" }, *ter[] = { "Road", "Gravel", "Rough" };
        int tl = sim.wh.tubeless, te = (int)(sim.wh.terrain + 0.5f);
        y += slider_row(UID(0), x, y, w, "Rear tyre pressure", &sim.wh.psi, 18, 70, 1, "%.0f psi");
        y += slider_row(UID(0), x, y, w, "You + bike + kit", &sim.wh.kg, 50, 130, 1, "%.0f kg");
        y += label(x, y, "Surface");
        if (ui_segmented(UID(0), rect(x, y, w, S(34)), ter, 3, &te)) sim.wh.terrain = (float)te;
        y += S(42);
        if (ui_segmented(UID(0), rect(x, y, w, S(34)), tt, 2, &tl)) sim.wh.tubeless = tl;
        y += S(44);
        y += para(F_SMALL, x, y, w, "The freehub follows the ride controls. Stop pedalling while moving to watch the pawls click.", C_MUTED) + S(8);
        if (ui_button(UID(0), rect(x, y, w, S(34)), sim.r.pedaling ? "Stop pedalling" : "Pedal", BTN_SECONDARY)) { sim.r.pedaling = !sim.r.pedaling; if (sim.r.pedaling && sim.r.v < 0.5f) sim.r.v = 0.5f; }
        y += S(42);
        break; }
    case B_STEER: {
        const char *st = sim.st.preload < 30 ? "loose" : sim.st.preload > 75 ? "over-tight" : "right";
        d_text(F_BODY, x, y, "Top cap tightness", C_INK); d_text_right(F_BOLD, x + w, y, st, C_INK);
        ui_slider(UID(0), rect(x + S(4), y + font_line(F_BODY) + S(4), w - S(8), S(22)), &sim.st.preload, 0, 100, 1);
        y += font_line(F_BODY) + S(34);
        y += para(F_SMALL, x, y, w, "Only adjust with the stem bolts loose. The top cap sets preload; the stem bolts lock it. Threaded BB: the drive-side cup is reverse-threaded (turn clockwise to remove).", C_MUTED) + S(8);
        break; }
    }
    *py = y;
}

static void right_benches(Rect r) {
    Rect in = panel(r); Rect area = rect(in.x - S(4), in.y, in.w + S(8), in.h);
    const BenchInfo *b = &BENCHES[A.bench];
    float y = scroll_begin(UID(0), &A.sc_right, area), y0 = y, x = in.x, w = in.w;
    d_text(F_SMALL, x, y, "BENCH", C_ACCENT); y += font_line(F_SMALL) + S(2);
    y += para(F_H2, x, y, w, b->name, C_INK) + S(8);
    y += para(F_BODY, x, y, w, b->intro, C_MUTED) + S(14);
    if (A.bench == B_GEARING) y += diagram_draw(B_GEARING, x, y, w) + S(12);
    y += msg_list(x, y, w, A.bench) + S(10);
    bench_controls(x, &y, w);
    y += S(8);
    d_rect(rect(x, y, w, S(1)), C_LINE); y += S(14);
    y += card_list(x, y, w, b->howto_title, b->howto, 1);
    scroll_end(&A.sc_right, area, y - y0 + S(12));
}

static void right_fix(Rect r) {
    Rect in = panel(r); Rect area = rect(in.x - S(4), in.y, in.w + S(8), in.h);
    const Problem *p; float y = scroll_begin(UID(0), &A.sc_right, area), y0 = y, x = in.x, w = in.w, px; int i;
    static const char *sev[] = { "", "Minor", "Fix soon", "Stop and fix" };
    if (A.prob < 0) A.prob = 0;
    p = &PROBS[A.prob];
    px = x;
    px += ui_pill(px, y, sev[p->sev], p->sev) + S(6);
    px += ui_pill(px, y, p->diff, 4) + S(6);
    ui_pill(px, y, p->time, 4);
    y += font_line(F_SMALL) + S(12);
    y += para(F_H2, x, y, w, p->title, C_INK) + S(4);
    y += para(F_BODY, x, y, w, p->sub, C_MUTED) + S(14);
    {
        float bw = (w - S(6)) / 2;
        if (p->bench >= 0 && ui_button(UID(0), rect(x, y, bw, S(38)), "Recreate this fault", BTN_PRIMARY)) recreate_fault(A.prob);
        if (ui_button(UID(0), rect(p->bench >= 0 ? x + bw + S(6) : x, y, p->bench >= 0 ? bw : w, S(38)), "Show on the bike", BTN_SECONDARY)) {
            memset(A.fix_hl, 0, sizeof A.fix_hl);
            for (i = 0; p->parts[i] >= 0; i++) A.fix_hl[p->parts[i]] = 1;
            A.fix_hl_on = 1; cam_focus_part(p->parts[0]);
        }
        y += S(52);
    }
    y += card_list(x, y, w, "Likely causes, most common first", p->causes, 1) + S(10);
    d_text(F_BOLD, x, y, "Quick check", C_INK); y += font_line(F_BOLD) + S(4);
    y += para(F_BODY, x, y, w, p->check, C_INK) + S(14);
    y += card_list(x, y, w, "Fix", p->steps, 1) + S(10);
    d_text(F_BOLD, x, y, "Tools", C_INK); y += font_line(F_BOLD) + S(4);
    y += para(F_BODY, x, y, w, p->tools, C_INK) + S(14);
    {
        char buf[300]; float h;
        snprintf(buf, sizeof buf, "Take it to a shop if %s", p->shop);
        h = font_wrap(F_BODY, 0, 0, w - S(28), buf, C_INK, 0) + S(20);
        d_rrect(rect(x, y, w, h), S(8), C_BAD_SOFT);
        d_rect(rect(x, y + S(6), S(3), h - S(12)), C_BAD);
        para(F_BODY, x + S(16), y + S(10), w - S(28), buf, C_INK);
        y += h + S(14);
    }
    y += label(x, y, "Parts involved");
    {
        float cx = x;
        for (i = 0; p->parts[i] >= 0; i++) {
            const char *n = PARTS[p->parts[i]].name; float bw = font_width(F_BODY, n, -1) + S(20);
            if (cx + bw > x + w) { cx = x; y += S(34); }
            if (ui_button(UID(i), rect(cx, y, bw, S(28)), n, BTN_CHIP)) { select_part(p->parts[i], 1); break; }
            cx += bw + S(6);
        }
        y += S(40);
    }
    scroll_end(&A.sc_right, area, y - y0 + S(8));
}

/* --- overlays on the 3D view --- */
static float schem_height(int b) { return b == B_RD ? 360 : b == B_FD ? 330 : b == B_BRAKES ? 345 : b == B_WHEELS ? 320 : 380; }
static void overlay_layout(float dt) {
    float top = A.vp.y + S(52), bottom = A.vp.y + A.vp.h - S(64) - S(14), target, desired;
    A.schem = rect(0, 0, 0, 0);
    if (A.mode == M_SYSTEMS) bottom -= S(96);
    if (A.mode == M_BENCHES && A.bench != B_GEARING && !A.hide_schematic) {
        float cw = fminf(A.vp.w - S(28), S(660)), cs = (cw - S(28)) / 640.0f, ch = schem_height(A.bench) * cs + S(56);
        A.schem = rect(A.vp.x + (A.vp.w - cw) / 2, bottom - S(10) - ch, cw, ch);
        bottom = A.schem.y;
    }
    desired = (top + bottom) / 2;
    target = (A.vp.y + A.vp.h / 2 - desired) / (A.vp.h / 2);
    A.lens_ty += (target - A.lens_ty) * (1 - expf(-dt * 6));
}
static void schematic_card(void) {
    Rect r = A.schem;
    if (A.mode != M_BENCHES || A.bench == B_GEARING) return;
    if (A.hide_schematic) {
        Rect b = rect(A.vp.x + S(14), A.vp.y + A.vp.h - S(64) - S(14) - S(50), S(150), S(36));
        block(b);
        d_shadow(b, S(8), S(6), 0.10f);
        if (ui_button(UID(0), b, "Show schematic", BTN_SECONDARY)) A.hide_schematic = 0;
        return;
    }
    block(r);
    d_shadow(r, S(14), S(12), 0.10f);
    d_rrect_border(r, S(14), S(1), C_PANEL, C_LINE);
    {
        float tx = d_text(F_SMALL, r.x + S(16), r.y + S(14), "SCHEMATIC", C_ACCENT) + S(8);
        d_text(F_SMALL, tx, r.y + S(14), BENCHES[A.bench].name, C_MUTED);
    }
    if (ui_button(UID(0), rect(r.x + r.w - S(76), r.y + S(8), S(64), S(28)), "Hide", BTN_GHOST)) A.hide_schematic = 1;
    clip_push(r);
    diagram_draw(A.bench, r.x + S(14), r.y + S(44), r.w - S(28));
    clip_pop();
}

static void draw_labels(void) {
    int p, np = 0; float vcx = A.vp.x + A.vp.w / 2, vcy = A.vp.y + A.vp.h / 2;
    Rect placed[P_COUNT];
    for (p = 0; p < P_COUNT; p++) {
        float sx, sy, dx, dy, l, lx, ly, tw; int tries, i;
        if (!m4_project(A.mvp, bike_anchor(p), A.vp.x, A.vp.y, A.vp.w, A.vp.h, &sx, &sy)) continue;
        if (!in_rect(A.vp, sx, sy)) continue;
        dx = sx - vcx; dy = sy - vcy; l = sqrtf(dx * dx + dy * dy);
        if (l < 1) { dx = 0; dy = -1; l = 1; }
        lx = sx + dx / l * S(46); ly = sy + dy / l * S(30);
        tw = font_width(F_SMALL, PARTS[p].name, -1) + S(12);
        if (dx < 0) lx -= tw;
        for (tries = 0; tries < 14; tries++) {   /* nudge until it doesn't overlap an earlier label */
            Rect me = rect(lx, ly - S(11), tw, S(22)); int hit = 0;
            for (i = 0; i < np; i++) {
                Rect o = placed[i];
                if (me.x < o.x + o.w && o.x < me.x + me.w && me.y < o.y + o.h && o.y < me.y + me.h) { hit = 1; break; }
            }
            if (!hit) break;
            ly += (dy >= 0 ? 1 : -1) * S(12);
        }
        placed[np++] = rect(lx, ly - S(11), tw, S(22));
        d_line(sx, sy, dx < 0 ? lx + tw : lx, ly, S(1), calpha(C_INK, 0.35f));
        d_circle(sx, sy, S(3), C_ACCENT);
        d_rrect(rect(lx, ly - S(10), tw, S(20)), S(10), calpha(C_PANEL, 0.94f));
        d_text(F_SMALL, lx + S(6), ly - font_line(F_SMALL) / 2, PARTS[p].name, C_INK);
    }
}

static void topbar(void) {
    float W = (float)A.in.win_w, h = S(56), x;
    Rect bar = rect(0, 0, W, h);
    d_rect(bar, calpha(C_PANEL, 0.96f)); d_rect(rect(0, h - S(1), W, S(1)), C_LINE);
    block(bar);
    x = S(22);
    {   /* small wheel mark */
        float cy = h / 2;
        d_ring(x + S(9), cy, S(6.5f), S(9), C_INK); d_ring(x + S(29), cy, S(6.5f), S(9), C_INK);
        d_line(x + S(9), cy, x + S(19), cy - S(10), S(2), C_ACCENT); d_line(x + S(19), cy - S(10), x + S(29), cy, S(2), C_ACCENT);
        x += S(48);
    }
    x = d_text(F_TITLE, x, (h - font_line(F_TITLE)) / 2, "Diverge E5", C_INK) + S(6);
    d_text(F_TITLE, x, (h - font_line(F_TITLE)) / 2, "Workshop", C_MUTED);
    {
        int m = A.mode; float sw = S(440);
        if (ui_segmented(UID(0), rect((W - sw) / 2, S(10), sw, S(36)), MODE_NAMES, M_COUNT, &m)) set_mode(m);
    }
    {
        float bw = S(104), bx = W - S(16) - bw;
        if (ui_button(UID(0), rect(bx, S(11), bw, S(34)), "Reset view", BTN_SECONDARY)) cam_fly(cam_default().target, cam_default().dist, cam_default().yaw * 180 / PI_F, cam_default().pitch * 180 / PI_F);
        bx -= S(86);
        if (ui_button(UID(0), rect(bx, S(11), S(78), S(34)), "Labels", A.labels ? BTN_CHIP_ON : BTN_SECONDARY)) A.labels = !A.labels;
    }
}

static void dock(void) {
    float w = fminf(A.vp.w - S(24), S(760)), h = S(64), x = A.vp.x + (A.vp.w - w) / 2, y = A.vp.y + A.vp.h - h - S(14), cx, cy;
    Rect r = rect(x, y, w, h); char buf[64];
    block(r);
    d_shadow(r, S(16), S(12), 0.12f);
    d_rrect_border(r, S(16), S(1), C_PANEL, C_LINE);
    cx = x + S(12); cy = y + S(14);
    if (ui_button(UID(0), rect(cx, cy, S(112), S(36)), sim.r.pedaling ? "Stop pedalling" : "Pedal", sim.r.pedaling ? BTN_SECONDARY : BTN_PRIMARY)) {
        sim.r.pedaling = !sim.r.pedaling; if (sim.r.pedaling && sim.r.v < 0.5f) sim.r.v = 0.5f; }
    cx += S(122);
    snprintf(buf, sizeof buf, "%.0f rpm", sim.r.cadence);
    d_text(F_SMALL, cx, y + S(9), "Cadence", C_MUTED); d_text_right(F_SMALL, cx + S(120), y + S(9), buf, C_INK);
    ui_slider(UID(0), rect(cx + S(4), y + S(30), S(112), S(22)), &sim.r.cadence, 30, 120, 1);
    cx += S(136);
    {
        const char *rl[2]; char a[16], b[16]; int f = sim.r.front;
        snprintf(a, sizeof a, "%dT", RINGSETS[sim.r.ringset].teeth[0]); snprintf(b, sizeof b, "%dT", RINGSETS[sim.r.ringset].teeth[1]);
        rl[0] = a; rl[1] = b;
        if (ui_segmented(UID(0), rect(cx, cy, S(108), S(36)), rl, 2, &f)) sim.r.front = f;
        cx += S(116);
    }
    if (ui_button(UID(0), rect(cx, cy, S(34), S(36)), "−", BTN_SECONDARY) && sim.r.rear > 0) sim.r.rear--;
    snprintf(buf, sizeof buf, "%dT", cog_teeth());
    d_text_center(F_BOLD, rect(cx + S(34), cy, S(44), S(36)), buf, C_INK);
    if (ui_button(UID(0), rect(cx + S(78), cy, S(34), S(36)), "+", BTN_SECONDARY) && sim.r.rear < ncogs() - 1) sim.r.rear++;
    cx += S(122);
    {   /* brake: held button, held B key, or the braking run holding it on */
        int key = A.in.key_down[KEY_B] && !ui.focus;
        int scripted = A.mode == M_SYSTEMS && A.run == 2 && sim.r.brake >= 1;
        int held = ui_hold(UID(0), rect(cx, cy, S(98), S(36)), "Brake", key || scripted);
        sim.r.brake = (held || key || scripted) ? 1.0f : 0.0f;
        cx += S(108);
    }
    {
        int on = sim.r.slowmo < 0.5f;
        if (ui_button(UID(0), rect(cx, cy, x + w - S(12) - cx, S(36)), on ? "¼ speed" : "Real speed", on ? BTN_SECONDARY : BTN_CHIP_ON)) sim.r.slowmo = on ? 1.0f : 0.25f;
    }
}

static void readout(void) {
    char buf[160]; float x = A.vp.x + S(18), y = A.vp.y + S(16), px;
    snprintf(buf, sizeof buf, "%d×%d   ratio %.2f   %.1f m per crank turn   %.1f km/h",
             ring_teeth(), cog_teeth(), ratio(), ratio() * CIRC_M, sim.r.v * 3.6f);
    {   /* soft backdrop so the text stays readable over the bike */
        float tw = font_width(F_BODY, buf, -1) + S(10) + ui_pill_w("Freewheeling") + S(6) + (cross_chained(sim.r.front, sim.r.rear) ? ui_pill_w("Cross-chained") + S(6) : 0);
        float hint = font_width(F_SMALL, "Drag to orbit · right-drag to pan · scroll to zoom · double-click a part to fly to it", -1);
        d_rrect(rect(x - S(10), y - S(8), fmaxf(tw, hint) + S(20), font_line(F_BODY) + font_line(F_SMALL) + S(18)), S(10), calpha(C_PANEL, 0.82f));
    }
    px = d_text(F_BODY, x, y, buf, C_INK) + S(10);
    ui_pill(px, y + S(1), sim.r.v < 0.05f ? "Stopped" : sim.r.engaged ? "Driving" : "Freewheeling", sim.r.v < 0.05f ? 4 : sim.r.engaged ? 1 : 2);
    if (cross_chained(sim.r.front, sim.r.rear)) ui_pill(px + ui_pill_w("Freewheeling") + S(6), y + S(1), "Cross-chained", 2);
    d_text(F_SMALL, x, y + font_line(F_BODY) + S(2), "Drag to orbit · right-drag to pan · scroll to zoom · double-click a part to fly to it", C_FAINT);
}

static void caption_card(void) {
    const Run *r = &RUNS[A.run]; const Step *st = &r->steps[A.step];
    float w = fminf(A.vp.w - S(24), S(760)), x = A.vp.x + (A.vp.w - w) / 2, th, h, y; char buf[96];
    th = font_wrap(F_BODY, 0, 0, w - S(40), st->text, C_INK, 0);
    h = S(16) + font_line(F_SMALL) + S(4) + th + S(16);
    y = A.vp.y + A.vp.h - S(64) - S(14) - S(10) - h;
    block(rect(x, y, w, h));
    d_shadow(rect(x, y, w, h), S(14), S(10), 0.10f);
    d_rrect_border(rect(x, y, w, h), S(14), S(1), C_PANEL, C_LINE);
    snprintf(buf, sizeof buf, "%s · step %d of %d · %s", r->name, A.step + 1, r->n, PARTS[st->part].name);
    d_text(F_SMALL, x + S(20), y + S(14), buf, C_ACCENT);
    para(F_BODY, x + S(20), y + S(16) + font_line(F_SMALL) + S(2), w - S(40), st->text, C_INK);
    if (A.playing) {   /* progress */
        float t = clampf(A.step_t / 4.5f, 0, 1);
        d_rect(rect(x + S(14), y + h - S(3), (w - S(28)) * t, S(2)), C_ACCENT);
    }
}

/* -------------------------------------------------------------- input/keys */
static void handle_keys(void) {
    int i;
    if (ui.focus) return;   /* typing in the search box */
    if (A.in.key_pressed[KEY_SPACE]) { sim.r.pedaling = !sim.r.pedaling; if (sim.r.pedaling && sim.r.v < 0.5f) sim.r.v = 0.5f; }
    if (A.in.key_pressed[KEY_ESC]) { if (A.mode == M_SYSTEMS) A.playing = 0; else A.sel = -1; A.fix_hl_on = 0; }
    if (A.mode == M_SYSTEMS) {
        if (A.in.key_pressed[KEY_RIGHT]) { A.playing = 0; run_goto(A.step + 1); }
        if (A.in.key_pressed[KEY_LEFT]) { A.playing = 0; run_goto(A.step - 1); }
    }
    for (i = 0; i < A.in.ntext; i++) {
        unsigned c = A.in.text[i];
        if (c == '[' && sim.r.rear < ncogs() - 1) sim.r.rear++;
        if (c == ']' && sim.r.rear > 0) sim.r.rear--;
        if (c == '1') sim.r.front = 0;
        if (c == '2') sim.r.front = 1;
        if (c == 'l' || c == 'L') A.labels = !A.labels;
        if (c == 'r' || c == 'R') { Cam d = cam_default(); cam_fly(d.target, d.dist, d.yaw * 180 / PI_F, d.pitch * 180 / PI_F); }
        if ((c == 'f' || c == 'F') && A.sel >= 0) cam_focus_part(A.sel);
    }
}

static int mouse_over_ui(void) {
    int i; float mx = (float)A.in.mx, my = (float)A.in.my;
    for (i = 0; i < A.nblock; i++) if (in_rect(A.blockers[i], mx, my)) return 1;
    return !in_rect(A.vp, mx, my);
}

static void handle_view_input(void) {
    int over_ui = mouse_over_ui(); int b;
    if (!A.drag) {
        for (b = 0; b < 3; b++) if (A.in.pressed[b] && !over_ui && !ui.active) {
            A.drag = 1; A.drag_btn = b; A.drag_moved = 0; A.press_x = A.in.mx; A.press_y = A.in.my; A.press_hover = A.hover;
        }
    }
    if (A.drag) {
        if (!A.in.down[A.drag_btn]) {
            if (!A.drag_moved && A.drag_btn == MOUSE_L) {   /* click */
                int p = A.press_hover;
                if (p >= 0) {
                    if (A.mode == M_SYSTEMS) A.playing = 0;
                    select_part(p, A.in.dbl);
                } else if (A.mode == M_EXPLORE) A.sel = -1;
            }
            A.drag = 0;
        } else {
            if (abs(A.in.mx - A.press_x) + abs(A.in.my - A.press_y) > 3) A.drag_moved = 1;
            if (A.drag_moved) {
                A.cam_anim = 0;
                if (A.drag_btn == MOUSE_L) {
                    A.cam.yaw -= A.in.mdx * 0.006f;
                    A.cam.pitch = clampf(A.cam.pitch + A.in.mdy * 0.005f, DEG(-4), DEG(84));
                } else {
                    V3 eye = cam_eye(), f = v3norm(v3sub(A.cam.target, eye)), rt = v3norm(v3cross(f, v3(0, 1, 0))), up = v3cross(rt, f);
                    float k = A.cam.dist * 0.00095f / ui.s;
                    A.cam.target = v3add(A.cam.target, v3add(v3mul(rt, -A.in.mdx * k), v3mul(up, A.in.mdy * k)));
                }
            }
        }
    }
    if (A.in.dbl && !over_ui && A.hover >= 0) { select_part(A.hover, 1); }
    if (A.in.wheel != 0 && !over_ui) {
        float f = powf(0.88f, A.in.wheel);
        if (A.cam_anim) { A.goal.dist = clampf(A.goal.dist * f, 220, 7000); }
        A.cam.dist = clampf(A.cam.dist * f, 220, 7000);
    }
}

/* ------------------------------------------------------------------- frame */
static void layout(void) {
    float W = (float)A.in.win_w, H = (float)A.in.win_h, top = S(56), m = S(14);
    float lw = S(290), rw = S(380);
    A.vp = rect(m + lw, top, W - (2 * m + lw + rw), H - top);
    if (A.vp.w < 100) A.vp.w = 100;
    if (A.vp.h < 100) A.vp.h = 100;
}

static void frame(float dt) {
    float W = (float)A.in.win_w, H = (float)A.in.win_h, top = S(56), m = S(14);
    float lw = S(290), rw = S(380);
    Rect lp = rect(m, top + m, lw - m, H - top - 2 * m), rp = rect(W - rw, top + m, rw - m, H - top - 2 * m);

    layout();
    /* systems autoplay */
    if (A.mode == M_SYSTEMS && A.playing) {
        A.step_t += dt;
        if (A.step_t > 4.5f) { if (A.step < RUNS[A.run].n - 1) run_goto(A.step + 1); else { A.playing = 0; sim.r.brake = 0; } }
    }
    /* bench brake squeeze */
    if (A.mode == M_BENCHES && A.bench == B_BRAKES) {
        if (A.squeeze_hold > 0) sim.br.lever = fminf(1, sim.br.lever + dt * 2.2f);
    }
    sim_step(dt);
    cam_update(dt);
    overlay_layout(dt);
    cam_matrices();

    /* hover pick before drawing the frame (uses the back buffer) */
    A.hover = -1;
    if ((!A.drag || !A.drag_moved) && !mouse_over_ui()) A.hover = pick_at(A.in.mx, A.in.my);

    glViewport(0, 0, A.in.win_w, A.in.win_h);
    glClearColor(1, 1, 1, 1);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ui_begin(&A.in, A.in.dpi, A.t, A.in.win_w, A.in.win_h);
    draw_background();
    render_3d();
    ui_begin(&A.in, A.in.dpi, A.t, A.in.win_w, A.in.win_h);   /* restore 2D state */
    ui.hot = ui.last_hot;
    if (A.labels) draw_labels();
    readout();

    A.nblock = 0;
    block(lp); block(rp);
    if (A.mode == M_SYSTEMS) caption_card();
    schematic_card();
    dock();
    switch (A.mode) {
    case M_EXPLORE: left_explore(lp); right_explore(rp); break;
    case M_SYSTEMS: left_systems(lp); right_systems(rp); break;
    case M_BENCHES: left_benches(lp); right_benches(rp); break;
    case M_FIX: left_fix(lp); right_fix(rp); break;
    }
    topbar();
    if (A.hover >= 0 && !A.drag) { ui_tooltip((float)A.in.mx, (float)A.in.my, PARTS[A.hover].name); ui.want_cursor = CURSOR_HAND; }
    if (A.drag && A.drag_moved) ui.want_cursor = CURSOR_MOVE;
    handle_view_input();
    handle_keys();
    ui_end();
    plat_cursor(ui.want_cursor);
}

/* -------------------------------------------------------------- self-test */
static int write_ppm(const char *path, int w, int h) {
    unsigned char *px = (unsigned char *)mem_alloc((size_t)w * h * 3); FILE *f; int y;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    f = fopen(path, "wb");
    if (!f) { mem_free(px); return 0; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (y = h - 1; y >= 0; y--) fwrite(px + (size_t)y * w * 3, 1, (size_t)w * 3, f);
    fclose(f); mem_free(px);
    return 1;
}

typedef struct { int frame; const char *name; } Shot;
static void selftest_script(int f, const char **shot) {
    *shot = NULL;
    switch (f) {
    case 10: { int i; for (i = 0; i < 6; i++) { set_cassette((i + 2) % 4); set_ringset(i % 4); } set_cassette(1); set_ringset(2); } break;
    case 40: *shot = "01_explore"; break;
    case 41: select_part(P_RD, 1); break;
    case 39: {   /* picking self-check: the pixel under the saddle and rear tyre should identify them */
        static const int probe[] = { P_SADDLE, P_TIRES, P_FRAME };
        int i;
        for (i = 0; i < 3; i++) {
            float sx, sy; V3 at = i == 1 ? v3(0, 18, 0) : i == 2 ? v3(631, 517, 0) : bike_anchor(probe[i]);
            if (m4_project(A.mvp, at, A.vp.x, A.vp.y, A.vp.w, A.vp.h, &sx, &sy)) {
                int got = pick_at((int)sx, (int)sy);
                printf("pick at %s: %s\n", PARTS[probe[i]].name, got >= 0 ? PARTS[got].name : "(nothing)");
            }
        }
        break; }
    case 120: *shot = "02_inspect_rd"; break;
    case 121: start_run(0); break;
    case 200: *shot = "03_systems"; break;
    case 201: recreate_fault(0); break;
    case 280: *shot = "04_bench_rd"; break;
    case 281: recreate_fault(9); sim.br.lever = 0.7f; break;
    case 360: *shot = "05_bench_brakes"; break;
    case 361: recreate_fault(13); break;
    case 440: *shot = "06_bench_wheels"; break;
    case 441: recreate_fault(16); break;
    case 520: *shot = "07_bench_steer"; break;
    case 521: recreate_fault(6); break;
    case 600: *shot = "08_bench_fd"; break;
    case 601: open_bench(B_GEARING); break;
    case 680: *shot = "09_bench_gearing"; break;
    case 681: open_problem(0); strcpy(A.search, "brake"); break;
    case 700: A.search[0] = 0; break;
    case 701: { int i; for (i = 0; i < 8; i++) { set_cassette(i % 4); set_ringset((i + 1) % 4); set_build(i % 2); } } break;
    case 760: *shot = "10_fixit"; break;
    case 761: set_mode(M_EXPLORE); A.sel = -1; A.labels = 1; cam_fly(cam_default().target, cam_default().dist, 28, 12); break;
    case 840: *shot = "11_labels"; break;
    }
}

/* -------------------------------------------------------------------- main */
int app_main(int argc, char **argv) {
    int i, ok;
    double last; long steady_blocks = -1; int max_frames = 845;
    memset(&A, 0, sizeof A);
    A.sel = -1; A.hover = -1; A.prob = 0; A.run = 0; A.sysf = -1; A.bench = B_RD;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest")) { A.selftest = 1; A.shot_dir = i + 1 < argc ? argv[++i] : "."; }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atoi(argv[++i]);
    }
    if (!plat_init("Diverge E5 Workshop", 1480, 900, 1180, 700, &A.msaa, &A.stencil)) {
        fprintf(stderr, "Could not create an OpenGL window.\n");
        return 1;
    }
    {   /* first poll establishes size & dpi */
        plat_poll(&A.in);
    }
    if (A.msaa) glEnable(GL_MULTISAMPLE);
    ok = font_init(A.in.dpi);
    if (!ok) { fprintf(stderr, "Could not load a system font.\n"); plat_shutdown(); return 1; }
    sim_reset(0);
    bike_init();
    A.cam = cam_default(); A.goal = A.cam;
    if (A.selftest) printf("msaa=%d stencil=%d dpi=%.2f GL_RENDERER=%s\n", A.msaa, A.stencil, A.in.dpi, (const char *)glGetString(GL_RENDERER));

    last = plat_time();
    while (!A.in.quit) {
        double now = plat_time(); float dt = (float)(now - last);
        const char *shot = NULL;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (A.selftest) dt = 1.0f / 60;
        A.t += dt;
        plat_poll(&A.in);
        if (A.in.quit) break;
        if (A.selftest) selftest_script(A.frames, &shot);
        frame(dt);
        if (shot) {
            char path[512];
            snprintf(path, sizeof path, "%s/%s.ppm", A.shot_dir, shot);
            write_ppm(path, A.in.win_w, A.in.win_h);
            printf("shot %s  heap blocks=%ld bytes=%ld\n", path, mem_live_blocks(), mem_live_bytes());
            if (steady_blocks < 0) steady_blocks = mem_live_blocks();
        }
        plat_swap();
        A.frames++;
        if (A.selftest && A.frames > max_frames) A.in.quit = 1;
        if (!plat_vsync()) {   /* cap at ~60 fps without vsync */
            double spent = plat_time() - now;
            if (!A.selftest && spent < 1.0 / 60) plat_sleep(1.0 / 60 - spent);
        }
        if (!A.in.focused && !A.selftest) plat_sleep(0.03);
    }

    if (plat_window_alive()) {   /* GL objects; if the window was destroyed under us, the context teardown frees them */
        bike_free();
        font_free();
    }
    plat_shutdown();
    if (A.selftest) {
        printf("frames=%d  heap after start-up=%ld blocks  at exit=%ld blocks / %ld bytes  peak=%ld bytes\n",
               A.frames, steady_blocks, mem_live_blocks(), mem_live_bytes(), mem_peak_bytes());
        return mem_live_blocks() == 0 ? 0 : 2;
    }
    return 0;
}
