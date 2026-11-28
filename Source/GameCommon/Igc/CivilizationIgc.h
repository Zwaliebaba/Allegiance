/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    CivilizationIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CcivilizationIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// CivilizationIgc.h : Declaration of the CcivilizationIGC

#ifndef __CIVILIZATIONIGC_H_
#define __CIVILIZATIONIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CcivilizationIGC
class       CcivilizationIGC : public IcivilizationIGC
{
    public:
        CcivilizationIGC(void)
        {
        }

    public:
    // IbaseIGC
        HRESULT                     Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void                        Terminate(void) override;

        int                         Export(void* data) const override;

        ObjectType                  GetObjectType(void) const override
        {
            return OT_civilization;
        }

        ObjectID                    GetObjectID(void) const override
        {
            return m_data.civilizationID;
        }

    // IcivilizationIGC
        const char*                 GetName(void) const override
        {
            return m_data.name;
        }

        const char*                 GetIconName(void) const override
        {
            return m_data.iconName;
        }

        IhullTypeIGC*               GetLifepod(void) const override
        {
            return m_phtLifepod;
        }

        const TechTreeBitMask&      GetBaseTechs(void) const override
        {
            return m_data.ttbmBaseTechs;
        }
        const TechTreeBitMask&      GetNoDevTechs(void) const override
        {
            return m_data.ttbmNoDevTechs;
        }
        const GlobalAttributeSet&   GetBaseAttributes(void) const override
        {
            return m_data.gasBaseAttributes;
        }
        IstationTypeIGC*            GetInitialStationType(void) const override
        {
            return m_pstInitial;
        }
        const char*                 GetHUDName(void) const override
        {
            return m_data.hudName;
        }

        float                       GetBonusMoney(void) const override
        {
            return m_data.bonusMoney;
        }

        float                       GetIncomeMoney(void) const override
        {
            return m_data.incomeMoney;
        }

    private:
        ImissionIGC*        m_pMission;
        DataCivilizationIGC m_data;

        IhullTypeIGC*       m_phtLifepod;
        IstationTypeIGC*    m_pstInitial;
};

#endif //__CIVILIZATIONIGC_H_
