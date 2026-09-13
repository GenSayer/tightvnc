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
// WIN32S / WINDOWS 3.1 SINGLE-THREADED BUILD
// ==========================================================================
//
// The entire NT thread implementation has been removed.  Every class that
// used to live here is now either an inline no-op in omnithread.h or gone
// altogether:
//
//   omni_mutex      -> inline no-op (no CRITICAL_SECTION)
//   omni_condition  -> removed (unused by the viewer, needs a real wait)
//   omni_semaphore  -> inline counter
//   omni_thread     -> removed entirely
//
// Why this had to go, specifically:
//
//  1. "static omni_thread::init_t omni_thread_init;" at the bottom of the old
//     omnithread.h gave every translation unit a pre-WinMain constructor which
//     called TlsAlloc(), TlsSetValue(), DuplicateHandle() and operator new.
//     Under Win32s the TLS calls are not dependable and the fabricated
//     "self_tls_index = 1" workaround wrote through a TLS slot the process
//     does not own.  Any failure there kills the process before a window or a
//     message box can exist - the observed silent crash at launch.
//
//  2. omni_thread::start() threw omni_thread_fatal(ERROR_NOT_SUPPORTED) on
//     Win32s.  ClientConnection::Run() ends with start_undetached(), so every
//     connection attempt threw a *type the caller does not catch*
//     (VNCviewerApp32::NewConnection catches AuthException and Exception, not
//     omni_thread_fatal).  An uncaught throw calls terminate()/abort() - the
//     observed crash "on any platform when making a connection".
//
//  3. omni_thread::exit() called ExitThread(0) on the Win32s path, which on
//     Win32s terminates the whole process without unwinding.
//
// This file is intentionally left with no code so that omnithread.lib still
// builds and links (the makefile and the .lib reference are unchanged).  The
// dummy symbol below exists only because some librarians object to an object
// module with an empty COMDAT/section list.
// ==========================================================================

#include "omnithread.h"

extern "C" int omnithread_win32s_single_threaded_build = 1;
