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

#include "vncviewer.h"
#include "VNCviewerApp32.h"
#include "FileTransfer.h"
#include "FileTransferItemInfo.h"
#include "Win32sApi.h"

#include <commctrl.h>

// ==========================================================================
// WIN32S NOTE
//
// The LoadImageA declaration + "#define LoadImage LoadImageA" that used to be
// at the bottom of this block has been removed.  LoadImage is a Win95+ USER32
// export; declaring it here put the name in the EXE's import table and stopped
// the program loading on Win32s.  It is now Win32sLoadImageIcon() from
// Win32sApi.h, which resolves it at run time and falls back to LoadIcon.
//
// The ListView_*/TreeView_* structure aliases below are genuine MSVC 4.1 SDK
// naming differences and are correct as they stand - they expand to SendMessage
// calls, not to new imports.
//
// Note however that this dialog also uses SysListView32, SysTreeView32 and
// msctls_progress32 directly.  Those require a COMCTL32 that Win32s does not
// ship, so on Win32s the file transfer dialog is expected not to work; the
// viewer must not offer it.  See CreateFileTransferDialog().
// ==========================================================================

// ============================================================================
// 1. LISTVIEW EXTENDED STYLES & MESSAGES (IE 3+)
// ============================================================================
#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif

#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT         0x00000020
#endif

// MSVC 4.1 Macro implementation for ListView_SetExtendedListViewStyleEx
#ifndef ListView_SetExtendedListViewStyleEx
#define ListView_SetExtendedListViewStyleEx(hwndLV, dwMask, dw) \
        (DWORD)SNDMSG((hwndLV), LVM_SETEXTENDEDLISTVIEWSTYLE, (WPARAM)(dwMask), (LPARAM)(dw))
#endif

// Graphical-button styles.  The MSVC 4.1 SDK headers used here do not define
// BS_ICON/BS_BITMAP (the .rc file carries its own copies for the same
// reason), so provide them for the Win32s text-caption fallback below.
#ifndef BS_PUSHBUTTON
#define BS_PUSHBUTTON 0x00000000L
#endif
#ifndef BS_ICON
#define BS_ICON 0x00000040L
#endif
#ifndef BS_BITMAP
#define BS_BITMAP 0x00000080L
#endif


// ============================================================================
// 2. ITEM ACTIVATION NOTIFICATIONS & STRUCTS (IE 4+)
// ============================================================================
#ifndef LVN_ITEMACTIVATE
#define LVN_ITEMACTIVATE             (LVN_FIRST - 14)
#endif

