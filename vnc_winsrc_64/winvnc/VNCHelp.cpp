// Copyright (C) 2003 TightVNC Development Team. All Rights Reserved.
//
//  TightVNC is free software; you can redistribute it and/or modify
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
// TightVNC homepage on the Web: http://www.tightvnc.com/

// VNCHelp.cpp: implementation of the VNCHelp class.
//
// The help for a dialog control is the string resource with the control's
// ID; HelpStringId() maps the labels to the control they describe.  How it is
// shown depends on the help system chosen in VNCHelp.h.

#include "stdhdrs.h"
#ifdef VNCHELP_HTML
#include "HtmlHelp.h"
#endif
#include "WinVNC.h"
#include "VNCHelp.h"

// The instance that holds the help strings.
#define HELP_INSTANCE	hAppInstance

// Names newer than some of the headers this file is compiled with.
#ifndef WM_HELP
#define WM_HELP				0x0053
#endif
#ifndef WS_EX_CONTEXTHELP
#define WS_EX_CONTEXTHELP	0x00000400L
#endif

static UINT HelpStringId(int ctrlId)
{
	switch (ctrlId) {
	case IDC_MAIN_LABEL:
		return IDC_PORTRFB;
	case IDC_HTTP_LABEL:
		return IDC_PORTHTTP;
	case IDC_STATIC_AND:
		return IDC_SPECPORT;
	case IDC_PASSWORD_LABEL:
		return IDC_PASSWORD;
	case IDC_PASSWORD_VIEWONLY_LABEL:
		return IDC_PASSWORD_VIEWONLY;
	case IDC_TIMEOUT_LABEL:
	case IDC_SECONDS_LABEL:
		return IDC_DISABLE_TIME;
	case IDC_STATIC_TIMEOUT:
	case IDC_STATIC_SECONDS:
		return IDQUERYTIMEOUT;
	case IDC_STATIC_PCYCLE:
	case IDC_STATIC_MS:
	case IDC_POLCYCLMS_LABEL:
		return IDC_POLLING_CYCLE;
	case IDC_STATIC_DRVSTATUS:
		return IDC_STATIC_DRVINFO;
	case IDC_LIVESHARE_LABEL:
		return IDC_LIVESHARE;
	}
	return (UINT)ctrlId;
}

// TRUE on Windows 95, NT 4.0 and later, which have the COLOR_INFO* colours.
// NT 3.x and Win32s report a major version of 3.
static BOOL HelpNewShell()
{
	return LOBYTE(LOWORD(GetVersion())) >= 4;
}

// TRUE where the system draws the caption "?" button and sends WM_HELP for
// it and for F1: NT 3.51 already does, besides the systems of HelpNewShell.
// NT 3.1, NT 3.5 and Win32s (Windows 3.1, minor version 10 or 11) do not.
static BOOL HelpHasContextButton()
{
	DWORD ver = GetVersion();
	BYTE major = LOBYTE(LOWORD(ver));
	BYTE minor = HIBYTE(LOWORD(ver));

	return major > 3 || (major == 3 && minor >= 51);
}

// Adds or removes the caption "?" button of a dialog made from a DIALOG
// template (or a DIALOGEX one), on the systems that have it.
static void HelpSetContextButton(HWND hwnd, BOOL on)
{
	LONG exstyle;
	LONG newstyle;

	if (!HelpHasContextButton())
		return;
	exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
	if (on)
		newstyle = exstyle | WS_EX_CONTEXTHELP;
	else
		newstyle = exstyle & ~WS_EX_CONTEXTHELP;
	if (newstyle != exstyle) {
		SetWindowLong(hwnd, GWL_EXSTYLE, newstyle);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
					 SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}
}

#ifdef VNCHELP_HTML

void VNCHelp::Popup(LPARAM lParam)
{
	LPHELPINFO hlp = (LPHELPINFO) lParam;
	HH_POPUP popup;

	if (hlp != NULL && hlp->iCtrlId != 0) {

		popup.cbStruct = sizeof(popup);
		popup.hinst = HELP_INSTANCE;
		popup.idString = HelpStringId(hlp->iCtrlId);
		SetRect(&popup.rcMargins, -1, -1, -1, -1);
		popup.pszFont = "MS Sans Serif, 8, , ";
		popup.clrForeground = -1;
		popup.clrBackground = -1;
		popup.pt.x = -1;
		popup.pt.y = -1;

		// DWORD_PTR, not DWORD as in the 32-bit tree: a DWORD would cut
		// the pointers in half on Win64.
		HtmlHelp((HWND)hlp->hItemHandle,
				 NULL,
				 HH_DISPLAY_TEXT_POPUP,
				 (DWORD_PTR)&popup);
	}
}

