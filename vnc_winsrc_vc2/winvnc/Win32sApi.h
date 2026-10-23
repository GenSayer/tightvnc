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

// SetWindowRgn: Win95+ and absent from the Win32s USER32.
//
// Used only by CMatchWindow to give the "select a window to share" frame its
// hollow shape.  Because the linker records the name as an import, its presence
// would stop the EXE LOADING on Win32s - so it must be resolved at run time even
// though the feature is purely cosmetic.  Fallback: do nothing, leaving a solid
// frame.  The caller must then free the region itself, so this wrapper reports
// whether it took ownership.
BOOL Win32sSetWindowRgn(HWND hwnd, HRGN hrgn, BOOL bRedraw);

// GetWindowRgn: Win95 / NT 3.51+, absent from NT 3.1, Win32s and VC++ 2.x's
// headers.  Fallback returns ERROR, i.e. "this window has no region", which
// both callers (MatchWindow.cpp, SharedDesktopArea.cpp) already handle.
int Win32sGetWindowRgn(HWND hwnd, HRGN hrgn);

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

// VC2: TRUE when SHELL32 exports Shell_NotifyIconA (Win95 / NT 4.0+).  NT 3.x
// has SHELL32 but no notification area.
bool Win32sHaveShellNotifyIcon(void);

// ---------------------------------------------------------------------------
// COMCTL32
// ---------------------------------------------------------------------------

// InitCommonControls exists in the COMCTL32 that ships with Win32s 1.30, but
// COMCTL32.DLL itself may be missing on a bare Windows 3.1 machine.  Because
// the viewer links comctl32.lib, a missing DLL stops the EXE loading before
// WinMain.  Win32sInitCommonControls() calls InitCommonControls only if the
// DLL is there.
//
// VC2: it then registers the built-in controls from vc2ctl (vc2ctl/vc2ctl.h)
// - all of them on NT before 3.51, elsewhere only those COMCTL32 lacks - so
// Win32sHaveCommonControls() reports whether the dialogs' controls can be
// created at all, natively or built-in.
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

// ---------------------------------------------------------------------------
// VC++ 2.x TREE: NT 3.1 / WIN32S 1.15a ADDITIONS
//
// The MSVC 4.1 tree called these directly because it only had to load on
// Win32s 1.30 and NT 3.5+.  NT 3.1 and the Win32s releases before 1.30 may not
// export them, and VC++ 2.x's NT 3.5 import libraries would link them anyway,
// so they are resolved here with the same rules as everything above.
// ---------------------------------------------------------------------------

// Icon-drawing flags for DrawIconEx; not in every VC++ 2.x winuser.h.
#ifndef DI_MASK
#define DI_MASK     0x0001
#define DI_IMAGE    0x0002
#define DI_NORMAL   0x0003
#endif

// CreateDIBSection: NT 3.5 / Win32s 1.30.  Fallback returns NULL with
// *ppvBits set to NULL, which vncDesktop already treats as "no DIB section":
// m_DIBbits stays NULL and CopyToBuffer uses the GetDIBits path.
HBITMAP Win32sCreateDIBSection(HDC hdc, CONST BITMAPINFO *pbmi, UINT iUsage,
                               VOID **ppvBits, HANDLE hSection, DWORD dwOffset);

// DrawIconEx: NT 3.5 / Win95.  Fallback: DrawIcon at the icon's natural size.
// DrawIcon always draws mask + image, so DI_IMAGE-only requests (rich cursor
// capture) come out as a normal draw onto the black background the caller
// prepared - close enough for the cursor shape, and the only alternative is
// sending no shape at all.
BOOL Win32sDrawIconEx(HDC hdc, int x, int y, HICON hIcon, int cx, int cy,
                      UINT istepIfAniCur, HBRUSH hbrFlickerFreeDraw,
                      UINT diFlags);

