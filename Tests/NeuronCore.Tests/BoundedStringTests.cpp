#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// Bounded::Copy, Append and Format (NeuronCore/Base/BoundedString.h): what
// does not fit is cut off, the result is always terminated, Copy leaves no
// stale bytes behind it, and Format's length is safe to append at.
//
//////////////////////////////////////////////////////////////////////////////

// A destination that is only a pointer must take the explicit-size form
template <class T> concept CopiesInto = requires(T _dst) { Bounded::Copy(_dst, ""); };

static_assert(CopiesInto<char (&)[8]>);
static_assert(!CopiesInto<char*>);

namespace BoundedStringTests
{
  TEST_CLASS(Copy)
  {
  public:
    TEST_METHOD(CopiesWhatFits)
    {
      char buffer[8];
      Assert::AreEqual<size_t>(3, Bounded::Copy(buffer, "abc"));
      AreEqualStr("abc", buffer);
    }

    TEST_METHOD(CutsOffWhatDoesNot)
    {
      char buffer[4];
      Assert::AreEqual<size_t>(3, Bounded::Copy(buffer, "abcdef"));
      AreEqualStr("abc", buffer);
    }

    TEST_METHOD(FillsTheRestWithZeros)
    {
      char buffer[8];
      memset(buffer, 'x', sizeof(buffer));
      Bounded::Copy(buffer, "ab");
      for (size_t index = 2; index < sizeof(buffer); index++)
      {
        Assert::AreEqual('\0', buffer[index]);
      }
    }

    TEST_METHOD(NullIsEmpty)
    {
      char buffer[4] = "xyz";
      Assert::AreEqual<size_t>(0, Bounded::Copy(buffer, nullptr));
      AreEqualStr("", buffer);
    }

    TEST_METHOD(ExplicitSize)
    {
      char buffer[8];
      char* pointer = buffer;
      Bounded::Copy(pointer, 3, "abcdef");
      AreEqualStr("ab", buffer);
    }
  };

  TEST_CLASS(Append)
  {
  public:
    TEST_METHOD(AppendsWhatFits)
    {
      char buffer[8] = "ab";
      Assert::AreEqual<size_t>(4, Bounded::Append(buffer, "cd"));
      AreEqualStr("abcd", buffer);
    }

    TEST_METHOD(CutsOffAtTheEnd)
    {
      char buffer[6] = "abc";
      Assert::AreEqual<size_t>(5, Bounded::Append(buffer, "defgh"));
      AreEqualStr("abcde", buffer);
    }

    TEST_METHOD(FullBufferStaysFull)
    {
      char buffer[4] = "abc";
      Assert::AreEqual<size_t>(3, Bounded::Append(buffer, "d"));
      AreEqualStr("abc", buffer);
    }
  };

  TEST_CLASS(Format)
  {
  public:
    TEST_METHOD(FormatsWhatFits)
    {
      char buffer[16];
      Assert::AreEqual(6, Bounded::Format(buffer, "%s=%d", "abc", 42));
      AreEqualStr("abc=42", buffer);
    }

    TEST_METHOD(CutsOffAndTerminates)
    {
      char buffer[6];
      Assert::AreEqual(5, Bounded::Format(buffer, "%s", "abcdefgh"));
      AreEqualStr("abcde", buffer);
    }

    TEST_METHOD(TheLengthIsSafeToAppendAt)
    {
      // The server appends at buffer + length; a cut-off first part must not
      // put the second outside the buffer
      char buffer[8];
      int length = Bounded::Format(buffer, "%s", "abcdefghijk");
      Assert::IsTrue(length < (int)sizeof(buffer));
      Bounded::Format(buffer + length, sizeof(buffer) - length, "%s", "zz");
      AreEqualStr("abcdefg", buffer);
    }
  };
}
