/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ActionList.cpp
**
**  Author:  
**
**  Description:
**      Implementation of the training library "actionlist" interface.
**
**  History:
*/
#include    "pch.h"
#include    "ActionList.h"

namespace Training
{
    //------------------------------------------------------------------------------
    // class methods
    //------------------------------------------------------------------------------
    /* void */  ActionList::ActionList (void)
    {
    }

    //------------------------------------------------------------------------------
    /* void */  ActionList::~ActionList (void)
    {
        // the list owns its actions
    }

    //------------------------------------------------------------------------------
    void        ActionList::Execute (void)
    {
        auto    iterator = m_actionList.begin ();
        for (; iterator != m_actionList.end (); iterator++)
            (*iterator)->Execute ();
    }

    //------------------------------------------------------------------------------
    void        ActionList::Stop (void)
    {
        auto    iterator = m_actionList.begin ();
        for (; iterator != m_actionList.end (); iterator++)
            (*iterator)->Stop ();
    }

    //------------------------------------------------------------------------------
    void        ActionList::AddAction (Action* pAction)
    {
        // Store the actions in the order they are added. This gives the
        // author some modicum of control over playback order.
        m_actionList.emplace_back (pAction);
    }

    //------------------------------------------------------------------------------
}