// If NMITEMACTIVATE is completely missing, reconstruct the Win32 layout
#ifndef LPNMITEMACTIVATE
typedef struct tagNMITEMACTIVATE {
    NMHDR  hdr;
    int    iItem;
    int    iSubItem;
    UINT   uNewState;
    UINT   uOldState;
    UINT   uChanged;
    POINT  ptAction;
    LPARAM lParam;
    UINT   uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;
#endif


// ============================================================================
// 3. TREEVIEW STRUCTURE DISAMBIGUATION
// ============================================================================
// NOTE: MSVC 4.1 DOES have TV_INSERTSTRUCT, but later SDKs renamed it to TVINSERTSTRUCT.
// We map the missing modern name directly onto the legacy name structure.
#ifndef TVINSERTSTRUCT
#define TVINSERTSTRUCT TV_INSERTSTRUCT
#endif
#ifndef LPTVINSERTSTRUCT
#define LPTVINSERTSTRUCT LPTV_INSERTSTRUCT
#endif

// --- ListView column structure ---
//
// REMOVED: a hand-written "LVCOLUMN" with an extra iOrder field, plus a
// definition of LVCF_ORDER.  Both belong to COMCTL32 4.70 (IE3) and neither is
// understood by the COMCTL32 that Win32s ships, so passing them made every
// ListView_InsertColumn call fail - see FTInsertColumn below.  The SDK's own
// LV_COLUMN is used instead.
//
// Note also that "#ifndef LVCOLUMN" could never have guarded anything: LVCOLUMN
// is a typedef, not a macro, so the preprocessor cannot see it.  On an SDK that
// does define LVCOLUMN this would have been a redefinition error rather than a
// no-op.

#ifdef __cplusplus
extern "C" {
#endif
// (LoadImageA declaration removed - see the note at the top of this file.)
#ifdef __cplusplus
}
#endif



const char FileTransfer::uploadText[] = ">>>";
const char FileTransfer::downloadText[] = "<<<";
const char FileTransfer::noactionText[] = "<--->";

FileTransfer::FileTransfer(ClientConnection * pCC, VNCviewerApp * pApp)
{
	m_clientconn = pCC;
	m_pApp = pApp;
    m_bUploadStarted = FALSE;
    m_bDownloadStarted = FALSE;
	m_bTransferEnable = FALSE;
	m_bReportUploadCancel = FALSE;
	m_bServerBrowseRequest = FALSE;
	m_bFirstFileDownloadMsg = TRUE;
	m_bBrowseReplyPending = FALSE;
	m_bInBrowseExpand = FALSE;
	m_bFTCOPY = FALSE;
	m_hTreeItem = NULL;
	m_ClientPath[0] = '\0';
	m_ClientPathTmp[0] = '\0';
	m_ServerPath[0] = '\0';
	m_ServerPathTmp[0] = '\0';
	m_ServerFilename[0] = '\0';
	m_ClientFilename[0] = '\0';
	m_UploadFilename[0] = '\0';
	m_DownloadFilename[0] = '\0';
	m_numOfFilesToDownload = -1;
	m_currentDownloadIndex = -1;

	// These window handles were left uninitialised.  Every ClientConnection
	// constructs a FileTransfer immediately (ClientConnection::Init), long
	// before any dialog exists, and several code paths - notably
	// CloseUndoneFileTransfers(), which PumpIdle() calls on a dropped
	// connection - touch them.  With garbage handles those become calls into
	// USER32 with arbitrary values.
	m_hwndFileTransfer = NULL;
	m_hwndFTClientList = NULL;
	m_hwndFTServerList = NULL;
	m_hwndFTClientPath = NULL;
	m_hwndFTServerPath = NULL;
	m_hwndFTProgress   = NULL;
	m_hwndFTStatus     = NULL;
	m_hwndFTBrowse     = NULL;
	m_hFiletoWrite     = INVALID_HANDLE_VALUE;
	m_hFiletoRead      = INVALID_HANDLE_VALUE;
	m_FTInstance       = (pApp != NULL) ? pApp->m_instance : NULL;
	m_dwDownloadRead      = 0;
	m_dwDownloadBlockSize = 0;
	m_sizeDownloadFile    = 0;
}

FileTransfer::~FileTransfer()
{
	m_FTClientItemInfo.Free();
	m_FTServerItemInfo.Free();
}

void
FileTransfer::CreateFileTransferDialog()
{
	// Win32s has no COMCTL32 with list-view/tree-view/progress support, so
	// this dialog cannot be built there.  Fail loudly instead of creating a
	// dialog whose controls do not exist (which yields NULL child HWNDs and
	// then a fault on the first ListView_ message).
	if (!Win32sHaveCommonControls()) {
		MessageBox(NULL,
			"File transfers require the common controls library,\n"
			"which is not available on this system.",
			"TightVNC Viewer", MB_OK | MB_ICONINFORMATION);
		m_clientconn->m_fileTransferDialogShown = false;
		return;
	}

	m_hwndFileTransfer = CreateDialog(m_pApp->m_instance, 
									  MAKEINTRESOURCE(IDD_FILETRANSFER_DLG),
									  NULL, 
									  (DLGPROC) FileTransferDlgProc); 
	if (m_hwndFileTransfer == NULL) {
		vnclog.Print(0, _T("Could not create file transfer dialog (%d)\n"),
					 GetLastError());
		m_clientconn->m_fileTransferDialogShown = false;
		return;
	}

	// Publish the 'this' pointer and cache the control handles BEFORE the dialog
	// becomes visible.  ShowWindow/UpdateWindow dispatch WM_PAINT and the list
	// views' first LVN_GETDISPINFO notifications, and the dialog procedure needs
	// both the object pointer and the handles by then.  The original order
	// (ShowWindow, then SetWindowLong, then GetDlgItem) meant those early
	// notifications were dropped - and the very first ones are what populate
	// the visible rows.
	SetWindowLong(m_hwndFileTransfer, GWL_USERDATA, (LONG) this);

	m_hwndFTProgress = GetDlgItem(m_hwndFileTransfer, IDC_FTPROGRESS);
	m_hwndFTClientList = GetDlgItem(m_hwndFileTransfer, IDC_FTCLIENTLIST);
	m_hwndFTServerList = GetDlgItem(m_hwndFileTransfer, IDC_FTSERVERLIST);
	m_hwndFTClientPath = GetDlgItem(m_hwndFileTransfer, IDC_CLIENTPATH);
	m_hwndFTServerPath = GetDlgItem(m_hwndFileTransfer, IDC_SERVERPATH);
	m_hwndFTStatus = GetDlgItem(m_hwndFileTransfer, IDC_FTSTATUS);

	// If the list views do not exist, COMCTL32 could not create them (no
	// SysListView32 window class).  Everything below assumes they are there.
	if (m_hwndFTClientList == NULL || m_hwndFTServerList == NULL) {
		vnclog.Print(0, _T("File transfer list views could not be created\n"));
		MessageBox(m_hwndFileTransfer,
			"The file transfer window needs the list view control,\n"
			"which is not available on this system.",
			"TightVNC Viewer", MB_OK | MB_ICONINFORMATION);
		DestroyWindow(m_hwndFileTransfer);
		m_hwndFileTransfer = NULL;
		m_clientconn->m_fileTransferDialogShown = false;
		return;
	}

#ifndef _WIN32_WCE
	VNCviewerApp32 *pApp = (VNCviewerApp32 *)(m_clientconn->m_pApp);
	pApp->AddModelessDialog(m_hwndFileTransfer);
#endif

	// Full-row selection is a COMCTL32 4.70 (IE3) extended style.  Sending
	// LVM_SETEXTENDEDLISTVIEWSTYLE to a 4.00 control is ignored rather than
	// fatal, so this is safe everywhere - it simply has no effect on Win32s.
	ListView_SetExtendedListViewStyleEx(m_hwndFTClientList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
	ListView_SetExtendedListViewStyleEx(m_hwndFTServerList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	// Button icons.
	//
	// TWO BUGS FIXED HERE:
	//
	// 1. DestroyIcon() must NOT be called on these handles.  Win32sLoadImageIcon
	//    falls back to LoadIcon() when LoadImage is unavailable - which is always
	//    the case on Win32s - and LoadIcon returns a SHARED resource handle that
	//    the caller does not own.  Destroying it frees an icon the module still
	//    references, and on Win32s the handle is immediately reused.  Worse, the
	//    icon was destroyed while two buttons were still displaying it.
	//    (Even with the real LoadImage, LR_SHARED handles must not be destroyed.)
	//
	// 2. BM_SETIMAGE only works on a button created with BS_ICON or BS_BITMAP.
	//    These four buttons are plain PUSHBUTTONs in the resource, so on
	//    COMCTL32 4.00 the message is ignored and the buttons show their caption
	//    text instead.  That is cosmetic and is left alone - but it is why the
	//    icons never appear under Win32s.
	HANDLE hIcon = Win32sLoadImageIcon(m_pApp->m_instance, MAKEINTRESOURCE(IDI_FILEUP), 16, 16);
	if (hIcon != NULL) {
		SendMessage(GetDlgItem(m_hwndFileTransfer, IDC_CLIENTUP), BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hIcon);
		SendMessage(GetDlgItem(m_hwndFileTransfer, IDC_SERVERUP), BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hIcon);
	}
	hIcon = Win32sLoadImageIcon(m_pApp->m_instance, MAKEINTRESOURCE(IDI_FILERELOAD), 16, 16);
	if (hIcon != NULL) {
		SendMessage(GetDlgItem(m_hwndFileTransfer, IDC_CLIENTRELOAD), BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hIcon);
		SendMessage(GetDlgItem(m_hwndFileTransfer, IDC_SERVERRELOAD), BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) hIcon);
	}

	// WIN32S: the icon fallback above yields native-size (32x32) icons that do
	// not fit these 14x12 BS_ICON buttons, so the buttons stay blank (but
	// clickable).  LoadImage with sizing is a Win95+ export.  On Win32s give
	// the buttons text captions instead: strip BS_ICON/BS_BITMAP back to a
	// plain pushbutton and label them.  Other platforms keep the icons.
	if (Win32sIsWin32s()) {
		struct { int id; const char *text; } fallbackButtons[] = {
			{ IDC_CLIENTUP, "Up" }, { IDC_SERVERUP, "Up" },
			{ IDC_CLIENTRELOAD, "Ref" }, { IDC_SERVERRELOAD, "Ref" }
		};
		for (int bi = 0; bi < 4; bi++) {
			HWND hBtn = GetDlgItem(m_hwndFileTransfer, fallbackButtons[bi].id);
			if (hBtn != NULL) {
				LONG style = GetWindowLong(hBtn, GWL_STYLE);
				style &= ~(BS_ICON | BS_BITMAP);
				style |= BS_PUSHBUTTON;
				SetWindowLong(hBtn, GWL_STYLE, style);
				SetWindowText(hBtn, fallbackButtons[bi].text);
			}
		}
	}

	// Column widths are a proportion of the list view's client width.  Guard
	// against a zero/absurd client rect: if GetClientRect fails or the control
	// has not been laid out yet, 0.7*0 == 0 and the columns exist but are
	// invisible - indistinguishable from the "empty list" symptom this whole
	// function was producing.  Use integer arithmetic while we are here; the
	// float constants pulled the floating-point library into the startup path,
	// which on Win32s means the emulator on a machine without a 387.
	RECT Rect;
	memset(&Rect, 0, sizeof(Rect));
	GetClientRect(m_hwndFTClientList, &Rect);
	int listWidth = Rect.right - Rect.left;
	if (listWidth < 80) {
		// Fall back to the dialog template's own width for this control.
		RECT wr;
		if (GetWindowRect(m_hwndFTClientList, &wr))
			listWidth = wr.right - wr.left;
		if (listWidth < 80)
			listWidth = 200;
	}
	int xwidth  = (listWidth * 70) / 100;
	int xwidth_ = (listWidth * 25) / 100;

	FTInsertColumn(m_hwndFTClientList, "Name", 0, xwidth);
	FTInsertColumn(m_hwndFTClientList, "Size", 1, xwidth_);
	FTInsertColumn(m_hwndFTServerList, "Name", 0, xwidth);
	FTInsertColumn(m_hwndFTServerList, "Size", 1, xwidth_);

	// Now that the columns exist and the object is reachable, make it visible.
	ShowWindow(m_hwndFileTransfer, SW_SHOW);
	UpdateWindow(m_hwndFileTransfer);

	ShowClientItems(m_ClientPathTmp);
	SendFileListRequestMessage(m_ServerPathTmp, 0);
}

LRESULT CALLBACK 
FileTransfer::FileTransferDlgProc(HWND hwnd, 
								  UINT uMsg, 
								  WPARAM wParam, 
								  LPARAM lParam)
{
	FileTransfer *_this = (FileTransfer *) GetWindowLong(hwnd, GWL_USERDATA);
	int i;

	// _this is NULL for every message that arrives before
	// CreateFileTransferDialog gets to its SetWindowLong(GWL_USERDATA) call -
	// which includes WM_INITDIALOG and the list views' first notifications.
	// WM_INITDIALOG does not need it; everything else does.
	if (_this == NULL && uMsg != WM_INITDIALOG)
		return 0;

	switch (uMsg)
	{

	case WM_INITDIALOG:
		{
			Win32sSetForegroundWindow(hwnd);
			CentreWindow(hwnd);
			return TRUE;
		}
	break;
	case WM_HELP:	
		help.Popup(lParam);
		return 0;
	case WM_COMMAND:
		{
		switch (LOWORD(wParam))
		{
			case IDC_CLIENTPATH:
				switch (HIWORD (wParam))
				{
					case EN_SETFOCUS:
						SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
						EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
						return TRUE;
				}
			break;
			case IDC_SERVERPATH:
				switch (HIWORD (wParam))
				{
					case EN_SETFOCUS:
						SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
						EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
						return TRUE;
				}
			break;
			case IDC_EXIT:
			case IDCANCEL:
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				return TRUE;
			case IDC_CLIENTUP:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				if (strcmp(_this->m_ClientPathTmp, "") == 0) return TRUE;
				for (i=(strlen(_this->m_ClientPathTmp)-2); i>=0; i--) {
					if (_this->m_ClientPathTmp[i] == '\\') {
						_this->m_ClientPathTmp[i] = '\0';
						break;
					}
					if (i == 0) _this->m_ClientPathTmp[0] = '\0';
				}
				_this->ShowClientItems(_this->m_ClientPathTmp);
				return TRUE;
			case IDC_SERVERUP:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				if (strcmp(_this->m_ServerPathTmp, "") == 0) return TRUE;
				for (i=(strlen(_this->m_ServerPathTmp)-2); i>=0; i--) {
					if (_this->m_ServerPathTmp[i] == '\\') {
						_this->m_ServerPathTmp[i] = '\0';
						break;
					}
					if (i == 0) _this->m_ServerPathTmp[0] = '\0';
				}
				_this->SendFileListRequestMessage(_this->m_ServerPathTmp, 0);
				return TRUE;
			case IDC_CLIENTRELOAD:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				_this->ShowClientItems(_this->m_ClientPath);
				return TRUE;
			case IDC_SERVERRELOAD:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				_this->SendFileListRequestMessage(_this->m_ServerPathTmp, 0);
				return TRUE;
			case IDC_FTCOPY:
				// First, check if the action is supported by the server.
				if (_this->m_bFTCOPY == FALSE) {
					// Upload was requested.
					if ( !_this->m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileUploadRequest) ||
						 !_this->m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileUploadData) ) {
						MessageBox(hwnd, "Sorry but the server does not support uploading files.",
								   "Error", MB_OK | MB_ICONEXCLAMATION);
						char buf[MAX_PATH];
						sprintf(buf, "File upload not supported by server");
						SetWindowText(_this->m_hwndFTStatus, buf);
						return TRUE;
					}
				} else {
					// Download was requested.
					if ( !_this->m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileDownloadRequest) ||
						 !_this->m_clientconn->m_serverMsgCaps.IsEnabled(rfbFileDownloadData) ) {
						MessageBox(hwnd, "Sorry but the server does not support downloading files.",
								   "Error", MB_OK | MB_ICONEXCLAMATION);
						char buf[MAX_PATH];
						sprintf(buf, "File download not supported by server");
						SetWindowText(_this->m_hwndFTStatus, buf);
						return TRUE;
					}
				}
				// Now, try to upload/download.
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				if (_this->m_ClientPath[0] == '\0' || _this->m_ServerPath[0] == '\0') {
					SetWindowText(_this->m_hwndFTStatus, "Cannot transfer files: illegal directory.");
					return TRUE;
				}
				if (_this->m_bFTCOPY == FALSE) {
					_this->m_bTransferEnable = TRUE;
					_this->m_bReportUploadCancel = TRUE;
					EnableWindow(GetDlgItem(hwnd, IDC_FTCANCEL), TRUE);
					_this->FileTransferUpload();			
				} else {
					return _this->SendMultipleFileDownloadRequests();
				}
				return TRUE;
			case IDC_FTCANCEL:
				// Check if we allowed to interrupt the transfer.
				if ( _this->m_bUploadStarted &&
					 !_this->m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileUploadFailed) ) {
					char buf[MAX_PATH];
					sprintf(buf, "Sorry, but interrupting upload is not supported by the server");
					SetWindowText(_this->m_hwndFTStatus, buf);
					return TRUE;
				}
				if ( _this->m_bDownloadStarted &&
					 !_this->m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileDownloadCancel) ) {
					char buf[MAX_PATH];
					sprintf(buf, "Sorry, but interrupting download is not supported by the server");
					SetWindowText(_this->m_hwndFTStatus, buf);
					return TRUE;
				}
				// Now try to cancel the operation.
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				_this->m_bTransferEnable = FALSE;
				EnableWindow(GetDlgItem(hwnd, IDC_FTCANCEL), FALSE);
				return TRUE;
			case IDC_CLIENTBROWSE_BUT:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				_this->CreateFTBrowseDialog(FALSE);
				return TRUE;
			case IDC_SERVERBROWSE_BUT:
				SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), noactionText);
				EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), FALSE);
				SendMessage(_this->m_hwndFTProgress, PBM_SETPOS, 0, 0);
				SetWindowText(_this->m_hwndFTStatus, "");
				_this->CreateFTBrowseDialog(TRUE);
				return TRUE;
		}
		}
	break;

	case WM_NOTIFY:
		{
		// ==================================================================
		// WIN32S NOTES ON THIS HANDLER
		//
		// 1. Dispatch on pnmh->idFrom, not LOWORD(wParam).  They agree on
		//    Win32, but reading the NMHDR is the documented way and it lets us
		//    NULL-check lParam first.
		//
		// 2. LVN_ITEMACTIVATE is COMCTL32 4.70 (IE3).  The COMCTL32 under
		//    Win32s is 4.00 and never sends it, so NM_DBLCLK is the only
		//    double-click notification available - that is the case that has to
		//    work here.
		//
		// 3. The old NM_DBLCLK handler used LVNI_FOCUSED to find the clicked
		//    row.  That is unreliable: on COMCTL32 4.00 a double-click on an
		//    item that is selected but not focused (which is the usual state
		//    right after the list is filled) returns -1, and the handler then
		//    did nothing at all.  Ask the control which item is under the
		//    cursor instead, and fall back to the selected item.
		//
		// 4. _this can be NULL: WM_NOTIFY from the list views arrives during
		//    CreateDialog, before SetWindowLong(GWL_USERDATA) has run in
		//    CreateFileTransferDialog.  The old code dereferenced it, and
		//    LVN_GETDISPINFO in particular fires immediately.
		// ==================================================================
		LPNMHDR pnmh = (LPNMHDR) lParam;
		if (pnmh == NULL || _this == NULL)
			return 0;

		switch (pnmh->idFrom)
		{
		case IDC_FTCLIENTLIST:
			switch (pnmh->code)
			{
				case NM_SETFOCUS:
					SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), uploadText);
					EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), TRUE);
					_this->m_bFTCOPY = FALSE;
					return TRUE;
				case LVN_GETDISPINFO:
					_this->OnGetDispClientInfo((NMLVDISPINFO *) lParam); 
					return TRUE;
				case NM_DBLCLK:
					{
						int iItem = _this->FTGetClickedItem(_this->m_hwndFTClientList);
						if (iItem != -1) {
							_this->ProcessListViewDBLCLK(_this->m_hwndFTClientList, _this->m_ClientPath, _this->m_ClientPathTmp, iItem);
						}
					}
					return TRUE;
