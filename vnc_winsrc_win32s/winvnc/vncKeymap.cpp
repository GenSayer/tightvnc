//  Copyright (C) 2002 RealVNC Ltd. All Rights Reserved.
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
// If the source code for the program is not available from the place from
// which you received this file, check http://www.realvnc.com/ or contact
// the authors on info@realvnc.com for information on obtaining it.

// vncKeymap.cpp

// This code originally just mapped between X keysyms and local Windows
// virtual keycodes.  Now it actually does the local-end simulation of
// key presses, to keep this messy code on one place!

// Disable warnings about truncated names caused by #include <map>
#pragma warning(disable : 4786)

#include "vncKeymap.h"

#define XK_MISCELLANY
#include "keysymdef.h"
#include "vncService.h"

#include "map.h"

// Mapping of X keysyms to windows VK codes.  Ordering here is the same as
// keysymdef.h to make checking easier

struct keymap_t {
  CARD32 keysym;
  CARD8 vk;
  bool extended;
};

static keymap_t keymap[] = {

  // TTY functions

  { XK_BackSpace,        VK_BACK, 0 },
  { XK_Tab,              VK_TAB, 0 },
  { XK_Clear,            VK_CLEAR, 0 },
  { XK_Return,           VK_RETURN, 0 },
  { XK_Pause,            VK_PAUSE, 0 },
  { XK_Escape,           VK_ESCAPE, 0 },
  { XK_Delete,           VK_DELETE, 1 },

  // Cursor control & motion

  { XK_Home,             VK_HOME, 1 },
  { XK_Left,             VK_LEFT, 1 },
  { XK_Up,               VK_UP, 1 },
  { XK_Right,            VK_RIGHT, 1 },
  { XK_Down,             VK_DOWN, 1 },
  { XK_Page_Up,          VK_PRIOR, 1 },
  { XK_Page_Down,        VK_NEXT, 1 },
  { XK_End,              VK_END, 1 },

  // Misc functions

  { XK_Select,           VK_SELECT, 0 },
  { XK_Print,            VK_SNAPSHOT, 0 },
  { XK_Execute,          VK_EXECUTE, 0 },
  { XK_Insert,           VK_INSERT, 1 },
  { XK_Help,             VK_HELP, 0 },
  { XK_Break,            VK_CANCEL, 1 },

  // Keypad Functions, keypad numbers

  { XK_KP_Space,         VK_SPACE, 0 },
  { XK_KP_Tab,           VK_TAB, 0 },
  { XK_KP_Enter,         VK_RETURN, 1 },
  { XK_KP_F1,            VK_F1, 0 },
  { XK_KP_F2,            VK_F2, 0 },
  { XK_KP_F3,            VK_F3, 0 },
  { XK_KP_F4,            VK_F4, 0 },
  { XK_KP_Home,          VK_HOME, 0 },
  { XK_KP_Left,          VK_LEFT, 0 },
  { XK_KP_Up,            VK_UP, 0 },
  { XK_KP_Right,         VK_RIGHT, 0 },
  { XK_KP_Down,          VK_DOWN, 0 },
  { XK_KP_End,           VK_END, 0 },
  { XK_KP_Page_Up,       VK_PRIOR, 0 },
  { XK_KP_Page_Down,     VK_NEXT, 0 },
  { XK_KP_Begin,         VK_CLEAR, 0 },
  { XK_KP_Insert,        VK_INSERT, 0 },
  { XK_KP_Delete,        VK_DELETE, 0 },
  // XXX XK_KP_Equal should map in the same way as ascii '='
  { XK_KP_Multiply,      VK_MULTIPLY, 0 },
  { XK_KP_Add,           VK_ADD, 0 },
  { XK_KP_Separator,     VK_SEPARATOR, 0 },
  { XK_KP_Subtract,      VK_SUBTRACT, 0 },
  { XK_KP_Decimal,       VK_DECIMAL, 0 },
  { XK_KP_Divide,        VK_DIVIDE, 1 },

  { XK_KP_0,             VK_NUMPAD0, 0 },
  { XK_KP_1,             VK_NUMPAD1, 0 },
  { XK_KP_2,             VK_NUMPAD2, 0 },
  { XK_KP_3,             VK_NUMPAD3, 0 },
  { XK_KP_4,             VK_NUMPAD4, 0 },
  { XK_KP_5,             VK_NUMPAD5, 0 },
  { XK_KP_6,             VK_NUMPAD6, 0 },
  { XK_KP_7,             VK_NUMPAD7, 0 },
  { XK_KP_8,             VK_NUMPAD8, 0 },
  { XK_KP_9,             VK_NUMPAD9, 0 },

  // Auxilliary Functions

  { XK_F1,               VK_F1, 0 },
  { XK_F2,               VK_F2, 0 },
  { XK_F3,               VK_F3, 0 },
  { XK_F4,               VK_F4, 0 },
  { XK_F5,               VK_F5, 0 },
  { XK_F6,               VK_F6, 0 },
  { XK_F7,               VK_F7, 0 },
  { XK_F8,               VK_F8, 0 },
  { XK_F9,               VK_F9, 0 },
  { XK_F10,              VK_F10, 0 },
  { XK_F11,              VK_F11, 0 },
  { XK_F12,              VK_F12, 0 },
  { XK_F13,              VK_F13, 0 },
  { XK_F14,              VK_F14, 0 },
  { XK_F15,              VK_F15, 0 },
  { XK_F16,              VK_F16, 0 },
  { XK_F17,              VK_F17, 0 },
  { XK_F18,              VK_F18, 0 },
  { XK_F19,              VK_F19, 0 },
  { XK_F20,              VK_F20, 0 },
  { XK_F21,              VK_F21, 0 },
  { XK_F22,              VK_F22, 0 },
  { XK_F23,              VK_F23, 0 },
  { XK_F24,              VK_F24, 0 },

    // Modifiers
    
  { XK_Shift_L,          VK_SHIFT, 0 },
  { XK_Shift_R,          VK_RSHIFT, 0 },
  { XK_Control_L,        VK_CONTROL, 0 },
  { XK_Control_R,        VK_CONTROL, 1 },
  { XK_Alt_L,            VK_MENU, 0 },
  { XK_Alt_R,            VK_RMENU, 1 },
};


