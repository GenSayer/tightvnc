// vc2commctl.h - common-control declarations missing from VC++ 2.x's commctrl.h
//
// This file is part of the TightVNC VC++ 2.x / NT 3.1 / Win32s port.
// Identical copies live in vncviewer\ and winvnc\.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
//
// The sources were written against MSVC 4.1's commctrl.h (the Windows 95 /
// NT 3.51 common controls: list view, tree view, tab, up-down, trackbar,
// progress bar, tooltips).  The commctrl.h that ships with VC++ 2.x predates
// most of those, so names such as TV_ITEM, LV_ITEM and HTREEITEM are simply
// undeclared - which is what produced
//
//     FileTransfer.h(46) : error C2239: unexpected token 'identifier'
//                          following declaration of 'TV_ITEM'
//
// Nothing here creates an import: the structures are plain data, and every
// "function" is a SendMessage macro.  At run time the controls come from
// COMCTL32 or from the built-in versions in vc2ctl (see vc2ctl/vc2ctl.h).
//
// HOW IT IS GUARDED
//
// Include it straight after commctrl.h.  Each control family is wrapped in a
// test for a macro that a commctrl.h providing that family would define
// (e.g. TVM_INSERTITEM for the tree view), so whatever the installed header
// already declares is left alone and only the gaps are filled.  The values and
// layouts are the Windows 95 / NT 3.51 ones (COMCTL32 4.00), i.e. exactly what
// MSVC 4.1 compiled this code against.
//
// The window-style constants are in vc2ctlstyles.h, which the .rc files
// include directly; this file is for C/C++ only.
// ---------------------------------------------------------------------------

#ifndef VC2COMMCTL_H
#define VC2COMMCTL_H

// Is the installed commctrl.h the pre-Windows 95 one?  VC++ 2.x's has the
// toolbar but no list view, and its TBBUTTON is the old NT 3.5 layout (see the
// toolbar section).  Tested here, before this file defines LVM_FIRST itself.
#if defined(TB_SETSTATE) && !defined(LVM_FIRST)
#define VC2COMMCTL_OLD_HEADER
#endif

// ===========================================================================
// Window styles: in vc2ctlstyles.h, which the .rc files also include.  They
// live in a separate file because the VC++ 2.x resource compiler cannot
// cope with the apostrophes in the comments of this one (see that file).
// ===========================================================================

#include "vc2ctlstyles.h"


#ifndef RC_INVOKED

// ===========================================================================
// NMHDR / WM_NOTIFY
//
// VC++ 2.x's winuser.h predates WM_NOTIFY.  Both copies of win32s_fix.h
// (force-included) declare NMHDR for VC++ 2.x and set _NMHDR_DEFINED, so this
// block only matters for a translation unit built without that header.
//
// msvc4 tree: a winuser.h that defines WM_NOTIFY (MSVC 4.x's) also declares
// NMHDR, so the block is skipped then too.
// ===========================================================================

#if !defined(_NMHDR_DEFINED) && !defined(WM_NOTIFY)
#define _NMHDR_DEFINED
typedef struct tagNMHDR {
    HWND hwndFrom;
    UINT idFrom;
    UINT code;
} NMHDR, FAR *LPNMHDR;
#endif

#ifndef WM_NOTIFY
#define WM_NOTIFY               0x004E
#endif

// SNDMSG is the commctrl.h spelling of SendMessage used by its helper macros
// (and by the ListView_SetExtendedListViewStyleEx shim in FileTransfer.cpp).
#ifndef SNDMSG
#define SNDMSG                  SendMessage
#endif

// "Ask me for the text" marker for list view / tree view / tooltip items.
#ifndef LPSTR_TEXTCALLBACK
#define LPSTR_TEXTCALLBACKA     ((LPSTR)-1L)
#define LPSTR_TEXTCALLBACK      LPSTR_TEXTCALLBACKA
#endif

// ===========================================================================
// Notification codes
// ===========================================================================

