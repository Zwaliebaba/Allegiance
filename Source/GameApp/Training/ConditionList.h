/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ConditionList.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "conditionlist" interface.
**
**  History:
*/
#ifndef _CONDITION_LIST_H_
#define _CONDITION_LIST_H_

#ifndef _CONDITION_H_
#include "Condition.h"
#endif  //_CONDITION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class ConditionList : public Condition
    {
        public:
            virtual /* void */          ~ConditionList (void);
            bool                Start (void) override;
            void                Stop (void) override;
            bool                Evaluate (void) override;
                    void                AddCondition (Condition* pCondition);

        protected:
                    std::list<Condition*>   m_conditionList;
    };

    //------------------------------------------------------------------------------
}

#endif  //_CONDITION_LIST_H_
