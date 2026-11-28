/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	MapMakerIgc.h
**
**  Author: Jeff Fink (jfink@microsoft.com)
**
**  Description:
**
**  History:
*/
// MapMakerIgc.h : Declaration of CmapMakerIGC

#ifndef __MAPMAKERIGC_H_
#define __MAPMAKERIGC_H_


typedef  Slist_utl<ImapMakerIGC*>       MapMakerListIGC;
typedef  Slink_utl<ImapMakerIGC*>       MapMakerLinkIGC;

//
// Constants currently being used.
//
const int c_MaxClustersPerMap = 50;
const int c_NumClusterNames = 68;
const int c_NumberOfPlanetPosters = 25;

const int c_FriendlyAleph = 0;          // Friend to Friend
const int c_EnemyAleph = 1;             // Friend to Enemy
const int c_PlayerToNeutralAleph = 2;   // Player to Neutral
const int c_NeutralToPlayerAleph = 3;   // Neutral to Player
const int c_NeutralAleph = 4;           // Neutral to Neutral
const int c_HiddenToPlayerAleph = 5;

//
// The CMapData class holds all the information needed to build
// a map.
//
class CMapData
{
    protected:
        static const char * smszClusterName[c_NumClusterNames];

    protected:
        ImissionIGC * mpMission;
        const MissionParams * mpMissionParams;
        Time mNow;

        //
        // Members for generating unique cluster names.
        //
        SectorID mClusterNamesLeft[c_NumClusterNames];
        SectorID mcClusterNamesLeft;
        
        //
        // Members for storing the sector data.
        //
        IclusterIGC * mpCluster[c_MaxClustersPerMap];
        SideID msideIDCluster[c_MaxClustersPerMap];

        //
        // Local storage.
        //
        SectorID mcTeamClustersPerTeam;
        SectorID mcNeutralClustersPerTeam;
        SectorID mcOtherClusters;
        WarpID mIDNextWarp;


    public:
        CMapData();
        ~CMapData();

        //
        // Reader functions.
        //
        ImissionIGC *   GetMission()
            { return(mpMission); }
        const MissionParams * GetMissionParams()
            { return(mpMissionParams); }
        Time            GetTime()
            { return(mNow); }

        IclusterIGC * GetCluster(SectorID sID)
            { return(mpCluster[sID]); }
        SideID GetClusterSide(SectorID sID)
            { return(msideIDCluster[sID]); }

        SectorID GetTeamClustersPerTeam()
            { return(mcTeamClustersPerTeam); }
        SectorID GetNeutralClustersPerTeam()
            { return(mcNeutralClustersPerTeam); }
        SectorID GetClustersPerTeam()
            { return(mcTeamClustersPerTeam + mcNeutralClustersPerTeam); }
        SideID GetTeams()
            { return(mpMissionParams->nTeams); }
        SectorID GetTeamClusters()
            { return(GetTeamClustersPerTeam() * GetTeams()); }
        SectorID GetNeutralClusters()
            { return(GetNeutralClustersPerTeam() * GetTeams()); }
        SectorID GetTotalClusters()
            { return(GetClustersPerTeam() * GetTeams() + mcOtherClusters); }
        SectorID GetOtherClusters()
            { return(mcOtherClusters); }

        short GetPlayerClusterMineableAsteroids()
            { return(mpMissionParams->nPlayerSectorMineableAsteroids); }
        short GetPlayerClusterSpecialAsteroids()
            { return(mpMissionParams->nPlayerSectorSpecialAsteroids); }
        short GetPlayerClusterGenericAsteroids()
            { return(mpMissionParams->nPlayerSectorAsteroids); }
        short GetPlayerClusterTreasure()
            { return(mpMissionParams->nPlayerSectorTreasures); }

        short GetNeutralClusterMineableAsteroids()
            { return(mpMissionParams->nNeutralSectorMineableAsteroids); }
        short GetNeutralClusterSpecialAsteroids()
            { return(mpMissionParams->nNeutralSectorSpecialAsteroids); }
        short GetNeutralClusterGenericAsteroids()
            { return(mpMissionParams->nNeutralSectorAsteroids); }
        short GetNeutralClusterTreasure()
            { return(mpMissionParams->nNeutralSectorTreasures); }

