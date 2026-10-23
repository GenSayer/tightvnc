// tab.cpp - built-in SysTabControl32 (single row of tabs)
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Messages: TCM_INSERTITEM, TCM_DELETEITEM, TCM_DELETEALLITEMS, TCM_GETITEM,
// TCM_SETITEM, TCM_GETITEMCOUNT, TCM_GETCURSEL, TCM_SETCURSEL,
// TCM_GETCURFOCUS, TCM_SETCURFOCUS, TCM_GETITEMRECT, TCM_ADJUSTRECT.
// Notifications: TCN_SELCHANGING (non-zero result vetoes), TCN_SELCHANGE.

#include "vc2ctli.h"

typedef struct {
	char *text;
	LPARAM lParam;
	int iImage;
	int width;              // laid-out width of the tab
} TABITEM;

typedef struct {
	TABITEM *items;
	int count;
	int cur;                // selected tab, -1 for none
	HFONT font;
	int tabH;               // height of the tab row
	BOOL focused;
} TABDATA;

#define TAB_PADX    6
#define TAB_LEFT    2       // x of the first tab
#define TAB_SELGROW 2       // the selected tab is this much bigger

static void TabLayout(HWND hwnd, TABDATA *td)
{
	HDC hdc = GetDC(hwnd);
	HFONT hOld;
	SIZE sz;
	TEXTMETRIC tm;
	int i;
	if (hdc == NULL)
		return;
	hOld = (HFONT)SelectObject(hdc, Vc2Font(td->font));
	GetTextMetrics(hdc, &tm);
	td->tabH = tm.tmHeight + 6 + TAB_SELGROW;
	for (i = 0; i < td->count; i++) {
		const char *t = td->items[i].text ? td->items[i].text : "";
		sz.cx = 0;
		GetTextExtentPoint(hdc, t, strlen(t), &sz);
		td->items[i].width = sz.cx + 2 * TAB_PADX;
	}
	SelectObject(hdc, hOld);
	ReleaseDC(hwnd, hdc);
}

// Rectangle of tab i (the unselected size), in client coordinates.
static BOOL TabItemRect(TABDATA *td, int i, RECT *prc)
{
	int x = TAB_LEFT, j;
	if (i < 0 || i >= td->count)
		return FALSE;
	for (j = 0; j < i; j++)
		x += td->items[j].width;
	prc->left = x;
	prc->right = x + td->items[i].width;
	prc->top = TAB_SELGROW;
	prc->bottom = td->tabH;
	return TRUE;
}

static int TabHitTest(TABDATA *td, int x, int y)
{
	RECT rc;
	POINT pt;
	int i;
	pt.x = x; pt.y = y;
	for (i = 0; i < td->count; i++) {
		if (TabItemRect(td, i, &rc)) {
			if (i == td->cur) {
				InflateRect(&rc, TAB_SELGROW, 0);
				rc.top -= TAB_SELGROW;
			}
			if (PtInRect(&rc, pt))
				return i;
		}
	}
	return -1;
}

// User-initiated change: notify, honour a veto, then select.
static void TabUserSelect(HWND hwnd, TABDATA *td, int i)
{
	if (i < 0 || i >= td->count || i == td->cur)
		return;
	if (Vc2NotifySimple(hwnd, TCN_SELCHANGING) != 0)
		return;
	td->cur = i;
	InvalidateRect(hwnd, NULL, TRUE);
	Vc2NotifySimple(hwnd, TCN_SELCHANGE);
}

