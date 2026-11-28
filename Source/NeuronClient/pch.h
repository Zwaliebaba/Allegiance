//////////////////////////////////////////////////////////////////////////////
//
// NeuronClient: the engine's client half (graphics, effects, sound, the
// application frame), used by the client and the tools. Its precompiled
// header (the headers its sources use).
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _PCH_H_
#define _PCH_H_

// __MODULE__ is used by ZAssert
#define __MODULE__ "NeuronClient"

#include <list>
#include <set>
#include <map>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <stdio.h>
#include <malloc.h>

#include <windows.h>
#include <xaudio2.h>

#include "guids.h"
#include "zlib.h"
#include "engine.h"
#include "enginep.h"
#include "effect.h"
#include "efart.h"

#endif
