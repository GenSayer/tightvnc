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

#include "WallpaperUtils.h"

WallpaperUtils::WallpaperUtils()
{
	m_restore_ActiveDesktop = false;
	m_restore_wallpaper = false;
}

void
WallpaperUtils::KillActiveDesktop()
{
  
  vnclog.Print(LL_INTINFO, VNCLOG("Active Desktop not supported in this build - ignoring\n"));
  m_restore_ActiveDesktop = false;
}

void
WallpaperUtils::KillWallpaper()
{
	if (!m_restore_wallpaper) {
		// Tell all applications that there is no wallpaper
		// Note that this doesn't change the wallpaper registry setting!
		//
		// WIN32S: SPI_SETDESKWALLPAPER exists in Windows 3.1's
		// SystemParametersInfo, so this genuinely works - removing the wallpaper
		// is worth doing here, because a tiled bitmap background is exactly the
		// kind of thing that makes every full-screen poll expensive.
		//
		// SPIF_SENDCHANGE is honoured on 3.1 as well (it broadcasts
		// WM_WININICHANGE).
		SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, "", SPIF_SENDCHANGE);
		m_restore_wallpaper = true;
	}

	// WIN32S: CoInitialize/CoUninitialize removed.
	//
	// These are OLE32 imports.  OLE32.DLL is not part of Win32s (OLE 2 for
	// Windows 3.1 exists as a separate 16-bit package, but its 32-bit entry
	// points do not), so linking against them stops the EXE from LOADING.
	//
	// They were only here for KillActiveDesktop(), which in this build already
	// does nothing but log "Active Desktop not supported in this build" - Active
	// Desktop is an Internet Explorer 4 shell feature and could not exist on
	// Windows 3.1 in any case.
	KillActiveDesktop();
}

void
WallpaperUtils::RestoreActiveDesktop()
{
 	m_restore_ActiveDesktop = false;
}

void
WallpaperUtils::RestoreWallpaper()
{
	// WIN32S: CoInitialize/CoUninitialize removed - see KillWallpaper above.
	RestoreActiveDesktop();

	if (m_restore_wallpaper) {
		// Passing NULL (rather than "") makes Windows re-read the wallpaper from
		// the registry / WIN.INI and restore it.
		SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, NULL, SPIF_SENDCHANGE);
		m_restore_wallpaper = false;
	}
}

