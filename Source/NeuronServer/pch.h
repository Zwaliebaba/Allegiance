//////////////////////////////////////////////////////////////////////////////
//
// NeuronServer: the engine's server half (running as a Windows service or a
// console program, account rights, the server's reader/writer lock), used by
// the server. Its precompiled header (the headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers

// __MODULE__ is used by ZAssert
#define __MODULE__ "NeuronServer"

#include <string>
#include <vector>
#include <stdio.h>
#include <windows.h>
#include <tchar.h>
#include <process.h> // for _beginthreadex and _endthreadex
#include <mmsystem.h>  // timeGetTime, for ZLib.h (WIN32_LEAN_AND_MEAN leaves it out)

#include "ZLib.h"

#endif
