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
// whence you received this file, check http://www.uk.research.att.com/vnc or 
// contact the authors on vnc@uk.research.att.com for information on obtaining it.
//

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ClientConnection.h"
#include "Exception.h"
#include "vncauth.h"


// This file contains the code for saving and loading connection info.

static OPENFILENAME ofn;

static void ofnInit()
{
	static char filter[] = "VNC files (*.vnc)\0*.vnc\0" \
						   "All files (*.*)\0*.*\0";
	memset((void *) &ofn, 0, sizeof(OPENFILENAME));

	// WIN32S: lStructSize must be the *Windows 3.1-era* size.
	//
	// COMDLG32 identifies the caller's OPENFILENAME version by this field.
	// The MSVC 4.1 SDK's OPENFILENAME is the original Win32 structure, so
	// sizeof() is correct for this build - but be explicit about the intent,
	// because if this project is ever built with a newer SDK the structure
	// grows (pvReserved/dwReserved/FlagsEx) and GetOpenFileName then fails on
	// Win32s with CDERR_STRUCTSIZE and returns 0, which LoadConnection reports
	// as "user cancelled".
	ofn.lStructSize = sizeof(OPENFILENAME);

	ofn.lpstrFilter = filter;
	ofn.nMaxFile = _MAX_PATH;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrDefExt = "vnc";
}

//
// SaveConnection
// Save info about current connection to a file
//

void ClientConnection::SaveConnection()
{
	vnclog.Print(2, _T("Saving connection info\n"));	
	char tname[_MAX_FNAME + _MAX_EXT];
	ofnInit();

	// Let's choose a reasonable file name based on hostname and port.
	//
	// WIN32S: the default filename must satisfy 16-bit COMMDLG, which fails
	// the whole call with FNERR_INVALIDFILENAME ("Invalid filename", no dialog
	// at all) for anything that is not 8.3-safe.  The old code copied up to 24
	// hostname chars verbatim, so an IP literal became "192.168.1.207.vnc"
	// (multiple dots - illegal) and an empty host became ".vnc".  Build an
	// 8.3-safe base instead: alphanumerics only, at most 8 chars, then ".vnc".
	// The port suffix is kept only if the base still fits 8.3.
	char fname[_MAX_PATH];
	int fn = 0;
	const char *hp = m_host;
	while (*hp != '\0' && fn < 8) {
		if (isalnum((unsigned char)*hp))
			fname[fn++] = *hp;
		hp++;
	}
	if (fn == 0) {
		strcpy(fname, "server");
		fn = 6;
	} else {
		fname[fn] = '\0';
	}
	// Append the port number if it's not the default port and still fits
	if (PORT_TO_DISPLAY(m_port) != 0) {
		char pbuf[8];
		sprintf(pbuf, "-%d", m_port);
		if (fn + (int)strlen(pbuf) <= 8) {
			strcpy(fname + fn, pbuf);
			fn += (int)strlen(pbuf);
		}
	}
	// Finally, append the .vnc suffix (note there will be no buffer overrun)
	strcat(fname, ".vnc");

	ofn.hwndOwner = m_hwnd;
	ofn.lpstrFile = fname;
	ofn.lpstrFileTitle = tname; 
	ofn.Flags = OFN_HIDEREADONLY;
	if (!GetSaveFileName(&ofn)) {
		DWORD err = CommDlgExtendedError();
		char msg[1024]; 
		switch(err) {
		case 0:	// user cancelled
			break;
		case FNERR_INVALIDFILENAME:
			strcpy(msg, "Invalid filename");
			MessageBox(m_hwnd, msg, "Error saving file", MB_ICONERROR | MB_OK);
			break;
		default:
			vnclog.Print(0, "Error %d from GetSaveFileName\n", err);
			break;
		}
		return;
	}
	vnclog.Print(1, "Saving to %s\n", fname);	
	int ret = WritePrivateProfileString("connection", "host", m_host, fname);
	char buf[32];
	sprintf(buf, "%d", m_port);
	WritePrivateProfileString("connection", "port", buf, fname);
	buf[0] = '\0';
	if (m_authScheme == rfbAuthVNC) {
		if (MessageBox(m_hwnd,
			"Do you want to save the password in this file?\n\r"
			"If you say Yes, anyone with access to this file could access your session\n\r"
			"and (potentially) discover your VNC password.",  
			"Security warning", 
			MB_YESNO | MB_ICONWARNING) == IDYES) 
		{
			for (int i = 0; i < MAXPWLEN; i++) {
				sprintf(buf+i*2, "%02x", (unsigned int) m_encPasswd[i]);
			}
			WritePrivateProfileString("connection", "password", buf, fname);
		}
	}
	m_opts.Save(fname);
	m_opts.Register();
}

// returns zero if successful
int ClientConnection::LoadConnection(char *fname, bool sess)
{
	if (sess) {
		char tname[_MAX_FNAME + _MAX_EXT];
		
		ofnInit();
		
		ofn.hwndOwner = m_hSess;
		ofn.lpstrFile = fname;
		ofn.lpstrFileTitle = tname;
		ofn.Flags = OFN_HIDEREADONLY;

		if (GetOpenFileName(&ofn) == 0) {
			return -1;
		}
	}
	// GetPrivateProfileString needs a real path.  A relative name is resolved
	// against the Windows directory, which is rarely what the user meant, but
	// leave that behaviour alone - just make sure fname is not NULL/empty,
	// which happens when the /config switch is given without a value.
	if (fname == NULL || fname[0] == '\0') {
		MessageBox(m_hwnd, "No configuration file specified", "Config file error",
				   MB_ICONERROR | MB_OK);
		return -1;
	}
	if (GetPrivateProfileString("connection", "host", "", m_host, MAX_HOST_NAME_LEN, fname) == 0) {
		MessageBox(m_hwnd, "Error reading host name from file", "Config file error", MB_ICONERROR | MB_OK);
		return -1;
	}
	if ((m_port = GetPrivateProfileInt("connection", "port", 0, fname)) == 0) {
		MessageBox(m_hwnd, "Error reading port number from file", "Config file error", MB_ICONERROR | MB_OK);
		return -1;
	}
	FormatDisplay(m_port, m_opts.m_display, m_host);

	char buf[1026];
	m_passwdSet = false;
	memset(buf, 0, sizeof(buf));
	// The password is stored as MAXPWLEN hex byte pairs.  Require the full
	// length before parsing: the old code accepted any non-empty string and
	// then read buf+i*2 for i up to MAXPWLEN-1, running past the terminator
	// (and past the 32-byte limit it passed to GetPrivateProfileString) for a
	// short or truncated entry.
	DWORD pwLen = GetPrivateProfileString("connection", "password", "",
										  buf, 32, fname);
	if (pwLen >= (DWORD)(MAXPWLEN * 2)) {
		for (int i = 0; i < MAXPWLEN; i++)	{
			int x = 0;
			if (sscanf(buf + i * 2, "%2x", &x) != 1) {
				m_passwdSet = false;
				memset(m_encPasswd, 0, sizeof(m_encPasswd));
				break;
			}
			m_encPasswd[i] = (unsigned char) x;
			m_passwdSet = true;
		}
	}
	if (sess) {
		m_opts.Load(fname);
		m_opts.Register();
	}
	return 0;
}
