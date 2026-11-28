/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	OrCondition.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "orcondition" interface.
**
**  History:
*/
#ifndef _OR_CONDITION_H_
#define _OR_CONDITION_H_

#ifndef _CONDITION_H_
#include "Condition.h"
#endif  //_CONDITION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class OrCondition : public Condition
    {
        public:
                    /* void */          OrCondition (Condition* pConditionA, Condition* pConditionB);
            virtual /* void */          ~OrCondition (void);
            bool                Start (void) override;
            void                Stop (void) override;
            bool                Evaluate (void) override;

        protected:
                    std::unique_ptr<Condition> m_pConditionA;
                    std::unique_ptr<Condition> m_pConditionB;
    };

    //------------------------------------------------------------------------------
}

#endif  //_OR_CONDITION_H_
