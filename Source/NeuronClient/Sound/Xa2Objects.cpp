//
// Xa2Objects.cpp: the Windows Spatial Sound renderer (see Xa2Objects.h).
//
// Everything that touches ISpatialAudioClient runs on one render thread in
// the multithreaded apartment, from activation to release, so the game
// thread's COM apartment doesn't matter. The game thread only changes slot
// states (atomics); XAudio2's thread only writes slot streams.
//
// Each pass (the stream's event, normally every 10 ms): new requests get a
// dynamic audio object, or are denied if Windows has none to give; every
// object gets its buffer filled from its stream, with the stream's latest
// position and gain; released slots end their object and become free.
// If the stream fails (the endpoint changes, spatial sound is turned off),
// the renderer stops and reports it, and the sounds go back to XAudio2.
//

#include "pch.h"
#include "Xa2Objects.h"

#include <mmdeviceapi.h>
#include <spatialaudioclient.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace SoundEngine {

namespace {

const UINT32 c_nMaxSlots = 32;

enum SlotState {
    SlotFree,
    SlotRequested,      // acquired by the game, waiting for an audio object
    SlotGranted,        // has an audio object
    SlotDenied,         // Windows had none to give
    SlotReleasing       // released by the game, the object to be ended
};

template<class T> void SafeRelease(T*& p)
{
    if (p) {
        p->Release();
        p = NULL;
    }
}

bool IsFloatMono(const WAVEFORMATEX* pformat)
{
    return pformat != NULL
        && pformat->nChannels == 1
        && pformat->wBitsPerSample == 32
        && (pformat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || pformat->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
}

}

class SpatialObjectsImpl : public SpatialObjects
{
    struct Slot
    {
        std::atomic<int>     m_state;
        ObjectStream         m_stream;
        ISpatialAudioObject* m_pobject;     // render thread only

        Slot() :
            m_state(SlotFree),
            m_pobject(NULL)
        {
        }
    };

    Slot                     m_slots[c_nMaxSlots];
    UINT32                   m_nSlots;
    UINT32                   m_nSampleRate;

    std::thread              m_thread;
    std::atomic<bool>        m_bStop;
    std::atomic<bool>        m_bFailed;

    // start-up hand-shake with the render thread
    std::mutex               m_mutex;
    std::condition_variable  m_cvStarted;
    bool                     m_bStartDone;
    bool                     m_bStarted;

    //
    // Render thread
    //

    void ThreadMain()
    {
        HRESULT hrCom = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

        IMMDeviceEnumerator*             penumerator = NULL;
        IMMDevice*                       pendpoint   = NULL;
        ISpatialAudioClient*             pclient     = NULL;
        IAudioFormatEnumerator*          pformats    = NULL;
        ISpatialAudioObjectRenderStream* pstream     = NULL;
        HANDLE                           hevent      = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        WAVEFORMATEX                     format      = {};
        UINT32                           nMaxObjects = 0;
        bool                             bOk         = false;

        HRESULT hr = ::CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&penumerator);

        if (SUCCEEDED(hr)) hr = penumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pendpoint);
        if (SUCCEEDED(hr)) hr = pendpoint->Activate(__uuidof(ISpatialAudioClient), CLSCTX_INPROC_SERVER, NULL, (void**)&pclient);

        //
        // No dynamic objects: spatial sound is off for this endpoint
        //

        if (SUCCEEDED(hr)) hr = pclient->GetMaxDynamicObjectCount(&nMaxObjects);
        if (SUCCEEDED(hr) && nMaxObjects == 0) hr = E_NOTIMPL;

        //
        // A mono float object format the engine supports
        //

        if (SUCCEEDED(hr)) hr = pclient->GetSupportedAudioObjectFormatEnumerator(&pformats);

