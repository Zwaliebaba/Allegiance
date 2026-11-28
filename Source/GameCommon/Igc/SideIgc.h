/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    SideIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CsideIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// SideIgc.h : Declaration of the CsideIGC

#ifndef __SIDEIGC_H_
#define __SIDEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CsideIGC
class       CsideIGC : public IsideIGC
{
    public:
        CsideIGC(void)
        :
            m_activeF(false), // sides are inactive until mission creation is over
            m_dwPrivate(NULL)
        {
        }

        ~CsideIGC(void)
        {
        }

    public:

    // IbaseIGC
        HRESULT                 Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void                    Terminate(void) override;
        void                    Update(Time now) override
        {
            //Update the buckets attached to the station.
            for (BucketLinkIGC* l = m_buckets.first();
                 (l != nullptr);
                 l = l->next())
            {
                l->data()->Update(now);
            }

            m_lastUpdate = now;
        }

        int                     Export(void* data) const override;

        ObjectType              GetObjectType(void) const override
        {
            return OT_side;
        }

        ObjectID                GetObjectID(void) const override
        {
            return m_data.sideID;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

    // IsideIGC
        IcivilizationIGC*       GetCivilization(void) const override
        {
            return m_pCivilization;
        }
        void                    SetCivilization(IcivilizationIGC*   pciv) override
        {
            assert (pciv);
            assert (m_pCivilization);

            if (m_pCivilization != pciv)
            {
                m_pCivilization->Release();
                m_pCivilization = pciv;
                pciv->AddRef();

                // set the tech bits for the new civ
                m_data.ttbmDevelopmentTechs = GetCivilization()->GetBaseTechs();
                m_ttbmBuildingTechs.ClearAll();
                m_data.civilizationID = GetCivilization()->GetObjectID();

                //Turn all space stations into the base space station for the new civ
                {
                    for (StationLinkIGC*    psl = m_stations.first(); (psl != nullptr); psl = psl->next())
                    {
                        IstationIGC*    pstation = psl->data();

                        pstation->SetBaseStationType(pciv->GetInitialStationType());
                    }
                }
            }
        }

        void        DestroyBuckets(void) override;
        void        CreateBuckets(void) override;

        void                SetName(const char* newVal) override
        {
            UTL::putName(m_data.name, newVal);
        }

        const char*             GetName(void) const override
        {
            return m_data.name;
        }

        SquadID     GetSquadID() const override
        {
            return m_data.squadID;
        }

        void        SetSquadID(SquadID squadID) override
        {
            m_data.squadID = squadID;
        }

        const TechTreeBitMask  GetTechs(void) const override
        {
            return m_ttbmBuildingTechs | m_data.ttbmDevelopmentTechs;
        }
        
        const TechTreeBitMask&  GetBuildingTechs(void) const override
        {
            return m_ttbmBuildingTechs;
        }
        void                    ResetBuildingTechs(void) override
        {
            //See what the new set of building techs is, from scratch
            TechTreeBitMask ttbm;
            ttbm.ClearAll();

            {
                for (StationLinkIGC*    l = m_stations.first();
                     (l != nullptr);
                     l = l->next())
                {
                    ttbm |= l->data()->GetStationType()->GetEffectTechs();
                }
            }

            SetBuildingTechs(ttbm);
        }

        void                    SetBuildingTechs(const TechTreeBitMask& ttbm) override
        {
            if (ttbm != m_ttbmBuildingTechs)
            {
                m_ttbmBuildingTechs = ttbm;

                m_pMission->GetIgcSite()->SideBuildingTechChange(this);

                AdjustBuckets();
            }
        }

        const TechTreeBitMask&  GetDevelopmentTechs(void) const override
        {
            return m_data.ttbmDevelopmentTechs;
        }
        void                    SetDevelopmentTechs(const TechTreeBitMask& ttbm) override
        {
            if (ttbm != m_data.ttbmDevelopmentTechs)
            {
                m_data.ttbmDevelopmentTechs = ttbm;
                m_pMission->GetIgcSite()->SideDevelopmentTechChange(this);

                AdjustBuckets();
            }
        }
        bool                    ApplyDevelopmentTechs(const TechTreeBitMask& ttbm) override
        {
            bool    rc;
            if (!(ttbm <= m_data.ttbmDevelopmentTechs))
            {
                rc = true;

                m_data.ttbmDevelopmentTechs |= ttbm;
                m_pMission->GetIgcSite()->SideDevelopmentTechChange(this);

                AdjustBuckets();
            }
            else
                rc = false;

            return rc;
        }
        const TechTreeBitMask&  GetInitialTechs(void) const override
        {
            return m_data.ttbmInitialTechs;
        }
        void                    SetInitialTechs(const TechTreeBitMask& ttbm) override
        {
            m_data.ttbmInitialTechs = ttbm;
        }
        void                    UpdateInitialTechs(void) override
        {
            m_data.ttbmInitialTechs = m_data.ttbmDevelopmentTechs;
        }

        bool                    CanBuy(const IbuyableIGC* b) const override
        {

            ObjectType  type = b->GetObjectType();
            if (type == OT_bucket)
            {
                b = ((IbucketIGC*)b)->GetBuyable();
                type = b->GetObjectType();
            }

            TechTreeBitMask         ttbmSide = (m_ttbmBuildingTechs | m_data.ttbmDevelopmentTechs);
            const TechTreeBitMask&  ttbmRequired = b->GetRequiredTechs();

            bool    bAvailable;
            if (type == OT_development)
            {
                //A non-localized item ... buyable based only on global tech bits
                bAvailable = (ttbmRequired <= ttbmSide) ||
                             (b->GetObjectID() == c_didTeamMoney);        //If you have the team money dev ... you can buy it
            }
            else
            {
                //Station types can not be bought if they are obsolete
                if (type == OT_stationType)
                {
                    //Station types can not be bought if their successor is available
                    const IstationTypeIGC*    pstSuccessor = ((IstationTypeIGC*)b)->GetSuccessorStationType(this);
                    if (pstSuccessor != b)
                        return false;
                }

                //Station type or drone type: these are built at stations so see if any station
                //can build them.
                bAvailable = false;
                for (StationLinkIGC*    psl = m_stations.first(); (psl != nullptr); psl = psl->next())
                {
                    TechTreeBitMask  ttbmStation = psl->data()->GetStationType()->GetLocalTechs() |
                                                   ttbmSide;

                    if (ttbmRequired <= ttbmStation)
                    {
                        bAvailable = true;
                        break;
                    }
                }
            }

            return bAvailable;
        }

        void                    AddStation(IstationIGC* s) override
        {
            AddIbaseIGC((BaseListIGC*)&m_stations, s);

            SetBuildingTechs(m_ttbmBuildingTechs |
                             s->GetStationType()->GetEffectTechs());
        }
        void                    DeleteStation(IstationIGC* s) override
        {
            DeleteIbaseIGC((BaseListIGC*)&m_stations, s);

            //See what the new set of building techs is, from scratch
            ResetBuildingTechs();
        }
        IstationIGC*   GetStation(StationID id) const override
        {
            return (IstationIGC*)GetIbaseIGC((BaseListIGC*)&m_stations, id);
        }
        const StationListIGC*   GetStations(void) const override
        {
            return &m_stations;
        }

        void                    AddShip(IshipIGC* s) override
        {
            AddIbaseIGC((BaseListIGC*)&m_ships, s);
        }
        void                    DeleteShip(IshipIGC* s) override
        {
            DeleteIbaseIGC((BaseListIGC*)&m_ships, s);
        }
        IshipIGC*   GetShip(ShipID id) const override
        {
            return (IshipIGC*)GetIbaseIGC((BaseListIGC*)&m_ships, id);
        }
        const ShipListIGC*   GetShips(void) const override
        {
            return &m_ships;
        }

        void                    AddBucket(IbucketIGC* b) override
        {
            AddIbaseIGC((BaseListIGC*)&m_buckets, b);
        }
        void                    DeleteBucket(IbucketIGC* b) override
        {
            DeleteIbaseIGC((BaseListIGC*)&m_buckets, b);
        }

        IbucketIGC*                     GetBucket(BucketID      bucketID) const override
        {
            return (IbucketIGC*)GetIbaseIGC((BaseListIGC*)&m_buckets, bucketID);
        }
        const BucketListIGC*   GetBuckets(void) const override
        {
            return &m_buckets;
        }

        const GlobalAttributeSet&   GetGlobalAttributeSet(void) const override
        {
            return m_data.gasAttributes;
        }

        void                        SetGlobalAttributeSet(const GlobalAttributeSet& gas) override
        {
            m_data.gasAttributes = gas;
            m_pMission->GetIgcSite()->SideGlobalAttributeChange(this);
        }

        void                        ApplyGlobalAttributeSet(const GlobalAttributeSet& gas) override
        {
            m_data.gasAttributes.Apply(gas);
            m_pMission->GetIgcSite()->SideGlobalAttributeChange(this);
        }

        void                        ResetGlobalAttributeSet(void) override
        {
            m_data.gasAttributes.Initialize();
            m_pMission->GetIgcSite()->SideGlobalAttributeChange(this);
        }

        const TechTreeBitMask&      GetUltimateTechs(void) const override
        {
            return m_ttbmUltimateTechs;
        }

        bool                        GetActiveF(void) const override
        {
            return m_activeF;
        }
        void                        SetActiveF(bool activeF) override
        {
            m_activeF = activeF;
        }

        const Color&                GetColor(void) const override
        {
            return m_data.color;
        }

        void                        SetPrivateData(DWORD_PTR dwPrivate) override
        {
            m_dwPrivate = dwPrivate;
        }

        DWORD_PTR                   GetPrivateData(void) const override
        {
            return m_dwPrivate;
        }

        void                        AddToStockpile(IbuyableIGC* b, short count) override
        {
            assert (count > 0);

            //See if there are already entries in the stockpile
            StockpileLink*  plink = m_stockpile.first();
            {
                while (plink)
                {
                    if (plink->data().buyable == b)
                        break;

                    plink = plink->next();
                }
            }

            if (!plink)
            {
                plink = new StockpileLink;
                plink->data().buyable = b;
                plink->data().count = 0;

                m_stockpile.first(plink);
            }

            assert (plink);
            plink->data().count += count;
        }
        short                        RemoveFromStockpile(IbuyableIGC* b, short count) override
        {
            assert (count > 0);

            StockpileLink*  plink = m_stockpile.first();
            while (plink)
            {
                assert (plink->data().count > 0);

                if (plink->data().buyable == b)
                {
                    if (count >= plink->data().count)
                    {
                        count = plink->data().count;
                        delete plink;
                    }
                    else
                        plink->data().count -= count;

                    return count;
                }

                plink = plink->next();
            }

            return 0;
        }
        short                       GetStockpile(IbuyableIGC* b) const override
        {
            StockpileLink*  plink = m_stockpile.first();
            while (plink)
            {
                if (plink->data().buyable == b)
                    return plink->data().count;

                plink = plink->next();
            }

            return 0;
        }
        const StockpileList*        GetStockpile(void) const override
        {
            return &m_stockpile;
        }

        short                       GetKills(void) const override
        {
            return m_data.nKills;
        }
        void                        AddKill(void) override
        {
            m_data.nKills++;
        }

        short                       GetDeaths(void) const override
        {
            return m_data.nDeaths;
        }
        void                        AddDeath(void) override
        {
            m_data.nDeaths++;
        }

        short                       GetEjections(void) const override
        {
            return m_data.nEjections;
        }
        void                        AddEjection(void) override
        {
            m_data.nEjections++;
        }

        short                       GetBaseKills(void) const override
        {
            return m_data.nBaseKills;
        }
        void                        AddBaseKill(void) override
        {
            m_data.nBaseKills++;
        }

        short                       GetBaseCaptures(void) const override
        {
            return m_data.nBaseCaptures;
        }
        void                        AddBaseCapture(void) override
        {
            m_data.nBaseCaptures++;
        }

        unsigned char GetConquestPercent(void) const override
        {
            return m_data.conquest;
        }
        void          SetConquestPercent(unsigned char newVal) override
        {
            m_data.conquest = newVal;
        }
        unsigned char GetTerritoryCount(void) const override
        {
            return m_data.territory;
        }
        void          SetTerritoryCount(unsigned char newVal) override
        {
            m_data.territory = newVal;
        }

        short GetFlags(void) const override
        {
            return m_data.nFlags;
        }
        void          SetFlags(short newVal) override
        {
            m_data.nFlags = newVal;
        }

        short GetArtifacts(void) const override
        {
            return m_data.nArtifacts;
        }
        void          SetArtifacts(short newVal) override
        {
            m_data.nArtifacts = newVal;
        }

        float GetTimeEndured() const override
        {
            return m_data.fTimeEndured;
        }

        void SetTimeEndured(float fSeconds) override
        {
            m_data.fTimeEndured = fSeconds;
        }

        long GetProsperityPercentBought(void) const override;
        long GetProsperityPercentComplete(void) const override;

        void Reset(void) override
        {
            m_data.nFlags = 0;
            m_data.nArtifacts = 0;
            m_data.nDeaths = 0;
            m_data.nEjections = 0;
            m_data.nKills = 0;
            m_data.nBaseKills = 0;
            m_data.nBaseCaptures = 0;
            m_data.conquest = 0;
        }

    private:
        void    AdjustBuckets(void)
        {
            //Empty the side buckets we can no longer build
            for (BucketLinkIGC* l = m_buckets.first();
                 (l != nullptr);
                 l = l->next())
            {
                IbucketIGC* b = l->data();
                if (!b->GetCompleteF())
                {
                    IbuyableIGC*    pbuy = b->GetBuyable();
                    switch (pbuy->GetObjectType())
                    {
                        case OT_development:
                        {
                            IdevelopmentIGC*    d = (IdevelopmentIGC*)pbuy;
                            assert (d->GetObjectType() == OT_development);

                            if (d->IsObsolete(m_data.ttbmDevelopmentTechs))
                            {
                                //This will not give us anything ... so mark it as complete
                                b->SetTimeAndMoney(1000 * b->GetTimeToBuild(),
                                                   b->GetPrice());
                            }
                            else if ((b->GetMoney() != 0) && !CanBuy(pbuy))
                                b->SetEmpty();
                        }
                        break;

                        case OT_stationType:
                        {
                            Money   money = b->GetMoney();
                            if ((money != 0) && !CanBuy(pbuy))
                            {
                                IstationTypeIGC*    pstSuccessor = ((IstationTypeIGC*)pbuy)->GetSuccessorStationType(this);
                                if (pstSuccessor != pbuy)
                                {
                                    //Find the bucket that corresponds to the successor station
                                    for (BucketLinkIGC* l2 = m_buckets.first();
                                         (l2 != nullptr);
                                         l2 = l2->next())
                                    {
                                        IbucketIGC* b2 = l2->data();
                                        if (b2->GetBuyable() == pstSuccessor)
                                        {
                                            b2->SetTimeAndMoney(b->GetTime(), money);
                                            break;
                                        }
                                    }
                                }

                                b->SetEmpty();
                            }
                        }
                        break;

                        default:
                        {
                            if ((b->GetMoney() != 0) && !CanBuy(pbuy))
                                b->SetEmpty();
                        }
                    }
                }
            }
        }

        ImissionIGC*        m_pMission;
        DWORD_PTR           m_dwPrivate; // private data for consumer
        DataSideIGC         m_data;

        IcivilizationIGC*   m_pCivilization;

        TechTreeBitMask     m_ttbmBuildingTechs;
        TechTreeBitMask     m_ttbmUltimateTechs;

        StationListIGC      m_stations;
        BucketListIGC       m_buckets;
        ShipListIGC         m_ships;

        StockpileList       m_stockpile;

        Time                m_lastUpdate;

        bool                m_activeF;
};

#endif //__SIDEIGC_H_
