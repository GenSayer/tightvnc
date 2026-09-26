//  Copyright (C) 2001-2004 HorizonLive.com, Inc. All Rights Reserved.
//  Copyright (C) 2001-2004 TightVNC Team. All Rights Reserved.
//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
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

// vncDesktop implementation

// System headers
#include "stdhdrs.h"
#include <omnithread.h>

// Custom headers
#include "WinVNC.h"
#include "VNCHooks\VNCHooks.h"
#include "vncServer.h"
#include "vncRegion.h"
#include "rectlist.h"
#include "vncDesktop.h"
#include "vncService.h"
#include "WallpaperUtils.h"
#include "TsSessions.h"

/* #if (_MSC_VER>= 1300)
#include <fstream>
#else
#include <fstream.h>
#endif */

#ifndef SM_CMONITORS
#define SM_CMONITORS		80
#define SM_XVIRTUALSCREEN	76
#define SM_YVIRTUALSCREEN	77
#define SM_CXVIRTUALSCREEN	78
#define SM_CYVIRTUALSCREEN	79
#endif

// Constants
const UINT RFB_SCREEN_UPDATE = RegisterWindowMessage("WinVNC.Update.DrawRect");
const UINT RFB_COPYRECT_UPDATE = RegisterWindowMessage("WinVNC.Update.CopyRect");
const UINT RFB_MOUSE_UPDATE = RegisterWindowMessage("WinVNC.Update.Mouse");
// Messages for blocking remote input events
const UINT RFB_LOCAL_KEYBOARD = RegisterWindowMessage("WinVNC.Local.Keyboard");
const UINT RFB_LOCAL_MOUSE = RegisterWindowMessage("WinVNC.Local.Mouse");

const char szDesktopSink[] = "WinVNC desktop sink";

// Atoms
const char *VNC_WINDOWPOS_ATOMNAME = "VNCHooks.CopyRect.WindowPos";
ATOM VNC_WINDOWPOS_ATOM = NULL;

// Static members to use with new polling algorithm
const int vncDesktop::m_pollingOrder[32] = {
	 0, 16,  8, 24,  4, 20, 12, 28,
	10, 26, 18,  2, 22,  6, 30, 14,
	 1, 17,  9, 25,  7, 23, 15, 31,
	19,  3, 27, 11, 29, 13,  5, 21
};
int vncDesktop::m_pollingStep = 0;


BOOL IsWinNT()
{
	return vncService::IsWinNT();
}

BOOL IsWinVerOrHigher(ULONG mj, ULONG mn)
{
	return vncService::VersionMajor() > mj ||
		vncService::VersionMajor() == mj && vncService::VersionMinor() >= mn;
}

BOOL IsNtVer(ULONG mj, ULONG mn)
{
	if (!vncService::IsWinNT())	
		return FALSE;
	return vncService::VersionMajor() == mj && vncService::VersionMinor() == mn;
}

BOOL vncDesktop::IsMultiMonDesktop()
{
	if (!IsWinVerOrHigher(4, 10))
		return FALSE;
	// WIN32S: GetSystemMetrics(SM_CMONITORS) returns 0 on a platform that does
	// not know the index, so this correctly reports "not multi-monitor".  Windows
	// 3.1 has no multi-monitor support.
	return GetSystemMetrics(SM_CMONITORS) > 1;
}

// The desktop handler thread
// This handles the messages posted by RFBLib to the vncDesktop window

// ==========================================================================
// WIN32S SINGLE-THREADED CONVERSION
//
// "class vncDesktopThread : public omni_thread" used to live here (lines
// 111-350 of the original).  It did three things:
//
//   1. Init() called start_undetached() and then BLOCKED on an omni_condition
//      until the new thread reported whether Startup() had succeeded.  On a
//      single thread that is an immediate deadlock: the condition can only be
//      signalled by the thread that is waiting for it.
//
//   2. run_undetached() owned the desktop sink window's message queue and ran
//      the polling loop:
//
//          while (TRUE) {
//              if (!PeekMessage(&msg, m_desktop->Window(), ...)) {
//                  if (!m_server->WallpaperWait())
//                      if (!m_desktop->CheckUpdates()) break;
//                  WaitMessage();
//              }
//              else if (msg.message == RFB_SCREEN_UPDATE) { ... }
//              else if (msg.message == RFB_MOUSE_UPDATE)  { ... }
//              ...
//          }
//
//      That structure cannot survive: there is now ONE message queue shared
//      with the tray window, the properties dialogs and everything else, so a
//      PeekMessage filtered to the sink window would starve the rest of the
//      application, and WaitMessage() inside it would block the whole program.
//
//   3. On exit it called Shutdown(), ResetDisplayToNormal(), BlankScreen(FALSE)
//      and ClearShiftKeys().
//
// REPLACEMENT
//
//   vncDesktop::Init()      - calls Startup() DIRECTLY and returns its result.
//                             No thread, no condition variable, no deadlock.
//
//   vncDesktop::PumpIdle()  - called from the application idle loop.  Does what
//                             the "message queue is empty" branch of the old
//                             loop did: WallpaperWait() check, then
//                             CheckUpdates().  Returns FALSE when the desktop
//                             wants to shut down.
//
//   DesktopWndProc          - the sink window's messages (RFB_SCREEN_UPDATE,
//                             RFB_MOUSE_UPDATE, RFB_LOCAL_KEYBOARD,
//                             RFB_LOCAL_MOUSE) are now handled in the window
//                             procedure, because the main message loop
//                             dispatches them there.  See the additions to
//                             DesktopWndProc below.
//
//   vncDesktop::Shutdown()  - unchanged, but now called from the destructor and
//                             from PumpIdle's failure path rather than at the
//                             end of a thread function.
//
// NOTE ON RFB_SCREEN_UPDATE: with VNCHooks stubbed out (see VNCHooksStub.cpp)
// nothing ever posts that message on this platform, so the handler is dead code
// here - kept so that the file still works if hooks are ever restored.  All
// change detection comes from CheckUpdates()/PerformPolling().
//
// The local-input-priority messages (RFB_LOCAL_KEYBOARD / RFB_LOCAL_MOUSE) are
// likewise never posted, because the priority hooks cannot install.  Their
// handling - including the GetSystemTime/SystemTimeToFileTime/ULARGE_INTEGER
// arithmetic, which needed 64-bit maths - has therefore been dropped rather
// than ported.
// ==========================================================================


// Implementation of the vncDesktop class

vncDesktop::vncDesktop()
{
	m_shutdown_requested = FALSE;

	m_hwnd = NULL;
	m_polling_flag = FALSE;
	m_lastPollTick = 0;
	m_timer_polling = 0;
	m_timer_blank_screen = 0;
	m_hnextviewer = NULL;
	m_hcursor = NULL;

	m_displaychanged = FALSE;

	m_hrootdc = NULL;
	m_hmemdc = NULL;
	m_hgetdibdc = NULL;
	m_flipbuff = NULL;
	m_flipbuffsize = 0;
	m_membitmap = NULL;

	m_initialClipBoardSeen = FALSE;

	// Vars for Will Dean's DIBsection patch
	m_DIBbits = NULL;
	m_DIBbits_bottom_up = FALSE;
	m_freemainbuff = FALSE;
	m_formatmunged = FALSE;
	m_mainbuff = NULL;
	m_backbuff = NULL;

	m_clipboard_active = FALSE;
	m_hooks_active = FALSE;
	m_hooks_may_change = FALSE;
	m_lpAlternateDevMode = NULL;
	m_copyrect_set = FALSE;

	// WIN32S: m_videodriver is retained as a member but is ALWAYS NULL - the
	// mirror driver requires Windows 2000 or later.  See InitVideoDriver().
	m_videodriver = NULL;

	m_timer_blank_screen = 0;
}

vncDesktop::~vncDesktop()
{
	vnclog.Print(LL_INTINFO, VNCLOG("killing desktop server\n"));

	// WIN32S: was
	//     if (m_thread != NULL) {
	//         PostMessage(Window(), WM_QUIT, 0, 0);
	//         void *returnval;
	//         m_thread->join(&returnval);   // wait for the thread to finish
	//     }
	//
	// There is no thread to join.  Note also that the original posted WM_QUIT to
	// the SINK WINDOW - PostMessage(hwnd, WM_QUIT, ...) does not do what
	// PostQuitMessage does; it delivers a WM_QUIT that the thread's own
	// PeekMessage loop recognised.  With a single shared message loop, posting
	// WM_QUIT here would terminate the WHOLE APPLICATION, so it must not be done.
	//
	// Everything the thread did on the way out (Shutdown, ResetDisplayToNormal,
	// BlankScreen, ClearShiftKeys) is done either by PumpIdle's shutdown path or
	// directly below.
	if (!m_shutdown_requested) {
		SetClipboardActive(FALSE);
		ResetDisplayToNormal();
		BlankScreen(FALSE);
		vncKeymap::ClearShiftKeys();
		m_shutdown_requested = TRUE;
	}

	// Let's call Shutdown just in case something went wrong...
	Shutdown();
	_ASSERTE(!m_lpAlternateDevMode);
}

// Routine to startup and install all the hooks and stuff
BOOL
vncDesktop::Startup()
{
	// Currently, we just check whether we're in the console session, and
	//   fail if not
	if (!inConsoleSession()) {
		vnclog.Print(LL_INTERR, VNCLOG("Console is not session zero - reconnect to restore Console session"));
		return FALSE;
	}

	// Configure the display for optimal VNC performance.
	SetupDisplayForConnection();

	// Initialise the Desktop object
	if (!InitDesktop())
		return FALSE;

	if (InitVideoDriver())
	{
// this isn't really necessary
//		InvalidateRect(NULL,NULL,TRUE);
	}

	if (!InitBitmap())
		return FALSE;

	if (!ThunkBitmapInfo())
		return FALSE;

	if (!SetPixFormat())
		return FALSE;

	if (!CreateBuffers())
		return FALSE;

	if (!SetPixShifts())
		return FALSE;

	if (!SetPalette())
		return FALSE;

	if (!InitWindow())
		return FALSE;

	// Add the system hook
	ActivateHooks();
	m_hooks_may_change = true;

#ifndef HORIZONLIVE
	// Start up the keyboard and mouse filters
	SetKeyboardFilterHook(m_server->LocalInputsDisabled());
	SetMouseFilterHook(m_server->LocalInputsDisabled());
#endif

	// Start up the keyboard and mouse hooks  for 
	// local event priority over remote impl.
	if (m_server->LocalInputPriority())
		SetLocalInputPriorityHook(true);

	// Start a timer to handle Polling Mode.  The timer will cause
	// an "idle" event, which is necessary if Polling Mode is being used,
	// to cause TriggerUpdate to be called.
	SetPollingFlag(FALSE);
	SetPollingTimer();

	// If necessary, start a separate timer to preserve the diplay turned off.
	UpdateBlankScreenTimer();

	// Get hold of the WindowPos atom!
	if ((VNC_WINDOWPOS_ATOM = GlobalAddAtom(VNC_WINDOWPOS_ATOMNAME)) == 0) {
		vnclog.Print(LL_INTERR, VNCLOG("GlobalAddAtom() failed.\n"));
		return FALSE;
	}

// this member must be initialized: we cant assume the absence
// of clients when desktop is created.
	m_cursorpos.left = 0;
	m_cursorpos.top = 0;
	m_cursorpos.right = 0;
	m_cursorpos.bottom = 0;

	// Everything is ok, so return TRUE
	return TRUE;
}

// Routine to shutdown all the hooks and stuff
BOOL vncDesktop::Shutdown()
{
	// If we created timers then kill them
	if (m_timer_polling)
	{
		KillTimer(Window(), TIMER_POLL);
		m_timer_polling = 0;
	}
	if (m_timer_blank_screen)
	{
		KillTimer(Window(), TIMER_BLANK_SCREEN);
		m_timer_blank_screen = 0;
	}

	// If we created a window then kill it and the hooks
	if (m_hwnd != NULL)
	{	
		//Remove the system hooks
		//Unset keyboard and mouse hooks
		SetLocalInputPriorityHook(false);
		m_hooks_may_change = false;
		ShutdownHooks();

#ifndef HORIZONLIVE
		// Stop the keyboard and mouse filters
		SetKeyboardFilterHook(false);
		SetMouseFilterHook(false);
#endif
		// The window is being closed - remove it from the viewer list
		ChangeClipboardChain(m_hwnd, m_hnextviewer);

		// Close the hook window
		DestroyWindow(m_hwnd);
		m_hwnd = NULL;
		m_hnextviewer = NULL;
	}

	// Now free all the bitmap stuff
	if (m_hrootdc != NULL)
	{
		// Release our device context
		if(ReleaseDC(NULL, m_hrootdc) == 0)
		{
			vnclog.Print(LL_INTERR, VNCLOG("failed to ReleaseDC(m_hrootdc)\n"));
		}
		m_hrootdc = NULL;
	}
	if (m_hmemdc != NULL)
	{
		// Release our device context
		if (!DeleteDC(m_hmemdc))
		{
			vnclog.Print(LL_INTERR, VNCLOG("failed to DeleteDC(m_hmemdc)\n"));
		}
		m_hmemdc = NULL;
	}
	if (m_hgetdibdc != NULL)
	{
		// WIN32S: the bitmap-free DC used for GetDIBits (see CopyToBuffer).
		// Windows 3.1 has a small, system-wide DC pool, so leaking this would
		// eventually starve the whole machine of device contexts.
		if (!DeleteDC(m_hgetdibdc))
		{
			vnclog.Print(LL_INTERR, VNCLOG("failed to DeleteDC(m_hgetdibdc)\n"));
		}
		m_hgetdibdc = NULL;
	}
	if (m_flipbuff != NULL)
	{
		// WIN32S: the scanline-inversion scratch buffer (see CopyToBuffer).
		delete [] m_flipbuff;
		m_flipbuff = NULL;
		m_flipbuffsize = 0;
	}
	if (m_membitmap != NULL)
	{
		// Release the custom bitmap, if any
		if (!DeleteObject(m_membitmap))
		{
			vnclog.Print(LL_INTERR, VNCLOG("failed to DeleteObject\n"));
		}
		m_membitmap = NULL;
	}

	// Free back buffer
	if (m_backbuff != NULL)
	{
		delete [] m_backbuff;
		m_backbuff = NULL;
	}

	if (m_freemainbuff)
	{
		// Slow blits were enabled - free the slow blit buffer
		if (m_mainbuff != NULL)
		{
			delete [] m_mainbuff;
			m_mainbuff = NULL;
		}
	}

	// Free the WindowPos atom!
	if (VNC_WINDOWPOS_ATOM != NULL)
	{
		if (GlobalDeleteAtom(VNC_WINDOWPOS_ATOM) != 0)
		{
			vnclog.Print(LL_INTERR, VNCLOG("failed to delete atom!\n"));
		}
	}

	ShutdownVideoDriver();

	return TRUE;
}

// Routines to set/unset hooks via VNCHooks.dll

void
vncDesktop::ActivateHooks()
{
	BOOL enable = !(m_server->DontSetHooks() && m_server->PollFullScreen());
	if (enable && !m_hooks_active) {
		m_hooks_active = SetHook(m_hwnd,
								 RFB_SCREEN_UPDATE,
								 RFB_COPYRECT_UPDATE,
								 RFB_MOUSE_UPDATE);
		if (!m_hooks_active) {
			vnclog.Print(LL_INTERR, VNCLOG("failed to set system hooks\n"));
			// Switch on full screen polling, so they can see something, at least...
			m_server->PollFullScreen(TRUE);
		}
	} else if (!enable) {
		ShutdownHooks();
	}
}

void
vncDesktop::ShutdownHooks()
{
	if (m_hooks_active)
		m_hooks_active = !UnSetHook(m_hwnd);
}

void
vncDesktop::TryActivateHooks()
{
	if (m_hooks_may_change)
		ActivateHooks();
}

// Routine to ensure we're on the correct NT desktop

BOOL
vncDesktop::InitDesktop()
{
	if (vncService::InputDesktopSelected())
		return TRUE;

	// Ask for the current input desktop
	return vncService::SelectDesktop(NULL);
}

// KillScreenSaverFunc() removed: it was the EnumDesktopWindows callback for the
// NT screen-saver desktop, and EnumDesktopWindows does not exist on Win32s.
// See KillScreenSaver() below.

// ==========================================================================
// KillScreenSaver - WIN32S VERSION
//
// The original branched on dwPlatformId:
//
//   Win9x: FindWindow("WindowsScreenSaverClass") + PostMessage(WM_CLOSE)
//   NT:    OpenDesktop("Screen-saver") + EnumDesktopWindows(...) +
//          CloseDesktop() + SystemParametersInfo(SPI_SETSCREENSAVEACTIVE)
//
// Neither applies here, and the NT branch is actively harmful: OpenDesktop,
// CloseDesktop and EnumDesktopWindows are NT-only USER32 exports, absent from
// Win32s.  Their presence in the import table stops the EXE from LOADING - the
// process never starts and there is no diagnostic.  (roytam1's winvnc333r9-vc4
// patch af90882 resolved EnumDesktopWindows dynamically for NT 3.50; here the
// whole branch is unnecessary because there are no desktops to enumerate.)
//
// Windows 3.1 screen savers are ordinary applications launched by USER's idle
// timer.  There is no standard window class to find and no documented way to
// dismiss one - and in practice a Windows 3.1 screen saver exits on the first
// mouse or keyboard event, which a connected VNC client generates anyway.
//
// SPI_SETSCREENSAVEACTIVE (17) IS supported by Windows 3.1's
// SystemParametersInfo, so the screen saver can be disabled outright.  That is
// the useful thing to do here: while a client is connected, a screen saver that
// kicks in would both blank what the client sees and waste the CPU that the
// polling capture needs.
// ==========================================================================

void
vncDesktop::KillScreenSaver()
{
	vnclog.Print(LL_INTINFO, VNCLOG("disabling screen saver...\n"));

	// Turn the screen saver off for the duration.  Windows 3.1 supports this
	// action, and it broadcasts WM_WININICHANGE so that Control Panel notices.
	//
	// NOTE: this is deliberately not restored when the client disconnects.  The
	// original never restored it either on the Win9x path, and re-enabling it
	// would require remembering the previous state - which is stored in WIN.INI
	// and may legitimately change while we run.  The user's own Control Panel
	// setting is unaffected; only the running instance is suppressed.
	if (!SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, 0,
							  SPIF_SENDWININICHANGE)) {
		vnclog.Print(LL_INTINFO,
			VNCLOG("could not disable the screen saver (ignored)\n"));
	}

	// If a Windows 3.1 screen saver is already running it is a normal
	// application; the client's first input event will dismiss it.
}

