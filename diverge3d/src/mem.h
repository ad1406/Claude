/* Tracked heap allocation. Every allocation in the program goes through
   these, so shutdown can prove nothing was leaked. */
#ifndef MEM_H
#define MEM_H
#include <stddef.h>

void  *mem_alloc(size_t n);
void  *mem_calloc(size_t n);
void  *mem_realloc(void *p, size_t n);
void   mem_free(void *p);
long   mem_live_blocks(void);
long   mem_live_bytes(void);
long   mem_peak_bytes(void);
#endif
