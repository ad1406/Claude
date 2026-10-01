#include "bike.h"
#include "mesh.h"
#include "sim.h"
#include "gl_inc.h"
#include <string.h>

/* ---------- geometry (mm) ---------- */
static const V3 R  = {0, 349, 0};       /* rear axle */
static const V3 F  = {1036, 349, 0};    /* front axle */
static const V3 BB = {419, 279, 0};
static const V3 STT = {271, 778, 0};    /* seat tube top */
static const V3 HTT = {804, 869, 0};    /* head tube top */
static const V3 HTB = {853, 727, 0};    /* head tube bottom */
#define CRANK_LEN 170.0f
#define ROTOR_Z_R (-56.0f)
#define ROTOR_Z_F (-43.0f)

enum {
    L_FRAME, L_FORK, L_HEADSET, L_STEM, L_BAR, L_HOOD, L_BLADE, L_CABLES, L_SEATPOST, L_SADDLE, L_RAILS,
    L_BB, L_SPIDER, L_CRANKARM, L_PEDAL, L_PLATE_IN, L_PLATE_OUT, L_ROLLER, L_PULLEY, L_BOX, L_CYL,
    L_HANGER, L_HUB_R, L_HUB_F, L_RIM, L_SPOKES_R, L_SPOKES_F, L_TREAD, L_WALL, L_KNOBS, L_THRU_R, L_THRU_F,
    L_ROTOR, L_FD_PLATE, L_RING0, L_RING1, L_COG0, L_COUNT = L_COG0 + MAX_COGS
};
static unsigned g_list[L_COUNT];
static int g_built;

/* ---------- material & draw state ---------- */
static const DrawOpts *g_o;
static int g_part;

static void begin_part(int p) {
    g_part = p;
    if (g_o->mode == DRAW_PICK) { glColor3ub((unsigned char)((p + 1) * 10), 0, 0); return; }
}
static void mat(float r, float g, float b, float spec) {
    float s[4];
    if (g_o->mode != DRAW_NORMAL) return;
    if (g_o->any_hl && !g_o->hl[g_part]) {   /* ghost the rest */
        r = lerpf(r, 0.87f, 0.80f); g = lerpf(g, 0.89f, 0.80f); b = lerpf(b, 0.90f, 0.80f); spec *= 0.3f;
    } else if (g_o->any_hl) {   /* highlighted: tint toward the accent, gently pulsing */
        float t = 0.38f + 0.27f * g_o->pulse;
        r = lerpf(r, 0.96f, t); g = lerpf(g, 0.40f, t); b = lerpf(b, 0.14f, t);
    }
    glColor3f(r, g, b);
    s[0] = s[1] = s[2] = spec; s[3] = 1;
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, s);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20 + spec * 60);
}
#define M_FRAME()  mat(0.25f, 0.38f, 0.33f, 0.35f)
#define M_SILVER() mat(0.74f, 0.76f, 0.77f, 0.85f)
#define M_DARK()   mat(0.24f, 0.26f, 0.27f, 0.45f)
#define M_BLACK()  mat(0.09f, 0.10f, 0.11f, 0.25f)
#define M_RUBBER() mat(0.12f, 0.12f, 0.13f, 0.08f)
#define M_TAN()    mat(0.64f, 0.50f, 0.34f, 0.08f)
#define M_CHAIN()  mat(0.52f, 0.54f, 0.55f, 0.75f)
#define M_ACCENT() mat(0.88f, 0.34f, 0.12f, 0.3f)

static void call(int l) { glCallList(g_list[l]); }

/* Draw unit box (half extents 1) stretched between two points */
static void beam(V3 a, V3 b, float hy, float hz) {
    V3 ex = v3sub(b, a), ey, ez; float len = v3len(ex); float m[16];
    V3 c = v3mul(v3add(a, b), 0.5f);
    ex = v3mul(ex, 1.0f / (len + 1e-6f));
    ey = v3(0, 1, 0);
    if (fabsf(v3dot(ex, ey)) > 0.95f) ey = v3(1, 0, 0);
    ez = v3norm(v3cross(ex, ey)); ey = v3cross(ez, ex);
    m[0] = ex.x * len / 2; m[1] = ex.y * len / 2; m[2] = ex.z * len / 2; m[3] = 0;
    m[4] = ey.x * hy; m[5] = ey.y * hy; m[6] = ey.z * hy; m[7] = 0;
    m[8] = ez.x * hz; m[9] = ez.y * hz; m[10] = ez.z * hz; m[11] = 0;
    m[12] = c.x; m[13] = c.y; m[14] = c.z; m[15] = 1;
    glPushMatrix(); glMultMatrixf(m); call(L_BOX); glPopMatrix();
}
static void boxat(V3 c, float hx, float hy, float hz, float rotz_deg) {
    glPushMatrix(); glTranslatef(c.x, c.y, c.z); glRotatef(rotz_deg, 0, 0, 1); glScalef(hx, hy, hz); call(L_BOX); glPopMatrix();
}
static void cylz(V3 c, float r, float z0, float z1) {   /* unit cylinder along z, r=1, z 0..1 */
    glPushMatrix(); glTranslatef(c.x, c.y, c.z + z0); glScalef(r, r, z1 - z0); call(L_CYL); glPopMatrix();
}

/* ---------- builders ---------- */
static unsigned compile(MB *b) { return mb_compile(b); }

