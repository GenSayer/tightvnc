//  Copyright (C) 2000 Constantin Kaplinsky. All Rights Reserved.
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

// XCursor and RichCursor encodings
//
// Support for cursor shape updates for ClientConnection class.

#include "stdhdrs.h"
#include "vncviewer.h"
#include "ClientConnection.h"
// ClientConnection.h does NOT include Exception.h (nor does anything it pulls
// in), so any file that names WarningException/QuietException/ErrorException
// must include it itself.  This file throws ErrorException on allocation
// failure in ReadCursorShape().
#include "Exception.h"

void ClientConnection::ReadCursorShape(rfbFramebufferUpdateRectHeader *pfburh) {

	vnclog.Print(6, _T("Receiving cursor shape update, cursor %dx%d\n"),
				 (int)pfburh->r.w, (int)pfburh->r.h);

	int bytesPerRow = (pfburh->r.w + 7) / 8;
	int bytesMaskData = bytesPerRow * pfburh->r.h;
	int bytesSourceData =
		pfburh->r.w * pfburh->r.h * (m_myFormat.bitsPerPixel / 8);
	CheckBufferSize(bytesMaskData);

	SoftCursorFree();

	if (pfburh->r.w * pfburh->r.h == 0)
		return;

	// Ignore cursor shape updates if requested by user
	if (m_opts.m_ignoreShapeUpdates) {
		int bytesToSkip = (pfburh->encoding == rfbEncodingXCursor) ?
			(6 + 2 * bytesMaskData) : (bytesSourceData + bytesMaskData);
		CheckBufferSize(bytesToSkip);
		ReadExact(m_netbuf, bytesToSkip);
		return;
	}

	// Read cursor pixel data.
	//
	// Bound the cursor size.  w and h are 16-bit server-supplied values, so
	// w*h can be up to ~4 billion; "new COLORREF[w*h]" then either throws, or
	// on MSVC 4.1 returns NULL, and the loops below write through it.  A cursor
	// larger than 128x128 is not meaningful on any platform this build targets.
	if (pfburh->r.w > 128 || pfburh->r.h > 128) {
		vnclog.Print(0, _T("Refusing %dx%d cursor from server\n"),
					 (int)pfburh->r.w, (int)pfburh->r.h);
		int bytesToSkip = (pfburh->encoding == rfbEncodingXCursor) ?
			(6 + 2 * bytesMaskData) : (bytesSourceData + bytesMaskData);
		CheckBufferSize(bytesToSkip);
		ReadExact(m_netbuf, bytesToSkip);
		return;
	}

	rcSource = new COLORREF[pfburh->r.w * pfburh->r.h];
	if (rcSource == NULL)
		throw ErrorException("Out of memory reading cursor shape.");

	if (pfburh->encoding == rfbEncodingXCursor) {
		CARD8 xcolors[6];
		ReadExact((char *)xcolors, 6);
		COLORREF rcolors[2];
		rcolors[1] = PALETTERGB(xcolors[0], xcolors[1], xcolors[2]);
		rcolors[0] = PALETTERGB(xcolors[3], xcolors[4], xcolors[5]);

		ReadExact(m_netbuf, bytesMaskData);
		int x, y, n, b;
		int i = 0;
		for (y = 0; y < pfburh->r.h; y++) {
			for (x = 0; x < pfburh->r.w / 8; x++) {
				b = m_netbuf[y * bytesPerRow + x];
				for (n = 7; n >= 0; n--)
					rcSource[i++] = rcolors[b >> n & 1];
			}
			for (n = 7; n >= 8 - pfburh->r.w % 8; n--) {
				rcSource[i++] = rcolors[m_netbuf[y * bytesPerRow + x] >> n & 1];
			}
		}
	} else {
		// rfb.EncodingRichCursor
		CheckBufferSize(bytesSourceData);
		ReadExact(m_netbuf, bytesSourceData);
		SETUP_COLOR_SHORTCUTS;
		char *p = m_netbuf;
		for (int i = 0; i < pfburh->r.w * pfburh->r.h; i++) {
			switch (m_myFormat.bitsPerPixel) {
			case 8:
				rcSource[i] = COLOR_FROM_PIXEL8_ADDRESS(p);
				p++;
				break;
			case 16:
				rcSource[i] = COLOR_FROM_PIXEL16_ADDRESS(p);
				p += 2;
				break;
			case 32:
				rcSource[i] = COLOR_FROM_PIXEL32_ADDRESS(p);
				p += 4;
				break;
			}
		}
	}

	// Read and decode mask data.

	ReadExact(m_netbuf, bytesMaskData);

	rcMask = new bool[pfburh->r.w * pfburh->r.h];
	if (rcMask == NULL) {
		delete [] rcSource;
		rcSource = NULL;
		throw ErrorException("Out of memory reading cursor mask.");
	}

	int x, y, n, b;
	int i = 0;
	for (y = 0; y < pfburh->r.h; y++) {
		for (x = 0; x < pfburh->r.w / 8; x++) {
			b = m_netbuf[y * bytesPerRow + x];
			for (n = 7; n >= 0; n--)
				rcMask[i++] = (b >> n & 1) != 0;
		}
		for (n = 7; n >= 8 - pfburh->r.w % 8; n--) {
			rcMask[i++] = (m_netbuf[y * bytesPerRow + x] >> n & 1) != 0;
		}
	}

	// Set remaining data associated with cursor.

	omni_mutex_lock l(m_cursorMutex);

	rcWidth = pfburh->r.w;
	rcHeight = pfburh->r.h;
	rcHotX = (pfburh->r.x < rcWidth) ? pfburh->r.x : rcWidth - 1;
	rcHotY = (pfburh->r.y < rcHeight) ? pfburh->r.y : rcHeight - 1;

	{
		omni_mutex_lock l(m_bitmapdcMutex);
		ObjectSelector b1(m_hBitmapDC, m_hBitmap);
		PaletteSelector ps1(m_hBitmapDC, m_hPalette);
		m_hSavedAreaDC = CreateCompatibleDC(m_hBitmapDC);
		m_hSavedAreaBitmap =
			CreateCompatibleBitmap(m_hBitmapDC, rcWidth, rcHeight);
	}

	// Neither result was checked.  On Win32s the DC pool and the GDI heap are
	// both small, and every SoftCursorDraw/SaveArea call below BitBlts through
	// these handles.
	if (m_hSavedAreaDC == NULL || m_hSavedAreaBitmap == NULL) {
		vnclog.Print(0, _T("Could not create cursor save area - disabling local cursor\n"));
		if (m_hSavedAreaBitmap != NULL) {
			DeleteObject(m_hSavedAreaBitmap);
			m_hSavedAreaBitmap = NULL;
		}
		if (m_hSavedAreaDC != NULL) {
			DeleteDC(m_hSavedAreaDC);
			m_hSavedAreaDC = NULL;
		}
		delete [] rcSource; rcSource = NULL;
		delete [] rcMask;   rcMask = NULL;
		prevCursorSet = false;
		return;
	}

	SoftCursorSaveArea();
	SoftCursorDraw();

	rcCursorHidden = false;
	rcLockSet = false;

	prevCursorSet = true;
}

