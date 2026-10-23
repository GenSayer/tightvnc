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
typedef BOOL   (WINAPI *PFNSETBRUSHORGEX)(HDC, int, int, LPPOINT);
typedef BOOL   (WINAPI *PFNSHELLNOTIFYICON)(DWORD, PNOTIFYICONDATA);
typedef void   (WINAPI *PFNINITCOMMONCONTROLS)(void);
typedef HWND   (WINAPI *PFNCREATETOOLBAREX)(HWND, DWORD, UINT, int, HINSTANCE,
                                            UINT, void *, int, int, int, int,
                                            int, UINT);
typedef HANDLE (WINAPI *PFNLOADIMAGE)(HINSTANCE, LPCSTR, UINT, int, int, UINT);

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
static PFNSETBRUSHORGEX         g_pSetBrushOrgEx         = NULL;
static PFNSHELLNOTIFYICON       g_pShellNotifyIcon       = NULL;
static PFNINITCOMMONCONTROLS    g_pInitCommonControls    = NULL;
static PFNCREATETOOLBAREX       g_pCreateToolbarEx       = NULL;
static PFNLOADIMAGE             g_pLoadImage             = NULL;

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
    }

    if (g_hGdi != NULL) {
        g_pSetPixelV =
            (PFNSETPIXELV)GetProcAddress(g_hGdi, "SetPixelV");
        g_pSetBrushOrgEx =
            (PFNSETBRUSHORGEX)GetProcAddress(g_hGdi, "SetBrushOrgEx");
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

// ---------------------------------------------------------------------------
// COMCTL32 wrappers
// ---------------------------------------------------------------------------

bool Win32sHaveCommonControls(void)
{
    if (!g_inited)
        Win32sApiInit();
    return (g_pInitCommonControls != NULL) ? true : false;
}

void Win32sInitCommonControls(void)
{
    if (!g_inited)
        Win32sApiInit();

    if (g_pInitCommonControls != NULL)
        g_pInitCommonControls();
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
