/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:  LauncherTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CpartTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// LauncherTypeIgc.h : Declaration of the CpartTypeIGC

#ifndef __LAUNCHERTYPEIGC_H_
#define __LAUNCHERTYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// ClauncherTypeIGC

class   ClauncherTypeIGC : public IlauncherTypeIGC
{
    public:
        ClauncherTypeIGC(void)
        :
            m_pexpendabletype(NULL)
        {
        }

        ~ClauncherTypeIGC(void)
        {
        }

// IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;

        int             Export(void* data) const override;    

        ObjectType      GetObjectType(void) const override
        {
            return OT_partType;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.partID;
        }

 // ItypeIGC
        const void*   GetData(void) const override
        {
            return m_launcherDef;
        }

// IbuyableIGC
        const char*          GetModelName(void) const override
        {
            return m_launcherDef->modelName;
        }

        const char*          GetIconName(void) const override
        {
            return m_launcherDef->iconName;
        }

        const char*          GetName(void) const override
        {
            return m_launcherDef->name;
        }
        const char*          GetDescription(void) const override
        {
            return m_launcherDef->description;
        }
        Money                GetPrice(void) const override
        {
            return m_launcherDef->price;
        }

        DWORD               GetTimeToBuild(void) const override
        {
            return m_launcherDef->timeToBuild;
        }

        BuyableGroupID      GetGroupID(void) const override
        {
            return m_launcherDef->groupID;
        }

        // IpartTypeIGC
        float                GetMass(void) const override
        {
            return m_launcherDef->mass;
        }

        float               GetSignature(void) const override
        {
            return m_launcherDef->signature;
        }
        EquipmentType       GetEquipmentType(void) const override
        {
            return m_equipmenttype;
        }
        PartMask            GetPartMask(void) const override
        {
            return m_launcherDef->partMask;
        }
        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_launcherDef->ttbmRequired;
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_launcherDef->ttbmEffects;
        }

        IpartTypeIGC*           GetSuccessorPartType(void) const override
        {
            return m_pptSuccessor;
        }

        const char*             GetInventoryLineMDLName(void) const override
        {
            return m_data.inventoryLineMDL;
        }

        short               GetAmount(IshipIGC* pship) const override
        {
            short amount;
            if (pship)
            {
                amount = (pship->GetHullType()->GetCapacity(m_equipmenttype) / m_launcherDef->expendableSize);
                if (amount == 0)
                    amount = 1;
            }
            else
                amount = m_data.amount;

            return amount;
        }

// IlauncherTypeIGC
        IexpendableTypeIGC* GetExpendableType(void) const override
        {
            return m_pexpendabletype;
        }

        short               GetLaunchCount (void) const override
        {
            return m_data.launchCount;
        }

    private:
        ImissionIGC*                m_pMission;
        TRef<IpartTypeIGC>          m_pptSuccessor;
        IexpendableTypeIGC*         m_pexpendabletype;
        DataLauncherTypeIGC         m_data;
        const LauncherDef*          m_launcherDef;
        EquipmentType               m_equipmenttype;
};

#endif //__LAUNCHERTYPEIGC_H_
