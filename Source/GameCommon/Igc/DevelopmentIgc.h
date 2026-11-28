/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    DevelopmentIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CdevelopmentIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// DevelopmentIgc.h : Declaration of the CdevelopmentIGC

#ifndef __DEVELOPMENTIGC_H_
#define __DEVELOPMENTIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CdevelopmentIGC
class       CdevelopmentIGC : public IdevelopmentIGC
{
    public:
        CdevelopmentIGC(void)
        {
        }

        ~CdevelopmentIGC(void)
        {
        }

    public:
    // IbaseIGC
        HRESULT     Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void        Terminate(void) override;

        int         Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_development;
        }
        ObjectID    GetObjectID(void) const override
        {
            return m_data.developmentID;
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

        // IdevelopmentIGC
        const GlobalAttributeSet&   GetGlobalAttributeSet(void) const override
        {
            return m_data.gas;
        }

        bool                        GetTechOnly(void) const override
        {
            return m_techOnly;
        }

        bool                        IsObsolete(const TechTreeBitMask&  ttbm) const override
        {
            return m_techOnly && (m_data.ttbmEffects <= ttbm);
        }

        SoundID                     GetCompletionSound() const override
        {
            return m_data.completionSound;
        }

    private:
        ImissionIGC*        m_pMission;
        DataDevelopmentIGC  m_data;
        bool                m_techOnly;
};

#endif //__DEVELOPMENTIGC_H_
