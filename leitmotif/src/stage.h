/* The stage: one plane on which every picture in the two chapters is drawn.
   Each scene is a function of time and a few parameters; some have handles
   the reader can drag and a slider. */
#ifndef STAGE_H
#define STAGE_H
#include "draw.h"
#include "platform.h"

enum {
    SC_NONE,
    SC_STRING,      /* p0 mode: 0 two free pulses, 1 one nail (odd mirror), 2 two nails (hall of mirrors), 3 two mirrors = one shift */
    SC_SPACETIME,   /* p0 mode: 0 u,v axes, 1 factored derivatives, 2 hold: constant along lines, 3 f(u)+g(v) field, 4 as (1+i)(x+ict) */
    SC_STANDING,    /* p0 n, p1 finger at l/N (0 = none), p2 show travelling parts */
    SC_PERIODS,     /* p0 mode: 0 a periodic function and its periods, 1 periods add, 2 shrinking periods vs continuity */
    SC_PHASORS,     /* p0 mode: 0 beats (phasors + sound), 1 parallelogram (drag), p1 beat Hz */
    SC_PLANE,       /* p0 op (PL_*) */
    SC_EXP,         /* p0 mode: 0 e^{it} on the circle, 1 e^{lambda t} spiral + velocity, 2 the map e^z, 3 turns add */
    SC_WINDING,     /* p0 mode: 0 whole turns average to zero, 1 geometric series, 2 Dirichlet sum; p1 k or N */
    SC_POLARIZE,    /* p0 phase: 0 the path, 1 turned back, 2 shadows */
    SC_RIEMANN,     /* p0 mode: 0 upper/lower sums, 1 delta-bad intervals, 2 M-L box, 3 lanes oscillate less, 4 piecewise continuous */
    SC_SEPARATION,  /* p0 initial k as a multiple of (pi/l)^2 */
    SC_LANES,       /* p0 mode: 0 a curve and its two lane shadows, 1 a sequence converging, 2 derivative lane by lane, 3 integral lane by lane */
    SC_SPLIT,       /* the Split etude: (a,b) <-> (middle, half-gap) */
    SC_HOLD,        /* the Hold etude: a field constant along one direction */
    SC_GROUND,      /* the drone */
    SC_COUNT
};
enum { PL_SUM, PL_PRODUCT, PL_CONJ, PL_REIM, PL_MODSQ, PL_INVERSE, PL_DIVIDE, PL_TRIANGLE, PL_SHADOW, PL_POLAR, PL_ITURN, PL_COUNT };

typedef struct {
    int scene; float p[4];
    float t;                 /* scene time, seconds */
    float slider; int has_slider; const char *slider_label; float smin, smax, sstep;
    float h[4][2];           /* draggable handles, world coords */
    int nh, drag;            /* handle being dragged */
    int sound;               /* scene wants its tones */
} Stage;

void stage_set(Stage *st, int scene, const float *p);
/* draws into r; returns 1 if the mouse is over a handle (for the cursor) */
int  stage_draw(Stage *st, Rect r, const Input *in, float dt, int interactive);
const char *stage_caption(const Stage *st);
#endif
