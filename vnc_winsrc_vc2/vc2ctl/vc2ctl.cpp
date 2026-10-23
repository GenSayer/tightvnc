// vc2ctl.cpp - when to use the built-in controls, and class registration
//
// Part of the TightVNC VC++ 2.x port.  See vc2ctl.h.

#include "vc2ctli.h"

HINSTANCE g_vc2ctlInst = NULL;

static DWORD g_registered = 0;      // classes registered by us

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------

// Windows NT before 3.51.  GetVersion: high bit clear on NT; low byte major,
// next byte minor.  Win32s also reports 3.1x, but has the high bit set.
BOOL Vc2CtlForceBuiltins(void)
{
	DWORD v = GetVersion();
	int major, minor;
	if (v & 0x80000000)
		return FALSE;
	major = (int)(v & 0xFF);
	minor = (int)((v >> 8) & 0xFF);
	return (major < 3 || (major == 3 && minor < 51)) ? TRUE : FALSE;
}

BOOL Vc2CtlIsBuiltin(DWORD cls)
{
	return ((g_registered & cls) == cls && cls != 0) ? TRUE : FALSE;
}

static BOOL ClassExists(const char *name)
{
	WNDCLASS wc;
	if (GetClassInfo(g_vc2ctlInst, name, &wc))
		return TRUE;
	if (GetClassInfo(NULL, name, &wc))
		return TRUE;
	return FALSE;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

BOOL Vc2RegisterClass(const char *name, WNDPROC proc, UINT style,
                      HCURSOR hcur, int brushColor)
{
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	// Application-local on purpose (no CS_GLOBALCLASS): a local class is found
	// before COMCTL32's global one of the same name, but only for windows
	// created with this instance - which is how the dialogs are created.
	wc.style = style;
	wc.lpfnWndProc = proc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = VC2CTL_EXTRA;
	wc.hInstance = g_vc2ctlInst;
	wc.hIcon = NULL;
	wc.hCursor = (hcur != NULL) ? hcur : LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (brushColor >= 0) ? (HBRUSH)(brushColor + 1) : NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = name;
	if (RegisterClass(&wc))
		return TRUE;
	// Already registered (an earlier call, or another copy of this code in
	// the same module) is fine.
	return GetClassInfo(g_vc2ctlInst, name, &wc) ? TRUE : FALSE;
}

typedef BOOL (*VC2REGFN)(void);

typedef struct {
	DWORD mask;
	const char *name;       // standard class name, or NULL (no native class)
	VC2REGFN reg;
} VC2CLASSENTRY;

static const VC2CLASSENTRY g_classes[] = {
	{ VC2CTL_TAB,      "SysTabControl32",   Vc2RegisterTab },
	{ VC2CTL_TRACKBAR, "msctls_trackbar32", Vc2RegisterTrackbar },
	{ VC2CTL_UPDOWN,   "msctls_updown32",   Vc2RegisterUpDown },
	{ VC2CTL_PROGRESS, "msctls_progress32", Vc2RegisterProgress },
	{ VC2CTL_LISTVIEW, "SysListView32",     Vc2RegisterListView },
	{ VC2CTL_TREEVIEW, "SysTreeView32",     Vc2RegisterTreeView },
	// The toolbar has a private class name: it is only created through
	// Vc2CtlCreateToolbarEx, and the caller decides whether to use it.
	{ VC2CTL_TOOLBAR,  NULL,                Vc2RegisterToolbar }
};

BOOL Vc2CtlInit(DWORD classes)
{
	BOOL force = Vc2CtlForceBuiltins();
	BOOL ok = TRUE;
	int i;
	int n = sizeof(g_classes) / sizeof(g_classes[0]);

	if (g_vc2ctlInst == NULL)
		g_vc2ctlInst = GetModuleHandle(NULL);

	for (i = 0; i < n; i++) {
		const VC2CLASSENTRY *e = &g_classes[i];
		if ((classes & e->mask) == 0)
			continue;
		if (g_registered & e->mask)
			continue;
		if (e->name != NULL && !force && ClassExists(e->name))
			continue;       // the native control is there: use it
		if (e->reg())
			g_registered |= e->mask;
		else
			ok = FALSE;
	}
	return ok;
}
