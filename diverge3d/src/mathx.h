#ifndef MATHX_H
#define MATHX_H
#include <math.h>

#define PI_F 3.14159265358979f
#define DEG(a) ((a) * (PI_F / 180.0f))

typedef struct { float x, y, z; } V3;
typedef struct { float m[16]; } M4;   /* column-major, like OpenGL */

static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 v3add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 v3sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline V3 v3mul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 v3cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float v3len(V3 a) { return sqrtf(v3dot(a, a)); }
static inline V3 v3norm(V3 a) { float l = v3len(a); return l > 1e-9f ? v3mul(a, 1.0f / l) : v3(0, 0, 0); }
static inline V3 v3lerp(V3 a, V3 b, float t) { return v3add(a, v3mul(v3sub(b, a), t)); }
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }

static inline M4 m4_identity(void) { M4 r = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}}; return r; }
static inline M4 m4_mul(M4 a, M4 b) {
    M4 r; int i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        float s = 0; for (k = 0; k < 4; k++) s += a.m[k * 4 + j] * b.m[i * 4 + k];
        r.m[i * 4 + j] = s;
    }
    return r;
}
static inline M4 m4_perspective(float fovy, float aspect, float n, float f) {
    M4 r = {{0}}; float t = 1.0f / tanf(fovy * 0.5f);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
static inline M4 m4_lookat(V3 eye, V3 at, V3 up) {
    V3 f = v3norm(v3sub(at, eye)), s = v3norm(v3cross(f, up)), u = v3cross(s, f);
    M4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3dot(s, eye); r.m[13] = -v3dot(u, eye); r.m[14] = v3dot(f, eye);
    return r;
}
/* Projects p; returns 0 if behind the camera. Out: window x,y (top-left origin) */
static inline int m4_project(M4 mvp, V3 p, float vx, float vy, float vw, float vh, float *ox, float *oy) {
    float x = mvp.m[0]*p.x + mvp.m[4]*p.y + mvp.m[8]*p.z + mvp.m[12];
    float y = mvp.m[1]*p.x + mvp.m[5]*p.y + mvp.m[9]*p.z + mvp.m[13];
    float w = mvp.m[3]*p.x + mvp.m[7]*p.y + mvp.m[11]*p.z + mvp.m[15];
    if (w <= 1e-4f) return 0;
    *ox = vx + (x / w * 0.5f + 0.5f) * vw;
    *oy = vy + (1.0f - (y / w * 0.5f + 0.5f)) * vh;
    return 1;
}
#endif
