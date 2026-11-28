/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	MissionIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CmissionIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// MissionIgc.h : Declaration of the CmissionIGC

#ifndef __MISSIONIGC_H_
#define __MISSIONIGC_H_


struct  Constants
{
    float   floatConstants[c_fcidMax];
    float   damageConstants[c_dmgidMax][c_defidMax];
};

//Define a class that will handle all of the static data (for all missions)
class   CstaticIGC
{
    public:
        CstaticIGC(void);

        ~CstaticIGC(void)
        {
        }

    // IstaticIGC
        void            Initialize(void)
        {
        }
        void            Terminate(void)
        {
            {
                DroneTypeLinkIGC*  l;
                while ((l = m_droneTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                StationTypeLinkIGC*  l;
                while ((l = m_stationTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                HullTypeLinkIGC*  l;
                while ((l = m_hullTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                ExpendableTypeLinkIGC*  l;
                while ((l = m_expendableTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                PartTypeLinkIGC*  l;
                while ((l = m_partTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                ProjectileTypeLinkIGC*  l;
                while ((l = m_projectileTypes.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                DevelopmentLinkIGC*  l;
                while ((l = m_developments.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                CivilizationLinkIGC*  l;
                while ((l = m_civilizations.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            {
                TreasureSetLinkIGC*  l;
                while ((l = m_treasureSets.first()) != nullptr)
                {
                    l->data()->Terminate();
                }
            }
            assert (m_stationTypes.n() == 0);
            assert (m_hullTypes.n() == 0);
            assert (m_partTypes.n() == 0);
            assert (m_projectileTypes.n() == 0);
            assert (m_developments.n() == 0);
            assert (m_droneTypes.n() == 0);
            assert (m_civilizations.n() == 0);
            assert (m_treasureSets.n() == 0);

            if (m_pptAmmoPack)
            {
                m_pptAmmoPack->Release();
                m_pptAmmoPack = nullptr;
            }

            if (m_pptFuelPack)
            {
                m_pptFuelPack->Release();
                m_pptFuelPack = nullptr;
            }
        }
        virtual float                   GetFloatConstant(FloatConstantID fcid) const
        {
            assert (fcid >= 0);
            assert (fcid < c_fcidMax);

            return m_constants.floatConstants[fcid];
        }
        virtual void                    SetFloatConstant(FloatConstantID fcid, float f)
        {
            assert (fcid >= 0);
            assert (fcid < c_fcidMax);

            m_constants.floatConstants[fcid] = f;
        }

        virtual float                           GetDamageConstant(DamageTypeID dmgid, DefenseTypeID defid) const
        {
            assert ((dmgid & c_dmgidMask) >= 0);
            assert ((dmgid & c_dmgidMask) < c_dmgidMax);
            assert (defid >= 0);
            assert (defid < c_defidMax);

            return m_constants.damageConstants[dmgid & c_dmgidMask][defid];
        }
        virtual void                            SetDamageConstant(DamageTypeID dmgid, DefenseTypeID defid, float f)
        {
            assert ((dmgid & c_dmgidMask) >= 0);
            assert ((dmgid & c_dmgidMask) < c_dmgidMax);
            assert (defid >= 0);
            assert (defid < c_defidMax);

            m_constants.damageConstants[dmgid & c_dmgidMask][defid] = f;
        }

        virtual const void*             GetConstants(void) const
        {
            return &m_constants;
        }
        virtual void                    SetConstants(const void* pf)
        {
            memcpy(&m_constants, pf, sizeof(m_constants));
        }
        virtual int                     GetSizeOfConstants(void) const
        {
            return sizeof(m_constants);
        }
        void                            AddDroneType(IdroneTypeIGC*                 dt)
        {
            AddIbaseIGC((BaseListIGC*)&m_droneTypes, dt);
        }
        void                            DeleteDroneType(IdroneTypeIGC*              dt)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_droneTypes, dt);
        }
        IdroneTypeIGC*                  GetDroneType(DroneTypeID                    id) const
        {
            return (IdroneTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_droneTypes, id);
        }
        const DroneTypeListIGC*         GetDroneTypes(void) const
        {
            return &m_droneTypes;
        }

        void                            AddDevelopment(IdevelopmentIGC*             d)
        {
            AddIbaseIGC((BaseListIGC*)&m_developments, d);
        }
        void                            DeleteDevelopment(IdevelopmentIGC*          d)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_developments, d);
        }
        IdevelopmentIGC*                GetDevelopment(DevelopmentID                id) const
        {
            return (IdevelopmentIGC*)GetIbaseIGC((BaseListIGC*)&m_developments, id);
        }
        const DevelopmentListIGC*       GetDevelopments(void) const
        {
            return &m_developments;
        }

        void                            AddStationType(IstationTypeIGC*             st)
        {
            AddIbaseIGC((BaseListIGC*)&m_stationTypes, st);
        }
        void                            DeleteStationType(IstationTypeIGC*          st)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_stationTypes, st);
        }
        IstationTypeIGC*                GetStationType(StationTypeID                id) const
        {
            return (IstationTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_stationTypes, id);
        }
        const StationTypeListIGC*       GetStationTypes(void) const
        {
            return &m_stationTypes;
        }

        void                            AddCivilization(IcivilizationIGC*           c)
        {
            AddIbaseIGC((BaseListIGC*)&m_civilizations, c);
        }
        void                            DeleteCivilization(IcivilizationIGC*        c)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_civilizations, c);
        }
        IcivilizationIGC*               GetCivilization(CivID                       id) const
        {
            return (IcivilizationIGC*)GetIbaseIGC((BaseListIGC*)&m_civilizations, id);
        }
        const CivilizationListIGC*      GetCivilizations(void) const
        {
            return &m_civilizations;
        }

        void                            AddHullType(IhullTypeIGC*                   ht)
        {
            AddIbaseIGC((BaseListIGC*)&m_hullTypes, ht);
        }
        void                            DeleteHullType(IhullTypeIGC*                ht)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_hullTypes, ht);
        }
        IhullTypeIGC*                   GetHullType(HullID                          id) const
        {
            return (IhullTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_hullTypes, id);
        }
        const HullTypeListIGC*          GetHullTypes(void) const
        {
            return &m_hullTypes;
        }

        void                            AddExpendableType(IexpendableTypeIGC*             mt)
        {
            AddIbaseIGC((BaseListIGC*)&m_expendableTypes, mt);
        }
        void                            DeleteExpendableType(IexpendableTypeIGC*          mt)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_expendableTypes, mt);
        }
        IexpendableTypeIGC*             GetExpendableType(ExpendableTypeID                id) const
        {
            return (IexpendableTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_expendableTypes, id);
        }
        const ExpendableTypeListIGC*    GetExpendableTypes(void) const
        {
            return &m_expendableTypes;
        }

        void                            AddPartType(IpartTypeIGC*                   pt)
        {
            AddIbaseIGC((BaseListIGC*)&m_partTypes, pt);

            if (pt->GetEquipmentType() == ET_Pack)
            {
                DataPackTypeIGC*    pdpt = (DataPackTypeIGC*)(pt->GetData());
                if (pdpt->packType == c_packAmmo)
                {
                    if (m_pptAmmoPack == nullptr)
                    {
                        m_pptAmmoPack = pt;
                        pt->AddRef();
                    }
                }
                else if (pdpt->packType == c_packFuel)
                {
                    if (m_pptFuelPack == nullptr)
                    {
                        m_pptFuelPack = pt;
                        pt->AddRef();
                    }
                }
            }
        }
        void                            DeletePartType(IpartTypeIGC*                pt)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_partTypes, pt);
        }
        IpartTypeIGC*                   GetPartType(PartID                          id) const
        {
            return (IpartTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_partTypes, id);
        }
        const PartTypeListIGC*          GetPartTypes(void) const
        {
            return &m_partTypes;
        }

        virtual void                            AddTreasureSet(ItreasureSetIGC*                   pt)
        {
            AddIbaseIGC((BaseListIGC*)&m_treasureSets, pt);
        }
        virtual void                            DeleteTreasureSet(ItreasureSetIGC*                pt)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_treasureSets, pt);
        }
        virtual ItreasureSetIGC*                GetTreasureSet(TreasureSetID                      id) const
        {
            return (ItreasureSetIGC*)GetIbaseIGC((BaseListIGC*)&m_treasureSets, id);
        }
        virtual const TreasureSetListIGC*       GetTreasureSets(void) const
        {
            return &m_treasureSets;
        }

        void                            AddProjectileType(IprojectileTypeIGC*       pt)
        {
            AddIbaseIGC((BaseListIGC*)&m_projectileTypes, pt);
        }
        void                            DeleteProjectileType(IprojectileTypeIGC*    pt)
        {
            DeleteIbaseIGC((BaseListIGC*)&m_projectileTypes, pt);
        }
        IprojectileTypeIGC*             GetProjectileType(ProjectileTypeID  id) const
        {
            return (IprojectileTypeIGC*)GetIbaseIGC((BaseListIGC*)&m_projectileTypes, id);
        }
        const ProjectileTypeListIGC*    GetProjectileTypes(void) const
        {
            return &m_projectileTypes;
        }

        IpartTypeIGC*           GetAmmoPack(void) const
        {
            return m_pptAmmoPack;
        }

        IpartTypeIGC*           GetFuelPack(void) const
        {
            return m_pptFuelPack;
        }

    private:
        HullTypeListIGC          m_hullTypes;
        PartTypeListIGC          m_partTypes;
        ProjectileTypeListIGC    m_projectileTypes;
        ExpendableTypeListIGC    m_expendableTypes;
        StationTypeListIGC       m_stationTypes;
        CivilizationListIGC      m_civilizations;
        DroneTypeListIGC         m_droneTypes;
        DevelopmentListIGC       m_developments;
        TreasureSetListIGC       m_treasureSets;

        IpartTypeIGC*            m_pptAmmoPack;
        IpartTypeIGC*            m_pptFuelPack;

        Constants                m_constants;
};

/////////////////////////////////////////////////////////////////////////////
// CmissionIGC
class   CmissionIGC : public ImissionIGC
{
    public:
        CmissionIGC(void)
        :
            m_pIgcSite(nullptr),
            m_dwPrivate(0),
            m_nextBuoyID(0),
            m_nextShipID(0),
            m_nextAsteroidID(0),
            m_nextTreasureID(0),
            m_nextMineID(0),
            m_nextProbeID(0),
            m_nextMissileID(0),
            m_nextStationID(0),
            m_stageMission(STAGE_NOTSTARTED),
            m_missionID(0),
            m_pStatic(nullptr),
            m_bHasGenerated(false),
            m_nReplayCount(0)
        {
            // Record the creation time
            SYSTEMTIME st;
            ::GetLocalTime(&st);
            ::SystemTimeToFileTime(&st, &m_ftCreated);
        }

