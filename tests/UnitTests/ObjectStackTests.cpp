#include "pch.h"
#include "objectstack.h"

//////////////////////////////////////////////////////////////////////////////
//
// The MDL evaluator's operand stack (engine/objectstack.h). The factories
// keep plain pointers to what they pop, so a popped object must stay alive
// until its slot is reused: the use-after-free behind the binary MDL reload
// crash (MODERNIZATION.md, "Library cleanup").
//
//////////////////////////////////////////////////////////////////////////////

namespace ObjectStackTests
{
    class Counted : public IObject {
    public:
        static int s_live;
        int        m_id;

        Counted(int id) : m_id(id) { s_live++; }
        ~Counted() { s_live--; }

        static int Id(IObject* pobject)
        {
            Counted* pcounted = dynamic_cast<Counted*>(pobject);
            Assert::IsNotNull(pcounted);
            return pcounted->m_id;
        }
    };

    int Counted::s_live = 0;

    TEST_CLASS(Stack)
    {
    public:
        TEST_METHOD_INITIALIZE(Reset)
        {
            Counted::s_live = 0;
        }

        TEST_METHOD(LastInFirstOut)
        {
            ObjectStack stack;
            Assert::AreEqual(0, stack.GetCount());

            stack.Push(new Counted(1));
            stack.Push(new Counted(2));
            stack.Push(new Counted(3));
            Assert::AreEqual(3, stack.GetCount());

            Assert::AreEqual(3, Counted::Id(stack.Pop()));
            Assert::AreEqual(2, Counted::Id(stack.Pop()));
            Assert::AreEqual(1, Counted::Id(stack.Pop()));
            Assert::AreEqual(0, stack.GetCount());
        }

        TEST_METHOD(PoppedObjectLivesUntilItsSlotIsReused)
        {
            {
                ObjectStack stack;

                stack.Push(new Counted(1));     // the stack holds the only reference
                IObject* pobject = stack.Pop(); // as the factories do

                Assert::AreEqual(1, Counted::s_live);
                Assert::AreEqual(1, Counted::Id(pobject));

                stack.Push(new Counted(2));     // reuses the slot: 1 goes
                Assert::AreEqual(1, Counted::s_live);
                Assert::AreEqual(2, Counted::Id(stack.Pop()));
            }

            Assert::AreEqual(0, Counted::s_live);
        }

        TEST_METHOD(GrowsPastEarlierDepth)
        {
            {
                ObjectStack stack;

                stack.Push(new Counted(1));
                stack.Push(new Counted(2));
                stack.Pop();
                stack.Pop();
                Assert::AreEqual(2, Counted::s_live);

                stack.Push(new Counted(3));
                stack.Push(new Counted(4));
                stack.Push(new Counted(5));
                Assert::AreEqual(3, Counted::s_live);
                Assert::AreEqual(3, stack.GetCount());
                Assert::AreEqual(5, Counted::Id(stack.Pop()));
            }

            Assert::AreEqual(0, Counted::s_live);
        }
    };
}