static void build_frame(void) {
    MB b; V3 ust = v3norm(v3sub(STT, BB)), aht = v3norm(v3sub(HTT, HTB));
    V3 tt0 = v3add(BB, v3mul(ust, 465)), tt1 = v3add(HTT, v3mul(aht, -26));
    V3 dt1 = v3add(HTB, v3mul(aht, 32));
    V3 ss0 = v3add(BB, v3mul(ust, 440));
    int s;
    mb_init(&b);
    mb_cylinder(&b, BB, v3add(STT, v3mul(ust, 6)), 17, 16, 28, 1);                 /* seat tube */
    mb_cylinder(&b, tt0, tt1, 17, 19, 28, 0);                                      /* top tube */
    mb_cylinder(&b, v3add(BB, v3mul(v3norm(v3sub(dt1, BB)), 10)), dt1, 26, 24, 32, 0);  /* down tube */
    mb_cylinder(&b, v3add(HTB, v3mul(aht, -6)), v3add(HTT, v3mul(aht, 4)), 29, 25, 32, 1);  /* head tube */
    mb_ellipsoid(&b, tt0, v3(19, 19, 18), 16, 10);
    mb_ellipsoid(&b, tt1, v3(22, 22, 20), 16, 10);
    mb_ellipsoid(&b, dt1, v3(26, 26, 25), 16, 10);
    mb_cylinder(&b, v3(BB.x, BB.y, -35), v3(BB.x, BB.y, 35), 22, 22, 28, 1);       /* BB shell */
    for (s = -1; s <= 1; s += 2) {
        V3 cs[4], ss[4];
        cs[0] = v3(BB.x - 20, BB.y + 6, 30.f * s); cs[1] = v3(BB.x - 120, BB.y + 22, 46.f * s);
        cs[2] = v3(80, 340, 66.f * s); cs[3] = v3(R.x + 8, R.y + 2, 70.f * s);
        mb_smooth_tube(&b, cs, 4, 12, 8, 8, 14, 1);                                  /* chainstay */
        ss[0] = v3(ss0.x + 4, ss0.y, 13.f * s); ss[1] = v3(200, 560, 40.f * s);
        ss[2] = v3(60, 420, 64.f * s); ss[3] = v3(R.x + 6, R.y + 10, 70.f * s);
        mb_smooth_tube(&b, ss, 4, 9, 7.5f, 8, 14, 1);                                /* seatstay */
        mb_box(&b, v3(R.x + 4, R.y + 4, 72.f * s), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 18, 16, 4);   /* dropout */
    }
    mb_ellipsoid(&b, ss0, v3(18, 18, 22), 14, 8);
    /* seat collar */
    mb_cylinder(&b, v3add(STT, v3mul(ust, -14)), v3add(STT, v3mul(ust, 6)), 20, 20, 24, 1);
    g_list[L_FRAME] = compile(&b);
}

static void build_fork(void) {
    MB b; V3 aht = v3norm(v3sub(HTT, HTB)); V3 crown = v3add(HTB, v3mul(aht, -22)); int s;
    mb_init(&b);
    mb_box(&b, crown, v3norm(v3(aht.y, -aht.x, 0)), aht, v3(0, 0, 1), 26, 14, 50);
    for (s = -1; s <= 1; s += 2) {
        V3 c[4];
        c[0] = v3(crown.x + 4, crown.y - 6, 40.f * s);
        c[1] = v3add(c[0], v3(v3mul(aht, -150).x, v3mul(aht, -150).y, 6.f * s));
        c[2] = v3(F.x - 18, F.y + 115, 54.f * s);
        c[3] = v3(F.x, F.y + 2, 56.f * s);
        mb_smooth_tube(&b, c, 4, 17, 11, 10, 18, 1);
        mb_box(&b, v3(F.x, F.y, 58.f * s), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 14, 14, 4);
    }
    g_list[L_FORK] = compile(&b);
}

static void build_headset(void) {
    MB b; V3 a = v3norm(v3sub(HTT, HTB));
    mb_init(&b);
    mb_cylinder(&b, v3add(HTT, v3mul(a, 4)), v3add(HTT, v3mul(a, 12)), 28, 24, 28, 1);    /* top cover */
    mb_cylinder(&b, v3add(HTB, v3mul(a, -6)), v3add(HTB, v3mul(a, -14)), 31, 33, 28, 1);  /* crown race */
    g_list[L_HEADSET] = compile(&b);
}

/* steerer stack + stem; bar separately */
static V3 g_bar_c;   /* bar clamp centre */
static void build_cockpit(void) {
    MB b; V3 a = v3norm(v3sub(HTT, HTB)); V3 p0 = v3add(HTT, v3mul(a, 12)), sp1, sc, top;
    V3 fwd; int s;
    mb_init(&b);
    sp1 = v3add(p0, v3mul(a, 22));
    mb_cylinder(&b, p0, sp1, 19, 19, 24, 1);              /* spacers */
    sc = v3add(sp1, v3mul(a, 40));
    mb_cylinder(&b, sp1, sc, 22, 22, 24, 1);              /* stem steerer clamp */
    top = v3add(sc, v3mul(a, 5));
    mb_cylinder(&b, sc, top, 20, 18, 24, 1);              /* top cap */
    fwd = v3norm(v3(cosf(DEG(13)), sinf(DEG(13)), 0));
    {
        V3 mid = v3add(sp1, v3mul(a, 20));
        g_bar_c = v3add(mid, v3mul(fwd, 92));
        mb_cylinder(&b, mid, g_bar_c, 17, 15, 20, 0);     /* stem body */
        mb_cylinder(&b, v3add(g_bar_c, v3(0, 0, -26)), v3add(g_bar_c, v3(0, 0, 26)), 21, 21, 24, 1);  /* faceplate */
    }
    g_list[L_STEM] = compile(&b);

    mb_init(&b);
    for (s = -1; s <= 1; s += 2) {
        V3 c[9]; float z = (float)s; V3 o = g_bar_c;
        c[0] = v3(o.x, o.y, 0);
        c[1] = v3(o.x, o.y, 110 * z);
        c[2] = v3(o.x + 18, o.y + 2, 180 * z);
        c[3] = v3(o.x + 62, o.y - 4, 205 * z);
        c[4] = v3(o.x + 88, o.y - 30, 212 * z);
        c[5] = v3(o.x + 92, o.y - 72, 218 * z);
        c[6] = v3(o.x + 72, o.y - 110, 228 * z);
        c[7] = v3(o.x + 30, o.y - 122, 238 * z);
        c[8] = v3(o.x - 8, o.y - 118, 244 * z);
        mb_smooth_tube(&b, c, 9, 12.5f, 12.5f, 10, 16, 1);
    }
    g_list[L_BAR] = compile(&b);

    /* hood (right side; mirrored for left) and lever blade (pivot at origin) */
    mb_init(&b);
    {
        V3 o = g_bar_c;
        mb_ellipsoid(&b, v3(o.x + 80, o.y + 6, 208), v3(34, 20, 15), 18, 12);
        mb_ellipsoid(&b, v3(o.x + 108, o.y + 22, 208), v3(10, 12, 11), 12, 8);   /* hood horn */
    }
    g_list[L_HOOD] = compile(&b);
    mb_init(&b);
    {
        V3 c[5];
        c[0] = v3(0, 0, 0); c[1] = v3(10, -30, 2); c[2] = v3(16, -70, 6); c[3] = v3(10, -105, 10); c[4] = v3(0, -125, 12);
        mb_smooth_tube(&b, c, 5, 7, 5, 8, 12, 1);
    }
    g_list[L_BLADE] = compile(&b);
}

