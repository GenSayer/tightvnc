//  Win32sApi.cpp - runtime resolution of APIs that do not exist on Win32s
//
//  See Win32sApi.h for the full rationale.  Short version: a Win32 EXE cannot
//  start if any imported name is missing from the target's DLLs, so every
//  Win95-and-later API used by the viewer is fetched here with
//  GetProcAddress() and given a Windows 3.1-compatible fallback.
//
//  Rules for this file:
//    * No static/global C++ objects with constructors - nothing must run
//      before WinMain on Win32s.
//    * Plain function pointers initialised in Win32sApiInit(), plus lazy
//      re-checks so a missed Init call cannot dereference NULL.
//    * Every wrapper must be safe to call before Win32sApiInit().

#include "stdhdrs.h"
#include "Win32sApi.h"
#include "vc2ctl.h"		// VC2: built-in common controls

#include <string.h>		// memset/strcpy used below

// CLR_INVALID is not present in every SDK of this era.
#ifndef CLR_INVALID
#define CLR_INVALID 0xFFFFFFFFL
#endif

// SIMPLEREGION is in wingdi.h; guard anyway since it is only a return value.
#ifndef SIMPLEREGION
#define SIMPLEREGION 2
#endif

// SetErrorMode flags.  Present in every Win32 SDK, but guarded so this file
// does not depend on the header version.
#ifndef SEM_FAILCRITICALERRORS
#define SEM_FAILCRITICALERRORS  0x0001
#endif
#ifndef SEM_NOOPENFILEERRORBOX
#define SEM_NOOPENFILEERRORBOX  0x8000
#endif

// ---------------------------------------------------------------------------
// Function pointer types
// ---------------------------------------------------------------------------
typedef HBRUSH (WINAPI *PFNGETSYSCOLORBRUSH)(int);
typedef int    (WINAPI *PFNSETSCROLLINFO)(HWND, int, LPSCROLLINFO, BOOL);
typedef BOOL   (WINAPI *PFNSETMENUDEFAULTITEM)(HMENU, UINT, UINT);
typedef HKL    (WINAPI *PFNLOADKEYBOARDLAYOUT)(LPCSTR, UINT);
typedef BOOL   (WINAPI *PFNGETKEYBOARDLAYOUTNAME)(LPSTR);
typedef BOOL   (WINAPI *PFNSYSTEMPARAMETERSINFO)(UINT, UINT, PVOID, UINT);
typedef int    (WINAPI *PFNSCROLLWINDOWEX)(HWND, int, int, CONST RECT *,
                                           CONST RECT *, HRGN, LPRECT, UINT);
typedef BOOL   (WINAPI *PFNSETFOREGROUNDWINDOW)(HWND);
typedef HWND   (WINAPI *PFNGETFOREGROUNDWINDOW)(void);
typedef BOOL   (WINAPI *PFNSETPIXELV)(HDC, int, int, COLORREF);
typedef int    (WINAPI *PFNSETWINDOWRGN)(HWND, HRGN, BOOL);
typedef int    (WINAPI *PFNGETWINDOWRGN)(HWND, HRGN);
typedef BOOL   (WINAPI *PFNSETBRUSHORGEX)(HDC, int, int, LPPOINT);
typedef BOOL   (WINAPI *PFNSHELLNOTIFYICON)(DWORD, PNOTIFYICONDATA);
typedef void   (WINAPI *PFNINITCOMMONCONTROLS)(void);
typedef HWND   (WINAPI *PFNCREATETOOLBAREX)(HWND, DWORD, UINT, int, HINSTANCE,
                                            UINT, void *, int, int, int, int,
                                            int, UINT);
typedef HANDLE (WINAPI *PFNLOADIMAGE)(HINSTANCE, LPCSTR, UINT, int, int, UINT);
typedef HBITMAP (WINAPI *PFNCREATEDIBSECTION)(HDC, CONST BITMAPINFO *, UINT,
                                              VOID **, HANDLE, DWORD);
typedef BOOL   (WINAPI *PFNDRAWICONEX)(HDC, int, int, HICON, int, int, UINT,
                                       HBRUSH, UINT);
typedef BOOL   (WINAPI *PFNGETICONINFO)(HICON, PICONINFO);
typedef VOID   (WINAPI *PFNKEYBDEVENT)(BYTE, BYTE, DWORD, DWORD);
typedef VOID   (WINAPI *PFNMOUSEEVENT)(DWORD, DWORD, DWORD, DWORD, DWORD);

#ifndef IMAGE_ICON
#define IMAGE_ICON 1
#endif
#ifndef LR_DEFAULTCOLOR
#define LR_DEFAULTCOLOR 0x0000
#endif

// ---------------------------------------------------------------------------
// Resolved pointers (all NULL until Win32sApiInit runs)
// ---------------------------------------------------------------------------
static int   g_inited          = 0;
static int   g_isWin32s        = 0;

static HINSTANCE g_hUser       = NULL;
static HINSTANCE g_hGdi        = NULL;
static HINSTANCE g_hShell      = NULL;
static HINSTANCE g_hComctl     = NULL;

