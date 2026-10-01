#include "tex.h"
#include "draw.h"
#include <string.h>
#include <stdlib.h>

enum { N_ROW, N_GLYPH, N_TEXT, N_FRAC, N_SCRIPT, N_SQRT, N_OVER, N_HAT, N_BIGOP, N_DELIM, N_SPACE,
       N_COLOR, N_PMAT, N_UNDER, N_BIGDELIM };
enum { C_ORD, C_OP, C_BIN, C_REL, C_OPEN, C_CLOSE, C_PUNCT, C_INNER, C_NONE };

typedef struct {
    unsigned char type, cls, italic, flag;   /* flag: frac style, bigop limits, hl, under/over */
    unsigned cp, cp2;
    const char *s; int n;
    int a, b, c, next, col;
    float sz;
    float px, w, asc, desc;
    float ox, oy;          /* offset inside parent: x from parent's left, y = baseline raise */
    int face; float gscale;
} Node;

#define MAXN 3000
static Node N[MAXN];
static int nn;
static const char *P;
static Color g_pal[16];
static int g_npal;
static float g_dpi_px_min = 8;
int tex_unknown;

void tex_set_palette(const Color *cols, int n) {
    int i; g_npal = n > 16 ? 16 : n;
    for (i = 0; i < g_npal; i++) g_pal[i] = cols[i];
}

static int nnew(int type) {
    Node *n;
    if (nn >= MAXN - 1) { n = &N[MAXN - 1]; memset(n, 0, sizeof *n); n->type = N_SPACE; n->a = n->b = n->c = n->next = n->col = -1; return MAXN - 1; }
    n = &N[nn]; memset(n, 0, sizeof *n);
    n->type = (unsigned char)type; n->a = n->b = n->c = n->next = n->col = -1; n->cls = C_ORD;
    return nn++;
}

/* ------------------------------------------------------------- symbols */
enum { K_SYM, K_FUNC, K_FRAC, K_SQRT, K_OVER, K_HAT, K_LEFT, K_INT, K_SUM, K_LIMOP, K_TEXT, K_BB,
       K_CAL, K_COL, K_HL, K_SPACE, K_PMAT, K_UNDER, K_OVERSET, K_BIG, K_NOT, K_IGNORE };