// ==========================================================================
// DISPLAY MODE CHANGING - REMOVED FOR THE WIN32S PORT
// ==========================================================================
//
// ChangeResNow() used to read "ResWidth"/"ResHeight" from the registry and call
// ChangeDisplaySettings() to switch the server's screen to that resolution while
// a client was connected, restoring it in ResetDisplayToNormal().
//
// This cannot work on Windows 3.1, for a reason that is worth being precise
// about:
//
//   * ChangeDisplaySettings() and EnumDisplaySettings() are Windows 95 / NT 3.5
//     APIs.  They are NOT in the Win32s USER32 stub set.  Because the linker
//     records every imported name whether or not the code can execute, having
//     them in this file at all prevents the EXE from LOADING on Win32s.
//
//   * Windows 3.1 has no run-time display mode switching in any case.  The
//     resolution and colour depth are fixed by the display driver chosen in
//     Windows Setup, and changing them requires editing SYSTEM.INI and
//     restarting Windows.
//
// The registry values are left alone: they are read and written by
// vncProperties, and silently ignoring them is better than deleting a user's
// settings.
//
// m_lpAlternateDevMode stays NULL for the life of the object, which is what
// ResetDisplayToNormal() and the _ASSERTE in the destructor already expect.
// Note that DEVMODE itself is declared by the MSVC 4.1 headers, so the member
// still compiles.
// ==========================================================================

void vncDesktop::ChangeResNow()
{
	// Deliberately empty - see the note above.  m_lpAlternateDevMode remains
	// NULL, so ResetDisplayToNormal() has nothing to undo.
}

void
vncDesktop::SetupDisplayForConnection()
{
	KillScreenSaver();

	ChangeResNow(); // *** - Jeremy Peaks
}

void
vncDesktop::ResetDisplayToNormal()
{
	// WIN32S: ChangeResNow() never changes the mode, so m_lpAlternateDevMode is
	// always NULL and there is nothing to restore.  The body is kept (rather than
	// emptied) so that the ownership contract is still visible, but the
	// ChangeDisplaySettings calls are gone - they are Win95/NT-only imports that
	// would stop the EXE loading.  See the note on ChangeResNow above.
	if (m_lpAlternateDevMode != NULL)
	{
		delete m_lpAlternateDevMode;
		m_lpAlternateDevMode = NULL;
	}
}

RECT vncDesktop::GetSourceRect()
{
	if (m_server->WindowShared())
	{
		RECT wrect;
		GetWindowRect(m_server->GetWindowShared(), &wrect);
		return wrect;
	}
	else if (m_server->ScreenAreaShared())
	{
		return m_server->GetScreenAreaRect();
	}
	else if (m_server->PrimaryDisplayOnlyShared())
	{
		RECT pdr = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		return pdr;
	}
	else
	{
#ifdef _DEBUG
		RECT rd;
		_ASSERTE(GetSourceDisplayRect(rd));
		_ASSERTE(EqualRect(&rd, &m_bmrect));
#endif
		return m_bmrect;
	}
}

RECT	GetScreenRect()
{
	// WIN32S NOTE: SM_XVIRTUALSCREEN and friends (76-79) are Win98/Win2000
	// metrics.  They are safe to REQUEST on any platform - GetSystemMetrics
	// returns 0 for an index it does not know, it is not an import problem - but
	// 0 would give a zero-size screen rect here.
	//
	// IsWinVerOrHigher(4, 10) is what keeps us out of that branch: Win32s reports
	// Windows 3.10/3.11, so the else branch runs and the rect is the single
	// physical screen.  That is correct - Windows 3.1 has no multi-monitor
	// support of any kind.
	RECT screenrect;
	if (IsWinVerOrHigher(4, 10))
	{
		screenrect.left		= GetSystemMetrics(SM_XVIRTUALSCREEN);
		screenrect.top		= GetSystemMetrics(SM_YVIRTUALSCREEN);
		screenrect.right	= screenrect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
		screenrect.bottom	= screenrect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
	}
	else
	{
		screenrect.left = 0;
		screenrect.top = 0;
		screenrect.right = GetSystemMetrics(SM_CXSCREEN);
		screenrect.bottom = GetSystemMetrics(SM_CYSCREEN);
	}
	return screenrect;
}

BOOL vncDesktop::GetSourceDisplayRect(RECT &rdisp_rect)
{
	if (!m_hrootdc)
		m_hrootdc = ::GetDC(NULL);
	if (!m_hrootdc)
	{
		vnclog.Print(LL_INTERR, VNCLOG("GetDC() failed, error=%d\n"), GetLastError());
		return FALSE;
	}

// TODO: refactor it
	rdisp_rect = GetScreenRect();
	return TRUE;
}

BOOL vncDesktop::InitBitmap()
{
// IMPORTANT: here an optimization may be implemented
// when only a fixed rect is shared.
// then m_bmrect should be set to that rect.
	if (!GetSourceDisplayRect(m_bmrect))
	{
		return FALSE;
	}

	vnclog.Print(
		LL_INTINFO,
		VNCLOG("source desktop metrics: (%d, %d, %d, %d)\n"),
		m_bmrect.left,
		m_bmrect.top,
		m_bmrect.right,
		m_bmrect.bottom);

	vnclog.Print(
		LL_INTINFO,
		VNCLOG("bitmap dimensions are %dx%d\n"),
		m_bmrect.right - m_bmrect.left,
		m_bmrect.bottom - m_bmrect.top);

	// Create a compatible memory DC
	m_hmemdc = CreateCompatibleDC(m_hrootdc);
	if (m_hmemdc == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("CreateCompatibleDC() failed, error=%d\n"),
					 GetLastError());
		return FALSE;
	}

	// Check that the device capabilities are ok
	if ((GetDeviceCaps(m_hrootdc, RASTERCAPS) & RC_BITBLT) == 0)
	{
// FIXME: MessageBox in a service
		MessageBox(
			NULL,
			"vncDesktop : root device doesn't support BitBlt\n"
			"WinVNC cannot be used with this graphic device driver",
			szAppName,
			MB_ICONSTOP | MB_OK
			);
		return FALSE;
	}
	if ((GetDeviceCaps(m_hmemdc, RASTERCAPS) & RC_DI_BITMAP) == 0)
	{
// FIXME: MessageBox in a service
		MessageBox(
			NULL,
			"vncDesktop : memory device doesn't support GetDIBits\n"
			"WinVNC cannot be used with this graphics device driver",
			szAppName,
			MB_ICONSTOP | MB_OK
			);
		return FALSE;
	}

	// Create the bitmap to be compatible with the ROOT DC!!!
	m_membitmap = CreateCompatibleBitmap(
		m_hrootdc,
		m_bmrect.right - m_bmrect.left,
		m_bmrect.bottom - m_bmrect.top);
	if (m_membitmap == NULL)
	{
		vnclog.Print(
			LL_INTERR,
			VNCLOG("failed to create memory bitmap, error=%d\n"),
			GetLastError());
		return FALSE;
	}
	vnclog.Print(LL_INTINFO, VNCLOG("created memory bitmap\n"));

	// ==================================================================
	// WIN32S: THIS IS WHERE EVERY CONNECTION WAS FAILING.
	//
	// The log showed "created memory bitmap" followed immediately by
	// "failed to initialize desktop object", with nothing in between - because
	// both GetDIBits calls below did "return FALSE" with no diagnostic.
	//
	// The code was:
	//
	//     int result;
	//     m_bminfo.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	//     m_bminfo.bmi.bmiHeader.biBitCount = 0;
	//     result = GetDIBits(m_hmemdc, m_membitmap, 0, 1, NULL,
	//                        &m_bminfo.bmi, DIB_RGB_COLORS);
	//
	// This is the documented "query the format" idiom: pass NULL bits and
	// biBitCount == 0 and GDI fills in the header.  It works on NT and Win9x.
	// It fails on Win32s, for two reasons:
	//
	//  1. m_bminfo IS NEVER ZEROED.  It is a plain member of vncDesktop and the
	//     constructor does not clear it, so only TWO of BITMAPINFOHEADER's
	//     eleven fields are initialised here - biWidth, biHeight, biPlanes,
	//     biCompression, biSizeImage, biXPelsPerMeter, biYPelsPerMeter,
	//     biClrUsed and biClrImportant are whatever was on the heap.  The NT
	//     query path ignores the incoming values; the 16-bit GDI that Win32s
	//     thunks down to validates the header and rejects it.
	//
	//  2. The SECOND call needs somewhere to put the palette.  On a 256-colour
	//     display (which this is) GetDIBits wants to write 256 RGBQUADs after
	//     the header.  That is what the "cmap[256] comes straight after
	//     BITMAPINFO - **HACK**" comment in vncDesktop.h is relying on: bmi is
	//     a BITMAPINFO, whose bmiColors[] is declared as a single element, and
	//     the extra 255 entries are expected to land in the cmap array that
	//     follows it in the struct.  That only holds if the compiler places
	//     cmap immediately after bmi with no padding.  MSVC 4.1's default
	//     packing does, but it is worth asserting rather than assuming - if it
	//     ever did not, GetDIBits would write 255 palette entries over whatever
	//     followed.
	//
	// The fix: zero the whole structure, set every field the query needs, verify
	// the bmi/cmap adjacency, and LOG the failure with GetLastError() so the next
	// problem here is not silent.
	// ==================================================================

	// Verify the bmi -> cmap adjacency the palette query depends on.  This is a
	// compile-time property, so a run-time check costs one comparison at startup
	// and turns a memory-corruption bug into a clear message.
	{
		size_t bmiOffset  = (size_t)((char *)&m_bminfo.bmi  - (char *)&m_bminfo);
		size_t cmapOffset = (size_t)((char *)&m_bminfo.cmap - (char *)&m_bminfo);
		size_t expected   = bmiOffset + sizeof(BITMAPINFO);
		if (cmapOffset != expected) {
			// BITMAPINFO already contains one RGBQUAD (bmiColors[1]), so cmap is
			// expected to start exactly sizeof(BITMAPINFO) after bmi.
			vnclog.Print(LL_INTERR,
				VNCLOG("BMInfo layout is wrong: cmap at %d, expected %d - "
					   "the palette query would corrupt memory\n"),
				(int)cmapOffset, (int)expected);
			return FALSE;
		}
	}

	// Get the bitmap's format and colour details.
	int result;

	// ------------------------------------------------------------------
	// THE QUERY FORM MUST BE EXACTLY THIS: biSize + biBitCount == 0.
	//
	// My first attempt at fixing this set biWidth, biHeight, biPlanes and
	// biCompression as well, on the theory that Win32s validates the whole
	// header.  That made things WORSE - the first call started failing with
	// error 87 where it had previously succeeded.
	//
	// The reason: biBitCount == 0 means "report this bitmap's format", and every
	// other field is then an OUTPUT.  Supplying biCompression = BI_RGB together
	// with biBitCount = 0 is self-contradictory, and the 16-bit GDI validates
	// the header before deciding what the call means, so it rejects the
	// combination.  NT tolerates it; Windows 3.1 does not.
	//
	// So: zero the structure (that part WAS necessary - see below), then set
	// only the two fields the documented query form uses.
	// ------------------------------------------------------------------

	// Clear EVERYTHING - the header, the single bmiColors entry inside
	// BITMAPINFO, and the 256-entry colour map that follows it.
	//
	// This is still required.  m_bminfo is a plain member of vncDesktop and the
	// constructor never touched it, so without this the header arrives holding
	// heap garbage in the nine fields the query does not set.
	memset(&m_bminfo, 0, sizeof(m_bminfo));

	m_bminfo.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_bminfo.bmi.bmiHeader.biBitCount = 0;

	result = ::GetDIBits(m_hmemdc, m_membitmap, 0, 1, NULL,
						 &m_bminfo.bmi, DIB_RGB_COLORS);
	if (result == 0) {
		// ==============================================================
		// WIN32S: THE FORMAT QUERY IS NOT SUPPORTED.
		//
		// Error 87 (ERROR_INVALID_PARAMETER) from
		//     GetDIBits(hdc, hbm, 0, 1, NULL, &bmi, DIB_RGB_COLORS)
		// with biBitCount == 0.
		//
		// This is the documented "tell me the bitmap's format" form, and it was
		// added in Windows 3.1 - but the Win32s thunk layer does not implement
		// the NULL-bits variant.  Passing a NULL lpvBits pointer through the
		// 32->16 bit thunk is exactly the case that layer handles badly: it has
		// to translate a flat pointer into a 16:16 segmented one, and it rejects
		// NULL rather than passing it through as "no buffer".
		//
		// (This is the same class of problem as ReadExact(NULL, n) in VSocket -
		// see the note there.  A NULL buffer pointer is not portable across the
		// thunk.)
		//
		// FALL BACK to deriving the format from the display DC directly.  Every
		// call used here is a core Windows 3.0/3.1 GDI function:
		//
		//   GetDeviceCaps(BITSPIXEL)  - bits per pixel
		//   GetDeviceCaps(PLANES)     - planes (must be 1; checked below)
		//   GetDeviceCaps(RASTERCAPS) - RC_PALETTE tells us palette vs truecolour
		//
		// The result is exactly what the query would have reported for a bitmap
		// created with CreateCompatibleBitmap(m_hrootdc, ...), because that
		// bitmap has the display's format by definition.
		// ==============================================================
		DWORD err = GetLastError();
		vnclog.Print(LL_INTWARN,
			VNCLOG("GetDIBits format query unsupported (error=%d) - "
				   "deriving format from the bitmap itself\n"), err);

		// ==============================================================
		// BEST SOURCE OF TRUTH: GetObject() on the bitmap.
		//
		// GetObject(hbm, sizeof(BITMAP), &bm) fills in a BITMAP structure that
		// reports the bitmap's OWN format:
		//
		//     bm.bmBitsPixel   bits per pixel
		//     bm.bmPlanes      planes
		//     bm.bmWidthBytes  the actual scanline stride, already aligned
		//
		// This is far better than GetDeviceCaps for our purpose:
		//
		//  * It describes m_membitmap, which is what we are about to read with
		//    GetDIBits - not the display, which we only care about indirectly.
		//
		//  * It takes NO pointer-to-buffer argument, so it does not hit the
		//    thunk's NULL-pointer problem that breaks the GetDIBits query.
		//
		//  * GetDeviceCaps(BITSPIXEL) has been observed on Win32s to report 24
		//    regardless of the actual Windows 3.1 colour setting - the same value
		//    at 256 colours, 32k and 64k.  GetObject reads the bitmap's header
		//    instead of asking the driver, so it is not subject to that.
		//
		//  * bmWidthBytes gives the real stride, which removes the guesswork in
		//    SetPixFormat's DWORD-alignment calculation.
		//
		// GetObject on a bitmap is core Windows 3.0 GDI, present on every target.
		// ==============================================================
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		BOOL haveBitmapInfo = FALSE;

		if (GetObject(m_membitmap, sizeof(BITMAP), &bm) == sizeof(BITMAP)) {
			haveBitmapInfo = TRUE;
			vnclog.Print(LL_INTINFO,
				VNCLOG("GetObject: %dx%d, %d bpp, %d planes, %d bytes/row\n"),
				(int)bm.bmWidth, (int)bm.bmHeight,
				(int)bm.bmBitsPixel, (int)bm.bmPlanes,
				(int)bm.bmWidthBytes);
		} else {
			vnclog.Print(LL_INTWARN,
				VNCLOG("GetObject on the bitmap failed, error=%d - "
					   "falling back to GetDeviceCaps\n"), GetLastError());
		}

		int bpp    = GetDeviceCaps(m_hrootdc, BITSPIXEL);
		int planes = GetDeviceCaps(m_hrootdc, PLANES);

		// Prefer what the BITMAP itself says.
		if (haveBitmapInfo && bm.bmBitsPixel > 0 && bm.bmPlanes > 0) {
			if (bm.bmBitsPixel != bpp || bm.bmPlanes != planes) {
				vnclog.Print(LL_INTWARN,
					VNCLOG("GetDeviceCaps says %d bpp x %d planes but the bitmap "
						   "is %d bpp x %d planes - trusting the bitmap\n"),
					bpp, planes, (int)bm.bmBitsPixel, (int)bm.bmPlanes);
			}
			bpp    = bm.bmBitsPixel;
			planes = bm.bmPlanes;
		}

		if (bpp <= 0 || planes <= 0) {
			vnclog.Print(LL_INTERR,
				VNCLOG("GetDeviceCaps reported %d bpp / %d planes - giving up\n"),
				bpp, planes);
			return FALSE;
		}

		// GetDeviceCaps reports bits-per-plane.  A planar display (planes > 1)
		// is rejected further down anyway, but compute the true depth so the
		// diagnostic is meaningful.
		int depth = bpp * planes;

		// ==============================================================
		// WIN32S: cross-check the reported depth.
		//
		// GetDeviceCaps(BITSPIXEL) on the Win32s thunk has been observed to
		// report 24 REGARDLESS of the actual Windows 3.1 display setting - the
		// same value came back at 256 colours, 32k, 64k and higher.  So it cannot
		// be trusted on its own.
		//
		// These three caps give an independent view, and all of them are core
		// Windows 3.0/3.1 GDI:
		//
		//   NUMCOLORS    - for a palette device, the number of entries in the
		//                  system palette (usually 20 reserved on a 256-colour
		//                  driver); -1 on a device with more than 8bpp.
		//   SIZEPALETTE  - total palette size, only meaningful when RC_PALETTE
		//                  is set; 256 on an 8-bit display.
		//   RASTERCAPS   - RC_PALETTE tells us definitively whether the device
		//                  is palette-managed, which on this vintage of hardware
		//                  means 8bpp or less.
		//
		// If RC_PALETTE is set, the display IS palette-based and the depth must
		// be <= 8 whatever BITSPIXEL claims.  Trust SIZEPALETTE for the count and
		// derive the depth from it.
		// ==============================================================
		int rastercaps  = GetDeviceCaps(m_hrootdc, RASTERCAPS);
		int numcolors   = GetDeviceCaps(m_hrootdc, NUMCOLORS);
		int sizepalette = GetDeviceCaps(m_hrootdc, SIZEPALETTE);

		vnclog.Print(LL_INTINFO,
			VNCLOG("device caps: BITSPIXEL=%d PLANES=%d NUMCOLORS=%d "
				   "SIZEPALETTE=%d RC_PALETTE=%d\n"),
			bpp, planes, numcolors, sizepalette,
			(rastercaps & RC_PALETTE) ? 1 : 0);

		if (rastercaps & RC_PALETTE) {
			// Palette-managed device.  Derive the depth from the palette size.
			int palDepth = 8;
			if (sizepalette >= 2 && sizepalette <= 2)        palDepth = 1;
			else if (sizepalette > 2 && sizepalette <= 16)   palDepth = 4;
			else if (sizepalette > 16 && sizepalette <= 256) palDepth = 8;

			if (depth != palDepth) {
				vnclog.Print(LL_INTWARN,
					VNCLOG("BITSPIXEL says %d bpp but the device is "
						   "palette-managed with %d entries - using %d bpp\n"),
					depth, sizepalette, palDepth);
				depth = palDepth;
			}
		}

		m_bminfo.bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		m_bminfo.bmi.bmiHeader.biWidth       = m_bmrect.right - m_bmrect.left;
		m_bminfo.bmi.bmiHeader.biHeight      = m_bmrect.bottom - m_bmrect.top;
		m_bminfo.bmi.bmiHeader.biPlanes      = (WORD)planes;
		m_bminfo.bmi.bmiHeader.biBitCount    = (WORD)depth;
		m_bminfo.bmi.bmiHeader.biCompression = BI_RGB;
		m_bminfo.bmi.bmiHeader.biSizeImage   = 0;
		m_bminfo.bmi.bmiHeader.biClrUsed     = 0;
		m_bminfo.bmi.bmiHeader.biClrImportant = 0;

		vnclog.Print(LL_INTINFO,
			VNCLOG("display DC reports %d bpp x %d planes = %d bpp\n"),
			bpp, planes, depth);

		// Note: BI_RGB is correct for the fallback.  SetPixShifts() uses the
		// standard 5-5-5 / 8-8-8 masks when biCompression == BI_RGB, and only
		// reads bmiColors[] for BI_BITFIELDS - which we cannot obtain without
		// the query, and which a Windows 3.1 display driver would not report
		// anyway.
	} else {
		vnclog.Print(LL_INTINFO, VNCLOG("GetDIBits format query succeeded\n"));
	}

	vnclog.Print(LL_INTINFO,
		VNCLOG("format query: %dx%d, %d bpp, %d planes, compression %d, clrused %d\n"),
		(int)m_bminfo.bmi.bmiHeader.biWidth,
		(int)m_bminfo.bmi.bmiHeader.biHeight,
		(int)m_bminfo.bmi.bmiHeader.biBitCount,
		(int)m_bminfo.bmi.bmiHeader.biPlanes,
		(int)m_bminfo.bmi.bmiHeader.biCompression,
		(int)m_bminfo.bmi.bmiHeader.biClrUsed);

	// ------------------------------------------------------------------
	// The second call.  NON-FATAL on this platform.
	//
	// Its purpose is to fill in the colour table (palette display) or the colour
	// masks (16/32-bit display) that follow the header.
	//
	// IMPORTANT FINDING: on the palette path, NOTHING IN THE SERVER READS THAT
	// TABLE.  m_bminfo.cmap has no consumer anywhere - the 256-colour palette the
	// encoder sends to the client is fetched independently by
	// GetSystemPaletteEntries() in tableinitcmtemplate.cpp:51, reached via
	// vncEncoder::GetRemotePalette -> rfbInitColourMapSingleTable8.  (See also
	// vncDesktop::InitPalette, which calls GetSystemPaletteEntries for the memory
	// DC's own palette.)
	//
	// So on an 8-bit display this call is pure ceremony, and failing it should not
	// stop the server.  On a truecolour display it does matter - biCompression
	// comes back as BI_BITFIELDS and the three colour masks follow the header, and
	// SetPixFormat/ThunkBitmapInfo use them - so the failure is still logged, and
	// treated as fatal only in that case.
	// ------------------------------------------------------------------
	//
	// SKIP IT ENTIRELY if the first call was unsupported: it is the same API with
	// the same NULL lpvBits pointer, so it will fail the same way, and its output
	// is unused on a palette display.  Calling it would only add a second scary
	// error line to the log.
	if (result != 0) {
		result = ::GetDIBits(m_hmemdc, m_membitmap, 0, 1, NULL,
							 &m_bminfo.bmi, DIB_RGB_COLORS);
		if (result == 0) {
			vnclog.Print(LL_INTERR,
				VNCLOG("GetDIBits (colour table) failed, error=%d "
					   "(%d bpp, biClrUsed=%d)\n"),
				GetLastError(),
				(int)m_bminfo.bmi.bmiHeader.biBitCount,
				(int)m_bminfo.bmi.bmiHeader.biClrUsed);

			if (m_bminfo.bmi.bmiHeader.biBitCount > 8 &&
				m_bminfo.bmi.bmiHeader.biCompression == BI_BITFIELDS) {
				// Truecolour with a non-standard layout: we genuinely need the
				// masks this call would have written.
				vnclog.Print(LL_INTERR,
					VNCLOG("cannot determine the colour masks - giving up\n"));
				return FALSE;
			}

			// Palette display, or truecolour with BI_RGB (standard masks).
			// Continue: the colour table is unused (see above), and the palette
			// itself comes from GetSystemPaletteEntries.
			vnclog.Print(LL_INTWARN,
				VNCLOG("continuing without the DIB colour table\n"));
		}
	}
	vnclog.Print(LL_INTINFO, VNCLOG("got bitmap format\n"));

	// Sanity-check the depth before anything divides by it.
	//
	// SetPixFormat computes m_bytesPerRow as width*bpp/8 and ScreenBuffSize
	// multiplies by it; a reported 0 bpp would give a zero-size buffer and then a
	// divide-by-zero in the encoders.  On Win32s an integer divide by zero faults
	// the whole VM rather than raising a catchable exception, so check here.
	switch (m_bminfo.bmi.bmiHeader.biBitCount) {
	case 1:
	case 4:
	case 8:
	case 16:
	case 24:
	case 32:
		break;
	default:
		vnclog.Print(LL_INTERR,
			VNCLOG("display reports %d bits per pixel, which is not supported\n"),
			(int)m_bminfo.bmi.bmiHeader.biBitCount);
		return FALSE;
	}

	vnclog.Print(LL_INTINFO, VNCLOG("DBG:display context has %d planes!\n"), GetDeviceCaps(m_hrootdc, PLANES));
	vnclog.Print(LL_INTINFO, VNCLOG("DBG:memory context has %d planes!\n"), GetDeviceCaps(m_hmemdc, PLANES));
	if (GetDeviceCaps(m_hmemdc, PLANES) != 1)
	{
// FIXME: MessageBox in a service
		MessageBox(
			NULL,
			"vncDesktop : current display is PLANAR, not CHUNKY!\n"
			"WinVNC cannot be used with this graphics device driver",
			szAppName,
			MB_ICONSTOP | MB_OK
			);
		return FALSE;
	}

	// Henceforth we want to use a top-down scanning representation.
	//
	// WIN32S NOTE: a NEGATIVE biHeight (top-down DIB) is a Windows 95 / NT
	// feature.  The 16-bit Windows 3.1 GDI only understands bottom-up DIBs and
	// treats the height as unsigned, so a negative value here is either ignored
	// or read as an enormous positive height.
	//
	// This is deliberately left as-is, because it is the format the server uses
	// INTERNALLY for its own buffers (see CopyToBuffer / the encoders, which all
	// assume top-down), not a value passed back to GDI for the capture:
	// CaptureScreenFromAdapterGeneral uses a DIBSection created earlier with its
	// own header, or GetDIBits with a header built in
	// vncDesktop::CopyRectToBuffer.  If screen content comes out vertically
	// mirrored on this platform, THIS LINE is the first thing to check.
	m_bminfo.bmi.bmiHeader.biHeight = - abs(m_bminfo.bmi.bmiHeader.biHeight);

	// Is the bitmap palette-based or truecolour?
	//
	// WIN32S: query the ROOT DC, not the memory DC.
	//
	// m_hmemdc is a memory DC created with CreateCompatibleDC(m_hrootdc).  On NT
	// it inherits the display's raster capabilities, so RC_PALETTE is reported
	// correctly.  On Win32s a memory DC's RASTERCAPS is not reliably inherited -
	// it can come back without RC_PALETTE even on a 256-colour display, which
	// makes the server declare a palette display "truecolour", skip SetPalette()
	// entirely (the log then says "no palette data for truecolour display") and
	// send the client 8-bit pixel indices as though they were RGB values.
	//
	// The display DC is the authority on what the display is.
	m_bminfo.truecolour = (GetDeviceCaps(m_hrootdc, RASTERCAPS) & RC_PALETTE) == 0;

	vnclog.Print(LL_INTINFO,
		VNCLOG("bitmap is %s (root RC_PALETTE=%d, mem RC_PALETTE=%d)\n"),
		m_bminfo.truecolour ? "truecolour" : "palette-based",
		(GetDeviceCaps(m_hrootdc, RASTERCAPS) & RC_PALETTE) ? 1 : 0,
		(GetDeviceCaps(m_hmemdc, RASTERCAPS) & RC_PALETTE) ? 1 : 0);

	return TRUE;
}


