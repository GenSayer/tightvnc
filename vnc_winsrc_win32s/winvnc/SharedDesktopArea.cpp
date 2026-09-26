// Copyright (C) 2004-2006 TightVNC Group. All Rights Reserved.
//
//  TightVNC is free software; you can redistribute it and/or modify
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
// TightVNC homepage on the Web: http://www.tightvnc.com/

// SharedDesktopArea.cpp: implementation of the SharedDesktopArea class.


#include "SharedDesktopArea.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SharedDesktopArea::SharedDesktopArea(HWND hwnd,
									 CMatchWindow *matchwindow,
									 vncServer *server)
{
	m_hwnd = hwnd;
	m_pMatchWindow = matchwindow;
	m_server = server;

	m_bCaptured = FALSE;
	m_KeepHandle = NULL;

	// If a matchwindow wasn't passed in, we're responsible for it
	m_deleteMatchWindow = (matchwindow == NULL);

	Init();
}

SharedDesktopArea::~SharedDesktopArea()
{
	if (m_deleteMatchWindow) {
		if (m_pMatchWindow != NULL) {
			delete m_pMatchWindow;
		}
	} else {
		if (m_server->ScreenAreaShared()) {
			m_pMatchWindow->Show();
		} else {
			m_pMatchWindow->Hide();
		}
	}
}

void SharedDesktopArea::Init()
{
	//
	// configure select window picture
	//

	// Subclass the bitmap-cursor static control so we can track the mouse over
	// it (the "pick a window to share" crosshair).
	//
	// WIN32S: IDC_BMPCURSOR is an SS_BITMAP static, and SS_BITMAP is Win95+ - a
	// Windows 3.1 static control cannot display a bitmap resource by ID (see the
	// note in WinVNC.rc).  The control still EXISTS, so subclassing works and the
	// window-picking still functions; only the crosshair picture is missing.
	//
	// GetDlgItem can return NULL if the control is absent from the template.
	// SetWindowLong(NULL, ...) is an invalid-window call, and m_OldBmpWndProc
	// would then be 0 - which BmpWndProc later hands to CallWindowProc.
	HWND bmp_hWnd = GetDlgItem(m_hwnd, IDC_BMPCURSOR);
	m_OldBmpWndProc = 0;
	if (bmp_hWnd != NULL) {
		m_OldBmpWndProc = GetWindowLong(bmp_hWnd, GWL_WNDPROC);
		SetWindowLong(bmp_hWnd, GWL_WNDPROC, (LONG)BmpWndProc);
		SetWindowLong(bmp_hWnd, GWL_USERDATA, (LONG)this);
	}

	//
	// setup match window
	//

	if (m_pMatchWindow == NULL)
		SetupMatchWindow();

	m_pMatchWindow->CanModify(TRUE);
	m_pMatchWindow->Hide();

	//
	// initialize controls
	//

	SetChecked(IDC_FULLSCREEN, m_server->FullScreen());
	SetChecked(IDC_PRIMARY_DISPLAY_ONLY, m_server->PrimaryDisplayOnlyShared());
	SetChecked(IDC_SCREEN, m_server->ScreenAreaShared());
	SetChecked(IDC_WINDOW, m_server->WindowShared());
	m_hwndShared = m_server->GetWindowShared();

	Validate();
}

void SharedDesktopArea::SetupMatchWindow()
{
	// get the desktop's bounds (primary display only)
	RECT desktopRect;
	GetWindowRect(GetDesktopWindow(), &desktopRect);

	//
	// instantiate object with default bounds
	// ( i.e. top-left quadrant of the desktop )
	//

	m_pMatchWindow = new CMatchWindow(m_server, 
									  desktopRect.left+5, desktopRect.top+5, 
									  desktopRect.right/2, desktopRect.bottom/2);

	//
	// if region sharing is the current setting,
	// the server rect is not whole screen, 
	// and the server rect is not empty, 
	// then use the server's bounds
	//

	if (m_server->ScreenAreaShared())
	{
		// get the server's current rect
		RECT serverRect;
		serverRect = m_server->GetSharedRect();

		if ( EqualRect(&desktopRect, &serverRect) == FALSE &&
			 IsRectEmpty(&serverRect) == FALSE ) {
			// use it for match window
			m_pMatchWindow->ModifyPosition(serverRect.left, serverRect.top,
										   serverRect.right, serverRect.bottom);
		}
	}
}

