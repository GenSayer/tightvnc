//  Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//
//  This file is part of the VNC system.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
//  USA.
//
// TightVNC distribution homepage on the Web: http://www.tightvnc.com/
//
// If the source code for the VNC system is not available from the place 
// whence you received this file, check http://www.uk.research.att.com/vnc or 
// contact the authors on vnc@uk.research.att.com for information on obtaining it.
//
// Log.cpp: implementation of the Log class.
//
//////////////////////////////////////////////////////////////////////

#include "stdhdrs.h"
#include "Log.h"

// bool/true/false come from win32s_fix.h (force-included).  Do not redefine.

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const int Log::ToDebug   =  1;
const int Log::ToFile    =  2;
const int Log::ToConsole =  4;

const static int LINE_BUFFER_SIZE = 1024;

Log::Log(int mode, int level, LPTSTR filename, bool append)
{
    hlogfile = NULL;
    m_todebug = false;
    m_toconsole = false;
    m_tofile = false;
    // m_level was NEVER initialised here.  Print() tests "level > m_level", so
    // with garbage in m_level the viewer either logged nothing or logged
    // everything (including OutputDebugString on every socket read) depending
    // on what happened to be on the stack.  This object is a file-scope global
    // in vncviewer.cpp, so its constructor runs before WinMain - all the more
    // reason for it to be fully deterministic.
    m_level = level;
    SetMode(mode);
    if (mode & ToFile)  {
        SetFile(filename, append);
    }
}

void Log::SetMode(int mode) {
    
    if (mode & ToDebug)
        m_todebug = true;
    else
        m_todebug = false;

    if (mode & ToFile)  {
        m_tofile = true;
    } else {
        CloseFile();
        m_tofile = false;
    }
    
#ifdef _WIN32_WCE
	m_toconsole = false;
#else
    if (mode & ToConsole) {
        // WIN32S: AllocConsole() does not exist on Win32s - there is no console
        // subsystem under Windows 3.1.  Resolve it dynamically so that (a) the
        // name is not in the import table, and (b) /logtoconsole degrades to
        // debug output instead of failing to load the program.
        if (!m_toconsole) {
            typedef BOOL (WINAPI *PFNALLOCCONSOLE)(void);
            HINSTANCE hK32 = GetModuleHandle("KERNEL32");
            PFNALLOCCONSOLE pAlloc = (hK32 == NULL) ? NULL :
                (PFNALLOCCONSOLE)GetProcAddress(hK32, "AllocConsole");
            if (pAlloc != NULL && pAlloc()) {
                m_toconsole = true;
            } else {
                m_toconsole = false;
                m_todebug = true;
            }
        }
    } else {
        m_toconsole = false;
    }
#endif
}


void Log::SetLevel(int level) {
    m_level = level;
}

