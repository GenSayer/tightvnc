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

// This file contains the code for getting text from, and putting text into
// the Windows clipboard.

//
// ProcessClipboardChange
// Called by ClientConnection::WndProc.
// We've been informed that the local clipboard has been updated.
// If it's text we want to send it to the server.
//

void ClientConnection::ProcessLocalClipboardChange()
{
	vnclog.Print(2, _T("Clipboard changed\n"));

	// WM_DRAWCLIPBOARD arrives for as long as we are in the viewer chain, which
	// includes the window-destruction window.  Also skip the work entirely if
	// the session is not live - SendClientCutText would just fail.
	if (m_hwnd == NULL)
		return;

	HWND hOwner = GetClipboardOwner();
	if (hOwner == m_hwnd) {
		vnclog.Print(2, _T("We changed it - ignore!\n"));
	} else if (!m_initialClipboardSeen) {
		vnclog.Print(2, _T("Don't send initial clipboard!\n"));
		m_initialClipboardSeen = true;
	} else if (!m_opts.m_DisableClipboard) {
		
		// (No-op lock: single-threaded.)
		omni_mutex_lock l(m_clipMutex);

		// do { ... } while(0) so that the early exits below use "break" and
		// still fall through to the clipboard-chain pass at the end of the
		// function.  A plain "return" here would silently drop us out of the
		// viewer chain's message relay, which breaks every other clipboard
		// viewer on the system.
		do {
		if (!m_running || m_sock == INVALID_SOCKET)
			break;
		
		if (OpenClipboard(m_hwnd)) { 
			HGLOBAL hglb = GetClipboardData(CF_TEXT); 
			if (hglb == NULL) {
				CloseClipboard();
			} else {
				LPSTR lpstr = (LPSTR) GlobalLock(hglb);  
				if (lpstr == NULL) {
					// GlobalLock can fail; the original dereferenced the result
					// immediately in strlen().
					CloseClipboard();
				} else {
				size_t srclen = strlen(lpstr);

				// Cap what we send.  A user copying a large file's worth of text
				// on the host would otherwise make the viewer allocate two
				// buffers of that size out of the Win32s heap and then block in
				// send() until it all goes out.
				if (srclen > 0x00010000)		// 64 KB
					srclen = 0x00010000;

				char *contents = new char[srclen + 1];
				char *unixcontents = new char[srclen + 1];
				if (contents == NULL || unixcontents == NULL) {
					if (contents != NULL) delete [] contents;
					if (unixcontents != NULL) delete [] unixcontents;
					GlobalUnlock(hglb);
					CloseClipboard();
					break;
				}
				memcpy(contents, lpstr, srclen);
				contents[srclen] = '\0';
				GlobalUnlock(hglb); 
				CloseClipboard();       		
				
				// Translate to Unix-format lines before sending
				size_t j = 0;
				for (size_t i = 0; i < srclen && contents[i] != '\0'; i++) {
					if (contents[i] != '\x0d') {
						unixcontents[j++] = contents[i];
					}
				}
				unixcontents[j] = '\0';
				try {
					SendClientCutText(unixcontents, j);
				} catch (WarningException &e) {
					vnclog.Print(0, _T("Exception while sending clipboard text : %s\n"), e.m_info);
					if (m_hwnd1 != NULL)
						PostMessage(m_hwnd1, WM_CLOSE, 0, 0);
					else
						m_dead = true;
				}
				delete [] contents; 
				delete [] unixcontents;
				}
			}
		}
		} while (0);
	}
	// Pass the message to the next window in clipboard viewer chain.
	// Guard against NULL: we are only in the chain if SetClipboardViewer
	// succeeded, and it returns NULL when we are the only viewer.
	if (m_hwndNextViewer != NULL)
		::SendMessage(m_hwndNextViewer, WM_DRAWCLIPBOARD , 0,0); 
}

// We've read some text from the remote server, and
// we need to copy it into the local clipboard.
// Called by ClientConnection::ReadServerCutText()

void ClientConnection::UpdateLocalClipboard(char *buf, size_t len) {
	
	if (m_opts.m_DisableClipboard)
		return;

	if (buf == NULL)
		return;

	// Copy to wincontents replacing LF with CR-LF.
	//
	// Two bugs fixed here.  First, the loop condition read m_netbuf[i] but
	// indexed buf[i]: those are the same pointer today only because the caller
	// happens to pass m_netbuf, and CheckBufferSize() can reallocate m_netbuf,
	// so this was a latent wild read.  Second, nothing bounded the loop by len,
	// so a server payload without a NUL ran off the end of the buffer.
	char *wincontents = new char[len * 2 + 1];
	if (wincontents == NULL)
		return;
	size_t j = 0;
	for (size_t i = 0; i < len && buf[i] != '\0'; i++) {
        if (buf[i] == '\x0a') {
			wincontents[j++] = '\x0d';
        }
		wincontents[j++] = buf[i];
	}
	wincontents[j] = '\0';
	size_t winlen = j;

    // (The clipboard mutex is a no-op now: single-threaded.)
    {
        omni_mutex_lock l(m_clipMutex);

        if (!OpenClipboard(m_hwnd)) {
			delete [] wincontents;
	        throw WarningException("Failed to open clipboard\n");
        }
        if (! ::EmptyClipboard()) {
			CloseClipboard();
			delete [] wincontents;
	        throw WarningException("Failed to empty clipboard\n");
        }

        // Allocate a global memory object for the text. 
        HGLOBAL hglbCopy = GlobalAlloc(GMEM_DDESHARE, (winlen + 1) * sizeof(TCHAR));
        if (hglbCopy != NULL) { 
	        // Lock the handle and copy the text to the buffer.  
	        LPTSTR lptstrCopy = (LPTSTR) GlobalLock(hglbCopy); 
			if (lptstrCopy != NULL) {
				memcpy(lptstrCopy, wincontents, winlen * sizeof(TCHAR));
				lptstrCopy[winlen] = (TCHAR) 0;    // null character
				GlobalUnlock(hglbCopy);          // Place the handle on the clipboard.
				SetClipboardData(CF_TEXT, hglbCopy);
			}
        }

        delete [] wincontents;

        if (! ::CloseClipboard()) {
	        throw WarningException("Failed to close clipboard\n");
        }
    }
}
