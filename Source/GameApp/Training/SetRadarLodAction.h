/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	SetRadarLodAction.h
**
**  Author:  
**
**  Description:
**      Header file for the training library "setradarlodaction" interface.
**
**  History:
*/
#ifndef _SET_RADAR_LOD_ACTION_H_
#define _SET_RADAR_LOD_ACTION_H_

#ifndef _ACTION_H_
#include "Action.h"
#endif// _ACTION_H_

namespace Training
{
    //------------------------------------------------------------------------------
    // class definitions
    //------------------------------------------------------------------------------
    class SetRadarLODAction : public Action
    {
        public:
                    /* void */          SetRadarLODAction (RadarImage::RadarLOD radarLOD);
            virtual /* void */          ~SetRadarLODAction (void);
            void                Execute (void) override;

        protected:
                    RadarImage::RadarLOD    m_radarLOD;
    };

    //------------------------------------------------------------------------------
}

#endif  //_SET_RADAR_LOD_ACTION_H_
