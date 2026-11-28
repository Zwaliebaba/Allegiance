/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ProjectileIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CprojectileIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ProjectileIgc.h : Declaration of the CprojectileIGC

#ifndef __PROJECTILEIGC_H_
#define __PROJECTILEIGC_H_

#include    "ModelIgc.h"

class CprojectileIGC : public TmodelIGC<IprojectileIGC>
{
    public:
        CprojectileIGC(void);
        ~CprojectileIGC(void);

    public:
    // IbaseIGC
	    HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
	    void                Terminate(void) override;
        void                Update(Time now) override;
        int                 Export(void* _data) const override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_projectile;
        }

    // ImodelIGC
        void                SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteModel(this);
            }

            TmodelIGC<IprojectileIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddModel(this);

            Release();
        }

        void                HandleCollision(Time       timeCollision,
                                                    float                  tCollision,
                                                    const CollisionEntry&  entry,
                                                    ImodelIGC* pModel) override;

    // IprojectileIGC
        IprojectileTypeIGC* GetProjectileType(void) const override
        {
            return m_projectileType;
        }
        ImodelIGC*           GetLauncher(void) const override
        {
            return m_launcher;
        }
        void                SetLauncher(ImodelIGC* newVal) override
        {
            assert (!m_launcher);       //Only set once
            assert (newVal);

            newVal->AddRef();
            m_launcher = newVal;

            GetHitTest()->SetNoHit(m_launcher->GetHitTest());
            SetSide(m_launcher->GetSide());
        }
        void                SetGunner(IshipIGC* newVal) override
        {
            assert (!m_launcher);       //Only set once
            assert (newVal);

            newVal->AddRef();
            m_launcher = newVal;

            GetHitTest()->SetNoHit(newVal->GetParentShip()->GetHitTest());
            SetSide(m_launcher->GetSide());
        }

    private:
        IprojectileTypeIGC* m_projectileType;
        ImodelIGC*          m_launcher;
        ExplosionData*      m_pExplosionData;
        Time                m_timeExpire;
};

#endif //__PROJECTILEIGC_H_
