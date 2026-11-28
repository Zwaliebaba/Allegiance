/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ActionList.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "actionlist" interface.
**
**  History:
*/
#ifndef _ACTION_LIST_H_
#define _ACTION_LIST_H_

#ifndef _ACTION_H_
#include "Action.h"
#endif// _ACTION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class ActionList : public Action
    {
        public:
                    /* void */          ActionList (void);
            virtual /* void */          ~ActionList (void);
            void                Execute (void) override;
            void                Stop (void) override;
                    void                AddAction (Action* pAction);

        protected:
                    std::list<Action*>              m_actionList;
    };

    //------------------------------------------------------------------------------
}

#endif  //_ACTION_LIST_H_
