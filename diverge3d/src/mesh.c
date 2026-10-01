#include "mesh.h"
#include "mem.h"
#include "gl_inc.h"
#include <string.h>

void mb_init(MB *b) { b->v = NULL; b->n = 0; b->cap = 0; }
void mb_free(MB *b) { mem_free(b->v); mb_init(b); }

static void push(MB *b, V3 p, V3 n) {
    float *d;
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 1024;
        b->v = (float *)mem_realloc(b->v, (size_t)b->cap * 6 * sizeof(float));
    }
    d = b->v + b->n * 6;
    d[0] = p.x; d[1] = p.y; d[2] = p.z; d[3] = n.x; d[4] = n.y; d[5] = n.z;
    b->n++;
}
void mb_tri(MB *b, V3 a, V3 na, V3 c, V3 nc, V3 d, V3 nd) { push(b, a, na); push(b, c, nc); push(b, d, nd); }
void mb_quad(MB *b, V3 a, V3 c, V3 d, V3 e, V3 n) { mb_tri(b, a, n, c, n, d, n); mb_tri(b, a, n, d, n, e, n); }

unsigned mb_compile(MB *b) {
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    if (b->n) {
        glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_NORMAL_ARRAY);
        glVertexPointer(3, GL_FLOAT, 6 * sizeof(float), b->v);
        glNormalPointer(GL_FLOAT, 6 * sizeof(float), b->v + 3);
        glDrawArrays(GL_TRIANGLES, 0, b->n);
        glDisableClientState(GL_NORMAL_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    }
    glEndList();
    mb_free(b);
    return list;
}

