/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	TreasureIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CtreasureIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// TreasureIgc.h : Declaration of the CtreasureIGC

#ifndef __TRESUREIGC_H_
#define __TRESUREIGC_H_

#include    "ModelIgc.h"

class CtreasureIGC : public TmodelIGC<ItreasureIGC>
{
    public:
    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
        void    Terminate(void) override
        {
            AddRef();

            GetMyMission()->DeleteTreasure(this);
            TmodelIGC<ItreasureIGC>::Terminate();

            if (m_buyable)
            {
                m_buyable->Release();
                m_buyable = nullptr;
            }

            Release();
        }

        void            Update(Time now) override;

        int             Export(void* data) const override;

        ObjectType      GetObjectType(void) const override
        {
            return OT_treasure;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.objectID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            //Overrride the model's cluster method so that we can maintain the cluster's treasure list
            //(as well as letting the model maintain its model list)
            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteTreasure(this);
            }

            TmodelIGC<ItreasureIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddTreasure(this);

            Release();
        }

        void                 HandleCollision(Time       timeCollision,
                                                     float                  tCollision,
                                                     const CollisionEntry&  entry,
                                                     ImodelIGC* pModel) override
        {
            if (pModel->GetObjectType() == OT_ship)
            {
                //A treasure hitting a ship ... can the ship pick treasures up?
                if ((((IshipIGC*)pModel)->GetPilotType() >= c_ptPlayer) &&
                    ((IshipIGC*)pModel)->GetBaseHullType()->HasCapability(c_habmRescue))
                {
                    //yes ...let the gamesite deal with it.
                    IIgcSite*   igc = GetMyMission()->GetIgcSite();

                    if (igc->HitTreasureEvent(timeCollision,
                                              (IshipIGC*)pModel,
                                              this))
                    {
                        igc->KillTreasureEvent(this);
                    }
                }
            }
            else if (pModel->GetAttributes() & c_mtStatic)
            {
                GetMyMission()->GetIgcSite()->KillTreasureEvent(this);
            }
        }

    // ItreasureIGC
        TreasureCode    GetTreasureCode(void) const override
        {
            return m_data.treasureCode;
        }

        IbuyableIGC*   GetBuyable(void) const override
        {
            return m_buyable;
        }
        ObjectID       GetTreasureID(void) const override
        {
            return m_data.treasureID;
        }
        void            SetTreasureID(ObjectID newVal) override
        {
            m_data.treasureID = newVal;
        }
        short           GetAmount(void) const override
        {
            return m_data.amount;
        }
        void            SetAmount(short a) override
        {
            m_data.amount = a;
        }
        void            ResetExpiration(Time now) override
        {
            m_expire = now + m_data.lifespan;
        }

        void            SetCreateNow (void) override
        {
            m_data.createNow = true;
        }

    private:
        DataTreasureIGC     m_data;
        IbuyableIGC*        m_buyable;
        Time                m_expire;
        float               m_speed0;

        bool                m_stationaryF;
};

#endif //__TRESUREIGC_H_
