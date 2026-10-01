/* Font atlas: several system fonts packed into one alpha texture with
   stb_truetype. Each face is a (font file, pixel size) pair; glyphs missing
   from a face's font are taken from a fallback chain (regular serif, then
   math and symbol fonts), so Greek, operators and music signs always resolve. */
#include "font.h"
#include "mem.h"
#include "platform.h"
#include "gl_inc.h"
#include <stdio.h>
#include <string.h>

#define STBTT_malloc(x, u) ((void)(u), mem_alloc(x))
#define STBTT_free(x, u)   ((void)(u), mem_free(x))
#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"

enum { FL_SANS, FL_SANSB, FL_SERIF, FL_SERIFI, FL_SERIFB, FL_SYM1, FL_SYM2, FL_COUNT };

typedef struct { const char *file; int index; } FontCand;
#ifdef _WIN32
static const FontCand CANDS[FL_COUNT][4] = {
    { {"segoeui.ttf",0}, {"arial.ttf",0}, {"tahoma.ttf",0}, {0,0} },
    { {"segoeuib.ttf",0}, {"arialbd.ttf",0}, {"tahomabd.ttf",0}, {0,0} },
    { {"cambria.ttc",0}, {"georgia.ttf",0}, {"times.ttf",0}, {0,0} },
    { {"cambriai.ttf",0}, {"georgiai.ttf",0}, {"timesi.ttf",0}, {0,0} },
    { {"cambriab.ttf",0}, {"georgiab.ttf",0}, {"timesbd.ttf",0}, {0,0} },
    { {"cambria.ttc",1}, {"seguisym.ttf",0}, {"times.ttf",0}, {0,0} },
    { {"seguisym.ttf",0}, {"arial.ttf",0}, {0,0}, {0,0} },
};
#else
#define DJ "/usr/share/fonts/truetype/dejavu/"
#define LB "/usr/share/fonts/truetype/liberation/"
#define FF "/usr/share/fonts/truetype/freefont/"
static const FontCand CANDS[FL_COUNT][4] = {
    { {DJ "DejaVuSans.ttf",0}, {LB "LiberationSans-Regular.ttf",0}, {"/usr/share/fonts/TTF/DejaVuSans.ttf",0}, {0,0} },
    { {DJ "DejaVuSans-Bold.ttf",0}, {LB "LiberationSans-Bold.ttf",0}, {"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",0}, {0,0} },
    { {LB "LiberationSerif-Regular.ttf",0}, {FF "FreeSerif.ttf",0}, {DJ "DejaVuSerif.ttf",0}, {0,0} },
    { {LB "LiberationSerif-Italic.ttf",0}, {FF "FreeSerifItalic.ttf",0}, {DJ "DejaVuSerif-Italic.ttf",0}, {0,0} },
    { {LB "LiberationSerif-Bold.ttf",0}, {FF "FreeSerifBold.ttf",0}, {DJ "DejaVuSerif-Bold.ttf",0}, {0,0} },
    { {FF "FreeSerif.ttf",0}, {DJ "DejaVuSerif.ttf",0}, {0,0}, {0,0} },
    { {DJ "DejaVuSans.ttf",0}, {FF "FreeSans.ttf",0}, {0,0}, {0,0} },
};
#endif

/* Code points baked into every face. The first 95 are printable ASCII so
   lookup for those is direct. TX marks text-only extras skipped by math faces. */
