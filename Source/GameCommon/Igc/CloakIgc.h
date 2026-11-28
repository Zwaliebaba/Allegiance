/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	CloakIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CcloakIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// CloakIgc.h : Declaration of the CcloakIGC

#ifndef __CLOAKIGC_H_
#define __CLOAKIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CcloakIGC
class CcloakIGC : public IcloakIGC
{
    public:
        CcloakIGC(void);

// IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;
        void    Update(Time now) override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_cloak;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

// IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return ET_Cloak;
        }

        IpartTypeIGC*    GetPartType(void) const override
        {
            return m_partType;
        }

        IshipIGC*        GetShip(void) const override
        {
            return m_ship;
        }
        void             SetShip(IshipIGC* newVal, Mount mount) override;

        Mount            GetMountID(void) const override
        {
            return m_mountID;
        }
        void             SetMountID(Mount newVal) override;

        bool             fActive(void) const override
        {
            return (m_mountID == 0);
        }

        void             Activate(void) override
        {
            assert (m_ship);
            if (!m_fActive)
            {
                m_ship->ChangeSignature(m_typeData->signature);
                m_fActive = true;

                m_cloaking = 0.0f;
                assert (m_ship->GetCloaking() == 1.0f);
            }
        }
        void             Deactivate(void) override
        {
            assert (m_ship);

            if (m_fActive)
            {
                m_ship->ChangeSignature(-m_typeData->signature);
                m_fActive = false;

                m_ship->SetCloaking(1.0f);  //1.0 - m_cloaking
            }
        }
        float            GetMass(void) const override
        {
            return m_typeData->mass;
        }

        Money            GetPrice(void) const override
        {
            return m_typeData->price;
        }

        float    GetMountedFraction(void) const override
        {
            return m_mountedFraction;
        }
        void     SetMountedFraction(float f) override
        {
            m_mountedFraction = f;
        }

        void    Arm(void) override
        {
            m_mountedFraction = 1.0f;
        }

    // IcloakIGC
        float    GetEnergyConsumption(void) const override
        {
            return m_typeData->energyConsumption;
        }
        float    GetMaxCloaking(void) const override
        {
            return m_typeData->maxCloaking; 
            /*
            return m_ship
                   ? (m_typeData->rateRegen * m_ship->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaRegenerationShield))
                   : m_typeData->rateRegen;
            */
        }
        float    GetOnRate(void) const override
        {
            return m_typeData->onRate; 
        }
        float    GetOffRate(void) const override
        {
            return m_typeData->offRate; 
        }

        SoundID  GetEngageSound() const override
        {
            return m_typeData->engageSound;
        }
        SoundID  GetDisengageSound() const override
        {
            return m_typeData->disengageSound;
        }

    private:
        const DataCloakTypeIGC*     m_typeData;
        IpartTypeIGC*               m_partType;
        IshipIGC*                   m_ship;
        ImissionIGC*                m_pMission;

        float                       m_cloaking;
        float                       m_mountedFraction;

        Mount                       m_mountID;
        bool                        m_fActive;
};

#endif //__CLOAKIGC_H_