typedef struct { const char *name; int kind; unsigned cp; int cls; int italic; float v; } Cmd;
static const Cmd CMDS[] = {
    {"alpha",K_SYM,0x3B1,C_ORD,1,0},{"beta",K_SYM,0x3B2,C_ORD,1,0},{"gamma",K_SYM,0x3B3,C_ORD,1,0},{"delta",K_SYM,0x3B4,C_ORD,1,0},
    {"epsilon",K_SYM,0x3F5,C_ORD,1,0},{"varepsilon",K_SYM,0x3B5,C_ORD,1,0},{"zeta",K_SYM,0x3B6,C_ORD,1,0},{"eta",K_SYM,0x3B7,C_ORD,1,0},
    {"theta",K_SYM,0x3B8,C_ORD,1,0},{"vartheta",K_SYM,0x3D1,C_ORD,1,0},{"iota",K_SYM,0x3B9,C_ORD,1,0},{"kappa",K_SYM,0x3BA,C_ORD,1,0},
    {"lambda",K_SYM,0x3BB,C_ORD,1,0},{"mu",K_SYM,0x3BC,C_ORD,1,0},{"nu",K_SYM,0x3BD,C_ORD,1,0},{"xi",K_SYM,0x3BE,C_ORD,1,0},
    {"pi",K_SYM,0x3C0,C_ORD,1,0},{"rho",K_SYM,0x3C1,C_ORD,1,0},{"sigma",K_SYM,0x3C3,C_ORD,1,0},{"tau",K_SYM,0x3C4,C_ORD,1,0},
    {"phi",K_SYM,0x3D5,C_ORD,1,0},{"varphi",K_SYM,0x3C6,C_ORD,1,0},{"chi",K_SYM,0x3C7,C_ORD,1,0},{"psi",K_SYM,0x3C8,C_ORD,1,0},
    {"omega",K_SYM,0x3C9,C_ORD,1,0},{"upsilon",K_SYM,0x3C5,C_ORD,1,0},
    {"Gamma",K_SYM,0x393,C_ORD,0,0},{"Delta",K_SYM,0x394,C_ORD,0,0},{"Theta",K_SYM,0x398,C_ORD,0,0},{"Lambda",K_SYM,0x39B,C_ORD,0,0},
    {"Xi",K_SYM,0x39E,C_ORD,0,0},{"Pi",K_SYM,0x3A0,C_ORD,0,0},{"Sigma",K_SYM,0x3A3,C_ORD,0,0},{"Phi",K_SYM,0x3A6,C_ORD,0,0},
    {"Psi",K_SYM,0x3A8,C_ORD,0,0},{"Omega",K_SYM,0x3A9,C_ORD,0,0},
    {"ell",K_SYM,0x2113,C_ORD,0,0},{"imath",K_SYM,0x131,C_ORD,1,0},{"partial",K_SYM,0x2202,C_ORD,0,0},{"infty",K_SYM,0x221E,C_ORD,0,0},
    {"cdot",K_SYM,0x22C5,C_BIN,0,0},{"times",K_SYM,0xD7,C_BIN,0,0},{"pm",K_SYM,0xB1,C_BIN,0,0},{"mp",K_SYM,0x2213,C_BIN,0,0},
    {"circ",K_SYM,0x2218,C_BIN,0,0},{"cup",K_SYM,0x222A,C_BIN,0,0},{"cap",K_SYM,0x2229,C_BIN,0,0},
    {"le",K_SYM,0x2264,C_REL,0,0},{"leq",K_SYM,0x2264,C_REL,0,0},{"leqslant",K_SYM,0x2264,C_REL,0,0},
    {"ge",K_SYM,0x2265,C_REL,0,0},{"geq",K_SYM,0x2265,C_REL,0,0},{"geqslant",K_SYM,0x2265,C_REL,0,0},
    {"ne",K_SYM,0x2260,C_REL,0,0},{"neq",K_SYM,0x2260,C_REL,0,0},{"approx",K_SYM,0x2248,C_REL,0,0},{"equiv",K_SYM,0x2261,C_REL,0,0},
    {"to",K_SYM,0x2192,C_REL,0,0},{"rightarrow",K_SYM,0x2192,C_REL,0,0},{"mapsto",K_SYM,0x21A6,C_REL,0,0},
    {"Rightarrow",K_SYM,0x21D2,C_REL,0,0},{"implies",K_SYM,0x21D2,C_REL,0,0},{"Leftarrow",K_SYM,0x21D0,C_REL,0,0},
    {"iff",K_SYM,0x21D4,C_REL,0,0},{"Leftrightarrow",K_SYM,0x21D4,C_REL,0,0},{"leftrightarrow",K_SYM,0x2194,C_REL,0,0},
    {"in",K_SYM,0x2208,C_REL,0,0},{"notin",K_SYM,0x2209,C_REL,0,0},{"subseteq",K_SYM,0x2286,C_REL,0,0},{"subset",K_SYM,0x2282,C_REL,0,0},
    {"mid",K_SYM,0x2223,C_REL,0,0},
    {"emptyset",K_SYM,0x2205,C_ORD,0,0},{"varnothing",K_SYM,0x2205,C_ORD,0,0},{"forall",K_SYM,0x2200,C_ORD,0,0},{"exists",K_SYM,0x2203,C_ORD,0,0},
    {"Box",K_SYM,0x25A1,C_ORD,0,0},{"square",K_SYM,0x25A1,C_ORD,0,0},{"prime",K_SYM,0x2032,C_ORD,0,0},{"neg",K_SYM,0xAC,C_ORD,0,0},
    {"langle",K_SYM,0x27E8,C_OPEN,0,0},{"rangle",K_SYM,0x27E9,C_CLOSE,0,0},{"lvert",K_SYM,'|',C_OPEN,0,0},{"rvert",K_SYM,'|',C_CLOSE,0,0},
    {"|",K_SYM,0x2225,C_ORD,0,0},{"{",K_SYM,'{',C_OPEN,0,0},{"}",K_SYM,'}',C_CLOSE,0,0},
    {"clubsuit",K_SYM,0x2663,C_ORD,0,0},{"ldots",K_SYM,0x2026,C_INNER,0,0},{"dots",K_SYM,0x2026,C_INNER,0,0},{"cdots",K_SYM,0x22EF,C_INNER,0,0},
    {"%",K_SYM,'%',C_ORD,0,0},{"#",K_SYM,'#',C_ORD,0,0},{"&",K_SYM,'&',C_ORD,0,0},{"_",K_SYM,'_',C_ORD,0,0},
    {"sin",K_FUNC,0,C_OP,0,0},{"cos",K_FUNC,0,C_OP,0,0},{"tan",K_FUNC,0,C_OP,0,0},{"exp",K_FUNC,0,C_OP,0,0},{"log",K_FUNC,0,C_OP,0,0},
    {"Re",K_FUNC,0,C_OP,0,0},{"Im",K_FUNC,0,C_OP,0,0},{"arg",K_FUNC,0,C_OP,0,0},{"arctan",K_FUNC,0,C_OP,0,0},
    {"const",K_FUNC,0,C_OP,0,0},{"floor",K_FUNC,0,C_OP,0,0},
    {"lim",K_LIMOP,0,C_OP,0,0},{"sup",K_LIMOP,0,C_OP,0,0},{"inf",K_LIMOP,0,C_OP,0,0},{"max",K_LIMOP,0,C_OP,0,0},{"min",K_LIMOP,0,C_OP,0,0},
    {"frac",K_FRAC,0,C_ORD,0,0},{"dfrac",K_FRAC,0,C_ORD,0,1},{"tfrac",K_FRAC,0,C_ORD,0,2},
    {"sqrt",K_SQRT,0,C_ORD,0,0},{"overline",K_OVER,0,C_ORD,0,0},{"bar",K_OVER,0,C_ORD,0,0},{"hat",K_HAT,0,C_ORD,0,0},
    {"left",K_LEFT,0,C_INNER,0,0},{"int",K_INT,0x222B,C_OP,0,0},{"sum",K_SUM,0x2211,C_OP,0,0},
    {"text",K_TEXT,0,C_ORD,0,0},{"mathrm",K_TEXT,0,C_ORD,0,1},{"operatorname",K_TEXT,0,C_OP,0,1},
    {"mathbb",K_BB,0,C_ORD,0,0},{"mathcal",K_CAL,0,C_ORD,0,0},{"R",K_SYM,0x211D,C_ORD,0,0},{"C",K_SYM,0x2102,C_ORD,0,0},{"N",K_SYM,0x2115,C_ORD,0,0},{"Z",K_SYM,0x2124,C_ORD,0,0},
    {"col",K_COL,0,C_ORD,0,0},{"hl",K_HL,0,C_ORD,0,0},
    {",",K_SPACE,0,C_NONE,0,0.17f},{":",K_SPACE,0,C_NONE,0,0.22f},{";",K_SPACE,0,C_NONE,0,0.28f},{"!",K_SPACE,0,C_NONE,0,-0.17f},
    {" ",K_SPACE,0,C_NONE,0,0.25f},{"quad",K_SPACE,0,C_NONE,0,1.0f},{"qquad",K_SPACE,0,C_NONE,0,2.0f},
    {"pmat",K_PMAT,0,C_INNER,0,0},{"under",K_UNDER,0,C_ORD,0,0},{"over",K_OVERSET,0,C_ORD,0,0},
    {"bigl",K_BIG,0,C_OPEN,0,1.25f},{"bigr",K_BIG,0,C_CLOSE,0,1.25f},{"Bigl",K_BIG,0,C_OPEN,0,1.7f},{"Bigr",K_BIG,0,C_CLOSE,0,1.7f},
    {"big",K_BIG,0,C_ORD,0,1.25f},{"Big",K_BIG,0,C_ORD,0,1.7f},
    {"not",K_NOT,0,C_REL,0,0},{"displaystyle",K_IGNORE,0,0,0,0},{"limits",K_IGNORE,0,0,0,0},{"nolimits",K_IGNORE,0,0,0,0},
};
#define NCMDS ((int)(sizeof CMDS / sizeof CMDS[0]))

static int is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static void skip_ws(void) { while (*P == ' ' || *P == '\t' || *P == '\n') P++; }

static int parse_row(int stop);
static int parse_atom(void);

static int parse_arg(void) {
    skip_ws();
    if (*P == '{') { P++; return parse_row('}'); }
    if (!*P) return nnew(N_ROW);
    return parse_atom();
}

/* raw text inside braces, for \text and \mathrm */
static void raw_group(const char **s, int *n) {
    int depth = 0;
    skip_ws();
    if (*P != '{') { *s = P; *n = *P ? 1 : 0; if (*P) P++; return; }
    P++; *s = P;
    while (*P && !(*P == '}' && depth == 0)) { if (*P == '{') depth++; if (*P == '}') depth--; P++; }
    *n = (int)(P - *s);
    if (*P == '}') P++;
}

