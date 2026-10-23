// toolbar.cpp - built-in toolbar (CreateToolbarEx replacement)
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Created only through Vc2CtlCreateToolbarEx, under a private class name.
// Button states: TBSTATE_ENABLED, CHECKED, PRESSED, HIDDEN, INDETERMINATE
// (drawn disabled).  Styles: TBSTYLE_BUTTON, SEP, CHECK, CHECKGROUP.
// Messages: TB_ENABLEBUTTON, TB_CHECKBUTTON, TB_PRESSBUTTON, TB_HIDEBUTTON,
// TB_ISBUTTON*, TB_SETSTATE, TB_GETSTATE, TB_BUTTONCOUNT,
// TB_BUTTONSTRUCTSIZE, TB_AUTOSIZE, TB_SETINDENT.
// A click sends WM_COMMAND (id, BN_CLICKED) to the parent.  With
// TBSTYLE_TOOLTIPS the parent is asked for tooltip text with WM_NOTIFY /
// TTN_NEEDTEXT (TOOLTIPTEXT: lpszText, szText, or hinst + string id).
//
// The height is fixed, as with COMCTL32 (unless CCS_NORESIZE): the viewer
// sizes it to (height - 3) on every WM_SIZE and relies on the toolbar
// restoring its own height.

#include "vc2ctli.h"

typedef struct {
	int iBitmap;
	int idCommand;
	BYTE fsState;
	BYTE fsStyle;
	RECT rc;                // laid out position; empty when hidden
} TBBTN;

typedef struct {
	TBBTN *btns;
	int n;
	HBITMAP bmp;
	int bmW, bmH;           // one image
	int btnW, btnH;
	int indent;
	int height;
	int pressed;            // button held down with the mouse, -1 for none
	BOOL inside;
	int hot;                // button under the cursor (tooltips), -1 for none
	HWND tip;
	DWORD tipHiddenAt;      // tick count when a tip was last hidden
} TBDATA;

#define TB_SEP_WIDTH    8
#define TB_TIMER_TIP    1
#define TB_TIMER_POLL   2
#define TB_TIP_DELAY    500
#define TB_TIP_QUICK    50      // moving from one button to the next
#define TB_POLL_DELAY   100

// ---------------------------------------------------------------------------
// Bitmap with the standard colours mapped to the button colours
// (black, dark grey, light grey, white -> text, shadow, face, highlight),
// which is what CreateMappedBitmap does for CreateToolbarEx.
// ---------------------------------------------------------------------------

static COLORREF TbMap(BYTE r, BYTE g, BYTE b, BOOL *mapped)
{
	*mapped = TRUE;
	if (r == 0 && g == 0 && b == 0)         return GetSysColor(COLOR_BTNTEXT);
	if (r == 128 && g == 128 && b == 128)   return GetSysColor(COLOR_BTNSHADOW);
	if (r == 192 && g == 192 && b == 192)   return GetSysColor(COLOR_BTNFACE);
	if (r == 255 && g == 255 && b == 255)   return GetSysColor(COLOR_BTNHIGHLIGHT);
	*mapped = FALSE;
	return RGB(r, g, b);
}

