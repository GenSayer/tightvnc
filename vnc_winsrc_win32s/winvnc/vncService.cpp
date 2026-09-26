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
// vncService - WIN32S / WINDOWS 3.1 VERSION
// ==========================================================================
//
// The original of this file is 1426 lines of Windows NT service and
// desktop-switching code.  The full NT version is preserved as
// vncService.cpp.nt-original for reference; this is a deliberate rewrite for
// the Win32s target.
//
// WHAT WAS REMOVED, AND WHY IT CANNOT BE WRAPPED
//
// These are not missing-API problems that a GetProcAddress shim can paper over.
// The underlying operating-system concepts do not exist on Windows 3.1:
//
//  1. SERVICES.  OpenSCManager, CreateService, DeleteService,
//     StartServiceCtrlDispatcher, SetServiceStatus, RegisterServiceCtrlHandler.
//     Windows 3.1 has no service control manager and no notion of a background
//     process that outlives the logged-on session.  WinVNCServiceMain,
//     InstallService, ReinstallService, RemoveService and the whole
//     ServiceMain/ServiceCtrl/ReportStatus machinery are stubs that report the
//     feature is unavailable.
//
//  2. WINDOW STATIONS AND DESKTOPS.  OpenDesktop, CloseDesktop,
//     OpenInputDesktop, GetThreadDesktop, SetThreadDesktop, SwitchDesktop,
//     GetUserObjectInformation, GetProcessWindowStation.  Windows 3.1 has
//     exactly one desktop and no window stations.  SelectDesktop/SelectHDESK
//     succeed trivially, and InputDesktopSelected always returns TRUE - which
//     is correct, because the one desktop always IS the input desktop.  This
//     matters: vncClient's main loop calls InputDesktopSelected() on every
//     iteration and disconnects the client if it returns FALSE.
//
//  3. SECURITY TOKENS AND IMPERSONATION.  ImpersonateLoggedOnUser,
//     RevertToSelf, OpenProcessToken, DuplicateToken.  Windows 3.1 is
//     single-user with no security model at all.  tryImpersonate() returns
//     true (meaning "proceed"), undoImpersonate() does nothing.  This is
//     load-bearing for FILE TRANSFER: every file-transfer message handler in
//     vncClient.cpp begins with "if (!vncService::tryImpersonate()) { ...send
//     failure... }", so a tryImpersonate() that returned false would break file
//     transfer entirely while appearing to be a permissions problem.
//
//  4. CTRL+ALT+DEL SIMULATION.  Requires the NT winlogon desktop.  Stub.
//
//  5. WORKSTATION LOCKING.  LockWorkstation is NT 4.0+; Windows 3.1 has
//     nothing to lock.  Stub returning FALSE.
//
//  6. GetUserName.  Present in the Win32s ADVAPI32 stub set but returns
//     nothing useful on a single-user system, and it is one more import to
//     resolve.  Replaced with a fixed "Default", which is exactly what
//     CurrentUser() already substituted when GetCurrentUser returned an empty
//     string.  The registry per-user settings path therefore resolves the same
//     way it did before.
//
// WHAT IS KEPT AND STILL FUNCTIONAL
//
//   PostToWinVNC and every Post*/Show*/Kill* helper.  These are pure
//   FindWindow + PostMessage, work identically on Win32s, and are how the
//   command-line switches (-showstatus, -connect, -killclients, ...) reach a
//   running instance.
//
//   IsWin95/IsWinNT/VersionMajor/VersionMinor, plus a new IsWin32s().  Several
//   callers branch on these; see the note on the constructor about how the
//   platform is detected without GetVersionEx.
//
//   FindWindowByTitle, for the -sharewindow switch.
//
//   KillRunningCopy, with its sleep loop replaced by a bounded message pump.
// ==========================================================================

#include "stdhdrs.h"

// WIN32S: <lmcons.h> is the LAN Manager header, included by the original solely
// for UNLEN (the maximum user name length).  It is not part of the MSVC 4.1
// core SDK and pulls in netapi declarations we do not want.  UNLEN is defined
// locally below instead.
// #include <lmcons.h>

// vncService.h declares the class this file implements.  It MUST be included -
// omitting it was my mistake when rewriting this file, and it produced a wall of
//     error C2653: 'vncService' : is not a class or namespace name
// plus, at the "vncService init;" line, C2501/C2239/C2061 - MSVC parses an
// unknown type name in a declaration as an implicit int, so "vncService init;"
// became "int init;" with a stray identifier after it.
//
// Nothing else in this file's include chain pulls it in transitively (checked:
// stdhdrs.h, WinVNC.h, vncMenu.h, vncTimedMsgBox.h, Win32sApi.h).
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

