/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ConditionList.cpp
**
**  Author:  
**
**  Description:
**      Implementation of the training library "conditionlist" interface.
**
**  History:
*/
#include    "pch.h"
#include    "ConditionList.h"

namespace Training
{
    //------------------------------------------------------------------------------
    // class methods
    //------------------------------------------------------------------------------
    /* void */  ConditionList::~ConditionList (void)
    {
        // the list owns its conditions
    }

    //------------------------------------------------------------------------------
    bool        ConditionList::Start (void)
    {
        // This condition list is to be considered "true" by default.
        bool    result = true;

        // Loop over the condition list. If any condition evaluates to false, then
        // the condition list evaluates to false. We need to traverse all of the
        // conditions, however, since they may have actions associated with them.
        auto    iterator = m_conditionList.begin ();
        for (; iterator != m_conditionList.end (); iterator++)
            if ((*iterator)->Start () == false)
                result = false;

        // return the result
        return result;
    }

    //------------------------------------------------------------------------------
    void        ConditionList::Stop (void)
    {
        // Loop over the condition list, calling stop on all of them.
        auto    iterator = m_conditionList.begin ();
        for (; iterator != m_conditionList.end (); iterator++)
            (*iterator)->Stop ();
    }

    //------------------------------------------------------------------------------
    bool        ConditionList::Evaluate (void)
    {
        // This condition list is to be considered "true" by default.
        bool    result = true;

        // Loop over the condition list. If any condition evaluates to false, then
        // the condition list evaluates to false. We need to traverse all of the 
        // conditions, however, since they may have actions associated with them.
        auto    iterator = m_conditionList.begin ();
        for (; iterator != m_conditionList.end (); iterator++)
            if ((*iterator)->Evaluate () == false)
                result = false;

        // return the result
        return result;
    }

    //------------------------------------------------------------------------------
    void        ConditionList::AddCondition (Condition* pCondition)
    {
        // Store the conditions in the order they are added. This gives the
        // author some modicum of control over playback order.
        m_conditionList.emplace_back (pCondition);
    }

    //------------------------------------------------------------------------------
}