static HBITMAP TbLoadMappedBitmap(HINSTANCE hInst, UINT id)
{
	HRSRC hr;
	HGLOBAL hg;
	BITMAPINFOHEADER *src;
	BITMAPINFO *bi;
	RGBQUAD *q;
	int nColors, i;
	DWORD hdrSize;
	HDC hdc;
	HBITMAP hbm = NULL;

	hr = FindResource(hInst, MAKEINTRESOURCE(id), RT_BITMAP);
	hg = (hr != NULL) ? LoadResource(hInst, hr) : NULL;
	src = (hg != NULL) ? (BITMAPINFOHEADER *)LockResource(hg) : NULL;
	if (src == NULL)
		return NULL;
	if (src->biSize != sizeof(BITMAPINFOHEADER) || src->biBitCount > 8 ||
		src->biCompression != BI_RGB)
		return LoadBitmap(hInst, MAKEINTRESOURCE(id));   // unmapped, but usable

	nColors = src->biClrUsed ? (int)src->biClrUsed : (1 << src->biBitCount);
	hdrSize = src->biSize + nColors * sizeof(RGBQUAD);
	bi = (BITMAPINFO *)Vc2Alloc(hdrSize);
	if (bi == NULL)
		return LoadBitmap(hInst, MAKEINTRESOURCE(id));
	memcpy(bi, src, hdrSize);
	q = (RGBQUAD *)((BYTE *)bi + bi->bmiHeader.biSize);
	for (i = 0; i < nColors; i++) {
		BOOL mapped;
		COLORREF c = TbMap(q[i].rgbRed, q[i].rgbGreen, q[i].rgbBlue, &mapped);
		if (mapped) {
			q[i].rgbRed = GetRValue(c);
			q[i].rgbGreen = GetGValue(c);
			q[i].rgbBlue = GetBValue(c);
		}
	}
	hdc = GetDC(NULL);
	if (hdc != NULL) {
		hbm = CreateDIBitmap(hdc, &bi->bmiHeader, CBM_INIT,
							 (BYTE *)src + hdrSize, bi, DIB_RGB_COLORS);
		ReleaseDC(NULL, hdc);
	}
	Vc2Free(bi);
	return hbm;
}

// ---------------------------------------------------------------------------
// Layout and drawing
// ---------------------------------------------------------------------------

static void TbLayout(HWND hwnd, TBDATA *td)
{
	int i, x = td->indent;
	for (i = 0; i < td->n; i++) {
		TBBTN *b = &td->btns[i];
		if (b->fsState & TBSTATE_HIDDEN) {
			SetRectEmpty(&b->rc);
			continue;
		}
		b->rc.left = x;
		b->rc.top = 2;
		b->rc.bottom = 2 + td->btnH;
		if (b->fsStyle & TBSTYLE_SEP)
			x += (b->iBitmap > 0) ? b->iBitmap : TB_SEP_WIDTH;
		else
			x += td->btnW;
		b->rc.right = x;
	}
	InvalidateRect(hwnd, NULL, TRUE);
}

static int TbHit(TBDATA *td, int x, int y)
{
	POINT pt;
	int i;
	pt.x = x; pt.y = y;
	for (i = 0; i < td->n; i++) {
		TBBTN *b = &td->btns[i];
		if (!(b->fsStyle & TBSTYLE_SEP) && !(b->fsState & TBSTATE_HIDDEN) &&
			PtInRect(&b->rc, pt))
			return i;
	}
	return -1;
}

static int TbIndex(TBDATA *td, int id)
{
	int i;
	for (i = 0; i < td->n; i++)
		if (!(td->btns[i].fsStyle & TBSTYLE_SEP) && td->btns[i].idCommand == id)
			return i;
	return -1;
}

static BOOL TbEnabled(TBBTN *b)
{
	return ((b->fsState & TBSTATE_ENABLED) && !(b->fsState & TBSTATE_INDETERMINATE)) ? TRUE : FALSE;
}

