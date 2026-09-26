//  Copyright (C) 2001-2006 Constantin Kaplinsky. All Rights Reserved.
//  Copyright (C) 2002 Vladimir Vologzhanin. All Rights Reserved.
//  Copyright (C) 2000 Tridia Corporation. All Rights Reserved.
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
// whence you received this file, check http://www.uk.research.att.com/vnc or contact
// the authors on vnc@uk.research.att.com for information on obtaining it.


// vncClient.cpp

// The per-client object.  This object takes care of all per-client stuff,
// such as socket input and buffering of updates.

// vncClient class handles the following functions:
// - Recieves requests from the connected client and
//   handles them
// - Handles incoming updates properly, using a vncBuffer
//   object to keep track of screen changes
// It uses a vncBuffer and is passed the vncDesktop and
// vncServer to communicate with.

// Includes
#include "stdhdrs.h"
#include <omnithread.h>
#include "resource.h"

// Custom
#include "vncClient.h"
#include "VSocket.h"
#include "vncDesktop.h"
#include "vncRegion.h"
#include "vncBuffer.h"
#include "vncService.h"
#include "vncPasswd.h"
#include "vncAcceptDialog.h"
#include "vncKeymap.h"
#include "Windows.h"
extern "C" {
#include "d3des.h"
}

#include "FileTransferItemInfo.h"
#include "vncMenu.h"
#include "Win32sApi.h"	// WIN32S: Win32sSetForegroundWindow

// ==========================================================================
// FILETIME <-> Unix time helpers.
//
// MUST APPEAR BEFORE FIRST USE.  These were originally placed next to
// FiletimeToTime70() near the bottom of the file, but Time70ToFiletime()
// (roughly 150 lines earlier) also uses FT70_EPOCH_LOW / FT70_EPOCH_HIGH, so the
// compiler reported them as undeclared identifiers.  C and C++ require a macro
// to be defined textually before the line that expands it - unlike a member
// function, there is no deferred lookup.
//
// The two static helpers are moved with the macros for the same reason: a static
// function must be declared before it is called, and keeping the whole group
// together makes the ordering obvious to the next reader.
// ==========================================================================

// Epoch difference between 1601-01-01 and 1970-01-01, in 100ns units:
// 116444736000000000 == 0x019DB1DED53E8000
#define FT70_EPOCH_HIGH  0x019DB1DEUL
#define FT70_EPOCH_LOW   0xD53E8000UL

// Divide the 64-bit value (high:low) by a 32-bit divisor, returning the low 32
// bits of the quotient.  Restoring shift-and-subtract division; 64 iterations.
static DWORD Div64By32(DWORD high, DWORD low, DWORD divisor)
{
	if (divisor == 0)
		return 0;

	DWORD rem = 0;
	DWORD quotLow = 0;
	int i;

	// Process the high word, then the low word, most significant bit first.
	for (i = 31; i >= 0; i--) {
		// rem = rem*2 + bit
		DWORD carry = (rem & 0x80000000UL) ? 1 : 0;
		rem = (rem << 1) | ((high >> i) & 1);
		// A carry out of rem means rem >= 2^32 > divisor, so subtract.
		if (carry || rem >= divisor) {
			rem -= divisor;
			// quotient bit belongs to the high half, which we discard
		}
	}
	for (i = 31; i >= 0; i--) {
		DWORD carry = (rem & 0x80000000UL) ? 1 : 0;
		rem = (rem << 1) | ((low >> i) & 1);
		quotLow <<= 1;
		if (carry || rem >= divisor) {
			rem -= divisor;
			quotLow |= 1;
		}
	}

	return quotLow;
}

// ==========================================================================
// WIN32S scrollbar helpers (pixel <-> scroll-position mapping).
//
// A window scrollbar is a strip of totalLen pixels with an arrow button of
// "arrow" pixels at each end; the thumb runs in the track between them.
// Windows 3.x scroll positions are 16-bit, so long is used for the multiply
// to keep MSVC 4.1 from emitting 64-bit CRT helpers.  All inputs are small.
// ==========================================================================

// Usable track length inside a scrollbar strip.
static int ScrollTrackLen(int totalLen, int arrow)
{
	int track = totalLen - 2 * arrow;
	return (track < 1) ? 1 : track;
}

// Pixel position of the thumb marker for the current scroll value.
static int ScrollThumbPix(int cur, int nMin, int nMax, int arrow, int trackLen)
{
	int span = nMax - nMin;
	long off;

	if (span <= 0)
		return arrow;
	off = (long)(cur - nMin) * (long)trackLen;
	return arrow + (int)(off / span);
}

// Scroll value for a cursor offset (pixels from the strip start).
static int ScrollPosFromPix(int pix, int arrow, int trackLen, int nMin, int nMax)
{
	int rel = pix - arrow;
	long pos;

	if (rel < 0)
		rel = 0;
	if (rel > trackLen)
		rel = trackLen;
	if (trackLen <= 0)
		return nMin;
	pos = (long)rel * (long)(nMax - nMin);
	return nMin + (int)(pos / trackLen);
}

// Subtract the 1601->1970 epoch offset from a FILETIME, in place.
// Returns FALSE if the FILETIME predates 1970 (in which case the result is 0).
static BOOL SubtractFt70Epoch(DWORD *pHigh, DWORD *pLow)
{
	DWORD high = *pHigh;
	DWORD low  = *pLow;

	if (high < FT70_EPOCH_HIGH ||
		(high == FT70_EPOCH_HIGH && low < FT70_EPOCH_LOW)) {
		// Before 1970 - the protocol has no way to express this.
		*pHigh = 0;
		*pLow = 0;
		return FALSE;
	}

	DWORD borrow = (low < FT70_EPOCH_LOW) ? 1 : 0;
	*pLow  = low - FT70_EPOCH_LOW;
	*pHigh = high - FT70_EPOCH_HIGH - borrow;
	return TRUE;
}


//
// Normally, using macros is no good, but this macro saves us from
// writing constants twice -- it constructs signature names from codes.
// Note that "code_sym" argument should be a single symbol, not an expression.
//

#define SetCapInfo(cap_ptr, code_sym, vendor)			\
{														\
	rfbCapabilityInfo *pcap = (cap_ptr);				\
	pcap->code = Swap32IfLE(code_sym);					\
	memcpy(pcap->vendorSignature, (vendor),				\
	sz_rfbCapabilityInfoVendor);						\
	memcpy(pcap->nameSignature, sig_##code_sym,			\
	sz_rfbCapabilityInfoName);							\
}

// vncClient thread class

// ==========================================================================
// WIN32S SINGLE-THREADED CONVERSION
//
// "class vncClientThread : public omni_thread" used to live here, together with
// its destructor and its Init() (which called omni_thread::start()).
//
// Win32s has no threads.  omni_thread::start() threw omni_thread_fatal on that
// platform, and nothing in the call chain catches it - so every incoming
// connection terminated the process.
//
// All of the thread class's members are now members of vncClient itself:
//
//   InitVersion, InitAuthenticate, GetAuthenticationType,
//   SendConnFailedMessage, SendTextStringMessage, NegotiateTunneling,
//   NegotiateAuthentication, AuthenticateNone, AuthenticateVNC,
//   ReadClientInit, SendInteractionCaps, ConvertPath
//
// Their bodies are otherwise unchanged; the only edit is that "x"
// becomes "x" and "GetClientId()" becomes "GetClientId()", because
// the code now lives inside the client object it was manipulating.
//
// The former run() body has been split in two:
//
//   RunHandshake()     - everything run() did BEFORE its "MAIN LOOP" comment.
//                        Called inline from Init().  Still blocking, which is
//                        acceptable: it is bounded by VSocket's read/send
//                        deadlines and runs during connection setup.
//
//   HandleOneMessage() - the body of run()'s "while (connected)" loop, i.e.
//                        exactly one RFB message.  Called from PumpIdle().
//
// The destructor's job ("if we have a client object then delete it") is gone
// too: there is no separate thread object owning the client any more.  The
// client is owned by vncServer and freed by vncServer::ReapDeadClients().
// ==========================================================================

// Forward declaration retained: some sources still refer to the name.
class vncClientThread;

BOOL
vncClient::InitVersion()
{
	// Generate the server's protocol version
	rfbProtocolVersionMsg protocolMsg;
	sprintf((char *)protocolMsg, rfbProtocolVersionFormat, 3, 8);

	// Send the protocol message
	if (!m_socket->SendExact((char *)&protocolMsg, sz_rfbProtocolVersionMsg))
		return FALSE;

	// Now, get the client's protocol version
	rfbProtocolVersionMsg protocol_ver;
	protocol_ver[12] = 0;
	if (!m_socket->ReadExact((char *)&protocol_ver, sz_rfbProtocolVersionMsg))
		return FALSE;

	// Check the protocol version
	int major, minor;
	sscanf((char *)&protocol_ver, rfbProtocolVersionFormat, &major, &minor);
	if (major != 3) {
		vnclog.Print(LL_CONNERR, VNCLOG("unsupported protocol version %d.%d\n"),
					 major, minor);
		return FALSE;
	}
	int effective_minor = minor;
	if (minor > 8) {						// buggy client
		effective_minor = 8;
	} else if (minor > 3 && minor < 7) {	// non-standard client
		effective_minor = 3;
	} else if (minor < 3) {					// ancient client
		effective_minor = 3;
	}
	if (effective_minor != minor) {
		vnclog.Print(LL_CONNERR,
					 VNCLOG("non-standard protocol version 3.%d, using 3.%d instead\n"),
					 minor, effective_minor);
	}

	// Save the minor number of the protocol version
	m_protocol_minor_version = effective_minor;

	// TightVNC protocol extensions are not enabled yet
	m_protocol_tightvnc = FALSE;

	vnclog.Print(LL_INTINFO, VNCLOG("negotiated protocol version, RFB 3.%d\n"),
				 effective_minor);
	return TRUE;
}

BOOL
vncClient::InitAuthenticate()
{
	int secType = GetAuthenticationType();
	if (secType == rfbSecTypeInvalid)
		return FALSE;

	if (m_protocol_minor_version >= 7) {
		CARD8 list[3];
		list[0] = (CARD8)2;					// number of security types
		list[1] = (CARD8)secType;			// primary security type
		list[2] = (CARD8)rfbSecTypeTight;	// support for TightVNC extensions
		if (!m_socket->SendExact((char *)&list, sizeof(list)))
			return FALSE;
		CARD8 type;
		if (!m_socket->ReadExact((char *)&type, sizeof(type)))
			return FALSE;
		if (type == (CARD8)rfbSecTypeTight) {
			vnclog.Print(LL_INTINFO, VNCLOG("enabling TightVNC protocol extensions\n"));
			m_protocol_tightvnc = TRUE;
			if (!NegotiateTunneling())
				return FALSE;
			if (!NegotiateAuthentication(secType))
				return FALSE;
		} else if (type != (CARD8)secType) {
			vnclog.Print(LL_CONNERR, VNCLOG("incorrect security type requested\n"));
			return FALSE;
		}
	} else {
		CARD32 authValue = Swap32IfLE(secType);
		if (!m_socket->SendExact((char *)&authValue, sizeof(authValue)))
			return FALSE;
	}

	switch (secType) {
	case rfbSecTypeNone:
		vnclog.Print(LL_CLIENTS, VNCLOG("no authentication necessary\n"));
		return AuthenticateNone();
	case rfbSecTypeVncAuth:
		vnclog.Print(LL_CLIENTS, VNCLOG("performing VNC authentication\n"));
		return AuthenticateVNC();
	}

	return FALSE;	// should not happen but just in case...
}

int
vncClient::GetAuthenticationType()
{
	if (!m_reverse && !m_server->ValidPasswordsSet())
	{
		vnclog.Print(LL_CONNERR,
					 VNCLOG("no password specified for server - client rejected\n"));

		// Send an error message to the client
		SendConnFailedMessage("This server does not have a valid password enabled. "
							  "Until a password is set, incoming connections cannot "
							  "be accepted.");
		return rfbSecTypeInvalid;
	}

	// By default we filter out local loop connections, because they're pointless
	if (!m_server->LoopbackOk())
	{
		char *localname = strdup(m_socket->GetSockName());
		char *remotename = strdup(m_socket->GetPeerName());

		// Check that the local & remote names are different!
		if (localname != NULL && remotename != NULL) {
			BOOL ok = strcmp(localname, remotename) != 0;

// FIXME: conceivable memory leak
			free(localname);
			free(remotename);

			if (!ok) {
				vnclog.Print(LL_CONNERR,
							 VNCLOG("loopback connection attempted - client rejected\n"));

				// Send an error message to the client
				SendConnFailedMessage("Local loop-back connections are disabled.");
				return rfbSecTypeInvalid;
			}
		}
	}

	// Verify the peer host name against the AuthHosts string
	vncServer::AcceptQueryReject verified;
	if (m_reverse) {
		verified = vncServer::aqrAccept;
	} else {
		verified = m_server->VerifyHost(m_socket->GetPeerName());
	}

	// If necessary, query the connection with a timed dialog
	BOOL skip_auth = FALSE;
	if (verified == vncServer::aqrQuery) {
		vncAcceptDialog *acceptDlg =
			new vncAcceptDialog(m_server->QueryTimeout(),
								m_server->QueryAccept(),
								m_server->QueryAllowNoPass(),
								m_socket->GetPeerName());
		if (acceptDlg == NULL) {
			if (m_server->QueryAccept()) {
				verified = vncServer::aqrAccept;
			} else {
				verified = vncServer::aqrReject;
			}
		} else {
			int action = acceptDlg->DoDialog();
			if (action > 0) {
				verified = vncServer::aqrAccept;
				if (action == 2)
					skip_auth = TRUE;	// accept without authentication
			} else {
				verified = vncServer::aqrReject;
			}
			delete acceptDlg;
		}
	}

	// The connection should be rejected, either due to AuthHosts settings,
	// or because of the "Reject" action performed in the query dialog
	if (verified == vncServer::aqrReject) {
		vnclog.Print(LL_CONNERR, VNCLOG("Client connection rejected\n"));
		SendConnFailedMessage("Your connection has been rejected.");
		return rfbSecTypeInvalid;
	}

	// Return preferred authentication type
	if (m_reverse || skip_auth || m_server->ValidPasswordsEmpty()) {
		return rfbSecTypeNone;
	} else {
		return rfbSecTypeVncAuth;
	}
}

//
// Send a "connection failed" message.
//

void
vncClient::SendConnFailedMessage(const char *reasonString)
{
	if (m_protocol_minor_version >= 7) {
		CARD8 zeroCount = 0;
		if (!m_socket->SendExact((char *)&zeroCount, sizeof(zeroCount)))
			return;
	} else {
		CARD32 authValue = Swap32IfLE(rfbSecTypeInvalid);
		if (!m_socket->SendExact((char *)&authValue, sizeof(authValue)))
			return;
	}
	SendTextStringMessage(reasonString);
}

//
// Send a text message preceded with a length counter.
//

BOOL
vncClient::SendTextStringMessage(const char *str)
{
	CARD32 len = Swap32IfLE(strlen(str));
	if (!m_socket->SendExact((char *)&len, sizeof(len)))
		return FALSE;
	if (!m_socket->SendExact(str, strlen(str)))
		return FALSE;

	return TRUE;
}

//
// Negotiate tunneling type (protocol versions 3.7t, 3.8t).
//

BOOL
vncClient::NegotiateTunneling()
{
	int nTypes = 0;

	// Advertise our tunneling capabilities (currently, nothing to advertise).
	rfbTunnelingCapsMsg caps;
	caps.nTunnelTypes = Swap32IfLE(nTypes);
	return m_socket->SendExact((char *)&caps, sz_rfbTunnelingCapsMsg);

	// Read tunneling type requested by the client (currently, not necessary).
	if (nTypes) {
		CARD32 tunnelType;
		if (!m_socket->ReadExact((char *)&tunnelType, sizeof(tunnelType)))
			return FALSE;
		tunnelType = Swap32IfLE(tunnelType);
		// We cannot do tunneling yet.
		vnclog.Print(LL_CONNERR, VNCLOG("unsupported tunneling type requested\n"));
		return FALSE;
	}

	vnclog.Print(LL_INTINFO, VNCLOG("negotiated tunneling type\n"));
	return TRUE;
}

//
// Negotiate authentication scheme (protocol versions 3.7t, 3.8t).
// NOTE: Here we always send en empty list for "no authentication".
//

BOOL
vncClient::NegotiateAuthentication(int authType)
{
	int nTypes = 0;

	if (authType == rfbAuthVNC) {
		nTypes++;
	} else if (authType != rfbAuthNone) {
		vnclog.Print(LL_INTERR, VNCLOG("unknown authentication type\n"));
		return FALSE;
	}

	rfbAuthenticationCapsMsg caps;
	caps.nAuthTypes = Swap32IfLE(nTypes);
	if (!m_socket->SendExact((char *)&caps, sz_rfbAuthenticationCapsMsg))
		return FALSE;

	if (authType == rfbAuthVNC) {
		// Inform the client about supported authentication types.
		rfbCapabilityInfo cap;
		SetCapInfo(&cap, rfbAuthVNC, rfbStandardVendor);
		if (!m_socket->SendExact((char *)&cap, sz_rfbCapabilityInfo))
			return FALSE;

		CARD32 type;
		if (!m_socket->ReadExact((char *)&type, sizeof(type)))
			return FALSE;
		type = Swap32IfLE(type);
		if (type != authType) {
			vnclog.Print(LL_CONNERR, VNCLOG("incorrect authentication type requested\n"));
			return FALSE;
		}
	}

	return TRUE;
}

//
// Handle security type for "no authentication".
//

BOOL
vncClient::AuthenticateNone()
{
	if (m_protocol_minor_version >= 8) {
		CARD32 secResult = Swap32IfLE(rfbAuthOK);
		if (!m_socket->SendExact((char *)&secResult, sizeof(secResult)))
			return FALSE;
	}
	return TRUE;
}

//
// Perform standard VNC authentication
//

