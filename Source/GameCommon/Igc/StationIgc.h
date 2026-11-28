/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	StationIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CstationIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ShipIgc.h : Declaration of the CshipIGC

#ifndef __STATIONIGC_H_
#define __STATIONIGC_H_

#include "ModelIgc.h"

class       CstationIGC;

class       MyStationType : public IstationTypeIGC
{
    public:
        MyStationType(CstationIGC*  pstation)
        :
            m_pstation(pstation),
            m_pStationType(nullptr)
        {
        }

        ~MyStationType(void)
        {
            if (m_pStationType)
                m_pStationType->Release();
        }

        //IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
        void            Terminate(void) override;
        void            Update(Time   now) override;

        ObjectType      GetObjectType(void) const override;
        ObjectID        GetObjectID(void) const override;

        //ItypeIGC
        const void*     GetData(void) const override;

        //IbuyableIGC
        const char*             GetName(void) const override;
        const char*             GetDescription(void) const override;
        const char*             GetModelName(void) const override;
        const TechTreeBitMask&  GetRequiredTechs(void) const override;
        const TechTreeBitMask&  GetEffectTechs(void) const override;

        Money                   GetPrice(void) const override;
        DWORD                   GetTimeToBuild(void) const override;
        BuyableGroupID          GetGroupID(void) const override;

        // IstationTypeIGC
        HitPoints               GetMaxArmorHitPoints(void) const override;
        DefenseTypeID           GetArmorDefenseType(void) const override;
        HitPoints               GetMaxShieldHitPoints(void) const override;
        DefenseTypeID           GetShieldDefenseType(void) const override;
        float                   GetArmorRegeneration(void) const override;
        float                   GetShieldRegeneration(void) const override;
        float                   GetSignature(void) const override;
        float                   GetRadius(void) const override;
        float                   GetScannerRange(void) const override;
        Money                   GetIncome(void) const override;
        const TechTreeBitMask&  GetLocalTechs(void) const override;
        StationAbilityBitMask   GetCapabilities(void) const override;
        bool                    HasCapability(StationAbilityBitMask sabm) const override;
        const char*             GetTextureName(void) const override;
        const char*             GetBuilderName(void) const override;
        const char*             GetIconName(void) const override;
        IstationTypeIGC*        GetSuccessorStationType(const IsideIGC*   pside) override;
        AsteroidAbilityBitMask  GetBuildAABM(void) const override;

        int                     GetLaunchSlots(void) const override;
        const Vector&           GetLaunchPosition(int   slotID) const override;
        const Vector&           GetLaunchDirection(int   slotID) const override;

        int                     GetLandSlots(void) const override;
        int                     GetCapLandSlots(void) const override;
        int                     GetLandPlanes(int   slotID) const override;
        const Vector&           GetLandPosition(int slotID, int planeID) const override;
        const Vector&           GetLandDirection(int slotID, int planeID) const override;

        SoundID                 GetInteriorSound() const override;
        SoundID                 GetInteriorAlertSound() const override;
        SoundID                 GetExteriorSound() const override;

        SoundID                 GetConstructorNeedRockSound() const override;
        SoundID                 GetConstructorUnderAttackSound() const override;
        SoundID                 GetConstructorDestroyedSound() const override;
        SoundID                 GetCompletionSound() const override;

        SoundID                 GetUnderAttackSound() const override;
        SoundID                 GetCriticalSound() const override;
        SoundID                 GetDestroyedSound() const override;
        SoundID                 GetCapturedSound() const override;
        SoundID                 GetEnemyCapturedSound() const override;
        SoundID                 GetEnemyDestroyedSound() const override;

        StationClassID          GetClassID() const override;
        IdroneTypeIGC*          GetConstructionDroneType(void) const override;

    private:
        IstationTypeIGC*    GetStationType(void) const
        {
            return m_pStationType;
        }
        void SetStationType(IstationTypeIGC*  pst)
        {
            if (m_pStationType)
                m_pStationType->Release();

            m_pStationType = pst;

            if (pst)
            {
                pst->AddRef();
                m_pStationData = (const DataStationTypeIGC*)(m_pStationType->GetData());
            }
            else
                m_pStationData = nullptr;
        }

