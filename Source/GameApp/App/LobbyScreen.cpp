#include "pch.h"


//////////////////////////////////////////////////////////////////////////////
//
// Lobby Screen
//
// Entry point for Internet play: downloads the config file and the message
// of the day, then logs on to the free lobby.
//
//////////////////////////////////////////////////////////////////////////////

extern bool    g_bDownloadMessageOfDay;
extern bool    g_bDownloadNewConfig;

const char * szValidCfg  = "THIS IS A VALID CONFIG FILE";
const char * szValidMotd = "THIS IS A VALID MESSAGE OF THE DAY FILE";

class LobbyScreen :
    public Screen,
    public LogonSite,
    public TrekClientEventSink,
    public IIntegerEventSink,
    public IHTTPSessionSink,
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
        m_pSession(nullptr),
        m_bErrorOccured(false),
        m_bMessageStage(false),
        m_pmsgBox(nullptr),
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
            m_pSession = nullptr;
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

        lstrcpyn(m_szName, trekClient.GetSavedCharacterName().c_str(), sizeof(m_szName));

        TRef<IPopup> plogonPopup = CreateLogonPopup(m_pmodeler, this, strPrompt.c_str(), m_szName);
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

        Bounded::Copy(m_szConfig, "http://Allegiance.zone.com/Allegiance.cfg");

        // CfgFile: a config file URL, or a local file, instead of the default
        std::string strCfgFile = ZSettings::GetString("CfgFile", "");
        if (!strCfgFile.empty() && strCfgFile.size() < sizeof(m_szConfig))
            Bounded::Copy(m_szConfig, strCfgFile.c_str());

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

        const char * szFileList[] = { m_szConfig, "temp.cfg", nullptr };

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

        int crc = FileCRC(path.c_str(), nullptr);

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

        PCC szMessageOfTheDayURL = trekClient.GetCfgInfo().strPublicMessageURL.c_str();

        debugf("Beginning messageoftheday download: %s.\n", szMessageOfTheDayURL);

        m_pSession = CreateHTTPSession(this);

        const char * szFileList[3] = { szMessageOfTheDayURL, szMessageOfTheDayFileName, nullptr };

        m_pSession->InitiateDownload(szFileList, trekClient.GetArtPath());
    }

    void OnError(char * szErrorMessage) override // on HTTP download error
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
                if (strstr(pData, szValidMotd) == nullptr)
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
                if (strstr(pData, szValidCfg) == nullptr)
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

    bool OnFileCompleted(char * szFileName) override
    {
        debugf("Downloaded file: %s\n", szFileName);

        char szPath[MAX_PATH+20];
        Bounded::Copy(szPath, m_pSession->GetDownloadPath());
        Bounded::Append(szPath, szFileName);

        if (!IsFileValid(szPath))
        {
            m_bErrorOccured = true;
            debugf("Aborting either cfg or motd because file is invalid.");

            m_pmsgBox = CreateMessageBox("Unable to reach the lobby.  Please try again later.");
            GetWindow()->GetPopupContainer()->OpenPopup(m_pmsgBox, false);
        }

        return true; // true means don't retry download
    }

    void OnFrame() override
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
            m_pSession = nullptr;
        }

        if (bUseEXEFolder)
        {
            PathString pathEXE(PathString::GetCurrentDirectory());
            PathString pathConfig(pathEXE + PathString(PathString(m_szConfig).GetFilename()));

            if (bDownloadSuccesful)
            {
                PathString pathTemp(pathEXE + "temp.cfg");
                trekClient.GetCfgInfo().Load(pathTemp.c_str());
                debugf("Loaded downloaded config file: %s\n", pathTemp.c_str());

                // rename file, so that next time if there is an error, there is
                // a recent config file to use.
                debugf("Renaming %s to %s\n", pathTemp.c_str(), pathConfig.c_str());
                DeleteFile(pathConfig.c_str());
                MoveFile(pathTemp.c_str(), pathConfig.c_str());
            }
            else
            {
                debugf("Error detected, loading existing config file: %s\n", pathConfig.c_str());
                trekClient.GetCfgInfo().Load(pathConfig.c_str());
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

        m_pbuttonMainMenu->SetEnabled(true);
        m_pbuttonGames->SetEnabled(true);
        m_pbuttonGamesBig->SetEnabled(true);

        BeginMessageOfDayDownload();
    }

    void OnMessageOfDayDone(bool bSuccessful)
    {
        if (m_pSession)
        {
            delete m_pSession;
            m_pSession = nullptr;
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
    // IIntegerEventSink
    //
    ////////////////////////////////////////////////////////////////////

    bool OnEvent(IIntegerEventSource* pevent, int value) override
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

    void OnLogon(const ZString& strName) override
    {
        lstrcpyn(m_szName, strName.c_str(), sizeof(m_szName));
        trekClient.SaveCharacterName(strName);

        GetWindow()->SetWaitCursor();
        TRef<IMessageBox> pmsgBox = CreateMessageBox("Connecting...", nullptr, false);
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

    void OnAbort() override
    {
    }

    void OnLogonLobby() override
    {
        GetWindow()->screen(m_screenPostConnect);
    }

    void OnLogonLobbyFailed(bool bRetry, const char* szReason) override
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

    Pane* GetPane() override
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
