//////////////////////////////////////////////////////////////////////////////
//
// NeuronCore: the engine's base, used by the client and the server. Its
// precompiled header (the headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "NeuronCore"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <malloc.h>

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <urlmon.h>

#include "ZLib.h"
#include "Utility.h"

#endif
