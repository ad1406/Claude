#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "gl_inc.h"
#include "platform.h"

#define WGL_DRAW_TO_WINDOW_ARB    0x2001
#define WGL_ACCELERATION_ARB      0x2003
#define WGL_SUPPORT_OPENGL_ARB    0x2010
#define WGL_DOUBLE_BUFFER_ARB     0x2011
#define WGL_PIXEL_TYPE_ARB        0x2013
#define WGL_COLOR_BITS_ARB        0x2014
#define WGL_DEPTH_BITS_ARB        0x2022
#define WGL_STENCIL_BITS_ARB      0x2023
#define WGL_FULL_ACCELERATION_ARB 0x2027
#define WGL_TYPE_RGBA_ARB         0x202B
#define WGL_SAMPLE_BUFFERS_ARB    0x2041
#define WGL_SAMPLES_ARB           0x2042
typedef BOOL (WINAPI *ChoosePF)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL (WINAPI *SwapInt)(int);

static HWND  g_hwnd;
static HDC   g_hdc;
static HGLRC g_rc;
static Input g_st;
static int   g_minw, g_minh, g_lastx, g_lasty;
static HCURSOR g_cur[CURSOR_COUNT];
static int   g_cursor;
static LARGE_INTEGER g_freq, g_t0;
static const char *CLASS_NAME = "LeitmotifScore";

static int map_vk(WPARAM vk) {
    switch (vk) {
    case VK_ESCAPE: return KEY_ESC;   case VK_RETURN: return KEY_ENTER;
    case VK_BACK: return KEY_BACKSPACE; case VK_DELETE: return KEY_DELETE;
    case VK_LEFT: return KEY_LEFT;    case VK_RIGHT: return KEY_RIGHT;
    case VK_UP: return KEY_UP;        case VK_DOWN: return KEY_DOWN;
    case VK_HOME: return KEY_HOME;    case VK_END: return KEY_END;
    case VK_TAB: return KEY_TAB;      case VK_SPACE: return KEY_SPACE;
    case VK_PRIOR: return KEY_PGUP;   case VK_NEXT: return KEY_PGDN;
    }
    return -1;
}

static void mouse_btn(int b, int isdown) {
    if (isdown) { g_st.down[b] = 1; g_st.pressed[b] = 1; SetCapture(g_hwnd); }
    else { g_st.down[b] = 0; g_st.released[b] = 1; if (!g_st.down[0] && !g_st.down[1] && !g_st.down[2]) ReleaseCapture(); }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CLOSE: g_st.quit = 1; return 0;
    case WM_SIZE:
        g_st.win_w = LOWORD(l); g_st.win_h = HIWORD(l); g_st.resized = 1; return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l; RECT r = {0, 0, 0, 0};
        r.right = (LONG)(g_minw * (g_st.dpi > 0 ? g_st.dpi : 1)); r.bottom = (LONG)(g_minh * (g_st.dpi > 0 ? g_st.dpi : 1));
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        mm->ptMinTrackSize.x = r.right - r.left; mm->ptMinTrackSize.y = r.bottom - r.top; return 0; }
    case 0x02E0: { /* WM_DPICHANGED */
        RECT *r = (RECT *)l; g_st.dpi = HIWORD(w) / 96.0f;
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        g_st.resized = 1; return 0; }
    case WM_MOUSEMOVE: g_st.mx = GET_X_LPARAM(l); g_st.my = GET_Y_LPARAM(l); return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: mouse_btn(MOUSE_L, 1); return 0;
    case WM_LBUTTONUP: mouse_btn(MOUSE_L, 0); return 0;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: mouse_btn(MOUSE_R, 1); return 0;
    case WM_RBUTTONUP: mouse_btn(MOUSE_R, 0); return 0;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: mouse_btn(MOUSE_M, 1); return 0;
    case WM_MBUTTONUP: mouse_btn(MOUSE_M, 0); return 0;
    case WM_MOUSEWHEEL: g_st.wheel += (float)GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA; return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        int k = map_vk(w);
        if (k >= 0) { if (!g_st.key_down[k] || (l & (1 << 30))) g_st.key_pressed[k] = 1; g_st.key_down[k] = 1; }
        break; }
    case WM_KEYUP: case WM_SYSKEYUP: {
        int k = map_vk(w); if (k >= 0) g_st.key_down[k] = 0; break; }
    case WM_CHAR:
        if (w >= 32 && w != 127 && (w < 0xD800 || w > 0xDFFF) && g_st.ntext < 32) g_st.text[g_st.ntext++] = (unsigned)w;
        return 0;
    case WM_SETFOCUS: g_st.focused = 1; return 0;
    case WM_KILLFOCUS:
        g_st.focused = 0; memset(g_st.down, 0, sizeof g_st.down); memset(g_st.key_down, 0, sizeof g_st.key_down); return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) { SetCursor(g_cur[g_cursor]); return TRUE; }
        break;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcA(h, m, w, l);
}

