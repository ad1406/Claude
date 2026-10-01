/* The 3D bicycle: geometry built once into display lists, drawn per frame
   with the moving parts posed from the simulation. Units are millimetres;
   x forward, y up, z toward the drive (right) side. */
#ifndef BIKE_H
#define BIKE_H
#include "mathx.h"
#include "content.h"

enum { DRAW_NORMAL, DRAW_PICK, DRAW_SHADOW };
typedef struct {
    int mode;
    unsigned char hl[P_COUNT];   /* highlighted parts */
    int any_hl;
    float pulse;                 /* 0..1 highlight glow */
    int show_patch;              /* draw tyre contact patch (wheels bench) */
    int fd_focus, rd_focus;      /* reserved */
} DrawOpts;

int  bike_init(void);
void bike_free(void);
void bike_rebuild_drive(void);      /* after chainring/cassette change */
void bike_draw(const DrawOpts *o);
void bike_draw_ground(int stencil, const DrawOpts *o);
V3   bike_anchor(int part);
int  pick_decode(const unsigned char rgb[3]);
#endif
