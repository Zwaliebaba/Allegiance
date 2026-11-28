/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	AfterburnerIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CafterburnerIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// AfterburnerIgc.h : Declaration of the CafterburnerIGC

#ifndef __AFTERBURNERIGC_H_
#define __AFTERBURNERIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CafterburnerIGC
class CafterburnerIGC : public IafterburnerIGC
{
    public:
        CafterburnerIGC(void);

// IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;
        void    Update(Time now) override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_afterburner;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

// IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return ET_Afterburner;
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
            return m_fActive;
        }

        void             Activate(void) override
        {
            assert (m_ship);

            if (!m_fActive)
            {
                m_ship->ChangeSignature(m_typeData->signature);
                m_fActive = true;
                m_power = 0.0f;
            }
        }
        void             Deactivate(void) override
        {
            assert (m_ship);

            if (m_fActive)
            {
                m_ship->ChangeSignature(-m_typeData->signature);
                m_fActive = false;
                m_power = 0.0f;
            }
        }
        virtual void             Reset()
        {
        }

        float            GetMass(void) const override
        {
            return m_typeData->mass;
        }

        Money            GetPrice(void) const override
        {
            return m_typeData->price;
        }

    // IafterburnerIGC
        float    GetFuelConsumption(void) const override
        {
            return m_typeData->fuelConsumption;
        }
        float    GetMaxThrust(void) const override
        {
            return m_typeData->maxThrust;
        }
        float    GetOnRate(void) const override
        {
            return m_typeData->onRate;
        }
        float    GetOffRate(void) const override
        {
            return m_typeData->offRate;
        }

        float    GetPower(void) const override
        {
            return m_power;
        }

        void     SetPower(float p) override
        {
            assert (p >= 0.0f);
            assert (p <= 1.0f);
            if (p != 0.0f)
                Activate();

            m_power = p;
        }

        void     IncrementalUpdate(Time lastUpdate, Time now, bool bUseFuel) override;

        SoundID  GetInteriorSound(void) const override
        {
            return m_typeData->interiorSound;
        }
        SoundID  GetExteriorSound(void) const override
        {
            return m_typeData->exteriorSound;
        }

        float    GetMountedFraction(void) const override
        {
            return m_mountedFraction;
        }
        void     SetMountedFraction(float f) override
        {
            m_mountedFraction = f;
            if (f != 1.0f)
                Deactivate();
        }

        void    Arm(void) override
        {
            m_mountedFraction = 1.0f;
        }

    private:
        ImissionIGC*                    m_pMission;
        const DataAfterburnerTypeIGC*   m_typeData;
        IpartTypeIGC*                   m_partType;
        IshipIGC*                       m_ship;
        float                           m_power;
        float                           m_mountedFraction;

        Mount                           m_mountID;
        bool                            m_fActive;
};

#endif //__AFTERBURNERIGC_H_
