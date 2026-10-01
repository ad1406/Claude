#include "diagrams.h"
#include "sim.h"
#include "content.h"
#include "mathx.h"
#include <stdio.h>
#include <string.h>

/* Virtual canvas: diagrams are authored in a fixed coordinate space and
   scaled to the panel width. */
static float CX, CY, CS;
#define VX(v) (CX + (v) * CS)
#define VY(v) (CY + (v) * CS)
static void vrect(float x, float y, float w, float h, Color c) { d_rect(rect(VX(x), VY(y), w * CS, h * CS), c); }
static void vrrect(float x, float y, float w, float h, float r, Color c) { d_rrect(rect(VX(x), VY(y), w * CS, h * CS), r * CS, c); }
static void vline(float x0, float y0, float x1, float y1, float w, Color c) { d_line(VX(x0), VY(y0), VX(x1), VY(y1), w * CS, c); }
static void vcirc(float x, float y, float r, Color c) { d_circle(VX(x), VY(y), r * CS, c); }
static void vring(float x, float y, float r0, float r1, Color c) { d_ring(VX(x), VY(y), r0 * CS, r1 * CS, c); }
static void vtext(float x, float y, const char *s, Color c, int align) {   /* align: 0 left, 1 centre, 2 right; y = centre line */
    float w = font_width(F_SMALL, s, -1), px = VX(x), py = VY(y) - font_line(F_SMALL) / 2;
    if (align == 1) px -= w / 2; else if (align == 2) px -= w;
    d_text(F_SMALL, px, py, s, c);
}
static void vpoly(const float *p, int n, Color c) {
    float q[32]; int i; for (i = 0; i < n && i < 16; i++) { q[i * 2] = VX(p[i * 2]); q[i * 2 + 1] = VY(p[i * 2 + 1]); }
    d_poly(q, n, c);
}