static PFNGETSYSCOLORBRUSH      g_pGetSysColorBrush      = NULL;
static PFNSETSCROLLINFO         g_pSetScrollInfo         = NULL;
static PFNSETMENUDEFAULTITEM    g_pSetMenuDefaultItem    = NULL;
static PFNLOADKEYBOARDLAYOUT    g_pLoadKeyboardLayout    = NULL;
static PFNGETKEYBOARDLAYOUTNAME g_pGetKeyboardLayoutName = NULL;
static PFNSYSTEMPARAMETERSINFO  g_pSystemParametersInfo  = NULL;
static PFNSCROLLWINDOWEX        g_pScrollWindowEx        = NULL;
static PFNSETFOREGROUNDWINDOW   g_pSetForegroundWindow   = NULL;
static PFNGETFOREGROUNDWINDOW   g_pGetForegroundWindow   = NULL;
static PFNSETPIXELV             g_pSetPixelV             = NULL;
static PFNSETWINDOWRGN          g_pSetWindowRgn          = NULL;
static PFNGETWINDOWRGN          g_pGetWindowRgn          = NULL;
static PFNSETBRUSHORGEX         g_pSetBrushOrgEx         = NULL;
static PFNSHELLNOTIFYICON       g_pShellNotifyIcon       = NULL;
static PFNINITCOMMONCONTROLS    g_pInitCommonControls    = NULL;
static PFNCREATETOOLBAREX       g_pCreateToolbarEx       = NULL;
static PFNLOADIMAGE             g_pLoadImage             = NULL;
static PFNCREATEDIBSECTION      g_pCreateDIBSection      = NULL;
static PFNDRAWICONEX            g_pDrawIconEx            = NULL;
static PFNGETICONINFO           g_pGetIconInfo           = NULL;
static PFNKEYBDEVENT            g_pKeybdEvent            = NULL;
static PFNMOUSEEVENT            g_pMouseEvent            = NULL;

// Cached fallback brushes for Win32sGetSysColorBrush.  COLOR_* indices used by
// the viewer are small; 32 covers every documented 3.1/95 index.
#define MAX_SYSCOLOR_BRUSHES 32
static HBRUSH g_sysColorBrush[MAX_SYSCOLOR_BRUSHES];

// ---------------------------------------------------------------------------

