//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//  Copyright (C) 2009 GlavSoft LLC. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.

// ==========================================================================
// vncService - VC++ 2.x TREE: NT SERVICE ON NT, NONE ON WIN32S
// ==========================================================================
//
// One WinVNC.exe runs on Win32s, Windows NT (3.1 and later) and Win9x, so
// this file is a merge of two earlier versions:
//
//   * the NT service / desktop / impersonation code from
//     vncService.cpp.nt-original, plus the NT 3.5 fixes from the msvc4 tree
//     (ImpersonateLoggedOnUser resolved at run time);
//
//   * the Win32s behaviour from the Win32s tree: GetVersion()-based platform
//     detection, IsWin32s(), a single always-selected desktop, "proceed"
//     impersonation, and service switches that explain themselves.
//
// Which one runs is decided at RUN TIME from GetVersion().  Nothing NT-only is
// a load-time import: every SCM, security, event-log and desktop call goes
// through the Win32sXxx wrappers in Win32sApi.cpp, which are never resolved
// on Win32s and fail cleanly where NT 3.1 lacks an entry point.
//
// THREADING.  This tree's omnithread is single-threaded (see omnithread.h),
// so the original's "ServiceMain spawns a work thread" design cannot be used.
// Instead ServiceMain - which the SCM already runs on a thread of its own -
// runs WinVNCAppMain() itself and reports SERVICE_STOPPED when it returns.
// The control handler runs on the dispatcher thread and only posts WM_QUIT to
// the ServiceMain thread, so the server code still only ever runs on one
// thread.  Ctrl+Alt+Del simulation uses a short-lived raw CreateThread worker
// because the thread doing it must have no windows or hooks; the caller waits
// for it, so again nothing runs concurrently.
//
// KNOWN LIMITATION.  Because all of the server's windows live on that one
// thread, SetThreadDesktop() can only succeed before the first window is
// created.  The service therefore serves the desktop that is current when it
// starts capturing; when the input desktop changes (Winlogon, a locked
// workstation) the desktop-change check in vncDesktop fails to re-attach and
// clients are dropped, the same as WinVNC in application mode.  Following
// the input desktop needs the capture moved to its own thread.
// ==========================================================================

#include "stdhdrs.h"

// WIN32S: <lmcons.h> is the LAN Manager header, included by the original solely
// for UNLEN (the maximum user name length).  It is not part of the MSVC 4.1
// core SDK and pulls in netapi declarations we do not want.  UNLEN is defined
// locally below instead.
// #include <lmcons.h>

// vncService.h declares the class this file implements.  It MUST be included:
// nothing else in this file's include chain pulls it in transitively.
#include "vncService.h"

#include "omnithread.h"
#include "WinVNC.h"
#include "vncMenu.h"
#include "vncTimedMsgBox.h"
#include "Win32sApi.h"

#ifndef UNLEN
#define UNLEN 256
#endif

#ifndef VER_PLATFORM_WIN32s
#define VER_PLATFORM_WIN32s 0
#endif

// Error message logging
void LogErrorMsg(char *message);

// OS-SPECIFIC ROUTINES

// Create an instance of the vncService class to cause the static fields to be
// initialised properly.
//
// NOTE: this is a file-scope object, so its constructor runs BEFORE WinMain.
// On Win32s that is dangerous territory - it is what killed the viewer at
// startup.  The constructor below is therefore restricted to GetVersion(),
// which is safe: it is in every Win32 implementation, takes no arguments, and
// allocates nothing.
vncService init;

DWORD	g_platform_id;
BOOL	g_impersonating_user = FALSE;
HANDLE	g_impersonation_token = 0;
DWORD	g_version_major;
DWORD	g_version_minor;
BOOL	g_is_win32s = FALSE;

#ifdef HORIZONLIVE
BOOL	g_nosettings_flag;
#endif

// Desktop access mask used by every OpenDesktop / OpenInputDesktop call.
#define VNC_DESKTOP_ACCESS \
	(DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | \
	 DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL | \
	 DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | \
	 DESKTOP_SWITCHDESKTOP | GENERIC_WRITE)

vncService::vncService()
{
	// Platform detection WITHOUT GetVersionEx.
	//
	// GetVersionEx is a Windows NT 3.5 / Windows 95 addition: neither NT 3.1
	// nor Win32s before 1.30 exports it, and an unresolvable import stops the
	// EXE loading before WinMain - with no diagnostic.  (The original also had
	// a real bug: "if (!GetVersionEx(...)) g_platform_id = 0;" was immediately
	// overwritten from the uninitialised structure.)
	//
	// GetVersion() has existed since the first Win32 and encodes everything we
	// need:  bit 31 set => not NT (i.e. Win32s or Win9x); low word => the
	// Windows version, which is 3.10/3.11 under Win32s, 4.x under Win9x and
	// 3.10 / 3.50 / 3.51 / 4.0 ... under NT.
	DWORD dwVersion = GetVersion();
	DWORD major = (DWORD)(LOBYTE(LOWORD(dwVersion)));
	DWORD minor = (DWORD)(HIBYTE(LOWORD(dwVersion)));

	g_version_major = major;
	g_version_minor = minor;

	if ((dwVersion & 0x80000000) == 0) {
		// Windows NT family.
		g_platform_id = VER_PLATFORM_WIN32_NT;
		g_is_win32s = FALSE;
	} else if (major < 4) {
		// Windows 3.1x hosting Win32s.
		g_platform_id = VER_PLATFORM_WIN32s;
		g_is_win32s = TRUE;
	} else {
		// Windows 95 or later 9x.
		g_platform_id = VER_PLATFORM_WIN32_WINDOWS;
		g_is_win32s = FALSE;
	}

#ifdef HORIZONLIVE
	g_nosettings_flag = false;
#endif
}

vncService::~vncService()
{
	if (g_impersonating_user) {
		g_impersonating_user = FALSE;
		if (g_impersonation_token != 0)
			CloseHandle(g_impersonation_token);
		g_impersonation_token = 0;
	}
}

#ifdef HORIZONLIVE
void
vncService::SetNoSettings(bool flag)
{
	g_nosettings_flag = flag;
}

BOOL
vncService::GetNoSettings()
{
	return g_nosettings_flag;
}

#endif


