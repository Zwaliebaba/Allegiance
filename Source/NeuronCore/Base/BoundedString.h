#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// Copying and formatting into fixed-size char buffers, bounded by the buffer
//
// The replacements for strcpy, strcat, sprintf and wsprintf. Each takes the
// destination as an array, so the size it writes within is the array's own
// and cannot be misstated; a destination that is only a pointer is a compile
// error, and takes the explicit-size form instead.
//
// What does not fit is cut off, and the result is always terminated. That is
// the right failure here: much of what these copy (names, chat, game
// settings) comes off the network, and crashing on a long one, as strcpy_s
// does, would let any client stop the server.
//
// Copy fills the rest of the buffer with zeros, as strncpy did: the buffers
// are often fields of a record that goes on the wire whole, and must not
// carry what was in memory before.
//
//////////////////////////////////////////////////////////////////////////////

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace Bounded
{

// _src, cut to fit, then zeros to the end of the buffer. Returns the length copied.
inline size_t Copy(char* _dst, size_t _size, const char* _src)
{
  if (_size == 0)
  {
    return 0;
  }

  size_t length = _src ? strnlen(_src, _size - 1) : 0;
  memcpy(_dst, _src ? _src : "", length);
  memset(_dst + length, 0, _size - length);
  return length;
}

template <size_t Size> size_t Copy(char (&_dst)[Size], const char* _src)
{
  return Copy(_dst, Size, _src);
}

// _src appended to the string in the buffer, cut to fit. Returns the new length.
inline size_t Append(char* _dst, size_t _size, const char* _src)
{
  if (_size == 0)
  {
    return 0;
  }

  size_t length = strnlen(_dst, _size - 1);
  return length + Copy(_dst + length, _size - length, _src);
}

template <size_t Size> size_t Append(char (&_dst)[Size], const char* _src)
{
  return Append(_dst, Size, _src);
}

// printf formatting, cut to fit. Returns the length written, which is less than
// the buffer's size, so a caller that appends at dst + length stays inside it.
inline int FormatList(char* _dst, size_t _size, _Printf_format_string_ const char* _format, va_list _args)
{
  if (_size == 0)
  {
    return 0;
  }

  int wanted = vsnprintf(_dst, _size, _format, _args);
  if (wanted < 0)
  {
    _dst[0] = '\0';
    return 0;
  }

  return wanted < static_cast<int>(_size) ? wanted : static_cast<int>(_size) - 1;
}

inline int Format(char* _dst, size_t _size, _Printf_format_string_ const char* _format, ...)
{
  va_list args;
  va_start(args, _format);
  int length = FormatList(_dst, _size, _format, args);
  va_end(args);
  return length;
}

template <size_t Size> int Format(char (&_dst)[Size], _Printf_format_string_ const char* _format, ...)
{
  va_list args;
  va_start(args, _format);
  int length = FormatList(_dst, Size, _format, args);
  va_end(args);
  return length;
}

} // namespace Bounded
