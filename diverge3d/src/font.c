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

/* Characters baked into the atlas: printable ASCII, Latin-1, and a few
   typographic symbols the content uses. */
static const int EXTRA[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
                             0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x2032, 0x2033, 0x2248, 0x2264, 0x2265 };
#define NEXTRA ((int)(sizeof EXTRA / sizeof EXTRA[0]))
#define NBASIC 95
#define NLATIN 96

typedef struct {
    stbtt_packedchar basic[NBASIC], latin[NLATIN], extra[NEXTRA];
    float size, ascent, line;
} Face;

static Face g_face[F_COUNT];
static GLuint g_tex;
static int g_aw, g_ah;

static unsigned char *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb"); unsigned char *buf; long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    buf = (unsigned char *)mem_alloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); mem_free(buf); return NULL; }
    fclose(f); *len = n; return buf;
}

int font_init(float dpi) {
    static const float sizes[F_COUNT] = { 12.5f, 14.5f, 14.5f, 17.5f, 22.0f, 15.5f };
    static const int bold[F_COUNT] = { 0, 0, 1, 1, 1, 1 };
    unsigned char *ttf[2] = { NULL, NULL }, *bmp = NULL;
    char path[512]; long len; int i, ok = 0, attempt;

    for (i = 0; i < 2; i++) {
        if (plat_font_path(i, path, sizeof path)) ttf[i] = read_file(path, &len);
        if (!ttf[i] && i == 1) ttf[1] = NULL;   /* fall back to regular for bold */
    }
    if (!ttf[0]) { fprintf(stderr, "no system font found\n"); return 0; }

    g_aw = 1024; g_ah = 1024;
    if (dpi > 1.3f) { g_aw = 2048; g_ah = 2048; }
    for (attempt = 0; attempt < 2 && !ok; attempt++) {
        stbtt_pack_context pc;
        bmp = (unsigned char *)mem_calloc((size_t)g_aw * g_ah);
        if (stbtt_PackBegin(&pc, bmp, g_aw, g_ah, 0, 1, NULL)) {
            ok = 1;
            stbtt_PackSetOversampling(&pc, 2, 1);
            for (i = 0; i < F_COUNT && ok; i++) {
                unsigned char *data = (bold[i] && ttf[1]) ? ttf[1] : ttf[0];
                Face *fc = &g_face[i];
                stbtt_pack_range r[3]; stbtt_fontinfo info; int asc, desc, gap; float sc;
                fc->size = sizes[i] * dpi;
                memset(r, 0, sizeof r);
                r[0].font_size = fc->size; r[0].first_unicode_codepoint_in_range = 32; r[0].num_chars = NBASIC; r[0].chardata_for_range = fc->basic;
                r[1].font_size = fc->size; r[1].first_unicode_codepoint_in_range = 160; r[1].num_chars = NLATIN; r[1].chardata_for_range = fc->latin;
                r[2].font_size = fc->size; r[2].array_of_unicode_codepoints = (int *)EXTRA; r[2].num_chars = NEXTRA; r[2].chardata_for_range = fc->extra;
                if (!stbtt_PackFontRanges(&pc, data, 0, r, 3)) ok = 0;
                stbtt_InitFont(&info, data, stbtt_GetFontOffsetForIndex(data, 0));
                stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
                sc = stbtt_ScaleForPixelHeight(&info, fc->size);
                fc->ascent = asc * sc; fc->line = (asc - desc + gap) * sc * 1.08f;
            }
            stbtt_PackEnd(&pc);
        }
        if (!ok) { mem_free(bmp); bmp = NULL; g_aw *= 2; g_ah *= 2; }
    }
    mem_free(ttf[0]); mem_free(ttf[1]);
    if (!ok) return 0;
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

static stbtt_packedchar *glyph(Face *f, unsigned cp) {
    int i;
    if (cp >= 32 && cp < 127) return &f->basic[cp - 32];
    if (cp >= 160 && cp < 256) return &f->latin[cp - 160];
    for (i = 0; i < NEXTRA; i++) if ((unsigned)EXTRA[i] == cp) return &f->extra[i];
    return &f->basic['?' - 32];
}

float font_width(int fi, const char *s, int n) {
    Face *f = &g_face[fi]; const char *end = n < 0 ? s + strlen(s) : s + n; float w = 0;
    while (s < end && *s) { unsigned cp = utf8_next(&s); w += glyph(f, cp)->xadvance; }
    return w;
}
float font_line(int f) { return g_face[f].line; }
float font_ascent(int f) { return g_face[f].ascent; }

float font_draw(int fi, float x, float y, const char *s, int n, Color c) {
    Face *f = &g_face[fi]; const char *end = n < 0 ? s + strlen(s) : s + n;
    float base = (float)(int)(y + f->ascent + 0.5f);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    while (s < end && *s) {
        unsigned cp = utf8_next(&s); stbtt_packedchar *g = glyph(f, cp);
        float iw = 1.0f / g_aw, ih = 1.0f / g_ah;
        float x0 = x + g->xoff, y0 = base + g->yoff, x1 = x + g->xoff2, y1 = base + g->yoff2;
        glTexCoord2f(g->x0 * iw, g->y0 * ih); glVertex2f(x0, y0);
        glTexCoord2f(g->x1 * iw, g->y0 * ih); glVertex2f(x1, y0);
        glTexCoord2f(g->x1 * iw, g->y1 * ih); glVertex2f(x1, y1);
        glTexCoord2f(g->x0 * iw, g->y1 * ih); glVertex2f(x0, y1);
        x += g->xadvance;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return x;
}

float font_wrap(int fi, float x, float y, float maxw, const char *s, Color c, int draw) {
    float lh = font_line(fi), h = 0;
    while (*s) {
        const char *line = s, *brk = NULL, *p = s; float w = 0;
        while (*p && *p != '\n') {
            const char *q = p; unsigned cp = utf8_next(&q);
            float cw = glyph(&g_face[fi], cp)->xadvance;
            if (cp == ' ') brk = p;
            if (w + cw > maxw && p > line) {
                if (!brk) brk = p;   /* no space: hard break */
                break;
            }
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
