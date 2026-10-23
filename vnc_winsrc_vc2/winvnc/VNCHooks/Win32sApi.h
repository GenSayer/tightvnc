//  Win32sApi.h - runtime resolution of APIs that do not exist on Win32s
//
//  This file is part of the TightVNC Win32s / Windows 3.1 port.
//
//  ---------------------------------------------------------------------------
//  WHY THIS EXISTS - and why the viewer crashed at launch without it
//  ---------------------------------------------------------------------------
//
//  Win32s implements a *subset* of Win32.  The missing entry points are simply
//  absent from its KERNEL32/USER32/GDI32/COMCTL32/SHELL32 stubs.
//
//  A Win32 EXE lists every imported function by name in its import table, and
//  the loader resolves them all *before* a single instruction of the program
//  runs.  If one name cannot be resolved, the process never starts.  Depending
//  on the loader you get either a "call to undefined dynalink" box or nothing
//  at all - which is exactly the reported symptom: the viewer dies on launch on
//  Win32s with no output, while the same binary runs on Win9x.
//
//  Note carefully what does NOT help here:
//
//    * Declaring the function yourself (as the current code does in
//      ClientConnection.cpp, Daemon.cpp, Daemon.h and FileTransfer.cpp, e.g.
//      "HBRUSH __stdcall GetSysColorBrush(int);").  That satisfies the
//      *compiler*, and the linker then happily adds the name to the import
//      table.  The load-time failure is unchanged - it is arguably made worse,
//      because it hides the problem until run time on the target machine.
//
//    * #ifdef / WINVER.  WINVER only controls what the headers declare; it has
//      no effect on what the loader demands.
//
//  The only fix is to keep these names out of the import table entirely and
//  fetch them with LoadLibrary/GetProcAddress at run time, falling back to
//  something that works on Win32s when they are absent.  That is what this
//  header does.  Nothing here is Win32s-specific at run time: on Win9x/NT the
//  real function is found and used, so one binary serves all targets.
//
//  Usage: include this header, then call the Win32sXxx() wrapper instead of the
//  raw API.  Call Win32sApiInit() once at the top of WinMain.
//  ---------------------------------------------------------------------------

#ifndef WIN32SAPI_H__
#define WIN32SAPI_H__

#include <windows.h>

// ---------------------------------------------------------------------------
// Platform test.  Win32s reports dwPlatformId == VER_PLATFORM_WIN32s (0).
// ---------------------------------------------------------------------------
#ifndef VER_PLATFORM_WIN32s
#define VER_PLATFORM_WIN32s 0
#endif

// Call once, first thing in WinMain, before any window is created.
void Win32sApiInit(void);

// True when running under Win32s on Windows 3.1x.
bool Win32sIsWin32s(void);

// ---------------------------------------------------------------------------
// USER32
// ---------------------------------------------------------------------------

// GetSysColorBrush: Win95+.  Fallback: CreateSolidBrush(GetSysColor(i)).
// The fallback brush is cached and never deleted, matching the lifetime of the
// shared system brush the real API returns (window classes keep it forever).
HBRUSH Win32sGetSysColorBrush(int nIndex);

// SetScrollInfo/GetScrollInfo: Win95+.  Fallback: SetScrollRange/SetScrollPos,
// which exist in Windows 3.1.  Proportional thumb size is simply not available
// under Win32s, so SIF_PAGE is ignored in the fallback path.
#ifndef SIF_RANGE
#define SIF_RANGE           0x0001
#define SIF_PAGE            0x0002
#define SIF_POS             0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS        0x0010
#define SIF_ALL             (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)
typedef struct tagSCROLLINFO {
    UINT cbSize;
    UINT fMask;
    int  nMin;
    int  nMax;
    UINT nPage;
    int  nPos;
    int  nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;
#endif

int Win32sSetScrollInfo(HWND hwnd, int fnBar, LPSCROLLINFO lpsi, BOOL fRedraw);

// SetMenuDefaultItem: Win95+.  No equivalent in 3.1; fallback does nothing
// (the tray menu simply has no bold entry).
BOOL Win32sSetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos);

// LoadKeyboardLayout: Win95+.  Fallback returns NULL (single layout on 3.1).
HKL Win32sLoadKeyboardLayout(LPCSTR pwszKLID, UINT Flags);

// GetKeyboardLayoutName: Win95+.  Fallback writes "(n/a)".
BOOL Win32sGetKeyboardLayoutName(LPSTR pwszKLID);

// SystemParametersInfo(SPI_GETWORKAREA): present on Win32s only in part, and
// the work area concept does not exist in 3.1 (no taskbar).  This helper
// always returns a usable rectangle.
//
// This replaces the #define SystemParametersInfo(...) macro previously in
// win32s_fix.h.  That macro was dangerous: it expanded any call to
// SystemParametersInfo, evaluated (pvParam) several times, cast it to LPRECT
// regardless of the actual action, and referenced the real function inside its
// own expansion - so a non-SPI_GETWORKAREA call still put the import in the
// table.
void Win32sGetWorkArea(RECT *pWorkArea);