// Disabled image: the glyph's outline embossed in highlight and shadow,
// drawn through a monochrome mask (no DrawState before NT 4).
static void TbDrawDisabled(HDC hdc, int x, int y, HDC hdcImg, int sx, int w, int h)
{
	HDC hdcMono = CreateCompatibleDC(hdc);
	HBITMAP hbmMono = CreateBitmap(w, h, 1, 1, NULL);
	HBITMAP hOldMono;
	HBRUSH hbr, hOldBr;
	COLORREF oldText, oldBk;
	if (hdcMono == NULL || hbmMono == NULL) {
		if (hbmMono != NULL) DeleteObject(hbmMono);
		if (hdcMono != NULL) DeleteDC(hdcMono);
		return;
	}
	hOldMono = (HBITMAP)SelectObject(hdcMono, hbmMono);

	// Face and highlight pixels -> 1 (white), everything else -> 0.
	oldBk = SetBkColor(hdcImg, GetSysColor(COLOR_BTNFACE));
	BitBlt(hdcMono, 0, 0, w, h, hdcImg, sx, 0, SRCCOPY);
	SetBkColor(hdcImg, GetSysColor(COLOR_BTNHIGHLIGHT));
	BitBlt(hdcMono, 0, 0, w, h, hdcImg, sx, 0, SRCPAINT);
	SetBkColor(hdcImg, oldBk);

	// Paint the brush where the mask is 0: ROP PSDPxax.
	oldText = SetTextColor(hdc, RGB(0, 0, 0));
	oldBk = SetBkColor(hdc, RGB(255, 255, 255));
	hbr = CreateSolidBrush(GetSysColor(COLOR_BTNHIGHLIGHT));
	hOldBr = (HBRUSH)SelectObject(hdc, hbr);
	BitBlt(hdc, x + 1, y + 1, w, h, hdcMono, 0, 0, 0x00B8074AL);
	SelectObject(hdc, hOldBr);
	DeleteObject(hbr);
	hbr = CreateSolidBrush(GetSysColor(COLOR_BTNSHADOW));
	hOldBr = (HBRUSH)SelectObject(hdc, hbr);
	BitBlt(hdc, x, y, w, h, hdcMono, 0, 0, 0x00B8074AL);
	SelectObject(hdc, hOldBr);
	DeleteObject(hbr);
	SetTextColor(hdc, oldText);
	SetBkColor(hdc, oldBk);

	SelectObject(hdcMono, hOldMono);
	DeleteObject(hbmMono);
	DeleteDC(hdcMono);
}

// 50% dither of face and highlight, for checked buttons.
static HBRUSH TbDitherBrush(void)
{
	static const WORD pat[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
	HBITMAP hbm = CreateBitmap(8, 8, 1, 1, pat);
	HBRUSH hbr = NULL;
	if (hbm != NULL) {
		hbr = CreatePatternBrush(hbm);
		DeleteObject(hbm);
	}
	return hbr;
}

static void TbPaint(HWND hwnd, TBDATA *td)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	HDC hdcImg = NULL;
	HBITMAP hOldImg = NULL;
	RECT rc;
	int i;
	COLORREF fr = GetSysColor(COLOR_WINDOWFRAME);
	COLORREF hi = GetSysColor(COLOR_BTNHIGHLIGHT);
	COLORREF sh = GetSysColor(COLOR_BTNSHADOW);

	GetClientRect(hwnd, &rc);
	Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
	Vc2Fill(hdc, rc.left, 0, rc.right, 1, sh);
	Vc2Fill(hdc, rc.left, 1, rc.right, 2, hi);

	if (td->bmp != NULL) {
		hdcImg = CreateCompatibleDC(hdc);
		if (hdcImg != NULL)
			hOldImg = (HBITMAP)SelectObject(hdcImg, td->bmp);
	}

	for (i = 0; i < td->n; i++) {
		TBBTN *b = &td->btns[i];
		RECT r = b->rc;
		BOOL down, checked, enabled;
		int ix, iy;
		if ((b->fsStyle & TBSTYLE_SEP) || (b->fsState & TBSTATE_HIDDEN))
			continue;
		enabled = TbEnabled(b);
		checked = (b->fsState & TBSTATE_CHECKED) ? TRUE : FALSE;
		down = (b->fsState & TBSTATE_PRESSED) || (i == td->pressed && td->inside);

		// Black frame with the corners left out, then the bevel.
		Vc2Fill(hdc, r.left + 1, r.top, r.right - 1, r.top + 1, fr);
		Vc2Fill(hdc, r.left + 1, r.bottom - 1, r.right - 1, r.bottom, fr);
		Vc2Fill(hdc, r.left, r.top + 1, r.left + 1, r.bottom - 1, fr);
		Vc2Fill(hdc, r.right - 1, r.top + 1, r.right, r.bottom - 1, fr);
		InflateRect(&r, -1, -1);
		if (down || checked) {
			if (checked && !down) {
				HBRUSH hbr = TbDitherBrush();
				if (hbr != NULL) {
					COLORREF oldT = SetTextColor(hdc, GetSysColor(COLOR_BTNFACE));
					COLORREF oldB = SetBkColor(hdc, hi);
					FillRect(hdc, &r, hbr);
					SetTextColor(hdc, oldT);
					SetBkColor(hdc, oldB);
					DeleteObject(hbr);
				}
			}
			Vc2Fill(hdc, r.left, r.top, r.right, r.top + 1, sh);
			Vc2Fill(hdc, r.left, r.top, r.left + 1, r.bottom, sh);
		} else {
			Vc2Fill(hdc, r.left, r.top, r.right - 1, r.top + 1, hi);
			Vc2Fill(hdc, r.left, r.top, r.left + 1, r.bottom - 1, hi);
			Vc2Fill(hdc, r.left, r.bottom - 1, r.right, r.bottom, sh);
			Vc2Fill(hdc, r.right - 1, r.top, r.right, r.bottom, sh);
			Vc2Fill(hdc, r.left + 1, r.bottom - 2, r.right - 1, r.bottom - 1, sh);
			Vc2Fill(hdc, r.right - 2, r.top + 1, r.right - 1, r.bottom - 1, sh);
		}

		if (hdcImg == NULL || b->iBitmap < 0)
			continue;
		ix = b->rc.left + (td->btnW - td->bmW) / 2 + ((down || checked) ? 1 : 0);
		iy = b->rc.top + (td->btnH - td->bmH) / 2 + ((down || checked) ? 1 : 0);
		if (enabled)
			BitBlt(hdc, ix, iy, td->bmW, td->bmH, hdcImg, b->iBitmap * td->bmW, 0, SRCCOPY);
		else
			TbDrawDisabled(hdc, ix, iy, hdcImg, b->iBitmap * td->bmW, td->bmW, td->bmH);
	}

	if (hdcImg != NULL) {
		SelectObject(hdcImg, hOldImg);
		DeleteDC(hdcImg);
	}
	EndPaint(hwnd, &ps);
}

