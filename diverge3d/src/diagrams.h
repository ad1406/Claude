#ifndef DIAGRAMS_H
#define DIAGRAMS_H
#include "ui.h"
typedef struct { int kind; char text[240]; } Msg;   /* kind: 0 note, 1 ok, 2 watch, 3 fault */
float diagram_draw(int bench, float x, float y, float w);   /* returns height used */
int   bench_messages(int bench, Msg *out, int max);
#endif