static unsigned read_delim(void) {
    skip_ws();
    if (!*P) return '.';
    if (*P == '\\') {
        const char *q = P + 1; char name[16]; int k = 0;
        if (!is_alpha(*q)) { P += 2; return q[0] == '{' ? '{' : q[0] == '}' ? '}' : q[0] == '|' ? 0x2225 : '.'; }
        while (is_alpha(*q) && k < 15) name[k++] = *q++;
        name[k] = 0; P = q;
        if (!strcmp(name, "langle")) return 0x27E8;
        if (!strcmp(name, "rangle")) return 0x27E9;
        if (!strcmp(name, "lvert") || !strcmp(name, "rvert")) return '|';
        if (!strcmp(name, "Vert") || !strcmp(name, "lVert") || !strcmp(name, "rVert")) return 0x2225;
        return '.';
    }
    return (unsigned)(unsigned char)*P++;
}

static int glyph_node(unsigned cp, int cls, int italic) {
    int i = nnew(N_GLYPH); N[i].cp = cp; N[i].cls = (unsigned char)cls; N[i].italic = (unsigned char)italic; return i;
}

static int parse_cmd(void) {
    const char *q = P + 1; char name[32]; int k = 0, i, ci = -1;
    if (!is_alpha(*q)) { name[0] = *q; name[1] = 0; P = q + (*q ? 1 : 0); }
    else { while (is_alpha(*q) && k < 31) name[k++] = *q++; name[k] = 0; P = q; }
    for (i = 0; i < NCMDS; i++) if (!strcmp(CMDS[i].name, name)) { ci = i; break; }
    if (ci < 0) { int t = nnew(N_TEXT); N[t].s = "?"; N[t].n = 1; tex_unknown++; return t; }
    {   const Cmd *c = &CMDS[ci]; int n;
        switch (c->kind) {
        case K_SYM: return glyph_node(c->cp, c->cls, c->italic);
        case K_FUNC: n = nnew(N_TEXT); N[n].s = c->name; N[n].n = (int)strlen(c->name); N[n].cls = C_OP; return n;
        case K_LIMOP: n = nnew(N_BIGOP); N[n].s = c->name; N[n].n = (int)strlen(c->name); N[n].flag = 1; N[n].cls = C_OP; return n;
        case K_INT: n = nnew(N_BIGOP); N[n].cp = c->cp; N[n].flag = 0; N[n].cls = C_OP; return n;
        case K_SUM: n = nnew(N_BIGOP); N[n].cp = c->cp; N[n].flag = 1; N[n].cls = C_OP; return n;
        case K_FRAC: n = nnew(N_FRAC); N[n].flag = (unsigned char)c->v; N[n].a = parse_arg(); N[n].b = parse_arg(); return n;
        case K_SQRT: n = nnew(N_SQRT); N[n].a = parse_arg(); return n;
        case K_OVER: n = nnew(N_OVER); N[n].a = parse_arg(); return n;
        case K_HAT: n = nnew(N_HAT); N[n].a = parse_arg(); return n;
        case K_TEXT: n = nnew(N_TEXT); raw_group(&N[n].s, &N[n].n); N[n].flag = 1; N[n].cls = (unsigned char)c->cls; return n;
        case K_BB: {
            const char *s; int len; unsigned cp = 'R';
            raw_group(&s, &len);
            if (len > 0) cp = s[0] == 'C' ? 0x2102 : s[0] == 'N' ? 0x2115 : s[0] == 'Z' ? 0x2124 : 0x211D;
            return glyph_node(cp, C_ORD, 0); }
        case K_CAL: {
            /* script capitals; letters the fonts lack fall back to italic */
            static const unsigned hole[26] = { 0, 0x212C, 0, 0, 0x2130, 0x2131, 0, 0x210B, 0x2110, 0, 0, 0x2112, 0x2133,
                                               0, 0, 0, 0, 0x211B, 0, 0, 0, 0, 0, 0, 0, 0 };
            const char *s; int len, k, row = nnew(N_ROW), last = -1;
            raw_group(&s, &len);
            for (k = 0; k < len; k++) {
                int g; unsigned cp;
                if (s[k] >= 'A' && s[k] <= 'Z') {
                    cp = hole[s[k] - 'A'] ? hole[s[k] - 'A'] : 0x1D49C + (unsigned)(s[k] - 'A');
                    g = font_has(F_MATH0 + 8, cp) ? glyph_node(cp, C_ORD, 0) : glyph_node((unsigned)s[k], C_ORD, 1);
                } else g = glyph_node((unsigned)(unsigned char)s[k], C_ORD, 1);
                if (last < 0) N[row].a = g; else N[last].next = g;
                last = g;
            }
            return row; }
        case K_COL: case K_HL: {
            const char *s; int len;
            raw_group(&s, &len);
            n = nnew(N_COLOR); N[n].col = len > 0 ? atoi(s) : 0; N[n].flag = c->kind == K_HL;
            N[n].a = parse_arg();
            return n; }
        case K_SPACE: n = nnew(N_SPACE); N[n].sz = c->v; N[n].cls = C_NONE; return n;
        case K_PMAT: n = nnew(N_PMAT); N[n].a = parse_arg(); N[n].b = parse_arg(); N[n].cls = C_INNER; return n;
        case K_UNDER: case K_OVERSET:
            n = nnew(N_UNDER); N[n].flag = c->kind == K_OVERSET; N[n].a = parse_arg(); N[n].b = parse_arg(); return n;
        case K_BIG: n = nnew(N_BIGDELIM); N[n].cp = read_delim(); N[n].sz = c->v; N[n].cls = (unsigned char)c->cls;
            if (c->cls == C_ORD) N[n].cls = (N[n].cp == '(' || N[n].cp == '[' || N[n].cp == '{' || N[n].cp == 0x27E8) ? C_OPEN : C_CLOSE;
            if (N[n].cp == '|' || N[n].cp == 0x2225) N[n].cls = (unsigned char)(c->cls == C_ORD ? C_ORD : c->cls);
            return n;
        case K_LEFT: {
            n = nnew(N_DELIM); N[n].cls = C_INNER;
            N[n].cp = read_delim();
            N[n].a = parse_row('R');
            if (!strncmp(P, "\\right", 6)) { P += 6; N[n].cp2 = read_delim(); } else N[n].cp2 = '.';
            return n; }
        case K_NOT: {   /* negate the next relation */
            int a = parse_atom();
            if (N[a].type == N_GLYPH) {
                if (N[a].cp == 0x2261) N[a].cp = 0x2262; else if (N[a].cp == '=') N[a].cp = 0x2260; else if (N[a].cp == 0x2208) N[a].cp = 0x2209;
                N[a].cls = C_REL;
            }
            return a; }
        case K_IGNORE: default: n = nnew(N_SPACE); N[n].sz = 0; N[n].cls = C_NONE; return n;
        }
    }
}

