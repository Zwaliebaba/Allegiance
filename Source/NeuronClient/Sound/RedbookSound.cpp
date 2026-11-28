//
// RedbookSound.cpp
//
// SoundEngine support for redbook audio.
//

#include "pch.h"
#include "SoundBase.h"
#include "RedbookSound.h"

#include <math.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <audiosessiontypes.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using std::list;

namespace SoundEngine {

//////////////////////////////////////////////////////////////////////////////
//
// The music player: the game's soundtrack, which shipped as Audio-CD tracks
// played through MCI, now comes from the WAVs in Artwork/Music, played by
// the Media Foundation Media Engine (audio only). Track n (the CD's track
// numbering; track 1 was the data track) is line n - 1 of tracks.lst.
//
//////////////////////////////////////////////////////////////////////////////

class MusicNotify : public IMFMediaEngineNotify
{
    LONG          m_cRef;
    volatile LONG m_fEnded;     // the current track played to its end
    volatile LONG m_fError;     // the current track could not be played

public:
    MusicNotify() : m_cRef(1), m_fEnded(0), m_fError(0) {}

    void Reset()
    {
        InterlockedExchange(&m_fEnded, 0);
        InterlockedExchange(&m_fError, 0);
    }

    bool IsDone() const { return m_fEnded || m_fError; }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify))
        {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release()
    {
        LONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0)
            delete this;
        return cRef;
    }

    // IMFMediaEngineNotify: called on Media Foundation's threads
    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR, DWORD)
    {
        if (event == MF_MEDIA_ENGINE_EVENT_ENDED)
            InterlockedExchange(&m_fEnded, 1);
        else if (event == MF_MEDIA_ENGINE_EVENT_ERROR)
            InterlockedExchange(&m_fError, 1);
        return S_OK;
    }
};

class DiskPlayerImpl : public IDiskPlayer
{
    std::vector<ZString>  m_vstrTracks;     // file paths, for tracks 2, 3, ...
    IMFMediaEngine*       m_pengine;
    MusicNotify*          m_pnotify;
    bool                  m_fStarted;       // MFStartup succeeded
    int                   m_nCurrentTrack;  // 0 when stopped
    float                 m_fGain;          // dB, 0 to -100

public:

    DiskPlayerImpl() :
        m_pengine(NULL),
        m_pnotify(nullptr),
        m_fStarted(false),
        m_nCurrentTrack(0),
        m_fGain(0)
    {
    }

    ~DiskPlayerImpl()
    {
        if (m_pengine)
        {
            m_pengine->Shutdown();
            m_pengine->Release();
        }
        if (m_pnotify)
            m_pnotify->Release();
        if (m_fStarted)
            MFShutdown();
    }

