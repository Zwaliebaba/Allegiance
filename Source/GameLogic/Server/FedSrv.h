/*-------------------------------------------------------------------------
 * e:\fed\src\fedsrv\FedSrv.H
 * 
 * Common stuff for server source files
 * 
 * Owner: 
 * 
 * Copyright 1986-1999 Microsoft Corporation, All Rights Reserved
 *-----------------------------------------------------------------------*/

#ifndef _FEDSRV_H_
#define _FEDSRV_H_

#include "EventSite.h"

#undef new // because we override it

const int c_cbArtFilePath = 128;
const int c_cPlayersMax   = 500; // max players allowed on a server
const int c_cDronesMax    = 500; // max drones  allowed on a server
const int c_cShipsMax     = c_cPlayersMax + c_cDronesMax;

  const short c_cMaxPlayersPerGame = 64;


HRESULT pascal FedSrv_Init();
HRESULT pascal FedSrv_Terminate(void);
HRESULT pascal FedSrv_Pause();
HRESULT pascal FedSrv_Continue();
void LoadSettings(); // the global settings (server.json); main() calls it first

const bool c_fCheatsOff = false;
const bool c_fCheatsOn  = true;

const int  c_cShipsPerSectorMax = 110;
const int  c_cMissionsMax       = 10; // arbitrary number of how many missions there can be
const int  c_cMemberListRelloc  = 8; // size of member reallocs, *** keep it a power of two ***

/*********************
      Globals 
 ********************/

// *** App related stuff ***

// !!! The following enum must match the indices of the items in rgCfg in
// !!! LoadSettings() !!!
typedef enum
{
    karg_fTimeout, 
    karg_fWantInt3,       
    karg_nUpdatesPerSecond,
    karg_fDebugBreak,
} FEDSRV_ARGS;

struct ClusterGroups 
{
  CFMGroup * pgrpClusterFlying;
  CFMGroup * pgrpClusterDocked;
};

struct  MapDescription
{
    short   mapID;
    char    szFileName[MAX_PATH + 1];
};


class MapFileSite : public IMissionSite, public IIgcSite
{
};