static int parse_atom(void) {
    unsigned cp; const char *s;
    skip_ws();
    if (*P == '{') { int r; P++; r = parse_row('}'); return r; }
    if (*P == '\\') return parse_cmd();
    if (*P == '~') { int n = nnew(N_SPACE); P++; N[n].sz = 0.3f; N[n].cls = C_NONE; return n; }
    s = P; cp = utf8_next(&s); P = s;
    if (cp < 128 && is_alpha((int)cp)) return glyph_node(cp, C_ORD, 1);
    if (cp >= '0' && cp <= '9') return glyph_node(cp, C_ORD, 0);
    switch (cp) {
    case '+': return glyph_node('+', C_BIN, 0);
    case '-': return glyph_node(0x2212, C_BIN, 0);
    case '*': return glyph_node(0x22C5, C_BIN, 0);
    case '=': case '<': case '>': return glyph_node(cp, C_REL, 0);
    case ':': return glyph_node(':', C_REL, 0);
    case ',': case ';': return glyph_node(cp, C_PUNCT, 0);
    case '(': case '[': return glyph_node(cp, C_OPEN, 0);
    case ')': case ']': return glyph_node(cp, C_CLOSE, 0);
    case '\'': return glyph_node(0x2032, C_ORD, 0);
    case '!': return glyph_node('!', C_CLOSE, 0);
    }
    if ((cp >= 0x3B1 && cp <= 0x3C9) || cp == 0x3D1 || cp == 0x3D5 || cp == 0x3F5) return glyph_node(cp, C_ORD, 1);
    if ((cp >= 0x2190 && cp <= 0x21FF) || cp == 0x2264 || cp == 0x2265 || cp == 0x2260 || cp == 0x2248 || cp == 0x2208 || cp == 0x2261)
        return glyph_node(cp, C_REL, 0);
    if (cp == 0xB1 || cp == 0xD7 || cp == 0x22C5 || cp == 0x2218 || cp == 0x2212) return glyph_node(cp, C_BIN, 0);
    return glyph_node(cp, C_ORD, 0);
}

static int parse_row(int stop) {
    int row = nnew(N_ROW), last = -1;
    for (;;) {
        int at;
        skip_ws();
        if (!*P) break;
        if (*P == '}') { if (stop == '}') P++; else P++; if (stop == '}') break; continue; }
        if (stop == 'R' && !strncmp(P, "\\right", 6)) break;
        if (*P == '^' || *P == '_') { at = nnew(N_ROW); }   /* script on nothing */
        else at = parse_atom();
        for (;;) {
            skip_ws();
            if (*P == '^' || *P == '_') {
                char k = *P++; int arg = parse_arg();
                if (N[at].type == N_BIGOP) { if (k == '^') N[at].b = arg; else N[at].c = arg; }
                else {
                    if (N[at].type != N_SCRIPT || (k == '^' ? N[at].b >= 0 : N[at].c >= 0)) {
                        int sn = nnew(N_SCRIPT); N[sn].a = at; N[sn].cls = N[at].cls; at = sn; }
                    if (k == '^') N[at].b = arg; else N[at].c = arg;
                }
            } else break;
        }
        if (last < 0) N[row].a = at; else N[last].next = at;
        last = at;
    }
    return row;
}

/* -------------------------------------------------------------- layout */
static float axis_of(float px) { return px * 0.26f; }
static float child_px(float px, int level) { float p = px * (level == 0 ? 0.72f : 0.8f); return p < g_dpi_px_min ? g_dpi_px_min : p; }

static int eff_cls(int i) {
    Node *n = &N[i];
    if (n->type == N_COLOR) { int a = n->a; if (a >= 0 && N[a].type == N_ROW && N[a].a >= 0 && N[N[a].a].next < 0) return eff_cls(N[a].a); if (a >= 0 && N[a].type != N_ROW) return eff_cls(a); return C_ORD; }
    if (n->type == N_ROW) { int a = n->a; if (a >= 0 && N[a].next < 0) return eff_cls(a); return C_ORD; }
    return n->cls;
}

static float space_between(int l, int r, int level, float em) {
    float thin = 0.17f * em, med = 0.24f * em, thick = 0.30f * em;
    if (l == C_NONE || r == C_NONE) return 0;
    switch (l) {
    case C_ORD: if (r == C_OP || r == C_INNER) return thin; if (r == C_BIN) return level ? 0 : med; if (r == C_REL) return level ? 0 : thick; return 0;
    case C_OP: if (r == C_ORD || r == C_OP || r == C_INNER) return thin; if (r == C_REL) return level ? 0 : thick; return 0;
    case C_BIN: if (r == C_ORD || r == C_OP || r == C_OPEN || r == C_INNER) return level ? 0 : med; return 0;
    case C_REL: if (r == C_ORD || r == C_OP || r == C_OPEN || r == C_INNER) return level ? 0 : thick; return 0;
    case C_CLOSE: if (r == C_OP || r == C_INNER) return thin; if (r == C_BIN) return level ? 0 : med; if (r == C_REL) return level ? 0 : thick; return 0;
    case C_PUNCT: if (r != C_BIN && r != C_REL) return level ? 0 : thin; return 0;
    case C_INNER: if (r == C_BIN) return level ? 0 : med; if (r == C_REL) return level ? 0 : thick; if (r == C_CLOSE) return 0; return level ? 0 : thin;
    }
    return 0;
}

static void layout(int i, float px, int level);

static void ink_text(int face, const char *s, int n, float *w, float *asc, float *desc) {
    const char *e = s + n; float a = 0, d = 0, x = 0;
    while (s < e && *s) { GlyphM m; unsigned cp = utf8_next(&s); font_glyph(face, cp, &m); if (-m.y0 > a) a = -m.y0; if (m.y1 > d) d = m.y1; x += m.adv; }
    *w = x; *asc = a; *desc = d;
}

static void delim_size(float px, float hh, unsigned cp, float *w) {
    if (cp == '.' || cp == 0) { *w = px * 0.06f; return; }
    if (cp == '|') { *w = px * 0.32f; return; }
    if (cp == 0x2225) { *w = px * 0.45f; return; }
    *w = px * 0.36f + hh * 0.06f;
}

