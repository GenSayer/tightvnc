//				Package : omnithread
// omnithread.h			Created : 7/94 tjr
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
// Win32s has no threads: there is exactly one thread of execution per
// process, TLS is not usable in the way NT/95 uses it, _beginthreadex does
// not exist in the single-threaded (/ML) CRT, and critical sections are
// meaningless.  The whole viewer has therefore been converted to a single
// threaded design (see ClientConnection::PumpIdle and the idle loop in
// vncviewer.cpp).
//
// This header keeps the *names* used all over the source tree so that the
// hundreds of "omni_mutex_lock l(m_xxxMutex);" statements still compile, but
// every one of them is now a zero-cost no-op.  Nothing here creates a kernel
// object, allocates memory, touches TLS, or runs code before WinMain.
//
// Deliberately removed compared with the original header:
//   * class omni_thread and all of its machinery (start/join/exit/self)
//   * the file-scope object "static omni_thread::init_t omni_thread_init;"
//     which ran a constructor in *every* translation unit before WinMain.
//     On Win32s that constructor called TlsAlloc/TlsSetValue/DuplicateHandle
//     and allocated an omni_thread with new.  A failure there aborts the
//     process before any window exists, which is exactly the "crashes on
//     launch, no message box" symptom.
//   * omni_condition (it needs a real wait primitive and is not used by the
//     viewer).
//
// The exception classes are kept because the viewer catches them.
// ==========================================================================

#ifndef __omnithread_h_
#define __omnithread_h_

#include <windows.h>

// The original header did "#define NULL (void*)0" when NULL was undefined,
// which breaks integer contexts.  windows.h always defines NULL, so this is
// no longer needed and has been dropped on purpose.

#define _OMNITHREAD_NTDLL_

class omni_mutex;
class omni_semaphore;

//
// Thrown in the event of a fatal error.  Retained for source compatibility;
// nothing in the single-threaded build throws it any more.
//
class _OMNITHREAD_NTDLL_ omni_thread_fatal {
public:
    int error;
    omni_thread_fatal(int e = 0) : error(e) {}
};

//
// Thrown when an operation is invoked with invalid arguments.  ClientConnection
// and ConnectingDialog used to catch this around join(); those catch blocks are
// harmless now but the type must still exist.
//
class _OMNITHREAD_NTDLL_ omni_thread_invalid {};


///////////////////////////////////////////////////////////////////////////
//
// Mutex - no-op in a single-threaded process.
//
///////////////////////////////////////////////////////////////////////////

class _OMNITHREAD_NTDLL_ omni_mutex {
public:
    omni_mutex(void) {}
    ~omni_mutex(void) {}

    void lock(void) {}
    void unlock(void) {}
    void acquire(void) {}
    void release(void) {}

private:
    // dummy copy constructor and operator= to prevent copying
    omni_mutex(const omni_mutex&);
    omni_mutex& operator=(const omni_mutex&);
};

//
// Scoped lock helper.  Empty, but kept so that the existing
//   omni_mutex_lock l(m_someMutex);
// statements scattered through the decoders still compile unchanged.
//
class _OMNITHREAD_NTDLL_ omni_mutex_lock {
public:
    omni_mutex_lock(omni_mutex&) {}
    ~omni_mutex_lock(void) {}
private:
    omni_mutex_lock(const omni_mutex_lock&);
    omni_mutex_lock& operator=(const omni_mutex_lock&);
};


///////////////////////////////////////////////////////////////////////////
//
// Counting semaphore - degenerate single-threaded implementation.
//
// wait() on an exhausted semaphore would deadlock a single-threaded program,
// so it simply returns; the viewer never relies on blocking here.
//
///////////////////////////////////////////////////////////////////////////

class _OMNITHREAD_NTDLL_ omni_semaphore {
public:
    omni_semaphore(unsigned int initial = 1) { value = initial; }
    ~omni_semaphore(void) {}

    void wait(void) { if (value > 0) value--; }
    int  trywait(void) { if (value > 0) { value--; return 1; } return 0; }
    void post(void) { value++; }

private:
    omni_semaphore(const omni_semaphore&);
    omni_semaphore& operator=(const omni_semaphore&);

    unsigned int value;
};

class _OMNITHREAD_NTDLL_ omni_semaphore_lock {
    omni_semaphore& sem;
public:
    omni_semaphore_lock(omni_semaphore& s) : sem(s) { sem.wait(); }
    ~omni_semaphore_lock(void) { sem.post(); }
private:
    omni_semaphore_lock(const omni_semaphore_lock&);
    omni_semaphore_lock& operator=(const omni_semaphore_lock&);
};


///////////////////////////////////////////////////////////////////////////
//
// omni_thread replacement.
//
// Only the two static helpers that the viewer actually used for their side
// effects are kept, as free functions in a namespace-like class.  There is no
// thread object, no TLS, and no static initialiser.
//
///////////////////////////////////////////////////////////////////////////

class _OMNITHREAD_NTDLL_ omni_sleep {
public:
    // Sleep for the given time.  Used by the old ConnectingDialog handshake;
    // kept because it is occasionally handy while porting.  Note that Sleep()
    // on Win32s does not yield to other Windows tasks, so callers should
    // prefer pumping messages instead.
    static void sleep(unsigned long secs, unsigned long nanosecs = 0) {
        Sleep(secs * 1000 + nanosecs / 1000000);
    }
};

#endif // __omnithread_h_