#ifndef NM_FIRST
#define NM_FIRST                (0U-  0U)
#endif
#ifndef NM_DBLCLK
#define NM_CLICK                (NM_FIRST-2)
#define NM_DBLCLK               (NM_FIRST-3)
#define NM_RETURN               (NM_FIRST-4)
#define NM_RCLICK               (NM_FIRST-5)
#define NM_RDBLCLK              (NM_FIRST-6)
#define NM_SETFOCUS             (NM_FIRST-7)
#define NM_KILLFOCUS            (NM_FIRST-8)
#endif
#ifndef LVN_FIRST
#define LVN_FIRST               (0U-100U)
#endif
#ifndef TVN_FIRST
#define TVN_FIRST               (0U-400U)
#endif
#ifndef TTN_FIRST
#define TTN_FIRST               (0U-520U)
#endif
#ifndef TCN_FIRST
#define TCN_FIRST               (0U-550U)
#endif
#ifndef UDN_FIRST
#define UDN_FIRST               (0U-721)
#endif

// ===========================================================================
// Toolbar (the viewer's toolbar; CreateToolbarEx is resolved at run time)
// ===========================================================================

#ifndef TB_SETSTATE
#define TBSTATE_CHECKED         0x01
#define TBSTATE_PRESSED         0x02
#define TBSTATE_ENABLED         0x04
#define TBSTATE_HIDDEN          0x08
#define TBSTATE_INDETERMINATE   0x10
#define TBSTATE_WRAP            0x20

#define TBSTYLE_BUTTON          0x00
#define TBSTYLE_SEP             0x01
#define TBSTYLE_CHECK           0x02
#define TBSTYLE_GROUP           0x04
#define TBSTYLE_CHECKGROUP      (TBSTYLE_GROUP | TBSTYLE_CHECK)
#define TBSTYLE_TOOLTIPS        0x0100
#define TBSTYLE_WRAPABLE        0x0200

#define TB_ENABLEBUTTON         (WM_USER + 1)
#define TB_CHECKBUTTON          (WM_USER + 2)
#define TB_PRESSBUTTON          (WM_USER + 3)
#define TB_HIDEBUTTON           (WM_USER + 4)
#define TB_SETSTATE             (WM_USER + 17)
#define TB_GETSTATE             (WM_USER + 18)

// A commctrl.h that has the toolbar messages declares TBBUTTON too, so it is
// only supplied together with them.
typedef struct _TBBUTTON {
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    BYTE bReserved[2];
    DWORD dwData;
    int iString;
} TBBUTTON, NEAR *PTBBUTTON, FAR *LPTBBUTTON;
typedef const TBBUTTON FAR *LPCTBBUTTON;
#endif

// VC++ 2.x's commctrl.h has the toolbar, but not these Windows 95 additions.
#ifndef TBSTATE_INDETERMINATE
#define TBSTATE_INDETERMINATE   0x10
#endif
#ifndef TBSTATE_WRAP
#define TBSTATE_WRAP            0x20
#endif
#ifndef TBSTYLE_TOOLTIPS
#define TBSTYLE_TOOLTIPS        0x0100
#endif
#ifndef TBSTYLE_WRAPABLE
#define TBSTYLE_WRAPABLE        0x0200
#endif

// VC++ 2.x's TBBUTTON is the NT 3.5 layout:
//
//     INT iBitmap; INT idCommand; BYTE fsState; BYTE fsStyle;
//     INT idsHelp; DWORD dwData; int iString;          -> 24 bytes
//
// The COMCTL32 the viewer actually runs against (Win32s 1.30, NT 3.51 and
// later) expects the Windows 95 layout, with BYTE bReserved[2] in place of
// idsHelp (20 bytes) - which is what the MSVC 4.1 build passed.  The toolbar
// code hands sizeof(TBBUTTON) to CreateToolbarEx, so with the old header the
// name is redirected to a Windows 95-layout struct.  CreateToolbarEx is called
// through Win32sApi with a void *, so nothing else sees the difference.
#ifdef VC2COMMCTL_OLD_HEADER
typedef struct _VNC_TBBUTTON95 {
    int iBitmap;
    int idCommand;
    BYTE fsState;
    BYTE fsStyle;
    BYTE bReserved[2];
    DWORD dwData;
    int iString;
} VNC_TBBUTTON95;
#define TBBUTTON                VNC_TBBUTTON95
#endif