static void layout(int i, float px, int level) {
    Node *n = &N[i];
    n->px = px;
    switch (n->type) {
    case N_GLYPH: {
        GlyphM m; n->face = font_math(px, n->italic); font_glyph(n->face, n->cp, &m);
        n->w = m.adv; n->asc = -m.y0; n->desc = m.y1;
        if (n->asc < px * 0.1f) n->asc = px * 0.1f;
        if (n->desc < 0) n->desc = 0;
        if (n->cls == C_REL || n->cls == C_BIN) { if (n->asc < axis_of(px) * 2) n->asc = axis_of(px) * 2; }
        break; }
    case N_TEXT: n->face = font_math(px, 0); ink_text(n->face, n->s, n->n, &n->w, &n->asc, &n->desc);
        if (n->flag) { float a = font_ascent(n->face) * 0.72f; if (n->asc < a) n->asc = a; }
        break;
    case N_SPACE: n->w = n->sz * px; n->asc = 0; n->desc = 0; break;
    case N_ROW: {
        int c, prev = -1, pc = -1; float x = 0, a = 0, d = 0;
        int k;
        for (c = n->a; c >= 0; c = N[c].next) layout(c, px, level);
        /* binary operators that have nothing to bind to become ordinary */
        for (c = n->a; c >= 0; c = N[c].next) {
            int cl = eff_cls(c);
            if (cl == C_BIN) {
                int nx = N[c].next, ncl = nx >= 0 ? eff_cls(nx) : -1;
                if (pc < 0 || pc == C_BIN || pc == C_OP || pc == C_REL || pc == C_OPEN || pc == C_PUNCT || ncl < 0 || ncl == C_REL || ncl == C_CLOSE || ncl == C_PUNCT) {
                    if (N[c].type == N_GLYPH || N[c].type == N_COLOR) N[c].cls = C_ORD;
                    if (N[c].type == N_COLOR) { int a2 = N[c].a; if (a2 >= 0 && N[a2].type == N_ROW && N[a2].a >= 0) N[N[a2].a].cls = C_ORD; }
                    cl = C_ORD;
                }
            }
            if (cl != C_NONE) pc = cl;
        }
        pc = -1;
        for (c = n->a, k = 0; c >= 0; c = N[c].next, k++) {
            int cl = eff_cls(c);
            if (prev >= 0 && pc >= 0) x += space_between(pc, cl, level, px);
            N[c].ox = x; N[c].oy = 0;
            x += N[c].w;
            if (N[c].asc > a) a = N[c].asc;
            if (N[c].desc > d) d = N[c].desc;
            prev = c; if (cl != C_NONE) pc = cl;
        }
        n->w = x; n->asc = a; n->desc = d;
        if (!n->a) { n->asc = px * 0.5f; }
        break; }
    case N_FRAC: {
        int lv = (n->flag == 1 || (level == 0 && n->flag != 2)) ? level : level + 1;
        float cpx = lv == level ? px : child_px(px, level);
        float t = px * 0.05f, ax = axis_of(px), gap = px * 0.14f, pad = px * 0.12f, u, v, w;
        Node *a, *b;
        if (t < 1) t = 1;
        layout(n->a, cpx, lv); layout(n->b, cpx, lv);
        a = &N[n->a]; b = &N[n->b];
        u = ax + t / 2 + gap + a->desc; v = -ax + t / 2 + gap + b->asc;
        w = (a->w > b->w ? a->w : b->w) + 2 * pad;
        a->ox = (w - a->w) / 2; a->oy = u;
        b->ox = (w - b->w) / 2; b->oy = -v;
        n->w = w + px * 0.08f; n->asc = u + a->asc; n->desc = v + b->desc; n->sz = t;
        break; }
    case N_SCRIPT: {
        Node *base; float spx = child_px(px, level), kern = 0, su = 0, sd = 0, w2 = 0;
        layout(n->a, px, level); base = &N[n->a];
        if (base->type == N_GLYPH && base->italic) kern = px * 0.05f;
        if (n->b >= 0) { layout(n->b, spx, level + 1); su = base->asc - spx * 0.38f; if (su < px * 0.36f) su = px * 0.36f; if (su < N[n->b].desc + px * 0.22f) su = N[n->b].desc + px * 0.22f; }
        if (n->c >= 0) { layout(n->c, spx, level + 1); sd = base->desc + spx * 0.12f; if (sd < px * 0.17f) sd = px * 0.17f; if (sd < N[n->c].asc - px * 0.42f) sd = N[n->c].asc - px * 0.42f; }
        if (n->b >= 0 && n->c >= 0) {
            float gap = (su - N[n->b].desc) - (N[n->c].asc - sd);
            if (gap < px * 0.12f) { float d = px * 0.12f - gap; su += d * 0.5f; sd += d * 0.5f; }
        }
        base->ox = 0; base->oy = 0;
        if (n->b >= 0) { N[n->b].ox = base->w + kern; N[n->b].oy = su; if (N[n->b].w + kern > w2) w2 = N[n->b].w + kern; }
        if (n->c >= 0) { N[n->c].ox = base->w; N[n->c].oy = -sd; if (N[n->c].w > w2) w2 = N[n->c].w; }
        n->w = base->w + w2 + px * 0.04f;
        n->asc = base->asc; n->desc = base->desc;
        if (n->b >= 0 && su + N[n->b].asc > n->asc) n->asc = su + N[n->b].asc;
        if (n->c >= 0 && sd + N[n->c].desc > n->desc) n->desc = sd + N[n->c].desc;
        break; }
    case N_SQRT: {
        Node *b; float t = px * 0.05f, gap = px * 0.12f;
        if (t < 1) t = 1;
        layout(n->a, px, level); b = &N[n->a];
        b->ox = px * 0.62f; b->oy = 0;
        n->w = b->w + px * 0.72f; n->asc = b->asc + gap + t; n->desc = b->desc + px * 0.06f; n->sz = t;
        if (n->asc < px * 0.8f) n->asc = px * 0.8f;
        break; }
    case N_OVER: case N_HAT: {
        Node *b; layout(n->a, px, level); b = &N[n->a];
        b->ox = px * 0.04f; b->oy = 0;
        n->w = b->w + px * 0.08f; n->asc = b->asc + px * (n->type == N_OVER ? 0.16f : 0.26f); n->desc = b->desc;
        break; }
    case N_COLOR: { Node *b; layout(n->a, px, level); b = &N[n->a]; b->ox = n->flag ? px * 0.1f : 0; b->oy = 0;
        n->w = b->w + (n->flag ? px * 0.2f : 0); n->asc = b->asc; n->desc = b->desc; break; }
    case N_UNDER: {
        Node *a, *b; float spx = child_px(px, level), gap = px * 0.16f, br = px * 0.16f, w;
        layout(n->a, px, level); layout(n->b, spx, level + 1); a = &N[n->a]; b = &N[n->b];
        w = a->w > b->w ? a->w : b->w;
        a->ox = (w - a->w) / 2; a->oy = 0;
        b->ox = (w - b->w) / 2;
        if (!n->flag) { b->oy = -(a->desc + gap + br + gap * 0.5f + b->asc); n->asc = a->asc; n->desc = -b->oy + b->desc; }
        else { b->oy = a->asc + gap + br + gap * 0.5f + b->desc; n->asc = b->oy + b->asc; n->desc = a->desc; }
        n->w = w; n->sz = br;
        break; }
    case N_BIGOP: {
        float w, oa, od, gap = px * 0.12f; int limits = n->flag == 1 && level == 0;
        n->sz = 0; n->gscale = 1;
        if (n->s) {   /* lim, sup, ...: an upright word */
            n->face = font_math(px, 0); ink_text(n->face, n->s, n->n, &w, &oa, &od);
            if (oa < px * 0.68f) oa = px * 0.68f;
        } else {
            GlyphM m; float target = px * (n->cp == 0x222B ? 1.65f : 1.3f) * (level ? 0.8f : 1);
            n->face = font_math(target, 0); font_glyph(n->face, n->cp, &m);
            {   float hgt = m.y1 - m.y0, want = n->cp == 0x222B ? target * 1.05f : target * 0.95f;
                if (hgt > 1) n->gscale = want / hgt; }
            w = m.adv * n->gscale;
            {   float top = m.y0 * n->gscale, bot = m.y1 * n->gscale, mid = (top + bot) / 2;
                n->sz = -axis_of(px) - mid;          /* centre the sign on the math axis */
                oa = -(top + n->sz); od = bot + n->sz; }
            if (n->cp == 0x222B) w += px * 0.05f;
        }
        n->w = w; n->asc = oa; n->desc = od;
        if (n->b >= 0) layout(n->b, child_px(px, level), level + 1);
        if (n->c >= 0) layout(n->c, child_px(px, level), level + 1);
        if (limits) {
            float W = w;
            if (n->b >= 0 && N[n->b].w > W) W = N[n->b].w;
            if (n->c >= 0 && N[n->c].w > W) W = N[n->c].w;
            if (n->b >= 0) { N[n->b].ox = (W - N[n->b].w) / 2; N[n->b].oy = oa + gap + N[n->b].desc; n->asc = N[n->b].oy + N[n->b].asc; }
            if (n->c >= 0) { N[n->c].ox = (W - N[n->c].w) / 2; N[n->c].oy = -(od + gap + N[n->c].asc); n->desc = -N[n->c].oy + N[n->c].desc; }
            n->cp2 = (unsigned)((W - w) / 2 * 16); n->flag = 3; n->w = W;
        } else {      /* scripts to the right */
            int isint = n->cp == 0x222B;
            float x2 = w + (isint ? -px * 0.02f : px * 0.06f), right = w;
            if (n->b >= 0) { Node *b = &N[n->b]; b->ox = x2 + (isint ? px * 0.1f : 0); b->oy = isint ? oa - b->asc * 0.55f : px * 0.42f;
                if (b->ox + b->w > right) right = b->ox + b->w; if (b->oy + b->asc > n->asc) n->asc = b->oy + b->asc; }
            if (n->c >= 0) { Node *c2 = &N[n->c]; c2->ox = x2 - (isint ? px * 0.12f : 0); c2->oy = isint ? -(od - c2->asc * 0.45f) : -px * 0.22f;
                if (c2->ox + c2->w > right) right = c2->ox + c2->w; if (-c2->oy + c2->desc > n->desc) n->desc = -c2->oy + c2->desc; }
            n->w = right + px * 0.06f; n->cp2 = 0;
        }
        break; }
    case N_DELIM: case N_BIGDELIM: {
        float ax = axis_of(px), hh, wl, wr, bw = 0;
        if (n->type == N_DELIM) {
            Node *b; layout(n->a, px, level); b = &N[n->a];
            hh = b->asc - ax; if (b->desc + ax > hh) hh = b->desc + ax;
            hh += px * 0.04f; if (hh < px * 0.55f) hh = px * 0.55f;
            bw = b->w;
        } else hh = n->sz * px * 0.5f;
        delim_size(px, hh, n->cp, &wl);
        delim_size(px, hh, n->type == N_DELIM ? n->cp2 : '.', &wr);
        if (n->type == N_BIGDELIM) wr = 0;
        if (n->type == N_DELIM) { N[n->a].ox = wl; N[n->a].oy = 0; }
        n->w = wl + bw + wr; n->asc = ax + hh; n->desc = hh - ax; n->sz = n->type == N_BIGDELIM ? n->sz : hh / px;
        n->gscale = hh;
        break; }
    case N_PMAT: {
        Node *a, *b; float gap = px * 0.25f, ax = axis_of(px), w, h, top, dw;
        layout(n->a, px, level); layout(n->b, px, level); a = &N[n->a]; b = &N[n->b];
        w = a->w > b->w ? a->w : b->w;
        h = a->asc + a->desc + gap + b->asc + b->desc;
        top = ax + h / 2;
        dw = px * 0.42f;
        a->ox = dw + (w - a->w) / 2; a->oy = top - a->asc;
        b->ox = dw + (w - b->w) / 2; b->oy = top - h + b->desc;
        n->w = w + 2 * dw; n->asc = top + px * 0.08f; n->desc = h - top + px * 0.08f; n->gscale = h / 2 + px * 0.08f;
        break; }
    }
}

