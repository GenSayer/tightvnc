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
// WIN32S / WINDOWS 3.1 SINGLE-THREADED BUILD  (winvnc)
// ==========================================================================
//
// Win32s has no threads.  This is the same replacement header already used by
// vncviewer, extended with the two pieces the SERVER needs and the viewer did
// not: omni_condition, and a compatibility shim for the omni_thread classes
// that the server subclasses.
//
// WHAT THE SERVER USED THREADS FOR, AND WHAT REPLACES IT:
//
//   vncClientThread    (vncClient.cpp)      one per connected client, ran the
//                                           whole RFB message loop.
//                                           -> vncClient::PumpIdle(), driven
//                                              from the app's idle loop.
//
//   vncDesktopThread   (vncDesktop.cpp)     owned the desktop sink window and
//                                           polled for screen changes.
//                                           -> vncDesktop::PumpIdle(), same
//                                              idle loop.  Note that the sink
//                                              window's messages are now
//                                              dispatched by the main loop,
//                                              because there is only one queue.
//
//   vncSockConnectThread                    blocking accept() on the listening
//                                           socket.
//                                           -> WSAAsyncSelect + FD_ACCEPT,
//                                              posted to a window.
//
//   vncHTTPConnectThread / ListenThread     same, for the built-in HTTP server.
//                                           -> same treatment.
//
//   vncTimedMsgBoxThread                    a MessageBox with a timeout.
//                                           -> SetTimer + a modeless dialog.
//
// omni_mutex / omni_mutex_lock are no-ops: with one thread there is nothing to
// serialise, and the ~90 "omni_mutex_lock l(...)" statements throughout the
// server therefore compile unchanged and cost nothing.
//
// omni_condition IS still declared, because vncServer holds one
// (m_clientquitsig) and vncDesktopThread used one to hand its init result back
// to the caller.  Its wait() CANNOT block - that would deadlock the single
// thread instantly - so it pumps messages instead.  See the comment on wait().
//
// DELIBERATELY REMOVED: class omni_thread and the file-scope
// "static omni_thread::init_t omni_thread_init;" object, which ran TlsAlloc /
// TlsSetValue / DuplicateHandle / operator new before WinMain in EVERY
// translation unit.  On Win32s that is a crash before any window exists - the
// exact failure the viewer had.
// ==========================================================================

#ifndef __omnithread_h_
#define __omnithread_h_

#include <windows.h>

#define _OMNITHREAD_NTDLL_

class omni_mutex;
class omni_semaphore;
class omni_condition;

//
// Thrown in the event of a fatal error.  Retained for source compatibility.
//
class _OMNITHREAD_NTDLL_ omni_thread_fatal {
public:
    int error;
    omni_thread_fatal(int e = 0) : error(e) {}
};

//
// Thrown when an operation is invoked with invalid arguments.
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

    friend class omni_condition;

private:
    omni_mutex(const omni_mutex&);
    omni_mutex& operator=(const omni_mutex&);
};

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
// Condition variable.
//
// SINGLE-THREADED SEMANTICS - read this before using it.
//
// A condition variable is meaningless with one thread: whatever you are waiting
// for can only be produced by the thread that is waiting.  A blocking wait()
// here is a guaranteed hang.
//
// wait() therefore does NOT block.  It pumps the message queue once and
// returns.  Every caller in the server is of the form
//
//     while (!condition_is_satisfied)
//         cond->wait();
//
// so pumping messages inside wait() lets the code that satisfies the condition
// (a window procedure, a socket notification, an idle-time pump) actually run.
// The loop then re-tests and exits.
//
// A caller that spins forever because nothing will ever satisfy its condition
// is a bug in the CALLER, not here - and it will spin visibly rather than
// deadlocking silently.  vncServer::WaitUntilAuthEmpty is the case to watch:
// it is called during shutdown and relies on clients being reaped.
//
///////////////////////////////////////////////////////////////////////////

class _OMNITHREAD_NTDLL_ omni_condition {
public:
    omni_condition(omni_mutex *m) { mutex = m; }
    ~omni_condition(void) {}

    // Pump one round of messages, then return.  See the note above.
    void wait(void) {
        MSG msg;
        int guard = 0;
        while (guard++ < 64 && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                // Re-post so the real message loop sees it and exits.
                PostQuitMessage((int)msg.wParam);
                return;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Yield to Windows if there was nothing to do, so a spinning caller
        // does not starve the rest of the (cooperatively scheduled) system.
        if (guard == 1)
            Sleep(10);
    }

    // Timed wait.  Returns 1 (as omniORB's does) meaning "not timed out";
    // behaviour is otherwise identical to wait().
    int timedwait(unsigned long /*secs*/, unsigned long /*nanosecs*/ = 0) {
        wait();
        return 1;
    }

    // No-ops: there is no other thread to wake.
    void signal(void) {}
    void broadcast(void) {}

private:
    omni_mutex *mutex;

    omni_condition(const omni_condition&);
    omni_condition& operator=(const omni_condition&);
};


///////////////////////////////////////////////////////////////////////////
//
// Counting semaphore - degenerate single-threaded implementation.
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
// There is no thread class any more.  What remains is the small set of static
// helpers that the server called for their side effects, as free functions in a
// class scope so that "omni_thread::sleep(1)" still compiles.
//
// NOTE: sleep() on Win32s does not yield to other Windows tasks the way it does
// on NT - scheduling is cooperative.  Prefer pumping messages.  The three
// vncService.cpp call sites that used omni_thread::sleep are in code paths that
// are stubbed out on Win32s anyway.
//
///////////////////////////////////////////////////////////////////////////

class _OMNITHREAD_NTDLL_ omni_thread {
public:
    static void sleep(unsigned long secs, unsigned long nanosecs = 0) {
        // Pump messages while waiting rather than blocking outright: on a
        // cooperatively scheduled system a bare Sleep() freezes everything.
        DWORD until = GetTickCount() + (DWORD)(secs * 1000 + nanosecs / 1000000);
        MSG msg;
        while ((long)(GetTickCount() - until) < 0) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    PostQuitMessage((int)msg.wParam);
                    return;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                Sleep(10);
            }
        }
    }

    // Formerly returned the current omni_thread*.  Only used in a debug
    // _RPT3 trace in vncDesktop.cpp; return the thread id so that trace still
    // prints something meaningful.
    static DWORD self(void) { return GetCurrentThreadId(); }

    // omni_thread::create(fn) started a detached thread.  There is no way to
    // honour that here.  Callers must be restructured; the two in
    // vncService.cpp are in Win32s-stubbed paths.  Returning NULL makes any
    // remaining caller fail visibly rather than silently doing nothing.
    static omni_thread *create(void (*fn)(void *), void *arg = 0) {
        return 0;
    }
};

#endif // __omnithread_h_