#ifdef LVN_ITEMACTIVATE
				case LVN_ITEMACTIVATE:
					{
						// Only ever delivered by COMCTL32 4.70+ (Win9x/NT4 with
						// IE3 or later), never under Win32s.
						LPNMITEMACTIVATE lpnmia = (LPNMITEMACTIVATE)lParam;
						_this->ProcessListViewDBLCLK(_this->m_hwndFTClientList, _this->m_ClientPath, _this->m_ClientPathTmp, lpnmia->iItem);
					}
					return TRUE;
#endif
			}
		break;
		case IDC_FTSERVERLIST:
			switch (pnmh->code)
			{
				case NM_SETFOCUS:
					SetWindowText(GetDlgItem(hwnd, IDC_FTCOPY), downloadText);
					EnableWindow(GetDlgItem(hwnd, IDC_FTCOPY), TRUE);
					_this->m_bFTCOPY = TRUE;
					return TRUE;
				case LVN_GETDISPINFO: 
					_this->OnGetDispServerInfo((NMLVDISPINFO *) lParam); 
					return TRUE;
				case NM_DBLCLK:
					{
						int iItem = _this->FTGetClickedItem(_this->m_hwndFTServerList);
						if (iItem != -1) {
							_this->ProcessListViewDBLCLK(_this->m_hwndFTServerList, _this->m_ServerPath, _this->m_ServerPathTmp, iItem);
						}
					}
					return TRUE;
#ifdef LVN_ITEMACTIVATE
				case LVN_ITEMACTIVATE:
					{
						LPNMITEMACTIVATE lpnmia = (LPNMITEMACTIVATE)lParam;
						_this->ProcessListViewDBLCLK(_this->m_hwndFTServerList, _this->m_ServerPath, _this->m_ServerPathTmp, lpnmia->iItem);
					}
					return TRUE;
#endif
			}
		break;
		}
		}
		break;
	case WM_CLOSE:
		_this->m_clientconn->m_fileTransferDialogShown = false;
		_this->m_FTClientItemInfo.Free();
		_this->m_FTServerItemInfo.Free();
#ifndef _WIN32_WCE
		{
			VNCviewerApp32 *pApp = (VNCviewerApp32 *)(_this->m_clientconn->m_pApp);
			pApp->RemoveModelessDialog(hwnd);
		}
#endif
		DestroyWindow(hwnd);
		return TRUE;
	}
	return 0;
}

// This method will be called, each time window message 'IDC_FTCOPY' is sent for 
// file download. In following cases IDC_FTCOPY will be sent for file download.
// 1. From UI, when download button is clicked
// 2. From 'FileTransferDownload' method after each file download is complete.
// This method will keep track of number of selected files for which download 
// request is not sent. With each call it will send download request for one file
// and will return. After the completion of file download 'FileTransferDownload' 
// method will do a PostMessage for 'IDC_FTCOPY', which will invoke this method 
// again and it will send another request for file download if download request for
// some of the selected files is still pending.
BOOL
FileTransfer:: SendMultipleFileDownloadRequests()
{
	// Download Request for all the selected files is sent.
	if(m_numOfFilesToDownload == 0) {

		m_numOfFilesToDownload = -1 ;
		m_currentDownloadIndex = -1;

		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
		BlockingFileTransferDialog(TRUE);

		return TRUE;
	}

	// This is the first call for currently select file list.
	// Count of slected files is not calculated yet 
	if(m_numOfFilesToDownload == -1) {

		m_numOfFilesToDownload = ListView_GetSelectedCount(m_hwndFTServerList);
		if (m_numOfFilesToDownload <= 0) {

			SetWindowText(m_hwndFTStatus, "No file is selected, nothing to download.");

			m_numOfFilesToDownload  = -1;
			m_currentDownloadIndex = -1;

			EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
			BlockingFileTransferDialog(TRUE);

			return TRUE;
		}
		else { 
			// file transfer will start for all the selected files now. set m_bTransferEnable to true
			// Enable cancel button and disable rest of the UI components.
			m_bTransferEnable = TRUE;
			BlockingFileTransferDialog(FALSE);
			EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), TRUE);
		}
	}

	// Calculate the next selected index for which file download request has to be sent.
	int index = -1;
	index = ListView_GetNextItem(m_hwndFTServerList, m_currentDownloadIndex, LVNI_SELECTED);
	if (index < 0) {
		SetWindowText(m_hwndFTStatus, "No file is selected, nothing to download.");

		m_numOfFilesToDownload  = -1;
		m_currentDownloadIndex = -1;

		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
		BlockingFileTransferDialog(TRUE);

		return TRUE;
	}

	// If Cancel button is clicked, dont send the file download request.
	if(m_bTransferEnable == FALSE) {
		m_numOfFilesToDownload  = -1;
		m_currentDownloadIndex = -1;

		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
		BlockingFileTransferDialog(TRUE);

		return TRUE;
	}

	// Update member variables for next call.
	m_currentDownloadIndex = index;
	m_numOfFilesToDownload -= 1;

	return SendFileDownloadRequest();
}

BOOL
FileTransfer:: SendFileDownloadRequest()
{
	char path[rfbMAX_PATH + rfbMAX_PATH];
	if (!m_FTServerItemInfo.IsFile(m_currentDownloadIndex)) {
		SetWindowText(m_hwndFTStatus, "Cannot download: not a regular file.");
		// Send message to start download for next selected file.
		PostMessage(m_hwndFileTransfer, WM_COMMAND, IDC_FTCOPY, 0);
		return TRUE;
	}
	// WIN32S FIX - this is why downloads did nothing.
	//
	// Was ListView_GetItemText(), which for an LPSTR_TEXTCALLBACK item asks the
	// control to ask us back via LVN_GETDISPINFO.  On COMCTL32 4.00 that round
	// trip does not fill the caller's buffer, so m_ServerFilename came back
	// EMPTY and the request below was built for "<path>\" - a file that does
	// not exist.  Same root cause as the double-click problem; read from our own
	// item list instead.  (See ProcessListViewDBLCLK.)
	{
		char *pName = m_FTServerItemInfo.GetNameAt(m_currentDownloadIndex);
		if (pName == NULL || pName[0] == '\0') {
			SetWindowText(m_hwndFTStatus, "Cannot download: no file name.");
			PostMessage(m_hwndFileTransfer, WM_COMMAND, IDC_FTCOPY, 0);
			return TRUE;
		}
		strncpy(m_ServerFilename, pName, rfbMAX_PATH - 1);
		m_ServerFilename[rfbMAX_PATH - 1] = '\0';
	}
	strcpy(m_ClientFilename, m_ServerFilename);
	char buffer[rfbMAX_PATH + rfbMAX_PATH + rfbMAX_PATH];
	sprintf(buffer, "Downloading: %s\\%s -> %s\\%s ...",
			m_ServerPath, m_ServerFilename,
			m_ClientPath, m_ClientFilename);
	SetWindowText(m_hwndFTStatus, buffer);
	m_sizeDownloadFile = m_FTServerItemInfo.GetIntSizeAt(m_currentDownloadIndex);
	rfbFileDownloadRequestMsg fdr;
	fdr.type = rfbFileDownloadRequest;
	fdr.compressedLevel = 0;
	fdr.position = Swap32IfLE(0);
	sprintf(path, "%s\\%s", m_ServerPath, m_ServerFilename);
	ConvertPath(path);
	int len = strlen(path);
	fdr.fNameSize = Swap16IfLE(len);
	m_clientconn->WriteExact((char *)&fdr, sz_rfbFileDownloadRequestMsg);
	m_clientconn->WriteExact(path, len);
	return TRUE;
}

void 
FileTransfer::OnGetDispClientInfo(NMLVDISPINFO *plvdi) 
{
  switch (plvdi->item.iSubItem)
    {
    case 0:
		plvdi->item.pszText = m_FTClientItemInfo.GetNameAt(plvdi->item.iItem);
      break;
    case 1:
		plvdi->item.pszText = m_FTClientItemInfo.GetSizeAt(plvdi->item.iItem);
      break;
    default:
      break;
    }
 } 

void 
FileTransfer::OnGetDispServerInfo(NMLVDISPINFO *plvdi) 
{
  switch (plvdi->item.iSubItem)
  {
    case 0:
		plvdi->item.pszText = m_FTServerItemInfo.GetNameAt(plvdi->item.iItem);
      break;
    case 1:
		plvdi->item.pszText = m_FTServerItemInfo.GetSizeAt(plvdi->item.iItem);
      break;
    default:
      break;
    }
 } 

