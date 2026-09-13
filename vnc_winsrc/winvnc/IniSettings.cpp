//  IniSettings.cpp - settings storage for the Win32s / Windows 3.1 port
//
//  This file is part of the TightVNC Win32s port.
//
//  The VNC system is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  See IniSettings.h for why this file exists: Win32s cannot store named, typed
//  registry values (RegSetValueEx returns ERROR_INVALID_PARAMETER for every
//  call), so settings are kept in WINVNC.INI via the Windows 3.1 profile API.

#include "stdhdrs.h"
#include "IniSettings.h"
#include "vncService.h"
#include "WinVNC.h"			// WINVNC_REGISTRY_KEY, used by the registry probe

// WinVNC.h defines WINVNC_REGISTRY_KEY, but only in its non-HORIZONLIVE branch
// (that build includes horizon/horizonMain.h instead, which is not present in
// this tree).  The registry probe below is the only user, and it is a fallback
// path that never runs on Win32s anyway, so provide the key if the header did
// not.
#ifndef WINVNC_REGISTRY_KEY
#define WINVNC_REGISTRY_KEY "Software\\ORL\\WinVNC3"
#endif

extern "C" {
#include "vncauth.h"		// MAXPWLEN
}

// The INI file and section.
//
// The leaf stays WINVNC.INI (8.3-safe: the 16-bit profile API behind
// Get/WritePrivateProfileString rejects long file names with error 87).
// It lives next to winvnc.exe, not in the Windows directory: a bare
// filename resolves against C:\WINDOWS, which fails on read-only/shared
// Windows setups and scatters test installs.  Same rule as the viewer's
// VNCVIEW.INI.  A pre-existing C:\WINDOWS\WINVNC.INI is imported once
// (see MigrateIniFile below) so no password re-entry is needed.
static const char INI_LEAF[]    = "WINVNC.INI";
static const char INI_SECTION[] = "WinVNC";
// Resolved INI path (EXE directory + leaf, or the bare leaf meaning the
// Windows directory if resolution fails).  Every profile call below uses
// this buffer, so no call site needs to change.
static char INI_FILE[_MAX_PATH] = "";

// Resolve INI_FILE once.  Idempotent; safe to call from every accessor.
static void ResolveIniPath()
{
	if (INI_FILE[0] != '\0')
		return;
	char mod[_MAX_PATH];
	if (GetModuleFileName(NULL, mod, sizeof(mod) - 16) != 0) {
		char *slash = strrchr(mod, '\\');
		if (slash == NULL)
			slash = strrchr(mod, '/');
		if (slash != NULL) {
			int dirLen = (int)(slash - mod) + 1;
			if (dirLen > 0 && dirLen + (int)strlen(INI_LEAF) < _MAX_PATH) {
				memcpy(INI_FILE, mod, dirLen);
				INI_FILE[dirLen] = '\0';
				strcat(INI_FILE, INI_LEAF);
				return;
			}
		}
	}
	strcpy(INI_FILE, INI_LEAF);
}