static void build_cables(void) {
    MB b; V3 o = g_bar_c; int s;
    mb_init(&b);
    for (s = -1; s <= 1; s += 2) {   /* from bar tape exits to head tube ports */
        V3 c[5];
        c[0] = v3(o.x + 20, o.y - 4, 95.f * s); c[1] = v3(o.x + 60, o.y - 60, 70.f * s);
        c[2] = v3(o.x + 40, o.y - 140, 50.f * s); c[3] = v3(HTB.x + 10, HTB.y + 70, 32.f * s);
        c[4] = v3(HTB.x - 8, HTB.y + 40, 26.f * s);
        mb_smooth_tube(&b, c, 5, 2.6f, 2.6f, 10, 8, 1);
        c[0] = v3(o.x + 30, o.y - 4, 120.f * s); c[1] = v3(o.x + 80, o.y - 70, 100.f * s);
        c[2] = v3(o.x + 50, o.y - 160, 60.f * s); c[3] = v3(HTB.x + 2, HTB.y + 90, 30.f * s);
        c[4] = v3(HTB.x - 14, HTB.y + 60, 26.f * s);
        mb_smooth_tube(&b, c, 5, 2.6f, 2.6f, 10, 8, 1);
    }
    {   /* front brake housing down the fork */
        V3 c[5];
        c[0] = v3(HTB.x + 6, HTB.y + 30, -30); c[1] = v3(HTB.x + 40, HTB.y - 60, -46);
        c[2] = v3(F.x - 60, F.y + 160, -56); c[3] = v3(F.x - 60, F.y + 70, -54); c[4] = v3(F.x - 68, F.y + 40, -48);
        mb_smooth_tube(&b, c, 5, 2.6f, 2.6f, 10, 8, 1);
    }
    {   /* rear derailleur housing loop from chainstay to derailleur */
        V3 c[5];
        c[0] = v3(90, 365, 62); c[1] = v3(40, 362, 74); c[2] = v3(-8, 350, 84); c[3] = v3(-20, 328, 86); c[4] = v3(-6, 316, 82);
        mb_smooth_tube(&b, c, 5, 2.6f, 2.6f, 10, 8, 1);
    }
    {   /* rear brake housing to caliper (left side) */
        V3 c[4];
        c[0] = v3(200, 318, -52); c[1] = v3(140, 336, -60); c[2] = v3(110, 360, -64); c[3] = v3(96, 372, -62);
        mb_smooth_tube(&b, c, 4, 2.6f, 2.6f, 10, 8, 1);
    }
    g_list[L_CABLES] = compile(&b);
}

static void build_seat(void) {
    MB b; V3 u = v3norm(v3sub(STT, BB)); V3 top = v3add(STT, v3mul(u, 228));
    int i, j, NT = 40, NP = 28;
    mb_init(&b);
    mb_cylinder(&b, v3add(STT, v3mul(u, -60)), top, 13.6f, 13.6f, 24, 0);
    mb_box(&b, v3add(top, v3(0, 6, 0)), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 26, 9, 18);
    g_list[L_SEATPOST] = compile(&b);

    /* saddle shell: lofted superellipse sections */
    mb_init(&b);
    for (i = 0; i < NT; i++) for (j = 0; j < NP; j++) {
        V3 p[4], n[4]; int q;
        for (q = 0; q < 4; q++) {
            float t = (float)(i + (q == 1 || q == 2)) / NT, ph = (float)(j + (q >= 2)) / NP * 2 * PI_F;
            float hw = 72 * powf(sinf(fminf(t * 3.5f, 1.0f) * PI_F / 2), 0.6f) * (1 - 0.78f * powf(t, 1.3f)) * (1 - powf(t, 10));
            float hh = 13 * (1 - 0.25f * t) * powf(sinf(fminf(t * 6, 1.0f) * PI_F / 2), 0.5f) * (1 - powf(t, 12)) + 0.01f;
            float yc = top.y + 24 - 6 * sinf(t * PI_F) + 4 * t;
            float cs = cosf(ph), sn = sinf(ph);
            float sy = sn >= 0 ? powf(sn, 0.6f) : -0.45f * powf(-sn, 0.9f);
            float sz = (cs >= 0 ? 1 : -1) * powf(fabsf(cs), 0.7f);
            float x = top.x - 150 + t * 285;
            p[q] = v3(x, yc + hh * sy, hw * sz);
            n[q] = v3norm(v3((-(hw * 0.3f) * (t > 0.5f ? 1.f : -0.3f)) * 0.02f, sy / (hh + 1), sz / (hw + 1)));
        }
        mb_tri(&b, p[0], n[0], p[1], n[1], p[2], n[2]);
        mb_tri(&b, p[0], n[0], p[2], n[2], p[3], n[3]);
    }
    g_list[L_SADDLE] = compile(&b);
    mb_init(&b);
    for (i = -1; i <= 1; i += 2) {
        V3 c[4];
        c[0] = v3(top.x - 120, top.y + 22, 30.f * i); c[1] = v3(top.x - 70, top.y + 8, 22.f * i);
        c[2] = v3(top.x + 50, top.y + 8, 20.f * i); c[3] = v3(top.x + 110, top.y + 22, 10.f * i);
        mb_smooth_tube(&b, c, 4, 3.5f, 3.5f, 8, 8, 1);
    }
    g_list[L_RAILS] = compile(&b);
}