void Win32sApiInit(void)
{
    if (g_inited)
        return;
    g_inited = 1;

    int i;
    for (i = 0; i < MAX_SYSCOLOR_BRUSHES; i++)
        g_sysColorBrush[i] = NULL;

    // Platform detection.
    //
    // GetVersion() is used rather than GetVersionEx() on purpose: GetVersionEx
    // is a Windows NT 3.5/95-era addition and, while Win32s 1.30 does export it,
    // it is exactly the kind of call that must not be a load-time import on a
    // machine with an older Win32s.  GetVersion() has existed since the first
    // Win32 and is documented to set the high bit of its return value when
    // Windows is *not* NT - and on Win32s the low word is the Windows 3.1x
    // version (3.10/3.11) rather than 4.0+.
    //
    //   bit 31 set  -> Win32s or Win9x
    //   major < 4    -> Windows 3.1x, i.e. Win32s
    DWORD v = GetVersion();
    if ((v & 0x80000000) != 0) {
        DWORD major = (DWORD)(LOBYTE(LOWORD(v)));
        g_isWin32s = (major < 4) ? 1 : 0;    // Win32s vs Win95/98/ME
    } else {
        g_isWin32s = 0;                      // Windows NT family
    }

    // GetModuleHandle first, LoadLibrary as a fallback.
    //
    // USER32/GDI32/KERNEL32 are always mapped because we link against them, so
    // GetModuleHandle succeeds and costs nothing.  Note the module names have no
    // ".DLL": GetModuleHandle matches the loaded module's base name, and both
    // forms work, but the plain name is what the loader records.
    g_hUser  = GetModuleHandle("USER32");
    if (g_hUser == NULL)
        g_hUser = GetModuleHandle("USER32.DLL");

    g_hGdi   = GetModuleHandle("GDI32");
    if (g_hGdi == NULL)
        g_hGdi = GetModuleHandle("GDI32.DLL");

    // SHELL32 and COMCTL32 are NOT linked any more (see the makefile), so they
    // may not be mapped - and on a bare Windows 3.1 machine they may not exist
    // at all.  LoadLibrary is correct here, and a failure is expected rather
    // than exceptional.  We deliberately never FreeLibrary these: the process
    // uses them for its whole lifetime, and releasing them at exit buys nothing.
    // Suppress the system's "Cannot find SHELL32.DLL / COMCTL32.DLL" message
    // box while we probe.  Windows 3.1 shows a modal error box for a failed
    // LoadLibrary unless SEM_NOOPENFILEERRORBOX is set - which would confront
    // the user with an alarming dialog during startup for a DLL we are perfectly
    // happy to do without.
    UINT savedErrorMode = SetErrorMode(SEM_NOOPENFILEERRORBOX |
                                       SEM_FAILCRITICALERRORS);

    g_hShell = GetModuleHandle("SHELL32");
    if (g_hShell == NULL)
        g_hShell = LoadLibrary("SHELL32.DLL");

    if (g_hUser != NULL) {
        g_pGetSysColorBrush =
            (PFNGETSYSCOLORBRUSH)GetProcAddress(g_hUser, "GetSysColorBrush");
        g_pSetScrollInfo =
            (PFNSETSCROLLINFO)GetProcAddress(g_hUser, "SetScrollInfo");
        g_pSetMenuDefaultItem =
            (PFNSETMENUDEFAULTITEM)GetProcAddress(g_hUser, "SetMenuDefaultItem");
        g_pLoadKeyboardLayout =
            (PFNLOADKEYBOARDLAYOUT)GetProcAddress(g_hUser, "LoadKeyboardLayoutA");
        g_pGetKeyboardLayoutName =
            (PFNGETKEYBOARDLAYOUTNAME)GetProcAddress(g_hUser, "GetKeyboardLayoutNameA");
        g_pSystemParametersInfo =
            (PFNSYSTEMPARAMETERSINFO)GetProcAddress(g_hUser, "SystemParametersInfoA");
        g_pScrollWindowEx =
            (PFNSCROLLWINDOWEX)GetProcAddress(g_hUser, "ScrollWindowEx");
        g_pSetForegroundWindow =
            (PFNSETFOREGROUNDWINDOW)GetProcAddress(g_hUser, "SetForegroundWindow");
        g_pGetForegroundWindow =
            (PFNGETFOREGROUNDWINDOW)GetProcAddress(g_hUser, "GetForegroundWindow");
        g_pLoadImage =
            (PFNLOADIMAGE)GetProcAddress(g_hUser, "LoadImageA");
        g_pSetWindowRgn =
            (PFNSETWINDOWRGN)GetProcAddress(g_hUser, "SetWindowRgn");
        g_pGetWindowRgn =
            (PFNGETWINDOWRGN)GetProcAddress(g_hUser, "GetWindowRgn");
        g_pDrawIconEx =
            (PFNDRAWICONEX)GetProcAddress(g_hUser, "DrawIconEx");
        g_pGetIconInfo =
            (PFNGETICONINFO)GetProcAddress(g_hUser, "GetIconInfo");
        g_pKeybdEvent =
            (PFNKEYBDEVENT)GetProcAddress(g_hUser, "keybd_event");
        g_pMouseEvent =
            (PFNMOUSEEVENT)GetProcAddress(g_hUser, "mouse_event");
    }

    if (g_hGdi != NULL) {
        g_pSetPixelV =
            (PFNSETPIXELV)GetProcAddress(g_hGdi, "SetPixelV");
        g_pSetBrushOrgEx =
            (PFNSETBRUSHORGEX)GetProcAddress(g_hGdi, "SetBrushOrgEx");
        g_pCreateDIBSection =
            (PFNCREATEDIBSECTION)GetProcAddress(g_hGdi, "CreateDIBSection");
    }

    if (g_hShell != NULL) {
        g_pShellNotifyIcon =
            (PFNSHELLNOTIFYICON)GetProcAddress(g_hShell, "Shell_NotifyIconA");
    }

    // COMCTL32 may be absent on a bare Windows 3.1 + Win32s machine.
    g_hComctl = GetModuleHandle("COMCTL32");
    if (g_hComctl == NULL)
        g_hComctl = LoadLibrary("COMCTL32.DLL");
    if (g_hComctl != NULL) {
        g_pInitCommonControls =
            (PFNINITCOMMONCONTROLS)GetProcAddress(g_hComctl, "InitCommonControls");
        g_pCreateToolbarEx =
            (PFNCREATETOOLBAREX)GetProcAddress(g_hComctl, "CreateToolbarEx");
    }

    SetErrorMode(savedErrorMode);
}

bool Win32sIsWin32s(void)
{
    if (!g_inited)
        Win32sApiInit();
    return g_isWin32s ? true : false;
}

// ---------------------------------------------------------------------------
// USER32 wrappers
// ---------------------------------------------------------------------------

HBRUSH Win32sGetSysColorBrush(int nIndex)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pGetSysColorBrush != NULL)
        return g_pGetSysColorBrush(nIndex);

    // Fallback: make our own and cache it.  Window classes hold on to their
    // background brush for the life of the class, so this must not be deleted.
    if (nIndex >= 0 && nIndex < MAX_SYSCOLOR_BRUSHES) {
        if (g_sysColorBrush[nIndex] == NULL)
            g_sysColorBrush[nIndex] = CreateSolidBrush(GetSysColor(nIndex));
        return g_sysColorBrush[nIndex];
    }

    return (HBRUSH)GetStockObject(LTGRAY_BRUSH);
}

int Win32sSetScrollInfo(HWND hwnd, int fnBar, LPSCROLLINFO lpsi, BOOL fRedraw)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pSetScrollInfo != NULL)
        return g_pSetScrollInfo(hwnd, fnBar, lpsi, fRedraw);

    // Windows 3.1 fallback.  No proportional thumb, so SIF_PAGE is dropped;
    // we clamp the range so the last page is reachable, which is what the
    // page size would otherwise have achieved.
    if (lpsi == NULL)
        return 0;

    if (lpsi->fMask & SIF_RANGE) {
        int nMax = lpsi->nMax;
        if (lpsi->fMask & SIF_PAGE) {
            nMax = lpsi->nMax - (int)lpsi->nPage;
            if (nMax < lpsi->nMin)
                nMax = lpsi->nMin;
        }
        SetScrollRange(hwnd, fnBar, lpsi->nMin, nMax, fRedraw);
    }
    if (lpsi->fMask & SIF_POS) {
        return SetScrollPos(hwnd, fnBar, lpsi->nPos, fRedraw);
    }
    return 0;
}