void Log::SetFile(LPTSTR filename, bool append) 
{
    // if a log file is open, close it now.
    CloseFile();

    m_tofile  = true;

    if (filename == NULL) {
        // The original comment said "If filename is NULL or invalid we should
        // throw an exception here" and then passed NULL straight to CreateFile.
        m_todebug = true;
        m_tofile = false;
        return;
    }
    
    hlogfile = CreateFile(
        filename,  GENERIC_WRITE, FILE_SHARE_READ, NULL,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
    
    if (hlogfile == INVALID_HANDLE_VALUE) {
        // Was: set m_todebug/m_tofile and carry on - but hlogfile was left as
        // INVALID_HANDLE_VALUE (not NULL), so CloseFile() later called
        // CloseHandle(INVALID_HANDLE_VALUE), and the SetFilePointer/
        // SetEndOfFile calls below ran on the invalid handle too.
        hlogfile = NULL;
        m_todebug = true;
        m_tofile = false;
        Print(0, _T("Error opening log file %s\n"), filename);
        return;
    }
    if (append) {
        SetFilePointer( hlogfile, 0, NULL, FILE_END );
    } else {
        SetEndOfFile( hlogfile );
    }
}

// if a log file is open, close it now.
void Log::CloseFile() {
    if (hlogfile != NULL) {
        CloseHandle(hlogfile);
        hlogfile = NULL;
    }
}


#ifndef UNDER_CE

// Non-CE version 

void Log::ReallyPrint(LPTSTR format, va_list ap) 
{
	TCHAR line[LINE_BUFFER_SIZE];
	// Was: _vsntprintf(line, sizeof(line) - 2*sizeof(TCHAR), ...).  The count
	// argument is in CHARACTERS, not bytes, and the same confusion appears in
	// the length checks below.  In an ANSI build sizeof == count so it happened
	// to work; it is wrong in principle and would overflow the buffer in a
	// _UNICODE build.
	const int maxChars = LINE_BUFFER_SIZE - 3;
	line[0] = (TCHAR)'\0';
	_vsntprintf(line, maxChars, format, ap);
	line[maxChars] = (TCHAR)'\0';
	int len = _tcslen(line);
	if (len > 0 && len <= maxChars && line[len-1] == (TCHAR)'\n') {
		// Replace trailing '\n' with MS-DOS style end-of-line.
		line[len-1] = (TCHAR)'\r';
		line[len] =   (TCHAR)'\n';
		line[len+1] = (TCHAR)'\0';
		len++;
	}

    if (m_todebug) OutputDebugString(line);

    if (m_toconsole) {
        // WIN32S: WriteConsole and GetStdHandle are console-subsystem APIs that
        // Win32s does not provide.  m_toconsole can only be true if AllocConsole
        // succeeded (see SetMode, which resolves that dynamically), which cannot
        // happen on Win32s - but the NAMES would still appear in the EXE's import
        // table and stop it loading.  Resolve them at run time too.
        //
        // Pointers cached in statics: this is called for every log line.
        typedef BOOL (WINAPI *PFNWRITECONSOLE)(HANDLE, const void *, DWORD, DWORD *, void *);
        typedef HANDLE (WINAPI *PFNGETSTDHANDLE)(DWORD);
        static PFNWRITECONSOLE pWriteConsole = NULL;
        static PFNGETSTDHANDLE pGetStdHandle = NULL;
        static int consoleResolved = 0;

        if (!consoleResolved) {
            consoleResolved = 1;
            HINSTANCE hK32 = GetModuleHandle("KERNEL32");
            if (hK32 != NULL) {
                pWriteConsole = (PFNWRITECONSOLE)GetProcAddress(hK32, "WriteConsoleA");
                pGetStdHandle = (PFNGETSTDHANDLE)GetProcAddress(hK32, "GetStdHandle");
            }
        }

        if (pWriteConsole != NULL && pGetStdHandle != NULL) {
            DWORD byteswritten;
            pWriteConsole(pGetStdHandle(STD_OUTPUT_HANDLE), line,
                          _tcslen(line), &byteswritten, NULL);
        }
    }

    if (m_tofile && (hlogfile != NULL)) {
        DWORD byteswritten;
        WriteFile(hlogfile, line, _tcslen(line)*sizeof(TCHAR), &byteswritten, NULL); 
    }
}

#else

// CE version 

void Log::ReallyPrint(LPTSTR format, va_list ap) 
{
    TCHAR line[LINE_BUFFER_SIZE];
    _vsntprintf(line, sizeof(line) - sizeof(TCHAR), format, ap);
    if (m_todebug) OutputDebugString(line);

    if (m_tofile && (hlogfile != NULL)) {
        DWORD byteswritten;
		
		// Log file is more readable if non-unicode!
		char ansiline[LINE_BUFFER_SIZE];
		int origlen = _tcslen(line);
		int newlen = WideCharToMultiByte(
			CP_ACP,    // code page
			0,         // performance and mapping flags
			line,      // address of wide-character string
			origlen,   // number of characters in string
			ansiline,  // address of buffer for new string
			255,       // size of buffer
			NULL, NULL );
		WriteFile(hlogfile, ansiline, newlen, &byteswritten, NULL); 
    }	
}

#endif

Log::~Log()
{
    CloseFile();
}

// NOTE: "Log theLog;" used to be here.  It was a second, unused, file-scope Log
// object (the one everything actually uses is "Log vnclog;" in vncviewer.cpp),
// so it ran an extra constructor before WinMain for no purpose.  Removed.
