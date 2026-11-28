/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	GoalList.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "goallist" interface.
**
**  History:
*/
#ifndef _GOAL_LIST_H_
#define _GOAL_LIST_H_

#ifndef _GOAL_H_
#include "Goal.h"
#endif  //_GOAL_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class GoalList : public Condition
    {
        public:
                    /* void */          GoalList (void);
            virtual /* void */          ~GoalList (void);
            bool                Start (void) override;
            void                Stop (void) override;
            bool                Evaluate (void) override;
                    void                AddGoal (Goal* pGoal);

        protected:
                    std::list<Goal*>            m_goalList;
                    std::list<Goal*>::iterator  m_iterator;
    };

    //------------------------------------------------------------------------------
}

#endif  //_GOAL_LIST_H_