void 
FileTransfer::FileTransferUpload()
{
	int numOfFilesToUpload = 0, currentUploadIndex = -1;
	DWORD sz_rfbFileSize;
	DWORD sz_rfbBlockSize= 8192;
	DWORD dwNumberOfBytesRead = 0;
	unsigned int mTime = 0;
	char path[rfbMAX_PATH + rfbMAX_PATH + 2];
	BOOL bResult;
	numOfFilesToUpload = ListView_GetSelectedCount(m_hwndFTClientList);
	// Was "< 0": GetSelectedCount returns 0 when nothing is selected, so the
	// "nothing to upload" message never appeared - the loop just did not run and
	// the dialog sat there looking as though the button had done nothing.
	if (numOfFilesToUpload <= 0) {
		SetWindowText(m_hwndFTStatus, "No file selected, nothing to upload.");
		BlockingFileTransferDialog(TRUE);
		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
		return;
	}

	for (int i = 0; i < numOfFilesToUpload; i++) {
		BlockingFileTransferDialog(FALSE);
		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), TRUE);
		int index = ListView_GetNextItem(m_hwndFTClientList, currentUploadIndex, LVNI_SELECTED);
		if (index < 0) {
			SetWindowText(m_hwndFTStatus, "No file is selected, nothing to download.");
			return;
		}
		currentUploadIndex = index;

		// WIN32S FIX - this is why uploads did nothing.  See the identical note
		// in SendFileDownloadRequest(): ListView_GetItemText does not work for
		// LPSTR_TEXTCALLBACK items on COMCTL32 4.00, so m_ClientFilename came
		// back empty.
		{
			char *pName = m_FTClientItemInfo.GetNameAt(currentUploadIndex);
			if (pName == NULL || pName[0] == '\0') {
				SetWindowText(m_hwndFTStatus, "Cannot upload: no file name.");
				continue;
			}
			strncpy(m_ClientFilename, pName, rfbMAX_PATH - 1);
			m_ClientFilename[rfbMAX_PATH - 1] = '\0';
		}

		// Skip directories before touching the filesystem: the list holds both.
		if (!m_FTClientItemInfo.IsFile(currentUploadIndex)) {
			SetWindowText(m_hwndFTStatus, "Cannot upload a directory");
			continue;
		}

		// Bounded path build.  m_ClientPath is a drive root ("C:") at the top
		// level, in which case no separator is wanted.
		if (strlen(m_ClientPath) >= 2 &&
			m_ClientPath[strlen(m_ClientPath) - 1] != '\\') {
			sprintf(path, "%.*s\\%.*s", rfbMAX_PATH - 1, m_ClientPath,
					rfbMAX_PATH - 1, m_ClientFilename);
		} else {
			sprintf(path, "%.*s%.*s", rfbMAX_PATH - 1, m_ClientPath,
					rfbMAX_PATH - 1, m_ClientFilename);
		}
		strcpy(m_UploadFilename, path);
		WIN32_FIND_DATA FindFileData;
		UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
		HANDLE hFile = FindFirstFile(path, &FindFileData);
		SetErrorMode(savedErrorMode);
		if (hFile == INVALID_HANDLE_VALUE) {
			SetWindowText(m_hwndFTStatus, "Could not find selected file, can't upload");
			// Continue with upload of other files.
			continue;
		} else if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			SetWindowText(m_hwndFTStatus, "Cannot upload a directory");
			// Continue with upload of other files.
			continue;
		} else {
			sz_rfbFileSize = FindFileData.nFileSizeLow;
			mTime = FiletimeToTime70(FindFileData.ftLastWriteTime);
			strcpy(m_ServerFilename, FindFileData.cFileName);
		}
		FindClose(hFile);
		if ((sz_rfbFileSize != 0) && (sz_rfbFileSize <= sz_rfbBlockSize)) sz_rfbBlockSize = sz_rfbFileSize;
		m_hFiletoRead = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (m_hFiletoRead == INVALID_HANDLE_VALUE) {
			SetWindowText(m_hwndFTStatus, "Upload failed: could not open selected file");
			BlockingFileTransferDialog(TRUE);
			EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
			return;
		}
		char buffer[rfbMAX_PATH + rfbMAX_PATH + rfbMAX_PATH + rfbMAX_PATH + 20];
		sprintf(buffer, "Uploading: %s\\%s -> %s\\%s ...",
				m_ClientPath, m_ClientFilename, m_ServerPath, m_ServerFilename);
		SetWindowText(m_hwndFTStatus, buffer);
		sprintf(path, "%s\\%s", m_ServerPath, m_ClientFilename);
		ConvertPath(path);
		int pathLen = strlen(path);

		char *pAllFURMessage = new char[sz_rfbFileUploadRequestMsg + pathLen];
		rfbFileUploadRequestMsg *pFUR = (rfbFileUploadRequestMsg *) pAllFURMessage;
		char *pFollowMsg = &pAllFURMessage[sz_rfbFileUploadRequestMsg];
		pFUR->type = rfbFileUploadRequest;
		pFUR->compressedLevel = 0;
		pFUR->fNameSize = Swap16IfLE(pathLen);
		pFUR->position = Swap32IfLE(0);
		memcpy(pFollowMsg, path, pathLen);
		m_clientconn->WriteExact(pAllFURMessage, sz_rfbFileUploadRequestMsg + pathLen); 
		delete [] pAllFURMessage;

		if (sz_rfbFileSize == 0) {
			SendFileUploadDataMessage(mTime);
		} else {
			int amount = sz_rfbFileSize / (sz_rfbBlockSize * 10);

			InitProgressBar(0, 0, amount, 1);

			DWORD dwPortionRead = 0;
			char *pBuff = new char [sz_rfbBlockSize];
			m_bUploadStarted = TRUE;
			while(m_bUploadStarted) {
				ProcessDlgMessage(m_hwndFileTransfer);
				if (m_bTransferEnable == FALSE) {
					SetWindowText(m_hwndFTStatus, "File transfer canceled");
					EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
					BlockingFileTransferDialog(TRUE);
					char reason[] = "File transfer canceled by user";
					int reasonLen = strlen(reason);
					char *pFUFMessage = new char[sz_rfbFileUploadFailedMsg + reasonLen];
					rfbFileUploadFailedMsg *pFUF = (rfbFileUploadFailedMsg *) pFUFMessage;
					char *pReason = &pFUFMessage[sz_rfbFileUploadFailedMsg];
					pFUF->type = rfbFileUploadFailed;
					pFUF->reasonLen = Swap16IfLE(reasonLen);
					memcpy(pReason, reason, reasonLen);
					m_clientconn->WriteExact(pFUFMessage, sz_rfbFileUploadFailedMsg + reasonLen);
					delete [] pFUFMessage;
					break;
				}
				bResult = ReadFile(m_hFiletoRead, pBuff, sz_rfbBlockSize, &dwNumberOfBytesRead, NULL);
				if (bResult && dwNumberOfBytesRead == 0) {
					/* This is the end of the file. */
					SendFileUploadDataMessage(mTime);
					break;
				}
				SendFileUploadDataMessage((unsigned short)dwNumberOfBytesRead, pBuff);
				dwPortionRead += dwNumberOfBytesRead;
				if (dwPortionRead >= (10 * sz_rfbBlockSize)) {
					dwPortionRead = 0;
					SendMessage(m_hwndFTProgress, PBM_STEPIT, 0, 0);
				}
			}
			if (m_bTransferEnable == FALSE)
				break;
			m_bUploadStarted = FALSE;
			delete [] pBuff;
		}
		SendMessage(m_hwndFTProgress, PBM_SETPOS, 0, 0);
		SetWindowText(m_hwndFTStatus, "");
		CloseHandle(m_hFiletoRead);
	}
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
	BlockingFileTransferDialog(TRUE);
	SendFileListRequestMessage(m_ServerPath, 0);
}

void
FileTransfer::CloseUndoneFileTransfers()
{
  // Called from ClientConnection::PumpIdle() when the connection drops, which
  // can happen before any transfer has ever run.  Check the handles, not just
  // the flags.
  if (m_bUploadStarted) {
    m_bUploadStarted = FALSE;
    if (m_hFiletoRead != INVALID_HANDLE_VALUE && m_hFiletoRead != NULL) {
      CloseHandle(m_hFiletoRead);
      m_hFiletoRead = INVALID_HANDLE_VALUE;
    }
  }
  if (m_bDownloadStarted) {
    m_bDownloadStarted = FALSE;
	m_bFirstFileDownloadMsg = TRUE;
	m_currentDownloadIndex = -1;
	m_numOfFilesToDownload = -1;
    if (m_hFiletoWrite != INVALID_HANDLE_VALUE && m_hFiletoWrite != NULL) {
      CloseHandle(m_hFiletoWrite);
      m_hFiletoWrite = INVALID_HANDLE_VALUE;
    }
    if (m_DownloadFilename[0] != '\0')
      DeleteFile(m_DownloadFilename);
  }
}

void 
FileTransfer::FileTransferDownload()
{
	rfbFileDownloadDataMsg fdd;
	m_clientconn->ReadExact((char *)&fdd, sz_rfbFileDownloadDataMsg);
	fdd.realSize = Swap16IfLE(fdd.realSize);
	fdd.compressedSize = Swap16IfLE(fdd.compressedSize);

	char path[rfbMAX_PATH + rfbMAX_PATH + 3];
	
	if (m_bFirstFileDownloadMsg) {
		m_dwDownloadBlockSize = fdd.compressedSize;

		// A local directory must have been chosen: with m_ClientPath empty the
		// old sprintf produced "\NAME", i.e. the root of whatever the current
		// drive happens to be.  Refuse instead of writing somewhere surprising.
		if (m_ClientPath[0] == '\0') {
			SetWindowText(m_hwndFTStatus,
						  "Choose a local folder before downloading.");
			CancelDownload("No local folder selected");
			return;
		}

		if (strlen(m_ClientPath) + 1 + strlen(m_ServerFilename) >= sizeof(path)) {
			CancelDownload("Local path too long");
			return;
		}
		if (m_ClientPath[strlen(m_ClientPath) - 1] == '\\')
			sprintf(path, "%s%s", m_ClientPath, m_ServerFilename);
		else
			sprintf(path, "%s\\%s", m_ClientPath, m_ServerFilename);
		strcpy(m_DownloadFilename, path);

		// FILE_FLAG_SEQUENTIAL_SCAN is a Win95/NT hint.  Win32s ignores unknown
		// flags here, so it is harmless - but note that CreateFile on Win32s
		// cannot create a file in a directory that does not exist, and returns
		// INVALID_HANDLE_VALUE rather than creating intermediate directories.
		m_hFiletoWrite = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
									NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (m_hFiletoWrite == INVALID_HANDLE_VALUE) {
			// Report immediately rather than 30 lines later: the old code carried
			// on reading the whole file from the network and only then noticed.
			vnclog.Print(0, _T("Could not create %s: %d\n"), path, GetLastError());
		}

		// Divide-by-zero guard: m_dwDownloadBlockSize can be 0 (a zero-length
		// first data message), and (0+1)*10 == 10 is fine, but m_sizeDownloadFile
		// itself may be 0 giving amount == 0, which PBM_SETRANGE rejects.
		int amount = 1;
		if (m_dwDownloadBlockSize > 0) {
			amount = (int)(m_sizeDownloadFile / ((m_dwDownloadBlockSize + 1) * 10));
			if (amount < 1)
				amount = 1;
		}
		InitProgressBar(0, 0, amount, 1);
		m_bFirstFileDownloadMsg = FALSE;
		m_bDownloadStarted = TRUE;
	}
	if ((fdd.realSize == 0) && (fdd.compressedSize == 0)) {
		unsigned int mTime;
		m_clientconn->ReadExact((char *) &mTime, sizeof(unsigned int));
		if (m_hFiletoWrite == INVALID_HANDLE_VALUE) {
			CancelDownload("Could not create file");
			MessageBox(m_hwndFileTransfer, "Download failed: could not create local file",
					   "Download Failed", MB_ICONEXCLAMATION | MB_OK);
			SendMessage(m_hwndFTProgress, PBM_SETPOS, 0, 0);
			return;
		}
		FILETIME Filetime;
		Time70ToFiletime(mTime, &Filetime);
		SetFileTime(m_hFiletoWrite, &Filetime, &Filetime, &Filetime);
		SendMessage(m_hwndFTProgress, PBM_SETPOS, 0, 0);
		SetWindowText(m_hwndFTStatus, "");
		CloseHandle(m_hFiletoWrite);
		ShowClientItems(m_ClientPath);
		m_bFirstFileDownloadMsg = TRUE;
		m_bDownloadStarted = FALSE;
		// Send message to start download for next selected file.
		PostMessage(m_hwndFileTransfer, WM_COMMAND, IDC_FTCOPY, 0);
		return;
	}
	// compressedSize is a 16-bit server-supplied value, so at most 64 KB - but
	// check the allocation, and consume the payload either way so the protocol
	// stream stays in sync.
	char * pBuff = new char [fdd.compressedSize + 1];
	DWORD dwNumberOfBytesWritten;
	if (pBuff == NULL) {
		char discard[256];
		int remaining = fdd.compressedSize;
		while (remaining > 0) {
			int chunk = (remaining > (int)sizeof(discard)) ? sizeof(discard) : remaining;
			m_clientconn->ReadExact(discard, chunk);
			remaining -= chunk;
		}
		CancelDownload("Out of memory receiving file");
		return;
	}
	m_clientconn->ReadExact(pBuff, fdd.compressedSize);
	ProcessDlgMessage(m_hwndFileTransfer);
	if (!m_bTransferEnable) {
		CancelDownload("Download cancelled by user");
		delete [] pBuff;
		return;
	}
	if (m_hFiletoWrite == INVALID_HANDLE_VALUE) {
		CancelDownload("Could not create file");
		MessageBox(m_hwndFileTransfer, "Download failed: could not create local file",
				   "Download Failed", MB_ICONEXCLAMATION | MB_OK);
		delete [] pBuff;
		return;
	}
	dwNumberOfBytesWritten = 0;
	if (!WriteFile(m_hFiletoWrite, pBuff, fdd.compressedSize,
				   &dwNumberOfBytesWritten, NULL) ||
		dwNumberOfBytesWritten != fdd.compressedSize) {
		// Disk full, or a read-only/removable volume - very much a real case on
		// these machines.  The old code ignored the result and reported success.
		vnclog.Print(0, _T("WriteFile failed (%d of %d bytes, error %d)\n"),
					 (int)dwNumberOfBytesWritten, (int)fdd.compressedSize,
					 GetLastError());
		delete [] pBuff;
		CancelDownload("Could not write to the local file (disk full?)");
		MessageBox(m_hwndFileTransfer,
				   "Download failed: could not write the local file.\r\n"
				   "The disk may be full or write-protected.",
				   "Download Failed", MB_ICONEXCLAMATION | MB_OK);
		return;
	}
	m_dwDownloadRead += dwNumberOfBytesWritten;
	if (m_dwDownloadRead >= (10 * m_dwDownloadBlockSize)) {
		m_dwDownloadRead = 0;
		SendMessage(m_hwndFTProgress, PBM_STEPIT, 0, 0); 
	}
	delete [] pBuff;
}

