// vc2ctli.h - internal declarations shared by the vc2ctl controls
//
// Rules for everything in this directory (see vc2ctl.h for why):
//   * Only NT 3.1 / Win32s 1.15a USER and GDI calls.  In particular none of
//     DrawEdge, DrawFrameControl, DrawState, GetSysColorBrush, SetScrollInfo,
//     GetScrollInfo, TrackMouseEvent, image lists, or the COLOR_3D* indexes.
//     Lines are drawn as 1-pixel FillRects; scroll bars use SetScrollRange /
//     SetScrollPos.
//   * VC++ 2.x: no bool, no namespaces, no templates, and a for-loop variable
//     stays in scope after the loop, so declare loop counters once at the top
//     of each function.
//   * ANSI only.

#ifndef VC2CTLI_H
#define VC2CTLI_H

#include <windows.h>
#include <commctrl.h>
#include "vc2commctl.h"
#include "vc2ctl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constants the controls need that neither VC++ 2.x's headers nor
// vc2commctl.h provide.  Values are the Windows 95 / COMCTL32 4.00 ones.
// ---------------------------------------------------------------------------

// Never sent by NT 3.1 (it is NT 3.51 / Win95); handled anyway.
#ifndef WM_CAPTURECHANGED
#define WM_CAPTURECHANGED       0x0215
#endif
#ifndef MAKEWPARAM
#define MAKEWPARAM(l, h)        ((WPARAM)MAKELONG(l, h))
#endif
#ifndef MAKELPARAM
#define MAKELPARAM(l, h)        ((LPARAM)MAKELONG(l, h))
#endif

#ifndef TB_LINEUP
#define TB_LINEUP               0
#endif
#ifndef TB_LINEDOWN
#define TB_LINEDOWN             1
#endif
#ifndef TB_PAGEUP
#define TB_PAGEUP               2
#endif
#ifndef TB_PAGEDOWN
#define TB_PAGEDOWN             3
#endif
#ifndef TB_THUMBPOSITION
#define TB_THUMBPOSITION        4
#endif
#ifndef TB_THUMBTRACK
#define TB_THUMBTRACK           5
#endif
#ifndef TB_TOP
#define TB_TOP                  6
#endif
#ifndef TB_BOTTOM
#define TB_BOTTOM               7
#endif
#ifndef TB_ENDTRACK
#define TB_ENDTRACK             8
#endif

#ifndef TB_ENABLEBUTTON
#define TB_ENABLEBUTTON         (WM_USER + 1)
#endif
#ifndef TB_CHECKBUTTON
#define TB_CHECKBUTTON          (WM_USER + 2)
#endif
#ifndef TB_PRESSBUTTON
#define TB_PRESSBUTTON          (WM_USER + 3)
#endif
#ifndef TB_HIDEBUTTON
#define TB_HIDEBUTTON           (WM_USER + 4)
#endif
#ifndef TB_ISBUTTONENABLED
#define TB_ISBUTTONENABLED      (WM_USER + 9)
#endif
#ifndef TB_ISBUTTONCHECKED
#define TB_ISBUTTONCHECKED      (WM_USER + 10)
#endif
#ifndef TB_ISBUTTONPRESSED
#define TB_ISBUTTONPRESSED      (WM_USER + 11)
#endif
#ifndef TB_ISBUTTONHIDDEN
#define TB_ISBUTTONHIDDEN       (WM_USER + 12)
#endif
#ifndef TB_BUTTONCOUNT
#define TB_BUTTONCOUNT          (WM_USER + 24)
#endif
#ifndef TB_BUTTONSTRUCTSIZE
#define TB_BUTTONSTRUCTSIZE     (WM_USER + 30)
#endif
#ifndef TB_AUTOSIZE
#define TB_AUTOSIZE             (WM_USER + 33)
#endif
#ifndef TB_SETINDENT
#define TB_SETINDENT            (WM_USER + 71)
#endif
#ifndef TBSTYLE_FLAT
#define TBSTYLE_FLAT            0x0800
#endif
#ifndef CCS_NORESIZE
#define CCS_NORESIZE            0x00000004L
#endif

#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif
#ifndef LVM_GETEXTENDEDLISTVIEWSTYLE
#define LVM_GETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 55)
#endif
#ifndef LVM_GETCOLUMNA
#define LVM_GETCOLUMNA          (LVM_FIRST + 25)
#endif
#ifndef LVM_SETCOLUMNWIDTH
#define LVM_SETCOLUMNWIDTH      (LVM_FIRST + 30)
#endif
#ifndef LVM_GETITEMA
#define LVM_GETITEMA            (LVM_FIRST + 5)
#endif
#ifndef LVM_SETITEMA
#define LVM_SETITEMA            (LVM_FIRST + 6)
#endif
#ifndef LVM_DELETEITEM
#define LVM_DELETEITEM          (LVM_FIRST + 8)
#endif
#ifndef LVM_SETITEMSTATE
#define LVM_SETITEMSTATE        (LVM_FIRST + 43)
#endif
#ifndef LVM_GETITEMSTATE
#define LVM_GETITEMSTATE        (LVM_FIRST + 44)
#endif
#ifndef LVM_SETITEMTEXTA
#define LVM_SETITEMTEXTA        (LVM_FIRST + 46)
#endif
#ifndef LVM_GETCOUNTPERPAGE
#define LVM_GETCOUNTPERPAGE     (LVM_FIRST + 40)
#endif
#ifndef LVM_REDRAWITEMS
#define LVM_REDRAWITEMS         (LVM_FIRST + 21)
#endif
#ifndef LVHT_ABOVE
#define LVHT_ABOVE              0x0008
#endif
#ifndef LVHT_BELOW
#define LVHT_BELOW              0x0010
#endif
#ifndef LVHT_TORIGHT
#define LVHT_TORIGHT            0x0020
#endif
#ifndef LVHT_TOLEFT
#define LVHT_TOLEFT             0x0040
#endif
#ifndef LVS_TYPEMASK
#define LVS_TYPEMASK            0x0003
#endif
#ifndef LVS_NOCOLUMNHEADER
#define LVS_NOCOLUMNHEADER      0x4000
#endif

