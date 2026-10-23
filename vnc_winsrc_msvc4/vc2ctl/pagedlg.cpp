// pagedlg.cpp - keyboard navigation for dialogs with child-dialog tab pages
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// The viewer Options and WinVNC Properties dialogs show each tab page as a
// child dialog.  IsDialogMessage only walks a page's controls if the page has
// WS_EX_CONTROLPARENT, which NT 4 / Windows 95 introduced: on NT 3.x and
// Win32s the pages cannot be reached from the keyboard at all.
//
// While such a dialog runs its modal loop, a WH_MSGFILTER hook (MSGF_DIALOGBOX)
// sees each message before IsDialogMessage does, and:
//   * Tab / Shift+Tab: moves through the WS_TABSTOP controls of the dialog
//     and of the visible page, in order, as if they were one dialog;
//   * arrow keys while the focus is in the page: handled by the page's own
//     IsDialogMessage (radio groups, the controls that want arrows);
//   * Ctrl+Tab / Ctrl+Shift+Tab: switches tabs, with the usual TCN_SELCHANGING
//     / TCN_SELCHANGE notifications to the dialog.
// Enter and Esc are left alone, so the dialog's OK / Cancel still work.
//
// The hook is removed by Vc2CtlEndPagedDialog (never from inside the hook);
// a dialog that is destroyed without it is simply forgotten.

#include "vc2ctli.h"

#define PD_MAX_DIALOGS  2
#define PD_MAX_PAGES    8
#define PD_MAX_CTLS     128

typedef struct {
	HWND hDlg;
	HWND hTab;
	HWND pages[PD_MAX_PAGES];
	int nPages;
} PAGEDDLG;

static PAGEDDLG g_pd[PD_MAX_DIALOGS];
static HHOOK g_pdHook = NULL;

BOOL Vc2CtlNeedPagedDialogHelper(void)
{
	// Major version below 4: NT 3.x, and Win32s (which reports Windows 3.1).
	// Windows 95 reports 4.
	return ((GetVersion() & 0xFF) < 4) ? TRUE : FALSE;
}

static int PdActiveCount(void)
{
	int i, n = 0;
	for (i = 0; i < PD_MAX_DIALOGS; i++)
		if (g_pd[i].hDlg != NULL)
			n++;
	return n;
}

static void PdUnhookIfIdle(void)
{
	if (g_pdHook != NULL && PdActiveCount() == 0) {
		UnhookWindowsHookEx(g_pdHook);
		g_pdHook = NULL;
	}
}

static PAGEDDLG *PdFind(HWND hwnd)
{
	int i;
	for (i = 0; i < PD_MAX_DIALOGS; i++) {
		HWND h = g_pd[i].hDlg;
		if (h == NULL)
			continue;
		if (!IsWindow(h)) {
			g_pd[i].hDlg = NULL;    // dialog gone: forget it
			continue;
		}
		if (hwnd == h || IsChild(h, hwnd))
			return &g_pd[i];
	}
	return NULL;
}

static HWND PdVisiblePage(PAGEDDLG *pd)
{
	int i;
	for (i = 0; i < pd->nPages; i++)
		if (pd->pages[i] != NULL && IsWindow(pd->pages[i]) && IsWindowVisible(pd->pages[i]))
			return pd->pages[i];
	return NULL;
}

static BOOL PdIsPage(PAGEDDLG *pd, HWND h)
{
	int i;
	for (i = 0; i < pd->nPages; i++)
		if (pd->pages[i] == h)
			return TRUE;
	return FALSE;
}

static BOOL PdTabStop(HWND h)
{
	DWORD st = GetWindowLong(h, GWL_STYLE);
	return ((st & WS_TABSTOP) && (st & WS_VISIBLE) && !(st & WS_DISABLED)) ? TRUE : FALSE;
}

// Tab order: the dialog's children in Z order, with the visible page's
// children spliced in where the page window is.
static int PdCollect(PAGEDDLG *pd, HWND page, HWND *out)
{
	int n = 0;
	HWND h, c;
	for (h = GetWindow(pd->hDlg, GW_CHILD); h != NULL && n < PD_MAX_CTLS;
		 h = GetWindow(h, GW_HWNDNEXT)) {
		if (PdIsPage(pd, h)) {
			if (h != page)
				continue;
			for (c = GetWindow(h, GW_CHILD); c != NULL && n < PD_MAX_CTLS;
				 c = GetWindow(c, GW_HWNDNEXT))
				if (PdTabStop(c))
					out[n++] = c;
			continue;
		}
		if (PdTabStop(h))
			out[n++] = h;
	}
	return n;
}

static void PdFocus(HWND h)
{
	char cls[16];
	SetFocus(h);
	cls[0] = '\0';
	GetClassName(h, cls, sizeof(cls));
	if (_stricmp(cls, "Edit") == 0)
		SendMessage(h, EM_SETSEL, 0, -1);
}