// GetIconInfo: fallback returns FALSE (no hotspot correction, no cursor shape
// updates); both callers already handle failure.
BOOL Win32sGetIconInfo(HICON hIcon, PICONINFO piconinfo);

// keybd_event / mouse_event: present on NT 3.1 and Win32s 1.30, but not
// guaranteed on earlier Win32s.  Fallback: the input is dropped (there is no
// other way to synthesise input there), and the wrappers return FALSE so a
// caller can log it.
BOOL Win32sKeybdEvent(BYTE bVk, BYTE bScan, DWORD dwFlags, DWORD dwExtraInfo);
BOOL Win32sMouseEvent(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData,
                      DWORD dwExtraInfo);

// ---------------------------------------------------------------------------
// VC++ 2.x TREE: NT SERVICE, SECURITY AND DESKTOP APIs (used by vncService.cpp)
//
// The NT service mode is back in this tree, but Win32s's ADVAPI32 and USER32
// stubs do not export any of these, and NT 3.1 lacks some of them (e.g.
// ImpersonateLoggedOnUser is NT 3.51+).  So none of them is a load-time
// import:
//
//   * each wrapper resolves its entry point by name on first use, and NEVER
//     on Win32s - there it fails immediately without touching the DLL;
//   * a missing entry point makes the wrapper fail the way the real API
//     would (NULL / FALSE / an error code) with GetLastError() ==
//     ERROR_CALL_NOT_IMPLEMENTED, so the existing NT error paths in
//     vncService.cpp handle it unchanged.
//
// Win32sHaveDesktopApi() says whether the window-station / desktop functions
// are all present; when they are not, vncService treats the machine as having
// one desktop, exactly as on Win32s.
// ---------------------------------------------------------------------------

// Constants that VC++ 2.x's headers may not define.
#ifndef SERVICE_INTERACTIVE_PROCESS
#define SERVICE_INTERACTIVE_PROCESS 0x00000100
#endif
#ifndef UOI_NAME
#define UOI_NAME        2
#endif
#ifndef UOI_USER_SID
#define UOI_USER_SID    4
#endif
// One guard per name: VC++ 2.x defines some of these (DESKTOP_READOBJECTS)
// but not others (DESKTOP_SWITCHDESKTOP), so a single block guard skipped them.
#ifndef DESKTOP_READOBJECTS
#define DESKTOP_READOBJECTS     0x0001L
#endif
#ifndef DESKTOP_CREATEWINDOW
#define DESKTOP_CREATEWINDOW    0x0002L
#endif
#ifndef DESKTOP_CREATEMENU
#define DESKTOP_CREATEMENU      0x0004L
#endif
#ifndef DESKTOP_HOOKCONTROL
#define DESKTOP_HOOKCONTROL     0x0008L
#endif
#ifndef DESKTOP_ENUMERATE
#define DESKTOP_ENUMERATE       0x0040L
#endif
#ifndef DESKTOP_WRITEOBJECTS
#define DESKTOP_WRITEOBJECTS    0x0080L
#endif
#ifndef DESKTOP_SWITCHDESKTOP
#define DESKTOP_SWITCHDESKTOP   0x0100L
#endif
#ifndef ERROR_NOT_LOGGED_ON
#define ERROR_NOT_LOGGED_ON     1245L
#endif
#ifndef ERROR_CALL_NOT_IMPLEMENTED
#define ERROR_CALL_NOT_IMPLEMENTED 120L
#endif

// ADVAPI32 - service control manager
SC_HANDLE Win32sOpenSCManager(LPCSTR lpMachineName, LPCSTR lpDatabaseName,
                              DWORD dwDesiredAccess);
SC_HANDLE Win32sCreateService(SC_HANDLE hSCManager, LPCSTR lpServiceName,
                              LPCSTR lpDisplayName, DWORD dwDesiredAccess,
                              DWORD dwServiceType, DWORD dwStartType,
                              DWORD dwErrorControl, LPCSTR lpBinaryPathName,
                              LPCSTR lpLoadOrderGroup, LPDWORD lpdwTagId,
                              LPCSTR lpDependencies, LPCSTR lpServiceStartName,
                              LPCSTR lpPassword);
