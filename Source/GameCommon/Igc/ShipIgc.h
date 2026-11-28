/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    ShipIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CshipIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ShipIgc.h : Declaration of the CshipIGC

#ifndef __SHIPIGC_H_
#define __SHIPIGC_H_

#include    "ModelIgc.h"

const float c_dtCheckRunaway = 31.0f;   //Must be slightly longer than ripcord time for drones.

class       CshipIGC;

class       MyHullType : public IhullTypeIGC
{
    public:
        MyHullType(CshipIGC*    pship)
        :
            m_pship(pship),
            m_pHullType(NULL)
        {
        }

        ~MyHullType(void)
        {
            if (m_pHullType)
                m_pHullType->Release();
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

        //IhullTypeIGC
        float                GetLength(void) const override;
        float                GetMaxSpeed(void) const override;
        float                GetMaxTurnRate(Axis        axis) const override;
        float                GetTurnTorque(Axis   axis) const override;
        float                GetThrust(void) const override;
        float                GetSideMultiplier(void) const override;
        float                GetBackMultiplier(void) const override;
        float                GetScannerRange(void) const override;

        float                GetMaxEnergy(void) const override;
        float                GetRechargeRate(void) const override;

        HitPoints            GetHitPoints(void) const override;
        DefenseTypeID        GetDefenseType(void) const override;
        bool                 CanMount(IpartTypeIGC* ppt, Mount  mountID) const override;
        PartMask             GetPartMask(EquipmentType et, Mount mountID) const override;
        short                GetCapacity(EquipmentType et) const override;
        Mount                GetMaxWeapons(void) const override;
        Mount                GetMaxFixedWeapons(void) const override;
        const HardpointData& GetHardpointData(Mount hardpointID) const override;                       

        const char*          GetTextureName(void) const override;
        const char*          GetIconName(void) const override;

        float                GetMass(void) const override;
        float                GetSignature(void) const override;

        HullAbilityBitMask   GetCapabilities(void) const override;
        bool                 HasCapability(HullAbilityBitMask habm) const override;

        const Vector&        GetCockpit(void) const override;

        const Vector&        GetWeaponPosition(Mount mount) const override;
        const Orientation&   GetWeaponOrientation(Mount mount) const override;
        float                GetScale() const override;

        short                GetMaxAmmo(void) const override;
        float                GetMaxFuel(void) const override;
        float                GetECM(void) const override;
        float                GetRipcordSpeed(void) const override;
        float                GetRipcordCost(void) const override;

        IhullTypeIGC*        GetSuccessorHullType(void) const override;
        /*
        virtual const char*          GetPilotHUDName(void) const;
        virtual const char*          GetObserverHUDName(void) const;
        */

        SoundID              GetInteriorSound(void) const override;
        SoundID              GetExteriorSound(void) const override;
        SoundID              GetMainThrusterInteriorSound(void) const override;
        SoundID              GetMainThrusterExteriorSound(void) const override;
        SoundID              GetManuveringThrusterInteriorSound(void) const override;
        SoundID              GetManuveringThrusterExteriorSound(void) const override;

        const PartTypeListIGC*      GetPreferredPartTypes(void) const override;

        IObject*            GetIcon(void) const override;

        int                     GetLaunchSlots(void) const override;
        const Vector&           GetLaunchPosition(int   slotID) const override;
        const Vector&           GetLaunchDirection(int   slotID) const override;

        int                     GetLandSlots(void) const override;
        int                     GetLandPlanes(int   slotID) const override;
        const Vector&           GetLandPosition(int slotID, int planeID) const override;
        const Vector&           GetLandDirection(int slotID, int planeID) const override;

    private:
        IhullTypeIGC*         GetHullType(void) const
        {
            return m_pHullType;
        }

        void SetHullType(IhullTypeIGC*  pht)
        {
            if (m_pHullType)
                m_pHullType->Release();

            m_pHullType = pht;

            if (pht)
            {
                pht->AddRef();
                m_pHullData = (DataHullTypeIGC*)(pht->GetData());
            }
            else
                m_pHullData = NULL;
        }

        const CshipIGC*         m_pship;
        IhullTypeIGC*           m_pHullType;
        const DataHullTypeIGC*  m_pHullData;

        friend class CshipIGC;
};

class       CshipIGC : public TmodelIGC<IshipIGC>
{
    public:
        CshipIGC(void);

        /*
        DWORD __stdcall  AddRef(void)
        {
            return TmodelIGC<IshipIGC>::AddRef();
        }

        DWORD __stdcall  Release(void)
        {
            return TmodelIGC<IshipIGC>::Release();
        }
        */

    public:
    // IbaseIGC
        HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
        void                Terminate(void) override;
        void                Update(Time now) override;
        int                 Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_ship;
        }

        ObjectID            GetObjectID(void) const override
        {
            return m_shipID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            IclusterIGC*    pclusterOld = GetCluster();

            if (cluster || pclusterOld)
            {
                //Can't move to a cluster or move to a cluster without a hull
                assert ((!cluster) || (m_myHullType.GetHullType() != NULL) || (m_pshipParent != NULL));
                assert (GetSide() != NULL);

                SetRipcordModel(NULL);

                if (cluster != pclusterOld)
                {
                    if (pclusterOld)
                        pclusterOld->DeleteShip(this);

                    TmodelIGC<IshipIGC>::SetCluster(cluster);

                    if (cluster)
                    {
                        //Can't be in a cluster and in a station simultaneously
                        assert (m_station == NULL);

                        cluster->AddShip(this);
                        m_maxMineAmount = 0.0f;
                        m_maxMineLauncher = NULL;
                    }
                }

                if (cluster)
                {
                    //Do this even if the cluster does not change: the ship
                    //mostly likely teleported to a new position.
                    Time    lastUpdate = cluster->GetLastUpdate();
                    SetMyLastUpdate(lastUpdate);
                    SetBB(lastUpdate, lastUpdate, 0.0f);
                }

                SetStateBits(oneWeaponIGC | allWeaponsIGC, 0); //Stop shooting any weapons.

                {
                    //Move the children of this ship to the new cluster as well
                    for (ShipLinkIGC*   psl = m_shipsChildren.first();
                         (psl != NULL);
                         psl = psl->next())
                    {
                        psl->data()->SetCluster(cluster);
                    }
                }

                GetMyMission()->GetIgcSite()->ChangeCluster(this, pclusterOld, cluster);

                if (pclusterOld == NULL)
                    ResetWaypoint();
            }
        }

        void                 HandleCollision(Time       timeCollision,
                                                     float                  tCollision,
                                                     const CollisionEntry&  entry,
                                                     ImodelIGC* pModel) override;

        void                SetSide(IsideIGC*   pside) override;  //override the default SetSide method

        float               GetSignature(void) const override
        {
            return TmodelIGC<IshipIGC>::GetSignature() * m_cloaking;
        }

        void                 Move(float t) override
        {
            if (m_pshipParent == NULL)
                TmodelIGC<IshipIGC>::Move(t);
        }
        void                 Move(void) override
        {
            if (m_pshipParent == NULL)
                TmodelIGC<IshipIGC>::Move();
        }
        void                 SetBB(Time tStart, Time tStop, float dt) override
        {
            if (m_pshipParent == NULL)
                TmodelIGC<IshipIGC>::SetBB(tStart, tStop, dt);
        }

        SideID              GetFlag(void) const override
        {
            return m_sidFlag;
        }

        void                 SetFlag(SideID sid) override
        {
            m_sidFlag = sid;
        }