static void build_crank_parts(void) {
    MB b;
    mb_init(&b);
    mb_cylinder(&b, v3(BB.x, BB.y, -66), v3(BB.x, BB.y, 66), 12, 12, 20, 1);    /* axle */
    mb_cylinder(&b, v3(BB.x, BB.y, -44), v3(BB.x, BB.y, -34), 24, 24, 28, 1);   /* cups */
    mb_cylinder(&b, v3(BB.x, BB.y, 34), v3(BB.x, BB.y, 44), 24, 24, 28, 1);
    g_list[L_BB] = compile(&b);
    /* spider: 4 arms in XY at origin */
    mb_init(&b);
    {
        int i;
        for (i = 0; i < 4; i++) {
            float a = DEG(45 + 90 * i);
            mb_box(&b, v3(cosf(a) * 32, sinf(a) * 32, 0), v3(cosf(a), sinf(a), 0), v3(-sinf(a), cosf(a), 0), v3(0, 0, 1), 30, 7, 4);
        }
        mb_cylinder(&b, v3(0, 0, -5), v3(0, 0, 5), 26, 26, 24, 1);
    }
    g_list[L_SPIDER] = compile(&b);
    /* crank arm along +x, at z=0 */
    mb_init(&b);
    mb_box(&b, v3(CRANK_LEN / 2, 0, 0), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), CRANK_LEN / 2, 12, 7);
    mb_cylinder(&b, v3(0, 0, -9), v3(0, 0, 9), 22, 22, 22, 1);
    mb_cylinder(&b, v3(CRANK_LEN, 0, -8), v3(CRANK_LEN, 0, 8), 13, 13, 18, 1);
    g_list[L_CRANKARM] = compile(&b);
    /* pedal body extending along +z from z=0 */
    mb_init(&b);
    mb_cylinder(&b, v3(0, 0, 0), v3(0, 0, 22), 6, 6, 12, 1);
    mb_box(&b, v3(0, 0, 62), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 50, 10, 42);
    g_list[L_PEDAL] = compile(&b);
}

static void build_chain_parts(void) {
    MB b;
    mb_init(&b);   /* inner plates pair */
    mb_box(&b, v3(6.35f, 0, 3.4f), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 8.6f, 4.0f, 0.5f);
    mb_box(&b, v3(6.35f, 0, -3.4f), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 8.6f, 4.0f, 0.5f);
    g_list[L_PLATE_IN] = compile(&b);
    mb_init(&b);
    mb_box(&b, v3(6.35f, 0, 4.6f), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 9.2f, 4.2f, 0.5f);
    mb_box(&b, v3(6.35f, 0, -4.6f), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 9.2f, 4.2f, 0.5f);
    g_list[L_PLATE_OUT] = compile(&b);
    mb_init(&b);
    mb_cylinder(&b, v3(0, 0, -5.2f), v3(0, 0, 5.2f), 3.6f, 3.6f, 8, 1);
    g_list[L_ROLLER] = compile(&b);
    mb_init(&b); mb_gear(&b, 11, pitch_radius(11), 8, 4, 6); mb_cylinder(&b, v3(0, 0, -4), v3(0, 0, 4), 8, 8, 12, 1);
    g_list[L_PULLEY] = compile(&b);
    mb_init(&b); mb_box(&b, v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 1, 1, 1);
    g_list[L_BOX] = compile(&b);
    mb_init(&b); mb_cylinder(&b, v3(0, 0, 0), v3(0, 0, 1), 1, 1, 20, 1);
    g_list[L_CYL] = compile(&b);
    mb_init(&b);
    mb_box(&b, v3(R.x + 4, R.y - 14, 78), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 10, 20, 3);
    g_list[L_HANGER] = compile(&b);
}

static void build_wheels(void) {
    MB b; int i, k;
    static const float rim[] = { 296, -8, 300, -12, 318, -12.5f, 322, -10, 322, 10, 318, 12.5f, 300, 12, 296, 8, 296, -8 };
    mb_init(&b); mb_lathe(&b, rim, 9, 96); g_list[L_RIM] = compile(&b);
    for (k = 0; k < 2; k++) {   /* spokes: 24, half from each flange */
        float zd = k == 0 ? 18.f : 32.f, zn = k == 0 ? -34.f : -32.f;
        mb_init(&b);
        for (i = 0; i < 24; i++) {
            float a = (float)i / 24 * 2 * PI_F, side = (i % 2) ? 1.f : -1.f;
            float z = (i % 2) ? zd : zn;
            float ar = a + side * 0.42f * ((i / 2) % 2 ? 1 : -1);
            V3 h = v3(cosf(a) * 25, sinf(a) * 25, z), r = v3(cosf(ar) * 297, sinf(ar) * 297, z * 0.08f);
            mb_cylinder(&b, h, r, 1.1f, 1.1f, 5, 0);
        }
        g_list[k == 0 ? L_SPOKES_R : L_SPOKES_F] = compile(&b);
    }
    mb_init(&b);
    mb_cylinder(&b, v3(0, 0, -42), v3(0, 0, 16), 17, 17, 24, 1);
    mb_cylinder(&b, v3(0, 0, 16), v3(0, 0, 60), 16, 16, 24, 1);      /* freehub body */
    mb_cylinder(&b, v3(0, 0, 16), v3(0, 0, 20), 29, 29, 28, 1);      /* drive flange */
    mb_cylinder(&b, v3(0, 0, -36), v3(0, 0, -32), 29, 29, 28, 1);    /* nds flange */
    mb_cylinder(&b, v3(0, 0, ROTOR_Z_R + 2), v3(0, 0, ROTOR_Z_R + 10), 24, 24, 24, 1);
    g_list[L_HUB_R] = compile(&b);
    mb_init(&b);
    mb_cylinder(&b, v3(0, 0, -50), v3(0, 0, 50), 15, 15, 24, 1);
    mb_cylinder(&b, v3(0, 0, 30), v3(0, 0, 34), 26, 26, 28, 1);
    mb_cylinder(&b, v3(0, 0, -34), v3(0, 0, -30), 26, 26, 28, 1);
    mb_cylinder(&b, v3(0, 0, ROTOR_Z_F + 2), v3(0, 0, ROTOR_Z_F + 10), 22, 22, 24, 1);
    g_list[L_HUB_F] = compile(&b);
    mb_init(&b); mb_torus_band(&b, 330, 19, DEG(-58), DEG(58), 160, 10); g_list[L_TREAD] = compile(&b);
    mb_init(&b); mb_torus_band(&b, 330, 19, DEG(58), DEG(150), 160, 8); mb_torus_band(&b, 330, 19, DEG(-150), DEG(-58), 160, 8);
    g_list[L_WALL] = compile(&b);
    mb_init(&b);
    for (k = -1; k <= 1; k++) {
        int n = k == 0 ? 120 : 110;
        for (i = 0; i < n; i++) {
            float u = ((float)i + (k == 0 ? 0.5f : 0)) / n * 2 * PI_F, v = DEG(34) * k;
            V3 rd = v3(cosf(u), sinf(u), 0), nn = v3add(v3mul(rd, cosf(v)), v3(0, 0, sinf(v)));
            V3 p = v3add(v3mul(rd, 330), v3mul(nn, 19.5f));
            V3 tang = v3(-sinf(u), cosf(u), 0), side = v3norm(v3cross(nn, tang));
            mb_box(&b, p, tang, nn, side, k == 0 ? 4.5f : 3.5f, 2.2f, k == 0 ? 5.5f : 4.0f);
        }
    }
    g_list[L_KNOBS] = compile(&b);
    mb_init(&b);
    mb_cylinder(&b, v3(R.x, R.y, -80), v3(R.x, R.y, 80), 7, 7, 16, 1);
    mb_box(&b, v3(R.x + 28, R.y + 4, -86), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 32, 6, 4);
    g_list[L_THRU_R] = compile(&b);
    mb_init(&b);
    mb_cylinder(&b, v3(F.x, F.y, -63), v3(F.x, F.y, 63), 7, 7, 16, 1);
    mb_box(&b, v3(F.x - 28, F.y + 4, -68), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1), 32, 6, 4);
    g_list[L_THRU_F] = compile(&b);
    /* rotor: braking track + spider arms */
    mb_init(&b);
    mb_annulus(&b, 62, 80, 1.8f, 96);
    for (i = 0; i < 6; i++) {
        float a = (float)i / 6 * 2 * PI_F, a2 = a + 0.5f;
        V3 p0 = v3(cosf(a) * 24, sinf(a) * 24, 0), p1 = v3(cosf(a2) * 64, sinf(a2) * 64, 0);
        V3 d = v3norm(v3sub(p1, p0));
        mb_box(&b, v3mul(v3add(p0, p1), 0.5f), d, v3(-d.y, d.x, 0), v3(0, 0, 1), v3len(v3sub(p1, p0)) / 2 + 3, 4, 0.9f);
    }
    mb_annulus(&b, 18, 27, 1.8f, 32);
    g_list[L_ROTOR] = compile(&b);
}

