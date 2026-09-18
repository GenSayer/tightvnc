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

// RRE (Rising Rectangle Encoding)
//
// The bits of the ClientConnection object to do with RRE.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ClientConnection.h"

void ClientConnection::ReadRRERect(rfbFramebufferUpdateRectHeader *pfburh)
{
	// An RRE rect is always followed by a background color
	// For speed's sake we read them together into a buffer.
	// Portable: tmpbuf is char-aligned only, so all multibyte fields
	// are accessed via memcpy (safe on IA64/AXP64/ARM64/AMD64).
	char tmpbuf[sz_rfbRREHeader+4];			// biggest pixel is 4 bytes long
	CARD8 *pcolor = (CARD8 *) tmpbuf + sz_rfbRREHeader;
	ReadExact(tmpbuf, sz_rfbRREHeader + m_minPixelBytes);

	CARD32 nSubrects = Swap32IfLE(ReadUnaligned32(tmpbuf));
	
	SETUP_COLOR_SHORTCUTS;
    COLORREF color;
    switch (m_myFormat.bitsPerPixel) {
        case 8:
            color = COLOR_FROM_PIXEL8_ADDRESS(pcolor); break;
        case 16:
			color = COLOR_FROM_PIXEL16_ADDRESS(pcolor); break;
        case 24:
        case 32:
            color = COLOR_FROM_PIXEL32_ADDRESS(pcolor); break;
    }

	// Draw the background of the rectangle
	{
		omni_mutex_lock l(m_bitmapdcMutex);
		ObjectSelector b(m_hBitmapDC, m_hBitmap);
		PaletteSelector ps(m_hBitmapDC, m_hPalette);

		FillSolidRect(pfburh->r.x, pfburh->r.y, pfburh->r.w, pfburh->r.h, color);
	}
		
    if (nSubrects == 0) return;

	// Draw the sub-rectangles
    rfbRectangle rect;

	// The size of an RRE subrect including color info
	int subRectSize = m_minPixelBytes + sz_rfbRectangle;

	// Read subrects into the buffer
	CheckBufferSize((size_t)subRectSize * nSubrects);
    ReadExact(m_netbuf, subRectSize * nSubrects);
	BYTE *p = (BYTE *) m_netbuf;

	// No other threads can use bitmap DC
	omni_mutex_lock l(m_bitmapdcMutex);
	ObjectSelector b(m_hBitmapDC, m_hBitmap);
	PaletteSelector ps(m_hBitmapDC, m_hPalette);

	for (CARD32 i = 0; i < nSubrects; i++) {
		const BYTE *pRectBuf = p + m_minPixelBytes;

		switch (m_myFormat.bitsPerPixel) {
		case 8:
			color = COLOR_FROM_PIXEL8_ADDRESS(p); break;
		case 16:
			color = COLOR_FROM_PIXEL16_ADDRESS(p); break;
		case 32:
			color = COLOR_FROM_PIXEL32_ADDRESS(p); break;
		};

		// Portable: pRectBuf may be unaligned (p + 1..4), copy fields.
		CARD16 rx, ry, rw, rh;
		memcpy(&rx, pRectBuf, 2);
		memcpy(&ry, pRectBuf + 2, 2);
		memcpy(&rw, pRectBuf + 4, 2);
		memcpy(&rh, pRectBuf + 6, 2);
		rect.x = (CARD16) (Swap16IfLE(rx) + pfburh->r.x);
		rect.y = (CARD16) (Swap16IfLE(ry) + pfburh->r.y);
		rect.w = Swap16IfLE(rw);
		rect.h = Swap16IfLE(rh);
		
		FillSolidRect(rect.x, rect.y, rect.w, rect.h, color);
		p += subRectSize;
	}
}
