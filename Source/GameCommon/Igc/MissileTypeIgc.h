/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    MissileTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CmissileTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// MissileTypeIgc.h : Declaration of the CmissileTypeIGC

#ifndef __MISSILETYPEIGC_H_
#define __MISSILETYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CmissileTypeIGC
class CmissileTypeIGC : public ImissileTypeIGC
{
    public:
        CmissileTypeIGC(void)
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
            return OT_missileType;
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
            return ET_Magazine;
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

    // ImissileTypeIGC
        float           GetAcceleration(void) const override
        {
            return m_data.acceleration;
        }
        float           GetTurnRate(void) const override
        {
            return m_data.turnRate;
        }
        float           GetInitialSpeed(void) const override
        {
            return m_data.initialSpeed;
        }
        //virtual float           GetArmTime(void) const
        //{
            //return m_data.armTime;
        //}

        float           GetMaxLock(void) const override
        {
            return m_data.maxLock;
        }
        float           GetChaffResistance(void) const override
        {
            return m_data.chaffResistance;
        }
        float           GetLockTime(void) const override
        {
            return m_data.lockTime;
        }
        float           GetReadyTime(void) const override
        {
            return m_data.readyTime;
        }
        float           GetDispersion(void) const override
        {
            return m_data.dispersion;
        }
        float           GetLockAngle(void) const override
        {
            return m_data.lockAngle;
        }
        float           GetPower(void) const override
        {
            return m_data.power;
        }
        float           GetBlastPower(void) const override
        {
            return m_data.blastPower;
        }
        float           GetBlastRadius(void) const override
        {
            return m_data.blastRadius;
        }
        DamageTypeID  GetDamageType(void) const override
        {
            return m_data.damageType;
        }

        SoundID         GetLaunchSound(void) const override
        {
            return m_data.launchSound;
        }
        SoundID         GetAmbientSound(void) const override
        {
            return m_data.ambientSound;
        }

    private:
        ImissionIGC*        m_pMission;
        DataMissileTypeIGC  m_data;
};

#endif //__MISSILETYPEIGC_H_