        virtual ~CmissionIGC(void);

    // IstaticIGC
        void                            Initialize(void) override
        {
            //Never called ... should use the initialize of ImissionIGC
            assert (false);
        }
        void                            Terminate(void) override;

        float                           GetFloatConstant(FloatConstantID fcid) const override
        {
            return m_pStatic->GetFloatConstant(fcid);
        }
        void                    SetFloatConstant(FloatConstantID fcid, float f) override
        {
            m_pStatic->SetFloatConstant(fcid, f);
        }

        float                           GetDamageConstant(DamageTypeID dmgid, DefenseTypeID defid) const override
        {
            return  m_pStatic->GetDamageConstant(dmgid, defid);
        }
        void                            SetDamageConstant(DamageTypeID dmgid, DefenseTypeID defid, float f) override
        {
             m_pStatic->SetDamageConstant(dmgid, defid, f);
        }
        const void*                     GetConstants(void) const override
        {
            return m_pStatic->GetConstants();
        }
        void                            SetConstants(const void* pf) override
        {
            m_pStatic->SetConstants(pf);
        }
        int                             GetSizeOfConstants(void) const override
        {
            return m_pStatic->GetSizeOfConstants();
        }
        void                            AddDroneType(IdroneTypeIGC*                 dt) override
        {
            m_pStatic->AddDroneType(dt);
        }
        void                            DeleteDroneType(IdroneTypeIGC*              dt) override
        {
            m_pStatic->DeleteDroneType(dt);
        }
        IdroneTypeIGC*                  GetDroneType(DroneTypeID                    id) const override
        {
            return m_pStatic->GetDroneType(id);
        }
        const DroneTypeListIGC*         GetDroneTypes(void) const override
        {
            return m_pStatic->GetDroneTypes();
        }