static float rd_diagram(void) {
    RDCalc c = rd_calc(); const Cassette *cs = &CASSETTES[sim.r.cassette];
    float k = 9, base = 470; int i;
    float spokeX = base - (c.Lstop > (c.n - 1) * c.sp ? (c.n - 1) * c.sp + 9 : (c.n - 1) * c.sp + 9) * k;
    float gpX = base - c.pos * k, gpY = 225, tpY = 290;
    float bend = DEG(sim.rd.hanger);
    float hx = base + 6 * k + 6, hy = 112, hbx = hx - sinf(bend) * 30, hby = hy + cosf(bend) * 30;
    int bad = fabsf(c.err) > c.sp * 0.5f, warn = fabsf(c.err) > 0.35f;
#define XM(mm) (base - (mm) * k)
    vrect(spokeX - 26, 14, 9, 320, C_SOFT); vtext(spokeX - 22, 344, "spokes", C_MUTED, 1);
    vrrect(XM(-6) - 4, 36, 22, 78, 3, rgb(0x3D5C52)); vtext(XM(-6) + 7, 26, "dropout", C_MUTED, 1);
    vline(spokeX - 17, 76, XM(-6), 76, 5, C_FAINT);
    for (i = 0; i < cs->n; i++) {
        float h = pitch_radius(cs->cogs[i]) * 1.35f; char t[8];
        vrrect(XM(i * c.sp) - 2.2f, 76 - h / 2, 4.4f, h, 1.5f, i == sim.r.rear ? C_INK : C_FAINT);
        snprintf(t, sizeof t, "%d", cs->cogs[i]);
        vtext(XM(i * c.sp), 76 + h / 2 + 10, t, C_MUTED, 1);
    }
    vline(hx, hy, hbx, hby, 9, C_FAINT);
    vtext(hx - 12, hy + 20, sim.rd.hanger > 0 ? "bent hanger" : "hanger", sim.rd.hanger > 0 ? C_BAD : C_MUTED, 2);
    {
        float k1x = hbx, k1y = hby + 4, k2y = hby + 26, m1x = gpX + 18, m1y = gpY - 46, m2y = gpY - 24, j;
        vline(k1x, k1y, m1x, m1y, 6, C_MUTED); vline(k1x, k2y, m1x, m2y, 6, C_MUTED);
        vline(k1x, k1y, k1x, k2y, 4, C_INK); vline(m1x, m1y, m1x, m2y, 4, C_INK);
        for (j = 0; j < 9; j++) {   /* spring zigzag */
            float t0 = j / 9, t1 = (j + 1) / 9;
            float x0 = k1x - 4 + (m1x - k1x + 4) * t0, x1 = k1x - 4 + (m1x - k1x + 4) * t1;
            float y0 = k1y + 10 + (m2y - k1y - 10) * t0 + ((int)j % 2 ? 5 : -5), y1 = k1y + 10 + (m2y - k1y - 10) * t1 + ((int)(j + 1) % 2 ? 5 : -5);
            vline(x0, y0, x1, y1, 1.4f, C_TEAL);
        }
        vtext((k1x + m1x) / 2, (k1y + m2y) / 2 + 30, "spring pulls out", C_TEAL, 1);
        vline(560, 150, m1x + 8, m1y + 10, 2.5f, C_ACCENT);
        vrrect(548, 144, 24, 12, 3, C_MUTED);
        vtext(600, 128, "cable pulls in", C_ACCENT, 1);
    }
    vline(gpX, gpY - 22, gpX, tpY + 22, 3, C_MUTED);
    vrrect(gpX - 4, gpY - 14, 8, 28, 3, C_INK); vrrect(gpX - 4, tpY - 14, 8, 28, 3, C_INK);
    vtext(gpX + 10, gpY, "guide pulley", C_MUTED, 0); vtext(gpX + 10, tpY, "tension pulley", C_MUTED, 0);
    vline(XM(sim.r.rear * c.sp), 76 + pitch_radius(cog_teeth()) * 0.68f, gpX, gpY - 14, 5, bad ? C_BAD : warn ? C_WARN : C_ACCENT);
    vline(gpX, gpY + 14, gpX, tpY - 14, 5, calpha(C_ACCENT, 0.8f));
    {
        float sy = 334;
        vline(XM(-4), sy, XM((c.n - 1) * c.sp + 4), sy, 2, C_LINE);
        for (i = 0; i < c.n; i++) vline(XM(i * c.sp), sy - 4, XM(i * c.sp), sy + 4, 1, C_FAINT);
        vrect(XM(c.Hstop) - 2, sy - 14, 4, 22, C_BAD); vtext(XM(c.Hstop) + 5, sy - 18, "H", C_BAD, 0);
        vrect(XM(c.Lstop) - 2, sy - 14, 4, 22, C_BAD); vtext(XM(c.Lstop) - 5, sy - 18, "L", C_BAD, 2);
        { float p[6] = { gpX, sy - 18, gpX - 5, sy - 26, gpX + 5, sy - 26 }; vpoly(p, 3, C_ACCENT); }
        vtext(XM(-4), sy + 16, "stops", C_MUTED, 0);
    }
#undef XM
    return 360;
}

