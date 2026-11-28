/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:  PartTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CpartTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// PartTypeIgc.h : Declaration of the CpartTypeIGC

#ifndef __PARTTYPEIGC_H_
#define __PARTTYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CpartTypeIGC

class   CpartTypeIGC : public IpartTypeIGC
{
    public:
        CpartTypeIGC(void)
        :
            m_data(nullptr)
        {
        }

        ~CpartTypeIGC(void)
        {
            delete [] m_data;
        }

// IbaseIGC
        HRESULT                 Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void                    Terminate(void) override;

        int                     Export(void* data) const override;    

        ObjectType              GetObjectType(void) const override
        {
            return OT_partType;
        }

        ObjectID                GetObjectID(void) const override
        {
            return m_data->partID;
        }
// ItypeIGC
        const void*             GetData(void) const override
        {
            return m_data;
        }

// IbuyableIGC
        const char*             GetModelName(void) const override
        {
            return m_data->modelName;
        }

        const char*          GetIconName(void) const override
        {
            return m_data->iconName;
        }

        const char*             GetName(void) const override
        {
            return m_data->name;
        }
        const char*             GetDescription(void) const override
        {
            return m_data->description;
        }
        Money                   GetPrice(void) const override
        {
            return m_data->price;
        }

        DWORD                   GetTimeToBuild(void) const override
        {
            return m_data->timeToBuild;
        }

        BuyableGroupID          GetGroupID(void) const override
        {
            return m_data->groupID;
        }

        // IpartTypeIGC
        float                   GetMass(void) const override
        {
            return m_data->mass;
        }

        float                   GetSignature(void) const override
        {
            return m_data->signature;
        }

        EquipmentType           GetEquipmentType(void) const override
        {
            return m_data->equipmentType;
        }
        PartMask                GetPartMask(void) const override
        {
            return m_data->partMask;
        }

        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_data->ttbmRequired;
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_data->ttbmEffects;
        }

        IpartTypeIGC*           GetSuccessorPartType(void) const override
        {
            return m_pptSuccessor;
        }

        const char*             GetInventoryLineMDLName(void) const override
        {
            return m_data->inventoryLineMDL;
        }

    private:
        ImissionIGC*        m_pMission;
        TRef<IpartTypeIGC>  m_pptSuccessor;
        DataPartTypeIGC*    m_data;
        int                 m_dataSize;
};

#endif //__PARTTYPEIGC_H_