        void                            AddDevelopment(IdevelopmentIGC*             d) override
        {
            m_pStatic->AddDevelopment(d);
        }
        void                            DeleteDevelopment(IdevelopmentIGC*          d) override
        {
            m_pStatic->DeleteDevelopment(d);
        }
        IdevelopmentIGC*                GetDevelopment(DevelopmentID                id) const override
        {
            return m_pStatic->GetDevelopment(id);
        }
        const DevelopmentListIGC*       GetDevelopments(void) const override
        {
            return m_pStatic->GetDevelopments();
        }

        void                            AddStationType(IstationTypeIGC*             st) override
        {
            m_pStatic->AddStationType(st);
        }
        void                            DeleteStationType(IstationTypeIGC*          st) override
        {
            m_pStatic->DeleteStationType(st);
        }
        IstationTypeIGC*                GetStationType(StationTypeID                id) const override
        {
            return m_pStatic->GetStationType(id);
        }
        const StationTypeListIGC*       GetStationTypes(void) const override
        {
            return m_pStatic->GetStationTypes();
        }

        void                            AddCivilization(IcivilizationIGC*           c) override
        {
            m_pStatic->AddCivilization(c);
        }
        void                            DeleteCivilization(IcivilizationIGC*        c) override
        {
            m_pStatic->DeleteCivilization(c);
        }
        IcivilizationIGC*               GetCivilization(CivID                       id) const override
        {
            return m_pStatic->GetCivilization(id);
        }
        const CivilizationListIGC*      GetCivilizations(void) const override
        {
            return m_pStatic->GetCivilizations();
        }