BOOL
vncDesktop::ThunkBitmapInfo()
{
	// If we leave the pixel format intact, the blits can be optimised (Will Dean's patch)
	m_formatmunged = FALSE;

	// HACK ***.  Optimised blits don't work with palette-based displays, yet
	if (!m_bminfo.truecolour) {
		m_formatmunged = TRUE;
	}

	// Attempt to force the actual format into one we can handle
	// We can handle 8-bit-palette and 16/32-bit-truecolour modes
	switch (m_bminfo.bmi.bmiHeader.biBitCount)
	{
	case 1:
	case 4:
		vnclog.Print(LL_INTINFO, VNCLOG("DBG:used/bits/planes/comp/size "
										"= %d/%d/%d/%d/%d\n"),
					 (int)m_bminfo.bmi.bmiHeader.biClrUsed,
					 (int)m_bminfo.bmi.bmiHeader.biBitCount,
					 (int)m_bminfo.bmi.bmiHeader.biPlanes,
					 (int)m_bminfo.bmi.bmiHeader.biCompression,
					 (int)m_bminfo.bmi.bmiHeader.biSizeImage);
		
		// Correct the BITMAPINFO header to the format we actually want
		m_bminfo.bmi.bmiHeader.biClrUsed = 0;
		m_bminfo.bmi.bmiHeader.biPlanes = 1;
		m_bminfo.bmi.bmiHeader.biCompression = BI_RGB;
		m_bminfo.bmi.bmiHeader.biBitCount = 8;
		m_bminfo.bmi.bmiHeader.biSizeImage =
			abs((m_bminfo.bmi.bmiHeader.biWidth *
				m_bminfo.bmi.bmiHeader.biHeight *
				m_bminfo.bmi.bmiHeader.biBitCount)/ 8);
		m_bminfo.bmi.bmiHeader.biClrImportant = 0;
		m_bminfo.truecolour = FALSE;

		// Display format is non-VNC compatible - use the slow blit method
		m_formatmunged = TRUE;
		break;	
	case 24:
		// Update the bitmapinfo header
		m_bminfo.bmi.bmiHeader.biBitCount = 32;
		m_bminfo.bmi.bmiHeader.biPlanes = 1;
		m_bminfo.bmi.bmiHeader.biCompression = BI_RGB;
		m_bminfo.bmi.bmiHeader.biSizeImage =
			abs((m_bminfo.bmi.bmiHeader.biWidth *
				m_bminfo.bmi.bmiHeader.biHeight *
				m_bminfo.bmi.bmiHeader.biBitCount)/ 8);
		// Display format is non-VNC compatible - use the slow blit method
		m_formatmunged = TRUE;
		break;
	}

	return TRUE;
}

BOOL
vncDesktop::SetPixFormat()
{
	// Examine the bitmapinfo structure to obtain the current pixel format
	m_scrinfo.format.trueColour = m_bminfo.truecolour;
	m_scrinfo.format.bigEndian = 0;

	// Set up the native buffer width, height and format
	m_scrinfo.framebufferWidth = (CARD16) (m_bmrect.right - m_bmrect.left);		// Swap endian before actually sending
	m_scrinfo.framebufferHeight = (CARD16) (m_bmrect.bottom - m_bmrect.top);	// Swap endian before actually sending
	m_scrinfo.format.bitsPerPixel = (CARD8) m_bminfo.bmi.bmiHeader.biBitCount;
	m_scrinfo.format.depth        = (CARD8) m_bminfo.bmi.bmiHeader.biBitCount;

	
	// Calculate the number of bytes per row.
	//
	// WIN32S: DIB scanlines are DWORD-ALIGNED.  This calculation is not.
	//
	// For a 1024-wide 8-bit screen (1024 bytes) the two agree, which is why this
	// has never been noticed - and 16/32bpp widths are always a multiple of 4
	// bytes too.  But at 8bpp any width that is not a multiple of 4 - 1000, 1022,
	// 800 is fine, 1002 is not - makes GetDIBits write padded rows into a buffer
	// indexed with unpadded arithmetic, and the image shears progressively down
	// the screen.
	//
	// Round up to the next DWORD boundary, which is what GDI actually does.
	m_bytesPerRow = m_scrinfo.framebufferWidth * m_scrinfo.format.bitsPerPixel / 8;
	m_bytesPerRow = (m_bytesPerRow + 3) & ~3;

	// WIN32S: if the bitmap can tell us its real stride, use that instead of
	// computing it.
	//
	// GetObject(hbm, ..., &bm) reports bmWidthBytes, which is the ACTUAL scanline
	// stride the driver chose - already aligned, and correct even if the driver
	// pads more than the DWORD minimum.  Guessing the stride is one of the
	// classic ways to get a sheared image, so prefer the authoritative value.
	//
	// Only accept it if it is at least as large as our computed minimum: a
	// smaller value would mean we had the depth wrong, and using it would
	// under-read every row.
	//
	// NOTE: this must agree with what ThunkBitmapInfo() may have done to
	// biBitCount (24 -> 32), so it is read AFTER that has run - hence taking the
	// value here rather than caching it in InitBitmap.
	if (m_membitmap != NULL) {
		BITMAP bm;
		memset(&bm, 0, sizeof(bm));
		if (GetObject(m_membitmap, sizeof(BITMAP), &bm) == sizeof(BITMAP) &&
			bm.bmWidthBytes > 0) {
			// Only trust it when the depth still matches the bitmap's own - if
			// ThunkBitmapInfo rewrote 24bpp to 32bpp, the bitmap's stride is for
			// 24bpp and does not apply to the buffer we are about to fill.
			if ((int)bm.bmBitsPixel == (int)m_scrinfo.format.bitsPerPixel) {
				if (bm.bmWidthBytes >= m_bytesPerRow) {
					if (bm.bmWidthBytes != m_bytesPerRow) {
						vnclog.Print(LL_INTINFO,
							VNCLOG("using the bitmap's own stride %d "
								   "(computed %d)\n"),
							(int)bm.bmWidthBytes, (int)m_bytesPerRow);
					}
					m_bytesPerRow = bm.bmWidthBytes;
				} else {
					vnclog.Print(LL_INTWARN,
						VNCLOG("bitmap stride %d is smaller than the computed %d "
							   "- keeping the computed value\n"),
						(int)bm.bmWidthBytes, (int)m_bytesPerRow);
				}
			} else {
				vnclog.Print(LL_INTINFO,
					VNCLOG("bitmap is %d bpp but the buffer format is %d bpp "
						   "(munged) - using the computed stride %d\n"),
					(int)bm.bmBitsPixel, (int)m_scrinfo.format.bitsPerPixel,
					(int)m_bytesPerRow);
			}
		}
	}

	vnclog.Print(LL_INTINFO,
		VNCLOG("pixel format: %d bpp, truecolour=%d, %d bytes per row\n"),
		(int)m_scrinfo.format.bitsPerPixel,
		(int)m_scrinfo.format.trueColour,
		(int)m_bytesPerRow);

	return TRUE;
}

BOOL
vncDesktop::SetPixShifts()
{
	// Sort out the colour shifts, etc.
	DWORD redMask=0, blueMask=0, greenMask = 0;

	switch (m_bminfo.bmi.bmiHeader.biBitCount)
	{
	case 16:
		// (Was: a special case for the Mirage driver, whose colour mask is
		// always 565.  No driver on this platform - see InitVideoDriver.)
		if (m_bminfo.bmi.bmiHeader.biCompression == BI_RGB)
		{
		// Standard 16-bit display
		// each word single pixel 5-5-5
			redMask = 0x7c00; greenMask = 0x03e0; blueMask = 0x001f;
		}
		else
		{
			if (m_bminfo.bmi.bmiHeader.biCompression == BI_BITFIELDS)
			{
				redMask =   *(DWORD *) &m_bminfo.bmi.bmiColors[0];
				greenMask = *(DWORD *) &m_bminfo.bmi.bmiColors[1];
				blueMask =  *(DWORD *) &m_bminfo.bmi.bmiColors[2];
			}
		}
		break;

	case 32:
		// Standard 24/32 bit displays
		if (m_bminfo.bmi.bmiHeader.biCompression == BI_RGB)
		{
			redMask = 0xff0000;
			greenMask = 0xff00;
			blueMask = 0x00ff;

			// The real color depth is 24 bits in this case. If the depth
			// is set to 32, the Tight encoder shows worse performance.
			m_scrinfo.format.depth = 24;
		}
		else
		{
			if (m_bminfo.bmi.bmiHeader.biCompression == BI_BITFIELDS)
			{
				redMask =   *(DWORD *) &m_bminfo.bmi.bmiColors[0];
				greenMask = *(DWORD *) &m_bminfo.bmi.bmiColors[1];
				blueMask =  *(DWORD *) &m_bminfo.bmi.bmiColors[2];
			}
		}
		break;

	default:
		// Other pixel formats are only valid if they're palette-based
		if (m_bminfo.truecolour)
		{
			vnclog.Print(LL_INTERR, "unsupported truecolour pixel format for SetPixShifts()\n");
			return FALSE;
		}
		vnclog.Print(LL_INTINFO, VNCLOG("DBG:palette-based desktop in SetPixShifts()\n"));
		return TRUE;
	}

	// Convert the data we just retrieved
	MaskToMaxAndShift(redMask, m_scrinfo.format.redMax, m_scrinfo.format.redShift);
	MaskToMaxAndShift(greenMask, m_scrinfo.format.greenMax, m_scrinfo.format.greenShift);
	MaskToMaxAndShift(blueMask, m_scrinfo.format.blueMax, m_scrinfo.format.blueShift);

	vnclog.Print(LL_INTINFO, VNCLOG("DBG:true-color desktop in SetPixShifts()\n"));
	return TRUE;
}