static float fd_diagram(void) {
    FDCalc c = fd_calc(); float k = 17;
    float half = c.clear + 3.5f, topY, cy; int bad = fabsf(c.d) > c.clear, f; char t[64];
#define XM(mm) (60 + ((mm) - 34) * k)
    vtext(XM(34), 14, "Rear view. Scale: mm from the frame centreline", C_MUTED, 0);
    vline(XM(34), 300, XM(56), 300, 1, C_LINE);
    for (f = 36; f <= 54; f += 2) { snprintf(t, sizeof t, "%d", f); vtext(XM(f), 314, t, C_FAINT, 1); }
    for (f = 0; f < 2; f++) {
        int teeth = RINGSETS[sim.r.ringset].teeth[f]; float h = pitch_radius(teeth) * 1.3f, mm = ring_z(f);
        vrrect(XM(mm) - k, 270 - h, 2 * k, h, 2, f == sim.r.front ? C_INK : C_FAINT);
        snprintf(t, sizeof t, "%dT", teeth); vtext(XM(mm) - k - 6, 262, t, C_MUTED, 2);
    }
    topY = 270 - pitch_radius(ring_teeth()) * 1.3f;
    {
        float cw = 3.5f * k; float p[8];
        p[0] = XM(c.ring_pos) - cw; p[1] = topY; p[2] = XM(c.chain_at) - cw; p[3] = topY - 70;
        p[4] = XM(c.chain_at) + cw; p[5] = topY - 70; p[6] = XM(c.ring_pos) + cw; p[7] = topY;
        vpoly(p, 4, calpha(bad ? C_BAD : C_ACCENT, 0.85f));
        snprintf(t, sizeof t, "chain to the %dT cog", cog_teeth());
        vtext(XM(c.chain_at), topY - 84, t, C_MUTED, 1);
    }
    cy = topY - 40;
    vrect(XM(c.cage - half) - 3, cy - 34, 6, 58, C_MUTED); vrect(XM(c.cage + half) - 3, cy - 34, 6, 58, C_MUTED);
    vrect(XM(c.cage - half) - 3, cy - 34, 2 * half * k + 6, 7, C_MUTED);
    vtext(XM(c.cage - half) - 8, cy + 10, "inner plate", C_MUTED, 2); vtext(XM(c.cage + half) + 8, cy + 10, "outer plate", C_MUTED, 0);
    vrect(XM(c.Lstop) - 2, 278, 4, 16, C_BAD); vtext(XM(c.Lstop) + 5, 286, "L", C_BAD, 0);
    vrect(XM(c.Hstop) - 2, 278, 4, 16, C_BAD); vtext(XM(c.Hstop) + 5, 286, "H", C_BAD, 0);
    { float p[6] = { XM(c.cage), 296, XM(c.cage) - 5, 288, XM(c.cage) + 5, 288 }; vpoly(p, 3, C_ACCENT); }
#undef XM
    return 330;
}