BOOL
vncClient::AuthenticateVNC()
{
	BOOL auth_ok = FALSE;

	// Retrieve local passwords
	char password[MAXPWLEN];
	BOOL password_set = m_server->GetPassword(password);
	vncPasswd::ToText plain(password);
	BOOL password_viewonly_set = m_server->GetPasswordViewOnly(password);
	vncPasswd::ToText plain_viewonly(password);

	// Now create a 16-byte challenge
	char challenge[16];
	char challenge_viewonly[16];

	vncRandomBytes((BYTE *)&challenge);
	memcpy(challenge_viewonly, challenge, 16);

	// Send the challenge to the client
	if (!m_socket->SendExact(challenge, sizeof(challenge)))
		return FALSE;

	// Read the response
	char response[16];
	if (!m_socket->ReadExact(response, sizeof(response)))
		return FALSE;

	// Encrypt the challenge bytes
	vncEncryptBytes((BYTE *)&challenge, plain);

	// Compare them to the response
	//
	// WIN32S DIAGNOSTIC: report which branch matched and what the input-enable
	// state is on the way in.
	//
	// The reported symptom is "the correct (non-view-only) password authenticates
	// but the session is view-only".  Three things could produce that and the log
	// has not yet distinguished them:
	//
	//   1. The primary comparison fails and the view-only one matches, so
	//      EnablePointer/EnableKeyboard(FALSE) below runs.  Then "primary=0
	//      viewonly=1" appears.
	//
	//   2. The primary comparison SUCCEEDS - so nothing here disables input - and
	//      input is disabled elsewhere.  The candidates are
	//      vncServer::AddClient's "EnableKeyboard(keysenabled &&
	//      m_enable_remote_inputs)" and the "InputsEnabled" INI value that feeds
	//      it.  Then "primary=1" appears together with "in: kbd=0 ptr=0".
	//
	//   3. Both passwords decrypt to the same plaintext despite being stored
	//      differently, in which case both comparisons match and the primary wins
	//      - "primary=1 viewonly=1(same)".
	//
	// The "in:" values are the state BEFORE this function can change anything, so
	// they show what AddClient set.
	{
		BOOL primaryMatch = (password_set &&
			memcmp(challenge, response, sizeof(response)) == 0);

		// Compute the view-only result too, for the diagnostic, before acting.
		char challenge_vo_test[16];
		memcpy(challenge_vo_test, challenge_viewonly, 16);
		vncEncryptBytes((BYTE *)&challenge_vo_test, plain_viewonly);
		BOOL viewonlyMatch = (password_viewonly_set &&
			memcmp(challenge_vo_test, response, sizeof(response)) == 0);

		vnclog.Print(LL_INTERR,
			VNCLOG("auth: primary=%d(set=%d) viewonly=%d(set=%d) "
				   "in: kbd=%d ptr=%d remote_inputs=%d\n"),
			(int)primaryMatch, (int)password_set,
			(int)viewonlyMatch, (int)password_viewonly_set,
			(int)IsKeyboardEnabled(), (int)IsPointerEnabled(),
			(int)m_server->RemoteInputsEnabled());

		if (primaryMatch) {
			// FULL-CONTROL login.  Do NOT touch the input-enable flags: they were
			// set by vncServer::AddClient from the server's own settings.
			auth_ok = TRUE;
			vnclog.Print(LL_INTERR,
				VNCLOG("auth: full-control login accepted\n"));
		} else if (viewonlyMatch) {
			memcpy(challenge_viewonly, challenge_vo_test, 16);
			EnablePointer(FALSE);
			EnableKeyboard(FALSE);
			auth_ok = TRUE;
			vnclog.Print(LL_INTERR,
				VNCLOG("auth: VIEW-ONLY login accepted - input disabled\n"));
		}
	}

	// Did the authentication work?
	CARD32 secResult;
	if (!auth_ok) {
		vnclog.Print(LL_CONNERR, VNCLOG("authentication failed\n"));

		secResult = Swap32IfLE(rfbAuthFailed);
		m_socket->SendExact((char *)&secResult, sizeof(secResult));
		SendTextStringMessage("Authentication failed");
		return FALSE;
	} else {
		// Tell the client we're ok
		secResult = Swap32IfLE(rfbAuthOK);
		if (!m_socket->SendExact((char *)&secResult, sizeof(secResult)))
			return FALSE;
	}

	return TRUE;
}

//
// Read client initialisation message
//

BOOL
vncClient::ReadClientInit()
{
	// Read the client's initialisation message
	rfbClientInitMsg client_ini;
	if (!m_socket->ReadExact((char *)&client_ini, sz_rfbClientInitMsg))
		return FALSE;

	// If the client wishes to have exclusive access then remove other clients
	if (!client_ini.shared && !m_shared)
	{
		// Which client takes priority, existing or incoming?
		if (m_server->ConnectPriority() < 1) {
			// Incoming
			vnclog.Print(LL_INTINFO, VNCLOG("non-shared connection - disconnecting old clients\n"));
			m_server->KillAuthClients();
		} else if (m_server->ConnectPriority() > 1) {
			// Existing
			if (m_server->AuthClientCount() > 0) {
				vnclog.Print(LL_CLIENTS, VNCLOG("connections already exist - client rejected\n"));
				return FALSE;
			}
		}
	}

	// Tell the server that this client is ok
	return m_server->Authenticated(GetClientId());
}

//
// Advertise our messaging capabilities (protocol version 3.7+).
//

BOOL
vncClient::SendInteractionCaps()
{
	// Update these constants on changing capability lists!
	const int MAX_SMSG_CAPS = 4;
	const int MAX_CMSG_CAPS = 6;
	const int MAX_ENC_CAPS = 14;

	int i;

	// Supported server->client message types
	rfbCapabilityInfo smsg_list[MAX_SMSG_CAPS];
	i = 0;

	if (m_server->FileTransfersEnabled() && IsInputEnabled()) {
		SetCapInfo(&smsg_list[i++], rfbFileListData,       rfbTightVncVendor);
		SetCapInfo(&smsg_list[i++], rfbFileDownloadData,   rfbTightVncVendor);
		SetCapInfo(&smsg_list[i++], rfbFileUploadCancel,   rfbTightVncVendor);
		SetCapInfo(&smsg_list[i++], rfbFileDownloadFailed, rfbTightVncVendor);
	}

	int nServerMsgs = i;
	if (nServerMsgs > MAX_SMSG_CAPS) {
		vnclog.Print(LL_INTERR,
					 VNCLOG("assertion failed, nServerMsgs > MAX_SMSG_CAPS\n"));
		return FALSE;
	}

	// Supported client->server message types
	rfbCapabilityInfo cmsg_list[MAX_CMSG_CAPS];
	i = 0;

	if (m_server->FileTransfersEnabled() && IsInputEnabled()) {
		SetCapInfo(&cmsg_list[i++], rfbFileListRequest,    rfbTightVncVendor);
		SetCapInfo(&cmsg_list[i++], rfbFileDownloadRequest,rfbTightVncVendor);
		SetCapInfo(&cmsg_list[i++], rfbFileUploadRequest,  rfbTightVncVendor);
		SetCapInfo(&cmsg_list[i++], rfbFileUploadData,     rfbTightVncVendor);
		SetCapInfo(&cmsg_list[i++], rfbFileDownloadCancel, rfbTightVncVendor);
		SetCapInfo(&cmsg_list[i++], rfbFileUploadFailed,   rfbTightVncVendor);
	}

	int nClientMsgs = i;
	if (nClientMsgs > MAX_CMSG_CAPS) {
		vnclog.Print(LL_INTERR,
					 VNCLOG("assertion failed, nClientMsgs > MAX_CMSG_CAPS\n"));
		return FALSE;
	}

	// Encoding types
	rfbCapabilityInfo enc_list[MAX_ENC_CAPS];
	i = 0;
	SetCapInfo(&enc_list[i++],  rfbEncodingCopyRect,       rfbStandardVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingRRE,            rfbStandardVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingCoRRE,          rfbStandardVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingHextile,        rfbStandardVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingZlib,           rfbTridiaVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingZlibHex,        rfbTridiaVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingTight,          rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingCompressLevel0, rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingQualityLevel0,  rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingXCursor,        rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingRichCursor,     rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingPointerPos,     rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingLastRect,       rfbTightVncVendor);
	SetCapInfo(&enc_list[i++],  rfbEncodingNewFBSize,      rfbTightVncVendor);
	int nEncodings = i;
	if (nEncodings > MAX_ENC_CAPS) {
		vnclog.Print(LL_INTERR,
					 VNCLOG("assertion failed, nEncodings > MAX_ENC_CAPS\n"));
		return FALSE;
	}

	// Create and send the header structure
	rfbInteractionCapsMsg intr_caps;
	intr_caps.nServerMessageTypes = Swap16IfLE(nServerMsgs);
	intr_caps.nClientMessageTypes = Swap16IfLE(nClientMsgs);
	intr_caps.nEncodingTypes = Swap16IfLE(nEncodings);
	intr_caps.pad = 0;
	if (!m_socket->SendExact((char *)&intr_caps, sz_rfbInteractionCapsMsg))
		return FALSE;

	// Send the capability lists
	if (nServerMsgs &&
		!m_socket->SendExact((char *)&smsg_list[0],
		sz_rfbCapabilityInfo * nServerMsgs))
		return FALSE;
	if (nClientMsgs &&
		!m_socket->SendExact((char *)&cmsg_list[0],
		sz_rfbCapabilityInfo * nClientMsgs))
		return FALSE;
	if (nEncodings &&
		!m_socket->SendExact((char *)&enc_list[0],
		sz_rfbCapabilityInfo * nEncodings))
		return FALSE;

	return TRUE;
}

void
ClearKeyState(BYTE key)
{
	// This routine is used by the VNC client handler to clear the
	// CAPSLOCK, NUMLOCK and SCROLL-LOCK states.

	BYTE keyState[256];

	GetKeyboardState((LPBYTE)&keyState);

	if(keyState[key] & 1)
	{
		// Simulate the key being pressed
		keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY, 0);

		// Simulate it being release
		keybd_event(key, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	}
}

// ==========================================================================
// RunHandshake - everything the old run() did before its "MAIN LOOP".
//
// Called inline from vncClient::Init().  Returns FALSE if the client must be
// rejected; the caller then removes it from the server.
//
// WIN32S NOTES ON THE CHANGES HERE:
//
//  * The GetThreadDesktop()/SelectHDESK() pair that saved and restored the
//    thread's "home" desktop is gone.  There is one desktop on Windows 3.1,
//    there is one thread, and GetThreadDesktop is an NT-only API that would
//    have been a load-time import.
//
//  * Every "m_server->RemoveClient(GetClientId()); return;" becomes
//    "return FALSE;".  Removing ourselves from the server from in here would
//    destroy this object while Init() is still running on it - the old code
//    got away with it only because RemoveClient merely signalled the thread.
// ==========================================================================

BOOL
vncClient::RunHandshake()
{
	// Blocking protocol handshake, run inline from Init().
	//
	// IMPORTANT: do NOT call m_server->RemoveClient() from in here.  That would
	// destroy this object while Init() is still executing on it.  Return FALSE
	// and let the caller reject the client.

	vnclog.Print(LL_CLIENTS, VNCLOG("client connected : %s (id %hd)\n"),
				 GetClientName(), GetClientId());

	// (Removed: GetThreadDesktop is NT-only and there is only one desktop.)

	// To avoid people connecting and then halting the connection, set a timeout
	if (!m_socket->SetTimeout(30000))
		vnclog.Print(LL_INTERR,
					 VNCLOG("failed to set socket timeout, error=%d\n"),
					 GetLastError());

	// Initially blacklist the client so that excess connections from it get dropped
	m_server->AddAuthHostsBlacklist(GetClientName());

	// LOCK INITIAL SETUP
	// All clients have the m_protocol_ready flag set to FALSE initially, to prevent
	// updates and suchlike interfering with the initial protocol negotiations.

	// GET PROTOCOL VERSION
	if (!InitVersion()) {
		return FALSE;
	}

	// AUTHENTICATE LINK
	if (!InitAuthenticate()) {
		return FALSE;
	}

	// READ CLIENT INITIALIZATION MESSAGE
	if (!ReadClientInit()) {
		return FALSE;
	}

	// Authenticated OK - remove from blacklist and remove timeout
	m_server->RemAuthHostsBlacklist(GetClientName());
	m_socket->SetTimeout(m_server->AutoIdleDisconnectTimeout()*1000);
	vnclog.Print(LL_INTINFO, VNCLOG("authenticated connection\n"));

	// INIT PIXEL FORMAT

	// Get the screen format
	m_fullscreen = m_buffer->GetSize();

	// Get the name of this desktop
	char desktopname[MAX_COMPUTERNAME_LENGTH+1];
	DWORD desktopnamelen = MAX_COMPUTERNAME_LENGTH + 1;
	if (GetComputerName(desktopname, &desktopnamelen))
	{
		// Make the name lowercase
		for (size_t x=0; x<strlen(desktopname); x++)
		{
			desktopname[x] = tolower(desktopname[x]);
		}
	}
	else
	{
		strcpy(desktopname, "WinVNC");
	}

	// Send the server format message to the client
	rfbServerInitMsg server_ini;
	server_ini.format = m_buffer->GetLocalFormat();

	// Endian swaps
	RECT sharedRect;
	sharedRect = m_server->GetSharedRect();
	server_ini.framebufferWidth = Swap16IfLE(sharedRect.right- sharedRect.left);
	server_ini.framebufferHeight = Swap16IfLE(sharedRect.bottom - sharedRect.top);
	server_ini.format.redMax = Swap16IfLE(server_ini.format.redMax);
	server_ini.format.greenMax = Swap16IfLE(server_ini.format.greenMax);
	server_ini.format.blueMax = Swap16IfLE(server_ini.format.blueMax);
	server_ini.nameLength = Swap32IfLE(strlen(desktopname));

	if (!m_socket->SendExact((char *)&server_ini, sizeof(server_ini)))
	{
		return FALSE;
	}
	if (!m_socket->SendExact(desktopname, strlen(desktopname)))
	{
		return FALSE;
	}
	vnclog.Print(LL_INTINFO, VNCLOG("sent pixel format to client\n"));

	// Inform the client about our interaction capabilities (protocol 3.7t)
	if (m_protocol_tightvnc) {
		if (!SendInteractionCaps()) {
			return FALSE;
		}
		vnclog.Print(LL_INTINFO, VNCLOG("list of interaction capabilities sent\n"));
	}

	// UNLOCK INITIAL SETUP
	// Initial negotiation is complete, so set the protocol ready flag
	{
		omni_mutex_lock l(m_regionLock);
		m_protocol_ready = TRUE;
	}

	// Clear the CapsLock and NumLock keys
	if (IsKeyboardEnabled())
	{
		ClearKeyState(VK_CAPITAL);
		// *** JNW - removed because people complain it's wrong
		//ClearKeyState(VK_NUMLOCK);
		ClearKeyState(VK_SCROLL);
	}

	return TRUE;
}

// ==========================================================================
// HandleOneMessage - the body of the old run() "while (connected)" loop.
//
// Processes EXACTLY ONE RFB message.  Returns FALSE when the conversation must
// end, which PumpIdle() turns into SetDead().
//
// The body is unchanged except that:
//   * "connected = FALSE; break;" inside the switch becomes "return FALSE;"
//     where the break was breaking out of the switch only - the original
//     relied on the while condition re-testing "connected" afterwards;
//   * the desktop check at the top is gone (one desktop, always selected).
// ==========================================================================

