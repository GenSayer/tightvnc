// ctldraw.cpp - helpers shared by the vc2ctl controls
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.

#include "vc2ctli.h"

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

void *Vc2Alloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (p != NULL)
		memset(p, 0, n ? n : 1);
	return p;
}

void *Vc2Realloc(void *p, size_t n)
{
	return realloc(p, n ? n : 1);
}

void Vc2Free(void *p)
{
	if (p != NULL)
		free(p);
}

char *Vc2StrDup(const char *s)
{
	char *d;
	if (s == NULL)
		return NULL;
	d = (char *)malloc(strlen(s) + 1);
	if (d != NULL)
		strcpy(d, s);
	return d;
}

void Vc2CopyText(char *dst, int cchDst, const char *src)
{
	if (dst == NULL || cchDst <= 0)
		return;
	if (src == NULL)
		src = "";
	strncpy(dst, src, cchDst - 1);
	dst[cchDst - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------

LRESULT Vc2Notify(HWND hwnd, UINT code, NMHDR *pnm)
{
	HWND hParent = GetParent(hwnd);
	if (hParent == NULL)
		return 0;
	pnm->hwndFrom = hwnd;
	pnm->idFrom = (UINT)GetWindowLong(hwnd, GWL_ID);
	pnm->code = code;
	return SendMessage(hParent, WM_NOTIFY, (WPARAM)pnm->idFrom, (LPARAM)pnm);
}

LRESULT Vc2NotifySimple(HWND hwnd, UINT code)
{
	NMHDR nm;
	memset(&nm, 0, sizeof(nm));
	return Vc2Notify(hwnd, code, &nm);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Vc2Fill(HDC hdc, int l, int t, int r, int b, COLORREF c)
{
	RECT rc;
	HBRUSH hbr;
	if (r <= l || b <= t)
		return;
	rc.left = l; rc.top = t; rc.right = r; rc.bottom = b;
	hbr = CreateSolidBrush(c);
	if (hbr != NULL) {
		FillRect(hdc, &rc, hbr);
		DeleteObject(hbr);
	}
}

void Vc2FillRect(HDC hdc, const RECT *prc, COLORREF c)
{
	Vc2Fill(hdc, prc->left, prc->top, prc->right, prc->bottom, c);
}

void Vc2Frame(HDC hdc, const RECT *prc, COLORREF tl, COLORREF br)
{
	int l = prc->left, t = prc->top, r = prc->right, b = prc->bottom;
	Vc2Fill(hdc, l, t, r - 1, t + 1, tl);       // top
	Vc2Fill(hdc, l, t, l + 1, b - 1, tl);       // left
	Vc2Fill(hdc, l, b - 1, r, b, br);           // bottom
	Vc2Fill(hdc, r - 1, t, r, b, br);           // right
}

// The NT 3.x push-button look: a black frame, then highlight / shadow.
void Vc2DrawRaised(HDC hdc, const RECT *prc)
{
	RECT rc = *prc;
	Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_WINDOWFRAME));
	InflateRect(&rc, -1, -1);
	Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
}

void Vc2DrawSunken(HDC hdc, const RECT *prc)
{
	RECT rc = *prc;
	Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNSHADOW), GetSysColor(COLOR_BTNHIGHLIGHT));
	InflateRect(&rc, -1, -1);
	Vc2Frame(hdc, &rc, GetSysColor(COLOR_WINDOWFRAME), GetSysColor(COLOR_BTNFACE));
}

// Solid triangle centred on (cx, cy), 'size' pixels from base to tip.
void Vc2DrawArrow(HDC hdc, int cx, int cy, int size, int dir, COLORREF c)
{
	int k;
	if (size < 1)
		size = 1;
	// Row (or column) k is k pixels from the tip and 2k+1 pixels wide.
	for (k = 0; k < size; k++) {
		switch (dir) {
		case VC2_ARROW_UP:
			Vc2Fill(hdc, cx - k, cy - size / 2 + k, cx + k + 1, cy - size / 2 + k + 1, c);
			break;
		case VC2_ARROW_DOWN:
			Vc2Fill(hdc, cx - k, cy + size / 2 - k, cx + k + 1, cy + size / 2 - k + 1, c);
			break;
		case VC2_ARROW_LEFT:
			Vc2Fill(hdc, cx - size / 2 + k, cy - k, cx - size / 2 + k + 1, cy + k + 1, c);
			break;
		default:
			Vc2Fill(hdc, cx + size / 2 - k, cy - k, cx + size / 2 - k + 1, cy + k + 1, c);
			break;
		}
	}
}

void Vc2DrawFocus(HDC hdc, const RECT *prc)
{
	// DrawFocusRect XORs, so the text colour must be the usual one for the
	// dotted rectangle to show on a button-face background.
	COLORREF oldText = SetTextColor(hdc, RGB(0, 0, 0));
	COLORREF oldBk = SetBkColor(hdc, RGB(255, 255, 255));
	DrawFocusRect(hdc, prc);
	SetTextColor(hdc, oldText);
	SetBkColor(hdc, oldBk);
}

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------

HFONT Vc2Font(HFONT hf)
{
	return (hf != NULL) ? hf : (HFONT)GetStockObject(SYSTEM_FONT);
}

int Vc2FontHeight(HWND hwnd, HFONT hf)
{
	TEXTMETRIC tm;
	HDC hdc = GetDC(hwnd);
	HFONT hOld;
	int h = 16;
	if (hdc == NULL)
		return h;
	hOld = (HFONT)SelectObject(hdc, Vc2Font(hf));
	if (GetTextMetrics(hdc, &tm))
		h = tm.tmHeight;
	SelectObject(hdc, hOld);
	ReleaseDC(hwnd, hdc);
	return h;
}