// ===========================================================================
// Tooltips (TTN_NEEDTEXT from the viewer's toolbar)
// ===========================================================================

#ifndef TTN_NEEDTEXT
#define TTN_NEEDTEXTA           (TTN_FIRST - 0)
#define TTN_NEEDTEXT            TTN_NEEDTEXTA

typedef struct tagTOOLTIPTEXTA {
    NMHDR hdr;
    LPSTR lpszText;
    char szText[80];
    HINSTANCE hinst;
    UINT uFlags;
} TOOLTIPTEXTA, FAR *LPTOOLTIPTEXTA;
#define TOOLTIPTEXT             TOOLTIPTEXTA
#define LPTOOLTIPTEXT           LPTOOLTIPTEXTA
#endif

// ===========================================================================
// Trackbar
// ===========================================================================

#ifndef TBM_GETPOS
#define TBM_GETPOS              (WM_USER)
#define TBM_GETRANGEMIN         (WM_USER + 1)
#define TBM_GETRANGEMAX         (WM_USER + 2)
#define TBM_SETPOS              (WM_USER + 5)
#define TBM_SETRANGE            (WM_USER + 6)
#define TBM_SETRANGEMIN         (WM_USER + 7)
#define TBM_SETRANGEMAX         (WM_USER + 8)
#endif

// ===========================================================================
// Progress bar
// ===========================================================================

#ifndef PBM_SETPOS
#define PBM_SETRANGE            (WM_USER + 1)
#define PBM_SETPOS              (WM_USER + 2)
#define PBM_DELTAPOS            (WM_USER + 3)
#define PBM_SETSTEP             (WM_USER + 4)
#define PBM_STEPIT              (WM_USER + 5)
#endif

// ===========================================================================
// Up-down
// ===========================================================================

#ifndef UDM_SETBUDDY
#define UDM_SETRANGE            (WM_USER + 101)
#define UDM_GETRANGE            (WM_USER + 102)
#define UDM_SETPOS              (WM_USER + 103)
#define UDM_GETPOS              (WM_USER + 104)
#define UDM_SETBUDDY            (WM_USER + 105)
#define UDM_GETBUDDY            (WM_USER + 106)
#endif

// VC++ 2.x's commctrl.h has the up-down messages but not its WM_NOTIFY
// notification, so this is guarded separately.
#ifndef UDN_DELTAPOS
#define UDN_DELTAPOS            (UDN_FIRST - 1)

typedef struct _NM_UPDOWN {
    NMHDR hdr;
    int iPos;
    int iDelta;
} NM_UPDOWN, FAR *LPNM_UPDOWN;
#endif

// ===========================================================================
// Tab control
// ===========================================================================

#ifndef TCM_FIRST
#define TCM_FIRST               0x1300
#endif

#ifndef TCM_INSERTITEM
#define TCIF_TEXT               0x0001
#define TCIF_IMAGE              0x0002
#define TCIF_PARAM              0x0008

typedef struct _TC_ITEMA {
    UINT mask;
    UINT lpReserved1;
    UINT lpReserved2;
    LPSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} TC_ITEMA, FAR *LPTC_ITEMA;
#define TC_ITEM                 TC_ITEMA
#define LPTC_ITEM               LPTC_ITEMA

#define TCM_GETITEMCOUNT        (TCM_FIRST + 4)
#define TCM_GETITEMA            (TCM_FIRST + 5)
#define TCM_SETITEMA            (TCM_FIRST + 6)
#define TCM_INSERTITEMA         (TCM_FIRST + 7)
#define TCM_INSERTITEM          TCM_INSERTITEMA
#define TCM_DELETEITEM          (TCM_FIRST + 8)
#define TCM_DELETEALLITEMS      (TCM_FIRST + 9)
#define TCM_GETCURSEL           (TCM_FIRST + 11)
#define TCM_SETCURSEL           (TCM_FIRST + 12)
#define TCM_ADJUSTRECT          (TCM_FIRST + 40)
#define TCM_GETCURFOCUS         (TCM_FIRST + 47)

