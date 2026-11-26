#include "pch.h"


//////////////////////////////////////////////////////////////////////////////
//
// Lobby Screen
//
// Entry point for Internet play: downloads the config file, runs the
// auto-updater and the message of the day, then logs on to the free lobby.
//
//////////////////////////////////////////////////////////////////////////////

extern bool    g_bDownloadMessageOfDay;
extern bool    g_bDownloadNewConfig;
extern bool    g_bSkipAutoUpdate;

const char * szValidCfg  = "THIS IS A VALID CONFIG FILE";
const char * szValidMotd = "THIS IS A VALID MESSAGE OF THE DAY FILE";

class LobbyScreen :
    public Screen,
    public LogonSite,
    public TrekClientEventSink,
    public IIntegerEventSink,
    public IAutoUpdateSink,
    public EventTargetContainer<LobbyScreen>
{
private:
    TRef<Modeler>      m_pmodeler;
    TRef<Pane>         m_ppane;
    TRef<ButtonPane>   m_pbuttonGames;
    TRef<ButtonPane>   m_pbuttonGamesBig;
    TRef<ButtonPane>   m_pbuttonMainMenu;
    TRef<MDLFileImage> m_pMDLFileImage;
    char               m_szName[c_cbName];
    char               m_szConfig[MAX_PATH];

    bool               m_bErrorOccured;
    bool               m_bMessageStage; // true: message of the day download, false: config download
    IHTTPSession *     m_pSession;

    TRef<IMessageBox>  m_pmsgBox;

    ScreenID           m_screenPostConnect;

public:
    LobbyScreen(Modeler* pmodeler, Number* ptime) :
        m_pSession(NULL),
        m_bErrorOccured(false),
        m_bMessageStage(false),
        m_pmsgBox(NULL),
        m_pmodeler(pmodeler),
        m_screenPostConnect(ScreenIDGameScreen)
    {
        TRef<INameSpace> pnsLobbyData = GetModeler()->CreateNameSpace("lobbydata");

        pnsLobbyData->AddMember("timeStart", ptime->MakeConstant());

        TRef<INameSpace> pns = pmodeler->GetNameSpace("zonepublicscreen");
        CastTo(m_ppane, pns->FindMember("screen"));

        CastTo(m_pMDLFileImage, (Value*)pns->FindMember("messageImage"));

        CastTo(m_pbuttonGames, pns->FindMember("gamesButtonPane"));
        CastTo(m_pbuttonGamesBig, pns->FindMember("gamesBigButtonPane"));
        CastTo(m_pbuttonMainMenu, pns->FindMember("mainmenuButtonPane"));
        AddEventTarget(&LobbyScreen::OnButtonGames, m_pbuttonGames->GetEventSource());
        AddEventTarget(&LobbyScreen::OnButtonGames, m_pbuttonGamesBig->GetEventSource());
        AddEventTarget(&LobbyScreen::OnButtonMainMenu, m_pbuttonMainMenu->GetEventSource());

        // keep the user here until the downloads have finished
        m_pbuttonMainMenu->SetEnabled(false);
        m_pbuttonGames->SetEnabled(false);
        m_pbuttonGamesBig->SetEnabled(false);

        pmodeler->UnloadNameSpace(pns);

        BeginConfigDownload();

        if (g_bQuickstart)
            AddEventTarget(&LobbyScreen::OnButtonGames, GetWindow(), 0.01f);

        trekClient.FlushSessionLostMessage();
    }

    ~LobbyScreen()
    {
        if (m_pSession)
        {
            delete m_pSession;
            m_pSession = NULL;
        }
    }

    void ConnectToLobby(ScreenID screenid)
    {
        ConnectToLobby(screenid, "Enter a call sign (player name) to use for this game.");
    }

    void ConnectToLobby(ScreenID screenid, const ZString& strPrompt)
    {
        if (trekClient.GetCfgInfo().strPublicLobby.empty())
        {
            TRef<IMessageBox> pmsgBox = CreateMessageBox(
                "The lobby is not available at the moment.  Please try again later.");
            GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);
            return;
        }

        m_screenPostConnect = screenid;

        assert (!trekClient.LoggedOn() && !trekClient.m_fm.IsConnected());

        lstrcpyn(m_szName, trekClient.GetSavedCharacterName(), sizeof(m_szName));

        TRef<IPopup> plogonPopup = CreateLogonPopup(m_pmodeler, this, strPrompt, m_szName);
        Point point(c_PopupX, c_PopupY);
        Rect rect(point, point);
        GetWindow()->GetPopupContainer()->OpenPopup(plogonPopup, rect, false);
    }

    bool OnButtonMainMenu()
    {
        GetWindow()->screen(ScreenIDIntroScreen);

        return true;
    }

    bool OnButtonGames()
    {
        ConnectToLobby(ScreenIDGameScreen);

        return true;
    }

    void BeginConfigDownload()
    {
        if (!g_bDownloadNewConfig)
        {
            debugf("Skipping download of config file due to command-line switch.\n");
            OnConfigDownloadDone(true, false);
            return;
        }

        lstrcpy(m_szConfig, "http://Allegiance.zone.com/Allegiance.cfg");

        // CfgFile: a config file URL, or a local file, instead of the default
        std::string strCfgFile = ZSettings::GetString("CfgFile", "");
        if (!strCfgFile.empty() && strCfgFile.size() < sizeof(m_szConfig))
            lstrcpy(m_szConfig, strCfgFile.c_str());

        if (StrFind(ZString(m_szConfig), "http://") == -1)
        {
            debugf("Using local config file because the CfgFile setting has no \"http://\": %s\n", m_szConfig);

            if (!IsFileValid(m_szConfig))
            {
                #ifndef DEBUG // don't give warning twice for debug users
                m_pmsgBox = CreateMessageBox("Warning, Local CFG missing validation string");
                GetWindow()->GetPopupContainer()->OpenPopup(m_pmsgBox, false);
                #endif
            }

            OnConfigDownloadDone(false, false);
            return;
        }

        if (m_pSession)
            return;

        debugf("Beginning Config Download: %s.\n", m_szConfig);

        m_pSession = CreateHTTPSession(this);

        const char * szFileList[] = { m_szConfig, "temp.cfg", NULL };

        m_pSession->InitiateDownload(szFileList, "."); // . means EXE path
    }

    void BeginMessageOfDayDownload()
    {
        if (!g_bDownloadMessageOfDay)
        {
            debugf("Skipping download of Message Of the Day due to command-line switch.\n");
            OnMessageOfDayDone(false);
            return;
        }

        PCC szMessageOfTheDayFileName = "publicmessageoftheday.mdl";

        PathString path(trekClient.GetArtPath());
        path = path + szMessageOfTheDayFileName;

        int crc = FileCRC(PCC(path), NULL);

        if (crc != 0 && crc == trekClient.GetCfgInfo().crcPublicMessageFile)
        {
            debugf("Existing messageoftheday file has the correct CRC, skipping download.\n");
            OnMessageOfDayDone(true);
            return;
        }
        else
        {
            debugf("Existing messageoftheday file has the wrong CRC (%X).\n", crc);
        }

        if (m_pSession)
            return;

        PCC szMessageOfTheDayURL = PCC(trekClient.GetCfgInfo().strPublicMessageURL);

        debugf("Beginning messageoftheday download: %s.\n", szMessageOfTheDayURL);

        m_pSession = CreateHTTPSession(this);

        const char * szFileList[3] = { szMessageOfTheDayURL, szMessageOfTheDayFileName, NULL };

        m_pSession->InitiateDownload(szFileList, trekClient.GetArtPath());
    }

    void OnError(char * szErrorMessage) // on HTTP download error
    {
        // Errors are essentially ignored
        debugf("Error while downloading file.\n");
        debugf("%s\n", szErrorMessage);
        m_bErrorOccured = true;
    }

    bool IsFileValid(char * szFileName)
    {
        ZFile file(szFileName);

        int n = file.GetLength(); // -1 means error
        if (n != -1 && n != 0)
        {
            char * pData = new char[n+1];
            memcpy(pData, file.GetPointer(), n);
            pData[n] = 0;

            if (m_bMessageStage)
            {
                if (strstr(pData, szValidMotd) == NULL)
                {
                    debugf("File %s is not a valid messageoftheday file.\n", szFileName);
#ifdef DEBUG
                    m_pmsgBox = CreateMessageBox("Warning, downloaded Motd file missing validation string");
                    GetWindow()->GetPopupContainer()->OpenPopup(m_pmsgBox, false);
#endif
                    delete[] pData;
                    return false;
                }
            }
            else
            {
                if (strstr(pData, szValidCfg) == NULL)
                {
                    debugf("File %s is not a valid config file.\n", szFileName);
#ifdef DEBUG
                    m_pmsgBox = CreateMessageBox("Warning, CFG file missing validation string");
                    GetWindow()->GetPopupContainer()->OpenPopup(m_pmsgBox, false);
#endif
                    delete[] pData;
                    return false;
                }
            }
            delete[] pData;
        }
        else
        {
             debugf("File %s error while trying to load downloaded config file.\n", szFileName);
             return false;
        }
        return true;
    }

    bool OnFileCompleted(char * szFileName)
    {
        debugf("Downloaded file: %s\n", szFileName);

        char szPath[MAX_PATH+20];
        strcpy(szPath, m_pSession->GetDownloadPath());
        strcat(szPath, szFileName);

        if (!IsFileValid(szPath))
        {
            m_bErrorOccured = true;
            debugf("Aborting either cfg or motd because file is invalid.");

            m_pmsgBox = CreateMessageBox("Unable to reach the lobby.  Please try again later.");
            GetWindow()->GetPopupContainer()->OpenPopup(m_pmsgBox, false);
        }

        return true; // true means don't retry download
    }

    void OnFrame()
    {
        if (m_pSession)
        {
            if(m_bErrorOccured || !m_pSession->ContinueDownload())
            {
                if (m_bMessageStage)
                    OnMessageOfDayDone(!m_bErrorOccured);
                else
                    OnConfigDownloadDone(true, !m_bErrorOccured);
            }
        }
    }

    void OnConfigDownloadDone(bool bUseEXEFolder, bool bDownloadSuccesful)
    {
        m_bMessageStage = true;

        if (m_pSession)
        {
            delete m_pSession;
            m_pSession = NULL;
        }

        if (bUseEXEFolder)
        {
            PathString pathEXE(PathString::GetCurrentDirectory());
            PathString pathConfig(pathEXE + PathString(PathString(m_szConfig).GetFilename()));

            if (bDownloadSuccesful)
            {
                PathString pathTemp(pathEXE + "temp.cfg");
                trekClient.GetCfgInfo().Load(PCC(pathTemp));
                debugf("Loaded downloaded config file: %s\n", PCC(pathTemp));

                // rename file, so that next time if there is an error, there is
                // a recent config file to use.
                debugf("Renaming %s to %s\n", PCC(pathTemp), PCC(pathConfig));
                DeleteFile(PCC(pathConfig));
                MoveFile(PCC(pathTemp), PCC(pathConfig));
            }
            else
            {
                debugf("Error detected, loading existing config file: %s\n", PCC(pathConfig));
                trekClient.GetCfgInfo().Load(PCC(pathConfig));
            }
        }
        else
        {
            debugf("Using local config file %s\n", m_szConfig);
            trekClient.GetCfgInfo().Load(m_szConfig);
        }

        if (m_bErrorOccured)
        {
            m_pbuttonMainMenu->SetEnabled(true); // allow users to leave screen even if cfg file failed to download
            return;
        }

        if (!g_bSkipAutoUpdate
            && trekClient.GetCfgInfo().crcFileList != 0
            && trekClient.GetCfgInfo().nFilelistSize != 0
            && trekClient.GetCfgInfo().strFilelistSite != ""
            && trekClient.GetCfgInfo().strFilelistDirectory != "")
        {
            bool bForceFileCheck = trekClient.ShouldCheckFiles();

            if (bForceFileCheck || trekClient.GetCfgInfo().crcFileList != FileCRC("Filelist.txt", NULL))
            {
                if (trekClient.m_pAutoDownload == NULL)
                    trekClient.m_pAutoDownload = CreateAutoDownload();

                IAutoUpdateSink * pAutoUpdateSink = trekClient.OnBeginAutoUpdate(this, false);
                assert(pAutoUpdateSink);

                trekClient.m_pAutoDownload->SetFTPSite(PCC(trekClient.GetCfgInfo().strFilelistSite),
                                                       PCC(trekClient.GetCfgInfo().strFilelistDirectory),
                                                       "blah", // account
                                                       "blah"); // pw

                trekClient.m_pAutoDownload->SetOfficialFileListAttributes(trekClient.GetCfgInfo().crcFileList,
                                                                          trekClient.GetCfgInfo().nFilelistSize);

                trekClient.m_pAutoDownload->SetArtPath(trekClient.GetArtPath());

                // m_pAutoDownload can be NULL after this if the updater decided not to
                // download after all (an error, or the client is already up to date).
                trekClient.m_pAutoDownload->BeginUpdate(pAutoUpdateSink, bForceFileCheck, false);
            }
            else
            {
                OnAutoUpdateSystemTermination(false, false); // already up to date
            }
        }
        else
        {
            if (!g_bSkipAutoUpdate)
                debugf("Your cfg file is missing one or more of the following:\nFilelistCRC, FilelistSize, FilelistSite, FilelistDirectory\nSkipping AutoUpdate.\n");

            OnAutoUpdateSystemTermination(false, false);
        }

        BeginMessageOfDayDownload();
    }

    void OnMessageOfDayDone(bool bSuccessful)
    {
        if (m_pSession)
        {
            delete m_pSession;
            m_pSession = NULL;
        }

        m_pbuttonMainMenu->SetEnabled(true);

        if (bSuccessful)
        {
            debugf("Loading messageoftheday file.\n");
            m_pMDLFileImage->Load("publicmessageoftheday");
        }
        else
        {
            debugf("Errors detected while downloading message of the day.  Skipping load.\n");
        }
    }

    ////////////////////////////////////////////////////////////////////
    //
    // IAutoUpdateSink
    //
    ////////////////////////////////////////////////////////////////////

    virtual void OnAutoUpdateSystemTermination(bool bErrorOccurred, bool bRestarting)
    {
        m_pbuttonMainMenu->SetEnabled(true);

        if (!bErrorOccurred && !bRestarting)
        {
            m_pbuttonGames->SetEnabled(true);
            m_pbuttonGamesBig->SetEnabled(true);
        }
    }

    ////////////////////////////////////////////////////////////////////
    //
    // IIntegerEventSink
    //
    ////////////////////////////////////////////////////////////////////

    bool OnEvent(IIntegerEventSource* pevent, int value)
    {
        //
        // User must have pressed Okay button after error
        //
        assert(m_pmsgBox);
        GetWindow()->GetPopupContainer()->ClosePopup(m_pmsgBox);
        GetWindow()->RestoreCursor();

        return false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // LogonSite methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void OnLogon(const ZString& strName)
    {
        lstrcpyn(m_szName, strName, sizeof(m_szName));
        trekClient.SaveCharacterName(strName);

        GetWindow()->SetWaitCursor();
        TRef<IMessageBox> pmsgBox = CreateMessageBox("Connecting...", NULL, false);
        Point point(c_PopupX, c_PopupY);
        Rect rect(point, point);
        GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, rect, false);

        // pause to let the "connecting..." box draw itself
        AddEventTarget(&LobbyScreen::OnConnect, GetWindow(), 0.1f);
    }

    bool OnConnect()
    {
        BaseClient::ConnectInfo ci;
        lstrcpyn(ci.szName, m_szName, sizeof(ci.szName));

        trekClient.ConnectToLobby(&ci);

        return false;
    }

    void OnAbort()
    {
    }

    void OnLogonLobby()
    {
        GetWindow()->screen(m_screenPostConnect);
    }

    void OnLogonLobbyFailed(bool bRetry, const char* szReason)
    {
        if (bRetry)
        {
            ConnectToLobby(m_screenPostConnect, szReason);
        }
        else
        {
            TRef<IMessageBox> pmsgBox = CreateMessageBox(szReason);
            GetWindow()->GetPopupContainer()->OpenPopup(pmsgBox, false);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Screen Methods
    //
    //////////////////////////////////////////////////////////////////////////////

    Pane* GetPane()
    {
        return m_ppane;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////////////

TRef<Screen> CreateLobbyScreen(Modeler* pmodeler, Number * ptime)
{
    return new LobbyScreen(pmodeler, ptime);
}