void VNCHelp::InitDialog(HWND hwnd)
{
	HelpSetContextButton(hwnd, TRUE);
}

#endif // VNCHELP_HTML

#ifdef VNCHELP_NONE

void VNCHelp::Popup(LPARAM lParam)
{
}

void VNCHelp::InitDialog(HWND hwnd)
{
	// No help to show, so no "?" button that leads nowhere.
	HelpSetContextButton(hwnd, FALSE);
}

#endif // VNCHELP_NONE

#ifdef VNCHELP_MINI

// Built-in replacement for the HtmlHelp text popups.
//
// Only USER and GDI calls that NT 3.1 and Win32s 1.15a have are used, so the
// same code serves every tree.  The popup is a bordered window with the text
// in MS Sans Serif 8, like the HtmlHelp one.  It is shown without taking the
// focus, and a local message loop closes it on the next key or mouse click
// (which is swallowed) or when the application loses the capture or the
// activation, as WinHelp popups do.
//
// NT 3.1, NT 3.5 and Win32s send no WM_HELP and have no caption "?" button,
// and this popup does not show there; see "The help popup of NT 3.1, NT 3.5
// and Win32s" and "F1 and the drawn "?" button" below.

#ifndef HELPINFO_WINDOW
#define HELPINFO_WINDOW		0x0001
#endif
#ifndef COLOR_INFOTEXT
#define COLOR_INFOTEXT		23
#endif
#ifndef COLOR_INFOBK
#define COLOR_INFOBK		24
#endif
#ifndef WM_CAPTURECHANGED
#define WM_CAPTURECHANGED	0x0215
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL		0x020A
#endif

#define HELP_POPUP_CLASS	"VNCHelpPopup"
#define HELP_MARGIN_X		6
#define HELP_MARGIN_Y		4
#define HELP_SM_CMONITORS			80
#define HELP_SM_XVIRTUALSCREEN		76
#define HELP_SM_YVIRTUALSCREEN		77
#define HELP_SM_CXVIRTUALSCREEN		78
#define HELP_SM_CYVIRTUALSCREEN		79

static char helpText[1024];
static HFONT helpFont = NULL;
static HWND helpPopupWnd = NULL;
static BOOL helpPopupDone;

static void HelpColours(COLORREF *bk, COLORREF *fg)
{
	if (HelpNewShell()) {
		*bk = GetSysColor(COLOR_INFOBK);
		*fg = GetSysColor(COLOR_INFOTEXT);
	} else {
		*bk = RGB(255, 255, 225);
		*fg = RGB(0, 0, 0);
	}
}

static LRESULT CALLBACK HelpPopupProc(HWND hwnd, UINT msg,
									  WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			RECT rc;
			COLORREF bk, fg;
			HBRUSH brush;
			HGDIOBJ oldFont;

			HelpColours(&bk, &fg);
			GetClientRect(hwnd, &rc);
			brush = CreateSolidBrush(bk);
			FillRect(hdc, &rc, brush);
			DeleteObject(brush);

			InflateRect(&rc, -HELP_MARGIN_X, -HELP_MARGIN_Y);
			oldFont = SelectObject(hdc, helpFont);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, fg);
			DrawText(hdc, helpText, -1, &rc,
					 DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
			SelectObject(hdc, oldFont);
			EndPaint(hwnd, &ps);
			return 0;
		}
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_ACTIVATEAPP:
		if (wParam)
			break;
		// fall through
	case WM_CANCELMODE:
	case WM_CAPTURECHANGED:
		// These are sent, not posted: wake the loop in HelpShowPopup.
		helpPopupDone = TRUE;
		PostMessage(hwnd, WM_NULL, 0, 0);
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static BOOL HelpRegisterPopupClass()
{
	static BOOL registered = FALSE;
	WNDCLASS wndclass;

	if (registered)
		return TRUE;

	memset(&wndclass, 0, sizeof(wndclass));
	wndclass.style			= CS_SAVEBITS;
	wndclass.lpfnWndProc	= HelpPopupProc;
	wndclass.hInstance		= HELP_INSTANCE;
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= NULL;
	wndclass.lpszClassName	= HELP_POPUP_CLASS;
	registered = (RegisterClass(&wndclass) != 0);
	return registered;
}