/* drivetrain lists that depend on the chosen chainrings and cassette */
void bike_rebuild_drive(void) {
    MB b; int i; const Cassette *cs = &CASSETTES[sim.r.cassette];
    for (i = 0; i < 2; i++) {
        int t = RINGSETS[sim.r.ringset].teeth[i]; float pr = pitch_radius(t);
        if (g_list[L_RING0 + i]) glDeleteLists(g_list[L_RING0 + i], 1);
        mb_init(&b); mb_gear(&b, t, pr, pr - 15, 2.6f, 6); g_list[L_RING0 + i] = compile(&b);
    }
    for (i = 0; i < MAX_COGS; i++) {
        if (g_list[L_COG0 + i]) { glDeleteLists(g_list[L_COG0 + i], 1); g_list[L_COG0 + i] = 0; }
        if (i < cs->n) {
            int t = cs->cogs[i]; float pr = pitch_radius(t);
            mb_init(&b); mb_gear(&b, t, pr, 17, 1.9f, 5.5f);
            if (pr > 45) mb_annulus(&b, 17, 24, 2.4f, 24);
            g_list[L_COG0 + i] = compile(&b);
        }
    }
    {   /* front derailleur cage plate: arc around the big ring */
        float rb = pitch_radius(RINGSETS[sim.r.ringset].teeth[1]);
        V3 c[6]; int k;
        if (g_list[L_FD_PLATE]) glDeleteLists(g_list[L_FD_PLATE], 1);
        mb_init(&b);
        for (k = 0; k < 6; k++) {
            float a = DEG(112 + k * 8);
            c[k] = v3(cosf(a) * (rb + 12), sinf(a) * (rb + 12), 0);
        }
        for (k = 0; k < 5; k++) {
            V3 d = v3norm(v3sub(c[k + 1], c[k]));
            mb_box(&b, v3mul(v3add(c[k], c[k + 1]), 0.5f), d, v3(-d.y, d.x, 0), v3(0, 0, 1), v3len(v3sub(c[k + 1], c[k])) / 2 + 1, 11, 0.9f);
        }
        g_list[L_FD_PLATE] = compile(&b);
    }
}

int bike_init(void) {
    memset(g_list, 0, sizeof g_list);
    build_frame(); build_fork(); build_headset(); build_cockpit(); build_cables(); build_seat();
    build_crank_parts(); build_chain_parts(); build_wheels();
    bike_rebuild_drive();
    g_built = 1;
    return 1;
}

void bike_free(void) {
    int i;
    for (i = 0; i < L_COUNT; i++) if (g_list[i]) { glDeleteLists(g_list[i], 1); g_list[i] = 0; }
    g_built = 0;
}

/* ---------- chain path ---------- */
typedef struct { V3 c; float r; int w; float z; } Circ;
#define MAXP 1400
static V3 g_pp[MAXP]; static float g_ps[MAXP]; static int g_np;