BOOL
vncClient::HandleOneMessage()
{
	rfbClientToServerMsg msg;

	// Read the message type.  PumpIdle() has already confirmed with
	// VSocket::HasData() that at least one byte is waiting, so this cannot
	// block indefinitely.
	if (!m_socket->ReadExact((char *)&msg.type, sizeof(msg.type)))
		return FALSE;

	switch(msg.type)
	{

	case rfbSetPixelFormat:
		// Read the rest of the message:
		if (!m_socket->ReadExact(((char *) &msg)+1, sz_rfbSetPixelFormatMsg-1))
		{
			return FALSE;
		}

		vnclog.Print(LL_INTINFO, VNCLOG("SetPixelFormat message received\n"));

		// Swap the relevant bits.
		msg.spf.format.redMax = Swap16IfLE(msg.spf.format.redMax);
		msg.spf.format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
		msg.spf.format.blueMax = Swap16IfLE(msg.spf.format.blueMax);

		{
			omni_mutex_lock l(m_regionLock);

			// Tell the buffer object of the change
			if (!m_buffer->SetClientFormat(msg.spf.format))
			{
				vnclog.Print(LL_CONNERR, VNCLOG("remote pixel format invalid\n"));

				return FALSE;
			}

			// Set the palette-changed flag, just in case...
			m_palettechanged = TRUE;
		}
		break;

	case rfbSetEncodings:
		// Read the rest of the message:
		if (!m_socket->ReadExact(((char *) &msg)+1, sz_rfbSetEncodingsMsg-1))
		{
			return FALSE;
		}

		vnclog.Print(LL_INTINFO, VNCLOG("SetEncodings message received\n"));

		m_buffer->SetQualityLevel(-1);
		m_buffer->SetCompressLevel(6);
		m_buffer->EnableXCursor(FALSE);
		m_buffer->EnableRichCursor(FALSE);
		m_buffer->EnableLastRect(FALSE);
		m_use_PointerPos = FALSE;
		m_use_NewFBSize = FALSE;

		m_cursor_update_pending = FALSE;
		m_cursor_update_sent = FALSE;
		m_cursor_pos_changed = FALSE;

		// Read in the preferred encodings
		msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);
		{
			int x;
			BOOL encoding_set = FALSE;
			BOOL shapeupdates_requested = FALSE;
			BOOL pointerpos_requested = FALSE;

			{
				omni_mutex_lock l(m_regionLock);
				// By default, don't use copyrect!
				m_copyrect_use = FALSE;
			}

			for (x = 0; x < msg.se.nEncodings; x++)
			{
				omni_mutex_lock l(m_regionLock);
				CARD32 encoding;

				// Read an encoding in
				if (!m_socket->ReadExact((char *)&encoding, sizeof(encoding)))
				{
					return FALSE;
				}

				// Is this the CopyRect encoding (a special case)?
				if (Swap32IfLE(encoding) == rfbEncodingCopyRect)
				{
					// Client wants us to use CopyRect
					m_copyrect_use = TRUE;
					continue;
				}

				// Is this an XCursor encoding request?
				if (Swap32IfLE(encoding) == rfbEncodingXCursor) {
					m_buffer->EnableXCursor(TRUE);
					shapeupdates_requested = TRUE;
					vnclog.Print(LL_INTINFO, VNCLOG("X-style cursor shape updates enabled\n"));
					continue;
				}

				// Is this a RichCursor encoding request?
				if (Swap32IfLE(encoding) == rfbEncodingRichCursor) {
					m_buffer->EnableRichCursor(TRUE);
					shapeupdates_requested = TRUE;
					vnclog.Print(LL_INTINFO, VNCLOG("Full-color cursor shape updates enabled\n"));
					continue;
				}

				// Is this a CompressLevel encoding?
				if ((Swap32IfLE(encoding) >= rfbEncodingCompressLevel0) &&
					(Swap32IfLE(encoding) <= rfbEncodingCompressLevel9))
				{
					// Client specified encoding-specific compression level
					int level = (int)(Swap32IfLE(encoding) - rfbEncodingCompressLevel0);
					m_buffer->SetCompressLevel(level);
					vnclog.Print(LL_INTINFO, VNCLOG("compression level requested: %d\n"), level);
					continue;
				}

				// Is this a QualityLevel encoding?
				if ((Swap32IfLE(encoding) >= rfbEncodingQualityLevel0) &&
					(Swap32IfLE(encoding) <= rfbEncodingQualityLevel9))
				{
					// Client specified image quality level used for JPEG compression
					int level = (int)(Swap32IfLE(encoding) - rfbEncodingQualityLevel0);
					m_buffer->SetQualityLevel(level);
					vnclog.Print(LL_INTINFO, VNCLOG("image quality level requested: %d\n"), level);
					continue;
				}

				// Is this a PointerPos encoding request?
				if (Swap32IfLE(encoding) == rfbEncodingPointerPos) {
					pointerpos_requested = TRUE;
					continue;
				}

				// Is this a LastRect encoding request?
				if (Swap32IfLE(encoding) == rfbEncodingLastRect) {
					m_buffer->EnableLastRect(TRUE);
					vnclog.Print(LL_INTINFO, VNCLOG("LastRect protocol extension enabled\n"));
					continue;
				}

				// Is this a NewFBSize encoding request?
				if (Swap32IfLE(encoding) == rfbEncodingNewFBSize) {
					m_use_NewFBSize = TRUE;
					vnclog.Print(LL_INTINFO, VNCLOG("NewFBSize protocol extension enabled\n"));
					continue;
				}

				// Have we already found a suitable encoding?
				if (!encoding_set)
				{
					// omni_mutex_lock l(m_regionLock);

					// No, so try the buffer to see if this encoding will work...
					if (m_buffer->SetEncoding(Swap32IfLE(encoding))) {
						encoding_set = TRUE;
					}

				}
			}

			// Enable CursorPos encoding only if cursor shape updates were
			// requested by the client.
			if (shapeupdates_requested && pointerpos_requested) {
				m_use_PointerPos = TRUE;
				SetCursorPosChanged();
				vnclog.Print(LL_INTINFO, VNCLOG("PointerPos protocol extension enabled\n"));
			}

			// If no encoding worked then default to RAW!
			// FIXME: Protocol extensions won't work in this case.
			if (!encoding_set)
			{
				omni_mutex_lock l(m_regionLock);

				vnclog.Print(LL_INTINFO, VNCLOG("defaulting to raw encoder\n"));

				if (!m_buffer->SetEncoding(Swap32IfLE(rfbEncodingRaw)))
				{
					vnclog.Print(LL_INTERR, VNCLOG("failed to select raw encoder!\n"));

					return FALSE;
				}
			}
		}

		break;

	case rfbFramebufferUpdateRequest:
		// Read the rest of the message:
		if (!m_socket->ReadExact(((char *) &msg)+1, sz_rfbFramebufferUpdateRequestMsg-1))
		{
			return FALSE;
		}

		if (msg.fur.incremental) {
			vnclog.Print(LL_INTINFO, VNCLOG("FramebufferUpdateRequest(incr) received\n"));
		} else {
			vnclog.Print(LL_INTINFO, VNCLOG("FramebufferUpdateRequest(full) received\n"));
		}

		{
			RECT update;
			RECT sharedRect;
			{
				omni_mutex_lock l(m_regionLock);

				sharedRect = m_server->GetSharedRect();
				// Get the specified rectangle as the region to send updates for.
				update.left = Swap16IfLE(msg.fur.x)+ sharedRect.left;
				update.top = Swap16IfLE(msg.fur.y)+ sharedRect.top;
				update.right = update.left + Swap16IfLE(msg.fur.w);

				_ASSERTE(Swap16IfLE(msg.fur.x) >= 0);
				_ASSERTE(Swap16IfLE(msg.fur.y) >= 0);

				//if (update.right > m_fullscreen.right)
				//	update.right = m_fullscreen.right;
				if (update.right > sharedRect.right)
					update.right = sharedRect.right;
				if (update.left < sharedRect.left)
					update.left = sharedRect.left;

				update.bottom = update.top + Swap16IfLE(msg.fur.h);
				//if (update.bottom > m_fullscreen.bottom)
				//	update.bottom = m_fullscreen.bottom;
				if (update.bottom > sharedRect.bottom)
					update.bottom = sharedRect.bottom;
				if (update.top < sharedRect.top)
					update.top = sharedRect.top;

				// Set the update-wanted flag to true
				m_updatewanted = TRUE;

				// Clip the rectangle to the screen
				if (IntersectRect(&update, &update, &sharedRect))
				{
					// Is this request for an incremental region?
					if (msg.fur.incremental)
					{
						// Yes, so add it to the incremental region
						m_incr_rgn.AddRect(update);
					}
					else
					{
						// No, so add it to the full update region
						m_full_rgn.AddRect(update);

						// Disable any pending CopyRect
						m_copyrect_set = FALSE;
					}
				}

				// Trigger an update
				m_server->RequestUpdate();
			}
		}
		break;

	case rfbKeyEvent:
		// Read the rest of the message:
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbKeyEventMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("KeyEvent message received\n"));

			// ==============================================================
			// WIN32S DIAGNOSTIC: report the RAW BYTES of the key message.
			//
			// The logs show only the six modifier keysyms (0xffe1-0xffea), all
			// with down=0, and NEVER the "latin-1 key:" line that
			// Keymapper::keyEvent emits for a printable character.  Since the
			// viewer works against every other server, the message is arriving
			// and being misread here.
			//
			// rfbKeyEventMsg is
			//     CARD8 type; CARD8 down; CARD16 pad; CARD32 key;   (8 bytes)
			//
			// That layout depends on the compiler placing CARD32 key at offset 4
			// with no additional padding.  MSVC 4.1's default packing is 8 bytes
			// (/Zp8), and there is NO #pragma pack in rfbproto.h - so if the
			// compiler inserted padding, or if 'pad' is not skipped correctly,
			// msg.ke.key would be read from the wrong offset and every keysym
			// would come out as garbage.
			//
			// Printing the raw bytes settles it: for the letter 'a' the wire
			// bytes are 04 01 00 00 00 00 00 61, so key should read 0x00000061.
			// ==============================================================
			{
				const unsigned char *raw = (const unsigned char *)&msg;
				vnclog.Print(LL_INTERR,
					VNCLOG("KeyEvent raw: %02x %02x %02x %02x %02x %02x %02x %02x "
						   "-> down=%d key=0x%08lx (sizeof=%d, keyoff=%d)\n"),
					raw[0], raw[1], raw[2], raw[3],
					raw[4], raw[5], raw[6], raw[7],
					(int)msg.ke.down,
					(unsigned long)Swap32IfLE(msg.ke.key),
					(int)sizeof(rfbKeyEventMsg),
					(int)((const char *)&msg.ke.key - (const char *)&msg.ke));
			}

			if (IsKeyboardEnabled() && !IsInputBlocked())
			{
				msg.ke.key = Swap32IfLE(msg.ke.key);
				// Get the keymapper to do the work
				vncKeymap::keyEvent(msg.ke.key, msg.ke.down != 0,
					m_server);
				m_remoteevent = TRUE;
			}
			else
			{
				vnclog.Print(LL_INTERR,
					VNCLOG("KeyEvent DISCARDED: kbd_enabled=%d input_blocked=%d\n"),
					(int)IsKeyboardEnabled(), (int)IsInputBlocked());
			}
		}
		break;

	case rfbPointerEvent:
		// Read the rest of the message:
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbPointerEventMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("PointerEvent message received\n"));

			if (IsPointerEnabled() && !IsInputBlocked())
			{
				// Convert the coords to Big Endian
				msg.pe.x = Swap16IfLE(msg.pe.x);
				msg.pe.y = Swap16IfLE(msg.pe.y);

				// Remember cursor position for this client
				m_cursor_pos.x = msg.pe.x;
				m_cursor_pos.y = msg.pe.y;

				// if we share only one window...

				RECT coord;
				{
					omni_mutex_lock l(m_regionLock);

					coord = m_server->GetSharedRect();
				}

				// to put position relative to screen
				msg.pe.x = (CARD16)(msg.pe.x + coord.left);
				msg.pe.y = (CARD16)(msg.pe.y + coord.top);

				// Work out the flags for this event
				DWORD flags = MOUSEEVENTF_ABSOLUTE;
				flags |= MOUSEEVENTF_MOVE;
				m_server->SetMouseCounter(1, m_cursor_pos, false );

				if ( (msg.pe.buttonMask & rfbButton1Mask) != 
					(m_ptrevent.buttonMask & rfbButton1Mask) )
				{
					if (GetSystemMetrics(SM_SWAPBUTTON))
						flags |= (msg.pe.buttonMask & rfbButton1Mask) 
						? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
					else
						flags |= (msg.pe.buttonMask & rfbButton1Mask) 
						? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
					m_server->SetMouseCounter(1, m_cursor_pos, false);
				}
				if ( (msg.pe.buttonMask & rfbButton2Mask) != 
					(m_ptrevent.buttonMask & rfbButton2Mask) )
				{
					flags |= (msg.pe.buttonMask & rfbButton2Mask) 
						? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
					m_server->SetMouseCounter(1, m_cursor_pos, false);
				}
				if ( (msg.pe.buttonMask & rfbButton3Mask) != 
					(m_ptrevent.buttonMask & rfbButton3Mask) )
				{
					if (GetSystemMetrics(SM_SWAPBUTTON))
						flags |= (msg.pe.buttonMask & rfbButton3Mask) 
						? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
					else
						flags |= (msg.pe.buttonMask & rfbButton3Mask) 
						? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
					m_server->SetMouseCounter(1, m_cursor_pos, false);
				}
				#define MOUSEEVENTF_WHEEL 0x0800
				// Treat buttons 4 and 5 presses as mouse wheel events
				DWORD wheel_movement = 0;
				if ((msg.pe.buttonMask & rfbButton4Mask) != 0 &&
					(m_ptrevent.buttonMask & rfbButton4Mask) == 0)
				{
					flags |= MOUSEEVENTF_WHEEL;
					wheel_movement = (DWORD)+120;
				}
				else if ((msg.pe.buttonMask & rfbButton5Mask) != 0 &&
					(m_ptrevent.buttonMask & rfbButton5Mask) == 0)
				{
					flags |= MOUSEEVENTF_WHEEL;
					wheel_movement = (DWORD)-120;
				}

				// Generate coordinate values
// PRB: should it be really only primary rect?
				HWND temp = GetDesktopWindow();
				GetWindowRect(temp,&coord);

				// WIN32S: guard the division.
				//
				// GetWindowRect on the desktop window can legitimately fail (it
				// leaves 'coord' untouched, and coord is not initialised), and a
				// 1x1 desktop would make the denominator zero.  An integer divide
				// by zero on Win32s faults the whole VM, not just this task - it
				// is not a catchable exception the way it is on NT.
				long spanX = coord.right - coord.left - 1;
				long spanY = coord.bottom - coord.top - 1;
				if (spanX < 1) spanX = 1;
				if (spanY < 1) spanY = 1;

				unsigned long x = (msg.pe.x * 65535) / spanX;
				unsigned long y = (msg.pe.y * 65535) / spanY;

				// ======================================================
				// WIN32S: MOUSEEVENTF_ABSOLUTE DOES NOT WORK.
				//
				// I previously asserted in a comment here that it "IS supported".
				// That was reasoning, not evidence, and the evidence contradicts
				// it: mouse_event is being called (1300+ times in one test log,
				// past the IsPointerEnabled gate) and the server's cursor does not
				// move AT ALL.  A wrong-coordinate bug would move the pointer
				// somewhere wrong; a stationary pointer means the call is being
				// ignored.
				//
				// MOUSEEVENTF_ABSOLUTE with 0..65535 normalised coordinates is
				// part of the Windows 95 mouse driver model.  Windows 3.1's driver
				// interface is relative-motion only, so mouse_event exists but
				// silently discards the absolute-position request.
				//
				// FIX: position the cursor with SetCursorPos(), which is a core
				// Windows 3.0 API taking plain screen pixel coordinates, and use
				// mouse_event ONLY for the button and wheel flags (with 0,0
				// movement, which is meaningful in the relative model).
				//
				// Note the coordinate difference: SetCursorPos wants absolute
				// SCREEN PIXELS, not the 0..65535 normalisation, so it takes
				// msg.pe.x/y directly - which are already screen-relative because
				// the shared-rect offset was added above.
				// ======================================================
				if (vncService::IsWin32s())
				{
					// 1. Move the pointer, if this event includes movement.
					//
					// If we are dragging a window by its caption (started in the
					// non-client branch below), move the window too.  This replaces
					// the system's modal move loop, which cannot be driven by
					// posted messages - see the long note there.
					if ((flags & MOUSEEVENTF_MOVE) && m_dragWindow != NULL)
					{
						if (IsWindow(m_dragWindow))
						{
							// WIN32S: MDI children (Progman groups, their icons)
							// are positioned in PARENT-client coordinates, not
							// screen coordinates.  Passing screen coordinates to
							// SetWindowPos for such a window makes it jump by the
							// parent's screen offset - the reported "icons move
							// when I click on them, not aligning to the pointer".
							// The grab offset itself is a pure pixel delta so it
							// is valid in either space; only the target origin
							// must be converted.
							int newX = (int)msg.pe.x - m_dragOffsetX;
							int newY = (int)msg.pe.y - m_dragOffsetY;
							HWND dragParent = GetParent(m_dragWindow);
							if (dragParent != NULL)
							{
								POINT org;
								org.x = newX;
								org.y = newY;
								ScreenToClient(dragParent, &org);
								newX = org.x;
								newY = org.y;
							}
							SetWindowPos(m_dragWindow, NULL,
										 newX,
										 newY,
										 0, 0,
										 SWP_NOSIZE | SWP_NOZORDER |
										 SWP_NOACTIVATE);
						}
						else
						{
							// The window went away mid-drag.
							m_dragWindow = NULL;
						}
					}

					// Interactive resize (started in the non-client branch on an
					// edge or corner hit).  Like dragging, this replaces the
					// system's modal resize loop.
					//
					// m_resizeHit tells us which edge or corner is being dragged, and
					// m_resizeRect is the original rectangle.  Windows 3.1 reports
					// mouse coordinates with the origin at the TOP-left and y
					// increasing DOWNWARD, and our msg.pe are already in screen
					// coordinates, so the maths is direct.
					if ((flags & MOUSEEVENTF_MOVE) && m_resizeActive &&
						m_resizeWindow != NULL && IsWindow(m_resizeWindow))
					{
						int x = (int)msg.pe.x;
						int y = (int)msg.pe.y;

						RECT r = m_resizeRect;

						switch (m_resizeHit)
						{
						case HTLEFT:        r.left   = x; break;
						case HTRIGHT:       r.right  = x; break;
						case HTTOP:         r.top    = y; break;
						case HTBOTTOM:      r.bottom = y; break;
						case HTTOPLEFT:     r.left   = x; r.top   = y; break;
						case HTTOPRIGHT:    r.right  = x; r.top   = y; break;
						case HTBOTTOMLEFT:  r.left   = x; r.bottom = y; break;
						case HTBOTTOMRIGHT: r.right  = x; r.bottom = y; break;
						default:            break;
						}

						// Enforce a minimum size and a sane orientation so the
						// rectangle cannot be dragged inside-out.
						if (r.right - r.left < 24)
						{
							if (m_resizeHit == HTLEFT) r.left = r.right - 24;
							else                       r.right = r.left + 24;
						}
						if (r.bottom - r.top < 24)
						{
							if (m_resizeHit == HTTOP)   r.top = r.bottom - 24;
							else                        r.bottom = r.top + 24;
						}

						// WIN32S: same parent-client conversion as dragging
						// above.  r is in screen coordinates; a child/MDI-child
						// window wants its origin in parent-client coordinates
						// while the size is invariant.
						{
							HWND rsParent = GetParent(m_resizeWindow);
							int rsX = r.left;
							int rsY = r.top;
							if (rsParent != NULL)
							{
								POINT rsOrg;
								rsOrg.x = rsX;
								rsOrg.y = rsY;
								ScreenToClient(rsParent, &rsOrg);
								rsX = rsOrg.x;
								rsY = rsOrg.y;
							}
						SetWindowPos(m_resizeWindow, NULL,
									 rsX, rsY,
									 r.right - r.left, r.bottom - r.top,
									 SWP_NOZORDER | SWP_NOACTIVATE);
						}
					}

					// WIN32S scrollbar thumb tracking (started in the non-client
					// branch on a thumb press).  Like dragging, this replaces the
					// system's modal scrollbar loop: map the cursor along the
					// strip to a scroll position and post THUMBTRACK.  The UP
					// block below posts THUMBPOSITION + ENDSCROLL to finish.
					if ((flags & MOUSEEVENTF_MOVE) && m_scrollActive &&
						m_scrollWindow != NULL)
					{
						if (IsWindow(m_scrollWindow))
						{
							int cPix = (m_scrollVert ? (int)msg.pe.y : (int)msg.pe.x)
									   - m_scrollStrip0;
							int sPos = ScrollPosFromPix(cPix, m_scrollArrow,
														m_scrollTrackLen,
														m_scrollMin, m_scrollMax);
							if (sPos < 0)
								sPos = 0;
							if (sPos > 32767)
								sPos = 32767;
							if (sPos != m_scrollLast)
							{
								m_scrollLast = sPos;
								PostMessage(m_scrollWindow,
											m_scrollVert ? WM_VSCROLL : WM_HSCROLL,
											(WPARAM)MAKELPARAM(SB_THUMBTRACK, sPos),
											0);
							}
						}
						else
						{
							// The window went away mid-drag.
							m_scrollWindow = NULL;
							m_scrollActive = FALSE;
						}
					}

					if (flags & MOUSEEVENTF_MOVE)
					{
						if (!SetCursorPos((int)msg.pe.x, (int)msg.pe.y))
						{
							static DWORD s_scpFail = 0;
							if ((s_scpFail++ % 100) == 0) {
								vnclog.Print(LL_INTERR,
									VNCLOG("SetCursorPos(%d,%d) failed, error=%d "
										   "[%d occurrences]\n"),
									(int)msg.pe.x, (int)msg.pe.y,
									GetLastError(), (int)s_scpFail);
							}
						}
					}

					// 2. Deliver the button transitions by POSTING MESSAGES.
					//
					// ==================================================
					// WIN32S: mouse_event CANNOT deliver button events.
					//
					// My previous attempt sent the button bits together with a
					// zero-delta MOUSEEVENTF_MOVE, on the theory that Windows 3.1
					// needs position and button state in one event record.  The
					// diagnostic disproved it: clicks arrive with correct flags
					// (0x2 down, 0x4 up), correct positions, less than a second
					// apart, and nothing happens.
					//
					// The real reason is the same one that defeats keybd_event
					// (see the long note in vncKeymap.cpp): on Win32s these
					// functions are thin stubs over the 16-bit Windows 3.1 entry
					// points, which inject into the HARDWARE EVENT QUEUE that only
					// a device driver may write to.  The Win95 behaviour of
					// synthesising into the system input stream does not exist.
					//
					// That is also why the movement fix worked - SetCursorPos
					// bypasses the queue and sets the position directly.
					//
					// REPLACEMENT: post the button message to the window under the
					// cursor, with client-relative coordinates in lParam.  This is
					// how remote-control software of the era did it and uses only
					// Windows 3.0 APIs.
					//
					// LIMITATION: an application that polls the physical button
					// state rather than processing messages will not see these.
					// ==================================================
					DWORD buttonBitsOnly = flags &
						~(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE);

					if (buttonBitsOnly != 0)
					{
						POINT pt;
						pt.x = (int)msg.pe.x;
						pt.y = (int)msg.pe.y;

						HWND target = WindowFromPoint(pt);
						if (target == NULL)
							target = GetDesktopWindow();

						// ==========================================
						// CLIENT AREA vs NON-CLIENT AREA.
						//
						// Posting WM_LBUTTONDOWN works for the client area, but
						// NOT for a title bar, border, menu bar, scroll bar or
						// system button - which is exactly the reported symptom
						// ("clicking inside a window works; the menu, borders and
						// dragging do not").
						//
						// Those are the NON-CLIENT area, and Windows delivers a
						// different message family for them:
						//
						//     WM_NCLBUTTONDOWN   wParam = hit-test code
						//                        lParam = SCREEN coordinates
						//
						// versus the client-area form:
						//
						//     WM_LBUTTONDOWN     wParam = button/modifier state
						//                        lParam = CLIENT coordinates
						//
						// The two differ in BOTH parameters, which is why sending
						// the client-area message to a title bar does nothing:
						// DefWindowProc reads wParam as a hit-test code, gets a
						// button mask instead, and ignores it.
						//
						// WM_NCHITTEST tells us which area the point is in and
						// yields the exact hit-test code to pass.  It is a Windows
						// 3.0 message, sent synchronously with SendMessage, and it
						// is the same mechanism the system itself uses.
						//
						// Note this makes dragging work as a consequence:
						// WM_NCLBUTTONDOWN with HTCAPTION is precisely what starts
						// a window move, and the system's own modal drag loop takes
						// over from there.
						// ==========================================
						LRESULT hit = SendMessage(target, WM_NCHITTEST, 0,
												  MAKELPARAM((short)pt.x,
															 (short)pt.y));

						BOOL nonClient = (hit != HTCLIENT && hit != HTNOWHERE);

						// Client-area parameters.
						POINT client = pt;
						ScreenToClient(target, &client);
						LPARAM lpClient = MAKELPARAM((short)client.x,
													 (short)client.y);

						// Non-client parameters: SCREEN coordinates.
						LPARAM lpScreen = MAKELPARAM((short)pt.x, (short)pt.y);

						// wParam for the client-area form: current button state.
						WPARAM wp = 0;
						if (msg.pe.buttonMask & rfbButton1Mask) wp |= MK_LBUTTON;
						if (msg.pe.buttonMask & rfbButton2Mask) wp |= MK_MBUTTON;
						if (msg.pe.buttonMask & rfbButton3Mask) wp |= MK_RBUTTON;

						// Bring the window forward on a press, so that clicking
						// into an inactive window behaves as a user would expect.
						//
						// Skipped for a non-client press: WM_NCLBUTTONDOWN on the
						// caption activates the window itself, and forcing it first
						// can steal the click.
						if (!nonClient &&
							(buttonBitsOnly & (MOUSEEVENTF_LEFTDOWN |
											   MOUSEEVENTF_RIGHTDOWN |
											   MOUSEEVENTF_MIDDLEDOWN)))
						{
							HWND top = target;
							HWND parent;
							while ((parent = GetParent(top)) != NULL)
								top = parent;
							Win32sSetForegroundWindow(top);
						}

						// WIN32S: end drag/resize on ANY left-button release,
						// regardless of where the release lands.  The old code
						// only cleared inside the non-client branch, so when a
						// drag moved the window out from under the cursor (or a
						// minimize made it vanish) the UP was classified as
						// client-area on a DIFFERENT window and the drag was
						// never cleared - every later motion kept moving the
						// window with no way to stop ("Progman group stuck in
						// drag").  This runs after the movement branch above,
						// so the window is dropped at the release position.
						if (buttonBitsOnly & MOUSEEVENTF_LEFTUP)
						{
							m_dragWindow = NULL;
							m_resizeActive = FALSE;
						}

						if (nonClient)
						{
							// ======================================
							// WM_NCLBUTTONDOWN DELEGATES TO A MODAL LOOP.
							// That is why the previous attempt misbehaved.
							//
							// DefWindowProc's response to WM_NCLBUTTONDOWN is not
							// to "handle the click" - it ENTERS one of the system's
							// modal tracking loops:
							//
							//   HTCAPTION / HTLEFT / HTTOP / ...  -> SC_MOVE or
							//        SC_SIZE, the window move/resize loop
							//   HTMENU / HTSYSMENU                -> the menu
							//        tracking loop
							//
							// Those loops POLL THE PHYSICAL MOUSE and wait for a
							// PHYSICAL button release.  Our posted messages never
							// reach them, which produced exactly the reported
							// behaviour:
							//
							//   * a border click always started a drag (the loop
							//     begins on button-down and we cannot tell it the
							//     click is over);
							//   * Enter was needed to finish (the loop's keyboard
							//     commit path - our WM_KEYDOWN does reach the
							//     queue);
							//   * menus did nothing (the loop opens and never sees
							//     our messages).
							//
							// SOLUTION: do not delegate.  Handle the non-client
							// cases ourselves with ordinary window management calls,
							// which need no physical input at all.
							//
							// TARGETING: aim at the HIT WINDOW, not its top-level parent.
							// The previous version walked up to the top-level and posted
							// SC_MINIMIZE etc. to it - which is why a Progman group
							// minimised Progman itself rather than the group.
							// ======================================

							if (buttonBitsOnly & MOUSEEVENTF_LEFTDOWN)
							{
								// WIN32S: synthesise non-client double-click.
								// The client-area branch below detects two rapid
								// presses and sends WM_LBUTTONDBLCLK, but this
								// branch never did - so double-clicking a caption
								// (maximize), a sysmenu (close) or a minimized
								// icon (restore, e.g. a Progman group or a
								// minimized program) did nothing.  Worse, the
								// second press started ANOTHER drag, which is why
								// double-clicking a group dragged it instead of
								// opening it.  Use the same rule as the system:
								// within GetDoubleClickTime() and double-click
								// distance, on the same window.
								DWORD ncNow = GetTickCount();
								int ncW = GetSystemMetrics(SM_CXDOUBLECLK);
								int ncH = GetSystemMetrics(SM_CYDOUBLECLK);
								if (ncW <= 0) ncW = 4;
								if (ncH <= 0) ncH = 4;
								BOOL ncDouble =
									(m_lastClickTime != 0) &&
									((DWORD)(ncNow - m_lastClickTime) <=
										GetDoubleClickTime()) &&
									(abs((int)msg.pe.x - m_lastClickX) <= ncW) &&
									(abs((int)msg.pe.y - m_lastClickY) <= ncH) &&
									(target == m_lastClickWindow);

								if (ncDouble && (hit == HTCAPTION || hit == HTSYSMENU))
								{
									// Second press of a double-click: do NOT
									// start a drag.  Perform the action directly.
									if (hit == HTCAPTION)
									{
										if (IsIconic(target))
											PostMessage(target, WM_SYSCOMMAND,
														(WPARAM)SC_RESTORE, 0);
										else
											PostMessage(target, WM_SYSCOMMAND,
														(WPARAM)(IsZoomed(target) ? SC_RESTORE
																				   : SC_MAXIMIZE),
														0);
									}
									else // HTSYSMENU double-click closes
									{
										PostMessage(target, WM_SYSCOMMAND,
													(WPARAM)SC_CLOSE, 0);
									}
									m_dragWindow = NULL;
									m_resizeActive = FALSE;
									m_lastClickTime = 0;
									// Cancel any pending single-click icon menu
									// armed by the first press's release - this
									// press proved to be a double-click.
									m_menuPendingWindow = NULL;
								}
								else
								{
								switch (hit)
								{
								case HTCAPTION:
									// WIN32S: iconic windows (minimized programs on
									// the desktop, minimized Progman groups in the
									// MDIClient) are dragged with the SAME
									// m_dragWindow mechanism as open windows - the
									// movement branch above already converts
									// parent-client coordinates, so icon dragging
									// is just caption dragging of a small window.
									//
									// Selection WITHOUT a modal loop: a physical
									// click would enter DefWindowProc's move loop,
									// which polls the physical mouse and can never
									// see our release.  Posting WM_LBUTTONDOWN to
									// the icon was tried and also hangs - Progman /
									// MDIClient enter their own tracking on it.
									// So this posts NOTHING: selection is done by
									// activating (MDI children via WM_MDIACTIVATE,
									// which is what highlights an MDI icon), and
									// a press-release without movement opens the
									// icon's system menu in the LEFTUP block below
									// (single-click on a 3.1 icon shows its Control
									// menu - that is the real behaviour, not our
									// invention).
									if (IsIconic(target))
									{
										HWND iconParent = GetParent(target);
										if (iconParent != NULL)
										{
											char parentCls[32];
											parentCls[0] = '\0';
											GetClassName(iconParent, parentCls,
														 sizeof(parentCls) - 1);
											if (lstrcmp(parentCls, "MDIClient") == 0)
												PostMessage(iconParent, WM_MDIACTIVATE,
															(WPARAM)target, 0);
											else
												Win32sSetForegroundWindow(target);
										}
										else
											Win32sSetForegroundWindow(target);
									}
									else
										Win32sSetForegroundWindow(target);
									m_dragWindow = target;
									{
										RECT wr;
										GetWindowRect(target, &wr);
										m_dragOffsetX = (int)msg.pe.x - wr.left;
										m_dragOffsetY = (int)msg.pe.y - wr.top;
									}
									break;

								case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
								case HTTOPLEFT: case HTTOPRIGHT: case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
									// Start an INTERACTIVE RESIZE.
									//
									// Like dragging, this replaces the system's modal
									// resize loop (SC_SIZE), which cannot be driven by
									// posted messages.  We remember the window, the hit
									// code (which edge/corner), and the original
									// rectangle; subsequent motion events resize it.
									//
									// This is not pixel-perfect - Windows 3.1's own loop
									// shows a rubber band and updates live, whereas we
									// snap to the cursor - but it works without a driver.
									Win32sSetForegroundWindow(target);
									m_resizeWindow = target;
									m_resizeHit = hit;
									GetWindowRect(target, &m_resizeRect);
									m_resizeActive = TRUE;
									break;

								case HTSYSMENU:
									// Open the window menu.  WM_SYSCOMMAND with
									// SC_KEYMENU is the documented way and does not
									// need a modal loop to be entered by us.
									Win32sSetForegroundWindow(target);
									PostMessage(target, WM_SYSCOMMAND,
												(WPARAM)SC_KEYMENU, (LPARAM)' ');
									break;

								case HTMENU:
									// A click on the MENU BAR.
									//
									// There is no message that says "open the menu
									// at this x position" - the system resolves that
									// inside its tracking loop, which we cannot
									// drive.  SC_KEYMENU with a null key opens the
									// first menu and then the ARROW KEYS navigate,
									// which our key injection does deliver.
									//
									// Not a perfect reproduction of a mouse click on
									// a specific menu title, but it makes menus
									// reachable, and keyboard navigation works from
									// there.
									Win32sSetForegroundWindow(target);
									PostMessage(target, WM_SYSCOMMAND,
												(WPARAM)SC_KEYMENU, (LPARAM)0);
									break;

								case HTMINBUTTON:
									PostMessage(target, WM_SYSCOMMAND,
												(WPARAM)SC_MINIMIZE, 0);
									break;

								case HTMAXBUTTON:
									// Toggle: restore if already maximised.
									PostMessage(target, WM_SYSCOMMAND,
												(WPARAM)(IsZoomed(target) ? SC_RESTORE
																	   : SC_MAXIMIZE),
												0);
									break;

								case HTCLOSE:
									PostMessage(target, WM_CLOSE, 0, 0);
									break;

								case HTVSCROLL:
								case HTHSCROLL:
									// WIN32S: window scrollbars (a Progman group's
									// scrollbars are WS_VSCROLL window scrollbars,
									// which hit-test as HTVSCROLL/HTHSCROLL).  The
									// old code fell through to default (activate
									// only), so clicks did nothing.  ScrollBar
									// CONTROLS are separate child windows that hit
									// as HTCLIENT and already work via the
									// client-area branch - untouched.
									//
									// No modal loop: arrows post one LINE scroll,
									// the shaft posts one PAGE scroll (sided by the
									// thumb marker), and a thumb press starts
									// tracking (moves post THUMBTRACK, release
									// posts THUMBPOSITION + ENDSCROLL).  No auto-
									// repeat while held - click again to repeat.
									//
									// NOTE: SB_LINEUP == SB_LINELEFT (0),
									// SB_LINEDOWN == SB_LINERIGHT (1),
									// SB_PAGEUP == SB_PAGELEFT (2),
									// SB_PAGEDOWN == SB_PAGERIGHT (3), so one code
									// path serves WM_VSCROLL and WM_HSCROLL.
									{
										BOOL sVert = (hit == HTVSCROLL);
										RECT sWrk;
										int sArrow;
										int sTotal;
										int sPix;
										int sMin;
										int sMax;
										int sCur;
										int sTrack;
										int sThumb;
										UINT sMsg;

										GetWindowRect(target, &sWrk);
										sArrow = GetSystemMetrics(sVert ? SM_CXVSCROLL
																		: SM_CYHSCROLL);
										if (sArrow <= 0)
											sArrow = 16;
										sTotal = sVert ? (sWrk.bottom - sWrk.top)
													   : (sWrk.right - sWrk.left);
										sPix = sVert ? ((int)msg.pe.y - sWrk.top)
													 : ((int)msg.pe.x - sWrk.left);
										if (sPix < 0)
											sPix = 0;
										if (sPix > sTotal)
											sPix = sTotal;
										sMin = 0;
										sMax = 0;
										GetScrollRange(target,
													   sVert ? SB_VERT : SB_HORZ,
													   &sMin, &sMax);
										sCur = GetScrollPos(target,
															sVert ? SB_VERT : SB_HORZ);
										sMsg = sVert ? WM_VSCROLL : WM_HSCROLL;
										Win32sSetForegroundWindow(target);
										if (sPix < sArrow)
										{
											PostMessage(target, sMsg,
														(WPARAM)SB_LINEUP, 0);
										}
										else if (sPix >= sTotal - sArrow)
										{
											PostMessage(target, sMsg,
														(WPARAM)SB_LINEDOWN, 0);
										}
										else
										{
											sTrack = ScrollTrackLen(sTotal, sArrow);
											sThumb = ScrollThumbPix(sCur, sMin, sMax,
																	sArrow, sTrack);
											if (sPix >= sThumb - 8 &&
												sPix <= sThumb + 8)
											{
												m_scrollWindow = target;
												m_scrollVert = sVert;
												m_scrollActive = TRUE;
												m_scrollMin = sMin;
												m_scrollMax = sMax;
												m_scrollArrow = sArrow;
												m_scrollTrackLen = sTrack;
												m_scrollStrip0 = sVert ? sWrk.top
																	   : sWrk.left;
												m_scrollLast = sCur;
											}
											else if (sPix < sThumb)
											{
												PostMessage(target, sMsg,
															(WPARAM)SB_PAGEUP, 0);
											}
											else
											{
												PostMessage(target, sMsg,
															(WPARAM)SB_PAGEDOWN, 0);
											}
										}
									}
									break;

								default:
									// Some other non-client area - just activate.
									Win32sSetForegroundWindow(target);
									break;
								}
									// Remember this press so the next one can be
									// recognised as a double-click.  Uses the same
									// m_lastClick* state as the client-area branch;
									// the window must match, so a drag that moves
									// to another window correctly starts a new pair.
									m_lastClickTime = ncNow;
									m_lastClickX = (int)msg.pe.x;
									m_lastClickY = (int)msg.pe.y;
									m_lastClickWindow = target;
								} // end else (single press)
							}

							if (buttonBitsOnly & MOUSEEVENTF_LEFTUP)
							{
								// End any drag or resize we started.
								m_dragWindow = NULL;
								m_resizeActive = FALSE;
								// WIN32S: single left-click on a minimized icon
								// opens its system menu - but NOT immediately.  On
								// real Windows 3.1 a quick press-release on an icon
								// (no drag) shows the Control menu
								// (Restore/Move/Close...); press + move instead
								// drags the icon.  The menu cannot open on UP,
								// because the UP of a double-click's FIRST press
								// would open a modal menu that swallows the SECOND
								// press (which then dismisses the menu instead of
								// reaching the icon) - double-click restore broke
								// exactly this way (repeated clicks, no restore).
								// So UP only ARMS a pending menu when the release
								// looks like a click: same window, still iconic,
								// barely moved, not the tail of a double-click
								// (which resets m_lastClickTime to 0).  PumpIdle
								// fires it once GetDoubleClickTime() passes with
								// no second press; the double-click path cancels
								// it below.  Gated to HTCAPTION so the HTSYSMENU
								// path (menu already posted on DOWN) does not arm
								// twice, and to iconic so an ordinary caption
								// click stays a no-op.
								if (hit == HTCAPTION &&
									IsWindow(target) && IsIconic(target) &&
									m_lastClickTime != 0 &&
									target == m_lastClickWindow &&
									abs((int)msg.pe.x - m_lastClickX) <= 4 &&
									abs((int)msg.pe.y - m_lastClickY) <= 4)
								{
									m_menuPendingWindow = target;
									m_menuPendingTime = GetTickCount();
								}
								// WIN32S: finish scrollbar thumb tracking.  The
								// movement branch posted THUMBTRACK while held;
								// commit the final position and close the gesture
								// so the app leaves tracking mode.  Runs for any
								// release (universal block), matching drag/resize.
								if (m_scrollActive && m_scrollWindow != NULL)
								{
									if (IsWindow(m_scrollWindow))
									{
										PostMessage(m_scrollWindow,
													m_scrollVert ? WM_VSCROLL
																 : WM_HSCROLL,
													(WPARAM)MAKELPARAM(SB_THUMBPOSITION,
																		m_scrollLast & 0xFFFF),
													0);
										PostMessage(m_scrollWindow,
													m_scrollVert ? WM_VSCROLL
																 : WM_HSCROLL,
													(WPARAM)SB_ENDSCROLL, 0);
									}
									m_scrollWindow = NULL;
									m_scrollActive = FALSE;
								}
							}

							if (buttonBitsOnly & MOUSEEVENTF_RIGHTDOWN)
							{
								// WIN32S: right-click brings up the window menu.
								// Intentional deviation, kept and documented:
								// Windows 3.1 has no right-click convention, and
								// the true mouse-menu modal loop (SC_MOUSEMENU /
								// TrackPopupMenu) polls the physical mouse so it
								// can never see an injected release.  SC_KEYMENU
								// opens the same system menu in keyboard mode,
								// which injected arrows + Enter CAN navigate -
								// the same reason the HTSYSMENU left-click path
								// uses it.  Applies to open and iconic windows.
								Win32sSetForegroundWindow(target);
								PostMessage(target, WM_SYSCOMMAND,
											(WPARAM)SC_KEYMENU, (LPARAM)' ');
							}
						}
						else
						{
							// ======================================
							// DOUBLE-CLICK must be synthesised.
							//
							// Windows generates WM_LBUTTONDBLCLK from two rapid
							// PHYSICAL clicks; it never sees ours, so the message is
							// never produced - which is why double-click has never
							// worked in this port.
							//
							// Detect it ourselves with the same rule the system
							// uses: two presses within GetDoubleClickTime() and
							// within a small distance of each other.  The second
							// press is then sent as WM_LBUTTONDBLCLK instead of
							// WM_LBUTTONDOWN, which is the sequence an application
							// expects (down, up, dblclk, up).
							//
							// GetDoubleClickTime and GetSystemMetrics(SM_CXDOUBLECLK)
							// are both Windows 3.0 APIs.
							// ======================================
							if (buttonBitsOnly & MOUSEEVENTF_LEFTDOWN)
							{
								DWORD nowClick = GetTickCount();
								int dcWidth = GetSystemMetrics(SM_CXDOUBLECLK);
								int dcHeight = GetSystemMetrics(SM_CYDOUBLECLK);
								if (dcWidth <= 0) dcWidth = 4;
								if (dcHeight <= 0) dcHeight = 4;

								BOOL isDouble =
									(m_lastClickTime != 0) &&
									((DWORD)(nowClick - m_lastClickTime) <=
										GetDoubleClickTime()) &&
									(abs((int)msg.pe.x - m_lastClickX) <= dcWidth) &&
									(abs((int)msg.pe.y - m_lastClickY) <= dcHeight) &&
									(target == m_lastClickWindow);

								if (isDouble)
								{
									PostMessage(target, WM_LBUTTONDBLCLK,
												wp, lpClient);
									// Reset so a third click starts a new pair
									// rather than chaining double-clicks.
									m_lastClickTime = 0;
								}
								else
								{
									PostMessage(target, WM_LBUTTONDOWN,
												wp, lpClient);
									m_lastClickTime = nowClick;
									m_lastClickX = (int)msg.pe.x;
									m_lastClickY = (int)msg.pe.y;
									m_lastClickWindow = target;
								}
							}
							if (buttonBitsOnly & MOUSEEVENTF_LEFTUP)
								PostMessage(target, WM_LBUTTONUP, wp, lpClient);
							if (buttonBitsOnly & MOUSEEVENTF_RIGHTDOWN)
								PostMessage(target, WM_RBUTTONDOWN, wp, lpClient);
							if (buttonBitsOnly & MOUSEEVENTF_RIGHTUP)
								PostMessage(target, WM_RBUTTONUP, wp, lpClient);
							if (buttonBitsOnly & MOUSEEVENTF_MIDDLEDOWN)
								PostMessage(target, WM_MBUTTONDOWN, wp, lpClient);
							if (buttonBitsOnly & MOUSEEVENTF_MIDDLEUP)
								PostMessage(target, WM_MBUTTONUP, wp, lpClient);
						}
					}

					// Diagnostic: confirm what is being injected, rate-limited.
					{
						static DWORD s_ptrCount = 0;
						static DWORD s_lastPtrReport = 0;
						s_ptrCount++;
						DWORD nowP = GetTickCount();
						if (s_lastPtrReport == 0)
							s_lastPtrReport = nowP;
						// Report EVERY button transition (they are rare and are
						// what we are debugging), plus a periodic movement summary.
						if (buttonBitsOnly != 0) {
							POINT pt2;
							pt2.x = (int)msg.pe.x;
							pt2.y = (int)msg.pe.y;
							HWND tgt = WindowFromPoint(pt2);
							char cls[64];
							cls[0] = '\0';
							if (tgt != NULL)
								GetClassName(tgt, cls, sizeof(cls) - 1);
							LRESULT ht = (tgt != NULL) ?
								SendMessage(tgt, WM_NCHITTEST, 0,
											MAKELPARAM((short)pt2.x, (short)pt2.y))
								: HTNOWHERE;
							vnclog.Print(LL_INTERR,
								VNCLOG("pointer BUTTON: flags=0x%lx at (%d,%d) "
									   "-> hwnd=%p class=\"%s\" hit=%d (%s) "
									   "mask=0x%02x icon=%d\n"),
								(unsigned long)buttonBitsOnly,
								(int)msg.pe.x, (int)msg.pe.y,
								tgt, cls, (int)ht,
								(ht == HTCLIENT) ? "client" : "non-client",
								(int)msg.pe.buttonMask,
								(tgt != NULL && IsWindow(tgt) && IsIconic(tgt)) ? 1 : 0);
						}
						else if ((DWORD)(nowP - s_lastPtrReport) >= 5000) {
							POINT actual;
							GetCursorPos(&actual);
							vnclog.Print(LL_INTERR,
								VNCLOG("pointer: %d events, last asked (%d,%d), "
									   "cursor now (%d,%d)\n"),
								(int)s_ptrCount, (int)msg.pe.x, (int)msg.pe.y,
								(int)actual.x, (int)actual.y);
							s_ptrCount = 0;
							s_lastPtrReport = nowP;
						}
					}
				}
				else
				{
					// Win9x / NT: the original absolute-coordinate path.
					::mouse_event(flags, (DWORD)x, (DWORD)y, wheel_movement, 0);
				}
				// Save the old position
				m_ptrevent = msg.pe;

				// Flag that a remote event occurred
				m_remoteevent = TRUE;
				m_pointer_event_time = time(NULL);

				// Flag that the mouse moved
				// FIXME: It should not set m_cursor_pos_changed here.
				UpdateMouse();

				// Trigger an update
				m_server->RequestUpdate();
			}
		}
		break;

	case rfbClientCutText:
		// Read the rest of the message:
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbClientCutTextMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("ClientCutText message received\n"));

			// Allocate storage for the text
			const UINT length = Swap32IfLE(msg.cct.length);
			char *text = new char [length+1];
			if (text == NULL)
				break;
			text[length] = 0;

			// Read in the text
			if (!m_socket->ReadExact(text, length)) {
				delete [] text;
				break;
			}

			// Get the server to update the local clipboard
			if (IsKeyboardEnabled() && IsPointerEnabled())
				m_server->UpdateLocalClipText(text);

			// Free the clip text we read
			delete [] text;
		}
		break;

	case rfbFileListRequest:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileListRequestMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileListRequest message received\n"));

			msg.flr.dirNameSize = Swap16IfLE(msg.flr.dirNameSize);

			// WIN32S: bail out CORRECTLY on an over-long name.
			//
			// The original was just "if (dirNameSize > 250) break;" - it did not
			// consume the name bytes that the client had already sent.  That
			// leaves the protocol stream desynchronised: the next message read
			// starts in the middle of this one, every subsequent message is
			// garbage, and the connection dies with "invalid message received".
			//
			// The same mistake is NOT present in the download/upload handlers
			// below, which do "ReadExact(NULL, fNameSize)" first - note that
			// VSocket::ReadExact with a NULL buffer is the idiom this code uses
			// for "discard n bytes".
			if (msg.flr.dirNameSize > 250) {
				m_socket->ReadExact(NULL, msg.flr.dirNameSize);
				omni_mutex_lock l(m_sendUpdateLock);
				rfbFileListDataMsg fld;
				fld.type = rfbFileListData;
				fld.numFiles = Swap16IfLE(0);
				fld.dataSize = Swap16IfLE(0);
				fld.compressedSize = Swap16IfLE(0);
				fld.flags = msg.flr.flags | 0x80;
				m_socket->SendExact((char *)&fld, sz_rfbFileListDataMsg);
				break;
			}
			
			char path[255 + 1];
			m_socket->ReadExact(path, msg.flr.dirNameSize);
			path[msg.flr.dirNameSize] = '\0';
			ConvertPath(path);
			
			if (!vncService::tryImpersonate()) {
				omni_mutex_lock l(m_sendUpdateLock);
				rfbFileListDataMsg fld;
				fld.type = rfbFileListData;
				fld.numFiles = Swap16IfLE(0);
				fld.dataSize = Swap16IfLE(0);
				fld.compressedSize = Swap16IfLE(0);
				fld.flags = msg.flr.flags | 0x80;
				m_socket->SendExact((char *)&fld, sz_rfbFileListDataMsg);
				break;
			}
			
			FileTransferItemInfo ftii;
			if (strlen(path) == 0) {
				// ==========================================================
				// WIN32S: enumerate the local drives for the client.
				//
				// Same problem, and the same fix, as on the viewer side:
				// GetLogicalDriveStrings() returns 0 on several Win32s builds
				// because the double-NUL-terminated multi-string form is not
				// filled in the way Win95 does it.  The original treated 0 as
				// fatal, replied with an empty list flagged 0x80 (error), and the
				// viewer showed an empty server pane with no explanation.
				//
				// Fall back to probing A: through Z: with GetDriveType(), which
				// works on every Win32 platform.
				//
				// The original walk was also wrong on its own terms:
				//   i += strcspn(&szDrivesList[i], "\0") + 1;
				// strcspn() with an empty reject set returns strlen(), so this
				// happened to advance correctly - but it is an obscure way to
				// write strlen() and it reads past the buffer if the final entry
				// is not terminated.  It also strdup'd and free'd every entry to
				// strip one trailing backslash.
				// ==========================================================
				char szDrivesList[512];
				char driveName[8];
				int nDrives = 0;

				memset(szDrivesList, 0, sizeof(szDrivesList));
				DWORD dwLen = GetLogicalDriveStrings(sizeof(szDrivesList) - 2,
													 szDrivesList);

				if (dwLen > 0 && dwLen < (DWORD)(sizeof(szDrivesList) - 2)) {
					char *p = szDrivesList;
					while (*p != '\0' &&
						   (DWORD)(p - szDrivesList) < dwLen) {
						// Entries look like "C:\".  Report "C:" - the viewer
						// appends its own separator when navigating.
						driveName[0] = p[0];
						driveName[1] = ':';
						driveName[2] = '\0';
						ftii.Add(driveName, -1, 0);
						nDrives++;
						p += strlen(p) + 1;
					}
				}

				if (nDrives == 0) {
					// Win32s fallback: probe each drive letter.
					//
					// SetErrorMode suppresses the "There is no disk in drive A:"
					// system modal box, which GetDriveType can otherwise trigger
					// for an empty floppy drive - on a server with nobody sitting
					// at it, that box would hang the machine.
					vnclog.Print(LL_INTINFO,
						VNCLOG("GetLogicalDriveStrings gave nothing - probing drives\n"));

					UINT savedMode = SetErrorMode(SEM_FAILCRITICALERRORS |
												  SEM_NOOPENFILEERRORBOX);
					char letter;
					for (letter = 'A'; letter <= 'Z'; letter++) {
						char root[8];
						root[0] = letter;
						root[1] = ':';
						root[2] = '\\';
						root[3] = '\0';
						UINT dt = GetDriveType(root);
						// 0 = DRIVE_UNKNOWN, 1 = DRIVE_NO_ROOT_DIR.  Anything
						// else is a real drive.
						if (dt > 1) {
							driveName[0] = letter;
							driveName[1] = ':';
							driveName[2] = '\0';
							ftii.Add(driveName, -1, 0);
							nDrives++;
						}
					}
					SetErrorMode(savedMode);
				}

				if (nDrives == 0) {
					// Genuinely nothing to report.
					vnclog.Print(LL_INTERR,
						VNCLOG("could not enumerate any local drives\n"));
					omni_mutex_lock l(m_sendUpdateLock);
					rfbFileListDataMsg fld;
					fld.type = rfbFileListData;
					fld.numFiles = Swap16IfLE(0);
					fld.dataSize = Swap16IfLE(0);
					fld.compressedSize = Swap16IfLE(0);
					fld.flags = msg.flr.flags | 0x80;
					m_socket->SendExact((char *)&fld, sz_rfbFileListDataMsg);
					vncService::undoImpersonate();
					break;
				}
			} else {
				// FIX: Check bounds before appending wildcard to completely avoid buffer overflow
				if (strlen(path) + 2 < sizeof(path)) {
					strcat(path, "\\*");
				}
				
				HANDLE FLRhandle;
				WIN32_FIND_DATA FindFileData;
				UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
				FLRhandle = FindFirstFile(path, &FindFileData);
				DWORD LastError = GetLastError();
				SetErrorMode(savedErrorMode);
				
				if (FLRhandle != INVALID_HANDLE_VALUE) {
					do {
						if (strcmp(FindFileData.cFileName, ".") != 0 &&
							strcmp(FindFileData.cFileName, "..") != 0) {

							// ==================================================
							// WIN32S: TWO REAL BUGS FIXED HERE.
							//
							// This block was:
							//
							//   LARGE_INTEGER li;
							//   __int64 total_ticks;
							//   li.LowPart  = ...ftLastWriteTime.dwLowDateTime;
							//   li.HighPart = ...ftLastWriteTime.dwHighDateTime;
							//   total_ticks = li.QuadPart;
							//   total_ticks = (total_ticks - 1164444736000000000i64)
							//                 / 10000000i64;
							//   li.QuadPart = total_ticks;
							//   ... ftii.Add(name, size, li.HighPart);
							//
							//  1. WRONG CONSTANT.  1164444736000000000 has one
							//     digit too many - the correct 1601->1970 epoch
							//     offset is 116444736000000000.  The subtraction
							//     therefore underflowed for every real file and
							//     produced a nonsense timestamp.
							//
							//  2. WRONG HALF.  It then passed li.HighPart - the
							//     HIGH 32 bits of the quotient - as the file's
							//     modification time.  For any date this century
							//     that is 0.  FiletimeToTime70() (further down
							//     this file) correctly returns the LOW half.
							//
							// Both are replaced by a call to FiletimeToTime70(),
							// which now does the conversion in 32-bit arithmetic
							// so MSVC 4.1 does not need its __aulldiv helper -
							// see the long comment on that function.
							// ==================================================
							unsigned int modTime =
								FiletimeToTime70(FindFileData.ftLastWriteTime);

							if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {	
								ftii.Add(FindFileData.cFileName, -1, 0);
							} else {
								if (!(msg.flr.flags & 0x10))
									ftii.Add(FindFileData.cFileName,
											 FindFileData.nFileSizeLow, modTime);
							}
						}

					} while (FindNextFile(FLRhandle, &FindFileData));
					FindClose(FLRhandle);
				} else {
					if (LastError != ERROR_SUCCESS && LastError != ERROR_FILE_NOT_FOUND) {
						omni_mutex_lock l(m_sendUpdateLock);

						rfbFileListDataMsg fld;
						fld.type = rfbFileListData;
						fld.numFiles = Swap16IfLE(0);
						fld.dataSize = Swap16IfLE(0);
						fld.compressedSize = Swap16IfLE(0);
						fld.flags = msg.flr.flags | 0x80;
						m_socket->SendExact((char *)&fld, sz_rfbFileListDataMsg);
						vncService::undoImpersonate();
						break;
					}
				}
			}
			
			// Fix loop tracking variable scopes for early-spec compiler standard execution
			//
			// WIN32S: the allocation below is unchecked in the original, and its
			// size is driven by how many entries the directory has.  A directory
			// with a few thousand files on a Win32s machine can easily fail to
			// allocate - and the code then wrote the whole message through a NULL
			// pointer.
			//
			// Note also that dsSize uses a hardcoded 8 for sizeof(FTSIZEDATA).
			// That is correct (two DWORDs) but fragile; use sizeof so it stays
			// correct.
			int dsSize = ftii.GetNumEntries() * sizeof(FTSIZEDATA);
			int msgLen = sz_rfbFileListDataMsg + dsSize + ftii.GetSummaryNamesLength() + ftii.GetNumEntries();
			char *pAllMessage = new char [msgLen];
			if (pAllMessage == NULL) {
				vnclog.Print(LL_INTERR,
					VNCLOG("out of memory building file list (%d bytes)\n"), msgLen);
				omni_mutex_lock l(m_sendUpdateLock);
				rfbFileListDataMsg fld;
				fld.type = rfbFileListData;
				fld.numFiles = Swap16IfLE(0);
				fld.dataSize = Swap16IfLE(0);
				fld.compressedSize = Swap16IfLE(0);
				fld.flags = msg.flr.flags | 0x80;
				m_socket->SendExact((char *)&fld, sz_rfbFileListDataMsg);
				vncService::undoImpersonate();
				break;
			}
			rfbFileListDataMsg *pFLD = (rfbFileListDataMsg *) pAllMessage;
			FTSIZEDATA *pftsd = (FTSIZEDATA *) &pAllMessage[sz_rfbFileListDataMsg];
			char *pFilenames = &pAllMessage[sz_rfbFileListDataMsg + dsSize];
			pFLD->type = rfbFileListData;
			pFLD->flags = msg.flr.flags & 0xF0;
			pFLD->numFiles = Swap16IfLE(ftii.GetNumEntries());
			pFLD->dataSize = Swap16IfLE(ftii.GetSummaryNamesLength() + ftii.GetNumEntries());
			pFLD->compressedSize = pFLD->dataSize;
			
			int loopIdx;
			for (loopIdx = 0; loopIdx < ftii.GetNumEntries(); loopIdx++) {
				// FIXED: Fetch the size into a signed integer to check for the -1 directory flag
				long currentSize = (long)ftii.GetSizeAt(loopIdx);

				if (currentSize == -1) {
					// Explicitly bind the network-layer representation of a Directory flag
					pftsd[loopIdx].size = Swap32IfLE(0xFFFFFFFF);
				} else {
					// Standard file sizing pipeline
					pftsd[loopIdx].size = Swap32IfLE((DWORD)currentSize);
				}

				pftsd[loopIdx].data = Swap32IfLE(ftii.GetDataAt(loopIdx));
				strcpy(pFilenames, ftii.GetNameAt(loopIdx));
				pFilenames = pFilenames + strlen(pFilenames) + 1;
			}
			
			omni_mutex_lock l(m_sendUpdateLock);
			m_socket->SendExact(pAllMessage, msgLen);
			delete[] pAllMessage;
			vncService::undoImpersonate();
		}
		break;

	case rfbFileDownloadRequest:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileDownloadRequestMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileDownloadRequest message received\n"));

			msg.fdr.fNameSize = Swap16IfLE(msg.fdr.fNameSize);
			msg.fdr.position = Swap32IfLE(msg.fdr.position);

			if (!vncService::tryImpersonate()) {
				m_socket->ReadExact(NULL, msg.fdr.fNameSize);
				char reason[] = "Cannot impersonate logged on user";
				int reasonLen = strlen(reason);
				SendFileDownloadFailed(reasonLen, reason);
				break;
			}
			// ">= 256" would be the exact bound for a char[256] buffer with a
			// NUL written at [fNameSize]; 255 is what the reason string says and
			// is safely inside it.
			if (msg.fdr.fNameSize > 255) {
				m_socket->ReadExact(NULL, msg.fdr.fNameSize);
				char reason[] = "Path length exceeds 255 bytes";
				int reasonLen = strlen(reason);
				SendFileDownloadFailed(reasonLen, reason);
				vncService::undoImpersonate();
				break;
			}
			char path_file[255 + 1];
			if (!m_socket->ReadExact(path_file, msg.fdr.fNameSize)) {
				vnclog.Print(LL_INTERR, VNCLOG("file download: short filename read\n"));
				vncService::undoImpersonate();
				break;
			}
			path_file[msg.fdr.fNameSize] = '\0';
			ConvertPath(path_file);
			strcpy(m_DownloadFilename, path_file);

			HANDLE hFile;
			DWORD sz_rfbFileSize;
			DWORD sz_rfbBlockSize = 8192;
			DWORD dwNumberOfBytesRead = 0;
			DWORD dwNumberOfAllBytesRead = 0;
			WIN32_FIND_DATA FindFileData;
			UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
			hFile = FindFirstFile(path_file, &FindFileData);
			DWORD LastError = GetLastError();
			SetErrorMode(savedErrorMode);

			vnclog.Print(LL_CLIENTS, VNCLOG("file download requested: %s\n"),
						 path_file);

			// WIN32S: validate the handle BEFORE touching FindFileData or
			// calling FindClose.  When FindFirstFile fails, FindFileData is
			// uninitialised stack garbage (so the attribute test below is
			// meaningless), and FindClose(INVALID_HANDLE_VALUE) faults this
			// process on Win32s - the "transfer crashes the server" case.
			if ((hFile == INVALID_HANDLE_VALUE) || (path_file[0] == '\0')) {
				char reason[] = "Cannot open file, perhaps it is absent or is a directory";
				int reasonLen = strlen(reason);
				SendFileDownloadFailed(reasonLen, reason);
				vncService::undoImpersonate();
				break;
			}
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				FindClose(hFile);
				char reason[] = "Cannot open file, perhaps it is absent or is a directory";
				int reasonLen = strlen(reason);
				SendFileDownloadFailed(reasonLen, reason);
				vncService::undoImpersonate();
				break;
			}
			sz_rfbFileSize = FindFileData.nFileSizeLow;
			FindClose(hFile);
			m_modTime = FiletimeToTime70(FindFileData.ftLastWriteTime);
			vnclog.Print(LL_INTINFO,
				VNCLOG("file download: starting %s, %d bytes\n"),
				path_file, (int)sz_rfbFileSize);
			if (sz_rfbFileSize == 0) {
				SendFileDownloadData(m_modTime);
			} else {
				if (sz_rfbFileSize <= sz_rfbBlockSize) sz_rfbBlockSize = sz_rfbFileSize;
				UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
				m_hFileToRead = CreateFile(path_file, GENERIC_READ, FILE_SHARE_READ, NULL,	OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
				SetErrorMode(savedErrorMode);
				if (m_hFileToRead != INVALID_HANDLE_VALUE) {
					m_bDownloadStarted = TRUE;
					SendFileDownloadPortion();
				} else {
					// WIN32S: the original silently did nothing here, so a client
					// asking for an unreadable file waited forever with no reply.
					// Tell it what happened.
					char reason[] = "Could not open file for reading";
					SendFileDownloadFailed((unsigned short)strlen(reason), reason);
				}
			}
			vncService::undoImpersonate();
		}
		break;

	case rfbFileUploadRequest:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileUploadRequestMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileUploadRequest message received\n"));

			msg.fupr.fNameSize = Swap16IfLE(msg.fupr.fNameSize);
			msg.fupr.position = Swap32IfLE(msg.fupr.position);

			if (!vncService::tryImpersonate()) {
				m_socket->ReadExact(NULL, msg.fupr.fNameSize);
				char reason[] = "Cannot impersonate logged on user";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				break;
			}
			// WIN32S: ">= MAX_PATH", not "> MAX_PATH".
			//
			// m_UploadFilename is char[MAX_PATH], and the next two lines write
			// fNameSize bytes and then a NUL at index fNameSize.  With
			// fNameSize == MAX_PATH that NUL lands one byte past the end of the
			// array - a classic off-by-one, and on Win32s the following member is
			// m_DownloadFilename, so it corrupted the download path.
			if (msg.fupr.fNameSize >= MAX_PATH) {
				m_socket->ReadExact(NULL, msg.fupr.fNameSize);
				char reason[] = "Path length exceeds MAX_PATH value";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				vncService::undoImpersonate();
				break;
			}
			if (!m_socket->ReadExact(m_UploadFilename, msg.fupr.fNameSize)) {
				vnclog.Print(LL_INTERR, VNCLOG("file upload: short filename read\n"));
				vncService::undoImpersonate();
				break;
			}
			m_UploadFilename[msg.fupr.fNameSize] = '\0';
			ConvertPath(m_UploadFilename);
			vnclog.Print(LL_CLIENTS, VNCLOG("file upload requested: %s\n"),
						 m_UploadFilename);

			// SetErrorMode around CreateFile: an upload to a path on a drive with
			// no disk in it (A:\...) would otherwise raise the system modal
			// "There is no disk in drive A:" box on a machine with nobody sitting
			// at it.  The download path above already does this.
			{
				UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS |
												   SEM_NOOPENFILEERRORBOX);
				m_hFileToWrite = CreateFile(m_UploadFilename, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
				SetErrorMode(savedErrorMode);
			}
			m_bUploadStarted = FALSE;
			if (m_hFileToWrite == INVALID_HANDLE_VALUE) {
				char reason[] = "Could not create file";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				vncService::undoImpersonate();
				break;
			}
			m_bUploadStarted = TRUE;
			/*
			DWORD dwError = GetLastError();
			SYSTEMTIME systime;
			FILETIME filetime;
			GetSystemTime(&systime);
			SystemTimeToFileTime(&systime, &filetime);
			beginUploadTime = FiletimeToTime70(filetime);
			*/        
			/*
			DWORD dwFilePtr;
			if (msg.fupr.position > 0) {
				dwFilePtr = SetFilePointer(m_hFiletoWrite, msg.fupr.position, NULL, FILE_BEGIN);
				if ((dwFilePtr == INVALID_SET_FILE_POINTER) && (dwError != NO_ERROR)) {
					char reason[] = "Invalid file pointer position";
					int reasonLen = strlen(reason);
					SendFileUploadCancel(reasonLen, reason);
					CloseHandle(m_hFiletoWrite);
					break;
				}
			}
			*/
			vncService::undoImpersonate();
		}				
		break;

	case rfbFileUploadData:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileUploadDataMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileUploadData message received\n"));

			msg.fud.realSize = Swap16IfLE(msg.fud.realSize);
			msg.fud.compressedSize = Swap16IfLE(msg.fud.compressedSize);

			if (!vncService::tryImpersonate()) {
				if (msg.fud.realSize == 0 && msg.fud.compressedSize == 0) {
					m_socket->ReadExact(NULL, sizeof(CARD32));
				} else {
					m_socket->ReadExact(NULL, msg.fud.compressedSize);
				}
				char reason[] = "Cannot impersonate logged on user";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				CloseUndoneFileTransfer();
				break;
			}
			if ((msg.fud.realSize == 0) && (msg.fud.compressedSize == 0)) {
				CARD32 mTime;
				if (!m_socket->ReadExact((char *) &mTime, sizeof(CARD32))) {
					vnclog.Print(LL_INTERR,
						VNCLOG("file upload: short read on end-marker (%s)\n"),
						m_UploadFilename);
					CloseUndoneFileTransfer();
					vncService::undoImpersonate();
					break;
				}
				mTime = Swap32IfLE(mTime);
				FILETIME Filetime;
				Time70ToFiletime(mTime, &Filetime);
				// WIN32S: m_hFileToWrite is INVALID when CreateFile failed (the
				// request handler now leaves it so).  SetFileTime/CloseHandle on
				// an invalid handle faults on Win32s.
				if (m_hFileToWrite != INVALID_HANDLE_VALUE && m_hFileToWrite != NULL) {
					if (!SetFileTime(m_hFileToWrite, &Filetime, &Filetime, &Filetime)) {
						vnclog.Print(LL_INTINFO, VNCLOG("SetFileTime() failed\n"));
					}
//					DWORD dwFileSize = GetFileSize(m_hFileToWrite, NULL);
					CloseHandle(m_hFileToWrite);
					m_hFileToWrite = INVALID_HANDLE_VALUE;
				}
				m_bUploadStarted = FALSE;
//					SYSTEMTIME systime;
//					FILETIME filetime;
//					GetSystemTime(&systime);
//					SystemTimeToFileTime(&systime, &filetime);
//					endUploadTime = FiletimeToTime70(filetime);
//					unsigned int uploadTime = endUploadTime - beginUploadTime + 1;
//					DWORD dwBytePerSecond = dwFileSize / uploadTime;
//					vnclog.Print(LL_CLIENTS, VNCLOG("file upload complete: %s; Speed (B/s) = %d; FileSize = %d, UploadTime = %d\n"),
//								 m_UploadFilename, dwBytePerSecond, dwFileSize, uploadTime);
				vnclog.Print(LL_CLIENTS, VNCLOG("file upload complete: %s;\n"),
							 m_UploadFilename);
				vncService::undoImpersonate();
				break;
			}
			DWORD dwNumberOfBytesWritten;
			// WIN32S: compressedSize comes from the wire (up to 64K) and MSVC
			// 4.1 new returns NULL on failure.  ReadExact(NULL, n) is the safe
			// discard idiom (see VSocket.cpp), so stay in sync before bailing.
			char *pBuff = new char [msg.fud.compressedSize];
			if (pBuff == NULL) {
				m_socket->ReadExact(NULL, msg.fud.compressedSize);
				char reason[] = "Server out of memory receiving file data";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
			if (!m_socket->ReadExact(pBuff, msg.fud.compressedSize)) {
				vnclog.Print(LL_INTERR,
					VNCLOG("file upload: short block read (%s)\n"),
					m_UploadFilename);
				delete[] pBuff;
				char reason[] = "Error receiving file data";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
			vnclog.Print(LL_INTINFO,
				VNCLOG("file upload: block real=%d comp=%d (%s)\n"),
				(int)msg.fud.realSize, (int)msg.fud.compressedSize,
				m_UploadFilename);
			if (msg.fud.compressedLevel != 0) {
				delete[] pBuff;
				char reason[] = "Server does not support data compression on upload";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
			BOOL bResult = WriteFile(m_hFileToWrite, pBuff, msg.fud.compressedSize, &dwNumberOfBytesWritten, NULL);
			delete[] pBuff;
			if ((dwNumberOfBytesWritten != msg.fud.compressedSize) || !bResult) {
				char reason[] = "Error writing file data";
				int reasonLen = strlen(reason);
				SendFileUploadCancel(reasonLen, reason);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
		}
		break;

	case rfbFileDownloadCancel:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileDownloadCancelMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileDownloadCancel message received\n"));

			vncService::tryImpersonate();
			msg.fdc.reasonLen = Swap16IfLE(msg.fdc.reasonLen);
			char *reason = new char[msg.fdc.reasonLen + 1];
			if (reason == NULL) {
				m_socket->ReadExact(NULL, msg.fdc.reasonLen);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
			m_socket->ReadExact(reason, msg.fdc.reasonLen);
			reason[msg.fdc.reasonLen] = '\0';
			CloseUndoneFileTransfer();
			delete [] reason;
			vncService::undoImpersonate();
		}
		break;

	case rfbFileUploadFailed:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileUploadFailedMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileUploadFailed message received\n"));

			vncService::tryImpersonate();
			msg.fuf.reasonLen = Swap16IfLE(msg.fuf.reasonLen);
			char *reason = new char[msg.fuf.reasonLen + 1];
			if (reason == NULL) {
				m_socket->ReadExact(NULL, msg.fuf.reasonLen);
				CloseUndoneFileTransfer();
				vncService::undoImpersonate();
				break;
			}
			m_socket->ReadExact(reason, msg.fuf.reasonLen);
			reason[msg.fuf.reasonLen] = '\0';
			CloseUndoneFileTransfer();
			delete [] reason;
			vncService::undoImpersonate();
		}
		break;

	case rfbFileCreateDirRequest:
		if (!m_server->FileTransfersEnabled() || !IsInputEnabled()) {
			return FALSE;
		}
		if (m_socket->ReadExact(((char *) &msg)+1, sz_rfbFileCreateDirRequestMsg-1))
		{
			vnclog.Print(LL_INTINFO, VNCLOG("FileCreateDirRequest message received\n"));

			vncService::tryImpersonate();
			msg.fcdr.dNameLen = Swap16IfLE(msg.fcdr.dNameLen);
			char *dirName = new char[msg.fcdr.dNameLen + 1];
			if (dirName == NULL) {
				m_socket->ReadExact(NULL, msg.fcdr.dNameLen);
				vncService::undoImpersonate();
				break;
			}
			m_socket->ReadExact(dirName, msg.fcdr.dNameLen);
			dirName[msg.fcdr.dNameLen] = '\0';
			dirName = ConvertPath(dirName);
			CreateDirectory((LPCTSTR) dirName, NULL);
			delete [] dirName;
			vncService::undoImpersonate();
		}

		break;

	default:
		// Unknown message, so fail!
		vnclog.Print(LL_CLIENTS, VNCLOG("invalid message received : %d\n"),
					 (int)msg.type);
		return FALSE;
	}

	return TRUE;
}

