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

// HotKeys.cpp: implementation of the HotKeys class.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "HotKeys.h"

HotKeys::HotKeys()
{
	m_hwnd = 0;
	m_hAccel = NULL;

	// NOTE: this constructor runs before WinMain (file-scope object).  It only
	// calls CreateAcceleratorTable, which is safe on Win32s - but do not add
	// anything here that needs the app object or a window.
	const int MAX_ACCELS = 16;
	ACCEL accel[MAX_ACCELS];
	int i = 0;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x4f;	// "O"
	accel[i++].cmd = IDC_OPTIONBUTTON;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x49;	// "I"
	accel[i++].cmd = ID_CONN_ABOUT;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x46;	// "F"
	accel[i++].cmd = ID_FULLSCREEN;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x52;	// "R"
	accel[i++].cmd = ID_REQUEST_REFRESH;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x4e;	// "N"
	accel[i++].cmd = ID_NEWCONN;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x53;	// "S"
	accel[i++].cmd = ID_CONN_SAVE_AS;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x54;	// "T"
	accel[i++].cmd = ID_TOOLBAR;

	accel[i].fVirt = FVIRTKEY | FALT | FCONTROL | FSHIFT | FNOINVERT;
	accel[i].key = 0x45;	// "E"
	accel[i++].cmd = IDD_FILETRANSFER;

	int numKeys = i;
	// Was assert(): compiled out of a release build, and there is no reason for
	// a bounds check to be debug-only when overrunning accel[] corrupts the
	// stack of a pre-WinMain constructor.
	if (numKeys > MAX_ACCELS)
		numKeys = MAX_ACCELS;

	m_hAccel = CreateAcceleratorTable((LPACCEL)accel, numKeys);
}

bool HotKeys::TranslateAccel(MSG *pmsg)
{
	// This object is a file-scope global ("HotKeys hotkeys;" in vncviewer.cpp),
	// so it exists before any window does and outlives every connection.
	// m_hwnd is 0 until SetWindow() is called from CreateDisplay, and it is
	// never cleared when that window is destroyed - so both handles must be
	// checked here.  TranslateAccelerator with a stale HWND is an
	// invalid-window call on every message the viewer receives.
	if (m_hwnd == 0 || m_hAccel == NULL || pmsg == NULL)
		return false;
	if (!IsWindow(m_hwnd)) {
		m_hwnd = 0;
		return false;
	}
	return (TranslateAccelerator(m_hwnd, m_hAccel, pmsg) != 0);
}

HotKeys::~HotKeys()
{
	if (m_hAccel != NULL) {
		DestroyAcceleratorTable(m_hAccel);
		m_hAccel = NULL;
	}
}
