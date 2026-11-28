/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    ProjectileTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CprojectileTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ProjectileTypeIgc.h : Declaration of the CprojectileTypeIGC

#ifndef __PROJECTILETYPEIGC_H_
#define __PROJECTILETYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CprojectileTypeIGC
class CprojectileTypeIGC : public IprojectileTypeIGC
{
    public:
        CprojectileTypeIGC(void)
        {
        }

    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override
        {
            m_pMission->DeleteProjectileType(this);
        }

        int             Export(void* data) const override;

        ObjectType      GetObjectType(void) const override
        {
            return OT_projectileType;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.projectileTypeID;
        }
    // ItypeIGC
        const void*     GetData(void) const override
        {
            return &m_data;
        }

    // IprojectileTypeIGC
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
        float           GetSpeed(void) const override
        {
            return m_data.speed;
        }
        bool            GetAbsoluteF(void) const override
        {
            return m_data.absoluteF;
        }
        const char*     GetModelName(void) const override
        {
            return m_data.modelName;
        }
        const char*     GetModelTexture(void) const override
        {
            return m_data.textureName;
        }
        float           GetLifespan(void) const override
        {
            return m_data.lifespan;
        }
        float           GetRadius(void) const override
        {
            return m_data.radius;
        }
        float           GetRotation(void) const override
        {
            return m_data.rotation;
        }
        D3DCOLORVALUE   GetColor(void) const override
        {
            return m_data.color;
        }
        DamageTypeID    GetDamageType(void) const override
        {
            return m_data.damageType;
        }

        SoundID          GetAmbientSound(void) const override
        {
            return m_data.ambientSound;
        }
    private:
        ImissionIGC*            m_pMission;
        DataProjectileTypeIGC   m_data;
};

#endif //__PROJECTILETYPEIGC_H_
