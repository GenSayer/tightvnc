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


// vncTimedMsgBox

// ==========================================================================
// WIN32S SINGLE-THREADED CONVERSION
// ==========================================================================
//
// The original spawned an omni_thread to display a MessageBox and then slept for
// four seconds in the CALLING thread, so that the box stayed up while the caller
// carried on (and ultimately until WinVNC quit, since nothing ever dismissed it).
//
// That design does not survive the loss of threads, and it was already dubious:
//
//   * vncTimedMsgBoxThread::run() called MessageBox(), which runs its own modal
//     message loop.  The thread therefore never returned until the user clicked
//     OK - and because the thread was started with start() (detached), nothing
//     ever joined it or freed the object.  The strdup'd caption and title leaked
//     on every call.
//
//   * The caller's "Sleep(4000)" was pure guesswork about how long the box needs
//     to appear.
//
// On Win32s there is no second thread to put the box on, and a modal MessageBox
// from the single thread would block the entire server - including screen
// polling and every connected client - until someone dismissed it.
//
// The replacement is a plain MessageBox with MB_OK, shown modally, but ONLY from
// the places that genuinely want to interrupt the user.  Since the sole remaining
// caller is vncService.cpp (which now consists of stubs that report unavailable
// features), the four-second flourish serves no purpose.
//
// If a non-blocking notification is ever needed, the right implementation on this
// platform is a modeless dialog created with CreateDialog() plus a WM_TIMER to
// dismiss it - not a thread.
// ==========================================================================

#include "stdhdrs.h"
#include "vncTimedMsgBox.h"

// The main vncTimedMsgBox class

void
vncTimedMsgBox::Do(const char *caption, const char *title, UINT type)
{
	if (caption == NULL)
		return;
	if (title == NULL)
		title = "WinVNC";

	// MB_SETFOREGROUND so the box is visible even if another application has
	// focus; MB_TASKMODAL rather than the default because we have no window to
	// parent it to and MB_TASKMODAL disables only OUR windows, which on Win32s
	// (one VM, one message queue) is the polite choice.
	MessageBox(NULL, caption, title, type | MB_OK | MB_SETFOREGROUND | MB_TASKMODAL);
}