void
FileTransfer::CancelDownload(char *reason)
{
	if (reason == NULL)
		reason = "Download cancelled";

	// Only tell the server if the connection is still usable AND the server
	// advertised the capability.  Sending rfbFileDownloadCancel to a server that
	// does not support it desynchronises the stream, and WriteExact on a dead
	// socket during teardown is what produced the disconnect error box.
	if (m_clientconn != NULL &&
		m_clientconn->m_clientMsgCaps.IsEnabled(rfbFileDownloadCancel)) {
		SendFileDownloadCancelMessage((unsigned short)strlen(reason), reason);
	}

	SetWindowText(m_hwndFTStatus, reason);
	CloseUndoneFileTransfers();
	if (m_hwndFileTransfer != NULL)
		EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
	BlockingFileTransferDialog(TRUE);
	m_bDownloadStarted = FALSE;
	m_bFirstFileDownloadMsg = TRUE;
	m_numOfFilesToDownload = -1;
	m_currentDownloadIndex = -1;
}

void 
FileTransfer::ShowClientItems(char *path)
{
	if (m_hwndFTClientList == NULL || path == NULL)
		return;

	if (strlen(path) == 0) {
		//Show Logical Drives
		ListView_DeleteAllItems(m_hwndFTClientList);
		m_FTClientItemInfo.Free();

		// ==================================================================
		// WIN32S FIX - second reason the local pane was empty.
		//
		// GetLogicalDriveStrings() IS present in the Win32s KERNEL32, but it
		// is one of the calls that behaves differently: several Win32s builds
		// return 0 (and some return the required size rather than the copied
		// size) because the double-NUL-terminated multi-string form is not
		// filled in the way Win95 does it.  The old code treated 0 as fatal
		// and returned with an empty list and no diagnostic.
		//
		// Fall back to probing A: through Z: with GetDriveType(), which works
		// on every Win32 platform including Win32s.
		//
		// The original loop was also wrong on its own terms: it wrote
		// DriveName[0] = '\0' *after* Add() (harmless but pointless), advanced
		// i by 3 inside a loop that also increments i - so it stepped 4 bytes,
		// which is correct only for "X:\\0" entries - and then read
		// DrivesString[i+1] up to index 256, one past the buffer.
		// ==================================================================
		char DrivesString[512];
		char DriveName[8];
		int nDrives = 0;

		memset(DrivesString, 0, sizeof(DrivesString));
		DWORD LengthDrivesString =
			GetLogicalDriveStrings(sizeof(DrivesString) - 2, DrivesString);

		strcpy(m_ClientPath, m_ClientPathTmp);
		SetWindowText(m_hwndFTClientPath, m_ClientPath);

		if (LengthDrivesString > 0 &&
			LengthDrivesString < (DWORD)(sizeof(DrivesString) - 2)) {
			// Walk the double-NUL-terminated multi-string properly.
			char *p = DrivesString;
			while (*p != '\0' && (DWORD)(p - DrivesString) < LengthDrivesString) {
				DriveName[0] = p[0];
				DriveName[1] = ':';
				DriveName[2] = '\0';
				char txt[16];
				strcpy(txt, m_FTClientItemInfo.folderText);
				m_FTClientItemInfo.Add(DriveName, txt, 0);
				nDrives++;
				p += strlen(p) + 1;		// next string in the multi-string
			}
		}

		if (nDrives == 0) {
			// Win32s fallback: probe each drive letter.
			vnclog.Print(4, _T("GetLogicalDriveStrings gave nothing - probing drives\n"));
			UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS |
											   SEM_NOOPENFILEERRORBOX);
			for (char letter = 'A'; letter <= 'Z'; letter++) {
				char root[8];
				root[0] = letter;
				root[1] = ':';
				root[2] = '\\';
				root[3] = '\0';
				UINT dt = GetDriveType(root);
				// DRIVE_UNKNOWN(0) and DRIVE_NO_ROOT_DIR(1) mean "not there".
				// Everything else (removable, fixed, remote, CDROM, RAMDISK) is
				// a drive we should list.
				if (dt > 1) {
					DriveName[0] = letter;
					DriveName[1] = ':';
					DriveName[2] = '\0';
					char txt[16];
					strcpy(txt, m_FTClientItemInfo.folderText);
					m_FTClientItemInfo.Add(DriveName, txt, 0);
					nDrives++;
				}
			}
			SetErrorMode(savedErrorMode);
		}

		if (nDrives == 0) {
			vnclog.Print(0, _T("Could not enumerate any local drives\n"));
			BlockingFileTransferDialog(TRUE);
			strcpy(m_ClientPathTmp, m_ClientPath);
			return;
		}

		m_FTClientItemInfo.Sort();
		ShowListViewItems(m_hwndFTClientList, &m_FTClientItemInfo);
	} else {
		//Show Files
		HANDLE m_handle;
		WIN32_FIND_DATA m_FindFileData;

		// ==================================================================
		// The search pattern is appended to the CALLER'S buffer (path is
		// m_ClientPath or m_ClientPathTmp, both rfbMAX_PATH == 255) and the two
		// characters were removed again afterwards by indexing
		// path[strlen(path)-2].  Two problems:
		//   * no length check before strcat - a 254-character path overflowed;
		//   * on the early-return paths the "\*" was sometimes left on the end,
		//     so the stored path became "C:\FOO\*" and every later navigation
		//     from it failed.
		// Build the pattern in a local buffer and leave 'path' untouched.
		// ==================================================================
		char pattern[rfbMAX_PATH + 4];
		int pathLen = strlen(path);
		if (pathLen == 0 || pathLen > rfbMAX_PATH - 3) {
			vnclog.Print(0, _T("Local path too long to list\n"));
			strcpy(m_ClientPathTmp, m_ClientPath);
			BlockingFileTransferDialog(TRUE);
			return;
		}
		strcpy(pattern, path);
		// Do not double the separator for a bare drive root ("C:").
		if (pattern[pathLen - 1] != '\\')
			strcat(pattern, "\\");
		strcat(pattern, "*.*");		// "*.*", not "*": on a real FAT volume
									// under Windows 3.1 the two are equivalent,
									// but "*.*" is what the 16-bit FindFirst
									// layer expects.

		UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS |
										   SEM_NOOPENFILEERRORBOX);
		m_handle = FindFirstFile(pattern, &m_FindFileData);
		DWORD LastError = GetLastError();
		SetErrorMode(savedErrorMode);

		if (m_handle == INVALID_HANDLE_VALUE) {
			// NOTE: no FindClose() here.  The old code called
			// FindClose(INVALID_HANDLE_VALUE) on both of these paths, which is
			// an invalid-handle call - benign on NT, not something to do on
			// Win32s, where the find handle is a real 16-bit DOS structure.
			if (LastError != ERROR_SUCCESS &&
				LastError != ERROR_FILE_NOT_FOUND &&
				LastError != ERROR_NO_MORE_FILES) {
				vnclog.Print(0, _T("FindFirstFile(%s) failed: %d\n"),
							 pattern, (int)LastError);
				strcpy(m_ClientPathTmp, m_ClientPath);
				BlockingFileTransferDialog(TRUE);
				return;
			}
			// Empty directory: that is a success, show it as empty.
			strcpy(m_ClientPath, m_ClientPathTmp);
			SetWindowText(m_hwndFTClientPath, m_ClientPath);
			ListView_DeleteAllItems(m_hwndFTClientList);
			m_FTClientItemInfo.Free();
			BlockingFileTransferDialog(TRUE);
			return;
		}
		ListView_DeleteAllItems(m_hwndFTClientList);
		m_FTClientItemInfo.Free();
		char buffer[16];
		while(1) {
			if ((strcmp(m_FindFileData.cFileName, ".") != 0) &&
		       (strcmp(m_FindFileData.cFileName, "..") != 0)) {
				if (!(m_FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					// %lu, not %d: nFileSizeLow is a DWORD, and a file over 2 GB
					// would have printed as negative.  buffer is char[16], which
					// holds any 32-bit decimal value.
					sprintf(buffer, "%lu", m_FindFileData.nFileSizeLow);

					// The item's third field ("Data") holds a modification
					// timestamp.  Nothing in the viewer reads it - the only
					// consumer of item data is GetIntSizeAt(), which parses the
					// Size string - so its exact value does not matter.
					//
					// WIN32S: the original computed it with
					// LARGE_INTEGER::QuadPart, i.e. a 64-bit divide, and then
					// stored only li.LowPart, discarding the high word - so it
					// was already approximate.  Use FileTimeToDosDateTime,
					// which exists on Win32s (Win32s file times ARE DOS
					// date/time stamps underneath) and needs no 64-bit maths or
					// helper routines from the CRT.
					WORD dosDate = 0, dosTime = 0;
					FileTimeToDosDateTime(&m_FindFileData.ftLastWriteTime,
										  &dosDate, &dosTime);
					m_FTClientItemInfo.Add(m_FindFileData.cFileName, buffer,
										   ((unsigned int)dosDate << 16) | dosTime);
				} else {
					strcpy(buffer, m_FTClientItemInfo.folderText);
					m_FTClientItemInfo.Add(m_FindFileData.cFileName, buffer, 0);
				}
			}
			if (!FindNextFile(m_handle, &m_FindFileData)) break;
		}
		FindClose(m_handle);
		m_FTClientItemInfo.Sort();
		ShowListViewItems(m_hwndFTClientList, &m_FTClientItemInfo);
		// 'path' was never modified (the pattern went into a local buffer), so
		// there is nothing to trim here any more.
		strcpy(m_ClientPath, m_ClientPathTmp);
		SetWindowText(m_hwndFTClientPath, m_ClientPath);
	}
	BlockingFileTransferDialog(TRUE);
}

