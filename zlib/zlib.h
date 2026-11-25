#ifndef _zlib_H_
#define _zlib_H_

//////////////////////////////////////////////////////////////////////////////
//
// zlib.H
//
//////////////////////////////////////////////////////////////////////////////

// warning C4355: 'this' : used in base member initializer list
#pragma warning(disable:4355)

    #define USEASM
    #define FLOATASM

// VS.Net 2003 port: typename is required for template
#if _MSC_VER >= 1300
	#define VSNET_TNFIX typename
#else
	#define VSNET_TNFIX
#endif
//
// Required CRT headers
//

#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "math.h"
#include "time.h"
#include "malloc.h"

// The float overloads of abs, floor, pow, sin, ... come from <cmath> in the
// modern CRT. zmath.h used to define them itself (compiled out since VS.NET
// 2003); without <cmath>, abs(float) is ambiguous between the integer overloads.
#include <cmath>
#include <cstdlib>
#include <utility>   // std::swap, see zmath.h


//
// Required Win32 headers
//

#include "windows.h"
#include "wininet.h"

//
// DirectX types. Only the legacy Direct3D vertex and colour types
// (D3DCOLOR, D3DVECTOR, D3DVERTEX, ...) are still used, as plain structs;
// rendering goes through Direct3D 11 in the engine.
//

#define DIRECT3D_VERSION   0x0700

#include <d3dtypes.h>

//
// Library Headers
//

#include "zassert.h"
#include "alloc.h"
#include "mask.h"
#include "tref.h"
#include "cast.h"
#include "zmath.h"
#include "zadapt.h"

#include "tcompare.h"
#include "tarray.h"
#include "tvector.h"
#include "tstack.h"
#include "tlist.h"
#include "tmap.h"

#include "comobj.h"

//
// Types
//

#include "ztime.h"
#include "zstring.h"
#include "point.h"
#include "rect.h"
#include "vector.h"
#include "vertex.h"
#include "orientation.h"
#include "matrix.h"
#include "quaternion.h"
#include "color.h"


//
// Interfaces
//

#include "event.h"
#include "input.h"
#include "genericlist.h"
#include "FTPSession.h"

//
// Win32 Wrapper Classes
//

#include "base.h"
#include "window.h"
#include "win32app.h"
#include "zsettings.h"
#include "VersionInfo.h"

//
// Parser
//

#include "token.h"

#endif
