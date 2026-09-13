// omnithread/win32s_fix.h  (winvnc)
//
// The omnithread sub-project is built with /FI"win32s_fix.h" and compiles from
// its own directory, so it picks up THIS file rather than ..\win32s_fix.h.
//
// Keeping two independently-edited copies of the same force-include is exactly
// how the bool/true/false definitions drifted apart in the first place (see the
// long note in ..\win32s_fix.h), so this file does nothing but include the one
// real copy.

#ifndef WIN32S_FIX_OMNITHREAD_SHIM_H
#define WIN32S_FIX_OMNITHREAD_SHIM_H

#include "..\win32s_fix.h"

#endif
