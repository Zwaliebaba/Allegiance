/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ProbeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CprobeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ProbeIgc.h : Declaration of the CprobeIGC

#ifndef __PROBEIGC_H_
#define __PROBEIGC_H_

#include    "ModelIgc.h"

class CprobeIGC : public TmodelIGC<IprobeIGC>
{
    public:
        CprobeIGC(void);
        ~CprobeIGC(void);

    public:
    // IbaseIGC
	    HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
	    void    Terminate(void) override;
        void    Update(Time now) override;
        int     Export(void*    data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_probe;
        }

        ObjectID    GetObjectID(void) const override
        {
            return m_probeID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteProbe(this);
            }

            TmodelIGC<IprobeIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddProbe(this);

            Release();
        }

    // IdamageIGC
        DamageResult        ReceiveDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  const Vector&           position1,
                                                  const Vector&           position2,
                                                  ImodelIGC*              launcher) override
        {
            IsideIGC*   pside = GetSide();

            if (launcher &&
                (!GetMyMission()->GetMissionParams()->bAllowFriendlyFire) &&
                (pside == launcher->GetSide()) &&
                (amount >= 0.0f) &&
                !m_probeType->HasCapability(c_eabmRescueAny))
            {
                return c_drNoDamage;
            }

            DamageResult    dr = c_drHullDamage;
            float   oldFraction = m_fraction;

            float   maxHP = m_probeType->GetHitPoints();
            amount *= GetMyMission()->GetDamageConstant(type, m_probeType->GetDefenseType());

            if (amount < 0.0f)
            {
                if (maxHP > 0.0f)
                {
                    m_fraction -= amount / maxHP;
                    if (m_fraction > 1.0f)
                        m_fraction = 1.0f;
                    GetThingSite ()->RemoveDamage (m_fraction);
                }
            }
            else if (amount > 0.0f)
            {
                if (launcher && (launcher->GetObjectType() == OT_ship))
                    amount *= ((IshipIGC*)launcher)->GetExperienceMultiplier();

                m_fraction = maxHP > 0.0f ? (m_fraction - amount / maxHP) : 0.0f;
                if (m_fraction <= 0.0f)
                {
                    m_fraction = 0.0f;

                    if (oldFraction > 0.0f)
                    {
                        GetMyMission()->GetIgcSite()->KillProbeEvent(this);
                        dr = c_drKilled;
                    }
                }
                else if ((type & c_dmgidNoDebris) == 0)
                    GetThingSite ()->AddDamage (position2 - position1, m_fraction);
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
            return m_fraction * m_probeType->GetHitPoints();
        }

    // IscannerIGC
        bool            InScannerRange(ImodelIGC*   pModel) const override
        {
            assert (pModel);

            bool    rc;
            IclusterIGC*    pcluster = GetCluster();
            if (pModel->GetCluster() == pcluster)
            {
                if (pModel->GetFlag() == NA)
                {
                    float   m = m_probeType->GetScannerRange() * pModel->GetSignature() * GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaScanRange);
                    {
                        IsideIGC*   pside = pModel->GetSide();
                        if (pside)
                            m /= pside->GetGlobalAttributeSet().GetAttribute(c_gaSignature);
                    }
                    float   r = GetRadius() + pModel->GetRadius() + m;

                    rc = ((GetPosition() - pModel->GetPosition()).LengthSquared() <= r * r) &&
                         LineOfSightExist(pcluster, this, pModel);
                }
                else
                    rc = true;
            }
            else
                rc = false;

            return rc;
        }

        bool             CanSee(ImodelIGC*   pModel) const override
        {
            assert (pModel);

            IsideIGC*   side = GetSide();

            return (pModel->GetSide() == side) ||                               //Always see things on our side
                   pModel->SeenBySide(side) ||
                   InScannerRange(pModel);                                       //or we can see it ourselves
        }

    //IprobeIGC
        IprobeTypeIGC*       GetProbeType(void) const override
        {
            return m_probeType;
        }

        IprojectileTypeIGC*  GetProjectileType(void) const override
        {
            return m_projectileType;
        }
        const Vector&        GetEmissionPt(void) const override
        {
            return m_probeType->GetEmissionPt();
        }

        float               GetProjectileLifespan(void) const override
        {
            float   ls;
            if (m_projectileType)
            {
                ls = m_projectileType->GetLifespan();
                if (m_ammo == 0)
                    ls *= GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaLifespanEnergy);
            }
            else
                ls = 0.0f;

            return ls;
        }
        float               GetLifespan(void) const override
        {
            return m_probeType->GetLifespan();
        }

        float               GetDtBurst(void) const override
        {
            return m_probeType->GetDtBurst();
        }

        float               GetAccuracy(void) const override
        {
            return m_probeType->GetAccuracy();
        }

        bool                GetCanRipcord(float ripcordSpeed) const override
        {
            float   dt = m_probeType->GetRipcordDelay();
            return (dt >= 0.0f) &&
                   ((GetMyLastUpdate() - m_time0) >= dt) &&
                   ((GetMyLastUpdate() + (ripcordSpeed + 1.0f)) <= m_timeExpire);
        }

        float               GetRipcordDelay(void) const override
        {
            return m_probeType->GetRipcordDelay();
        }

        SoundID              GetAmbientSound(void) const override
        {
            return m_probeType->GetAmbientSound();
        }

        void            SetCreateNow (void) override
        {
            m_bCreateNow = true;
        }

        float            GetTimeFraction(void) const override
        {
            Time    timeNow = GetMyLastUpdate();
            float   f = (m_timeExpire - timeNow) / (m_timeExpire - m_time0);
            if (f < 0.0f)
                f = 0.0f;
            else if (f > 1.0f)
                f = 1.0f;

            return f;
        }

    private:
        void ValidTarget(ImodelIGC*  pmodel,
                         IsideIGC*            pside,
                         const Vector&        myPosition,
                         float                dtUpdate,
                         float                accuracy,
                         float                speed,
                         float                lifespan,
                         ObjectType           type,
                         ImodelIGC**          ppmodelMin,
                         float*               pdistance2Min,
                         Vector*              pdirectionMin);

        void  GetTarget(const ModelListIGC*  models,
                        IsideIGC*            pside,
                        const Vector&        myPosition,
                        float                dtUpdate,
                        float                accuracy,
                        float                speed,
                        float                lifespan,
                        ObjectType           type,
                        ImodelIGC**          ppmodelMin,
                        float*               pdistance2Min,
                        Vector*              pdirectionMin);

        ImissionIGC*        m_pMission;
        IprobeTypeIGC*      m_probeType;
        IprojectileTypeIGC* m_projectileType;

        TRef<IshipIGC>      m_launcher;
        TRef<ImodelIGC>     m_target;

        float               m_fraction;
        Time                m_time0;
        Time                m_timeExpire;

        Time                m_nextFire;

        short               m_ammo;

        ProbeID             m_probeID;
        bool                m_bSeenByAll;
        bool                m_bCreateNow;
};

#endif //__PROBEIGC_H_