// doKeyboardEvent wraps the system keybd_event function and attempts to find
// the appropriate scancode corresponding to the supplied virtual keycode.

// ==========================================================================
// WIN32S: SYNTHESISE INPUT WITH WINDOW MESSAGES, NOT keybd_event.
//
// THE FINDING.  The diagnostic settled this conclusively:
//
//   kbd inject: vk=0x41 scan=0x1e flags=0x0 asyncstate=0x0000 keystate=0x0000
//
// vk=0x41 is 'A' and scan=0x1e is the correct scan code, so MapVirtualKey works
// and keybd_event is called with entirely correct arguments.  But asyncstate and
// keystate read 0x0000 on EVERY line, including immediately after injecting a
// key-DOWN - so the system does not believe the key was pressed.  The event is
// discarded below the API.
//
// The same is true of mouse_event: the log shows clicks arriving with correct
// flags (0x2 down, 0x4 up) and correct positions, less than a second apart, with
// no effect.  Not lag - never delivered.
//
// WHY.  keybd_event and mouse_event on Win32s are thin stubs over the 16-bit
// Windows 3.1 entry points, and on that platform those functions inject into the
// HARDWARE EVENT QUEUE, which only a real device driver may write to.  The Win95
// behaviour of synthesising into the system input stream does not exist here.
//
// This also explains why the mouse MOVEMENT fix worked: SetCursorPos bypasses
// the event queue entirely and sets the cursor position directly.
//
// THE REPLACEMENT.  Post window messages directly to the target window, which is
// how remote-control software of this era did it and which uses only Windows 3.0
// APIs:
//
//   keys   -> GetFocus()/GetActiveWindow(), then WM_KEYDOWN / WM_KEYUP, plus
//             WM_CHAR for printable characters (many 3.1 applications read
//             WM_CHAR only and ignore WM_KEYDOWN).
//   clicks -> WindowFromPoint(), then WM_LBUTTONDOWN / WM_LBUTTONUP etc. with
//             client-relative coordinates in lParam.
//
// KNOWN LIMITATION, stated plainly: synthesised messages bypass the driver, so
// an application that reads the physical keyboard or mouse state directly
// (GetAsyncKeyState, or a DOS-style polling loop) will not see them.  For normal
// Windows applications that process messages - which is essentially all of them -
// this works.  There is no better option on this platform.
// ==========================================================================