static int basic_format(HDC dc) {
    PIXELFORMATDESCRIPTOR pfd; int pf;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
    pf = ChoosePixelFormat(dc, &pfd);
    if (!pf || !SetPixelFormat(dc, pf, &pfd)) return 0;
    return pf;
}

/* Multisampling needs wglChoosePixelFormatARB, which needs a live context:
   make a throwaway window to fetch it. */
static ChoosePF get_choose_pf(HINSTANCE hi) {
    ChoosePF fn = NULL;
    HWND dw = CreateWindowA(CLASS_NAME, "", WS_OVERLAPPEDWINDOW, 0, 0, 8, 8, NULL, NULL, hi, NULL);
    HDC dc; HGLRC rc;
    if (!dw) return NULL;
    dc = GetDC(dw);
    if (basic_format(dc) && (rc = wglCreateContext(dc)) != NULL) {
        wglMakeCurrent(dc, rc);
        fn = (ChoosePF)(void *)wglGetProcAddress("wglChoosePixelFormatARB");
        wglMakeCurrent(NULL, NULL); wglDeleteContext(rc);
    }
    ReleaseDC(dw, dc); DestroyWindow(dw);
    return fn;
}

int plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa) {
    HINSTANCE hi = GetModuleHandleA(NULL);
    WNDCLASSA wc; RECT r; ChoosePF choose; SwapInt swapint; HDC sdc;
    int pf = 0; UINT n = 0; PIXELFORMATDESCRIPTOR pfd;
    BOOL (WINAPI *dpiaware)(void);

    dpiaware = (BOOL (WINAPI *)(void))(void *)GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDPIAware");
    if (dpiaware) dpiaware();
    sdc = GetDC(NULL); g_st.dpi = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f; ReleaseDC(NULL, sdc);
    if (g_st.dpi < 1.0f) g_st.dpi = 1.0f;
    g_minw = min_w; g_minh = min_h;
    QueryPerformanceFrequency(&g_freq); QueryPerformanceCounter(&g_t0);

    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassA(&wc)) { plat_log("RegisterClass failed (%lu)", (unsigned long)GetLastError()); return 0; }
    g_cur[CURSOR_ARROW] = LoadCursor(NULL, IDC_ARROW);
    g_cur[CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
    g_cur[CURSOR_MOVE] = LoadCursor(NULL, IDC_SIZEALL);

    choose = get_choose_pf(hi);
    plat_log("dpi scale %.2f; wglChoosePixelFormatARB %s", g_st.dpi, choose ? "available" : "missing");

    r.left = 0; r.top = 0; r.right = (LONG)(w * g_st.dpi); r.bottom = (LONG)(h * g_st.dpi);
    {   RECT wa; SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
        if (r.right > (wa.right - wa.left) * 95 / 100) r.right = (wa.right - wa.left) * 95 / 100;
        if (r.bottom > (wa.bottom - wa.top) * 92 / 100) r.bottom = (wa.bottom - wa.top) * 92 / 100;
    }
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA(CLASS_NAME, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (!g_hwnd) { plat_log("CreateWindow failed (%lu)", (unsigned long)GetLastError()); return 0; }
    {   /* the title is UTF-8; give Windows the wide-character version */
        WCHAR wt[256];
        if (MultiByteToWideChar(CP_UTF8, 0, title, -1, wt, 256) > 0) SetWindowTextW(g_hwnd, wt); }
    g_hdc = GetDC(g_hwnd);
    *msaa = 0;
    if (choose) {
        int samples;
        for (samples = 8; samples >= 2 && !pf; samples /= 2) {
            int attr[] = { WGL_DRAW_TO_WINDOW_ARB, 1, WGL_SUPPORT_OPENGL_ARB, 1, WGL_DOUBLE_BUFFER_ARB, 1,
                           WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB, WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                           WGL_COLOR_BITS_ARB, 32, WGL_DEPTH_BITS_ARB, 24, WGL_STENCIL_BITS_ARB, 8,
                           WGL_SAMPLE_BUFFERS_ARB, 1, WGL_SAMPLES_ARB, 0, 0 };
            attr[19] = samples;
            if (choose(g_hdc, attr, NULL, 1, &pf, &n) && n > 0) {
                DescribePixelFormat(g_hdc, pf, sizeof pfd, &pfd);
                if (SetPixelFormat(g_hdc, pf, &pfd)) *msaa = samples; else pf = 0;
            } else pf = 0;
        }
    }
    if (!pf && !(pf = basic_format(g_hdc))) { plat_log("no usable pixel format (%lu)", (unsigned long)GetLastError()); return 0; }
    plat_log("pixel format %d, msaa %d", pf, *msaa);
    g_rc = wglCreateContext(g_hdc);
    if (!g_rc || !wglMakeCurrent(g_hdc, g_rc)) { plat_log("OpenGL context failed (%lu)", (unsigned long)GetLastError()); return 0; }
    swapint = (SwapInt)(void *)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapint) swapint(1);

    ShowWindow(g_hwnd, SW_SHOWNORMAL);
    UpdateWindow(g_hwnd);
    GetClientRect(g_hwnd, &r);
    g_st.win_w = r.right; g_st.win_h = r.bottom; g_st.focused = 1;
    return 1;
}

