/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    MineTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CmineTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// MineTypeIgc.h : Declaration of the CmineTypeIGC

#ifndef __MINETYPEIGC_H_
#define __MINETYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CmineTypeIGC
class CmineTypeIGC : public ImineTypeIGC
{
    public:
        CmineTypeIGC(void)
        {
        }

    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override
        {
            m_pMission->DeleteExpendableType(this);
        }
        void            Update(Time now) override
        {
        }

        int             Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_mineType;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.expendabletypeID;
        }

    // ItypeIGC
        const void*     GetData(void) const override
        {
            return &m_data;
        }

    // IexpendableTypeIGC
        const LauncherDef*  GetLauncherDef(void) const override
        {
            return &(m_data.launcherDef);
        }
        EquipmentType       GetEquipmentType(void) const override
        {
            return ET_Dispenser;
        }
        float           GetLoadTime(void) const override
        {
            return m_data.loadTime;
        }
        float               GetMass(void) const override
        {
            return m_data.launcherDef.mass;
        }
        float           GetLifespan(void) const override
        {
            return m_data.lifespan;
        }

        float           GetSignature(void) const override
        {
            return m_data.signature;
        }
        HitPoints       GetHitPoints(void) const override
        {
            return m_data.hitPoints;
        }
        DefenseTypeID   GetDefenseType(void) const override
        {
            return m_data.defenseType;
        }
        char const*     GetName(void) const override
        {
            return m_data.launcherDef.name;
        }

        char const*     GetModelName(void) const override
        {
            return m_data.modelName;
        }
        char const*     GetModelTexture(void) const override
        {
            return m_data.textureName;
        }
        float           GetRadius(void) const override
        {
            return m_data.radius;
        }

        ExpendableAbilityBitMask    GetCapabilities(void) const override
        {
            return m_data.eabmCapabilities;
        }

        bool                        HasCapability(ExpendableAbilityBitMask eabm) const override
        {
            return (m_data.eabmCapabilities & eabm) != 0;
        }

        // ImineTypeIGC
        float               GetPower(void) const override
        {
            return m_data.power;
        }
        float               GetEndurance(void) const override
        {
            return m_data.endurance;
        }
        DamageTypeID        GetDamageType(void) const override
        {
            return m_data.damageType;
        }

    private:
        ImissionIGC*        m_pMission;
        DataMineTypeIGC     m_data;
};

#endif //__MINETYPEIGC_H_