BOOL
vncDesktop::SetPalette()
{
	// Lock the current display palette into the memory DC we're holding
	// *** CHECK THIS FOR LEAKS!
	if (!m_bminfo.truecolour)
	{
		LOGPALETTE *palette;
		UINT size = sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 256);

		palette = (LOGPALETTE *) new char[size];
		if (palette == NULL) {
			vnclog.Print(LL_INTERR, VNCLOG("error allocating palette\n"));
			return FALSE;
		}

		// Initialise the structure
		palette->palVersion = 0x300;
		palette->palNumEntries = 256;

		// Get the system colours
		if (GetSystemPaletteEntries(m_hrootdc, 0, 256, palette->palPalEntry) == 0)
		{
			vnclog.Print(LL_INTERR, VNCLOG("GetSystemPaletteEntries() failed.\n"));
			delete [] palette;
			return FALSE;
		}

		// Create a palette from those
		HPALETTE pal = CreatePalette(palette);
		if (pal == NULL)
		{
			vnclog.Print(LL_INTERR, VNCLOG("CreatePalette() failed.\n"));
			delete [] palette;
			return FALSE;
		}

		// Select the palette into our memory DC
		HPALETTE oldpalette = SelectPalette(m_hmemdc, pal, FALSE);
		if (oldpalette == NULL)
		{
			vnclog.Print(LL_INTERR, VNCLOG("SelectPalette() failed.\n"));
			delete [] palette;
			DeleteObject(pal);
			return FALSE;
		}

		// Worked, so realise the palette
		if (RealizePalette(m_hmemdc) == GDI_ERROR)
			vnclog.Print(LL_INTWARN, VNCLOG("warning - failed to RealizePalette\n"));

		// It worked!
		delete [] palette;
		DeleteObject(oldpalette);

		vnclog.Print(LL_INTINFO, VNCLOG("initialised palette OK\n"));
		return TRUE;
	}

	// Not a palette based local screen - forget it!
	vnclog.Print(LL_INTINFO, VNCLOG("no palette data for truecolour display\n"));
	return TRUE;
}

LRESULT CALLBACK DesktopWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

ATOM m_wndClass = 0;

BOOL
vncDesktop::InitWindow()
{
	if (m_wndClass == 0) {
		// Create the window class
		// WIN32S: plain WNDCLASS / RegisterClass, not WNDCLASSEX /
		// RegisterClassEx.
		//
		// RegisterClassEx is a Windows 95/NT API and is not exported by the
		// Win32s USER32 stub set.  Because the linker records it as an import,
		// the EXE cannot be LOADED on Win32s at all - the process never starts
		// and there is no diagnostic.  (This is the same failure that stopped
		// the viewer launching; see Daemon.cpp there.)
		//
		// The only thing WNDCLASSEX adds is hIconSm, which this window sets to
		// NULL anyway and which Windows 3.1 has no concept of, so nothing is
		// lost.
		//
		// This matches roytam1's winvnc333r9-vc4 patch 94f9037, which made the
		// same change for NT 3.50.
		WNDCLASS wndclass;

		wndclass.style			= 0;
		wndclass.lpfnWndProc	= &DesktopWndProc;
		wndclass.cbClsExtra		= 0;
		wndclass.cbWndExtra		= 0;
		wndclass.hInstance		= hAppInstance;
		wndclass.hIcon			= NULL;
		wndclass.hCursor		= NULL;
		wndclass.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName	= (const char *) NULL;
		wndclass.lpszClassName	= szDesktopSink;

		// Register it
		m_wndClass = RegisterClass(&wndclass);
	}

	// And create a window
	m_hwnd = CreateWindow(szDesktopSink,
				"WinVNC",
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT,
				CW_USEDEFAULT,
				400, 200,
				NULL,
				NULL,
				hAppInstance,
				NULL);

	if (m_hwnd == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("CreateWindow() failed.\n"));
		return FALSE;
	}

	// Set the "this" pointer for the window
	SetWindowLong(m_hwnd, GWL_USERDATA, (long)this);

	// Enable clipboard hooking
	m_hnextviewer = SetClipboardViewer(m_hwnd);

	return TRUE;
}

BOOL
vncDesktop::CreateBuffers()
{
	vnclog.Print(LL_INTINFO, VNCLOG("attempting to create main and back buffers\n"));

	// Create a new DIB section ***
	//
	// WIN32S NOTE - THIS IS THE MOST IMPORTANT PERFORMANCE PATH IN THE SERVER.
	//
	// CreateDIBSection gives the server DIRECT pointer access to the bitmap bits
	// (m_DIBbits), so a screen capture is BitBlt-to-memory-DC followed by a plain
	// memory read.  The fallback is GetDIBits, which copies the whole rectangle
	// through GDI on every single capture - and on Win32s every GDI call is a
	// 32->16 bit thunk.  With polling as the only change-detection mechanism (no
	// hooks, no mirror driver), the difference is the difference between a usable
	// server and an unusable one.
	//
	// CreateDIBSection EXISTS on Win32s: it is a Windows 3.1 GDI function (added
	// in 3.1 alongside the DIB.DRV work), so there is no import problem.  It can
	// still FAIL for a large screen, because the section comes out of the shared
	// 16-bit GDI heap - which is exactly why the GetDIBits fallback below is kept
	// rather than made fatal.
	//
	// If the log says "reverting to slow blits" on the target machine, that is the
	// first thing to investigate for poor frame rates: reducing the server's
	// colour depth or screen resolution frees enough GDI heap to get the fast
	// path back.
	HBITMAP tempbitmap = NULL;
	if (!m_formatmunged)
	{
		// WIN32S: CreateDIBSection also cannot take a negative biHeight.
		//
		// A top-down DIB section is a Win95/NT feature; the 16-bit GDI rejects the
		// header exactly as GetDIBits does (see the long note in CopyToBuffer).
		//
		// This matters more than it looks: if this call fails, the server falls
		// back to the slow GetDIBits-per-region path for the whole session, so on
		// an 8-bit display - where m_formatmunged is FALSE and this path WOULD be
		// taken - a rejected header costs the fast capture path permanently.
		//
		// Request bottom-up here.  Note the consequence: m_DIBbits then holds
		// BOTTOM-UP data, so the fast-blit branch of CopyToBuffer would need to
		// read its rows in reverse.  Rather than silently produce an upside-down
		// image, the fast path is disabled on this platform - see below.
		LONG savedHeight = m_bminfo.bmi.bmiHeader.biHeight;
		BOOL wasTopDown = (savedHeight < 0);

		if (wasTopDown && vncService::IsWin32s())
			m_bminfo.bmi.bmiHeader.biHeight = -savedHeight;

		tempbitmap = CreateDIBSection(
			m_hmemdc,
			&m_bminfo.bmi,
			DIB_RGB_COLORS,
			&m_DIBbits,
			NULL,
			0);

		m_bminfo.bmi.bmiHeader.biHeight = savedHeight;

		if (tempbitmap == NULL)
		{
			vnclog.Print(LL_INTWARN, VNCLOG("failed to build DIB section (error %d) - reverting to slow blits\n"),
						 GetLastError());
		}
		else if (wasTopDown && vncService::IsWin32s())
		{
			// The section exists but its rows are BOTTOM-UP.
			//
			// This is worth keeping rather than discarding: a DIB section gives
			// the server direct pointer access to the bits (m_DIBbits), so a
			// capture becomes BitBlt + memcpy instead of a GetDIBits call per
			// region.  On Win32s, where every GDI call is a 32->16 bit thunk and
			// polling is the only change-detection mechanism, that is the single
			// largest performance difference available.
			//
			// The consequence is recorded in m_DIBbits_bottom_up, and the
			// fast-blit branch of CopyToBuffer inverts the row index when it is
			// set.
			//
			// (The three-argument CopyToBuffer(rect, dest, src) overload needs no
			// change: it has NO CALLERS - verified by grep - and its 'src' is a
			// caller-supplied buffer rather than m_DIBbits.)
			vnclog.Print(LL_INTINFO,
				VNCLOG("DIB section is bottom-up on this platform - "
					   "rows will be inverted on copy\n"));
			m_DIBbits_bottom_up = TRUE;
		}
		else
		{
			m_DIBbits_bottom_up = FALSE;
		}
	}

	m_freemainbuff = false;

// NOTE m_mainbuff and m_backbuff allocation can not be supressed
// even with direct access mirror surface view

	if (tempbitmap == NULL)
	{
		m_DIBbits = NULL;
		// No DIB section, so the bottom-up flag is meaningless - clear it so a
		// later re-init cannot inherit a stale TRUE.
		m_DIBbits_bottom_up = FALSE;
		// create our own buffer to copy blits through
		if ((m_mainbuff = new BYTE [ScreenBuffSize()]) == NULL) {
				vnclog.Print(LL_INTERR, VNCLOG("unable to allocate main buffer[%d]\n"), ScreenBuffSize());
				return FALSE;
		}
		m_freemainbuff = true;
		if ((m_backbuff = new BYTE [ScreenBuffSize()]) == NULL) {
			vnclog.Print(LL_INTERR, VNCLOG("unable to allocate back buffer[%d]\n"), ScreenBuffSize());
			return FALSE;
		}
		return TRUE;
	}
	
	// Create our own buffer to copy blits through
	if ((m_backbuff = new BYTE [ScreenBuffSize()]) == NULL) {
		vnclog.Print(LL_INTERR, VNCLOG("unable to allocate back buffer[%d]\n"), ScreenBuffSize());
		if (tempbitmap!= NULL)
			DeleteObject(tempbitmap);
		return FALSE;
	}

	// Delete the old memory bitmap
	if (m_membitmap != NULL) {
		DeleteObject(m_membitmap);
		m_membitmap = NULL;
	}

	// Replace old membitmap with DIB section
	m_membitmap = tempbitmap;
	m_mainbuff = (BYTE *)m_DIBbits;
	vnclog.Print(LL_INTINFO, VNCLOG("enabled fast DIBsection blits OK\n"));
	return TRUE;
}

BOOL
vncDesktop::Init(vncServer *server)
{
	vnclog.Print(LL_INTINFO, VNCLOG("initialising desktop server\n"));

	// Save the server pointer
	m_server = server;

	// Load in the arrow cursor
	m_hdefcursor = LoadCursor(NULL, IDC_ARROW);
	m_hcursor = m_hdefcursor;

	// ------------------------------------------------------------------
	// WIN32S: run Startup() directly instead of spawning a thread.
	//
	// Was:
	//     vncDesktopThread *thread = new vncDesktopThread;
	//     m_thread = thread;
	//     return thread->Init(this, m_server);
	//
	// vncDesktopThread::Init() called start_undetached() and then blocked on an
	// omni_condition waiting for the new thread to report success.  With one
	// thread that is an unconditional deadlock - the condition can only ever be
	// signalled by the thread that is sitting in the wait.
	//
	// Startup() is what the thread did first anyway, so call it here.  The rest
	// of what the thread did (the polling loop) is now PumpIdle(), and the sink
	// window's messages are dispatched by the application's message loop into
	// DesktopWndProc.
	// ------------------------------------------------------------------
	if (!Startup())
	{
		// Startup may have changed the video mode in
		// SetupDisplayForConnection() before failing; undo that.  This mirrors
		// what vncDesktopThread::run_undetached did on its failure path.
		ResetDisplayToNormal();
		return FALSE;
	}

	// The old thread did this immediately after a successful Startup().
	RECT rect = GetSourceRect();
	IntersectRect(&rect, &rect, &m_bmrect);
	m_server->SetSharedRect(rect);

	// It is now safe to handle clipboard messages.
	SetClipboardActive(TRUE);

	m_shutdown_requested = FALSE;

	return TRUE;
}

// ==========================================================================
// PumpIdle - idle-time driver for the desktop.
//
// This is the "message queue is empty" branch of the old
// vncDesktopThread::run_undetached loop:
//
//     if (!PeekMessage(...)) {
//         if (!m_server->WallpaperWait())
//             if (!m_desktop->CheckUpdates())
//                 break;
//         WaitMessage();
//     }
//
// The application idle loop calls this when it has no messages left to
// dispatch, so the PeekMessage test is implicit and the WaitMessage() belongs to
// the caller.
//
// Returns FALSE when the desktop wants to shut down - CheckUpdates() returns
// FALSE when the screen format changed in a way that requires a restart, or when
// the last client has gone.
// ==========================================================================
BOOL
vncDesktop::PumpIdle()
{
	if (m_shutdown_requested)
		return FALSE;

	// Wait for scheduled wallpaper removal to complete before looking for
	// changes, exactly as the old loop did.
	if (m_server->WallpaperWait())
		return TRUE;

	// WIN32S DIAGNOSTIC: is this being reached, and what does it see?
	//
	// The log shows FramebufferUpdateRequest arriving and the encoder reporting
	// "data=0, encoded=0, sent=0", which means SendUpdate() either never ran or
	// found nothing to send.  This narrows it down: if these counters stay at
	// zero, the polling flag is never set and PerformPolling() never runs; if
	// they climb but "rects" stays 0, the change detection finds nothing; if
	// "rects" climbs, the problem is downstream in SendUpdate.
	//
	// Rate-limited to once a second so it cannot flood the log.
	{
		static DWORD lastReport = 0;
		static DWORD pumpCount = 0;
		static DWORD pollCount = 0;
		pumpCount++;
		if (GetPollingFlag())
			pollCount++;

		DWORD now = GetTickCount();
		if (lastReport == 0)
			lastReport = now;
		if ((DWORD)(now - lastReport) >= 5000) {
			// NOTE: m_changed_rgn is sampled here at the START of a pass, but
			// CheckUpdates() CLEARS it at the end of every pass - so this always
			// read 0 and told us nothing useful.  What matters is whether the
			// region is EMPTY (region == NULL) versus merely already-consumed, so
			// report that instead of a rectangle count.
			vnclog.Print(LL_INTERR,
				VNCLOG("pump: %d passes, %d with poll flag, changed_rgn empty=%d, "
					   "full=%d incr=%d\n"),
				(int)pumpCount, (int)pollCount,
				(int)m_changed_rgn.IsEmpty(),
				(int)m_server->FullRgnRequested(),
				(int)m_server->IncrRgnRequested());

			pumpCount = 0;
			pollCount = 0;
			lastReport = now;
		}
	}

	if (!CheckUpdates()) {
		// The desktop is finished with.  Do what the tail of
		// run_undetached did.
		vnclog.Print(LL_INTINFO, VNCLOG("desktop shutting down\n"));
		m_shutdown_requested = TRUE;
		SetClipboardActive(FALSE);
		Shutdown();
		ResetDisplayToNormal();
		BlankScreen(FALSE);
		vncKeymap::ClearShiftKeys();
		return FALSE;
	}

	return TRUE;
}

void
vncDesktop::RequestUpdate()
{
	PostMessage(m_hwnd, WM_TIMER, TIMER_POLL, 0);
}

int
vncDesktop::ScreenBuffSize()
{
	// WIN32S: must use the DWORD-ALIGNED row stride, not width * bytesPerPixel.
	//
	// GetDIBits writes DWORD-aligned scanlines, and CopyToBuffer indexes the
	// buffer with m_bytesPerRow - which is now rounded up to a DWORD boundary
	// (see SetPixFormat).  If this function keeps using the unpadded width, the
	// buffer is too SMALL for the padded rows GDI writes and the last rows of a
	// full-screen capture overrun the allocation.
	//
	// At 1024x768x8 the two agree (1024 is already a multiple of 4), which is why
	// the original never showed it - but an 8-bit mode with a width like 1002
	// would have written 768*2 bytes past the end of the heap block.
	return m_bytesPerRow * m_scrinfo.framebufferHeight;
}

void
vncDesktop::FillDisplayInfo(rfbServerInitMsg *scrinfo)
{
	memcpy(scrinfo, &m_scrinfo, sz_rfbServerInitMsg);
}

// Function to capture an area of the screen immediately prior to sending
// an update.

void vncDesktop::CaptureScreen(RECT &UpdateArea, BYTE *scrBuff)
{
// ASSUME rect related to virtual desktop
	// WIN32S: only one capture path.  CaptureScreenFromMirage() has been removed
	// along with the mirror driver - see the note on InitVideoDriver().
	CaptureScreenFromAdapterGeneral(UpdateArea, scrBuff);
}

void vncDesktop::CaptureScreenFromAdapterGeneral(RECT rect, BYTE *scrBuff)
{
// ASSUME rect related to virtual desktop
	// Protect the memory bitmap
	omni_mutex_lock l(m_bitbltlock);

	// Finish drawing anything in this thread 
	// Wish we could do this for the whole system - maybe we should
	// do something with LockWindowUpdate here.
	GdiFlush();

	// Select the memory bitmap into the memory DC
	HBITMAP oldbitmap;
	if ((oldbitmap = (HBITMAP) SelectObject(m_hmemdc, m_membitmap)) == NULL)
		return;

	// Capture screen into bitmap
	BOOL blitok = BitBlt(
		m_hmemdc,
// source in m_hrootdc is relative to a virtual desktop,
// whereas dst coordinates of m_hmemdc are relative to its top-left corner (0, 0)
		rect.left - m_bmrect.left,
		rect.top - m_bmrect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		m_hrootdc,
		rect.left, rect.top,
		SRCCOPY);

	// Select the old bitmap back into the memory DC
	SelectObject(m_hmemdc, oldbitmap);
	
	if (blitok)
	{
	// Copy the new data to the screen buffer (CopyToBuffer optimises this if possible)
		CopyToBuffer(rect, scrBuff);
	}

}

// CaptureScreenFromMirage() removed: it read directly from the mirror driver's
// shared frame buffer via m_videodriver->GetScreenView().  There is no driver on
// this platform.

void	vncDesktop::CaptureMouseRect()
{
	POINT CursorPos;
	ICONINFO IconInfo;

	// If the mouse cursor handle is invalid then forget it
	if (m_hcursor == NULL)
		return;

	// Get the cursor position
	if (!GetCursorPos(&CursorPos))
		return;

	// Translate position for hotspot
	if (GetIconInfo(m_hcursor, &IconInfo))
	{
		CursorPos.x -= ((int) IconInfo.xHotspot);
		CursorPos.y -= ((int) IconInfo.yHotspot);

		if (IconInfo.hbmMask != NULL)
			DeleteObject(IconInfo.hbmMask);
		if (IconInfo.hbmColor != NULL)
			DeleteObject(IconInfo.hbmColor);
	}

	// Save the bounding rectangle
	m_cursorpos.left = CursorPos.x;
	m_cursorpos.top = CursorPos.y;
	m_cursorpos.right = CursorPos.x + GetSystemMetrics(SM_CXCURSOR);
	m_cursorpos.bottom = CursorPos.y + GetSystemMetrics(SM_CYCURSOR);
}

