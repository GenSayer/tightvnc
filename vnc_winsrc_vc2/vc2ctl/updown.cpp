// updown.cpp - built-in msctls_updown32
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.
//
// Messages: UDM_SETRANGE, UDM_GETRANGE, UDM_SETPOS, UDM_GETPOS, UDM_SETBUDDY,
// UDM_GETBUDDY.
// Styles: UDS_ALIGNRIGHT / UDS_ALIGNLEFT, UDS_AUTOBUDDY, UDS_SETBUDDYINT,
// UDS_NOTHOUSANDS, UDS_WRAP, UDS_HORZ, UDS_ARROWKEYS.
// Notifications: UDN_DELTAPOS (non-zero result vetoes), then WM_VSCROLL /
// WM_HSCROLL with SB_THUMBPOSITION, and SB_ENDSCROLL when the button is let go.
//
// As in COMCTL32, the default range is 100 (lower) to 0 (upper), and the up
// (or right) arrow moves towards 'upper'; iDelta is the signed change, so with
// the default range the up arrow sends iDelta = -1.  VNCOptions relies on that.

#include "vc2ctli.h"

#define UD_PROP_CTL     "Vc2UpDownCtl"
#define UD_PROP_OLD     "Vc2UpDownOldProc"

#define UD_TIMER        1
#define UD_FIRST_DELAY  500
#define UD_REPEAT_DELAY 100

typedef struct {
	int lower, upper, pos;
	HWND buddy;
	int pressed;            // 0 none, 1 up / right, 2 down / left
	BOOL inside;            // cursor still over the pressed half
	BOOL repeating;
} UDDATA;

static BOOL UdHorz(HWND hwnd)
{
	return (GetWindowLong(hwnd, GWL_STYLE) & UDS_HORZ) ? TRUE : FALSE;
}

// Which half of the control is at (x, y): 1 = up / right, 2 = down / left.
static int UdHit(HWND hwnd, int x, int y)
{
	RECT rc;
	GetClientRect(hwnd, &rc);
	if (UdHorz(hwnd))
		return (x >= rc.right / 2) ? 1 : 2;
	return (y < rc.bottom / 2) ? 1 : 2;
}

static void UdHalfRect(HWND hwnd, int part, RECT *prc)
{
	GetClientRect(hwnd, prc);
	if (UdHorz(hwnd)) {
		if (part == 1) prc->left = prc->right / 2;
		else prc->right = prc->right / 2;
	} else {
		if (part == 1) prc->bottom = prc->bottom / 2;
		else prc->top = prc->bottom / 2;
	}
}

static void UdPaint(HWND hwnd, UDDATA *ud)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	BOOL enabled = IsWindowEnabled(hwnd);
	COLORREF arrow = GetSysColor(enabled ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
	int part;

	for (part = 1; part <= 2; part++) {
		RECT rc;
		int dir, cx, cy, size;
		BOOL down = (ud->pressed == part && ud->inside);
		UdHalfRect(hwnd, part, &rc);
		Vc2FillRect(hdc, &rc, GetSysColor(COLOR_BTNFACE));
		if (down)
			Vc2DrawSunken(hdc, &rc);
		else
			Vc2DrawRaised(hdc, &rc);
		if (UdHorz(hwnd))
			dir = (part == 1) ? VC2_ARROW_RIGHT : VC2_ARROW_LEFT;
		else
			dir = (part == 1) ? VC2_ARROW_UP : VC2_ARROW_DOWN;
		size = ((UdHorz(hwnd) ? (rc.bottom - rc.top) : (rc.right - rc.left)) - 4) / 2;
		if (size > 4) size = 4;
		if (size < 2) size = 2;
		cx = (rc.left + rc.right) / 2 + (down ? 1 : 0);
		cy = (rc.top + rc.bottom) / 2 + (down ? 1 : 0);
		Vc2DrawArrow(hdc, cx, cy, size, dir, arrow);
	}
	EndPaint(hwnd, &ps);
}