static float brake_diagram(void) {
    BRCalc c = br_calc(sim.br.lever); int mech = !sim.br.hydro;
    float k = 40, RX = 320, wob = sim.br.warp ? sinf(sim.rotor_phase) * 0.55f : 0;
    float flex = mech ? fminf(c.gi, fmaxf(0, c.pad_move - c.go)) : 0;
    float rotorX = RX + (wob - flex) * k;
    float mat = fmaxf(0.15f, sim.br.pad) * k * 0.6f, back = 12;
    float iF = mech ? RX - 4 - c.gi * k : fminf(RX - 4 - c.gi * k + c.pad_move / 2 * k, rotorX - 4);
    float oF = fmaxf(mech ? RX + 4 + c.go * k - c.pad_move * k : RX + 4 + c.go * k - c.pad_move / 2 * k, rotorX + 4);
    Color pad = sim.br.contam ? C_WARN : (c.force > 0 ? C_BAD : rgb(0x2A2F31));
    vtext(16, 14, "hub side", C_MUTED, 0); vtext(624, 14, "outside", C_MUTED, 2);
    vrrect(RX - 170, 60, 340, 190, 16, C_SOFT);
    vtext(RX - 160, 50, "caliper body", C_MUTED, 0);
    vrrect(rotorX - 4, 30, 8, 250, 2, rgb(0x9AA5A8)); vtext(rotorX + 10, 36, "rotor", C_MUTED, 0);
    vrect(iF - mat, 100, mat, 110, pad); vrect(iF - mat - back, 94, back, 122, C_MUTED);
    vrect(oF, 100, mat, 110, pad); vrect(oF + mat, 94, back, 122, C_MUTED);
    vtext(iF - mat - back, 84, "inner pad", C_MUTED, 0); vtext(oF, 84, "outer pad", C_MUTED, 0);
    if (mech) {
        float ax = oF + mat + back + 30, ang = DEG(-30 + sim.br.lever * 55 - 90);
        vrect(iF - mat - back - 40, 146, 40, 18, C_FAINT); vcirc(iF - mat - back - 50, 155, 16, C_MUTED);
        vtext(iF - mat - back - 50, 186, "pad dial", C_MUTED, 1);
        vrect(oF + mat + back, 147, 30, 16, C_FAINT);
        vcirc(ax, 155, 14, C_MUTED);
        vline(ax, 155, ax + cosf(ang) * 70, 155 + sinf(ang) * 70, 8, C_INK);
        vline(ax + cosf(ang) * 70, 155 + sinf(ang) * 70, 630, 40, 2.5f, C_ACCENT);
        vtext(ax, 192, "arm + cam", C_MUTED, 1); vtext(600, 60, "cable", C_ACCENT, 1);
    } else {
        int i, nb = (int)(sim.br.air * 6 + 0.5f);
        vrect(iF - mat - back - 36, 125, 36, 60, calpha(C_TEAL, 0.5f)); vrect(oF + mat + back, 125, 36, 60, calpha(C_TEAL, 0.5f));
        vtext(iF - mat - back - 18, 196, "piston", C_MUTED, 1); vtext(oF + mat + back + 18, 196, "piston", C_MUTED, 1);
        vline(oF + mat + back + 36, 155, 600, 155, 6, calpha(C_TEAL, 0.7f)); vline(600, 155, 630, 40, 6, calpha(C_TEAL, 0.7f));
        vtext(540, 60, "oil from lever", C_TEAL, 1);
        for (i = 0; i < nb; i++) { float bx = oF + mat + back + 50 + i * 18; vcirc(bx, 155, 4, C_PANEL); vring(bx, 155, 3.2f, 4.5f, C_TEAL); }
    }
    {
        float gx = 30, gw = 580, gy = 300, biteX = gx + fminf(1, c.bite) * gw, barX = gx + 0.9f * gw;
        vtext(gx, gy - 6, "lever travel", C_MUTED, 0);
        vrrect(gx, gy + 4, gw, 8, 4, C_LINE);
        vrrect(gx, gy + 4, fmaxf(8, sim.br.lever * gw), 8, 4, C_ACCENT);
        vrect(biteX - 1.5f, gy - 2, 3, 20, C_OK); vtext(biteX, fabsf(biteX - barX) < 40 ? gy - 10 : gy + 28, "bite", C_OK, 1);
        vrect(barX - 1.5f, gy - 2, 3, 20, C_BAD); vtext(barX, gy + 28, "bar", C_BAD, 1);
    }
    return 345;
}