// Track the modifier state ourselves.
//
// GetAsyncKeyState and GetKeyState both report 0 for injected keys (see above),
// so the real system state can never reflect what we inject.  Anything that
// needs to know whether we are "holding" shift/ctrl/alt has to ask us.
static BOOL g_fakeShiftDown = FALSE;
static BOOL g_fakeCtrlDown  = FALSE;
static BOOL g_fakeAltDown   = FALSE;

static void Win32sTrackModifier(BYTE vkCode, BOOL down)
{
  switch (vkCode) {
  case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
    g_fakeShiftDown = down; break;
  case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
    g_fakeCtrlDown = down; break;
  case VK_MENU: case VK_LMENU: case VK_RMENU:
    g_fakeAltDown = down; break;
  }
}

// Post a key event as window messages.
//
// 'ch' is the character to send via WM_CHAR, or 0 for a non-character key.
static void Win32sPostKey(BYTE vkCode, BOOL down, BOOL extended, char ch)
{
  Win32sTrackModifier(vkCode, down);

  // Find the window that should receive it.  GetFocus() returns the focus window
  // only for the CALLING thread's queue on Win32; on Win32s there is a single
  // input queue, so it gives the system focus window - which is what we want.
  // Fall back to the active window, then the desktop.
  HWND target = GetFocus();
  if (target == NULL)
    target = GetActiveWindow();
  if (target == NULL)
    target = GetForegroundWindow();
  if (target == NULL) {
    static DWORD s_noTarget = 0;
    if ((s_noTarget++ % 100) == 0) {
      vnclog.Print(LL_INTERR,
        VNCLOG("no focus window for key vk=0x%02x [%d occurrences]\n"),
        (int)vkCode, (int)s_noTarget);
    }
    return;
  }

  // Build lParam: repeat count 1, scan code, extended flag, and for a key-up
  // the transition and previous-state bits.
  UINT scan = MapVirtualKey(vkCode, 0);
  LPARAM lp = 1;                              // repeat count
  lp |= ((LPARAM)(scan & 0xFF)) << 16;        // scan code
  if (extended)
    lp |= 0x01000000L;                        // KF_EXTENDED
  if (!down)
    lp |= 0xC0000000L;                        // KF_UP | KF_REPEAT (previous down)

  // ==================================================================
  // WM_KEYDOWN vs WM_CHAR - THE RULE, and how I got it wrong twice.
  //
  // ATTEMPT 1: post WM_KEYDOWN *and* WM_CHAR.
  //   Result: "press k, get kk".  The application's own message loop calls
  //   TranslateMessage(), which turns our WM_KEYDOWN into a WM_CHAR - so it
  //   received the character twice, once translated and once from us.
  //
  // ATTEMPT 2: post WM_KEYDOWN only, and let TranslateMessage produce the
  //            character.
  //   Result: single characters, but SHIFT STOPPED WORKING - everything came out
  //   lowercase.  TranslateMessage calls ToAscii(), which reads the PHYSICAL
  //   keyboard state to decide case.  Our injected Shift never touches that
  //   state (proven earlier: GetAsyncKeyState always reports 0 for injected
  //   keys), so ToAscii always sees Shift up.
  //
  // CORRECT: for a PRINTABLE character, post WM_CHAR ONLY, carrying the exact
  // character.  The RFB keysym already encodes case - 'K' is 0x4B and 'k' is
  // 0x6B are different keysyms - so the client has already told us precisely
  // which character it wants and no modifier state is needed to work it out.
  //
  // For a NON-PRINTABLE key (arrows, function keys, Enter, Backspace, Tab...)
  // post WM_KEYDOWN/WM_KEYUP, because those keys are meaningful as key events
  // and produce no character.
  //
  // This also removes the double-post entirely: exactly one message per event,
  // and no reliance on physical modifier state anywhere.
  // ==================================================================
  if (ch != 0) {
    // Printable character: WM_CHAR only, and only on the press.  A key-up
    // produces no character, and WM_KEYUP for a character key is not something
    // applications act on.
    if (down)
      PostMessage(target, WM_CHAR, (WPARAM)(unsigned char)ch, lp);
    return;
  }

  if (down) {
    PostMessage(target, WM_KEYDOWN, (WPARAM)vkCode, lp);
  } else {
    PostMessage(target, WM_KEYUP, (WPARAM)vkCode, lp);
  }
}

