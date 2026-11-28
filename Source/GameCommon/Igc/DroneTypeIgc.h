/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    DroneTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CdroneTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// DroneTypeIgc.h : Declaration of the CdroneTypeIGC

#ifndef __DRONETYPEIGC_H_
#define __DRONETYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CdroneTypeIGC
class       CdroneTypeIGC : public IdroneTypeIGC
{
    public:
        CdroneTypeIGC(void)
        {
        }

        ~CdroneTypeIGC(void)
        {
        }

    public:
    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override;

        int             Export(void* data) const override;

        ObjectType      GetObjectType(void) const override
        {
            return OT_droneType;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.droneTypeID;
        }

    // ItypeIGC
        const void*   GetData(void) const override
        {
            return &m_data;
        }

    // IbuyableIGC
        const char*          GetModelName(void) const override
        {
            return m_data.modelName;
        }

        const char*          GetIconName(void) const override
        {
            return m_data.iconName;
        }

        const char*          GetName(void) const override
        {
            return m_data.name;
        }
        const char*          GetDescription(void) const override
        {
            return m_data.description;
        }
        Money                GetPrice(void) const override
        {
            return m_data.price;
        }
        BuyableGroupID       GetGroupID(void) const override 
        { 
            return m_data.groupID; 
        };

        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_data.ttbmRequired;
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_data.ttbmEffects;
        }

        DWORD                   GetTimeToBuild(void) const override
        {
            return m_data.timeToBuild;
        }

        // IdroneTypeIGC
        HullID                  GetHullTypeID(void) const override
        {
            return m_data.hullTypeID;
        }
        PilotType               GetPilotType(void) const override
        {
            return m_data.pilotType;
        }

        float                   GetShootSkill(void) const override
        {
            return m_data.shootSkill;
        }
        float                   GetMoveSkill(void) const override
        {
            return m_data.moveSkill;
        }
        float                   GetBravery(void) const override
        {
            return m_data.bravery;
        }

        IexpendableTypeIGC*     GetLaidExpendable(void) const override
        {
            return m_petLaid;
        }

    private:
        ImissionIGC*                m_pMission;
        DataDroneTypeIGC            m_data;
        TRef<IexpendableTypeIGC>    m_petLaid;
};

#endif //__DRONETYPEIGC_H_
