// listview.cpp - built-in SysListView32 (report view only)
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Every style is shown as LVS_REPORT, which is all the viewer uses.  The
// header is drawn by the control itself (there is no header control here);
// columns can be resized by dragging a divider.
//
// Messages: LVM_INSERTCOLUMN, LVM_GETCOLUMN, LVM_GETCOLUMNWIDTH,
// LVM_SETCOLUMNWIDTH, LVM_INSERTITEM, LVM_SETITEM, LVM_GETITEM,
// LVM_SETITEMTEXT, LVM_GETITEMTEXT, LVM_DELETEITEM, LVM_DELETEALLITEMS,
// LVM_GETITEMCOUNT, LVM_GETNEXTITEM, LVM_GETSELECTEDCOUNT, LVM_SETITEMSTATE,
// LVM_GETITEMSTATE, LVM_HITTEST, LVM_GETITEMRECT, LVM_ENSUREVISIBLE,
// LVM_SCROLL, LVM_GETTOPINDEX, LVM_GETCOUNTPERPAGE, LVM_REDRAWITEMS,
// LVM_SETEXTENDEDLISTVIEWSTYLE (stored, otherwise ignored: rows are always
// selected across their full width).
//
// Item text may be LPSTR_TEXTCALLBACK; it is then fetched with
// LVN_GETDISPINFO, for sub-items too.  Notifications follow COMCTL32 4.00,
// which FileTransfer.cpp is written for: NM_CLICK, NM_DBLCLK, NM_RCLICK,
// NM_RDBLCLK, NM_RETURN, NM_SETFOCUS, NM_KILLFOCUS (bare NMHDRs) - and no
// LVN_ITEMACTIVATE.  NM_DBLCLK is sent while the double-click message is
// being processed, so GetMessagePos() is the click position.

#include "vc2ctli.h"

#define LV_CALLBACK     ((char *)LPSTR_TEXTCALLBACK)

typedef struct {
	int fmt;
	int cx;
	char *text;
	int iSubItem;
} LVCOL;

typedef struct {
	char *text;             // LV_CALLBACK, a copy, or NULL
	char **sub;             // sub-item texts, index 1..nSub-1 (0 unused)
	int nSub;
	UINT state;
	LPARAM lParam;
	int iImage;
} LVROW;

typedef struct {
	LVCOL *cols;
	int nCols;
	LVROW *rows;
	int nRows;
	int top;                // first visible row
	int xOff;               // horizontal scroll, pixels
	int focus;              // focused row, -1 for none
	int anchor;             // Shift+click anchor
	HFONT font;
	int rowH, hdrH;
	BOOL focused;
	int sizeCol;            // column being resized, -1 for none
	int sizeStartX, sizeStartCx;
	DWORD exStyle;
	BOOL inScroll;          // re-entrancy guard for scroll-bar updates
	char textBuf[MAX_PATH + 1];
} LVDATA;

#define LV_PAD          4
#define LV_DIVIDER_HIT  3

// ---------------------------------------------------------------------------
// Basics
// ---------------------------------------------------------------------------

static void LvMetrics(HWND hwnd, LVDATA *lv)
{
	int fh = Vc2FontHeight(hwnd, lv->font);
	lv->rowH = fh + 2;
	lv->hdrH = (GetWindowLong(hwnd, GWL_STYLE) & LVS_NOCOLUMNHEADER) ? 0 : fh + 6;
}

static int LvTotalWidth(LVDATA *lv)
{
	int w = 0, i;
	for (i = 0; i < lv->nCols; i++)
		w += lv->cols[i].cx;
	return w;
}

static int LvColLeft(LVDATA *lv, int c)
{
	int x = 0, i;
	for (i = 0; i < c && i < lv->nCols; i++)
		x += lv->cols[i].cx;
	return x;
}

static int LvPerPage(HWND hwnd, LVDATA *lv)
{
	RECT rc;
	int n;
	GetClientRect(hwnd, &rc);
	n = (rc.bottom - lv->hdrH) / (lv->rowH > 0 ? lv->rowH : 1);
	return (n > 0) ? n : 1;
}

static void LvUpdateScroll(HWND hwnd, LVDATA *lv)
{
	RECT rc;
	int maxTop, maxX, pass;
	if (lv->inScroll)
		return;
	lv->inScroll = TRUE;
	// Twice: showing one scroll bar can shrink the client area enough to
	// need the other.
	for (pass = 0; pass < 2; pass++) {
		GetClientRect(hwnd, &rc);
		maxTop = lv->nRows - LvPerPage(hwnd, lv);
		if (maxTop < 0) maxTop = 0;
		if (lv->top > maxTop) lv->top = maxTop;
		if (lv->top < 0) lv->top = 0;
		maxX = LvTotalWidth(lv) - rc.right;
		if (maxX < 0) maxX = 0;
		if (lv->xOff > maxX) lv->xOff = maxX;
		if (lv->xOff < 0) lv->xOff = 0;
		// The NT 3.x scroll-bar API: a zero range hides the bar.
		SetScrollRange(hwnd, SB_VERT, 0, maxTop, FALSE);
		SetScrollPos(hwnd, SB_VERT, lv->top, TRUE);
		SetScrollRange(hwnd, SB_HORZ, 0, maxX, FALSE);
		SetScrollPos(hwnd, SB_HORZ, lv->xOff, TRUE);
	}
	lv->inScroll = FALSE;
}