BOOL CALLBACK 
FileTransfer::FTBrowseDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	FileTransfer *_this = (FileTransfer *) GetWindowLong(hwnd, GWL_USERDATA);
	switch (uMsg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLong(hwnd, GWL_USERDATA, lParam);
			_this = (FileTransfer *) lParam;
			CentreWindow(hwnd);
			_this->m_hwndFTBrowse = hwnd;
			if (_this->m_bServerBrowseRequest) {
				// Modal dialog: the main idle loop cannot PumpIdle(), so the
				// reply would sit unread and the tree would stay empty (this
				// is why server Choose Directory showed nothing while local
				// worked).  Flag the request and pump until ShowServerItems
				// clears it.
				_this->m_hTreeItem = NULL;
				_this->m_bBrowseReplyPending = TRUE;
				_this->SendFileListRequestMessage("", 0x10);
				_this->WaitForBrowseReply(15000);
				_this->m_bBrowseReplyPending = FALSE;
				return TRUE;
			} else {
				// WIN32S: local drive enumeration for the browse tree.  The old
				// code inserted every drive twice (two InsertItem calls per
				// pass), used tvins.hParent before ever setting it, and walked
				// the drive-string buffer with a fixed i+=3 stride that lands
				// mid-entry on any layout but 4-byte "X:\"+NUL - the tree came
				// up empty or corrupt, so "Choose Directory" showed nothing.
				// Walk NUL-terminated entries instead, insert each drive once
				// under TVI_ROOT, and fall back to probing A-Z with
				// GetDriveType when GetLogicalDriveStrings returns nothing
				// (as on several Win32s builds).
				HWND hTree = GetDlgItem(hwnd, IDC_FTBROWSETREE);
				char DrivesString[512];
				char drive[8];
				memset(DrivesString, 0, sizeof(DrivesString));
				DWORD driveLen = GetLogicalDriveStrings(sizeof(DrivesString) - 2,
														DrivesString);
				int nDrives = 0;
				if (driveLen > 0 &&
					driveLen < (DWORD)(sizeof(DrivesString) - 2)) {
					char *dp = DrivesString;
					while (*dp != '\0' &&
						   (DWORD)(dp - DrivesString) < driveLen) {
						drive[0] = dp[0];
						drive[1] = ':';
						drive[2] = '\0';
						TVINSERTSTRUCT tvins;
						memset(&tvins, 0, sizeof(tvins));
						tvins.hParent = TVI_ROOT;
						tvins.hInsertAfter = TVI_LAST;
						tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
						tvins.item.pszText = drive;
						tvins.item.cChildren = 1;
						TreeView_InsertItem(hTree, &tvins);
						nDrives++;
						dp += strlen(dp) + 1;
					}
				}
				if (nDrives == 0) {
					// Win32s fallback: probe each drive letter.  Suppress the
					// "no disk in drive" system box for empty floppies.
					UINT savedMode = SetErrorMode(SEM_FAILCRITICALERRORS |
												  SEM_NOOPENFILEERRORBOX);
					for (char letter = 'A'; letter <= 'Z'; letter++) {
						char root[8];
						root[0] = letter;
						root[1] = ':';
						root[2] = '\\';
						root[3] = '\0';
						if (GetDriveType(root) > 1) {
							drive[0] = letter;
							drive[1] = ':';
							drive[2] = '\0';
							TVINSERTSTRUCT tvins;
							memset(&tvins, 0, sizeof(tvins));
							tvins.hParent = TVI_ROOT;
							tvins.hInsertAfter = TVI_LAST;
							tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
							tvins.item.pszText = drive;
							tvins.item.cChildren = 1;
							TreeView_InsertItem(hTree, &tvins);
						}
					}
					SetErrorMode(savedMode);
				}
			}
			return TRUE;
		}
	break;
	case WM_COMMAND:
		{
		switch (LOWORD(wParam))
		{
			case IDC_FTBROWSECANCEL:
				EndDialog(hwnd, TRUE);
				_this->m_bServerBrowseRequest = FALSE;
				return TRUE;
			case IDC_FTBROWSEOK:
				char path[rfbMAX_PATH];
				if (GetWindowText(GetDlgItem(hwnd, IDC_FTBROWSEEDIT), path, rfbMAX_PATH) == 0) {
					EndDialog(hwnd, TRUE);
					_this->m_bServerBrowseRequest = FALSE;
					return TRUE;
				}
				if (_this->m_bServerBrowseRequest) {
					strcpy(_this->m_ServerPathTmp, path);
					EndDialog(hwnd,TRUE);
					_this->m_bServerBrowseRequest = FALSE;
					_this->SendFileListRequestMessage(_this->m_ServerPathTmp, 0);
					return TRUE;
				} else {
					strcpy(_this->m_ClientPathTmp, path);
					EndDialog(hwnd,TRUE);
					_this->ShowClientItems(_this->m_ClientPathTmp);
				}
				return TRUE;
		}
		}
	break;
	case WM_NOTIFY:
		switch (LOWORD(wParam))
		{
		case IDC_FTBROWSETREE:
			switch (((LPNMHDR) lParam)->code)
			{
			case TVN_SELCHANGED:
				{
					NMTREEVIEW *m_lParam = (NMTREEVIEW *) lParam;
					char path[rfbMAX_PATH];
					_this->GetTVPath(GetDlgItem(hwnd, IDC_FTBROWSETREE), m_lParam->itemNew.hItem, path);
					SetWindowText(GetDlgItem(hwnd, IDC_FTBROWSEEDIT), path);
					return TRUE;
				}
				break;
			case TVN_ITEMEXPANDING:
				{
				NMTREEVIEW *m_lParam = (NMTREEVIEW *) lParam;
				char Path[rfbMAX_PATH];
				// Ignore the notification we cause ourselves while
				// populating (see ShowServerItems).  Without this the
				// sequence expand -> request -> populate -> Expand ->
				// expand ... recurses until Win32s locks up.
				if (_this->m_bInBrowseExpand)
					return TRUE;
				if (m_lParam -> action == 2) {
					if (_this->m_bServerBrowseRequest) {
						_this->m_hTreeItem = m_lParam->itemNew.hItem;
						_this->GetTVPath(GetDlgItem(hwnd, IDC_FTBROWSETREE), m_lParam->itemNew.hItem, Path);
						_this->m_bInBrowseExpand = TRUE;
						_this->m_bBrowseReplyPending = TRUE;
						_this->SendFileListRequestMessage(Path, 0x10);
						_this->WaitForBrowseReply(15000);
						_this->m_bBrowseReplyPending = FALSE;
						_this->m_bInBrowseExpand = FALSE;
						return TRUE;
					} else {
						_this->ShowTreeViewItems(hwnd, m_lParam);
					}
				}
				return TRUE;
				}
			}
			break;
		}
	break;
	case WM_CLOSE:
	case WM_DESTROY:
		EndDialog(hwnd, FALSE);
		_this->m_bServerBrowseRequest = FALSE;
		return TRUE;
	}
	return 0;
}

void 
FileTransfer::CreateFTBrowseDialog(BOOL status)
{
	m_bServerBrowseRequest = status;
	DialogBoxParam(m_pApp->m_instance, MAKEINTRESOURCE(IDD_FTBROWSE_DLG), m_hwndFileTransfer, (DLGPROC) FTBrowseDlgProc, (LONG) this);
}

void 
FileTransfer::GetTVPath(HWND hwnd, HTREEITEM hTItem, char *path)
{
	char szText[rfbMAX_PATH];
	TVITEM _tvi;
	if (hwnd == NULL || path == NULL) {
		if (path != NULL)
			path[0] = '\0';
		return;
	}
	path[0] = '\0';
	if (hTItem == NULL)
		return;
	do {
		memset(&_tvi, 0, sizeof(_tvi));
		memset(szText, 0, sizeof(szText));
		_tvi.mask = TVIF_TEXT | TVIF_HANDLE;
		_tvi.hItem = hTItem;
		_tvi.pszText = szText;
		_tvi.cchTextMax = rfbMAX_PATH - 1;
		if (!TreeView_GetItem(hwnd, &_tvi) || szText[0] == '\0')
			break;
		// Bounded: path holds rfbMAX_PATH bytes including the terminator.
		if (strlen(path) + 1 + strlen(szText) >= (unsigned)rfbMAX_PATH)
			break;
		strcat(path, "\\");
		strcat(path, szText);
		hTItem = TreeView_GetParent(hwnd, hTItem);
	}
	while(hTItem != NULL);
	char path_tmp[rfbMAX_PATH], path_out[rfbMAX_PATH];
	path_tmp[0] = '\0';
	path_out[0] = '\0';
	int len = strlen(path);
	int ii = 0;
	for (int i = (len-1); i>=0; i--) {
		if (path[i] == '\\') {
			StrInvert(path_tmp);
			strcat(path_out, path_tmp);
			strcat(path_out, "\\");
			path_tmp[0] = '\0';
			ii = 0;
		} else {
			path_tmp[ii] = path[i];
			path_tmp[ii+1] = '\0';
			ii++;
		}
	}
	if (path_out[strlen(path_out)-1] == '\\') path_out[strlen(path_out)-1] = '\0';
	strcpy(path, path_out);
}

void
FileTransfer::StrInvert(char str[rfbMAX_PATH])
{
	int len = strlen(str), i;
	char str_out[rfbMAX_PATH];
	str_out[0] = '\0';
	for (i = (len-1); i>=0; i--) str_out[len-i-1] = str[i];
	str_out[len] = '\0';
	strcpy(str, str_out);
}

void 
FileTransfer::ShowTreeViewItems(HWND hwnd, LPNMTREEVIEW m_lParam)
{
	HANDLE m_handle;
	WIN32_FIND_DATA m_FindFileData;
	TVITEM tvi;
	TVINSERTSTRUCT tvins;
	char path[rfbMAX_PATH];
	GetTVPath(GetDlgItem(hwnd, IDC_FTBROWSETREE), m_lParam->itemNew.hItem, path);
	strcat(path, "\\*");
	while (TreeView_GetChild(GetDlgItem(hwnd, IDC_FTBROWSETREE), m_lParam->itemNew.hItem) != NULL) {
		TreeView_DeleteItem(GetDlgItem(hwnd, IDC_FTBROWSETREE), TreeView_GetChild(GetDlgItem(hwnd, IDC_FTBROWSETREE), m_lParam->itemNew.hItem));
	}
	UINT savedErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	m_handle = FindFirstFile(path, &m_FindFileData);
	SetErrorMode(savedErrorMode);
	if (m_handle == INVALID_HANDLE_VALUE) return;
	while(1) {
		if ((strcmp(m_FindFileData.cFileName, ".") != 0) && 
			(strcmp(m_FindFileData.cFileName, "..") != 0)) {
			if (m_FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				TVINSERTSTRUCT tvins2;
				memset(&tvins2, 0, sizeof(tvins2));
				tvins2.hParent = m_lParam->itemNew.hItem;
				tvins2.hInsertAfter = TVI_LAST;
				tvins2.item.mask = TVIF_TEXT | TVIF_CHILDREN;
				tvins2.item.pszText = m_FindFileData.cFileName;
				tvins2.item.cChildren = 1;
				TreeView_InsertItem(GetDlgItem(hwnd, IDC_FTBROWSETREE), &tvins2);
			}
		}
		if (!FindNextFile(m_handle, &m_FindFileData)) break;
	}
	FindClose(m_handle);
}

void 
FileTransfer::ProcessDlgMessage(HWND hwnd)
{
	// Pump only this dialog's messages.
	//
	// SINGLE-THREADED NOTE: this is called from inside the upload/download
	// loops, which now run on the same thread as everything else.  It must stay
	// restricted to hwnd - passing NULL here would dispatch viewer window
	// messages (including WM_CLOSE, and WM_REGIONUPDATED which re-enters the
	// decoders) while we are part-way through a file transfer.
	if (hwnd == NULL)
		return;

	MSG msg;
	while(PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
		if (!IsDialogMessage(hwnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void 
FileTransfer::BlockingFileTransferDialog(BOOL status)
{
	// Called from ShowClientItems/ShowServerItems, which can run before the
	// dialog exists (CreateFileTransferDialog calls ShowClientItems at the end,
	// but ShowServerItems can be reached from PumpIdle for a reply that arrives
	// after the dialog has been closed).  EnableWindow(NULL, ...) is an
	// invalid-window call.
	if (m_hwndFileTransfer == NULL)
		return;

	EnableWindow(m_hwndFTClientList, status);
	EnableWindow(m_hwndFTServerList, status);
	EnableWindow(m_hwndFTClientPath, status);
	EnableWindow(m_hwndFTServerPath, status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_UPLOAD), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_DOWNLOAD), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_CLIENTUP), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_SERVERUP), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_CLIENTRELOAD), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_SERVERRELOAD), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_CLIENTBROWSE_BUT), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_SERVERBROWSE_BUT), status);
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_EXIT), status);
}