// Create the INI file at startup if missing, so the first
// WritePrivateProfileString is an update, not a create, through the
// 16-bit thunk.  Then import a legacy C:\WINDOWS\WINVNC.INI once, if the
// EXE-side file did not already exist.
static void EnsureIniFileExists()
{
	ResolveIniPath();
	DWORD attrs = GetFileAttributes(INI_FILE);
	if (attrs != 0xFFFFFFFF)
		return;						// already there, nothing to do
	HANDLE h = CreateFile(INI_FILE, GENERIC_WRITE, FILE_SHARE_READ,
						  NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return;						// not writable here; writes will fail loudly
	CloseHandle(h);
	// One-time migration from the old Windows-directory location.
	char winDir[_MAX_PATH];
	char srcPath[_MAX_PATH];
	if (GetWindowsDirectory(winDir, sizeof(winDir) - 16) == 0)
		return;
	int n = strlen(winDir);
	if (n > 0 && winDir[n - 1] != '\\' && winDir[n - 1] != '/') {
		winDir[n] = '\\';
		winDir[n + 1] = '\0';
	}
	if ((int)(strlen(winDir) + strlen(INI_LEAF)) >= _MAX_PATH)
		return;
	strcpy(srcPath, winDir);
	strcat(srcPath, INI_LEAF);
	HANDLE hSrc = CreateFile(srcPath, GENERIC_READ, FILE_SHARE_READ,
							 NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hSrc == INVALID_HANDLE_VALUE)
		return;						// no legacy file, fresh start
	HANDLE hDst = CreateFile(INI_FILE, GENERIC_WRITE, FILE_SHARE_READ,
							 NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDst != INVALID_HANDLE_VALUE) {
		char copyBuf[4096];
		DWORD got = 0, put = 0;
		BOOL ok = TRUE;
		while (ok && ReadFile(hSrc, copyBuf, sizeof(copyBuf), &got, NULL) && got > 0) {
			if (!WriteFile(hDst, copyBuf, got, &put, NULL) || put != got)
				ok = FALSE;
		}
		CloseHandle(hDst);
		vnclog.Print(LL_STATE,
			VNCLOG("imported legacy settings from %s\n"), srcPath);
	}
	CloseHandle(hSrc);
}

// -1 = not yet determined, 0 = use the registry, 1 = use the INI file.
static int g_useIni = -1;

const char *IniFileName()
{
	ResolveIniPath();
	return INI_FILE;
}

BOOL IniSettingsInUse()
{
	if (g_useIni >= 0)
		return g_useIni ? TRUE : FALSE;

	// Decide once.
	//
	// The platform test comes first because it is the definitive answer for this
	// port.  The probe that follows is a safety net for a platform we have
	// misidentified: if the registry genuinely cannot store a named value, we
	// must not keep writing to it and losing everything.
	if (vncService::IsWin32s()) {
		g_useIni = 1;
		EnsureIniFileExists();
		vnclog.Print(LL_STATE,
			VNCLOG("using %s for settings (Win32s has no named registry values)\n"),
			INI_FILE);
		return TRUE;
	}

	// Probe: try to write and delete a throwaway named DWORD.
	HKEY hk;
	DWORD dw;
	g_useIni = 0;
	if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, WINVNC_REGISTRY_KEY,
					   0, REG_NONE, REG_OPTION_NON_VOLATILE,
					   KEY_ALL_ACCESS, NULL, &hk, &dw) == ERROR_SUCCESS) {
		DWORD probe = 1;
		LONG res = RegSetValueEx(hk, "_WriteProbe", 0, REG_DWORD,
								 (LPBYTE)&probe, sizeof(probe));
		if (res == ERROR_SUCCESS) {
			RegDeleteValue(hk, "_WriteProbe");
		} else {
			vnclog.Print(LL_INTERR,
				VNCLOG("registry rejected a named value (error %d) - "
					   "falling back to %s\n"), (int)res, INI_FILE);
			g_useIni = 1;
		}
		RegCloseKey(hk);
	} else {
		// Cannot even open the key.  The INI file is more likely to work.
		vnclog.Print(LL_INTERR,
			VNCLOG("cannot open the registry key - falling back to %s\n"),
			INI_FILE);
		g_useIni = 1;
	}

	if (g_useIni)
		EnsureIniFileExists();

	return g_useIni ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------
// Integers
// ---------------------------------------------------------------------------

LONG IniGetInt(LPCSTR valname, LONG defval)
{
	char buf[64];

	ResolveIniPath();
	if (valname == NULL)
		return defval;

	buf[0] = '\0';
	// A default of "" lets us distinguish "absent" from "present but zero".
	// GetPrivateProfileInt cannot do that, and it also cannot return a negative
	// number - see the note in IniSettings.h about LockSetting == -1.
	GetPrivateProfileString(INI_SECTION, valname, "", buf, sizeof(buf) - 1,
							INI_FILE);
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '\0')
		return defval;

	// Validate before converting: atol() returns 0 for junk, which would
	// silently turn a corrupt entry into a meaningful "off".
	const char *p = buf;
	if (*p == '-' || *p == '+')
		p++;
	if (*p == '\0')
		return defval;
	while (*p != '\0') {
		if (*p < '0' || *p > '9')
			return defval;
		p++;
	}

	return (LONG)atol(buf);
}

BOOL IniSetInt(LPCSTR valname, LONG val)
{
	char buf[64];

	ResolveIniPath();
	if (valname == NULL)
		return FALSE;

	// wsprintf rather than sprintf: it is in USER32, always present, and avoids
	// pulling the CRT's full printf into a path called ~35 times per save.
	wsprintf(buf, "%ld", val);

	if (!WritePrivateProfileString(INI_SECTION, valname, buf, INI_FILE)) {
		vnclog.Print(LL_INTERR,
			VNCLOG("failed to write \"%s\" to %s\n"), valname, INI_FILE);
		return FALSE;
	}
	return TRUE;
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

char *IniGetString(LPCSTR valname)
{
	// AuthHosts can be long; 1 KB is generous for a host filter list and matches
	// what the registry version accepted in practice.
	char buf[1024];

	ResolveIniPath();
	if (valname == NULL)
		return NULL;

	buf[0] = '\0';
	GetPrivateProfileString(INI_SECTION, valname, "", buf, sizeof(buf) - 1,
							INI_FILE);
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '\0')
		return NULL;

	char *result = new char[strlen(buf) + 1];
	if (result == NULL)
		return NULL;
	strcpy(result, buf);
	return result;
}

