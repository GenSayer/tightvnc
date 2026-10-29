// WndClassSm.h - window classes with a small icon, where the system has them
//
// RegisterClassEx and LoadImage came with NT 3.51 and Windows 95.  NT 3.1,
// NT 3.5 and Win32s do not have them, and a static import of either would
// stop the programs from loading there.  These helpers look them up at run
// time and fall back to RegisterClass and no small icon (those systems draw
// no small icons anyway).  Only from NT 3.51 on: the USER32 of NT 3.5 can
// export the names without supporting them, and a class that fails to
// register leaves the server without its tray window and the viewer without
// its main window.

#ifndef WNDCLASSSM_H
#define WNDCLASSSM_H

#ifndef IMAGE_ICON
#define IMAGE_ICON		1
#endif
#ifndef LR_SHARED
#define LR_SHARED		0x8000
#endif
#ifndef SM_CXSMICON
#define SM_CXSMICON		49
#define SM_CYSMICON		50
#endif

// The layout of WNDCLASSEXA, declared here because not every set of headers
// this tree is compiled with has it.
typedef struct {
	UINT		cbSize;
	UINT		style;
	WNDPROC		lpfnWndProc;
	int			cbClsExtra;
	int			cbWndExtra;
	HINSTANCE	hInstance;
	HICON		hIcon;
	HCURSOR		hCursor;
	HBRUSH		hbrBackground;
	LPCSTR		lpszMenuName;
	LPCSTR		lpszClassName;
	HICON		hIconSm;
} WNDCLASSSM;

typedef ATOM (WINAPI *WNDCLASSSM_REGISTEREX)(const WNDCLASSSM *);
typedef HANDLE (WINAPI *WNDCLASSSM_LOADIMAGE)(HINSTANCE, LPCSTR, UINT,
											  int, int, UINT);

static FARPROC WndClassSmProc(LPCSTR name)
{
	DWORD ver = GetVersion();
	BYTE major = LOBYTE(LOWORD(ver));
	BYTE minor = HIBYTE(LOWORD(ver));
	HMODULE user32;

	if (major < 3 || (major == 3 && minor < 51))
		return NULL;
	user32 = GetModuleHandle("USER32.DLL");
	if (user32 == NULL)
		return NULL;
	return GetProcAddress(user32, name);
}

// The 16x16 image of an icon resource, rather than the 32x32 one shrunk by
// the system; NULL where there is no LoadImage.
static HICON LoadSmallIcon(HINSTANCE hinst, LPCSTR name)
{
	WNDCLASSSM_LOADIMAGE pLoadImage =
		(WNDCLASSSM_LOADIMAGE)WndClassSmProc("LoadImageA");

	if (pLoadImage == NULL)
		return NULL;
	return (HICON)pLoadImage(hinst, name, IMAGE_ICON,
							 GetSystemMetrics(SM_CXSMICON),
							 GetSystemMetrics(SM_CYSMICON), LR_SHARED);
}

// RegisterClassEx with the given small icon where it exists and works,
// otherwise RegisterClass.
static ATOM RegisterClassSm(const WNDCLASS *wc, HICON hIconSm)
{
	WNDCLASSSM_REGISTEREX pRegisterEx =
		(WNDCLASSSM_REGISTEREX)WndClassSmProc("RegisterClassExA");
	WNDCLASSSM wcx;
	ATOM atom;

	if (pRegisterEx == NULL)
		return RegisterClass(wc);

	wcx.cbSize			= sizeof(wcx);
	wcx.style			= wc->style;
	wcx.lpfnWndProc		= wc->lpfnWndProc;
	wcx.cbClsExtra		= wc->cbClsExtra;
	wcx.cbWndExtra		= wc->cbWndExtra;
	wcx.hInstance		= wc->hInstance;
	wcx.hIcon			= wc->hIcon;
	wcx.hCursor			= wc->hCursor;
	wcx.hbrBackground	= wc->hbrBackground;
	wcx.lpszMenuName	= wc->lpszMenuName;
	wcx.lpszClassName	= wc->lpszClassName;
	wcx.hIconSm			= hIconSm;
	atom = pRegisterEx(&wcx);
	if (atom == 0)
		atom = RegisterClass(wc);
	return atom;
}

#endif // WNDCLASSSM_H