// Add the mouse pointer to the buffer
void vncDesktop::CaptureMouse(BYTE *scrBuff, UINT scrBuffSize)
{
	// Protect the memory bitmap
	omni_mutex_lock l(m_bitbltlock);

	CaptureMouseRect();

	// Select the memory bitmap into the memory DC
	HBITMAP oldbitmap;
	if ((oldbitmap = (HBITMAP) SelectObject(m_hmemdc, m_membitmap)) == NULL)
		return;

	// Draw the cursor
	DrawIconEx(
		m_hmemdc,									// handle to device context 
		m_cursorpos.left - m_bmrect.left,
		m_cursorpos.top - m_bmrect.top,
		m_hcursor,									// handle to icon to draw 
		0,0,										// width of the icon 
		0,											// index of frame in animated cursor 
		NULL,										// handle to background brush 
		DI_NORMAL									// icon-drawing flags 
		);

	// Select the old bitmap back into the memory DC
	SelectObject(m_hmemdc, oldbitmap);

	// Clip the bounding rect to the screen
	RECT screen = m_server->GetSharedRect();
	// Copy the mouse cursor into the screen buffer, if any of it is visible
	if (IntersectRect(&m_cursorpos, &m_cursorpos, &screen))
		CopyToBuffer(m_cursorpos, scrBuff);
}

// Obtain cursor image data in server's local format.
// The length of databuf[] should be at least (width * height * 4).
BOOL
vncDesktop::GetRichCursorData(BYTE *databuf, HCURSOR hcursor, int width, int height)
{
	// Protect the memory bitmap (is it really necessary here?)
	omni_mutex_lock l(m_bitbltlock);

	// Create bitmap, select it into memory DC
	HBITMAP membitmap = CreateCompatibleBitmap(m_hrootdc, width, height);
	if (membitmap == NULL) {
		return FALSE;
	}
	HBITMAP oldbitmap = (HBITMAP) SelectObject(m_hmemdc, membitmap);
	if (oldbitmap == NULL) {
		DeleteObject(membitmap);
		return FALSE;
	}

	// Draw the cursor
	DrawIconEx(m_hmemdc, 0, 0, hcursor, 0, 0, 0, NULL, DI_IMAGE);
	SelectObject(m_hmemdc, oldbitmap);

	// Prepare BITMAPINFO structure (copy most m_bminfo fields)
	//
	// WIN32S: check the allocation.  This runs on the cursor-shape path, which is
	// reached for every pointer-shape change, and calloc CAN fail on a machine
	// with a few megabytes free - the original then memcpy'd 1KB+ through NULL.
	BITMAPINFO *bmi = (BITMAPINFO *)calloc(1, sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));
	if (bmi == NULL) {
		DeleteObject(membitmap);
		return FALSE;
	}
	memcpy(bmi, &m_bminfo.bmi, sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));
	bmi->bmiHeader.biWidth = width;

	// WIN32S: a negative biHeight (top-down DIB) is Win95/NT only - the 16-bit
	// GDI treats the height as unsigned and either ignores the sign or reads it
	// as an enormous positive value, which makes GetDIBits fail.
	//
	// Request a bottom-up DIB there and flip the rows afterwards.  The caller
	// (ReadCursorShape / the RichCursor encoder) expects top-down data.
	BOOL flipRows = FALSE;
	if (vncService::IsWin32s()) {
		bmi->bmiHeader.biHeight = height;		// bottom-up
		flipRows = TRUE;
	} else {
		bmi->bmiHeader.biHeight = -height;		// top-down
	}

	// Clear data buffer and extract RGB data
	memset(databuf, 0x00, width * height * 4);
	int lines = GetDIBits(m_hmemdc, membitmap, 0, height, databuf, bmi, DIB_RGB_COLORS);

	if (lines == 0) {
		vnclog.Print(LL_INTERR,
			VNCLOG("GetDIBits (cursor) failed, error=%d\n"), GetLastError());
	} else if (flipRows) {
		// Reverse the row order in place: row i <-> row (height-1-i).
		// 4 bytes per pixel, as the memset above assumes.
		DWORD rowBytes = (DWORD)width * 4;
		BYTE *tmpRow = new BYTE[rowBytes];
		if (tmpRow != NULL) {
			int i;
			for (i = 0; i < height / 2; i++) {
				BYTE *top = (BYTE *)databuf + (DWORD)i * rowBytes;
				BYTE *bot = (BYTE *)databuf + (DWORD)(height - 1 - i) * rowBytes;
				memcpy(tmpRow, top, rowBytes);
				memcpy(top, bot, rowBytes);
				memcpy(bot, tmpRow, rowBytes);
			}
			delete [] tmpRow;
		}
	}

	// Cleanup
	free(bmi);
	DeleteObject(membitmap);

	return (lines != 0);
}

// Return the current mouse pointer position
RECT
vncDesktop::MouseRect()
{
	return m_cursorpos;
}

void vncDesktop::SetCursor(HCURSOR cursor)
{
	if (cursor == NULL)
		m_hcursor = m_hdefcursor;
	else
		m_hcursor = cursor;
}

//
// DEBUG: Some visualization for LF->CRLF conversion code.
//
/*
static void ShowClipText(char *caption, char *text)
{
	int len = strlen(text);
	char *out_text = new char[len * 4 + 8];
	int pos = 0;

	out_text[pos++] = '"';
	for (int i = 0; i < len; i++) {
		if (text[i] == '\r') {
			strcpy(&out_text[pos], "\\r");
			pos += 2;
		} else if (text[i] == '\n') {
			strcpy(&out_text[pos], "\\n");
			pos += 2;
		} else if (text[i] < ' ') {
			strcpy(&out_text[pos], "\\?");
			pos += 2;
		} else {
			out_text[pos++] = text[i];
		}
	}
	out_text[pos++] = '"';
	out_text[pos++] = '\0';

	MessageBox(NULL, out_text, caption, MB_OK);

	delete[] out_text;
}
*/

//
// Convert text from Unix (LF only) format to CR+LF.
// NOTE: The size of dst[] buffer must be at least (strlen(src) * 2 + 1).
//

void
vncDesktop::ConvertClipText(char *dst, const char *src)
{
	const char *ptr0 = src;
	const char *ptr1;
	int dst_pos = 0;

	while ((ptr1 = strchr(ptr0, '\n')) != NULL) {
		// Copy the string before the LF
		if (ptr1 != ptr0) {
			memcpy(&dst[dst_pos], ptr0, ptr1 - ptr0);
			dst_pos += ptr1 - ptr0;
		}
		// Don't insert CR if there is one already
		if (ptr1 == ptr0 || *(ptr1 - 1) != '\r') {
			dst[dst_pos++] = '\r';
		}
		// Append LF
		dst[dst_pos++] = '\n';
		// Next position in the source text
		ptr0 = ptr1 + 1;
	}
	// Copy the last string with no LF, but with '\0'
	memcpy(&dst[dst_pos], ptr0, &src[strlen(src)] - ptr0 + 1);
}

//
// Manipulation of the clipboard
//

void
vncDesktop::SetClipText(LPSTR text)
{
	// Open the system clipboard
	if (OpenClipboard(m_hwnd))
	{
		// Empty it
		if (EmptyClipboard())
		{
			HANDLE hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
									  strlen(text) * 2 + 1);

			if (hMem != NULL)
			{
				LPSTR pMem = (char*)GlobalLock(hMem);

				// Get the data (with line endings converted to CR+LF)
				ConvertClipText(pMem, text);

				// Tell the clipboard
				GlobalUnlock(hMem);
				SetClipboardData(CF_TEXT, hMem);
			}
		}
	}

	// Now close it
	CloseClipboard();
}

// INTERNAL METHODS

inline void
vncDesktop::MaskToMaxAndShift(DWORD mask, CARD16 &max, CARD8 &shift)
{
	for (shift = 0; (mask & 1) == 0; shift++)
		mask >>= 1;
	max = (CARD16) mask;
}

// Copy data from the memory bitmap into a buffer
void vncDesktop::CopyToBuffer(RECT rect, BYTE *destbuff)
{
	// Are we being asked to blit from the DIBsection to itself?
	if (destbuff == m_DIBbits)
	{
		// Yes.  Ignore the request!
		return;
	}

	// Protect the memory bitmap
	omni_mutex_lock l(m_bitbltlock);

	const int crect_re_vd_left = rect.left - m_bmrect.left;
	const int crect_re_vd_top = rect.top - m_bmrect.top;
	_ASSERTE(crect_re_vd_left >= 0);
	_ASSERTE(crect_re_vd_top >= 0);

	// Calculate the scanline-ordered y position to copy from
// NB: m_membitmap is bottom2top
	const int y_inv_re_vd = m_bmrect.bottom - m_bmrect.top - rect.bottom;
	_ASSERTE(y_inv_re_vd >= 0);

	// Calculate where in the output buffer to put the data
	BYTE * destbuffpos = destbuff + (m_bytesPerRow * crect_re_vd_top);

	// Set the number of bytes for GetDIBits to actually write
	// NOTE : GetDIBits pads the destination buffer if biSizeImage < no. of bytes required
	m_bminfo.bmi.bmiHeader.biSizeImage = (rect.bottom-rect.top) * m_bytesPerRow;

	// Get the actual bits from the bitmap into the bit buffer
	// If fast (DIBsection) blits are disabled then use the old GetDIBits technique
	if (m_DIBbits == NULL)
	{
		// ==============================================================
		// WIN32S: GetDIBits MUST NOT BE CALLED WITH THE BITMAP STILL
		// SELECTED INTO A DC.
		//
		// This is why nothing displays: every capture returns error 87.
		//
		// The rule (documented, and enforced by the 16-bit GDI but not by NT):
		// GetDIBits fails if the bitmap passed to it is selected into ANY device
		// context.  vncDesktop selects m_membitmap into m_hmemdc during
		// CaptureScreenFromAdapterGeneral's BitBlt, and although that function
		// selects the old bitmap back afterwards, m_hmemdc is ALSO the DC passed
		// to GetDIBits here - and on Win32s the 16-bit layer treats "hdc has a
		// non-stock bitmap selected" as invalid regardless of which bitmap it is.
		//
		// NT is permissive about this, which is why the original code has worked
		// for 25 years.
		//
		// The fix is to hand GetDIBits a DC that has NO bitmap selected.  A
		// screen-compatible memory DC created for the purpose is exactly right:
		// GetDIBits only needs the DC for its palette and colour context, not for
		// the bitmap itself.
		//
		// The DC is created once and cached in m_hgetdibdc rather than per call -
		// this is the hot path, once per changed region, and CreateCompatibleDC on
		// Win32s draws from the small 16-bit DC pool.
		// ==============================================================
		if (m_hgetdibdc == NULL) {
			m_hgetdibdc = CreateCompatibleDC(m_hrootdc);
			if (m_hgetdibdc == NULL) {
				vnclog.Print(LL_INTERR,
					VNCLOG("CreateCompatibleDC for GetDIBits failed, error=%d\n"),
					GetLastError());
			} else if (!m_bminfo.truecolour) {
				// A palette display needs a realised palette in the DC that
				// GetDIBits uses, or the returned indices are meaningless.
				//
				// NOTE: vncDesktop::SetPalette() does NOT keep the HPALETTE it
				// creates - it selects the palette into m_hmemdc and then
				// DeleteObject()s the handle it got back, so there is no
				// m_hpalette member to reuse here.  Build one from the current
				// system palette, exactly as SetPalette does.
				UINT size = sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 256);
				LOGPALETTE *lp = (LOGPALETTE *)new char[size];
				if (lp != NULL) {
					lp->palVersion = 0x300;
					lp->palNumEntries = 256;
					if (GetSystemPaletteEntries(m_hrootdc, 0, 256,
												lp->palPalEntry) != 0) {
						HPALETTE hpal = CreatePalette(lp);
						if (hpal != NULL) {
							HPALETTE hold = SelectPalette(m_hgetdibdc, hpal, FALSE);
							RealizePalette(m_hgetdibdc);
							// Keep hpal selected; release the one it replaced.
							if (hold != NULL)
								DeleteObject(hold);
						}
					}
					delete [] (char *)lp;
				}
			}
		}

		HDC hdcForGetDIBits = (m_hgetdibdc != NULL) ? m_hgetdibdc : m_hmemdc;

		// ==============================================================
		// WIN32S: A NEGATIVE biHeight (TOP-DOWN DIB) IS NOT SUPPORTED.
		//
		// A negative biHeight means "top-down DIB", a Windows 95 / NT addition.
		// The 16-bit GDI has no concept of it and rejects the header as an invalid
		// parameter - error 87 on every capture.  (The evidence was that CURSOR
		// capture worked while SCREEN capture did not on the same DC: the cursor
		// path already sets a positive height for Win32s.)
		//
		// So we must ask for a BOTTOM-UP DIB.  The question is then where the rows
		// land, and this is where my first attempt was wrong:
		//
		//   IT CAPTURED INTO destbuffpos AND THEN REVERSED THE ROWS IN PLACE.
		//
		// That is wrong because the reversal and the destination offset disagree.
		// GetDIBits with a positive height writes the rectangle BOTTOM ROW FIRST,
		// but 'destbuffpos' is the address of the rectangle's TOP row in a
		// top-down buffer.  Reversing after the fact fixes the order of the rows
		// relative to each other, yet the block as a whole is still anchored at
		// the wrong end for any rectangle taller than one row.  A 1-row rectangle
		// was unaffected (nothing to reverse), which is exactly why the
		// single-scanline change detection in PollArea appeared to work - the
		// server correctly noticed that windows had moved - while every real
		// update rectangle was placed incorrectly and the screen never resolved.
		//
		// THE CORRECT APPROACH: capture into a scratch buffer of exactly the
		// rectangle's size, then copy each row to its proper place in the output
		// buffer, inverting the row index as we go.  One extra memcpy per row, no
		// ambiguity about anchoring, and it is obvious what the mapping is:
		//
		//     scratch row i   (i = 0 is the BOTTOM row, bottom-up DIB)
		//        ->  output row (rowCount - 1 - i)   (0 = TOP row, top-down)
		//
		// The scratch buffer is cached across calls (m_flipbuff) because this is
		// the hot path and Win32s allocation is not cheap.
		//
		// NOTE: GetDIBits writes DWORD-ALIGNED rows of biWidth pixels - the FULL
		// bitmap width, not the rectangle width - because biWidth in the header
		// describes the bitmap.  So the scratch stride is m_bytesPerRow, the same
		// as the output, and each row copy is the full row.  That also means the
		// horizontal offset within the row is already correct; only the vertical
		// order needs fixing.
		// ==============================================================
		const int rowCount = rect.bottom - rect.top;
		int gotLines = 0;

		if (m_bminfo.bmi.bmiHeader.biHeight < 0)
		{
			// Bottom-up capture into scratch, then invert row order on the way out.
			LONG savedHeight = m_bminfo.bmi.bmiHeader.biHeight;
			m_bminfo.bmi.bmiHeader.biHeight = -savedHeight;

			DWORD needed = (DWORD)m_bytesPerRow * (DWORD)rowCount;
			if (m_flipbuff == NULL || m_flipbuffsize < needed) {
				if (m_flipbuff != NULL)
					delete [] m_flipbuff;
				m_flipbuff = new BYTE[needed];
				m_flipbuffsize = (m_flipbuff != NULL) ? needed : 0;
			}

			if (m_flipbuff != NULL)
			{
				gotLines = GetDIBits(
					hdcForGetDIBits,
					m_membitmap,
					y_inv_re_vd,
					rowCount,
					m_flipbuff,
					&m_bminfo.bmi,
					DIB_RGB_COLORS);

				if (gotLines != 0)
				{
					// Copy row by row, inverting the vertical order.
					int i;
					for (i = 0; i < rowCount; i++) {
						memcpy(destbuffpos + (DWORD)(rowCount - 1 - i) * m_bytesPerRow,
							   m_flipbuff + (DWORD)i * m_bytesPerRow,
							   m_bytesPerRow);
					}
				}
			}
			else
			{
				static BOOL warnedFlip = FALSE;
				if (!warnedFlip) {
					warnedFlip = TRUE;
					vnclog.Print(LL_INTERR,
						VNCLOG("out of memory for the scanline flip buffer "
							   "(%d bytes) - captures will fail\n"), (int)needed);
				}
			}

			m_bminfo.bmi.bmiHeader.biHeight = savedHeight;
		}
		else
		{
			// Already bottom-up in the header (or a platform that accepts
			// top-down): capture straight into place, as the original did.
			gotLines = GetDIBits(
				hdcForGetDIBits,
				m_membitmap,
				y_inv_re_vd,
				rowCount,
				destbuffpos,
				&m_bminfo.bmi,
				DIB_RGB_COLORS);
		}

		// WIN32S DIAGNOSTIC (temporary): is the capture writing data?
		//
		// GetDIBits returning non-zero means it reported success, but that does
		// not prove it wrote to the address we think it did.  Compare a few bytes
		// of the destination before and after.
		//
		// If "changed=0" with gotLines>0, the call is succeeding but writing
		// somewhere else - which would point at the destination offset or the
		// biSizeImage/stride the header carries.
		{
			static DWORD s_capCalls = 0;
			static DWORD s_capLines = 0;
			static DWORD s_capChanged = 0;
			static DWORD s_lastCapReport = 0;

			s_capCalls++;
			if (gotLines != 0)
				s_capLines++;

			DWORD now = GetTickCount();
			if (s_lastCapReport == 0)
				s_lastCapReport = now;
			if ((DWORD)(now - s_lastCapReport) >= 5000) {
				unsigned long destSample = 0;
				memcpy(&destSample, destbuffpos, 4);
				vnclog.Print(LL_INTERR,
					VNCLOG("capture: %d calls, %d returned data, dest=%08lx "
						   "rows=%d y_inv=%d bpr=%d flip=%d\n"),
					(int)s_capCalls, (int)s_capLines, destSample,
					(int)rowCount, (int)y_inv_re_vd, (int)m_bytesPerRow,
					(int)(m_bminfo.bmi.bmiHeader.biHeight < 0));
				s_capCalls = 0;
				s_capLines = 0;
				s_capChanged = 0;
				s_lastCapReport = now;
			}
		}

		if (gotLines == 0)
		{
			// WIN32S: report this properly.
			//
			// The original only emitted _RPT debug output, which is compiled out
			// of a release build - so a failing capture produced a blank or
			// stale screen with no explanation anywhere.  This is the hot path
			// (once per changed region), so rate-limit the message: log the first
			// failure and then every 100th, otherwise a persistent failure fills
			// the log faster than anything else in the server.
			static DWORD failCount = 0;
			if ((failCount++ % 100) == 0) {
				vnclog.Print(LL_INTERR,
					VNCLOG("GetDIBits (capture) failed, error=%d "
						   "(y=%d height=%d bpp=%d) [%d occurrences]\n"),
					GetLastError(), y_inv_re_vd, (rect.bottom - rect.top),
					(int)m_bminfo.bmi.bmiHeader.biBitCount,
					(int)failCount);
			}
#ifdef _MSC_VER
			_RPT1(_CRT_WARN, "vncDesktop : [1] GetDIBits failed! %d\n", GetLastError());
			_RPT2(_CRT_WARN, "vncDesktop : y = %d, height = %d\n", y_inv_re_vd, (rect.bottom-rect.top));
#endif
		}
	}
	else
	{
		// Fast blits are enabled.  [I have a sneaking suspicion this will never get used, unless
		// something weird goes wrong in the code.  It's here to keep the function general, though!]
		//
		// WIN32S: on this platform it IS used, and the DIB section's rows are
		// BOTTOM-UP (see the note where the section is created - the 16-bit GDI
		// cannot produce a top-down section).  m_DIBbits_bottom_up records that.

		const int bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
		const int widthBytes = (rect.right - rect.left) * bytesPerPixel;
		const int rowCount = rect.bottom - rect.top;

		destbuffpos += bytesPerPixel * crect_re_vd_left;

		if (m_DIBbits_bottom_up)
		{
			// The section's row 0 is the BOTTOM of the bitmap.  The bitmap is
			// (m_bmrect.bottom - m_bmrect.top) rows tall, so the section row
			// holding output row 'crect_re_vd_top + n' is:
			//
			//     bitmapHeight - 1 - (crect_re_vd_top + n)
			//
			// Walk the source DOWNWARDS while the destination walks upwards.
			const int bitmapHeight = m_bmrect.bottom - m_bmrect.top;
			BYTE *srcbase = (BYTE *)m_DIBbits + (bytesPerPixel * crect_re_vd_left);

			int n;
			for (n = 0; n < rowCount; n++)
			{
				const int srcRow = bitmapHeight - 1 - (crect_re_vd_top + n);
				if (srcRow < 0)
					break;			// defensive: should not happen
				memcpy(destbuffpos + (DWORD)n * m_bytesPerRow,
					   srcbase + (DWORD)srcRow * m_bytesPerRow,
					   widthBytes);
			}
		}
		else
		{
			BYTE *srcbuffpos = (BYTE*)m_DIBbits;
			srcbuffpos += (m_bytesPerRow * crect_re_vd_top) + (bytesPerPixel * crect_re_vd_left);

			for (int y = rect.top; y < rect.bottom; y++)
			{
				memcpy(destbuffpos, srcbuffpos, widthBytes);
				srcbuffpos += m_bytesPerRow;
				destbuffpos += m_bytesPerRow;
			}
		}
	}
}