static void UdSetBuddyText(HWND hwnd, UDDATA *ud)
{
	char buf[16];
	if (ud->buddy == NULL || !(GetWindowLong(hwnd, GWL_STYLE) & UDS_SETBUDDYINT))
		return;
	sprintf(buf, "%d", ud->pos);     // no thousands separators, ever
	SetWindowText(ud->buddy, buf);
}

// With UDS_SETBUDDYINT the buddy's text is the position; re-read it first.
static void UdReadBuddy(HWND hwnd, UDDATA *ud)
{
	char buf[32];
	char *p, *q;
	long v;
	if (ud->buddy == NULL || !(GetWindowLong(hwnd, GWL_STYLE) & UDS_SETBUDDYINT))
		return;
	GetWindowText(ud->buddy, buf, sizeof(buf));
	for (p = q = buf; *p; p++)
		if (*p != ',' && *p != '.' && *p != ' ')
			*q++ = *p;
	*q = '\0';
	if (buf[0] == '\0')
		return;
	v = strtol(buf, &p, 10);
	if (*p == '\0')
		ud->pos = (int)v;
}

static int UdMin(UDDATA *ud) { return (ud->lower < ud->upper) ? ud->lower : ud->upper; }
static int UdMax(UDDATA *ud) { return (ud->lower < ud->upper) ? ud->upper : ud->lower; }

// One click of the given half.
static void UdStep(HWND hwnd, UDDATA *ud, int part)
{
	NM_UPDOWN nm;
	int delta, np;
	HWND hParent;

	UdReadBuddy(hwnd, ud);
	delta = (ud->upper >= ud->lower) ? 1 : -1;
	if (part == 2)
		delta = -delta;

	memset(&nm, 0, sizeof(nm));
	nm.iPos = ud->pos;
	nm.iDelta = delta;
	if (Vc2Notify(hwnd, UDN_DELTAPOS, &nm.hdr) != 0)
		return;
	delta = nm.iDelta;      // the parent may change it

	np = ud->pos + delta;
	if (np < UdMin(ud) || np > UdMax(ud)) {
		if (GetWindowLong(hwnd, GWL_STYLE) & UDS_WRAP)
			np = (np < UdMin(ud)) ? UdMax(ud) : UdMin(ud);
		else
			np = (np < UdMin(ud)) ? UdMin(ud) : UdMax(ud);
	}
	ud->pos = np;
	UdSetBuddyText(hwnd, ud);

	hParent = GetParent(hwnd);
	if (hParent != NULL)
		SendMessage(hParent, UdHorz(hwnd) ? WM_HSCROLL : WM_VSCROLL,
					MAKEWPARAM(SB_THUMBPOSITION, ud->pos), (LPARAM)hwnd);
}

static void UdEndScroll(HWND hwnd)
{
	HWND hParent = GetParent(hwnd);
	if (hParent != NULL)
		SendMessage(hParent, UdHorz(hwnd) ? WM_HSCROLL : WM_VSCROLL,
					MAKEWPARAM(SB_ENDSCROLL, 0), (LPARAM)hwnd);
}

