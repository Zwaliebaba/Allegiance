#include "pch.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

// winbase.h defines GetCurrentTime() as GetTickCount(), a 16-bit Windows
// leftover that would rewrite IMFMediaEngine::GetCurrentTime
#undef GetCurrentTime
#include <mfmediaengine.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

//////////////////////////////////////////////////////////////////////////////
//
// VideoImage
//
// Plays a movie (the intro logos) with Media Foundation. The original used
// DirectShow's multimedia streaming API into a DirectDraw surface, and the
// movies were Indeo AVIs, which Windows no longer decodes; they were
// converted once to H.264/AAC MP4 (MODERNIZATION.md, "Movies").
//
// The video comes from a source reader, which decodes to RGB32 frames that
// are copied into an engine surface and drawn scaled into the image's rect.
// The sound plays through an audio-only Media Engine, whose position is the
// clock the frames follow (wall-clock time if the file has no sound).
//
//////////////////////////////////////////////////////////////////////////////

class VideoImageImpl : public VideoImage {
public:
    RectValue* GetRect() { return RectValue::Cast(GetChild(0)); }

    class AudioNotify : public IMFMediaEngineNotify
    {
        LONG m_cRef;

    public:
        AudioNotify() : m_cRef(1) {}

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
        {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify))
            {
                *ppv = static_cast<IMFMediaEngineNotify*>(this);
                AddRef();
                return S_OK;
            }
            *ppv = NULL;
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
        STDMETHODIMP EventNotify(DWORD, DWORD_PTR, DWORD) { return S_OK; }
    };

    TRef<EventSourceImpl> m_peventSource;
    TRef<Engine>          m_pengine;
    TRef<Surface>         m_psurface;
    ZString               m_strFile;

    IMFSourceReader*      m_preader;
    IMFMediaEngine*       m_paudio;
    bool                  m_bStarted;     // MFStartup succeeded
    bool                  m_bPlaying;
    bool                  m_bTriggered;
    bool                  m_bEndOfStream;
    bool                  m_bFirstFrame;
    WinPoint              m_sizeVideo;
    LONGLONG              m_timeNextFrame;  // 100 ns; the timestamp of m_psamplePending
    IMFSample*            m_psamplePending; // decoded, not yet due
    DWORD                 m_dwStartTick;

    VideoImageImpl(
        Engine*        pengine,
        RectValue*     prect,
        const ZString& str
    ) :
        VideoImage(prect),
        m_peventSource(new EventSourceImpl()),
        m_pengine(pengine),
        m_strFile(str),
        m_preader(NULL),
        m_paudio(NULL),
        m_bStarted(false),
        m_bPlaying(false),
        m_bTriggered(false),
        m_bEndOfStream(false),
        m_bFirstFrame(true),
        m_sizeVideo(0, 0),
        m_timeNextFrame(0),
        m_psamplePending(NULL),
        m_dwStartTick(0)
    {
        Open();
    }

    ~VideoImageImpl()
    {
        if (m_psamplePending)
            m_psamplePending->Release();
        if (m_preader)
            m_preader->Release();
        if (m_paudio)
        {
            m_paudio->Shutdown();
            m_paudio->Release();
        }
        if (m_bStarted)
            MFShutdown();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Opening the movie
    //
    //////////////////////////////////////////////////////////////////////////////

    static BSTR WidePath(const ZString& str)
    {
        int  cwch = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        BSTR bstr = SysAllocStringLen(NULL, cwch);
        if (bstr)
            MultiByteToWideChar(CP_ACP, 0, str, -1, bstr, cwch);
        return bstr;
    }

    void Open()
    {
        if (GetFileAttributesA(m_strFile) == INVALID_FILE_ATTRIBUTES)
            return;

        if (FAILED(MFStartup(MF_VERSION)))
            return;
        m_bStarted = true;

        BSTR bstrPath = WidePath(m_strFile);
        if (!bstrPath)
            return;

        //
        // Video: a source reader converting to RGB32
        //

        IMFAttributes* pattributes = NULL;
        if (SUCCEEDED(MFCreateAttributes(&pattributes, 1)))
        {
            pattributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            if (FAILED(MFCreateSourceReaderFromURL(bstrPath, pattributes, &m_preader)))
                m_preader = NULL;
            pattributes->Release();
        }

        if (m_preader)
        {
            m_preader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
            m_preader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

            IMFMediaType* ptype = NULL;
            HRESULT hr = MFCreateMediaType(&ptype);
            if (SUCCEEDED(hr))
            {
                ptype->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                ptype->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                hr = m_preader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, ptype);
                ptype->Release();
            }

            IMFMediaType* pcurrent = NULL;
            UINT32 cx = 0, cy = 0;
            if (SUCCEEDED(hr) && SUCCEEDED(m_preader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pcurrent)))
            {
                MFGetAttributeSize(pcurrent, MF_MT_FRAME_SIZE, &cx, &cy);
                pcurrent->Release();
            }

            if (FAILED(hr) || cx == 0 || cy == 0)
            {
                debugf("VideoImage: cannot decode %s (0x%08x)\n", (const char*)m_strFile, hr);
                m_preader->Release();
                m_preader = NULL;
            }
            else
            {
                m_sizeVideo = WinPoint((int)cx, (int)cy);
                m_psurface  = m_pengine->CreateSurface(m_sizeVideo, SurfaceType2D());
                m_psurface->FillSurface(Color(0, 0, 0));
            }
        }

        //
        // Sound: an audio-only Media Engine (optional)
        //

        if (m_preader)
        {
            IMFMediaEngineClassFactory* pfactory = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfactory))))
            {
                IMFAttributes* pengineAttributes = NULL;
                if (SUCCEEDED(MFCreateAttributes(&pengineAttributes, 1)))
                {
                    AudioNotify* pnotify = new AudioNotify();
                    pengineAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, pnotify);
                    pnotify->Release();

                    if (FAILED(pfactory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY, pengineAttributes, &m_paudio)))
                        m_paudio = NULL;
                    else if (FAILED(m_paudio->SetSource(bstrPath)))
                    {
                        m_paudio->Shutdown();
                        m_paudio->Release();
                        m_paudio = NULL;
                    }
                    pengineAttributes->Release();
                }
                pfactory->Release();
            }
        }

        SysFreeString(bstrPath);
    }

    bool IsValid()
    {
        return m_preader != NULL;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Playback
    //
    //////////////////////////////////////////////////////////////////////////////

    void End()
    {
        if (m_paudio)
            m_paudio->Pause();

        if (!m_bTriggered) {
            m_bTriggered = true;
            m_peventSource->Trigger();
        }
    }

    // where playback is, in 100 ns units
    LONGLONG Position()
    {
        if (m_paudio)
        {
            double dSeconds = m_paudio->GetCurrentTime();
            if (dSeconds > 0)
                return (LONGLONG)(dSeconds * 10000000.0);
        }
        return (LONGLONG)(GetTickCount() - m_dwStartTick) * 10000;
    }

    void CopyFrame(IMFSample* psample)
    {
        IMFMediaBuffer* pbuffer = NULL;
        if (FAILED(psample->ConvertToContiguousBuffer(&pbuffer)))
            return;

        BYTE* pbScan0 = NULL;
        LONG  lPitch  = 0;
        IMF2DBuffer* p2d = NULL;
        BYTE* pbLocked = NULL;
        DWORD cbLocked = 0;

        if (SUCCEEDED(pbuffer->QueryInterface(IID_PPV_ARGS(&p2d))))
        {
            if (FAILED(p2d->Lock2D(&pbScan0, &lPitch)))
                pbScan0 = NULL;
        }
        else if (SUCCEEDED(pbuffer->Lock(&pbLocked, NULL, &cbLocked)))
        {
            // a plain buffer: top-down rows of width * 4 bytes
            if (cbLocked >= (DWORD)(m_sizeVideo.X() * m_sizeVideo.Y() * 4))
            {
                pbScan0 = pbLocked;
                lPitch  = m_sizeVideo.X() * 4;
            }
        }

        if (pbScan0)
        {
            //
            // BGRX to the surface's pixel format (normally BGRA, where the
            // frame is opaque)
            //

            PixelFormat* ppf    = m_psurface->GetPixelFormat();
            DWORD        cbPixel = ppf->PixelBytes();
            BYTE*        pdest  = m_psurface->GetWritablePointer();
            int          pitch  = m_psurface->GetPitch();
            bool         bBGRA  =
                   cbPixel             == 4
                && ppf->RedMask()      == 0x00ff0000
                && ppf->GreenMask()    == 0x0000ff00
                && ppf->BlueMask()     == 0x000000ff;

            for (int y = 0; y < m_sizeVideo.Y(); y++)
            {
                const BYTE* psrc = pbScan0 + (LONGLONG)lPitch * y;
                BYTE*       prow = pdest + pitch * y;

                if (bBGRA)
                {
                    const DWORD* psrc32 = (const DWORD*)psrc;
                    DWORD        alpha  = ppf->AlphaMask();

                    for (int x = 0; x < m_sizeVideo.X(); x++)
                        ((DWORD*)prow)[x] = (psrc32[x] & 0x00ffffff) | alpha;

                    continue;
                }

                for (int x = 0; x < m_sizeVideo.X(); x++, psrc += 4)
                {
                    DWORD pixel = ppf->MakePixel(Color(psrc[2] / 255.0f, psrc[1] / 255.0f, psrc[0] / 255.0f)).Value();

                    if (cbPixel == 2)
                        ((WORD*)prow)[x] = (WORD)pixel;
                    else if (cbPixel == 4)
                        ((DWORD*)prow)[x] = pixel;
                }
            }

            m_psurface->ReleasePointer();
        }

        if (p2d)
        {
            if (pbScan0)
                p2d->Unlock2D();
            p2d->Release();
        }
        else if (pbLocked)
            pbuffer->Unlock();

        pbuffer->Release();
    }

    // show the latest frame that is due; returns false at the end
    bool Advance()
    {
        LONGLONG position = Position();

        for (;;)
        {
            if (!m_psamplePending)
            {
                if (m_bEndOfStream)
                    return false;

                DWORD      dwFlags = 0;
                LONGLONG   timestamp = 0;
                IMFSample* psample = NULL;
                HRESULT hr = m_preader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &dwFlags, &timestamp, &psample);

                if (FAILED(hr) || (dwFlags & (MF_SOURCE_READERF_ENDOFSTREAM | MF_SOURCE_READERF_ERROR)))
                {
                    if (psample)
                        psample->Release();
                    m_bEndOfStream = true;
                    return false;
                }

                if (!psample)
                    continue;   // a gap or a format change: read on

                m_psamplePending = psample;
                m_timeNextFrame  = timestamp;
            }

            if (m_timeNextFrame > position)
                return true;    // not due yet

            CopyFrame(m_psamplePending);
            m_psamplePending->Release();
            m_psamplePending = NULL;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // VideoImage methods
    //
    //////////////////////////////////////////////////////////////////////////////

    IEventSource* GetEventSource()
    {
        return m_peventSource;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Image methods
    //
    //////////////////////////////////////////////////////////////////////////////

    void CalcBounds()
    {
        m_bounds.SetRect(GetRect()->GetValue());
    }

    void Render(Context* pcontextArg)
    {
        // the original skipped the first frame too
        if (m_bFirstFrame) {
            m_bFirstFrame = false;
            return;
        }

        if (!m_preader) {
            End();
            return;
        }

        if (!m_bPlaying) {
            m_bPlaying    = true;
            m_dwStartTick = GetTickCount();
            if (m_paudio)
                m_paudio->Play();
        }

        bool bMore = Advance();

        //
        // Scale the frame to fit the rect, keeping its aspect ratio (the
        // original doubled a small movie, or centred it)
        //

        PrivateContext* pcontext; CastTo(pcontext, pcontextArg);
        WinRect  rect = WinRect::Cast(GetRect()->GetValue());
        WinPoint size = m_sizeVideo;

        int nScaleX = rect.XSize() * 1000 / size.X();
        int nScaleY = rect.YSize() * 1000 / size.Y();
        int nScale  = nScaleX < nScaleY ? nScaleX : nScaleY;
        WinPoint sizeDest(size.X() * nScale / 1000, size.Y() * nScale / 1000);
        WinPoint offset = rect.Min() + (rect.Size() - sizeDest) / 2;

        pcontext->GetSurface()->BitBlt(WinRect(offset, offset + sizeDest), m_psurface);

        if (!bMore) {
            End();
        }
    }

    ZString GetFunctionName() { return "VideoImage"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

TRef<VideoImage> CreateVideoImage(
    Engine*        pengine,
    RectValue*     prect,
    const ZString& str
) {
    return new VideoImageImpl(pengine, prect, str);
}
