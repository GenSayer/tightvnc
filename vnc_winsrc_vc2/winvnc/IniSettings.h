//  IniSettings.h - settings storage for the Win32s / Windows 3.1 port
//
//  This file is part of the TightVNC Win32s port.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
//
// WinVNC stores every setting as a NAMED, TYPED registry value:
//
//     RegSetValueEx(key, "SocketConnect", 0, REG_DWORD,  ...)
//     RegSetValueEx(key, "Password",      0, REG_BINARY, ...)
//
// WIN32S CANNOT DO THIS.  Its registry is an emulation over the Windows 3.1
// REG.DAT, whose data model is a hierarchy of keys where each key holds ONE
// UNNAMED STRING.  Consequently:
//
//     RegCreateKeyEx / RegOpenKeyEx / RegCloseKey    work
//     RegSetValue    / RegQueryValue  (unnamed, string only)   work
//     RegSetValueEx  / RegQueryValueEx (named, typed)          FAIL
//
// and they fail with ERROR_INVALID_PARAMETER (87) on every call, regardless of
// the key, the access mask or the value type.  Observed on a real Windows 3.11 +
// Win32s 1.30 system: every one of the ~35 settings and both passwords returned
// 87.  That is what made settings appear not to save, and - because the password
// is stored the same way - it is also why the viewer reported "this server does
// not have a valid password enabled": nothing was ever persisted, so on the next
// Load() m_pref_passwd_set stayed FALSE and ValidPasswordsSet() correctly said
// no.
//
// THE REPLACEMENT
//
// Windows 3.1's own configuration mechanism: private profile (.INI) files.
// GetPrivateProfileString / WritePrivateProfileString are fully supported on
// Win32s - they are what a native 3.1 application would use - and they are also
// present on every later Win32, so the same code path works everywhere if you
// ever want it to.
//
// Settings live in WINVNC.INI next to winvnc.exe, under one section:
//
//     [WinVNC]
//     SocketConnect=1
//     PortNumber=5900
//     Password=1a2b3c4d5e6f7081
//
// DESIGN NOTES
//
//  * ONE SECTION, not three.  The registry version has three scopes
//    (HKLM\...\Default, HKLM\...\<user>, HKCU\...).  Windows 3.1 is single-user,
//    so that distinction has no meaning here.  It is also safe: the machine-level
//    value names (ConnectPriority, AuthRequired, DebugMode, DebugLevel,
//    AllowLoopback, ...) and the per-user names (SocketConnect, PortNumber,
//    Password, ...) are disjoint sets, so collapsing them cannot collide.
//
//  * atol(), NOT GetPrivateProfileInt().  LockSetting is legitimately -1, and
//    GetPrivateProfileInt cannot return a negative value - it treats the leading
//    '-' as a terminator and yields 0.  That would silently turn "no lock
//    setting" into "lock on disconnect".
//
//  * The password is 8 BINARY bytes (MAXPWLEN, see vncauth.h) and INI values are
//    strings, so it is stored as 16 hex characters.  The viewer already uses
//    exactly this representation for passwords in .vnc files
//    (ClientConnectionFile.cpp), so there is precedent in-tree and the encoding
//    is familiar.
//
//  * The file lives next to the EXE (resolved via GetModuleFileName), not in
//    the Windows directory: a bare filename would resolve against C:\WINDOWS,
//    which fails on read-only/shared Windows setups.  Same rule as the
//    viewer's VNCVIEW.INI.  A legacy C:\WINDOWS\WINVNC.INI is imported once
//    on first run so existing passwords survive the move.
// ---------------------------------------------------------------------------

#ifndef INISETTINGS_H__
#define INISETTINGS_H__

#include "stdhdrs.h"

// TRUE when settings should go to the INI file rather than the registry.
//
// This is decided ONCE, on first call, and is TRUE when either:
//   * we are running on Win32s (vncService::IsWin32s()), or
//   * a probe write to the registry failed - which covers any other platform
//     whose registry rejects named values, and means a misdetected platform
//     still ends up with working settings rather than silently losing them.
BOOL IniSettingsInUse();

// Integer settings.  IniGetInt returns defval if the name is absent or the
// stored text is not a number.
LONG IniGetInt(LPCSTR valname, LONG defval);
BOOL IniSetInt(LPCSTR valname, LONG val);

// String settings (used for AuthHosts).  Returns a new[]-allocated string that
// the caller must delete[], or NULL if the name is absent.
char *IniGetString(LPCSTR valname);
BOOL IniSetString(LPCSTR valname, const char *value);

// Password settings.  'buffer' is MAXPWLEN binary bytes, stored as hex.
BOOL IniGetPassword(char *buffer, LPCSTR valname);
BOOL IniSetPassword(const char *buffer, LPCSTR valname);

// Full path of the INI file in use, for logging.
const char *IniFileName();

#endif // INISETTINGS_H__