void vncDesktop::CopyToBuffer(RECT rect, BYTE *destbuff, const BYTE *srcbuffpos)
{
	const int crect_re_vd_left = rect.left - m_bmrect.left;
	const int crect_re_vd_top = rect.top - m_bmrect.top;
	_ASSERTE(crect_re_vd_left >= 0);
	_ASSERTE(crect_re_vd_top >= 0);

	const int bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;

	const int bmoffset = (m_bytesPerRow * crect_re_vd_top) + (bytesPerPixel * crect_re_vd_left);
	BYTE *destbuffpos = destbuff + bmoffset;
	srcbuffpos += bmoffset;

	const int widthBytes = (rect.right - rect.left) * bytesPerPixel;

	for (int y = rect.top; y < rect.bottom; y++)
	{
		memcpy(destbuffpos, srcbuffpos, widthBytes);
		srcbuffpos += m_bytesPerRow;
		destbuffpos += m_bytesPerRow;
	}
}

// Callback routine used internally to catch window movement...
BOOL CALLBACK
EnumWindowsFnCopyRect(HWND hwnd, LPARAM arg)
{

	//For excluding the popup windows
	if ((GetWindowLong( hwnd, GWL_STYLE) & WS_POPUP) ==0)
	{
	
		HANDLE prop = GetProp(hwnd, (LPCTSTR) MAKELONG(VNC_WINDOWPOS_ATOM, 0));
		if (prop != NULL) {
			
			if (IsWindowVisible(hwnd)) {
				
				RECT dest;
				POINT source;

				// Get the window rectangle
				if (GetWindowRect(hwnd, &dest)) {
					// Old position
					source.x = (SHORT) LOWORD(prop);
					source.y = (SHORT) HIWORD(prop);

					// Got the destination position.  Now send to clients!
					if ((source.x != dest.left) || (source.y != dest.top)) {
						// Update the property entry
						SHORT x = (SHORT) dest.left;
						SHORT y = (SHORT) dest.top;
						SetProp(hwnd,
							(LPCTSTR) MAKELONG(VNC_WINDOWPOS_ATOM, 0),
							(HANDLE) MAKELONG(x, y));

						// Store of the copyrect 
						((vncDesktop*)arg)->CopyRect(dest, source);
						
					}
				} else {
					RemoveProp(hwnd, (LPCTSTR) MAKELONG(VNC_WINDOWPOS_ATOM, 0));
				}
			} else {
				RemoveProp(hwnd, (LPCTSTR) MAKELONG(VNC_WINDOWPOS_ATOM, 0));
			}
		} else {
			// If the window has become visible then save its position!
			if (IsWindowVisible(hwnd)) {
				RECT dest;

				if (GetWindowRect(hwnd, &dest)) {
					SHORT x = (SHORT) dest.left;
					SHORT y = (SHORT) dest.top;
					SetProp(hwnd,
						(LPCTSTR) MAKELONG(VNC_WINDOWPOS_ATOM, 0),
						(HANDLE) MAKELONG(x, y));
				}
			}
		}
	}
	return TRUE;
}


void
vncDesktop::SetLocalInputDisableHook(BOOL enable)
{
	SetKeyboardFilterHook(enable);
	SetMouseFilterHook(enable);
}

void
vncDesktop::SetLocalInputPriorityHook(BOOL enable)
{
	if (vncService::IsWin95()) {
		SetKeyboardPriorityHook(m_hwnd,enable,RFB_LOCAL_KEYBOARD);
		SetMousePriorityHook(m_hwnd,enable,RFB_LOCAL_MOUSE);
	} else {
		SetKeyboardPriorityLLHook(m_hwnd,enable,RFB_LOCAL_KEYBOARD);
		SetMousePriorityLLHook(m_hwnd,enable,RFB_LOCAL_MOUSE);
	}

	if (!enable)
// FIXME: incremental semantics broken here;
// that's why we're compelled to consume extra unlocks
		m_server->BlockRemoteInput(false);
}

// Routine to find out which windows have moved
void
vncDesktop::CalcCopyRects()
{
	// Enumerate all the desktop windows for movement
	EnumWindows((WNDENUMPROC)EnumWindowsFnCopyRect, (LPARAM) this);
}


// Window procedure for the Desktop window
LRESULT CALLBACK
DesktopWndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	vncDesktop *_this = (vncDesktop*)GetWindowLong(hwnd, GWL_USERDATA);

	// WIN32S: _this is NULL for every message that arrives during CreateWindow,
	// before InitWindow() gets to its SetWindowLong(GWL_USERDATA) call -
	// WM_NCCREATE, WM_CREATE, WM_GETMINMAXINFO, WM_NCCALCSIZE.  The original
	// dereferenced it unconditionally.  On NT an invalid write to a NULL 'this'
	// happened to fall on unmapped memory and fault visibly; the sooner it is
	// checked the better.
	if (_this == NULL)
		return DefWindowProc(hwnd, iMsg, wParam, lParam);

	switch (iMsg)
	{

		// GENERAL

	case WM_DISPLAYCHANGE:
		// The display resolution is changing

		// We must kick off any clients since their screen size will be wrong
		_this->m_displaychanged = TRUE;
		return 0;

	case WM_SYSCOLORCHANGE:
	case WM_PALETTECHANGED:
		// The palette colours have changed, so tell the server

		// Get the system palette
		if (!_this->SetPalette())
			PostQuitMessage(0);
		// Update any palette-based clients, too
		_this->m_server->UpdatePalette();
		return 0;

	case WM_TIMER:
		switch (wParam) {
		case vncDesktop::TIMER_POLL:
			_this->SetPollingFlag(true);
			break;
		case vncDesktop::TIMER_BLANK_SCREEN:
			if (_this->m_server->GetBlankScreen())
				_this->BlankScreen(TRUE);
			break;
		case vncDesktop::TIMER_RESTORE_SCREEN:
			_this->BlankScreen(FALSE);
			break;
		}
		return 0;

		// CLIPBOARD MESSAGES

	case WM_CHANGECBCHAIN:
		// The clipboard chain has changed - check our nextviewer handle
		if ((HWND)wParam == _this->m_hnextviewer)
			_this->m_hnextviewer = (HWND)lParam;
		else
			if (_this->m_hnextviewer != NULL)
				SendMessage(_this->m_hnextviewer,
							WM_CHANGECBCHAIN,
							wParam, lParam);

		return 0;

	case WM_DRAWCLIPBOARD:
		// The clipboard contents have changed
		if((GetClipboardOwner() != _this->Window()) &&
		    _this->m_initialClipBoardSeen &&
			_this->m_clipboard_active)
		{
			LPSTR cliptext = NULL;

			// Open the clipboard
			if (OpenClipboard(_this->Window()))
			{
				// Get the clipboard data
				HGLOBAL cliphandle = GetClipboardData(CF_TEXT);
				if (cliphandle != NULL)
				{
					LPSTR clipdata = (LPSTR) GlobalLock(cliphandle);

					// Copy it into a new buffer
					if (clipdata == NULL)
						cliptext = NULL;
					else
						cliptext = strdup(clipdata);

					// Release the buffer and close the clipboard
					GlobalUnlock(cliphandle);
				}

				CloseClipboard();
			}

			if (cliptext != NULL)
			{
				int cliplen = strlen(cliptext);
				LPSTR unixtext = (char *)malloc(cliplen+1);

				// Replace CR-LF with LF - never send CR-LF on the wire,
				// since Unix won't like it
				int unixpos=0;
				for (int x=0; x<cliplen; x++)
				{
					if (cliptext[x] != '\x0d')
					{
						unixtext[unixpos] = cliptext[x];
						unixpos++;
					}
				}
				unixtext[unixpos] = 0;

				// Free the clip text
				free(cliptext);
				cliptext = NULL;

				// Now send the unix text to the server
				_this->m_server->UpdateClipText(unixtext);

				free(unixtext);
			}
		}

		_this->m_initialClipBoardSeen = TRUE;

		if (_this->m_hnextviewer != NULL)
		{
			// Pass the message to the next window in clipboard viewer chain.  
			return SendMessage(_this->m_hnextviewer, WM_DRAWCLIPBOARD, 0,0); 
		}

		return 0;

	default:
		// ==============================================================
		// WIN32S: hook-notification messages are handled HERE now.
		//
		// These are RegisterWindowMessage() values, so they are not compile-time
		// constants and cannot appear as case labels - hence the if-chain in the
		// default arm.  The old vncDesktopThread::run_undetached loop tested
		// them the same way, before its DispatchMessage() call.
		//
		// With VNCHooks stubbed out (VNCHooksStub.cpp) nothing on this platform
		// posts RFB_SCREEN_UPDATE or RFB_MOUSE_UPDATE, so this is effectively
		// dead code.  It is kept, and kept correct, so that the file still works
		// if the hook DLL is ever restored for a Win32 build from this branch.
		//
		// NOT PORTED: the RFB_LOCAL_KEYBOARD / RFB_LOCAL_MOUSE handling.  Those
		// messages come from the keyboard/mouse PRIORITY hooks, which cannot
		// install on Win32s either, and the original handler needed
		// ULARGE_INTEGER 64-bit arithmetic on FILETIMEs to implement its
		// timeout.  Since the messages can never arrive, porting that would be
		// dead weight; vncServer::LocalInputPriority() simply has no event
		// source and the feature is inert.
		// ==============================================================
		if (_this == NULL)
			return DefWindowProc(hwnd, iMsg, wParam, lParam);

		if (iMsg == RFB_SCREEN_UPDATE)
		{
			// An area of the screen has changed.
			// (The original ignored this when a mirror driver was active; there
			// is no driver on this platform.)
			{
				RECT rect;
				rect.left =	(SHORT)LOWORD(wParam);
				rect.top = (SHORT)HIWORD(wParam);
				rect.right = (SHORT)LOWORD(lParam);
				rect.bottom = (SHORT)HIWORD(lParam);
				_this->m_changed_rgn.AddRect(rect);
			}
			return 0;
		}

		if (iMsg == RFB_MOUSE_UPDATE)
		{
			// Save the cursor ID
			_this->SetCursor((HCURSOR) wParam);
			return 0;
		}

		return DefWindowProc(hwnd, iMsg, wParam, lParam);
	}
}

BOOL vncDesktop::CheckUpdates()
{
#ifndef _DEBUG
	try
	{
#endif
		// Re-install polling timer if necessary
		if (m_server->PollingCycleChanged())
		{
			SetPollingTimer();
			m_server->PollingCycleChanged(false);
		}

		// Update the state of blank screen timer
		UpdateBlankScreenTimer();

		// Has the display resolution or desktop changed?
		if (m_displaychanged || !vncService::InputDesktopSelected() || !inConsoleSession())
		{
			vnclog.Print(LL_STATE, VNCLOG("display resolution or desktop changed.\n"));

			rfbServerInitMsg oldscrinfo = m_scrinfo;
			m_displaychanged = FALSE;

			// Attempt to close the old hooks
			if (!Shutdown())
			{
				vnclog.Print(LL_INTERR, VNCLOG("failed to close desktop server.\n"));
				m_server->KillAuthClients();
				return FALSE;
			}

			// Now attempt to re-install them!
			ChangeResNow();

			if (!Startup())
			{
				vnclog.Print(LL_INTERR, VNCLOG("failed to re-start desktop server.\n"));
				m_server->KillAuthClients();
				return FALSE;
			}

			// Check if the screen info has changed
			vnclog.Print(LL_INTINFO,
						VNCLOG("SCR: old screen format %dx%dx%d\n"),
						oldscrinfo.framebufferWidth,
						oldscrinfo.framebufferHeight,
						oldscrinfo.format.bitsPerPixel);
			vnclog.Print(LL_INTINFO,
						VNCLOG("SCR: new screen format %dx%dx%d\n"),
						m_scrinfo.framebufferWidth,
						m_scrinfo.framebufferHeight,
						m_scrinfo.format.bitsPerPixel);
			if (memcmp(&m_scrinfo, &oldscrinfo, sizeof(oldscrinfo)) != 0)
			{
				vnclog.Print(LL_INTINFO, VNCLOG("screen format has changed.\n"));
			}

			// Call this regardless of screen format change
			m_server->UpdateLocalFormat();

			// Add a full screen update to all the clients
			m_changed_rgn.AddRect(m_bmrect);
			m_server->UpdatePalette();
		}

		// TRIGGER THE UPDATE

		RECT rect = m_server->GetSharedRect();
		RECT new_rect = GetSourceRect();
		IntersectRect(&new_rect, &new_rect, &m_bmrect);

		// Update screen size if required
		if (!EqualRect(&new_rect, &rect))
		{
			m_server->SetSharedRect(new_rect);
			bool sendnewfb = false;

			if (rect.right - rect.left != new_rect.right - new_rect.left ||
				rect.bottom - rect.top != new_rect.bottom - new_rect.top)
				sendnewfb = true;

			// FIXME: We should not send NewFBSize if a client
			//        did not send framebuffer update request.
			m_server->SetNewFBSize(sendnewfb);

			m_changed_rgn.Clear();

			if (sendnewfb && m_server->WindowShared())
			{
				if (new_rect.right - new_rect.left == 0 &&
					new_rect.bottom - new_rect.top == 0)
				{
// window is minimized
					return TRUE;
				}
				else
				{
// window is restored
// window is resized
					m_changed_rgn.AddRect(new_rect);
				}
			}
			else
			{
				return TRUE;
			}
		}

		// If we have clients full region requests
		if (m_server->FullRgnRequested())
		{
			// Capture screen to main buffer
			CaptureScreen(rect, m_mainbuff);
			// (Was: reset the mirror driver's change counter.  No driver here -
			// see the note on InitVideoDriver.)
		}

		// If we have incremental update requests
		if (m_server->IncrRgnRequested())
		{
			vncRegion rgn;

			// WIN32S: polling is the ONLY change-detection mechanism here.
			//
			// The original chose between the mirror video driver and polling.
			// There is no driver on this platform (InitVideoDriver returns FALSE)
			// and no hooks either (see VNCHooksStub.cpp), so every screen change
			// is found by PerformPolling() comparing the captured screen against
			// the previous frame.
			//
			// GetPollingFlag() is set by the TIMER_POLL timer, which
			// SetPollingTimer() installs at the configured polling cycle.  That
			// rate limiting matters a great deal here: a full-screen capture is
			// expensive on Win32s (see CaptureScreen), and without the flag this
			// would capture on every single idle pass.
			//
			// ==========================================================
			// WIN32S: DO NOT RELY ON THE TIMER ALONE.
			//
			// The pump diagnostic showed only 53 of 2303 passes with the flag set,
			// and long stretches with ZERO - so WM_TIMER for TIMER_POLL is being
			// delivered erratically.  That is expected on this platform: the
			// timer is a window timer on the desktop sink window, Windows 3.1 has
			// a small system-wide timer pool, and our own message loop is
			// dispatching thousands of messages per second.  A window timer is
			// also coalesced aggressively - if the queue is busy, ticks are
			// silently dropped rather than queued.
			//
			// Fall back to an elapsed-time check so polling happens at the
			// intended rate whether or not the timer message arrives.  The timer
			// is kept because when it DOES fire it gives better pacing than
			// polling GetTickCount.
			// ==========================================================
			BOOL doPoll = GetPollingFlag();

			if (!doPoll) {
				DWORD nowTick = GetTickCount();
				if (m_lastPollTick == 0)
					m_lastPollTick = nowTick;
				// Same floor as SetPollingTimer uses.
				if ((DWORD)(nowTick - m_lastPollTick) >= 100)
					doPoll = TRUE;
			}

			if (doPoll)
			{
				SetPollingFlag(false);
				m_lastPollTick = GetTickCount();
				PerformPolling();
			}

			// Check for moved windows
// PrimaryDisplayOnlyShared: check if any problems when
// dragging from another display
			// (The "&& !driver handles screen-to-screen blits" condition is gone
			// with the driver.)
			if (m_server->FullScreen() || m_server->PrimaryDisplayOnlyShared())
			{
				CalcCopyRects();
			}

			if (m_copyrect_set)
			{
				// Send copyrect to all clients
				m_server->CopyRect(m_copyrect_rect, m_copyrect_src);
				m_copyrect_set = false;

// DEBUG: Continue auditing the code from this point.

// IMPORTANT: this order: CopyRectToBuffer, CaptureScreen, GetChangedRegion
				// Copy old window rect to back buffer
				CopyRectToBuffer(m_copyrect_rect, m_copyrect_src);

				// Copy new window rect to main buffer
				CaptureScreen(m_copyrect_rect, m_mainbuff);

				// Get changed pixels to rgn
				GetChangedRegion(rgn, m_copyrect_rect);

				RECT rect;
				rect.left= m_copyrect_src.x;
				rect.top = m_copyrect_src.y;
				rect.right = rect.left + (m_copyrect_rect.right - m_copyrect_rect.left);
				rect.bottom = rect.top + (m_copyrect_rect.bottom - m_copyrect_rect.top);
				// Refresh old window rect
				m_changed_rgn.AddRect(rect);
				// Don't refresh new window rect
				m_changed_rgn.SubtractRect(m_copyrect_rect);
			} 

			// Get only desktop area
			vncRegion temprgn;
			temprgn.Clear();
			temprgn.AddRect(rect);
			m_changed_rgn.Intersect(temprgn);

			// Get list of rectangles for checking
			rectlist rectsToScan;
			m_changed_rgn.Rectangles(rectsToScan);

			// Capture and check them
			CheckRects(rgn, rectsToScan);

			// Update the mouse
			m_server->UpdateMouse();

			// Send changed region data to all clients
			m_server->UpdateRegion(rgn);

			// Clear changed region
			m_changed_rgn.Clear();
		}

		// Trigger an update to be sent
		if (m_server->FullRgnRequested() || m_server->IncrRgnRequested())
		{
			m_server->TriggerUpdate();
		}

#ifndef _DEBUG
	}
	catch (...)
	{
		vnclog.Print(LL_INTERR, VNCLOG("vncDesktop::CheckUpdates caught an exception.\n"));
		m_server->KillAuthClients();
		return FALSE;
	}
#endif

	return TRUE;
}

