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
#include "ZLib.h"

//
// DX headers
//

#include <dinput.h>

//
// NeuronClient, NeuronCore and GameCommon
//

#include "Engine.h"
#include "Effect.h"
#include "Help.h"
#include "Utility.h"
#include "Igc.h"
#include "MessagesAll.h"
#include "Messages.h"
#include "SoundEngine.h"

//
// GameApp
//

#include "TrekInput.h"
#include "resource.h"
#include "MessagesLc.h"
#include "ClintLib.h"
#include "Screen.h"
#include "WinTrek.h"
#include "Treki.h"
#include "Indicator.h"
#include "RadarImage.h"
#include "TrekCtrls.h"
#include "TrekMdl.h"
#include "Loadout.h"
#include "WinTrekP.h"
#include "Artwork.h"
#include "Logon.h"
#include "PasswordDialog.h"
#include "TrekSound.h"
#include "GameTypes.h"

#endif
