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


// vncRegion implementation
// This implementation uses the system region handling routines
// to speed things up and give the best results

#include "stdhdrs.h"

// Header

#include "vncRegion.h"

// Implementation

vncRegion::vncRegion()
{
	region = NULL;
}

vncRegion::~vncRegion()
{
	Clear();
}

// ==========================================================================
// WIN32S: CreateRectRgnIndirect -> CreateRectRgn.
//
// The diagnostic showed "full=0" on EVERY pump pass despite the log recording
// "FramebufferUpdateRequest(full) received", and "changed_rgn has 0 rects"
// throughout.  Both come back to this function: FullRgnRequested() is
// !m_full_rgn.IsEmpty(), IsEmpty() is (region == NULL), and the only thing that
// makes 'region' non-NULL for a fresh vncRegion is the call below.
//
// CreateRectRgnIndirect takes a POINTER TO A RECT.  That is the same 32->16 bit
// thunk pointer-marshalling case that has broken every other failure in this
// port:
//
//     GetDIBits(hdc, hbm, 0, 1, NULL, &bmi, ...)     format query - error 87
//     GetRegionData(region, NULL, 0)                 sizing call - returns 0
//     recv(sock, NULL, len, 0)                       ReadExact(NULL, n)
//
// CreateRectRgn takes FOUR INTEGERS and no pointer at all, so it cannot be
// affected.  It is the same age as CreateRectRgnIndirect (both Windows 1.0-era
// GDI) and produces an identical region - note that vncRegion::Combine() below
// already uses CreateRectRgn(0,0,0,0) successfully, which is direct evidence
// from this same file that the non-pointer form works on this platform.
//
// The return value is also CHECKED now.  Previously a failed creation left
// 'region' NULL and every caller silently treated the region as empty - which
// is exactly the behaviour that made this so hard to find: no error, no log
// line, just an update path that quietly did nothing.
// ==========================================================================

void vncRegion::AddRect(const RECT &new_rect)
{
	HRGN newregion;

	if (region == NULL)
	{
		// Create the region and set it to contain this rectangle
		region = CreateRectRgn(new_rect.left, new_rect.top,
							   new_rect.right, new_rect.bottom);
		if (region == NULL)
		{
			// Report this: a silent failure here disables the whole update path.
			// Rate-limited because AddRect is called per changed tile.
			static DWORD failCount = 0;
			if ((failCount++ % 100) == 0)
			{
				vnclog.Print(LL_INTERR,
					VNCLOG("CreateRectRgn(%d,%d,%d,%d) failed, error=%d "
						   "[%d occurrences] - no updates can be sent\n"),
					(int)new_rect.left, (int)new_rect.top,
					(int)new_rect.right, (int)new_rect.bottom,
					GetLastError(), (int)failCount);
			}
		}
	}
	else
	{
		// Create a new region containing the appropriate rectangle
		newregion = CreateRectRgn(new_rect.left, new_rect.top,
								  new_rect.right, new_rect.bottom);
		if (newregion == NULL)
		{
			static DWORD failCount2 = 0;
			if ((failCount2++ % 100) == 0)
			{
				vnclog.Print(LL_INTERR,
					VNCLOG("CreateRectRgn (merge) failed, error=%d "
						   "[%d occurrences]\n"),
					GetLastError(), (int)failCount2);
			}
			return;
		}

		// Merge it into the existing region
		if (CombineRgn(region, region, newregion, RGN_OR) == NULLREGION)
			Clear();

		// Now delete the temporary region
		DeleteObject(newregion);
	}
}

void vncRegion::AddRect(RECT R, int xoffset, int yoffset)
{
	R.left += xoffset;
	R.top += yoffset;
	R.right += xoffset;
	R.bottom += yoffset;
	AddRect(R);
}

void vncRegion::SubtractRect(RECT &new_rect)
{
	HRGN newregion;

	if (region == NULL)
		return;

	// Create a new region containing the appropriate rectangle.
	//
	// WIN32S: CreateRectRgn rather than CreateRectRgnIndirect - see the long note
	// on AddRect above.
	newregion = CreateRectRgn(new_rect.left, new_rect.top,
							  new_rect.right, new_rect.bottom);
	if (newregion == NULL)
		return;

	// Remove it from the existing region
	if (CombineRgn(region, region, newregion, RGN_DIFF) == NULLREGION)
		Clear();

	// Now delete the temporary region
	DeleteObject(newregion);
}

