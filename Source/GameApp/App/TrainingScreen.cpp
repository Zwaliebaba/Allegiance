#include "pch.h"
#include "Training.h"

// The local server for training mission 7
#include <TrainingServer.h>

//////////////////////////////////////////////////////////////////////////////
//
// Training Screen
//
//////////////////////////////////////////////////////////////////////////////

TRef<ModifiableNumber>  g_pnumberMissionNumber;

class TrainingScreen :
    public Screen, 
    public EventTargetContainer<TrainingScreen>,
    public TrekClientEventSink,
    public IItemEvent::Sink
{
private:
    TRef<Pane>                          m_ppane;
    TRef<ButtonPane>                    m_pbuttonBack;
    TRef<ButtonPane>                    m_pbuttonTrainMission1;
    TRef<ButtonPane>                    m_pbuttonTrainMission2;
    TRef<ButtonPane>                    m_pbuttonTrainMission3;
    TRef<ButtonPane>                    m_pbuttonTrainMission4;
    TRef<ButtonPane>                    m_pbuttonTrainMission5;
    TRef<ButtonPane>                    m_pbuttonTrainMission6;
    TRef<ButtonPane>                    m_pbuttonTrainMission7;
    TRef<ButtonPane>                    m_pbuttonTrain;

    // The local server for training mission 7 (see CreateStandaloneGame)
    static  HANDLE                      m_hServerProcess;
    static  HANDLE                      m_hServerJob;
    static  HANDLE                      m_hServerStop;

    static  int                         m_iMissionNext;

public:
    int                                 m_civID;

    class TrainLiveDialogPopup : 
        public IPopup, 
        public EventTargetContainer<TrainLiveDialogPopup>,
        public TrekClientEventSink
    {
    private:
        TRef<Pane>              m_ppane;
        TRef<ButtonPane>        m_pbuttonOk;
        TRef<ButtonPane>        m_pbuttonCancel;
        TRef<ButtonPane>        m_pbuttonCancel2;
        TRef<ComboPane>         m_pcomboCiv;

        TrainingScreen*         m_pparent;

    public:

        TrainLiveDialogPopup (TRef<INameSpace> pns, TrainingScreen* pparent)
        {
            m_pparent = pparent;

            // find the dialog stuff in the namespace
            CastTo (m_ppane,            pns->FindMember("TrainLiveDialog"));
            CastTo (m_pbuttonOk,        pns->FindMember("okButtonPane"));
            CastTo (m_pbuttonCancel,    pns->FindMember("cancelButtonPane"));
            CastTo (m_pbuttonCancel2,   pns->FindMember("cancelButtonPane2"));
            CastTo (m_pcomboCiv,        pns->FindMember("civComboPane"));

            // handle the ok and cancel buttons
            AddEventTarget(&TrainLiveDialogPopup::OnButtonOk, m_pbuttonOk->GetEventSource());
            AddEventTarget(&TrainLiveDialogPopup::OnButtonCancel, m_pbuttonCancel->GetEventSource());
            AddEventTarget(&TrainLiveDialogPopup::OnButtonCancel, m_pbuttonCancel2->GetEventSource());

            // fill in the combo pane of civs
            m_pcomboCiv->AddItem("Iron Coalition", 25);
            m_pcomboCiv->AddItem("Bios", 35);
            m_pcomboCiv->AddItem("Gigacorp", 18);
            //m_pcomboCiv->AddItem("Belters", 101);
        }

        void OnClose() override
        {
            IPopup::OnClose();
            debugf("train live popup closed\n");
        }

        Pane* GetPane() override
        {
            return m_ppane;
        }

        void CloseThePopup (void)
        {
            if (m_ppopupOwner) {
                m_ppopupOwner->ClosePopup(this);
            } else if (m_pcontainer) {
                m_pcontainer->ClosePopup(this);
            }
            GetWindow()->RestoreCursor();
        }
        
        bool OnButtonOk()
        {
            CloseThePopup ();

            // now 
            m_pparent->m_civID = m_pcomboCiv->GetSelection ();
            m_pparent->DoTrainLive ();
            return true;
        }

        bool OnButtonCancel()
        {
            CloseThePopup ();

            return true;
        }
    };
    
    TRef<TrainLiveDialogPopup>          m_pTrainLivePopup;


public:
    TrainingScreen(Modeler* pmodeler)
    {
        // terminate existing training game if there was one
        KillStandaloneGame ();

        //
        // exports
        //

        TRef<INameSpace> pnsTrainingData = pmodeler->CreateNameSpace("trainingdata", pmodeler->GetNameSpace("gamepanes"));

        pnsTrainingData->AddMember("missionNumber", g_pnumberMissionNumber = new ModifiableNumber(m_iMissionNext));

        // Load the members from MDL

        TRef<INameSpace> pns = pmodeler->GetNameSpace("TrainingScreen");

        CastTo(m_ppane,                     pns->FindMember("screen"                    ));
        CastTo(m_pbuttonBack,               pns->FindMember("backButtonPane"            ));
        CastTo(m_pbuttonTrainMission1,      pns->FindMember("trainMission1ButtonPane"   ));
        CastTo(m_pbuttonTrainMission2,      pns->FindMember("trainMission2ButtonPane"   ));
        CastTo(m_pbuttonTrainMission3,      pns->FindMember("trainMission3ButtonPane"   ));
        CastTo(m_pbuttonTrainMission4,      pns->FindMember("trainMission4ButtonPane"   ));
        CastTo(m_pbuttonTrainMission5,      pns->FindMember("trainMission5ButtonPane"   ));
        CastTo(m_pbuttonTrainMission6,      pns->FindMember("trainMission6ButtonPane"   ));
        CastTo(m_pbuttonTrainMission7,      pns->FindMember("trainMission7ButtonPane"   ));
        CastTo(m_pbuttonTrain,              pns->FindMember("trainButtonPane"           ));

        m_pTrainLivePopup = new TrainLiveDialogPopup(pns, this);

        pmodeler->UnloadNameSpace("TrainingScreen");

        //
        // buttons
        //

        AddEventTarget(&TrainingScreen::OnButtonBack, m_pbuttonBack->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrain->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission1, m_pbuttonTrainMission1->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission2, m_pbuttonTrainMission2->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission3, m_pbuttonTrainMission3->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission4, m_pbuttonTrainMission4->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission5, m_pbuttonTrainMission5->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission6, m_pbuttonTrainMission6->GetEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrainMission7, m_pbuttonTrainMission7->GetEventSource());

        //
        // buttons
        //

        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission1->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission2->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission3->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission4->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission5->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission6->GetDoubleClickEventSource());
        AddEventTarget(&TrainingScreen::OnButtonTrain, m_pbuttonTrainMission7->GetDoubleClickEventSource());

        // hilite the current mission
        switch (m_iMissionNext)
        {
            // here we will determine which button to hilight
            case Training::c_TM_1_Introduction:
                m_pbuttonTrainMission1->SetChecked (true);
                break;
            case Training::c_TM_2_Basic_Flight:
                m_pbuttonTrainMission2->SetChecked (true);
                break;
            case Training::c_TM_3_Basic_Weaponry:
                m_pbuttonTrainMission3->SetChecked (true);
                break;
            case Training::c_TM_4_Enemy_Engagement:
                m_pbuttonTrainMission4->SetChecked (true);
                break;
            case Training::c_TM_5_Command_View:
                m_pbuttonTrainMission5->SetChecked (true);
                break;
            case Training::c_TM_6_Practice_Arena:
                m_pbuttonTrainMission6->SetChecked (true);
                break;
            case Training::c_TM_7_Live:
            default:
                m_pbuttonTrainMission7->SetChecked (true);
                break;
        }
        m_pbuttonTrain->SetChecked (true);
    }

    ~TrainingScreen()
    {
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // UI Events
    //
    //////////////////////////////////////////////////////////////////////////////

    bool OnEvent(IItemEvent::Source *pevent, ItemID pitem) override
    {
        return true;
    }

    bool OnButtonBack()
    {
        GetWindow()->screen(ScreenIDIntroScreen);
        return false;
    }

    bool OnButtonTrain()
    {
        // here we mark the fact that a training mission has been launched
        ZSettings::SetDWORD("HasTrained", 1);

        // check to see which mission this is, and advance to the next mission
        int iMission = static_cast<int> (g_pnumberMissionNumber->GetValue ());
        if (iMission == m_iMissionNext)
        {
            m_iMissionNext++;
            if (m_iMissionNext > Training::c_TM_6_Practice_Arena)
                m_iMissionNext = Training::c_TM_6_Practice_Arena;
        }

        // check to see which mission this is
        switch (iMission)
        {
            case Training::c_TM_1_Introduction:
                GetWindow ()->screen (ScreenIDPostTrainSlideshow);
                break;
            case Training::c_TM_2_Basic_Flight:
            case Training::c_TM_3_Basic_Weaponry:
            case Training::c_TM_4_Enemy_Engagement:
            case Training::c_TM_5_Command_View:
            case Training::c_TM_6_Practice_Arena:
                // note that the training slideshow can get the mission number from the global
                // value when subsequently launching.
                GetWindow ()->screen (ScreenIDTrainSlideshow);
                break;
            case Training::c_TM_7_Live:
                TrainLive ();
                break;
        }
        return true;
    }

    bool OnButtonTrainMission1()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_1_Introduction);
        m_pbuttonTrainMission1->SetChecked (true);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission2()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_2_Basic_Flight);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (true);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission3()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_3_Basic_Weaponry);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (true);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission4()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_4_Enemy_Engagement);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (true);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission5()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_5_Command_View);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (true);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission6()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_6_Practice_Arena);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (true);
        m_pbuttonTrainMission7->SetChecked (false);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    bool OnButtonTrainMission7()
    {
        g_pnumberMissionNumber->SetValue (Training::c_TM_7_Live);
        m_pbuttonTrainMission1->SetChecked (false);
        m_pbuttonTrainMission2->SetChecked (false);
        m_pbuttonTrainMission3->SetChecked (false);
        m_pbuttonTrainMission4->SetChecked (false);
        m_pbuttonTrainMission5->SetChecked (false);
        m_pbuttonTrainMission6->SetChecked (false);
        m_pbuttonTrainMission7->SetChecked (true);
        m_pbuttonTrain->SetEnabled (true);
        m_pbuttonTrain->SetChecked (true);
        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Screen Methods
    //
    //////////////////////////////////////////////////////////////////////////////

    Pane* GetPane() override
    {
        return m_ppane;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Game Creation Methods
    //
    //////////////////////////////////////////////////////////////////////////////
    #define TRAINING_MISSION_7_GAME_NAME    c_szTrainingGameName

    //////////////////////////////////////////////////////////////////////////////
    static
    void    ShowError (const ZString& strError)
    {
        TRef<IMessageBox>   pMsgBox = CreateMessageBox (strError, NULL, true);
        GetWindow()->GetPopupContainer()->OpenPopup (pMsgBox, false);
    }

    //////////////////////////////////////////////////////////////////////////////
    void    TrainLive (void)
    {
        // Make sure that they have entered a CD Key, since the server will boot 
        // them if they have not.  
        if (g_bAskForCDKey && trekClient.GetCDKey ().empty () )
        {
            TRef<IPopup> ppopupCDKey = CreateCDKeyPopup ();
            GetWindow ()->GetPopupContainer ()->OpenPopup (ppopupCDKey, false);
        }
        else
        {
            // open the TrainLiveDialogPopup
            GetWindow ()->GetPopupContainer ()->OpenPopup (m_pTrainLivePopup, false);
            // this then goes to DoTrainLive if the user clicks OK
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    bool    DoTrainLive (void)
    {
        // pop up the "creating game..." dialog.   
        GetWindow()->SetWaitCursor ();
        TRef<IMessageBox> pMsgBox = CreateMessageBox ("Creating game...", NULL, false);
        GetWindow ()->GetPopupContainer ()->OpenPopup (pMsgBox, false);

        // pause to let the "creating game..." box draw itself
        AddEventTarget(&TrainingScreen::OnTrainLive, GetWindow(), 0.1f);

        // this then goes to OnTrainLive

        // return false, I don't know why
        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    bool OnTrainLive (void)
    {
        // close the "creating game..." popup (it won't really close until after we get through this function)
        while (!GetWindow()->GetPopupContainer()->IsEmpty())
            GetWindow()->GetPopupContainer()->ClosePopup(NULL);
        GetWindow()->RestoreCursor();

        // create the game, and logon to it if successful
        CreateStandaloneGame ();

        // return false, I don't know why
        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Training mission 7 runs on a local server: AllSrv32.exe next to us,
    // started as a child process with "-training <civ> -stopevent <handle>"
    // (see TrainingServer.h). The server hosts a LAN session with the one
    // training game, starts the game when we have joined, and shuts down
    // when we set the stop event. It is in a job object, so it also goes
    // away if we crash.
    //
    //////////////////////////////////////////////////////////////////////////////
    HRESULT CreateStandaloneGame (void)
    {
        // only one training server at a time
        KillStandaloneGame ();

        // AllSrv32.exe sits next to Allegiance.exe
        char    szServer[MAX_PATH + 16];
        DWORD   cch = GetModuleFileName (NULL, szServer, MAX_PATH);
        char*   pchName = (cch > 0 && cch < MAX_PATH) ? strrchr (szServer, '\\') : NULL;
        if (!pchName)
        {
            ShowError ("Unable to create a game: cannot find the folder Allegiance runs from.");
            return E_FAIL;
        }
        pchName[1] = '\0';
        char    szFolder[MAX_PATH + 16];
        strcpy (szFolder, szServer);
        strcpy (pchName + 1, "AllSrv32.exe");

        // the stop event, inherited by the server
        SECURITY_ATTRIBUTES sa = { sizeof (sa), NULL, TRUE };
        m_hServerStop = CreateEvent (&sa, TRUE, FALSE, NULL);

        // the job that ties the server's lifetime to ours
        m_hServerJob = CreateJobObject (NULL, NULL);
        if (m_hServerJob)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
            ZeroMemory (&jeli, sizeof (jeli));
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject (m_hServerJob, JobObjectExtendedLimitInformation, &jeli, sizeof (jeli));
        }

        // inherit only the stop event
        SIZE_T  cbAttributes = 0;
        InitializeProcThreadAttributeList (NULL, 1, 0, &cbAttributes);
        LPPROC_THREAD_ATTRIBUTE_LIST    pattributes = (LPPROC_THREAD_ATTRIBUTE_LIST) _alloca (cbAttributes);
        bool    bAttributes = m_hServerStop
            && InitializeProcThreadAttributeList (pattributes, 1, 0, &cbAttributes)
            && UpdateProcThreadAttribute (pattributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &m_hServerStop, sizeof (HANDLE), NULL, NULL);

        char    szCommandLine[MAX_PATH + 96];
        sprintf (szCommandLine, "\"%s\" -training %d -stopevent %llu", szServer, m_civID, (unsigned long long) (UINT_PTR) m_hServerStop);

        STARTUPINFOEX           si;
        PROCESS_INFORMATION     pi;
        ZeroMemory (&si, sizeof (si));
        si.StartupInfo.cb = sizeof (si);
        si.lpAttributeList = pattributes;

        BOOL    bCreated = bAttributes
            && CreateProcess (NULL, szCommandLine, NULL, NULL, TRUE,
                              CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                              NULL, szFolder, &si.StartupInfo, &pi);
        DWORD   dwError = GetLastError ();

        if (bAttributes)
            DeleteProcThreadAttributeList (pattributes);

        if (!bCreated)
        {
            KillStandaloneGame ();

            char    szError[MAX_PATH + 128];
            sprintf (szError, "Unable to create a game: the training server (%s) could not be started (error %lu).", szServer, dwError);
            ShowError (szError);
            return HRESULT_FROM_WIN32 (dwError);
        }

        if (m_hServerJob)
            AssignProcessToJobObject (m_hServerJob, pi.hProcess);
        ResumeThread (pi.hThread);
        CloseHandle (pi.hThread);
        m_hServerProcess = pi.hProcess;

        // pop up the Connecting... dialog.   
        GetWindow()->SetWaitCursor ();
        TRef<IMessageBox> pMsgBox = CreateMessageBox ("Connecting...", NULL, false);
        GetWindow ()->GetPopupContainer ()->OpenPopup (pMsgBox, false);

        // pause to let the "connecting..." box draw itself
        AddEventTarget(&TrainingScreen::OnTryLogon, GetWindow(), 0.1f);

        // this then goes to OnTryLogon
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    static
    bool    HasServerExited (void)
    {
        return m_hServerProcess && (WaitForSingleObject (m_hServerProcess, 0) == WAIT_OBJECT_0);
    }

    //////////////////////////////////////////////////////////////////////////////
    static
    HRESULT KillStandaloneGame (void)
    {
        // ask the server to stop, and give it a moment to do so cleanly
        if (m_hServerStop)
            SetEvent (m_hServerStop);

        if (m_hServerProcess)
        {
            if (WaitForSingleObject (m_hServerProcess, 3000) != WAIT_OBJECT_0)
                TerminateProcess (m_hServerProcess, 1);
            CloseHandle (m_hServerProcess);
            m_hServerProcess = NULL;
        }

        // closing the job ends anything left in it
        if (m_hServerJob)
        {
            CloseHandle (m_hServerJob);
            m_hServerJob = NULL;
        }

        if (m_hServerStop)
        {
            CloseHandle (m_hServerStop);
            m_hServerStop = NULL;
        }

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////
    bool OnTryLogon (void)
    {
        TList<TRef<LANServerInfo> > listResults;

        // try ten times
        int iTryCount = 10;
        while (iTryCount-- && (listResults.GetCount () == 0) && !HasServerExited ())
        {
            // initiate a query for the local server
            trekClient.FindStandaloneServersByName ("127.0.0.1", listResults);

            // now, for 6 seconds, check to see if we found it every half second
            for (int i = 0; (i < 12) && (listResults.GetCount () == 0) && !HasServerExited (); i++)
            {
                Sleep (500);
                trekClient.FindStandaloneServersByName (NULL, listResults);
            }
        }

        // if we found a server...
        if (listResults.GetCount () == 1)
        {
            // there should be only one game on the server
            TList<TRef<LANServerInfo> >::Iterator iterNew(listResults);
            if (iterNew.Value ()->strGameName == TRAINING_MISSION_7_GAME_NAME)
            {
                // set ourselves to not be in the lobby
                trekClient.SetIsLobbied(false);

                // set up the connection info
                BaseClient::ConnectInfo     ci;
                ci.guidSession = iterNew.Value ()->guidSession;
                strcpy (ci.szName, "Cadet");

                // we try five times
                iTryCount = 5;
                while (iTryCount-- && !trekClient.m_fm.IsConnected ())
                {
                    // close any popups
                    while (!GetWindow()->GetPopupContainer()->IsEmpty())
                        GetWindow()->GetPopupContainer()->ClosePopup(NULL);
                    GetWindow()->RestoreCursor();
                    
                    // connect to the server
                    trekClient.ConnectToServer (ci, NA, Time::Now(), "", true);
                }
            }
        }

        // if we didn't get connected
        if (!trekClient.m_fm.IsConnected ())
        {
            // close any popups
            while (!GetWindow()->GetPopupContainer()->IsEmpty())
                GetWindow()->GetPopupContainer()->ClosePopup(NULL);
            GetWindow()->RestoreCursor();

            // give some indication of what happened
            TRef<IMessageBox> pmsgBox = CreateMessageBox(HasServerExited ()
                ? "The training server stopped before it could be joined. Another Allegiance server may be running on this machine."
                : "Connection to standalone server failed.");
            GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);

            // terminate the game so they can try again
            KillStandaloneGame ();

            // report the number of servers found as a diagnostic
            #ifdef _DEBUG
            char    szBuffer[256];
            sprintf (szBuffer, "Found %d servers.\n", listResults.GetCount ());
            debugf (szBuffer);
            #endif
        }
        else
        {
            // tell training that this is the live mission
            Training::StartMission (Training::c_TM_7_Live);
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    void OnLogonGameServer() override
    {
        // wait for a join message.
        GetWindow()->SetWaitCursor();
        TRef<IMessageBox> pmsgBox = 
            CreateMessageBox("Joining mission....", NULL, false, false);
        GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);
    }

    //////////////////////////////////////////////////////////////////////////////
    void OnLogonGameServerFailed(bool bRetry, const char* szReason) override
    {
        // tell the user why the logon failed
        TRef<IMessageBox> pmsgBox = CreateMessageBox(szReason);
        GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);
    }

    //////////////////////////////////////////////////////////////////////////////
    void OnAddPlayer(MissionInfo* pMissionInfo, SideID sideID, PlayerInfo* pPlayerInfo) override
    {
        if ((Training::GetTrainingMissionID () == Training::c_TM_7_Live) && pMissionInfo && (pPlayerInfo == trekClient.MyPlayerInfo()))
        {
            debugf ("Player was added, waiting for the game to start\n");

            // the training server starts the game now that we have joined;
            // next will be OnEnterMission
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    void OnEnterMission (void) override
    {
        debugf ("Game has started!\n");

        // set the cluster
        GetWindow ()->SetCluster (trekClient.GetShip ()->GetCluster ());

        // switch to the combat screen
        GetWindow ()->screen (ScreenIDCombat);

        // set the station so we don't launch in a lifepod
        IstationIGC*    pstation = trekClient.GetShip ()->GetStation();
        if (pstation)
            trekClient.ReplaceLoadout (pstation);
    }

    //////////////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
//
// static initializer
//
//////////////////////////////////////////////////////////////////////////////
int             TrainingScreen::m_iMissionNext = Training::c_TM_1_Introduction;
HANDLE          TrainingScreen::m_hServerProcess = NULL;
HANDLE          TrainingScreen::m_hServerJob = NULL;
HANDLE          TrainingScreen::m_hServerStop = NULL;

//////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////////

TRef<Screen> CreateTrainingScreen(Modeler* pmodeler)
{
    return new TrainingScreen(pmodeler);
}

// destructor
void    KillTrainingStandaloneGame (void)
{
    TrainingScreen::KillStandaloneGame ();
}

