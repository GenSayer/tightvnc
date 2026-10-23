//				Package : omnithread
// omnithread/nt.cc		Created : 6/95 tjr
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
// ==========================================================================
//
// The NT thread implementation is gone.  Everything omnithread.h now declares
// is an inline no-op, so there is nothing to compile here.
//
// Specifically removed, and why it mattered:
//
//  1. "static omni_thread::init_t omni_thread_init;" at the bottom of the old
//     omnithread.h gave every translation unit a pre-WinMain constructor that
//     called TlsAlloc(), TlsSetValue(), DuplicateHandle() and operator new.
//     Under Win32s the TLS calls are not dependable, and a failure there kills
//     the process before any window or message box can exist.
//
//  2. omni_thread::start()/start_undetached() threw
//     omni_thread_fatal(ERROR_NOT_SUPPORTED) on Win32s.  Every one of the
//     server's five thread classes called it during connection setup, and none
//     of the callers catch omni_thread_fatal - an uncaught throw calls
//     terminate().
//
//  3. omni_thread::exit() called ExitThread(0), which on Win32s terminates the
//     whole process without unwinding.
//
//  4. omni_condition::wait() blocked on a kernel object.  With one thread that
//     is an unconditional deadlock; the replacement in omnithread.h pumps
//     messages instead.
//
// The dummy symbol below exists only so that omnithread.lib still contains a
// valid object module.
// ==========================================================================

#include "omnithread.h"

extern "C" int omnithread_win32s_single_threaded_build_winvnc = 1;
