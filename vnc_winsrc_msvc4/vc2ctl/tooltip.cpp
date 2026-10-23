// tooltip.cpp - the small popup the built-in toolbar uses for its tooltips
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Not a tooltips_class32 replacement: the toolbar drives it directly with
// Vc2TipShow / Vc2TipHide.  The colours are fixed (black on pale yellow),
// because COLOR_INFOBK / COLOR_INFOTEXT do not exist before Windows 95.

#include "vc2ctli.h"

#define TIP_BK      RGB(255, 255, 225)
#define TIP_TEXT    RGB(0, 0, 0)

static LRESULT CALLBACK TipWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			RECT rc;
			char text[256];
			HFONT hOld;
			GetClientRect(hwnd, &rc);
			Vc2FillRect(hdc, &rc, TIP_BK);
			Vc2Frame(hdc, &rc, TIP_TEXT, TIP_TEXT);
			GetWindowText(hwnd, text, sizeof(text));
			hOld = (HFONT)SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, TIP_TEXT);
			DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
			SelectObject(hdc, hOld);
			EndPaint(hwnd, &ps);
		}
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterToolTip(void)
{
	return Vc2RegisterClass(VC2_TOOLTIP_CLASS, (WNDPROC)TipWndProc, 0, NULL, -1);
}

HWND Vc2TipCreate(HWND hwndOwner)
{
	// Owned popup, so it stays above the viewer window and goes with it.
	return CreateWindowEx(WS_EX_TOPMOST, VC2_TOOLTIP_CLASS, "",
						  WS_POPUP | WS_DISABLED,
						  0, 0, 0, 0, hwndOwner, NULL, g_vc2ctlInst, NULL);
}

void Vc2TipShow(HWND hwndTip, const char *text, int x, int y)
{
	HDC hdc;
	HFONT hOld;
	SIZE sz;
	int w, h, sw, sh;
	if (hwndTip == NULL || text == NULL || text[0] == '\0') {
		Vc2TipHide(hwndTip);
		return;
	}
	SetWindowText(hwndTip, text);
	sz.cx = 0;
	sz.cy = 12;
	hdc = GetDC(hwndTip);
	if (hdc != NULL) {
		hOld = (HFONT)SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));
		GetTextExtentPoint(hdc, text, strlen(text), &sz);
		SelectObject(hdc, hOld);
		ReleaseDC(hwndTip, hdc);
	}
	w = sz.cx + 8;
	h = sz.cy + 4;
	sw = GetSystemMetrics(SM_CXSCREEN);
	sh = GetSystemMetrics(SM_CYSCREEN);
	if (x + w > sw) x = sw - w;
	if (x < 0) x = 0;
	if (y + h > sh) y = y - h - 24;
	if (y < 0) y = 0;
	SetWindowPos(hwndTip, HWND_TOPMOST, x, y, w, h,
				 SWP_NOACTIVATE | SWP_SHOWWINDOW);
	InvalidateRect(hwndTip, NULL, TRUE);
	UpdateWindow(hwndTip);
}

void Vc2TipHide(HWND hwndTip)
{
	if (hwndTip != NULL && IsWindowVisible(hwndTip))
		ShowWindow(hwndTip, SW_HIDE);
}
