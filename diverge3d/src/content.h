#ifndef CONTENT_H
#define CONTENT_H
#include "mathx.h"

enum { P_FRAME, P_FORK, P_HEADSET, P_COCKPIT, P_LEVERS, P_CABLES, P_SEATPOST, P_SADDLE, P_BB, P_CRANKSET,
       P_PEDALS, P_CHAIN, P_FD, P_RD, P_HANGER, P_CASSETTE, P_HUBS, P_WHEELS, P_TIRES, P_THRUAXLE,
       P_ROTORS, P_CALIPERS, P_COUNT };
enum { G_FRAME, G_DRIVE, G_BRAKE, G_WHEEL, G_CONTACT, G_COUNT };
enum { B_GEARING, B_RD, B_FD, B_BRAKES, B_WHEELS, B_STEER, B_COUNT };
enum { SYS_SHIFT = 1, SYS_DRIVE = 2, SYS_BRAKE = 4, SYS_WHEEL = 8, SYS_STEER = 16, SYS_NOISE = 32 };
#define SYS_N 6
enum { A_NONE, A_PEDAL_ON, A_PEDAL_OFF, A_SHIFT_EASIER, A_BRAKE_ON, A_BRAKE_OFF };

typedef struct {
    const char *name; int group;
    const char *does, *spec, *check;
    int links[8];      /* -1 terminated */
    int probs[8];      /* -1 terminated, problem indices */
    int bench;         /* -1 none */
    V3 anchor; float dist, yaw, pitch;
} PartInfo;

typedef struct {
    const char *title, *sub; int sys, sev; const char *time, *diff;
    const char *causes[6], *check, *steps[8], *tools, *shop;
    int parts[8]; int bench;   /* bench used by "Recreate this fault", -1 none */
} Problem;

typedef struct { int part; const char *text; int action; } Step;
typedef struct { const char *name, *sub; Step steps[10]; int n; } Run;

typedef struct {
    const char *name, *sub, *intro, *howto_title, *howto[8];
    V3 target; float dist, yaw, pitch;
} BenchInfo;

extern const PartInfo PARTS[P_COUNT];
extern const char *GROUP_NAMES[G_COUNT];
extern const Problem PROBS[];
extern const int NPROBS;
extern const char *SYS_NAMES[SYS_N];
extern const Run RUNS[];
extern const int NRUNS;
extern const BenchInfo BENCHES[B_COUNT];
#endif
