#include "pch.h"
#include <objbase.h>
#include <malloc.h>

//////////////////////////////////////////////////////////////////////////////
//
// Include the main function
//
//////////////////////////////////////////////////////////////////////////////

#include "main.h"

extern bool g_bEnableSound = true;
extern bool g_bCheckFiles;
bool    g_bSkipAutoUpdate = false;
bool    g_bDownloadMessageOfDay = true;
bool    g_bDisableNewCivs = true;
bool    g_bQuickstart = false;
bool    g_bReloaded = false;
int     g_civStart    = -1;
bool    g_bDownloadNewConfig = true;
bool    g_bAskForCDKey = false;

//////////////////////////////////////////////////////////////////////////////
//
// Trek Application Implementation
//
//////////////////////////////////////////////////////////////////////////////

//
// Check to make sure that they are running DX 7 Dsound or better
//
bool CheckDSoundVersion()
{
    // Get version information from the application 
    
    HMODULE hmodDSound = GetModuleHandle("dsound");
    if (NULL == hmodDSound) // why isn't the dll loaded???
      return false;

    char  szDSoundPath[MAX_PATH];
    GetModuleFileName(hmodDSound, szDSoundPath, sizeof(szDSoundPath)); 

    DWORD dwTemp;
    DWORD dwVerInfoSize = GetFileVersionInfoSize(szDSoundPath, &dwTemp);
    if (0 == dwVerInfoSize)
      return false;

    void *pvVerInfo = _alloca(dwVerInfoSize);
    GetFileVersionInfo(szDSoundPath, NULL, dwVerInfoSize, pvVerInfo);

    VS_FIXEDFILEINFO *lpvsFixedFileInfo = NULL;
    unsigned uTemp;
    if (!VerQueryValue(pvVerInfo, "\\", (LPVOID*) &lpvsFixedFileInfo, &uTemp))
      return false;


    // check the version info

    WORD ver1 = HIWORD(lpvsFixedFileInfo->dwFileVersionMS);
    WORD ver2 = LOWORD(lpvsFixedFileInfo->dwFileVersionMS);
    WORD ver3 = HIWORD(lpvsFixedFileInfo->dwFileVersionLS);
    WORD ver4 = LOWORD(lpvsFixedFileInfo->dwFileVersionLS);

    if (ver1 > 5) // assume all major new versions work
      return true;

    if (ver1 == 5) // NT -- WARNING -- if future versions of DX go outside 4.??? and spill into NT versions, we're in trouble
      return ver2 > 0 || ver3 >= 2113; // ver3 == NT build #
    else if (ver1 == 4) // Stand-alone DX runtimes
    {
      if (ver2 > 7) // new major dx versions (DX8+)
        return true;
      else if (ver2 == 7) // DX7
        return ver3 > 0 || ver4 >= 700;
    }

    return false;
}

//
// Check to make sure that they are running DX 7 Dsound or better
//
bool CheckFreeMemory()
{
    const int nAppSize = 40 * 1024 * 1024;

    // first, try a non-invasive check to see if we would fit in the current
    // swap file.  
    MEMORYSTATUS ms;
    GlobalMemoryStatus(&ms);

    if (ms.dwAvailVirtual == -1 || ms.dwAvailPhys == -1 
        || ms.dwAvailVirtual + ms.dwAvailPhys >= nAppSize)
    {
        return true;
    }

    // if that fails, try allocating a big chunk of memory to try to force the 
    // swap file to grow.
    void *pv = VirtualAlloc(NULL, nAppSize, MEM_COMMIT, PAGE_NOACCESS);

    if (pv)
    {
        VirtualFree(pv, 0, MEM_RELEASE);
        return true;
    }
    else
        return false;
}


class TrekAppImpl : public EffectApp {
public:
    TrekAppImpl()
    {
        AddRef();
    }