SC_HANDLE Win32sOpenService(SC_HANDLE hSCManager, LPCSTR lpServiceName,
                            DWORD dwDesiredAccess);
BOOL Win32sDeleteService(SC_HANDLE hService);
BOOL Win32sControlService(SC_HANDLE hService, DWORD dwControl,
                          LPSERVICE_STATUS lpServiceStatus);
BOOL Win32sQueryServiceStatus(SC_HANDLE hService,
                              LPSERVICE_STATUS lpServiceStatus);
BOOL Win32sCloseServiceHandle(SC_HANDLE hSCObject);
BOOL Win32sStartServiceCtrlDispatcher(SERVICE_TABLE_ENTRY *lpServiceStartTable);
SERVICE_STATUS_HANDLE Win32sRegisterServiceCtrlHandler(LPCSTR lpServiceName,
                                                       LPHANDLER_FUNCTION lpHandlerProc);
BOOL Win32sSetServiceStatus(SERVICE_STATUS_HANDLE hServiceStatus,
                            LPSERVICE_STATUS lpServiceStatus);

// ADVAPI32 - security, user name, event log, registry values
BOOL Win32sOpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess,
                            PHANDLE TokenHandle);
BOOL Win32sImpersonateLoggedOnUser(HANDLE hToken);

// USER32 AttachThreadInput (NT only; FALSE where it cannot be resolved).
BOOL Win32sAttachThreadInput(DWORD idAttach, DWORD idAttachTo, BOOL fAttach);
BOOL Win32sRevertToSelf(void);
BOOL Win32sGetUserName(LPSTR lpBuffer, LPDWORD nSize);
HANDLE Win32sRegisterEventSource(LPCSTR lpUNCServerName, LPCSTR lpSourceName);
BOOL Win32sReportEvent(HANDLE hEventLog, WORD wType, WORD wCategory,
                       DWORD dwEventID, PSID lpUserSid, WORD wNumStrings,
                       DWORD dwDataSize, LPCSTR *lpStrings, LPVOID lpRawData);
BOOL Win32sDeregisterEventSource(HANDLE hEventLog);
LONG Win32sRegDeleteValue(HKEY hKey, LPCSTR lpValueName);

// KERNEL32 - process and thread
HANDLE Win32sOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle,
                         DWORD dwProcessId);
HANDLE Win32sCreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes,
                          DWORD dwStackSize,
                          LPTHREAD_START_ROUTINE lpStartAddress,
                          LPVOID lpParameter, DWORD dwCreationFlags,
                          LPDWORD lpThreadId);
DWORD Win32sWaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

// USER32 - window stations, desktops, thread messages
bool Win32sHaveDesktopApi(void);
HDESK Win32sOpenDesktop(LPSTR lpszDesktop, DWORD dwFlags, BOOL fInherit,
                        DWORD dwDesiredAccess);
HDESK Win32sOpenInputDesktop(DWORD dwFlags, BOOL fInherit,
                             DWORD dwDesiredAccess);
BOOL Win32sCloseDesktop(HDESK hDesktop);
HDESK Win32sGetThreadDesktop(DWORD dwThreadId);
BOOL Win32sSetThreadDesktop(HDESK hDesktop);
BOOL Win32sGetUserObjectInformation(HANDLE hObj, int nIndex, PVOID pvInfo,
                                    DWORD nLength, LPDWORD lpnLengthNeeded);
HWINSTA Win32sGetProcessWindowStation(void);
BOOL Win32sPostThreadMessage(DWORD idThread, UINT Msg, WPARAM wParam,
                             LPARAM lParam);

#endif // WIN32SAPI_H__