// GetCurrentUser - fills a buffer with the name of the current user.
//
// Win32s: Windows 3.1 is single-user and has no security database, so this
// returns "Default" - exactly what CurrentUser() below substitutes for an
// empty name, and the name vncProperties uses to build the per-user settings
// path.
//
// NT / Win9x: the original logic.  GetUserName and the window-station query
// go through Win32sApi so that they are not load-time imports.
BOOL
vncService::GetCurrentUser(char *buffer, UINT size)
{
	if (buffer == NULL || size == 0)
		return FALSE;

	if (g_is_win32s) {
		const char *name = "Default";
		if (strlen(name) >= size)
			return FALSE;
		strcpy(buffer, name);
		return TRUE;
	}

	// How to obtain the name of the current user depends upon the OS being used
	if ((g_platform_id == VER_PLATFORM_WIN32_NT) && vncService::RunningAsService())
	{
		// Windows NT, service-mode

		// -=- FIRSTLY - verify that a user is logged on.
		//
		// VC2: if the window-station API is missing we cannot tell, so carry on
		// to the impersonation check below as if a user were logged on.
		HWINSTA station = Win32sGetProcessWindowStation();
		if (station != NULL)
		{
			// Get the current user SID size
			DWORD usersize = 0;
			Win32sGetUserObjectInformation(station,
				UOI_USER_SID, NULL, 0, &usersize);

			// Check the required buffer size isn't zero
			if (usersize == 0)
			{
				// No user is logged in - ensure we're not impersonating anyone
				Win32sRevertToSelf();
				g_impersonating_user = FALSE;
				if (g_impersonation_token != 0)
					CloseHandle(g_impersonation_token);
				g_impersonation_token = 0;

				// Return "" as the name...
				strcpy(buffer, "");
				return TRUE;
			}
		}

		// -=- SECONDLY - a user is logged on but if we're not impersonating
		//     them then we can't continue!
		if (!g_impersonating_user) {
			strcpy(buffer, "");
			return TRUE;
		}
	}

	// -=- When we reach here, we're either running under Win9x, or we're running
	//     under NT as an application or as a service impersonating a user
	// Either way, we should find a suitable user name.

	switch (g_platform_id)
	{

	case VER_PLATFORM_WIN32_WINDOWS:
	case VER_PLATFORM_WIN32_NT:
		{
			DWORD length = size;

			if (Win32sGetUserName(buffer, &length) == 0)
			{
				UINT error = GetLastError();

				if (error == ERROR_NOT_LOGGED_ON)
				{
					// No user logged on
					strcpy(buffer, "");
					return TRUE;
				}
				else
				{
					// Genuine error...
					vnclog.Print(LL_INTERR, VNCLOG("GetUserName() failed, error=%d\n"), error);
					return FALSE;
				}
			}
		}
		return TRUE;
	};

	// OS was not recognised!
	return FALSE;
}

BOOL
vncService::CurrentUser(char *buffer, UINT size)
{
	BOOL result = GetCurrentUser(buffer, size);
	if (result && (strcmp(buffer, "") == 0) && !vncService::RunningAsService()) {
		strncpy(buffer, "Default", size);
		if (size > 0)
			buffer[size - 1] = '\0';
	}
	return result;
}

// IsWin95 - returns a BOOL indicating whether the current OS is Win95
BOOL
vncService::IsWin95()
{
	return (g_platform_id == VER_PLATFORM_WIN32_WINDOWS);
}

// IsWinNT - returns a bool indicating whether the current OS is WinNT
BOOL
vncService::IsWinNT()
{
	return (g_platform_id == VER_PLATFORM_WIN32_NT);
}

// IsWin32s - TRUE when running under Win32s on Windows 3.1x.
BOOL
vncService::IsWin32s()
{
	return g_is_win32s;
}

// Version info
DWORD
vncService::VersionMajor()
{
	return g_version_major;
}

DWORD
vncService::VersionMinor()
{
	return g_version_minor;
}

// Internal routine to find the WinVNC menu class window and
// post a message to it.
//
// FindWindow and PostMessage are core Windows 3.0 APIs, so this works on every
// platform.  It is the mechanism behind every command-line switch that talks to
// a running instance.
BOOL
PostToWinVNC(UINT message, WPARAM wParam, LPARAM lParam)
{
	// Locate the hidden WinVNC menu window
	HWND hservwnd = FindWindow(MENU_CLASS_NAME, NULL);
	if (hservwnd == NULL)
		return FALSE;

	// Post the message to WinVNC
	PostMessage(hservwnd, message, wParam, lParam);
	return TRUE;
}

// ==========================================================================
// DESKTOP SELECTION
//
// NT: the original window-station logic, through Win32sApi.
//
// Win32s, and NT without the desktop API: there is exactly one desktop, so
// these succeed trivially.  Do not make them return FALSE there: vncClient's
// main loop tests InputDesktopSelected() on every pass and drops the client if
// it is false, and vncDesktop::InitDesktop refuses to start if SelectDesktop
// fails.
// ==========================================================================

static BOOL
UseDesktopApi()
{
	return vncService::IsWinNT() && Win32sHaveDesktopApi();
}

// - SelectHDESK(HDESK)
// Switches the current thread into a different desktop by desktop handle.
// On success the old desktop handle is closed and new_desktop is kept.
BOOL
vncService::SelectHDESK(HDESK new_desktop)
{
	if (!UseDesktopApi())
		return TRUE;

	HDESK old_desktop = Win32sGetThreadDesktop(GetCurrentThreadId());

	DWORD dummy;
	char new_name[256];
	char old_name[256];

	if (!Win32sGetUserObjectInformation(new_desktop, UOI_NAME, &new_name, 256, &dummy)) {
		vnclog.Print(LL_INTERR, VNCLOG("GetUserObjectInformation() failed\n"));
		return FALSE;
	}

	vnclog.Print(LL_INTINFO, VNCLOG("SelectHDESK() to %s (%x) from %x\n"),
				 new_name, new_desktop, old_desktop);

	// VC2: already there?  With every window on this one thread (see the note
	// at the top), SetThreadDesktop fails once a window exists, even towards
	// the desktop the thread is already on - so do not try.
	if (old_desktop != NULL &&
		Win32sGetUserObjectInformation(old_desktop, UOI_NAME, &old_name, 256, &dummy) &&
		strcmp(old_name, new_name) == 0)
	{
		if (new_desktop != old_desktop && !Win32sCloseDesktop(new_desktop))
			vnclog.Print(LL_INTWARN, VNCLOG("SelectHDESK failed to close duplicate desktop handle\n"));
		return TRUE;
	}

	// Switch the desktop
	if (!Win32sSetThreadDesktop(new_desktop)) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to SetThreadDesktop(), error=%d\n"), GetLastError());
		return FALSE;
	}

	// Switched successfully - destroy the old desktop
	if (old_desktop != NULL && !Win32sCloseDesktop(old_desktop))
		vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK failed to close old desktop %x, error=%d\n"), old_desktop, GetLastError());

	return TRUE;
}