void 
FileTransfer::ShowServerItems()
{
	rfbFileListDataMsg fld;
	m_clientconn->ReadExact((char *) &fld, sz_rfbFileListDataMsg);
	if ((fld.flags & 0x80) && !m_bServerBrowseRequest) {
		BlockingFileTransferDialog(TRUE);
		return;
	}
	fld.numFiles = Swap16IfLE(fld.numFiles);
	fld.dataSize = Swap16IfLE(fld.dataSize);
	fld.compressedSize = Swap16IfLE(fld.compressedSize);

	// ======================================================================
	// This is the server side of "I can't double-click into the server
	// directories": the reply was being read, but any failure here left the
	// protocol stream mid-message and the list empty.
	//
	//  * "new FTSIZEDATA[0]" / "new char[0]" for an empty listing: legal, but
	//    then ReadExact(..., 0) and the delete[] pair were still executed.
	//  * No allocation-failure check, then ReadExact() wrote through the null
	//    pointer.  numFiles and dataSize are 16-bit server-supplied values, so
	//    the largest allocation is 64 KB - which on Win32s can genuinely fail.
	//  * The struct is read as "numFiles * 8" bytes while the array is
	//    numFiles * sizeof(FTSIZEDATA).  Those agree only because FTSIZEDATA
	//    happens to be two DWORDs; use sizeof so it stays true.
	//  * CRITICAL: if we return early without consuming both blocks, the
	//    connection desynchronises and the next message read is garbage - which
	//    is what killed the whole session, not just the file dialog.  Every exit
	//    path below now happens AFTER both reads.
	// ======================================================================
	FTSIZEDATA *pftSD = NULL;
	char *pFilenames = NULL;

	if (fld.numFiles > 0) {
		pftSD = new FTSIZEDATA[fld.numFiles];
		if (pftSD == NULL)
			vnclog.Print(0, _T("Out of memory for %d file entries\n"), (int)fld.numFiles);
	}
	if (fld.dataSize > 0) {
		pFilenames = new char[fld.dataSize + 1];
		if (pFilenames != NULL)
			pFilenames[fld.dataSize] = '\0';	// terminate the name blob
	}

	// Consume the payload unconditionally, even if an allocation failed - the
	// alternative is a desynchronised protocol stream.
	if (fld.numFiles > 0) {
		if (pftSD != NULL) {
			m_clientconn->ReadExact((char *)pftSD,
									fld.numFiles * sizeof(FTSIZEDATA));
		} else {
			char discard[256];
			int remaining = fld.numFiles * sizeof(FTSIZEDATA);
			while (remaining > 0) {
				int chunk = (remaining > (int)sizeof(discard)) ? sizeof(discard) : remaining;
				m_clientconn->ReadExact(discard, chunk);
				remaining -= chunk;
			}
		}
	}
	if (fld.dataSize > 0) {
		if (pFilenames != NULL) {
			m_clientconn->ReadExact(pFilenames, fld.dataSize);
		} else {
			char discard[256];
			int remaining = fld.dataSize;
			while (remaining > 0) {
				int chunk = (remaining > (int)sizeof(discard)) ? sizeof(discard) : remaining;
				m_clientconn->ReadExact(discard, chunk);
				remaining -= chunk;
			}
		}
	}

	if (pftSD == NULL && fld.numFiles > 0) {
		if (pFilenames != NULL) delete [] pFilenames;
		m_bBrowseReplyPending = FALSE;
		BlockingFileTransferDialog(TRUE);
		return;
	}
	if (pFilenames == NULL && fld.dataSize > 0) {
		if (pftSD != NULL) delete [] pftSD;
		m_bBrowseReplyPending = FALSE;
		BlockingFileTransferDialog(TRUE);
		return;
	}

	if (!m_bServerBrowseRequest) {
		if (fld.numFiles == 0) {
			// Empty directory - show it as empty, and still commit the path so
			// that the user can navigate out of it again.
			strcpy(m_ServerPath, m_ServerPathTmp);
			SetWindowText(m_hwndFTServerPath, m_ServerPath);
			m_FTServerItemInfo.Free();
			ListView_DeleteAllItems(m_hwndFTServerList); 
			if (pftSD != NULL) delete [] pftSD;
			if (pFilenames != NULL) delete [] pFilenames;
			BlockingFileTransferDialog(TRUE);
			return;
		} else {
			m_FTServerItemInfo.Free();
			ListView_DeleteAllItems(m_hwndFTServerList); 
			strcpy(m_ServerPath, m_ServerPathTmp);
			SetWindowText(m_hwndFTServerPath, m_ServerPath);
			CreateServerItemInfoList(&m_FTServerItemInfo, pftSD, fld.numFiles, pFilenames, fld.dataSize);
			m_FTServerItemInfo.Sort();
			ShowListViewItems(m_hwndFTServerList, &m_FTServerItemInfo);
		}
	} else {
		// Browse-tree reply: wakes WaitForBrowseReply() in the modal dialog.
		// m_hTreeItem is NULL for the initial "" listing, whose entries are
		// top-level roots - insert under TVI_ROOT in that case.
		HWND hTree = (m_hwndFTBrowse != NULL) ?
			GetDlgItem(m_hwndFTBrowse, IDC_FTBROWSETREE) : NULL;
		m_bBrowseReplyPending = FALSE;
		if (hTree != NULL) {
			HTREEITEM hParent = (m_hTreeItem != NULL) ? m_hTreeItem : TVI_ROOT;
			while (TreeView_GetChild(hTree, hParent) != NULL) {
				TreeView_DeleteItem(hTree, TreeView_GetChild(hTree, hParent));
			}
			// WIN32S: the old code inserted every directory twice (two InsertItem
			// calls per pass, the second nested under the first via hParent being
			// reassigned to the previous result) and never set hInsertAfter -
			// uninitialised stack, so items landed at an undefined position or the
			// insert failed outright and the server tree came up empty.  Insert
			// each directory once, fully initialised, under the expanded node.
			int pos = 0;
			for (int i = 0; i < fld.numFiles; i++) {
				if (pos >= fld.dataSize)
					break;
				if (pftSD[i].size == -1) {
					TVINSERTSTRUCT tvins;
					memset(&tvins, 0, sizeof(tvins));
					tvins.hParent = hParent;
					tvins.hInsertAfter = TVI_LAST;
					tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
					tvins.item.pszText = pFilenames + pos;
					tvins.item.cChildren = 1;
					TreeView_InsertItem(hTree, &tvins);
				}
				pos += strlen(pFilenames + pos) + 1;
			}
			// Do NOT call TreeView_Expand here.  We are populating in
			// response to the control's own TVN_ITEMEXPANDING for this
			// item; the control expands it itself when we return.  An
			// explicit Expand re-enters TVN_ITEMEXPANDING synchronously
			// (expand -> populate -> Expand -> ...), which flickers the
			// [+] and recurses until Windows 3.1 locks up.
		}
	}
	if (pftSD != NULL) delete [] pftSD;
	if (pFilenames != NULL) delete [] pFilenames;
	BlockingFileTransferDialog(TRUE);
}