// ==========================================================================
// PumpIdle - idle-time driver for one client.
//
// Called from vncServer::PumpClients(), which the application idle loop calls.
// Contract:
//   * never blocks;
//   * services at most one client message per call;
//   * sends at most one file-download portion per call, so an in-progress
//     download keeps moving without any posted message;
//   * always flushes pending output, so update data keeps moving even when
//     the client is silent;
//   * returns TRUE if it did something, so the caller can pump again before
//     going back to sleep in GetMessage/WaitMessage.
// ==========================================================================

BOOL
vncClient::PumpIdle()
{
	if (m_dead || m_socket == NULL || m_socket->IsClosed())
		return FALSE;

	BOOL didWork = FALSE;

	// 0. Continue an in-progress file download: one 8K portion per pass.
	//
	// WIN32S: this used to be driven by PostToWinVNC(fileTransferDownloadMessage)
	// to the menu window, which then called back via checkPointer().  That chain
	// was observed to deliver exactly one pump message per download (one 53007 in
	// the log) and then go silent with the server otherwise healthy - downloads
	// stalled after the first block with no diagnostic.  Driving one portion
	// from here removes the FindWindow + registered-message + pointer-check
	// round trip entirely; the menu-window branch is retained as an inert
	// fallback (its checkPointer guard makes a stray firing safe).
	if (m_bDownloadStarted && m_protocol_ready) {
		SendFileDownloadPortion();
		didWork = TRUE;
	}

	// Fire a pending iconic single-click Control menu once the double-click
	// window has passed with no second press (see the UP handler: it arms but
	// never opens, so that a fast second press still reaches the icon and
	// restores it instead of landing on an open modal menu).  The ~one
	// double-click-time delay before the menu appears is the price of telling
	// click and double-click apart - real Windows pays it internally too.
	// Gated re-checked here because the window may have been restored,
	// closed, or destroyed while pending.
	if (m_menuPendingWindow != NULL) {
		if ((DWORD)(GetTickCount() - m_menuPendingTime) > GetDoubleClickTime()) {
			HWND pendingMenu = m_menuPendingWindow;
			m_menuPendingWindow = NULL;
			if (IsWindow(pendingMenu) && IsIconic(pendingMenu))
				PostMessage(pendingMenu, WM_SYSCOMMAND,
							(WPARAM)SC_KEYMENU, (LPARAM)' ');
			didWork = TRUE;
		}
	}

	// 1. Push out anything still queued from a previous update.
	if (m_socket->HasQueuedData()) {
		if (!m_socket->FlushQueued()) {
			vnclog.Print(LL_CONNERR,
				VNCLOG("client %hd: send failed, disconnecting\n"), GetClientId());
			SetDead();
			return FALSE;
		}
		didWork = TRUE;
	}

	// 2. Service one incoming message, if one has started arriving.
	if (m_socket->HasData()) {
		if (!HandleOneMessage()) {
			vnclog.Print(LL_CLIENTS,
				VNCLOG("client disconnected : %s (id %hd)\n"),
				GetClientName(), GetClientId());
			SetDead();
			return FALSE;
		}
		didWork = TRUE;
	}

	return didWork;
}