static void TabPaint(HWND hwnd, TABDATA *td)
{
	PAINTSTRUCT ps;
	RECT rcClient, rc;
	HDC hdc = BeginPaint(hwnd, &ps);
	COLORREF hi = GetSysColor(COLOR_BTNHIGHLIGHT);
	COLORREF sh = GetSysColor(COLOR_BTNSHADOW);
	COLORREF fr = GetSysColor(COLOR_WINDOWFRAME);
	HFONT hOld;
	int i;
	BOOL enabled = IsWindowEnabled(hwnd);

	GetClientRect(hwnd, &rcClient);

	// Leave visible siblings above us (the pages) alone.
	{
		HWND h;
		for (h = GetWindow(hwnd, GW_HWNDPREV); h != NULL; h = GetWindow(h, GW_HWNDPREV)) {
			RECT rs;
			if (!IsWindowVisible(h))
				continue;
			GetWindowRect(h, &rs);
			MapWindowPoints(NULL, hwnd, (POINT *)&rs, 2);
			ExcludeClipRect(hdc, rs.left, rs.top, rs.right, rs.bottom);
		}
	}

	Vc2FillRect(hdc, &rcClient, GetSysColor(COLOR_BTNFACE));

	// Body frame below the tab row.
	rc = rcClient;
	rc.top = td->tabH - 1;
	Vc2Fill(hdc, rc.left, rc.top, rc.right - 1, rc.top + 1, hi);
	Vc2Fill(hdc, rc.left, rc.top, rc.left + 1, rc.bottom - 1, hi);
	Vc2Fill(hdc, rc.left, rc.bottom - 1, rc.right, rc.bottom, fr);
	Vc2Fill(hdc, rc.right - 1, rc.top, rc.right, rc.bottom, fr);
	Vc2Fill(hdc, rc.left + 1, rc.bottom - 2, rc.right - 1, rc.bottom - 1, sh);
	Vc2Fill(hdc, rc.right - 2, rc.top + 1, rc.right - 1, rc.bottom - 1, sh);

	hOld = (HFONT)SelectObject(hdc, Vc2Font(td->font));
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, GetSysColor(enabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT));

	for (i = 0; i < td->count; i++) {
		BOOL sel = (i == td->cur);
		RECT rt;
		const char *t = td->items[i].text ? td->items[i].text : "";
		if (!TabItemRect(td, i, &rc))
			continue;
		if (sel) {
			rc.left -= TAB_SELGROW;
			rc.right += TAB_SELGROW;
			rc.top -= TAB_SELGROW;
			rc.bottom += 1;     // covers the body's top line
		}
		// Left, top (with cut corners) and right edges.
		Vc2Fill(hdc, rc.left, rc.top + 2, rc.left + 1, rc.bottom, hi);
		Vc2Fill(hdc, rc.left + 1, rc.top + 1, rc.left + 2, rc.top + 2, hi);
		Vc2Fill(hdc, rc.left + 2, rc.top, rc.right - 2, rc.top + 1, hi);
		Vc2Fill(hdc, rc.right - 2, rc.top + 1, rc.right - 1, rc.top + 2, fr);
		Vc2Fill(hdc, rc.right - 1, rc.top + 2, rc.right, rc.bottom, fr);
		Vc2Fill(hdc, rc.right - 2, rc.top + 2, rc.right - 1, rc.bottom, sh);
		if (sel)
			Vc2Fill(hdc, rc.left + 1, rc.bottom - 1, rc.right - 2, rc.bottom,
					GetSysColor(COLOR_BTNFACE));

		rt = rc;
		rt.top += 2;
		DrawText(hdc, t, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		if (sel && td->focused) {
			SIZE sz;
			RECT rf;
			sz.cx = 0; sz.cy = 0;
			GetTextExtentPoint(hdc, t, strlen(t), &sz);
			rf.left = (rt.left + rt.right - sz.cx) / 2 - 2;
			rf.right = rf.left + sz.cx + 4;
			rf.top = (rt.top + rt.bottom - sz.cy) / 2 - 1;
			rf.bottom = rf.top + sz.cy + 2;
			Vc2DrawFocus(hdc, &rf);
		}
	}
	SelectObject(hdc, hOld);
	EndPaint(hwnd, &ps);
}

static void TabFreeItems(TABDATA *td)
{
	int i;
	for (i = 0; i < td->count; i++)
		Vc2Free(td->items[i].text);
	Vc2Free(td->items);
	td->items = NULL;
	td->count = 0;
	td->cur = -1;
}