    HRESULT Initialize(const ZString& strCommandLine)
    {
#if defined(_M_IX86)
        _controlfp(_PC_53, _MCW_PC);    // x87 double precision; x64 has no precision control
#endif

        //
        // Make sure reloader finished correctly--this must be first before any other files are opened
        //
        //
        {
          // Make sure the current path is where Allegiance.exe is for the AutoUpdate: 
          // For Download -AND- for Reloader.exe -AND- Loading FileList which happens 
          // when client logs onto lobby
          //
          char    path[MAX_PATH + 16];
          ::GetModuleFileName(NULL, path, MAX_PATH);
          char*   p = strrchr(path, '\\');
          if (!p)
              p = path;
          else
              p++;

          *p = 0; // erase filename
          ::SetCurrentDirectory(path);

          // MoveInProgress: an auto-update was still moving files when it stopped
          {
              if (ZSettings::GetDWORD("MoveInProgress", 0) == 1)
              {
                  if (::MessageBox(NULL, "The AutoUpdate process failed to finish.  Try again to finish?  (YES is recommended)", "Error", MB_ICONERROR | MB_YESNO) == IDYES)
                  {
                      if (!LaunchReloaderAndExit(false))
                      {
                          ::MessageBox(NULL, "Couldn't launch Reloader.exe", "Fatal Error", MB_ICONERROR);
                          ::ExitProcess(0);
                          return S_FALSE;
                      }
                  }
                  else
                  {
                      ::ExitProcess(0);
                      return S_FALSE;
                  }
              }
          }
        }


        //
        // Check for sufficient free memory
        //

        if (!CheckFreeMemory())
        {
            if (MessageBox(NULL, 
                "You are low on free memory and/or hard drive space.  "
                "You may experience proplems running Allegiance.  Run anyway?", 
                "Allegiance",
                MB_ICONERROR | MB_YESNO
                ) != IDYES)
            {
               ::ExitProcess(0);
               return S_FALSE;
            }
        }

        //
        // Check the DSound version
        //

        if (!CheckDSoundVersion())
        {
            MessageBox(NULL, 
                "Allegiance requires DirectX 7 or higher, which was not detected.  "
                    "Please re-run setup and choose to install DirectX 7.", 
                "Allegiance",
                MB_ICONERROR | MB_OK
                );

            return E_FAIL;
        }


        HRESULT hr = CoInitialize(NULL);

        if (FAILED(hr))
            return hr;

        // Fix success HRESULT
        hr = S_OK;

        EffectApp::Initialize(strCommandLine);

        //
        // get the artpath
        //

        // ArtPath: where the artwork is; by default "artwork" next to the executable
        PathString pathStr(ZSettings::GetString("ArtPath", "").c_str());

        if (pathStr.IsEmpty()) {
            char    szArtPath[MAX_PATH + 16];
            GetModuleFileNameA(NULL, szArtPath, MAX_PATH);
            char*   p = strrchr(szArtPath, '\\');
            if (!p)
                p = szArtPath;
            else
                p++;
            strcpy(p, "artwork");
            pathStr = szArtPath;
        }

        // LogFrameData = 1: log per-frame statistics to the file LogFrameDataPath
        if (ZSettings::GetDWORD("LogFrameData", 0) == 1)
        {
            ZString strFile(ZSettings::GetString("LogFrameDataPath", "").c_str());
            if (!strFile.IsEmpty())
            {
                g_pzfFrameDump = new ZWriteFile(strFile);
                // check for a valid file handle
                if (g_pzfFrameDump->IsValid()) 
                {
                    // dump out the header row of data
                    g_pzfFrameDump->Write(
                            "Mspf,"
                            "Fps,"
                            "Warps,"
                            "Ships,"
                            "Projectiles,"
                            "Asteroids,"
                            "Stations,"
                            "Treasures,"
                            "Missiles,"
                            "SectorWarps,"
                            "SectorShips,"
                            "SectorProjectiles,"
                            "SectorAsteroids,"
                            "SectorStations,"
                            "SectorTreasures,"
                            "SectorMissiles,"
                            "Triangles,"
                            "DrawStringCalls,"
                            "Chars"
                            "\n"
                        );
                }
                else
                {
                  // pop up error message box
                  MessageBox(NULL, "Framerate log file location is invalid.\n"
                                   "Set LogFrameDataPath in the settings file to "
                                   "a valid location.",
                                   "Framerate Logging Error", MB_ICONEXCLAMATION | MB_OK);
                }
            }
        }

        GetModeler()->SetArtPath(pathStr);
        UTL::SetArtPath(pathStr);

        //
        // load the fonts
        //

        TrekResources::Initialize(GetModeler());

        //
        // Initialize the runtime
        //

        srand((unsigned)timeGetTime());

        //
        // Parse the command line
        //

        bool bLogonDialog     = true;
        bool bStartOffline    = false;
        bool bStartTraining   = false;
        bool bMovies          = true;
        bool bSingleInstance  = true;
        bool bSoftware        = false;
        bool bHardware        = false;
        bool bPrimary         = false;
        bool bSecondary       = false;
        ZString strMap;

        PCC pcc = strCommandLine;
        CommandLineToken token(pcc, strCommandLine.GetLength());

        while (token.MoreTokens()) {
            ZString str;

            if (token.IsMinus(str)) 
            {
                if (str == "training") {
                    bStartTraining = true;
                    bLogonDialog = false;
                    bMovies = false;
                } else if (str == "software") {
                    bSoftware = true;
                } else if (str == "hardware") {
                    bHardware = true;
                } else if (str == "primary") {
                    bPrimary = true;
                } else if (str == "secondary") {
                    bSecondary = true;
                } else if (str == "nooutput") {
                    g_bOutput = false;
                } else if (str == "quickstart") {
                    g_bQuickstart = true;
                    float civStart;
                    if (token.IsNumber(civStart)) 
                        g_civStart = (int)civStart;
                } else if (str == "nocfgdl")  {
                    g_bDownloadNewConfig = false;
                } else if (str == "checkfiles") {
                    g_bCheckFiles = true;
                } else if (str == "noautoupdate") {
                    g_bSkipAutoUpdate = true;
                } else if (str == "nosound") {
                    g_bEnableSound = false;
                } else if (str == "nomessage")  {
                    g_bDownloadMessageOfDay = false;
                } else if (str == "nomovies")  {
                    bMovies = false;
                } else if (str == "newcivs")  {
                    g_bDisableNewCivs = false;
                } else if (str == "map")  {
                    if (token.IsString(strMap)) 
                    {
                        bStartOffline = true;
                        bLogonDialog = false;
                    }
                } else if(str == "reloaded") {
                    //
                    // reloaded after an auto-update
                    // 
                    //g_bReloaded = true;

                    // TODO: make two types of reloaded: one for internet, one for zone
                    // that way we know how to log on.  Right now there it just relogs on
                    // using internet connect
                } else if (str == "multi") { 
                    bSingleInstance = false;
                } else if (str == "cdkey") {
                    g_bAskForCDKey = true;
                }                 
            }
            else
            if (token.IsString(str)) 
            {
                // do nothing if string (just skip it!)...this avoids infinite loops if given bad command-lines
            }
        }

        // 
        // Check for other running copies of the app
        //
        if (bSingleInstance)
        {
            HWND hOldInstance = FindWindow(TrekWindow::GetTopLevelWindowClassname(), 
                TrekWindow::GetWindowTitle());

            // if we found another copy of the app
            if (hOldInstance)
            {
                // find out if it has any popups
                HWND hPopup = GetLastActivePopup(hOldInstance);

                // Bring the main window to the top.
                if (SetForegroundWindow(hOldInstance))
                {
                    // If the old instance was minimized, restore it.
                    if (IsIconic(hOldInstance))
                    {
                        ShowWindow(hOldInstance, SW_RESTORE);
                    }

                    // If the old instance had a pop-up, bring that to the foreground
                    if (hPopup != hOldInstance)
                    {
                        SetForegroundWindow(hPopup);
                    }
                }
    
                return S_FALSE;
            }
        }

        //
        // Create the window
        //

        TRef<TrekWindow> pwindow = 
            TrekWindow::Create(
                this, 
                strCommandLine, 
                bMovies,
                bSoftware,
                bHardware,
                bPrimary,
                bSecondary
            );

        if (!pwindow->IsValid()) {
            return E_FAIL;
        }

        //
        // Handling command line options
        //

        if (bStartTraining)
            GetWindow ()->screen (ScreenIDTrainScreen);

        return hr;
    }

    void Terminate()
    {
      EffectApp::Terminate();
      CoUninitialize();
    }
} g_trekImpl;