    HRESULT Init(const ZString& strFolder)
    {
        //
        // The track list
        //

        ZString strList = strFolder + "\\tracks.lst";
        FILE*   pfile = fopen(strList.c_str(), "rt");
        if (!pfile)
        {
            debugf("Music: no track list %s\n", strList.c_str());
            return E_FAIL;
        }

        char szLine[MAX_PATH];
        while (fgets(szLine, sizeof(szLine), pfile))
        {
            char* pch = szLine + strlen(szLine);
            while (pch > szLine && (pch[-1] == '\n' || pch[-1] == '\r' || pch[-1] == ' ' || pch[-1] == '\t'))
                *--pch = '\0';
            char* pchStart = szLine;
            while (*pchStart == ' ' || *pchStart == '\t')
                ++pchStart;
            if (*pchStart)
                m_vstrTracks.push_back(strFolder + "\\" + pchStart);
        }
        fclose(pfile);

        if (m_vstrTracks.empty())
            return E_FAIL;

        //
        // The Media Engine, audio only
        //

        HRESULT hr = MFStartup(MF_VERSION);
        if (FAILED(hr))
            return hr;
        m_fStarted = true;

        IMFMediaEngineClassFactory* pfactory = NULL;
        hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&pfactory));
        if (FAILED(hr))
            return hr;

        IMFAttributes* pattributes = nullptr;
        hr = MFCreateAttributes(&pattributes, 2);
        if (SUCCEEDED(hr))
        {
            m_pnotify = new MusicNotify();
            pattributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, m_pnotify);
            pattributes->SetUINT32(MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_GameMedia);
            hr = pfactory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY, pattributes, &m_pengine);
            pattributes->Release();
        }
        pfactory->Release();

        if (FAILED(hr))
        {
            debugf("Music: could not create the Media Engine (0x%08x)\n", hr);
            return hr;
        }

        return S_OK;
    }

    // plays one track
    HRESULT Play(int nTrack) override
    {
        int iFile = nTrack - 2;
        if (iFile < 0 || iFile >= (int)m_vstrTracks.size())
            return E_INVALIDARG;

        if (nTrack == m_nCurrentTrack && IsPlaying() == S_OK)
            return S_OK;

        const char* szPath = m_vstrTracks[iFile].c_str();
        int         cwch   = MultiByteToWideChar(CP_ACP, 0, szPath, -1, nullptr, 0);
        BSTR        bstr   = SysAllocStringLen(nullptr, cwch);
        if (!bstr)
            return E_OUTOFMEMORY;
        MultiByteToWideChar(CP_ACP, 0, szPath, -1, bstr, cwch);

        m_pnotify->Reset();
        HRESULT hr = m_pengine->SetSource(bstr);
        SysFreeString(bstr);
        if (FAILED(hr))
            return hr;

        SetGain(m_fGain);
        hr = m_pengine->Play();
        m_nCurrentTrack = SUCCEEDED(hr) ? nTrack : 0;
        return hr;
    }

    // stops the music
    HRESULT Stop() override
    {
        if (m_nCurrentTrack)
        {
            m_pengine->Pause();
            m_nCurrentTrack = 0;
        }
        return S_OK;
    }

    // returns S_OK while a track is playing, S_FALSE otherwise
    HRESULT IsPlaying() override
    {
        if (m_nCurrentTrack && m_pnotify->IsDone())
            m_nCurrentTrack = 0;

        return m_nCurrentTrack ? S_OK : S_FALSE;
    }

    // returns the current track
    HRESULT GetCurrentTrack(int& nTrack) override
    {
        IsPlaying();
        nTrack = m_nCurrentTrack;
        return m_nCurrentTrack ? S_OK : E_FAIL;
    }

    // sets the music gain, from 0 to -100 dB (it used to set the system
    // mixer's CD line volume)
    HRESULT SetGain(float fGain) override
    {
        m_fGain = fGain;
        double dVolume = (fGain <= -100.0f) ? 0.0 : pow(10.0, fGain / 20.0);
        return m_pengine->SetVolume(dVolume > 1.0 ? 1.0 : dVolume);
    }
};

// Creates the music player, reading tracks.lst in strFolder (normally the
// artwork's Music folder)
HRESULT CreateDiskPlayer(TRef<IDiskPlayer>& pdiskplayer, const ZString& strFolder)
{
    TRef<DiskPlayerImpl> pdiskplayerimpl = new DiskPlayerImpl();

    HRESULT hr = pdiskplayerimpl->Init(strFolder);

    if (SUCCEEDED(hr))
        pdiskplayer = pdiskplayerimpl;

    return hr;
};


class DummyDiskPlayer : public IDiskPlayer
{
public:
    // plays one track
    HRESULT Play(int nTrack) override
    {
        return S_OK;
    };

    // stops the CD player
    HRESULT Stop() override
    {
        return S_OK;
    };

    // returns S_OK if the CD player is playing
    HRESULT IsPlaying() override
    {
        return S_FALSE;
    };

    // returns the current track of the CD player
    HRESULT GetCurrentTrack(int& nTrack) override
    {
        nTrack = 1;
        return S_OK;
    };


