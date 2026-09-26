// Copyright (C) 2003 TightVNC Development Team. All Rights Reserved.
//
//  TightVNC is free software; you can redistribute it and/or modify
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
// TightVNC homepage on the Web: http://www.tightvnc.com/

// VNCHelp.cpp: implementation of the VNCHelp class.

#include "stdhdrs.h"
#include "Htmlhelp.h"
#include "vncviewer.h"
#include "VNCHelp.h"

// ==========================================
// Win32s WM_HELP / HELPINFO Stubs
// ==========================================

#ifndef HELPINFO_DEFINED
#define HELPINFO_DEFINED

// The standard HELPINFO structure fields layout
typedef struct tagHELPINFO {
    UINT    cbSize;             // Size of this structure
    int     iContextType;       // HELPINFO_WINDOW or HELPINFO_MENUITEM
    int     iCtrlId;            // Component ID
    HANDLE  hItemHandle;        // Associated HWND or HMENU
    DWORD   dwContextId;        // Context help ID
    POINT   MousePos;           // Mouse coordinates
} HELPINFO, *LPHELPINFO;

// Standard Context Types (if needed by your code)
#ifndef HELPINFO_WINDOW
#define HELPINFO_WINDOW     0x0001
#endif
#ifndef HELPINFO_MENUITEM
#define HELPINFO_MENUITEM   0x0002
#endif

#endif // HELPINFO_DEFINED


VNCHelp::VNCHelp()
{
	m_dwCookie = 0;		// DWORD, not a pointer
	// HtmlHelp(NULL, NULL, HH_INITIALIZE, (DWORD)&m_dwCookie);
}

void VNCHelp::Popup(LPARAM lParam) 
{
	// HTML Help is disabled in this build (htmlhelp.lib / HHCTRL.OCX do not
	// exist on Win32s, and linking htmlhelp.lib would put HtmlHelpA in the
	// import table and stop the EXE loading).  All the HtmlHelp calls below are
	// commented out, so this function now computes a popup topic and discards
	// it.
	//
	// The important part for the port is the guard: WM_HELP is a Win95+ message
	// and is never sent on Win32s, but the handlers that call this
	// (ClientConnection::WndProc1, the dialogs) pass lParam straight through.
	// A NULL lParam - which is what a stray or synthesised WM_HELP carries -
	// used to be dereferenced immediately.
	LPHELPINFO hlp = (LPHELPINFO) lParam;
	if (hlp == NULL)
		return;

	HH_POPUP popup;
	memset(&popup, 0, sizeof(popup));

	if (hlp->iCtrlId != 0) {
		
		popup.cbStruct = sizeof(popup);
		popup.hinst = pApp->m_instance;
		popup.idString = (UINT)hlp->iCtrlId;
		SetRect(&popup.rcMargins, -1, -1, -1, -1);
		popup.pszFont = "MS Sans Serif, 8, , ";
		popup.clrForeground = -1;
		popup.clrBackground = -1;
		popup.pt.x = -1;
		popup.pt.y = -1;

		switch  (hlp->iCtrlId) {
		case IDC_STATIC_LEVEL:
		case IDC_STATIC_TEXT_LEVEL:
		case IDC_STATIC_FAST:
		case IDC_STATIC_BEST:
			popup.idString = IDC_COMPRESSLEVEL;
			break;
		case IDC_STATIC_QUALITY:
		case IDC_STATIC_TEXT_QUALITY:
		case IDC_STATIC_POOR:
		case IDC_STATIC_QBEST:
			popup.idString = IDC_QUALITYLEVEL;
			break;
		case IDC_STATIC_ENCODING:
			popup.idString = IDC_ENCODING;
			break;
		case IDC_STATIC_SCALE:
		case IDC_STATIC_P:
			popup.idString = IDC_SCALE_EDIT;
			break;
		case IDC_STATIC_SERVER:
			popup.idString = IDC_HOSTNAME_EDIT;
			break;
		case IDC_STATIC_LIST:
		case IDC_SPIN1:
			popup.idString = IDC_EDIT_AMOUNT_LIST;
			break;
		case IDC_STATIC_LOG_LEVEL:
		case IDC_SPIN2:
			popup.idString = IDC_EDIT_LOG_LEVEL;
			break;
		case IDC_SPIN3:
		case IDC_STATIC_PORT:
			popup.idString = IDC_LISTEN_PORT;
			break;
		}

		/* HtmlHelp((HWND)hlp->hItemHandle,
				 NULL,
				 HH_DISPLAY_TEXT_POPUP,
				 (DWORD)&popup); */
	}
}

BOOL VNCHelp::TranslateMsg(MSG *pmsg)
{
	// HTML Help disabled - see Popup() above.  Return FALSE (not NULL: this is
	// a BOOL, and returning NULL for a BOOL is exactly the kind of thing MSVC
	// 4.1 accepts silently and a reader misreads).
	// (void)pmsg;
	return FALSE;
}

VNCHelp::~VNCHelp()
{
	// HtmlHelp(NULL, NULL, HH_UNINITIALIZE, (DWORD)m_dwCookie);
}