#define TCN_SELCHANGE           (TCN_FIRST - 1)
#define TCN_SELCHANGING         (TCN_FIRST - 2)

#define TabCtrl_InsertItem(hwnd, iItem, pitem) \
    (int)SendMessage((hwnd), TCM_INSERTITEM, (WPARAM)(int)(iItem), (LPARAM)(const TC_ITEM FAR*)(pitem))
#define TabCtrl_GetCurSel(hwnd) \
    (int)SendMessage((hwnd), TCM_GETCURSEL, 0, 0)
#define TabCtrl_SetCurSel(hwnd, i) \
    (int)SendMessage((hwnd), TCM_SETCURSEL, (WPARAM)(i), 0)
#define TabCtrl_GetCurFocus(hwnd) \
    (int)SendMessage((hwnd), TCM_GETCURFOCUS, 0, 0)
#define TabCtrl_AdjustRect(hwnd, bLarger, prc) \
    (int)SendMessage((hwnd), TCM_ADJUSTRECT, (WPARAM)(BOOL)(bLarger), (LPARAM)(RECT FAR *)(prc))
#endif

// ===========================================================================
// List view
// ===========================================================================

#ifndef LVM_FIRST
#define LVM_FIRST               0x1000
#endif

#ifndef LVM_INSERTITEM
#define LVIF_TEXT               0x0001
#define LVIF_IMAGE              0x0002
#define LVIF_PARAM              0x0004
#define LVIF_STATE              0x0008

#define LVIS_FOCUSED            0x0001
#define LVIS_SELECTED           0x0002

#define LVNI_ALL                0x0000
#define LVNI_FOCUSED            0x0001
#define LVNI_SELECTED           0x0002

#define LVCF_FMT                0x0001
#define LVCF_WIDTH              0x0002
#define LVCF_TEXT               0x0004
#define LVCF_SUBITEM            0x0008

#define LVCFMT_LEFT             0x0000
#define LVCFMT_RIGHT            0x0001
#define LVCFMT_CENTER           0x0002

#define LVHT_NOWHERE            0x0001
#define LVHT_ONITEMICON         0x0002
#define LVHT_ONITEMLABEL        0x0004
#define LVHT_ONITEMSTATEICON    0x0008
#define LVHT_ONITEM             (LVHT_ONITEMICON | LVHT_ONITEMLABEL | LVHT_ONITEMSTATEICON)

typedef struct _LV_ITEMA {
    UINT mask;
    int iItem;
    int iSubItem;
    UINT state;
    UINT stateMask;
    LPSTR pszText;
    int cchTextMax;
    int iImage;
    LPARAM lParam;
} LV_ITEMA, FAR *LPLV_ITEMA;
#define LV_ITEM                 LV_ITEMA
#define LPLV_ITEM               LPLV_ITEMA

typedef struct _LV_COLUMNA {
    UINT mask;
    int fmt;
    int cx;
    LPSTR pszText;
    int cchTextMax;
    int iSubItem;
} LV_COLUMNA, FAR *LPLV_COLUMNA;
#define LV_COLUMN               LV_COLUMNA
#define LPLV_COLUMN             LPLV_COLUMNA

typedef struct _LV_DISPINFO {
    NMHDR hdr;
    LV_ITEMA item;
} LV_DISPINFOA;
#define LV_DISPINFO             LV_DISPINFOA

typedef struct _LV_HITTESTINFO {
    POINT pt;
    UINT flags;
    int iItem;
} LV_HITTESTINFO, FAR *LPLV_HITTESTINFO;