// Buddy subclass for UDS_ARROWKEYS: Up / Down in the buddy click the arrows.
static LRESULT CALLBACK UdBuddyProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hUd = (HWND)GetProp(hwnd, UD_PROP_CTL);
	LONG old = (LONG)GetProp(hwnd, UD_PROP_OLD);

	if (old == 0)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_GETDLGCODE:
		return CallWindowProc((FARPROC)old, hwnd, msg, wParam, lParam) | DLGC_WANTARROWS;

	case WM_KEYDOWN:
		if (hUd != NULL && IsWindow(hUd) && IsWindowEnabled(hUd)) {
			UDDATA *ud = (UDDATA *)Vc2GetData(hUd);
			BOOL horz = UdHorz(hUd);
			if (ud != NULL) {
				if ((!horz && wParam == VK_UP) || (horz && wParam == VK_RIGHT)) {
					UdStep(hUd, ud, 1);
					return 0;
				}
				if ((!horz && wParam == VK_DOWN) || (horz && wParam == VK_LEFT)) {
					UdStep(hUd, ud, 2);
					return 0;
				}
			}
		}
		break;

	case WM_KEYUP:
		if (hUd != NULL && IsWindow(hUd) &&
			(wParam == VK_UP || wParam == VK_DOWN)) {
			UdEndScroll(hUd);
		}
		break;

	case WM_NCDESTROY:
		SetWindowLong(hwnd, GWL_WNDPROC, old);
		RemoveProp(hwnd, UD_PROP_CTL);
		RemoveProp(hwnd, UD_PROP_OLD);
		return CallWindowProc((FARPROC)old, hwnd, msg, wParam, lParam);
	}
	return CallWindowProc((FARPROC)old, hwnd, msg, wParam, lParam);
}

static void UdUnhookBuddy(HWND hwnd, UDDATA *ud)
{
	LONG old;
	if (ud->buddy == NULL || !IsWindow(ud->buddy))
		return;
	if ((HWND)GetProp(ud->buddy, UD_PROP_CTL) != hwnd)
		return;
	old = (LONG)GetProp(ud->buddy, UD_PROP_OLD);
	// Only unhook if nobody subclassed the buddy after us.
	if (old != 0 && GetWindowLong(ud->buddy, GWL_WNDPROC) == (LONG)UdBuddyProc) {
		SetWindowLong(ud->buddy, GWL_WNDPROC, old);
		RemoveProp(ud->buddy, UD_PROP_CTL);
		RemoveProp(ud->buddy, UD_PROP_OLD);
	}
}

static void UdAttachBuddy(HWND hwnd, UDDATA *ud, HWND buddy)
{
	DWORD style = GetWindowLong(hwnd, GWL_STYLE);

	UdUnhookBuddy(hwnd, ud);
	ud->buddy = buddy;
	if (buddy == NULL)
		return;

	if ((style & UDS_ARROWKEYS) && GetProp(buddy, UD_PROP_OLD) == NULL) {
		LONG old = GetWindowLong(buddy, GWL_WNDPROC);
		SetProp(buddy, UD_PROP_CTL, (HANDLE)hwnd);
		SetProp(buddy, UD_PROP_OLD, (HANDLE)old);
		SetWindowLong(buddy, GWL_WNDPROC, (LONG)UdBuddyProc);
	}

	// Sit against the buddy's right (or left) edge, taking that much of its
	// width, at the buddy's height - as COMCTL32 does.
	if (style & (UDS_ALIGNRIGHT | UDS_ALIGNLEFT)) {
		HWND hParent = GetParent(hwnd);
		RECT rb, rs;
		int w, h;
		GetWindowRect(buddy, &rb);
		GetWindowRect(hwnd, &rs);
		if (hParent != NULL) {
			MapWindowPoints(NULL, hParent, (POINT *)&rb, 2);
		}
		w = rs.right - rs.left;
		h = rb.bottom - rb.top;
		if (w <= 0 || w >= rb.right - rb.left)
			return;
		if (style & UDS_ALIGNRIGHT) {
			MoveWindow(buddy, rb.left, rb.top, rb.right - rb.left - w + 1, h, TRUE);
			MoveWindow(hwnd, rb.right - w, rb.top, w, h, TRUE);
		} else {
			MoveWindow(buddy, rb.left + w - 1, rb.top, rb.right - rb.left - w + 1, h, TRUE);
			MoveWindow(hwnd, rb.left, rb.top, w, h, TRUE);
		}
	}
	UdSetBuddyText(hwnd, ud);
}