void SharedDesktopArea::Validate()
{
	if (IsChecked(IDC_SCREEN)) {
		m_pMatchWindow->Show();
	} else {
		m_pMatchWindow->Hide();
	}

	LPCSTR info = NULL;

	if (IsChecked(IDC_FULLSCREEN)) {
		info = "* full desktop selected *";
	} else if (IsChecked(IDC_PRIMARY_DISPLAY_ONLY)) {
		info = "* primary display selected *";
	} else if (IsChecked(IDC_SCREEN)) {
		info = "* screen area selected *";
	}

	BOOL bWindowShared = IsChecked(IDC_WINDOW);

	// Enable/disable window cursor
	Enable(IDC_BMPCURSOR, bWindowShared);

	// Set proper cursor image
	HWND bmp_hWnd = GetDlgItem(m_hwnd, IDC_BMPCURSOR);
	HBITMAP hNewImage = LoadBitmap(hAppInstance, bWindowShared ? MAKEINTRESOURCE(IDB_BITMAP1) : MAKEINTRESOURCE(IDB_BITMAP3));
	HBITMAP hOldImage = (HBITMAP)::SendMessage(bmp_hWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hNewImage);
	DeleteObject(hOldImage);

	// Set window name
	if (bWindowShared) {
		SetWindowCaption(m_hwndShared);
	} else if (info) {
		SetText(IDC_NAME_APPLI, info);
	}
	Enable(IDC_NAME_APPLI, bWindowShared);
}

bool SharedDesktopArea::Apply()
{
	if (IsChecked(IDC_WINDOW) && m_hwndShared == NULL) {
		MessageBox(NULL,
				"You have not yet selected a window to share.\n"
				"Please first select a window with the 'Window Target'\n"
				"icon, and try again.", "No Window Selected",
				 MB_OK | MB_ICONEXCLAMATION);
		return false;
	}

	m_server->FullScreen(IsChecked(IDC_FULLSCREEN));
	m_server->PrimaryDisplayOnlyShared(IsChecked(IDC_PRIMARY_DISPLAY_ONLY));
	m_server->ScreenAreaShared(IsChecked(IDC_SCREEN));
	m_server->WindowShared(IsChecked(IDC_WINDOW));

	if (m_server->FullScreen()) {
		RECT temp = GetScreenRect();
		m_server->SetMatchSizeFields(temp.left, temp.top, temp.right, temp.bottom);
	} else if (m_server->PrimaryDisplayOnlyShared()) {
		int w = GetSystemMetrics(SM_CXSCREEN);
		int h = GetSystemMetrics(SM_CYSCREEN);
		m_server->SetMatchSizeFields(0, 0, w, h);
	} else if (m_server->ScreenAreaShared()) {
		int left, right, top, bottom;
		m_pMatchWindow->GetPosition(left, top, right, bottom);
		m_server->SetMatchSizeFields(left, top, right, bottom);
	} else if  (m_server->WindowShared()) {
		m_server->SetWindowShared(m_hwndShared);
	}

	return true;
}

