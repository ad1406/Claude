#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static int   g_vsync, g_minw, g_minh, g_lastx, g_lasty;
static HCURSOR g_cur[CURSOR_COUNT];
static int   g_cursor;
static LARGE_INTEGER g_freq, g_t0;
static const char *CLASS_NAME = "DivergeE5Workshop";

static int map_vk(WPARAM vk) {
    switch (vk) {
    case VK_ESCAPE: return KEY_ESC;   case VK_RETURN: return KEY_ENTER;
    case VK_BACK: return KEY_BACKSPACE; case VK_DELETE: return KEY_DELETE;
    case VK_LEFT: return KEY_LEFT;    case VK_RIGHT: return KEY_RIGHT;
    case VK_UP: return KEY_UP;        case VK_DOWN: return KEY_DOWN;
    case VK_HOME: return KEY_HOME;    case VK_END: return KEY_END;
    case VK_TAB: return KEY_TAB;      case VK_SPACE: return KEY_SPACE;
    case 'B': return KEY_B;
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
    case WM_LBUTTONDOWN: mouse_btn(MOUSE_L, 1); return 0;
    case WM_LBUTTONDBLCLK: mouse_btn(MOUSE_L, 1); g_st.dbl = 1; return 0;
    case WM_LBUTTONUP: mouse_btn(MOUSE_L, 0); return 0;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: mouse_btn(MOUSE_R, 1); return 0;
    case WM_RBUTTONUP: mouse_btn(MOUSE_R, 0); return 0;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: mouse_btn(MOUSE_M, 1); return 0;
    case WM_MBUTTONUP: mouse_btn(MOUSE_M, 0); return 0;
    case WM_MOUSEWHEEL: g_st.wheel += (float)GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA; return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        int k = map_vk(w);
        if (k >= 0) { if (!g_st.key_down[k]) g_st.key_pressed[k] = 1; g_st.key_down[k] = 1; }
        if (k == KEY_BACKSPACE || k == KEY_DELETE || k == KEY_LEFT || k == KEY_RIGHT || k == KEY_HOME || k == KEY_END)
            if (l & (1 << 30)) g_st.key_pressed[k] = 1;   /* allow auto-repeat for editing keys */
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

/* A pixel format with multisampling needs wglChoosePixelFormatARB, which
   needs a live GL context. Create a throwaway window to get it. */
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

int plat_init(const char *title, int w, int h, int min_w, int min_h, int *msaa, int *stencil) {
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
    wc.style = CS_OWNDC | CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassA(&wc)) return 0;
    g_cur[CURSOR_ARROW] = LoadCursor(NULL, IDC_ARROW);
    g_cur[CURSOR_HAND] = LoadCursor(NULL, IDC_HAND);
    g_cur[CURSOR_MOVE] = LoadCursor(NULL, IDC_SIZEALL);
    g_cur[CURSOR_TEXT] = LoadCursor(NULL, IDC_IBEAM);

    choose = get_choose_pf(hi);

    r.left = 0; r.top = 0; r.right = (LONG)(w * g_st.dpi); r.bottom = (LONG)(h * g_st.dpi);
    {   /* keep the window inside the work area */
        RECT wa; SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
        if (r.right > (wa.right - wa.left) * 95 / 100) r.right = (wa.right - wa.left) * 95 / 100;
        if (r.bottom > (wa.bottom - wa.top) * 92 / 100) r.bottom = (wa.bottom - wa.top) * 92 / 100;
    }
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA(CLASS_NAME, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (!g_hwnd) return 0;
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
    if (!pf && !(pf = basic_format(g_hdc))) return 0;
    DescribePixelFormat(g_hdc, pf, sizeof pfd, &pfd);
    *stencil = pfd.cStencilBits >= 8;
    g_rc = wglCreateContext(g_hdc);
    if (!g_rc || !wglMakeCurrent(g_hdc, g_rc)) return 0;
    swapint = (SwapInt)(void *)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapint && swapint(1)) g_vsync = 1;

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
    g_st.dbl = 0; g_st.wheel = 0; g_st.ntext = 0; g_st.resized = 0;
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
void plat_cursor(int c) {
    if (c != g_cursor) { g_cursor = c; SetCursor(g_cur[c]); }
}
int plat_vsync(void) { return g_vsync; }
int plat_window_alive(void) { return g_hwnd && IsWindow(g_hwnd); }

int plat_font_path(int bold, char *out, int cap) {
    static const char *names[2][3] = { { "segoeui.ttf", "arial.ttf", "tahoma.ttf" },
                                       { "segoeuib.ttf", "arialbd.ttf", "tahomabd.ttf" } };
    char dir[MAX_PATH]; int i;
    UINT n = GetWindowsDirectoryA(dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return 0;
    for (i = 0; i < 3; i++) {
        _snprintf(out, (size_t)cap, "%s\\Fonts\\%s", dir, names[bold ? 1 : 0][i]);
        out[cap - 1] = 0;
        if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) return 1;
    }
    return 0;
}

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR c, int d) {
    (void)a; (void)b; (void)c; (void)d;
    return app_main(__argc, __argv);
}
#endif