static void TbInvalidateButton(HWND hwnd, TBDATA *td, int i)
{
	if (i >= 0 && i < td->n)
		InvalidateRect(hwnd, &td->btns[i].rc, TRUE);
}

// ---------------------------------------------------------------------------
// Tooltips
// ---------------------------------------------------------------------------

static void TbHideTip(HWND hwnd, TBDATA *td)
{
	KillTimer(hwnd, TB_TIMER_TIP);
	if (td->tip != NULL && IsWindowVisible(td->tip)) {
		Vc2TipHide(td->tip);
		td->tipHiddenAt = GetTickCount();
	}
}

static void TbShowTip(HWND hwnd, TBDATA *td)
{
	TOOLTIPTEXT tt;
	char buf[256];
	const char *text;
	POINT pt;
	HWND hParent = GetParent(hwnd);
	int id;

	if (td->tip == NULL || td->hot < 0 || td->hot >= td->n || hParent == NULL)
		return;
	id = td->btns[td->hot].idCommand;
	memset(&tt, 0, sizeof(tt));
	tt.hdr.hwndFrom = td->tip;
	tt.hdr.idFrom = (UINT)id;
	tt.hdr.code = TTN_NEEDTEXT;
	tt.lpszText = tt.szText;
	SendMessage(hParent, WM_NOTIFY, (WPARAM)id, (LPARAM)&tt);

	if (tt.hinst != NULL && tt.lpszText != NULL && HIWORD((DWORD)tt.lpszText) == 0) {
		buf[0] = '\0';
		LoadString(tt.hinst, LOWORD((DWORD)tt.lpszText), buf, sizeof(buf));
		text = buf;
	} else if (tt.lpszText != NULL && tt.lpszText != LPSTR_TEXTCALLBACK) {
		text = tt.lpszText;
	} else {
		tt.szText[sizeof(tt.szText) - 1] = '\0';
		text = tt.szText;
	}
	GetCursorPos(&pt);
	Vc2TipShow(td->tip, text, pt.x, pt.y + 20);
}

