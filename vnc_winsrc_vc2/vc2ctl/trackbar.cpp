// trackbar.cpp - built-in msctls_trackbar32
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Messages: TBM_GETPOS, TBM_SETPOS, TBM_SETRANGE, TBM_SETRANGEMIN,
// TBM_SETRANGEMAX, TBM_GETRANGEMIN, TBM_GETRANGEMAX.
// Styles: TBS_HORZ / TBS_VERT, TBS_AUTOTICKS, TBS_NOTICKS.
// Sends WM_HSCROLL / WM_VSCROLL (TB_* code, position; lParam = this window)
// to the parent, as the real control does.

#include "vc2ctli.h"

typedef struct {
	int lo, hi, pos;
	BOOL dragging;
	int dragOffset;         // cursor offset from the thumb centre
	BOOL focused;
} TRKDATA;

#define TRK_THUMB_W     10      // across the direction of travel: see below
#define TRK_MARGIN      (TRK_THUMB_W / 2 + 3)

static BOOL TrkVert(HWND hwnd)
{
	return (GetWindowLong(hwnd, GWL_STYLE) & TBS_VERT) ? TRUE : FALSE;
}

// Geometry in "along / across" terms, so one implementation serves both
// orientations.  'len' is the client size along the direction of travel.
typedef struct {
	int len, across;
	int chanStart, chanEnd;     // range of thumb centres
	int thumbLen, thumbAcross;  // thumb size along / across
	int thumbTop;               // across offset of the thumb
	BOOL ticks;
} TRKGEOM;

static void TrkGeom(HWND hwnd, TRKGEOM *g)
{
	RECT rc;
	DWORD style = GetWindowLong(hwnd, GWL_STYLE);
	GetClientRect(hwnd, &rc);
	if (style & TBS_VERT) {
		g->len = rc.bottom;
		g->across = rc.right;
	} else {
		g->len = rc.right;
		g->across = rc.bottom;
	}
	g->ticks = ((style & TBS_NOTICKS) == 0) ? TRUE : FALSE;
	g->thumbLen = TRK_THUMB_W;
	g->thumbAcross = g->across - (g->ticks ? 8 : 4);
	if (g->thumbAcross > 20)
		g->thumbAcross = 20;
	if (g->thumbAcross < 6)
		g->thumbAcross = 6;
	g->thumbTop = 2;
	g->chanStart = TRK_MARGIN;
	g->chanEnd = g->len - TRK_MARGIN;
	if (g->chanEnd <= g->chanStart)
		g->chanEnd = g->chanStart + 1;
}

static int TrkPosToCoord(TRKDATA *td, TRKGEOM *g, int pos)
{
	if (td->hi <= td->lo)
		return g->chanStart;
	return g->chanStart + (int)((long)(pos - td->lo) * (g->chanEnd - g->chanStart) / (td->hi - td->lo));
}

static int TrkCoordToPos(TRKDATA *td, TRKGEOM *g, int c)
{
	int span = g->chanEnd - g->chanStart;
	int p;
	if (td->hi <= td->lo || span <= 0)
		return td->lo;
	p = td->lo + (int)(((long)(c - g->chanStart) * (td->hi - td->lo) + span / 2) / span);
	if (p < td->lo) p = td->lo;
	if (p > td->hi) p = td->hi;
	return p;
}

// Build a client rectangle from along / across coordinates.
static void TrkRect(HWND hwnd, RECT *prc, int a0, int c0, int a1, int c1)
{
	if (TrkVert(hwnd)) {
		prc->left = c0; prc->right = c1; prc->top = a0; prc->bottom = a1;
	} else {
		prc->left = a0; prc->right = a1; prc->top = c0; prc->bottom = c1;
	}
}