        CstationIGC*                m_pstation;
        IstationTypeIGC*            m_pStationType;
        const DataStationTypeIGC*   m_pStationData;

        friend class CstationIGC;
};

class       CstationIGC : public TmodelIGC<IstationIGC>
{
    public:
        CstationIGC(void)
        :
            m_myStationType(this),
            m_hullFraction(1.0f),
            m_shieldFraction(0.0f)
        {
        }

        /*
        DWORD   AddRef(void)
        {
            return TmodelIGC<IstationIGC>::AddRef();
        }

        DWORD   Release(void)
        {
            return TmodelIGC<IstationIGC>::Release();
        }
        */
    // IbaseIGC
	    HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
	    void    Terminate(void) override
        {
            AddRef();
            {
                //Force all of the ships docked at the station to undock
                //NYI terminate all of the ships docked at the station?
                ShipLinkIGC*    l;
                while (l = m_shipsDocked.first())   //intentional assignment
                    l->data()->SetStation(nullptr);
            }

            GetMyMission()->DeleteStation(this);
            TmodelIGC<IstationIGC>::Terminate();

            m_myStationType.SetStationType(nullptr);
            Release();
        }
        void    Update(Time now) override
        {
            float   dt = now - GetMyLastUpdate();

            if (dt > 0.0f)
            {
                if (m_hullFraction != 1.0f)
                {
                    m_hullFraction += dt * m_myStationType.GetArmorRegeneration() / m_myStationType.GetMaxArmorHitPoints();
                    if (m_hullFraction > 1.0f)
                        m_hullFraction = 1.0f;
                }
                else
                {
                    float   m = m_myStationType.GetMaxShieldHitPoints();
                    if ((m_shieldFraction != 1.0f) && (m != 0.0f))
                    {
                        //Use SetShieldFraction so the station's shape will be adjusted as appropriate.
                        SetShieldFraction(m_shieldFraction + dt * m_myStationType.GetShieldRegeneration() / m);
                    }
                }

                TmodelIGC<IstationIGC>::Update(now);
            }
        }
        int             Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_station;
        }

        ObjectID    GetObjectID(void) const override
        {
            return m_stationID;
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
                {
                    assert (cluster == NULL);
                    c->DeleteStation(this);
                    c->GetClusterSite()->MoveStation();
                }
            }

            TmodelIGC<IstationIGC>::SetCluster(cluster);

            if (cluster)
            {
                cluster->AddStation(this);
                cluster->GetClusterSite()->MoveStation();
            }
            Release();
        }

        void    SetSide(IsideIGC*   psideNew) override
        {
            IsideIGC*   psideOld = GetSide();
            if (psideOld != psideNew)
            {
                IclusterIGC*    pcluster = GetCluster();

                if (psideOld != nullptr)
                {
                    //Remove the station from previous side's list of stations
                    if (pcluster)
                        pcluster->GetClusterSite()->DeleteScanner(psideOld->GetObjectID(), this);
                    psideOld->DeleteStation(this);
                }

                TmodelIGC<IstationIGC>::SetSide(psideNew);

                if (psideNew != nullptr)
                {
                    //Add the station to the new side's list of stations
                    psideNew->AddStation(this);
                    if (pcluster)
                        pcluster->GetClusterSite()->AddScanner(psideNew->GetObjectID(), this);

                    GetHitTest()->SetUseTrueShapeSelf(psideNew);
                }

                if (pcluster != nullptr)
                    pcluster->GetClusterSite()->MoveStation();
            }
        }

    // IdamageIGC
        DamageResult        ReceiveDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  const Vector&           position1,
                                                  const Vector&           position2,
                                                  ImodelIGC*              launcher) override;

        float   GetFraction(void) const override
        {
            return m_hullFraction;
        }
        void    SetFraction(float newVal) override
        {
            m_hullFraction = newVal;
        }

        float               GetHitPoints(void) const override
        {
            return m_hullFraction * (float)m_myStationType.GetMaxArmorHitPoints();
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
                    float   m = m_myStationType.GetScannerRange() * pModel->GetSignature();
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

    // IstationIGC
        const IstationTypeIGC*  GetStationType(void) const override
        {
            return &m_myStationType;
        }

        IstationTypeIGC*        GetBaseStationType(void) const override
        {
            return m_myStationType.GetStationType();
        }

        void                SetBaseStationType(IstationTypeIGC* pst) override;

        float   GetShieldFraction(void) const override
        {
            return m_shieldFraction;
        }
        void    SetShieldFraction(float newVal) override
        {
            if (newVal < 0.0f)
                newVal = 0.0f;
            else if (newVal > 1.0f)
                newVal = 1.0f;

            m_shieldFraction = newVal;
        }

        bool                    CanBuy(const IbuyableIGC* b) const override
        {
            IsideIGC*   s = GetSide();
            assert (s);

            assert (b->GetObjectType() != OT_bucket);

            return b->GetRequiredTechs() <= (m_myStationType.GetLocalTechs() | s->GetTechs());
        }

        bool                    IsObsolete(IbuyableIGC* b) const override
        {
            IbuyableIGC*    pSuccessor = GetSuccessor(b);
            return (pSuccessor != b);
        }

        IbuyableIGC*    GetSuccessor(IbuyableIGC* b) const override
        {
            assert (b);

            IbuyableIGC*    pSuccessor = b;

            if (CanBuy(b))
            {
                IbuyableIGC*    pNext = pSuccessor;
                ObjectType  type = b->GetObjectType();
                while (true)
                {
                    switch (type)
                    {
                        case OT_partType:
                        {
                            pNext = ((IpartTypeIGC*)pNext)->GetSuccessorPartType();
                        }
                        break;
                        case OT_hullType:
                        {
                            pNext = ((IhullTypeIGC*)pNext)->GetSuccessorHullType();
                        }
                        break;
                        default:
                        {
                            pNext = nullptr;
                        }
                    }

                    if (pNext == nullptr)
                        break;

                    if (CanBuy(pNext))
                        pSuccessor = pNext;
                }

                assert (pSuccessor);
            }

            return pSuccessor;                   
        }

        IpartTypeIGC*           GetSimilarPart(IpartTypeIGC* ppt) const override
        {
            //Walk through the predecessor parts until one is found that can be bought
            //We need to make only a single pass because sucessors always precede their predecessors
            for (PartTypeLinkIGC*   pptl = GetMission()->GetPartTypes()->first(); (pptl != nullptr); pptl = pptl->next())
            {
                IpartTypeIGC*   pptTry = pptl->data();
                if (pptTry->GetSuccessorPartType() == ppt)
                {
                    if (CanBuy(pptTry))
                        return pptTry;
                    else
                        ppt = pptTry;
                }
            }

            return nullptr;
        }
        
        void                    AddShip(IshipIGC* s) override;
        void                    DeleteShip(IshipIGC* s) override;
        IshipIGC*               GetShip(ShipID shipID) const override;
        const ShipListIGC*      GetShips(void) const override;

        void                    Launch(IshipIGC* pship) override;
        void                    RepairAndRefuel(IshipIGC* pship) const override;
        bool                    InGarage(IshipIGC* pship, const Vector& position) override;

        void                    SetLastDamageReport(Time timeLastDamage) override
        {
            m_timeLastDamageReport = timeLastDamage;
        }
        Time                    GetLastDamageReport(void) const override
        {
            return m_timeLastDamageReport;
        }

        SoundID                 GetInteriorSound() const override
        {
            return (GetFraction() > 0.8) ? m_myStationType.GetInteriorSound() 
                : m_myStationType.GetInteriorAlertSound();
        }
        SoundID                 GetExteriorSound() const override
        {
            return m_myStationType.GetExteriorSound();
        }

    private:
        MyStationType               m_myStationType;

        float                       m_hullFraction;
        float                       m_shieldFraction;

        Time                        m_timeLastDamageReport;

        ShipListIGC                 m_shipsDocked;
        StationID                   m_stationID;
        unsigned char               m_undockPosition;
};

#endif //__STATIONIGC_H_
