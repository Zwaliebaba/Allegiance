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

#define _ATL_STATIC_REGISTRY

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

// GameCommon
#include <igc.h>

#include "resource.h"
#include "gameparamdata.h"
#include "serverevents.h"

// This also includes <atlbase.h> and <atlcom.h>
#include "allsrvmodule.h"

// NeuronServer
#include <TCLib/TCLib.h>
#include <TCLib/AutoHandle.h>
#include <TCLib/AutoCriticalSection.h>
#include <TCAtl/TCAtl.h>
#include <TCAtl/UserAcct.h>
#include <TCLib/TCThread.h>
#include "SWMRG.h"

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

/////////////////////////////////////////////////////////////////////////////
// This is included very last since it conflicts with <atlwin.h>, included
// indirectly by <TCAtl.h>.
//
#include <windowsx.h> // For GlobalAllocPtr and GlobalFreePtr

#endif // _FEDSRV_PCH_
