/* The material of Chapters 1-2 as data: seven motifs (plus the ground they
   stand on), and every result and exercise as a "piece" whose derivation is
   a sequence of moves, each move one motif. */
#ifndef CONTENT_H
#define CONTENT_H
#include "draw.h"

typedef struct {
    const char *name;       /* "Mirror" */
    const char *verb;       /* what the move does, as a phrase */
    const char *question;   /* the question that summons it */
    const char *essence;    /* rich text */
    int harmonic;           /* which harmonic of the string it sounds */
    int scene; float p[4];  /* its etude */
} MotifInfo;
extern const MotifInfo MOTIFS[MO_COUNT];

typedef struct {
    int motif;
    const char *cue;        /* the question you ask yourself (rich text) */
    const char *tex;        /* what the move produces */
    const char *why;        /* rich text */
    const char *ref;        /* id of a piece this move leans on, or NULL */
    int scene; float p[4];  /* stage for this step; scene < 0 keeps the piece's */
} Step;

enum { K_GROUND, K_DEF, K_RESULT, K_PROBLEM };

typedef struct {
    const char *id;         /* "1.13", "L1.1", "P2.11b" */
    const char *label;      /* how the book names it: "(1.13)", "Lemma 1.1", "Problem 2.11(b)" */
    const char *name;
    int chapter;
    int section;            /* index into SECTIONS */
    int kind, essential;
    const char *statement;  /* tex */
    const char *gist;       /* rich text */
    const char *needs[6];
    const Step *steps; int nsteps;
    int scene; float p[4];
    const char *note;       /* errata and remarks (rich text) */
    const char *echo;       /* where the same music returns (rich text) */
} Piece;

typedef struct { const char *num, *title; int chapter; } Section;
extern const Section SECTIONS[];
extern const int NSECTIONS;

extern const Piece PIECES[];
extern const int NPIECES;

/* derived tables, built by content_init */
extern unsigned char PIECE_MOTIFS[][MO_COUNT];   /* count of steps per motif */
int  content_init(void);              /* returns number of unresolved references */
int  piece_find(const char *id);
int  piece_need(int p, int k);        /* resolved index of needs[k], -1 if none */
int  step_ref(int p, int s);          /* resolved index of steps[s].ref */
int  piece_users(int p, int *out, int cap);   /* pieces that need or cite p */
#endif