class FedSrvSiteBase :
  public IMissionSite,
  public IIgcSite,
  public IFedMessagingSite
{
public:
  FedSrvSiteBase() :
    m_pfsMission(nullptr)
  {}
// IIgcSite  
  void ClusterUpdateEvent(IclusterIGC* c) override;
  TRef<ThingSite>   CreateThingSite(ImodelIGC* pModel) override;
  TRef<ClusterSite>   CreateClusterSite(IclusterIGC* pCluster) override;
  void KillAsteroidEvent(IasteroidIGC* pasteroid, bool explodeF) override;
  void DrainAsteroidEvent(IasteroidIGC* pasteroid) override;
  void KillProbeEvent(IprobeIGC* pprobe) override;
  void KillMissileEvent(ImissileIGC* pmissile, const Vector& position) override;
  void KillTreasureEvent(ItreasureIGC* ptreasure) override;
  void KillBuildingEffectEvent(IbuildingEffectIGC* pbe) override;
  void KillMineEvent(ImineIGC* pmine) override;
  void KillShipEvent(Time timeCollision, IshipIGC* pship, ImodelIGC* plauncher, float amount, const Vector& p1, const Vector& p2) override;

  void DamageShipEvent(Time now, IshipIGC* ship, ImodelIGC* launcher, DamageTypeID type, float amount, float leakage, const Vector& p1, const Vector& p2) override;
  void DamageStationEvent(IstationIGC* station, ImodelIGC* launcher, DamageTypeID type, float amount) override;

  void KillStationEvent(IstationIGC* pstation, ImodelIGC* plauncher, float amount) override;
  bool DockWithStationEvent(IshipIGC* pship, IstationIGC* pstation) override;
  void CaptureStationEvent(IshipIGC* pship, IstationIGC* pstation) override;
  void WarpBombEvent(IwarpIGC* pwarp, ImissileIGC* pmissile) override;
  bool RescueShipEvent(IshipIGC* pshipRescued, IshipIGC* pshipRescuer) override;
  bool LandOnCarrierEvent(IshipIGC* pshipCarrier, IshipIGC* pship, float tCollision) override;
  void ChangeStation(IshipIGC* pship, IstationIGC* pstationOld, IstationIGC* pstationNew) override;
  void * GetDroneFromShip(IshipIGC * pship) override;
  bool ContinueRipcord(IshipIGC* pship, ImodelIGC* pmodel) override;
  bool UseRipcord(IshipIGC* pship, ImodelIGC* pmodel) override;
  void RequestRipcord(IshipIGC* pship, IclusterIGC* pcluster) override;
  void HitWarpEvent(IshipIGC* pship, IwarpIGC* pwarp) override;
  bool HitTreasureEvent(Time now, IshipIGC* ship, ItreasureIGC* treasure) override;
  void PaydayEvent(IsideIGC* pside, float money) override;
  void DevelopmentCompleted(IbucketIGC*  b, IdevelopmentIGC*   d, Time now) override;
  void HullTypeCompleted(IsideIGC* pside, IhullTypeIGC* pht) override;
  void PartTypeCompleted(IsideIGC* pside, IpartTypeIGC* ppt) override;
  void StationTypeCompleted(IbucketIGC * pbucket, IstationIGC* pstation, IstationTypeIGC * pstationtype, Time now) override;
  void DroneTypeCompleted(IbucketIGC*  b, IstationIGC* pstation, IdroneTypeIGC*   dt, Time now) override;
  void BuildStation(IasteroidIGC*    pasteroid,
                    IsideIGC*        pside,
                    IstationTypeIGC* pstationtype, Time now) override;
  void SideDevelopmentTechChange(IsideIGC*    pside) override;
  void SideGlobalAttributeChange(IsideIGC*    pside) override;
  void ChangeCluster(IshipIGC* pship,
                     IclusterIGC* pclusterOld,
                     IclusterIGC* pclusterNew) override;
  void LoadoutChangeEvent(IshipIGC* pship, IpartIGC* ppart, LoadoutChange lc) override;
  void CreateSideEvent(IsideIGC * pIsideIGC) override;
  void DestroySideEvent(IsideIGC * pIsideIGC) override;
  void TerminateModelEvent(ImodelIGC* model) override;
  void TerminateMissionEvent(ImissionIGC* pMission) override {}
  void CommandChangedEvent(Command i, IshipIGC * pship, ImodelIGC* ptarget, CommandID cid) override;

  void        CreateBuildingEffect(Time           now,
                                   IasteroidIGC*  pasteroid,
                                   IshipIGC*      pshipBuilder) override;
  void        LayExpendable(Time                   now,
                            IexpendableTypeIGC*    pet,
                            IshipIGC*              pshipLayer) override;
  void SendChat(IshipIGC*   pshipSender,
                ChatTarget  ctRecipient,
                ObjectID    oidRecipient,
                SoundID     soVoiceOver,
                const char* pszText,
                CommandID   cid       = c_cidNone,
                ObjectType  otTarget  = NA,
                ObjectType  oidTarget = NA,
                ImodelIGC*  pmodelTarget = nullptr,
                bool        bObjectModel = false) override;

// IMissionSite
  void Create(CFSMission * pfsMission) override {m_pfsMission = pfsMission; }
  void Destroy(CFSMission * pfsMission) override;

// IFedMessagingSite
  HRESULT OnAppMessage(FedMessaging * pthis, CFMConnection & cnxnFrom, FEDMESSAGE * pfm) override ;
  int     OnMessageBox(FedMessaging * pthis, const char * strText, const char * strCaption, UINT nType) override ;
  HRESULT OnNewConnection(FedMessaging * pthis, CFMConnection & cnxn) override ;
  HRESULT OnDestroyConnection(FedMessaging * pthis, CFMConnection & cnxn) override ;
  HRESULT OnSessionLost(FedMessaging * pthis) override ;
  HRESULT OnSysMessage(FedMessaging * pthis) override;
  void    OnMessageSent(FedMessaging * pthis, CFMRecipient * precip, const void * pv, DWORD cb, FMGuaranteed fmg) override;
#ifndef NO_MSG_CRC
  void    OnBadCRC(FedMessaging * pthis, CFMConnection & cnxn, BYTE * pMsg, DWORD cbMsg) override;
#endif

  virtual int  OnMessageBox(const char * strText, const char * strCaption, UINT nType);

//Not part of IIgcSite
  void UpgradeShip(IshipIGC*  pship, const IstationIGC* pstation);
  void UpgradeDrones(IsideIGC* pside);

  void RespawnFlag(SideID sidFlag);
  
protected:
  CFSMission * m_pfsMission;
};


typedef CEventSite<FedSrvSiteBase> FedSrvSite;

class FedSrvLobbySite : public IFedMessagingSite
{
public:
  FedSrvLobbySite() 
  {}
// IFedMessagingSite
  HRESULT OnAppMessage(FedMessaging * pthis, CFMConnection & cnxnFrom, FEDMESSAGE * pfm) override ;
  int     OnMessageBox(FedMessaging * pthis, const char * strText, const char * strCaption, UINT nType) override ;
  HRESULT OnSessionLost(FedMessaging * pthis) override;
#ifndef NO_MSG_CRC
  void    OnBadCRC(FedMessaging * pthis, CFMConnection & cnxn, BYTE * pMsg, DWORD cbMsg) override;
#endif
};


struct FedTechBit
{
public:
  FedTechBit() {ZeroMemory(this, sizeof(*this));}
  char szName[CbTechBitName];
  int bitnum;
};

//
// A logon request, as FM_C_LOGONREQ checked it, for GotLogonDetails
//
struct LogonDetails
{
  DWORD dwConnectionID;                 // who is logging on
  DWORD dwCookie;                       // what mission they want to join
  bool  fValid;                         // if false, the logon fails with szReason
  bool  fRetry;                         // whether the client should retry the logon
  char  szReason[256];                  // if !fValid (if fValid, the ship's name)
  char  szPassword[c_cbGamePassword];   // password entered by user
  char  szCDKey[c_cbCDKey];             // the CD key they entered
  char  szCharacterName[c_cbName];
  int   characterID;
  bool  fCanCheat;
};

