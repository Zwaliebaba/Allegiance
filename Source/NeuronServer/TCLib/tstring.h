#ifndef __tstring_h__
#define __tstring_h__

#pragma once

/////////////////////////////////////////////////////////////////////////////

#pragma warning(disable: 4786)
#include <functional>
#include <string>
#include <string.h>
#include <tchar.h>
#include <comutil.h>


/////////////////////////////////////////////////////////////////////////////
// Types

typedef std::basic_string<TCHAR> tstring;


/////////////////////////////////////////////////////////////////////////////
// ci_less
struct ci_less
{
  bool operator()(const tstring& x, const tstring& y) const
  {
    return 0 > _tcsicmp(x.c_str(), y.c_str());
  }
};
struct ci_greater { bool operator()(const tstring& x, const tstring& y) const { return !ci_less()(x, y); } };

/////////////////////////////////////////////////////////////////////////////
// ci_less_bstr_t
struct ci_less_bstr_t
{
  bool operator()(const _bstr_t& x, const _bstr_t& y) const
  {
	  if (!x.length())
		  return y.length() ? true : false;
	  if (!y.length())
		  return false;
    int n = _wcsnicmp(x, y, min(x.length(), y.length()));
    if (0 > n)
      return true;
    if (0 == n)
      return x.length() < y.length();
    else
      return false;
  }
};
struct ci_greater_bstr_t { bool operator()(const _bstr_t& x, const _bstr_t& y) const { return !ci_less_bstr_t()(x, y); } };


/////////////////////////////////////////////////////////////////////////////

#endif // !__tstring_h__
