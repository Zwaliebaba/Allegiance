/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	OneTimeAction.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "onetimeaction" interface.
**
**  History:
*/
#ifndef _ONE_TIME_ACTION_H_
#define _ONE_TIME_ACTION_H_

#ifndef _ACTION_H_
#include "Action.h"
#endif// _ACTION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class OneTimeAction : public Action
    {
        public:
                    /* void */          OneTimeAction (Action* pAction);
            virtual /* void */          ~OneTimeAction (void);
            void                Execute (void) override;
            void                Stop (void) override;

        protected:
                    Action*             m_pAction;
                    bool                m_bHasExecuted;
    };

    //------------------------------------------------------------------------------
}

#endif  //_ONE_TIME_ACTION_H_
