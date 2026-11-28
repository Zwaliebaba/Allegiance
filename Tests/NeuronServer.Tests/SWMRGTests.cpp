#include "pch.h"
#include "SWMRG.H"

//////////////////////////////////////////////////////////////////////////////
//
// The server's single-writer, multiple-reader guard (Base/SWMRG.H), which
// guards the server's global state (g.swmrg). The waits that must fail are
// made from another thread: the writer's mutex is recursive, so the thread
// holding it would get it again.
//
//////////////////////////////////////////////////////////////////////////////

namespace SWMRGTests
{
    // Run f on another thread and return what it returns
    template <class F>
    static DWORD OnOtherThread(F f)
    {
        DWORD result = 0;
        std::thread([&] { result = f(); }).join();
        return result;
    }

    TEST_CLASS(Guard)
    {
        SWMRG m_guard;

    public:
        TEST_METHOD_INITIALIZE(Create)
        {
            Assert::IsTrue(FSWMRGInitialize(&m_guard, NULL));
        }

        TEST_METHOD_CLEANUP(Delete)
        {
            SWMRGDelete(&m_guard);
        }

        TEST_METHOD(ReadersShare)
        {
            Assert::AreEqual(WAIT_OBJECT_0, SWMRGWaitToRead(&m_guard, 0));
            Assert::AreEqual(WAIT_OBJECT_0, OnOtherThread([&] {
                DWORD dw = SWMRGWaitToRead(&m_guard, 0);
                if (dw == WAIT_OBJECT_0)
                    SWMRGDoneReading(&m_guard);
                return dw;
            }));
            SWMRGDoneReading(&m_guard);
        }

        TEST_METHOD(WriterExcludesReaders)
        {
            Assert::AreEqual(WAIT_OBJECT_0, SWMRGWaitToWrite(&m_guard, 0));
            Assert::AreEqual((DWORD)WAIT_TIMEOUT, OnOtherThread([&] { return SWMRGWaitToRead(&m_guard, 0); }));
            SWMRGDoneWriting(&m_guard);

            Assert::AreEqual(WAIT_OBJECT_0, OnOtherThread([&] {
                DWORD dw = SWMRGWaitToRead(&m_guard, 0);
                if (dw == WAIT_OBJECT_0)
                    SWMRGDoneReading(&m_guard);
                return dw;
            }));
        }

        TEST_METHOD(ReaderExcludesWriters)
        {
            Assert::AreEqual(WAIT_OBJECT_0, SWMRGWaitToRead(&m_guard, 0));
            Assert::AreEqual((DWORD)WAIT_TIMEOUT, OnOtherThread([&] { return SWMRGWaitToWrite(&m_guard, 0); }));
            SWMRGDoneReading(&m_guard);

            // The last reader out lets the writer in
            Assert::AreEqual(WAIT_OBJECT_0, OnOtherThread([&] {
                DWORD dw = SWMRGWaitToWrite(&m_guard, 0);
                if (dw == WAIT_OBJECT_0)
                    SWMRGDoneWriting(&m_guard);
                return dw;
            }));
        }

        TEST_METHOD(WriterExcludesWriters)
        {
            Assert::AreEqual(WAIT_OBJECT_0, SWMRGWaitToWrite(&m_guard, 0));
            Assert::AreEqual((DWORD)WAIT_TIMEOUT, OnOtherThread([&] { return SWMRGWaitToWrite(&m_guard, 0); }));
            SWMRGDoneWriting(&m_guard);
        }

        TEST_METHOD(NamesTooLongAreRefused)
        {
            // The object names are a prefix plus the name, in 100 characters
            SWMRG guard;
            std::string strName(100, 'x');
            Assert::IsFalse(FSWMRGInitialize(&guard, strName.c_str()));
        }
    };
}