void vncRegion::Clear()
{
	// Set the region to be empty
	if (region != NULL)
	{
		DeleteObject(region);
		region = NULL;
	}
}

void
vncRegion::Combine(vncRegion &rgn)
{
	if (rgn.region == NULL)
		return;
	if (region == NULL)
	{
		region = CreateRectRgn(0, 0, 0, 0);
		if (region == NULL)
			return;

		// Copy the specified region into this one...
		if (CombineRgn(region, rgn.region, 0, RGN_COPY) == NULLREGION)
			Clear();
		return;
	}

	// Otherwise, combine the two
	if (CombineRgn(region, region, rgn.region, RGN_OR) == NULLREGION)
		Clear();
}

void
vncRegion::Intersect(vncRegion &rgn)
{
	if (rgn.region == NULL)
		return;
	if (region == NULL)
		return;

	// Otherwise, intersect the two
	if (CombineRgn(region, region, rgn.region, RGN_AND) == NULLREGION)
		Clear();
}

void
vncRegion::Subtract(vncRegion &rgn)
{
	if (rgn.region == NULL)
		return;
	if (region == NULL)
		return;

	// Otherwise, intersect the two
	if (CombineRgn(region, region, rgn.region, RGN_DIFF) == NULLREGION)
		Clear();
}