/* ------------------------------------------------------------- drawing */
static void stroke_paren(float x, float w, float ymid, float hh, int right, float px, Color c) {
    /* a parenthesis whose stroke thickens towards the middle */
    float pts[2 * 33], wid[33]; int k, n = 32;
    float xo = right ? x + w * 0.18f : x + w * 0.82f, xi = right ? x + w * 0.72f : x + w * 0.28f;
    for (k = 0; k <= n; k++) {
        float t = (float)k / n * 2 - 1, bulge = 1 - t * t;
        pts[2 * k] = xo + (xi - xo) * bulge;
        pts[2 * k + 1] = ymid + t * hh;
        wid[k] = px * (0.035f + 0.045f * bulge);
    }
    for (k = 0; k < n; k++) d_line(pts[2 * k], pts[2 * k + 1], pts[2 * k + 2], pts[2 * k + 3], wid[k] + 0.3f, c);
}
static void stroke_brace(float x, float w, float ymid, float hh, int right, float px, Color c) {
    float pts[2 * 41]; int k, n = 40; float lw = px * 0.05f;
    for (k = 0; k <= n; k++) {
        float t = (float)k / n * 2 - 1, at = t < 0 ? -t : t, xx;
        /* S-curves meeting in a cusp at the middle */
        float s = at < 0.12f ? 1 - at / 0.12f : 0;
        float body = 0.42f + 0.1f * (1 - (2 * at - 1) * (2 * at - 1));
        xx = at > 0.94f ? body + (at - 0.94f) / 0.06f * 0.28f : body;
        xx = xx * (1 - s) + 0.0f * s;
        if (!right) pts[2 * k] = x + w * xx + w * 0.08f; else pts[2 * k] = x + w * (1 - xx) - w * 0.08f;
        pts[2 * k + 1] = ymid + t * hh;
    }
    d_polyline(pts, n + 1, lw < 1 ? 1 : lw, c);
}
static void draw_delim(unsigned cp, float x, float w, float base, float px, float hh, int right, Color c) {
    float ymid = base - axis_of(px), lw = px * 0.05f;
    if (lw < 1) lw = 1;
    switch (cp) {
    case '(': stroke_paren(x, w, ymid, hh, 0, px, c); break;
    case ')': stroke_paren(x, w, ymid, hh, 1, px, c); break;
    case '[': d_line(x + w * 0.7f, ymid - hh, x + w * 0.3f, ymid - hh, lw, c); d_line(x + w * 0.3f, ymid - hh, x + w * 0.3f, ymid + hh, lw * 1.3f, c); d_line(x + w * 0.3f, ymid + hh, x + w * 0.7f, ymid + hh, lw, c); break;
    case ']': d_line(x + w * 0.3f, ymid - hh, x + w * 0.7f, ymid - hh, lw, c); d_line(x + w * 0.7f, ymid - hh, x + w * 0.7f, ymid + hh, lw * 1.3f, c); d_line(x + w * 0.7f, ymid + hh, x + w * 0.3f, ymid + hh, lw, c); break;
    case '{': stroke_brace(x, w, ymid, hh, 0, px, c); break;
    case '}': stroke_brace(x, w, ymid, hh, 1, px, c); break;
    case '|': d_line(x + w * 0.5f, ymid - hh, x + w * 0.5f, ymid + hh, lw * 1.1f, c); break;
    case 0x2225: d_line(x + w * 0.33f, ymid - hh, x + w * 0.33f, ymid + hh, lw * 1.1f, c); d_line(x + w * 0.67f, ymid - hh, x + w * 0.67f, ymid + hh, lw * 1.1f, c); break;
    case 0x27E8: d_line(x + w * 0.75f, ymid - hh, x + w * 0.25f, ymid, lw, c); d_line(x + w * 0.25f, ymid, x + w * 0.75f, ymid + hh, lw, c); break;
    case 0x27E9: d_line(x + w * 0.25f, ymid - hh, x + w * 0.75f, ymid, lw, c); d_line(x + w * 0.75f, ymid, x + w * 0.25f, ymid + hh, lw, c); break;
    default: break;
    }
    (void)right;
}