static void trav(Circ *C, float px, float py, float *tx, float *ty) {
    float rx = (px - C->c.x) / C->r, ry = (py - C->c.y) / C->r;
    *tx = C->w * ry; *ty = -C->w * rx;
}
static void tangent(Circ *A, Circ *B, float p1[2], float p2[2]) {
    float dx = B->c.x - A->c.x, dy = B->c.y - A->c.y, d = sqrtf(dx * dx + dy * dy), ux = dx / d, uy = dy / d, best = -1e9f;
    int s, k;
    for (s = -1; s <= 1; s += 2) {
        float R2 = s * B->r, cc = (A->r - R2) / d, h;
        if (fabsf(cc) > 1) continue;
        h = sqrtf(1 - cc * cc);
        for (k = -1; k <= 1; k += 2) {
            float nx = ux * cc - k * h * uy, ny = uy * cc + k * h * ux;
            float ax = A->c.x + A->r * nx, ay = A->c.y + A->r * ny, bx = B->c.x + R2 * nx, by = B->c.y + R2 * ny;
            float tx = bx - ax, ty = by - ay, L = sqrtf(tx * tx + ty * ty), t1x, t1y, t2x, t2y, sc;
            tx /= L; ty /= L;
            trav(A, ax, ay, &t1x, &t1y); trav(B, bx, by, &t2x, &t2y);
            sc = tx * t1x + ty * t1y + tx * t2x + ty * t2y;
            if (sc > best) { best = sc; p1[0] = ax; p1[1] = ay; p2[0] = bx; p2[1] = by; }
        }
    }
}
static void addp(V3 p) {
    if (g_np >= MAXP) return;
    g_ps[g_np] = g_np ? g_ps[g_np - 1] + v3len(v3sub(p, g_pp[g_np - 1])) : 0;
    g_pp[g_np++] = p;
}
static void build_chain_path(Circ *cs, int n) {
    float T[8][4]; int i;
    for (i = 0; i < n; i++) tangent(&cs[i], &cs[(i + 1) % n], &T[i][0], &T[i][2]);
    g_np = 0;
    for (i = 0; i < n; i++) {
        Circ *C = &cs[i]; float *in = T[(i - 1 + n) % n] + 2, *out = T[i];
        float a1 = atan2f(in[1] - C->c.y, in[0] - C->c.x), a2 = atan2f(out[1] - C->c.y, out[0] - C->c.x), delta;
        int k, steps;
        delta = C->w > 0 ? a1 - a2 : a2 - a1;
        while (delta < 0) delta += 2 * PI_F;
        while (delta >= 2 * PI_F) delta -= 2 * PI_F;
        steps = 2 + (int)(delta * C->r / 3);
        for (k = 0; k <= steps; k++) {
            float a = a1 - C->w * delta * k / steps;
            addp(v3(C->c.x + cosf(a) * C->r, C->c.y + sinf(a) * C->r, C->z));
        }
        /* straight run to next circle: endpoints carry z of each circle; lerp */
        {
            Circ *N = &cs[(i + 1) % n]; int m, segs = 12;
            for (m = 1; m < segs; m++) {
                float t = (float)m / segs;
                addp(v3(lerpf(out[0], T[i][2], t), lerpf(out[1], T[i][3], t), lerpf(C->z, N->z, t)));
            }
        }
    }
    addp(g_pp[0]);
}
static V3 path_at(float s, V3 *dir) {
    int lo = 0, hi = g_np - 1;
    float L = g_ps[g_np - 1];
    s = fmodf(s, L); if (s < 0) s += L;
    while (hi - lo > 1) { int m = (lo + hi) / 2; if (g_ps[m] <= s) lo = m; else hi = m; }
    {
        float seg = g_ps[hi] - g_ps[lo], t = seg > 1e-6f ? (s - g_ps[lo]) / seg : 0;
        *dir = v3norm(v3sub(g_pp[hi], g_pp[lo]));
        return v3lerp(g_pp[lo], g_pp[hi], t);
    }
}

/* ---------- drawing ---------- */
static float g_rc, g_rr, g_zp, g_zc, g_zr;
static V3 g_gp, g_tp;

static void pose_drive(void) {
    const Cassette *cs = &CASSETTES[sim.r.cassette];
    RDCalc rc = rd_calc();
    float rmin_c = pitch_radius(cs->cogs[0]), rmax_c = pitch_radius(cs->cogs[cs->n - 1]);
    float rr0 = pitch_radius(RINGSETS[sim.r.ringset].teeth[0]), rr1 = pitch_radius(RINGSETS[sim.r.ringset].teeth[1]);
    float th;
    g_rc = pitch_radius(cog_teeth()); g_rr = pitch_radius(ring_teeth());
    g_zc = cog_z(sim.r.rear); g_zr = ring_z(sim.r.front);
    g_zp = cog_z(0) - rc.pos;
    g_gp = v3(R.x - 10, R.y - g_rc - 31, g_zp);
    th = DEG(8 + 48 * ((g_rc - rmin_c) + (g_rr - rr0)) / ((rmax_c - rmin_c) + (rr1 - rr0) + 1e-3f));
    g_tp = v3(g_gp.x + 78 * sinf(th), g_gp.y - 78 * cosf(th), g_zp);
}

static void draw_chain(void) {
    Circ cs[4]; float pr11 = pitch_radius(11), L, pitch, off; int n, k;
    cs[0].c = BB; cs[0].r = g_rr; cs[0].w = 1; cs[0].z = g_zr;
    cs[1].c = g_tp; cs[1].r = pr11; cs[1].w = 1; cs[1].z = g_zp;
    cs[2].c = g_gp; cs[2].r = pr11; cs[2].w = -1; cs[2].z = g_zp;
    cs[3].c = R; cs[3].r = g_rc; cs[3].w = 1; cs[3].z = g_zc;
    build_chain_path(cs, 4);
    L = g_ps[g_np - 1];
    n = (int)(L / PITCH_MM + 0.5f); if (n % 2) n++;
    pitch = L / n;
    off = (float)fmod(sim.r.chain, 2.0 * pitch);
    for (k = 0; k < n; k++) {
        V3 d, p = path_at(k * pitch + off, &d), d2, q = path_at(k * pitch + off + pitch, &d2);
        V3 dir = v3sub(q, p); float ang = atan2f(dir.y, dir.x) * 180 / PI_F;
        glPushMatrix(); glTranslatef(p.x, p.y, p.z); glRotatef(ang, 0, 0, 1);
        if (k % 2) { M_SILVER(); call(L_PLATE_OUT); } else { M_CHAIN(); call(L_PLATE_IN); }
        M_DARK(); call(L_ROLLER);
        glPopMatrix();
    }
}

static void draw_wheel(V3 c, int rear) {
    float ang = (float)sim.r.wheel * 180 / PI_F;
    glPushMatrix(); glTranslatef(c.x, c.y, c.z); glRotatef(ang, 0, 0, 1);
    begin_part(P_TIRES); M_RUBBER(); call(L_TREAD); call(L_KNOBS); M_TAN(); call(L_WALL);
    begin_part(P_WHEELS); M_DARK(); call(L_RIM); M_DARK(); call(rear ? L_SPOKES_R : L_SPOKES_F);
    begin_part(P_HUBS); M_SILVER(); call(rear ? L_HUB_R : L_HUB_F);
    begin_part(P_ROTORS);
    {
        float wob = 0;
        if (rear && sim.br.warp) wob = 1.4f * sinf(sim.rotor_phase);
        glPushMatrix(); glTranslatef(0, 0, rear ? ROTOR_Z_R : ROTOR_Z_F);
        if (wob != 0) glRotatef(wob, 1, 0, 0);
        mat(0.70f, 0.72f, 0.73f, 0.9f); call(L_ROTOR);
        glPopMatrix();
    }
    glPopMatrix();
}

