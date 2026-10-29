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

// VNCHelp.h: interface for the VNCHelp class.

#ifndef VNCHELP_H__
#define VNCHELP_H__

// The help system is chosen at compile time (build.cmd defines one of these):
//
//   VNCHELP_MINI   built-in text popups (VNCHelp.cpp); needs neither
//                  htmlhelp.lib nor HtmlHelp.h nor hhctrl.ocx
//   VNCHELP_NONE   no context help at all
//
// With neither defined (the Developer Studio projects), VNCHELP_MINI.  The
// HtmlHelp popups (VNCHELP_HTML) are only in vnc_winsrc and vnc_winsrc_64:
// HTML Help does not exist on the systems this tree is for.
#ifdef VNCHELP_HTML
#error VNCHELP_HTML is not supported in this tree; use VNCHELP_MINI
#endif
#if !defined(VNCHELP_MINI) && !defined(VNCHELP_NONE)
#define VNCHELP_MINI
#endif
#if defined(VNCHELP_MINI) && defined(VNCHELP_NONE)
#error Define only one of VNCHELP_MINI and VNCHELP_NONE
#endif

class VNCHelp  
{
	DWORD m_dwCookie;
public:
	VNCHelp();

	// Shows the help for the control in a WM_HELP HELPINFO (lParam).
	void Popup(LPARAM lParam);

	// Called for every message of the application's message loop; TRUE if
	// the message was used (F1 on NT 3.x and Win32s).
	BOOL TranslateMsg(MSG *pmsg);

	// Called from WM_INITDIALOG of a top-level dialog that has context help.
	// Gives it the caption "?" button: the system's from NT 3.51 on, and one
	// drawn by VNCHelp on NT 3.1, NT 3.5 and Win32s, which also get F1; with
	// VNCHELP_NONE it removes the "?" button instead.
	static void InitDialog(HWND hwnd);

	virtual ~VNCHelp();
};

#endif