#define TX 0x80000000u
static const unsigned CP_EXTRA[] = {
    0xA0, 0xA7, 0xB0, 0xB1, 0xB2, 0xB3, 0xB7, 0xB9, 0xD7, 0xF7, 0xAC,
    0x131, 0xC0|TX, 0xC9|TX, 0xE0|TX, 0xE1|TX, 0xE8|TX, 0xE9|TX, 0xEA|TX, 0xED|TX, 0xF3|TX, 0xF6|TX, 0xFC|TX, 0xE7|TX,
    0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0398, 0x039B, 0x039E, 0x03A0, 0x03A3, 0x03A6, 0x03A8, 0x03A9,
    0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD,
    0x03BE, 0x03C0, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03D1, 0x03D5, 0x03F5,
    0x2080, 0x2081, 0x2082, 0x2083, 0x2096, 0x2099, 0x2009, 0x2013, 0x203A, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2032, 0x2033,
    0x2102, 0x2113, 0x2115, 0x211D, 0x2124,
    0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x21A6, 0x21D0, 0x21D2, 0x21D4, 0x21BA, 0x21BB,
    0x2200, 0x2202, 0x2203, 0x2205, 0x2206, 0x2208, 0x2209, 0x2211, 0x2212, 0x2213, 0x2218, 0x221A, 0x221E,
    0x2223, 0x2225, 0x2227, 0x2228, 0x2229, 0x222A, 0x222B, 0x2248, 0x2260, 0x2261, 0x2262, 0x2264, 0x2265,
    0x2282, 0x2286, 0x2295, 0x22C5, 0x22EF, 0x25A1, 0x25B8, 0x25BE, 0x25CB, 0x25CF, 0x2663, 0x2669, 0x266A, 0x266D, 0x266F,
    0x27E8, 0x27E9, 0x1D11E, 0x1D122,
    0x212C, 0x2130, 0x2131, 0x210B, 0x2110, 0x2112, 0x2133, 0x211B,
    0x1D49C, 0x1D49E, 0x1D49F, 0x1D4A2, 0x1D4A5, 0x1D4A6, 0x1D4A9, 0x1D4AA, 0x1D4AB, 0x1D4AC, 0x1D4AE, 0x1D4AF,
    0x1D4B0, 0x1D4B1, 0x1D4B2, 0x1D4B3, 0x1D4B4, 0x1D4B5,
};
#define NEXTRA ((int)(sizeof CP_EXTRA / sizeof CP_EXTRA[0]))
#define NCP (95 + NEXTRA)

static unsigned g_cps[NCP];       /* sorted code points */
static int g_textonly[NCP];

typedef struct {
    stbtt_packedchar pc[NCP];
    unsigned char have[NCP];
    float px, ascent, descent, line;
} Face;

static Face g_face[F_COUNT];
static GLuint g_tex;
static int g_aw, g_ah;
/* math ladder in pixels at 96 dpi */
static const float MATH_PX[MATH_SIZES] = { 9.5f, 11, 12.5f, 14, 16, 18, 20.5f, 23, 26, 30, 35, 41 };
static float g_dpi = 1;

static unsigned char *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb"); unsigned char *buf; long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    buf = (unsigned char *)mem_alloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); mem_free(buf); return NULL; }
    fclose(f); *len = n; return buf;
}

typedef struct { unsigned char *data; int index, offset; stbtt_fontinfo info; int ok; } Loaded;

static int face_file(int f, int *chain) {
    /* returns primary file and fills a fallback chain (terminated by -1) */
    int prim;
    if (f <= F_UI) prim = FL_SANS;
    else if (f == F_UIB || f == F_CAP) prim = FL_SANSB;
    else if (f == F_TXI || f == F_TXSI || f == F_H2I || f == F_H1I) prim = FL_SERIFI;
    else if (f == F_TXB) prim = FL_SERIFB;
    else if (f < F_MATH0) prim = FL_SERIF;
    else prim = ((f - F_MATH0) % 2) ? FL_SERIFI : FL_SERIF;
    chain[0] = prim;
    if (prim == FL_SERIFI || prim == FL_SERIFB) { chain[1] = FL_SERIF; chain[2] = FL_SYM1; chain[3] = FL_SYM2; chain[4] = -1; }
    else if (prim == FL_SANS || prim == FL_SANSB) { chain[1] = FL_SYM2; chain[2] = FL_SYM1; chain[3] = -1; }
    else { chain[1] = FL_SYM1; chain[2] = FL_SYM2; chain[3] = -1; }
    return prim;
}

