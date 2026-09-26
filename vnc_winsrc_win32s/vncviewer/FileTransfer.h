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

#if !defined(FILETRANSFER)
#define FILETRANSFER

#include "windows.h"
#include "commctrl.h"
#include "ClientConnection.h"
#include "FileTransferItemInfo.h"

// bool/true/false come from win32s_fix.h (force-included).  Do not redefine.

#pragma pack(push, 8)

// 1. NMHDR is already safely defined inside your windows.h/commctrl.h 
// No manual declaration block needed here.

// 2. Map local convenience aliases to the MSVC 4 SDK structure names
// This bridges legacy naming variations smoothly.
#ifndef TVITEM_DEFINED
typedef TV_ITEM          TVITEM;
typedef TV_ITEM*        LPTVITEM;
#define TVITEM_DEFINED
#endif

#ifndef NMTREEVIEW_DEFINED
typedef NM_TREEVIEW      NMTREEVIEW;
typedef NM_TREEVIEW*     LPNMTREEVIEW;
#define NMTREEVIEW_DEFINED
#endif

#ifndef LVITEM_DEFINED
typedef LV_ITEM          LVITEM;
typedef LV_ITEM*         LPLVITEM; 
#define LVITEM_DEFINED
#endif

#ifndef NMLVDISPINFO_DEFINED
// MSVC 4 SDK natively named this structure LV_DISPINFO instead of NMLVDISPINFO
typedef LV_DISPINFO      NMLVDISPINFO;
typedef LV_DISPINFO*     LPNMLVDISPINFO;
#define NMLVDISPINFO_DEFINED
#endif

#pragma pack(pop)




class ClientConnection;

class FileTransfer  
{
private:
	static const char uploadText[];
	static const char downloadText[];
	static const char noactionText[];

public:
	FileTransfer(ClientConnection * pCC, VNCviewerApp * pApp);
	~FileTransfer();

	void FTInsertColumn(HWND hwnd, char *iText, int iOrder, int xWidth);
	void CreateFileTransferDialog();
	void ShowListViewItems(HWND hwnd, FileTransferItemInfo *ftii);
	void ConvertPath(char *path);
	void ProcessListViewDBLCLK(HWND hwnd, char *Path, char *PathTmp, int iItem);
	void SendFileListRequestMessage(char *filename, unsigned char flags);
	void ShowServerItems();
	void ShowClientItems(char *path);
	void BlockingFileTransferDialog(BOOL status);
	void ProcessDlgMessage(HWND hwnd);
	void ShowTreeViewItems(HWND hwnd, LPNMTREEVIEW m_lParam);
	void CreateFTBrowseDialog(BOOL status);
	void StrInvert(char *str);
	void GetTVPath(HWND hwnd, HTREEITEM hTItem, char *path);
	char m_ServerPath[rfbMAX_PATH];
	char m_ClientPath[rfbMAX_PATH];
	char m_ServerPathTmp[rfbMAX_PATH];
	char m_ClientPathTmp[rfbMAX_PATH];
	char m_ServerFilename[rfbMAX_PATH];
	char m_ClientFilename[rfbMAX_PATH];
	char m_UploadFilename[rfbMAX_PATH];
	char m_DownloadFilename[rfbMAX_PATH];
	void OnGetDispClientInfo(NMLVDISPINFO *plvdi); 
	void OnGetDispServerInfo(NMLVDISPINFO *plvdi); 

	// Pump network + browse-dialog messages until the outstanding server
	// browse reply arrives (or the timeout expires).  Must only be called
	// from the browse dialog's UI thread while m_bBrowseReplyPending is set.
	void WaitForBrowseReply(DWORD timeoutMs);

	// Which list-view row did the user just double-click?
	//
	// COMCTL32 4.00 (the version under Win32s) does not send LVN_ITEMACTIVATE
	// and its NM_DBLCLK notification carries no item index, so the index has to
	// be recovered by hit-testing the cursor.  See the implementation in
	// FileTransfer.cpp for why LVNI_FOCUSED was not good enough.
	int FTGetClickedItem(HWND hwndList);

	static LRESULT CALLBACK FileTransferDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK FTBrowseDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void FileTransferDownload();
	void FileTransferUpload();
	void CloseUndoneFileTransfers();

	void ReadUploadCancel();
	void ReadDownloadFailed();

	BOOL SendFileDownloadRequest();
	BOOL SendMultipleFileDownloadRequests();

	ClientConnection * m_clientconn;
	VNCviewerApp * m_pApp; 
	
private:
	DWORD m_dwDownloadRead;
	DWORD m_dwDownloadBlockSize;
	int m_sizeDownloadFile;
	int m_numOfFilesToDownload;
	int m_currentDownloadIndex;
	void Time70ToFiletime(unsigned int time70, FILETIME *pftime);
	unsigned int FiletimeToTime70(FILETIME ftime);
	void SendFileUploadDataMessage(unsigned short size, char *pFile);
	void SendFileUploadDataMessage(unsigned int mTime);
	void CancelDownload(char *reason);
	void SendFileDownloadCancelMessage(unsigned short reasonLen, char *reason);
	void CreateServerItemInfoList(FileTransferItemInfo *pftii, FTSIZEDATA *ftsd, int ftsdNum, char *pfnames, int fnamesSize);
	void InitProgressBar(int nPosition, int nMinRange, int nMaxRange, int nStep);
	HWND m_hwndFileTransfer;
	HWND m_hwndFTClientList;
	HWND m_hwndFTServerList;
	HWND m_hwndFTClientPath;
	HWND m_hwndFTServerPath;
	HWND m_hwndFTProgress;
	HWND m_hwndFTStatus;
	HWND m_hwndFTBrowse;
	
	BOOL m_bFTCOPY;
    BOOL m_bUploadStarted;
    BOOL m_bDownloadStarted;
	BOOL m_bTransferEnable;
	BOOL m_bReportUploadCancel;
	BOOL m_bServerBrowseRequest;
	BOOL m_bFirstFileDownloadMsg;
	// Set while a server directory listing for the browse tree is in
	// flight.  The browse dialog is modal (DialogBoxParam), so the main
	// idle loop cannot PumpIdle() for us - the sender waits on this flag
	// (see WaitForBrowseReply in FileTransfer.cpp).  ShowServerItems()
	// clears it once the reply has been consumed.
	BOOL m_bBrowseReplyPending;
	// Re-entrancy guard for server tree expansion.  Populating the tree
	// must not recursively trigger another TVN_ITEMEXPANDING for the same
	// item (TreeView_Expand inside ShowServerItems did exactly that:
	// expand -> request -> populate -> Expand -> expand ... infinitely,
	// flickering the [+] and locking Win32s).  The notify handler bails
	// out while this is set.
	BOOL m_bInBrowseExpand;

	HANDLE m_hFiletoWrite;
    HANDLE m_hFiletoRead;
	HTREEITEM m_hTreeItem;
	HINSTANCE m_FTInstance;

	FileTransferItemInfo m_FTClientItemInfo;
	FileTransferItemInfo m_FTServerItemInfo;
};

#endif // !defined(FILETRANSFER)