inline void doKeyboardEvent(BYTE vkCode, vncServer *server, DWORD flags) {
  if (server != NULL) {
    server->SetKeyboardCounter(1);
  }

  // WIN32S: keybd_event does not work here - post messages instead.
  //
  // 'flags' carries KEYEVENTF_KEYUP and KEYEVENTF_EXTENDEDKEY.  The character to
  // send with WM_CHAR is not known at this level, so this path sends key events
  // only; Keymapper::keyEvent calls Win32sPostKey directly for printable
  // characters so it can supply the character.
  if (vncService::IsWin32s()) {
    Win32sPostKey(vkCode,
                  (flags & KEYEVENTF_KEYUP) ? FALSE : TRUE,
                  (flags & KEYEVENTF_EXTENDEDKEY) ? TRUE : FALSE,
                  0);
    return;
  }

  // Win9x / NT: the normal path.  keybd_event works properly there.
  keybd_event(vkCode, MapVirtualKey(vkCode, 0), flags, 0);
}

// KeyStateModifier is a class which helps simplify generating a "fake" press
// or release of shift, ctrl, alt, etc.  An instance of the class is created
// for every key which may need to be pressed or released.  Then either press()
// or release() may be called to make sure that the corresponding key is in the
// right state.  The destructor of the class automatically reverts to the
// previous state.

class KeyStateModifier {
public:
  KeyStateModifier(int vkCode_, vncServer *server_, int flags_=0)
    : vkCode(vkCode_), flags(flags_), server(server_),
      pressed(false), released(false)
  {}
  void press() {
    if (!(GetAsyncKeyState(vkCode) & 0x8000)) {
      doKeyboardEvent(vkCode, server, flags);
      vnclog.Print(LL_INTINFO, "fake %d down\n", vkCode);
      pressed = true;
    }
  }
  void release() {
    if (GetAsyncKeyState(vkCode) & 0x8000) {
      doKeyboardEvent(vkCode, server, flags | KEYEVENTF_KEYUP);
      vnclog.Print(LL_INTINFO, "fake %d up\n", vkCode);
      released = true;
    }
  }
  ~KeyStateModifier() {
    if (pressed) {
      doKeyboardEvent(vkCode, server, flags | KEYEVENTF_KEYUP);
      vnclog.Print(LL_INTINFO, "fake %d up\n", vkCode);
    } else if (released) {
      doKeyboardEvent(vkCode, server, flags);
      vnclog.Print(LL_INTINFO, "fake %d down\n", vkCode);
    }
  }
  int vkCode;
  int flags;
  bool pressed;
  bool released;
private:
  vncServer *server;
};

// Keymapper - a single instance of this class is used to generate Windows key
// events.

