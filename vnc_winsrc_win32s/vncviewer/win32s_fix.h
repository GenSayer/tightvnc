#ifndef WIN32S_FIX_H
#define WIN32S_FIX_H

// ==========================================================================
// 0. THE one and only definition of bool/true/false for this project.
//
// This header is force-included into every translation unit (/FI"win32s_fix.h")
// so it is guaranteed to be seen before any project header.  MSVC 4.1 has no
// native bool, so bool is an int here.
//
// It is critical that this appears in exactly one place.  Previously several
// headers each carried their own copy (VNCOptions.h, CapsContainer.h,
// FileTransfer.h, FileTransferItemInfo.h all said "typedef int bool", while
// Exception.h and Log.h said "typedef bool" which declares nothing).  Because
// those headers are reached in different orders by different .cpp files, the
// size of bool - and therefore the memory layout of every class holding a bool
// member, including VNCOptions and ClientConnection - could differ between
// object files.  That is an ODR violation and it silently corrupts memory as
// soon as such an object is constructed in one file and used in another.
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

struct tagNMHDR {
	HWND	hwndFrom;
	UINT	idFrom;
	UINT	code;
};

typedef struct tagNMHDR NMHDR;
typedef struct tagNMHDR* LPNMHDR;

#define _NMHDR_DEFINED

// 1. Globally available MessageBox Flags
#ifndef MB_TOPMOST
#define MB_TOPMOST          0x00040000L
#endif

// 2. Microsoft CRT Debugging Report Macros & Constants
#ifndef _CRT_WARN
#define _CRT_WARN           0
#endif

#ifndef _RPT1
#define _RPT1(reportType, format, arg1) \
        ((void)0) // Converts debug report statements to zero-overhead stubs in Release mode
#endif

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

#ifndef MB_ICONERROR
#define MB_ICONERROR        0x00000010L // Maps directly to MB_ICONHAND / MB_ICONSTOP
#endif

#ifndef MB_ICONWARNING
#define MB_ICONWARNING      0x00000030L // Maps directly to MB_ICONEXCLAMATION
#endif

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