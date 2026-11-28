//////////////////////////////////////////////////////////////////////////////
//
// GameCommon: the game's rules and objects (IGC), the client-server protocol
// and the version resource, used by the client and the server. Its
// precompiled header (the headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "GameCommon"

#include <windows.h>
#include <mmsystem.h>

#include "ZLib.h"
#include "Utility.h"
#include "Igc.h"
#include "Private.h"

#endif
