/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    ClusterIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CclusterIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ClusterIgc.h : Declaration of the CclusterIGC

#ifndef __CLUSTERIGC_H_
#define __CLUSTERIGC_H_

const int   c_maxExplosions = 20;
const int   c_nPassesPerUpdate = 10;

/////////////////////////////////////////////////////////////////////////////
// CclusterIGC
class       CclusterIGC : public IclusterIGC
{
    public:
        CclusterIGC(void)
        :
            m_pClusterSite(nullptr),
            m_nExplosions(0),
            m_kdrStatic(true),
            m_kdrMoving(false),
            m_collisionID(0),
            m_fPendingTreasures(0.0f)
        {
        }

    // IbaseIGC
        HRESULT                 Initialize(ImissionIGC* pMission, Time now, const void* data, int length) override;
        void                    Terminate(void) override;
        void                    Update(Time now) override;

        int                     Export(void* data) const override;
        ObjectType              GetObjectType(void) const override
        {
            return OT_cluster;
        }
        ObjectID                GetObjectID(void) const override
        {
            return m_data.clusterID;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

    // IclusterIGC
        const char*             GetName(void) const override
        {
            return m_data.name;
        }

        Time                    GetLastUpdate(void) const override
        {
            return m_lastUpdate;
        }

        void                    SetLastUpdate(Time now) override
        {
            m_lastUpdate = now;
        }

        void                    AddModel(ImodelIGC* modelNew) override;
        void                    DeleteModel(ImodelIGC* modelOld) override;

        ImodelIGC*              GetModel(const char* name) const override;
        const ModelListIGC*     GetModels(void) const override
        {
            return &m_models;
        }
        const ModelListIGC*     GetPickableModels(void) const override
        {
            return &m_modelsPickable;
        }

        void                    AddTreasure(ItreasureIGC* treasureNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_treasures, treasureNew);
            AddModel(treasureNew);
        }
        void                    DeleteTreasure(ItreasureIGC* treasureOld) override
        {
            DeleteModel(treasureOld);
            DeleteIbaseIGC((BaseListIGC*)&m_treasures, treasureOld);
        }
        ItreasureIGC*           GetTreasure(TreasureID  id) const override
        {
            return (ItreasureIGC*)GetIbaseIGC((BaseListIGC*)&m_treasures, id);
        }
        const TreasureListIGC*  GetTreasures(void) const override
        {
            return &m_treasures;
        }