class Keymapper {

public:
  // WIN32S NOTE ON THIS CONSTRUCTOR.
  //
  // "Keymapper key_mapper;" at the bottom of this file is a FILE-SCOPE object,
  // so this constructor runs BEFORE WinMain.  That is the pattern that killed
  // the viewer at startup (omni_thread's init_t), so it is worth being explicit
  // about why it is safe here:
  //
  //   * It only inserts into two std::map objects, which are our own
  //     hand-written containers (see map.h) - a linked list and operator new.
  //     No API calls, no TLS, no LoadLibrary, no window creation.
  //
  //   * MSVC 4.1's operator new returns 0 rather than throwing, and map's
  //     push-at-head insert tolerates that (it simply loses the entry).  A
  //     partially built keymap degrades to "some keys do not work", not a crash.
  //
  // Do not add anything to this constructor that needs Windows to be
  // initialised.
  Keymapper()
  {
    // "int i" compared against an unsigned sizeof expression: MSVC 4.1 warns
    // (C4018, signed/unsigned mismatch) at /W3, which this project uses.  Cast
    // once rather than leaving a warning in every build.
    int nEntries = (int)(sizeof(keymap) / sizeof(keymap_t));
    for (int i = 0; i < nEntries; i++) {
      vkMap[keymap[i].keysym] = keymap[i].vk;
      extendedMap[keymap[i].keysym] = keymap[i].extended;
    }
  }

  void keyEvent(CARD32 keysym, bool down, vncServer *server)
  {
    if ((keysym >= 32 && keysym <= 126) ||
        (keysym >= 160 && keysym <= 255))
    {
      // ordinary Latin-1 character

      // WIN32S: VkKeyScan exists in Windows 3.1 and behaves as documented.
      // Note that it maps according to the CURRENT keyboard layout, and Windows
      // 3.1 has exactly one - so a client with a different layout will produce
      // wrong characters for anything outside ASCII.  That is a limitation of
      // the platform, not a bug here: LoadKeyboardLayout does not exist.
      SHORT s = VkKeyScan((CHAR)keysym);
      if (s == -1) {
        vnclog.Print(LL_INTWARN, "ignoring unrecognised Latin-1 keysym %d\n",
                     keysym);
        return;
      }

      BYTE vkCode = LOBYTE(s);

      // ==============================================================
      // WIN32S: post the character directly and skip the modifier dance.
      //
      // KeyStateModifier below decides whether to fake a shift/ctrl/alt press by
      // reading GetAsyncKeyState.  On this platform that always returns 0 for
      // injected keys (proven by the diagnostic: asyncstate=0x0000 immediately
      // after a key-down), so press() always thinks the key is already up and
      // release() always thinks it is already down - the modifier state can never
      // be tracked through the system.
      //
      // It is also unnecessary here.  Win32sPostKey sends WM_CHAR with the ACTUAL
      // character - and the RFB keysym already encodes case ('K' 0x4B vs 'k'
      // 0x6B), so the client has told us exactly which character it wants.  No
      // modifier state is needed to work it out, and sending a synthetic shift
      // would only risk leaving it stuck.
      //
      // See the long note in Win32sPostKey about why WM_CHAR alone is correct
      // here and why letting TranslateMessage produce the character broke Shift.
      // ==============================================================
      if (vncService::IsWin32s()) {
        vnclog.Print(LL_INTINFO,
                     "latin-1 key (win32s): keysym %d(0x%x) vkCode 0x%x down %d\n",
                     keysym, keysym, vkCode, down);
        Win32sPostKey(vkCode, down ? TRUE : FALSE, FALSE, (char)keysym);
        return;
      }

      KeyStateModifier ctrl(VK_CONTROL, server);
      KeyStateModifier alt(VK_MENU, server);
      KeyStateModifier shift(VK_SHIFT, server);
      KeyStateModifier lshift(VK_LSHIFT, server);
      KeyStateModifier rshift(VK_RSHIFT, server);

      if (down) {
        BYTE modifierState = HIBYTE(s);
        if (modifierState & 2) ctrl.press();
        if (modifierState & 4) alt.press();
        if (modifierState & 1) {
          shift.press(); 
        } else {
          if (vncService::IsWin95()) {
            shift.release();
          } else {
            lshift.release();
            rshift.release();
          }
        }
      }
      vnclog.Print(LL_INTINFO,
                   "latin-1 key: keysym %d(0x%x) vkCode 0x%x down %d\n",
                   keysym, keysym, vkCode, down);

      doKeyboardEvent(vkCode, server, down ? 0 : KEYEVENTF_KEYUP);

    } else {

      // see if it's a recognised keyboard key, otherwise ignore it

      if (vkMap.find(keysym) == vkMap.end()) {
        vnclog.Print(LL_INTWARN, "ignoring unknown keysym %d\n",keysym);
        return;
      }
      BYTE vkCode = vkMap[keysym];
      DWORD flags = 0;
      if (extendedMap[keysym]) flags |= KEYEVENTF_EXTENDEDKEY;
      if (!down) flags |= KEYEVENTF_KEYUP;

      vnclog.Print(LL_INTINFO,
                  "keyboard key: keysym %d(0x%x) vkCode 0x%x ext %d down %d\n",
                   keysym, keysym, vkCode, extendedMap[keysym], down);

      // NOTE: this is NT-only and therefore never taken on Win32s (where
      // SimulateCtrlAltDel is a stub anyway - there is no Winlogon).  Left as-is;
      // the GetAsyncKeyState reads are harmless because IsWinNT() is false.
      if (down && (vkCode == VK_DELETE) &&
          ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) &&
          ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) &&
          vncService::IsWinNT())
      {
        vncService::SimulateCtrlAltDel();
        return;
      }