        void                            AddHullType(IhullTypeIGC*                   ht) override
        {
            m_pStatic->AddHullType(ht);
        }
        void                            DeleteHullType(IhullTypeIGC*                ht) override
        {
            m_pStatic->DeleteHullType(ht);
        }
        IhullTypeIGC*                   GetHullType(HullID                          id) const override
        {
            return m_pStatic->GetHullType(id);
        }
        const HullTypeListIGC*          GetHullTypes(void) const override
        {
            return m_pStatic->GetHullTypes();
        }

        void                            AddExpendableType(IexpendableTypeIGC*             mt) override
        {
            m_pStatic->AddExpendableType(mt);
        }
        void                            DeleteExpendableType(IexpendableTypeIGC*          mt) override
        {
            m_pStatic->DeleteExpendableType(mt);
        }
        IexpendableTypeIGC*             GetExpendableType(ExpendableTypeID                id) const override
        {
            return m_pStatic->GetExpendableType(id);
        }
        const ExpendableTypeListIGC*    GetExpendableTypes(void) const override
        {
            return m_pStatic->GetExpendableTypes();
        }

        void                            AddPartType(IpartTypeIGC*                   pt) override
        {
            m_pStatic->AddPartType(pt);

            {
                //Preload the art for the corresponding model
                m_pIgcSite->Preload(pt->GetModelName(), nullptr);
            }
        }
        void                            DeletePartType(IpartTypeIGC*                pt) override
        {
            m_pStatic->DeletePartType(pt);
        }
        IpartTypeIGC*                   GetPartType(PartID                          id) const override
        {
            return m_pStatic->GetPartType(id);
        }
        const PartTypeListIGC*          GetPartTypes(void) const override
        {
            return m_pStatic->GetPartTypes();
        }