static void TbSetHot(HWND hwnd, TBDATA *td, int hot)
{
	if (hot == td->hot)
		return;
	TbHideTip(hwnd, td);
	td->hot = hot;
	if (hot >= 0 && td->tip != NULL) {
		DWORD since = GetTickCount() - td->tipHiddenAt;
		SetTimer(hwnd, TB_TIMER_TIP, (since < 500) ? TB_TIP_QUICK : TB_TIP_DELAY, NULL);
		// No TrackMouseEvent on NT 3.x: poll for the cursor leaving.
		SetTimer(hwnd, TB_TIMER_POLL, TB_POLL_DELAY, NULL);
	}
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static void TbClick(HWND hwnd, TBDATA *td, int i)
{
	TBBTN *b = &td->btns[i];
	HWND hParent = GetParent(hwnd);
	int id = b->idCommand;
	if ((b->fsStyle & TBSTYLE_CHECKGROUP) == TBSTYLE_CHECKGROUP) {
		int j;
		// The group is the run of CHECKGROUP buttons around this one.
		for (j = i - 1; j >= 0 && (td->btns[j].fsStyle & TBSTYLE_CHECKGROUP) == TBSTYLE_CHECKGROUP; j--)
			td->btns[j].fsState &= ~TBSTATE_CHECKED;
		for (j = i + 1; j < td->n && (td->btns[j].fsStyle & TBSTYLE_CHECKGROUP) == TBSTYLE_CHECKGROUP; j++)
			td->btns[j].fsState &= ~TBSTATE_CHECKED;
		b->fsState |= TBSTATE_CHECKED;
		InvalidateRect(hwnd, NULL, TRUE);
	} else if (b->fsStyle & TBSTYLE_CHECK) {
		b->fsState ^= TBSTATE_CHECKED;
		TbInvalidateButton(hwnd, td, i);
	}
	if (hParent != NULL)
		SendMessage(hParent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)hwnd);
}

static LRESULT TbSetStateBits(HWND hwnd, TBDATA *td, int id, BYTE bits, BOOL on)
{
	int i = TbIndex(td, id);
	BYTE old;
	if (i < 0)
		return FALSE;
	old = td->btns[i].fsState;
	if (on)
		td->btns[i].fsState |= bits;
	else
		td->btns[i].fsState &= ~bits;
	if ((old ^ td->btns[i].fsState) & TBSTATE_HIDDEN)
		TbLayout(hwnd, td);
	else
		TbInvalidateButton(hwnd, td, i);
	return TRUE;
}