static LRESULT CALLBACK TabWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TABDATA *td = (TABDATA *)Vc2GetData(hwnd);

	if (td == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		td = (TABDATA *)Vc2Alloc(sizeof(TABDATA));
		if (td == NULL)
			return FALSE;
		td->cur = -1;
		Vc2SetData(hwnd, td);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		// The pages are sibling windows lying on top of the tab control, and
		// this control paints its whole client area.  Without
		// WS_CLIPSIBLINGS that paint lands on the pages and blanks them (as
		// COMCTL32 avoids by always setting it).  TabPaint also clips them
		// out explicitly, in case the style change is not honoured.
		SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_CLIPSIBLINGS);
		TabLayout(hwnd, td);
		return 0;

	case WM_NCDESTROY:
		TabFreeItems(td);
		Vc2Free(td);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_SETFONT:
		td->font = (HFONT)wParam;
		TabLayout(hwnd, td);
		if (LOWORD(lParam))
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETFONT:
		return (LRESULT)td->font;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		TabPaint(hwnd, td);
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
			int i = TabHitTest(td, (short)LOWORD(lParam), (short)HIWORD(lParam));
			if (GetWindowLong(hwnd, GWL_STYLE) & WS_TABSTOP)
				SetFocus(hwnd);
			if (i >= 0)
				TabUserSelect(hwnd, td, i);
		}
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_LEFT && td->cur > 0)
			TabUserSelect(hwnd, td, td->cur - 1);
		else if (wParam == VK_RIGHT && td->cur < td->count - 1)
			TabUserSelect(hwnd, td, td->cur + 1);
		return 0;

	case TCM_GETITEMCOUNT:
		return td->count;

	case TCM_INSERTITEMA:
		{
			const TC_ITEM *pi = (const TC_ITEM *)lParam;
			int i = (int)wParam;
			TABITEM *n;
			if (pi == NULL)
				return -1;
			n = (TABITEM *)Vc2Realloc(td->items, (td->count + 1) * sizeof(TABITEM));
			if (n == NULL)
				return -1;
			td->items = n;
			if (i < 0 || i > td->count)
				i = td->count;
			memmove(&td->items[i + 1], &td->items[i], (td->count - i) * sizeof(TABITEM));
			memset(&td->items[i], 0, sizeof(TABITEM));
			if ((pi->mask & TCIF_TEXT) && pi->pszText != NULL)
				td->items[i].text = Vc2StrDup(pi->pszText);
			if (pi->mask & TCIF_PARAM)
				td->items[i].lParam = pi->lParam;
			if (pi->mask & TCIF_IMAGE)
				td->items[i].iImage = pi->iImage;
			td->count++;
			if (td->cur < 0)
				td->cur = 0;
			else if (i <= td->cur)
				td->cur++;
			TabLayout(hwnd, td);
			InvalidateRect(hwnd, NULL, TRUE);
			return i;
		}

	case TCM_SETITEMA:
	case TCM_GETITEMA:
		{
			TC_ITEM *pi = (TC_ITEM *)lParam;
			int i = (int)wParam;
			if (pi == NULL || i < 0 || i >= td->count)
				return FALSE;
			if (msg == TCM_GETITEMA) {
				if (pi->mask & TCIF_TEXT)
					Vc2CopyText(pi->pszText, pi->cchTextMax, td->items[i].text);
				if (pi->mask & TCIF_PARAM)
					pi->lParam = td->items[i].lParam;
				if (pi->mask & TCIF_IMAGE)
					pi->iImage = td->items[i].iImage;
			} else {
				if (pi->mask & TCIF_TEXT) {
					Vc2Free(td->items[i].text);
					td->items[i].text = Vc2StrDup(pi->pszText);
				}
				if (pi->mask & TCIF_PARAM)
					td->items[i].lParam = pi->lParam;
				if (pi->mask & TCIF_IMAGE)
					td->items[i].iImage = pi->iImage;
				TabLayout(hwnd, td);
				InvalidateRect(hwnd, NULL, TRUE);
			}
			return TRUE;
		}

	case TCM_DELETEITEM:
		{
			int i = (int)wParam;
			if (i < 0 || i >= td->count)
				return FALSE;
			Vc2Free(td->items[i].text);
			memmove(&td->items[i], &td->items[i + 1], (td->count - i - 1) * sizeof(TABITEM));
			td->count--;
			if (td->cur >= td->count)
				td->cur = td->count - 1;
			else if (i < td->cur)
				td->cur--;
			InvalidateRect(hwnd, NULL, TRUE);
			return TRUE;
		}

	case TCM_DELETEALLITEMS:
		TabFreeItems(td);
		InvalidateRect(hwnd, NULL, TRUE);
		return TRUE;

	case TCM_GETCURSEL:
	case TCM_GETCURFOCUS:
		return td->cur;

	case TCM_SETCURSEL:
	case TCM_SETCURFOCUS:
		{
			// No notifications: the caller changes the page itself.
			int old = td->cur;
			int i = (int)wParam;
			if (i < 0 || i >= td->count)
				return -1;
			td->cur = i;
			InvalidateRect(hwnd, NULL, TRUE);
			return (msg == TCM_SETCURSEL) ? old : 0;
		}

	case TCM_GETITEMRECT:
		return TabItemRect(td, (int)wParam, (RECT *)lParam);

	case TCM_ADJUSTRECT:
		{
			// TRUE: display rectangle -> window rectangle; FALSE: the reverse.
			RECT *prc = (RECT *)lParam;
			if (prc == NULL)
				return 0;
			if (wParam) {
				prc->left -= 4;
				prc->right += 4;
				prc->top -= td->tabH + 3;
				prc->bottom += 4;
			} else {
				prc->left += 4;
				prc->right -= 4;
				prc->top += td->tabH + 3;
				prc->bottom -= 4;
			}
			return 0;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterTab(void)
{
	return Vc2RegisterClass("SysTabControl32", (WNDPROC)TabWndProc,
							CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, NULL, -1);
}