        void                            AddTreasureSet(ItreasureSetIGC*                   pt) override
        {
            m_pStatic->AddTreasureSet(pt);
        }
        void                            DeleteTreasureSet(ItreasureSetIGC*                pt) override
        {
            m_pStatic->DeleteTreasureSet(pt);
        }
        ItreasureSetIGC*                GetTreasureSet(TreasureSetID                      id) const override
        {
            return m_pStatic->GetTreasureSet(id);
        }
        const TreasureSetListIGC*       GetTreasureSets(void) const override
        {
            return m_pStatic->GetTreasureSets();
        }

        void                            AddProjectileType(IprojectileTypeIGC*       pt) override
        {
            m_pStatic->AddProjectileType(pt);
        }
        void                            DeleteProjectileType(IprojectileTypeIGC*    pt) override
        {
            m_pStatic->DeleteProjectileType(pt);
        }
        IprojectileTypeIGC*             GetProjectileType(ProjectileTypeID              id) const override
        {
            return m_pStatic->GetProjectileType(id);
        }
        const ProjectileTypeListIGC*    GetProjectileTypes(void) const override
        {
            return m_pStatic->GetProjectileTypes();
        }

    // ImissionIGC
        void                            Initialize(Time now, IIgcSite* pIgcSite) override;
        void                            Update(Time now) override;

        MissionID                       GetMissionID(void) const override
        {
            return m_missionID;
        }

        int                     Export(__int64  maskTypes,
                                               char*    pdata) const override;
        void                    Import(Time     now,
                                               __int64  maskTypes,
                                               char*    pdata,
                                               int      datasize) override;

        void                            SetMissionID(MissionID mid) override
        {
            m_missionID = mid;
        }

        IIgcSite*                       GetIgcSite(void) const override
        {
            return m_pIgcSite;
        }

        IbaseIGC*                       CreateObject(Time now, ObjectType objecttype,
                                                             const void* data, int dataSize) override;

        ImodelIGC*                      GetModel(ObjectType type, ObjectID id) const override;

        IbaseIGC*                       GetBase(ObjectType type, ObjectID id) const override;

        DamageTrack*                    CreateDamageTrack(void) override
        {
            return m_damageTracks.Create();
        }
        
        //Global list of stations
        void                            AddStation(IstationIGC* s) override;
        void                            DeleteStation(IstationIGC* s) override;
        IstationIGC*                    GetStation(StationID stationID) const override;
        const StationListIGC*           GetStations(void) const override;

        //Global list of warps
        void                            AddWarp(IwarpIGC* w) override;
        void                            DeleteWarp(IwarpIGC* w) override;
        IwarpIGC*                       GetWarp(WarpID warpID) const override;
        const WarpListIGC*              GetWarps(void) const override;

        //Global list of asteroids
        void                            AddAsteroid(IasteroidIGC* s) override;
        void                            DeleteAsteroid(IasteroidIGC* s) override;
        IasteroidIGC*                   GetAsteroid(AsteroidID id) const override;
        const AsteroidListIGC*          GetAsteroids(void) const override;

        //Global list of mines
        void                            AddMine(ImineIGC* ms) override;
        void                            DeleteMine(ImineIGC* m) override;
        ImineIGC*                       GetMine(MineID mineID) const override;
        const MineListIGC*              GetMines(void) const override;

        //Global list of probes
        void                            AddProbe(IprobeIGC* ms) override;
        void                            DeleteProbe(IprobeIGC* m) override;
        IprobeIGC*                      GetProbe(ProbeID probeID) const override;
        const ProbeListIGC*             GetProbes(void) const override;

        //Global list of treasures
        void                            AddTreasure(ItreasureIGC* t) override;
        void                            DeleteTreasure(ItreasureIGC* t) override;
        ItreasureIGC*                   GetTreasure(TreasureID treasureID) const override;
        const TreasureListIGC*          GetTreasures(void) const override;