static void TrkThumbRect(HWND hwnd, TRKDATA *td, RECT *prc)
{
	TRKGEOM g;
	int c;
	TrkGeom(hwnd, &g);
	c = TrkPosToCoord(td, &g, td->pos);
	TrkRect(hwnd, prc, c - g.thumbLen / 2, g.thumbTop,
			c - g.thumbLen / 2 + g.thumbLen, g.thumbTop + g.thumbAcross);
}

static void TrkClamp(TRKDATA *td)
{
	if (td->hi < td->lo)
		td->hi = td->lo;
	if (td->pos < td->lo) td->pos = td->lo;
	if (td->pos > td->hi) td->pos = td->hi;
}

static void TrkNotify(HWND hwnd, TRKDATA *td, int code)
{
	HWND hParent = GetParent(hwnd);
	if (hParent != NULL)
		SendMessage(hParent, TrkVert(hwnd) ? WM_VSCROLL : WM_HSCROLL,
					MAKEWPARAM(code, td->pos), (LPARAM)hwnd);
}

static void TrkPaint(HWND hwnd, TRKDATA *td)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rc;
	TRKGEOM g;
	int chanC, i, n;

	GetClientRect(hwnd, &rc);
	Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
	TrkGeom(hwnd, &g);

	// Channel: a 4-pixel sunken groove through the middle of the thumb.
	chanC = g.thumbTop + g.thumbAcross / 2;
	TrkRect(hwnd, &rc, g.chanStart - g.thumbLen / 2, chanC - 2,
			g.chanEnd + g.thumbLen / 2, chanC + 2);
	Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
	InflateRect(&rc, -1, -1);
	Vc2FillRect(hdc, &rc, GetSysColor(COLOR_WINDOW));

	// Ticks below (or right of) the thumb, one per value while they fit.
	if (g.ticks && td->hi > td->lo) {
		n = td->hi - td->lo;
		if (n <= (g.chanEnd - g.chanStart) / 2 &&
			(GetWindowLong(hwnd, GWL_STYLE) & TBS_AUTOTICKS)) {
			for (i = 0; i <= n; i++) {
				int c = TrkPosToCoord(td, &g, td->lo + i);
				TrkRect(hwnd, &rc, c, g.thumbTop + g.thumbAcross + 1,
						c + 1, g.thumbTop + g.thumbAcross + 4);
				Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNTEXT));
			}
		} else {
			// Ends only.
			int ends[2];
			ends[0] = g.chanStart;
			ends[1] = g.chanEnd;
			for (i = 0; i < 2; i++) {
				TrkRect(hwnd, &rc, ends[i], g.thumbTop + g.thumbAcross + 1,
						ends[i] + 1, g.thumbTop + g.thumbAcross + 4);
				Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNTEXT));
			}
		}
	}

	// Thumb.
	TrkThumbRect(hwnd, td, &rc);
	Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
	Vc2DrawRaised(hdc, &rc);
	if (!IsWindowEnabled(hwnd)) {
		InflateRect(&rc, -3, -3);
		Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNSHADOW));
	}

	if (td->focused) {
		GetClientRect(hwnd, &rc);
		Vc2DrawFocus(hdc, &rc);
	}
	EndPaint(hwnd, &ps);
}

static void TrkMove(HWND hwnd, TRKDATA *td, int newPos, int code)
{
	int old = td->pos;
	td->pos = newPos;
	TrkClamp(td);
	if (td->pos != old)
		InvalidateRect(hwnd, NULL, TRUE);
	TrkNotify(hwnd, td, code);
}