static LRESULT CALLBACK TbWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TBDATA *td = (TBDATA *)Vc2GetData(hwnd);

	if (td == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		td = (TBDATA *)Vc2Alloc(sizeof(TBDATA));
		if (td == NULL)
			return FALSE;
		td->pressed = -1;
		td->hot = -1;
		td->bmW = 16;
		td->bmH = 15;
		td->btnW = 24;
		td->btnH = 22;
		td->height = td->btnH + 6;
		Vc2SetData(hwnd, td);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		if (GetWindowLong(hwnd, GWL_STYLE) & TBSTYLE_TOOLTIPS) {
			HWND owner = GetParent(hwnd);
			while (owner != NULL && (GetWindowLong(owner, GWL_STYLE) & WS_CHILD))
				owner = GetParent(owner);
			td->tip = Vc2TipCreate(owner);
		}
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, TB_TIMER_TIP);
		KillTimer(hwnd, TB_TIMER_POLL);
		if (td->tip != NULL) {
			DestroyWindow(td->tip);
			td->tip = NULL;
		}
		return 0;

	case WM_NCDESTROY:
		if (td->bmp != NULL)
			DeleteObject(td->bmp);
		Vc2Free(td->btns);
		Vc2Free(td);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS *wp = (WINDOWPOS *)lParam;
			if (!(GetWindowLong(hwnd, GWL_STYLE) & CCS_NORESIZE) &&
				!(wp->flags & SWP_NOSIZE))
				wp->cy = td->height;
		}
		break;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		TbPaint(hwnd, td);
		return 0;

	case WM_SYSCOLORCHANGE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		{
			int i = TbHit(td, (short)LOWORD(lParam), (short)HIWORD(lParam));
			TbHideTip(hwnd, td);
			if (i < 0 || !TbEnabled(&td->btns[i]))
				return 0;
			td->pressed = i;
			td->inside = TRUE;
			SetCapture(hwnd);
			TbInvalidateButton(hwnd, td, i);
			UpdateWindow(hwnd);
		}
		return 0;

	case WM_MOUSEMOVE:
		{
			int i = TbHit(td, (short)LOWORD(lParam), (short)HIWORD(lParam));
			if (td->pressed >= 0) {
				BOOL in = (i == td->pressed);
				if (in != td->inside) {
					td->inside = in;
					TbInvalidateButton(hwnd, td, td->pressed);
				}
			} else {
				TbSetHot(hwnd, td, i);
			}
		}
		return 0;

	case WM_LBUTTONUP:
		if (td->pressed >= 0) {
			int i = td->pressed;
			BOOL in = td->inside;
			td->pressed = -1;
			td->inside = FALSE;
			ReleaseCapture();
			TbInvalidateButton(hwnd, td, i);
			if (in && i < td->n && TbEnabled(&td->btns[i]))
				TbClick(hwnd, td, i);
		}
		return 0;

	case WM_CAPTURECHANGED:
		if (td->pressed >= 0) {
			TbInvalidateButton(hwnd, td, td->pressed);
			td->pressed = -1;
		}
		return 0;

	case WM_TIMER:
		if (wParam == TB_TIMER_TIP) {
			KillTimer(hwnd, TB_TIMER_TIP);
			if (td->pressed < 0)
				TbShowTip(hwnd, td);
		} else if (wParam == TB_TIMER_POLL) {
			POINT pt;
			GetCursorPos(&pt);
			if (WindowFromPoint(pt) != hwnd || td->pressed >= 0) {
				KillTimer(hwnd, TB_TIMER_POLL);
				TbHideTip(hwnd, td);
				td->hot = -1;
			}
		}
		return 0;

	case TB_ENABLEBUTTON:
		return TbSetStateBits(hwnd, td, (int)wParam, TBSTATE_ENABLED, LOWORD(lParam) != 0);

	case TB_CHECKBUTTON:
		return TbSetStateBits(hwnd, td, (int)wParam, TBSTATE_CHECKED, LOWORD(lParam) != 0);

	case TB_PRESSBUTTON:
		return TbSetStateBits(hwnd, td, (int)wParam, TBSTATE_PRESSED, LOWORD(lParam) != 0);

	case TB_HIDEBUTTON:
		return TbSetStateBits(hwnd, td, (int)wParam, TBSTATE_HIDDEN, LOWORD(lParam) != 0);

	case TB_ISBUTTONENABLED:
	case TB_ISBUTTONCHECKED:
	case TB_ISBUTTONPRESSED:
	case TB_ISBUTTONHIDDEN:
		{
			int i = TbIndex(td, (int)wParam);
			BYTE bit = (msg == TB_ISBUTTONENABLED) ? TBSTATE_ENABLED :
					   (msg == TB_ISBUTTONCHECKED) ? TBSTATE_CHECKED :
					   (msg == TB_ISBUTTONPRESSED) ? TBSTATE_PRESSED : TBSTATE_HIDDEN;
			if (i < 0)
				return 0;
			return (td->btns[i].fsState & bit) ? TRUE : FALSE;
		}

	case TB_SETSTATE:
		{
			int i = TbIndex(td, (int)wParam);
			BYTE old;
			if (i < 0)
				return FALSE;
			old = td->btns[i].fsState;
			td->btns[i].fsState = (BYTE)LOWORD(lParam);
			if (!TbEnabled(&td->btns[i]) && td->pressed == i) {
				td->pressed = -1;
				ReleaseCapture();
			}
			if ((old ^ td->btns[i].fsState) & TBSTATE_HIDDEN)
				TbLayout(hwnd, td);
			else
				TbInvalidateButton(hwnd, td, i);
			return TRUE;
		}

	case TB_GETSTATE:
		{
			int i = TbIndex(td, (int)wParam);
			return (i < 0) ? -1 : td->btns[i].fsState;
		}

	case TB_BUTTONCOUNT:
		return td->n;

	case TB_BUTTONSTRUCTSIZE:
		return 0;

	case TB_SETINDENT:
		td->indent = (int)wParam;
		TbLayout(hwnd, td);
		return TRUE;

	case TB_AUTOSIZE:
		{
			HWND hParent = GetParent(hwnd);
			RECT rc;
			if (hParent != NULL && !(GetWindowLong(hwnd, GWL_STYLE) & CCS_NORESIZE)) {
				GetClientRect(hParent, &rc);
				SetWindowPos(hwnd, NULL, 0, 0, rc.right, td->height,
							 SWP_NOZORDER | SWP_NOACTIVATE);
			}
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterToolbar(void)
{
	if (!Vc2RegisterToolTip())
		return FALSE;
	return Vc2RegisterClass(VC2_TOOLBAR_CLASS, (WNDPROC)TbWndProc,
							CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, NULL, -1);
}

// ---------------------------------------------------------------------------
// CreateToolbarEx
// ---------------------------------------------------------------------------

HWND Vc2CtlCreateToolbarEx(HWND hwnd, DWORD ws, UINT wID, int nBitmaps,
                           HINSTANCE hBMInst, UINT wBMID,
                           const void *lpButtons, int iNumButtons,
                           int dxButton, int dyButton,
                           int dxBitmap, int dyBitmap, UINT uStructSize)
{
	HWND hTb;
	TBDATA *td;
	RECT rcParent;
	BITMAP bm;
	int i;
	const BYTE *p;

	if (!Vc2CtlIsBuiltin(VC2CTL_TOOLBAR) && !Vc2CtlInit(VC2CTL_TOOLBAR))
		return NULL;
	// The fields used (iBitmap, idCommand, fsState, fsStyle) are at the same
	// offsets in the Windows 95 (20-byte) and NT 3.5 (24-byte) layouts.
	if (uStructSize < 10 || (lpButtons == NULL && iNumButtons > 0))
		return NULL;

	GetClientRect(hwnd, &rcParent);
	hTb = CreateWindowEx(0, VC2_TOOLBAR_CLASS, NULL, ws | WS_CHILD,
						 0, 0, rcParent.right, 28, hwnd, (HMENU)wID,
						 g_vc2ctlInst, NULL);
	if (hTb == NULL)
		return NULL;
	td = (TBDATA *)Vc2GetData(hTb);
	if (td == NULL) {
		DestroyWindow(hTb);
		return NULL;
	}

	if (hBMInst != NULL && wBMID != 0)
		td->bmp = TbLoadMappedBitmap(hBMInst, wBMID);
	if (td->bmp != NULL && GetObject(td->bmp, sizeof(bm), &bm)) {
		td->bmW = (dxBitmap > 0) ? dxBitmap :
				  (nBitmaps > 0 ? bm.bmWidth / nBitmaps : 16);
		td->bmH = (dyBitmap > 0) ? dyBitmap : bm.bmHeight;
	} else {
		td->bmW = (dxBitmap > 0) ? dxBitmap : 16;
		td->bmH = (dyBitmap > 0) ? dyBitmap : 15;
	}
	td->btnW = (dxButton > 0) ? dxButton : td->bmW + 8;
	td->btnH = (dyButton > 0) ? dyButton : td->bmH + 7;
	td->height = td->btnH + 6;

	if (iNumButtons > 0) {
		td->btns = (TBBTN *)Vc2Alloc(iNumButtons * sizeof(TBBTN));
		if (td->btns == NULL) {
			DestroyWindow(hTb);
			return NULL;
		}
		p = (const BYTE *)lpButtons;
		for (i = 0; i < iNumButtons; i++, p += uStructSize) {
			td->btns[i].iBitmap = *(const int *)(p + 0);
			td->btns[i].idCommand = *(const int *)(p + sizeof(int));
			td->btns[i].fsState = p[2 * sizeof(int)];
			td->btns[i].fsStyle = p[2 * sizeof(int) + 1];
		}
		td->n = iNumButtons;
	}
	TbLayout(hTb, td);
	SetWindowPos(hTb, NULL, 0, 0, rcParent.right, td->height,
				 SWP_NOZORDER | SWP_NOACTIVATE);
	return hTb;
}