    // IdamageIGC
        DamageResult        ReceiveDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  const Vector&           position1,
                                                  const Vector&           position2,
                                                  ImodelIGC*              launcher) override;

        float               GetFraction(void) const override
        {
            return m_fraction;
        }
        void                SetFraction(float newVal) override
        {
            assert (newVal >= 0.0f);
            assert (newVal <= 1.0f);
            m_fraction = newVal;
            if (newVal > m_fractionLastOrder)
                m_fractionLastOrder = newVal;
            GetThingSite ()->RemoveDamage (m_fraction);
        }

        float               GetHitPoints(void) const override
        {
            return m_fraction * m_myHullType.GetHitPoints();
        }

        // IscannerIGC
        bool            InScannerRange(ImodelIGC*   pModel) const override
        {
            assert (pModel);

            bool    rc;
            if (m_pshipParent == NULL)
            {
                IclusterIGC*    pcluster = GetCluster();
                if (pcluster && (pModel->GetCluster() == pcluster))
                {
                    if (pModel->GetFlag() == NA)
                    {
                        float   m = m_myHullType.GetScannerRange() * pModel->GetSignature();
                        {
                            IsideIGC*   pside = pModel->GetSide();
                            if (pside)
                                m /= pside->GetGlobalAttributeSet().GetAttribute(c_gaSignature);
                        }
                        float   r = GetRadius() + pModel->GetRadius() + m;

                        //Get the difference in positions in the local coordinate space.
                        Vector  deltaP = GetOrientation().TimesInverse(GetPosition() - pModel->GetPosition());
                        if (deltaP.z < 0.0f)
                            deltaP.z *= 4.0f;

                        rc = (deltaP.LengthSquared() <= r * r) && LineOfSightExist(pcluster, this, pModel);
                    }
                    else
                        rc = true;
                }
                else
                    rc = false;
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

    // IshipIGC
        void                SetObjectID(ObjectID id) override
        {
            m_shipID = id;
        }
        Money               GetValue(void) const override
        {
            //Money for the hull
            Money money = m_myHullType.GetHullType() ? m_myHullType.GetPrice() : 0;

            //Money for the parts
            for (PartLinkIGC* ppartlink = m_parts.first(); ppartlink; ppartlink = ppartlink->next())
                money += ppartlink->data()->GetPrice();

            return money;
        }

        void                SetMission(ImissionIGC* pMission) override;

        void                ReInitialize(DataShipIGC * dataShip, Time now) override;

        virtual void                SetShipID(ShipID    shipID)
        {
            m_shipID = shipID;
        }

        IstationIGC*         GetStation(void) const override
        {
            return m_station;
        }
        void                 SetStation(IstationIGC* s) override
        {
            IstationIGC*    pstationOld = m_station;
            if (pstationOld != s)
            {
                if (m_station)
                {
                    m_station->DeleteShip(this);
                    m_station->Release();
                }

                m_station = s;

                if (s)
                {
                    if (m_pshipParent == NULL)
                    {
                        IafterburnerIGC*    pafter = (IafterburnerIGC*)(m_mountedOthers[ET_Afterburner]);
                        if (pafter)
                            pafter->Deactivate();   //The station is a no smoking area

                        if ((m_fOre > 0.0f) && (s->GetStationType()->HasCapability(c_sabmUnload)))
                        {
                            IsideIGC*   pside = GetSide();

                            GetMyMission()->GetIgcSite()->PaydayEvent(pside,
                                                                      m_fOre *
                                                                      GetMyMission()->GetFloatConstant(c_fcidValueHe3) *
                                                                      pside->GetGlobalAttributeSet().GetAttribute(c_gaMiningYield));
                            m_fOre = 0.0f;
                        }

                        ResetDamageTrack();
                    }

                    s->AddRef();
                    s->AddShip(this);           //This also sets the ship's cluster to NULL

                    assert (GetCluster() == NULL);

                    if (m_bAutopilot)
                    {
                        if (m_pilotType >= c_ptPlayer)
                        {
                            //Players always exit autopilot when docking
                            m_bAutopilot = false;
                        }
                        else
                        {
                            if ((m_commandTargets[c_cmdAccepted] == s) && (m_commandIDs[c_cmdAccepted] == c_cidGoto))
                            {
                                //We docked with the station under autopilot ... that order is complete
                                SetCommand(c_cmdAccepted, NULL, c_cidNone);
                            }
                            else
                            {
                                SetCommand(c_cmdPlan, NULL, c_cidNone);
                            }
                        }
                    }
                }

                GetMyMission()->GetIgcSite()->ChangeStation(this, pstationOld, s);
            }
        }

        void                 Reset(bool bFull) override
        {
            m_nKills = 0;
            if (bFull)
            {
                m_nDeaths = 0;
                m_nEjections = 0;
            }

            m_experience = bFull ? 1.0f : 0.0f;

            SetFlag(NA);
            ResetDamageTrack();

            SetAutoDonate(NULL);

            SetCommand(c_cmdQueued, NULL, c_cidNone);
            SetCommand(c_cmdAccepted, NULL, c_cidNone);
            SetCommand(c_cmdCurrent, NULL, c_cidNone);
            SetCommand(c_cmdPlan, NULL, c_cidNone);

            if (m_station)
            {
                m_station->DeleteShip(this);
                m_station->Release();
                m_station = NULL;
            }
            SetRipcordModel(NULL);

            SetCluster(NULL);
            //TmodelIGC<IshipIGC>::SetCluster(NULL);
            assert (m_station == NULL);

            if (m_pshipParent != NULL)
            {
                SetParentShip(NULL);
                assert (m_myHullType.GetHullType() == NULL);
            }
            else
            {
                PartLinkIGC*    pl;
                while (pl = m_parts.first())    //intentional assignment
                    pl->data()->Terminate();

                //Blow away the children
                ShipLinkIGC*    psl;
                while (psl = m_shipsChildren.first())   //intentional assignment
                {
                    psl->data()->SetParentShip(NULL);
                }
            }            

            SetBaseHullType(GetSide()->GetCivilization()->GetLifepod());
        }

        float               GetTorqueMultiplier(void) const override
        {
            static const float  c_fMultiplierAtZero = 0.50f;

            float   fraction = GetVelocity().Length() / m_myHullType.GetMaxSpeed();

            return  c_fMultiplierAtZero + (1.0f - c_fMultiplierAtZero) * 2.0f * fraction / (fraction + 1.0f);
        }

        float               GetCurrentTurnRate(Axis axis) const override
        {
            return m_turnRates[axis];
        }
        void                SetCurrentTurnRate(Axis axis, float newVal) override
        {
            m_turnRates[axis] = newVal;
        }

        const IhullTypeIGC*       GetHullType(void) const override
        {
            return &m_myHullType;
        }

        IhullTypeIGC*       GetBaseHullType(void) const override
        {
            return m_myHullType.GetHullType();
        }
        void                SetBaseHullType(IhullTypeIGC* newVal) override;

        void                AddPart(IpartIGC*    part) override;
        void                DeletePart(IpartIGC* part) override;
        const PartListIGC*  GetParts(void) const override
        {
            return &m_parts;
        }

        IpartIGC*           GetMountedPart(EquipmentType type, Mount mount) const override;
        void                MountPart(IpartIGC* p, Mount mountNew, Mount* pmountOld) override;

        virtual const Vector&       GetCockpit(void) const
        {
            return m_cockpit;
        }

        short                 GetAmmo(void) const override
        {
            return m_ammo;
        }

        void                SetAmmo(short newVal) override
        {
            short   maxAmmo = m_myHullType.GetMaxAmmo();
            if (newVal > maxAmmo)
                newVal = maxAmmo;
            
            short   oldAmmo = m_ammo;
            m_ammo = newVal;

            if ((newVal == 0) && (oldAmmo > 0) && (GetMyLastUpdate() >= m_timeReloadAmmo))
            {
                m_timeReloadAmmo = GetMyLastUpdate() + 0.5f / GetMyMission()->GetFloatConstant(c_fcidMountRate);
                IIgcSite*   pigc = GetMyMission()->GetIgcSite();
                if (!pigc->Reload(this, NULL, ET_Weapon))
                {
                    pigc->PostNotificationText(this, false, "Ammo depleted.");
                }
                else
                {
                    pigc->PlayNotificationSound(salReloadingAmmoSound, this);
                    pigc->PlayNotificationSound(startReloadSound, this);
                    pigc->PostNotificationText(this, false, "Reloading ammo...");
                }
            }
        }

        float               GetFuel(void) const override
        {
            return m_fuel;
        }
        void                SetFuel(float newVal) override
        {
            float   maxFuel = m_myHullType.GetMaxFuel();
            if (newVal > maxFuel)
                newVal = maxFuel;

            float   oldFuel = m_fuel;
            m_fuel = newVal;

            if ((newVal == 0.0f) && (oldFuel > 0.0f) && (GetMyLastUpdate() >= m_timeReloadFuel))
            {
                m_timeReloadFuel = GetMyLastUpdate() + 0.5f / GetMyMission()->GetFloatConstant(c_fcidMountRate);
                IIgcSite*   pigc = GetMyMission()->GetIgcSite();
                if (!pigc->Reload(this, NULL, ET_Afterburner))
                {
                    pigc->PostNotificationText(this, false, "Fuel depleted.");
                }
                else
                {
                    pigc->PlayNotificationSound(salReloadingFuelSound, this);
                    pigc->PlayNotificationSound(startReloadSound, this);
                    pigc->PostNotificationText(this, false, "Reloading fuel...");
                }
            }
        }

        float               GetEnergy(void) const override
        {
            return m_energy;
        }
        void                SetEnergy(float newVal) override
        {
            m_energy = newVal;
        }

        float                GetCloaking(void) const override
        {
            return m_cloaking;
        }
        void                 SetCloaking(float newVal) override
        {
            m_cloaking = newVal;
        }

        bool                GetVectorLock(void) const override
        {
            return (m_stateM & coastButtonIGC) ? true : false;
        }
        void                SetVectorLock(bool bVectorLock) override
        {
            if (bVectorLock)
                m_stateM |= coastButtonIGC;
            else
                m_stateM &= ~coastButtonIGC;
        }

        const ControlData&  GetControls(void) const override
        {
            return m_controls;
        }
        void                SetControls(const ControlData&  newVal) override
        {
            m_controls = newVal;
        }

        const Vector&       GetEngineVector(void) const override
        {
            return m_engineVector;
        }

        int                 GetStateM(void) const override
        {
            return m_stateM;
        }
        void                SetStateM(int newVal) override;
        void                SetStateBits(int mask, int newBits) override
        {
            SetStateM((m_stateM & (~mask)) | (newBits & mask));
        }

        ImodelIGC*           GetCommandTarget(Command i)  const override
        {
            assert (i >= 0);
            assert (i < c_cmdMax);

            return m_commandTargets[i];
        }
        CommandID            GetCommandID(Command i) const override
        {
            assert (i >= 0);
            assert (i < c_cmdMax);

            return m_commandIDs[i];
        }
        void                 SetCommand(Command i, ImodelIGC* target, CommandID cid) override
        {
            assert (i >= 0);
            assert (i < c_cmdMax);

            CommandID cidOld = m_commandIDs[i];
            ImodelIGC*  pmodelOld = m_commandTargets[i];
            //if ((target != pmodelOld) || (cid != m_commandIDs[i]))
            {
                m_commandTargets[i] = target;
                if (target)
                {
                    target->AddRef();
                    if (target->GetObjectType() == OT_buoy)
                        ((IbuoyIGC*)target)->AddConsumer();
                }

                m_commandIDs[i] = cid;

                switch (i)
                {
                    case c_cmdCurrent:
                    {
                        if ((pmodelOld != target) && m_mountedOthers[ET_Magazine])
                            ((ImagazineIGC*)m_mountedOthers[ET_Magazine])->SetLock(0.0f);
                    }
                    break;

                    case c_cmdPlan:
                    {
                        //Clear the drilling mask (either it wasn't set or it no longer applies).
                        m_stateM &= ~drillingMaskIGC;

                        ResetWaypoint();

                        m_fractionLastOrder = m_fraction;
                        m_timeRanAway = GetMyLastUpdate();
                        m_bRunningAway = false;
                    }
                    break;

                    case c_cmdAccepted:
                    {
                        if (m_bAutopilot && (m_pilotType < c_ptPlayer))
                        {
                            SetCommand(c_cmdCurrent, target, cid);
                            SetCommand(c_cmdPlan, target, cid);
                        }
                    }
                    break;
                }

                if (pmodelOld)
                {
                    if (pmodelOld->GetObjectType() == OT_buoy)
                        ((IbuoyIGC*)pmodelOld)->ReleaseConsumer();
                    pmodelOld->Release();
                }

                if ((i == c_cmdAccepted || i == c_cmdCurrent) && 
                    (cidOld != cid || pmodelOld != target))
                    GetMyMission()->GetIgcSite()->CommandChangedEvent(i, this, target, cid);
            }
        }

        void                 ExecuteTurretMove(Time          timeStart,
                                                      Time          timeStop,
                                                      Orientation*  pOrientation) override;

        void                 PreplotShipMove(Time          timeStop) override;
        void                 PlotShipMove(Time          timeStop) override;
        void                 ExecuteShipMove(Time          timeStop) override;

        void                 ExecuteShipMove(Time          timeStart,
                                                     Time          timeStop,
                                                     Vector*       pVelocity,
                                                     Orientation*  pOrientation) override;
        short               ExportShipLoadout(ShipLoadout*  ploadout) override
        {
            if (ploadout)
            {
                ploadout->hullID = m_myHullType.GetObjectID();
                assert (ploadout->hullID != NA);

                ExpandedPartData*   ppd = ploadout->PartData0();
                for (PartLinkIGC*   ppl = m_parts.first(); (ppl != NULL); ppl = ppl->next())
                {
                    IpartIGC*   ppart = ppl->data();

                    ppd->partID              = ppart->GetPartType()->GetObjectID();
                    ppd->mountID             = ppart->GetMountID();
                    ppd->amount              = ppart->GetAmount();
                    (ppd++)->fractionMounted = ppart->GetMountedFraction();
                }
            }
            return sizeof(ShipLoadout) + sizeof(ExpandedPartData) * m_parts.n();
        }

        bool                EquivalentShip(IshipIGC* pship) const override
        {
            if (m_myHullType.GetHullType() != pship->GetBaseHullType())
                return false;
            else
            {
                Mount   maxWeapons = m_myHullType.GetMaxWeapons();
                for (Mount i = 0; (i < maxWeapons); i++)
                {
                    IpartIGC*   p1 = m_mountedWeapons[i];
                    IpartIGC*   p2 = pship->GetMountedPart(ET_Weapon, i);

                    if ((p1 != NULL) || (p2 != NULL))
                    {
                        if ((p1 == NULL) || (p2 == NULL) ||
                            (p1->GetPartType() != p2->GetPartType()))
                        {
                            return false;
                        }
                    }
                }

                for (Mount j = 0; (j < ET_MAX); j++)
                {
                    IpartIGC*   p1 = GetMountedPart(j, 0);
                    IpartIGC*   p2 = pship->GetMountedPart(j, 0);

                    if ((p1 != NULL) || (p2 != NULL))
                    {
                        if ((p1 == NULL) || (p2 == NULL) ||
                            (p1->GetPartType() != p2->GetPartType()) ||
                            (p1->GetAmount() != p2->GetAmount()))
                        {
                            return false;
                        }
                    }
                }

                for (Mount k = -1; (k >= -c_maxCargo); k--)
                {
                    IpartIGC*   p1 = m_mountedCargos[k + c_maxCargo];
                    IpartIGC*   p2 = pship->GetMountedPart(NA, k);

                    if ((p1 != NULL) || (p2 != NULL))
                    {
                        if ((p1 == NULL) || (p2 == NULL) ||
                            (p1->GetPartType() != p2->GetPartType()) ||
                            (p1->GetAmount() != p2->GetAmount()))
                        {
                            return false;
                        }
                    }
                }
            }

            return true;
        }
        void                ProcessShipLoadout(short                cbLoadout,
                                                       const ShipLoadout*   ploadout,
                                                       bool                 bReady) override
        {
            //Did the hull change?
            {
                IhullTypeIGC*   pht = m_myHullType.GetHullType();
                if ((pht == NULL) || (ploadout->hullID != pht->GetObjectID()))
                {
                    //Yes ... change it. Trash all the parts first, though
                    {
                        PartLinkIGC*    ppl;
                        while (ppl = m_parts.first())   //Intentional
                            ppl->data()->Terminate();
                    }
                    SetBaseHullType(GetMyMission()->GetHullType(ploadout->hullID));
                }
            }

            const ExpandedPartData* ppartLC     = ploadout->PartData0();
            const ExpandedPartData* ppdLastPart = ploadout->PartDataN(cbLoadout);

            PartLinkIGC*            pplShip = m_parts.first();

            //Walk both lists in parallel, looking for differences
            while (true)
            {
                if (ppartLC != ppdLastPart)
                {
                    //We have a part in the message
                    if (pplShip)
                    {
                        //We have a corresponding part on the ship ... are they the same part?
                        IpartIGC*   ppart = pplShip->data();
            
                        if (ppart->GetPartType()->GetObjectID() == ppartLC->partID)
                        {
                            //Yes
                
                            //has the mount changed?
                            if (ppart->GetMountID() != ppartLC->mountID)
                            {
                                //Is there another part already mounted in the desired slot?
                                //If so, remove it
                                IpartIGC*   ppartMounted = GetMountedPart(ppart->GetEquipmentType(), ppartLC->mountID);
                                if (ppartMounted)
                                    ppartMounted->Terminate();

                                ppart->SetMountID(ppartLC->mountID);
                            }

                            //has the quantity changed (for parts where that matters)?
                            ObjectType  type = ppart->GetObjectType();
                            if (ppartLC->amount != ppart->GetAmount())
                            {
                                ppart->SetAmount(ppartLC->amount);
                            }

                            //In any case ... set the mounted fraction to agree with the fraction specified in the part definition
                            if (bReady)
                                ppart->Arm();
                            else
                                ppart->SetMountedFraction(ppartLC->fractionMounted);

                            //Go to the next part pair.
                            pplShip = pplShip->next();
                            ppartLC++;
                        }
                        else
                        {
                            //No ... delete this part and go to the next part on the ship
                            //while not advancing the ppartLC pointer
                            pplShip = pplShip->next();

                            ppart->Terminate();
                        }
                    }
                    else
                    {
                        //No corresponding part on the ship: create one
                        IpartIGC* ppart = CreateAndAddPart(ppartLC);
                        if (bReady)
                            ppart->Arm();
                        else
                            ppart->SetMountedFraction(ppartLC->fractionMounted);

                        ppartLC++;
                    }
                }
                else if (pplShip)
                {
                    //No part in the message but one on the ship. Nuke it.
                    IpartIGC*   ppart = pplShip->data();

                    //Go to the next part before the nuke
                    pplShip = pplShip->next();

                    ppart->Terminate();
                }
                else
                {
                    //No parts in either the  message or the ship
                    //All done.
                    break;
                }
            }
        }
        bool                PurchaseShipLoadout(short                cbLoadout,
                                                        const ShipLoadout*   ploadout) override
        {
            assert (m_station);

            //Did the hull change?
            IhullTypeIGC*   pht = m_myHullType.GetHullType();
            if ((pht == NULL) || (ploadout->hullID != pht->GetObjectID()))
            {
                pht = GetMyMission()->GetHullType(ploadout->hullID);

                if (!(pht && m_station->CanBuy(pht)))
                    return false;               //Can't buy the hull: abort

                //Select the successor of the part.
                pht = (IhullTypeIGC*)(m_station->GetSuccessor(pht));
            }

            //So far so good ... trash the parts
            PartListIGC     partsOld;
            {
                PartLinkIGC*    ppl;
                while (ppl = m_parts.first())   //Intentional
                {
                    IpartIGC*   ppart = ppl->data();
                    ppart->AddRef();
                    ppart->SetShip(NULL, NA);
                    partsOld.last(ppart);
                }
            }
            if (pht != m_myHullType.GetHullType())
                SetBaseHullType(pht);

            const ExpandedPartData* ppdNext     = ploadout->PartData0();
            const ExpandedPartData* ppdLastPart = ploadout->PartDataN(cbLoadout);

            bool    bComplete = true;
            while (ppdNext < ppdLastPart)
            {
                IpartTypeIGC*   ppt = GetMyMission()->GetPartType(ppdNext->partID);
                if (ppt &&
                    m_myHullType.CanMount(ppt, ppdNext->mountID) &&
                    (GetMountedPart(ppt->GetEquipmentType(), ppdNext->mountID) == NULL))
                {
                    //Use an upgraded version if available
                    if (m_station->CanBuy(ppt))
                    {
                        ppt = (IpartTypeIGC*)(m_station->GetSuccessor(ppt));
                        assert (m_myHullType.CanMount(ppt, ppdNext->mountID));

                        IpartIGC*   ppart = CreateAndAddPart(ppt, ppdNext->mountID, ppdNext->amount);
                        ppart->Arm();
                    }
                    else
                    {
                        //Can't buy the part: can we re-use an existing part?
                        for (PartLinkIGC*   pplOld = partsOld.first(); (pplOld != NULL); pplOld = pplOld->next())
                        {
                            if (ppt == pplOld->data()->GetPartType())
                            {
                                IpartIGC*   ppart = pplOld->data();

                                ppart->SetShip(this, ppdNext->mountID);
                                ppart->Arm();

                                ppart->Release();
                                delete pplOld;

                                //If this is an expendable ... scavenge all the old parts to fill it up
                                if (IlauncherTypeIGC::IsLauncherType(ppt->GetEquipmentType()))
                                {
                                    short   maxAmmo = ppdNext->amount;
                                    short   ammo = ppart->GetAmount();
                                    if (ammo < maxAmmo)
                                    {
                                        PartLinkIGC*    pplNext;
                                        for (PartLinkIGC*   pplOther = partsOld.first(); (pplOther != NULL); pplOther = pplNext)
                                        {
                                            pplNext = pplOther->next();

                                            IpartIGC*   ppartOther = pplOther->data();
                                            if (ppartOther->GetPartType() == ppt)
                                            {
                                                short   newAmmo = ammo + ppartOther->GetAmount();
                                                if (newAmmo > maxAmmo)
                                                {
                                                    ppartOther->SetAmount(maxAmmo - newAmmo);
                                                    newAmmo = maxAmmo;
                                                }
                                                else
                                                {
                                                    ppartOther->Terminate();
                                                    ppartOther->Release();

                                                    delete pplOther;
                                                }

                                                ppart->SetAmount(newAmmo);
                                            }
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                else
                    bComplete = false;

                ppdNext++;
            }

            {
                PartLinkIGC*    ppl;
                while (ppl = partsOld.first())  //Intentional
                {
                    ppl->data()->Terminate();
                    ppl->data()->Release();
                    delete ppl;
                }
            }

            return bComplete;
        }

        void                ExportFractions(CompactShipFractions* pfractions) const override
        {
            pfractions->SetHullFraction(m_fraction);
            {                                                                   
                IshieldIGC* s = (IshieldIGC*)(m_mountedOthers[ET_Shield]);
                pfractions->SetShieldFraction(s ? s->GetFraction() : 0.0f);
            }                                                             
            pfractions->SetFuel(m_myHullType.GetMaxFuel(), m_fuel);  
            pfractions->SetAmmo(m_myHullType.GetMaxAmmo(), m_ammo);   
            pfractions->SetEnergy(m_myHullType.GetMaxEnergy(), m_energy);
        }


#define SetSC           pshipupdate->stateM.Set(m_stateM);                  \
                        pshipupdate->controls.Set(m_controls);

#define SetOVTP         pshipupdate->orientation.Set(GetOrientation());                                         \
                        pshipupdate->velocity.Set(GetVelocity());                                               \
                        pshipupdate->turnRates.Set(m_turnRates);                                                \
                        {                                                                                       \
                            IafterburnerIGC* p = (IafterburnerIGC*)(m_mountedOthers[ET_Afterburner]);           \
                            pshipupdate->power = (p ? BytePercentage(p->GetPower()) : BytePercentage(0.0f));    \
                        }

#define SetF            pshipupdate->fractions.SetHullFraction(m_fraction);                             \
                        {                                                                               \
                            IshieldIGC* s = (IshieldIGC*)(m_mountedOthers[ET_Shield]);                  \
                            pshipupdate->fractions.SetShieldFraction(s ? s->GetFraction() : 0.0f);      \
                        }                                                                               \
                        pshipupdate->fractions.SetFuel(m_myHullType.GetMaxFuel(), m_fuel);              \
                        pshipupdate->fractions.SetAmmo(m_myHullType.GetMaxAmmo(), m_ammo);              \
                        pshipupdate->fractions.SetEnergy(m_myHullType.GetMaxEnergy(), m_energy);

        void                ExportShipUpdate(ServerLightShipUpdate*     pshipupdate) const override
        {
            pshipupdate->shipID = m_shipID;

            SetSC;
        }
        void                ExportShipUpdate(Time                       timeReference,
                                                     const Vector&              positionReference,
                                                     ServerHeavyShipUpdate*     pshipupdate) const override
        {
            assert (GetPosition().LengthSquared() != 0.0f);
            pshipupdate->shipID = m_shipID;

            pshipupdate->time.Set(timeReference, GetMyLastUpdate());
            pshipupdate->position.Set(positionReference, GetPosition());

            SetSC;

            SetOVTP;

            SetF;
        }

        void                ExportShipUpdate(ClientShipUpdate*     pshipupdate) const override
        {
            assert (GetPosition().LengthSquared() != 0.0f);

            //Message generated on the client so convert to server time
            pshipupdate->time = GetMyMission()->GetIgcSite()->ServerTimeFromClientTime(GetMyLastUpdate());
            pshipupdate->position = GetPosition();

            SetSC;

            SetOVTP;

            pshipupdate->orientation.Validate();
        }

        void                ExportShipUpdate(ServerSingleShipUpdate*     pshipupdate) const override
        {
            pshipupdate->shipID = m_shipID;

            pshipupdate->time = GetMyLastUpdate();
            pshipupdate->position = GetPosition();

            SetSC;

            SetOVTP;

            SetF;
        }

#undef  SetSC
#undef  SetOVTP
#undef  SetF

        void                ExportShipUpdate(ClientActiveTurretUpdate*  pshipupdate) const override
        {
            pshipupdate->time = GetMyMission()->GetIgcSite()->ServerTimeFromClientTime(GetMyLastUpdate());

            pshipupdate->controls.Set(m_controls);
            pshipupdate->orientation.Set(GetOrientation());
        }

        void                ProcessFractions(const CompactShipFractions& fractions) override;

        ShipUpdateStatus    ProcessShipUpdate(const ServerLightShipUpdate&      shipupdate) override;
        ShipUpdateStatus    ProcessShipUpdate(Time                              timeReference,
                                                      Vector                            positionReference,
                                                      const ServerHeavyShipUpdate&      shipupdate) override;

        ShipUpdateStatus    ProcessShipUpdate(const ClientShipUpdate&           shipupdate) override;
        ShipUpdateStatus    ProcessShipUpdate(const ServerSingleShipUpdate&     shipupdate, bool bOrient = true) override;

        ShipUpdateStatus    ProcessShipUpdate(Time                              timeReference,
                                                      const ServerActiveTurretUpdate&   shipupdate) override;
        ShipUpdateStatus    ProcessShipUpdate(const ClientActiveTurretUpdate&   shipupdate) override;


        void                SetPrivateData(DWORD_PTR dwPrivate) override
        {
            m_dwPrivate = dwPrivate;
        }

        DWORD_PTR           GetPrivateData(void) const override
        {
            return m_dwPrivate;
        }

        Mount   HitTreasure(TreasureCode treasureCode, ObjectID objectID, short amount) override
        {
            assert (m_myHullType.GetHullType());

            switch (treasureCode)
            {
                case c_tcPart:
                {
                    IpartTypeIGC*   ppt = GetMyMission()->GetPartType(objectID);
                    assert (ppt);

                    //Can we carry the thing?
                    EquipmentType   et = ppt->GetEquipmentType();
                    Mount           maxMounts = (et == ET_Weapon)
                                                ? m_myHullType.GetMaxWeapons()
                                                : 1;

                    //Try to mount the part in each possible location
                    IpartIGC*   ppart = NULL;
                    Mount       mount;
                    short       a{};

                    for (mount = 0; (mount < maxMounts); mount++)
                    {
                        if (m_myHullType.CanMount(ppt, mount))
                        {
                            ppart = *PartLocation(et, mount);
                            if (ppart == NULL)
                                break;

                            if ((ppart->GetPartType() == ppt) &&
                                IlauncherIGC::IsLauncher(ppart->GetObjectType()))
                            {
                                short   maxAmount = ppt->GetAmount(this);
                                a = ppart->GetAmount();
                                if ((a < maxAmount) && ((a + amount) <= maxAmount))
                                    break;
                            }
                        }
                    }

                    if (mount == maxMounts)
                    {
                        //No place to mount it ... see if there is room in cargo
                        for (mount = -1; (mount >= -c_maxCargo); mount--)
                        {
                            ppart = *PartLocation(NA, mount);
                            if (ppart == NULL)
                                break;

                            if ((ppart->GetPartType() == ppt) &&
                                IlauncherIGC::IsLauncher(ppart->GetObjectType()))
                            {
                                short   maxAmount = ppt->GetAmount(this);
                                a = ppart->GetAmount();
                                if ((a < maxAmount) && ((a + amount) <= maxAmount))
                                    break;
                            }
                        }

                        if (mount < -c_maxCargo)
                        {
                            //No place to put it in cargo
                            GetMyMission()->GetIgcSite()->PlayNotificationSound(salCargoFullSound, this);
                            return c_mountNA;
                        }
                    }

                    GetMyMission()->GetIgcSite()->PlayNotificationSound(pickUpPartSound, this);

                    if (ppart)
                        ppart->SetAmount(a + amount);
                    else
                        CreateAndAddPart(ppt, mount, amount);

                    return mount;
                }
                break;
                case c_tcDevelopment:
                {
                    IbucketIGC*         pb = (IbucketIGC*)GetIbaseIGC((BaseListIGC*)(GetSide()->GetBuckets()), objectID);
                    if (pb && !pb->GetCompleteF())
                    {
                        pb->ForceComplete(GetMyLastUpdate());
                    }
                }
                break;
                case c_tcPowerup:
                {
                    if (objectID & c_pcHull)
                        SetFraction(1.0f);

                    if (objectID & c_pcShield)
                    {
                        IshieldIGC* pshield = (IshieldIGC*)(m_mountedOthers[ET_Shield]);
                        if (pshield)
                            pshield->SetFraction(1.0f);
                    }

                    if (objectID & c_pcEnergy)
                        m_energy = m_myHullType.GetMaxEnergy();

                    if (objectID & c_pcFuel)
                        SetFuel(FLT_MAX);

                    if (objectID & c_pcAmmo)
                        SetAmmo(0x7fff);
                }
                break;
                case c_tcFlag:
                {
                    assert (m_sidFlag == NA);
                    assert (objectID != NA);
                    m_sidFlag = objectID;
                }
                break;
            }

            return c_mountNA;
        }

        ImissileIGC*        GetLastMissileFired(void) const override
        {
            return m_pmissileLast;
        }
        void                SetLastMissileFired(ImissileIGC* pmissile) override
        {
            if (m_pmissileLast)
                m_pmissileLast->Release();

            m_pmissileLast = pmissile;

            if (pmissile)
                pmissile->AddRef();
        }

        void                Promote(void) override;

        void                SetParentShip(IshipIGC* pship) override;
        IshipIGC*           GetParentShip(void) const override
        {
            return m_pshipParent;
        }
        Mount               GetTurretID(void) const override
        {
            return m_turretID;
        }
        void                SetTurretID(Mount   turretID) override
        {
            if (m_turretID != turretID)
            {
                assert (m_pshipParent);

                if (m_turretID != NA)
                {
                    assert (!m_pshipParent->GetHullType()->GetHardpointData(m_turretID).bFixed);

                    IweaponIGC* pw = (IweaponIGC*)(m_pshipParent->GetMountedPart(ET_Weapon, m_turretID));
                    if (pw)
                    {
                        assert (pw->GetGunner() == this);
                        pw->SetGunner(NULL);
                    }
                }

                m_turretID = turretID;

                if (m_turretID != NA)
                {
                    IweaponIGC* pw = (IweaponIGC*)(m_pshipParent->GetMountedPart(ET_Weapon, m_turretID));
                    if (pw)
                    {
                        assert (!m_pshipParent->GetHullType()->GetHardpointData(m_turretID).bFixed);
                        pw->SetGunner(this);
                    }
                }

                GetMyMission()->GetIgcSite()->LoadoutChangeEvent(this, NULL, c_lcTurretChange);
                GetMyMission()->GetIgcSite()->LoadoutChangeEvent(m_pshipParent, NULL, c_lcPassengerMove);
            }
        }

        IshipIGC*           GetGunner(Mount turretID) const override
        {
            IweaponIGC* pweapon = (IweaponIGC*)*PartLocation(ET_Weapon, turretID);

            if (pweapon)
            {
                return pweapon->GetGunner();
            }
            else
            {
                // we have to manually walk through the child ships
                for (ShipLinkIGC*   psl = m_shipsChildren.first();
                     (psl != NULL);
                     psl = psl->next())
                {
                    IshipIGC* pshipChild = psl->data();

                    if (pshipChild->GetTurretID() == turretID)
                        return pshipChild;
                }

                return NULL;
            }
        }

        const ShipListIGC*  GetChildShips(void) const override
        {
            return &m_shipsChildren;
        }

        void                AddChildShip(IshipIGC* pship) override
        {
            assert (pship);
            AddIbaseIGC((BaseListIGC*)&m_shipsChildren, pship);
        }
        void                DeleteChildShip(IshipIGC* pship) override
        {
            assert (pship);
            DeleteIbaseIGC((BaseListIGC*)&m_shipsChildren, pship);
        }

        IshipIGC*           GetSourceShip(void) override
        {
            return m_pshipParent ? m_pshipParent : this;
        }
        IpartIGC*          CreateAndAddPart(const PartData* ppd) override
        {
            return CreateAndAddPart(GetMyMission()->GetPartType(ppd->partID), ppd->mountID, ppd->amount);
        }

        IpartIGC*           CreateAndAddPart(IpartTypeIGC*  ppt, Mount mount, short amount) override
        {
            assert (ppt);

            assert (GetMountedPart(ppt->GetEquipmentType(), mount) == NULL);
            assert (m_myHullType.CanMount(ppt, mount));

            IpartIGC*   part = GetMyMission()->CreatePart(GetMyLastUpdate(), ppt);
            assert (part);

            part->SetShip(this, mount);
            assert (part->GetShip() == this);
            assert (part->GetMountID() == mount);

            part->SetAmount(amount);

            part->Release();

            return part;        //Bad form to return after a release but it is not dead since the ship holds a pointer
        }

        WingID              GetWingID(void) const override
        {
            return m_wingID;
        }

        void                SetWingID(WingID    wid) override
        {
            m_wingID = wid;
        }

        bool                fRipcordActive(void) const override 
        {
            return m_pmodelRipcord != NULL;
        }

        float               GetRipcordTimeLeft(void) const override
        {
            assert(fRipcordActive());

            return m_dtRipcordCountdown;
        }
        void                ResetRipcordTimeLeft(void) override
        {
            assert (m_myHullType.GetHullType());
            m_dtRipcordCountdown = m_myHullType.GetRipcordSpeed();
        }

        PilotType           GetPilotType(void) const override
        {
            return m_pilotType;
        }

        AbilityBitMask      GetOrdersABM(void) const override
        {
            return m_abmOrders;
        }

        bool                GetAutopilot(void) const override
        {
            return m_bAutopilot;
        }
        
        void                SetAutopilot(bool bAutopilot) override;

        bool                LegalCommand(CommandID          cid) const override
        {
            bool    bLegal = true;
            switch (m_pilotType)
            {
                case c_ptCarrier:
                {
                    bLegal = (cid == c_cidDefault) || (cid == c_cidGoto) || (cid == c_cidJoin) || (cid == c_cidDoNothing);
                }
                break;
                case c_ptMiner:
                {
                    bLegal = (cid == c_cidDefault) || (cid == c_cidGoto) || (cid == c_cidJoin) || (cid == c_cidDoNothing) || (cid == c_cidMine);
                }
                break;
                case c_ptLayer:
                {
                    bLegal = (cid == c_cidDefault) || (cid == c_cidGoto) || (cid == c_cidJoin) || (cid == c_cidDoNothing) || (cid == c_cidBuild);
                }
                break;

                case c_ptBuilder:
                {
                    bLegal = ((m_stateM & (drillingMaskIGC | buildingMaskIGC)) == 0) &&
                             ((cid == c_cidDefault) || (cid == c_cidGoto) || (cid == c_cidJoin) || (cid == c_cidDoNothing) || (cid == c_cidBuild));
                }
                break;

                case c_ptWingman:
                {
                    bLegal = (cid == c_cidDefault) || (cid == c_cidGoto) || (cid == c_cidAttack) || (cid == c_cidPickup) || (cid == c_cidDoNothing);
                }
                break;

                case c_ptPlayer:
                case c_ptCheatPlayer:
                {
                    bLegal = (cid >= c_cidDefault) && (cid < c_cidMine);
                }
            }

            return bLegal;
        }

        bool                LegalCommand(CommandID    cid,
                                                 ImodelIGC*   pmodel) const override
        {
            bool    bLegal = true;
            if ((pmodel == NULL) || 
                ((m_stateM & buildingMaskIGC) != 0) ||
                (pmodel == (ImodelIGC*)this) ||
                (pmodel->GetMission() != GetMyMission()) ||
                (pmodel->GetObjectType() == OT_buoy &&
                 ((IbuoyIGC*)pmodel)->GetBuoyType() == c_buoyCluster &&
                 ((IbuoyIGC*)pmodel)->GetCluster() == GetCluster()))
            {
                bLegal = (cid == c_cidDoNothing);
            }
            else
            {
                ObjectType  type = pmodel->GetObjectType();
                bool        bFriendly = pmodel->GetSide() == GetSide();

                switch (cid)
                {
                    case c_cidNone:
                    {
                        bLegal = false;
                    }
                    break;

                    case c_cidDefault:
                    case c_cidGoto:
                    {
                        bLegal = (m_pilotType >= c_ptPlayer) || (type != OT_station) || bFriendly;
                    }
                    break;

                    case c_cidAttack:
                    {
                        bLegal = ((m_pilotType == c_ptWingman) ||
                                  (m_pilotType >= c_ptPlayer)) && (type != OT_warp) && (type != OT_treasure) && !bFriendly;
                    }
                    break;

                    case c_cidCapture:
                    {
                        bLegal = (m_pilotType >= c_ptPlayer) && (type == OT_station) && !bFriendly;
                    }
                    break;

                    case c_cidDefend:
                    {
                        bLegal = (m_pilotType >= c_ptPlayer);
                    }
                    break;

                    case c_cidPickup:
                    {
                        if (((m_pilotType == c_ptWingman) || (m_pilotType >= c_ptPlayer)) && (m_pshipParent == NULL))
                        {
                            if (type == OT_ship)
                            {
                                IhullTypeIGC*   pht = ((IshipIGC*)pmodel)->GetBaseHullType();
                                bLegal = (pht && m_myHullType.GetHullType() &&
                                          ((pht->HasCapability(c_habmLifepod) && m_myHullType.GetHullType()->HasCapability(c_habmRescue)) ||
                                           (pht->HasCapability(c_habmRescue) && m_myHullType.GetHullType()->HasCapability(c_habmLifepod))));
                            }
                            else if (type != OT_treasure)
                                bLegal = false;
                        }
                        else
                            bLegal = false;
                    }
                    break;

                    case c_cidRepair:
                    {
                        //Can only repair friendly ships or stations
                        bLegal = (m_pilotType >= c_ptPlayer) && ((type == OT_ship) || (type == OT_station)) && bFriendly;
                    }
                    break;

                    case c_cidJoin:
                    {
                        //Can only join other ships
                        bLegal = type == OT_ship;
                    }
                    break;

                    case c_cidBuild:
                    {
                        if (m_pilotType == c_ptLayer)
                            bLegal = (type == OT_buoy) || (type == OT_warp);
                        else if (m_pilotType == c_ptBuilder)
                        {
                            bLegal = (type == OT_asteroid) &&
                                     ((IasteroidIGC*)pmodel)->HasCapability(m_abmOrders);
                        }
                        else
                            bLegal = false;
                    }
                    break;

                    case c_cidMine:
                    {
                        if (m_pilotType == c_ptMiner)
                            bLegal = (type == OT_asteroid) &&
                                     ((IasteroidIGC*)pmodel)->HasCapability(m_abmOrders);
                        else
                            bLegal = false;
                    }
                }
            }

            return bLegal;
        }

        void    Complain(SoundID    sid, const char* pszMsg) override
        {
            if (GetMyLastUpdate() >= m_timeLastComplaint + m_dtTimeBetweenComplaints)
            {
                //There is another side here at the asteroid ... complain
                m_timeLastComplaint = GetMyLastUpdate();
                if (m_dtTimeBetweenComplaints < 600.0f)
                    m_dtTimeBetweenComplaints *= random(1.5f, 2.5f);

                GetMyMission()->GetIgcSite()->SendChat(this, CHAT_TEAM, GetSide()->GetObjectID(),
                                                       sid, pszMsg);
            }
        }

        IshipIGC*           GetAutoDonate(void) const override
        {
            return m_pshipAutoDonate;
        }
        void                SetAutoDonate(IshipIGC* pship) override
        {
            assert (pship != this);

            //Anyone donating to me now donates to my new target
            if (pship)
            {
                IsideIGC*   pside = GetSide();
                assert (pside);
                assert (pship->GetSide() == pside);
                assert (pship->GetAutoDonate() == NULL);

                //Was anyone on my ide donating to me ... if so, they start donating to the new person
                for (ShipLinkIGC*   psl = pside->GetShips()->first();
                     (psl != NULL);
                     psl = psl->next())
                {
                    if (psl->data()->GetAutoDonate() == this)
                        psl->data()->SetAutoDonate(pship == psl->data() ? NULL : pship);
                }
            }

            m_pshipAutoDonate = pship;
        }

        short                      GetKills(void) const override
        {
            return m_nKills;
        }
        void                       SetKills(short n) override
        {
            m_nKills = n;
        }
        void                        AddKill(void) override
        {
            m_nKills++;
            GetSide()->AddKill();
        }

        short                       GetDeaths(void) const override
        {
            return m_nDeaths;
        }
        void                       SetDeaths(short n) override
        {
            m_nDeaths = n;
        }
        void                        AddDeath(void) override
        {
            m_nDeaths++;
            GetSide()->AddDeath();
        }

        short                       GetEjections(void) const override
        {
            return m_nEjections;
        }
        void                       SetEjections(short n) override
        {
            m_nEjections = n;
        }
        void                        AddEjection(void) override
        {
            m_nEjections++;
            GetSide()->AddEjection();
        }

        float               GetExperienceMultiplier(void) const override
        {
            const float c_maxBonus = 0.5f;
            const float c_halfExperience = 4.0f;

            return 1.0f + c_maxBonus * m_experience / (m_experience + c_halfExperience);
        }
        float               GetExperience(void) const override
        {
            return m_experience;
        }
        void                SetExperience(float f) override
        {
            m_experience = f;
        }
        void                AddExperience(void) override
        {
            m_experience += 1.0f;
        }

        ImodelIGC*          GetRipcordModel(void) const override
        {
            return m_pmodelRipcord;
        }
        void                SetRipcordModel(ImodelIGC*  pmodel) override
        {
            //Turn off cloak if ripcording
            if (pmodel && (m_stateM & cloakActiveIGC))
                SetStateM(m_stateM & ~cloakActiveIGC);

            if (m_pmodelRipcord && (m_pmodelRipcord->GetObjectType() == OT_ship))
                ((IshipIGC*)(ImodelIGC*)m_pmodelRipcord)->AdjustRipcordDebt(-m_ripcordCost);

            m_pmodelRipcord = pmodel;

            if (m_pmodelRipcord && (m_pmodelRipcord->GetObjectType() == OT_ship))
                ((IshipIGC*)(ImodelIGC*)m_pmodelRipcord)->AdjustRipcordDebt(m_ripcordCost);
        }
        ImodelIGC*          FindRipcordModel(IclusterIGC*   pcluster) override;

        float               GetRipcordDebt(void) const override
        {
            return m_ripcordDebt;
        }
        void                AdjustRipcordDebt(float delta) override
        {
            m_ripcordDebt += delta;
        }

        bool                OkToLaunch(Time now) override
        {
            //Spend 10 seconds docked (MyLastUpdate is not being updated while docked)
            if (now > GetMyLastUpdate() + 10.0f)
            {
                IclusterIGC*    pcluster   = m_station->GetCluster();
                const Vector&   positionMe = m_station->GetPosition();
                if (LegalCommand(m_commandIDs[c_cmdAccepted], m_commandTargets[c_cmdAccepted]) ||
                    PickDefaultOrder(pcluster, positionMe, true))
                {
                    assert (m_station);
                    IsideIGC*   psideMe = GetSide();

                    int     cEnemy = 0;
                    int     cFriend = 0;
                    float   d2Enemy = FLT_MAX;
                    float   d2Friend = FLT_MAX;
                    for (ShipLinkIGC*   psl = m_station->GetCluster()->GetShips()->first(); (psl != NULL); psl = psl->next())
                    {
                        IshipIGC*   pship = psl->data();
                        if ((pship->GetPilotType() >= c_ptPlayer) && (pship->GetParentShip() == NULL) &&
                            !pship->GetBaseHullType()->HasCapability(c_habmLifepod))
                        {
                            IsideIGC*   pside = pship->GetSide();

                            if (pside == psideMe)
                            {
                                cFriend++;
                                float d2 = (positionMe - pship->GetPosition()).LengthSquared();
                                if (d2 < d2Friend)
                                    d2Friend = d2;
                            }
                            else if (pship->SeenBySide(psideMe))
                            {
                                cEnemy++;
                                float d2 = (positionMe - pship->GetPosition()).LengthSquared();
                                if (d2 < d2Enemy)
                                    d2Enemy = d2;
                            }
                        }
                    }

                    if (cFriend >= cEnemy)
                        return true;
                    else
                    {
                        static const float  c_d2AlwaysRun = 1000.0f;
                        if ((d2Enemy > c_d2AlwaysRun * c_d2AlwaysRun) &&
                            (d2Enemy >= d2Friend))
                        {
                            return true;
                        }
                    }
                }

                SetMyLastUpdate(now);
            }

            return false;
        }

        DamageTrack*    GetDamageTrack(void) override
        {
            return m_damageTrack;
        }
        void            CreateDamageTrack(void) override
        {
            delete m_damageTrack;
            m_damageTrack = GetMyMission()->CreateDamageTrack();
        }
        void            DeleteDamageTrack(void) override
        {
            delete m_damageTrack;
            m_damageTrack = NULL;
        }
        void            ResetDamageTrack(void) override
        {
            if (m_damageTrack)
                m_damageTrack->Reset();
        }

        void                AddMineDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  ImodelIGC*              pmodelLauncher,
                                                  const Vector&           position1,
                                                  const Vector&           position2) override
        {
            if (amount > m_maxMineAmount)
            {
                m_maxMineType = type;
                m_maxMineAmount = amount;
                m_maxMineTime = timeCollision;
                m_maxMineP1 = position1;
                m_maxMineP2 = position2;

                m_maxMineLauncher = pmodelLauncher;

            }
        }
        void                ApplyMineDamage(void) override
        {
            if (m_maxMineAmount > 0.0f)
            {
                if (m_maxMineTime > m_timeLastMineExplosion + 0.5f)
                {
                    m_timeLastMineExplosion = m_maxMineTime;
                    GetCluster()->GetClusterSite()->AddExplosion(m_maxMineP2, 1.5f, c_etMine);
                }

                TRef<ImodelIGC> pmodelLauncher = m_maxMineLauncher;
                m_maxMineLauncher = NULL;

                float amount = m_maxMineAmount;
                m_maxMineAmount = 0.0f;

                ReceiveDamage(m_maxMineType, amount, m_maxMineTime, m_maxMineP1, m_maxMineP2, pmodelLauncher);
            }
        }

        WarningMask         GetWarningMask(void) const override
        {
            return m_warningMask;
        }
        void                SetWarningMaskBit(WarningMask wm) override
        {
            m_warningMask |= wm;
        }
        void                ClearWarningMaskBit(WarningMask wm) override
        {
            m_warningMask &= ~wm;
        }

        //Builders ...
        void                SetBaseData(IbaseIGC*  pbase) override
        {
            m_pbaseData = pbase;
            if (m_pilotType == c_ptBuilder)
            {
                assert (pbase->GetObjectType() == OT_stationType);
                m_abmOrders = ((IstationTypeIGC*)pbase)->GetBuildAABM();
            }
            else
            {
                assert (m_pilotType == c_ptLayer);
                assert ((pbase->GetObjectType() == OT_mineType) ||
                        (pbase->GetObjectType() == OT_probeType));
            }
        }
        IbaseIGC*           GetBaseData(void) const override
        {
            return m_pbaseData;
        }

        CommandID           GetDefaultOrder(ImodelIGC*  pmodel) override
        {
            IsideIGC*   psideHim = pmodel->GetSide();
            ObjectType  type = pmodel->GetObjectType();

            CommandID   cid = c_cidGoto;

            switch (m_pilotType)
            {
                case c_ptMiner:
                case c_ptBuilder:
                {
                    if ((psideHim == NULL) &&
                        (type == OT_asteroid) &&
                        ((IasteroidIGC*)pmodel)->HasCapability(m_abmOrders))
                    {
                        cid = (m_pilotType == c_ptMiner) ? c_cidMine : c_cidBuild;
                    }
                }
                break;

                case c_ptLayer:
                {
                    if ((type == OT_buoy) || (type == OT_warp))
                        cid = c_cidBuild;
                }
                break;

                case c_ptWingman:
                case c_ptPlayer:
                case c_ptCheatPlayer:
                {
                    if (psideHim == GetSide())
                    {
                        cid = c_cidDefend;
                        if (m_pshipParent == NULL)
                        {
                            if (type == OT_ship)
                            {
                                IhullTypeIGC*   pht = ((IshipIGC*)pmodel)->GetBaseHullType();
                                if (pht && m_myHullType.GetHullType())
                                {
                                    if ( (pht->HasCapability(c_habmLifepod) && m_myHullType.GetHullType()->HasCapability(c_habmRescue)) ||
                                         (pht->HasCapability(c_habmRescue) && m_myHullType.GetHullType()->HasCapability(c_habmLifepod)) )
                                        cid = c_cidPickup;
                                }
                            }
                            else if (m_myHullType.GetHullType())
                            {
                                if (type == OT_station)
                                {
                                    StationAbilityBitMask   sabm = ((IstationIGC*)pmodel)->GetStationType()->GetCapabilities();
                                    HullAbilityBitMask      habm = m_myHullType.GetCapabilities();
                                    if ((habm & c_habmFighter) == 0)
                                    {
                                        if (sabm & c_sabmCapLand)
                                            cid = c_cidGoto;
                                    }
                                    else if (habm & c_habmLifepod)
                                    {
                                        if (sabm & (c_sabmRescue | c_sabmLand))
                                            cid = c_cidGoto;
                                    }
                                    else if (sabm & c_sabmLand)
                                        cid = c_cidGoto;
                                }
                                else if (type == OT_probe)
                                {
                                    if (m_myHullType.HasCapability(c_habmLifepod) &&
                                        ((IprobeIGC*)pmodel)->GetProbeType()->HasCapability(c_eabmRescue | c_eabmRescueAny))
                                    {
                                        cid = c_cidGoto;
                                    }
                                }
                            }
                        }
                    }
                    else if (type == OT_treasure)
                        cid = c_cidPickup;
                    else if (type == OT_probe)
                    {
                        cid = c_cidAttack;
                        if (m_myHullType.GetHullType() && m_myHullType.HasCapability(c_habmLifepod) &&
                            ((IprobeIGC*)pmodel)->GetProbeType()->HasCapability(c_eabmRescueAny))
                        {
                            cid = c_cidGoto;
                        }
                    }
                    else if (psideHim != NULL)
                        cid = c_cidAttack;
                }
            }

            return cid;
        }

        short               GetLaunchSlot(void) override
        {
            m_nextLaunchSlot = (m_nextLaunchSlot + 1) % m_myHullType.GetHullType()->GetLaunchSlots();

            return m_nextLaunchSlot;
        }

        //Miners
        float               GetOre(void) const override
        {
            return m_fOre;
        }

        bool                PickDefaultOrder(IclusterIGC*   pcluster,
                                             const Vector&  position,
                                             bool           bDocked) override
        {
            bool fGaveOrder = false;

            //No orders ... pick something
            switch (m_pilotType)
            {
                case c_ptMiner:
                {
                    float   capacity = GetMyMission()->GetFloatConstant(c_fcidCapacityHe3) * 
                                       GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaMiningCapacity);
                    if (m_fOre < capacity / 2.0f)
                    {
                        ImodelIGC*  pmodel = FindTarget(this,
                                                        c_ttNeutral | c_ttAsteroid | c_ttNearest |
                                                        c_ttLeastTargeted | c_ttAnyCluster | c_ttCowardly,
                                                        NULL, pcluster, &position, NULL,
                                                        m_abmOrders);

                        if (pmodel)
                        {
                            SetCommand(c_cmdAccepted, pmodel, c_cidMine);
                            fGaveOrder = true;
                        }
                    }

                    if ((!m_commandTargets[c_cmdCurrent]) && ((m_fOre > 0.0f) || !bDocked))
                    {
                        ImodelIGC*  pmodel = NULL;

                        if (m_fOre > 0.0f)
                            pmodel = FindTarget(this, c_ttFriendly | c_ttStation | c_ttNearest | c_ttAnyCluster,
                                                NULL, pcluster, &position, NULL,
                                                c_sabmUnload);

                        if (pmodel == NULL)
                            pmodel = FindTarget(this, c_ttFriendly | c_ttStation | c_ttNearest | c_ttAnyCluster,
                                                NULL, pcluster, &position, NULL,
                                                c_sabmLand);

                        if (pmodel)
                        {
                            SetCommand(c_cmdAccepted, pmodel, c_cidGoto);
                            fGaveOrder = true;
                        }
                    }
                }
                break;

                case c_ptBuilder:
                {
                    if ((m_abmOrders == c_aabmBuildable) && !bDocked)
                        break;

                    ImodelIGC*  pmodel = FindTarget(this,
                                                    c_ttNeutral | c_ttAsteroid | c_ttNearest |
                                                    c_ttLeastTargeted | c_ttAnyCluster | c_ttCowardly,
                                                    NULL, pcluster, &position, NULL,
                                                    m_abmOrders);

                    if (pmodel)
                    {
                        assert (m_pbaseData);

                        CommandID   cid = (m_abmOrders != c_aabmBuildable) ? c_cidBuild : c_cidGoto;

                        SetCommand(c_cmdAccepted, pmodel, cid);
                        fGaveOrder = true;
                        if (cid == c_cidBuild)
                            GetMyMission()->GetIgcSite()->SendChatf(this, CHAT_TEAM, GetSide()->GetObjectID(),
                                                                    droneInTransitSound,
                                                                    "Building %s at %s",
                                                                    ((IstationTypeIGC*)(IbaseIGC*)m_pbaseData)->GetName(),
                                                                    GetModelName(pmodel));
                    }
                }
                break;

                case c_ptLayer:
                {
                    if (!bDocked)
                        break;

                    ImodelIGC*  pmodel = FindTarget(this,
                                                    c_ttNeutral | c_ttAsteroid | c_ttNearest |
                                                    c_ttLeastTargeted | c_ttAnyCluster | c_ttCowardly,
                                                    NULL, pcluster, &position, NULL,
                                                    0);

                    if (pmodel)
                    {
                        SetCommand(c_cmdAccepted, pmodel, c_cidGoto);
                        fGaveOrder = true;
                    }
                }
                break;

                case c_ptWingman:
                {
                    ImodelIGC*  pmodel = FindTarget(this,
                                                    c_ttEnemy | c_ttShip | c_ttNearest,
                                                    NULL, pcluster, &position, NULL, 0);

                    if (pmodel)
                    {
                        SetCommand(c_cmdAccepted, pmodel, c_cidAttack);
                        fGaveOrder = true;
                        GetMyMission()->GetIgcSite()->SendChatf(this, CHAT_TEAM, GetSide()->GetObjectID(),
                                                                droneInTransitSound,
                                                                "Attacking %s", GetModelName(pmodel));
                    }
                }
                break;
            }

            if (!fGaveOrder)
                SetCommand(c_cmdAccepted, NULL, c_cidNone);
              

            return (m_commandIDs[c_cmdAccepted] != c_cidNone);
        }

        bool    IsGhost(void) const override
        {
            assert (GetMyMission());
            return m_nDeaths > GetMyMission()->GetMissionParams()->iLives;
        }

        bool                InGarage(IshipIGC*  pship, float tCollision) const override;

        float   GetEndurance(void) const override
        {
            float   e;

            if (m_myHullType.GetHullType() &&
                m_myHullType.GetHullType()->HasCapability(c_habmLifepod) &&
                GetCluster())
            {
                e = (m_timeToDie - GetLastUpdate()) / GetMyMission()->GetFloatConstant(c_fcidLifepodEndurance);
                if (e < 0.0f)
                    e = 0.0f;
                else if (e > 1.0f)
                    e = 1.0f;
            }
            else
                e = 1.0f;

            return e;
        }

        bool IsUsingAreaOfEffectWeapon(void) const override
        {
            if (m_pshipParent != NULL)
            {
                if (m_turretID != NA)
                {
                    IweaponIGC* pweapon = (IweaponIGC*)(m_pshipParent->GetMountedPart(ET_Weapon, m_turretID));

                    if (pweapon && pweapon->GetProjectileType()->GetBlastRadius() > 0.0f)
                        return true;
                }
            }
            else
            {
                IhullTypeIGC* pht = m_myHullType.GetHullType();

                if (pht)
                {
                    for (Mount mount = 0; mount < pht->GetMaxFixedWeapons(); ++mount)
                    {
                        IweaponIGC* pweapon = m_mountedWeapons[mount];

                        if (pweapon && pweapon->GetProjectileType()->GetBlastRadius() > 0.0f)
                            return true;
                    }
                }
            }

            return false;
        }

    private:
        bool    bShouldUseRipcord(IclusterIGC*  pcluster);

        void                WarpShip(Time           timeUpdate,
                                     float          deltaT,
                                     Vector*        position,
                                     Vector*        velocity,
                                     Orientation*   orientation);
        void                CalculateShip(Time           timeUpdate,
                                          float          deltaT,
                                          Vector*        position,
                                          Vector*        velocity,
                                          Orientation*   orientation);

        bool    IsSafeToRipcord(void);

        void                ResetWaypoint(void);

        IpartIGC* const*    PartLocation(EquipmentType type, Mount mount) const;

        DWORD_PTR           m_dwPrivate; // private data for consumer

        MyHullType          m_myHullType;

        IstationIGC*        m_station;

        TRef<ImodelIGC>     m_pmodelRipcord;

        IshipIGC*           m_pshipParent;
        ShipListIGC         m_shipsChildren;

        PartListIGC         m_parts;
        IweaponIGC*         m_mountedWeapons[c_maxMountedWeapons];
        IpartIGC*           m_mountedOthers[ET_MAX];
        IpartIGC*           m_mountedCargos[c_maxCargo];

        TRef<IclusterIGC>   m_pclusterRequestRipcord;
        DamageTrack*        m_damageTrack;

        TRef<IshipIGC>      m_pshipAutoDonate;

        TRef<IbaseIGC>      m_pbaseData;

        Vector              m_cockpit;

        GotoPlan            m_gotoplan;

        ControlData         m_controls;
        int                 m_stateM;

        float               m_energy;

        float               m_cloaking;

        float               m_turnRates[3];

        float               m_dtRipcordCountdown;

        float               m_fraction;
        float               m_fractionLastOrder;
        float               m_fuel;
        float               m_ripcordDebt;
        float               m_ripcordCost;
        Vector              m_engineVector;

        Time                m_timeLastComplaint;
        Time                m_timeRanAway;
        Time                m_timeLastMineExplosion;

        Time                m_timeReloadAmmo;
        Time                m_timeReloadFuel;

        Time                m_timeToDie;

        Time                m_timeRequestRipcord;
        float               m_experience;
        float               m_maxMineAmount;
        Time                m_maxMineTime;
        Vector              m_maxMineP1;
        Vector              m_maxMineP2;
        TRef<ImodelIGC>     m_maxMineLauncher;

        DamageTypeID        m_maxMineType;

        short               m_nKills;
        short               m_nDeaths;
        short               m_nEjections;


        short               m_ammo;

        ImissileIGC*        m_pmissileLast;

        ImodelIGC*          m_commandTargets[c_cmdMax];

        float               m_dtTimeBetweenComplaints;
        float               m_fOre;
        AbilityBitMask      m_abmOrders;

        WingID              m_wingID;
        CommandID           m_commandIDs[c_cmdMax];

        ShipID              m_shipID;
        SideID              m_sidFlag;

        Mount               m_turretID;

        short               m_nextLaunchSlot;

        PilotType           m_pilotType;
        bool                m_bAutopilot;
        bool                m_bRunningAway;
        WarningMask         m_warningMask;
};

#endif //__SHIPIGC_H_
