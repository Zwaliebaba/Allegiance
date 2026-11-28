//////////////////////////////////////////////////////////////////////////////
//
// GameLogic: the game server (FedSrv): missions, sides, clusters and ships
// on the server, the lobby connection, the service. Used by the server
// executable. Its sources' precompiled header, and the header the server
// executable includes.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _FEDSRV_PCH_
#define _FEDSRV_PCH_

// __MODULE__ is used by ZAssert
#define __MODULE__ "GameLogic"

// Standard C Includes
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

// Standard C++ Includes
#include <algorithm>
#include <map>
#include <set>
#include <vector>

// Windows Includes
#include <windows.h>
#include <commctrl.h>

// NeuronCore
#include <ZLib.h>
#include <Utility.h>
#include "AutoCriticalSection.h"   // ZAutoCriticalSection

// GameCommon
#include <Igc.h>

#include "resource.h"
#include "GameParamData.h"
#include "ServerEvents.h"

// NeuronServer
#include "ServiceHost.h"
#include "Swmrg.h"

#include "AllSrvModule.h"

// GameCommon: the protocol
#include <MessagesAll.h>
#include <Messages.h>
#include <MessagesLs.h>
#include <TrainingServer.h>

#include "Point.h"
#include "Counters.h"
#include "FsSide.h"
#include "FsCluster.h"
#include "FsMission.h"
#include "FedSrv.h"

extern Global g;

#include "FsShip.h"
#include "DbgUtil.h"
#include "ServerSettings.h"

#include <windowsx.h> // For GlobalAllocPtr and GlobalFreePtr

#endif // _FEDSRV_PCH_
