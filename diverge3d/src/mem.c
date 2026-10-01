#include "mem.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Each block carries a small header with its size so we can keep exact
   byte counts without a lookup table. */
typedef struct { size_t size; size_t magic; } Hdr;
#define MAGIC ((size_t)0x5EEDB10Cu)
#define HDR_SZ ((sizeof(Hdr) + 15) & ~(size_t)15)

static long g_blocks, g_bytes, g_peak;

static void *finish(char *raw, size_t n) {
    Hdr *h;
    if (!raw) { fprintf(stderr, "out of memory (%lu bytes)\n", (unsigned long)n); exit(1); }
    h = (Hdr *)raw; h->size = n; h->magic = MAGIC;
    g_blocks++; g_bytes += (long)n; if (g_bytes > g_peak) g_peak = g_bytes;
    return raw + HDR_SZ;
}
void *mem_alloc(size_t n)  { return finish((char *)malloc(HDR_SZ + n), n); }
void *mem_calloc(size_t n) { return finish((char *)calloc(1, HDR_SZ + n), n); }
void mem_free(void *p) {
    Hdr *h;
    if (!p) return;
    h = (Hdr *)((char *)p - HDR_SZ);
    if (h->magic != MAGIC) { fprintf(stderr, "mem_free: bad pointer\n"); abort(); }
    h->magic = 0; g_blocks--; g_bytes -= (long)h->size;
    free(h);
}
void *mem_realloc(void *p, size_t n) {
    Hdr *h; char *raw;
    if (!p) return mem_alloc(n);
    h = (Hdr *)((char *)p - HDR_SZ);
    if (h->magic != MAGIC) { fprintf(stderr, "mem_realloc: bad pointer\n"); abort(); }
    g_bytes -= (long)h->size; g_blocks--;
    raw = (char *)realloc(h, HDR_SZ + n);
    return finish(raw, n);
}
long mem_live_blocks(void) { return g_blocks; }
long mem_live_bytes(void)  { return g_bytes; }
long mem_peak_bytes(void)  { return g_peak; }
