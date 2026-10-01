/* Ride physics and the mechanism models behind each bench. */
#ifndef SIM_H
#define SIM_H

#define PITCH_MM 12.7f
#define CIRC_M   2.19f
#define MAX_COGS 10

typedef struct { const char *name; int teeth[2]; } RingSet;
typedef struct { const char *name; int n; int cogs[MAX_COGS]; float pitch; } Cassette;
extern const RingSet RINGSETS[4];
extern const Cassette CASSETTES[4];

typedef struct {
    int build;                 /* 0 base (Claris, mech), 1 elite (GRX, hydro) */
    int ringset, cassette;
    int front, rear;           /* indices */
    int pedaling; float cadence;
    float v;                   /* m/s */
    float brake;               /* 0..1 lever */
    double crank, wheel, cog, chain;   /* angles (rad) and chain travel (mm) */
    int engaged;
    float slowmo;              /* animation time scale */
} Ride;

typedef struct { float barrel, H, L, hanger; } RDState;    /* barrel in quarter turns */
typedef struct { float tension, L, H; int trim; } FDState;
typedef struct { int hydro; float lever, pad, stretch, barrel, inner, air; int contam, warp; } BRState;
typedef struct { float psi, kg; int tubeless; float terrain; } WHState;
typedef struct { float preload; } STState;

typedef struct { float pos, err, Hstop, Lstop, sp; int n; } RDCalc;
typedef struct { float chain_at, cage, d, clear, Lstop, Hstop, ring_pos, cog_pos; } FDCalc;
typedef struct { float go, gi, need, pad_move, bite, force, slack; } BRCalc;
typedef struct { float rear_kg, front_kg, guide, guide_f, area, len, sag; } WHCalc;

typedef struct { Ride r; RDState rd; FDState fd; BRState br; WHState wh; STState st; float rotor_phase, hub_a, body_a, pawl_phase; } Sim;
extern Sim sim;

void sim_reset(int build);
void sim_apply_build(int build);
void sim_step(float dt);
int  ring_teeth(void);
int  cog_teeth(void);
int  ncogs(void);
float ratio(void);
float v_drive(void);
float cog_z(int i);          /* lateral position of cog i, mm from centreline */
float ring_z(int f);
int  cross_chained(int f, int i);
RDCalc rd_calc(void);
FDCalc fd_calc(void);
BRCalc br_calc(float lever);
WHCalc wh_calc(void);
float pitch_radius(int teeth);   /* mm */
#endif