static BOOL PdTab(PAGEDDLG *pd, HWND focus, BOOL back)
{
	HWND list[PD_MAX_CTLS];
	HWND page = PdVisiblePage(pd);
	int n = PdCollect(pd, page, list);
	int i, cur = -1;
	if (n == 0)
		return FALSE;
	for (i = 0; i < n; i++) {
		if (list[i] == focus || IsChild(list[i], focus)) {
			cur = i;
			break;
		}
	}
	if (cur < 0)
		i = back ? n - 1 : 0;
	else
		i = back ? (cur + n - 1) % n : (cur + 1) % n;
	PdFocus(list[i]);
	return TRUE;
}

static BOOL PdSwitchTab(PAGEDDLG *pd, BOOL back)
{
	NMHDR nm;
	int count, cur, next;
	if (pd->hTab == NULL || !IsWindow(pd->hTab))
		return FALSE;
	count = (int)SendMessage(pd->hTab, TCM_GETITEMCOUNT, 0, 0);
	cur = (int)SendMessage(pd->hTab, TCM_GETCURSEL, 0, 0);
	if (count < 2)
		return TRUE;
	next = back ? (cur + count - 1) % count : (cur + 1) % count;
	nm.hwndFrom = pd->hTab;
	nm.idFrom = (UINT)GetWindowLong(pd->hTab, GWL_ID);
	nm.code = TCN_SELCHANGING;
	if (SendMessage(pd->hDlg, WM_NOTIFY, nm.idFrom, (LPARAM)&nm) != 0)
		return TRUE;
	SendMessage(pd->hTab, TCM_SETCURSEL, next, 0);
	nm.code = TCN_SELCHANGE;
	SendMessage(pd->hDlg, WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
	return TRUE;
}

static LRESULT CALLBACK PdHookProc(int code, WPARAM wParam, LPARAM lParam)
{
	MSG *pm = (MSG *)lParam;
	if (code == MSGF_DIALOGBOX && pm != NULL &&
		(pm->message == WM_KEYDOWN || pm->message == WM_CHAR)) {
		PAGEDDLG *pd = PdFind(pm->hwnd);
		if (pd != NULL) {
			BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) ? TRUE : FALSE;
			BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) ? TRUE : FALSE;
			BOOL alt = (GetKeyState(VK_MENU) & 0x8000) ? TRUE : FALSE;
			HWND page = PdVisiblePage(pd);
			if (pm->message == WM_KEYDOWN && pm->wParam == VK_TAB && !alt) {
				if (ctrl) {
					if (PdSwitchTab(pd, shift))
						return TRUE;
				} else if (PdTab(pd, GetFocus(), shift)) {
					return TRUE;
				}
			} else if (pm->message == WM_CHAR && pm->wParam == '\t') {
				return TRUE;    // the Tab was handled on WM_KEYDOWN
			} else if (pm->message == WM_KEYDOWN && page != NULL &&
					   (pm->wParam == VK_LEFT || pm->wParam == VK_RIGHT ||
						pm->wParam == VK_UP || pm->wParam == VK_DOWN) &&
					   IsChild(page, pm->hwnd)) {
				if (IsDialogMessage(page, pm))
					return TRUE;
			}
		}
	}
	return CallNextHookEx(g_pdHook, code, wParam, lParam);
}

BOOL Vc2CtlBeginPagedDialog(HWND hDlg, HWND hTab, const HWND *pages, int nPages)
{
	int i, slot = -1;
	if (!Vc2CtlNeedPagedDialogHelper() || hDlg == NULL || pages == NULL || nPages <= 0)
		return FALSE;
	PdFind(NULL);   // drop dialogs that have gone
	for (i = 0; i < PD_MAX_DIALOGS; i++) {
		if (g_pd[i].hDlg == hDlg || (slot < 0 && g_pd[i].hDlg == NULL))
			slot = i;
	}
	if (slot < 0)
		return FALSE;
	if (nPages > PD_MAX_PAGES)
		nPages = PD_MAX_PAGES;
	memset(&g_pd[slot], 0, sizeof(PAGEDDLG));
	g_pd[slot].hDlg = hDlg;
	g_pd[slot].hTab = hTab;
	for (i = 0; i < nPages; i++)
		g_pd[slot].pages[i] = pages[i];
	g_pd[slot].nPages = nPages;
	if (g_pdHook == NULL) {
		g_pdHook = SetWindowsHookEx(WH_MSGFILTER, (HOOKPROC)PdHookProc,
									NULL, GetCurrentThreadId());
		if (g_pdHook == NULL) {
			g_pd[slot].hDlg = NULL;
			return FALSE;
		}
	}
	return TRUE;
}

void Vc2CtlEndPagedDialog(HWND hDlg)
{
	int i;
	for (i = 0; i < PD_MAX_DIALOGS; i++)
		if (g_pd[i].hDlg == hDlg)
			g_pd[i].hDlg = NULL;
	PdUnhookIfIdle();
}