// SetWindowPlacement/GetWindowPlacement exist on Win32s, but the SW_SHOWxxx
// handling differs; wrappers keep the call sites readable and give one place
// to change behaviour if testing shows a problem.
BOOL Win32sGetWindowPlacement(HWND hwnd, WINDOWPLACEMENT *pwp);
BOOL Win32sSetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *pwp);

// ScrollWindowEx: Win95+ in the form used here.  Fallback: ScrollWindow.
int Win32sScrollWindowEx(HWND hwnd, int dx, int dy, CONST RECT *prcScroll,
                         CONST RECT *prcClip, HRGN hrgnUpdate,
                         LPRECT prcUpdate, UINT flags);

// SetForegroundWindow: Win95+.  Fallback: BringWindowToTop + SetActiveWindow.
BOOL Win32sSetForegroundWindow(HWND hwnd);

// GetForegroundWindow: Win95+.  Fallback: GetActiveWindow.
HWND Win32sGetForegroundWindow(void);

// ---------------------------------------------------------------------------
// GDI32
// ---------------------------------------------------------------------------

// SetPixelV: Win95+.  Fallback: SetPixel (returns the colour, ignored).
// Used through the SETPIXEL macro in ClientConnection.h by the Raw/Zlib/
// Hextile/CoRRE decoders, so it is on the hot path for every screen update.
BOOL Win32sSetPixelV(HDC hdc, int x, int y, COLORREF color);

// SetBrushOrgEx: exists in 3.1 as SetBrushOrg; wrapper hides the difference.
BOOL Win32sSetBrushOrgEx(HDC hdc, int x, int y, LPPOINT ppt);

// ---------------------------------------------------------------------------
// SHELL32
// ---------------------------------------------------------------------------

// Shell_NotifyIcon: Win95+ and there is no system tray on Windows 3.1 at all.
// Fallback returns FALSE.  Callers must cope: see Daemon.cpp, which now shows
// a small ordinary window instead of a tray icon on Win32s.
#ifndef NIM_ADD
#define NIM_ADD     0x00000000
#define NIM_MODIFY  0x00000001
#define NIM_DELETE  0x00000002
#define NIF_MESSAGE 0x00000001
#define NIF_ICON    0x00000002
#define NIF_TIP     0x00000004
typedef struct _NOTIFYICONDATAA {
    DWORD cbSize;
    HWND  hWnd;
    UINT  uID;
    UINT  uFlags;
    UINT  uCallbackMessage;
    HICON hIcon;
    char  szTip[64];
} NOTIFYICONDATAA, *PNOTIFYICONDATAA;
typedef NOTIFYICONDATAA NOTIFYICONDATA;
typedef PNOTIFYICONDATAA PNOTIFYICONDATA;
#endif

BOOL Win32sShellNotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpData);

// ---------------------------------------------------------------------------
// COMCTL32
// ---------------------------------------------------------------------------

// InitCommonControls exists in the COMCTL32 that ships with Win32s 1.30, but
// COMCTL32.DLL itself may be missing on a bare Windows 3.1 machine.  Because
// the viewer links comctl32.lib, a missing DLL stops the EXE loading before
// WinMain.  Win32sHaveCommonControls() reports whether the DLL was found;
// Win32sInitCommonControls() calls InitCommonControls only if it is.
//
// IMPORTANT: this wrapper cannot remove the load-time dependency by itself.
// Any *directly called* comctl32 function (CreateToolbarEx, the ListView_*
// and TreeView_* macros, TabCtrl_*) still puts an import in the table.  See
// the porting notes: on Win32s the toolbar, options tab control and file
// transfer dialog must either be dropped or reached the same dynamic way.
bool Win32sHaveCommonControls(void);
void Win32sInitCommonControls(void);

// CreateToolbarEx is a real COMCTL32 export, so calling it directly keeps a
// load-time dependency on COMCTL32.DLL in the EXE.  Resolved at run time here
// so that comctl32.lib can be dropped from the link line entirely: the
// ListView_/TreeView_/TabCtrl_ macros are all SendMessage-based and generate no
// imports, and InitCommonControls is resolved above.  With that done the viewer
// loads on a Windows 3.1 machine that has no COMCTL32.DLL at all.
//
// Declared with the fields this project actually passes; see the call in
// ClientConnection::CreateToolbar.
HWND Win32sCreateToolbarEx(HWND hwnd, DWORD ws, UINT wID, int nBitmaps,
                           HINSTANCE hBMInst, UINT wBMID, void *lpButtons,
                           int iNumButtons, int dxButton, int dyButton,
                           int dxBitmap, int dyBitmap, UINT uStructSize);

// LoadImage is Win95+.  It is used only to fetch two 16x16 button icons in the
// file transfer dialog.  Fallback: LoadIcon (which ignores the requested size).
HANDLE Win32sLoadImageIcon(HINSTANCE hInst, LPCSTR name, int cx, int cy);

// ---------------------------------------------------------------------------
// WINMM
// ---------------------------------------------------------------------------

// PlaySound: winmm on Win32s exists but SND_APPLICATION/SND_ALIAS lookups go
// through the registry sound-event scheme, which Windows 3.1 does not have.
// Fallback: MessageBeep.
BOOL Win32sPlayBell(void);

#endif // WIN32SAPI_H__