    // sets the gain on the CD player, from 0 to -100 dB
    HRESULT SetGain(float fGain) override
    {
        return S_OK;
    };
};
    
// Creates a new disk player object that only has stubs for each of the calls
HRESULT CreateDummyDiskPlayer(TRef<IDiskPlayer>& pdiskplayer)
{
    pdiskplayer = new DummyDiskPlayer();
    return S_OK;
}



// a template for redbook audio
class RedbookSoundTemplate : public ISoundTemplate
{
private:
    TRef<IDiskPlayer> m_pdiskplayer;
    int m_nTrack;

    //
    // Playback controls
    //
    class RedbookSoundInstance : public ISoundInstance
    {
        TRef<IDiskPlayer> m_pdiskplayer;
        int m_nTrack;

    public:

        RedbookSoundInstance(TRef<IDiskPlayer> pdiskplayer, int nTrack) :
            m_pdiskplayer(pdiskplayer),
            m_nTrack(nTrack)
        {
            pdiskplayer->Play(nTrack);
        }

        // Stops the sound.  If bForceNow is true the sound will stop ASAP, 
        // possibly popping.  If it is false some sounds may play a trail-off 
        // sound or fade away.  
        HRESULT Stop(bool bForceNow = false) override
        {
            HRESULT hr = IsPlaying();

            if (hr == S_OK)
            {
                return m_pdiskplayer->Stop();
            }
            else if (SUCCEEDED(hr))
                return S_OK;
            else
                return hr;
        }

        // returns S_OK if the sound is currently playing, S_FALSE otherwise.
        HRESULT IsPlaying() override
        {
            HRESULT hr = m_pdiskplayer->IsPlaying();

            if (hr == S_OK)
            {
                // it's playing, but is it playing our track or something else?
                int nTrack;
                hr = m_pdiskplayer->GetCurrentTrack(nTrack);

                if (FAILED(hr))
                    return hr;

                return (nTrack == m_nTrack) ? S_OK : S_FALSE;
            }
            else
                return hr;
        }

        // Gets an event which fires when the sound finishes playing (for any 
        // reason)
        IEventSource* GetFinishEventSource() override
        {
            ZError("NYI");
            return nullptr;
        }

        // Gets an interface for tweaking the sound, if supported, NULL otherwise.
        TRef<ISoundTweakable> GetISoundTweakable() override
        {
            return NULL;
        }

        TRef<ISoundTweakable3D> GetISoundTweakable3D() override
        {
            return NULL;
        }
    };


public:
    // tries to initialize the object with the given file.
    HRESULT Init(TRef<IDiskPlayer> pdiskplayer, int nTrack)
    {
        if (!pdiskplayer)
        {
            ZAssert(false);
            return E_POINTER;
        }

        if (nTrack <= 0)
        {
            ZAssert(false);
            return E_INVALIDARG;
        }

        m_pdiskplayer = pdiskplayer;
        m_nTrack = nTrack;

        return S_OK;
    };

    // Creates a new instance of the given sound
    HRESULT CreateSound(TRef<ISoundInstance>& psoundNew, 
        ISoundBufferSource* pbufferSource, ISoundPositionSource* psource = nullptr) override
    {
        if (!pbufferSource)
        {
            ZAssert(false);
            return E_POINTER;
        }

        if (psource)
        {
            ZAssert(false);
            return E_NOTIMPL;
        }

        psoundNew = new RedbookSoundInstance(m_pdiskplayer, m_nTrack);

        return S_OK;
    }
};

// creates a sound template representing a redbook audio track
HRESULT CreateRedbookSoundTemplate(TRef<ISoundTemplate>& pstDest, TRef<IDiskPlayer> pdiskplayer, int nTrack)
{
    TRef<RedbookSoundTemplate> ptemplate = new RedbookSoundTemplate();
    HRESULT hr = ptemplate->Init(pdiskplayer, nTrack);
    
    if (ZSucceeded(hr))
        pstDest = ptemplate; 

    return hr;
}

};