BOOL Win32sSetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pSetMenuDefaultItem != NULL)
        return g_pSetMenuDefaultItem(hMenu, uItem, fByPos);

    // No bold default item on 3.1.  Purely cosmetic; report success.
    return TRUE;
}

HKL Win32sLoadKeyboardLayout(LPCSTR pwszKLID, UINT Flags)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pLoadKeyboardLayout != NULL)
        return g_pLoadKeyboardLayout(pwszKLID, Flags);

    return NULL;
}

BOOL Win32sGetKeyboardLayoutName(LPSTR pwszKLID)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pGetKeyboardLayoutName != NULL)
        return g_pGetKeyboardLayoutName(pwszKLID);

    if (pwszKLID != NULL)
        strcpy(pwszKLID, "(n/a)");
    return FALSE;
}

void Win32sGetWorkArea(RECT *pWorkArea)
{
    if (!g_inited)
        Win32sApiInit();

    if (pWorkArea == NULL)
        return;

    // Always initialise first, so a failing/absent API cannot leave the caller
    // using an uninitialised RECT.  The original code passed an uninitialised
    // "RECT workrect" straight into SystemParametersInfo and then used it -
    // if the call failed, the window was sized and positioned from stack junk.
    pWorkArea->left   = 0;
    pWorkArea->top    = 0;
    pWorkArea->right  = GetSystemMetrics(SM_CXSCREEN);
    pWorkArea->bottom = GetSystemMetrics(SM_CYSCREEN);

    if (g_isWin32s)
        return;						// no taskbar; screen == work area

    if (g_pSystemParametersInfo != NULL) {
        RECT r;
        if (g_pSystemParametersInfo(SPI_GETWORKAREA, 0, &r, 0)) {
            if (r.right > r.left && r.bottom > r.top)
                *pWorkArea = r;
        }
    }
}

BOOL Win32sGetWindowPlacement(HWND hwnd, WINDOWPLACEMENT *pwp)
{
    if (pwp == NULL)
        return FALSE;
    pwp->length = sizeof(WINDOWPLACEMENT);
    return GetWindowPlacement(hwnd, pwp);
}

BOOL Win32sSetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *pwp)
{
    if (pwp == NULL)
        return FALSE;
    return SetWindowPlacement(hwnd, pwp);
}

int Win32sScrollWindowEx(HWND hwnd, int dx, int dy, CONST RECT *prcScroll,
                         CONST RECT *prcClip, HRGN hrgnUpdate,
                         LPRECT prcUpdate, UINT flags)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pScrollWindowEx != NULL)
        return g_pScrollWindowEx(hwnd, dx, dy, prcScroll, prcClip,
                                 hrgnUpdate, prcUpdate, flags);

    // Windows 3.1: ScrollWindow has no update region/rect outputs and always
    // invalidates.  The viewer only ever asked for SW_INVALIDATE, so this is
    // behaviourally equivalent.
    ScrollWindow(hwnd, dx, dy, prcScroll, prcClip);
    return SIMPLEREGION;
}

BOOL Win32sSetForegroundWindow(HWND hwnd)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pSetForegroundWindow != NULL)
        return g_pSetForegroundWindow(hwnd);

    if (hwnd == NULL)
        return FALSE;
    BringWindowToTop(hwnd);
    SetActiveWindow(hwnd);
    return TRUE;
}

HWND Win32sGetForegroundWindow(void)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pGetForegroundWindow != NULL)
        return g_pGetForegroundWindow();

    return GetActiveWindow();
}

BOOL Win32sSetWindowRgn(HWND hwnd, HRGN hrgn, BOOL bRedraw)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pSetWindowRgn != NULL) {
        // On success the SYSTEM owns hrgn and the caller must not free it.
        return (g_pSetWindowRgn(hwnd, hrgn, bRedraw) != 0);
    }

    // Not available: the window keeps its rectangular shape.  The caller still
    // owns the region and must delete it.
    return FALSE;
}

int Win32sGetWindowRgn(HWND hwnd, HRGN hrgn)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pGetWindowRgn != NULL)
        return g_pGetWindowRgn(hwnd, hrgn);

    // Not available: report "no window region" - the same answer the real API
    // gives for an ordinary rectangular window.
    return ERROR;
}

// ---------------------------------------------------------------------------
// GDI32 wrappers
// ---------------------------------------------------------------------------

BOOL Win32sSetPixelV(HDC hdc, int x, int y, COLORREF color)
{
    // No lazy init test on this one: it is called per pixel by the Raw
    // decoder.  g_pSetPixelV is NULL until Init runs, which just means the
    // slower SetPixel path is used for those first pixels.
    if (g_pSetPixelV != NULL)
        return g_pSetPixelV(hdc, x, y, color);

    return (SetPixel(hdc, x, y, color) != CLR_INVALID);
}

