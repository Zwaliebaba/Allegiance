#ifndef _objectstack_h_
#define _objectstack_h_

// Needs zlib.h (TRef, IObject, ZAssert) and <vector>.

//////////////////////////////////////////////////////////////////////////////
//
// Stack
//
//////////////////////////////////////////////////////////////////////////////

//
// The MDL evaluator's operand stack. Pop returns the value, which the
// factories use in expressions (std::stack's pop returns nothing).
//
// A popped object stays referenced by its slot until a later Push reuses
// the slot, as with the TStack this replaces: many factories keep a plain
// pointer to what they popped (Number* pframe = Number::Cast(stack.Pop())),
// and an unnamed object (an ImportImage, say) has no other reference.
//

class ObjectStack {
    std::vector<TRef<IObject>> m_vstack;
    int                        m_count = 0;

public:
    void Push(IObject* pobject)
    {
        if (m_count < (int)m_vstack.size()) {
            m_vstack[m_count] = pobject;
        } else {
            m_vstack.push_back(pobject);
        }

        m_count++;
    }

    TRef<IObject> Pop()
    {
        ZAssert(m_count > 0);
        return m_vstack[--m_count];
    }

    int GetCount() const { return m_count; }
};

#endif