// The messages that close the popup.  They are swallowed.
static BOOL HelpIsDismissMessage(UINT msg)
{
	switch (msg) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDBLCLK:
	case WM_NCLBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
	case WM_NCMBUTTONDOWN:
	case WM_MOUSEWHEEL:
		return TRUE;
	}
	return FALSE;
}

// The top-level window that owns a control.
static HWND HelpTopLevel(HWND hwnd)
{
	while (hwnd != NULL && (GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD))
		hwnd = GetParent(hwnd);
	return hwnd;
}

// Shows helpText under the item (or at the mouse, if the mouse is on the
// item) until the user dismisses it.
static void HelpShowPopup(HWND item, POINT mouse)
{
	HWND owner = NULL;
	HWND popup;
	HDC hdc;
	HGDIOBJ oldFont;
	RECT rc, itemRect, screen;
	BOOL haveItem;
	int maxWidth, cx, cy, x, y;
	MSG msg;

	if (!HelpRegisterPopupClass())
		return;

	haveItem = (item != NULL && IsWindow(item));
	if (haveItem) {
		GetWindowRect(item, &itemRect);
		owner = HelpTopLevel(item);
	}
	// An unowned popup would get a taskbar button.
	if (owner == NULL)
		owner = GetActiveWindow();

	screen.left = 0;
	screen.top = 0;
	screen.right = GetSystemMetrics(SM_CXSCREEN);
	screen.bottom = GetSystemMetrics(SM_CYSCREEN);
	// Systems before Windows 98 / 2000 return 0 for SM_CMONITORS.
	if (GetSystemMetrics(HELP_SM_CMONITORS) > 1) {
		screen.left = GetSystemMetrics(HELP_SM_XVIRTUALSCREEN);
		screen.top = GetSystemMetrics(HELP_SM_YVIRTUALSCREEN);
		screen.right = screen.left + GetSystemMetrics(HELP_SM_CXVIRTUALSCREEN);
		screen.bottom = screen.top + GetSystemMetrics(HELP_SM_CYVIRTUALSCREEN);
	}

	// Measure the text in MS Sans Serif 8, wrapped at a third of the screen.
	hdc = GetDC(NULL);
	helpFont = CreateFont(-MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72),
						  0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
						  ANSI_CHARSET, OUT_DEFAULT_PRECIS,
						  CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
						  VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
	if (helpFont == NULL)
		helpFont = (HFONT)GetStockObject(ANSI_VAR_FONT);
	oldFont = SelectObject(hdc, helpFont);
	maxWidth = GetSystemMetrics(SM_CXSCREEN) / 3;
	if (maxWidth < 240)
		maxWidth = 240;
	SetRect(&rc, 0, 0, maxWidth, 0);
	DrawText(hdc, helpText, -1, &rc,
			 DT_CALCRECT | DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
	SelectObject(hdc, oldFont);
	ReleaseDC(NULL, hdc);

	cx = (rc.right - rc.left) + 2 * HELP_MARGIN_X +
		 2 * GetSystemMetrics(SM_CXBORDER);
	cy = (rc.bottom - rc.top) + 2 * HELP_MARGIN_Y +
		 2 * GetSystemMetrics(SM_CYBORDER);

	// Centred under the item, or just below the mouse when the user pointed
	// at the item (the "?" button, What's This?).
	if (haveItem && !PtInRect(&itemRect, mouse)) {
		x = (itemRect.left + itemRect.right) / 2 - cx / 2;
		y = itemRect.bottom + 2;
	} else {
		x = mouse.x - cx / 2;
		y = mouse.y + GetSystemMetrics(SM_CYCURSOR) / 2;
	}
	if (y + cy > screen.bottom) {
		if (haveItem && !PtInRect(&itemRect, mouse))
			y = itemRect.top - 2 - cy;
		else
			y = mouse.y - 2 - cy;
	}
	if (x + cx > screen.right)
		x = screen.right - cx;
	if (x < screen.left)
		x = screen.left;
	if (y < screen.top)
		y = screen.top;

	popup = CreateWindowEx(0, HELP_POPUP_CLASS, "", WS_POPUP | WS_BORDER,
						   x, y, cx, cy, owner, NULL, HELP_INSTANCE, NULL);
	if (popup != NULL) {
		helpPopupWnd = popup;
		helpPopupDone = FALSE;
		SetWindowPos(popup, HWND_TOP, 0, 0, 0, 0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE |
					 SWP_SHOWWINDOW);
		UpdateWindow(popup);
		SetCapture(popup);

		while (!helpPopupDone) {
			if (!GetMessage(&msg, NULL, 0, 0)) {
				// Leave WM_QUIT for the application's own loop.
				PostQuitMessage((int)msg.wParam);
				break;
			}
			if (HelpIsDismissMessage(msg.message))
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (GetCapture() != popup)
				break;
		}

		if (GetCapture() == popup)
			ReleaseCapture();
		DestroyWindow(popup);
		helpPopupWnd = NULL;
	}

	DeleteObject(helpFont);
	helpFont = NULL;
}

void VNCHelp::Popup(LPARAM lParam)
{
	LPHELPINFO hlp = (LPHELPINFO) lParam;

	// A stray or synthesised WM_HELP may carry no HELPINFO.
	if (hlp == NULL || hlp->iCtrlId == 0)
		return;
	if (helpPopupWnd != NULL)
		return;

	// Controls without a help string show nothing.
	if (LoadString(HELP_INSTANCE, HelpStringId(hlp->iCtrlId),
				   helpText, sizeof(helpText)) <= 0)
		return;

	HelpShowPopup((HWND)hlp->hItemHandle, hlp->MousePos);
}

// F1 and the drawn "?" button for NT 3.1, NT 3.5 and Win32s.
//
// NT 3.51 handles both itself.  On the older systems the dialogs passed to
// InitDialog are subclassed: the subclass draws a "?" button at the right end
// of the caption, where NT 3.51 puts its own, and a click on it starts the
// What's This mode.  F1 is caught by a WH_MSGFILTER hook (and by the viewer's
// TranslateMsg for the modeless dialogs) and posted back to the dialog.  Both
// show the help in the popup below, not through WM_HELP, and the same hooks
// hide it.

static BOOL HelpIsDialog(HWND hwnd)
{
	char name[16];

	if (GetClassName(hwnd, name, sizeof(name)) == 0)
		return FALSE;
	return (strcmp(name, "#32770") == 0);
}

static const char helpOldProcProp[] = "VNCHelpOldProc";

// Posted to a dialog to show the help for the control in lParam.
static UINT helpShowMsg = 0;

// The help popup of NT 3.1, NT 3.5 and Win32s.
//
// Built like the toolbar tooltips of vc2ctl, which work there: one hidden
// window per dialog, created by InitDialog, disabled and transparent to the
// mouse, with the text in the stock ANSI_VAR_FONT.  Showing it is just a
// SetWindowPos; it takes no capture and runs no message loop.  The message
// hooks of the dialog (HelpTipDismiss) hide it on the next key or click.

#define HELP_TIP_CLASS		"VNCHelpTip"
#define HELP_TIP_BK			RGB(255, 255, 225)
#define HELP_TIP_TEXT		RGB(0, 0, 0)

static const char helpTipProp[] = "VNCHelpTip";
static HWND helpTipShown = NULL;

static LRESULT CALLBACK HelpTipProc(HWND hwnd, UINT msg,
									WPARAM wParam, LPARAM lParam)
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
			char text[sizeof(helpText)];
			RECT rc;
			HBRUSH brush;
			HGDIOBJ oldFont;

			GetClientRect(hwnd, &rc);
			brush = CreateSolidBrush(HELP_TIP_BK);
			FillRect(hdc, &rc, brush);
			DeleteObject(brush);
			brush = CreateSolidBrush(HELP_TIP_TEXT);
			FrameRect(hdc, &rc, brush);
			DeleteObject(brush);

			GetWindowText(hwnd, text, sizeof(text));
			InflateRect(&rc, -HELP_MARGIN_X, -HELP_MARGIN_Y);
			oldFont = SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, HELP_TIP_TEXT);
			DrawText(hdc, text, -1, &rc, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
			SelectObject(hdc, oldFont);
			EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_ACTIVATEAPP:
		if (!wParam)
			ShowWindow(hwnd, SW_HIDE);
		break;
	case WM_DESTROY:
		if (hwnd == helpTipShown)
			helpTipShown = NULL;
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static BOOL HelpTipRegister()
{
	static BOOL registered = FALSE;
	WNDCLASS wndclass;

	if (registered)
		return TRUE;
	memset(&wndclass, 0, sizeof(wndclass));
	wndclass.lpfnWndProc	= HelpTipProc;
	wndclass.hInstance		= HELP_INSTANCE;
	wndclass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground	= NULL;
	wndclass.lpszClassName	= HELP_TIP_CLASS;
	registered = (RegisterClass(&wndclass) != 0 ||
				  GetClassInfo(HELP_INSTANCE, HELP_TIP_CLASS, &wndclass));
	return registered;
}

// The hidden help popup of a dialog, owned by it so that it stays above it
// and goes with it.
static HWND HelpTipCreate(HWND hdlg)
{
	HWND tip = (HWND)GetProp(hdlg, helpTipProp);

	if (tip != NULL && IsWindow(tip))
		return tip;
	if (!HelpTipRegister())
		return NULL;
	tip = CreateWindowEx(WS_EX_TOPMOST, HELP_TIP_CLASS, "",
						 WS_POPUP | WS_DISABLED, 0, 0, 0, 0,
						 hdlg, NULL, HELP_INSTANCE, NULL);
	if (tip != NULL)
		SetProp(hdlg, helpTipProp, (HANDLE)tip);
	return tip;
}

// Shows helpText under the control (or at the mouse, if the mouse is on the
// control).
static void HelpTipShow(HWND ctl, POINT mouse)
{
	HWND top = HelpTopLevel(ctl);
	HWND tip;
	HDC hdc;
	HGDIOBJ oldFont;
	RECT rc, itemRect;
	BOOL underItem;
	int maxWidth, cx, cy, x, y, sw, sh;

	if (top == NULL || (tip = HelpTipCreate(top)) == NULL)
		return;
	if (helpTipShown != NULL && helpTipShown != tip && IsWindow(helpTipShown))
		ShowWindow(helpTipShown, SW_HIDE);
	SetWindowText(tip, helpText);

	sw = GetSystemMetrics(SM_CXSCREEN);
	sh = GetSystemMetrics(SM_CYSCREEN);
	maxWidth = sw / 3;
	if (maxWidth < 240)
		maxWidth = 240;
	SetRect(&rc, 0, 0, maxWidth, 0);
	hdc = GetDC(tip);
	if (hdc == NULL)
		return;
	oldFont = SelectObject(hdc, GetStockObject(ANSI_VAR_FONT));
	DrawText(hdc, helpText, -1, &rc,
			 DT_CALCRECT | DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
	SelectObject(hdc, oldFont);
	ReleaseDC(tip, hdc);
	cx = (rc.right - rc.left) + 2 * HELP_MARGIN_X + 2;
	cy = (rc.bottom - rc.top) + 2 * HELP_MARGIN_Y + 2;

	GetWindowRect(ctl, &itemRect);
	underItem = !PtInRect(&itemRect, mouse);
	if (underItem) {
		x = (itemRect.left + itemRect.right) / 2 - cx / 2;
		y = itemRect.bottom + 2;
	} else {
		x = mouse.x - cx / 2;
		y = mouse.y + GetSystemMetrics(SM_CYCURSOR) / 2;
	}
	if (y + cy > sh)
		y = (underItem ? itemRect.top : mouse.y) - 2 - cy;
	if (x + cx > sw)
		x = sw - cx;
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;

	SetWindowPos(tip, HWND_TOPMOST, x, y, cx, cy,
				 SWP_NOACTIVATE | SWP_SHOWWINDOW);
	InvalidateRect(tip, NULL, TRUE);
	UpdateWindow(tip);
	helpTipShown = tip;
}

// The next key or click hides the popup, and is swallowed.
static BOOL HelpTipDismiss(MSG *pmsg)
{
	if (helpTipShown == NULL)
		return FALSE;
	if (!IsWindow(helpTipShown) || !IsWindowVisible(helpTipShown)) {
		helpTipShown = NULL;
		return FALSE;
	}
	if (!HelpIsDismissMessage(pmsg->message))
		return FALSE;
	ShowWindow(helpTipShown, SW_HIDE);
	helpTipShown = NULL;
	// F1 goes on to show the help for the control with the focus.
	return !(pmsg->message == WM_KEYDOWN && pmsg->wParam == VK_F1);
}

// TRUE if a control has a help string.
static BOOL HelpHasString(HWND ctl)
{
	char probe[4];

	return LoadString(HELP_INSTANCE, HelpStringId(GetDlgCtrlID(ctl)),
					  probe, sizeof(probe)) > 0;
}

// The help for a control; nothing for a control without a help string.
static void HelpShowFor(HWND ctl, POINT pt)
{
	if (ctl == NULL || !IsWindow(ctl))
		return;
	if (LoadString(HELP_INSTANCE, HelpStringId(GetDlgCtrlID(ctl)),
				   helpText, sizeof(helpText)) <= 0)
		return;
	HelpTipShow(ctl, pt);
}

// Plain F1 on a control of a dialog set up by InitDialog: posts the dialog
// the control that has the focus (or the dialog control that contains it,
// such as the combo box of an edit).
static BOOL HelpHandleF1(MSG *pmsg)
{
	HWND ctl;
	HWND parent;
	HWND top;

	if (pmsg->message != WM_KEYDOWN || pmsg->wParam != VK_F1)
		return FALSE;
	if (GetKeyState(VK_SHIFT) < 0 || GetKeyState(VK_CONTROL) < 0)
		return FALSE;

	ctl = pmsg->hwnd;
	while (ctl != NULL && (GetWindowLong(ctl, GWL_STYLE) & WS_CHILD)) {
		parent = GetParent(ctl);
		if (parent != NULL && HelpIsDialog(parent)) {
			top = HelpTopLevel(ctl);
			if (top == NULL || GetProp(top, helpOldProcProp) == NULL)
				return FALSE;
			PostMessage(top, helpShowMsg, 0, (LPARAM)ctl);
			return TRUE;
		}
		ctl = parent;
	}
	return FALSE;
}

// One WH_MSGFILTER hook per thread that has a help dialog open.
struct HelpThreadHook {
	DWORD thread;
	HHOOK hook;
	int refs;
};

#define HELP_MAX_HOOKS 8
static HelpThreadHook helpHooks[HELP_MAX_HOOKS];

static HelpThreadHook *HelpFindHook(DWORD thread)
{
	int i;

	for (i = 0; i < HELP_MAX_HOOKS; i++) {
		if (helpHooks[i].hook != NULL && helpHooks[i].thread == thread)
			return &helpHooks[i];
	}
	return NULL;
}

// Sees the messages of modal dialog loops (DialogBox, and the property
// pages inside them) before IsDialogMessage does.
static LRESULT CALLBACK HelpMsgFilterProc(int code, WPARAM wParam,
										  LPARAM lParam)
{
	HelpThreadHook *h;

	if (code == MSGF_DIALOGBOX &&
		(HelpTipDismiss((MSG *)lParam) || HelpHandleF1((MSG *)lParam)))
		return TRUE;
	h = HelpFindHook(GetCurrentThreadId());
	return CallNextHookEx(h != NULL ? h->hook : NULL, code, wParam, lParam);
}

static void HelpHookThread(BOOL install)
{
	DWORD thread = GetCurrentThreadId();
	HelpThreadHook *h = HelpFindHook(thread);
	int i;

	if (install) {
		if (h != NULL) {
			h->refs++;
			return;
		}
		for (i = 0; i < HELP_MAX_HOOKS; i++) {
			if (helpHooks[i].hook == NULL) {
				helpHooks[i].hook = SetWindowsHookEx(WH_MSGFILTER,
										(HOOKPROC)HelpMsgFilterProc,
										NULL, thread);
				if (helpHooks[i].hook != NULL) {
					helpHooks[i].thread = thread;
					helpHooks[i].refs = 1;
				}
				return;
			}
		}
	} else if (h != NULL && --h->refs == 0) {
		UnhookWindowsHookEx(h->hook);
		h->hook = NULL;
	}
}

// The smallest visible control with a help string under a point, looking
// inside child dialogs (property pages).  The smallest wins so that a control
// beats the group box around it, and a control without help (a plain label,
// the tab control) never hides one that has it.
static void HelpFindAtPoint(HWND parent, POINT pt, HWND *best, long *bestArea)
{
	HWND child;
	long area;
	RECT rc;

	for (child = GetWindow(parent, GW_CHILD); child != NULL;
		 child = GetWindow(child, GW_HWNDNEXT)) {
		if (!IsWindowVisible(child))
			continue;
		GetWindowRect(child, &rc);
		if (!PtInRect(&rc, pt))
			continue;
		if (HelpIsDialog(child)) {
			HelpFindAtPoint(child, pt, best, bestArea);
			continue;
		}
		if (!HelpHasString(child))
			continue;
		area = (long)(rc.right - rc.left) * (long)(rc.bottom - rc.top);
		if (*best == NULL || area < *bestArea) {
			*best = child;
			*bestArea = area;
		}
	}
}

static HWND HelpChildAtPoint(HWND parent, POINT pt)
{
	HWND best = NULL;
	long bestArea = 0;

	HelpFindAtPoint(parent, pt, &best, &bestArea);
	return best;
}

// What's This? mode: an arrow with a question mark until the next click,
// which shows the help for the control under it.  Any other key or button
// cancels.
static void HelpWhatsThis(HWND hdlg)
{
	HCURSOR cursor = NULL;
	HWND ctl;
	MSG msg;

#ifdef IDC_HELPARROW
	cursor = LoadCursor(HELP_INSTANCE, MAKEINTRESOURCE(IDC_HELPARROW));
#endif
	if (cursor == NULL)
		cursor = LoadCursor(NULL, IDC_CROSS);
	SetCapture(hdlg);
	SetCursor(cursor);
	for (;;) {
		if (!GetMessage(&msg, NULL, 0, 0)) {
			PostQuitMessage((int)msg.wParam);
			break;
		}
		if (msg.message == WM_LBUTTONDOWN) {
			ReleaseCapture();
			ctl = HelpChildAtPoint(hdlg, msg.pt);
			if (ctl != NULL)
				HelpShowFor(ctl, msg.pt);
			return;
		}
		if (HelpIsDismissMessage(msg.message))
			break;
		if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) {
			SetCursor(cursor);
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (GetCapture() != hdlg)
			break;
	}
	if (GetCapture() == hdlg)
		ReleaseCapture();
}

// A screen point in the coordinates of the whole window.
static void HelpScreenToWindow(HWND hwnd, POINT *pt)
{
	RECT win;

	GetWindowRect(hwnd, &win);
	pt->x -= win.left;
	pt->y -= win.top;
}

// The drawn "?" button, in window coordinates: the right end of the caption,
// as wide as a minimize / maximize button.  FALSE where the system draws its
// own (NT 3.51 and later) or the window has no caption.
static BOOL HelpButtonRect(HWND hwnd, RECT *rc)
{
	RECT win, client;
	POINT org;
	int frameX, frameY, captionBottom, cx;

	if (HelpHasContextButton())
		return FALSE;
	if ((GetWindowLong(hwnd, GWL_STYLE) & WS_CAPTION) != WS_CAPTION)
		return FALSE;

	// The frame sizes from where the client area is, so that any frame
	// style is right; the caption ends above the client area, less the line
	// under it.
	GetWindowRect(hwnd, &win);
	GetClientRect(hwnd, &client);
	org.x = 0;
	org.y = 0;
	ClientToScreen(hwnd, &org);
	frameX = org.x - win.left;
	frameY = win.bottom - (org.y + client.bottom);
	captionBottom = org.y - win.top - 1;
	if (frameX < 0 || frameY < 0 || captionBottom <= frameY)
		return FALSE;

	cx = GetSystemMetrics(SM_CXSIZE);
	rc->right = (win.right - win.left) - frameX;
	rc->left = rc->right - cx;
	rc->top = frameY;
	rc->bottom = captionBottom;
	return (rc->left > frameX);
}

static void HelpFillSys(HDC hdc, int left, int top, int right, int bottom,
						int colour)
{
	RECT rc;
	HBRUSH brush = CreateSolidBrush(GetSysColor(colour));

	SetRect(&rc, left, top, right, bottom);
	FillRect(hdc, &rc, brush);
	DeleteObject(brush);
}

// Draws the "?" button like the minimize / maximize buttons of these
// systems: a line on the left, then a raised (or pushed) grey face.
static void HelpDrawButton(HWND hwnd, BOOL pushed)
{
	RECT rc;
	HDC hdc;
	HGDIOBJ oldFont;
	int l, t, r, b;

	if (!HelpButtonRect(hwnd, &rc))
		return;
	hdc = GetWindowDC(hwnd);
	if (hdc == NULL)
		return;

	HelpFillSys(hdc, rc.left, rc.top, rc.left + 1, rc.bottom,
				COLOR_WINDOWFRAME);
	l = rc.left + 1;
	t = rc.top;
	r = rc.right;
	b = rc.bottom;
	HelpFillSys(hdc, l, t, r, b, COLOR_BTNFACE);
	if (pushed) {
		HelpFillSys(hdc, l, t, r, t + 1, COLOR_BTNSHADOW);
		HelpFillSys(hdc, l, t, l + 1, b, COLOR_BTNSHADOW);
	} else {
		HelpFillSys(hdc, l, t, r - 1, t + 1, COLOR_BTNHIGHLIGHT);
		HelpFillSys(hdc, l, t, l + 1, b - 1, COLOR_BTNHIGHLIGHT);
		HelpFillSys(hdc, l, b - 2, r, b, COLOR_BTNSHADOW);
		HelpFillSys(hdc, r - 2, t, r, b, COLOR_BTNSHADOW);
	}

	SetRect(&rc, l, t, r - 1, b - 1);
	if (pushed)
		OffsetRect(&rc, 1, 1);
	oldFont = SelectObject(hdc, GetStockObject(SYSTEM_FONT));
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
	DrawText(hdc, "?", 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	SelectObject(hdc, oldFont);
	ReleaseDC(hwnd, hdc);
}

// Holds the "?" button down while the mouse button is; TRUE if it was
// released over the button.
static BOOL HelpTrackButton(HWND hwnd)
{
	RECT rc;
	MSG msg;
	POINT pt;
	BOOL in = TRUE;
	BOOL clicked = FALSE;

	if (!HelpButtonRect(hwnd, &rc))
		return FALSE;
	SetCapture(hwnd);
	HelpDrawButton(hwnd, TRUE);
	for (;;) {
		if (!GetMessage(&msg, NULL, 0, 0)) {
			PostQuitMessage((int)msg.wParam);
			break;
		}
		if (msg.message == WM_MOUSEMOVE || msg.message == WM_LBUTTONUP) {
			pt = msg.pt;
			HelpScreenToWindow(hwnd, &pt);
			if (msg.message == WM_LBUTTONUP) {
				clicked = PtInRect(&rc, pt);
				break;
			}
			if (PtInRect(&rc, pt) != in) {
				in = !in;
				HelpDrawButton(hwnd, in);
			}
			continue;
		}
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
			break;
		if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)
			continue;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (GetCapture() != hwnd)
			break;
	}
	if (GetCapture() == hwnd)
		ReleaseCapture();
	HelpDrawButton(hwnd, FALSE);
	return clicked;
}

// CallWindowProc takes a WNDPROC with STRICT, but a FARPROC in the
// non-STRICT builds of the older SDKs.
#ifdef STRICT
#define HELP_CWP(proc)	(proc)
#else
#define HELP_CWP(proc)	((FARPROC)(proc))
#endif

static LRESULT CALLBACK HelpDialogSubclassProc(HWND hwnd, UINT msg,
											   WPARAM wParam, LPARAM lParam)
{
	WNDPROC oldProc = (WNDPROC)GetProp(hwnd, helpOldProcProp);
	LRESULT result;
	POINT pt;
	RECT rc;

	if (msg == helpShowMsg && helpShowMsg != 0) {
		GetCursorPos(&pt);
		HelpShowFor((HWND)lParam, pt);
		return 0;
	}

	switch (msg) {
	case WM_NCLBUTTONDOWN:
	case WM_NCLBUTTONDBLCLK:
		pt.x = (short)LOWORD(lParam);
		pt.y = (short)HIWORD(lParam);
		HelpScreenToWindow(hwnd, &pt);
		if (HelpButtonRect(hwnd, &rc) && PtInRect(&rc, pt)) {
			if (HelpTrackButton(hwnd))
				HelpWhatsThis(hwnd);
			return 0;
		}
		break;
	case WM_NCPAINT:
	case WM_NCACTIVATE:
	case WM_SETTEXT:
		// These draw the caption, over the button.
		result = CallWindowProc(HELP_CWP(oldProc), hwnd, msg, wParam, lParam);
		HelpDrawButton(hwnd, FALSE);
		return result;
	case WM_NCDESTROY:
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)oldProc);
		RemoveProp(hwnd, helpOldProcProp);
		RemoveProp(hwnd, helpTipProp);
		HelpHookThread(FALSE);
		break;
	}
	return CallWindowProc(HELP_CWP(oldProc), hwnd, msg, wParam, lParam);
}

void VNCHelp::InitDialog(HWND hwnd)
{
	// The "?" button where the system draws it (NT 3.51 and later), which
	// also sends WM_HELP for F1 itself.
	HelpSetContextButton(hwnd, TRUE);
	if (HelpHasContextButton())
		return;

	// NT 3.1, NT 3.5 and Win32s: F1 through the hook, and a drawn "?" button.
	if (GetProp(hwnd, helpOldProcProp) != NULL)
		return;
	if (helpShowMsg == 0)
		helpShowMsg = RegisterWindowMessage("VNCHelpShow");
	SetProp(hwnd, helpOldProcProp,
			(HANDLE)GetWindowLongPtr(hwnd, GWLP_WNDPROC));
	SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)HelpDialogSubclassProc);
	HelpHookThread(TRUE);
	HelpTipCreate(hwnd);
}

#endif // VNCHELP_MINI