        if (SUCCEEDED(hr)) {
            UINT32 nFormats = 0;
            hr = pformats->GetCount(&nFormats);

            bool bFound = false;

            for (UINT32 index = 0; SUCCEEDED(hr) && index < nFormats && !bFound; index++) {
                WAVEFORMATEX* pformat = NULL;

                if (SUCCEEDED(pformats->GetFormat(index, &pformat)) && IsFloatMono(pformat)) {
                    format = *pformat;
                    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
                    format.cbSize     = 0;
                    bFound = true;
                }
            }

            if (SUCCEEDED(hr) && !bFound) hr = E_NOTIMPL;
        }

        if (SUCCEEDED(hr)) hr = pclient->IsAudioObjectFormatSupported(&format);

        if (SUCCEEDED(hr) && hevent == NULL) hr = E_OUTOFMEMORY;

        if (SUCCEEDED(hr)) {
            m_nSlots = nMaxObjects < c_nMaxSlots ? nMaxObjects : c_nMaxSlots;

            SpatialAudioObjectRenderStreamActivationParams params = {};
            params.ObjectFormat          = &format;
            params.StaticObjectTypeMask  = AudioObjectType_None;
            params.MinDynamicObjectCount = 0;
            params.MaxDynamicObjectCount = m_nSlots;
            params.Category              = AudioCategory_GameEffects;
            params.EventHandle           = hevent;
            params.NotifyObject          = NULL;

            PROPVARIANT activation;
            PropVariantInit(&activation);
            activation.vt             = VT_BLOB;
            activation.blob.cbSize    = sizeof(params);
            activation.blob.pBlobData = (BYTE*)&params;

            hr = pclient->ActivateSpatialAudioStream(&activation, __uuidof(ISpatialAudioObjectRenderStream), (void**)&pstream);
        }

        if (SUCCEEDED(hr)) hr = pstream->Start();

        bOk = SUCCEEDED(hr);

        if (bOk) {
            m_nSampleRate = format.nSamplesPerSec;
            debugf("Windows Spatial Sound: %u dynamic objects at %u Hz\n", m_nSlots, m_nSampleRate);
        } else {
            debugf("Windows Spatial Sound not used (%X)\n", hr);
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_bStarted   = bOk;
            m_bStartDone = true;
        }

        m_cvStarted.notify_all();

        if (bOk) {
            Render(pstream, hevent);
            pstream->Stop();
        }

        //
        // Done: end every object
        //

        for (UINT32 index = 0; index < c_nMaxSlots; index++) {
            SafeRelease(m_slots[index].m_pobject);
        }

        if (pstream) {
            pstream->Reset();
        }

        SafeRelease(pstream);
        SafeRelease(pformats);
        SafeRelease(pclient);
        SafeRelease(pendpoint);
        SafeRelease(penumerator);

        if (hevent) {
            ::CloseHandle(hevent);
        }

