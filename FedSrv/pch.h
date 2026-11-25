/*-------------------------------------------------------------------------
 * pch.h
 * 
 * Headers for fedsrv
 * 
 * Owner: 
 * 
 * Copyright 1986-1998 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/
#ifndef _FEDSRV_PCH_
#define _FEDSRV_PCH_

#define _ATL_STATIC_REGISTRY

// browse info overflow warnings for STL objects
#pragma warning(disable : 4786)

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
#include <sqlext.h>


// ZLib Includes
  #define __MODULE__ "AllSrv32"
#include <guids.h>
#include <zlib.h>
                     
#include "resource.h"
#include <utility.h>
#include <igc.h>

#include "gameparamdata.h"
#include "serverevents.h"

//#include <atlbase.h>
//extern CServiceModule _Module;
//#include <atlcom.h>

// <ZoneAuthDecrypt.h> (Zone SDK, not in the source release) was included here; nothing in FedSrv uses it.

// This also includes <atlbase.h> and <atlcom.h>
#include "allsrvmodule.h"

#include <TCLib.h>
#include <..\TCLib\AutoHandle.h>
#include <..\TCLib\AutoCriticalSection.h>

#include <TCAtl.h>
#include <..\TCAtl\UserAcct.h>
#include <..\TCLib\TCThread.h>



#include <messagesAll.h>
#include <Messages.h>
#include <MessagesLS.h>

#include "SWMRG.h"
#include "BitArray.h"
#include "point.h"
//#include "srvdbg.h"

//#include "config.h"
#include "counters.h"
#include "srvqueries.h" // and allegdb.h

#include "fsside.h"
#include "fscluster.h"
#include "fsmission.h"
#include "fedsrv.h"
extern Global g;
#include "fsship.h"

#include "dbgutil.h"


#include "serversettings.h"
#include <trainingserver.h>


/////////////////////////////////////////////////////////////////////////////
// This is included very last since it conflicts with <atlwin.h>, included
// indirectly by <TCAtl.h>.
//
#include <windowsx.h> // For GlobalAllocPtr and GlobalFreePtr


/////////////////////////////////////////////////////////////////////////////

#endif // _FEDSRV_PCH_

