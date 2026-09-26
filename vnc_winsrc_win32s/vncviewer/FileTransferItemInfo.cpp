//  Copyright (C) 2003 Dennis Syrovatsky. All Rights Reserved.
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
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.

#define WINVER 0x030a

#include "FileTransferItemInfo.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "windows.h"

const char FileTransferItemInfo::folderText[] = "<Folder>";

int 
CompareFTItemInfo(const void *F, const void *S)
{
	if (strcmp(((FTITEMINFO *)F)->Size, ((FTITEMINFO *)S)->Size) == 0) {
		return stricmp(((FTITEMINFO *)F)->Name, ((FTITEMINFO *)S)->Name);
	} else {
		if (strcmp(((FTITEMINFO *)F)->Size, FileTransferItemInfo::folderText) == 0) return -1;
		if (strcmp(((FTITEMINFO *)S)->Size, FileTransferItemInfo::folderText) == 0) {
			return 1;
		} else {
		return stricmp(((FTITEMINFO *)F)->Name, ((FTITEMINFO *)S)->Name);
		}
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FileTransferItemInfo::FileTransferItemInfo()
{
	m_NumEntries = 0;
	m_Capacity = 0;
	m_pEntries = NULL;
}

FileTransferItemInfo::~FileTransferItemInfo()
{
	Free();
}

void FileTransferItemInfo::Add(char *Name, char *Size, unsigned int Data)
{
	// WIN32S NOTES
	//
	// 1. Bounds.  Name is char[rfbMAX_PATH] (255) and Size is char[16], and both
	//    were filled with strcpy() from caller-supplied strings.  Size in
	//    particular is filled from a server-supplied string in the file-list
	//    reply, so an over-long field corrupted the next array element.
	//
	// 2. Reallocation cost.  This grows the array by ONE entry per call, copying
	//    everything each time - O(n^2) allocations and copies.  Each FTITEMINFO
	//    is 275 bytes, so listing a 400-file directory performs 400 allocations
	//    totalling ~22 MB of copying.  On Win32s, where the heap is the shared
	//    16-bit heap, that is slow enough to look like a hang and is a plausible
	//    contributor to the file dialog appearing to do nothing.  Grow
	//    geometrically instead.
	if (Name == NULL || Size == NULL)
		return;

	if (m_NumEntries >= m_Capacity) {
		int newCap = (m_Capacity < 32) ? 32 : (m_Capacity * 2);
		FTITEMINFO *pTemporary = new FTITEMINFO[newCap];
		if (pTemporary == NULL)
			return;
		if (m_NumEntries != 0 && m_pEntries != NULL)
			memcpy(pTemporary, m_pEntries, m_NumEntries * sizeof(FTITEMINFO));
		if (m_pEntries != NULL)
			delete [] m_pEntries;
		m_pEntries = pTemporary;
		m_Capacity = newCap;
	}

	strncpy(m_pEntries[m_NumEntries].Name, Name, rfbMAX_PATH - 1);
	m_pEntries[m_NumEntries].Name[rfbMAX_PATH - 1] = '\0';
	strncpy(m_pEntries[m_NumEntries].Size, Size, 15);
	m_pEntries[m_NumEntries].Size[15] = '\0';
	m_pEntries[m_NumEntries].Data = Data;
	m_NumEntries++;
}

void FileTransferItemInfo::Free()
{
	if (m_pEntries != NULL) {
		delete [] m_pEntries;
		m_pEntries = NULL;
	}
	m_NumEntries = 0;
	m_Capacity = 0;
}

void FileTransferItemInfo::Sort()
{
	// qsort with a NULL base or a zero count is undefined; the old code called
	// it unconditionally, and Sort() runs on an empty list whenever a directory
	// listing produced no entries.
	if (m_pEntries == NULL || m_NumEntries < 2)
		return;
	qsort(m_pEntries, m_NumEntries, sizeof(FTITEMINFO), CompareFTItemInfo);
}

// NOTE on all three accessors: the bound was "Number <= m_NumEntries", which
// allows one element PAST the end of the array.  These are called from the
// LVN_GETDISPINFO handler with an index supplied by the list view, so a stale
// notification for a row that has since been removed read past the array - and
// GetNameAt's result is handed straight to the control as a text pointer.
static char s_ftEmptyString[] = "";

char * FileTransferItemInfo::GetNameAt(int Number)
{
	if ((m_pEntries != NULL) && (Number >= 0) && (Number < m_NumEntries))
		return m_pEntries[Number].Name;
	// Return an empty string rather than NULL: the list view dereferences the
	// pszText it is given.
	return s_ftEmptyString;
}

char * FileTransferItemInfo::GetSizeAt(int Number)
{
	if ((m_pEntries != NULL) && (Number >= 0) && (Number < m_NumEntries))
		return m_pEntries[Number].Size; 
	return s_ftEmptyString;
}

unsigned int FileTransferItemInfo::GetDataAt(int Number)
{
	if ((m_pEntries != NULL) && (Number >= 0) && (Number < m_NumEntries))
		return m_pEntries[Number].Data;
	return 0;
}

int FileTransferItemInfo::GetNumEntries()
{
	return m_NumEntries;
}

int FileTransferItemInfo::GetIntSizeAt(int Number)
{
	return ConvertCharToInt(GetSizeAt(Number));
}

bool FileTransferItemInfo::IsFile(int Number)
{
	// Was "if ((Number < 0) && (Number > m_NumEntries)) return FALSE;" - an &&
	// where an || was meant, so the test could never be true and the array was
	// indexed with whatever came in.
	if ((m_pEntries == NULL) || (Number < 0) || (Number >= m_NumEntries))
		return FALSE;
    if (strcmp(m_pEntries[Number].Size, folderText) != 0) return TRUE;
	return FALSE;
}

int FileTransferItemInfo::ConvertCharToInt(char *pStr)
{
	if (pStr == NULL)
		return 0;
	int strLen = strlen(pStr);
	int res = 0, tenX = 1;
	for (int i = (strLen - 1); i >= 0; i--) {
		switch (pStr[i])
		{
		case '1': res = res + 1 * tenX; break;
		case '2': res = res + 2 * tenX;	break;
		case '3': res = res + 3 * tenX;	break;
		case '4': res = res + 4 * tenX;	break;
		case '5': res = res + 5 * tenX;	break;
		case '6': res = res + 6 * tenX; break;
		case '7': res = res + 7 * tenX;	break;
		case '8': res = res + 8 * tenX; break;
		case '9': res = res + 9 * tenX;	break;
		}
		tenX = tenX * 10;
	}
	return res;
}