static float wheel_diagram(void) {
    WHCalc c = wh_calc();
    float k = 3.4f, cx = 150, gy = 220, sag = fminf(28, c.sag / 100 * 36 * k * 1.2f);
    float top = gy - 36 * k + sag - 6, hw = 19 * k * (1 + sag / 140), rimw = 10.5f * k;
    int i;
    vtext(cx, 14, sim.wh.tubeless ? "tyre section, tubeless" : "tyre section, with inner tube", C_MUTED, 1);
    vrrect(cx - rimw - 6, top - 30, rimw * 2 + 12, 40, 6, C_FAINT);
    {   /* tyre outline as polyline */
        float pts[40][2]; int n = 0, j;
        for (j = 0; j <= 18; j++) {
            float t = (float)j / 18 * PI_F;   /* half ellipse from left bead round the bottom to right bead */
            float x = cx - cosf(t) * hw, y = top + sinf(t) * (gy - top);
            if (y > gy) y = gy;
            if (j == 0) x = cx - rimw;
            if (j == 18) x = cx + rimw;
            pts[n][0] = x; pts[n][1] = y; n++;
        }
        for (j = 0; j < n - 1; j++) vline(pts[j][0], pts[j][1], pts[j + 1][0], pts[j + 1][1], 7, rgb(0x2A2F31));
        if (!sim.wh.tubeless) for (j = 1; j < n - 2; j += 2) vline(pts[j][0] + (cx - pts[j][0]) * 0.12f, pts[j][1] - 7, pts[j + 1][0] + (cx - pts[j + 1][0]) * 0.12f, pts[j + 1][1] - 7, 2, C_ACCENT);
        else vline(cx - hw * 0.5f, gy - 9, cx + hw * 0.5f, gy - 9, 4, calpha(C_TEAL, 0.6f));
    }
    vline(10, gy + 4, 290, gy + 4, 3, C_LINE);
    {
        char t[64]; float fl = fminf(140, c.len * 0.9f), fw = 38 * 0.8f * 0.9f; int j;
        snprintf(t, sizeof t, "tyre sag about %.0f%%", c.sag); vtext(cx, gy + 20, t, C_MUTED, 1);
        for (j = 0; j < 2; j++) {   /* footprint ellipse */
            float pts[64]; int q;
            for (q = 0; q < 32; q++) { float a = (float)q / 32 * 2 * PI_F; pts[q * 2] = VX(cx + cosf(a) * fl / 2); pts[q * 2 + 1] = VY(275 + sinf(a) * fw / 2); }
            if (j == 0) d_poly(pts, 32, C_ACCENT_SOFT);
        }
        snprintf(t, sizeof t, "footprint %.0f × 30 mm", c.len); vtext(cx, 305, t, C_MUTED, 1);
    }
    {   /* freehub */
        float hx = 470, hy = 140, R1 = 96, R2 = 76, ha = DEG(sim.hub_a), ba = DEG(sim.body_a);
        float lift = sim.r.engaged ? 0 : fabsf(sinf(sim.pawl_phase)) * 0.5f;
        vcirc(hx, hy, R1, C_SOFT); vring(hx, hy, R1 - 2, R1, C_FAINT);
        for (i = 0; i < 24; i++) {
            float a = ha + (float)i / 24 * 2 * PI_F; float p[6];
            p[0] = hx + cosf(a) * (R2 + 2); p[1] = hy + sinf(a) * (R2 + 2);
            p[2] = hx + cosf(a + 0.12f) * (R2 + 12); p[3] = hy + sinf(a + 0.12f) * (R2 + 12);
            p[4] = hx + cosf(a - 0.13f) * (R2 + 12); p[5] = hy + sinf(a - 0.13f) * (R2 + 12);
            vpoly(p, 3, C_MUTED);
        }
        vcirc(hx, hy, R2 - 8, C_PANEL); vring(hx, hy, R2 - 9, R2 - 7, C_INK);
        for (i = 0; i < 3; i++) {
            float a = ba + (float)i / 3 * 2 * PI_F;
            vline(hx + cosf(a) * (R2 - 10), hy + sinf(a) * (R2 - 10), hx + cosf(a + 0.32f) * (R2 + 6 - lift * 14), hy + sinf(a + 0.32f) * (R2 + 6 - lift * 14), 7, C_ACCENT);
        }
        vcirc(hx, hy, 10, C_MUTED);
        vtext(hx, 14, sim.r.v < 0.05f ? "freehub: stopped" : sim.r.engaged ? "freehub: pawls engaged, driving" : "freehub: pawls clicking, coasting", C_MUTED, 1);
        vtext(hx, 262, "grey ring: hub shell, turns with the wheel", C_MUTED, 1);
        vtext(hx, 280, "white body: freehub, turns with the cassette", C_MUTED, 1);
    }
    return 320;
}