BOOL Win32sSetBrushOrgEx(HDC hdc, int x, int y, LPPOINT ppt)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pSetBrushOrgEx != NULL)
        return g_pSetBrushOrgEx(hdc, x, y, ppt);

    return FALSE;
}

// ---------------------------------------------------------------------------
// SHELL32 wrapper
// ---------------------------------------------------------------------------

BOOL Win32sShellNotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpData)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pShellNotifyIcon != NULL)
        return g_pShellNotifyIcon(dwMessage, lpData);

    // No system tray on Windows 3.1.
    return FALSE;
}

bool Win32sHaveShellNotifyIcon(void)
{
    if (!g_inited)
        Win32sApiInit();
    return (g_pShellNotifyIcon != NULL) ? true : false;
}

// ---------------------------------------------------------------------------
// COMCTL32 wrappers
// ---------------------------------------------------------------------------

// VC2: the controls the dialogs need also exist when COMCTL32 does not - the
// vc2ctl library registers built-in versions (always on NT before 3.51,
// elsewhere for whatever COMCTL32 lacks; see vc2ctl.h).  So "have
// common controls" now means "native or built-in".
static bool g_ctlInited = false;
static bool g_ctlOk = false;

bool Win32sHaveCommonControls(void)
{
    Win32sInitCommonControls();
    return g_ctlOk;
}

void Win32sInitCommonControls(void)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pInitCommonControls != NULL)
        g_pInitCommonControls();

    if (!g_ctlInited) {
        g_ctlInited = true;
        g_ctlOk = Vc2CtlInit(VC2CTL_TAB) ? true : false;
    }
}

HWND Win32sCreateToolbarEx(HWND hwnd, DWORD ws, UINT wID, int nBitmaps,
                           HINSTANCE hBMInst, UINT wBMID, void *lpButtons,
                           int iNumButtons, int dxButton, int dyButton,
                           int dxBitmap, int dyBitmap, UINT uStructSize)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pCreateToolbarEx == NULL)
        return NULL;        // no COMCTL32: caller must cope with no toolbar

    return g_pCreateToolbarEx(hwnd, ws, wID, nBitmaps, hBMInst, wBMID,
                              lpButtons, iNumButtons, dxButton, dyButton,
                              dxBitmap, dyBitmap, uStructSize);
}

HANDLE Win32sLoadImageIcon(HINSTANCE hInst, LPCSTR name, int cx, int cy)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pLoadImage != NULL)
        return g_pLoadImage(hInst, name, IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);

    // Windows 3.1: LoadIcon only, at whatever size the resource happens to be.
    return (HANDLE)LoadIcon(hInst, name);
}

// ---------------------------------------------------------------------------
// Bell
// ---------------------------------------------------------------------------

BOOL Win32sPlayBell(void)
{
    if (!g_inited)
        Win32sApiInit();

    // PlaySound with SND_APPLICATION|SND_ALIAS resolves a registry sound
    // scheme entry, which does not exist on Windows 3.1.  Rather than import
    // winmm's PlaySound at all (another load-time dependency for a beep), use
    // MessageBeep, which exists everywhere.
    return MessageBeep(MB_OK);
}

// ---------------------------------------------------------------------------
// VC++ 2.x tree: NT 3.1 / Win32s 1.15a additions (see Win32sApi.h)
// ---------------------------------------------------------------------------

HBITMAP Win32sCreateDIBSection(HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage,
                               VOID **ppvBits, HANDLE hSection, DWORD dwOffset)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pCreateDIBSection != NULL)
        return g_pCreateDIBSection(hdc, pbmi, iUsage, ppvBits, hSection,
                                   dwOffset);

    if (ppvBits != NULL)
        *ppvBits = NULL;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}

BOOL Win32sDrawIconEx(HDC hdc, int x, int y, HICON hIcon, int cx, int cy,
                      UINT istepIfAniCur, HBRUSH hbrFlickerFreeDraw,
                      UINT diFlags)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pDrawIconEx != NULL)
        return g_pDrawIconEx(hdc, x, y, hIcon, cx, cy, istepIfAniCur,
                             hbrFlickerFreeDraw, diFlags);

    return DrawIcon(hdc, x, y, hIcon);
}

BOOL Win32sGetIconInfo(HICON hIcon, PICONINFO piconinfo)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pGetIconInfo != NULL)
        return g_pGetIconInfo(hIcon, piconinfo);

    return FALSE;
}

BOOL Win32sKeybdEvent(BYTE bVk, BYTE bScan, DWORD dwFlags, DWORD dwExtraInfo)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pKeybdEvent == NULL)
        return FALSE;
    g_pKeybdEvent(bVk, bScan, dwFlags, dwExtraInfo);
    return TRUE;
}

BOOL Win32sMouseEvent(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData,
                      DWORD dwExtraInfo)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pMouseEvent == NULL)
        return FALSE;
    g_pMouseEvent(dwFlags, dx, dy, dwData, dwExtraInfo);
    return TRUE;
}