        short GetMinableAsteroids(void)
            {
                return mpMissionParams->nPlayerSectorMineableAsteroids * 
                       GetTeamClusters() +
                       mpMissionParams->nNeutralSectorMineableAsteroids *
                       (GetOtherClusters() + GetNeutralClusters());
            }

        short GetPlayerStartTSI()
            { return mpMissionParams->tsiPlayerStart; }
        short GetNeutralStartTSI()
            { return mpMissionParams->tsiNeutralStart; }

        short GetMaxPlayersOnTeam()
            { return(mpMissionParams->nMaxPlayersPerTeam); }
        short GetMaxPlayersInMission()
            { return(GetMaxPlayersOnTeam() * GetTeams()); }
        CivID GetCivID(SideID sideID)
            { return(mpMissionParams->rgCivID[sideID]); }

        WarpID GetNextWarpID()
            { return(mIDNextWarp++); }

        //
        // Writer functions.
        //
        VOID SetMission(ImissionIGC * pMission)
            { mpMission = pMission; }
        VOID SetMissionParams(const MissionParams * pMissionParams)
            { mpMissionParams = pMissionParams; }
        VOID SetTime(Time now)
            { mNow = now; }
        VOID SetTeamClustersPerTeam(SectorID sID)
            { mcTeamClustersPerTeam = sID; }
        VOID SetNeutralClustersPerTeam(SectorID sID)
            { mcNeutralClustersPerTeam = sID; }
        VOID SetOtherClusters(SectorID sID)
            { mcOtherClusters = sID; }

        VOID SetCluster(SectorID sID, IclusterIGC * pCluster, SideID sideID);
        VOID SetClusterSide(SectorID sID, SideID sideID)
            { msideIDCluster[sID] = sideID; }

        //
        // Our own custom methods.
        //
        VOID GetNewClusterName(CHAR * szClusterName);
} ;


//
// Base class for the map maker.
//
//
// Shape is a circle (2 alephs in and out of each sector).
// If Teams > 3, neutral sectors get extra aleph to another neutral sector.
// If Players On Team > 30, central player sectors get extra aleph to
//      another team's central player sector.
//
class CmapMakerIGC : public ImapMakerIGC
{
    protected:
        static const char * smszPlanetName[c_NumberOfPlanetPosters];
        MapMakerID mMMID;
        char mszName[c_cbDescription + 4];

    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerIGC();
        virtual ~CmapMakerIGC();

        MapMakerID  GetMapMakerID() const override           { return(mMMID); }
        VOID        SetMapMakerID(MapMakerID mmID) override  { mMMID = mmID; }
        char const* GetName() const override
            { return(mszName); }
        VOID        SetName(const char * newVal) override
            { strcpy(mszName, newVal); }

        VOID        GenerateMission(Time now,
                                            const MissionParams * pmp,
                                            ImissionIGC * pMission) override;

        //
        // Our own custom helper methods.
        //
        //virtual VOID        GenerateSides(CMapData * pMapData);

        virtual VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID);
        virtual IclusterIGC * GenerateTeamCluster(CMapData * pMapData,
                                            SectorID sID);
        virtual VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID);
        virtual IclusterIGC * GenerateNeutralCluster(CMapData * pMapData,
                                            SectorID sID);
        virtual VOID        GenerateRingClusters(CMapData * pMapData);

        virtual VOID        GenerateWarp(CMapData*      pMapData,
                                         IclusterIGC*   pCluster,
                                         DataWarpIGC*   pdw);
        virtual VOID        LinkClusters(CMapData* pMapData,
                                         SectorID  sID1,
                                         SectorID  sID2,
                                         int       alephType = NA);
        virtual VOID        CrossLinkClusters(CMapData * pMapData,
                                            SectorID sFirstID);
        virtual VOID        LinkClusters(CMapData * pMapData);
        virtual VOID        GenerateLayout(CMapData * pMapData);

        virtual VOID        GenerateStarbase(CMapData * pMapData,
                                             IclusterIGC*   pcluster);
        virtual VOID        PopulateCluster(CMapData*       pMapData,
                                            IclusterIGC*    pcluster,
                                            float           amountHe3);
        virtual VOID        PopulateClusters(CMapData * pMapData);

        virtual VOID        RevealHomeClusters(ImissionIGC * pMission);
        virtual VOID        RevealMap(ImissionIGC * pMission);
        virtual VOID        ActivateSides(ImissionIGC * pMission);
} ;


