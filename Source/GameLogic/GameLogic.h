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
#include <zlib.h>
#include <utility.h>
#include "AutoCriticalSection.h"   // ZAutoCriticalSection

// GameCommon
#include <igc.h>

#include "resource.h"
#include "gameparamdata.h"
#include "serverevents.h"

// NeuronServer
#include "servicehost.h"
#include "SWMRG.h"

#include "allsrvmodule.h"

// GameCommon: the protocol
#include <messagesAll.h>
#include <Messages.h>
#include <messagesLS.h>
#include <trainingserver.h>

#include "point.h"
#include "counters.h"
#include "fsside.h"
#include "fscluster.h"
#include "fsmission.h"
#include "fedsrv.h"

extern Global g;

#include "fsship.h"
#include "dbgutil.h"
#include "serversettings.h"

#include <windowsx.h> // For GlobalAllocPtr and GlobalFreePtr

#endif // _FEDSRV_PCH_