void ClientConnection::ReadCursorPos(rfbFramebufferUpdateRectHeader *pfburh)
{
	int x = (int)pfburh->r.x;
	if (x >= m_si.framebufferWidth)
		x = m_si.framebufferWidth - 1;
	int y = (int)pfburh->r.y;
	if (y >= m_si.framebufferHeight)
		y = m_si.framebufferHeight - 1;

	SoftCursorMove(x, y);
}

//
// SoftCursorLockArea(). This method should be used to prevent
// collisions between simultaneous framebuffer update operations and
// cursor drawing operations caused by movements of pointing device.
// The parameters denote a rectangle where mouse cursor should not
// be drawn. Every next call to this function expands locked area so
// previous locks remain active.
//

void ClientConnection::SoftCursorLockArea(int x, int y, int w, int h) {

	omni_mutex_lock l(m_cursorMutex);

	if (!prevCursorSet)
		return;

	if (!rcLockSet) {
		rcLockX = x;
		rcLockY = y;
		rcLockWidth = w;
		rcLockHeight = h;
		rcLockSet = true;
	} else {
		int newX = (x < rcLockX) ? x : rcLockX;
		int newY = (y < rcLockY) ? y : rcLockY;
		rcLockWidth = (x + w > rcLockX + rcLockWidth) ?
			(x + w - newX) : (rcLockX + rcLockWidth - newX);
		rcLockHeight = (y + h > rcLockY + rcLockHeight) ?
			(y + h - newY) : (rcLockY + rcLockHeight - newY);
		rcLockX = newX;
		rcLockY = newY;
	}

	if (!rcCursorHidden && SoftCursorInLockedArea()) {
		SoftCursorRestoreArea();
		rcCursorHidden = true;
	}
}

//
// SoftCursorUnlockScreen(). This function discards all locks
// performed since previous SoftCursorUnlockScreen() call.
//