static void draw(int i, float x, float base, Color c) {
    Node *n = &N[i];
    float px = n->px;
    switch (n->type) {
    case N_GLYPH: {
        unsigned cp = n->cp;
        font_glyph_draw(n->face, cp, x, base, 1, c);
        break; }
    case N_TEXT: font_draw_base(n->face, x, base, n->s, n->n, c); break;
    case N_SPACE: break;
    case N_ROW: { int k; for (k = n->a; k >= 0; k = N[k].next) draw(k, x + N[k].ox, base - N[k].oy, c); break; }
    case N_FRAC: {
        float y = base - axis_of(px), pad = px * 0.06f;
        draw(n->a, x + N[n->a].ox, base - N[n->a].oy, c);
        draw(n->b, x + N[n->b].ox, base - N[n->b].oy, c);
        d_rect(rect(x + pad, y - n->sz / 2, n->w - px * 0.08f - 2 * pad, n->sz), c);
        break; }
    case N_SCRIPT:
        draw(n->a, x + N[n->a].ox, base, c);
        if (n->b >= 0) draw(n->b, x + N[n->b].ox, base - N[n->b].oy, c);
        if (n->c >= 0) draw(n->c, x + N[n->c].ox, base - N[n->c].oy, c);
        break;
    case N_SQRT: {
        float t = n->sz, top = base - n->asc + t / 2, bot = base + n->desc, bx = x + N[n->a].ox;
        float pts[10];
        pts[0] = x + px * 0.06f; pts[1] = base - px * 0.22f;
        pts[2] = x + px * 0.2f; pts[3] = base - px * 0.3f;
        pts[4] = x + px * 0.36f; pts[5] = bot;
        pts[6] = bx - px * 0.06f; pts[7] = top;
        pts[8] = x + n->w - px * 0.04f; pts[9] = top;
        d_polyline(pts, 2, t, c);
        d_line(pts[2], pts[3], pts[4], pts[5], t * 1.8f, c);
        d_polyline(pts + 4, 3, t, c);
        draw(n->a, bx, base, c);
        break; }
    case N_OVER: {
        Node *b = &N[n->a]; float t = px * 0.05f; if (t < 1) t = 1;
        draw(n->a, x + b->ox, base, c);
        d_rect(rect(x + b->ox + px * 0.02f, base - b->asc - px * 0.1f - t, b->w - px * 0.02f, t), c);
        break; }
    case N_HAT: {
        Node *b = &N[n->a]; float cx = x + b->ox + b->w / 2 + (b->type == N_GLYPH && b->italic ? px * 0.08f : 0), y = base - b->asc - px * 0.08f, t = px * 0.045f;
        if (t < 1) t = 1;
        draw(n->a, x + b->ox, base, c);
        d_line(cx - px * 0.2f, y, cx, y - px * 0.16f, t, c); d_line(cx, y - px * 0.16f, cx + px * 0.2f, y, t, c);
        break; }
    case N_COLOR: {
        Color cc = (n->col >= 0 && n->col < g_npal) ? g_pal[n->col] : c;
        if (n->flag) {
            Rect r = rect(x, base - n->asc - px * 0.1f, n->w, n->asc + n->desc + px * 0.2f);
            d_rrect(r, px * 0.18f, calpha(cc, 0.12f));
        }
        draw(n->a, x + N[n->a].ox, base, cc);
        break; }
    case N_UNDER: {
        Node *a = &N[n->a], *b = &N[n->b]; float br = n->sz, t = px * 0.04f, y;
        if (t < 1) t = 1;
        draw(n->a, x + a->ox, base, c);
        draw(n->b, x + b->ox, base - b->oy, calpha(c, 0.85f));
        if (!n->flag) {
            y = base + a->desc + px * 0.14f;
            d_line(x + a->ox, y, x + a->ox + a->w, y, t, calpha(c, 0.7f));
            d_line(x + a->ox, y, x + a->ox, y - br * 0.6f, t, calpha(c, 0.7f));
            d_line(x + a->ox + a->w, y, x + a->ox + a->w, y - br * 0.6f, t, calpha(c, 0.7f));
            d_line(x + a->ox + a->w / 2, y, x + a->ox + a->w / 2, y + br * 0.6f, t, calpha(c, 0.7f));
        } else {
            y = base - a->asc - px * 0.14f;
            d_line(x + a->ox, y, x + a->ox + a->w, y, t, calpha(c, 0.7f));
            d_line(x + a->ox, y, x + a->ox, y + br * 0.6f, t, calpha(c, 0.7f));
            d_line(x + a->ox + a->w, y, x + a->ox + a->w, y + br * 0.6f, t, calpha(c, 0.7f));
            d_line(x + a->ox + a->w / 2, y, x + a->ox + a->w / 2, y - br * 0.6f, t, calpha(c, 0.7f));
        }
        break; }
    case N_BIGOP: {
        float ox = n->flag == 3 ? (float)n->cp2 / 16 : 0;
        if (n->s) font_draw_base(n->face, x + ox, base, n->s, n->n, c);
        else font_glyph_draw(n->face, n->cp, x + ox, base + n->sz, n->gscale, c);
        if (n->b >= 0) draw(n->b, x + N[n->b].ox, base - N[n->b].oy, c);
        if (n->c >= 0) draw(n->c, x + N[n->c].ox, base - N[n->c].oy, c);
        break; }
    case N_DELIM: case N_BIGDELIM: {
        float wl, wr, hh = n->gscale;
        delim_size(px, hh, n->cp, &wl);
        if (n->type == N_DELIM) {
            delim_size(px, hh, n->cp2, &wr);
            draw_delim(n->cp, x, wl, base, px, hh, 0, c);
            draw(n->a, x + wl, base, c);
            draw_delim(n->cp2, x + n->w - wr, wr, base, px, hh, 1, c);
        } else draw_delim(n->cp, x, wl, base, px, hh, 0, c);
        break; }
    case N_PMAT: {
        float dw = px * 0.42f, hh = n->gscale;
        draw_delim('(', x, dw, base, px, hh, 0, c);
        draw(n->a, x + N[n->a].ox, base - N[n->a].oy, c);
        draw(n->b, x + N[n->b].ox, base - N[n->b].oy, c);
        draw_delim(')', x + n->w - dw, dw, base, px, hh, 1, c);
        break; }
    }
}