static LRESULT CALLBACK TrkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TRKDATA *td = (TRKDATA *)Vc2GetData(hwnd);

	if (td == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		td = (TRKDATA *)Vc2Alloc(sizeof(TRKDATA));
		if (td == NULL)
			return FALSE;
		td->lo = 0;
		td->hi = 100;
		Vc2SetData(hwnd, td);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_NCDESTROY:
		Vc2Free(td);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		TrkPaint(hwnd, td);
		return 0;

	case WM_ENABLE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS;

	case WM_SETFOCUS:
	case WM_KILLFOCUS:
		td->focused = (msg == WM_SETFOCUS);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			RECT rt;
			POINT pt;
			TRKGEOM g;
			int along, thumbC, page;
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);
			SetFocus(hwnd);
			TrkGeom(hwnd, &g);
			TrkThumbRect(hwnd, td, &rt);
			along = TrkVert(hwnd) ? pt.y : pt.x;
			thumbC = TrkPosToCoord(td, &g, td->pos);
			if (PtInRect(&rt, pt)) {
				td->dragging = TRUE;
				td->dragOffset = along - thumbC;
				SetCapture(hwnd);
			} else {
				page = (td->hi - td->lo) / 5;
				if (page < 1)
					page = 1;
				if (along < thumbC)
					TrkMove(hwnd, td, td->pos - page, TB_PAGEUP);
				else
					TrkMove(hwnd, td, td->pos + page, TB_PAGEDOWN);
				TrkNotify(hwnd, td, TB_ENDTRACK);
			}
		}
		return 0;

	case WM_MOUSEMOVE:
		if (td->dragging) {
			TRKGEOM g;
			int along = TrkVert(hwnd) ? (short)HIWORD(lParam) : (short)LOWORD(lParam);
			int p;
			TrkGeom(hwnd, &g);
			p = TrkCoordToPos(td, &g, along - td->dragOffset);
			if (p != td->pos)
				TrkMove(hwnd, td, p, TB_THUMBTRACK);
		}
		return 0;

	case WM_LBUTTONUP:
		if (td->dragging) {
			td->dragging = FALSE;
			ReleaseCapture();
			TrkNotify(hwnd, td, TB_THUMBPOSITION);
			TrkNotify(hwnd, td, TB_ENDTRACK);
		}
		return 0;

	case WM_CAPTURECHANGED:
		td->dragging = FALSE;
		return 0;

	case WM_KEYDOWN:
		{
			int page = (td->hi - td->lo) / 5;
			if (page < 1)
				page = 1;
			switch (wParam) {
			case VK_LEFT:
			case VK_UP:
				TrkMove(hwnd, td, td->pos - 1, TB_LINEUP);
				break;
			case VK_RIGHT:
			case VK_DOWN:
				TrkMove(hwnd, td, td->pos + 1, TB_LINEDOWN);
				break;
			case VK_PRIOR:
				TrkMove(hwnd, td, td->pos - page, TB_PAGEUP);
				break;
			case VK_NEXT:
				TrkMove(hwnd, td, td->pos + page, TB_PAGEDOWN);
				break;
			case VK_HOME:
				TrkMove(hwnd, td, td->lo, TB_TOP);
				break;
			case VK_END:
				TrkMove(hwnd, td, td->hi, TB_BOTTOM);
				break;
			default:
				return 0;
			}
		}
		return 0;

	case WM_KEYUP:
		switch (wParam) {
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_PRIOR: case VK_NEXT: case VK_HOME: case VK_END:
			TrkNotify(hwnd, td, TB_ENDTRACK);
			break;
		}
		return 0;

	case TBM_GETPOS:
		return td->pos;

	case TBM_GETRANGEMIN:
		return td->lo;

	case TBM_GETRANGEMAX:
		return td->hi;

	case TBM_SETPOS:
		td->pos = (int)lParam;
		TrkClamp(td);
		if (wParam)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case TBM_SETRANGE:
		td->lo = (int)(short)LOWORD(lParam);
		td->hi = (int)(short)HIWORD(lParam);
		TrkClamp(td);
		if (wParam)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case TBM_SETRANGEMIN:
		td->lo = (int)lParam;
		TrkClamp(td);
		if (wParam)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case TBM_SETRANGEMAX:
		td->hi = (int)lParam;
		TrkClamp(td);
		if (wParam)
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterTrackbar(void)
{
	return Vc2RegisterClass("msctls_trackbar32", (WNDPROC)TrkWndProc,
							CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, NULL, -1);
}
