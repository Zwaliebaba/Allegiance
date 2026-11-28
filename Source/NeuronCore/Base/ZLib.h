#ifndef _zlib_H_
#define _zlib_H_

//////////////////////////////////////////////////////////////////////////////
//
// ZLib.h
//
//////////////////////////////////////////////////////////////////////////////

// warning C4355: 'this' : used in base member initializer list
#pragma warning(disable:4355)

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
// modern CRT. ZMath.h used to define them itself (compiled out since VS.NET
// 2003); without <cmath>, abs(float) is ambiguous between the integer overloads.
#include <cmath>
#include <cstdlib>
#include <utility>   // std::swap, see ZMath.h
#include <memory>    // std::construct_at (engine/Mdl.cpp); before Alloc.h #defines new
#include <string>    // ZString is a std::string


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

#include "ZAssert.h"
#include "Alloc.h"
#include "Mask.h"
#include "TRef.h"
#include "Cast.h"
#include "ZMath.h"
#include "ZAdapt.h"

#include <array>
#include <vector>
#include <algorithm>

#include "TCompare.h"
#include "TList.h"
#include "TMap.h"

#include "ComObj.h"

//
// Types
//

#include "ZTime.h"
#include "ZString.h"
#include "Point.h"
#include "Rect.h"
#include "Vector.h"
#include "Vertex.h"
#include "Orientation.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Color.h"


//
// Interfaces
//

#include "Event.h"
#include "Input.h"
#include "GenericList.h"
#include "FtpSession.h"

//
// Win32 Wrapper Classes
//

#include "Base.h"
#include "Window.h"
#include "Win32App.h"
#include "ZSettings.h"
#include "VersionInfo.h"

//
// Parser
//

#include "Token.h"

#endif
