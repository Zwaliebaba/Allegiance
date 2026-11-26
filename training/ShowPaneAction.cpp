/*
**  Copyright (C) 1999 Microsoft Corporation. All Rights Reserved.
**
**  File:	ShowPaneAction.cpp
**
**  Author:  
**
**  Description:
**      Implementation of the training library "ShowPaneAction" interface.
**
**  History:
*/
#include    "pch.h"
#include    "ShowPaneAction.h"

namespace Training
{
    //------------------------------------------------------------------------------
    // class methods
    //------------------------------------------------------------------------------
    /* void */  ShowPaneAction::ShowPaneAction (OverlayMask overlayMask) :
    m_overlayMask (overlayMask)
    {
    }

    //------------------------------------------------------------------------------
    /* void */  ShowPaneAction::~ShowPaneAction (void)
    {
    }

    //------------------------------------------------------------------------------
    void        ShowPaneAction::Execute (void)
    {
        TrekWindow* window = GetWindow ();
        window->TurnOffOverlayFlags (ofTeleportPane bitor 
                                     ofInvestment bitor 
                                     ofGameState bitor 
                                     ofTeam bitor 
                                     ofExpandedTeam bitor 
                                     ofInventory bitor 
                                     ofCommandPane bitor 
                                     ofSectorPane);
        window->TurnOnOverlayFlags (m_overlayMask);
    }

    //------------------------------------------------------------------------------
}