static BOOL LvValid(LVDATA *lv, int i)
{
	return (i >= 0 && i < lv->nRows) ? TRUE : FALSE;
}

static void LvInvalidateRow(HWND hwnd, LVDATA *lv, int i)
{
	RECT rc;
	if (!LvValid(lv, i))
		return;
	GetClientRect(hwnd, &rc);
	rc.top = lv->hdrH + (i - lv->top) * lv->rowH;
	rc.bottom = rc.top + lv->rowH;
	if (rc.bottom > lv->hdrH)
		InvalidateRect(hwnd, &rc, TRUE);
}

static void LvEnsureVisible(HWND hwnd, LVDATA *lv, int i)
{
	int per = LvPerPage(hwnd, lv);
	int old = lv->top;
	if (!LvValid(lv, i))
		return;
	if (i < lv->top)
		lv->top = i;
	else if (i >= lv->top + per)
		lv->top = i - per + 1;
	if (lv->top != old) {
		LvUpdateScroll(hwnd, lv);
		InvalidateRect(hwnd, NULL, TRUE);
	}
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

static void LvFreeRow(LVROW *r)
{
	int s;
	if (r->text != LV_CALLBACK)
		Vc2Free(r->text);
	for (s = 1; s < r->nSub; s++)
		if (r->sub[s] != LV_CALLBACK)
			Vc2Free(r->sub[s]);
	Vc2Free(r->sub);
	memset(r, 0, sizeof(LVROW));
}

static char *LvDupText(const char *s)
{
	if (s == LPSTR_TEXTCALLBACK)
		return LV_CALLBACK;
	return Vc2StrDup(s);
}

static BOOL LvSetSubText(LVROW *r, int sub, const char *s)
{
	if (sub == 0) {
		if (r->text != LV_CALLBACK)
			Vc2Free(r->text);
		r->text = LvDupText(s);
		return TRUE;
	}
	if (sub >= r->nSub) {
		char **n = (char **)Vc2Realloc(r->sub, (sub + 1) * sizeof(char *));
		int k;
		if (n == NULL)
			return FALSE;
		r->sub = n;
		for (k = r->nSub; k <= sub; k++)
			r->sub[k] = NULL;   // slot 0 is never used
		r->nSub = sub + 1;
	} else if (r->sub[sub] != LV_CALLBACK) {
		Vc2Free(r->sub[sub]);
	}
	r->sub[sub] = LvDupText(s);
	return TRUE;
}

// Text of row i, sub-item 'sub'.  A sub-item that was never set is a
// callback if the item itself is (COMCTL32 does the same in report view).
// The result may point into buf, into the row, or into the parent's memory.
static const char *LvGetText(HWND hwnd, LVDATA *lv, int i, int sub, char *buf, int cch)
{
	LVROW *r;
	const char *t;
	LV_DISPINFO di;

	if (!LvValid(lv, i))
		return "";
	r = &lv->rows[i];
	if (sub == 0)
		t = r->text;
	else if (sub < r->nSub && r->sub[sub] != NULL)
		t = r->sub[sub];
	else
		t = (r->text == LV_CALLBACK) ? LV_CALLBACK : NULL;

	if (t != LV_CALLBACK)
		return (t != NULL) ? t : "";

	buf[0] = '\0';
	memset(&di, 0, sizeof(di));
	di.item.mask = LVIF_TEXT;
	di.item.iItem = i;
	di.item.iSubItem = sub;
	di.item.pszText = buf;
	di.item.cchTextMax = cch;
	di.item.lParam = r->lParam;
	Vc2Notify(hwnd, LVN_GETDISPINFO, &di.hdr);
	// The parent may either fill our buffer or point pszText at its own.
	if (di.item.pszText == NULL || di.item.pszText == LPSTR_TEXTCALLBACK)
		return "";
	return di.item.pszText;
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

static void LvSetState(HWND hwnd, LVDATA *lv, int i, UINT state, UINT mask)
{
	UINT old;
	if (!LvValid(lv, i))
		return;
	old = lv->rows[i].state;
	lv->rows[i].state = (old & ~mask) | (state & mask);
	if ((mask & LVIS_FOCUSED) && (state & LVIS_FOCUSED)) {
		if (lv->focus != i && LvValid(lv, lv->focus)) {
			lv->rows[lv->focus].state &= ~LVIS_FOCUSED;
			LvInvalidateRow(hwnd, lv, lv->focus);
		}
		lv->focus = i;
	} else if ((mask & LVIS_FOCUSED) && lv->focus == i) {
		lv->focus = -1;
	}
	if (lv->rows[i].state != old)
		LvInvalidateRow(hwnd, lv, i);
}

static void LvSelectOnly(HWND hwnd, LVDATA *lv, int keep)
{
	int i;
	for (i = 0; i < lv->nRows; i++)
		LvSetState(hwnd, lv, i, (i == keep) ? LVIS_SELECTED : 0, LVIS_SELECTED);
}

static void LvSelectRange(HWND hwnd, LVDATA *lv, int a, int b)
{
	int i, lo = (a < b) ? a : b, hi = (a < b) ? b : a;
	for (i = 0; i < lv->nRows; i++)
		LvSetState(hwnd, lv, i, (i >= lo && i <= hi) ? LVIS_SELECTED : 0, LVIS_SELECTED);
}

static void LvSetFocusRow(HWND hwnd, LVDATA *lv, int i)
{
	if (!LvValid(lv, i))
		return;
	LvSetState(hwnd, lv, i, LVIS_FOCUSED, LVIS_FOCUSED);
	LvEnsureVisible(hwnd, lv, i);
}

// Keyboard or mouse selection of row i, honouring Ctrl / Shift.
static void LvUserSelect(HWND hwnd, LVDATA *lv, int i, BOOL ctrl, BOOL shift)
{
	BOOL single = (GetWindowLong(hwnd, GWL_STYLE) & LVS_SINGLESEL) ? TRUE : FALSE;
	if (!LvValid(lv, i))
		return;
	if (single || (!ctrl && !shift)) {
		LvSelectOnly(hwnd, lv, i);
		lv->anchor = i;
	} else if (shift) {
		if (!LvValid(lv, lv->anchor))
			lv->anchor = i;
		LvSelectRange(hwnd, lv, lv->anchor, i);
	} else {
		LvSetState(hwnd, lv, i, lv->rows[i].state ^ LVIS_SELECTED, LVIS_SELECTED);
		lv->anchor = i;
	}
	LvSetFocusRow(hwnd, lv, i);
}

// ---------------------------------------------------------------------------
// Hit testing / rectangles
// ---------------------------------------------------------------------------

static int LvRowAt(HWND hwnd, LVDATA *lv, int x, int y, UINT *pflags)
{
	RECT rc;
	int row;
	GetClientRect(hwnd, &rc);
	if (y < lv->hdrH || x < 0 || y >= rc.bottom || x >= rc.right) {
		if (pflags) {
			*pflags = LVHT_NOWHERE;
			if (y < lv->hdrH) *pflags = LVHT_ABOVE;
			else if (y >= rc.bottom) *pflags = LVHT_BELOW;
			else if (x < 0) *pflags = LVHT_TOLEFT;
			else if (x >= rc.right) *pflags = LVHT_TORIGHT;
		}
		return -1;
	}
	row = lv->top + (y - lv->hdrH) / lv->rowH;
	if (!LvValid(lv, row) || x + lv->xOff >= LvTotalWidth(lv)) {
		if (pflags)
			*pflags = LVHT_NOWHERE;
		return -1;
	}
	if (pflags)
		*pflags = LVHT_ONITEMLABEL;
	return row;
}

// Column whose right-hand divider is at header x (within a few pixels).
static int LvDividerAt(LVDATA *lv, int x, int y)
{
	int c, right = -lv->xOff;
	if (y < 0 || y >= lv->hdrH)
		return -1;
	for (c = 0; c < lv->nCols; c++) {
		right += lv->cols[c].cx;
		if (x >= right - LV_DIVIDER_HIT && x <= right + LV_DIVIDER_HIT)
			return c;
	}
	return -1;
}

static BOOL LvItemRect(HWND hwnd, LVDATA *lv, int i, int code, RECT *prc)
{
	if (!LvValid(lv, i) || prc == NULL)
		return FALSE;
	prc->top = lv->hdrH + (i - lv->top) * lv->rowH;
	prc->bottom = prc->top + lv->rowH;
	prc->left = -lv->xOff;
	switch (code) {
	case LVIR_ICON:
		prc->right = prc->left;     // no icons
		break;
	case LVIR_LABEL:
		prc->right = prc->left + (lv->nCols > 0 ? lv->cols[0].cx : 0);
		break;
	default:        // LVIR_BOUNDS, LVIR_SELECTBOUNDS
		prc->right = prc->left + LvTotalWidth(lv);
		break;
	}
	return TRUE;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

static UINT LvAlign(int fmt)
{
	switch (fmt & 3) {
	case LVCFMT_RIGHT:  return DT_RIGHT;
	case LVCFMT_CENTER: return DT_CENTER;
	}
	return DT_LEFT;
}

static void LvPaint(HWND hwnd, LVDATA *lv)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rcClient, rc;
	HFONT hOld;
	int c, i, x, y, per;
	BOOL showSel = lv->focused ||
		(GetWindowLong(hwnd, GWL_STYLE) & LVS_SHOWSELALWAYS);
	BOOL enabled = IsWindowEnabled(hwnd);

	GetClientRect(hwnd, &rcClient);
	hOld = (HFONT)SelectObject(hdc, Vc2Font(lv->font));
	SetBkMode(hdc, TRANSPARENT);

	// Header.
	if (lv->hdrH > 0) {
		x = -lv->xOff;
		SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
		for (c = 0; c < lv->nCols; c++) {
			rc.left = x;
			rc.right = x + lv->cols[c].cx;
			rc.top = 0;
			rc.bottom = lv->hdrH;
			x = rc.right;
			if (rc.right <= 0 || rc.left >= rcClient.right)
				continue;
			Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
			Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
			InflateRect(&rc, -LV_PAD, 0);
			DrawText(hdc, lv->cols[c].text ? lv->cols[c].text : "", -1, &rc,
					 LvAlign(c == 0 ? LVCFMT_LEFT : lv->cols[c].fmt) |
					 DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		if (x < rcClient.right) {
			rc.left = (x > 0) ? x : 0;
			rc.right = rcClient.right;
			rc.top = 0;
			rc.bottom = lv->hdrH;
			Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
			Vc2Frame(hdc, &rc, GetSysColor(COLOR_BTNHIGHLIGHT), GetSysColor(COLOR_BTNSHADOW));
		}
	}

	// Rows.
	per = LvPerPage(hwnd, lv) + 1;
	y = lv->hdrH;
	for (i = lv->top; i < lv->nRows && i < lv->top + per; i++, y += lv->rowH) {
		BOOL sel = (lv->rows[i].state & LVIS_SELECTED) && showSel;
		COLORREF bg, fg;
		RECT rr;
		rr.left = 0;
		rr.right = rcClient.right;
		rr.top = y;
		rr.bottom = y + lv->rowH;
		if (rr.top >= rcClient.bottom)
			break;
		if (sel && lv->focused) {
			bg = GetSysColor(COLOR_HIGHLIGHT);
			fg = GetSysColor(COLOR_HIGHLIGHTTEXT);
		} else if (sel) {
			bg = GetSysColor(COLOR_BTNFACE);
			fg = GetSysColor(COLOR_BTNTEXT);
		} else {
			bg = GetSysColor(COLOR_WINDOW);
			fg = GetSysColor(enabled ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT);
		}
		Vc2FillRect(hdc, &rr, GetSysColor(COLOR_WINDOW));
		if (sel) {
			RECT rs = rr;
			rs.left = -lv->xOff;
			rs.right = rs.left + LvTotalWidth(lv);
			Vc2FillRect(hdc, &rs, bg);
		}
		SetTextColor(hdc, fg);
		x = -lv->xOff;
		for (c = 0; c < lv->nCols; c++) {
			RECT rt;
			const char *t;
			rt.left = x + LV_PAD;
			rt.right = x + lv->cols[c].cx - LV_PAD;
			rt.top = y;
			rt.bottom = y + lv->rowH;
			x += lv->cols[c].cx;
			if (rt.right <= rt.left || rt.right <= 0 || rt.left >= rcClient.right)
				continue;
			t = LvGetText(hwnd, lv, i, lv->cols[c].iSubItem, lv->textBuf, sizeof(lv->textBuf));
			DrawText(hdc, t, -1, &rt,
					 LvAlign(c == 0 ? LVCFMT_LEFT : lv->cols[c].fmt) |
					 DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		if (lv->focused && i == lv->focus) {
			RECT rf = rr;
			rf.left = -lv->xOff;
			rf.right = rf.left + LvTotalWidth(lv);
			if (rf.right > rcClient.right)
				rf.right = rcClient.right;
			if (rf.left < 0)
				rf.left = 0;
			Vc2DrawFocus(hdc, &rf);
		}
	}
	if (y < rcClient.bottom) {
		rc = rcClient;
		rc.top = (y > lv->hdrH) ? y : lv->hdrH;
		Vc2FillRect(hdc, &rc, GetSysColor(COLOR_WINDOW));
	}

	SelectObject(hdc, hOld);
	EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Item / column management
// ---------------------------------------------------------------------------

static void LvDeleteAll(HWND hwnd, LVDATA *lv)
{
	int i;
	for (i = 0; i < lv->nRows; i++)
		LvFreeRow(&lv->rows[i]);
	Vc2Free(lv->rows);
	lv->rows = NULL;
	lv->nRows = 0;
	lv->top = 0;
	lv->focus = -1;
	lv->anchor = -1;
	LvUpdateScroll(hwnd, lv);
	InvalidateRect(hwnd, NULL, TRUE);
}

static int LvInsertRow(HWND hwnd, LVDATA *lv, const LV_ITEM *pi)
{
	LVROW *n;
	int i;
	if (pi == NULL || pi->iSubItem != 0)
		return -1;
	n = (LVROW *)Vc2Realloc(lv->rows, (lv->nRows + 1) * sizeof(LVROW));
	if (n == NULL)
		return -1;
	lv->rows = n;
	i = pi->iItem;
	if (i < 0 || i > lv->nRows)
		i = lv->nRows;
	memmove(&lv->rows[i + 1], &lv->rows[i], (lv->nRows - i) * sizeof(LVROW));
	memset(&lv->rows[i], 0, sizeof(LVROW));
	lv->nRows++;
	if (lv->focus >= i) lv->focus++;
	if (lv->anchor >= i) lv->anchor++;

	if (pi->mask & LVIF_TEXT)
		lv->rows[i].text = LvDupText(pi->pszText);
	if (pi->mask & LVIF_PARAM)
		lv->rows[i].lParam = pi->lParam;
	if (pi->mask & LVIF_IMAGE)
		lv->rows[i].iImage = pi->iImage;
	if (pi->mask & LVIF_STATE) {
		UINT st = pi->state & pi->stateMask & (LVIS_SELECTED | LVIS_FOCUSED);
		lv->rows[i].state = st & LVIS_SELECTED;
		if (st & LVIS_FOCUSED)
			LvSetState(hwnd, lv, i, LVIS_FOCUSED, LVIS_FOCUSED);
	}
	LvUpdateScroll(hwnd, lv);
	InvalidateRect(hwnd, NULL, TRUE);
	return i;
}

static BOOL LvDeleteRow(HWND hwnd, LVDATA *lv, int i)
{
	if (!LvValid(lv, i))
		return FALSE;
	LvFreeRow(&lv->rows[i]);
	memmove(&lv->rows[i], &lv->rows[i + 1], (lv->nRows - i - 1) * sizeof(LVROW));
	lv->nRows--;
	if (lv->focus == i) lv->focus = -1;
	else if (lv->focus > i) lv->focus--;
	if (lv->anchor == i) lv->anchor = -1;
	else if (lv->anchor > i) lv->anchor--;
	LvUpdateScroll(hwnd, lv);
	InvalidateRect(hwnd, NULL, TRUE);
	return TRUE;
}

static int LvInsertColumn(HWND hwnd, LVDATA *lv, int i, const LV_COLUMN *pc)
{
	LVCOL *n;
	if (pc == NULL)
		return -1;
	n = (LVCOL *)Vc2Realloc(lv->cols, (lv->nCols + 1) * sizeof(LVCOL));
	if (n == NULL)
		return -1;
	lv->cols = n;
	if (i < 0 || i > lv->nCols)
		i = lv->nCols;
	memmove(&lv->cols[i + 1], &lv->cols[i], (lv->nCols - i) * sizeof(LVCOL));
	memset(&lv->cols[i], 0, sizeof(LVCOL));
	lv->cols[i].cx = (pc->mask & LVCF_WIDTH) ? pc->cx : 60;
	if (lv->cols[i].cx < 0)
		lv->cols[i].cx = 0;
	lv->cols[i].fmt = (pc->mask & LVCF_FMT) ? pc->fmt : LVCFMT_LEFT;
	lv->cols[i].iSubItem = (pc->mask & LVCF_SUBITEM) ? pc->iSubItem : i;
	if ((pc->mask & LVCF_TEXT) && pc->pszText != NULL && pc->pszText != LPSTR_TEXTCALLBACK)
		lv->cols[i].text = Vc2StrDup(pc->pszText);
	lv->nCols++;
	LvUpdateScroll(hwnd, lv);
	InvalidateRect(hwnd, NULL, TRUE);
	return i;
}

// ---------------------------------------------------------------------------
// Scrolling
// ---------------------------------------------------------------------------

static void LvScrollTo(HWND hwnd, LVDATA *lv, int top, int xOff)
{
	int oldTop = lv->top, oldX = lv->xOff;
	lv->top = top;
	lv->xOff = xOff;
	LvUpdateScroll(hwnd, lv);
	if (lv->top != oldTop || lv->xOff != oldX)
		InvalidateRect(hwnd, NULL, TRUE);
}

static void LvOnScroll(HWND hwnd, LVDATA *lv, BOOL vert, int code, int thumb)
{
	int per = LvPerPage(hwnd, lv);
	RECT rc;
	int v = vert ? lv->top : lv->xOff;
	int line = vert ? 1 : 8;
	int page;
	GetClientRect(hwnd, &rc);
	page = vert ? per : (rc.right > 16 ? rc.right - 16 : 8);
	switch (code) {
	case SB_LINEUP:         v -= line; break;
	case SB_LINEDOWN:       v += line; break;
	case SB_PAGEUP:         v -= page; break;
	case SB_PAGEDOWN:       v += page; break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:  v = thumb; break;
	case SB_TOP:            v = 0; break;
	case SB_BOTTOM:         v = 0x7FFF; break;
	default:                return;
	}
	if (vert)
		LvScrollTo(hwnd, lv, v, lv->xOff);
	else
		LvScrollTo(hwnd, lv, lv->top, v);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static BOOL KeyDown(int vk)
{
	return (GetKeyState(vk) & 0x8000) ? TRUE : FALSE;
}

static LRESULT CALLBACK LvWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LVDATA *lv = (LVDATA *)Vc2GetData(hwnd);

	if (lv == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		lv = (LVDATA *)Vc2Alloc(sizeof(LVDATA));
		if (lv == NULL)
			return FALSE;
		lv->focus = -1;
		lv->anchor = -1;
		lv->sizeCol = -1;
		Vc2SetData(hwnd, lv);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		LvMetrics(hwnd, lv);
		LvUpdateScroll(hwnd, lv);
		return 0;

	case WM_NCDESTROY:
		{
			int c;
			LvDeleteAll(hwnd, lv);
			for (c = 0; c < lv->nCols; c++)
				Vc2Free(lv->cols[c].text);
			Vc2Free(lv->cols);
			Vc2Free(lv);
			Vc2SetData(hwnd, NULL);
		}
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_SETFONT:
		lv->font = (HFONT)wParam;
		LvMetrics(hwnd, lv);
		LvUpdateScroll(hwnd, lv);
		if (LOWORD(lParam))
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETFONT:
		return (LRESULT)lv->font;

	case WM_SIZE:
		LvUpdateScroll(hwnd, lv);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		LvPaint(hwnd, lv);
		return 0;

	case WM_ENABLE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS | DLGC_WANTCHARS;

	case WM_SETFOCUS:
		lv->focused = TRUE;
		InvalidateRect(hwnd, NULL, TRUE);
		Vc2NotifySimple(hwnd, NM_SETFOCUS);
		return 0;

	case WM_KILLFOCUS:
		lv->focused = FALSE;
		InvalidateRect(hwnd, NULL, TRUE);
		Vc2NotifySimple(hwnd, NM_KILLFOCUS);
		return 0;

	case WM_VSCROLL:
	case WM_HSCROLL:
		LvOnScroll(hwnd, lv, msg == WM_VSCROLL, LOWORD(wParam), (short)HIWORD(wParam));
		return 0;

	case WM_SETCURSOR:
		if ((HWND)wParam == hwnd && LOWORD(lParam) == HTCLIENT) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(hwnd, &pt);
			if (lv->sizeCol >= 0 || LvDividerAt(lv, pt.x, pt.y) >= 0) {
				SetCursor(LoadCursor(NULL, IDC_SIZEWE));
				return TRUE;
			}
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		{
			int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
			int row, div;
			BOOL left = (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK);
			BOOL dbl = (msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDBLCLK);
			if (GetFocus() != hwnd)
				SetFocus(hwnd);
			if (left && (div = LvDividerAt(lv, x, y)) >= 0) {
				lv->sizeCol = div;
				lv->sizeStartX = x;
				lv->sizeStartCx = lv->cols[div].cx;
				SetCapture(hwnd);
				return 0;
			}
			if (y < lv->hdrH)
				return 0;       // header click: no sorting (LVS_NOSORTHEADER)
			row = LvRowAt(hwnd, lv, x, y, NULL);
			if (row >= 0) {
				if (left || !(lv->rows[row].state & LVIS_SELECTED))
					LvUserSelect(hwnd, lv, row,
								 left ? (wParam & MK_CONTROL) != 0 : FALSE,
								 left ? (wParam & MK_SHIFT) != 0 : FALSE);
			} else if (left && !(wParam & (MK_CONTROL | MK_SHIFT))) {
				LvSelectOnly(hwnd, lv, -1);
			}
			// Last: the parent may refill the list from the notification.
			if (left)
				Vc2NotifySimple(hwnd, dbl ? NM_DBLCLK : NM_CLICK);
			else
				Vc2NotifySimple(hwnd, dbl ? NM_RDBLCLK : NM_RCLICK);
		}
		return 0;

	case WM_MOUSEMOVE:
		if (lv->sizeCol >= 0 && lv->sizeCol < lv->nCols) {
			int cx = lv->sizeStartCx + ((short)LOWORD(lParam) - lv->sizeStartX);
			if (cx < 0)
				cx = 0;
			if (cx != lv->cols[lv->sizeCol].cx) {
				lv->cols[lv->sizeCol].cx = cx;
				LvUpdateScroll(hwnd, lv);
				InvalidateRect(hwnd, NULL, TRUE);
			}
		}
		return 0;

	case WM_LBUTTONUP:
		if (lv->sizeCol >= 0) {
			lv->sizeCol = -1;
			ReleaseCapture();
		}
		return 0;

	case WM_CAPTURECHANGED:
		lv->sizeCol = -1;
		return 0;

	case WM_KEYDOWN:
		{
			int per = LvPerPage(hwnd, lv);
			int cur = LvValid(lv, lv->focus) ? lv->focus : -1;
			int to = cur;
			BOOL ctrl = KeyDown(VK_CONTROL), shift = KeyDown(VK_SHIFT);
			switch (wParam) {
			case VK_UP:     to = (cur < 0) ? 0 : cur - 1; break;
			case VK_DOWN:   to = (cur < 0) ? 0 : cur + 1; break;
			case VK_PRIOR:  to = (cur < 0) ? 0 : cur - (per > 1 ? per - 1 : 1); break;
			case VK_NEXT:   to = (cur < 0) ? 0 : cur + (per > 1 ? per - 1 : 1); break;
			case VK_HOME:   to = 0; break;
			case VK_END:    to = lv->nRows - 1; break;
			case VK_LEFT:   LvOnScroll(hwnd, lv, FALSE, SB_LINEUP, 0); return 0;
			case VK_RIGHT:  LvOnScroll(hwnd, lv, FALSE, SB_LINEDOWN, 0); return 0;
			case VK_SPACE:
				if (ctrl && cur >= 0)
					LvUserSelect(hwnd, lv, cur, TRUE, FALSE);
				return 0;
			case VK_RETURN:
				Vc2NotifySimple(hwnd, NM_RETURN);
				return 0;
			default:
				return 0;
			}
			if (lv->nRows == 0)
				return 0;
			if (to < 0) to = 0;
			if (to >= lv->nRows) to = lv->nRows - 1;
			if (ctrl && !shift)
				LvSetFocusRow(hwnd, lv, to);     // move the focus only
			else
				LvUserSelect(hwnd, lv, to, FALSE, shift);
		}
		return 0;

	// ---- columns ----
	case LVM_INSERTCOLUMNA:
		return LvInsertColumn(hwnd, lv, (int)wParam, (const LV_COLUMN *)lParam);

	case LVM_GETCOLUMNA:
		{
			LV_COLUMN *pc = (LV_COLUMN *)lParam;
			int c = (int)wParam;
			if (pc == NULL || c < 0 || c >= lv->nCols)
				return FALSE;
			if (pc->mask & LVCF_FMT) pc->fmt = lv->cols[c].fmt;
			if (pc->mask & LVCF_WIDTH) pc->cx = lv->cols[c].cx;
			if (pc->mask & LVCF_SUBITEM) pc->iSubItem = lv->cols[c].iSubItem;
			if (pc->mask & LVCF_TEXT)
				Vc2CopyText(pc->pszText, pc->cchTextMax, lv->cols[c].text);
			return TRUE;
		}

	case LVM_GETCOLUMNWIDTH:
		if ((int)wParam < 0 || (int)wParam >= lv->nCols)
			return 0;
		return lv->cols[wParam].cx;

	case LVM_SETCOLUMNWIDTH:
		if ((int)wParam < 0 || (int)wParam >= lv->nCols)
			return FALSE;
		lv->cols[wParam].cx = ((short)LOWORD(lParam) < 0) ? 60 : (short)LOWORD(lParam);
		LvUpdateScroll(hwnd, lv);
		InvalidateRect(hwnd, NULL, TRUE);
		return TRUE;

	// ---- items ----
	case LVM_INSERTITEMA:
		return LvInsertRow(hwnd, lv, (const LV_ITEM *)lParam);

	case LVM_DELETEITEM:
		return LvDeleteRow(hwnd, lv, (int)wParam);

	case LVM_DELETEALLITEMS:
		LvDeleteAll(hwnd, lv);
		return TRUE;

	case LVM_GETITEMCOUNT:
		return lv->nRows;

	case LVM_SETITEMA:
		{
			const LV_ITEM *pi = (const LV_ITEM *)lParam;
			if (pi == NULL || !LvValid(lv, pi->iItem))
				return FALSE;
			if (pi->mask & LVIF_TEXT)
				LvSetSubText(&lv->rows[pi->iItem], pi->iSubItem, pi->pszText);
			if (pi->iSubItem == 0) {
				if (pi->mask & LVIF_PARAM)
					lv->rows[pi->iItem].lParam = pi->lParam;
				if (pi->mask & LVIF_IMAGE)
					lv->rows[pi->iItem].iImage = pi->iImage;
				if (pi->mask & LVIF_STATE)
					LvSetState(hwnd, lv, pi->iItem, pi->state, pi->stateMask);
			}
			LvInvalidateRow(hwnd, lv, pi->iItem);
			return TRUE;
		}

	case LVM_SETITEMTEXTA:
		{
			const LV_ITEM *pi = (const LV_ITEM *)lParam;
			if (pi == NULL || !LvValid(lv, (int)wParam))
				return FALSE;
			LvSetSubText(&lv->rows[wParam], pi->iSubItem, pi->pszText);
			LvInvalidateRow(hwnd, lv, (int)wParam);
			return TRUE;
		}

	case LVM_GETITEMA:
		{
			LV_ITEM *pi = (LV_ITEM *)lParam;
			if (pi == NULL || !LvValid(lv, pi->iItem))
				return FALSE;
			if (pi->mask & LVIF_TEXT) {
				const char *t = LvGetText(hwnd, lv, pi->iItem, pi->iSubItem,
										  lv->textBuf, sizeof(lv->textBuf));
				if (t != pi->pszText)
					Vc2CopyText(pi->pszText, pi->cchTextMax, t);
			}
			if (pi->mask & LVIF_PARAM)
				pi->lParam = lv->rows[pi->iItem].lParam;
			if (pi->mask & LVIF_IMAGE)
				pi->iImage = lv->rows[pi->iItem].iImage;
			if (pi->mask & LVIF_STATE)
				pi->state = lv->rows[pi->iItem].state & pi->stateMask;
			return TRUE;
		}

	case LVM_GETITEMTEXTA:
		{
			LV_ITEM *pi = (LV_ITEM *)lParam;
			const char *t;
			if (pi == NULL || pi->pszText == NULL || pi->cchTextMax <= 0)
				return 0;
			if (!LvValid(lv, (int)wParam)) {
				pi->pszText[0] = '\0';
				return 0;
			}
			t = LvGetText(hwnd, lv, (int)wParam, pi->iSubItem,
						  lv->textBuf, sizeof(lv->textBuf));
			if (t != pi->pszText)
				Vc2CopyText(pi->pszText, pi->cchTextMax, t);
			return strlen(pi->pszText);
		}

	case LVM_SETITEMSTATE:
		{
			const LV_ITEM *pi = (const LV_ITEM *)lParam;
			int i;
			if (pi == NULL)
				return FALSE;
			if ((int)wParam == -1) {
				for (i = 0; i < lv->nRows; i++)
					LvSetState(hwnd, lv, i, pi->state, pi->stateMask);
				return TRUE;
			}
			if (!LvValid(lv, (int)wParam))
				return FALSE;
			LvSetState(hwnd, lv, (int)wParam, pi->state, pi->stateMask);
			return TRUE;
		}

	case LVM_GETITEMSTATE:
		if (!LvValid(lv, (int)wParam))
			return 0;
		return lv->rows[wParam].state & (UINT)lParam;

	case LVM_GETNEXTITEM:
		{
			int i = (int)wParam + 1;
			UINT flags = LOWORD(lParam);
			if (i < 0)
				i = 0;
			for (; i < lv->nRows; i++) {
				UINT st = lv->rows[i].state;
				if ((flags & LVNI_SELECTED) && !(st & LVIS_SELECTED))
					continue;
				if ((flags & LVNI_FOCUSED) && !(st & LVIS_FOCUSED))
					continue;
				return i;
			}
			return -1;
		}

	case LVM_GETSELECTEDCOUNT:
		{
			int i, n = 0;
			for (i = 0; i < lv->nRows; i++)
				if (lv->rows[i].state & LVIS_SELECTED)
					n++;
			return n;
		}

	// ---- geometry ----
	case LVM_HITTEST:
		{
			LV_HITTESTINFO *ph = (LV_HITTESTINFO *)lParam;
			if (ph == NULL)
				return -1;
			ph->iItem = LvRowAt(hwnd, lv, ph->pt.x, ph->pt.y, &ph->flags);
			return ph->iItem;
		}

	case LVM_GETITEMRECT:
		{
			RECT *prc = (RECT *)lParam;
			if (prc == NULL)
				return FALSE;
			return LvItemRect(hwnd, lv, (int)wParam, prc->left, prc);
		}

	case LVM_ENSUREVISIBLE:
		LvEnsureVisible(hwnd, lv, (int)wParam);
		return LvValid(lv, (int)wParam);

	case LVM_SCROLL:
		{
			// Report view: dy is in pixels and is rounded to whole rows.
			int dy = (int)lParam;
			int rows = (lv->rowH > 0) ? dy / lv->rowH : 0;
			if (dy != 0 && rows == 0)
				rows = (dy > 0) ? 1 : -1;
			LvScrollTo(hwnd, lv, lv->top + rows, lv->xOff + (int)wParam);
			return TRUE;
		}

	case LVM_GETTOPINDEX:
		return lv->top;

	case LVM_GETCOUNTPERPAGE:
		return LvPerPage(hwnd, lv);

	case LVM_REDRAWITEMS:
		InvalidateRect(hwnd, NULL, TRUE);
		return TRUE;

	case LVM_SETEXTENDEDLISTVIEWSTYLE:
		{
			DWORD old = lv->exStyle;
			DWORD mask = wParam ? (DWORD)wParam : 0xFFFFFFFF;
			lv->exStyle = (lv->exStyle & ~mask) | ((DWORD)lParam & mask);
			return old;
		}

	case LVM_GETEXTENDEDLISTVIEWSTYLE:
		return lv->exStyle;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterListView(void)
{
	return Vc2RegisterClass("SysListView32", (WNDPROC)LvWndProc,
							CS_DBLCLKS, NULL, -1);
}