        if (SUCCEEDED(hrCom)) {
            ::CoUninitialize();
        }
    }

    void Render(ISpatialAudioObjectRenderStream* pstream, HANDLE hevent)
    {
        ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        int nTimeouts = 0;

        while (!m_bStop) {
            //
            // A change of endpoint or render format stops the event; a few
            // quiet passes in a row mean the stream is gone
            //

            if (::WaitForSingleObject(hevent, 100) != WAIT_OBJECT_0) {
                if (++nTimeouts >= 5) {
                    debugf("Windows Spatial Sound: the stream stopped\n");
                    break;
                }
                continue;
            }

            nTimeouts = 0;

            UINT32 nAvailable = 0;
            UINT32 nFrames    = 0;

            if (FAILED(pstream->BeginUpdatingAudioObjects(&nAvailable, &nFrames))) {
                debugf("Windows Spatial Sound: the stream failed\n");
                break;
            }

            for (UINT32 index = 0; index < m_nSlots; index++) {
                RenderSlot(pstream, m_slots[index], nFrames);
            }

            pstream->EndUpdatingAudioObjects();
        }

        m_bFailed = true;
    }

    void RenderSlot(ISpatialAudioObjectRenderStream* pstream, Slot& slot, UINT32 nFrames)
    {
        int state = slot.m_state.load(std::memory_order_acquire);

        if (state == SlotRequested) {
            HRESULT hr = pstream->ActivateSpatialAudioObject(AudioObjectType_Dynamic, &slot.m_pobject);

            if (SUCCEEDED(hr)) {
                state = SlotGranted;
            } else {
                slot.m_pobject = NULL;
                state = SlotDenied;
            }

            // the game may have released it meanwhile: then it stays released
            int expected = SlotRequested;
            if (!slot.m_state.compare_exchange_strong(expected, state)) {
                state = expected;
            }
        }

        if (state == SlotReleasing) {
            if (slot.m_pobject) {
                slot.m_pobject->SetEndOfStream(0);
            }

            SafeRelease(slot.m_pobject);
            slot.m_stream.Clear();
            slot.m_state.store(SlotFree, std::memory_order_release);
            return;
        }

        //
        // An object must get its buffer every pass, or Windows ends it
        //

        if (slot.m_pobject) {
            BYTE*  pbuffer = NULL;
            UINT32 cb      = 0;

            if (SUCCEEDED(slot.m_pobject->GetBuffer(&pbuffer, &cb)) && pbuffer) {
                UINT32 nBufferFrames = cb / sizeof(float);
                slot.m_stream.Read((float*)pbuffer, nBufferFrames < nFrames ? nBufferFrames : nFrames);

                ObjectPose pose = slot.m_stream.GetPose();
                slot.m_pobject->SetPosition(pose.x, pose.y, pose.z);
                slot.m_pobject->SetVolume(pose.fGain < 0 ? 0 : (pose.fGain > 1 ? 1 : pose.fGain));
            }
        }
    }

public:
    SpatialObjectsImpl() :
        m_nSlots(0),
        m_nSampleRate(0),
        m_bStop(false),
        m_bFailed(false),
        m_bStartDone(false),
        m_bStarted(false)
    {
    }

    ~SpatialObjectsImpl()
    {
        m_bStop = true;

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool Start()
    {
        m_thread = std::thread([this]() { ThreadMain(); });

        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvStarted.wait(lock, [this]() { return m_bStartDone; });

        return m_bStarted;
    }

    UINT32 GetSampleRate() override
    {
        return m_nSampleRate;
    }

    int AcquireSlot() override
    {
        if (m_bFailed) {
            return -1;
        }

        for (UINT32 index = 0; index < m_nSlots; index++) {
            int expected = SlotFree;

            if (m_slots[index].m_state.compare_exchange_strong(expected, SlotRequested)) {
                return (int)index;
            }
        }

        return -1;
    }

    bool IsSlotDenied(int nSlot) override
    {
        return nSlot >= 0 && nSlot < (int)m_nSlots
            && m_slots[nSlot].m_state.load(std::memory_order_acquire) == SlotDenied;
    }

    void ReleaseSlot(int nSlot) override
    {
        if (nSlot < 0 || nSlot >= (int)m_nSlots) {
            return;
        }

        // the render thread ends the object and frees the slot (once it has
        // failed, slots are never acquired again)
        m_slots[nSlot].m_state.store(SlotReleasing, std::memory_order_release);
    }

    bool IsFailed() override
    {
        return m_bFailed;
    }

    ObjectStream* GetStream(int nSlot) override
    {
        return nSlot >= 0 && nSlot < (int)m_nSlots ? &m_slots[nSlot].m_stream : NULL;
    }
};

TRef<SpatialObjects> SpatialObjects::Create()
{
    TRef<SpatialObjectsImpl> pobjects = new SpatialObjectsImpl();

    if (!pobjects->Start()) {
        return NULL;
    }

    return (SpatialObjects*)pobjects;
}

}