void ClientConnection::SoftCursorUnlockScreen() {

	omni_mutex_lock l(m_cursorMutex);

	if (!prevCursorSet)
		return;

	if (rcCursorHidden) {
		SoftCursorSaveArea();
		SoftCursorDraw();
		rcCursorHidden = false;
	}
	rcLockSet = false;
}

//
// SoftCursorMove(). Moves soft cursor in particular location. This
// function respects locking of screen areas so when the cursor is
// moved in the locked area, it becomes invisible until
// SoftCursorUnlockScreen() method is called.
//

void ClientConnection::SoftCursorMove(int x, int y) {

	omni_mutex_lock l(m_cursorMutex);

	if (prevCursorSet && !rcCursorHidden) {
		SoftCursorRestoreArea();
		rcCursorHidden = true;
	}

	rcCursorX = x;
	rcCursorY = y;

	if (prevCursorSet && !(rcLockSet && SoftCursorInLockedArea())) {
		SoftCursorSaveArea();
		SoftCursorDraw();
		rcCursorHidden = false;
	}
}

 //
 // Free all data associated with cursor.
 //

void ClientConnection::SoftCursorFree() {

	omni_mutex_lock l(m_cursorMutex);

	if (prevCursorSet) {
		if (!rcCursorHidden)
			SoftCursorRestoreArea();
		// Clear each handle/pointer after releasing it.  The originals were
		// left dangling, and SoftCursorFree() is called more than once in
		// sequence on some paths (ReadCursorShape calls it at the top, and
		// ReadNewFBSize/SendAppropriateFramebufferUpdateRequest call it too).
		// prevCursorSet guarded that by luck; being explicit removes the
		// double-delete risk entirely, which matters on Win32s where GDI
		// handle reuse is immediate.
		if (m_hSavedAreaBitmap != NULL) {
			DeleteObject(m_hSavedAreaBitmap);
			m_hSavedAreaBitmap = NULL;
		}
		if (m_hSavedAreaDC != NULL) {
			DeleteDC(m_hSavedAreaDC);
			m_hSavedAreaDC = NULL;
		}
		if (rcSource != NULL) {
			delete[] rcSource;
			rcSource = NULL;
		}
		if (rcMask != NULL) {
			delete[] rcMask;
			rcMask = NULL;
		}
		prevCursorSet = false;
		rcCursorHidden = false;
		rcLockSet = false;
	}
}

//////////////////////////////////////////////////////////////////
//
// Low-level methods implementing software cursor functionality.
//

//
// Check if cursor is within locked part of screen.
//

bool ClientConnection::SoftCursorInLockedArea() {

    return (rcLockX < rcCursorX - rcHotX + rcWidth &&
			rcLockY < rcCursorY - rcHotY + rcHeight &&
			rcLockX + rcLockWidth > rcCursorX - rcHotX &&
			rcLockY + rcLockHeight > rcCursorY - rcHotY);
}

//
// Save screen data in memory buffer.
//

void ClientConnection::SoftCursorSaveArea() {

	// All three low-level helpers are reachable while the cursor state is
	// half-built (ReadCursorShape calls SaveArea/Draw immediately after
	// creating the DCs) and after it has been torn down.  The originals ran
	// BitBlt / SETPIXEL through whatever the handles happened to be.
	if (m_hBitmapDC == NULL || m_hBitmap == NULL ||
		m_hSavedAreaDC == NULL || m_hSavedAreaBitmap == NULL)
		return;

	RECT r;
	SoftCursorToScreen(&r, NULL);
	int x = r.left;
	int y = r.top;
	int w = r.right - r.left;
	int h = r.bottom - r.top;

	omni_mutex_lock l(m_bitmapdcMutex);
	ObjectSelector b1(m_hBitmapDC, m_hBitmap);
	PaletteSelector ps1(m_hBitmapDC, m_hPalette);
	ObjectSelector b2(m_hSavedAreaDC, m_hSavedAreaBitmap);
	PaletteSelector ps2(m_hSavedAreaDC, m_hPalette);

	if (!BitBlt(m_hSavedAreaDC, 0, 0, w, h, m_hBitmapDC, x, y, SRCCOPY)) {
		vnclog.Print(0, _T("Error saving screen under cursor\n"));
	}
}

//
// Restore screen data saved in memory buffer.
//