BOOL IniSetString(LPCSTR valname, const char *value)
{
	ResolveIniPath();
	if (valname == NULL)
		return FALSE;

	// A NULL value deletes the entry, which is what the caller means by
	// "no AuthHosts set".
	if (!WritePrivateProfileString(INI_SECTION, valname, value, INI_FILE)) {
		vnclog.Print(LL_INTERR,
			VNCLOG("failed to write \"%s\" to %s\n"), valname, INI_FILE);
		return FALSE;
	}
	return TRUE;
}

// ---------------------------------------------------------------------------
// Passwords
//
// MAXPWLEN (8) binary bytes <-> 16 hex characters.  Same representation the
// viewer uses for passwords in .vnc files, so it is already familiar in-tree.
// ---------------------------------------------------------------------------

BOOL IniGetPassword(char *buffer, LPCSTR valname)
{
	char buf[MAXPWLEN * 2 + 2];

	ResolveIniPath();
	if (buffer == NULL || valname == NULL)
		return FALSE;

	buf[0] = '\0';
	GetPrivateProfileString(INI_SECTION, valname, "", buf, sizeof(buf) - 1,
							INI_FILE);
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '\0') {
		vnclog.Print(LL_INTINFO,
			VNCLOG("no stored password \"%s\" in %s\n"), valname, INI_FILE);
		return FALSE;
	}

	// Must be exactly the right length, or it is not a password we wrote.
	if (strlen(buf) != MAXPWLEN * 2) {
		vnclog.Print(LL_INTERR,
			VNCLOG("password \"%s\" is %d chars, expected %d - ignoring\n"),
			valname, (int)strlen(buf), (int)(MAXPWLEN * 2));
		return FALSE;
	}

	int i;
	for (i = 0; i < MAXPWLEN; i++) {
		int hi = buf[i * 2];
		int lo = buf[i * 2 + 1];

		// Decode by hand rather than with sscanf("%2x"): sscanf would accept
		// leading whitespace and a short field, and we want a strict check.
		if      (hi >= '0' && hi <= '9') hi -= '0';
		else if (hi >= 'a' && hi <= 'f') hi = hi - 'a' + 10;
		else if (hi >= 'A' && hi <= 'F') hi = hi - 'A' + 10;
		else {
			vnclog.Print(LL_INTERR,
				VNCLOG("password \"%s\" is not valid hex - ignoring\n"), valname);
			return FALSE;
		}

		if      (lo >= '0' && lo <= '9') lo -= '0';
		else if (lo >= 'a' && lo <= 'f') lo = lo - 'a' + 10;
		else if (lo >= 'A' && lo <= 'F') lo = lo - 'A' + 10;
		else {
			vnclog.Print(LL_INTERR,
				VNCLOG("password \"%s\" is not valid hex - ignoring\n"), valname);
			return FALSE;
		}

		buffer[i] = (char)((hi << 4) | lo);
	}

	vnclog.Print(LL_INTINFO,
		VNCLOG("loaded password \"%s\" from %s\n"), valname, INI_FILE);
	return TRUE;
}

BOOL IniSetPassword(const char *buffer, LPCSTR valname)
{
	char buf[MAXPWLEN * 2 + 2];
	static const char hexdigits[] = "0123456789abcdef";

	ResolveIniPath();
	if (buffer == NULL || valname == NULL)
		return FALSE;

	int i;
	for (i = 0; i < MAXPWLEN; i++) {
		unsigned char b = (unsigned char)buffer[i];
		buf[i * 2]     = hexdigits[(b >> 4) & 0x0F];
		buf[i * 2 + 1] = hexdigits[b & 0x0F];
	}
	buf[MAXPWLEN * 2] = '\0';

	if (!WritePrivateProfileString(INI_SECTION, valname, buf, INI_FILE)) {
		vnclog.Print(LL_INTERR,
			VNCLOG("failed to write password \"%s\" to %s\n"), valname, INI_FILE);
		return FALSE;
	}

	vnclog.Print(LL_INTINFO,
		VNCLOG("saved password \"%s\" to %s\n"), valname, INI_FILE);
	return TRUE;
}