// - SelectDesktop(char *)
// Switches the current thread into a different desktop, by name
// Calling with a valid desktop name will place the thread in that desktop.
// Calling with a NULL name will place the thread in the current input desktop.
BOOL
vncService::SelectDesktop(char *name)
{
	if (!UseDesktopApi()) {
		// There is only one desktop; we are always on it.
		return TRUE;
	}

	HDESK desktop;

	if (name != NULL)
	{
		// Attempt to open the named desktop
		desktop = Win32sOpenDesktop(name, 0, FALSE, VNC_DESKTOP_ACCESS);
	}
	else
	{
		// No, so open the input desktop
		desktop = Win32sOpenInputDesktop(0, FALSE, VNC_DESKTOP_ACCESS);
	}

	// Did we succeed?
	if (desktop == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to open desktop, error=%d\n"), GetLastError());
		return FALSE;
	}

	// Switch to the new desktop
	if (!SelectHDESK(desktop)) {
		// Failed to enter the new desktop, so free it!
		vnclog.Print(LL_INTERR, VNCLOG("SelectDesktop() failed to select desktop\n"));
		if (!Win32sCloseDesktop(desktop))
			vnclog.Print(LL_INTERR, VNCLOG("SelectDesktop failed to close desktop, error=%d\n"), GetLastError());
		return FALSE;
	}

	// We successfully switched desktops!
	return TRUE;
}

// Establish whether we're on the current input desktop
BOOL
vncService::InputDesktopSelected()
{
	if (!UseDesktopApi()) {
		// The single desktop always is the input desktop.
		return TRUE;
	}

	// Get the input and thread desktops
	HDESK threaddesktop = Win32sGetThreadDesktop(GetCurrentThreadId());
	HDESK inputdesktop = Win32sOpenInputDesktop(0, FALSE, VNC_DESKTOP_ACCESS);

	if (inputdesktop == NULL) {
		// Returning TRUE on ERROR_BUSY fixes the bug #1109102.
		// FIXME: Probably this is not the most correct way to do it.
		return (GetLastError() == ERROR_BUSY) ? TRUE : FALSE;
	}

	DWORD dummy;
	char threadname[256];
	char inputname[256];

	if (!Win32sGetUserObjectInformation(threaddesktop, UOI_NAME, &threadname, 256, &dummy)) {
		if (!Win32sCloseDesktop(inputdesktop))
			vnclog.Print(LL_INTWARN, VNCLOG("failed to close input desktop\n"));
		return FALSE;
	}
	_ASSERT(dummy <= 256);
	if (!Win32sGetUserObjectInformation(inputdesktop, UOI_NAME, &inputname, 256, &dummy)) {
		if (!Win32sCloseDesktop(inputdesktop))
			vnclog.Print(LL_INTWARN, VNCLOG("failed to close input desktop\n"));
		return FALSE;
	}
	_ASSERT(dummy <= 256);

	if (!Win32sCloseDesktop(inputdesktop))
		vnclog.Print(LL_INTWARN, VNCLOG("failed to close input desktop\n"));

	if (strcmp(threadname, inputname) != 0)
		return FALSE;

	return TRUE;
}

// ==========================================================================
// CTRL+ALT+DEL
//
// NT: post WM_HOTKEY to the "SAS window" on the Winlogon desktop.  The thread
// doing that must switch to the Winlogon desktop, which it can only do if it
// has no windows or hooks - so it is a fresh worker thread.  The original used
// omni_thread::create; this tree's omnithread has no threads, so it is a raw
// CreateThread resolved at run time, and the caller waits for it.
//
// Win32s: there is no Winlogon (Ctrl+Alt+Del is handled by the BIOS/DOS layer
// and reboots the machine), and simulating a reboot on a remote user's behalf
// would be actively harmful.
// ==========================================================================

static DWORD WINAPI
SimulateCtrlAltDelThreadFn(LPVOID context)
{
	HDESK old_desktop = Win32sGetThreadDesktop(GetCurrentThreadId());

	// Switch into the Winlogon desktop
	if (!vncService::SelectDesktop("Winlogon"))
	{
		vnclog.Print(LL_INTERR, VNCLOG("failed to select logon desktop\n"));
		return 0;
	}

	vnclog.Print(LL_ALL, VNCLOG("generating ctrl-alt-del\n"));

	HWND hwndCtrlAltDel = FindWindow("SAS window class", "SAS window");
	if (hwndCtrlAltDel == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("\"SAS window\" not found\n"));
		hwndCtrlAltDel = HWND_BROADCAST;
	}

	PostMessage(hwndCtrlAltDel, WM_HOTKEY, 0, MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));

	// Switch back to our original desktop
	if (old_desktop != NULL)
		vncService::SelectHDESK(old_desktop);

	return 0;
}

BOOL
vncService::SimulateCtrlAltDel()
{
	if (!IsWinNT() || !Win32sHaveDesktopApi()) {
		vnclog.Print(LL_INTWARN,
			VNCLOG("Ctrl+Alt+Del is not supported on this platform\n"));
		return FALSE;
	}

	vnclog.Print(LL_ALL, VNCLOG("spawn ctrl-alt-del thread...\n"));

	DWORD threadId;
	HANDLE thread = Win32sCreateThread(NULL, 0, SimulateCtrlAltDelThreadFn,
									   NULL, 0, &threadId);
	if (thread == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to create ctrl-alt-del thread, error=%d\n"),
					 GetLastError());
		return FALSE;
	}

	// Wait for it, so the server code never runs on two threads at once.
	Win32sWaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	return TRUE;
}

