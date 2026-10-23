/* vc2gate.h - when the msvc4 tree uses the built-in common controls
 *
 * Only for the msvc4.mak build, which defines USE_VC2CTL; the Developer
 * Studio makefiles do not, and never include this file.
 *
 * Policy for this tree (narrower than the VC++ 2.x tree's, see vc2ctl.h):
 *
 *   * Windows NT before 3.51 (NT 3.1, NT 3.5, on every CPU): the built-in
 *     controls from vc2ctl, always.  COMCTL32 is not loaded at all, so a
 *     COMCTL32 copied from NT 3.51 is neither needed nor used.
 *   * NT 3.51 and later, Windows 95: COMCTL32 exactly as before.  vc2ctl is
 *     never called, so nothing of it runs there.
 *
 * COMCTL32 is loaded with LoadLibrary instead of being imported, so the EXE
 * still starts on an NT 3.5 system without a usable COMCTL32.DLL.  COMCTL32
 * registers its classes when it is loaded, which happens here, before any
 * window that uses them - the same as the static import did.
 */

#ifndef VC2GATE_H
#define VC2GATE_H

#include <windows.h>
#include "vc2ctl.h"

typedef void (WINAPI *VC2GATE_INITCC)(void);

/* Replaces InitCommonControls().  'classes' is the VC2CTL_* mask of built-in
 * controls the program needs on NT before 3.51. */
static void Vc2GateInitCommonControls(DWORD classes)
{
	HMODULE hComCtl32;
	VC2GATE_INITCC pInit;

	if (Vc2CtlForceBuiltins()) {
		Vc2CtlInit(classes);
		return;
	}

	hComCtl32 = GetModuleHandle("comctl32.dll");
	if (hComCtl32 == NULL)
		hComCtl32 = LoadLibrary("comctl32.dll");
	if (hComCtl32 == NULL)
		return;
	pInit = (VC2GATE_INITCC)GetProcAddress(hComCtl32, "InitCommonControls");
	if (pInit != NULL)
		pInit();
}

#endif /* VC2GATE_H */