// Return all the rectangles
//
// ==========================================================================
// WIN32S: THIS FUNCTION IS WHY NOTHING RENDERS.
//
// The original sized its buffer with the standard two-call idiom:
//
//     buffsize = GetRegionData(region, NULL, 0);       // <-- NULL output pointer
//     buff = (RGNDATA *) new BYTE [buffsize];
//     if (GetRegionData(region, buffsize, buff)) { ...walk the rects... }
//
// A NULL output pointer does not survive the Win32s 32->16 bit thunk.  The thunk
// has to translate a flat pointer into a 16:16 segmented one and it rejects NULL
// rather than passing it through as "no buffer, just tell me the size" - the same
// failure that breaks GetDIBits' format query (see vncDesktop::InitBitmap) and
// that required rewriting VSocket::ReadExact(NULL, n).
//
// The consequences here were completely silent:
//
//   buffsize = 0  ->  new BYTE[0] SUCCEEDS (returns a valid pointer)
//                 ->  the second GetRegionData fails
//                 ->  the for loop never executes
//                 ->  rects is EMPTY and the function returns FALSE
//
// vncClient::SendUpdate then finds numrects == 0 and sends nothing at all.  No
// error, no log line, no update - forever.  The capture path was working
// correctly by this point; every captured region evaporated here.
//
// It also explains the one thing that DID render: the mouse cursor.  The cursor
// rectangles are added to toBeSent directly from m_oldmousepos and the cursor
// shape is sent by its own encoder path, neither of which passes through this
// function.  That is why moving the mouse produced visible marks while the
// desktop never appeared.
//
// THE FIX: do not ask for the size.  Compute it.
//
// A region's data is RGNDATAHEADER followed by nCount RECTs, and GetRgnBox +
// the region's complexity give us no count - so allocate a buffer that is
// generously large, call GetRegionData once, and grow if it reports it needs
// more.  GetRegionData DOES return the required size when the supplied buffer is
// too small (it returns the size rather than filling), so one retry is enough,
// and the common case is a single call with no retry.
//
// The starting size is chosen to cover the region shapes this server actually
// produces: the changed-region tracking works in 32x32 tiles, so even a heavily
// fragmented full-screen update is a few hundred rectangles.  256 rectangles is
// 4 KB and covers essentially everything; the retry handles the rest.
// ==========================================================================
BOOL vncRegion::Rectangles(rectlist &rects)
{
	DWORD x;
	RGNDATA *buff;

	// If the region is empty then return empty rectangle list
	if (region == NULL)
		return FALSE;

	// Start with room for 256 rectangles.
	DWORD rectCapacity = 256;
	DWORD buffsize = sizeof(RGNDATAHEADER) + rectCapacity * sizeof(RECT);

	buff = (RGNDATA *) new BYTE [buffsize];
	if (buff == NULL)
		return FALSE;

	DWORD result = GetRegionData(region, buffsize, buff);

	if (result == 0) {
		// ==============================================================
		// WIN32S: GetRegionData IS NOT AVAILABLE AT ALL.
		//
		// The diagnostics proved change detection works - PollArea reported
		// "33792 tiles checked, 33242 differed" - yet the pump kept seeing
		// "changed_rgn has 0 rects".  Region CREATION works (CreateRectRgn), and
		// region COMBINING works.  What does not work is getting the rectangles
		// back out.
		//
		// GetRegionData is a Windows 3.1 GDI function on paper, but the Win32s
		// implementation cannot return the RGNDATA structure: it is a
		// variable-length buffer of RECTs that the 32->16 bit thunk has to
		// translate, and it fails regardless of how the buffer is sized.  Passing
		// a correctly-sized buffer (this call) fails exactly as passing NULL did.
		//
		// FALLBACK: use GetRgnBox() and report the region's BOUNDING RECTANGLE as
		// a single rectangle.
		//
		// This is a real behavioural compromise and it is worth being explicit
		// about the cost: instead of sending only the tiles that changed, the
		// server sends one rectangle covering all of them.  For scattered changes
		// that means sending more data than necessary - in the worst case a
		// full-screen rectangle when two opposite corners changed.
		//
		// It is nevertheless the right trade here:
		//   * it is CORRECT - the bounding box always contains every changed
		//     pixel, so the client never misses an update;
		//   * it is the only option, since the rectangle list is unobtainable;
		//   * the encoders still compress what they are given, so a mostly-static
		//     screen with one small change still sends a small rectangle.
		//
		// GetRgnBox takes a pointer to a RECT, which is the pattern that has
		// failed elsewhere - so the result is checked and the failure logged.
		// ==============================================================
		delete [] buff;

		RECT box;
		box.left = box.top = box.right = box.bottom = 0;

		int cplx = GetRgnBox(region, &box);
		if (cplx == NULLREGION || cplx == 0) {
			static DWORD s_boxFail = 0;
			if ((s_boxFail++ % 100) == 0) {
				vnclog.Print(LL_INTERR,
					VNCLOG("GetRegionData and GetRgnBox both failed "
						   "(complexity=%d, error=%d) [%d occurrences]\n"),
					cplx, GetLastError(), (int)s_boxFail);
			}
			return FALSE;
		}

		if (IsRectEmpty(&box))
			return FALSE;

		// Report it once so the compromise is visible in the log rather than
		// silent.
		static BOOL s_warnedBox = FALSE;
		if (!s_warnedBox) {
			s_warnedBox = TRUE;
			vnclog.Print(LL_INTERR,
				VNCLOG("GetRegionData unavailable on this platform - "
					   "using region bounding boxes instead (updates will be "
					   "larger than strictly necessary)\n"));
		}

		rects.push_front(box);
		return TRUE;
	}

	if (result > buffsize) {
		// The buffer was too small and GetRegionData told us the size it needs.
		// Allocate exactly that and try once more.
		delete [] buff;

		buffsize = result;
		buff = (RGNDATA *) new BYTE [buffsize];
		if (buff == NULL)
			return FALSE;

		result = GetRegionData(region, buffsize, buff);
		if (result == 0 || result > buffsize) {
			// Same fallback as above: report the bounding box.
			delete [] buff;

			RECT box;
			box.left = box.top = box.right = box.bottom = 0;
			int cplx = GetRgnBox(region, &box);
			if (cplx == NULLREGION || cplx == 0 || IsRectEmpty(&box))
				return FALSE;
			rects.push_front(box);
			return TRUE;
		}
	}

	// Sanity-check the count against the buffer we actually have, so a bogus
	// nCount cannot walk off the end.
	DWORD maxRects = (buffsize - sizeof(RGNDATAHEADER)) / sizeof(RECT);
	DWORD nRects = buff->rdh.nCount;
	if (nRects > maxRects)
		nRects = maxRects;

	for (x = 0; x < nRects; x++)
	{
		// Obtain the rectangles from the list
		RECT *rect = (RECT *) (((BYTE *) buff) + sizeof(RGNDATAHEADER) + x * sizeof(RECT));
		rects.push_front(*rect);
	}

	// Delete the temporary buffer
	delete [] buff;

	// Return whether there are any rects!
	return !rects.empty();
}

// Return rectangles clipped to a certain area
BOOL vncRegion::Rectangles(rectlist &rects, RECT &cliprect)
{
	vncRegion cliprgn;

	// Create the clip-region
	cliprgn.AddRect(cliprect);

	// Calculate the intersection with this region
	cliprgn.Intersect(*this);

	return cliprgn.Rectangles(rects);
}