void
vncDesktop::SetPollingTimer()
{
	// WIN32S NOTES
	//
	//  * The driver branch is gone (no mirror driver - see InitVideoDriver).
	//
	//  * minPollingCycle raised from 5 ms to 100 ms.  This is the single most
	//    important tuning value in the port.  Each timer tick sets the polling
	//    flag, and the next idle pass then performs a screen capture -
	//    BitBlt from the screen DC into a memory DC, then GetDIBits or a
	//    DIBSection read.  On Win32s every one of those is a 32->16 bit thunk
	//    into the 16-bit GDI, and a full-screen capture at 640x480x8 is already
	//    300 KB of copying.  At 5 ms the machine would spend all of its time
	//    capturing and would appear frozen to its local user - which defeats the
	//    purpose of a remote-control server.
	//
	//    100 ms gives roughly 10 captures per second in the best case, which is
	//    a usable remote frame rate, and leaves the machine responsive.  The
	//    /16 divisor on the configured cycle is kept so that the Properties
	//    dialog's polling-cycle setting still has an effect.
	//
	//  * The result of SetTimer is now checked.  Windows 3.1 has a small,
	//    system-wide timer limit (and WinVNC also wants a blank-screen timer and
	//    the main loop's idle timer), so this can genuinely fail.  Without the
	//    polling timer GetPollingFlag() never becomes true and the server would
	//    silently never send an update - a confusing failure worth logging.
	const UINT minPollingCycle = 100;

	UINT msec = m_server->GetPollingCycle() / 16;
	if (msec < minPollingCycle) {
		msec = minPollingCycle;
	}

	// Replace any existing timer rather than leaking it: SetPollingTimer() is
	// called again from CheckUpdates() whenever the polling cycle changes, and
	// the original overwrote m_timer_polling without killing the old timer.
	if (m_timer_polling != 0) {
		KillTimer(Window(), TIMER_POLL);
		m_timer_polling = 0;
	}

	m_timer_polling = SetTimer(Window(), TIMER_POLL, msec, NULL);
	if (m_timer_polling == 0) {
		vnclog.Print(LL_INTERR,
			VNCLOG("could not create polling timer - no screen updates will be sent\n"));
	} else {
		vnclog.Print(LL_INTINFO, VNCLOG("polling every %d ms\n"), (int)msec);
	}
}

inline void vncDesktop::CheckRects(vncRegion &rgn, rectlist &rects)
{
#ifndef _DEBUG
	try
	{
#endif
		rectlist::iterator i;

		for (i = rects.begin(); i != rects.end(); i++)
		{
			// Copy data to the main buffer
			// FIXME: Maybe call CaptureScreen() just once?
			//        Check what would be more efficient.
			CaptureScreen(*i, m_mainbuff);

// Check for changes in the rectangle
			GetChangedRegion(rgn, *i);
		}
#ifndef _DEBUG
	}
	catch (...)
	{
		vnclog.Print(LL_INTERR, VNCLOG("vncDesktop::CheckRects caught an exception.\n"));
		throw;
	}
#endif
}

// This notably improves performance when using Visual C++ 6.0 compiler
#pragma function(memcpy, memcmp)

static const int BLOCK_SIZE = 32;

/*
// A dummy version of GetChangedRegion() created for troubleshoot purposes
// when GetChangedRegion() et al are suspected for bugs/need changes.
// The code below is as simple and clear as possible.
void vncDesktop::GetChangedRegion(vncRegion &rgn, const RECT &rect)
{
	rgn.AddRect(rect);

	// Copy the changes to the back buffer
	const int c2rect_re_vd_top = rect.top - m_bmrect.top;
	const int c3rect_re_vd_left = rect.left - m_bmrect.left;
	_ASSERTE(c2rect_re_vd_top >= 0);
	_ASSERTE(c3rect_re_vd_left >= 0);

	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	const int offset = c2rect_re_vd_top * m_bytesPerRow + c3rect_re_vd_left * bytesPerPixel;

	unsigned char *o_ptr = m_backbuff + offset;
	unsigned char *n_ptr = m_mainbuff + offset;
	const int bytes_in_row = (rect.right - rect.left) * bytesPerPixel;
	for (int y = rect.top; y < rect.bottom; y++)
	{
		memcpy(o_ptr, n_ptr, bytes_in_row);
		n_ptr += m_bytesPerRow;
		o_ptr += m_bytesPerRow;
	}
}
*/

/*
// DEBUG: Another dumb and slow version of GetChangedRegion().
void vncDesktop::GetChangedRegion(vncRegion &rgn, const RECT &rect)
{
	RECT newRect;

	vnclog.Print(LL_INTERR, VNCLOG("DEBUG: GetChangedRegion for %dx%d at (%d,%d)\n"),
		(int)(rect.right - rect.left), (int)(rect.bottom - rect.top),
		(int)rect.left, (int)rect.top);

	// Copy the changes to the back buffer
	const int top = rect.top - m_bmrect.top;
	const int left = rect.left - m_bmrect.left;
	_ASSERTE(top >= 0);
	_ASSERTE(left >= 0);

	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	const int offset = top * m_bytesPerRow + left * bytesPerPixel;

	unsigned char *o_ptr = m_backbuff + offset;
	unsigned char *n_ptr = m_mainbuff + offset;
	const int bytes_in_row = (rect.right - rect.left) * bytesPerPixel;
	for (int y = rect.top; y < rect.bottom; y++) {
		int x0 = rect.left;
		while (x0 < rect.right) {
			unsigned char *pNew = m_mainbuff + y * m_bytesPerRow + x0 * bytesPerPixel;
			unsigned char *pOld = m_backbuff + y * m_bytesPerRow + x0 * bytesPerPixel;
			if (memcmp(pOld, pNew, bytesPerPixel) != 0) {
				break;	// x0 points to the first difference at the left
			}
			x0++;
		}
		SetRect(&newRect, x0, y, rect.right, y + 1);
		if (newRect.right - newRect.left > 0 && newRect.bottom - newRect.top > 0) {
			rgn.AddRect(newRect);
			vnclog.Print(LL_INTERR, VNCLOG("DEBUG:   added %dx%d at (%d,%d)\n"),
				(int)(newRect.right - newRect.left), (int)(newRect.bottom - newRect.top),
				(int)newRect.left, (int)newRect.top);
		}

		memcpy(o_ptr, n_ptr, bytes_in_row);
		n_ptr += m_bytesPerRow;
		o_ptr += m_bytesPerRow;
	}
	vnclog.Print(LL_INTERR, VNCLOG("DEBUG: numRects = %d\n"), rgn.numRects());
}
*/

void vncDesktop::GetChangedRegion(vncRegion &rgn, const RECT &rect)
{
	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	const int bytes_per_scanline = (rect.right - rect.left) * bytesPerPixel;

	const int crect_re_vd_left = rect.left - m_bmrect.left;
	const int crect_re_vd_top = rect.top - m_bmrect.top;
	_ASSERTE(crect_re_vd_left >= 0);
	_ASSERTE(crect_re_vd_top >= 0);

	const int offset = crect_re_vd_top * m_bytesPerRow + crect_re_vd_left * bytesPerPixel;
	unsigned char *o_ptr = m_backbuff + offset;
	unsigned char *n_ptr = m_mainbuff + offset;

	RECT new_rect = rect;

	// Fast processing for small rectangles
	if (rect.right - rect.left <= BLOCK_SIZE &&
		rect.bottom - rect.top <= BLOCK_SIZE)
	{
		for (int y = rect.top; y < rect.bottom; y++)
		{
			if (memcmp(o_ptr, n_ptr, bytes_per_scanline) != 0)
			{
				new_rect.top = y;
				UpdateChangedSubRect(rgn, new_rect);
				break;
			}
			o_ptr += m_bytesPerRow;
			n_ptr += m_bytesPerRow;
		}
		return;
	}

// Process bigger rectangles
	BOOL bTop4Move = TRUE;
	for (int y = rect.top; y < rect.bottom; y++)
	{
		if (memcmp(o_ptr, n_ptr, bytes_per_scanline) != 0)
		{
			if (bTop4Move)
			{
				new_rect.top = y;
				bTop4Move = FALSE;
			}
			// Skip a number of lines after a non-matched one
			int n = BLOCK_SIZE / 2 - 1;
			y += n;
			o_ptr += n * m_bytesPerRow;
			n_ptr += n * m_bytesPerRow;
		}
		else
		{
			if (!bTop4Move)
			{
				new_rect.bottom = y;
				UpdateChangedRect(rgn, new_rect);
				bTop4Move = TRUE;
			}
		}
		o_ptr += m_bytesPerRow;
		n_ptr += m_bytesPerRow;
	}
	if (!bTop4Move)
	{
		new_rect.bottom = rect.bottom;
		UpdateChangedRect(rgn, new_rect);
	}
}

void vncDesktop::UpdateChangedRect(vncRegion &rgn, const RECT &rect)
{
	// Pass small rectangles directly to UpdateChangedSubRect
	if (rect.right - rect.left <= BLOCK_SIZE &&
		rect.bottom - rect.top <= BLOCK_SIZE)
	{
		UpdateChangedSubRect(rgn, rect);
		return;
	}

	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;

	RECT new_rect;
	int x, y, ay;

	const int crect_re_vd_left = rect.left - m_bmrect.left;
	const int crect_re_vd_top = rect.top - m_bmrect.top;
	_ASSERTE(crect_re_vd_left >= 0);
	_ASSERTE(crect_re_vd_top >= 0);

	// Scan down the rectangle
	const int offset = crect_re_vd_top * m_bytesPerRow + crect_re_vd_left * bytesPerPixel;
	unsigned char *o_topleft_ptr = m_backbuff + offset;
	unsigned char *n_topleft_ptr = m_mainbuff + offset;

	for (y = rect.top; y < rect.bottom; y += BLOCK_SIZE)
	{
		// Work out way down the bitmap
		unsigned char *o_row_ptr = o_topleft_ptr;
		unsigned char *n_row_ptr = n_topleft_ptr;

		const int blockbottom = Min(y + BLOCK_SIZE, rect.bottom);
		new_rect.bottom = blockbottom;

		BOOL bLeft4Move = TRUE;

		for (x = rect.left; x < rect.right; x += BLOCK_SIZE)
		{
			// Work our way across the row
			unsigned char *n_block_ptr = n_row_ptr;
			unsigned char *o_block_ptr = o_row_ptr;

			const UINT blockright = Min(x + BLOCK_SIZE, rect.right);
			const UINT bytesPerBlockRow = (blockright-x) * bytesPerPixel;

			// Scan this block
			for (ay = y; ay < blockbottom; ay++)
			{
				if (memcmp(n_block_ptr, o_block_ptr, bytesPerBlockRow) != 0)
					break;
				n_block_ptr += m_bytesPerRow;
				o_block_ptr += m_bytesPerRow;
			}
			if (ay < blockbottom)
			{
				// There were changes, so this block will need to be updated
				if (bLeft4Move)
				{
					new_rect.left = x;
					bLeft4Move = FALSE;
					new_rect.top = ay;
				}
				else if (ay < new_rect.top)
				{
					new_rect.top = ay;
				}
			}
			else
			{
				// No changes in this block, process previous changed blocks if any
				if (!bLeft4Move)
				{
					new_rect.right = x;
					UpdateChangedSubRect(rgn, new_rect);
					bLeft4Move = TRUE;
				}
			}

			o_row_ptr += bytesPerBlockRow;
			n_row_ptr += bytesPerBlockRow;
		}

		if (!bLeft4Move)
		{
			new_rect.right = rect.right;
			UpdateChangedSubRect(rgn, new_rect);
		}

		o_topleft_ptr += m_bytesPerRow * BLOCK_SIZE;
		n_topleft_ptr += m_bytesPerRow * BLOCK_SIZE;
	}
}

void vncDesktop::UpdateChangedSubRect(vncRegion &rgn, const RECT &rect)
{
	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	int bytes_in_row = (rect.right - rect.left) * bytesPerPixel;
	int y, i;

	const int crect_re_vd_left = rect.left - m_bmrect.left;
	const int crect_re_vd_bottom = rect.bottom - m_bmrect.top;
	_ASSERTE(crect_re_vd_left >= 0);
	_ASSERTE(crect_re_vd_bottom >= 0);

	// Exclude unchanged scan lines at the bottom
	int offset = (crect_re_vd_bottom - 1) * m_bytesPerRow + crect_re_vd_left * bytesPerPixel;
	unsigned char *o_ptr = m_backbuff + offset;
	unsigned char *n_ptr = m_mainbuff + offset;
	RECT final_rect = rect;
	final_rect.bottom = rect.top + 1;
	for (y = rect.bottom - 1; y > rect.top; y--)
	{
		if (memcmp(o_ptr, n_ptr, bytes_in_row) != 0)
		{
			final_rect.bottom = y + 1;
			break;
		}
		n_ptr -= m_bytesPerRow;
		o_ptr -= m_bytesPerRow;
	}

	// Exclude unchanged pixels at left and right sides
	const int c2rect_re_vd_left = final_rect.left - m_bmrect.left;
	const int c2rect_re_vd_top = final_rect.top - m_bmrect.top;
	_ASSERTE(c2rect_re_vd_left >= 0);
	_ASSERTE(c2rect_re_vd_top >= 0);

	offset = c2rect_re_vd_top * m_bytesPerRow + c2rect_re_vd_left * bytesPerPixel;
	o_ptr = m_backbuff + offset;
	n_ptr = m_mainbuff + offset;
	int left_delta = bytes_in_row - 1;
	int right_delta = 0;
	for (y = final_rect.top; y < final_rect.bottom; y++)
	{
		for (i = 0; i < bytes_in_row - 1; i++)
		{
			if (n_ptr[i] != o_ptr[i])
			{
				if (i < left_delta)
					left_delta = i;
				break;
			}
		}
		for (i = bytes_in_row - 1; i > 0; i--)
		{
			if (n_ptr[i] != o_ptr[i])
			{
				if (i > right_delta)
					right_delta = i;
				break;
			}
		}
		n_ptr += m_bytesPerRow;
		o_ptr += m_bytesPerRow;
	}
	final_rect.right = final_rect.left + right_delta / bytesPerPixel + 1;
	final_rect.left += left_delta / bytesPerPixel;

	// Update the rectangle
	rgn.AddRect(final_rect);

	// Copy the changes to the back buffer
	const int c3rect_re_vd_left = final_rect.left - m_bmrect.left;
	_ASSERTE(c3rect_re_vd_left >= 0);

	offset = c2rect_re_vd_top * m_bytesPerRow + c3rect_re_vd_left * bytesPerPixel;

	o_ptr = m_backbuff + offset;
	n_ptr = m_mainbuff + offset;
	bytes_in_row = (final_rect.right - final_rect.left) * bytesPerPixel;
	for (y = final_rect.top; y < final_rect.bottom; y++)
	{
		memcpy(o_ptr, n_ptr, bytes_in_row);
		n_ptr += m_bytesPerRow;
		o_ptr += m_bytesPerRow;
	}
}


void vncDesktop::PerformPolling()
{
	if (m_server->PollFullScreen())
	{
		// Poll full screen
		RECT full_rect = m_server->GetSharedRect();
		PollArea(full_rect);
	}
	else
	{
		// Poll a window
		if (m_server->PollForeground())
		{
			// Get the window rectangle for the currently selected window
			HWND hwnd = GetForegroundWindow();
			if (hwnd != NULL)
				PollWindow(hwnd);
		}
		if (m_server->PollUnderCursor())
		{
			// Find the mouse position
			POINT mousepos;
			if (GetCursorPos(&mousepos))
			{
				// Find the window under the mouse
				HWND hwnd = WindowFromPoint(mousepos);
				if (hwnd != NULL)
					PollWindow(hwnd);
			}
		}
	}
}

void
vncDesktop::PollWindow(HWND hwnd)
{
	// Are we set to low-load polling?
	if (m_server->PollOnEventOnly())
	{
		// Yes, so only poll if the remote user has done something
		if (!m_server->RemoteEventReceived()) {
			return;
		}
	}

	// Does the client want us to poll only console windows?
	if (m_server->PollConsoleOnly())
	{
		char classname[20];

		// Yes, so check that this is a console window...
		if (GetClassName(hwnd, classname, sizeof(classname))) {
			if ((strcmp(classname, "tty") != 0) &&
				(strcmp(classname, "ConsoleWindowClass") != 0)) {
				return;
			}
		}
	}

	RECT full_rect = m_server->GetSharedRect();
	RECT rect;

	// Get the rectangle
	if (GetWindowRect(hwnd, &rect)) {
		if (IntersectRect(&rect, &rect, &full_rect)) {
			PollArea(rect);
		}
	}
}

//
// Implementation of the polling algorithm.
//