static void draw_caliper(V3 axle, float ang_deg, float rz, int rear) {
    BRCalc bc = br_calc(sim.br.lever > sim.r.brake ? sim.br.lever : sim.r.brake);
    float a = DEG(ang_deg), gi, go;
    V3 c = v3(axle.x + cosf(a) * 71, axle.y + sinf(a) * 71, rz);
    gi = 0.8f + bc.gi * 2; go = 0.8f + bc.go * 2;
    if (bc.force > 0) { gi = 0.9f; go = 0.9f; }
    glPushMatrix(); glTranslatef(c.x, c.y, c.z); glRotatef(ang_deg - 90, 0, 0, 1);
    M_DARK();
    boxat(v3(0, 4, -10 - gi), 26, 17, 8, 0);       /* inner body */
    boxat(v3(0, 4, 10 + go), 26, 17, 8, 0);        /* outer body */
    boxat(v3(0, 22, 0), 26, 4, 18, 0);             /* bridge */
    if (!sim.br.hydro || !rear) { M_SILVER(); boxat(v3(-14, 6, 22 + go), 10, 4, 3, -25); }  /* actuating arm hint */
    if (bc.force > 0 && g_o->mode == DRAW_NORMAL) mat(0.85f, 0.25f, 0.18f, 0.2f); else mat(0.35f, 0.33f, 0.30f, 0.1f);
    boxat(v3(0, 0, -1.6f - gi * 0.5f), 18, 9, 1.0f, 0);
    boxat(v3(0, 0, 1.6f + go * 0.5f), 18, 9, 1.0f, 0);
    glPopMatrix();
    (void)rear;
}

static void draw_rd(void) {
    V3 hb = v3(R.x + 6, R.y - 32, 80);
    V3 pk = v3(g_gp.x - 4, g_gp.y + 30, g_zp + 12);
    float tilt = sim.rd.hanger;
    begin_part(P_HANGER); M_SILVER();
    glPushMatrix();
    glTranslatef(R.x, R.y, 78); glRotatef(tilt, 1, 0, 0); glTranslatef(-R.x, -R.y, -78);
    call(L_HANGER);
    begin_part(P_RD); M_DARK();
    boxat(hb, 12, 16, 10, 0);
    glPopMatrix();
    M_DARK();
    beam(v3(hb.x - 4, hb.y - 6, hb.z - 2), v3(pk.x - 2, pk.y + 4, pk.z), 5, 3);
    M_SILVER();
    beam(v3(hb.x + 6, hb.y - 14, hb.z + 4), v3(pk.x + 6, pk.y - 2, pk.z + 4), 6, 2);
    M_DARK();
    boxat(pk, 12, 14, 9, 0);
    /* cage plates */
    beam(v3(g_gp.x, g_gp.y, g_zp + 6), v3(g_tp.x, g_tp.y, g_zp + 6), 13, 1.2f);
    beam(v3(g_gp.x, g_gp.y, g_zp - 6), v3(g_tp.x, g_tp.y, g_zp - 6), 9, 1.2f);
    cylz(v3(g_gp.x, g_gp.y, 0), 13, g_zp + 5, g_zp + 7.5f);
    cylz(v3(g_tp.x, g_tp.y, 0), 13, g_zp + 5, g_zp + 7.5f);
    /* pulleys turn with the chain */
    {
        float pa = (float)(sim.r.chain / pitch_radius(11)) * 180 / PI_F;
        M_BLACK();
        glPushMatrix(); glTranslatef(g_gp.x, g_gp.y, g_zp); glRotatef(pa, 0, 0, 1); call(L_PULLEY); glPopMatrix();
        glPushMatrix(); glTranslatef(g_tp.x, g_tp.y, g_zp); glRotatef(-pa, 0, 0, 1); call(L_PULLEY); glPopMatrix();
    }
}

static void draw_fd(void) {
    FDCalc fc = fd_calc();
    float half = fc.clear + 3.5f + 1;
    float rb = pitch_radius(RINGSETS[sim.r.ringset].teeth[1]);
    V3 ust = v3norm(v3sub(STT, BB));
    V3 mount = v3add(BB, v3mul(ust, rb + 62));
    V3 top = v3(BB.x + cosf(DEG(132)) * (rb + 22), BB.y + sinf(DEG(132)) * (rb + 22), fc.cage);
    begin_part(P_FD);
    M_SILVER();
    glPushMatrix(); glTranslatef(BB.x, BB.y, fc.cage + half); call(L_FD_PLATE); glPopMatrix();
    glPushMatrix(); glTranslatef(BB.x, BB.y, fc.cage - half); call(L_FD_PLATE); glPopMatrix();
    M_DARK();
    beam(v3(top.x, top.y, fc.cage - half), v3(top.x, top.y, fc.cage + half), 6, 4);
    beam(v3(mount.x + 8, mount.y - 4, 18), v3(top.x + 4, top.y + 6, fc.cage + 2), 7, 5);
    boxat(v3(mount.x + 10, mount.y, 12), 9, 14, 8, 0);
}

