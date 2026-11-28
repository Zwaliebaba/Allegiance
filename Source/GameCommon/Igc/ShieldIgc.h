/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ShieldIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CshieldIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ShieldIgc.h : Declaration of the CshieldIGC

#ifndef __SHIELDIGC_H_
#define __SHIELDIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CshieldIGC
class CshieldIGC : public IshieldIGC
{
    public:
        CshieldIGC(void);

// IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;
        void    Update(Time now) override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_shield;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

// IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return ET_Shield;
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
            m_ship->ChangeSignature(m_typeData->signature);
        }
        void             Deactivate(void) override
        {
            m_ship->ChangeSignature(-m_typeData->signature);
            m_ship->GetHitTest()->SetShape(c_htsConvexHullMax);
        }
        float            GetMass(void) const override
        {
            return m_typeData->mass;
        }

        Money            GetPrice(void) const override
        {
            return m_typeData->price;
        }

        float           GetMountedFraction(void) const override
        {
            return m_mountedFraction;
        }

        void            SetMountedFraction(float f) override
        {
            m_mountedFraction = f;
        }

        void            Arm(void) override
        {
            m_mountedFraction = 1.0f;
            m_fraction = 1.0f;
        }

    // IshieldIGC
        float           GetRegeneration(void) const override
        {
            return m_ship 
                   ? (m_typeData->rateRegen * m_ship->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaShieldRegenerationShip))
                   : m_typeData->rateRegen;
        }
	    float           GetMaxStrength(void) const override
        {
            return m_ship 
                   ? (m_typeData->maxStrength * m_ship->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaMaxShieldShip))
                   : m_typeData->maxStrength;
        }
        DefenseTypeID   GetDefenseType(void) const override
        {
            return m_typeData->defenseType;
        }
	    float           GetFraction(void) const override
        {
            return m_fraction;
        }
	    void           SetFraction(float newVal) override
        {
            float down = m_pMission->GetFloatConstant(c_fcidDownedShield);
            if (newVal <= down)
            {
                if (newVal < 0.0f)
                    newVal = 0.0f;

                if (m_fraction > down)
                    m_ship->GetHitTest()->SetShape(c_htsConvexHullMax);
            }
            else
            {
                if (newVal > 1.0f)
                    newVal = 1.0f;

                if (m_fraction <= down)
                    m_ship->GetHitTest()->SetShape(c_htsEllipse);
            }

            m_fraction = newVal;
        }

	    float           ApplyDamage(Time           timeCollision,
                                            DamageTypeID   type,
                                            float          amount,
                                            const Vector&  deltaP) override;

    private:
        const DataShieldTypeIGC*    m_typeData;
        IpartTypeIGC*               m_partType;
        IshipIGC*                   m_ship;
        ImissionIGC*                m_pMission;

        float                       m_fraction;
        float                       m_mountedFraction;

        Mount                       m_mountID;
};

#endif //__SHIELDIGC_H_
