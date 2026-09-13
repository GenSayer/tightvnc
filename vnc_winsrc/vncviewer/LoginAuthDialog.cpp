//  Copyright (C) 2003-2006 Constantin Kaplinsky. All Rights Reserved.
//  Copyright (C) 2000 Tridia Corporation. All Rights Reserved.
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


// LoginAuthDialog.cpp: implementation of the dialog box for authentication
// with a username/password pair.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "LoginAuthDialog.h"
#include "Exception.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LoginAuthDialog::LoginAuthDialog(char *vnchost, char *title, char *username)
{
	if (title != NULL) {
		strncpy(m_title, title, sizeof(m_title)-1);
		m_title[sizeof(m_title)-1] = '\0';
	} else {
		m_title[0] = '\0';
	}

	m_username_disabled = (username == NULL);

	if (username == NULL || username[0] == '\0') {
		m_username[0] = TEXT('\0');
	} else {
		_tcsncpy(m_username, username, 255);
		m_username[255] = TEXT('\0');
	}
	m_passwd[0] = TEXT('\0');
	m_cancelled = false;
	m_vnchost = (vnchost != NULL) ? vnchost : "[unknown]";
}

LoginAuthDialog::~LoginAuthDialog()
{
}

int LoginAuthDialog::DoDialog()
{
	m_cancelled = false;
	int res = DialogBoxParam(pApp->m_instance,
							 DIALOG_MAKEINTRESOURCE(IDD_LOGIN_AUTH_DIALOG), 
							 NULL, (DLGPROC)DlgProc, (LONG)this);
	// Throw *here*, after the dialog's message loop has fully unwound - never
	// from inside DlgProc (see the note in the IDCANCEL handler below).
	if (res == -1) {
		// DialogBoxParam itself failed (bad template, no memory).  Treat as a
		// hard error rather than silently continuing with an empty password.
		vnclog.Print(0, _T("Could not create authentication dialog: %d\n"),
					 GetLastError());
		throw ErrorException("Could not display the authentication dialog.");
	}
	if (m_cancelled || res == 0) {
		throw QuietException("User canceled authentication.");
	}
	return res;
}

BOOL CALLBACK LoginAuthDialog::DlgProc(HWND hwnd, UINT uMsg,
									   WPARAM wParam, LPARAM lParam) {
	// This is a static method, so we don't know which instantiation we're 
	// dealing with. But we can get a pseudo-this from the parameter to 
	// WM_INITDIALOG, which we therafter store with the window and retrieve
	// as follows:
	LoginAuthDialog *_this =
		(LoginAuthDialog *)GetWindowLong(hwnd, GWL_USERDATA);

	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowLong(hwnd, GWL_USERDATA, lParam);
		_this = (LoginAuthDialog *)lParam;
		if (_this->m_title[0] != '\0')
			SetWindowText(hwnd, _this->m_title);
		SetDlgItemText(hwnd, IDC_VNCHOST, _this->m_vnchost);
		CentreWindow(hwnd);
		if (_this->m_username[0] != '\0') {
			SetDlgItemText(hwnd, IDC_LOGIN_EDIT, _this->m_username);
			SetFocus(GetDlgItem(hwnd, IDC_PASSWD_EDIT));
			return FALSE;
		}
		if (_this->m_username_disabled) {
			EnableWindow(GetDlgItem(hwnd, IDC_LOGIN_EDIT), FALSE);
		}
		return TRUE;
	case WM_COMMAND:
		if (_this == NULL)
			break;
		switch (LOWORD(wParam)) {
		case IDOK:
			GetDlgItemText(hwnd, IDC_LOGIN_EDIT, _this->m_username, 256);
			GetDlgItemText(hwnd, IDC_PASSWD_EDIT, _this->m_passwd, 256);
			_this->m_cancelled = false;
			EndDialog(hwnd, TRUE);
			return TRUE;
		case IDCANCEL:
			// Was:
			//     EndDialog(hwnd, FALSE);
			//     throw QuietException("User canceled authentication.");
			//
			// Throwing out of a window procedure means throwing across the
			// USER32 call frame that dispatched the message.  There is no C++
			// unwind information for that frame, so with MSVC 4.1 this either
			// terminates the process or corrupts the stack.  It is a prime
			// suspect for "crashes when making a connection" on every platform:
			// it fires whenever the user presses Cancel at the password prompt.
			//
			// Record the decision and let DoDialog() throw once we are back on
			// our own stack.
			_this->m_cancelled = true;
			EndDialog(hwnd, FALSE);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		if (_this != NULL)
			_this->m_cancelled = true;
		EndDialog(hwnd, FALSE);
		return TRUE;
	case WM_DESTROY:
		// Do not EndDialog here - the dialog is already going away.
		return TRUE;
	}
	return 0;
}

