// treeview.cpp - built-in SysTreeView32
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Messages: TVM_INSERTITEM (TVI_ROOT / TVI_FIRST / TVI_LAST / TVI_SORT or a
// sibling), TVM_DELETEITEM (TVI_ROOT deletes everything), TVM_GETNEXTITEM
// (ROOT, NEXT, PREVIOUS, PARENT, CHILD, CARET, FIRSTVISIBLE, NEXTVISIBLE,
// PREVIOUSVISIBLE), TVM_GETITEM, TVM_SETITEM, TVM_EXPAND (COLLAPSE, EXPAND,
// TOGGLE, COLLAPSERESET), TVM_SELECTITEM, TVM_GETITEMRECT, TVM_ENSUREVISIBLE,
// TVM_GETCOUNT.  Image fields are accepted and ignored.
// Styles: TVS_HASBUTTONS, TVS_HASLINES, TVS_LINESATROOT, TVS_SHOWSELALWAYS.
// Notifications (NM_TREEVIEW): TVN_ITEMEXPANDING (non-zero result vetoes),
// TVN_ITEMEXPANDED, TVN_SELCHANGING (vetoes), TVN_SELCHANGED; and NM_CLICK,
// NM_DBLCLK, NM_RETURN, NM_SETFOCUS, NM_KILLFOCUS.
//
// TVN_ITEMEXPANDING is sent on every expand or collapse the user makes, and
// for TVM_EXPAND only until the item has been expanded once (or after
// TVE_COLLAPSERESET) - the COMCTL32 behaviour FileTransfer.cpp is written
// around.  The parent may pump messages and add or delete the item's
// children while it handles the notification (the server browse does); input
// that arrives meanwhile is ignored.

#include "vc2ctli.h"

typedef struct TVNODE {
	struct TVNODE *parent, *child, *next, *prev;
	char *text;
	UINT state;
	int cChildren;
	LPARAM lParam;
	int iImage, iSelImage;
} TVNODE;

typedef struct {
	TVNODE *first;          // first top-level item
	TVNODE *sel;
	int top;                // index of the first visible row
	int xOff;
	HFONT font;
	int rowH;
	int indent;
	int contentW;           // widest visible row, for the horizontal bar
	BOOL focused;
	int busy;               // inside a notification that may pump messages
	BOOL inScroll;
} TVDATA;

#define TV_BUTTON       9
#define H(n)            ((HTREEITEM)(n))
#define N(h)            ((TVNODE *)(h))

// ---------------------------------------------------------------------------
// Tree walking
// ---------------------------------------------------------------------------

static BOOL TvExpanded(TVNODE *n)
{
	return (n->state & TVIS_EXPANDED) ? TRUE : FALSE;
}

static BOOL TvHasButton(TVNODE *n)
{
	return (n->child != NULL || n->cChildren != 0) ? TRUE : FALSE;
}

static TVNODE *TvNextVisible(TVNODE *n)
{
	if (n == NULL)
		return NULL;
	if (TvExpanded(n) && n->child != NULL)
		return n->child;
	while (n != NULL) {
		if (n->next != NULL)
			return n->next;
		n = n->parent;
	}
	return NULL;
}

static TVNODE *TvLastVisibleDescendant(TVNODE *n)
{
	while (TvExpanded(n) && n->child != NULL) {
		n = n->child;
		while (n->next != NULL)
			n = n->next;
	}
	return n;
}

static TVNODE *TvPrevVisible(TVNODE *n)
{
	if (n == NULL)
		return NULL;
	if (n->prev != NULL)
		return TvLastVisibleDescendant(n->prev);
	return n->parent;
}

// Is n shown (all its ancestors expanded)?
static BOOL TvIsShown(TVNODE *n)
{
	TVNODE *p;
	for (p = n->parent; p != NULL; p = p->parent)
		if (!TvExpanded(p))
			return FALSE;
	return TRUE;
}

static int TvIndexOf(TVDATA *td, TVNODE *n)
{
	TVNODE *v;
	int i = 0;
	for (v = td->first; v != NULL; v = TvNextVisible(v), i++)
		if (v == n)
			return i;
	return -1;
}

static TVNODE *TvNodeAt(TVDATA *td, int index)
{
	TVNODE *v;
	int i = 0;
	if (index < 0)
		return NULL;
	for (v = td->first; v != NULL; v = TvNextVisible(v), i++)
		if (i == index)
			return v;
	return NULL;
}

static int TvVisibleCount(TVDATA *td)
{
	TVNODE *v;
	int i = 0;
	for (v = td->first; v != NULL; v = TvNextVisible(v))
		i++;
	return i;
}

static int TvDepth(TVNODE *n)
{
	int d = 0;
	for (n = n->parent; n != NULL; n = n->parent)
		d++;
	return d;
}

static BOOL TvIsAncestor(TVNODE *a, TVNODE *n)
{
	for (n = n->parent; n != NULL; n = n->parent)
		if (n == a)
			return TRUE;
	return FALSE;
}

static int TvCountAll(TVNODE *n)
{
	int c = 0;
	for (; n != NULL; n = n->next)
		c += 1 + TvCountAll(n->child);
	return c;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

static int TvLevel(HWND hwnd, TVNODE *n)
{
	// Column of the item's button / line; the text starts one indent later.
	int lvl = TvDepth(n);
	if (GetWindowLong(hwnd, GWL_STYLE) & TVS_LINESATROOT)
		lvl++;
	return lvl;
}

static int TvTextWidth(HWND hwnd, TVDATA *td, HDC hdc, TVNODE *n)
{
	SIZE sz;
	const char *t = n->text ? n->text : "";
	sz.cx = 0;
	GetTextExtentPoint(hdc, t, strlen(t), &sz);
	(void)hwnd; (void)td;
	return sz.cx + 4;
}

static int TvPerPage(HWND hwnd, TVDATA *td)
{
	RECT rc;
	int n;
	GetClientRect(hwnd, &rc);
	n = rc.bottom / (td->rowH > 0 ? td->rowH : 1);
	return (n > 0) ? n : 1;
}

static void TvMetrics(HWND hwnd, TVDATA *td)
{
	int fh = Vc2FontHeight(hwnd, td->font);
	td->rowH = fh + 2;
	if (td->rowH < TV_BUTTON + 4)
		td->rowH = TV_BUTTON + 4;
	td->indent = 19;
}

static void TvUpdateScroll(HWND hwnd, TVDATA *td)
{
	RECT rc;
	HDC hdc;
	HFONT hOld;
	TVNODE *v;
	int maxTop, maxX, pass, count;

	if (td->inScroll)
		return;
	td->inScroll = TRUE;

	td->contentW = 0;
	hdc = GetDC(hwnd);
	if (hdc != NULL) {
		hOld = (HFONT)SelectObject(hdc, Vc2Font(td->font));
		for (v = td->first; v != NULL; v = TvNextVisible(v)) {
			int r = TvLevel(hwnd, v) * td->indent + 2 + TvTextWidth(hwnd, td, hdc, v);
			if (r > td->contentW)
				td->contentW = r;
		}
		SelectObject(hdc, hOld);
		ReleaseDC(hwnd, hdc);
	}
	count = TvVisibleCount(td);

	for (pass = 0; pass < 2; pass++) {
		GetClientRect(hwnd, &rc);
		maxTop = count - TvPerPage(hwnd, td);
		if (maxTop < 0) maxTop = 0;
		if (td->top > maxTop) td->top = maxTop;
		if (td->top < 0) td->top = 0;
		maxX = td->contentW - rc.right;
		if (maxX < 0) maxX = 0;
		if (td->xOff > maxX) td->xOff = maxX;
		if (td->xOff < 0) td->xOff = 0;
		SetScrollRange(hwnd, SB_VERT, 0, maxTop, FALSE);
		SetScrollPos(hwnd, SB_VERT, td->top, TRUE);
		SetScrollRange(hwnd, SB_HORZ, 0, maxX, FALSE);
		SetScrollPos(hwnd, SB_HORZ, td->xOff, TRUE);
	}
	td->inScroll = FALSE;
}

static void TvChanged(HWND hwnd, TVDATA *td)
{
	TvUpdateScroll(hwnd, td);
	InvalidateRect(hwnd, NULL, TRUE);
}

static void TvScrollIntoView(HWND hwnd, TVDATA *td, TVNODE *n)
{
	int i = TvIndexOf(td, n);
	int per = TvPerPage(hwnd, td);
	int old = td->top;
	if (i < 0)
		return;
	if (i < td->top)
		td->top = i;
	else if (i >= td->top + per)
		td->top = i - per + 1;
	if (td->top != old)
		TvChanged(hwnd, td);
}

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------

static void TvFillItem(TV_ITEM *pi, TVNODE *n)
{
	memset(pi, 0, sizeof(TV_ITEM));
	if (n == NULL)
		return;
	pi->mask = TVIF_HANDLE | TVIF_STATE | TVIF_PARAM | TVIF_TEXT | TVIF_CHILDREN;
	pi->hItem = H(n);
	pi->state = n->state;
	pi->stateMask = 0xFFFF;
	pi->lParam = n->lParam;
	pi->pszText = n->text;
	pi->cChildren = TvHasButton(n) ? 1 : 0;
}

static LRESULT TvNotifyTree(HWND hwnd, TVDATA *td, UINT code, UINT action,
							TVNODE *oldN, TVNODE *newN)
{
	NM_TREEVIEW nm;
	LRESULT r;
	memset(&nm, 0, sizeof(nm));
	nm.action = action;
	TvFillItem(&nm.itemOld, oldN);
	TvFillItem(&nm.itemNew, newN);
	td->busy++;
	r = Vc2Notify(hwnd, code, &nm.hdr);
	td->busy--;
	return r;
}

// ---------------------------------------------------------------------------
// Selection / expansion
// ---------------------------------------------------------------------------

static BOOL TvSelect(HWND hwnd, TVDATA *td, TVNODE *n, UINT action)
{
	TVNODE *old = td->sel;
	if (n == old)
		return TRUE;
	if (TvNotifyTree(hwnd, td, TVN_SELCHANGING, action, old, n) != 0)
		return FALSE;
	if (old != NULL)
		old->state &= ~TVIS_SELECTED;
	td->sel = n;
	if (n != NULL) {
		n->state |= TVIS_SELECTED;
		TvScrollIntoView(hwnd, td, n);
	}
	InvalidateRect(hwnd, NULL, TRUE);
	TvNotifyTree(hwnd, td, TVN_SELCHANGED, action, old, n);
	return TRUE;
}

static void TvFreeSubtree(TVDATA *td, TVNODE *n);

static void TvDeleteChildren(TVDATA *td, TVNODE *n)
{
	TVNODE *c = n->child, *nx;
	while (c != NULL) {
		nx = c->next;
		TvFreeSubtree(td, c);
		c = nx;
	}
	n->child = NULL;
}

static void TvFreeSubtree(TVDATA *td, TVNODE *n)
{
	TvDeleteChildren(td, n);
	if (td->sel == n)
		td->sel = NULL;
	Vc2Free(n->text);
	Vc2Free(n);
}

static void TvUnlink(TVDATA *td, TVNODE *n)
{
	if (n->prev != NULL)
		n->prev->next = n->next;
	else if (n->parent != NULL)
		n->parent->child = n->next;
	else
		td->first = n->next;
	if (n->next != NULL)
		n->next->prev = n->prev;
	n->prev = n->next = NULL;
}

static BOOL TvExpand(HWND hwnd, TVDATA *td, TVNODE *n, UINT code, BOOL byUser)
{
	UINT action = code & TVE_TOGGLE;
	BOOL notify;

	if (n == NULL)
		return FALSE;
	if (action == TVE_TOGGLE)
		action = TvExpanded(n) ? TVE_COLLAPSE : TVE_EXPAND;
	if (action == TVE_EXPAND && TvExpanded(n))
		return TRUE;
	if (action == TVE_COLLAPSE && !TvExpanded(n) && !(code & TVE_COLLAPSERESET))
		return FALSE;
	if (action == 0)
		return FALSE;

	notify = byUser || (action == TVE_EXPAND && !(n->state & TVIS_EXPANDEDONCE));
	if (notify && TvNotifyTree(hwnd, td, TVN_ITEMEXPANDING, action, NULL, n) != 0)
		return FALSE;

	if (action == TVE_EXPAND) {
		n->state |= TVIS_EXPANDEDONCE;
		if (n->child != NULL) {
			n->state |= TVIS_EXPANDED;
		} else {
			// Nothing to show: drop the [+] as COMCTL32 does.
			n->state &= ~TVIS_EXPANDED;
			n->cChildren = 0;
		}
	} else {
		n->state &= ~TVIS_EXPANDED;
		if (td->sel != NULL && TvIsAncestor(n, td->sel))
			TvSelect(hwnd, td, n, TVC_UNKNOWN);
		if (code & TVE_COLLAPSERESET) {
			TvDeleteChildren(td, n);
			n->state &= ~TVIS_EXPANDEDONCE;
		}
	}
	TvChanged(hwnd, td);

	if (notify)
		TvNotifyTree(hwnd, td, TVN_ITEMEXPANDED, action, NULL, n);
	return TRUE;
}

static void TvEnsureVisible(HWND hwnd, TVDATA *td, TVNODE *n)
{
	TVNODE *p;
	// Expand outermost first.
	for (;;) {
		TVNODE *hidden = NULL;
		for (p = n->parent; p != NULL; p = p->parent)
			if (!TvExpanded(p))
				hidden = p;
		if (hidden == NULL)
			break;
		if (!TvExpand(hwnd, td, hidden, TVE_EXPAND, FALSE))
			break;
		if (!TvExpanded(hidden))
			break;
	}
	TvScrollIntoView(hwnd, td, n);
}

// ---------------------------------------------------------------------------
// Insertion
// ---------------------------------------------------------------------------

static HTREEITEM TvInsert(HWND hwnd, TVDATA *td, const TV_INSERTSTRUCT *pis)
{
	TVNODE *n, *parent, *after, *first, *s;
	const TV_ITEM *pi;

	if (pis == NULL)
		return NULL;
	pi = &pis->item;
	parent = (pis->hParent == NULL || pis->hParent == TVI_ROOT) ? NULL : N(pis->hParent);

	n = (TVNODE *)Vc2Alloc(sizeof(TVNODE));
	if (n == NULL)
		return NULL;
	n->parent = parent;
	if ((pi->mask & TVIF_TEXT) && pi->pszText != NULL && pi->pszText != LPSTR_TEXTCALLBACK)
		n->text = Vc2StrDup(pi->pszText);
	if (pi->mask & TVIF_STATE)
		n->state = pi->state & pi->stateMask & ~(TVIS_SELECTED | TVIS_EXPANDED | TVIS_EXPANDEDONCE);
	if (pi->mask & TVIF_CHILDREN)
		n->cChildren = (pi->cChildren == I_CHILDRENCALLBACK) ? 1 : pi->cChildren;
	if (pi->mask & TVIF_PARAM)
		n->lParam = pi->lParam;
	if (pi->mask & TVIF_IMAGE)
		n->iImage = pi->iImage;
	if (pi->mask & TVIF_SELECTEDIMAGE)
		n->iSelImage = pi->iSelectedImage;

	first = (parent != NULL) ? parent->child : td->first;
	after = NULL;   // insert after this sibling; NULL = at the front
	if (pis->hInsertAfter == TVI_FIRST) {
		after = NULL;
	} else if (pis->hInsertAfter == TVI_SORT) {
		const char *t = n->text ? n->text : "";
		for (s = first; s != NULL; s = s->next) {
			if (_stricmp(s->text ? s->text : "", t) > 0)
				break;
			after = s;
		}
	} else if (pis->hInsertAfter == TVI_LAST || pis->hInsertAfter == NULL ||
			   N(pis->hInsertAfter)->parent != parent) {
		for (s = first; s != NULL; s = s->next)
			after = s;
	} else {
		after = N(pis->hInsertAfter);
	}

	if (after == NULL) {
		n->next = first;
		if (first != NULL)
			first->prev = n;
		if (parent != NULL)
			parent->child = n;
		else
			td->first = n;
	} else {
		n->prev = after;
		n->next = after->next;
		if (after->next != NULL)
			after->next->prev = n;
		after->next = n;
	}
	if (parent != NULL && TvExpanded(parent))
		TvChanged(hwnd, td);
	else if (parent == NULL)
		TvChanged(hwnd, td);
	else
		InvalidateRect(hwnd, NULL, TRUE);   // parent's button may appear
	return H(n);
}

static BOOL TvDelete(HWND hwnd, TVDATA *td, HTREEITEM h)
{
	TVNODE *n, *p;
	if (h == NULL || h == TVI_ROOT) {
		while (td->first != NULL) {
			n = td->first;
			TvUnlink(td, n);
			TvFreeSubtree(td, n);
		}
		td->sel = NULL;
		td->top = 0;
		TvChanged(hwnd, td);
		return TRUE;
	}
	n = N(h);
	p = n->parent;
	TvUnlink(td, n);
	TvFreeSubtree(td, n);
	if (p != NULL && p->child == NULL)
		p->state &= ~TVIS_EXPANDED;
	TvChanged(hwnd, td);
	return TRUE;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

static void TvDots(HDC hdc, int x0, int y0, int x1, int y1, COLORREF c)
{
	// Dotted 1-pixel line, horizontal or vertical.
	int x, y;
	if (y0 == y1) {
		for (x = x0; x < x1; x++)
			if (((x + y0) & 1) == 0)
				SetPixel(hdc, x, y0, c);
	} else {
		for (y = y0; y < y1; y++)
			if (((x0 + y) & 1) == 0)
				SetPixel(hdc, x0, y, c);
	}
}

static void TvPaint(HWND hwnd, TVDATA *td)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rcClient, rc;
	HFONT hOld;
	TVNODE *v, *a;
	DWORD style = GetWindowLong(hwnd, GWL_STYLE);
	BOOL lines = (style & TVS_HASLINES) ? TRUE : FALSE;
	BOOL buttons = (style & TVS_HASBUTTONS) ? TRUE : FALSE;
	BOOL showSel = td->focused || (style & TVS_SHOWSELALWAYS);
	COLORREF lineC = GetSysColor(COLOR_BTNSHADOW);
	int y, lvl, bx, cy, tx, per, row;

	GetClientRect(hwnd, &rcClient);
	Vc2FillRect(hdc, &rcClient, GetSysColor(COLOR_WINDOW));
	hOld = (HFONT)SelectObject(hdc, Vc2Font(td->font));
	SetBkMode(hdc, TRANSPARENT);

	per = TvPerPage(hwnd, td) + 1;
	v = TvNodeAt(td, td->top);
	for (row = 0, y = 0; v != NULL && row < per; v = TvNextVisible(v), row++, y += td->rowH) {
		lvl = TvLevel(hwnd, v);
		cy = y + td->rowH / 2;
		tx = lvl * td->indent + 2 - td->xOff;

		if (lvl > 0) {
			bx = (lvl - 1) * td->indent + td->indent / 2 - td->xOff;
			if (lines) {
				// This item's own connector.
				TvDots(hdc, bx, cy, tx - 1, cy, lineC);
				TvDots(hdc, bx, (v->prev != NULL || v->parent != NULL) ? y : cy,
					   bx, (v->next != NULL) ? y + td->rowH : cy + 1, lineC);
				// Lines continuing past this row for each ancestor.
				for (a = v->parent; a != NULL; a = a->parent) {
					int al = TvLevel(hwnd, a);
					if (al > 0 && a->next != NULL) {
						int ax = (al - 1) * td->indent + td->indent / 2 - td->xOff;
						TvDots(hdc, ax, y, ax, y + td->rowH, lineC);
					}
				}
			}
			if (buttons && TvHasButton(v)) {
				RECT rb;
				rb.left = bx - TV_BUTTON / 2;
				rb.top = cy - TV_BUTTON / 2;
				rb.right = rb.left + TV_BUTTON;
				rb.bottom = rb.top + TV_BUTTON;
				Vc2FillRect(hdc, &rb, GetSysColor(COLOR_WINDOW));
				Vc2Frame(hdc, &rb, lineC, lineC);
				Vc2Fill(hdc, bx - 2, cy, bx + 3, cy + 1, GetSysColor(COLOR_WINDOWTEXT));
				if (!TvExpanded(v))
					Vc2Fill(hdc, bx, cy - 2, bx + 1, cy + 3, GetSysColor(COLOR_WINDOWTEXT));
			}
		}

		{
			const char *t = v->text ? v->text : "";
			SIZE sz;
			BOOL sel = (v == td->sel) && showSel;
			sz.cx = 0;
			GetTextExtentPoint(hdc, t, strlen(t), &sz);
			rc.left = tx;
			rc.right = tx + sz.cx + 4;
			rc.top = y;
			rc.bottom = y + td->rowH;
			if (sel) {
				Vc2FillRect(hdc, &rc, GetSysColor(td->focused ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
				SetTextColor(hdc, GetSysColor(td->focused ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT));
			} else {
				SetTextColor(hdc, GetSysColor(IsWindowEnabled(hwnd) ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
			}
			ExtTextOut(hdc, rc.left + 2, y + (td->rowH - sz.cy) / 2, 0, NULL, t, strlen(t), NULL);
			if (v == td->sel && td->focused)
				Vc2DrawFocus(hdc, &rc);
		}
	}
	SelectObject(hdc, hOld);
	EndPaint(hwnd, &ps);
}

// ---------------------------------------------------------------------------
// Mouse helpers
// ---------------------------------------------------------------------------

#define TVHIT_NONE      0
#define TVHIT_BUTTON    1
#define TVHIT_LABEL     2
#define TVHIT_ROW       3

static TVNODE *TvHit(HWND hwnd, TVDATA *td, int x, int y, int *where)
{
	TVNODE *n;
	int lvl, bx, tx;
	*where = TVHIT_NONE;
	if (y < 0)
		return NULL;
	n = TvNodeAt(td, td->top + y / td->rowH);
	if (n == NULL)
		return NULL;
	lvl = TvLevel(hwnd, n);
	tx = lvl * td->indent + 2 - td->xOff;
	if (lvl > 0) {
		bx = (lvl - 1) * td->indent + td->indent / 2 - td->xOff;
		if (TvHasButton(n) && x >= bx - TV_BUTTON / 2 - 2 && x <= bx + TV_BUTTON / 2 + 2) {
			*where = TVHIT_BUTTON;
			return n;
		}
	}
	*where = (x >= tx) ? TVHIT_LABEL : TVHIT_ROW;
	return n;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK TvWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TVDATA *td = (TVDATA *)Vc2GetData(hwnd);

	if (td == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		td = (TVDATA *)Vc2Alloc(sizeof(TVDATA));
		if (td == NULL)
			return FALSE;
		Vc2SetData(hwnd, td);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		TvMetrics(hwnd, td);
		TvUpdateScroll(hwnd, td);
		return 0;

	case WM_NCDESTROY:
		while (td->first != NULL) {
			TVNODE *n = td->first;
			TvUnlink(td, n);
			TvFreeSubtree(td, n);
		}
		Vc2Free(td);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_SETFONT:
		td->font = (HFONT)wParam;
		TvMetrics(hwnd, td);
		TvUpdateScroll(hwnd, td);
		if (LOWORD(lParam))
			InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETFONT:
		return (LRESULT)td->font;

	case WM_SIZE:
		TvChanged(hwnd, td);
		return 0;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		TvPaint(hwnd, td);
		return 0;

	case WM_ENABLE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_GETDLGCODE:
		return DLGC_WANTARROWS | DLGC_WANTCHARS;

	case WM_SETFOCUS:
		td->focused = TRUE;
		InvalidateRect(hwnd, NULL, TRUE);
		Vc2NotifySimple(hwnd, NM_SETFOCUS);
		return 0;

	case WM_KILLFOCUS:
		td->focused = FALSE;
		InvalidateRect(hwnd, NULL, TRUE);
		Vc2NotifySimple(hwnd, NM_KILLFOCUS);
		return 0;

	case WM_VSCROLL:
	case WM_HSCROLL:
		{
			BOOL vert = (msg == WM_VSCROLL);
			RECT rc;
			int v = vert ? td->top : td->xOff;
			int page;
			GetClientRect(hwnd, &rc);
			page = vert ? TvPerPage(hwnd, td) : (rc.right > 16 ? rc.right - 16 : 8);
			switch (LOWORD(wParam)) {
			case SB_LINEUP:         v -= vert ? 1 : 8; break;
			case SB_LINEDOWN:       v += vert ? 1 : 8; break;
			case SB_PAGEUP:         v -= page; break;
			case SB_PAGEDOWN:       v += page; break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:  v = (short)HIWORD(wParam); break;
			case SB_TOP:            v = 0; break;
			case SB_BOTTOM:         v = 0x7FFF; break;
			default:                return 0;
			}
			if (vert) td->top = v; else td->xOff = v;
			TvChanged(hwnd, td);
		}
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
		{
			int where;
			TVNODE *n;
			if (td->busy)
				return 0;
			if (GetFocus() != hwnd)
				SetFocus(hwnd);
			n = TvHit(hwnd, td, (short)LOWORD(lParam), (short)HIWORD(lParam), &where);
			if (n == NULL)
				return 0;
			if (msg == WM_RBUTTONDOWN) {
				Vc2NotifySimple(hwnd, NM_RCLICK);
				return 0;
			}
			if (where == TVHIT_BUTTON) {
				TvExpand(hwnd, td, n, TVE_TOGGLE, TRUE);
				return 0;
			}
			TvSelect(hwnd, td, n, TVC_BYMOUSE);
			if (msg == WM_LBUTTONDBLCLK) {
				// The default action, unless the parent handles it.
				if (Vc2NotifySimple(hwnd, NM_DBLCLK) == 0 && td->sel == n &&
					TvHasButton(n))
					TvExpand(hwnd, td, n, TVE_TOGGLE, TRUE);
			} else {
				Vc2NotifySimple(hwnd, NM_CLICK);
			}
		}
		return 0;

	case WM_KEYDOWN:
		{
			TVNODE *s = td->sel, *to = NULL;
			int i, per;
			if (td->busy)
				return 0;
			if (s == NULL) {
				if (td->first != NULL)
					TvSelect(hwnd, td, td->first, TVC_BYKEYBOARD);
				return 0;
			}
			switch (wParam) {
			case VK_UP:
				to = TvPrevVisible(s);
				break;
			case VK_DOWN:
				to = TvNextVisible(s);
				break;
			case VK_HOME:
				to = td->first;
				break;
			case VK_END:
				to = TvNodeAt(td, TvVisibleCount(td) - 1);
				break;
			case VK_PRIOR:
			case VK_NEXT:
				per = TvPerPage(hwnd, td) - 1;
				if (per < 1) per = 1;
				to = s;
				for (i = 0; i < per; i++) {
					TVNODE *t = (wParam == VK_PRIOR) ? TvPrevVisible(to) : TvNextVisible(to);
					if (t == NULL)
						break;
					to = t;
				}
				break;
			case VK_LEFT:
				if (TvExpanded(s))
					TvExpand(hwnd, td, s, TVE_COLLAPSE, TRUE);
				else
					to = s->parent;
				break;
			case VK_RIGHT:
				if (!TvExpanded(s) && TvHasButton(s))
					TvExpand(hwnd, td, s, TVE_EXPAND, TRUE);
				else
					to = s->child;
				break;
			case VK_ADD:
				if (!TvExpanded(s) && TvHasButton(s))
					TvExpand(hwnd, td, s, TVE_EXPAND, TRUE);
				break;
			case VK_SUBTRACT:
				if (TvExpanded(s))
					TvExpand(hwnd, td, s, TVE_COLLAPSE, TRUE);
				break;
			case VK_RETURN:
				Vc2NotifySimple(hwnd, NM_RETURN);
				break;
			}
			if (to != NULL)
				TvSelect(hwnd, td, to, TVC_BYKEYBOARD);
		}
		return 0;

	case TVM_INSERTITEMA:
		return (LRESULT)TvInsert(hwnd, td, (const TV_INSERTSTRUCT *)lParam);

	case TVM_DELETEITEM:
		return TvDelete(hwnd, td, (HTREEITEM)lParam);

	case TVM_GETCOUNT:
		return TvCountAll(td->first);

	case TVM_EXPAND:
		return TvExpand(hwnd, td, N(lParam), (UINT)wParam, FALSE);

	case TVM_GETNEXTITEM:
		{
			TVNODE *n = ((HTREEITEM)lParam == TVI_ROOT) ? NULL : N(lParam);
			switch (wParam) {
			case TVGN_ROOT:             return (LRESULT)td->first;
			case TVGN_CARET:            return (LRESULT)td->sel;
			case TVGN_FIRSTVISIBLE:     return (LRESULT)TvNodeAt(td, td->top);
			case TVGN_DROPHILITE:       return 0;
			case TVGN_CHILD:
				return (LRESULT)((n == NULL) ? td->first : n->child);
			}
			if (n == NULL)
				return 0;
			switch (wParam) {
			case TVGN_NEXT:             return (LRESULT)n->next;
			case TVGN_PREVIOUS:         return (LRESULT)n->prev;
			case TVGN_PARENT:           return (LRESULT)n->parent;
			case TVGN_NEXTVISIBLE:
				return TvIsShown(n) ? (LRESULT)TvNextVisible(n) : 0;
			case TVGN_PREVIOUSVISIBLE:
				return TvIsShown(n) ? (LRESULT)TvPrevVisible(n) : 0;
			}
			return 0;
		}

	case TVM_GETITEMA:
		{
			TV_ITEM *pi = (TV_ITEM *)lParam;
			TVNODE *n;
			if (pi == NULL || pi->hItem == NULL)
				return FALSE;
			n = N(pi->hItem);
			if (pi->mask & TVIF_TEXT)
				Vc2CopyText(pi->pszText, pi->cchTextMax, n->text);
			if (pi->mask & TVIF_STATE)
				pi->state = n->state & (pi->stateMask ? pi->stateMask : 0xFFFF);
			if (pi->mask & TVIF_PARAM)
				pi->lParam = n->lParam;
			if (pi->mask & TVIF_CHILDREN)
				pi->cChildren = TvHasButton(n) ? 1 : 0;
			if (pi->mask & TVIF_IMAGE)
				pi->iImage = n->iImage;
			if (pi->mask & TVIF_SELECTEDIMAGE)
				pi->iSelectedImage = n->iSelImage;
			return TRUE;
		}

	case TVM_SETITEMA:
		{
			const TV_ITEM *pi = (const TV_ITEM *)lParam;
			TVNODE *n;
			if (pi == NULL || pi->hItem == NULL)
				return FALSE;
			n = N(pi->hItem);
			if ((pi->mask & TVIF_TEXT) && pi->pszText != LPSTR_TEXTCALLBACK) {
				Vc2Free(n->text);
				n->text = Vc2StrDup(pi->pszText);
			}
			if (pi->mask & TVIF_STATE) {
				UINT m = pi->stateMask & ~(TVIS_SELECTED | TVIS_EXPANDED | TVIS_EXPANDEDONCE);
				n->state = (n->state & ~m) | (pi->state & m);
			}
			if (pi->mask & TVIF_PARAM)
				n->lParam = pi->lParam;
			if (pi->mask & TVIF_CHILDREN)
				n->cChildren = (pi->cChildren == I_CHILDRENCALLBACK) ? 1 : pi->cChildren;
			if (pi->mask & TVIF_IMAGE)
				n->iImage = pi->iImage;
			if (pi->mask & TVIF_SELECTEDIMAGE)
				n->iSelImage = pi->iSelectedImage;
			TvChanged(hwnd, td);
			return TRUE;
		}

	case TVM_SELECTITEM:
		if (wParam == TVGN_CARET) {
			TVNODE *n = N(lParam);
			if (n != NULL)
				TvEnsureVisible(hwnd, td, n);
			return TvSelect(hwnd, td, n, TVC_UNKNOWN);
		}
		return FALSE;

	case TVM_GETITEMRECT:
		{
			// lParam points to a RECT whose first bytes hold the HTREEITEM.
			RECT *prc = (RECT *)lParam;
			TVNODE *n;
			int i;
			RECT rcClient;
			if (prc == NULL)
				return FALSE;
			n = *(TVNODE **)prc;
			if (n == NULL || !TvIsShown(n))
				return FALSE;
			i = TvIndexOf(td, n);
			if (i < 0)
				return FALSE;
			GetClientRect(hwnd, &rcClient);
			prc->top = (i - td->top) * td->rowH;
			prc->bottom = prc->top + td->rowH;
			if (wParam) {
				HDC hdc = GetDC(hwnd);
				HFONT hOld;
				int w = 0;
				if (hdc != NULL) {
					hOld = (HFONT)SelectObject(hdc, Vc2Font(td->font));
					w = TvTextWidth(hwnd, td, hdc, n);
					SelectObject(hdc, hOld);
					ReleaseDC(hwnd, hdc);
				}
				prc->left = TvLevel(hwnd, n) * td->indent + 2 - td->xOff;
				prc->right = prc->left + w;
			} else {
				prc->left = 0;
				prc->right = rcClient.right;
			}
			return TRUE;
		}

	case TVM_ENSUREVISIBLE:
		if (lParam == 0)
			return FALSE;
		TvEnsureVisible(hwnd, td, N(lParam));
		return TRUE;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterTreeView(void)
{
	return Vc2RegisterClass("SysTreeView32", (WNDPROC)TvWndProc,
							CS_DBLCLKS, NULL, -1);
}