        //Global list of sides
        void                            AddSide(IsideIGC* s) override;
        void                            DeleteSide(IsideIGC* s) override;
        IsideIGC*                       GetSide(SideID sideID) const override;
        const SideListIGC*              GetSides(void) const override;

        //Global list of clusters
        void                            AddCluster(IclusterIGC* c) override;
        void                            DeleteCluster(IclusterIGC* c) override;
        IclusterIGC*                    GetCluster(SectorID clusterID) const override;
        const ClusterListIGC*           GetClusters(void) const override;

        //Global list of ships
        void                            AddShip(IshipIGC* s) override;
        void                            DeleteShip(IshipIGC* s) override;
        IshipIGC*                       GetShip(ShipID shipID) const override;
        const ShipListIGC*              GetShips(void) const override;

        //Global list of buoys
        void                            AddBuoy(IbuoyIGC* s) override;
        void                            DeleteBuoy(IbuoyIGC* s) override;
        IbuoyIGC*                       GetBuoy(BuoyID buoyID) const override;
        const BuoyListIGC*              GetBuoys(void) const override;

        IpartTypeIGC*                   GetAmmoPack(void) const override
        {
            return m_pStatic->GetAmmoPack();
        }
        IpartTypeIGC*                   GetFuelPack(void) const override
        {
            return m_pStatic->GetFuelPack();
        }

        IpartIGC*   CreatePart(Time             now,
                               IpartTypeIGC*    ppt) override
        {
            static const ObjectType    objectTypes[ET_MAX] =
                                        {OT_dispenser,        
                                         OT_weapon,
                                         OT_magazine,
                                         OT_dispenser,
                                         OT_shield,
                                         OT_cloak,
                                         OT_pack,
                                         OT_afterburner
                                        };
            assert (objectTypes[ET_ChaffLauncher] == OT_dispenser);
            assert (objectTypes[ET_Weapon] == OT_weapon);
            assert (objectTypes[ET_Magazine] == OT_magazine);
            assert (objectTypes[ET_Dispenser] == OT_dispenser);
            assert (objectTypes[ET_Shield] == OT_shield);
            assert (objectTypes[ET_Cloak] == OT_cloak);
            assert (objectTypes[ET_Pack] == OT_pack);
            assert (objectTypes[ET_Afterburner] == OT_afterburner);

            //The "data" used to create a part is a pointer to the part type.
            return (IpartIGC*)CreateObject(now, objectTypes[ppt->GetEquipmentType()], &ppt, sizeof(ppt));
        }

        void                    SetMissionStage(STAGE  st) override
        {
            m_stageMission = st;
        }

        STAGE                   GetMissionStage() override
        {
            return m_stageMission;
        }

        void    EnterGame(void) override
        {
            if (m_missionParams.IsProsperityGame())
            {
                IdevelopmentIGC*    pdTeamMoney = GetDevelopment(c_didTeamMoney);
                assert (pdTeamMoney);

                assert (m_missionParams.fGoalTeamMoney > 0);

                for (SideLinkIGC*   psl = m_sides.first(); (psl != nullptr); psl = psl->next())
                {
                    //It is something we might be able to build and it will actually
                    //do something useful
                    DataBucketIGC   db = {pdTeamMoney, psl->data()};
                    IbucketIGC*     b = (IbucketIGC*)(CreateObject(m_lastUpdate,
                                                                   OT_bucket,
                                                                   &db,
                                                                   sizeof(db)));
                    assert (b);

                    b->SetPrice((Money)(m_missionParams.fGoalTeamMoney * GetFloatConstant(c_fcidWinTheGameMoney)));

                    b->Release();   //Creating the bucket adds it to the side's list of buckets
                }
            }

            //Go through all sides and create the prosperity development (adjusting the price as needed)
            //Go through all the treasures and reset their expiration time
            for (TreasureLinkIGC*   ptl = m_treasures.first(); (ptl != nullptr); ptl = ptl->next())
                ptl->data()->ResetExpiration(m_lastUpdate);

            //Go through all stations and reset their start time
            for (StationLinkIGC*  psl = m_stations.first(); (psl != nullptr); psl = psl->next())
                psl->data()->SetLastUpdate(m_lastUpdate);
        }