        void                    AddWarp(IwarpIGC* warpNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_warps, warpNew);
            AddModel(warpNew);
        }
        void                    DeleteWarp(IwarpIGC* warpOld) override
        {
            DeleteModel(warpOld);
            DeleteIbaseIGC((BaseListIGC*)&m_warps, warpOld);
        }
        const WarpListIGC*      GetWarps(void) const override
        {
            return &m_warps;
        }

        void                    AddStation(IstationIGC* stationNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_stations, stationNew);
            m_pClusterSite->AddScanner(stationNew->GetSide()->GetObjectID(), stationNew);
            AddModel(stationNew);
        }
        void                    DeleteStation(IstationIGC* stationOld) override
        {
            DeleteModel(stationOld);
            m_pClusterSite->DeleteScanner(stationOld->GetSide()->GetObjectID(), stationOld);
            DeleteIbaseIGC((BaseListIGC*)&m_stations, stationOld);
        }
        IstationIGC*            GetStation(StationID   id) const override
        {
            return (IstationIGC*)GetIbaseIGC((BaseListIGC*)&m_stations, id);
        }
        const StationListIGC*   GetStations(void) const override
        {
            return &m_stations;
        }

        void                    AddShip(IshipIGC* shipNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_ships, shipNew);
            if (shipNew->GetParentShip() == nullptr)
                m_pClusterSite->AddScanner(shipNew->GetSide()->GetObjectID(), shipNew);
            AddModel(shipNew);
        }
        void                    DeleteShip(IshipIGC* shipOld) override
        {
            assert (shipOld);
            DeleteModel(shipOld);
            if (shipOld->GetParentShip() == nullptr)
                m_pClusterSite->DeleteScanner(shipOld->GetSide()->GetObjectID(), shipOld);
            DeleteIbaseIGC((BaseListIGC*)&m_ships, shipOld);
        }
        IshipIGC*               GetShip(ShipID   id) const override
        {
            return (IshipIGC*)GetIbaseIGC((BaseListIGC*)&m_ships, id);
        }
        const ShipListIGC*      GetShips(void) const override
        {
            return &m_ships;
        }

        void                    AddMine(ImineIGC* mineNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_mines, mineNew);
            AddModel(mineNew);
        }
        void                    DeleteMine(ImineIGC* mineOld) override
        {
            assert (mineOld);
            DeleteModel(mineOld);

            DeleteIbaseIGC((BaseListIGC*)&m_mines, mineOld);
        }
        ImineIGC*               GetMine(MineID   id) const override
        {
            return (ImineIGC*)GetIbaseIGC((BaseListIGC*)&m_mines, id);
        }
        const MineListIGC*      GetMines(void) const override
        {
            return &m_mines;
        }

        void                    AddMissile(ImissileIGC* missileNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_missiles, missileNew);
            AddModel(missileNew);
        }
        void                    DeleteMissile(ImissileIGC* missileOld) override
        {
            assert (missileOld);
            DeleteModel(missileOld);

            DeleteIbaseIGC((BaseListIGC*)&m_missiles, missileOld);
        }
        ImissileIGC*               GetMissile(MissileID   id) const override
        {
            return (ImissileIGC*)GetIbaseIGC((BaseListIGC*)&m_missiles, id);
        }
        const MissileListIGC*      GetMissiles(void) const override
        {
            return &m_missiles;
        }

        void                    AddProbe(IprobeIGC* probeNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_probes, probeNew);
            m_pClusterSite->AddScanner(probeNew->GetSide()->GetObjectID(), probeNew);
            AddModel(probeNew);
        }
        void                    DeleteProbe(IprobeIGC* probeOld) override
        {
            assert (probeOld);
            DeleteModel(probeOld);
            m_pClusterSite->DeleteScanner(probeOld->GetSide()->GetObjectID(), probeOld);
            DeleteIbaseIGC((BaseListIGC*)&m_probes, probeOld);
        }
        IprobeIGC*               GetProbe(ProbeID   id) const override
        {
            return (IprobeIGC*)GetIbaseIGC((BaseListIGC*)&m_probes, id);
        }
        const ProbeListIGC*      GetProbes(void) const override
        {
            return &m_probes;
        }

        void                    AddAsteroid(IasteroidIGC* asteroidNew) override
        {
            AddIbaseIGC((BaseListIGC*)&m_asteroids, asteroidNew);

            AddModel(asteroidNew);
        }
        void                    DeleteAsteroid(IasteroidIGC* asteroidOld) override
        {
            DeleteModel(asteroidOld);
            DeleteIbaseIGC((BaseListIGC*)&m_asteroids, asteroidOld);
        }
        IasteroidIGC*           GetAsteroid(AsteroidID   id) const override
        {
            return (IasteroidIGC*)GetIbaseIGC((BaseListIGC*)&m_asteroids, id);
        }
        const AsteroidListIGC*  GetAsteroids(void) const override
        {
            return &m_asteroids;
        }

        ClusterSite*            GetClusterSite(void) const override
        {
            return m_pClusterSite;
        }
        IbuildingEffectIGC*     CreateBuildingEffect(Time           now,
                                                             IasteroidIGC*  pasteroid,
                                                             IstationIGC*   pstation,
                                                             IshipIGC*      pshipBuilder,
                                                             float          radiusAsteroid,
                                                             float          radiusStation,
                                                             const Vector&  positionStart,
                                                             const Vector&  positionStop) override;

        short                   GetNdebris(void) const override
        {
            return m_data.nDebris;
        }

        short                   GetNstars(void) const override
        {
            return m_data.nStars;
        }

        unsigned int            GetStarSeed(void) const override
        {
            return m_data.starSeed;
        }

        const Vector&           GetLightDirection(void) const override
        {
            return m_data.lightDirection;
        }

        Color                GetLightColor(void) const override
        {
            return
                Color(
                    ((float)RGB_GETRED(  m_data.lightColor)) / 255.0f,
                    ((float)RGB_GETGREEN(m_data.lightColor)) / 255.0f,
                    ((float)RGB_GETBLUE( m_data.lightColor)) / 255.0f
                );
        }

        Color                GetLightColorAlt(void) const override
        {
            return Color(0, 0.5f, 1.0f);
        }

        float                GetAmbientLevel(void) const override
        {
            return 0.5f;
        }

        float                GetBidirectionalAmbientLevel(void) const override
        {
            return 0.25f;
        }

        void                    RecalculateCollisions(float        tOffset,
                                                              ImodelIGC*   pModel1,
                                                              ImodelIGC*   pModel2) override;

        ExplosionData*          CreateExplosion(DamageTypeID   damageType,
                                                        float          amount,
                                                        float          radius,
                                                        int            explosionType,
                                                        Time           time,
                                                        const Vector&  position,
                                                        ImodelIGC*     launcher) override
        {
            if (m_nExplosions == c_maxExplosions)
                return nullptr;
            else
            {
                ExplosionData&  e = m_explosions[m_nExplosions++];

                e.damageType    = damageType;
                e.amount        = amount;
                e.radius        = radius;
                e.explosionType = explosionType;
                e.time          = time;
                e.position      = position;
                e.launcher      = launcher;

                if (launcher)
                    launcher->AddRef();

                return &e;
            }
        }

        float                   GetScreenX(void) const override
        {
            return m_data.screenX;
        }

        float                   GetScreenY(void) const override
        {
            return m_data.screenY;
        }

        void                    SetActive(bool bActive) override
        {
            m_data.activeF = bActive;
        }

        const char*             GetPosterName(void) const override
        {
            return m_data.posterName;
        }

        void                    SetPrivateData(DWORD_PTR dwPrivate) override
        {
            m_dwPrivate = dwPrivate;
        }

        DWORD_PTR               GetPrivateData(void) const override
        {
            return m_dwPrivate;
        }

        void                    FlushCollisionQueue(void) override
        {
            if (m_collisionID < m_collisions.n())
            {
                //Called from inside the collision detection loop
                //so this object might just be switching clusters
                //(which would add it to another cluster). This probably
                //isn't the case ... but let's be safe
                m_collisions.flush(m_collisionID + 1, nullptr, nullptr);
            }
        }

        void                    MakeModelStatic(ImodelIGC* pmodel) override
        {
            assert (pmodel);

            HitTest*    ht = pmodel->GetHitTest();
            assert (ht);

            assert (pmodel->GetCluster() == this);
            assert (pmodel->GetAttributes() & c_mtHitable);
            assert (pmodel->GetAttributes() & c_mtStatic);
            assert ((pmodel->GetAttributes() & c_mtCastRay) == 0);

            DeleteIbaseIGC((BaseListIGC*)&m_modelsCastRay, pmodel);

            m_kdrStatic.addHitTest(ht);
        }

        void                    MakeModelUnhitable(ImodelIGC* pmodel) override
        {
            assert (pmodel);

            HitTest*    ht = pmodel->GetHitTest();
            assert (ht);
            m_kdrMoving.deleteHitTest(ht);

            assert (pmodel->GetCluster() == this);
            assert ((pmodel->GetAttributes() & c_mtHitable) == 0);
            assert ((pmodel->GetAttributes() & c_mtCastRay) == 0);

            DeleteIbaseIGC((BaseListIGC*)&m_modelsCastRay, pmodel);
        }

        void             FreeThingSite(ModelAttributes  ma, ImodelIGC*   pmodel, HitTest* pht) override
        {
            if (ma & c_mtHitable)
            {
                assert ((ma & c_mtStatic) == 0);
                m_kdrMoving.deleteHitTest(pht);
            }

            if ((ma & c_mtNotPickable) == 0)
            {
                DeleteIbaseIGC((BaseListIGC*)&m_modelsPickable, pmodel);
            }

            if (ma & c_mtCastRay)
            {
                DeleteIbaseIGC((BaseListIGC*)&m_modelsCastRay, pmodel);
            }
        }

        bool             GetHomeSector(void) const override
        {
            return m_data.bHomeSector;
        }

        float             GetPendingTreasures(void) const override
        {
            return m_fPendingTreasures;
        }
        void             SetPendingTreasures(float  fpt) override
        {
            m_fPendingTreasures = fpt;
        }

        float            GetCost(void) const override
        {
            return m_fCost;
        }

    private:
        ImissionIGC*        m_pMission;
        DWORD_PTR           m_dwPrivate; // private data for consumer
        DataClusterIGC      m_data;
        ModelListIGC        m_models;
        ModelListIGC        m_modelsPickable;
        ModelListIGC        m_modelsCastRay;
        ProbeListIGC        m_probes;
        WarpListIGC         m_warps;
        StationListIGC      m_stations;
        TreasureListIGC     m_treasures;
        AsteroidListIGC     m_asteroids;
        MissileListIGC      m_missiles;
        MineListIGC         m_mines;
        ShipListIGC         m_ships;
        Time                m_lastUpdate;
        KDroot              m_kdrStatic;
        KDroot              m_kdrMoving;
        CollisionQueue      m_collisions;
        TRef<ClusterSite>   m_pClusterSite;
        ExplosionData       m_explosions[c_maxExplosions];

        int                 m_collisionID;
        float               m_tOffset;
        float               m_tMax;
        float               m_fPendingTreasures;
        float               m_fCost;

        int                 m_nExplosions;

        int                 m_nPass;
};

#endif //__CLUSTERIGC_H_