#ifndef TVIS_SELECTED
#define TVIS_SELECTED           0x0002
#endif
#ifndef TVIS_EXPANDED
#define TVIS_EXPANDED           0x0020
#endif
#ifndef TVIS_EXPANDEDONCE
#define TVIS_EXPANDEDONCE       0x0040
#endif
#ifndef TVC_UNKNOWN
#define TVC_UNKNOWN             0x0000
#endif
#ifndef TVC_BYMOUSE
#define TVC_BYMOUSE             0x0001
#endif
#ifndef TVC_BYKEYBOARD
#define TVC_BYKEYBOARD          0x0002
#endif
#ifndef TVGN_PREVIOUSVISIBLE
#define TVGN_PREVIOUSVISIBLE    0x0007
#endif
#ifndef TVGN_DROPHILITE
#define TVGN_DROPHILITE         0x0008
#endif
#ifndef TVN_SELCHANGING
#define TVN_SELCHANGINGA        (TVN_FIRST - 1)
#define TVN_SELCHANGING         TVN_SELCHANGINGA
#endif
#ifndef TVM_GETCOUNT
#define TVM_GETCOUNT            (TV_FIRST + 5)
#endif
#ifndef TVM_SETITEMA
#define TVM_SETITEMA            (TV_FIRST + 13)
#endif
#ifndef I_CHILDRENCALLBACK
#define I_CHILDRENCALLBACK      (-1)
#endif

#ifndef TCM_GETITEMRECT
#define TCM_GETITEMRECT         (TCM_FIRST + 10)
#endif
#ifndef TCM_SETCURFOCUS
#define TCM_SETCURFOCUS         (TCM_FIRST + 48)
#endif

// ---------------------------------------------------------------------------
// Shared helpers (ctldraw.cpp)
// ---------------------------------------------------------------------------

// Instance the classes are registered under: the EXE, which is also what the
// dialog templates are created with.
extern HINSTANCE g_vc2ctlInst;

// Per-window data pointer, in the class's first window extra LONG.
#define VC2CTL_EXTRA            sizeof(LONG)
#define Vc2GetData(hwnd)        ((void *)GetWindowLong((hwnd), 0))
#define Vc2SetData(hwnd, p)     SetWindowLong((hwnd), 0, (LONG)(p))

void *Vc2Alloc(size_t n);               // zero-filled; NULL on failure
void *Vc2Realloc(void *p, size_t n);
void  Vc2Free(void *p);
char *Vc2StrDup(const char *s);         // NULL in, NULL out
void  Vc2CopyText(char *dst, int cchDst, const char *src);

// Send WM_NOTIFY to the control's parent.  Fills hwndFrom / idFrom / code.
LRESULT Vc2Notify(HWND hwnd, UINT code, NMHDR *pnm);
LRESULT Vc2NotifySimple(HWND hwnd, UINT code);

// Drawing.  All colours are GetSysColor indexes NT 3.1 has.
void Vc2Fill(HDC hdc, int l, int t, int r, int b, COLORREF c);
void Vc2FillRect(HDC hdc, const RECT *prc, COLORREF c);
void Vc2Frame(HDC hdc, const RECT *prc, COLORREF tl, COLORREF br);
void Vc2DrawRaised(HDC hdc, const RECT *prc);   // push-button edge
void Vc2DrawSunken(HDC hdc, const RECT *prc);   // pressed / field edge
void Vc2DrawArrow(HDC hdc, int cx, int cy, int size, int dir, COLORREF c);
#define VC2_ARROW_UP            0
#define VC2_ARROW_DOWN          1
#define VC2_ARROW_LEFT          2
#define VC2_ARROW_RIGHT         3
void Vc2DrawFocus(HDC hdc, const RECT *prc);

// Font from WM_SETFONT, or the stock system font.
HFONT Vc2Font(HFONT hf);
int   Vc2FontHeight(HWND hwnd, HFONT hf);

// Registration helpers (vc2ctl.cpp).
BOOL Vc2RegisterClass(const char *name, WNDPROC proc, UINT style,
                      HCURSOR hcur, int brushColor);

// Per-control registration entry points.
BOOL Vc2RegisterTab(void);
BOOL Vc2RegisterTrackbar(void);
BOOL Vc2RegisterUpDown(void);
BOOL Vc2RegisterProgress(void);
BOOL Vc2RegisterListView(void);
BOOL Vc2RegisterTreeView(void);
BOOL Vc2RegisterToolbar(void);
BOOL Vc2RegisterToolTip(void);

// Tooltip window (tooltip.cpp), used by the toolbar.
#define VC2_TOOLTIP_CLASS       "Vc2ToolTip"
#define VC2_TOOLBAR_CLASS       "Vc2ToolbarWindow"
HWND Vc2TipCreate(HWND hwndOwner);
void Vc2TipShow(HWND hwndTip, const char *text, int x, int y);
void Vc2TipHide(HWND hwndTip);

#endif // VC2CTLI_H
