#include "sim.h"
#include "mathx.h"
#include <string.h>

const RingSet RINGSETS[4] = { {"50/34", {34, 50}}, {"46/34", {34, 46}}, {"46/32", {32, 46}}, {"46/30", {30, 46}} };
const Cassette CASSETTES[4] = {
    {"8sp 11-32", 8, {11, 13, 15, 18, 21, 24, 28, 32}, 4.8f},
    {"8sp 11-34", 8, {11, 13, 15, 18, 21, 24, 28, 34}, 4.8f},
    {"10sp 11-34", 10, {11, 13, 15, 17, 19, 21, 23, 26, 30, 34}, 4.35f},
    {"10sp 11-36", 10, {11, 13, 15, 17, 19, 21, 24, 28, 32, 36}, 4.35f},
};
Sim sim;

float pitch_radius(int t) { return PITCH_MM / (2.0f * sinf(PI_F / t)); }
int ncogs(void) { return CASSETTES[sim.r.cassette].n; }
int ring_teeth(void) { return RINGSETS[sim.r.ringset].teeth[sim.r.front]; }
int cog_teeth(void) { return CASSETTES[sim.r.cassette].cogs[sim.r.rear]; }
float ratio(void) { return (float)ring_teeth() / (float)cog_teeth(); }
float v_drive(void) { return sim.r.cadence / 60.0f * ratio() * CIRC_M; }
float cog_z(int i) { const Cassette *c = &CASSETTES[sim.r.cassette]; return 44.5f + ((c->n - 1) * 0.5f - i) * c->pitch; }
float ring_z(int f) { return f ? 47.5f : 40.5f; }
int cross_chained(int f, int i) { int n = ncogs(); return (f == 1 && i >= n - 2) || (f == 0 && i <= 1); }

void sim_apply_build(int b) {
    sim.r.build = b;
    sim.r.ringset = b ? 3 : 2;
    sim.r.cassette = b ? 3 : 1;
    sim.br.hydro = b;
    if (sim.r.rear >= ncogs()) sim.r.rear = ncogs() - 1;
}

void sim_reset(int build) {
    memset(&sim, 0, sizeof sim);
    sim.r.front = 1; sim.r.rear = 4; sim.r.pedaling = 1; sim.r.cadence = 80; sim.r.slowmo = 0.25f;
    sim.r.crank = DEG(-30); sim.r.engaged = 1;
    sim.br.pad = 3; sim.wh.psi = 40; sim.wh.kg = 85; sim.wh.terrain = 1; sim.st.preload = 50;
    sim_apply_build(build);
    sim.r.v = v_drive();
}

float brake_effect(void);
float brake_effect(void) { BRCalc c = br_calc(1); float f = c.force > 0.1f ? c.force : 0.1f; return f * (sim.br.contam ? 0.35f : 1.0f); }

void sim_step(float dt) {
    Ride *r = &sim.r;
    float vd = v_drive(), crank_rate = 0, chain_speed, af = r->slowmo;
    if (r->brake > 0) {
        r->v -= r->brake * (sim.br.hydro ? 7.0f : 5.5f) * dt * brake_effect();
        if (r->v < 0) r->v = 0;
    }
    if (r->pedaling) {
        if (r->brake > 0) { crank_rate = r->v / (ratio() * CIRC_M); if (crank_rate > r->cadence / 60) crank_rate = r->cadence / 60; r->engaged = 1; }
        else if (r->v < vd - 0.02f) { r->v += 1.8f * dt; if (r->v > vd) r->v = vd; crank_rate = r->v / (ratio() * CIRC_M); r->engaged = 1; }
        else if (r->v > vd + 0.05f) { r->v -= (0.06f + 0.004f * r->v * r->v) * dt; crank_rate = r->cadence / 60; r->engaged = 0; }
        else { r->v = vd; crank_rate = r->cadence / 60; r->engaged = 1; }
    } else {
        r->v -= (0.06f + 0.004f * r->v * r->v) * dt; if (r->v < 0) r->v = 0;
        r->engaged = 0;
    }
    chain_speed = crank_rate * ring_teeth() * PITCH_MM;          /* mm/s */
    r->crank -= crank_rate * 2 * PI_F * dt * af;                   /* clockwise seen from drive side */
    r->chain += chain_speed * dt * af;
    r->cog -= chain_speed / (cog_teeth() * PITCH_MM) * 2 * PI_F * dt * af;
    r->wheel -= r->v / CIRC_M * 2 * PI_F * dt * af;
    /* wrap to keep precision */
    if (r->crank < -1000) r->crank += 2 * PI_F * 159;
    if (r->wheel < -1000) r->wheel += 2 * PI_F * 159;
    if (r->cog < -1000) r->cog += 2 * PI_F * 159;
    if (r->chain > 1e6) r->chain -= 12.7 * 78740;
    sim.rotor_phase += r->v / CIRC_M * 2 * PI_F * dt * af;
    {
        float w = r->v / CIRC_M * 360 * dt * af;
        sim.hub_a += w;
        if (r->engaged) sim.body_a += w; else sim.pawl_phase += w * 0.4f;
        if (sim.hub_a > 3600) { sim.hub_a -= 3600; sim.body_a -= 3600; }
    }
}

