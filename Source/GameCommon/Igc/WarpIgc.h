/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	WarpIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CwarpIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// WarpIgc.h : Declaration of the CwarpIGC

#ifndef __WARPIGC_H_
#define __WARPIGC_H_

#include    "ModelIgc.h"

class CwarpIGC : public TmodelIGC<IwarpIGC>
{
    public:
        CwarpIGC(void)
        :
            m_destination(nullptr)
        {
        }

        ~CwarpIGC(void)
        {
        }

    // IbaseIGC
	    HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
	    void    Terminate(void) override
        {
            AddRef();
            GetMyMission()->DeleteWarp(this);
            TmodelIGC<IwarpIGC>::Terminate();

            if (m_destination)
            {
                m_destination->Release();
                m_destination = nullptr;
            }

            m_bombs.purge();

            Release();
        }
        void            Update(Time   now) override
        {
            WarpBombLink*   plink;

            while ((plink = m_bombs.first()) &&               //Intentional =
                   (plink->data().timeExplosion <= now))
            {
                ImissileTypeIGC*    pmt = plink->data().pmt;
                assert (pmt);
                assert (pmt->GetObjectType() == OT_missileType);

                DamageTypeID    dtid = pmt->GetDamageType();
                float           p    = pmt->GetPower();
                float           r    = pmt->GetBlastRadius();

                IclusterIGC*    pcluster = GetCluster();
                pcluster->CreateExplosion(dtid,
                                          p,
                                          r,
                                          c_etBigShip,
                                          plink->data().timeExplosion,
                                          GetPosition(),
                                          nullptr);

                IwarpIGC*   pwarp = GetDestination();
                pcluster = pwarp->GetCluster();
                pcluster->CreateExplosion(dtid,
                                          p,
                                          r,
                                          c_etBigShip,
                                          plink->data().timeExplosion,
                                          pwarp->GetPosition(),
                                          nullptr);

                delete plink;
            }

            TmodelIGC<IwarpIGC>::Update(now);
        }

        int                 Export(void* data) const override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_warp;
        }

	    ObjectID            GetObjectID(void) const override
        {
            return m_warpDef.warpID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            //Overrride the model's cluster method so that we can maintain the cluster's warp list
            //(as well as letting the model maintain its model list)
            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteWarp(this);
            }

            TmodelIGC<IwarpIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddWarp(this);

            Release();
        }

    // IwarpIGC
	    IwarpIGC*       GetDestination(void) override
        {
            if (!m_destination)
            {
                //Do a lazy evaluation of the destination so that all warps do not
                //need to be defined before defining any warps
                m_destination = GetMyMission()->GetWarp(m_warpDef.destinationID);
                if (m_destination)
                    m_destination->AddRef();
            }

            return m_destination;
        }

        void         AddBomb(Time               timeExplosion,
                                     ImissileTypeIGC*   pmt) override
        {
            WarpBombLink*   p = new WarpBombLink;
            p->data().timeExplosion = timeExplosion;
            p->data().pmt = pmt;

            // add the pulse effects
            Color       blastColor (0.6f, 0.8f, 1.0f);
            float       fExplodeTime = timeExplosion - Time::Now ();
            GetCluster ()->GetClusterSite ()->AddPulse (fExplodeTime, GetPosition (), pmt->GetBlastRadius (), blastColor);
            IwarpIGC*   pDestination = GetDestination();
            pDestination->GetCluster ()->GetClusterSite ()->AddPulse (fExplodeTime, m_destination->GetPosition (), pmt->GetBlastRadius (), blastColor);

            // this should tell the aleph rendering site about the pulse
            ThingSite*  pThingSite = GetThingSite ();
            pThingSite->AddPulse (fExplodeTime, GetPosition (), pmt->GetBlastRadius (), blastColor);
            pThingSite = pDestination->GetThingSite ();
            pThingSite->AddPulse (fExplodeTime, GetPosition (), pmt->GetBlastRadius (), blastColor);

            m_bombs.last(p);
        }

        const WarpBombList* GetBombs(void) const override
        {
            return &m_bombs;
        }

    private:
        IwarpIGC*           m_destination;
        WarpDef             m_warpDef;
        WarpBombList        m_bombs;
};

#endif //__WARPIGC_H_
