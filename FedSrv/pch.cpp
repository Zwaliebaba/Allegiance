/*-------------------------------------------------------------------------
 * pch.cpp
 * 
 * Precompiled header for fedsrv
 * 
 * Owner: 
 * 
 * Copyright 1986-1998 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/

#include "pch.h"

// ATL 3 needed its implementation compiled into one translation unit
// (atlimpl.cpp, statreg.cpp). Current ATL is header-only plus atls.lib, so
// those includes are gone; statreg.h alone carries the static registrar.
#pragma push_macro("new")
#undef new
#include <atlbase.h>

#ifdef _ATL_STATIC_REGISTRY
#include <statreg.h>
#endif
#pragma pop_macro("new")

#include <../TCAtl/UserAcct.cpp>