//
// Derived classes.
// Just use the base class without changes
class   CmapMakerSingleRingIGC  :   public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }
};

//

//
// Double ring.
//
// Shape is two concentric circles.  The center circle contains 1
// neutral sector for each team.  The outer circle contains 1
// sector for each 15 players.  Each player sector is connected
// to two neutral sectors.
//

class CmapMakerDoubleRingIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerDoubleRingIGC();

        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

//
// HiLo
//

class CmapMakerHiLoIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerHiLoIGC();
        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

//
// HiHigher
//

class CmapMakerHiHigherIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerHiHigherIGC();
        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

//
// Star
//

class CmapMakerStarIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerStarIGC();
        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

//
// Big ring
//
// Shape is a single ring with 2 neutral sectors between home sectors
//

class CmapMakerBigRingIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerBigRingIGC();
        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

//
// Brawl
//
// Shape is a single sector containing all of the starbases.
//

class CmapMakerBrawlIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerBrawlIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
        VOID        PopulateCluster(CMapData*       pMapData,
                                            IclusterIGC*    pcluster,
                                            float           amountHe3) override;
} ;

//
// Pin wheel.
//
// Shape is one or more neutral sectors with 1 player sector per
// team outstretched from the center.  If TotalPlayers > 30,
// center contains 1 neutral sector per team.  Otherwise, center 
// is 1 neutral sector.  If Teams > 3, neutral sectors are cross
// connected.
//

class CmapMakerPinWheelIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerPinWheelIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
        VOID        PopulateCluster(CMapData*       pMapData,
                                            IclusterIGC*    pcluster,
                                            float           amountHe3) override;
} ;


//
// Diamond ring.
//
// Shape is one ring of neutral sectors in the center and one ring of
// neutral sectors around the outside.  Each team gets 1 to 3 sectors,
// each one connected to the outside and inside rings with 1 aleph.
//
class CmapMakerDiamondRingIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerDiamondRingIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;


//
// SnowFlake.
//
// 3 neutral sectors per team connected in a triangle.  Each triangle
// is then connected to form a double ring (or with teams > 3, a single
// ring with the center sectors cross connected).  Each team gets 1 to 3
// sectors connected to the outside of the neutral ring.
//
class CmapMakerSnowFlakeIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerSnowFlakeIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                                    CMapData * pMapData,
                                                    DataClusterIGC * pdc,
                                                    SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;


//
// LargeSplit.
//
// Must have an even number of player sectors.  Each team sector is
// connected to 2 neutral sectors.  Each team has its sectors spread out
// so that they are not connected.
//
class CmapMakerLargeSplitIGC : public CmapMakerHiHigherIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerLargeSplitIGC();
};

class CmapMakerInsideOutIGC : public CmapMakerHiHigherIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerInsideOutIGC();
};

class CmapMakerGridIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp)
        {
            return nullptr;
        }

        CmapMakerGridIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
};

class CmapMakerEastWestIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerEastWestIGC();

        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
        VOID        PopulateCluster(CMapData*       pMapData,
                                            IclusterIGC*    pcluster,
                                            float           amountHe3) override;
};

class CmapMakerSplitBaseIGC : public CmapMakerIGC
{
    public:
        static const char*  IsValid(const MissionParams* pmp);

        CmapMakerSplitBaseIGC();
        VOID        GenerateLayout(CMapData * pMapData) override;
        VOID        GenerateTeamClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        GenerateNeutralClusterScreenPosition(
                                            CMapData * pMapData,
                                            DataClusterIGC * pdc,
                                            SectorID sID) override;
        VOID        LinkClusters(CMapData * pMapData) override;
} ;

#endif //__MAPMAKERIGC_H_