#define LVM_DELETEALLITEMS      (LVM_FIRST + 9)
#define LVM_INSERTITEMA         (LVM_FIRST + 7)
#define LVM_INSERTITEM          LVM_INSERTITEMA
#define LVM_GETNEXTITEM         (LVM_FIRST + 12)
#define LVM_HITTEST             (LVM_FIRST + 18)
#define LVM_INSERTCOLUMNA       (LVM_FIRST + 27)
#define LVM_INSERTCOLUMN        LVM_INSERTCOLUMNA
#define LVM_GETITEMTEXTA        (LVM_FIRST + 45)
#define LVM_GETITEMTEXT         LVM_GETITEMTEXTA
#define LVM_GETSELECTEDCOUNT    (LVM_FIRST + 50)

#define LVN_GETDISPINFOA        (LVN_FIRST - 50)
#define LVN_GETDISPINFO         LVN_GETDISPINFOA

#define ListView_DeleteAllItems(hwnd) \
    (BOOL)SendMessage((hwnd), LVM_DELETEALLITEMS, 0, 0L)
#define ListView_InsertItem(hwnd, pitem) \
    (int)SendMessage((hwnd), LVM_INSERTITEM, 0, (LPARAM)(const LV_ITEM FAR*)(pitem))
#define ListView_GetNextItem(hwnd, i, flags) \
    (int)SendMessage((hwnd), LVM_GETNEXTITEM, (WPARAM)(int)(i), MAKELPARAM((flags), 0))
#define ListView_HitTest(hwndLV, pinfo) \
    (int)SendMessage((hwndLV), LVM_HITTEST, 0, (LPARAM)(LV_HITTESTINFO FAR*)(pinfo))
#define ListView_InsertColumn(hwnd, iCol, pcol) \
    (int)SendMessage((hwnd), LVM_INSERTCOLUMN, (WPARAM)(int)(iCol), (LPARAM)(const LV_COLUMN FAR*)(pcol))
#define ListView_GetItemText(hwndLV, i, iSubItem_, pszText_, cchTextMax_) \
{ LV_ITEM _ms_lvi;\
  _ms_lvi.iSubItem = iSubItem_;\
  _ms_lvi.cchTextMax = cchTextMax_;\
  _ms_lvi.pszText = pszText_;\
  SendMessage((hwndLV), LVM_GETITEMTEXT, (WPARAM)(i), (LPARAM)(LV_ITEM FAR *)&_ms_lvi);\
}
#define ListView_GetSelectedCount(hwndLV) \
    (UINT)SendMessage((hwndLV), LVM_GETSELECTEDCOUNT, 0, 0L)
#endif

// ===========================================================================
// Tree view
// ===========================================================================

#ifndef TV_FIRST
#define TV_FIRST                0x1100
#endif

#ifndef TVM_INSERTITEM
struct _TREEITEM;
typedef struct _TREEITEM FAR* HTREEITEM;

#define TVIF_TEXT               0x0001
#define TVIF_IMAGE              0x0002
#define TVIF_PARAM              0x0004
#define TVIF_STATE              0x0008
#define TVIF_HANDLE             0x0010
#define TVIF_SELECTEDIMAGE      0x0020
#define TVIF_CHILDREN           0x0040

#define TVI_ROOT                ((HTREEITEM)0xFFFF0000)
#define TVI_FIRST               ((HTREEITEM)0xFFFF0001)
#define TVI_LAST                ((HTREEITEM)0xFFFF0002)
#define TVI_SORT                ((HTREEITEM)0xFFFF0003)

#define TVGN_ROOT               0x0000
#define TVGN_NEXT               0x0001
#define TVGN_PREVIOUS           0x0002
#define TVGN_PARENT             0x0003
#define TVGN_CHILD              0x0004
#define TVGN_CARET              0x0009

#define TVE_COLLAPSE            0x0001
#define TVE_EXPAND              0x0002
#define TVE_TOGGLE              0x0003
#define TVE_COLLAPSERESET       0x8000

typedef struct _TV_ITEMA {
    UINT      mask;
    HTREEITEM hItem;
    UINT      state;
    UINT      stateMask;
    LPSTR     pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
} TV_ITEMA, FAR *LPTV_ITEMA;
#define TV_ITEM                 TV_ITEMA
#define LPTV_ITEM               LPTV_ITEMA