static float steer_diagram(void) {
    static const char *names[] = { "Top cap & bolt", "Stem", "Spacers", "Dust cover", "Compression ring", "Upper bearing", "Head tube", "Lower bearing", "Crown race" };
    static const float ys[] = { 30, 60, 110, 142, 160, 182, 210, 312, 336 }, ws[] = { 60, 80, 58, 74, 50, 64, 70, 80, 84 }, hs[] = { 10, 40, 22, 8, 12, 16, 90, 16, 8 };
    float p = sim.st.preload, gap = fmaxf(0, (40 - p) / 40) * 10; int bind = p > 75, i;
    float cx = 150;
    vrect(cx - 12, 24, 24, 330, C_SOFT); vtext(cx, 366, "fork steerer", C_MUTED, 1);
    for (i = 0; i < 9; i++) {
        float yy = i < 6 ? ys[i] - (6 - i) * gap * 0.6f : ys[i];
        Color col = i == 0 ? C_INK : i == 4 ? C_ACCENT : i == 6 ? rgb(0x3D5C52) : (i == 5 || i == 7) ? (bind ? C_BAD : p < 30 ? C_WARN : C_FAINT) : C_MUTED;
        vrrect(cx - ws[i] / 2, yy, ws[i], hs[i], 3, col);
        vline(cx + ws[i] / 2 + 4, yy + hs[i] / 2, cx + 70, yy + hs[i] / 2, 1, C_LINE);
        vtext(cx + 74, yy + hs[i] / 2, names[i], C_MUTED, 0);
    }
    {
        float bx = 470, by = 190;
        vtext(bx, by - 46, "BB shell 68 mm, threaded", C_MUTED, 1);
        vrrect(bx - 70, by - 30, 140, 60, 6, rgb(0x3D5C52));
        vrrect(bx - 96, by - 38, 26, 76, 4, C_FAINT); vrrect(bx + 70, by - 38, 26, 76, 4, C_FAINT);
        vrrect(bx - 130, by - 8, 260, 16, 4, C_MUTED);
        vtext(bx - 83, by + 54, "left cup", C_MUTED, 1); vtext(bx - 83, by + 68, "normal thread", C_MUTED, 1);
        vtext(bx + 83, by + 54, "drive-side cup", C_MUTED, 1); vtext(bx + 83, by + 68, "reverse thread", C_ACCENT, 1);
        vtext(bx, by, "24 mm axle", C_PANEL, 1);
    }
    return 380;
}

/* Gear table: interactive, ring rows × cog columns */
static float gear_diagram(float x, float y, float w) {
    const Cassette *cs = &CASSETTES[sim.r.cassette]; int f, i;
    float hw = S(56), cw = (w - hw) / cs->n, rh = S(34), yy = y;
    char t[32];
    d_rrect(rect(x, y, w, rh * 3), S(8), C_SOFT);
    d_text(F_SMALL, x + S(10), yy + (rh - font_line(F_SMALL)) / 2, "Ring / cog", C_MUTED);
    for (i = 0; i < cs->n; i++) { snprintf(t, sizeof t, "%d", cs->cogs[i]); d_text_center(F_SMALL, rect(x + hw + cw * i, yy, cw, rh), t, C_MUTED); }
    for (f = 1; f >= 0; f--) {
        yy += rh;
        snprintf(t, sizeof t, "%dT", RINGSETS[sim.r.ringset].teeth[f]);
        d_text(F_BOLD, x + S(10), yy + (rh - font_line(F_BOLD)) / 2, t, C_INK);
        for (i = 0; i < cs->n; i++) {
            Rect c = rect(x + hw + cw * i + S(1), yy + S(1), cw - S(2), rh - S(2));
            int cur = f == sim.r.front && i == sim.r.rear, cross = cross_chained(f, i);
            if (ui_row(UID(f * 16 + i), c, cur)) { sim.r.front = f; sim.r.rear = i; }
            if (cross && !cur) {
                int s; clip_push(c);
                for (s = -4; s < 10; s++) d_line(c.x + s * S(8), c.y + c.h, c.x + s * S(8) + c.h, c.y, S(1), C_LINE);
                clip_pop();
            }
            snprintf(t, sizeof t, "%.1f", (float)RINGSETS[sim.r.ringset].teeth[f] / cs->cogs[i] * CIRC_M);
            d_text_center(cur ? F_BOLD : F_SMALL, c, t, cur ? C_ACCENT : (cross ? C_FAINT : C_INK));
        }
    }
    return rh * 3;
}