static float face_size(int f) {
    static const float sizes[F_MATH0] = { 11, 12.5f, 14, 14, 10.5f, 16, 16, 16, 14, 14, 19, 25, 25, 42, 42 };
    if (f < F_MATH0) return sizes[f];
    return MATH_PX[(f - F_MATH0) / 2];
}

int font_init(float dpi) {
    Loaded L[FL_COUNT];
    char dir[512], path[1024]; long len; int i, j, ok = 0, attempt;
    unsigned char *bmp = NULL;
    static int cps_ready;
    static int idx_tmp[NCP]; static stbtt_packedchar pc_tmp[NCP]; static int cp_tmp[NCP];

    g_dpi = dpi;
    if (!cps_ready) {
        for (i = 0; i < 95; i++) { g_cps[i] = 32 + (unsigned)i; g_textonly[i] = 0; }
        for (i = 0; i < NEXTRA; i++) {
            unsigned c = CP_EXTRA[i] & ~TX; int t = (CP_EXTRA[i] & TX) != 0;
            j = 95 + i - 1;   /* insertion sort by code point */
            while (j >= 95 && g_cps[j] > c) { g_cps[j + 1] = g_cps[j]; g_textonly[j + 1] = g_textonly[j]; j--; }
            g_cps[j + 1] = c; g_textonly[j + 1] = t;
        }
        cps_ready = 1;
    }

    plat_font_dir(dir, sizeof dir);
    memset(L, 0, sizeof L);
    for (i = 0; i < FL_COUNT; i++) {
        for (j = 0; j < 4 && CANDS[i][j].file && !L[i].ok; j++) {
            snprintf(path, sizeof path, "%s%s", dir, CANDS[i][j].file);
            L[i].data = read_file(path, &len);
            if (!L[i].data) continue;
            L[i].index = CANDS[i][j].index;
            L[i].offset = stbtt_GetFontOffsetForIndex(L[i].data, L[i].index);
            if (L[i].offset < 0 || !stbtt_InitFont(&L[i].info, L[i].data, L[i].offset)) { mem_free(L[i].data); L[i].data = NULL; continue; }
            L[i].ok = 1;
            plat_log("font %d: %s (face %d, %ld bytes)", i, path, CANDS[i][j].index, len);
        }
    }
    /* any missing family borrows the nearest one that loaded */
    {   static const int alt[FL_COUNT][3] = { {FL_SERIF, FL_SYM2, FL_SYM1}, {FL_SANS, FL_SERIFB, FL_SERIF}, {FL_SYM1, FL_SANS, FL_SYM2},
                                              {FL_SERIF, FL_SANS, FL_SYM1}, {FL_SERIF, FL_SANSB, FL_SANS}, {FL_SERIF, FL_SYM2, FL_SANS}, {FL_SANS, FL_SYM1, FL_SERIF} };
        for (i = 0; i < FL_COUNT; i++) if (!L[i].ok) for (j = 0; j < 3; j++) if (L[alt[i][j]].ok && L[alt[i][j]].data) {
            L[i].info = L[alt[i][j]].info; L[i].offset = L[alt[i][j]].offset; L[i].index = L[alt[i][j]].index; L[i].ok = 2; break; }
    }
    if (!L[FL_SANS].ok && !L[FL_SERIF].ok) { plat_log("no system font found in '%s'", dir); fprintf(stderr, "no system font found\n"); return 0; }

    g_aw = 2048; g_ah = 2048;
    if (dpi > 1.3f) { g_aw = 4096; g_ah = 4096; }
    for (attempt = 0; attempt < 3 && !ok; attempt++) {
        stbtt_pack_context pc;
        bmp = (unsigned char *)mem_calloc((size_t)g_aw * g_ah);
        if (stbtt_PackBegin(&pc, bmp, g_aw, g_ah, 0, 1, NULL)) {
            int f;
            ok = 1;
            for (f = 0; f < F_COUNT && ok; f++) {
                Face *fc = &g_face[f]; int chain[6], c, prim = face_file(f, chain);
                int asc, desc, gap; float sc;
                fc->px = face_size(f) * dpi;
                memset(fc->have, 0, sizeof fc->have);
                for (c = 0; chain[c] >= 0 && ok; c++) {
                    Loaded *ld = &L[chain[c]]; int n = 0, k;
                    stbtt_pack_range r;
                    if (!ld->ok) continue;
                    for (k = 0; k < NCP; k++) {
                        if (fc->have[k]) continue;
                        if (f >= F_MATH0 && g_textonly[k]) continue;
                        if (k >= 95 || c == 0) {
                            if (!stbtt_FindGlyphIndex(&ld->info, (int)g_cps[k])) continue;
                        }
                        idx_tmp[n] = k; cp_tmp[n] = (int)g_cps[k]; n++;
                    }
                    if (!n) continue;
                    memset(&r, 0, sizeof r);
                    r.font_size = fc->px; r.array_of_unicode_codepoints = cp_tmp; r.num_chars = n; r.chardata_for_range = pc_tmp;
                    stbtt_PackSetOversampling(&pc, fc->px < 20 ? 2 : 1, 1);
                    /* stb wants the font's index within a collection here, not its byte offset */
                    if (!stbtt_PackFontRanges(&pc, ld->info.data, ld->index, &r, 1)) { ok = 0; break; }
                    for (k = 0; k < n; k++) { fc->pc[idx_tmp[k]] = pc_tmp[k]; fc->have[idx_tmp[k]] = 1; }
                }
                {   Loaded *ld = L[prim].ok ? &L[prim] : &L[FL_SERIF];
                    stbtt_GetFontVMetrics(&ld->info, &asc, &desc, &gap);
                    sc = stbtt_ScaleForPixelHeight(&ld->info, fc->px);
                    fc->ascent = asc * sc; fc->descent = -desc * sc; fc->line = (asc - desc + gap) * sc * 1.06f;
                }
            }
            stbtt_PackEnd(&pc);
        }
        plat_log("font atlas %dx%d: %s", g_aw, g_ah, ok ? "packed" : "too small, retrying");
        if (!ok) { mem_free(bmp); bmp = NULL; if (g_aw == g_ah) g_aw *= 2; else g_ah *= 2; }
    }
    for (i = 0; i < FL_COUNT; i++) if (L[i].ok == 1) mem_free(L[i].data);
    if (!ok) { fprintf(stderr, "font atlas too large\n"); return 0; }
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, g_aw, g_ah, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bmp);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    mem_free(bmp);
    return 1;
}