// ==========================================================================
// The old run() epilogue is no longer a separate step.
//
// It used to do:
//     vncService::SelectHDESK(home_desktop);   // NT-only, one desktop here
//     log "client disconnected"                // now done in PumpIdle
//     m_server->RemoveClient(GetClientId());   // now done by the reaper
//
// RemoveClient must NOT be called from the pump: it walks the client list
// that PumpClients() is iterating, and it deletes this object.  PumpIdle
// marks the client dead instead and vncServer::ReapDeadClients() removes it
// once iteration is finished.
// ==========================================================================


// The vncClient itself

vncClient::vncClient()
{
	vnclog.Print(LL_INTINFO, VNCLOG("vncClient() executing...\n"));

	m_socket = NULL;
	m_client_name = 0;
	m_server_name = 0;
	m_buffer = NULL;

	m_keyboardenabled = FALSE;
	m_pointerenabled = FALSE;
	m_inputblocked = FALSE;

	m_copyrect_use = FALSE;

	m_mousemoved = FALSE;
	m_ptrevent.buttonMask = 0;
	m_ptrevent.x = 0;
	m_ptrevent.y = 0;

	m_cursor_update_pending = FALSE;
	m_cursor_update_sent = FALSE;
	m_cursor_pos_changed = FALSE;
	m_pointer_event_time = (time_t)0;
	m_cursor_pos.x = -1;
	m_cursor_pos.y = -1;

	// WIN32S: m_thread is gone (there is no thread object any more).  The state
	// it implied is now explicit:
	m_dead = FALSE;
	m_reverse = FALSE;
	m_shared = FALSE;

	// WIN32S input synthesis state.
	m_dragWindow = NULL;
	m_dragOffsetX = 0;
	m_dragOffsetY = 0;
	m_resizeWindow = NULL;
	m_resizeHit = 0;
	m_resizeActive = FALSE;
	m_resizeRect.left = m_resizeRect.top = 0;
	m_resizeRect.right = m_resizeRect.bottom = 0;
	m_scrollWindow = NULL;
	m_scrollVert = FALSE;
	m_scrollActive = FALSE;
	m_scrollMin = 0;
	m_scrollMax = 0;
	m_scrollArrow = 0;
	m_scrollTrackLen = 0;
	m_scrollStrip0 = 0;
	m_scrollLast = 0;
	m_lastClickTime = 0;
	m_lastClickX = 0;
	m_lastClickY = 0;
	m_lastClickWindow = NULL;
	m_menuPendingWindow = NULL;
	m_menuPendingTime = 0;

	m_updatewanted = FALSE;

	m_palettechanged = FALSE;

	m_copyrect_set = FALSE;

	m_remoteevent = FALSE;

	m_bDownloadStarted = FALSE;
	m_bUploadStarted = FALSE;

	// These file-transfer members were never initialised, yet
	// CloseUndoneFileTransfer() - which Kill() calls, and Kill() runs on every
	// disconnect including one that never transferred a file - closes them.
	// CloseHandle() on stack garbage is an invalid-handle call.
	m_hFileToRead = INVALID_HANDLE_VALUE;
	m_hFileToWrite = INVALID_HANDLE_VALUE;
	m_UploadFilename[0] = '\0';
	m_DownloadFilename[0] = '\0';
	m_modTime = 0;
	beginUploadTime = 0;
	endUploadTime = 0;
	m_rfbBlockSize = 0;

	// Uninitialised in the original: m_protocol_minor_version is used by
	// InitAuthenticate to decide the security-type handshake, and
	// m_protocol_tightvnc gates the interaction-caps exchange.
	m_protocol_minor_version = 3;
	m_protocol_tightvnc = FALSE;
	m_id = 0;
	m_fullscreen.left = m_fullscreen.top = 0;
	m_fullscreen.right = m_fullscreen.bottom = 0;
	m_oldmousepos.left = m_oldmousepos.top = 0;
	m_oldmousepos.right = m_oldmousepos.bottom = 0;
	m_hcursor = NULL;
	m_use_PointerPos = FALSE;

	// IMPORTANT: Initially, client is not protocol-ready.
	m_protocol_ready = FALSE;
	m_fb_size_changed = FALSE;

	m_use_NewFBSize = FALSE;

}