vncService::vncService()
{
	// Platform detection WITHOUT GetVersionEx.
	//
	// The original called GetVersionEx(&osversioninfo) and used dwPlatformId.
	// Two problems on this target:
	//
	//  * GetVersionEx is a Windows NT 3.5 / Windows 95 addition.  Win32s 1.30
	//    does export it, but earlier Win32s does not, and an unresolvable
	//    import stops the EXE loading before WinMain - with no diagnostic.
	//
	//  * The original also had a real bug: "if (!GetVersionEx(...))
	//    g_platform_id = 0;" and then unconditionally overwrote g_platform_id
	//    from the (uninitialised) structure on the next line, so the failure
	//    path did nothing.
	//
	// GetVersion() has existed since the first Win32 and encodes everything we
	// need:  bit 31 set => not NT (i.e. Win32s or Win9x); low word => the
	// Windows version, which is 3.10/3.11 under Win32s and 4.x under Win9x.
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
	// No impersonation token is ever acquired on this platform, but keep the
	// teardown symmetrical in case the NT paths are ever restored.
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
// WIN32S: Windows 3.1 is single-user and has no security database, so there is
// no user name to report.  The original walked a maze of NT window-station and
// impersonation checks and then called GetUserName().
//
// We return "Default" directly.  That is not a placeholder chosen at random: it
// is exactly what CurrentUser() below already substituted whenever
// GetCurrentUser() produced an empty string, and it is the name vncProperties
// uses to build the per-user registry path.  So the settings written and read
// on Win32s land in the same place they always did for an unidentified user.
BOOL
vncService::GetCurrentUser(char *buffer, UINT size)
{
	if (buffer == NULL || size == 0)
		return FALSE;

	const char *name = "Default";
	if (strlen(name) >= size)
		return FALSE;

	strcpy(buffer, name);
	return TRUE;
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
//
// New in this port.  Several places need to know this specifically rather than
// just "not NT": the tray icon, the wallpaper handling, and the polling
// strategy all differ.
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
// Works unchanged on Win32s: FindWindow and PostMessage are core Windows 3.0
// APIs.  This is the mechanism behind every command-line switch that talks to a
// running instance, and behind vncClient's file-transfer completion
// notification (vncClient.cpp:2531).
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
// Windows 3.1 has exactly one desktop and no window stations, so all of these
// succeed trivially.  Do not make them return FALSE: vncClient's main loop
// tests InputDesktopSelected() on every pass and drops the client if it is
// false, and vncDesktop::InitDesktop refuses to start if SelectDesktop fails.
// ==========================================================================

BOOL
vncService::SelectHDESK(HDESK new_desktop)
{
	// Nothing to switch to.  Returning TRUE means "we are where we should be".
	return TRUE;
}

BOOL
vncService::SelectDesktop(char *name)
{
	// There is only one desktop; we are always on it.
	return TRUE;
}

BOOL
vncService::InputDesktopSelected()
{
	// The single desktop always is the input desktop.
	return TRUE;
}

// ==========================================================================
// CTRL+ALT+DEL
//
// The NT implementation opened the Winlogon desktop and posted a hot-key
// message to it.  There is no equivalent on Windows 3.1 (Ctrl+Alt+Del is
// handled by the BIOS/DOS layer and reboots the machine), and simulating a
// reboot on a remote user's behalf would be actively harmful.
// ==========================================================================
BOOL
vncService::SimulateCtrlAltDel()
{
	vnclog.Print(LL_INTWARN,
		VNCLOG("Ctrl+Alt+Del is not supported on this platform\n"));
	return FALSE;
}

// ==========================================================================
// WORKSTATION LOCKING
//
// LockWorkstation is NT 4.0 and later.  Windows 3.1 has no login session to
// lock.
// ==========================================================================
BOOL
vncService::LockWorkstation()
{
	vnclog.Print(LL_INTWARN,
		VNCLOG("Workstation locking is not supported on this platform\n"));
	return FALSE;
}

// ==========================================================================
// MESSAGE-POSTING HELPERS
//
// All unchanged in behaviour - they only use FindWindow/PostMessage.
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
// The enumeration callback and its context struct are unchanged from the
// original; EnumWindows (not EnumDesktopWindows) is used, which exists on
// Win32s.
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

		// Case-insensitive substring search.  The original lowercased both
		// strings in place; doing it on a copy avoids modifying the caller's
		// buffer, which the -sharewindow parser still owns.
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

	// Lowercase the search string once (the command line is already lowercased
	// by WinMain, but do not depend on that).
	char lowerSubstr[256];
	strncpy(lowerSubstr, substr, sizeof(lowerSubstr) - 1);
	lowerSubstr[sizeof(lowerSubstr) - 1] = '\0';
	int i;
	for (i = 0; lowerSubstr[i] != '\0'; i++)
		lowerSubstr[i] = (char)tolower((unsigned char)lowerSubstr[i]);

	FindWindowByTitleData data;
	data.substr = lowerSubstr;
	data.hwnd = NULL;

	// MSVC 4.1: the callback needs an explicit cast.
	//
	// This produced
	//   error C2664: 'EnumWindows' : cannot convert parameter 1 from
	//   'int (void *,long)' to 'int (__stdcall *)(void)'
	//
	// Two things are going on:
	//
	//  * The 4.1 SDK declares EnumWindows' first parameter as a PARAMETERLESS
	//    __stdcall function pointer in a non-STRICT build - the same FARPROC-style
	//    declaration that made CallWindowProc need a cast in
	//    SharedDesktopArea.cpp.  C++ will not implicitly convert between
	//    function-pointer types, so the cast has to be explicit.
	//
	//  * The reported source type in the diagnostic is "int (void *, long)" -
	//    HWND is void* and LPARAM is long in a non-STRICT build, so that is the
	//    callback's own signature.  The mismatch is purely the parameter list of
	//    the DESTINATION type, not a calling-convention problem.
	//
	// vncDesktop.cpp:1741 already casts its EnumWindows callback to WNDENUMPROC
	// for exactly this reason; this call site was written without one.
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

// ==========================================================================
// SERVICE MODE - ALL STUBBED
//
// Windows 3.1 has no service control manager.  Every one of these reported
// success or did real SCM work in the original; they now tell the user the
// feature is unavailable and return failure, so that "winvnc -install" does
// something explicable rather than silently appearing to work.
// ==========================================================================

BOOL	g_servicemode = FALSE;

BOOL
vncService::RunningAsService()
{
	// Never a service on this platform.
	return FALSE;
}

BOOL
vncService::KillRunningCopy()
{
	// Post WM_CLOSE to every running instance and wait for it to go away.
	//
	// The original looped "PostMessage(...); omni_thread::sleep(1);" until
	// FindWindow returned NULL.  Two changes:
	//
	//  * omni_thread::sleep pumps messages in this build (see omnithread.h),
	//    which is necessary - the instance being killed is in the SAME process
	//    address space under Win32s in the -kill case, and in any case a bare
	//    Sleep on a cooperatively scheduled system prevents the target from
	//    processing the WM_CLOSE we just posted.
	//
	//  * The loop is now bounded.  Unbounded was an infinite hang if the target
	//    ignored WM_CLOSE.
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

BOOL
vncService::PostUserHelperMessage()
{
	// NT service-helper mechanism: the helper process posts its process ID to
	// the running service so the service can duplicate its user token.  There
	// are no tokens and no services here.
	return TRUE;
}

BOOL
vncService::PostReloadMessage()
{
	// Kept functional - this is a plain message post and the properties reload
	// is useful on any platform.
	if (!PostToWinVNC(MENU_RELOAD_MSG, 0, 0))
		return FALSE;

	return TRUE;
}

BOOL
vncService::ProcessUserHelperMessage(DWORD processId)
{
	// Counterpart of PostUserHelperMessage: would OpenProcess the helper,
	// OpenProcessToken it and DuplicateToken the result.  No security model
	// here.
	return FALSE;
}

bool
vncService::tryImpersonate()
{
	// IMPORTANT: must return TRUE.
	//
	// Windows 3.1 has no security model, so there is nothing to impersonate and
	// no restriction to work around - the server already runs with full access
	// to everything the logged-on user can reach.
	//
	// Returning false here would be a quiet disaster: every file-transfer
	// handler in vncClient.cpp is written as
	//
	//     if (!vncService::tryImpersonate()) { ...send failure to client...; break; }
	//
	// so a false return disables file transfer completely and reports it to the
	// user as an impersonation failure.
	return true;
}

void
vncService::undoImpersonate()
{
	// Nothing was impersonated.  Deliberately does NOT call RevertToSelf():
	// that is an ADVAPI32 import we do not need, and calling it without a
	// preceding impersonation is meaningless.
}

int
vncService::WinVNCServiceMain()
{
	MessageBox(NULL,
		"Service mode is not available on this version of Windows.\r\n\r\n"
		"Run WinVNC normally instead - it will appear as an ordinary "
		"application.",
		szAppName, MB_ICONINFORMATION | MB_OK);
	return 1;
}

int
vncService::InstallService(BOOL silent)
{
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

int
vncService::ReinstallService(BOOL silent)
{
	return InstallService(silent);
}

int
vncService::RemoveService(BOOL silent)
{
	if (!silent) {
		MessageBox(NULL,
			"Service removal is not available on this version of Windows.",
			szAppName, MB_ICONINFORMATION | MB_OK);
	}
	return 1;
}

// Error reporting helper, retained because WinVNC.cpp and vncMenu.cpp
// reference it.
//
// The NT version wrote to the event log via RegisterEventSource /
// ReportEvent / DeregisterEventSource.  There is no event log on Windows 3.1,
// so this goes to the normal VNC log instead.
void
LogErrorMsg(char *message)
{
	if (message == NULL)
		message = "(no message)";

	vnclog.Print(LL_INTERR, VNCLOG("%s (error %d)\n"),
				 message, (int)GetLastError());
}