void plat_shutdown(void) {
    if (g_rc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_rc); g_rc = NULL; }
    if (g_hdc) { ReleaseDC(g_hwnd, g_hdc); g_hdc = NULL; }
    if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; }
    UnregisterClassA(CLASS_NAME, GetModuleHandleA(NULL));
}

void plat_poll(Input *in) {
    MSG msg;
    memset(g_st.pressed, 0, sizeof g_st.pressed); memset(g_st.released, 0, sizeof g_st.released);
    memset(g_st.key_pressed, 0, sizeof g_st.key_pressed);
    g_st.wheel = 0; g_st.ntext = 0; g_st.resized = 0;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) g_st.quit = 1;
        TranslateMessage(&msg); DispatchMessageA(&msg);
    }
    g_st.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    g_st.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    g_st.mdx = g_st.mx - g_lastx; g_st.mdy = g_st.my - g_lasty;
    g_lastx = g_st.mx; g_lasty = g_st.my;
    *in = g_st;
}

void plat_swap(void) { SwapBuffers(g_hdc); }
double plat_time(void) {
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)(t.QuadPart - g_t0.QuadPart) / (double)g_freq.QuadPart;
}
void plat_sleep(double s) { if (s > 0) Sleep((DWORD)(s * 1000.0)); }
void plat_cursor(int c) { if (c != g_cursor) { g_cursor = c; SetCursor(g_cur[c]); } }

int plat_font_dir(char *out, int cap) {
    char dir[MAX_PATH]; UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { out[0] = 0; return 0; }
    _snprintf(out, (size_t)cap, "%s\\Fonts\\", dir); out[cap - 1] = 0;
    return 1;
}

/* ------------------------------------------------------------------ audio */
#define NBUF 6
#define BUF_FRAMES 1024
static HWAVEOUT g_wo;
static WAVEHDR g_hdr[NBUF];
static short g_pcm[NBUF][BUF_FRAMES];
static float g_mix[BUF_FRAMES];
static int g_rate, g_queued[NBUF];

