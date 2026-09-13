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

// Raw Encoding
//
// The bits of the ClientConnection object to do with Raw.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ClientConnection.h"

void ClientConnection::ReadRawRect(rfbFramebufferUpdateRectHeader *pfburh) {

	UINT numpixels = pfburh->r.w * pfburh->r.h;
    // this assumes at least one byte per pixel. Naughty.
	UINT numbytes = numpixels * m_minPixelBytes;
	// TEMP-DIAG (Raw blank): prove the decoder runs with sane geometry.
	// Bounded to the first 3 rects so the log stays small.
	{
		static int s_rawDiag = 0;
		if (s_rawDiag < 3) {
			s_rawDiag++;
			vnclog.Print(0, _T("DIAG Raw rect %d: %dx%d at %d,%d bpp=%d bytes=%u\n"),
						 s_rawDiag, (int)pfburh->r.w, (int)pfburh->r.h,
						 (int)pfburh->r.x, (int)pfburh->r.y,
						 (int)m_myFormat.bitsPerPixel, (unsigned)numbytes);
		}
	}
	// Read in the whole thing
    CheckBufferSize(numbytes);
	ReadExact(m_netbuf, numbytes);

	SETUP_COLOR_SHORTCUTS;

	{
		// (No-op lock: single-threaded.)
		omni_mutex_lock l(m_bitmapdcMutex);
		ObjectSelector b(m_hBitmapDC, m_hBitmap);
		PaletteSelector p(m_hBitmapDC, m_hPalette);

		// WIN32S: was SETPIXELS(), i.e. one SetPixel call per pixel - 307,200
		// GDI thunks for a 640x480 update, which is why the first screen took
		// tens of seconds.  DrawPixelBlock builds a DIB and issues one
		// SetDIBitsToDevice per band of rows instead.  See the long comment on
		// DrawPixelBlock in ClientConnection.cpp.
		switch (m_myFormat.bitsPerPixel) {
		case 8:
			DrawPixelBlock(m_netbuf, 8, pfburh->r.x, pfburh->r.y,
						   pfburh->r.w, pfburh->r.h);
			break;
		case 16:
			DrawPixelBlock(m_netbuf, 16, pfburh->r.x, pfburh->r.y,
						   pfburh->r.w, pfburh->r.h);
			break;
		case 24:
		case 32:
			DrawPixelBlock(m_netbuf, 32, pfburh->r.x, pfburh->r.y,
						   pfburh->r.w, pfburh->r.h);
			break;
		default:
			vnclog.Print(0, _T("Invalid number of bits per pixel: %d\n"), m_myFormat.bitsPerPixel);
			return;
		}
		
	}
}