void ClientConnection::SoftCursorRestoreArea() {

	if (m_hBitmapDC == NULL || m_hBitmap == NULL ||
		m_hSavedAreaDC == NULL || m_hSavedAreaBitmap == NULL)
		return;

	RECT r;
	SoftCursorToScreen(&r, NULL);
	int x = r.left;
	int y = r.top;
	int w = r.right - r.left;
	int h = r.bottom - r.top;

	omni_mutex_lock l(m_bitmapdcMutex);
	ObjectSelector b1(m_hBitmapDC, m_hBitmap);
	PaletteSelector ps1(m_hBitmapDC, m_hPalette);
	ObjectSelector b2(m_hSavedAreaDC, m_hSavedAreaBitmap);
	PaletteSelector ps2(m_hSavedAreaDC, m_hPalette);

	if (!BitBlt(m_hBitmapDC, x, y, w, h, m_hSavedAreaDC, 0, 0, SRCCOPY)) {
		vnclog.Print(0, _T("Error restoring screen under cursor\n"));
	}

	InvalidateScreenRect(&r);
}

//
// Draw cursor.
//

void ClientConnection::SoftCursorDraw() {

	if (m_hBitmapDC == NULL || m_hBitmap == NULL ||
		rcSource == NULL || rcMask == NULL)
		return;

 	omni_mutex_lock l(m_bitmapdcMutex);
 	ObjectSelector b(m_hBitmapDC, m_hBitmap);
 	PaletteSelector p(m_hBitmapDC, m_hPalette);
 
	// WIN32S: the old per-pixel SETPIXEL loop drew nothing - GetPixel
	// readback returns the untouched background (DIAG: want 000000 got
	// c8ccc8), so SetPixel is a silent no-op on these memory DCs, exactly
	// like SetDIBitsToDevice was for the decoders.  Push the cursor with
	// the proven CreateDIBitmap+BitBlt path instead, with a classic
	// SRCAND/SRCPAINT masked blit for transparency:
	//   mask  = WHITE where the background shows (rcMask off)
	//   image = cursor COLORREF where rcMask is on, black elsewhere
	// Both bitmaps are tiny (server shapes are capped at 128x128 in
	// ReadCursorShape) and use only Win3.0-core calls.
	{
		int imgStride = ((rcWidth * 3) + 3) & ~3;
		int maskStride = ((rcWidth + 31) / 32) * 4;
		unsigned char *imgBits =
			new unsigned char[imgStride * rcHeight];
		unsigned char *maskBits =
			new unsigned char[maskStride * rcHeight];
		if (imgBits != NULL && maskBits != NULL) {
			int my, mx;
			memset(imgBits, 0, imgStride * rcHeight);
			memset(maskBits, 0xFF, maskStride * rcHeight);
			for (my = 0; my < rcHeight; my++) {
				unsigned char *imgRow =
					imgBits + (size_t)(rcHeight - 1 - my) * imgStride;
				unsigned char *maskRow =
					maskBits + (size_t)(rcHeight - 1 - my) * maskStride;
				for (mx = 0; mx < rcWidth; mx++) {
					int srcOff = my * rcWidth + mx;
					if (rcMask[srcOff]) {
						COLORREF c = rcSource[srcOff];
						*imgRow++ = (unsigned char)((c >> 16) & 0xFF);
						*imgRow++ = (unsigned char)((c >> 8) & 0xFF);
						*imgRow++ = (unsigned char)(c & 0xFF);
						maskRow[mx >> 3] &= ~(unsigned char)(0x80 >> (mx & 7));
					} else {
						imgRow += 3;
					}
				}
			}
 
			BITMAPINFOHEADER imgBih;
			memset(&imgBih, 0, sizeof(imgBih));
			imgBih.biSize = sizeof(BITMAPINFOHEADER);
			imgBih.biWidth = rcWidth;
			imgBih.biHeight = rcHeight;
			imgBih.biPlanes = 1;
			imgBih.biBitCount = 24;
			imgBih.biCompression = BI_RGB;
 
			struct {
				BITMAPINFOHEADER h;
				RGBQUAD pal[2];
			} maskBi;
			memset(&maskBi, 0, sizeof(maskBi));
			maskBi.h.biSize = sizeof(BITMAPINFOHEADER);
			maskBi.h.biWidth = rcWidth;
			maskBi.h.biHeight = rcHeight;
			maskBi.h.biPlanes = 1;
			maskBi.h.biBitCount = 1;
			maskBi.h.biCompression = BI_RGB;
			maskBi.pal[0].rgbBlue = maskBi.pal[0].rgbGreen =
				maskBi.pal[0].rgbRed = 0;
			maskBi.pal[1].rgbBlue = maskBi.pal[1].rgbGreen =
				maskBi.pal[1].rgbRed = 0xFF;
 
			int dstX = rcCursorX - rcHotX;
			int dstY = rcCursorY - rcHotY;
			HBITMAP hImg = CreateDIBitmap(m_hBitmapDC, &imgBih, CBM_INIT,
										  imgBits, (BITMAPINFO *)&imgBih,
										  DIB_RGB_COLORS);
			HBITMAP hMask = CreateDIBitmap(m_hBitmapDC, &maskBi.h, CBM_INIT,
										   maskBits, (BITMAPINFO *)&maskBi,
										   DIB_RGB_COLORS);
			if (hImg != NULL && hMask != NULL) {
				HDC sdc = CreateCompatibleDC(m_hBitmapDC);
				if (sdc != NULL) {
					HGDIOBJ old = SelectObject(sdc, hMask);
					BitBlt(m_hBitmapDC, dstX, dstY, rcWidth, rcHeight,
						   sdc, 0, 0, SRCAND);
					SelectObject(sdc, hImg);
					BitBlt(m_hBitmapDC, dstX, dstY, rcWidth, rcHeight,
						   sdc, 0, 0, SRCPAINT);
					SelectObject(sdc, old);
					DeleteDC(sdc);
				} else {
					vnclog.Print(0, _T("Could not stage cursor draw\n"));
				}
			} else {
				vnclog.Print(0, _T("Could not build cursor bitmaps\n"));
			}
			if (hImg != NULL)
				DeleteObject(hImg);
			if (hMask != NULL)
				DeleteObject(hMask);
		}
		if (imgBits != NULL)
			delete [] imgBits;
		if (maskBits != NULL)
			delete [] maskBits;
	}

	// TEMP-DIAG (invisible remote cursor): verify SETPIXEL actually lands on
	// this memory DC.  Bounded to the first cursor drawn.  Reads back one
	// masked pixel with GetPixel and compares against what was requested.
	{
		static int s_curDiag = 0;
		if (s_curDiag < 2 && rcWidth > 0 && rcHeight > 0) {
			s_curDiag++;
			int probe = -1;
			for (int pi = 0; pi < rcWidth * rcHeight; pi++) {
				if (rcMask[pi]) {
					probe = pi;
					break;
				}
			}
			if (probe >= 0) {
				int px = rcCursorX - rcHotX + (probe % rcWidth);
				int py = rcCursorY - rcHotY + (probe / rcWidth);
				COLORREF want = rcSource[probe];
				COLORREF got = GetPixel(m_hBitmapDC, px, py);
				vnclog.Print(0, _T("DIAG cursor %d: %dx%d hot %d,%d at %d,%d fb %dx%d probe %d,%d want %06lx got %06lx\n"),
							 s_curDiag, rcWidth, rcHeight, rcHotX, rcHotY,
							 rcCursorX, rcCursorY,
							 (int)m_si.framebufferWidth, (int)m_si.framebufferHeight,
							 px, py,
							 (unsigned long)(want & 0xFFFFFF),
							 (unsigned long)(got & 0xFFFFFF));
			} else {
				vnclog.Print(0, _T("DIAG cursor %d: %dx%d fully transparent mask\n"),
							 s_curDiag, rcWidth, rcHeight);
			}
		}
	}

 	RECT r;
 	SoftCursorToScreen(&r, NULL);
 	InvalidateScreenRect(&r);
}

//
// Calculate position, size and offset for the part of cursor
// located inside framebuffer bounds.
//

void ClientConnection::SoftCursorToScreen(RECT *screenArea, POINT *cursorOffset) {

	int cx = 0, cy = 0;

	int x = rcCursorX - rcHotX;
	int y = rcCursorY - rcHotY;
	int w = rcWidth;
	int h = rcHeight;

	if (x < 0) {
		cx = -x;
		w -= cx;
		x = 0;
	} else if (x + w > m_si.framebufferWidth) {
		w = m_si.framebufferWidth - x;
	}
	if (y < 0) {
		cy = -y;
		h -= cy;
		y = 0;
	} else if (y + h > m_si.framebufferHeight) {
		h = m_si.framebufferHeight - y;
	}

	if (w < 0) {
		cx = 0; x = 0; w = 0;
	}
	if (h < 0) {
		cy = 0; y = 0; h = 0;
	}

	if (screenArea != NULL) {
		SetRect(screenArea, x, y, x + w, y + h);
	}
	if (cursorOffset != NULL) {
		cursorOffset->x = cx;
		cursorOffset->y = cy;
	}
}

