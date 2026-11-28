//////////////////////////////////////////////////////////////////////////////
//
// The client executable, Allegiance.exe: its entry point. WinMain calls
// Win32Main (NeuronCore), which runs the application object: the game's is
// g_trekImpl in GameApp (trekapp.cpp), which registers itself when it is
// constructed. The client links GameApp with /WHOLEARCHIVE so the linker
// keeps it although nothing here refers to it.
//
//////////////////////////////////////////////////////////////////////////////

#define __MODULE__ "Allegiance"

#include <windows.h>
#include <crtdbg.h>

#include "zlib.h"
#include "main.h"