vncClient::~vncClient()
{
	vnclog.Print(LL_INTINFO, VNCLOG("~vncClient() executing...\n"));

	// We now know the thread is dead, so we can clean up
	if (m_client_name != 0) {
		free(m_client_name);
		m_client_name = 0;
	}
	if (m_server_name != 0) {
		free(m_server_name);
		m_server_name = 0;
	}

	// If we have a socket then kill it
	if (m_socket != NULL)
	{
		vnclog.Print(LL_INTINFO, VNCLOG("deleting socket\n"));

		delete m_socket;
		m_socket = NULL;
	}

	// Kill the screen buffer
	if (m_buffer != NULL)
	{
		vnclog.Print(LL_INTINFO, VNCLOG("deleting buffer\n"));

		delete m_buffer;
		m_buffer = NULL;
	}
}

// Init
BOOL
vncClient::Init(vncServer *server,
				VSocket *socket,
				BOOL reverse,
				BOOL shared,
				vncClientId newid)
{
	// Save the server id;
	m_server = server;

	// Save the socket
	m_socket = socket;

	// Save the name/ip of the connecting client
	char *name = m_socket->GetPeerName();
	if (name != 0)
		m_client_name = strdup(name);
	else
		m_client_name = strdup("<unknown>");

	// Save the server name/ip
	name = m_socket->GetSockName();
	if (name != 0)
		m_server_name = strdup(name);
	else
		m_server_name = strdup("<unknown>");

	// Save the client id
	m_id = newid;

	// Save the connection flags that the thread object used to hold.
	m_reverse = reverse;
	m_shared = shared;

	// ------------------------------------------------------------------
	// WIN32S: run the handshake inline instead of spawning a thread.
	//
	// Was:
	//     m_thread = new vncClientThread;
	//     return ((vncClientThread *)m_thread)->Init(this, m_server,
	//                                                m_socket, reverse, shared);
	//
	// which called omni_thread::start().  On Win32s that threw
	// omni_thread_fatal(ERROR_NOT_SUPPORTED), and nothing between here and
	// WinMain catches it - so every incoming connection killed the process.
	//
	// RunHandshake() does the version exchange, authentication, ClientInit and
	// pixel-format negotiation.  It blocks, which is acceptable here: we are in
	// connection setup, and VSocket's read/send calls now have deadlines and
	// pump messages while they wait.
	//
	// After it succeeds, the conversation is driven by PumpIdle() from the
	// application idle loop.
	// ------------------------------------------------------------------
	m_dead = FALSE;

	if (!RunHandshake()) {
		// The caller (vncServer::AddClient) removes the client on FALSE.
		// Mark it dead too, so that if it does somehow stay in the list the
		// pump will not touch a half-initialised object.
		SetDead();
		return FALSE;
	}

	return TRUE;
}

