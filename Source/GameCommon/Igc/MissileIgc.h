/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	MissileIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CmissileIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// MissileIgc.h : Declaration of the CmissileIGC

#ifndef __MISSILEIGC_H_
#define __MISSILEIGC_H_

#include    "ModelIgc.h"

class CmissileIGC : public TmodelIGC<ImissileIGC>
{
    public:
        CmissileIGC(void);
        ~CmissileIGC(void);

    public:
    // IbaseIGC
	    HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
	    void    Terminate(void) override;
        void    Update(Time now) override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_missile;
        }

        ObjectID    GetObjectID(void) const override
        {
            return m_missileID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteMissile(this);
            }

            TmodelIGC<ImissileIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddMissile(this);

            Release();
        }

        void                 HandleCollision(Time       timeCollision,
                                                     float                  tCollision,
                                                     const CollisionEntry&  entry,
                                                     ImodelIGC* pModel) override;

        /*
        virtual void                SetSide(IsideIGC*   pside)  //override the default SetSide method
        {
            TmodelIGC<ImissileIGC>::SetSide(pside);

            if (pside)
                GetThingSite()->SetTrailColor(pside->GetColor());
        }
        */

    // IdamageIGC
        DamageResult        ReceiveDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  const Vector&           position1,
                                                  const Vector&           position2,
                                                  ImodelIGC*              launcher) override
        {
            DamageResult dr = c_drHullDamage;
            amount *= GetMyMission()->GetDamageConstant(type, m_missileType->GetDefenseType());

            float   maxHP = m_missileType->GetHitPoints();

            if (amount < 0.0f)
            {
                if (maxHP > 0.0f)
                {
                    m_fraction -= amount / maxHP;
                    if (m_fraction > 1.0f)
                        m_fraction = 1.0f;
                }
            }
            else if (amount != 0.0)
            {
                float   oldFraction = m_fraction;
                m_fraction = (maxHP > 0.0f) ? (m_fraction - amount / maxHP) : 0.0f;
                if (m_fraction <= 0.0f)
                {
                    m_fraction = 0.0f;
                    if (oldFraction > 0.0f)
                    {
                        GetMyMission()->GetIgcSite()->KillMissileEvent(this, position1);
                        dr = c_drKilled;
                    }
                }
            }

            return dr;
        }
        float   GetFraction(void) const override
        {
            return m_fraction;
        }
        void    SetFraction(float newVal) override
        {
            m_fraction = newVal;
        }

        float               GetHitPoints(void) const override
        {
            return m_fraction * m_missileType->GetHitPoints();
        }

    // ImissileIGC
        ImissileTypeIGC*    GetMissileType(void) const override
        {
            return m_missileType;
        }
        IshipIGC*           GetLauncher(void) const override
        {
            return m_launcher;
        }
        ImodelIGC*          GetTarget(void) const override
        {
            return m_target;
        }
        void                SetTarget(ImodelIGC* newVal) override
        {
            if (m_target)
                m_target->Release();

            m_target = newVal;

            if (newVal)
                newVal->AddRef();

            m_lock = 1.0f;

            //Assume m_tImpact does not change (not unreasonable given that the only reason to call SetTarget
            //is when the missile is spoofed and then the chaff is on top of the target.
        }

        float               GetLock(void) const override
        {
            return m_lock;
        }

        void                Explode(const Vector& position) override;

        void                Disarm(void) override
        {
            m_bDisarmed = true;
            if (m_launcher->GetLastMissileFired() == this)
                m_launcher->SetLastMissileFired(nullptr);
        }

    private:
        ImissileTypeIGC*    m_missileType;
        IshipIGC*           m_launcher;
        ImodelIGC*          m_target;
        Time                m_timeActivate;
        Time                m_timeExpire;
        float               m_tImpact;     //current estimated time of impact
        float               m_fraction;
        float               m_lock;
        MissileID           m_missileID;
        Vector              m_explodePosition;
        bool                m_bDisarmed;
};

#endif //__MISSILEIGC_H_