// ---------------------------------------------------------------------------
// VC++ 2.x tree: NT service, security and desktop APIs (see Win32sApi.h)
//
// Unlike the wrappers above, these are resolved lazily, one at a time, on
// first use - not in Win32sApiInit - so a normal desktop session never looks
// them up, and on Win32s they are never looked up at all.
// ---------------------------------------------------------------------------

// Look up an NT-only entry point.  Returns NULL on Win32s without touching the
// DLL.  The modules named here are all mapped already (KERNEL32, USER32 and
// ADVAPI32 are linked), so GetModuleHandle is enough.
static FARPROC Win32sNtProc(const char *module, const char *name)
{
    if (Win32sIsWin32s())
        return NULL;
    HINSTANCE h = GetModuleHandle(module);
    if (h == NULL)
        return NULL;
    return GetProcAddress(h, name);
}

// Declares a function-local cached pointer 'p' of type 'type' and resolves it
// once.  Function-local statics with constant initialisers need no
// constructor, so this is safe before WinMain (see the rules at the top).
#define WIN32S_NT_PROC(type, module, name)                  \
    static type p = NULL;                                   \
    static int  p_tried = 0;                                \
    if (!p_tried) {                                         \
        p_tried = 1;                                        \
        p = (type)Win32sNtProc(module, name);               \
    }

typedef SC_HANDLE (WINAPI *PFNOPENSCMANAGER)(LPCSTR, LPCSTR, DWORD);
typedef SC_HANDLE (WINAPI *PFNCREATESERVICE)(SC_HANDLE, LPCSTR, LPCSTR, DWORD,
                                             DWORD, DWORD, DWORD, LPCSTR,
                                             LPCSTR, LPDWORD, LPCSTR, LPCSTR,
                                             LPCSTR);
typedef SC_HANDLE (WINAPI *PFNOPENSERVICE)(SC_HANDLE, LPCSTR, DWORD);
typedef BOOL (WINAPI *PFNSCHANDLEOP)(SC_HANDLE);
typedef BOOL (WINAPI *PFNCONTROLSERVICE)(SC_HANDLE, DWORD, LPSERVICE_STATUS);
typedef BOOL (WINAPI *PFNQUERYSERVICESTATUS)(SC_HANDLE, LPSERVICE_STATUS);
typedef BOOL (WINAPI *PFNSTARTSERVICECTRLDISPATCHER)(SERVICE_TABLE_ENTRY *);
typedef SERVICE_STATUS_HANDLE (WINAPI *PFNREGISTERSERVICECTRLHANDLER)(LPCSTR,
                                                     LPHANDLER_FUNCTION);
typedef BOOL (WINAPI *PFNSETSERVICESTATUS)(SERVICE_STATUS_HANDLE,
                                           LPSERVICE_STATUS);
typedef BOOL (WINAPI *PFNOPENPROCESSTOKEN)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *PFNHANDLEOP)(HANDLE);
typedef BOOL (WINAPI *PFNVOIDOP)(void);
typedef BOOL (WINAPI *PFNGETUSERNAME)(LPSTR, LPDWORD);
typedef HANDLE (WINAPI *PFNREGISTEREVENTSOURCE)(LPCSTR, LPCSTR);
typedef BOOL (WINAPI *PFNREPORTEVENT)(HANDLE, WORD, WORD, DWORD, PSID, WORD,
                                      DWORD, LPCSTR *, LPVOID);
typedef LONG (WINAPI *PFNREGDELETEVALUE)(HKEY, LPCSTR);
typedef HANDLE (WINAPI *PFNOPENPROCESS)(DWORD, BOOL, DWORD);
typedef HANDLE (WINAPI *PFNCREATETHREAD)(LPSECURITY_ATTRIBUTES, DWORD,
                                         LPTHREAD_START_ROUTINE, LPVOID,
                                         DWORD, LPDWORD);
typedef DWORD (WINAPI *PFNWAITFORSINGLEOBJECT)(HANDLE, DWORD);
typedef HDESK (WINAPI *PFNOPENDESKTOP)(LPSTR, DWORD, BOOL, DWORD);
typedef HDESK (WINAPI *PFNOPENINPUTDESKTOP)(DWORD, BOOL, DWORD);
typedef BOOL (WINAPI *PFNDESKTOPOP)(HDESK);
typedef HDESK (WINAPI *PFNGETTHREADDESKTOP)(DWORD);
typedef BOOL (WINAPI *PFNGETUSEROBJECTINFORMATION)(HANDLE, int, PVOID, DWORD,
                                                   LPDWORD);
typedef HWINSTA (WINAPI *PFNGETPROCESSWINDOWSTATION)(void);
typedef BOOL (WINAPI *PFNPOSTTHREADMESSAGE)(DWORD, UINT, WPARAM, LPARAM);

// --- service control manager ---

