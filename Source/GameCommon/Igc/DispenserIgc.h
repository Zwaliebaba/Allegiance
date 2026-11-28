/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	DispenserIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CdispenserIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// DispenserIgc.h : Declaration of the CdispenserIGC

#ifndef __DISPENSERIGC_H_
#define __DISPENSERIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CdispenserIGC
class CdispenserIGC : public IdispenserIGC
{
    public:
        CdispenserIGC(void);
        ~CdispenserIGC(void);

// IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override;
        void            Update(Time now) override;

        ObjectType      GetObjectType(void) const override
        {
            return OT_dispenser;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

// IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return m_expendableType->GetEquipmentType();
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
            return m_mountID == 0;
        }

        void             Activate(void) override
        {
            m_ship->ChangeSignature(m_expendableType->GetLauncherDef()->signature);
            m_timeLoaded = m_ship->GetLastUpdate() + ((1.0f/m_pMission->GetFloatConstant(c_fcidMountRate)) + m_expendableType->GetLoadTime());
        }
        void             Deactivate(void) override
        {
            m_ship->ChangeSignature(-m_expendableType->GetLauncherDef()->signature);
        }
        float               GetMass(void) const override
        {
            return m_amount * m_expendableType->GetMass();
        }

        Money            GetPrice(void) const override
        {
            return ((Money)m_amount) * m_expendableType->GetLauncherDef()->price;
        }

        float           GetMountedFraction(void) const override
        {
            return m_mountedFraction;
        }

        void           SetMountedFraction(float f) override
        {
            m_mountedFraction = f;
        }

        void            Arm(void) override
        {
            m_mountedFraction = 1.0f;
            m_timeLoaded = m_ship->GetLastUpdate();
        }

    // IlauncherIGC
        void                SetAmount(short a) override
        {
            {
                short   maxAmount = m_partType->GetAmount(m_ship);
                if (a > maxAmount)
                    a = maxAmount;
            }

            if (a != m_amount)
            {
                //Adjust the mass of the ship (if there is one) to account for the change in the number of mines
                if (m_ship)
                {
                    m_ship->SetMass(m_ship->GetMass() +
                                    (a - m_amount) * m_expendableType->GetMass());

                    m_pMission->GetIgcSite()->LoadoutChangeEvent(m_ship, this, c_lcQuantityChange);
                }

                m_amount = a;
            }
        }
        short               GetAmount(void) const override
        {
            return m_amount;
        }

        void                SetTimeFired(Time   now) override
        {
            m_timeLoaded = now + m_expendableType->GetLoadTime();
        }

        Time            GetTimeLoaded(void) const override
        {
            return m_timeLoaded;
        }
        void            SetTimeLoaded(Time  tl) override
        {
            m_timeLoaded = tl;
        }
        void            ResetTimeLoaded(void) override
        {
            assert (m_ship);
            m_timeLoaded = m_ship->GetLastUpdate() + ((1.0f/m_pMission->GetFloatConstant(c_fcidMountRate)) + m_expendableType->GetLoadTime());
        }

        const Vector&        GetEmissionPt(void) const override
        {
            return m_emissionPt;
        }


    // IdispenserIGC
        IexpendableTypeIGC*    GetExpendableType(void) const override
        {
            return m_expendableType;
        }
        float               GetArmedFraction(void) override 
        {
            if (GetMountedFraction() < 1.0f)
            {
                return 0;
            }
            else
            {
                return 1.0f - max(0.0f, (m_timeLoaded - Time::Now()) / m_expendableType->GetLoadTime());
            }
        }

    private:
        ImissionIGC*                    m_pMission;
        IlauncherTypeIGC*               m_partType;
        IexpendableTypeIGC*             m_expendableType;
        IshipIGC*                       m_ship;

        Vector                          m_emissionPt;
        float                           m_mountedFraction;

        Time                            m_timeLoaded;

        short                           m_amount;

        Mount                           m_mountID;
};

#endif //__DISPENSERIGC_H_