static int build(const char *src, float px) {
    int root;
    nn = 0; P = src ? src : "";
    g_dpi_px_min = 8.5f * (g_s > 1 ? g_s : 1);
    root = parse_row(0);
    layout(root, px, 0);
    return root;
}

TexBox tex_measure(const char *src, float px) {
    TexBox b; int r = build(src, px);
    b.w = N[r].w; b.asc = N[r].asc; b.desc = N[r].desc; return b;
}
TexBox tex_draw(const char *src, float x, float base, float px, Color c) {
    TexBox b; int r = build(src, px);
    draw(r, x, base, c);
    b.w = N[r].w; b.asc = N[r].asc; b.desc = N[r].desc; return b;
}
float tex_fit(const char *src, float px, float maxw) {
    TexBox b = tex_measure(src, px);
    if (b.w > maxw && b.w > 0) { px *= maxw / b.w; b = tex_measure(src, px); if (b.w > maxw) px *= maxw / b.w * 0.98f; }
    return px;
}

/* ----------------------------------------------------------- rich text */
typedef struct { const char *s; int n; int math, ital, col; float w, asc, desc; int glue; } Tok;

float rich(const char *s, float x0, float y0, float maxw, RichStyle st, Color c, int draw_it) {
    static Tok toks[600];
    int nt = 0, ital = 0, col = -1, i;
    const char *p = s;
    float y = y0, lh = font_line(st.face);
    float asc0 = font_ascent(st.face), desc0 = font_line(st.face) - font_ascent(st.face);
    int glue = 0;
    /* tokenise */
    while (*p && nt < 600) {
        if (*p == ' ') { glue = 0; p++; continue; }
        if (*p == '\n') { toks[nt].s = p; toks[nt].n = 0; toks[nt].math = 2; toks[nt].glue = 0; nt++; glue = 0; p++; continue; }
        if (*p == '*') { ital = !ital; p++; continue; }
        if (*p == '[' && p[1] >= '0' && p[1] <= '9' && p[2] == '|') { col = p[1] - '0'; p += 3; continue; }
        if (*p == ']' && col >= 0) { col = -1; p++; continue; }
        if (*p == '$') {
            const char *q = p + 1; while (*q && *q != '$') q++;
            toks[nt].s = p + 1; toks[nt].n = (int)(q - p - 1); toks[nt].math = 1; toks[nt].ital = 0; toks[nt].col = col; toks[nt].glue = glue;
            nt++; p = *q ? q + 1 : q; glue = 1; continue;
        }
        {   const char *q = p;
            while (*q && *q != ' ' && *q != '\n' && *q != '$' && *q != '*' && !(*q == ']' && col >= 0) && !(*q == '[' && q[1] >= '0' && q[1] <= '9' && q[2] == '|')) q++;
            toks[nt].s = p; toks[nt].n = (int)(q - p); toks[nt].math = 0; toks[nt].ital = ital; toks[nt].col = col; toks[nt].glue = glue;
            nt++; p = q; glue = 1; }
    }
    /* measure */
    for (i = 0; i < nt; i++) {
        Tok *t = &toks[i];
        if (t->math == 1) {
            char buf[512]; int n = t->n < 511 ? t->n : 511; TexBox b;
            memcpy(buf, t->s, (size_t)n); buf[n] = 0;
            b = tex_measure(buf, st.mathpx);
            t->w = b.w; t->asc = b.asc; t->desc = b.desc;
        } else if (t->math == 0) {
            t->w = font_width(t->ital ? st.iface : st.face, t->s, t->n); t->asc = asc0; t->desc = desc0;
        } else t->w = 0;
    }
    {   float spw = font_width(st.face, " ", 1);
        i = 0;
        while (i < nt) {
            int j = i, k; float w = 0, a = asc0, d = desc0;
            if (toks[i].math == 2) { y += lh * 0.55f; i++; continue; }
            /* take tokens while they fit; glued tokens move together */
            while (j < nt && toks[j].math != 2) {
                int e = j + 1; float gw = toks[j].w;
                while (e < nt && toks[e].glue && toks[e].math != 2) { gw += toks[e].w; e++; }
                if (j > i && w + spw + gw > maxw) break;
                w += (j > i ? spw : 0) + gw; j = e;
            }
            if (j == i) j = i + 1;
            for (k = i; k < j; k++) { if (toks[k].asc > a) a = toks[k].asc; if (toks[k].desc > d) d = toks[k].desc; }
            if (draw_it) {
                float xx = x0, base = y + a;
                for (k = i; k < j; k++) {
                    Tok *t = &toks[k];
                    Color cc = (t->col >= 0 && t->col < g_npal) ? g_pal[t->col] : c;
                    if (k > i && !t->glue) xx += spw;
                    if (t->math == 1) {
                        char buf[512]; int n = t->n < 511 ? t->n : 511;
                        memcpy(buf, t->s, (size_t)n); buf[n] = 0;
                        tex_draw(buf, xx, base, st.mathpx, cc);
                    } else font_draw_base(t->ital ? st.iface : st.face, xx, base, t->s, t->n, cc);
                    xx += t->w;
                }
            }
            y += a + d + st.line_gap + (lh - asc0 - desc0);
            i = j;
        }
    }
    return y - y0;
}