void
vncClient::Kill()
{
	// Close file transfer
	CloseUndoneFileTransfer();

	// Close the socket.
	//
	// WIN32S: closing the socket is still how a client is told to go away - the
	// next PumpIdle() sees IsClosed() and stops - but mark ourselves dead
	// explicitly as well.  The original relied on the client THREAD noticing the
	// closed socket and returning, which then deleted the object.  With no
	// thread, nothing would ever notice if the client was in a state where
	// PumpIdle() is not being called (for example between AddClient and the
	// first idle pass).
	SetDead();

	if (m_socket != NULL)
		m_socket->Close();
}

// Client manipulation functions for use by the server
void
vncClient::SetBuffer(vncBuffer *buffer)
{
	// Until authenticated, the client object has no access
	// to the screen buffer.  This means that there only need
	// be a buffer when there's at least one authenticated client.
	m_buffer = buffer;
}


void
vncClient::TriggerUpdate()
{
	// Lock the updates stored so far
	omni_mutex_lock l(m_regionLock);
	if (!m_protocol_ready)
		return;

	if (m_updatewanted)
	{
		// Check if cursor shape update has to be sent
		m_cursor_update_pending = m_buffer->IsCursorUpdatePending();

		// Send an update if one is waiting
		if (!m_changed_rgn.IsEmpty() ||
			!m_full_rgn.IsEmpty() ||
			m_copyrect_set ||
			m_cursor_update_pending ||
			m_cursor_pos_changed ||
			(m_mousemoved && !m_use_PointerPos))
		{
			// Has the palette changed?
			if (m_palettechanged)
			{
				m_palettechanged = FALSE;
				if (!SendPalette())
					return;
			}

			// Now send the update
			m_updatewanted = !SendUpdate();
		}
	}
}

void
vncClient::UpdateMouse()
{
	if (!m_mousemoved && !m_cursor_update_sent)	{
		omni_mutex_lock l(m_regionLock);

		if (IntersectRect(&m_oldmousepos, &m_oldmousepos, &m_server->GetSharedRect()))
			m_changed_rgn.AddRect(m_oldmousepos);

		m_mousemoved = TRUE;
	} else if (m_use_PointerPos) {
		omni_mutex_lock l(m_regionLock);

		SetCursorPosChanged();
	}
}

void
vncClient::UpdateRect(RECT &rect)
{
	// Add the rectangle to the update region
	if (IsRectEmpty(&rect))
		return;

	omni_mutex_lock l(m_regionLock);

	if (IntersectRect(&rect, &rect, &m_server->GetSharedRect()))
		m_changed_rgn.AddRect(rect);
}

void
vncClient::UpdateRegion(vncRegion &region)
{
	// Merge our current update region with the supplied one
	if (region.IsEmpty())
		return;

	{
		omni_mutex_lock l(m_regionLock);

		// Merge the two
		vncRegion dummy;
		dummy.AddRect(m_server->GetSharedRect());
		region.Intersect(dummy);

		m_changed_rgn.Combine(region);
	}
}

void
vncClient::CopyRect(RECT &dest, POINT &source)
{
	// If CopyRect encoding is disabled or we already have a CopyRect pending,
	// then just redraw the region.
	if (!m_copyrect_use || m_copyrect_set) {
		UpdateRect(dest);
		return;
	}

	{
		omni_mutex_lock l(m_regionLock);

		// Clip the destination to the screen
		RECT destrect;
		if (!IntersectRect(&destrect, &dest, &m_server->GetSharedRect()))
			return;

		// Adjust the source correspondingly
		source.x = source.x + (destrect.left - dest.left);
		source.y = source.y + (destrect.top - dest.top);

		// Work out the source rectangle
		RECT srcrect;
		srcrect.left = source.x;
		srcrect.top = source.y;

		// And fill out the right & bottom using the dest rect
		srcrect.right = destrect.right-destrect.left + srcrect.left;
		srcrect.bottom = destrect.bottom-destrect.top + srcrect.top;

		// Clip the source to the screen
		RECT srcrect2;
		if (!IntersectRect(&srcrect2, &srcrect, &m_server->GetSharedRect()))
			return;

		// Correct the destination rectangle
		destrect.left += (srcrect2.left - srcrect.left);
		destrect.top += (srcrect2.top - srcrect.top);
		destrect.right = srcrect2.right-srcrect2.left + destrect.left;
		destrect.bottom = srcrect2.bottom-srcrect2.top + destrect.top;

		// Set the copyrect...
		m_copyrect_rect = destrect;
		m_copyrect_src.x = srcrect2.left;
		m_copyrect_src.y = srcrect2.top;

		m_copyrect_set = TRUE;
	}
}

void
vncClient::UpdateClipText(LPSTR text)
{
	if (!m_protocol_ready) return;

	// Don't send the clipboard contents to a view-only client
	if (!IsKeyboardEnabled() || !IsPointerEnabled())
		return;

	// Lock out any update sends and send clip text to the client
	omni_mutex_lock l(m_sendUpdateLock);

	rfbServerCutTextMsg message;
	message.length = Swap32IfLE(strlen(text));
	if (!SendRFBMsg(rfbServerCutText, (BYTE *) &message, sizeof(message)))
	{
		Kill();
		return;
	}
	if (!m_socket->SendQueued(text, strlen(text)))
	{
		Kill();
		return;
	}
}

void
vncClient::UpdatePalette()
{
	omni_mutex_lock l(m_regionLock);

	m_palettechanged = TRUE;
}

// Functions used to set and retrieve the client settings
const char*
vncClient::GetClientName()
{
	return (m_client_name != NULL) ? m_client_name : "[unknown]";
}

const char*
vncClient::GetServerName()
{
	return (m_server_name != NULL) ? m_server_name : "[unknown]";
}

// Internal methods
BOOL
vncClient::SendRFBMsg(CARD8 type, BYTE *buffer, int buflen)
{
	// Set the message type
	((rfbServerToClientMsg *)buffer)->type = type;

	// Send the message
	if (!m_socket->SendQueued((char *) buffer, buflen))
	{
		vnclog.Print(LL_CONNERR, VNCLOG("failed to send RFB message to client\n"));

		Kill();
		return FALSE;
	}
	return TRUE;
}


BOOL vncClient::SendUpdate()
{
#ifndef _DEBUG
	try
	{
#endif
		// First, check if we need to send pending NewFBSize message
		if (m_use_NewFBSize && m_fb_size_changed) {
			SetNewFBSize(TRUE);
			return TRUE;
		}

		vncRegion toBeSent;			// Region to actually be sent
		rectlist toBeSentList;		// List of rectangles to actually send
		vncRegion toBeDone;			// Region to check

		// Prepare to send cursor position update if necessary
		if (m_cursor_pos_changed) {
			POINT cursor_pos;
			if (!GetCursorPos(&cursor_pos)) {
				cursor_pos.x = 0;
				cursor_pos.y = 0;
			}
			RECT shared_rect = m_server->GetSharedRect();
			cursor_pos.x -= shared_rect.left;
			cursor_pos.y -= shared_rect.top;
			if (cursor_pos.x < 0) {
				cursor_pos.x = 0;
			} else if (cursor_pos.x >= shared_rect.right - shared_rect.left) {
				cursor_pos.x = shared_rect.right - shared_rect.left - 1;
			}
			if (cursor_pos.y < 0) {
				cursor_pos.y = 0;
			} else if (cursor_pos.y >= shared_rect.bottom - shared_rect.top) {
				cursor_pos.y = shared_rect.bottom - shared_rect.top - 1;
			}
			if (cursor_pos.x == m_cursor_pos.x && cursor_pos.y == m_cursor_pos.y) {
				m_cursor_pos_changed = FALSE;
			} else {
				m_cursor_pos.x = cursor_pos.x;
				m_cursor_pos.y = cursor_pos.y;
			}
		}

		toBeSent.Clear();
		if (!m_full_rgn.IsEmpty()) {
			m_incr_rgn.Clear();
			m_copyrect_set = false;
			toBeSent.Combine(m_full_rgn);
			m_changed_rgn.Clear();
			m_full_rgn.Clear();
		} else {
			if (!m_incr_rgn.IsEmpty()) {
				// Get region to send from vncDesktop
				toBeSent.Combine(m_changed_rgn);

				// Mouse stuff for the case when cursor shape updates are off
				if (!m_cursor_update_sent && !m_cursor_update_pending) {
					// If the mouse hasn't moved, see if its position is in the rect
					// we're sending. If so, make sure the full mouse rect is sent.
					if (!m_mousemoved) {
						vncRegion tmpMouseRgn;
						tmpMouseRgn.AddRect(m_oldmousepos);
						tmpMouseRgn.Intersect(toBeSent);
						if (!tmpMouseRgn.IsEmpty()) 
							m_mousemoved = TRUE;
					}
					// If the mouse has moved (or otherwise needs an update):
					if (m_mousemoved) {
						// Include an update for its previous position
						if (IntersectRect(&m_oldmousepos, &m_oldmousepos, &m_server->GetSharedRect())) 
							toBeSent.AddRect(m_oldmousepos);
						// Update the cached mouse position
						m_oldmousepos = m_buffer->GrabMouse();
						// Include an update for its current position
						if (IntersectRect(&m_oldmousepos, &m_oldmousepos, &m_server->GetSharedRect())) 
							toBeSent.AddRect(m_oldmousepos);
						// Indicate the move has been handled
						m_mousemoved = FALSE;
					}
				}
				m_changed_rgn.Clear();
			}
		}

		// Get the list of changed rectangles!
		int numrects = 0;
		if (toBeSent.Rectangles(toBeSentList))
		{
			// Find out how many rectangles this update will contain
			rectlist::iterator i;
			int numsubrects;
			for (i=toBeSentList.begin(); i != toBeSentList.end(); i++)
			{
				numsubrects = m_buffer->GetNumCodedRects(*i);

				// Skip remaining rectangles if an encoder will use LastRect extension.
				if (numsubrects == 0) {
					numrects = 0xFFFF;
					break;
				}
				numrects += numsubrects;
			}
		}

		if (numrects != 0xFFFF) {
			// Count cursor shape and cursor position updates.
			if (m_cursor_update_pending)
				numrects++;
			if (m_cursor_pos_changed)
				numrects++;
			// Handle the copyrect region
			if (m_copyrect_set)
				numrects++;
			// If there are no rectangles then return
			if (numrects != 0)
				m_incr_rgn.Clear();
			else
				return FALSE;
		}

		omni_mutex_lock l(m_sendUpdateLock);

		// Otherwise, send <number of rectangles> header
		rfbFramebufferUpdateMsg header;
		header.nRects = Swap16IfLE(numrects);
		if (!SendRFBMsg(rfbFramebufferUpdate, (BYTE *) &header, sz_rfbFramebufferUpdateMsg))
			return TRUE;

		// Send mouse cursor shape update
		if (m_cursor_update_pending) {
			if (!SendCursorShapeUpdate())
				return TRUE;
		}

		// Send cursor position update
		if (m_cursor_pos_changed) {
			if (!SendCursorPosUpdate())
				return TRUE;
		}

		// Encode & send the copyrect
		if (m_copyrect_set) {
			m_copyrect_set = FALSE;
			if(!SendCopyRect(m_copyrect_rect, m_copyrect_src))
				return TRUE;
		}

		// Encode & send the actual rectangles
		if (!SendRectangles(toBeSentList))
			return TRUE;

		// Send LastRect marker if needed.
		if (numrects == 0xFFFF) {
			if (!SendLastRect())
				return TRUE;
		}

		// Both lists should be empty when we exit
		_ASSERT(toBeSentList.empty());
#ifndef _DEBUG
	}
	catch (...)
	{
		vnclog.Print(LL_INTERR, VNCLOG("vncClient::SendUpdate caught an exception.\n"));
		throw;
	}
#endif

	return TRUE;
}

// Send a set of rectangles
BOOL
vncClient::SendRectangles(rectlist &rects)
{
	RECT rect;

	// Work through the list of rectangles, sending each one
	while(!rects.empty())
	{
		rect = rects.front();
		if (!SendRectangle(rect))
			return FALSE;

		rects.pop_front();
	}
	rects.erase(rects.begin(), rects.end());//rects.clear();

	return TRUE;
}

// Tell the encoder to send a single rectangle
BOOL vncClient::SendRectangle(RECT &rect)
{
	RECT sharedRect;
	{
		omni_mutex_lock l(m_regionLock);
		sharedRect = m_server->GetSharedRect();
	}

	IntersectRect(&rect, &rect, &sharedRect);
	// Get the buffer to encode the rectangle
	UINT bytes = m_buffer->TranslateRect(
		rect,
		m_socket,
		sharedRect.left,
		sharedRect.top);

    // Send the encoded data
    return m_socket->SendQueued((char *)(m_buffer->GetClientBuffer()), bytes);
}

// Send a single CopyRect message
BOOL vncClient::SendCopyRect(RECT &dest, POINT &source)
{
	RECT rc_shr = m_server->GetSharedRect();

	// Create the message header
	rfbFramebufferUpdateRectHeader copyrecthdr;
	copyrecthdr.r.x = Swap16IfLE(dest.left - rc_shr.left);
	copyrecthdr.r.y = Swap16IfLE(dest.top - rc_shr.top);

	copyrecthdr.r.w = Swap16IfLE(dest.right-dest.left);
	copyrecthdr.r.h = Swap16IfLE(dest.bottom-dest.top);
	copyrecthdr.encoding = Swap32IfLE(rfbEncodingCopyRect);

	// Create the CopyRect-specific section
	rfbCopyRect copyrectbody;
	copyrectbody.srcX = Swap16IfLE(source.x - rc_shr.left);
	copyrectbody.srcY = Swap16IfLE(source.y - rc_shr.top);

	// Now send the message;
	if (!m_socket->SendQueued((char *)&copyrecthdr, sizeof(copyrecthdr)))
		return FALSE;
	if (!m_socket->SendQueued((char *)&copyrectbody, sizeof(copyrectbody)))
		return FALSE;

	return TRUE;
}