      if (vncService::IsWin95()) {
        switch (vkCode) {
        case VK_RSHIFT:   vkCode = VK_SHIFT;   break;
        case VK_RCONTROL: vkCode = VK_CONTROL; break;
        case VK_RMENU:    vkCode = VK_MENU;    break;
        }
      }

      doKeyboardEvent(vkCode, server, flags);
    }
  }

private:
  std::map<CARD32,CARD8> vkMap;
  std::map<CARD32,bool> extendedMap;
} key_mapper;

void vncKeymap::keyEvent(CARD32 keysym, bool down, vncServer *server)
{
  key_mapper.keyEvent(keysym, down, server);
}



void
SetShiftState(BYTE key, BOOL down)
{
	BOOL keystate = (GetAsyncKeyState(key) & 0x8000) != 0;

	// This routine sets the specified key to the desired value (up or down)
	if ((keystate && down) || ((!keystate) && (!down)))
		return;

	vnclog.Print(LL_INTINFO,
		VNCLOG("setshiftstate %d - (%s->%s)\n"),
		key, keystate ? "down" : "up",
		down ? "down" : "up");

	// Now send a key event to set the key to the new value
	doKeyboardEvent(key, NULL, down ? 0 : KEYEVENTF_KEYUP);
	keystate = (GetAsyncKeyState(key) & 0x8000) != 0;

	vnclog.Print(LL_INTINFO,
		VNCLOG("new state %d (%s)\n"),
		key, keystate ? "down" : "up");
}

void
vncKeymap::ClearShiftKeys()
{
	if (vncService::IsWinNT())
	{
		// On NT, clear both sets of keys

		// LEFT
		SetShiftState(VK_LSHIFT, FALSE);
		SetShiftState(VK_LCONTROL, FALSE);
		SetShiftState(VK_LMENU, FALSE);

		// RIGHT
		SetShiftState(VK_RSHIFT, FALSE);
		SetShiftState(VK_RCONTROL, FALSE);
		SetShiftState(VK_RMENU, FALSE);
	}
	else
	{
		// Otherwise, we can't distinguish the keys anyway...

		// Clear the shift key states
		SetShiftState(VK_SHIFT, FALSE);
		SetShiftState(VK_CONTROL, FALSE);
		SetShiftState(VK_MENU, FALSE);
	}
}
