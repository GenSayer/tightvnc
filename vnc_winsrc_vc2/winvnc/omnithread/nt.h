//				Package : omnithread
// omnithread/nt.h		Created : 6/95 tjr
//
//    Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//    This file is part of the omnithread library
//
//    The omnithread library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//    02111-1307, USA
//

// ==========================================================================
// WIN32S / WINDOWS 3.1 SINGLE-THREADED BUILD  (winvnc)
//
// The NT implementation macros are gone.  The classes omnithread.h declares no
// longer have any platform-specific state:
//
//   OMNI_MUTEX_IMPLEMENTATION      was  CRITICAL_SECTION crit;
//   OMNI_CONDITION_IMPLEMENTATION  was  CRITICAL_SECTION + a waiter list
//   OMNI_SEMAPHORE_IMPLEMENTATION  was  HANDLE nt_sem;
//   OMNI_THREAD_IMPLEMENTATION     was  HANDLE handle; DWORD nt_id; ...
//   OMNI_THREAD_WRAPPER            was  the _beginthreadex entry point
//
// None of them exist on Win32s in any usable form.  This file is kept, and kept
// includable, only so that a stray "#include <nt.h>" or a stale .dep entry
// still works.
// ==========================================================================

#ifndef __omnithread_nt_h_
#define __omnithread_nt_h_

#include <windows.h>

#endif // __omnithread_nt_h_
