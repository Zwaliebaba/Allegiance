/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	PackIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CpackIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// PackIgc.h : Declaration of the CpackIGC

#ifndef __PACKIGC_H_
#define __PACKIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CpackIGC
class CpackIGC : public IpackIGC
{
    public:
        CpackIGC(void);
        ~CpackIGC(void);

// IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_pack;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

// IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return ET_Pack;
        }

        IpartTypeIGC*    GetPartType(void) const override
        {
            return m_partType;
        }

        IshipIGC*        GetShip(void) const override
        {
            return m_pship;
        }
        void             SetShip(IshipIGC* newVal, Mount mount) override;

        Mount            GetMountID(void) const override
        {
            return m_mountID;
        }
        void             SetMountID(Mount newVal) override;

        bool             fActive(void) const override
        {
            return false;
        }
        void             Activate(void) override
        {
        }
        void             Deactivate(void) override
        {
        }
        float            GetMass(void) const override
        {
            return 0.0f;
        }

        Money           GetPrice(void) const override
        {
            return 0;
        }

        float           GetMountedFraction(void) const override
        {
            return 1.0f;
        }

        void           SetMountedFraction(float f) override
        {
        }
        void           Arm(void) override
        {
        }

    // IpackIGC
        PackType    GetPackType(void) const override
        {
            return m_typeData->packType;
        }
        void        SetAmount(short a) override
        {
            if (m_pship)
            {
                const IhullTypeIGC*   pht = m_pship->GetHullType();

                short   maxAmount;
                if (m_typeData->packType == c_packAmmo)
                {
                    maxAmount = pht->GetMaxAmmo();
                }
                else
                {
                    assert (m_typeData->packType == c_packFuel);
                    maxAmount = short(pht->GetMaxFuel());
                }

                if (a > maxAmount)
                    a = maxAmount;
            }

            m_amount = a;
        }
        short       GetAmount(void) const override
        {
            return m_amount;
        }

    private:
        ImissionIGC*                    m_pMission;
        const DataPackTypeIGC*          m_typeData;
        IpartTypeIGC*                   m_partType;
        IshipIGC*                       m_pship;
        short                           m_amount;
        Mount                           m_mountID;
};

#endif //__PACKIGC_H_