typedef struct _TV_INSERTSTRUCTA {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
    TV_ITEMA  item;
} TV_INSERTSTRUCTA, FAR *LPTV_INSERTSTRUCTA;
#define TV_INSERTSTRUCT         TV_INSERTSTRUCTA
#define LPTV_INSERTSTRUCT       LPTV_INSERTSTRUCTA

typedef struct _NM_TREEVIEWA {
    NMHDR    hdr;
    UINT     action;
    TV_ITEMA itemOld;
    TV_ITEMA itemNew;
    POINT    ptDrag;
} NM_TREEVIEWA, FAR *LPNM_TREEVIEWA;
#define NM_TREEVIEW             NM_TREEVIEWA
#define LPNM_TREEVIEW           LPNM_TREEVIEWA

#define TVM_INSERTITEMA         (TV_FIRST + 0)
#define TVM_INSERTITEM          TVM_INSERTITEMA
#define TVM_DELETEITEM          (TV_FIRST + 1)
#define TVM_EXPAND              (TV_FIRST + 2)
#define TVM_GETNEXTITEM         (TV_FIRST + 10)
#define TVM_SELECTITEM          (TV_FIRST + 11)
#define TVM_GETITEMA            (TV_FIRST + 12)
#define TVM_GETITEM             TVM_GETITEMA

#define TVN_SELCHANGEDA         (TVN_FIRST - 2)
#define TVN_SELCHANGED          TVN_SELCHANGEDA
#define TVN_ITEMEXPANDINGA      (TVN_FIRST - 5)
#define TVN_ITEMEXPANDING       TVN_ITEMEXPANDINGA

#define TreeView_InsertItem(hwnd, lpis) \
    (HTREEITEM)SendMessage((hwnd), TVM_INSERTITEM, 0, (LPARAM)(LPTV_INSERTSTRUCT)(lpis))
#define TreeView_DeleteItem(hwnd, hitem) \
    (BOOL)SendMessage((hwnd), TVM_DELETEITEM, 0, (LPARAM)(HTREEITEM)(hitem))
#define TreeView_Expand(hwnd, hitem, code) \
    (BOOL)SendMessage((hwnd), TVM_EXPAND, (WPARAM)(code), (LPARAM)(HTREEITEM)(hitem))
#define TreeView_GetNextItem(hwnd, hitem, code) \
    (HTREEITEM)SendMessage((hwnd), TVM_GETNEXTITEM, (WPARAM)(code), (LPARAM)(HTREEITEM)(hitem))
#define TreeView_GetChild(hwnd, hitem)  TreeView_GetNextItem(hwnd, hitem, TVGN_CHILD)
#define TreeView_GetParent(hwnd, hitem) TreeView_GetNextItem(hwnd, hitem, TVGN_PARENT)
#define TreeView_GetItem(hwnd, pitem) \
    (BOOL)SendMessage((hwnd), TVM_GETITEM, 0, (LPARAM)(TV_ITEM FAR*)(pitem))
#endif

// ===========================================================================
// List view / tree view scrolling helpers
//
// Implemented by the built-in list and tree views in vc2ctl.  One guard per
// name, since a commctrl.h that has the list and tree views may still lack
// some of these.
// ===========================================================================

#ifndef LVIR_BOUNDS
#define LVIR_BOUNDS             0
#define LVIR_ICON               1
#define LVIR_LABEL              2
#define LVIR_SELECTBOUNDS       3
#endif
#ifndef LVM_GETITEMCOUNT
#define LVM_GETITEMCOUNT        (LVM_FIRST + 4)
#endif
#ifndef LVM_GETITEMRECT
#define LVM_GETITEMRECT         (LVM_FIRST + 14)
#endif
#ifndef LVM_ENSUREVISIBLE
#define LVM_ENSUREVISIBLE       (LVM_FIRST + 19)
#endif
#ifndef LVM_SCROLL
#define LVM_SCROLL              (LVM_FIRST + 20)
#endif
#ifndef LVM_GETCOLUMNWIDTH
#define LVM_GETCOLUMNWIDTH      (LVM_FIRST + 29)
#endif
#ifndef LVM_GETTOPINDEX
#define LVM_GETTOPINDEX         (LVM_FIRST + 39)
#endif

