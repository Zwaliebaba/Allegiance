//////////////////////////////////////////////////////////////////////////////
//
// NeuronServer: the engine's server half (TCLib's threads and COM helpers,
// TCAtl's user accounts, the server's locks and bit arrays), used by the
// server. Its precompiled header (the headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#define _WIN32_DCOM

// __MODULE__ is used by ZAssert
#define __MODULE__ "NeuronServer"

#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <crtdbg.h>
#include <windows.h>
#include <tchar.h>
#include <malloc.h>
#include <comcat.h>
#include <comdef.h>
#include <commctrl.h>
#include <atlconv.h>
#include <process.h> // for _beginthreadex and _endthreadex
#include <mmsystem.h>

#include "zlib.h"

#endif