// Send LastRect marker indicating that there are no more rectangles to send
BOOL
vncClient::SendLastRect()
{
	// Create the message header
	rfbFramebufferUpdateRectHeader hdr;
	hdr.r.x = 0;
	hdr.r.y = 0;
	hdr.r.w = 0;
	hdr.r.h = 0;
	hdr.encoding = Swap32IfLE(rfbEncodingLastRect);

	// Now send the message;
	if (!m_socket->SendQueued((char *)&hdr, sizeof(hdr)))
		return FALSE;

	return TRUE;
}

// Send the encoder-generated palette to the client
// This function only returns FALSE if the SendQueued fails - any other
// error is coped with internally...
BOOL
vncClient::SendPalette()
{
	rfbSetColourMapEntriesMsg setcmap;
	RGBQUAD *rgbquad;
	UINT ncolours = 256;

	// Reserve space for the colour data
	rgbquad = new RGBQUAD[ncolours];
	if (rgbquad == NULL)
		return TRUE;

	// Get the data
	if (!m_buffer->GetRemotePalette(rgbquad, ncolours))
	{
		delete [] rgbquad;
		return TRUE;
	}

	// Compose the message
	omni_mutex_lock l(m_sendUpdateLock);

	setcmap.type = rfbSetColourMapEntries;
	setcmap.firstColour = Swap16IfLE(0);
	setcmap.nColours = Swap16IfLE(ncolours);

	if (!m_socket->SendQueued((char *) &setcmap, sz_rfbSetColourMapEntriesMsg))
	{
		delete [] rgbquad;
		return FALSE;
	}

	// Now send the actual colour data...
	for (UINT i=0; i<ncolours; i++)
	{
		struct _PIXELDATA {
			CARD16 r, g, b;
		} pixeldata;

		pixeldata.r = Swap16IfLE(((CARD16)rgbquad[i].rgbRed) << 8);
		pixeldata.g = Swap16IfLE(((CARD16)rgbquad[i].rgbGreen) << 8);
		pixeldata.b = Swap16IfLE(((CARD16)rgbquad[i].rgbBlue) << 8);

		if (!m_socket->SendQueued((char *) &pixeldata, sizeof(pixeldata)))
		{
			delete [] rgbquad;
			return FALSE;
		}
	}

	// Delete the rgbquad data
	delete [] rgbquad;

	return TRUE;
}

BOOL
vncClient::SendCursorShapeUpdate()
{
	m_cursor_update_pending = FALSE;

	if (!m_buffer->SendCursorShape(m_socket)) {
		m_cursor_update_sent = FALSE;

		return m_buffer->SendEmptyCursorShape(m_socket);
	}

	m_cursor_update_sent = TRUE;
	return TRUE;
}

BOOL
vncClient::SendCursorPosUpdate()
{
	m_cursor_pos_changed = FALSE;

	rfbFramebufferUpdateRectHeader hdr;
	hdr.encoding = Swap32IfLE(rfbEncodingPointerPos);
	hdr.r.x = Swap16IfLE(m_cursor_pos.x);
	hdr.r.y = Swap16IfLE(m_cursor_pos.y);
	hdr.r.w = Swap16IfLE(0);
	hdr.r.h = Swap16IfLE(0);

	return m_socket->SendQueued((char *)&hdr, sizeof(hdr));
}

// Send NewFBSize pseudo-rectangle to notify the client about
// framebuffer size change
BOOL
vncClient::SetNewFBSize(BOOL sendnewfb)
{
	rfbFramebufferUpdateRectHeader hdr;
	RECT sharedRect;

	sharedRect = m_server->GetSharedRect();

	m_full_rgn.Clear();
	m_incr_rgn.Clear();
	m_full_rgn.AddRect(sharedRect);

	if (!m_use_NewFBSize) {
		// We cannot send NewFBSize message right now, maybe later
		m_fb_size_changed = TRUE;

	} else if (sendnewfb) {
		hdr.r.x = 0;
		hdr.r.y = 0;
		hdr.r.w = Swap16IfLE(sharedRect.right - sharedRect.left);
		hdr.r.h = Swap16IfLE(sharedRect.bottom - sharedRect.top);
		hdr.encoding = Swap32IfLE(rfbEncodingNewFBSize);

		rfbFramebufferUpdateMsg header;
		header.nRects = Swap16IfLE(1);
		if (!SendRFBMsg(rfbFramebufferUpdate, (BYTE *)&header,
			sz_rfbFramebufferUpdateMsg))
            return FALSE;

		// Now send the message
		if (!m_socket->SendQueued((char *)&hdr, sizeof(hdr)))
			return FALSE;

		// No pending NewFBSize anymore
		m_fb_size_changed = FALSE;
	}

	return TRUE;
}

void
vncClient::UpdateLocalFormat()
{
	m_buffer->UpdateLocalFormat();
}

char * 
vncClient::ConvertPath(char *path)
{
	size_t len = strlen(path);
	size_t i;
	if (len == 0 || len >= 255) return path;

	// Safe handling for root forward slashes
	if (path[0] == '/' && len == 1) {
		path[0] = '\0'; 
		return path;
	}

	// 1. Transform all web/VNC forward slashes into canonical Windows backslashes
	for (i = 0; i < len; i++) {
		if (path[i] == '/') {
			path[i] = '\\';
		}
	}

	// 2. Safely strip a leading slash if it precedes a drive letter (e.g. "\C:" -> "C:")
	// FIX: memmove must include the null terminator (len - 1 + 1 = len)
	if (path[0] == '\\' && len >= 3 && path[2] == ':') {
		memmove(path, path + 1, len); 
		len--;
	}

	// 3. Ensure a raw root drive letter always has a trailing slash for Win32 (e.g. "C:" -> "C:\")
	if (len == 2 && path[1] == ':') {
		path[2] = '\\';
		path[3] = '\0';
	}

	return path;
}


void 
vncClient::SendFileUploadCancel(unsigned short reasonLen, char *reason)
{
	omni_mutex_lock l(m_sendUpdateLock);

	int msgLen = sz_rfbFileUploadCancelMsg + reasonLen;
	char *pAllFUCMessage = new char[msgLen];
	if (pAllFUCMessage == NULL)		// WIN32S: MSVC 4.1 new returns 0, not throws
		return;
	rfbFileUploadCancelMsg *pFUC = (rfbFileUploadCancelMsg *) pAllFUCMessage;
	char *pFollow = &pAllFUCMessage[sz_rfbFileUploadCancelMsg];
	pFUC->type = rfbFileUploadCancel;
	pFUC->reasonLen = Swap16IfLE(reasonLen);
	memcpy(pFollow, reason, reasonLen);
	m_socket->SendExact(pAllFUCMessage, msgLen);
	delete [] pAllFUCMessage;
}

void 
vncClient::Time70ToFiletime(unsigned int mTime, FILETIME *pFiletime)
{
	// WIN32S: was
	//     LONGLONG ll = Int32x32To64(mTime, 10000000) + 116444736000000000;
	//
	// Int32x32To64 is a macro that expands to a 64-bit multiply; on MSVC 4.1
	// targeting a 386 that becomes a CRT helper call.  Do the 32x32->64 multiply
	// and the 64-bit add by hand instead - both are simple and exact.
	if (pFiletime == NULL)
		return;

	// 32x32 -> 64 multiply: mTime * 10,000,000
	//
	// Split both operands into 16-bit halves so that every partial product fits
	// in 32 bits without overflow:
	//
	//     a*b = p3<<32 + (p1 + p2)<<16 + p0
	//
	// Two details that are easy to get wrong here (and that I did get wrong in a
	// first draft - verified against a 64-bit reference before committing):
	//
	//   * (p1 & 0xFFFF) + (p2 & 0xFFFF) can reach 0x1FFFE, i.e. it carries into
	//     bit 16.  That carry bit must go to the HIGH word, so the value shifted
	//     into the low word has to be masked to 16 bits first.
	//
	//   * The carry out of the low-word addition must be folded into 'high'
	//     *after* it is detected but it is simplest to accumulate it with the
	//     other high-word terms in one expression.
	const DWORD mul = 10000000UL;
	DWORD aLo = mTime & 0xFFFFUL;
	DWORD aHi = mTime >> 16;
	DWORD bLo = mul & 0xFFFFUL;
	DWORD bHi = mul >> 16;

	DWORD p0 = aLo * bLo;
	DWORD p1 = aLo * bHi;
	DWORD p2 = aHi * bLo;
	DWORD p3 = aHi * bHi;

	DWORD midLo16 = (p1 & 0xFFFFUL) + (p2 & 0xFFFFUL);	// may be up to 0x1FFFE
	DWORD low = p0 + ((midLo16 & 0xFFFFUL) << 16);
	DWORD carry = (low < p0) ? 1UL : 0UL;
	DWORD high = p3 + (p1 >> 16) + (p2 >> 16) + (midLo16 >> 16) + carry;

	// Add the 1601->1970 epoch offset.
	DWORD newLow = low + FT70_EPOCH_LOW;
	if (newLow < low)
		high++;
	high += FT70_EPOCH_HIGH;

	pFiletime->dwLowDateTime = newLow;
	pFiletime->dwHighDateTime = high;
}

void 
vncClient::SendFileDownloadFailed(unsigned short reasonLen, char *reason)
{
	omni_mutex_lock l(m_sendUpdateLock);

	int msgLen = sz_rfbFileDownloadFailedMsg + reasonLen;
	char *pAllFDFMessage = new char[msgLen];
	if (pAllFDFMessage == NULL)
		return;
	rfbFileDownloadFailedMsg *pFDF = (rfbFileDownloadFailedMsg *) pAllFDFMessage;
	char *pFollow = &pAllFDFMessage[sz_rfbFileDownloadFailedMsg];
	pFDF->type = rfbFileDownloadFailed;
	pFDF->reasonLen = Swap16IfLE(reasonLen);
	memcpy(pFollow, reason, reasonLen);
	m_socket->SendExact(pAllFDFMessage, msgLen);
	delete [] pAllFDFMessage;
}

void 
vncClient::SendFileDownloadData(unsigned int mTime)
{
	omni_mutex_lock l(m_sendUpdateLock);

	int msgLen = sz_rfbFileDownloadDataMsg + sizeof(unsigned int);
	char *pAllFDDMessage = new char[msgLen];
	if (pAllFDDMessage == NULL)
		return;
	rfbFileDownloadDataMsg *pFDD = (rfbFileDownloadDataMsg *) pAllFDDMessage;
	unsigned int *pFollow = (unsigned int *) &pAllFDDMessage[sz_rfbFileDownloadDataMsg];
	pFDD->type = rfbFileDownloadData;
	pFDD->compressLevel = 0;
	pFDD->compressedSize = Swap16IfLE(0);
	pFDD->realSize = Swap16IfLE(0);
	memcpy(pFollow, &mTime, sizeof(unsigned int));
	m_socket->SendExact(pAllFDDMessage, msgLen);
	delete [] pAllFDDMessage;
}

void
vncClient::SendFileDownloadPortion()
{
	if (!m_bDownloadStarted) return;
	DWORD dwNumberOfBytesRead = 0;
	m_rfbBlockSize = 8192;
	// WIN32S: MSVC 4.1 new returns NULL on failure instead of throwing, and
	// an 8K block is a big ask on a Win3.1 machine mid-transfer.
	char *pBuff = new char[m_rfbBlockSize];
	if (pBuff == NULL) {
		vnclog.Print(LL_INTERR,
			VNCLOG("file download: out of memory for 8K block (%s)\n"),
			m_DownloadFilename);
		CloseHandle(m_hFileToRead);
		m_hFileToRead = INVALID_HANDLE_VALUE;
		m_bDownloadStarted = FALSE;
		char reason[] = "Server out of memory reading file";
		SendFileDownloadFailed((unsigned short)strlen(reason), reason);
		return;
	}
	BOOL bResult = ReadFile(m_hFileToRead, pBuff, m_rfbBlockSize, &dwNumberOfBytesRead, NULL);
	if (!bResult) {
		// WIN32S: the original fell through and sent a zero/short block
		// WITHOUT the end-of-file mTime tail, desynchronising the RFB stream
		// (the viewer reads mTime out of the next message), then reposted the
		// pump - an infinite garbage loop.  Fail clean instead.
		DWORD readErr = GetLastError();
		vnclog.Print(LL_INTERR,
			VNCLOG("file download: ReadFile failed, error=%d (%s)\n"),
			(int)readErr, m_DownloadFilename);
		delete [] pBuff;
		CloseHandle(m_hFileToRead);
		m_hFileToRead = INVALID_HANDLE_VALUE;
		m_bDownloadStarted = FALSE;
		char reason[] = "Error reading file data";
		SendFileDownloadFailed((unsigned short)strlen(reason), reason);
		return;
	}
	if (dwNumberOfBytesRead == 0) {
		/* This is the end of the file. */
		SendFileDownloadData(m_modTime);
		vnclog.Print(LL_CLIENTS, VNCLOG("file download complete: %s\n"), m_DownloadFilename);
		CloseHandle(m_hFileToRead);
		m_hFileToRead = INVALID_HANDLE_VALUE;
		m_bDownloadStarted = FALSE;
		return;
	}
	SendFileDownloadData((unsigned short)dwNumberOfBytesRead, pBuff);
	delete [] pBuff;
	vnclog.Print(LL_INTINFO,
		VNCLOG("file download: sent %d bytes (%s)\n"),
		(int)dwNumberOfBytesRead, m_DownloadFilename);
	// WIN32S: the next portion is sent by PumpIdle() on its next pass - do NOT
	// PostToWinVNC(fileTransferDownloadMessage) here.  Posting as well would
	// drive the same portion twice (duplicate blocks on the wire).  The
	// vncMenu pump branch stays as an inert fallback; its checkPointer guard
	// makes any stray firing safe.
}

void 
vncClient::SendFileDownloadData(unsigned short sizeFile, char *pFile)
{
	omni_mutex_lock l(m_sendUpdateLock);

	int msgLen = sz_rfbFileDownloadDataMsg + sizeFile;
	char *pAllFDDMessage = new char[msgLen];
	if (pAllFDDMessage == NULL)
		return;
	rfbFileDownloadDataMsg *pFDD = (rfbFileDownloadDataMsg *) pAllFDDMessage;
	char *pFollow = &pAllFDDMessage[sz_rfbFileDownloadDataMsg];
	pFDD->type = rfbFileDownloadData;
	pFDD->compressLevel = 0;
	pFDD->compressedSize = Swap16IfLE(sizeFile);
	pFDD->realSize = Swap16IfLE(sizeFile);
	memcpy(pFollow, pFile, sizeFile);
	m_socket->SendExact(pAllFDDMessage, msgLen);
	delete [] pAllFDDMessage;

}

// ==========================================================================
// FILETIME <-> Unix time conversion, WITHOUT 64-bit arithmetic.
//
// The RFB file-transfer protocol carries modification times as a Unix time_t
// (seconds since 1970-01-01), which the viewer displays and uses to set the
// timestamp on a downloaded file.  A Win32 FILETIME counts 100-nanosecond
// intervals since 1601-01-01, so the conversion is
//
//     unix_seconds = (filetime - 116444736000000000) / 10000000
//
// The original did that with LARGE_INTEGER::QuadPart, i.e. __int64.
//
// WIN32S / MSVC 4.1 NOTES
//
//   MSVC 4.1 does support __int64 as a type, but it has no hardware 64-bit
//   division on a 386/486 - the compiler emits a call to a CRT helper
//   (__aulldiv / __alldiv).  Those helpers live in the CRT, so they link; but
//   this is on the file-listing path, called once per directory entry, and on a
//   16 MHz 386 the cost is noticeable when listing a large directory.
//
//   More importantly the original code was WRONG in two places, and the bug is
//   easy to miss because it only affects the displayed timestamp:
//
//     * The file-list handler in HandleOneMessage used the constant
//       1164444736000000000 - one digit too many (an extra 4).  The correct
//       epoch offset is 116444736000000000.  With the wrong constant the
//       subtraction underflows and the resulting "time" is garbage, which is why
//       remote file dates looked wrong.
//
//     * It then stored li.HighPart - the HIGH word of the result - as the time
//       value, rather than the low word.  FiletimeToTime70 (correctly) returns
//       LowPart.
//
//   The division is done in two 32-bit steps with a shift-and-subtract loop, so
//   no 64-bit CRT helper is needed.  Precision is exact for every value a
//   FILETIME can hold.
//
//   NOTE: the implementation - FT70_EPOCH_HIGH/LOW, Div64By32() and
//   SubtractFt70Epoch() - lives near the TOP of this file, immediately after the
//   includes.  It has to: Time70ToFiletime() above uses the same macros, and a
//   macro must be defined textually before the line that expands it.
// ==========================================================================

unsigned int 
vncClient::FiletimeToTime70(FILETIME filetime)
{
	DWORD high = filetime.dwHighDateTime;
	DWORD low  = filetime.dwLowDateTime;

	if (!SubtractFt70Epoch(&high, &low))
		return 0;

	// Divide by 10,000,000 to get seconds.  The result fits in 32 bits until
	// the year 2106.
	return (unsigned int)Div64By32(high, low, 10000000UL);
}

void
vncClient::CloseUndoneFileTransfer()
{
	// WIN32S: check the handles, not just the flags.
	//
	// Kill() calls this on EVERY disconnect, including one where no transfer ever
	// happened, and m_hFileToRead / m_hFileToWrite were never initialised in the
	// constructor - so this was CloseHandle() on stack garbage.  (The constructor
	// now initialises them to INVALID_HANDLE_VALUE as well; both halves of the fix
	// are needed, because a completed transfer also leaves the flags set.)
	if (m_bUploadStarted) {
		m_bUploadStarted = FALSE;
		if (m_hFileToWrite != INVALID_HANDLE_VALUE && m_hFileToWrite != NULL) {
			CloseHandle(m_hFileToWrite);
			m_hFileToWrite = INVALID_HANDLE_VALUE;
		}
		// Remove the partial upload.  DeleteFile on an empty name is harmless but
		// pointless.
		if (m_UploadFilename[0] != '\0')
			DeleteFile(m_UploadFilename);
	}
	if (m_bDownloadStarted) {
		m_bDownloadStarted = FALSE;
		if (m_hFileToRead != INVALID_HANDLE_VALUE && m_hFileToRead != NULL) {
			CloseHandle(m_hFileToRead);
			m_hFileToRead = INVALID_HANDLE_VALUE;
		}
	}
}
