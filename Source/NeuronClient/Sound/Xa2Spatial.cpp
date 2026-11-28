//
// xa2spatial.cpp: the head model (spatializer.h) as an XAudio2 effect. A
// plain IXAPO and IXAPOParameters, without the XAPO base classes
// (xapobase.lib): one input buffer of one or two float channels, one stereo
// output buffer at the same rate.
//

#include "pch.h"
#include "spatializer.h"
#include "xa2objects.h"
#include "xa2spatial.h"

#include <xapo.h>
#include <atomic>

#ifndef XAPO_E_FORMAT_UNSUPPORTED
#define XAPO_E_FORMAT_UNSUPPORTED ((HRESULT)0x88970001)     // MAKE_HRESULT(SEVERITY_ERROR, FACILITY_XAPO, 1)
#endif

namespace SoundEngine {

// {6C1E2B3A-5D84-4F3B-9E1C-7A5B2D9C4E10}
static const GUID c_clsidSpatial = { 0x6c1e2b3a, 0x5d84, 0x4f3b, { 0x9e, 0x1c, 0x7a, 0x5b, 0x2d, 0x9c, 0x4e, 0x10 } };

static const XAPO_REGISTRATION_PROPERTIES c_registration = {
    c_clsidSpatial,
    L"Allegiance head model",
    L"Allegiance",
    1, 0,
    XAPO_FLAG_FRAMERATE_MUST_MATCH | XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH | XAPO_FLAG_BUFFERCOUNT_MUST_MATCH,
    1, 1, 1, 1
};

// XAudio2 processes 32-bit floats
static bool IsFloatFormat(const WAVEFORMATEX* pformat)
{
    return pformat != NULL
        && pformat->wBitsPerSample == 32
        && (pformat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || pformat->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
}

static WAVEFORMATEX* CopyFormat(const WAVEFORMATEX* pformat)
{
    UINT32        cb     = sizeof(WAVEFORMATEX) + (pformat->wFormatTag == WAVE_FORMAT_EXTENSIBLE ? pformat->cbSize : 0);
    WAVEFORMATEX* pcopy  = (WAVEFORMATEX*)::CoTaskMemAlloc(cb);

    if (pcopy) {
        memcpy(pcopy, pformat, cb);
    }

    return pcopy;
}

class SpatialEffect : public IXAPO, public IXAPOParameters
{
    std::atomic<ULONG>      m_cRef;

    // the parameters: set by XAudio2 (SetEffectParameters), read on the audio thread
    SRWLOCK                 m_lock;
    SpatialEffectParameters m_params;
    bool                    m_bParamsChanged;
    SpatialEffectParameters m_current;          // audio thread: as last read

    SpatialObjects*         m_pobjects;

    // fixed by LockForProcess
    UINT32                  m_nInChannels;
    UINT32                  m_nSampleRate;
    bool                    m_bLocked;

    Spatializer             m_spatializer;

public:
    SpatialEffect(SpatialObjects* pobjects) :
        m_cRef(1),
        m_bParamsChanged(false),
        m_pobjects(pobjects),
        m_nInChannels(1),
        m_nSampleRate(48000),
        m_bLocked(false)
    {
        ::InitializeSRWLock(&m_lock);
        m_params  = SpatialEffectParameters{ 0, 0, 1, 0, -1, 0 };
        m_current = m_params;
    }

    //
    // IUnknown
    //

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
    {
        if (ppv == NULL) {
            return E_POINTER;
        }

        if (riid == __uuidof(IUnknown) || riid == __uuidof(IXAPO)) {
            *ppv = static_cast<IXAPO*>(this);
        } else if (riid == __uuidof(IXAPOParameters)) {
            *ppv = static_cast<IXAPOParameters*>(this);
        } else {
            *ppv = NULL;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

    STDMETHOD_(ULONG, AddRef)() override
    {
        return ++m_cRef;
    }

    STDMETHOD_(ULONG, Release)() override
    {
        ULONG cRef = --m_cRef;

        if (cRef == 0) {
            delete this;
        }

        return cRef;
    }

    //
    // IXAPO
    //

    STDMETHOD(GetRegistrationProperties)(XAPO_REGISTRATION_PROPERTIES** ppproperties) override
    {
        if (ppproperties == NULL) {
            return E_POINTER;
        }

        *ppproperties = (XAPO_REGISTRATION_PROPERTIES*)::CoTaskMemAlloc(sizeof(XAPO_REGISTRATION_PROPERTIES));

        if (*ppproperties == NULL) {
            return E_OUTOFMEMORY;
        }

        **ppproperties = c_registration;
        return S_OK;
    }

    // Input: float, one or two channels, at the output's rate
    STDMETHOD(IsInputFormatSupported)(const WAVEFORMATEX* poutput, const WAVEFORMATEX* prequested, WAVEFORMATEX** ppsupported) override
    {
        if (prequested == NULL) {
            return E_POINTER;
        }

        bool bSupported =
               IsFloatFormat(prequested)
            && (prequested->nChannels == 1 || prequested->nChannels == 2)
            && (poutput == NULL || poutput->nSamplesPerSec == prequested->nSamplesPerSec);

        if (bSupported) {
            if (ppsupported) {
                *ppsupported = NULL;
            }
            return S_OK;
        }

        if (ppsupported) {
            *ppsupported = CopyFormat(prequested);
        }

        return XAPO_E_FORMAT_UNSUPPORTED;
    }

    // Output: float, two channels, at the input's rate
    STDMETHOD(IsOutputFormatSupported)(const WAVEFORMATEX* pinput, const WAVEFORMATEX* prequested, WAVEFORMATEX** ppsupported) override
    {
        if (prequested == NULL) {
            return E_POINTER;
        }

        bool bSupported =
               IsFloatFormat(prequested)
            && prequested->nChannels == 2
            && (pinput == NULL || pinput->nSamplesPerSec == prequested->nSamplesPerSec);

        if (bSupported) {
            if (ppsupported) {
                *ppsupported = NULL;
            }
            return S_OK;
        }

        if (ppsupported) {
            *ppsupported = CopyFormat(prequested);
        }

        return XAPO_E_FORMAT_UNSUPPORTED;
    }

    STDMETHOD(Initialize)(const void* pdata, UINT32 cbData) override
    {
        return S_OK;
    }

    STDMETHOD_(void, Reset)() override
    {
        m_spatializer.Reset();
    }

    STDMETHOD(LockForProcess)(
        UINT32 nInput, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pinput,
        UINT32 nOutput, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* poutput) override
    {
        if (nInput != 1 || nOutput != 1 || pinput == NULL || poutput == NULL
            || pinput->pFormat == NULL || poutput->pFormat == NULL
            || poutput->pFormat->nChannels != 2
            || (pinput->pFormat->nChannels != 1 && pinput->pFormat->nChannels != 2)
        ) {
            return XAPO_E_FORMAT_UNSUPPORTED;
        }

        m_nInChannels = pinput->pFormat->nChannels;
        m_nSampleRate = pinput->pFormat->nSamplesPerSec;
        m_spatializer.Init((float)m_nSampleRate);
        m_bParamsChanged = true;
        m_bLocked        = true;
        return S_OK;
    }

    STDMETHOD_(void, UnlockForProcess)() override
    {
        m_bLocked = false;
    }

    STDMETHOD_(void, Process)(
        UINT32 nInput, const XAPO_PROCESS_BUFFER_PARAMETERS* pinput,
        UINT32 nOutput, XAPO_PROCESS_BUFFER_PARAMETERS* poutput,
        BOOL bEnabled) override
    {
        if (nInput != 1 || nOutput != 1 || pinput == NULL || poutput == NULL) {
            return;
        }

        //
        // The newest parameters, if they aren't being written right now
        //

        if (::TryAcquireSRWLockShared(&m_lock)) {
            if (m_bParamsChanged) {
                m_current        = m_params;
                m_bParamsChanged = false;

                m_spatializer.SetDirection(
                    HeadDirection{ m_current.x, m_current.y, m_current.z },
                    m_current.bSpatial != 0 && m_current.nSlot < 0 && bEnabled
                );
            }

            ::ReleaseSRWLockShared(&m_lock);
        }

        UINT32       nFrames = pinput->ValidFrameCount;
        const float* pin     = pinput->BufferFlags == XAPO_BUFFER_VALID ? (const float*)pinput->pBuffer : NULL;
        float*       pout    = (float*)poutput->pBuffer;

        //
        // A Windows Spatial Sound object: hand the sound over, with its
        // direction in Windows' frame (+z behind) at a meter, and its gain
        // (the voice's volume and attenuation; Windows doesn't attenuate)
        //

        ObjectStream* pstream = m_pobjects && m_current.nSlot >= 0 ? m_pobjects->GetStream(m_current.nSlot) : NULL;

        if (pstream) {
            HeadDirection d = HeadModel::Normalize(HeadDirection{ m_current.x, m_current.y, m_current.z });
            pstream->Write(pin, m_nInChannels, nFrames, ObjectPose{ d.x, d.y, -d.z, m_current.fGain });
        }

        //
        // Silence in: the filters ring out, then the output is silent too
        //

        if (pin == NULL && m_spatializer.IsQuiet()) {
            poutput->ValidFrameCount = nFrames;
            poutput->BufferFlags     = XAPO_BUFFER_SILENT;
            return;
        }

        m_spatializer.Process(pin, m_nInChannels, pout, nFrames);

        poutput->ValidFrameCount = nFrames;
        poutput->BufferFlags     = XAPO_BUFFER_VALID;
    }

    STDMETHOD_(UINT32, CalcInputFrames)(UINT32 nOutputFrames) override
    {
        return nOutputFrames;
    }

    STDMETHOD_(UINT32, CalcOutputFrames)(UINT32 nInputFrames) override
    {
        return nInputFrames;
    }

    //
    // IXAPOParameters
    //

    STDMETHOD_(void, SetParameters)(const void* pparams, UINT32 cbParams) override
    {
        if (pparams == NULL || cbParams != sizeof(SpatialEffectParameters)) {
            return;
        }

        ::AcquireSRWLockExclusive(&m_lock);
        m_params         = *(const SpatialEffectParameters*)pparams;
        m_bParamsChanged = true;
        ::ReleaseSRWLockExclusive(&m_lock);
    }

    STDMETHOD_(void, GetParameters)(void* pparams, UINT32 cbParams) override
    {
        if (pparams == NULL || cbParams != sizeof(SpatialEffectParameters)) {
            return;
        }

        ::AcquireSRWLockShared(&m_lock);
        *(SpatialEffectParameters*)pparams = m_params;
        ::ReleaseSRWLockShared(&m_lock);
    }
};

HRESULT CreateSpatialEffect(IUnknown** ppeffect, SpatialObjects* pobjects)
{
    if (ppeffect == NULL) {
        return E_POINTER;
    }

    *ppeffect = static_cast<IXAPO*>(new SpatialEffect(pobjects));
    return S_OK;
}

}
