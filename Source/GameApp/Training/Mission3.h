/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	Mission3.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "Mission3" interface.
**
**  History:
*/
#ifndef _MISSION_ONE_H_
#define _MISSION_ONE_H_

#ifndef _TRAINING_MISSION_H_
#include "TrainingMission.h"
#endif  //_TRAINING_MISSION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // a needed value
    //------------------------------------------------------------------------------
    #define TM_NUMBER_OF_DERELICTS  10

    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class Mission3 : public TrainingMission
    {
        public:
            virtual /* void */              ~Mission3 (void);
            int                     GetMissionID (void) override;
            SectorID                GetStartSectorID (void) override;
            bool                    RecordKeyPress (TrekKey key) override;
            bool                    ShipLanded (void) override;

        protected:
                    ShipID                  m_derelict_hulls[TM_NUMBER_OF_DERELICTS];

        protected:
            void                    CreateUniverse (void) override;
            Condition*              CreateMission (void) override;

                    Condition*              CreateGotTargetCondition (void);

                    Goal*                   CreateGoal01 (void);
                    Goal*                   CreateGoal02 (void);
                    Goal*                   CreateGoal03 (void);
                    Goal*                   CreateGoal04 (void);
                    Goal*                   CreateGoal05 (void);
                    Goal*                   CreateGoal06 (void);
                    Goal*                   CreateGoal07 (void);
                    Goal*                   CreateGoal08 (void);
                    Goal*                   CreateGoal09 (void);
                    Goal*                   CreateGoal10 (void);
                    Goal*                   CreateGoal11 (void);
                    Goal*                   CreateGoal12 (void);
                    Goal*                   CreateGoal13 (void);
    };

    //------------------------------------------------------------------------------
}

#endif  //_MISSION_ONE_H_