void mb_basis(V3 d, V3 *u, V3 *w) {
    V3 ref = fabsf(d.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    *u = v3norm(v3cross(d, ref)); *w = v3cross(d, *u);
}

static void ring_pts(V3 c, V3 u, V3 w, float r, int seg, int i, V3 *p, V3 *n) {
    float a = (float)i / seg * 2 * PI_F;
    *n = v3add(v3mul(u, cosf(a)), v3mul(w, sinf(a)));
    *p = v3add(c, v3mul(*n, r));
}

static void cap(MB *b, V3 c, V3 u, V3 w, float r, int seg, V3 nrm, int flip) {
    int i;
    for (i = 0; i < seg; i++) {
        V3 p0, p1, n0, n1;
        ring_pts(c, u, w, r, seg, i, &p0, &n0); ring_pts(c, u, w, r, seg, i + 1, &p1, &n1);
        if (flip) mb_tri(b, c, nrm, p1, nrm, p0, nrm); else mb_tri(b, c, nrm, p0, nrm, p1, nrm);
    }
}

void mb_cylinder(MB *b, V3 a, V3 c, float ra, float rc, int seg, int caps) {
    V3 d = v3norm(v3sub(c, a)), u, w; int i;
    float slope = (ra - rc) / (v3len(v3sub(c, a)) + 1e-6f);
    mb_basis(d, &u, &w);
    for (i = 0; i < seg; i++) {
        V3 p0, p1, q0, q1, n0, n1, m0, m1;
        ring_pts(a, u, w, ra, seg, i, &p0, &n0); ring_pts(a, u, w, ra, seg, i + 1, &p1, &n1);
        ring_pts(c, u, w, rc, seg, i, &q0, &m0); ring_pts(c, u, w, rc, seg, i + 1, &q1, &m1);
        n0 = v3norm(v3add(n0, v3mul(d, slope))); n1 = v3norm(v3add(n1, v3mul(d, slope)));
        mb_tri(b, p0, n0, q0, n0, q1, n1); mb_tri(b, p0, n0, q1, n1, p1, n1);
    }
    if (caps) { cap(b, a, u, w, ra, seg, v3mul(d, -1), 1); cap(b, c, u, w, rc, seg, d, 0); }
}

/* Sweep a circle along a polyline using parallel-transported frames */
void mb_tube(MB *b, const V3 *p, const float *rad, int n, int seg, int caps) {
    V3 u, w, prevd; int i, j;
    V3 d0 = v3norm(v3sub(p[1], p[0]));
    mb_basis(d0, &u, &w); prevd = d0;
    {
        V3 ring_prev[64], nrm_prev[64];
        if (seg > 63) seg = 63;
        for (i = 0; i < n; i++) {
            V3 d, ring[64], nrm[64];
            if (i == 0) d = d0;
            else if (i == n - 1) d = v3norm(v3sub(p[i], p[i - 1]));
            else d = v3norm(v3add(v3norm(v3sub(p[i], p[i - 1])), v3norm(v3sub(p[i + 1], p[i]))));
            if (i > 0) {   /* rotate frame from prevd to d */
                V3 ax = v3cross(prevd, d); float s = v3len(ax), c = v3dot(prevd, d);
                if (s > 1e-6f) {
                    float ang = atan2f(s, c); V3 k = v3mul(ax, 1.0f / s);
                    V3 vs[2]; int q; vs[0] = u; vs[1] = w;
                    for (q = 0; q < 2; q++) {   /* Rodrigues */
                        V3 v = vs[q];
                        vs[q] = v3add(v3add(v3mul(v, cosf(ang)), v3mul(v3cross(k, v), sinf(ang))), v3mul(k, v3dot(k, v) * (1 - cosf(ang))));
                    }
                    u = v3norm(vs[0]); w = v3norm(vs[1]);
                }
                prevd = d;
            }
            for (j = 0; j <= seg; j++) ring_pts(p[i], u, w, rad[i], seg, j, &ring[j], &nrm[j]);
            if (i > 0)
                for (j = 0; j < seg; j++) {
                    mb_tri(b, ring_prev[j], nrm_prev[j], ring[j], nrm[j], ring[j + 1], nrm[j + 1]);
                    mb_tri(b, ring_prev[j], nrm_prev[j], ring[j + 1], nrm[j + 1], ring_prev[j + 1], nrm_prev[j + 1]);
                }
            if (caps && (i == 0 || i == n - 1)) cap(b, p[i], u, w, rad[i], seg, i == 0 ? v3mul(d, -1) : d, i == 0);
            memcpy(ring_prev, ring, sizeof(V3) * (size_t)(seg + 1));
            memcpy(nrm_prev, nrm, sizeof(V3) * (size_t)(seg + 1));
        }
    }
}

static V3 catmull(V3 p0, V3 p1, V3 p2, V3 p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    V3 r = v3mul(p1, 2);
    r = v3add(r, v3mul(v3sub(p2, p0), t));
    r = v3add(r, v3mul(v3add(v3sub(v3add(v3mul(p0, 2), v3mul(p2, 4)), v3mul(p1, 5)), v3mul(p3, -1)), t2));
    r = v3add(r, v3mul(v3add(v3sub(v3mul(p1, 3), p0), v3sub(p3, v3mul(p2, 3))), t3));
    return v3mul(r, 0.5f);
}

void mb_smooth_tube(MB *b, const V3 *c, int nc, float r0, float r1, int sub, int seg, int caps) {
    V3 pts[256]; float rad[256]; int i, k, n = 0, total = (nc - 1) * sub + 1;
    for (i = 0; i < nc - 1; i++)
        for (k = 0; k < sub; k++) {
            V3 p0 = c[i > 0 ? i - 1 : 0], p3 = c[i + 2 < nc ? i + 2 : nc - 1];
            if (n < 255) { pts[n] = catmull(p0, c[i], c[i + 1], p3, (float)k / sub); rad[n] = lerpf(r0, r1, (float)n / (total - 1)); n++; }
        }
    pts[n] = c[nc - 1]; rad[n] = r1; n++;
    mb_tube(b, pts, rad, n, seg, caps);
}

void mb_torus_band(MB *b, float R, float r, float v0, float v1, int su, int sv) {
    int i, j;
    for (i = 0; i < su; i++) for (j = 0; j < sv; j++) {
        V3 p[4], nn[4]; int q;
        for (q = 0; q < 4; q++) {
            float u = (float)(i + (q == 1 || q == 2)) / su * 2 * PI_F;
            float v = v0 + (v1 - v0) * (float)(j + (q >= 2)) / sv;
            V3 rd = v3(cosf(u), sinf(u), 0);
            nn[q] = v3add(v3mul(rd, cosf(v)), v3(0, 0, sinf(v)));
            p[q] = v3add(v3mul(rd, R), v3mul(nn[q], r));
        }
        mb_tri(b, p[0], nn[0], p[3], nn[3], p[2], nn[2]);
        mb_tri(b, p[0], nn[0], p[2], nn[2], p[1], nn[1]);
    }
}

void mb_box(MB *b, V3 c, V3 ax, V3 ay, V3 az, float hx, float hy, float hz) {
    V3 X = v3mul(ax, hx), Y = v3mul(ay, hy), Z = v3mul(az, hz);
    V3 v[8]; int i;
    static const int f[6][4] = { {1,3,7,5}, {0,4,6,2}, {2,6,7,3}, {0,1,5,4}, {4,5,7,6}, {0,2,3,1} };
    V3 nr[6];
    nr[0] = ax; nr[1] = v3mul(ax, -1); nr[2] = ay; nr[3] = v3mul(ay, -1); nr[4] = az; nr[5] = v3mul(az, -1);
    for (i = 0; i < 8; i++)
        v[i] = v3add(c, v3add(v3mul(X, (i & 1) ? 1.f : -1.f), v3add(v3mul(Y, (i & 2) ? 1.f : -1.f), v3mul(Z, (i & 4) ? 1.f : -1.f))));
    for (i = 0; i < 6; i++) mb_quad(b, v[f[i][0]], v[f[i][1]], v[f[i][2]], v[f[i][3]], nr[i]);
}

void mb_ellipsoid(MB *b, V3 c, V3 r, int su, int sv) {
    int i, j;
    for (i = 0; i < su; i++) for (j = 0; j < sv; j++) {
        V3 p[4], n[4]; int q;
        for (q = 0; q < 4; q++) {
            float u = (float)(i + (q == 1 || q == 2)) / su * 2 * PI_F;
            float v = -PI_F / 2 + PI_F * (float)(j + (q >= 2)) / sv;
            V3 s = v3(cosf(v) * cosf(u), sinf(v), cosf(v) * sinf(u));
            p[q] = v3add(c, v3(s.x * r.x, s.y * r.y, s.z * r.z));
            n[q] = v3norm(v3(s.x / r.x, s.y / r.y, s.z / r.z));
        }
        mb_tri(b, p[0], n[0], p[2], n[2], p[1], n[1]);
        mb_tri(b, p[0], n[0], p[3], n[3], p[2], n[2]);
    }
}

void mb_lathe(MB *b, const float *pr, int np, int seg) {
    int i, j;
    for (j = 0; j < np - 1; j++) {
        float r0 = pr[j * 2], z0 = pr[j * 2 + 1], r1 = pr[j * 2 + 2], z1 = pr[j * 2 + 3];
        float dr = r1 - r0, dz = z1 - z0, l = sqrtf(dr * dr + dz * dz) + 1e-6f;
        float nr = dz / l, nz = -dr / l;   /* outward normal of profile edge */
        for (i = 0; i < seg; i++) {
            float a0 = (float)i / seg * 2 * PI_F, a1 = (float)(i + 1) / seg * 2 * PI_F;
            V3 c0 = v3(cosf(a0), sinf(a0), 0), c1 = v3(cosf(a1), sinf(a1), 0);
            V3 n0 = v3add(v3mul(c0, nr), v3(0, 0, nz)), n1 = v3add(v3mul(c1, nr), v3(0, 0, nz));
            V3 p00 = v3add(v3mul(c0, r0), v3(0, 0, z0)), p01 = v3add(v3mul(c1, r0), v3(0, 0, z0));
            V3 p10 = v3add(v3mul(c0, r1), v3(0, 0, z1)), p11 = v3add(v3mul(c1, r1), v3(0, 0, z1));
            mb_tri(b, p00, n0, p01, n1, p11, n1); mb_tri(b, p00, n0, p11, n1, p10, n0);
        }
    }
}

void mb_annulus(MB *b, float r0, float r1, float t, int seg) {
    float prof[10];
    prof[0] = r0; prof[1] = t / 2; prof[2] = r1; prof[3] = t / 2; prof[4] = r1; prof[5] = -t / 2;
    prof[6] = r0; prof[7] = -t / 2; prof[8] = r0; prof[9] = t / 2;
    mb_lathe(b, prof, 5, seg);
}

/* Toothed disc: teeth on the outside, flat faces, inner cut-out. */
void mb_gear(MB *b, int teeth, float pr, float ir, float t, float th) {
    int n = teeth * 4, i;
    float z0 = -t / 2, z1 = t / 2;
    for (i = 0; i < n; i++) {
        float a0 = (float)i / n * 2 * PI_F, a1 = (float)(i + 1) / n * 2 * PI_F;
        int k0 = i % 4, k1 = (i + 1) % 4;
        /* profile: root, flank up, tip, flank down */
        float r0 = (k0 == 1 || k0 == 2) ? pr + th * 0.55f : pr - th * 0.45f;
        float r1 = (k1 == 1 || k1 == 2) ? pr + th * 0.55f : pr - th * 0.45f;
        V3 o0 = v3(cosf(a0) * r0, sinf(a0) * r0, 0), o1 = v3(cosf(a1) * r1, sinf(a1) * r1, 0);
        V3 i0 = v3(cosf(a0) * ir, sinf(a0) * ir, 0), i1 = v3(cosf(a1) * ir, sinf(a1) * ir, 0);
        V3 zf = v3(0, 0, 1), zb = v3(0, 0, -1);
        V3 O0f = v3add(o0, v3(0, 0, z1)), O1f = v3add(o1, v3(0, 0, z1)), I0f = v3add(i0, v3(0, 0, z1)), I1f = v3add(i1, v3(0, 0, z1));
        V3 O0b = v3add(o0, v3(0, 0, z0)), O1b = v3add(o1, v3(0, 0, z0)), I0b = v3add(i0, v3(0, 0, z0)), I1b = v3add(i1, v3(0, 0, z0));
        V3 en = v3norm(v3cross(v3sub(o1, o0), zf));   /* outward edge normal */
        V3 inn0 = v3(-cosf(a0), -sinf(a0), 0), inn1 = v3(-cosf(a1), -sinf(a1), 0);
        mb_quad(b, I0f, O0f, O1f, I1f, zf);
        mb_quad(b, I0b, I1b, O1b, O0b, zb);
        mb_quad(b, O0b, O1b, O1f, O0f, en);
        mb_tri(b, I0b, inn0, I0f, inn0, I1f, inn1); mb_tri(b, I0b, inn0, I1f, inn1, I1b, inn1);
    }
}
