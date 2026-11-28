/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	PeriodicCondition.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "periodiccondition" interface.
**
**  History:
*/
#ifndef _PERIODIC_CONDITION_H_
#define _PERIODIC_CONDITION_H_

#ifndef _CONDITION_H_
#include "Condition.h"
#endif  //_CONDITION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class PeriodicCondition : public Condition
    {
        public:
                    /* void */          PeriodicCondition (Condition* pCondition, float fPeriod);
            virtual /* void */          ~PeriodicCondition (void);
            bool                Start (void) override;
            void                Stop (void) override;
            bool                Evaluate (void) override;

        protected:
                    Condition*          m_pCondition;
                    DWORD               m_dwPeriod;
                    DWORD               m_dwStartTime;
    };

    //------------------------------------------------------------------------------
}

#endif  //_PERIODIC_CONDITION_H_