SC_HANDLE Win32sOpenSCManager(LPCSTR lpMachineName, LPCSTR lpDatabaseName,
                              DWORD dwDesiredAccess)
{
    WIN32S_NT_PROC(PFNOPENSCMANAGER, "ADVAPI32", "OpenSCManagerA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(lpMachineName, lpDatabaseName, dwDesiredAccess);
}

SC_HANDLE Win32sCreateService(SC_HANDLE hSCManager, LPCSTR lpServiceName,
                              LPCSTR lpDisplayName, DWORD dwDesiredAccess,
                              DWORD dwServiceType, DWORD dwStartType,
                              DWORD dwErrorControl, LPCSTR lpBinaryPathName,
                              LPCSTR lpLoadOrderGroup, LPDWORD lpdwTagId,
                              LPCSTR lpDependencies, LPCSTR lpServiceStartName,
                              LPCSTR lpPassword)
{
    WIN32S_NT_PROC(PFNCREATESERVICE, "ADVAPI32", "CreateServiceA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(hSCManager, lpServiceName, lpDisplayName, dwDesiredAccess,
             dwServiceType, dwStartType, dwErrorControl, lpBinaryPathName,
             lpLoadOrderGroup, lpdwTagId, lpDependencies, lpServiceStartName,
             lpPassword);
}

SC_HANDLE Win32sOpenService(SC_HANDLE hSCManager, LPCSTR lpServiceName,
                            DWORD dwDesiredAccess)
{
    WIN32S_NT_PROC(PFNOPENSERVICE, "ADVAPI32", "OpenServiceA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(hSCManager, lpServiceName, dwDesiredAccess);
}

BOOL Win32sDeleteService(SC_HANDLE hService)
{
    WIN32S_NT_PROC(PFNSCHANDLEOP, "ADVAPI32", "DeleteService");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hService);
}

BOOL Win32sControlService(SC_HANDLE hService, DWORD dwControl,
                          LPSERVICE_STATUS lpServiceStatus)
{
    WIN32S_NT_PROC(PFNCONTROLSERVICE, "ADVAPI32", "ControlService");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hService, dwControl, lpServiceStatus);
}

BOOL Win32sQueryServiceStatus(SC_HANDLE hService,
                              LPSERVICE_STATUS lpServiceStatus)
{
    WIN32S_NT_PROC(PFNQUERYSERVICESTATUS, "ADVAPI32", "QueryServiceStatus");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hService, lpServiceStatus);
}

BOOL Win32sCloseServiceHandle(SC_HANDLE hSCObject)
{
    WIN32S_NT_PROC(PFNSCHANDLEOP, "ADVAPI32", "CloseServiceHandle");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hSCObject);
}

BOOL Win32sStartServiceCtrlDispatcher(SERVICE_TABLE_ENTRY *lpServiceStartTable)
{
    WIN32S_NT_PROC(PFNSTARTSERVICECTRLDISPATCHER, "ADVAPI32",
                   "StartServiceCtrlDispatcherA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(lpServiceStartTable);
}

SERVICE_STATUS_HANDLE Win32sRegisterServiceCtrlHandler(LPCSTR lpServiceName,
                                                       LPHANDLER_FUNCTION lpHandlerProc)
{
    WIN32S_NT_PROC(PFNREGISTERSERVICECTRLHANDLER, "ADVAPI32",
                   "RegisterServiceCtrlHandlerA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return 0; }
    return p(lpServiceName, lpHandlerProc);
}

BOOL Win32sSetServiceStatus(SERVICE_STATUS_HANDLE hServiceStatus,
                            LPSERVICE_STATUS lpServiceStatus)
{
    WIN32S_NT_PROC(PFNSETSERVICESTATUS, "ADVAPI32", "SetServiceStatus");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hServiceStatus, lpServiceStatus);
}

// --- security, user name, event log, registry values ---

BOOL Win32sOpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess,
                            PHANDLE TokenHandle)
{
    WIN32S_NT_PROC(PFNOPENPROCESSTOKEN, "ADVAPI32", "OpenProcessToken");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(ProcessHandle, DesiredAccess, TokenHandle);
}

BOOL Win32sImpersonateLoggedOnUser(HANDLE hToken)
{
    // NT 3.51+; on NT 3.1 / 3.5 this fails and the service simply does not
    // impersonate (same as the msvc4 tree's MyImpersonateLoggedOnUser).
    WIN32S_NT_PROC(PFNHANDLEOP, "ADVAPI32", "ImpersonateLoggedOnUser");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hToken);
}

typedef BOOL (WINAPI *PFNATTACHTHREADINPUT)(DWORD, DWORD, BOOL);

BOOL Win32sAttachThreadInput(DWORD idAttach, DWORD idAttachTo, BOOL fAttach)
{
    WIN32S_NT_PROC(PFNATTACHTHREADINPUT, "USER32", "AttachThreadInput");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(idAttach, idAttachTo, fAttach);
}

BOOL Win32sRevertToSelf(void)
{
    WIN32S_NT_PROC(PFNVOIDOP, "ADVAPI32", "RevertToSelf");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p();
}

