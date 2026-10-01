#include "synth.h"
#include <math.h>
#include <string.h>

#define NV 24
typedef struct { float f, a, ph, t, dur; int on; } Voice;
typedef struct { float f, target, a, ph; } Hold;
static Voice g_v[NV];
static Hold g_h[4];
static int g_mute;
static float g_master = 0.5f, g_gain = 0;

void synth_init(void) { memset(g_v, 0, sizeof g_v); memset(g_h, 0, sizeof g_h); }
void synth_mute(int m) { g_mute = m; }
int synth_muted(void) { return g_mute; }

void synth_pluck(float freq, float amp, float dur) {
    int i, best = 0; float bt = -1;
    for (i = 0; i < NV; i++) {
        if (!g_v[i].on) { best = i; break; }
        if (g_v[i].t > bt) { bt = g_v[i].t; best = i; }
    }
    g_v[best].f = freq; g_v[best].a = amp; g_v[best].ph = 0; g_v[best].t = 0; g_v[best].dur = dur; g_v[best].on = 1;
}
void synth_hold(int s, float freq, float amp) {
    if (s < 0 || s > 3) return;
    if (amp > 0 && g_h[s].a < 1e-4f) g_h[s].ph = 0;
    if (amp > 0) g_h[s].f = freq;
    g_h[s].target = amp;
}
void synth_release_all(void) { int i; for (i = 0; i < 4; i++) g_h[i].target = 0; }

void synth_fill(float *out, int n, int rate) {
    int k, i; float dt = 1.0f / (float)rate, tw = 6.283185307f;
    for (k = 0; k < n; k++) {
        float s = 0;
        for (i = 0; i < NV; i++) {
            Voice *v = &g_v[i]; float env;
            if (!v->on) continue;
            env = v->t < 0.012f ? v->t / 0.012f : expf(-(v->t - 0.012f) * 4.0f / v->dur);
            s += sinf(v->ph) * v->a * env;
            v->ph += tw * v->f * dt; if (v->ph > tw) v->ph -= tw;
            v->t += dt;
            if (v->t > v->dur * 1.6f) v->on = 0;
        }
        for (i = 0; i < 4; i++) {
            Hold *h = &g_h[i];
            h->a += (h->target - h->a) * 0.0015f;
            if (h->a < 1e-5f && h->target == 0) continue;
            s += sinf(h->ph) * h->a;
            h->ph += tw * h->f * dt; if (h->ph > tw) h->ph -= tw;
        }
        g_gain += ((g_mute ? 0.f : 1.f) - g_gain) * 0.002f;
        out[k] = tanhf(s * g_master) * g_gain;
    }
}
