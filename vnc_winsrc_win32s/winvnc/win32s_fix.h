#ifndef WIN32S_FIX_H
#define WIN32S_FIX_H

// ==========================================================================
// 0. THE one and only definition of bool/true/false for this project.
//
// This header is force-included into every translation unit (/FI"win32s_fix.h")
// so it is guaranteed to be seen before any project header.  MSVC 4.1 has no
// native bool, so bool is an int here.
//
// It is critical that this appears in exactly one place.
//
// In vncviewer, several headers each carried their own copy (VNCOptions.h,
// CapsContainer.h, FileTransfer.h, FileTransferItemInfo.h said "typedef int
// bool", while Exception.h and Log.h said "typedef bool" - which declares
// nothing at all).
//
// In winvnc the same problem existed in Log.h and vncKeymap.h, made worse by a
// typo: both were guarded by "#ifndef _BOOLHACKDEFINED" but vncKeymap.h defined
// _BOOKHACKDEFINED (K, not L), so the guard never fired and whichever header
// was included first silently won.
//
// Because those headers are reached in different orders by different .cpp
// files, the size of bool - and therefore the memory layout of every class
// holding a bool member, including vncServer, vncClient and VNCOptions - could
// differ between object files.  That is an ODR violation and it silently
// corrupts memory as soon as such an object is constructed in one file and used
// in another.
// ==========================================================================
#ifndef VNC_BOOL_DEFINED
#define VNC_BOOL_DEFINED
typedef int bool;
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif
#endif // VNC_BOOL_DEFINED

// ==========================================================================
// 1. Core primitive types.
//
// KEEP THESE.  This header is force-included with /FI before any other header,
// so at this point windows.h has not been seen and the NMHDR definition below
// would not compile without them.  Removing them is what produced the
// "missing construct" errors.
//
// They are safe only because they are *identical* to what windows.h declares in
// a non-STRICT build (UINT = unsigned int, HANDLE = void*, HWND = HANDLE).
// C++ permits a typedef to be repeated with the same type, which is why there
// is no redefinition error.
//
// TWO THINGS TO WATCH:
//   * If STRICT is ever defined, windows.h declares HWND as "struct HWND__ *"
//     and this typedef becomes a hard conflict.  Do not define STRICT.
//   * The NMHDR below must stay layout-identical to the SDK's (HWND, UINT,
//     UINT).  It is - but if commctrl.h is ever reached in a translation unit
//     that also sees this file and the compiler accepts both, the two must
//     agree or WM_NOTIFY handling reads the wrong offsets.
// ==========================================================================
typedef unsigned int UINT;
typedef void*	     HANDLE;
typedef HANDLE       HWND;

// Not needed for WinVNC
/* struct tagNMHDR {
	HWND	hwndFrom;
	UINT	idFrom;
	UINT	code;
};

typedef struct tagNMHDR NMHDR;
typedef struct tagNMHDR* LPNMHDR; */

#define _NMHDR_DEFINED

// 1. Globally available MessageBox Flags
#ifndef MB_TOPMOST
#define MB_TOPMOST          0x00040000L
#endif

// 2. Microsoft CRT Debugging Report Macros & Constants
#ifndef _CRT_WARN
#define _CRT_WARN           0
#endif

// _RPT1
//
// Same C4005 situation as MB_ICONERROR above, but with an important difference:
// crtdbg.h's own definition is the one we WANT in a debug build, and ours is a
// no-op stub for release.
//
//   crtdbg.h(47) : warning C4005: '_RPT1' : macro redefinition
//
// crtdbg.h is included by stdhdrs.h, i.e. after this file, so ITS definition
// wins - which is correct in both configurations:
//
//   * Release (NDEBUG): crtdbg.h defines _RPTn as ((void)0) itself, so the
//     result is the same as our stub.
//   * Debug: crtdbg.h defines the real reporting macro, which is what a debug
//     build should have.
//
// So this definition only matters for a translation unit that somehow does not
// reach crtdbg.h.  Guarded with #undef to keep it quiet.
#undef _RPT1
#define _RPT1(reportType, format, arg1) \
        ((void)0) // zero-overhead stub if crtdbg.h is not reached

// 1. Core Windows Message Extensions
#ifndef WM_NOTIFY
#define WM_NOTIFY            0x004E
#endif

#ifndef WM_HELP
#define WM_HELP              0x0053
#endif


// 2. Button Control Messages (Typo fix for BM_SETIMATE)
#ifndef BM_SETIMAGE
#define BM_SETIMAGE          0x00F7
#endif


// 3. User Graphics Loading Infrastructure (Windows 95+ Extensions)
//
// These are constants only - defining them cannot create an import.  The
// *functions* that use them (LoadImage) must still be resolved at run time; see
// Win32sLoadImageIcon in Win32sApi.h.
#ifndef IMAGE_BITMAP
#define IMAGE_BITMAP         0
#endif

#ifndef IMAGE_ICON
#define IMAGE_ICON           1
#endif

#ifndef LR_SHARED
#define LR_SHARED            0x8000
#endif