float diagram_draw(int b, float x, float y, float w) {
    float vw = 640;
    if (b == B_GEARING) return gear_diagram(x, y, w);
    CS = w / vw; CX = x; CY = y;
    switch (b) {
    case B_RD: return rd_diagram() * CS;
    case B_FD: return fd_diagram() * CS;
    case B_BRAKES: return brake_diagram() * CS;
    case B_WHEELS: return wheel_diagram() * CS;
    case B_STEER: return steer_diagram() * CS;
    }
    return 0;
}

#define ADD(k, ...) do { if (n < max) { out[n].kind = (k); snprintf(out[n].text, sizeof out[n].text, __VA_ARGS__); n++; } } while (0)
int bench_messages(int b, Msg *out, int max) {
    int n = 0;
    if (b == B_GEARING) {
        float ang = atan2f(cog_z(sim.r.rear) - ring_z(sim.r.front), 425) * 180 / PI_F;
        int cr = cross_chained(sim.r.front, sim.r.rear);
        if (cr) ADD(2, "Cross-chained: the chain runs %.1f° off straight. Expect derailleur rub and faster wear. The %s ring has a similar gear with a straighter line.", fabsf(ang), sim.r.front ? "small" : "big");
        else ADD(1, "Chain line %.1f° off straight. Fine.", fabsf(ang));
        ADD(0, "At %.0f rpm this gear does %.1f km/h. Easiest gear: %.1f m per crank turn; hardest: %.1f m.", sim.r.cadence, v_drive() * 3.6f,
            (float)RINGSETS[sim.r.ringset].teeth[0] / CASSETTES[sim.r.cassette].cogs[ncogs() - 1] * CIRC_M,
            (float)RINGSETS[sim.r.ringset].teeth[1] / CASSETTES[sim.r.cassette].cogs[0] * CIRC_M);
    } else if (b == B_RD) {
        RDCalc c = rd_calc(); float e = c.err;
        if (fabsf(e) <= 0.35f) ADD(1, "Guide pulley within %.2f mm of the %dT cog. Quiet, crisp shifts.", fabsf(e), cog_teeth());
        else if (fabsf(e) <= c.sp * 0.5f) {
            if (e < 0) ADD(2, "Pulley %.1f mm outboard of the cog (cable slack). The chain rattles toward the smaller cog and hesitates going to bigger cogs. Fix: barrel adjuster out.", -e);
            else ADD(2, "Pulley %.1f mm inboard of the cog (cable tight). The chain rattles toward the bigger cog and drops to smaller cogs late. Fix: barrel adjuster in.", e);
        } else ADD(3, "Pulley %.1f mm off. The chain wants to sit on the neighbouring cog, so it skips or changes gear by itself under load.", fabsf(e));
        if (c.Hstop < -1.2f) ADD(3, "H limit too loose: on the smallest cog the chain can be thrown off into the gap next to the frame.");
        if (c.Hstop > 0.4f) ADD(2, "H limit too tight: the pulley can't reach the smallest cog.");
        if (c.Lstop > (c.n - 1) * c.sp + 1.2f) ADD(3, "L limit too loose: the chain can overshoot the biggest cog into the spokes.");
        if (c.Lstop < (c.n - 1) * c.sp - 0.4f) ADD(2, "L limit too tight: the derailleur can't reach the biggest cog.");
        if (sim.rd.hanger > 0) ADD(2, "Bent hanger adds %.1f mm of error on this cog, and more on bigger cogs. Barrel adjustments can't fix every cog at once.", sim.rd.hanger * 0.28f * (1 + 0.16f * sim.r.rear));
    } else if (b == B_FD) {
        FDCalc c = fd_calc(); int bad = fabsf(c.d) > c.clear;
        if (!bad) ADD(1, "Chain clears both cage plates (%.1f mm to spare).", c.clear - fabsf(c.d));
        else ADD(2, "Chain rubs the %s plate. %s", c.d < 0 ? "inner" : "outer",
                 cross_chained(sim.r.front, sim.r.rear) ? "You are cross-chained: try the other ring." :
                 (sim.r.front == 1 && c.d < 0 && !sim.fd.trim) ? "Try the trim click." : "Adjust cable tension or the limit screw.");
        if (sim.r.front == 0 && c.Lstop < 39) ADD(3, "L limit too loose: the cage swings past the small ring and can drop the chain onto the frame.");
        if (sim.r.front == 1 && c.Hstop > 52) ADD(3, "H limit too loose: the cage can throw the chain off the outside of the big ring.");
        if (sim.r.front == 1 && c.cage < 47.2f && !sim.fd.trim) ADD(2, "Cable too slack or H too tight: the cage can't fully lift the chain to the big ring.");
    } else if (b == B_BRAKES) {
        BRCalc c = br_calc(sim.br.lever); int bite = (int)(c.bite * 100 + 0.5f);
        if (c.bite >= 0.9f) ADD(3, "The lever reaches the bar before the pads even touch the rotor. No braking. Do not ride.");
        else if (c.bite + 0.15f >= 0.9f) ADD(3, "Bite point at %d%% of lever travel: the lever will reach the bar before full power. Unsafe.", bite);
        else if (c.bite < 0.5f) ADD(1, "Bite point at %d%% of lever travel.", bite);
        else ADD(2, "Bite point at %d%% of lever travel. Getting long; adjust soon.", bite);
        if (sim.br.lever > 0) ADD(c.force > 0.6f ? 1 : 0, "Clamping force %d%%, stopping power %d%%.", (int)(c.force * 100), (int)(c.force * (sim.br.contam ? 35 : 100)));
        if (sim.br.pad < 0.5f) ADD(3, "Pads below 0.5 mm: replace now. The metal backing plates will score the rotor.");
        if (!sim.br.hydro && c.gi <= 0.06f) ADD(2, "The inner pad is touching the rotor and will drag. Back the dial off one click.");
        if (sim.br.hydro && sim.br.air > 0.2f) ADD(2, "Air compresses, so the lever feels spongy and the bite point drifts. Bleed the brake.");
        if (sim.br.contam) ADD(3, "Contaminated pads: loud, weak braking. Replace the pads and clean the rotor with isopropyl alcohol.");
        if (sim.br.warp) ADD(2, "Bent rotor: it touches the pads once per turn (tick-tick) and pulses under braking. Watch it wobble in the 3D view.");
    } else if (b == B_WHEELS) {
        WHCalc c = wh_calc(); float r = sim.wh.psi / c.guide;
        if (r < 0.8f) { if (sim.wh.tubeless) ADD(2, "Very low: the tyre can fold in corners and burp air, and rim strikes can dent the rim."); else ADD(3, "Too low for inner tubes: high pinch-flat (snakebite) risk on rocks and edges."); }
        else if (r > 1.3f) ADD(2, "Over-inflated for this load: harsh, less grip on gravel, and slower on rough ground.");
        else ADD(1, "In a good range: grippy, comfortable, low flat risk.");
        ADD(0, "Suggested: %.0f psi rear, %.0f psi front. Contact patch %.0f mm² (shown in orange under the rear tyre). Never exceed the maximum printed on the tyre or rim.", c.guide, c.guide_f, c.area);
    } else if (b == B_STEER) {
        float p = sim.st.preload;
        if (p < 30) ADD(3, "Play in the bearings: knocks over bumps, clunks under front braking, and wears the bearing seats.");
        else if (p > 75) ADD(2, "Bearings squeezed too hard: steering feels stiff and notchy, and the bike wanders at low speed.");
        else ADD(1, "No play, free rotation. Now tighten the stem bolts to lock it.");
    }
    return n;
}
