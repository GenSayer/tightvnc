// progress.cpp - built-in msctls_progress32
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Messages: PBM_SETRANGE, PBM_SETPOS, PBM_DELTAPOS, PBM_SETSTEP, PBM_STEPIT.
// Drawn as COMCTL32 4.00 does: segments in the highlight colour.

#include "vc2ctli.h"

typedef struct {
	int lo, hi, pos, step;
} PBDATA;

static void PbClamp(PBDATA *pb)
{
	if (pb->pos < pb->lo) pb->pos = pb->lo;
	if (pb->pos > pb->hi) pb->pos = pb->hi;
}

static void PbPaint(HWND hwnd, PBDATA *pb)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rc;
	int w, h, filled, seg, x;
	COLORREF bar = GetSysColor(COLOR_HIGHLIGHT);

	GetClientRect(hwnd, &rc);
	Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
	InflateRect(&rc, -1, -1);
	w = rc.right - rc.left;
	h = rc.bottom - rc.top;
	if (w > 0 && h > 0 && pb->hi > pb->lo) {
		filled = (int)((long)(pb->pos - pb->lo) * w / (pb->hi - pb->lo));
		seg = (h * 2) / 3;
		if (seg < 2)
			seg = 2;
		for (x = 0; x < filled; x += seg + 2) {
			int r = x + seg;
			if (r > filled)
				r = filled;
			Vc2Fill(hdc, rc.left + x, rc.top, rc.left + r, rc.bottom, bar);
		}
	}
	EndPaint(hwnd, &ps);
}

static LRESULT PbSet(HWND hwnd, PBDATA *pb, int newPos)
{
	int old = pb->pos;
	pb->pos = newPos;
	PbClamp(pb);
	if (pb->pos != old)
		InvalidateRect(hwnd, NULL, TRUE);
	return old;
}

static LRESULT CALLBACK PbWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PBDATA *pb = (PBDATA *)Vc2GetData(hwnd);

	if (pb == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		pb = (PBDATA *)Vc2Alloc(sizeof(PBDATA));
		if (pb == NULL)
			return FALSE;
		pb->lo = 0;
		pb->hi = 100;
		pb->step = 10;
		Vc2SetData(hwnd, pb);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_NCDESTROY:
		Vc2Free(pb);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		PbPaint(hwnd, pb);
		return 0;

	case PBM_SETRANGE:
		{
			LRESULT old = MAKELONG((short)pb->lo, (short)pb->hi);
			pb->lo = (int)(short)LOWORD(lParam);
			pb->hi = (int)(short)HIWORD(lParam);
			if (pb->hi < pb->lo)
				pb->hi = pb->lo;
			PbClamp(pb);
			InvalidateRect(hwnd, NULL, TRUE);
			return old;
		}

	case PBM_SETPOS:
		return PbSet(hwnd, pb, (int)(short)LOWORD(wParam));

	case PBM_DELTAPOS:
		return PbSet(hwnd, pb, pb->pos + (int)(short)LOWORD(wParam));

	case PBM_SETSTEP:
		{
			int old = pb->step;
			pb->step = (int)(short)LOWORD(wParam);
			return old;
		}

	case PBM_STEPIT:
		{
			// Wraps back to the start when it passes the end, as COMCTL32 does.
			int np = pb->pos + pb->step;
			if (np > pb->hi)
				np = pb->lo + (np - pb->hi);
			return PbSet(hwnd, pb, np);
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterProgress(void)
{
	return Vc2RegisterClass("msctls_progress32", (WNDPROC)PbWndProc,
							CS_HREDRAW | CS_VREDRAW, NULL, -1);
}
