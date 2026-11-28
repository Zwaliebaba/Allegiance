//////////////////////////////////////////////////////////////////////////////
//
// GameApp: the game client (WinTrek): its screens, combat view, input and
// sound, the client's side of the protocol (ClientLib) and the training
// missions. Used by the client executable. Its precompiled header (the
// headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "GameApp"

//
// C Headers
//

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

//
// Windows headers
//

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <commctrl.h>
#include <tchar.h>

//
// STL headers
//

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <vector>

// Need to include zlib before any DirectX headers
#define ID_APP_ICON "winTrekIcon"
#include "zlib.h"

//
// DX headers
//

#include <dinput.h>

//
// NeuronClient, NeuronCore and GameCommon
//

#include "engine.h"
#include "effect.h"
#include "help.h"
#include "utility.h"
#include "igc.h"
#include "messagesAll.h"
#include "messages.h"
#include "soundengine.h"

//
// GameApp
//

#include "trekinput.h"
#include "resource.h"
#include "messagesLC.h"
#include "clintlib.h"
#include "screen.h"
#include "wintrek.h"
#include "treki.h"
#include "indicator.h"
#include "radarimage.h"
#include "trekctrls.h"
#include "trekmdl.h"
#include "loadout.h"
#include "wintrekp.h"
#include "artwork.h"
#include "logon.h"
#include "passworddialog.h"
#include "treksound.h"
#include "GameTypes.h"

#endif
