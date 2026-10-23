/* vc2ctlstyles.h - common-control window styles for VC++ 2.x builds
 *
 * Included by res\vncviewer.rc and by vc2commctl.h.  The resource compiler
 * needs these because VC++ 2.x headers do not declare the list view and
 * tree view styles, and its windows.h does not pull in commctrl.h when
 * RC_INVOKED is defined, so even the up-down and trackbar styles are missing
 * there.
 *
 * KEEP THIS FILE RESOURCE-COMPILER SAFE: only #ifndef / #define / #endif and
 * C-style comments, and no apostrophes anywhere.  The VC++ 2.x resource
 * compiler preprocessor treats a single quote inside a comment as the start
 * of a character constant and silently swallows the following lines - which
 * is how these definitions went missing when they lived in vc2commctl.h.
 *
 * Values are the Windows 95 / NT 3.51 ones; each group is skipped if a
 * header already defined it.
 */

#ifndef VC2CTLSTYLES_H
#define VC2CTLSTYLES_H

#ifndef LVS_REPORT
#define LVS_ICON                0x0000
#define LVS_REPORT              0x0001
#define LVS_SMALLICON           0x0002
#define LVS_LIST                0x0003
#define LVS_SINGLESEL           0x0004
#define LVS_SHOWSELALWAYS       0x0008
#define LVS_SORTASCENDING       0x0010
#define LVS_SORTDESCENDING      0x0020
#define LVS_NOSORTHEADER        0x8000
#endif

#ifndef TVS_HASBUTTONS
#define TVS_HASBUTTONS          0x0001
#define TVS_HASLINES            0x0002
#define TVS_LINESATROOT         0x0004
#define TVS_EDITLABELS          0x0008
#define TVS_DISABLEDRAGDROP     0x0010
#define TVS_SHOWSELALWAYS       0x0020
#endif

#ifndef UDS_ALIGNRIGHT
#define UDS_WRAP                0x0001
#define UDS_SETBUDDYINT         0x0002
#define UDS_ALIGNRIGHT          0x0004
#define UDS_ALIGNLEFT           0x0008
#define UDS_AUTOBUDDY           0x0010
#define UDS_ARROWKEYS           0x0020
#endif
#ifndef UDS_HORZ
#define UDS_HORZ                0x0040
#endif
#ifndef UDS_NOTHOUSANDS
#define UDS_NOTHOUSANDS         0x0080
#endif

#ifndef TBS_AUTOTICKS
#define TBS_AUTOTICKS           0x0001
#endif
#ifndef TBS_VERT
#define TBS_VERT                0x0002
#endif
#ifndef TBS_HORZ
#define TBS_HORZ                0x0000
#endif
#ifndef TBS_NOTICKS
#define TBS_NOTICKS             0x0010
#endif

#endif /* VC2CTLSTYLES_H */