void
FileTransfer::WaitForBrowseReply(DWORD timeoutMs)
{
	// The browse dialog is modal, so no other code can PumpIdle() while we
	// wait.  Pump the connection ourselves and keep our own dialog
	// responsive.  A bounded wait only: on timeout the tree simply stays
	// unexpanded rather than hanging the viewer.
	DWORD start = GetTickCount();
	while (m_bBrowseReplyPending) {
		if (m_clientconn != NULL)
			m_clientconn->PumpIdle();
		if (!m_bBrowseReplyPending)
			break;
		MSG msg;
		while (PeekMessage(&msg, m_hwndFTBrowse, 0, 0, PM_REMOVE)) {
			if (!IsDialogMessage(m_hwndFTBrowse, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		if ((GetTickCount() - start) > timeoutMs)
			break;
		// Yield the rest of the timeslice; Sleep(0) exists on Win32s.
		Sleep(0);
	}
}

void 
FileTransfer::SendFileListRequestMessage(char *filename, unsigned char flags)
{
	char _filename[rfbMAX_PATH];
	if (filename == NULL)
		filename = "";
	strncpy(_filename, filename, rfbMAX_PATH - 1);
	_filename[rfbMAX_PATH - 1] = '\0';
	int len = strlen(_filename);
	if (len > 0 && _filename[len-1] == '\\') _filename[len-1] = '\0';
	ConvertPath(_filename);
	len = strlen(_filename);
	rfbFileListRequestMsg flr;
	flr.type = rfbFileListRequest;
	flr.dirNameSize = Swap16IfLE(len);
	flr.flags = flags;
	m_clientconn->WriteExact((char *)&flr, sz_rfbFileListRequestMsg);
	m_clientconn->WriteExact(_filename, len);
}

void 
FileTransfer::ProcessListViewDBLCLK(HWND hwnd, char *Path, char *PathTmp, int iItem)
{
	if (hwnd == NULL || Path == NULL || PathTmp == NULL || iItem < 0)
		return;

	SendMessage(m_hwndFTProgress, PBM_SETPOS, 0, 0);
	SetWindowText(m_hwndFTStatus, "");
	strcpy(PathTmp, Path);

	// ======================================================================
	// This was the other half of "I can't double-click into the server
	// directories".
	//
	// ListView_GetItemText sends LVM_GETITEMTEXT, which for an LPSTR_TEXTCALLBACK
	// item makes the control ask US for the text via LVN_GETDISPINFO - and then
	// copies it into the supplied buffer.  That works on COMCTL32 4.70+, but on
	// 4.00 the callback round trip during a GETITEMTEXT does not reliably fill
	// the buffer, so buffer_tmp came back empty and the strcmp against
	// "<Folder>" never matched.  Nothing happened, for both panes.
	//
	// Since we are the owner of the data, read it from our own item list
	// instead of asking the control to ask us.  This is both correct and faster.
	// ======================================================================
	FileTransferItemInfo *pInfo = NULL;
	if (hwnd == m_hwndFTClientList)
		pInfo = &m_FTClientItemInfo;
	else if (hwnd == m_hwndFTServerList)
		pInfo = &m_FTServerItemInfo;
	if (pInfo == NULL)
		return;

	if (iItem >= pInfo->GetNumEntries())
		return;

	char *pName = pInfo->GetNameAt(iItem);
	char *pSize = pInfo->GetSizeAt(iItem);
	if (pName == NULL || pSize == NULL || pName[0] == '\0')
		return;

	// Only directories are navigable.
	if (strcmp(pSize, FileTransferItemInfo::folderText) != 0)
		return;

	BlockingFileTransferDialog(FALSE);

	// Build the new path with bounds checking.  The old code appended with
	// strcat() into a 255-byte buffer with no length test at all.
	//
	// The "strlen(PathTmp) >= 2" test is how a drive root is detected: at the
	// top level PathTmp is "" and the item is "C:", so no separator is wanted;
	// below that PathTmp is "C:" or longer and a backslash is needed.
	int pathLen = strlen(PathTmp);
	int nameLen = strlen(pName);
	int sepLen  = (pathLen >= 2) ? 1 : 0;
	if (pathLen + sepLen + nameLen >= rfbMAX_PATH) {
		vnclog.Print(0, _T("Path too long to enter: %s\n"), pName);
		strcpy(PathTmp, Path);		// leave the stored path unchanged
		BlockingFileTransferDialog(TRUE);
		return;
	}

	if (sepLen)
		strcat(PathTmp, "\\");
	strcat(PathTmp, pName);

	if (hwnd == m_hwndFTClientList)
		ShowClientItems(PathTmp);
	else
		SendFileListRequestMessage(PathTmp, 0);
}

void
FileTransfer::ConvertPath(char *path)
{
	int len = strlen(path);
	if (len >= rfbMAX_PATH) return;
	if (strcmp(path, "") == 0) {strcpy(path, "/"); return;}
	for (int i = (len - 1); i >= 0; i--) {
		if (path[i] == '\\') path[i] = '/';
		path[i+1] = path[i];
	}
	path[len + 1] = '\0';
	path[0] = '/';
	return;
}

void 
FileTransfer::ShowListViewItems(HWND hwnd, FileTransferItemInfo *ftii)
{
	LVITEM LVItem;
	LVItem.mask = LVIF_TEXT | LVIF_STATE; 
	LVItem.state = 0; 
	LVItem.stateMask = 0; 
	for (int i=0; i<ftii->GetNumEntries(); i++) {
		LVItem.iItem = i;
		LVItem.iSubItem = 0;
		LVItem.pszText = LPSTR_TEXTCALLBACK;
		ListView_InsertItem(hwnd, &LVItem);
	}
}

//
// FTGetClickedItem - recover the row index for an NM_DBLCLK notification.
//
// COMCTL32 4.00 sends a bare NMHDR for NM_DBLCLK: there is no iItem field (the
// NMITEMACTIVATE structure that carries one arrived with 4.70).  The index must
// therefore be derived from the mouse position.
//
// The previous code used ListView_GetNextItem(..., LVNI_FOCUSED), which fails in
// the common case: immediately after ShowListViewItems() fills the control, rows
// are inserted with state 0, so nothing has the focus flag and it returned -1 -
// the double-click was silently dropped and directory navigation appeared dead.
//
int
FileTransfer::FTGetClickedItem(HWND hwndList)
{
	if (hwndList == NULL)
		return -1;

	// Hit-test the current cursor position, mapped into the list view's client
	// area.  GetMessagePos gives the position of the message being processed,
	// which is the double-click itself - GetCursorPos would give wherever the
	// mouse happens to be now.
	DWORD msgPos = GetMessagePos();
	POINT pt;
	pt.x = (int)(short)LOWORD(msgPos);
	pt.y = (int)(short)HIWORD(msgPos);
	ScreenToClient(hwndList, &pt);

	LV_HITTESTINFO ht;
	memset(&ht, 0, sizeof(ht));
	ht.pt = pt;
	int iItem = ListView_HitTest(hwndList, &ht);
	if (iItem != -1)
		return iItem;

	// Fall back to the selected item, then to the focused item.
	iItem = ListView_GetNextItem(hwndList, -1, LVNI_SELECTED);
	if (iItem != -1)
		return iItem;

	return ListView_GetNextItem(hwndList, -1, LVNI_FOCUSED);
}

void
FileTransfer::FTInsertColumn(HWND hwnd, char *iText, int iOrder, int xWidth)
{
  // ==========================================================================
  // WIN32S FIX - this is why the file lists were empty.
  //
  // Was: an LVCOLUMN struct carrying an extra iOrder field, with LVCF_ORDER in
  // the mask.  Both iOrder and LVCF_ORDER were introduced in COMCTL32 4.70
  // (Internet Explorer 3).  The COMCTL32 available under Win32s / Windows 3.1
  // is 4.00, and its LVM_INSERTCOLUMN:
  //
  //   * does not understand LVCF_ORDER, and
  //   * reads a structure four bytes SHORTER than the one we passed.
  //
  // An unrecognised mask bit makes LVM_INSERTCOLUMN fail and return -1.  A
  // report-mode list view with no columns has nothing to draw, so BOTH panes
  // came up blank regardless of how many items were inserted - which also
  // means there was nothing to double-click, so the navigation "failure" was
  // the same bug seen from the other end.
  //
  // Use the SDK's own LV_COLUMN and only the four fields that exist in every
  // COMCTL32 version.  Also check the result: a silent failure here is exactly
  // what made this hard to see.
  // ==========================================================================
  if (hwnd == NULL)
    return;

  LV_COLUMN lvc;
  memset(&lvc, 0, sizeof(lvc));
  lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
  // LVCFMT_RIGHT on column 0 is ignored by the control anyway (column 0 is
  // always left-aligned); keep RIGHT for the size column only, so the name
  // column looks correct on old COMCTL32 too.
  lvc.fmt = (iOrder == 0) ? LVCFMT_LEFT : LVCFMT_RIGHT;
  lvc.iSubItem = iOrder;
  lvc.pszText = iText;
  lvc.cchTextMax = strlen(iText) + 1;	// was a hardcoded 10
  lvc.cx = xWidth;

  if (ListView_InsertColumn(hwnd, iOrder, &lvc) == -1) {
    vnclog.Print(0, _T("ListView_InsertColumn(%d) failed\n"), iOrder);
  }
}

void 
FileTransfer::InitProgressBar(int nPosition, int nMinRange, int nMaxRange, int nStep)
{
  SendMessage(m_hwndFTProgress, PBM_SETPOS, (WPARAM) nPosition, (LPARAM) 0);
  SendMessage(m_hwndFTProgress, PBM_SETRANGE, (WPARAM) 0, MAKELPARAM(nMinRange, nMaxRange)); 
  SendMessage(m_hwndFTProgress, PBM_SETSTEP, (WPARAM) nStep, 0); 
}

void 
FileTransfer::CreateServerItemInfoList(FileTransferItemInfo *pftii, 
                  										 FTSIZEDATA *ftsd, int ftsdNum,
											                 char *pfnames, int fnamesSize)
{
  if (pftii == NULL || ftsd == NULL || pfnames == NULL || fnamesSize <= 0)
    return;

  // The name blob is a run of NUL-terminated strings whose total length the
  // server declared as fnamesSize.  The old loop walked it with
  // "pos += strlen(pfnames + pos) + 1" and never checked pos against
  // fnamesSize - so a blob whose last string was not terminated, or whose
  // declared numFiles exceeded the number of strings actually present, walked
  // straight off the end of the allocation.  The caller now NUL-terminates the
  // blob, and this loop stops at the boundary.
  int pos = 0;
  for (int i = 0; i < ftsdNum; i++) {
    if (pos >= fnamesSize)
      break;

    char buf[16];
    ftsd[i].size = Swap32IfLE(ftsd[i].size);
    ftsd[i].data = Swap32IfLE(ftsd[i].data);
    if (ftsd[i].size == (unsigned int)-1) {
      strcpy(buf, FileTransferItemInfo::folderText);
    } else {
      // %lu: size is an unsigned int and a >2 GB file printed as negative,
      // which then failed the "is this a folder?" string comparison in
      // ProcessListViewDBLCLK in a confusing way.
      sprintf(buf, "%lu", (unsigned long)ftsd[i].size);
    }
    pftii->Add(pfnames + pos, buf, ftsd[i].data);
    pos += strlen(pfnames + pos) + 1;
  }
}

void 
FileTransfer::SendFileUploadDataMessage(unsigned int mTime)
{
	rfbFileUploadDataMsg msg;
	msg.type = rfbFileUploadData;
	msg.compressedLevel = 0;
	msg.realSize = Swap16IfLE(0);
	msg.compressedSize = Swap16IfLE(0);

	CARD32 time32 = Swap32IfLE((CARD32)mTime);

	char data[sz_rfbFileUploadDataMsg + sizeof(CARD32)];
	memcpy(data, &msg, sz_rfbFileUploadDataMsg);
	memcpy(&data[sz_rfbFileUploadDataMsg], &time32, sizeof(CARD32));

	m_clientconn->WriteExact(data, sz_rfbFileUploadDataMsg + sizeof(CARD32));
}

void 
FileTransfer::SendFileUploadDataMessage(unsigned short size, char *pFile)
{
	int msgLen = sz_rfbFileUploadDataMsg + size;
	char *pAllFUDMessage = new char[msgLen];
	rfbFileUploadDataMsg *pFUD = (rfbFileUploadDataMsg *) pAllFUDMessage;
	char *pFollow = &pAllFUDMessage[sz_rfbFileUploadDataMsg];
	pFUD->type = rfbFileUploadData;
	pFUD->compressedLevel = 0;
	pFUD->realSize = Swap16IfLE(size);
	pFUD->compressedSize = Swap16IfLE(size);
	memcpy(pFollow, pFile, size);
	m_clientconn->WriteExact(pAllFUDMessage, msgLen);
	delete [] pAllFUDMessage;
}

void 
FileTransfer::SendFileDownloadCancelMessage(unsigned short reasonLen, char *reason)
{
  int msgLen = sz_rfbFileDownloadCancelMsg + reasonLen;
  char *pAllFDCMessage = new char[msgLen];
  rfbFileDownloadCancelMsg *pFDC = (rfbFileDownloadCancelMsg *) pAllFDCMessage;
  char *pFollow = &pAllFDCMessage[sz_rfbFileDownloadCancelMsg];
  pFDC->type = rfbFileDownloadCancel;
  pFDC->reasonLen = Swap16IfLE(reasonLen);
  memcpy(pFollow, reason, reasonLen);
  m_clientconn->WriteExact(pAllFDCMessage, msgLen);
  delete [] pAllFDCMessage;
}

void
FileTransfer::ReadUploadCancel()
{
  if (m_bUploadStarted) {
    m_bUploadStarted = FALSE;
    CloseHandle(m_hFiletoRead);
  }
	// Stop file transfer
	CloseUndoneFileTransfers();

	// Read the message
	rfbFileUploadCancelMsg msg;
	m_clientconn->ReadExact((char *)&msg, sz_rfbFileUploadCancelMsg);
	int len = Swap16IfLE(msg.reasonLen);
	char *reason = new char[len + 1];
	m_clientconn->ReadExact(reason, len);
	reason[len] = '\0';

	// Report error (only once per upload)
	if (m_bReportUploadCancel) {
		char *errmsg = new char[128 + len];
		sprintf(errmsg, "Upload failed: %s", reason);
		MessageBox(m_hwndFileTransfer, errmsg, "Upload Failed", MB_ICONEXCLAMATION | MB_OK);
		SetWindowText(m_hwndFTStatus, errmsg);
		vnclog.Print(1, _T("Upload failed: %s\n"), reason);
		m_bReportUploadCancel = FALSE;
		delete[] errmsg;
	}
	delete[] reason;

	// Enable dialog
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
	BlockingFileTransferDialog(TRUE);
}

void
FileTransfer::ReadDownloadFailed()
{
	// We'll report the error only if we're actually downloading
	BOOL downloadActive = m_bDownloadStarted;

	// Stop file transfer
	CloseUndoneFileTransfers();

	// Read the message
	rfbFileDownloadFailedMsg msg;
	m_clientconn->ReadExact((char *)&msg, sz_rfbFileDownloadFailedMsg);
	int len = Swap16IfLE(msg.reasonLen);
	char *reason = new char[len + 1];
	m_clientconn->ReadExact(reason, len);
	reason[len] = '\0';

	// Report error
	if (downloadActive) {
		char *errmsg = new char[128 + len];
		sprintf(errmsg, "Download failed: %s", reason);
		MessageBox(m_hwndFileTransfer, errmsg, "Download Failed", MB_ICONEXCLAMATION | MB_OK);
		SetWindowText(m_hwndFTStatus, errmsg);
		vnclog.Print(1, _T("Download failed: %s\n"), reason);
		delete[] errmsg;
	}
	delete[] reason;

	// Enable dialog
	EnableWindow(GetDlgItem(m_hwndFileTransfer, IDC_FTCANCEL), FALSE);
	BlockingFileTransferDialog(TRUE);
}

unsigned int FileTransfer::FiletimeToTime70(FILETIME ftime)
{
	LARGE_INTEGER uli;
	uli.LowPart = ftime.dwLowDateTime;
	uli.HighPart = ftime.dwHighDateTime;
	uli.QuadPart = (uli.QuadPart - 116444736000000000) / 10000000;
	return uli.LowPart;
}

void FileTransfer::Time70ToFiletime(unsigned int time70, FILETIME *pftime)
{
    LONGLONG ll = Int32x32To64(time70, 10000000) + 116444736000000000;
    pftime->dwLowDateTime = (DWORD) ll;
    pftime->dwHighDateTime = (DWORD)(ll >> 32);
}