BOOL Win32sGetUserName(LPSTR lpBuffer, LPDWORD nSize)
{
    WIN32S_NT_PROC(PFNGETUSERNAME, "ADVAPI32", "GetUserNameA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(lpBuffer, nSize);
}

HANDLE Win32sRegisterEventSource(LPCSTR lpUNCServerName, LPCSTR lpSourceName)
{
    WIN32S_NT_PROC(PFNREGISTEREVENTSOURCE, "ADVAPI32", "RegisterEventSourceA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(lpUNCServerName, lpSourceName);
}

BOOL Win32sReportEvent(HANDLE hEventLog, WORD wType, WORD wCategory,
                       DWORD dwEventID, PSID lpUserSid, WORD wNumStrings,
                       DWORD dwDataSize, LPCSTR *lpStrings, LPVOID lpRawData)
{
    WIN32S_NT_PROC(PFNREPORTEVENT, "ADVAPI32", "ReportEventA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hEventLog, wType, wCategory, dwEventID, lpUserSid, wNumStrings,
             dwDataSize, lpStrings, lpRawData);
}

BOOL Win32sDeregisterEventSource(HANDLE hEventLog)
{
    WIN32S_NT_PROC(PFNHANDLEOP, "ADVAPI32", "DeregisterEventSource");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hEventLog);
}

LONG Win32sRegDeleteValue(HKEY hKey, LPCSTR lpValueName)
{
    WIN32S_NT_PROC(PFNREGDELETEVALUE, "ADVAPI32", "RegDeleteValueA");
    if (p == NULL)
        return ERROR_CALL_NOT_IMPLEMENTED;
    return p(hKey, lpValueName);
}

// --- process and thread ---

HANDLE Win32sOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle,
                         DWORD dwProcessId)
{
    WIN32S_NT_PROC(PFNOPENPROCESS, "KERNEL32", "OpenProcess");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(dwDesiredAccess, bInheritHandle, dwProcessId);
}

HANDLE Win32sCreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                          DWORD dwStackSize,
                          LPTHREAD_START_ROUTINE lpStartAddress,
                          LPVOID lpParameter, DWORD dwCreationFlags,
                          LPDWORD lpThreadId)
{
    WIN32S_NT_PROC(PFNCREATETHREAD, "KERNEL32", "CreateThread");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter,
             dwCreationFlags, lpThreadId);
}

DWORD Win32sWaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    WIN32S_NT_PROC(PFNWAITFORSINGLEOBJECT, "KERNEL32", "WaitForSingleObject");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return WAIT_FAILED; }
    return p(hHandle, dwMilliseconds);
}

// --- window stations, desktops, thread messages ---

HDESK Win32sOpenDesktop(LPSTR lpszDesktop, DWORD dwFlags, BOOL fInherit,
                        DWORD dwDesiredAccess)
{
    WIN32S_NT_PROC(PFNOPENDESKTOP, "USER32", "OpenDesktopA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(lpszDesktop, dwFlags, fInherit, dwDesiredAccess);
}

HDESK Win32sOpenInputDesktop(DWORD dwFlags, BOOL fInherit,
                             DWORD dwDesiredAccess)
{
    WIN32S_NT_PROC(PFNOPENINPUTDESKTOP, "USER32", "OpenInputDesktop");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(dwFlags, fInherit, dwDesiredAccess);
}

BOOL Win32sCloseDesktop(HDESK hDesktop)
{
    WIN32S_NT_PROC(PFNDESKTOPOP, "USER32", "CloseDesktop");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hDesktop);
}

HDESK Win32sGetThreadDesktop(DWORD dwThreadId)
{
    WIN32S_NT_PROC(PFNGETTHREADDESKTOP, "USER32", "GetThreadDesktop");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p(dwThreadId);
}

BOOL Win32sSetThreadDesktop(HDESK hDesktop)
{
    WIN32S_NT_PROC(PFNDESKTOPOP, "USER32", "SetThreadDesktop");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hDesktop);
}

BOOL Win32sGetUserObjectInformation(HANDLE hObj, int nIndex, PVOID pvInfo,
                                    DWORD nLength, LPDWORD lpnLengthNeeded)
{
    WIN32S_NT_PROC(PFNGETUSEROBJECTINFORMATION, "USER32",
                   "GetUserObjectInformationA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(hObj, nIndex, pvInfo, nLength, lpnLengthNeeded);
}

HWINSTA Win32sGetProcessWindowStation(void)
{
    WIN32S_NT_PROC(PFNGETPROCESSWINDOWSTATION, "USER32",
                   "GetProcessWindowStation");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return NULL; }
    return p();
}

BOOL Win32sPostThreadMessage(DWORD idThread, UINT Msg, WPARAM wParam,
                             LPARAM lParam)
{
    WIN32S_NT_PROC(PFNPOSTTHREADMESSAGE, "USER32", "PostThreadMessageA");
    if (p == NULL) { SetLastError(ERROR_CALL_NOT_IMPLEMENTED); return FALSE; }
    return p(idThread, Msg, wParam, lParam);
}

bool Win32sHaveDesktopApi(void)
{
    // Resolved once.  All six are needed for desktop switching to make sense.
    static int known = 0;
    static int have = 0;
    if (!known) {
        known = 1;
        have = (Win32sNtProc("USER32", "OpenDesktopA") != NULL &&
                Win32sNtProc("USER32", "OpenInputDesktop") != NULL &&
                Win32sNtProc("USER32", "CloseDesktop") != NULL &&
                Win32sNtProc("USER32", "GetThreadDesktop") != NULL &&
                Win32sNtProc("USER32", "SetThreadDesktop") != NULL &&
                Win32sNtProc("USER32", "GetUserObjectInformationA") != NULL)
               ? 1 : 0;
    }
    return have ? true : false;
}
