# TightVNC 1.3.10 Port for All Windows Architectures
This is a specialized, unofficial, comprehensive fork of TightVNC 1.3.10 built natively for virtually every Windows processor architecture to ever exist. This project extends the ultra-lightweight, classic 1.3.x branch to run smoothly on modern 64-bit platforms, efficient ARM hardware, and legacy retro-computing environments.

## Supported Windows Architectures
This project provides native binaries compiled explicitly for the following target environments:
## Modern Platforms

| Architecture | Platform Target | Common Use Cases / Operating Systems |
|---|---|---|
| x64 / amd64 | All 64-bit Windows versions | Mainstream desktops, laptops, and servers. |
| ARM64 | Windows 10+ on ARM64 devices | Qualcomm Snapdragon-powered laptops and tablets. |
| ARM32 | Windows RT 8.1 / Windows 10 ARM Build 15035 / ARM64 Windows versions with the ARM32 compatibility layer | Older ARM hardware and ARM32 boards. |

## Legacy & Vintage NT Platforms

| Architecture | Platform Target | Common Use Cases / Operating Systems |
|---|---|---|
| AXP64 (Alpha64) | DEC Alpha 64-bit Windows NT | Pre-beta Windows 2000 64-bit builds for Alpha. |
| AXP (Alpha 32-bit) | DEC Alpha Windows NT Systems | Windows NT 3.51 / 4.0 / 2000 betas on Digital workstations. |
| IA64 (Itanium) | Intel Itanium Systems | Windows XP 64-Bit Edition / Windows Server 2003/2008 (may be broken on the latter 2). |
| PowerPC | IBM / Motorola / Mac & Wii Systems | Legacy Windows NT 3.51 / 4.0 PowerPC editions. |
| MIPS | MIPS R4000 and higher | Legacy Windows NT 3.51 / 4.0 MIPS editions. |

## Features & Enhancements
* Native Execution: No emulation layers or performance overhead; built explicitly for each instruction set for maximum performance on each architecture.
* Ultra-Lightweight Codebase: Preserves the fast, efficient performance characteristic of the classic 1.3.10 branch.
* No Extra Dependencies: Self-contained executable files that do not require external runtime installations.

## Download & Quick Start
1. Head over to the Releases tab.
2. Select the directory matching your machine's system type (e.g., tightvnc-1.3.10_arm64.zip).
3. Extract the archive folder.
4. Launch WinVNC.exe to host a server or vncviewer.exe to connect to a remote session.

> Looking for the official, untouched upstream source?
> If you require standard 32-bit legacy x86 setups natively supported by the original creators, you can grab them from the [Official TightVNC Archive](https://www.tightvnc.com/download-old.php).

## Notes
I have every port separated into separate branches because that is how I originally started it and to track changes on each port. I do eventually intend on merging most of the branches, while more than likely keeping the x64 (haven't decided on that yet) and PPC/MIPS branches separated from the main branch, making it either an eventual total of 2 or 3 branches, and I may eventually come up with a name of this fork that isn't TightVNC.

### Toolchains used:
* PowerPC/MIPS: MSVC 4.1 with the Windows NT 4.0 SDK and the ActiveX Software Development Kit for wininet.h for the OS architecture corresponding versions
* AXP: Visual Studio 6.0 for DEC Alpha Platforms with the October 1999 Microsoft Platform SDK
* AXP64: The AXP64 cross-compiler from the October 1999 Platform SDK, which is a very early version of the MSVC 7.0 compiler
* IA64: MSVC 7.1 cross-compiler from the February 2003 Platform SDK
* AMD64: Visual Studio 2005
* ARM: Visual Studio 2015
* ARM64: Visual Studio 2022
* Future x86 builds when I make separate code changes from the official source code that affects x86 will be done in Visual Studio 6.0

## Feedback & Issue Reporting
Since this is an initial port adapting legacy code for multiple architectures, you may run into edge cases. If you encounter bugs, architecture-specific crashes, or display quirks:
1. Open a ticket through our GitHub Issue Tracker.
2. Please mention your exact Windows device model and target architecture (e.g., Surface Pro 11 running Windows 11 ARM64). Also please mention if you are running from an emulator/Virtual Machine.

PRs are also welcome, and please send all PRs to the specific branch which you are targeting, not main unless it's an overall fix.

## License
This port maintains the original project formatting and licensing constraints. It is distributed under the standard GNU General Public License (GPLv2).

## Credits & Copyright
* Original TightVNC Source: The core software and original code are Copyright © Constantin Kaplinsky and the TightVNC Team.
* VNC Core Engine: Based on the original VNC software developed by AT&T Laboratories Cambridge.
* Architecture Porting: This multi-architecture Windows compilation, project modernization, and platform-specific code adaptations were implemented by GenSayer.