// ==========================================================================
// WORKSTATION LOCKING
//
// LockWorkStation is Windows 2000 and later, so it is looked up by name on
// NT; Win32s and Win9x have nothing to lock.  (vncServer does not currently
// call this - its lock-on-disconnect handling is disabled in this tree.)
// ==========================================================================
BOOL
vncService::LockWorkstation()
{
	if (!IsWinNT()) {
		vnclog.Print(LL_INTWARN,
			VNCLOG("Workstation locking is not supported on this platform\n"));
		return FALSE;
	}

	vnclog.Print(LL_ALL, VNCLOG("locking workstation\n"));

	HMODULE user32 = GetModuleHandle("USER32");
	if (!user32) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to find User32 DLL (%u)\n"), GetLastError());
		return FALSE;
	}

	typedef BOOL (WINAPI *LWProc) (void);
	LWProc lockworkstation = (LWProc)GetProcAddress(user32, "LockWorkStation");
	if (!lockworkstation) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to locate LockWorkStation - requires Windows 2000 or above (%u)\n"), GetLastError());
		return FALSE;
	}

	BOOL result = (lockworkstation)();
	if (!result)
		vnclog.Print(LL_INTERR, VNCLOG("call to LockWorkstation failed\n"));
	return result;
}

// ==========================================================================
// MESSAGE-POSTING HELPERS
//
// FindWindow/PostMessage only - identical on every platform.
// ==========================================================================