void font_free(void) { if (g_tex) { glDeleteTextures(1, &g_tex); g_tex = 0; } }

unsigned utf8_next(const char **ps) {
    const unsigned char *s = (const unsigned char *)*ps; unsigned c = s[0];
    if (c < 0x80) { *ps += 1; return c; }
    if ((c & 0xE0) == 0xC0 && s[1]) { *ps += 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0 && s[1] && s[2]) { *ps += 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    if ((c & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) { *ps += 4; return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); }
    *ps += 1; return '?';
}

static int cp_index(unsigned cp) {
    int lo = 95, hi = NCP - 1;
    if (cp >= 32 && cp < 127) return (int)cp - 32;
    while (lo <= hi) { int m = (lo + hi) / 2; if (g_cps[m] == cp) return m; if (g_cps[m] < cp) lo = m + 1; else hi = m - 1; }
    return -1;
}
static stbtt_packedchar *glyph(Face *f, unsigned cp) {
    int i = cp_index(cp);
    if (cp == 0x2009) { i = 0; }      /* thin space drawn as a narrowed space below */
    if (i < 0 || !f->have[i]) i = '?' - 32;
    return &f->pc[i];
}

int font_has(int f, unsigned cp) { int i = cp_index(cp); return i >= 0 && g_face[f].have[i]; }
float font_px(int f) { return g_face[f].px; }

float font_width(int fi, const char *s, int n) {
    Face *f = &g_face[fi]; const char *end = n < 0 ? s + strlen(s) : s + n; float w = 0;
    while (s < end && *s) { unsigned cp = utf8_next(&s); w += cp == 0x2009 ? glyph(f, ' ')->xadvance * 0.5f : glyph(f, cp)->xadvance; }
    return w;
}
float font_line(int f) { return g_face[f].line; }
float font_ascent(int f) { return g_face[f].ascent; }
float font_descent(int f) { return g_face[f].descent; }

static void quad(stbtt_packedchar *g, float x, float base, float s) {
    float iw = 1.0f / g_aw, ih = 1.0f / g_ah;
    float x0 = x + g->xoff * s, y0 = base + g->yoff * s, x1 = x + g->xoff2 * s, y1 = base + g->yoff2 * s;
    glTexCoord2f(g->x0 * iw, g->y0 * ih); glVertex2f(x0, y0);
    glTexCoord2f(g->x1 * iw, g->y0 * ih); glVertex2f(x1, y0);
    glTexCoord2f(g->x1 * iw, g->y1 * ih); glVertex2f(x1, y1);
    glTexCoord2f(g->x0 * iw, g->y1 * ih); glVertex2f(x0, y1);
}

float font_draw_base(int fi, float x, float base, const char *s, int n, Color c) {
    Face *f = &g_face[fi]; const char *end = n < 0 ? s + strlen(s) : s + n;
    base = (float)(int)(base + 0.5f);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    while (s < end && *s) {
        unsigned cp = utf8_next(&s); stbtt_packedchar *g;
        if (cp == 0x2009) { x += glyph(f, ' ')->xadvance * 0.5f; continue; }
        g = glyph(f, cp);
        quad(g, x, base, 1);
        x += g->xadvance;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return x;
}
float font_draw(int fi, float x, float y, const char *s, int n, Color c) {
    return font_draw_base(fi, x, y + g_face[fi].ascent, s, n, c);
}

void font_glyph(int fi, unsigned cp, GlyphM *m) {
    stbtt_packedchar *g = glyph(&g_face[fi], cp);
    m->adv = g->xadvance; m->x0 = g->xoff; m->y0 = g->yoff; m->x1 = g->xoff2; m->y1 = g->yoff2;
}
void font_glyph_draw(int fi, unsigned cp, float x, float base, float s, Color c) {
    stbtt_packedchar *g = glyph(&g_face[fi], cp);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS); quad(g, x, s == 1 ? (float)(int)(base + 0.5f) : base, s); glEnd();
    glDisable(GL_TEXTURE_2D);
}

int font_math(float px, int italic) {
    int i, best = 0; float bd = 1e9f;
    for (i = 0; i < MATH_SIZES; i++) { float d = MATH_PX[i] * g_dpi - px; if (d < 0) d = -d * 1.15f; if (d < bd) { bd = d; best = i; } }
    return F_MATH0 + best * 2 + (italic ? 1 : 0);
}
float font_math_px(int i) { return MATH_PX[i]; }

float font_wrap(int fi, float x, float y, float maxw, const char *s, Color c, int draw) {
    float lh = font_line(fi), h = 0;
    while (*s) {
        const char *line = s, *brk = NULL, *p = s; float w = 0;
        while (*p && *p != '\n') {
            const char *q = p; unsigned cp = utf8_next(&q);
            float cw = glyph(&g_face[fi], cp)->xadvance;
            if (cp == ' ') brk = p;
            if (w + cw > maxw && p > line) { if (!brk) brk = p; break; }
            w += cw; p = q;
        }
        if (!*p || *p == '\n') brk = p;
        if (draw) font_draw(fi, x, y + h, line, (int)(brk - line), c);
        h += lh;
        s = brk;
        if (*s == ' ' || *s == '\n') s++;
    }
    return h;
}
