//  Copyright (C) 2003 Constantin Kaplinsky. All Rights Reserved.
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

// ConnectingDialog

// ==========================================================================
// WIN32S SINGLE-THREADED REWRITE
//
// Was: a modal DialogBoxParam() running on its own omni_thread, with the
// creating thread spinning in "while (!m_started) sleep(0, 50000000);" until
// the dialog thread signalled that WM_INITDIALOG had run.
//
// On Win32s that design cannot work at all - there is no second thread, so
// start_undetached() threw and, even if it had not, the spin loop would have
// hung the single thread forever.
//
// Now: a *modeless* dialog created with CreateDialogParam() on the one and
// only thread.  The connection code calls SetStatus() as it progresses; each
// call updates the static text and then pumps the dialog's own messages so
// the box paints and the Hide button works, without a nested modal loop and
// without needing the caller to return to the main message loop first.
// ==========================================================================

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ConnectingDialog.h"
#include "Win32sApi.h"

#ifndef IDCLOSE
#define IDCLOSE             8
#endif

ConnectingDialog::ConnectingDialog(HINSTANCE hInst, const char *vnchost)
{
	m_hInst = hInst;
	m_hwnd = NULL;

	if (vnchost != NULL) {
		strncpy(m_vnchost, vnchost, sizeof(m_vnchost) - 1);
		m_vnchost[sizeof(m_vnchost) - 1] = '\0';
		m_hostKnown = true;
	} else {
		m_vnchost[0] = '\0';
		m_hostKnown = false;
	}

	// Modeless: returns immediately, no nested message loop, no thread.
	//
	// A failure here is not fatal - the connection sequence just proceeds
	// without a progress box - so do not throw.  That matters because this is
	// constructed from ClientConnection::Run() before the viewer window exists.
	m_hwnd = CreateDialogParam(m_hInst,
							   MAKEINTRESOURCE(IDD_CONNECTING_DIALOG),
							   NULL, (DLGPROC)DlgProc, (LPARAM)this);
	if (m_hwnd != NULL) {
		CentreWindow(m_hwnd);
		ShowWindow(m_hwnd, SW_SHOW);
		UpdateWindow(m_hwnd);
	} else {
		vnclog.Print(2, _T("Could not create the Connecting dialog (%d)\n"),
					 GetLastError());
	}
}

ConnectingDialog::~ConnectingDialog()
{
	Close();
}

void
ConnectingDialog::Close()
{
	if (m_hwnd != NULL) {
		HWND hwnd = m_hwnd;
		m_hwnd = NULL;
		// DestroyWindow, not EndDialog: this is a modeless dialog.
		DestroyWindow(hwnd);
	}
}

void
ConnectingDialog::SetStatus(const char *msg)
{
	if (m_hwnd == NULL)
		return;

	char buf[256];
	// _snprintf is available in MSVC 4.1; keep the same 240-char clamp the
	// original sprintf relied on.
	_snprintf(buf, sizeof(buf) - 1, "Status: %.240s.", msg);
	buf[sizeof(buf) - 1] = '\0';
	SetDlgItemText(m_hwnd, IDC_STATUS_STATIC, buf);

	// Give the dialog a chance to repaint and to see a click on "Hide".
	// This is the single-threaded stand-in for the old separate UI thread.
	Pump();
}

//
// Pump only this dialog's messages.  Deliberately narrow: we must not
// dispatch the viewer window's messages here, because we are called from
// deep inside the connection sequence where the viewer window may not be
// fully constructed yet.
//
void
ConnectingDialog::Pump()
{
	if (m_hwnd == NULL)
		return;

	MSG msg;
	while (PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
		if (!IsDialogMessage(m_hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (m_hwnd == NULL)		// user pressed Hide during dispatch
			return;
	}
	UpdateWindow(m_hwnd);
}

LRESULT CALLBACK
ConnectingDialog::DlgProc(HWND hwnd, UINT uMsg,
						  WPARAM wParam, LPARAM lParam)
{
	ConnectingDialog *_this =
		(ConnectingDialog *)GetWindowLong(hwnd, GWL_USERDATA);

	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowLong(hwnd, GWL_USERDATA, lParam);
		_this = (ConnectingDialog *)lParam;
		if (_this == NULL)
			return TRUE;
		_this->m_hwnd = hwnd;
		if (_this->m_hostKnown) {
			char buf[256];
			if (_this->m_vnchost[0] != '\0') {
				_snprintf(buf, sizeof(buf) - 1,
						  "Connecting to %.200s ...", _this->m_vnchost);
			} else {
				strcpy(buf, "Accepting reverse connection...");
			}
			buf[sizeof(buf) - 1] = '\0';
			SetDlgItemText(hwnd, IDC_CONNECTING_STATIC, buf);
		}
		// Win32sSetForegroundWindow: SetForegroundWindow is Win95+ and must not
		// be a load-time import (see Win32sApi.h).
		Win32sSetForegroundWindow(hwnd);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCLOSE:
		case IDCANCEL:
			if (_this != NULL)
				_this->m_hwnd = NULL;
			DestroyWindow(hwnd);
			return TRUE;
		}
		break;

	case WM_CLOSE:
		if (_this != NULL)
			_this->m_hwnd = NULL;
		DestroyWindow(hwnd);
		return TRUE;
	}

	return FALSE;
}
