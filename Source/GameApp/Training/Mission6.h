/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	Mission6.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "Mission6" interface.
**
**  History:
*/
#ifndef _MISSION_FIVE_H_
#define _MISSION_FIVE_H_

#ifndef _TRAINING_MISSION_H_
#include "TrainingMission.h"
#endif  //_TRAINING_MISSION_H_

#ifndef _GOAL_H_
#include "Goal.h"
#endif  //_GOAL_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class Mission6 : public TrainingMission
    {
        public:
            virtual /* void */              ~Mission6 (void);
            int                     GetMissionID (void) override;
            SectorID                GetStartSectorID (void) override;
            bool                    RestoreShip (void) override;

        protected:
            void                    CreateUniverse (void) override;
            Condition*              CreateMission (void) override;
    };

    //------------------------------------------------------------------------------
}

#endif  //_MISSION_FIVE_H_