void vncDesktop::PollArea(const RECT &rect)
{
	// ==================================================================
	// WIN32S DIAGNOSTIC (temporary): does the change detection see anything?
	//
	// The pump diagnostic showed "changed_rgn has 0 rects" on every pass while
	// the client-side regions work, which narrows the failure to here.  Two
	// possibilities, and these counters separate them:
	//
	//   tiles>0, diff=0   ->  the memcmp never differs.  Either both buffers hold
	//                         the same data (the capture is not reaching
	//                         m_mainbuff at the offset the comparison reads) or
	//                         both are all-zero (the capture is writing nowhere).
	//                         The "first bytes" values distinguish those: if main
	//                         and back are both 00, nothing is being captured; if
	//                         they are equal and non-zero, the capture works and
	//                         the screen genuinely has not changed.
	//
	//   tiles=0           ->  the loop does not run, i.e. the rect is empty or
	//                         the alignment arithmetic excludes everything.
	// ==================================================================
	static DWORD s_pollCalls = 0;
	static DWORD s_tilesChecked = 0;
	static DWORD s_tilesDiffered = 0;
	static DWORD s_lastPollReport = 0;

	s_pollCalls++;

	const int scanLine = m_pollingOrder[m_pollingStep++ % 32];
	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;

	// Align 32x32 tiles to the left top corner of the shared area
	const RECT shared = m_server->GetSharedRect();
	const int leftAligned = ((rect.left - shared.left) & 0xFFFFFFE0) + shared.left;
	const int topAligned = ((rect.top - shared.top) & 0xFFFFFFE0) + shared.top;

	RECT rowRect = rect;	// we'll need left and right borders

	for (int y = topAligned; y < rect.bottom; y += 32)
	{
		const int tile_h = min(rect.bottom - y, 32);
// TODO: refactor it
		int sl = scanLine;
// window captions suffer an arbitrary scanline...
		if (y == topAligned)
			sl = 31;
		sl = min(sl, tile_h-1);
		const int scan_y = y + sl;

		_ASSERTE(scan_y >= rect.top);
		_ASSERTE(scan_y < rect.bottom);

		rowRect.top = scan_y;
		rowRect.bottom = scan_y + 1;
		CaptureScreen(rowRect, m_mainbuff);
		const int offset = (scan_y-m_bmrect.top) * m_bytesPerRow + (leftAligned-m_bmrect.left) * bytesPerPixel;
		const unsigned char *o_ptr = m_backbuff + offset;
		const unsigned char *n_ptr = m_mainbuff + offset;
		for (int x = leftAligned; x < rect.right; x += 32)
		{
			const int tile_w = min(rect.right - x, 32);
			const int nBytes = tile_w * bytesPerPixel;
			s_tilesChecked++;
			if (memcmp(o_ptr, n_ptr, nBytes) != 0)
			{
				s_tilesDiffered++;
				RECT tileRect;
				tileRect.left = x;
				tileRect.top = y;
				tileRect.right = x + tile_w;
				tileRect.bottom = y + tile_h;
				m_changed_rgn.AddRect(tileRect);
			}
			o_ptr += nBytes;
			n_ptr += nBytes;
		}
	}

	// Report once every 5 seconds.
	{
		DWORD now = GetTickCount();
		if (s_lastPollReport == 0)
			s_lastPollReport = now;
		if ((DWORD)(now - s_lastPollReport) >= 5000)
		{
			// Sample the first few bytes of both buffers at the shared origin, so
			// we can tell "identical and non-zero" from "both all zero".
			const int sampleOff = 0;
			unsigned long mainSample = 0, backSample = 0;
			if (m_mainbuff != NULL)
				memcpy(&mainSample, m_mainbuff + sampleOff, 4);
			if (m_backbuff != NULL)
				memcpy(&backSample, m_backbuff + sampleOff, 4);

			vnclog.Print(LL_INTERR,
				VNCLOG("poll: %d calls, %d tiles checked, %d differed; "
					   "main=%08lx back=%08lx bpr=%d bpp=%d rect=(%d,%d,%d,%d)\n"),
				(int)s_pollCalls, (int)s_tilesChecked, (int)s_tilesDiffered,
				mainSample, backSample,
				(int)m_bytesPerRow, (int)bytesPerPixel,
				(int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom);

			s_pollCalls = 0;
			s_tilesChecked = 0;
			s_tilesDiffered = 0;
			s_lastPollReport = now;
		}
	}
}

void vncDesktop::CopyRect(const RECT &rcDest, const POINT &ptSrc)
{
	const int offset_x = rcDest.left - ptSrc.x;
	const int offset_y = rcDest.top - ptSrc.y;

	// Clip the destination to the screen
	RECT destrect;
	if (!IntersectRect(&destrect, &rcDest, &m_server->GetSharedRect()))
		return;

	// NOTE: This is important. Each pixel in destrect is either salvaged
	//       by copyrect or became dirty.
	m_changed_rgn.AddRect(destrect);

	// Work out the source rectangle
	RECT srcrect;
	srcrect.left = destrect.left - offset_x;
	srcrect.top = destrect.top - offset_y;
	srcrect.right = srcrect.left + destrect.right - destrect.left;
	srcrect.bottom = srcrect.top + destrect.bottom - destrect.top;

	// Clip the source to the screen
	RECT srcrect2;
	if (!IntersectRect(&srcrect2, &srcrect, &m_server->GetSharedRect()))
		return;

	destrect.left += (srcrect2.left - srcrect.left);
	destrect.top += (srcrect2.top - srcrect.top);
	destrect.right = srcrect2.right - srcrect2.left + destrect.left;
	destrect.bottom = srcrect2.bottom - srcrect2.top + destrect.top;

	if ( destrect.right - destrect.left >= 16 &&
		 destrect.bottom - destrect.top >= 16 ) {
		m_changed_rgn.SubtractRect(destrect);

		m_copyrect_rect = destrect;
		m_copyrect_src.x = srcrect2.left;
		m_copyrect_src.y = srcrect2.top;
		m_copyrect_set = TRUE;

		//DPF(("CopyRect: (%d, %d) (%d, %d, %d, %d)\n",
		//	m_copyrect_src.x,
		//	m_copyrect_src.y,
		//	m_copyrect_rect.left,
		//	m_copyrect_rect.top,
		//	m_copyrect_rect.right,
		//	m_copyrect_rect.bottom));
	}
}

//
// Copy the data from one rectangle of the back buffer to another.
//

void vncDesktop::CopyRectToBuffer(const RECT &dest, const POINT &source)
{
	const int src_x = source.x - m_bmrect.left;
	const int src_y = source.y - m_bmrect.top;
	_ASSERTE(src_x >= 0);
	_ASSERTE(src_y >= 0);

	const int dst_x = dest.left - m_bmrect.left;
	const int dst_y = dest.top - m_bmrect.top;
	_ASSERTE(dst_x >= 0);
	_ASSERTE(dst_y >= 0);

	const unsigned int bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	const unsigned int bytesPerLine = (dest.right - dest.left) * bytesPerPixel;

	BYTE *srcptr = m_backbuff + src_y * m_bytesPerRow + src_x * bytesPerPixel;
	BYTE *destptr = m_backbuff + dst_y * m_bytesPerRow + dst_x * bytesPerPixel;

	if (dst_y < src_y) {
		for (int y = dest.top; y < dest.bottom; y++) {
			memmove(destptr, srcptr, bytesPerLine);
			srcptr += m_bytesPerRow;
			destptr += m_bytesPerRow;
		}
	} else {
		srcptr += m_bytesPerRow * (dest.bottom - dest.top - 1);
		destptr += m_bytesPerRow * (dest.bottom - dest.top - 1);
		for (int y = dest.bottom; y > dest.top; y--) {
			memmove(destptr, srcptr, bytesPerLine);
			srcptr -= m_bytesPerRow;
			destptr -= m_bytesPerRow;
		}
	}
}

// IsBadDirectAccessConfig() removed: it existed solely to decide whether to ask
// the mirror driver for direct frame-buffer access, working around an XP bug with
// negative virtual-screen origins.  Its only caller was InitVideoDriver().
//
// Note that SM_XVIRTUALSCREEN/SM_YVIRTUALSCREEN are Win98/Win2000 metrics
// (locally #defined at the top of this file); GetSystemMetrics returns 0 for
// unknown indices on Win32s, so the function would simply have returned FALSE.

// ==========================================================================
// MIRROR VIDEO DRIVER - REMOVED FOR THE WIN32S PORT
// ==========================================================================
//
// The mirror driver ("Mirage") is a kernel-mode display driver that mirrors the
// primary display into a shared memory buffer, so the server can read changed
// rectangles directly instead of polling with GetDIBits.  It is the fastest
// capture path by a wide margin - and it is completely unavailable here.
//
// The original InitVideoDriver() already refused to run on anything below
// Windows 2000:
//
//     if (!vncService::IsWinNT())      return FALSE;   // not NT at all
//     if (!IsWinVerOrHigher(5, 0))     return FALSE;   // NT4 support "broken"
//
// so on Win32s it returned FALSE on the very first line.  Everything below that
// point was unreachable on this platform.
//
// Why the whole thing is removed rather than left to fail at run time:
//
//   * VideoDriver.cpp calls ExtEscape(), ChangeDisplaySettingsEx(),
//     EnumDisplayDevices(), CreateDC() with driver names, and
//     MapViewOfFile()/CreateFileMapping() against a kernel object.  Several of
//     those - ChangeDisplaySettingsEx and EnumDisplayDevices in particular - are
//     Win98/Win2000-era APIs that do NOT exist on Win32s.  Because the linker
//     records every imported name whether or not the code path can execute, their
//     mere presence in the import table prevents the EXE from LOADING on Win32s.
//     That is the same failure that stopped the viewer starting, and it cannot be
//     fixed by dead-code elimination - the compiler cannot know the calls are
//     unreachable.
//
//   * VideoDriver.cpp also uses __int64 arithmetic and 64-bit shared-buffer
//     offsets that MSVC 4.1 compiles poorly.
//
// InitVideoDriver() now returns FALSE unconditionally and ShutdownVideoDriver()
// is a no-op.  m_videodriver is therefore ALWAYS NULL, which every call site
// already tests for - the driver was optional by design, and the polling path
// (PerformPolling / CheckUpdates) is the fallback that the original used whenever
// the driver was absent.
//
// VideoDriver.cpp / VideoDriver.h have been removed from WinVNC.mak.  The files
// are left in the tree for anyone building the NT version from this branch.
//
// Also removed with it: DriverDirectAccess / DontUseDriver had no other purpose,
// but they are settings that persist in the registry and are read by
// vncProperties, so the accessors remain (they simply no longer influence
// anything).
// ==========================================================================

BOOL vncDesktop::InitVideoDriver()
{
	// See the note above: the mirror driver requires Windows 2000 or later.
	return FALSE;
}

void vncDesktop::ShutdownVideoDriver()
{
	// Nothing to shut down - m_videodriver is always NULL on this platform.
	_ASSERTE(m_videodriver == NULL);
}

void
vncDesktop::UpdateBlankScreenTimer()
{
	// WIN32S: BlankScreen() is a no-op here (see below), so there is nothing for
	// this timer to do.  It is left disabled rather than removed so that the
	// setting still round-trips through the Properties dialog.
	//
	// This also matters for a practical reason: Windows 3.1 has a small
	// system-wide timer limit, and the server already needs the polling timer and
	// the main loop's idle timer.  A 50 ms timer that does nothing is not worth
	// one of them.
	BOOL active = FALSE;

	if (active && !m_timer_blank_screen) {
		m_timer_blank_screen = SetTimer(Window(), TIMER_BLANK_SCREEN, 50, NULL);
	} else if (!active && m_timer_blank_screen) {
		KillTimer(Window(), TIMER_BLANK_SCREEN);
		m_timer_blank_screen = 0;
		PostMessage(m_hwnd, WM_TIMER, TIMER_RESTORE_SCREEN, 0);
	}
}

void
vncDesktop::BlankScreen(BOOL set)
{
	// "Blank the server's monitor while a client is connected."
	//
	// WIN32S: this cannot work and is now a no-op.
	//
	//   * SPI_SETPOWEROFFACTIVE (value 86) is a Windows 95 addition and is not
	//     recognised by the Windows 3.1 SystemParametersInfo, which returns
	//     FALSE for an unknown action.  Harmless, but useless.
	//
	//   * SC_MONITORPOWER is likewise Win95+: it is an APM/display-power request
	//     that Windows 3.1's DefWindowProc does not implement.  Windows 3.1 has
	//     no display power management at all - monitor blanking on that vintage
	//     of hardware was a screen-saver, not a power state.
	//
	// The alternative - covering the screen with a black window - is deliberately
	// NOT implemented: with one thread and one message queue, a full-screen
	// topmost window would interfere with the screen capture we are performing on
	// the same desktop.
	//
	// The function is kept (rather than removed) because UpdateBlankScreenTimer()
	// and the TIMER_BLANK_SCREEN / TIMER_RESTORE_SCREEN handlers call it, and the
	// "Blank screen" setting persists in the registry.
	//
	// Log once so that a user who enables the option understands why nothing
	// happens.
	static BOOL warned = FALSE;
	if (set && !warned) {
		warned = TRUE;
		vnclog.Print(LL_INTWARN,
			VNCLOG("blank-screen is not supported on this platform - ignoring\n"));
	}
}

// created for debug purposes
bool	SaveBitmapToBMPFile(
			HANDLE hFile,
			void *ptrBm,
			void *ptrPal,
			int bmwidth,
			int bmheight,
			int bmstride,
			int bmclrdepth)
{
	BITMAPINFOHEADER bih = {0};
	bih.biSize			= sizeof(bih);
	bih.biWidth			= bmwidth;
	bih.biHeight		= bmheight;
	bih.biPlanes		= 1;
	bih.biCompression	= BI_RGB;

	DWORD bitFields[3] = {0, 0, 0};

	if (bmclrdepth == 1)
	{
		bih.biBitCount = 1;
		bih.biClrUsed = 2;
	}
	else if (bmclrdepth == 2)
	{
		bih.biBitCount = 2;
		bih.biClrUsed = 4;
	}
	else if (bmclrdepth == 4)
	{
		bih.biBitCount = 4;
		bih.biClrUsed = 0x10;
	}
	else if (bmclrdepth == 8)
	{
		bih.biBitCount = 8;
		bih.biClrUsed = 0x100;
	}
	else if (bmclrdepth == 16)
	{
		bih.biBitCount = 16;
		bih.biCompression = BI_BITFIELDS;
// TODO: use actual masks
		bitFields[0] = 0xF800;
		bitFields[1] = 0x07E0;
		bitFields[2] = 0x001F;
	}
	else if (bmclrdepth == 24)
	{
		bih.biBitCount = 24;
	}
	else if (bmclrdepth == 32)
	{
		bih.biBitCount = 32;
	}
	else
		_ASSERTE(false);

	BITMAPFILEHEADER bfh = {0};
	bfh.bfType			= 0x4d42;	// 0x42 = "B" 0x4d = "M" 
	bfh.bfOffBits		= sizeof(BITMAPFILEHEADER) + bih.biSize;

	if (bih.biClrUsed)
	{
		bfh.bfOffBits += bih.biClrUsed * sizeof(RGBQUAD);
	}
	else if (bitFields[0] || bitFields[1] || bitFields[2])
	{
		bfh.bfOffBits += sizeof(bitFields);
	}

	unsigned lineSize = (((bih.biWidth * bih.biBitCount) + 15) / 8) & ~1;
	bfh.bfSize = bfh.bfOffBits + lineSize * bih.biHeight; 

	ULONG ulnWr = 0;
	if (!WriteFile(hFile, &bfh, sizeof(bfh), &ulnWr, NULL) || ulnWr!=sizeof(bfh))
		return false;
	if (!WriteFile(hFile, &bih, sizeof(bih), &ulnWr, NULL) || ulnWr!=sizeof(bih))
		return false;

	if (ptrPal)
	{
		if (!WriteFile(hFile, ptrPal, bih.biClrUsed * sizeof(RGBQUAD), &ulnWr, NULL) || ulnWr!=bih.biClrUsed * sizeof(RGBQUAD))
			return false;
	}
	else if (bih.biCompression == BI_BITFIELDS)
	{
		if (!WriteFile(hFile, bitFields, sizeof(bitFields), &ulnWr, NULL) || ulnWr!=sizeof(bitFields))
			return false;
	}

	for (int i = 0; i < bih.biHeight; i++)
	{
		char *pDWr = (char*)ptrBm + (bih.biHeight - i - 1) * bmstride;
		if (!WriteFile(hFile, pDWr, lineSize, &ulnWr, NULL) || ulnWr!=lineSize)
			return false;
	}

	return true;
}

// created for debug purposes
bool	bDbgBmDump(
			void *ptr,
			int bmwidth,
			int bmheight,
			int bmstride,
			int bmclrdepth)
{
	if (bmclrdepth!=16 && bmclrdepth!=32)
	{
		// TODO: add 8 bpp
		return false;
	}

	SYSTEMTIME stm;
	GetSystemTime(&stm);
	TCHAR szFileName[MAX_PATH];
	sprintf(
		szFileName,
		"%04u.%02u.%02u-%02u-%02u-%02u-0x%08x.bmp",
		stm.wYear, stm.wMonth, stm.wDay,
		stm.wHour, stm.wMinute, stm.wSecond,
		ptr);

	HANDLE hFile = CreateFile(
		szFileName,
		FILE_WRITE_DATA,
		0,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile==INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool b= SaveBitmapToBMPFile(
		hFile,
		ptr,
		NULL,
		bmwidth,
		bmheight,
		bmstride,
		bmclrdepth);

	CloseHandle(hFile);
	return b;
}

// created for debug purposes
bool	vncDesktop::bDbgDumpSurfBuffers(const RECT &rcl)
{
	const int c2rect_re_vd_top = rcl.top - m_bmrect.top;
	const int c3rect_re_vd_left = rcl.left - m_bmrect.left;
	_ASSERTE(c2rect_re_vd_top >= 0);
	_ASSERTE(c3rect_re_vd_left >= 0);
	const UINT bytesPerPixel = m_scrinfo.format.bitsPerPixel / 8;
	const int offset = c2rect_re_vd_top * m_bytesPerRow + c3rect_re_vd_left * bytesPerPixel;

	bool b1 = bDbgBmDump(
		m_mainbuff+offset,
		rcl.right - rcl.left,
		rcl.bottom - rcl.top,
		m_bytesPerRow,
		m_scrinfo.format.bitsPerPixel);

	bool b2 = bDbgBmDump(
		m_backbuff+offset,
		rcl.right - rcl.left,
		rcl.bottom - rcl.top,
		m_bytesPerRow,
		m_scrinfo.format.bitsPerPixel);
	return b1 && b2;
}