        void                    UpdateSides(Time now,
                                            const MissionParams * pmp,
                                            const char  sideNames[c_cSidesMax][c_cbSideName]) override;
        void                    ResetMission() override;

        void                    GenerateMission(Time                   now,
                                                const MissionParams *  pmp,
                                                TechTreeBitMask *      pttbmShouldOverride,
                                                TechTreeBitMask *      pttbmOverrideValue) override;

        void                    GenerateTreasure(Time           now,
                                                 IclusterIGC*   pcluster,
                                                 short          tsi) override;
        void            SetPrivateData(DWORD_PTR dwPrivate) override
        {
            m_dwPrivate = dwPrivate;
        }

        DWORD_PTR       GetPrivateData(void) const override
        {
            return m_dwPrivate;
        }

        BuoyID                  GenerateNewBuoyID(void) override
        {
            return m_nextBuoyID++;
        }

        ShipID                  GenerateNewShipID(void) override
        {
            return m_nextShipID++;
        }

        AsteroidID              GenerateNewAsteroidID(void) override
        {
            return m_nextAsteroidID++;
        }

        TreasureID              GenerateNewTreasureID(void) override
        {
            return m_nextTreasureID++;
        }

        MineID                  GenerateNewMineID(void) override
        {
            return m_nextMineID++;
        }
        ProbeID                 GenerateNewProbeID(void) override
        {
            return m_nextProbeID++;
        }
        MissileID                  GenerateNewMissileID(void) override
        {
            return m_nextMissileID++;
        }
        StationID                  GenerateNewStationID(void) override
        {
            return m_nextStationID++;
        }

        Time                    GetLastUpdate(void) const override
        {
            return m_lastUpdate;
        }

        const MissionParams*    GetMissionParams(void) const override
        {
            return &m_missionParams;
        }
        void                    SetMissionParams(const MissionParams* pmp) override
        {
            m_missionParams = *pmp;
        }
        void                    SetStartTime(Time timeStart) override
        {
            m_missionParams.timeStart = timeStart;
        }

        CstaticIGC*             GetStaticCore(void) const override
        {
            return m_pStatic;
        }

        void                    SetStaticCore(CstaticIGC*   pStatic) override
        {
            assert (m_pStatic == NULL);

            if (pStatic)
                m_pStatic = pStatic;
            else
            {
                m_pStatic = new CstaticIGC;
                m_pStatic->Initialize();
            }
        }

        short                   GetReplayCount(void) const override;
        const char*             GetContextName(void) override;

    private:
        MissionParams            m_missionParams;
        DWORD_PTR                m_dwPrivate;
        MissionID                m_missionID;
        CstaticIGC*              m_pStatic;        

        IIgcSite*                m_pIgcSite;

        DamageTrackSet           m_damageTracks;
        
        ClusterListIGC           m_clusters;
        BuoyListIGC              m_buoys;
        ShipListIGC              m_ships;
        WarpListIGC              m_warps;
        StationListIGC           m_stations;
        AsteroidListIGC          m_asteroids;
        ProbeListIGC             m_probes;
        MineListIGC              m_mines;
        TreasureListIGC          m_treasures;
        SideListIGC              m_sides;
        IsideIGC*                m_sideTeamLobby;
        Time                     m_lastUpdate;

        BuoyID                   m_nextBuoyID;
        ShipID                   m_nextShipID;
        AsteroidID               m_nextAsteroidID;
        TreasureID               m_nextTreasureID;
        MineID                   m_nextMineID;
        ProbeID                  m_nextProbeID;
        MissileID                m_nextMissileID;
        StationID                m_nextStationID;

        STAGE                    m_stageMission;

        FILETIME                 m_ftCreated;
        bool                     m_bHasGenerated;
        short                    m_nReplayCount;
        ZString                  m_strContextName;
};

#endif //__MISSIONIGC_H_