struct Global
{ 
  Global() :
    hInst(nullptr),
    trekCore(nullptr),

    idReceiveThread(0),
    hReceiveThread(nullptr),
    hKillReceiveEvent(nullptr),
    hPlayerEvent(nullptr),
    fSecure(false),
    fTimeOut(false),
    pServerCounters(&serverCounters),
    civTraining(NA),
    hTrainingStop(nullptr),
    fService(false),
    fPublicLobby(false),
    pargPoster(nullptr),
    cbPosters(0),
    cMsgsToOthers(0),
    cBytesToOthers(0),
    timeNow(Time::Now()),
    fm(&siteFedSrv),
    fmLobby(&siteFSLobby),
    fProtocol(false),
    fPaused(false)
  {
    ZeroMemory(rgTechs, sizeof(rgTechs));
    ZeroMemory(&serverCounters, sizeof(serverCounters));
  }

  // Explicit initialization
  void Init()
  {
    // Explicitly initialize the ZString objects
    strLobbyServer.clear();
    strArtPath.clear();
  }


  HINSTANCE           hInst;
  SWMRG               swmrg;
  FedSrvSite          siteFedSrv;
  FedSrvLobbySite     siteFSLobby;

  ImissionIGC *       trekCore;         // gateway to everything that is IGC

  // *** Network layer stuff ***
  DWORD               idReceiveThread;
  HANDLE              hReceiveThread;
  HANDLE              hKillReceiveEvent;
  HANDLE              hPlayerEvent;
  FedMessaging        fm;               // all missions share a session for now
  FedMessaging        fmLobby;
  bool                fSecure;
  bool                fTimeOut;
  bool                fWantInt3;
  ZString             strArtPath;
  int                 nUpdatesPerSecond;

  // *** Game state stuff ***
  Time                timeNow;             // cached value (for very short duration) of timeGetTime()

  // *** Server statistics (were Windows performance counters; now only in memory) ***
  SERVER_COUNTERS     serverCounters;
  SERVER_COUNTERS *   pServerCounters;  // always &serverCounters

  // *** How the server runs ***
  short               civTraining;      // -training <civ>: the client's training mission 7 (NA otherwise)
  HANDLE              hTrainingStop;    // -stopevent <handle>: set by that client to stop the server
  bool                fService;         // -service: started by the service control manager
  bool                fPublicLobby;     // server.json PublicLobby: host the public session and use the lobby

  // Keep list of everything not in IGC, so no database access on each client connect
  FMD_S_POSTER *      pargPoster;
  CB                  cbPosters;

  int                 cMsgsToOthers;  // temp stuff for tracking amount of data sent during logon to players
  int                 cBytesToOthers; // other than the player logging on

  ZString             strLobbyServer;
  ZString             strLocalAddress;
  char                rgTechs[cTechs][CbTechBitName+1];

  RankInfo*           vRankInfo;
  short               cRankInfo;
  StaticMapInfo*      vStaticMapInfo;
  short               cStaticMapInfo;

  bool                fProtocol;
  bool                fPaused;
};


// ******* Functions/Declarations that need prototypes *********
class CFSShip;
class CFSPlayer;
class CFSDrone;



HRESULT pascal FedSrv_Init(); // needed by AllSrvModule.cpp
HRESULT pascal FedSrv_Terminate(void);
void DisconnectFromLobby();
void ChangeStationOwners(IstationIGC * pstation, CFSMission * pfsMission, IsideIGC* iSideNew);
void ExportObj(IbaseIGC * pIGC, ObjectType ot, FMD_S_EXPORT ** ppfmExport);

void SendPlayerInfo(CFSPlayer * pfsPlayer, CFSShip * pfsShip, CFSMission* pfsMission, bool bSend = true);
bool LeaveShip(CFSShip* pfsShip, IshipIGC* pshipParent);
CFMGroup * GetGroupSectorDocked(IclusterIGC * pCluster);
CFMGroup * GetGroupSectorFlying(IclusterIGC * pCluster);

CFSDrone * CreateStockDrone(IdroneTypeIGC * pdronetype, IsideIGC* pSide, IbaseIGC* pbase = nullptr);

void SetCharStats(int characterID, CFSPlayer* pfsPlayer, IsideIGC * pSide, PlayerScoreObject & pso, CFSMission* pfsMission);
void RecordSquadGame(const SideListIGC*  psides, IsideIGC* psideWon);

void GetInvitations(int nInvitationID, MissionID missionID);

void StartDefaultGames();

ItreasureIGC*  CreateTreasure(Time now, IshipIGC* pship, DataTreasureIGC* pdt, const Vector& position, float dv);

IshipIGC*   PickNewLeader(const ShipListIGC*    pships,
                          IshipIGC*             pshipIgnore,
                          int                   maxVotes);
void SetSessionDetails();
HRESULT ConnectToLobby(const char * pszLobby_OverrideCFG /* NULL == do not override */);
  
#define TrapHack(x) assert(x)

#define wm_fs_pause WM_APP // wparam/lparam not used

#endif // _FEDSRV_H_