BOOL
vncService::ShowProperties()
{
	if (!PostToWinVNC(MENU_PROPERTIES_SHOW, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted",
				   szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

// Helper for the -sharewindow switch: find a top-level window whose title
// contains the given substring.
//
// EnumWindows (not EnumDesktopWindows) is used, which exists on Win32s.
typedef struct _FindWindowByTitleData {
	char *substr;
	HWND hwnd;
} FindWindowByTitleData;

static BOOL CALLBACK
FindWindowByTitleEnumProc(HWND hwnd, LPARAM lParam)
{
	FindWindowByTitleData *pData = (FindWindowByTitleData *)lParam;
	if (pData == NULL)
		return FALSE;

	char title[256];
	title[0] = '\0';
	if (GetWindowText(hwnd, title, sizeof(title) - 1) > 0) {
		title[sizeof(title) - 1] = '\0';

		// Case-insensitive substring search, on a copy so the caller's buffer
		// is not modified.
		char lower[256];
		strcpy(lower, title);
		int i;
		for (i = 0; lower[i] != '\0'; i++)
			lower[i] = (char)tolower((unsigned char)lower[i]);

		if (strstr(lower, pData->substr) != NULL) {
			pData->hwnd = hwnd;
			return FALSE;		// stop enumerating
		}
	}
	return TRUE;
}

HWND
vncService::FindWindowByTitle(char *substr)
{
	if (substr == NULL)
		return NULL;

	char lowerSubstr[256];
	strncpy(lowerSubstr, substr, sizeof(lowerSubstr) - 1);
	lowerSubstr[sizeof(lowerSubstr) - 1] = '\0';
	int i;
	for (i = 0; lowerSubstr[i] != '\0'; i++)
		lowerSubstr[i] = (char)tolower((unsigned char)lowerSubstr[i]);

	FindWindowByTitleData data;
	data.substr = lowerSubstr;
	data.hwnd = NULL;

	// The old SDKs declare EnumWindows' callback as a parameterless FARPROC in
	// a non-STRICT build, so the cast is required.
	EnumWindows((WNDENUMPROC)FindWindowByTitleEnumProc, (LPARAM)&data);

	if (data.hwnd == NULL) {
		MessageBox(NULL, "Unable to find a window with the specified title.",
				   szAppName, MB_ICONEXCLAMATION | MB_OK);
	}
	return data.hwnd;
}

BOOL
vncService::PostShareAll()
{
	if (!PostToWinVNC(MENU_SERVER_SHAREALL, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::PostSharePrimary()
{
	if (!PostToWinVNC(MENU_SERVER_SHAREPRIMARY, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::PostShareArea(unsigned short x, unsigned short y,
						  unsigned short w, unsigned short h)
{
	if (!PostToWinVNC(MENU_SERVER_SHAREAREA,
					  MAKEWPARAM(x,y), MAKELPARAM(w,h))) {
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::PostShareWindow(HWND hwnd)
{
	if (!PostToWinVNC(MENU_SERVER_SHAREWINDOW, (WPARAM)hwnd, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::ShowDefaultProperties()
{
	if (!PostToWinVNC(MENU_DEFAULT_PROPERTIES_SHOW, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::ShowAboutBox()
{
	if (!PostToWinVNC(MENU_ABOUTBOX_SHOW, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::PostAddNewClient(unsigned long ipaddress, unsigned short port)
{
	if (!PostToWinVNC(MENU_ADD_CLIENT_MSG, (WPARAM)port, (LPARAM)ipaddress))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL
vncService::KillAllClients()
{
	if (!PostToWinVNC(MENU_KILL_ALL_CLIENTS_MSG, 0, 0))
	{
		MessageBox(NULL, "No existing instance of WinVNC could be contacted", szAppName, MB_ICONEXCLAMATION | MB_OK);
		return FALSE;
	}

	return TRUE;
}

// SERVICE-MODE ROUTINES

// Service-mode defines:

// Executable name
#define VNCAPPNAME            "winvnc"

// Internal service name
#define VNCSERVICENAME        "winvnc"

// Displayed service name
#define VNCSERVICEDISPLAYNAME "VNC Server"

// List of other required services ("dependency 1\0dependency 2\0\0")
// *** These need filling in properly
#define VNCDEPENDENCIES       ""

// Internal service state
SERVICE_STATUS          g_srvstatus;       // current status of the service
SERVICE_STATUS_HANDLE   g_hstatus;
DWORD                   g_error = 0;
DWORD					g_servicethread = 0;

// Forward defines of internal service functions
void WINAPI ServiceMain(DWORD argc, char **argv);
void ServiceStop();
void WINAPI ServiceCtrl(DWORD ctrlcode);
BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);

// Shown by every service switch on Win32s.
static void
ServiceNotAvailable(const char *what, BOOL silent)
{
	if (silent)
		return;

	char msg[512];
	sprintf(msg, "%s is not available on this version of Windows.", what);
	MessageBox(NULL, msg, szAppName, MB_ICONINFORMATION | MB_OK);
}

// ROUTINE TO QUERY WHETHER THIS PROCESS IS RUNNING AS A SERVICE OR NOT

BOOL	g_servicemode = FALSE;

BOOL
vncService::RunningAsService()
{
	return g_servicemode;
}

BOOL
vncService::KillRunningCopy()
{
	// Post WM_CLOSE to every running instance and wait for it to go away.
	//
	// The loop is bounded (the original's was an infinite hang if the target
	// ignored WM_CLOSE), and omni_thread::sleep pumps messages in this build,
	// which a cooperatively scheduled Win32s system needs.
	HWND hservwnd;
	int attempts = 0;

	while ((hservwnd = FindWindow(MENU_CLASS_NAME, NULL)) != NULL)
	{
		if (attempts++ >= 10) {
			vnclog.Print(LL_INTERR,
				VNCLOG("running copy did not exit after %d attempts\n"), attempts);
			return FALSE;
		}

		PostMessage(hservwnd, WM_CLOSE, 0, 0);
		omni_thread::sleep(1);
	}

	return TRUE;
}


// ROUTINE TO POST THE HANDLE OF THE CURRENT USER TO THE RUNNING WINVNC, IN ORDER
// THAT IT CAN LOAD THE APPROPRIATE SETTINGS.  THIS IS USED ONLY BY THE SVCHELPER
// OPTION, WHEN RUNNING UNDER NT
BOOL
vncService::PostUserHelperMessage()
{
	// - Check the platform type
	if (!IsWinNT())
		return TRUE;

	// - Get the current process ID
	DWORD processId = GetCurrentProcessId();

	// - Post it to the existing WinVNC
	int retries = 6;
	while (!PostToWinVNC(MENU_SERVICEHELPER_MSG, 0, (LPARAM)processId) && retries--)
		omni_thread::sleep(10);

	// - Wait until it's been used
	omni_thread::sleep(5);

	return retries;
}

BOOL
vncService::PostReloadMessage()
{
	// Win32s: a plain post - there is no service to retry against.
	if (!IsWinNT())
		return PostToWinVNC(MENU_RELOAD_MSG, 0, 0);

	// - Get the current process ID
	DWORD processId = GetCurrentProcessId();

	// - Post it to the existing WinVNC
	// FIXME: Code duplication, see PostUserHelperMessage().
	int retries = 6;
	while (!PostToWinVNC(MENU_RELOAD_MSG, 0, (LPARAM)processId) && retries--)
		omni_thread::sleep(10);

	// - Wait until it's been used
	omni_thread::sleep(5);

	return retries;
}


// ROUTINE TO PROCESS AN INCOMING INSTANCE OF THE ABOVE MESSAGE
BOOL
vncService::ProcessUserHelperMessage(DWORD processId) {
	// - Check the platform type
	if (!IsWinNT() || !vncService::RunningAsService())
		return TRUE;

	// - Close the HKEY_CURRENT_USER key, to force NT to reload it for the new user
	// NB: Note that this is _really_ dodgy if ANY other thread is accessing the key!
	if (RegCloseKey(HKEY_CURRENT_USER) != ERROR_SUCCESS) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to close current registry hive\n"));
		return FALSE;
	}

	// - Revert to our own identity
	Win32sRevertToSelf();
	g_impersonating_user = FALSE;
	if (g_impersonation_token != 0)
		CloseHandle(g_impersonation_token);
	g_impersonation_token = 0;

	// - Open the specified process
	HANDLE processHandle = Win32sOpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
	if (processHandle == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to open specified process, error=%d\n"),
					 GetLastError());
		return FALSE;
	}

	// - Get the token for the given process
	HANDLE userToken = NULL;
	if (!Win32sOpenProcessToken(processHandle, TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &userToken)) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to get user token, error=%d\n"),
					 GetLastError());
		CloseHandle(processHandle);
		return FALSE;
	}
	CloseHandle(processHandle);

	// - Set this thread to impersonate them.  ImpersonateLoggedOnUser is NT
	//   3.51+, so on NT 3.1 / 3.5 this fails and the service keeps using the
	//   default settings.
	if (!Win32sImpersonateLoggedOnUser(userToken)) {
		vnclog.Print(LL_INTERR, VNCLOG("failed to impersonate user, error=%d\n"),
					 GetLastError());
		CloseHandle(userToken);
		return FALSE;
	}

	g_impersonating_user = TRUE;
	g_impersonation_token = userToken;
	vnclog.Print(LL_INTINFO, VNCLOG("impersonating logged on user\n"));
	return TRUE;
}

bool
vncService::tryImpersonate()
{
	// IMPORTANT: must return true everywhere except an NT service.
	//
	// Every file-transfer handler in vncClient.cpp is written as
	//
	//     if (!vncService::tryImpersonate()) { ...send failure to client...; break; }
	//
	// so a false return disables file transfer completely.  On Win32s there is
	// no security model, and an NT application already runs as the user.
	if (!IsWinNT() || !vncService::RunningAsService())
		return true;

	if (!g_impersonating_user) {
		vnclog.Print(LL_INTERR, VNCLOG("impersonation failure, user unknown\n"));
		return false;
	}
	if (!Win32sImpersonateLoggedOnUser(g_impersonation_token)) {
		vnclog.Print(LL_INTERR, VNCLOG("user impersonation failure, error=%d\n"),
					 GetLastError());
		return false;
	}

	vnclog.Print(LL_INTINFO, VNCLOG("impersonated logged on user\n"));
	return true;
}

void
vncService::undoImpersonate()
{
	// Only an NT service ever impersonates; elsewhere there is nothing to undo.
	if (!IsWinNT() || !vncService::RunningAsService())
		return;

	Win32sRevertToSelf();
	vnclog.Print(LL_INTINFO, VNCLOG("reverted impersonation\n"));
}

// SERVICE MAIN ROUTINE
int
vncService::WinVNCServiceMain()
{
	typedef DWORD (WINAPI * RegisterServiceProc)(DWORD, DWORD);
	const ULONG RSP_SIMPLE_SERVICE = 0x00000001;
	const ULONG RSP_UNREGISTER_SERVICE = 0x00000000;

	if (g_is_win32s) {
		MessageBox(NULL,
			"Service mode is not available on this version of Windows.\r\n\r\n"
			"Run WinVNC normally instead - it will appear as an ordinary "
			"application.",
			szAppName, MB_ICONINFORMATION | MB_OK);
		return 1;
	}

	g_servicemode = TRUE;

	// How to run as a service depends upon the OS being used
	switch (g_platform_id)
	{

		// Windows 95/98
	case VER_PLATFORM_WIN32_WINDOWS:
		{
			// Obtain a handle to the kernel library
			HINSTANCE kerneldll = GetModuleHandle("KERNEL32");
			if (kerneldll == NULL)
				break;

			// And find the RegisterServiceProcess function
			RegisterServiceProc RegisterService;
			RegisterService = (RegisterServiceProc) GetProcAddress(kerneldll, "RegisterServiceProcess");
			if (RegisterService == NULL)
				break;

			// Register this process with the OS as a service!
			RegisterService(NULL, RSP_SIMPLE_SERVICE);

			// Run the service itself
			WinVNCAppMain();

			// Then remove the service from the system service table
			RegisterService(NULL, RSP_UNREGISTER_SERVICE);
		}
		break;

		// Windows NT
	case VER_PLATFORM_WIN32_NT:
		{
			// Create a service entry table
			SERVICE_TABLE_ENTRY dispatchTable[] =
		    {
				{VNCSERVICENAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
				{NULL, NULL}
			};

			// Call the service control dispatcher with our entry table.
			// It returns once ServiceMain (below) has finished.
			if (!Win32sStartServiceCtrlDispatcher(dispatchTable))
				LogErrorMsg("StartServiceCtrlDispatcher failed.");
		}
		break;

	};

	return 0;
}

// SERVICE MAIN ROUTINE
//
// VC2: runs the server on THIS thread (the SCM created it for us) rather than
// spawning a work thread - see the threading note at the top of the file.
void WINAPI ServiceMain(DWORD argc, char**argv)
{
	// Register the service control handler
	g_hstatus = Win32sRegisterServiceCtrlHandler(VNCSERVICENAME, ServiceCtrl);

	if (g_hstatus == 0)
		return;

	// Set up some standard service state values
	g_srvstatus.dwServiceType = SERVICE_WIN32 | SERVICE_INTERACTIVE_PROCESS;
	g_srvstatus.dwServiceSpecificExitCode = 0;

	// Give this status to the SCM
	if (!ReportStatus(
		SERVICE_START_PENDING,	// Service state
		NO_ERROR,				// Exit code type
		15000))					// Hint as to how long WinVNC should have hung before you assume error
	{
		ReportStatus(
			SERVICE_STOPPED,
			g_error,
			0);
		return;
	}

	// Save the current thread identifier, for ServiceStop's WM_QUIT
	g_servicethread = GetCurrentThreadId();

	if (!ReportStatus(
		SERVICE_RUNNING,       // service state
		NO_ERROR,              // exit code
		0))                    // wait hint
	{
		g_servicethread = 0;
		return;
	}

	// RUN!
	WinVNCAppMain();

	// Mark that we're no longer running
	g_servicethread = 0;

	// Tell the service manager that we've stopped.
	ReportStatus(
		SERVICE_STOPPED,
		g_error,
		0);
}

// SERVICE STOP ROUTINE - post a quit message to the server thread.
// Called from ServiceCtrl, i.e. on the dispatcher thread.
void ServiceStop()
{
	if (g_servicethread != 0)
	{
		vnclog.Print(LL_INTINFO, VNCLOG("quitting from ServiceStop\n"));
		Win32sPostThreadMessage(g_servicethread, WM_QUIT, 0, 0);
	}
}

// SERVICE INSTALL ROUTINE
int
vncService::ReinstallService(BOOL silent) {
	if (g_is_win32s) {
		ServiceNotAvailable("Service installation", silent);
		return 1;
	}
	RemoveService(1);
	InstallService(silent);
	return 0;
}

int
vncService::InstallService(BOOL silent)
{
	if (g_is_win32s) {
		if (!silent) {
			MessageBox(NULL,
				"Service installation is not available on this version of "
				"Windows.\r\n\r\n"
				"To start WinVNC automatically, put a shortcut to it in your "
				"Startup group instead.",
				szAppName, MB_ICONINFORMATION | MB_OK);
		}
		return 1;
	}

	const int pathlength = 2048;
	char path[pathlength];
	char servicecmd[pathlength];

	// Get the filename of this executable
	if (GetModuleFileName(NULL, path, pathlength-(strlen(winvncRunService)+2)) == 0) {
		if (!silent) {
			MessageBox(NULL, "Unable to install WinVNC service", szAppName, MB_ICONEXCLAMATION | MB_OK);
		}
		return 0;
	}

	// Append the service-start flag to the end of the path:
	if (strlen(path) + 4 + strlen(winvncRunService) < pathlength)
		sprintf(servicecmd, "\"%s\" %s", path, winvncRunService);
	else
		return 0;

	// How to add the WinVNC service depends upon the OS
	switch (g_platform_id)
	{

		// Windows 95/98
	case VER_PLATFORM_WIN32_WINDOWS:
		{
			// Locate the RunService registry entry
			HKEY runservices;
			if (RegCreateKey(HKEY_LOCAL_MACHINE,
				"Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
				&runservices) != ERROR_SUCCESS)
			{
				if (!silent) {
					MessageBox(NULL, "The SCM could not be contacted - the WinVNC service was not installed", szAppName, MB_ICONEXCLAMATION | MB_OK);
				}
				break;
			}

			// Attempt to add a WinVNC key
			if (RegSetValueEx(runservices, szAppName, 0, REG_SZ, (unsigned char *)servicecmd, strlen(servicecmd)+1) != ERROR_SUCCESS)
			{
				RegCloseKey(runservices);
				if (!silent) {
					MessageBox(NULL, "The WinVNC service could not be registered", szAppName, MB_ICONEXCLAMATION | MB_OK);
				}
				break;
			}

			RegCloseKey(runservices);

			// We have successfully installed the service!
			if (!silent) {
				vncTimedMsgBox::Do(
					"The WinVNC service was successfully installed\n"
					"The service will start now and will automatically\n"
					"be run the next time this machine is reset",
					szAppName,
					MB_ICONINFORMATION | MB_OK);
			}

			// Run the service...
			STARTUPINFO si;
			memset(&si, 0, sizeof(si));
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;
			if (!CreateProcess(
				NULL, servicecmd,							// Program name & path
				NULL, NULL,									// Security attributes
				FALSE,										// Inherit handles?
				NORMAL_PRIORITY_CLASS,						// Extra startup flags
				NULL,										// Environment table
				NULL,										// Current directory
				&si,
				&pi
				))
			{
				if (!silent) {
					MessageBox(NULL, "The WinVNC service failed to start",
							   szAppName, MB_ICONSTOP | MB_OK);
				}
				break;
			}
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		break;

		// Windows NT
	case VER_PLATFORM_WIN32_NT:
		{
			SC_HANDLE   hservice;
			SC_HANDLE   hsrvmanager;

			// Open the default, local Service Control Manager database
			hsrvmanager = Win32sOpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (hsrvmanager == NULL)
			{
				if (!silent) {
					MessageBox(NULL,
						"The Service Control Manager could not be contacted - the WinVNC service was not registered",
						szAppName,
						MB_ICONEXCLAMATION | MB_OK);
				}
				break;
			}

			// Create an entry for the WinVNC service
			hservice = Win32sCreateService(
				hsrvmanager,				// SCManager database
				VNCSERVICENAME,				// name of service
				VNCSERVICEDISPLAYNAME,		// name to display
				SERVICE_ALL_ACCESS,			// desired access
				SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
											// service type
				SERVICE_AUTO_START,			// start type
				SERVICE_ERROR_NORMAL,		// error control type
				servicecmd,					// service's binary
				NULL,						// no load ordering group
				NULL,						// no tag identifier
				VNCDEPENDENCIES,			// dependencies
				NULL,						// LocalSystem account
				NULL);						// no password

			// VC2: an SCM that predates interactive services rejects the
			// SERVICE_INTERACTIVE_PROCESS bit as an invalid parameter.  Register
			// a plain own-process service instead; it then runs on the service
			// desktop rather than the user's, which is the best that SCM offers.
			if (hservice == NULL && GetLastError() == ERROR_INVALID_PARAMETER)
			{
				vnclog.Print(LL_INTWARN,
					VNCLOG("SCM rejected SERVICE_INTERACTIVE_PROCESS - registering without it\n"));
				hservice = Win32sCreateService(
					hsrvmanager, VNCSERVICENAME, VNCSERVICEDISPLAYNAME,
					SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
					SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, servicecmd,
					NULL, NULL, VNCDEPENDENCIES, NULL, NULL);
			}

			if (hservice == NULL)
			{
				DWORD error = GetLastError();
				if (!silent) {
					if (error == ERROR_SERVICE_EXISTS) {
						MessageBox(NULL,
							"The WinVNC service is already registered",
							szAppName,
							MB_ICONEXCLAMATION | MB_OK);
					} else {
						MessageBox(NULL,
							"The WinVNC service could not be registered",
							szAppName,
							MB_ICONEXCLAMATION | MB_OK);
					}
				}
 				Win32sCloseServiceHandle(hsrvmanager);
				break;
			}
			Win32sCloseServiceHandle(hsrvmanager);
			Win32sCloseServiceHandle(hservice);

			// Now install the servicehelper registry setting...
			// Locate the RunService registry entry
			HKEY runapps;
			if (RegCreateKey(HKEY_LOCAL_MACHINE,
				"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
				&runapps) != ERROR_SUCCESS)
			{
				if (!silent) {
					MessageBox(NULL, "WARNING:Unable to install the ServiceHelper hook\nGlobal user-specific registry settings will not be loaded", szAppName, MB_ICONEXCLAMATION | MB_OK);
				}
			} else {
				char servicehelpercmd[pathlength];

				// Append the service-helper-start flag to the end of the path:
				if (strlen(path) + 4 + strlen(winvncRunServiceHelper) < pathlength)
					sprintf(servicehelpercmd, "\"%s\" %s", path, winvncRunServiceHelper);
				else {
					RegCloseKey(runapps);
					return 0;
				}

				// Add the VNCserviceHelper entry
				if (RegSetValueEx(runapps, szAppName, 0, REG_SZ,
					(unsigned char *)servicehelpercmd, strlen(servicehelpercmd)+1) != ERROR_SUCCESS)
				{
					if (!silent) {
						MessageBox(NULL, "WARNING:Unable to install the ServiceHelper hook\nGlobal user-specific registry settings will not be loaded", szAppName, MB_ICONEXCLAMATION | MB_OK);
					}
				}
				RegCloseKey(runapps);
			}

			// Everything went fine
			if (!silent) {
				vncTimedMsgBox::Do(
					"The WinVNC service was successfully registered\n"
					"The service may be started from the Control Panel, and will\n"
					"automatically be run the next time this machine is reset",
					szAppName,
					MB_ICONINFORMATION | MB_OK);
			}
		}
		break;
	};

	return 0;
}

// SERVICE REMOVE ROUTINE
int
vncService::RemoveService(BOOL silent)
{
	if (g_is_win32s) {
		ServiceNotAvailable("Service removal", silent);
		return 1;
	}

	// How to remove the WinVNC service depends upon the OS
	switch (g_platform_id)
	{

		// Windows 95/98
	case VER_PLATFORM_WIN32_WINDOWS:
		{
			// Locate the RunService registry entry
			HKEY runservices;
			if (RegOpenKey(HKEY_LOCAL_MACHINE,
				"Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
				&runservices) != ERROR_SUCCESS)
			{
				if (!silent) {
					MessageBox(NULL, "The Service Control Manager could not be contacted - the WinVNC service was not unregistered", szAppName, MB_ICONEXCLAMATION | MB_OK);
				}
			}
			else
			{
				// Attempt to delete the WinVNC key
				if (Win32sRegDeleteValue(runservices, szAppName) != ERROR_SUCCESS)
				{
					if (!silent) {
						MessageBox(NULL, "The WinVNC service could not be unregistered", szAppName, MB_ICONEXCLAMATION | MB_OK);
					}
				}

				RegCloseKey(runservices);
				break;
			}

			// Try to kill any running copy of WinVNC
			if (!KillRunningCopy())
			{
				if (!silent) {
					MessageBox(NULL,
						"The WinVNC service could not be contacted",
						szAppName,
						MB_ICONEXCLAMATION | MB_OK);
				}
				break;
			}

			// We have successfully removed the service!
			if (!silent) {
				vncTimedMsgBox::Do("The WinVNC service has been unregistered", szAppName, MB_ICONINFORMATION | MB_OK);
			}
		}
		break;

		// Windows NT
	case VER_PLATFORM_WIN32_NT:
		{
			SC_HANDLE   hservice;
			SC_HANDLE   hsrvmanager;

			// Attempt to remove the service-helper hook
			HKEY runapps;
			if (RegOpenKey(HKEY_LOCAL_MACHINE,
				"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
				&runapps) == ERROR_SUCCESS)
			{
				// Attempt to delete the WinVNC key
				if (Win32sRegDeleteValue(runapps, szAppName) != ERROR_SUCCESS)
				{
					if (!silent) {
						MessageBox(NULL, "WARNING:The ServiceHelper hook entry could not be removed from the registry", szAppName, MB_ICONEXCLAMATION | MB_OK);
					}
				}
				RegCloseKey(runapps);
			}

			// Open the SCM
			hsrvmanager = Win32sOpenSCManager(
						NULL,                   // machine (NULL == local)
						NULL,                   // database (NULL == default)
						SC_MANAGER_ALL_ACCESS   // access required
						);
			if (hsrvmanager)
			{
				hservice = Win32sOpenService(hsrvmanager, VNCSERVICENAME, SERVICE_ALL_ACCESS);

				if (hservice != NULL)
				{
					SERVICE_STATUS status;

					// Try to stop the WinVNC service
					if (Win32sControlService(hservice, SERVICE_CONTROL_STOP, &status))
					{
						while(Win32sQueryServiceStatus(hservice, &status))
						{
							if (status.dwCurrentState == SERVICE_STOP_PENDING)
								Sleep(1000);
							else
								break;
						}

						if (status.dwCurrentState != SERVICE_STOPPED) {
							if (!silent) {
								MessageBox(NULL, "The WinVNC service could not be stopped", szAppName, MB_ICONEXCLAMATION | MB_OK);
							}
						}
					}

					// Now remove the service from the SCM
					if (Win32sDeleteService(hservice)) {
						if (!silent) {
							vncTimedMsgBox::Do("The WinVNC service has been unregistered", szAppName, MB_ICONINFORMATION | MB_OK);
						}
					} else {
						DWORD error = GetLastError();
						if (error == ERROR_SERVICE_MARKED_FOR_DELETE) {
							if (!silent)
								MessageBox(NULL, "The WinVNC service is already marked to be unregistered", szAppName, MB_ICONEXCLAMATION | MB_OK);
						} else {
							if (!silent)
								MessageBox(NULL, "The WinVNC service could not be unregistered", szAppName, MB_ICONEXCLAMATION | MB_OK);
						}
					}
					Win32sCloseServiceHandle(hservice);
				}
				else if (!silent)
					MessageBox(NULL, "The WinVNC service could not be found", szAppName, MB_ICONEXCLAMATION | MB_OK);

				Win32sCloseServiceHandle(hsrvmanager);
			}
			else if (!silent)
				MessageBox(NULL, "The Service Control Manager could not be contacted - the WinVNC service was not unregistered", szAppName, MB_ICONEXCLAMATION | MB_OK);
		}
		break;
	};
	return 0;
}

// USEFUL SERVICE SUPPORT ROUTINES

// Service control routine (dispatcher thread)
void WINAPI ServiceCtrl(DWORD ctrlcode)
{
	// What control code have we been sent?
	switch(ctrlcode)
	{

	case SERVICE_CONTROL_STOP:
		// STOP : The service must stop
		g_srvstatus.dwCurrentState = SERVICE_STOP_PENDING;
		ServiceStop();
		break;

	case SERVICE_CONTROL_INTERROGATE:
		// QUERY : Service control manager just wants to know our state
		break;

	default:
		// Control code not recognised
		break;

	}

	// Tell the control manager what we're up to.
	ReportStatus(g_srvstatus.dwCurrentState, NO_ERROR, 0);
}

// Service manager status reporting
BOOL ReportStatus(DWORD state,
				  DWORD exitcode,
				  DWORD waithint)
{
	static DWORD checkpoint = 1;
	BOOL result = TRUE;

	// If we're in the start state then we don't want the control manager
	// sending us control messages because they'll confuse us.
	if (state == SERVICE_START_PENDING)
		g_srvstatus.dwControlsAccepted = 0;
	else
		g_srvstatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	// Save the new status we've been given
	g_srvstatus.dwCurrentState = state;
	g_srvstatus.dwWin32ExitCode = exitcode;
	g_srvstatus.dwWaitHint = waithint;

	// Update the checkpoint variable to let the SCM know that we
	// haven't died if requests take a long time
	if ((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED))
		g_srvstatus.dwCheckPoint = 0;
	else
		g_srvstatus.dwCheckPoint = checkpoint++;

	// Tell the SCM our new status
	if (!(result = Win32sSetServiceStatus(g_hstatus, &g_srvstatus)))
		LogErrorMsg("SetServiceStatus failed");

	return result;
}

// Error reporting.
//
// Always goes to the VNC log.  On NT it also goes to the event log, as in the
// original; there is no event log on Win32s or Win9x, and the wrappers simply
// fail there.
void
LogErrorMsg(char *message)
{
	if (message == NULL)
		message = "(no message)";

	// Save the error code
	g_error = GetLastError();

	vnclog.Print(LL_INTERR, VNCLOG("%s (error %d)\n"), message, (int)g_error);

	if (!vncService::IsWinNT())
		return;

	char	msgbuff[256];
	LPCSTR	strings[2];

	// Use event logging to log the error
	HANDLE heventsrc = Win32sRegisterEventSource(NULL, VNCSERVICENAME);

	sprintf(msgbuff, "%.200s error: %d", VNCSERVICENAME, g_error);
	strings[0] = msgbuff;
	strings[1] = message;

	if (heventsrc != NULL)
	{
		MessageBeep(MB_OK);

		Win32sReportEvent(
			heventsrc,				// handle of event source
			EVENTLOG_ERROR_TYPE,	// event type
			0,						// event category
			0,						// event ID
			NULL,					// current user's SID
			2,						// strings in 'strings'
			0,						// no bytes of raw data
			strings,				// array of error strings
			NULL);					// no raw data

		Win32sDeregisterEventSource(heventsrc);
	}
}