RDCalc rd_calc(void) {
    RDCalc c; const Cassette *cs = &CASSETTES[sim.r.cassette];
    float bend = sim.rd.hanger * 0.28f * (1 + 0.16f * sim.r.rear), desired;
    c.n = cs->n; c.sp = cs->pitch;
    c.Hstop = sim.rd.H;
    c.Lstop = (c.n - 1) * c.sp + sim.rd.L;
    desired = sim.r.rear * c.sp + sim.rd.barrel * 0.35f;
    c.pos = clampf(desired, c.Hstop, c.Lstop) + bend;
    c.err = c.pos - sim.r.rear * c.sp;
    return c;
}

FDCalc fd_calc(void) {
    FDCalc c; float big_ideal = 47.9f;
    c.cog_pos = cog_z(sim.r.rear); c.ring_pos = ring_z(sim.r.front);
    c.chain_at = c.ring_pos + (c.cog_pos - c.ring_pos) * 0.15f;
    c.Lstop = 40.6f + sim.fd.L; c.Hstop = 51.0f + sim.fd.H;
    if (sim.r.front == 0) c.cage = c.Lstop;
    else { c.cage = big_ideal + sim.fd.tension - (sim.fd.trim ? 1.2f : 0); if (c.cage > c.Hstop) c.cage = c.Hstop; }
    if (c.cage < c.Lstop) c.cage = c.Lstop;
    c.clear = 2.25f; c.d = c.chain_at - c.cage;
    return c;
}

BRCalc br_calc(float lever) {
    BRCalc c; float wear = 3 - sim.br.pad;
    memset(&c, 0, sizeof c);
    if (!sim.br.hydro) {
        float pull = lever * 6;
        c.slack = 0.4f + sim.br.stretch - sim.br.barrel * 0.25f;
        if (c.slack < 0) c.slack = 0;
        c.go = 0.35f + wear;
        c.gi = 0.3f + wear - sim.br.inner * 0.25f; if (c.gi < 0.05f) c.gi = 0.05f;
        c.pad_move = (pull - c.slack) > 0 ? (pull - c.slack) * 0.35f : 0;
        c.need = c.go + c.gi;
        c.bite = (c.need / 0.35f + c.slack) / 6;
        c.force = clampf((c.pad_move - c.need) / 0.5f, 0, 1);
    } else {
        float slope;
        c.gi = 0.25f; c.go = 0.25f; c.need = 0.5f;
        c.bite = 0.22f + sim.br.air * 0.5f;
        slope = 1.0f / (0.18f + sim.br.air * 0.9f);
        c.force = clampf((lever - c.bite) * slope, 0, 1);
        c.pad_move = lever < c.bite ? c.need * lever / c.bite : c.need;
    }
    return c;
}

WHCalc wh_calc(void) {
    WHCalc c; int ter = (int)(sim.wh.terrain + 0.5f);
    static const float ter_adj[3] = { 0, 3, 6 };
    c.rear_kg = sim.wh.kg * 0.6f; c.front_kg = sim.wh.kg * 0.4f;
    c.guide = 0.47f * sim.wh.kg - ter_adj[clampi(ter, 0, 2)] - (sim.wh.tubeless ? 3 : 0);
    c.guide_f = c.guide - 3;
    c.area = (c.rear_kg * 2.2046f) / sim.wh.psi * 645.16f;
    c.len = c.area / (0.8f * 38);
    c.sag = 15 * c.guide / sim.wh.psi;
    return c;
}