LRESULT CALLBACK SharedDesktopArea::BmpWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HBITMAP hNewImage, hOldImage;
	HCURSOR hNewCursor, hOldCursor;
	SharedDesktopArea* pDialog = (SharedDesktopArea*) GetWindowLong(hWnd, GWL_USERDATA);

	// WIN32S: pDialog can be NULL.
	//
	// The subclass is installed in three steps (GetWindowLong / SetWindowLong
	// WNDPROC / SetWindowLong USERDATA), and messages can be delivered to this
	// procedure between the second and third - the control is already subclassed
	// but the back-pointer is not yet stored.  Every case below dereferences
	// pDialog.
	//
	// It is also NULL if the SetWindowLong(GWL_USERDATA) was skipped because
	// GetDlgItem returned NULL (see InitPage).
	if (pDialog == NULL)
		return DefWindowProc(hWnd, message, wParam, lParam);

	switch (message) {

	case WM_SETCURSOR :
		if (HIWORD(lParam) == WM_LBUTTONDOWN) {
			SetCapture(hWnd);
			hNewImage = LoadBitmap(hAppInstance, MAKEINTRESOURCE(IDB_BITMAP2));
			hOldImage = (HBITMAP)::SendMessage(hWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hNewImage);
			DeleteObject(hOldImage);
			// WIN32S: LoadImage is Win95+ and is not exported by the Win32s
			// USER32 - having the name in the import table would stop the EXE
			// loading.  Win32sLoadImageIcon resolves it at run time; when it is
			// unavailable it falls back to LoadIcon, so pass the cursor through
			// LoadCursor instead, which exists everywhere.
			hNewCursor = LoadCursor(hAppInstance, MAKEINTRESOURCE(IDC_CURSOR1));
			hOldCursor = SetCursor(hNewCursor);
			DestroyCursor(hOldCursor);
			pDialog->m_bCaptured = TRUE;
		}
		break;

	case WM_LBUTTONUP:
		ReleaseCapture();
		
		if (pDialog->m_KeepHandle != NULL) {
			// We need to remove frame
			DrawFrameAroundWindow(pDialog->m_KeepHandle);
			pDialog->m_hwndShared = pDialog->m_KeepHandle;
			// No more need
			pDialog->m_KeepHandle = NULL;
		} else {
			pDialog->m_hwndShared = NULL;
		}

		hNewImage = LoadBitmap(hAppInstance, MAKEINTRESOURCE(IDB_BITMAP1));
		hOldImage = (HBITMAP)::SendMessage(hWnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hNewImage);
		DeleteObject(hOldImage);
		hNewCursor = LoadCursor(hAppInstance, MAKEINTRESOURCE(IDC_ARROW));
		hOldCursor = SetCursor(hNewCursor);
		DestroyCursor(hOldCursor);
		pDialog->m_bCaptured = FALSE;
		break;
	
	case WM_MOUSEMOVE:
		if (pDialog->m_bCaptured) {
			HWND hParent = ::GetParent(hWnd);
			POINTS pnt;
			POINT pnt1;
			pnt = MAKEPOINTS(lParam);
			pnt1.x = pnt.x;
			pnt1.y = pnt.y;
			ClientToScreen(hWnd, &pnt1);
			HWND hMouseWnd=::WindowFromPoint(pnt1);
			if (pDialog->m_KeepHandle != hMouseWnd) {
				// New Windows Handle
				// Was KeepHandle A Real Window ?
				if (pDialog->m_KeepHandle != NULL) {
					// We need to remove frame
					SharedDesktopArea::DrawFrameAroundWindow(pDialog->m_KeepHandle);
				}
				pDialog->m_KeepHandle = hMouseWnd;
				if (!IsChild(hParent, hMouseWnd) && hMouseWnd != hParent) {
					pDialog->SetWindowCaption(hMouseWnd);
					SharedDesktopArea::DrawFrameAroundWindow(hMouseWnd);
				} else {	// My Window
					pDialog->m_KeepHandle = NULL;
					pDialog->SetWindowCaption(NULL);
				}
			}
		}
		break;

	case WM_PAINT:
	case STM_SETIMAGE:
		// ==================================================================
		// MSVC 4.1 / WIN32S: the cast must be to FARPROC, not WNDPROC.
		//
		// This produced
		//   error C2664: 'CallWindowProcA' : cannot convert parameter 1 from
		//   'long (__stdcall *)(void *,unsigned int,unsigned int,long)'
		//   to 'int (__stdcall *)(void)'
		//
		// The MSVC 4.1 SDK declares CallWindowProc's first parameter as FARPROC,
		// which in a NON-STRICT build is "int (__stdcall *)(void)" - a
		// parameterless function pointer.  Later SDKs declare it as WNDPROC, so
		// "(WNDPROC)" is the correct cast there and this code compiles
		// unchanged on them.
		//
		// C++ will not implicitly convert between two different function-pointer
		// types, so casting to WNDPROC does not help: the compiler still has to
		// convert WNDPROC -> FARPROC to make the call, and it refuses.
		//
		// Casting to FARPROC directly is what this SDK wants.  Note this is
		// specifically a consequence of NOT defining STRICT (see the note in
		// stdhdrs.h): with STRICT, windows.h would declare the parameter as
		// WNDPROC even on 4.1 - but STRICT breaks the HWND/HANDLE typedefs that
		// win32s_fix.h has to make before windows.h is seen.
		//
		// m_OldBmpWndProc is a LONG (the return value of GetWindowLong), so the
		// cast is from an integer type either way.
		// ==================================================================
		// Guard against a failed subclass (m_OldBmpWndProc == 0): CallWindowProc
		// with a NULL proc is undefined, and on Win32s it would call through a
		// null pointer.
		if (pDialog->m_OldBmpWndProc == 0)
			return DefWindowProc(hWnd, message, wParam, lParam);

		return CallWindowProc((FARPROC)pDialog->m_OldBmpWndProc,
							  hWnd, message, wParam, lParam);
	
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


void SharedDesktopArea::DrawFrameAroundWindow(HWND hWnd)
{
	HDC hWindowDc=::GetWindowDC(hWnd);
	HBRUSH hBrush=CreateSolidBrush(RGB(0,0,0));
	HRGN Rgn=CreateRectRgn(0,0,1,1);
	int iRectResult=GetWindowRgn(hWnd,Rgn);
	if (iRectResult==ERROR || iRectResult==NULLREGION || Rgn==NULL)
	{
		RECT rect;
		GetWindowRect(hWnd,&rect);
		OffsetRect(&rect,-rect.left,-rect.top);
		Rgn=CreateRectRgn(rect.left,rect.top,rect.right,rect.bottom);
	}
	
	SetROP2(hWindowDc,R2_MERGEPENNOT);
	FrameRgn(hWindowDc,Rgn,hBrush,3,3);
	::DeleteObject(Rgn);
	::DeleteObject(hBrush);
    ::ReleaseDC(hWnd,hWindowDc);
}

void SharedDesktopArea::SetWindowCaption(HWND hWnd)
{
	char strWindowText[256];
	if (hWnd == NULL) {
		strcpy(strWindowText, "* no window selected *");
	} else {
		GetWindowText(hWnd, strWindowText, sizeof(strWindowText));
		if (!strWindowText[0]) 
		{
			int bytes = sprintf(strWindowText, "0x%x ", hWnd);
			GetClassName(hWnd, strWindowText + bytes,
						 sizeof(strWindowText) - bytes);
		}
	}

	SetText(IDC_NAME_APPLI, strWindowText);
}
