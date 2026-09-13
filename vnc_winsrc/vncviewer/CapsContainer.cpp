//  Copyright (C) 2003 Constantin Kaplinsky. All Rights Reserved.
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

// CapsContainer.cpp

#include "CapsContainer.h"

//
// The constructor.
//

CapsContainer::CapsContainer(int maxCaps)
{
	// Bound maxCaps: it is a constructor default (64) everywhere today, but
	// "new CARD32[maxSize]" with a zero or negative value would be silently
	// accepted by MSVC 4.1 and then written through by Enable().
	if (maxCaps < 1)
		maxCaps = 1;
	if (maxCaps > 1024)
		maxCaps = 1024;

	maxSize = maxCaps;
	listSize = 0;
	plist = new CARD32[maxSize];
	if (plist == NULL)
		maxSize = 0;			// Enable() checks listSize < maxSize
}

//
// The destructor.
//

CapsContainer::~CapsContainer()
{
	if (plist != NULL) {
		delete[] plist;
		plist = NULL;
	}
	maxSize = 0;
	listSize = 0;

	// Remove char[] strings allocated by the new[] operator.
	// Note: descMap stores NULL for capabilities added without a description,
	// and "delete[] (char *)NULL" is legal, so no check is strictly needed - but
	// be explicit, and null the entry so a double destruction cannot double-free
	// (see the copying note in map.h).
	std::map<CARD32,char*>::const_iterator iter;
	for (iter = descMap.begin(); iter != descMap.end(); iter++) {
		if (iter->second != NULL) {
			delete[] iter->second;
			iter->second = NULL;
		}
	}
}

//
// Add information about a particular capability into the object. There are
// two functions to perform this task. These functions overwrite capability
// records with the same code.
//

void
CapsContainer::Add(CARD32 code, const char *vendor, const char *name,
				   const char *desc)
{
	// Fill in an rfbCapabilityInfo structure and pass it to the overloaded
	// function.
	//
	// vendor and name are fixed-length signature fields (4 and 8 bytes), NOT
	// NUL-terminated strings, and the callers pass string literals of exactly
	// that length.  memcpy of the exact field size is therefore correct - but it
	// reads past the end of the literal if a caller ever passes a shorter one,
	// so guard against NULL at least, and zero-fill first so a short literal
	// cannot leak stack contents into the protocol.
	rfbCapabilityInfo capinfo;
	memset(&capinfo, 0, sizeof(capinfo));
	capinfo.code = code;
	if (vendor != NULL)
		memcpy(capinfo.vendorSignature, vendor, sz_rfbCapabilityInfoVendor);
	if (name != NULL)
		memcpy(capinfo.nameSignature, name, sz_rfbCapabilityInfoName);
	Add(&capinfo, desc);
}

void
CapsContainer::Add(const rfbCapabilityInfo *capinfo, const char *desc)
{
	if (capinfo == NULL)
		return;

	infoMap[capinfo->code] = *capinfo;
	enableMap[capinfo->code] = false;

	// IsKnown() consults descMap, and descMap[code] below inserts - so on the
	// first Add for a code, IsKnown() is false and nothing is deleted; on a
	// repeat Add the old string is freed here.  Correct, but fragile: keep the
	// order (test, free, replace).
	if (IsKnown(capinfo->code)) {
		delete[] descMap[capinfo->code];
		descMap[capinfo->code] = NULL;
	}
	char *desc_copy = NULL;
	if (desc != NULL) {
		desc_copy = new char[strlen(desc) + 1];
		if (desc_copy != NULL)
			strcpy(desc_copy, desc);
	}
	descMap[capinfo->code] = desc_copy;
}

//
// Check if a capability with the specified code was added earlier.
//

bool
CapsContainer::IsKnown(CARD32 code)
{
	return (descMap.find(code) != descMap.end());
}

//
// Fill in a rfbCapabilityInfo structure with contents corresponding to the
// specified code. Returns true on success, false if the specified code is
// not known.
//

bool
CapsContainer::GetInfo(CARD32 code, rfbCapabilityInfo *capinfo)
{
	if (IsKnown(code)) {
		*capinfo = infoMap[code];
		return true;
	}

	return false;
}

//
// Get a description string for the specified capability code. Returns NULL
// either if the code is not known, or if there is no description for this
// capability.
//

char *
CapsContainer::GetDescription(CARD32 code)
{
	return (IsKnown(code)) ? descMap[code] : NULL;
}

//
// Mark the specified capability as "enabled". This function checks "vendor"
// and "name" signatures in the existing record and in the argument structure
// and enables the capability only if both records are the same.
//

bool
CapsContainer::Enable(const rfbCapabilityInfo *capinfo)
{
	// capinfo comes from ReadCapabilityList, i.e. straight off the wire.
	if (capinfo == NULL)
		return false;

	if (!IsKnown(capinfo->code))
		return false;

	const rfbCapabilityInfo *known = &(infoMap[capinfo->code]);
	if ( memcmp(known->vendorSignature, capinfo->vendorSignature,
				sz_rfbCapabilityInfoVendor) != 0 ||
		 memcmp(known->nameSignature, capinfo->nameSignature,
				sz_rfbCapabilityInfoName) != 0 ) {
		enableMap[capinfo->code] = false;
		return false;
	}

	enableMap[capinfo->code] = true;
	if (listSize < maxSize) {
		plist[listSize++] = capinfo->code;
	}
	return true;
}

//
// Check if the specified capability is known and enabled.
//

bool
CapsContainer::IsEnabled(CARD32 code)
{
	return (IsKnown(code)) ? enableMap[code] : false;
}

//
// Return the capability code at the specified index.
// If the index is not valid, return 0.
//

CARD32
CapsContainer::GetByOrder(int idx)
{
	return (idx < listSize) ? plist[idx] : 0;
}

