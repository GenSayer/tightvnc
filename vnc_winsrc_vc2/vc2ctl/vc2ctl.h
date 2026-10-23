/* vc2ctl.h - built-in common controls for the VC++ 2.x / NT 3.1 / Win32s port
 *
 * This file is part of the TightVNC VC++ 2.x port (vnc_winsrc_vc2).
 *
 * WHY THIS EXISTS
 *
 * Windows NT 3.1 has none of the Windows 95 / NT 3.51 common controls (tab,
 * trackbar, up-down, progress, list view, tree view) and no CreateToolbarEx.
 * A dialog template that names a control class nobody has registered fails to
 * create as a whole, so the viewer Options / File Transfer dialogs and the
 * WinVNC Properties dialog could not open there at all.  NT 3.51 COMCTL32
 * cannot be used on NT 3.1, and on NT 3.5 it is unreliable.
 *
 * vc2ctl implements the subset of those controls that the viewer and WinVNC
 * use, and registers them as APPLICATION-LOCAL classes under the standard
 * names ("SysTabControl32" and so on).  Windows looks up local classes before
 * global ones (COMCTL32 registers CS_GLOBALCLASS classes), so the existing
 * dialog templates and every TCM_ / TBM_ / UDM_ / PBM_ / LVM_ / TVM_ message
 * in the sources work unchanged.
 *
 * WHEN THE BUILT-INS ARE USED
 *
 *   * Windows NT before 3.51: always, for every requested class - even if a
 *     COMCTL32 copied from NT 3.51 is installed.
 *   * Everywhere else: only for a class that is still not registered after
 *     InitCommonControls (for example Win32s without COMCTL32).  NT 3.51,
 *     NT 4 and Windows 95 keep the native controls.
 *
 * Only APIs that NT 3.1 and Win32s 1.15a export are used, so linking this
 * library adds no import that could stop an EXE loading there.
 */

#ifndef VC2CTL_H
#define VC2CTL_H

#include <windows.h>

/* Class masks for Vc2CtlInit */
#define VC2CTL_TAB          0x0001  /* SysTabControl32 */
#define VC2CTL_TRACKBAR     0x0002  /* msctls_trackbar32 */
#define VC2CTL_UPDOWN       0x0004  /* msctls_updown32 */
#define VC2CTL_PROGRESS     0x0008  /* msctls_progress32 */
#define VC2CTL_LISTVIEW     0x0010  /* SysListView32 (report view) */
#define VC2CTL_TREEVIEW     0x0020  /* SysTreeView32 */
#define VC2CTL_TOOLBAR      0x0040  /* built-in toolbar + its tooltip window */
#define VC2CTL_ALL          0x007F

#ifdef __cplusplus
extern "C" {
#endif

/* Register the built-in version of each class in 'classes' that the policy
 * above says to use.  Call it after InitCommonControls (if any).  Safe to
 * call more than once.  Returns TRUE if every requested class can now be
 * created (built-in or native). */
BOOL Vc2CtlInit(DWORD classes);

/* TRUE on Windows NT before 3.51, where the built-ins always replace the
 * native controls. */
BOOL Vc2CtlForceBuiltins(void);

/* TRUE if Vc2CtlInit registered the built-in version of this class. */
BOOL Vc2CtlIsBuiltin(DWORD cls);

/* Built-in toolbar with CreateToolbarEx semantics.  lpButtons points to
 * Windows 95 layout TBBUTTONs (20 bytes; uStructSize is checked).  The
 * bitmap colours are mapped to the button-face colours as COMCTL32 does.
 * Requires Vc2CtlInit(VC2CTL_TOOLBAR) first. */
HWND Vc2CtlCreateToolbarEx(HWND hwnd, DWORD ws, UINT wID, int nBitmaps,
                           HINSTANCE hBMInst, UINT wBMID,
                           const void *lpButtons, int iNumButtons,
                           int dxButton, int dyButton,
                           int dxBitmap, int dyBitmap, UINT uStructSize);

/* Keyboard navigation for a dialog whose tab pages are child dialogs.
 *
 * Those dialogs rely on WS_EX_CONTROLPARENT, which only NT 4 / Windows 95
 * honour.  Elsewhere (Vc2CtlNeedPagedDialogHelper) this installs a message
 * filter hook for the dialog's modal loop: Tab / Shift+Tab move through the
 * visible page and the dialog's own controls, arrow keys work inside the
 * page, and Ctrl+Tab / Ctrl+Shift+Tab switch tabs.  The registration ends by
 * itself when the dialog is destroyed; Vc2CtlEndPagedDialog ends it early.
 * hTab must be the dialog's tab control, pages[] its page dialogs (up to 8). */
BOOL Vc2CtlNeedPagedDialogHelper(void);
BOOL Vc2CtlBeginPagedDialog(HWND hDlg, HWND hTab, const HWND *pages, int nPages);
void Vc2CtlEndPagedDialog(HWND hDlg);

#ifdef __cplusplus
}
#endif

#endif /* VC2CTL_H */
