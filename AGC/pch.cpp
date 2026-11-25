/////////////////////////////////////////////////////////////////////////////
// pch.cpp : source file that includes just the standard includes
//  pch.pch will be the pre-compiled header
//  pch.obj will contain the pre-compiled type information

#include "pch.h"

// atlimpl.cpp / statreg.cpp: ATL 3 implementation files; current ATL is
// header-only plus atls.lib, and statreg.h carries the static registrar.
#include <TCAtl.cpp>
#include <..\TCAtl\PropertyClass.cpp>
#include <..\TCAtl\TCNullStreamImpl.cpp>

#ifdef _ATL_STATIC_REGISTRY
  #include <statreg.h>
#endif

