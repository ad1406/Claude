/* Procedural mesh builder. Shapes are generated into a temporary
   vertex buffer, then compiled into an OpenGL display list; the CPU
   copy is freed immediately, so meshes cost no heap after start-up. */
#ifndef MESH_H
#define MESH_H
#include "mathx.h"

typedef struct { float *v; int n, cap; } MB;   /* n = vertex count; 6 floats each (pos, normal) */

void mb_init(MB *b);
void mb_free(MB *b);
unsigned mb_compile(MB *b);                      /* returns GL list; frees b */
void mb_tri(MB *b, V3 a, V3 na, V3 c, V3 nc, V3 d, V3 nd);
void mb_quad(MB *b, V3 a, V3 bb, V3 c, V3 d, V3 n);  /* flat quad */

/* Frame around an axis: returns two unit vectors perpendicular to dir */
void mb_basis(V3 dir, V3 *u, V3 *w);

void mb_cylinder(MB *b, V3 a, V3 c, float ra, float rc, int seg, int caps);
void mb_tube(MB *b, const V3 *pts, const float *rad, int n, int seg, int caps);
void mb_smooth_tube(MB *b, const V3 *ctrl, int nctrl, float r0, float r1, int sub, int seg, int caps);
void mb_torus_band(MB *b, float R, float r, float v0, float v1, int su, int sv);   /* around z axis */
void mb_box(MB *b, V3 c, V3 ax, V3 ay, V3 az, float hx, float hy, float hz);       /* oriented box */
void mb_ellipsoid(MB *b, V3 c, V3 rad, int su, int sv);
void mb_lathe(MB *b, const float *prof, int np, int seg);   /* profile (r,z) pairs, around z axis */
void mb_gear(MB *b, int teeth, float pitch_r, float inner_r, float thick, float tooth_h);   /* in XY, centred z=0 */
void mb_annulus(MB *b, float r0, float r1, float thick, int seg);
#endif