static LRESULT CALLBACK UdWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	UDDATA *ud = (UDDATA *)Vc2GetData(hwnd);

	if (ud == NULL && msg != WM_NCCREATE)
		return DefWindowProc(hwnd, msg, wParam, lParam);

	switch (msg) {
	case WM_NCCREATE:
		ud = (UDDATA *)Vc2Alloc(sizeof(UDDATA));
		if (ud == NULL)
			return FALSE;
		ud->lower = 100;
		ud->upper = 0;
		ud->pos = 0;
		Vc2SetData(hwnd, ud);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		if (GetWindowLong(hwnd, GWL_STYLE) & UDS_AUTOBUDDY)
			UdAttachBuddy(hwnd, ud, GetWindow(hwnd, GW_HWNDPREV));
		return 0;

	case WM_NCDESTROY:
		UdUnhookBuddy(hwnd, ud);
		Vc2Free(ud);
		Vc2SetData(hwnd, NULL);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		UdPaint(hwnd, ud);
		return 0;

	case WM_ENABLE:
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if (!IsWindowEnabled(hwnd))
			return 0;
		if (ud->buddy != NULL && IsWindow(ud->buddy))
			SetFocus(ud->buddy);
		ud->pressed = UdHit(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
		ud->inside = TRUE;
		ud->repeating = FALSE;
		SetCapture(hwnd);
		InvalidateRect(hwnd, NULL, TRUE);
		UdStep(hwnd, ud, ud->pressed);
		SetTimer(hwnd, UD_TIMER, UD_FIRST_DELAY, NULL);
		return 0;

	case WM_MOUSEMOVE:
		if (ud->pressed) {
			POINT pt;
			RECT rc;
			BOOL in;
			pt.x = (short)LOWORD(lParam);
			pt.y = (short)HIWORD(lParam);
			UdHalfRect(hwnd, ud->pressed, &rc);
			in = PtInRect(&rc, pt);
			if (in != ud->inside) {
				ud->inside = in;
				InvalidateRect(hwnd, NULL, TRUE);
			}
		}
		return 0;

	case WM_TIMER:
		if (wParam == UD_TIMER && ud->pressed) {
			if (!ud->repeating) {
				ud->repeating = TRUE;
				KillTimer(hwnd, UD_TIMER);
				SetTimer(hwnd, UD_TIMER, UD_REPEAT_DELAY, NULL);
			}
			if (ud->inside)
				UdStep(hwnd, ud, ud->pressed);
		}
		return 0;

	case WM_LBUTTONUP:
	case WM_CAPTURECHANGED:
		if (ud->pressed) {
			ud->pressed = 0;
			KillTimer(hwnd, UD_TIMER);
			if (msg == WM_LBUTTONUP)
				ReleaseCapture();
			InvalidateRect(hwnd, NULL, TRUE);
			UdEndScroll(hwnd);
		}
		return 0;

	case UDM_SETRANGE:
		ud->upper = (int)(short)LOWORD(lParam);
		ud->lower = (int)(short)HIWORD(lParam);
		if (ud->pos < UdMin(ud)) ud->pos = UdMin(ud);
		if (ud->pos > UdMax(ud)) ud->pos = UdMax(ud);
		return 0;

	case UDM_GETRANGE:
		return MAKELONG((short)ud->upper, (short)ud->lower);

	case UDM_SETPOS:
		{
			int old = ud->pos;
			ud->pos = (int)(short)LOWORD(lParam);
			if (ud->pos < UdMin(ud)) ud->pos = UdMin(ud);
			if (ud->pos > UdMax(ud)) ud->pos = UdMax(ud);
			UdSetBuddyText(hwnd, ud);
			return old;
		}

	case UDM_GETPOS:
		UdReadBuddy(hwnd, ud);
		return MAKELONG((short)ud->pos, 0);

	case UDM_SETBUDDY:
		{
			HWND old = ud->buddy;
			UdAttachBuddy(hwnd, ud, (HWND)wParam);
			return (LRESULT)old;
		}

	case UDM_GETBUDDY:
		return (LRESULT)ud->buddy;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL Vc2RegisterUpDown(void)
{
	return Vc2RegisterClass("msctls_updown32", (WNDPROC)UdWndProc,
							CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW, NULL, -1);
}