#ifndef ListView_GetItemCount
#define ListView_GetItemCount(hwnd) \
    (int)SendMessage((hwnd), LVM_GETITEMCOUNT, 0, 0L)
#endif
#ifndef ListView_GetItemRect
#define ListView_GetItemRect(hwnd, i, prc, code) \
    (BOOL)SendMessage((hwnd), LVM_GETITEMRECT, (WPARAM)(int)(i), \
        ((prc) ? (((RECT FAR *)(prc))->left = (code), (LPARAM)(RECT FAR *)(prc)) : (LPARAM)(RECT FAR *)NULL))
#endif
#ifndef ListView_EnsureVisible
#define ListView_EnsureVisible(hwndLV, i, fPartialOK) \
    (BOOL)SendMessage((hwndLV), LVM_ENSUREVISIBLE, (WPARAM)(int)(i), MAKELPARAM((fPartialOK), 0))
#endif
#ifndef ListView_Scroll
#define ListView_Scroll(hwndLV, dx, dy) \
    (BOOL)SendMessage((hwndLV), LVM_SCROLL, (WPARAM)(int)(dx), (LPARAM)(int)(dy))
#endif
#ifndef ListView_GetColumnWidth
#define ListView_GetColumnWidth(hwnd, iCol) \
    (int)SendMessage((hwnd), LVM_GETCOLUMNWIDTH, (WPARAM)(int)(iCol), 0)
#endif
#ifndef ListView_GetTopIndex
#define ListView_GetTopIndex(hwndLV) \
    (int)SendMessage((hwndLV), LVM_GETTOPINDEX, 0, 0)
#endif

#ifndef TVGN_FIRSTVISIBLE
#define TVGN_FIRSTVISIBLE       0x0005
#endif
#ifndef TVGN_NEXTVISIBLE
#define TVGN_NEXTVISIBLE        0x0006
#endif
#ifndef TVM_GETITEMRECT
#define TVM_GETITEMRECT         (TV_FIRST + 4)
#endif
#ifndef TVM_ENSUREVISIBLE
#define TVM_ENSUREVISIBLE       (TV_FIRST + 20)
#endif
#ifndef TVN_ITEMEXPANDED
#define TVN_ITEMEXPANDEDA       (TVN_FIRST - 6)
#define TVN_ITEMEXPANDED        TVN_ITEMEXPANDEDA
#endif

#ifndef TreeView_GetRoot
#define TreeView_GetRoot(hwnd) \
    TreeView_GetNextItem(hwnd, NULL, TVGN_ROOT)
#endif
#ifndef TreeView_GetFirstVisible
#define TreeView_GetFirstVisible(hwnd) \
    TreeView_GetNextItem(hwnd, NULL, TVGN_FIRSTVISIBLE)
#endif
#ifndef TreeView_GetNextVisible
#define TreeView_GetNextVisible(hwnd, hitem) \
    TreeView_GetNextItem(hwnd, hitem, TVGN_NEXTVISIBLE)
#endif
#ifndef TreeView_GetItemRect
#define TreeView_GetItemRect(hwnd, hitem, prc, code) \
    (*(HTREEITEM FAR *)(prc) = (hitem), \
     (BOOL)SendMessage((hwnd), TVM_GETITEMRECT, (WPARAM)(code), (LPARAM)(RECT FAR *)(prc)))
#endif
#ifndef TreeView_EnsureVisible
#define TreeView_EnsureVisible(hwnd, hitem) \
    (BOOL)SendMessage((hwnd), TVM_ENSUREVISIBLE, 0, (LPARAM)(HTREEITEM)(hitem))
#endif

#endif // !RC_INVOKED

#endif // VC2COMMCTL_H
