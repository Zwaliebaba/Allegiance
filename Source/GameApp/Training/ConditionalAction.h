/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ConditionalAction.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "conditionalaction" interface.
**
**  History:
*/
#ifndef _CONDITIONAL_ACTION_H_
#define _CONDITIONAL_ACTION_H_

#ifndef _CONDITION_H_
#include "Condition.h"
#endif  //_CONDITION_H_

#ifndef _ACTION_H_
#include "Action.h"
#endif  //_ACTION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class ConditionalAction : public Condition
    {
        public:
                    enum    {IMMEDIATE_EXECUTION = true, DELAYED_EXECUTION = false};

        public:
                    /* void */          ConditionalAction (Condition* pcondition, Action* paction, bool bImmediateExecute = IMMEDIATE_EXECUTION);
            virtual /* void */          ~ConditionalAction (void);
            bool                Start (void) override;
            void                Stop (void) override;
            bool                Evaluate (void) override;

        protected:
                    std::unique_ptr<Condition> m_pCondition;
                    std::unique_ptr<Action> m_pAction;
                    bool                m_bImmediateExecute;
                    bool                m_bExecute;
    };

    //------------------------------------------------------------------------------
}

#endif  //_CONDITIONAL_ACTION_H_