int plat_audio_open(int rate) {
    WAVEFORMATEX f; int i;
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = (DWORD)rate;
    f.wBitsPerSample = 16; f.nBlockAlign = 2; f.nAvgBytesPerSec = (DWORD)rate * 2;
    {   MMRESULT mr = waveOutOpen(&g_wo, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL);
        if (mr != MMSYSERR_NOERROR) { plat_log("waveOutOpen failed (%u): no sound", (unsigned)mr); g_wo = NULL; return 0; } }
    plat_log("audio open, %d Hz", rate);
    g_rate = rate;
    for (i = 0; i < NBUF; i++) {
        memset(&g_hdr[i], 0, sizeof g_hdr[i]);
        g_hdr[i].lpData = (LPSTR)g_pcm[i]; g_hdr[i].dwBufferLength = BUF_FRAMES * 2;
        waveOutPrepareHeader(g_wo, &g_hdr[i], sizeof g_hdr[i]);
        g_queued[i] = 0;
    }
    return 1;
}

void plat_audio_pump(AudioFill fill) {
    int i, k;
    if (!g_wo) return;
    for (i = 0; i < NBUF; i++) {
        if (g_queued[i] && !(g_hdr[i].dwFlags & WHDR_DONE)) continue;
        fill(g_mix, BUF_FRAMES, g_rate);
        for (k = 0; k < BUF_FRAMES; k++) {
            float v = g_mix[k]; if (v > 1) v = 1; if (v < -1) v = -1;
            g_pcm[i][k] = (short)(v * 32000.0f);
        }
        g_hdr[i].dwFlags &= ~WHDR_DONE;
        waveOutWrite(g_wo, &g_hdr[i], sizeof g_hdr[i]);
        g_queued[i] = 1;
    }
}

void plat_audio_close(void) {
    int i;
    if (!g_wo) return;
    waveOutReset(g_wo);
    for (i = 0; i < NBUF; i++) waveOutUnprepareHeader(g_wo, &g_hdr[i], sizeof g_hdr[i]);
    waveOutClose(g_wo); g_wo = NULL;
}

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#endif

/* ------------------------------------------------------------ diagnostics */
static char g_logpath[MAX_PATH + 32];
static FILE *g_log;
const char *plat_log_path(void) { return g_logpath; }
void plat_log(const char *fmt, ...) {
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}
void plat_fatal(const char *msg) {
    char buf[1024];
    plat_log("FATAL: %s", msg);
    _snprintf(buf, sizeof buf, "%s\n\nA log was written to:\n%s", msg, g_logpath); buf[sizeof buf - 1] = 0;
    MessageBoxA(g_hwnd, buf, "Leitmotif", MB_OK | MB_ICONERROR);
}
static LONG WINAPI on_crash(EXCEPTION_POINTERS *ep) {
    char buf[512];
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    void *addr = ep->ExceptionRecord->ExceptionAddress;
    size_t base = (size_t)GetModuleHandleA(NULL);
    plat_log("CRASH: exception 0x%08lx at %p (exe offset 0x%llx)", (unsigned long)code, addr, (unsigned long long)((size_t)addr - base));
    _snprintf(buf, sizeof buf, "Leitmotif crashed (exception 0x%08lx at offset 0x%llx).\n\nPlease send this file:\n%s",
              (unsigned long)code, (unsigned long long)((size_t)addr - base), g_logpath); buf[sizeof buf - 1] = 0;
    MessageBoxA(NULL, buf, "Leitmotif", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d) {
    DWORD n;
    (void)a; (void)b; (void)c; (void)d;
    n = GetTempPathA(MAX_PATH, g_logpath);
    if (!n || n >= MAX_PATH) g_logpath[0] = 0;
    strcat(g_logpath, "leitmotif-log.txt");
    g_log = fopen(g_logpath, "w");
    SetUnhandledExceptionFilter(on_crash);
    {   OSVERSIONINFOA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; GetVersionExA(&v);
        plat_log("Leitmotif starting; Windows %lu.%lu build %lu", (unsigned long)v.dwMajorVersion, (unsigned long)v.dwMinorVersion, (unsigned long)v.dwBuildNumber); }
    {   int rc = app_main(__argc, __argv); plat_log("exit code %d", rc); if (g_log) fclose(g_log); return rc; }
}
#endif
