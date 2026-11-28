#ifndef __EventSite_h__
#define __EventSite_h__

/////////////////////////////////////////////////////////////////////////////
//
// CEventSite: raises the server events for a few IGC site callbacks, then
// passes them on. This was AGC's CAGCSite, which also removed the AGC COM
// objects that shadowed IGC objects; AGC and those objects are gone.
//
/////////////////////////////////////////////////////////////////////////////

#include <Igc.h>
#include <stdio.h>

template <class T>
class CEventSite : public T
{
public:
  /*-------------------------------------------------------------------------
   * ChangeCluster()
   *-------------------------------------------------------------------------
   */
  void ChangeCluster(IshipIGC* pship,
                     IclusterIGC* pclusterOld,
                     IclusterIGC* pclusterNew) override
  {
    LPCSTR      pszShip       = pship       ?       pship->GetName    () : "";
    AGCUniqueID shipID        = pship       ?       pship->GetUniqueID() : -1;
    AGCUniqueID newClusterID  = pclusterOld ? pclusterOld->GetUniqueID() : -1;
    AGCUniqueID oldClusterID  = pclusterNew ? pclusterNew->GetUniqueID() : -1;
    LPCSTR      pszClusterOld = pclusterOld ? pclusterOld->GetName()     : "";
    LPCSTR      pszClusterNew = pclusterNew ? pclusterNew->GetName()     : "";

    LPCSTR pszContext = pship ? pship->GetMission()->GetContextName() : NULL;

    g_events.TriggerContextEvent(NULL, EventID_ShipChangesSectors, pszContext,
      pszShip, shipID, newClusterID, oldClusterID, 4,
      "New Sector"     , VT_I4   , newClusterID,
      "Old Sector"     , VT_I4   , oldClusterID,
      "New Sector Name", VT_LPSTR, pszClusterNew,
      "Old Sector Name", VT_LPSTR, pszClusterOld);

    // Allow the server/client site class to invoke event
    T::ChangeCluster(pship, pclusterOld, pclusterNew);
  }

  /*-------------------------------------------------------------------------
   * KillShipEvent()
   *-------------------------------------------------------------------------
   */
  void KillShipEvent(Time now, IshipIGC* ship, ImodelIGC* launcher,
    float amount, const Vector& p1, const Vector& p2) override
  {
    LPCSTR      pszShip     = ship     ?     ship->GetName    () : "";
    AGCUniqueID shipID      = ship     ?     ship->GetUniqueID() : -1;
    AGCUniqueID launcherID  = launcher ? launcher->GetUniqueID() : -1;
    LPCSTR      pszLauncher = launcher ? GetModelName(launcher)  : "";

    HAGCLISTENERS hListeners = g_events.EventListeners(
      EventID_ShipKilled, shipID, -1, -1);
    if (hListeners)
    {
      // The positions as text (they were AGCVector objects)
      char szVector1[64], szVector2[64];
      sprintf_s(szVector1, "%g %g %g", p1.x, p1.y, p1.z);
      sprintf_s(szVector2, "%g %g %g", p2.x, p2.y, p2.z);

      LPCSTR pszContext = ship ? ship->GetMission()->GetContextName() : NULL;

      // Trigger the event
      g_events.TriggerContextEvent(hListeners, EventID_ShipKilled, pszContext,
        pszShip, shipID, -1, -1, 5,
        "Launcher"     , VT_I4   , launcherID,
        "LauncherName" , VT_LPSTR, pszLauncher,
        "Amount"       , VT_R4   , amount,
        "Vector1"      , VT_LPSTR, szVector1,
        "Vector2"      , VT_LPSTR, szVector2);
    }

    // Allow the server/client site class to invoke event
    T::KillShipEvent(now, ship, launcher, amount, p1, p2);
  }

  /*-------------------------------------------------------------------------
   * UseRipcord()
   *-------------------------------------------------------------------------
   */
  bool UseRipcord(IshipIGC* pship, ImodelIGC* pmodel) override
  {
    LPCSTR      pszShip     = pship    ? pship->GetName    () : "";
    AGCUniqueID shipID      = pship    ? pship->GetUniqueID() : -1;
    AGCUniqueID modelID     = pmodel   ? pmodel->GetUniqueID() : -1;
    LPCSTR      pszModel    = pmodel   ? GetModelName(pmodel)  : "";

    HAGCLISTENERS hListeners = g_events.EventListeners(
      EventID_ShipUsesRipcord, shipID, -1, -1);
    if (hListeners)
    {
      LPCSTR pszContext = pship ? pship->GetMission()->GetContextName() : NULL;

      // Trigger the event
      g_events.TriggerContextEvent(hListeners, EventID_ShipUsesRipcord,
        pszContext, pszShip, shipID, -1, -1, 2,
        "Model"     , VT_I4   , modelID,
        "ModelName" , VT_LPSTR, pszModel);
    }

    // Allow the server/client site class to invoke event
    return T::UseRipcord(pship, pmodel);
  }
};

#endif // !__EventSite_h__