// MB_ICONERROR / MB_ICONWARNING
//
// WARNING C4005 EXPLAINED.  These #ifndef guards do NOT work, because this file
// is force-included with /FI - i.e. BEFORE windows.h.  At this point the macros
// genuinely are undefined, so we define them; winuser.h then defines them again
// at lines 5616-5617 and the compiler reports:
//
//   winuser.h(5616) : warning C4005: 'MB_ICONWARNING' : macro redefinition
//   winuser.h(5617) : warning C4005: 'MB_ICONERROR' : macro redefinition
//
// The values are identical (MB_ICONERROR == MB_ICONHAND == 0x10,
// MB_ICONWARNING == MB_ICONEXCLAMATION == 0x30), so the redefinition is benign -
// but it is noise on every single translation unit.
//
// #undef immediately before defining is the clean fix: it makes the intent
// explicit ("provide these if the SDK will not") and lets winuser.h's later
// definition win without complaint.  Note we cannot simply delete these: the
// MSVC 4.1 winuser.h does define them, but only when WINVER >= 0x0400, and this
// build deliberately does not set WINVER.
#undef MB_ICONERROR
#define MB_ICONERROR        0x00000010L // == MB_ICONHAND / MB_ICONSTOP

#undef MB_ICONWARNING
#define MB_ICONWARNING      0x00000030L // == MB_ICONEXCLAMATION

#ifndef BM_CLICK
#define BM_CLICK             0x00F5
#endif

// ==========================================
// Win32s Button State Constants (BST_*)
// ==========================================

#ifndef BST_UNCHECKED
#define BST_UNCHECKED      0x0000
#endif

#ifndef BST_CHECKED
#define BST_CHECKED        0x0001
#endif

#ifndef BST_INDETERMINATE
#define BST_INDETERMINATE  0x0002
#endif

// ==========================================
// Win32s Desktop Geometry Fallbacks
// ==========================================
//
// The SystemParametersInfo() function-like macro that used to live here has
// been REMOVED.  It was defined as:
//
//   #define SystemParametersInfo(action, uiParam, pvParam, fWinIni) \
//       ((action) == SPI_GETWORKAREA ? ( ...fill *(LPRECT)pvParam... ) : \
//        (SystemParametersInfo)(action, uiParam, pvParam, fWinIni))
//
// Three problems:
//   1. The else-branch still references the real SystemParametersInfo, so the
//      name stays in the EXE's import table.  On Win32s the loader cannot
//      resolve it and the process never starts - no window, no message box.
//   2. It expands for *every* action, blind-casting pvParam to LPRECT.
//   3. pvParam is evaluated four times.
//
// Use Win32sGetWorkArea() from Win32sApi.h instead.
#ifndef SPI_GETWORKAREA
#define SPI_GETWORKAREA 48 // Windows 95/NT standard constant value
#endif

// ==========================================
// Win32s Keyboard Layout Flags (KLF_*)
// ==========================================

#ifndef KLF_ACTIVATE
#define KLF_ACTIVATE        0x00000001
#endif

#ifndef KLF_SUBSTITUTE_OK
#define KLF_SUBSTITUTE_OK   0x00000002
#endif

#ifndef KLF_REORDER
#define KLF_REORDER         0x00000008
#endif

#ifndef KLF_REPLACELANG
#define KLF_REPLACELANG     0x00000010
#endif

#ifndef KLF_NOTELLSHELL
#define KLF_NOTELLSHELL     0x00000080
#endif

// ==========================================================================
// GetDeviceCaps indices used by the Win32s display-format detection.
//
// All of these are core Windows 3.0/3.1 GDI, but MSVC 4.1's wingdi.h may not
// declare every one depending on WINVER.  They are plain integers - defining
// them creates no import.
// ==========================================================================
#ifndef NUMCOLORS
#define NUMCOLORS       24
#endif
#ifndef SIZEPALETTE
#define SIZEPALETTE     104
#endif
#ifndef COLORRES
#define COLORRES        108
#endif

// ==========================================================================
// Extended window styles used by the server's dialogs.
//
// WS_EX_CONTROLPARENT is Win95+ and may not be declared by the MSVC 4.1
// headers depending on WINVER.  It is only a style bit - defining it creates no
// import - and vncProperties.cpp applies it at run time to restore Tab
// navigation into the property pages (the resource can no longer carry an
// EXSTYLE line; see WinVNC.rc).  Windows 3.1 ignores unknown style bits.
// ==========================================================================
#ifndef WS_EX_CONTROLPARENT
#define WS_EX_CONTROLPARENT 0x00010000L
#endif
#ifndef WS_EX_TOOLWINDOW
#define WS_EX_TOOLWINDOW    0x00000080L
#endif

// ==========================================================================
// Socket shutdown constants.
//
// SD_BOTH and friends are WinSock 2 names.  WinSock 1.1 (which is what Win32s
// has) documents the same values as plain integers, so these are correct - but
// they must be defined, because wsock32.lib's header may not have them.
// ==========================================================================
#ifndef SD_RECEIVE
#define SD_RECEIVE          0
#endif
#ifndef SD_SEND
#define SD_SEND             1
#endif
#ifndef SD_BOTH
#define SD_BOTH             2
#endif


#endif // WIN32S_FIX_H