static void draw_bike(void) {
    float crank_deg = (float)sim.r.crank * 180 / PI_F;
    int i;
    pose_drive();

    begin_part(P_FRAME); M_FRAME(); call(L_FRAME);
    begin_part(P_FORK); M_FRAME(); call(L_FORK);
    begin_part(P_HEADSET); M_DARK();
    {
        float gap = sim.st.preload < 30 ? (30 - sim.st.preload) / 30.0f * 3.0f : 0;
        V3 a = v3norm(v3sub(HTT, HTB));
        call(L_HEADSET);
        begin_part(P_COCKPIT);
        glPushMatrix(); glTranslatef(a.x * gap, a.y * gap, 0);
        M_DARK(); call(L_STEM); M_BLACK(); call(L_BAR);
        begin_part(P_LEVERS);
        {
            int s;
            for (s = 0; s < 2; s++) {
                glPushMatrix(); if (s) glScalef(1, 1, -1);
                M_BLACK(); call(L_HOOD);
                glPushMatrix(); glTranslatef(g_bar_c.x + 100, g_bar_c.y + 2, 210);
                glRotatef(-(sim.r.brake > 0 ? 14 * sim.r.brake : 0), 0, 0, 1);
                M_SILVER(); call(L_BLADE); glPopMatrix();
                glPopMatrix();
            }
        }
        glPopMatrix();
    }
    begin_part(P_CABLES); mat(0.16f, 0.17f, 0.18f, 0.3f); call(L_CABLES);
    begin_part(P_SEATPOST); M_SILVER(); call(L_SEATPOST);
    begin_part(P_SADDLE); M_BLACK(); call(L_SADDLE); M_SILVER(); call(L_RAILS);
    begin_part(P_BB); M_DARK(); call(L_BB);

    /* crankset */
    begin_part(P_CRANKSET);
    glPushMatrix(); glTranslatef(BB.x, BB.y, 0); glRotatef(crank_deg, 0, 0, 1);
    for (i = 0; i < 2; i++) {
        glPushMatrix(); glTranslatef(0, 0, ring_z(i));
        if (i == sim.r.front) M_DARK(); else mat(0.32f, 0.34f, 0.35f, 0.5f);
        call(L_RING0 + i); glPopMatrix();
    }
    M_BLACK();
    glPushMatrix(); glTranslatef(0, 0, 44); call(L_SPIDER); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0, 60); call(L_CRANKARM); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 0, -60); glRotatef(180, 0, 0, 1); call(L_CRANKARM); glPopMatrix();
    glPopMatrix();
    begin_part(P_PEDALS);
    {
        float a = (float)sim.r.crank;
        glPushMatrix(); glTranslatef(BB.x + cosf(a) * CRANK_LEN, BB.y + sinf(a) * CRANK_LEN, 68); M_BLACK(); call(L_PEDAL); glPopMatrix();
        glPushMatrix(); glTranslatef(BB.x - cosf(a) * CRANK_LEN, BB.y - sinf(a) * CRANK_LEN, -68); glScalef(1, 1, -1); call(L_PEDAL); glPopMatrix();
    }

    /* cassette */
    begin_part(P_CASSETTE);
    glPushMatrix(); glTranslatef(R.x, R.y, 0); glRotatef((float)sim.r.cog * 180 / PI_F, 0, 0, 1);
    for (i = 0; i < ncogs(); i++) {
        glPushMatrix(); glTranslatef(0, 0, cog_z(i));
        if (i == sim.r.rear) mat(0.62f, 0.64f, 0.65f, 0.9f); else mat(0.48f, 0.50f, 0.51f, 0.7f);
        call(L_COG0 + i); glPopMatrix();
    }
    glPopMatrix();

    begin_part(P_CHAIN); draw_chain();
    draw_rd();
    draw_fd();

    draw_wheel(R, 1);
    draw_wheel(F, 0);
    begin_part(P_THRUAXLE); M_DARK(); call(L_THRU_R); call(L_THRU_F);
    begin_part(P_CALIPERS);
    draw_caliper(R, 14, ROTOR_Z_R, 1);
    draw_caliper(F, 160, ROTOR_Z_F, 0);
}

void bike_draw(const DrawOpts *o) {
    g_o = o;
    draw_bike();
}

static void ellipse(float cx, float cz, float rx, float rz, float y, float a0, float a1, float r, float g, float b) {
    int i;
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r, g, b, a0); glVertex3f(cx, y, cz);
    for (i = 0; i <= 48; i++) { float t = (float)i / 48 * 2 * PI_F; glColor4f(r, g, b, a1); glVertex3f(cx + cosf(t) * rx, y, cz + sinf(t) * rz); }
    glEnd();
}

void bike_draw_ground(int stencil, const DrawOpts *o) {
    DrawOpts sh = *o;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    /* broad soft ambient shadow plus contact shadows under each tyre */
    ellipse(520, 0, 820, 300, 0.2f, 0.10f, 0.0f, 0.08f, 0.11f, 0.13f);
    ellipse(R.x + 10, 0, 160, 40, 0.3f, 0.30f, 0.0f, 0.05f, 0.07f, 0.08f);
    ellipse(F.x + 10, 0, 160, 40, 0.3f, 0.30f, 0.0f, 0.05f, 0.07f, 0.08f);
    if (o->show_patch) {
        WHCalc w = wh_calc();
        float hl = w.len * 0.5f, hw = 15;
        ellipse(R.x, 0, hl, hw, 0.6f, 0.85f, 0.85f, 0.88f, 0.34f, 0.12f);
    }
    if (stencil) {   /* planar projected shadow, each pixel darkened once */
        float lx = -0.35f, ly = -1.0f, lz = -0.45f;
        float m[16] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0.4f, 0, 1 };
        m[4] = -lx / ly; m[6] = -lz / ly;
        glEnable(GL_STENCIL_TEST);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilFunc(GL_EQUAL, 0, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
        glColor4f(0.06f, 0.09f, 0.11f, 0.11f);
        glPushMatrix(); glMultMatrixf(m);
        sh.mode = DRAW_SHADOW; g_o = &sh;
        draw_bike();
        glPopMatrix();
        glDisable(GL_STENCIL_TEST);
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

V3 bike_anchor(int p) {
    if (p == P_PEDALS) { float a = (float)sim.r.crank; return v3(BB.x + cosf(a) * CRANK_LEN, BB.y + sinf(a) * CRANK_LEN, 110); }
    if (p == P_RD) { pose_drive(); return v3(g_gp.x, g_gp.y - 20, g_zp + 10); }
    if (p == P_FD) { FDCalc fc = fd_calc(); return v3(PARTS[p].anchor.x, PARTS[p].anchor.y, fc.cage + 8); }
    return PARTS[p].anchor;
}

int pick_decode(const unsigned char rgb[3]) {
    int id = (rgb[0] + 5) / 10 - 1;
    if (rgb[1] > 8 || rgb[2] > 8) return -1;
    return (id >= 0 && id < P_COUNT) ? id : -1;
}
