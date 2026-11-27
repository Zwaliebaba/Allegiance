//
// xa2voice.cpp: the XAudio2 device and the source voice that plays one sound
//

#include "pch.h"
#include "soundbase.h"
#include "soundmath.h"
#include "x3dpan.h"
#include "xa2voice.h"

#include <memory>

namespace SoundEngine {

// the most a voice's pitch (with doppler) can raise its frequency
const float c_fMaxFrequencyRatio = 8.0f;

/////////////////////////////////////////////////////////////////////////////
//
// XA2Device
//
/////////////////////////////////////////////////////////////////////////////

XA2Device::XA2Device() :
    m_pxaudio2(NULL),
    m_pmaster(NULL),
    m_dwChannelMask(0),
    m_nChannels(0),
    m_fRolloffFactor(1.0f),
    m_fDopplerFactor(1.0f),
    m_fDistanceFactor(1.0f),
    m_vectListenerPosition(0, 0, 0)
{
    memset(m_x3daudio, 0, sizeof(m_x3daudio));
    m_listener = { { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    m_listenerRelative = m_listener;
}

XA2Device::~XA2Device()
{
    if (m_pmaster) {
        m_pmaster->DestroyVoice();
    }

    if (m_pxaudio2) {
        m_pxaudio2->StopEngine();
        m_pxaudio2->Release();
    }
}

HRESULT XA2Device::Init()
{
    HRESULT hr = XAudio2Create(&m_pxaudio2);
    if (FAILED(hr)) {
        debugf("XAudio2Create failed: %X\n", hr);
        return hr;
    }

    // fails without an audio device; the game then uses the dummy engine
    hr = m_pxaudio2->CreateMasteringVoice(&m_pmaster);
    if (FAILED(hr)) {
        debugf("No audio output (CreateMasteringVoice: %X)\n", hr);
        return hr;
    }

    XAUDIO2_VOICE_DETAILS details;
    m_pmaster->GetVoiceDetails(&details);
    m_nChannels = details.InputChannels;

    m_pmaster->GetChannelMask(&m_dwChannelMask);

    if (m_dwChannelMask == 0) {
        m_dwChannelMask = m_nChannels == 1 ? SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    }

    SetDistanceFactor(m_fDistanceFactor);

    debugf("XAudio2: %u output channels (mask %X) at %u Hz\n", m_nChannels, m_dwChannelMask, details.InputSampleRate);
    return S_OK;
}

void XA2Device::SetDistanceFactor(float fMetersPerUnit)
{
    m_fDistanceFactor = fMetersPerUnit;

    // X3DAUDIO_SPEED_OF_SOUND, in meters per second
    if (!X3DInitialize(m_dwChannelMask, 343.5f / fMetersPerUnit, m_x3daudio)) {
        debugf("X3DAudioInitialize failed\n");
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// XA2Voice
//
/////////////////////////////////////////////////////////////////////////////

//
// The voices of one sound share one copy of its data. The cache holds weak
// references, so the copy goes with the last voice; each voice also holds
// the sound, so a cached address can't be a different sound's.
//

static std::shared_ptr<const std::vector<BYTE>> GetSoundBytes(ISoundPCMData* pdata)
{
    static std::map<ISoundPCMData*, std::weak_ptr<const std::vector<BYTE>>> s_cache;

    std::shared_ptr<const std::vector<BYTE>> pbytes = s_cache[pdata].lock();

    if (!pbytes) {
        std::shared_ptr<std::vector<BYTE>> pnew = std::make_shared<std::vector<BYTE>>(pdata->GetSize());

        if (!pnew->empty()) {
            pdata->GetData(pnew->data(), 0, (unsigned)pnew->size());
        }

        pbytes = pnew;
        s_cache[pdata] = pbytes;

        // drop the entries of sounds that have gone
        for (auto iter = s_cache.begin(); iter != s_cache.end();) {
            iter = iter->second.expired() ? s_cache.erase(iter) : std::next(iter);
        }
    }

    return pbytes;
}

XA2Voice::XA2Voice() :
    m_pvoice(NULL),
    m_nChannels(1),
    m_nBlockAlign(1),
    m_nLength(0),
    m_bLooping(false),
    m_bASR(false),
    m_nLoopStart(0),
    m_nLoopLength(0),
    m_bSupport3D(false),
    m_bStarted(false),
    m_bStopped(false),
    m_fGain(0),
    m_fPitch(1.0f),
    m_fDoppler(1.0f),
    m_b3D(false),
    m_bListenerRelative(false),
    m_fMinimumDistance(1.0f),
    m_fInnerAngle(360), m_fOuterAngle(360), m_fOutsideGain(0),
    m_vectPosition(0, 0, 0),
    m_vectVelocity(0, 0, 0),
    m_vectOrientation(0, 0, 1)
{
}

XA2Voice::~XA2Voice()
{
    // synchronous: the audio thread is done with the data afterwards
    if (m_pvoice) {
        m_pvoice->DestroyVoice();
    }
}

HRESULT XA2Voice::Init(XA2Device* pdevice, ISoundPCMData* pdata, bool bLooping,
    bool bASR, DWORD dwLoopStart, DWORD dwLoopLength, bool bSupport3D)
{
    if (!pdevice || !pdata) {
        ZAssert(false);
        return E_POINTER;
    }

    m_pdevice     = pdevice;
    m_pdata       = pdata;
    m_nChannels   = pdata->GetNumberOfChannels();
    m_nBlockAlign = pdata->GetBytesPerSample();
    m_bLooping    = bLooping;
    m_bASR        = bASR;
    m_bSupport3D  = bSupport3D;

    if (m_nBlockAlign == 0 || m_nChannels == 0 || m_nChannels > 2) {
        ZAssert(false);
        return E_INVALIDARG;
    }

    m_nLength     = pdata->GetSize() / m_nBlockAlign;
    m_nLoopStart  = dwLoopStart / m_nBlockAlign;
    m_nLoopLength = dwLoopLength / m_nBlockAlign;

    WAVEFORMATEX format;
    format.wFormatTag      = WAVE_FORMAT_PCM;
    format.nChannels       = (WORD)m_nChannels;
    format.nSamplesPerSec  = pdata->GetSampleRate();
    format.wBitsPerSample  = (WORD)pdata->GetBitsPerSample();
    format.nBlockAlign     = (WORD)m_nBlockAlign;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize          = 0;

    HRESULT hr = pdevice->m_pxaudio2->CreateSourceVoice(&m_pvoice, &format, 0, c_fMaxFrequencyRatio);
    if (FAILED(hr)) {
        debugf("CreateSourceVoice failed: %X\n", hr);
        m_pvoice = NULL;
        return hr;
    }

    m_pbytes = GetSoundBytes(pdata);

    ApplyFrequency();
    ApplyOutput();
    return S_OK;
}

HRESULT XA2Voice::Submit(const VoiceRegion& region, bool bLast)
{
    m_regions.push_back(region);

    XAUDIO2_BUFFER buffer = {};
    buffer.Flags      = bLast ? XAUDIO2_END_OF_STREAM : 0;
    buffer.AudioBytes = (UINT32)m_pbytes->size();
    buffer.pAudioData = m_pbytes->data();
    buffer.PlayBegin  = region.m_nBegin;
    buffer.PlayLength = region.m_nLength;
    buffer.pContext   = (void*)(INT_PTR)m_regions.size();

    if (region.m_bLoop) {
        buffer.LoopBegin  = region.m_nBegin;
        buffer.LoopLength = region.m_nLength;
        buffer.LoopCount  = XAUDIO2_LOOP_INFINITE;
    }

    return m_pvoice->SubmitSourceBuffer(&buffer);
}

HRESULT XA2Voice::Start(DWORD dwPosition, bool bIsStopping)
{
    if (!m_pvoice || m_bStarted) {
        ZAssert(false);
        return E_UNEXPECTED;
    }

    std::vector<VoiceRegion> regions = PlanVoiceRegions(m_nLength, dwPosition / m_nBlockAlign,
        m_bLooping, m_bASR, m_nLoopStart, m_nLoopLength, bIsStopping);

    for (size_t i = 0; i < regions.size(); i++) {
        HRESULT hr = Submit(regions[i], i + 1 == regions.size());
        if (FAILED(hr)) return hr;
    }

    m_bStarted = true;

    // nothing left to play: it has finished
    if (regions.empty()) {
        return S_OK;
    }

    return m_pvoice->Start(0);
}

HRESULT XA2Voice::Stop(bool bForceNow)
{
    if (!m_pvoice || !m_bStarted || m_bStopped) {
        return S_OK;
    }

    if (bForceNow || !m_bASR) {
        m_pvoice->Stop(0);
        m_pvoice->FlushSourceBuffers();
        m_bStopped = true;
        return S_OK;
    }

    //
    // An ASR sound plays its release: from a sustain loop pass, at the end
    // of the pass; from the attack, straight after it (skipping the
    // sustain, as the position tracker has it)
    //

    XAUDIO2_VOICE_STATE state;
    m_pvoice->GetState(&state);

    size_t index = (size_t)(INT_PTR)state.pCurrentBufferContext;

    if (state.BuffersQueued == 0 || index == 0 || index > m_regions.size()) {
        return S_OK;
    }

    switch (m_regions[index - 1].m_part) {
        case VoiceRegion::SustainLoop:
            return m_pvoice->ExitLoop();

        case VoiceRegion::Attack:
        case VoiceRegion::Sustain:
        {
            // drop the queued sustain loop and release (the playing buffer
            // stays), then queue the release again
            HRESULT hr = m_pvoice->FlushSourceBuffers();
            if (FAILED(hr)) return hr;

            unsigned nLoopEnd = m_nLoopStart + m_nLoopLength;

            if (m_nLength > nLoopEnd) {
                return Submit({ VoiceRegion::Release, nLoopEnd, m_nLength - nLoopEnd, false }, true);
            }

            return S_OK;
        }

        default:
            return S_OK;
    }
}

bool XA2Voice::IsPlaying()
{
    if (!m_pvoice || !m_bStarted || m_bStopped) {
        return false;
    }

    XAUDIO2_VOICE_STATE state;
    m_pvoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued > 0;
}

void XA2Voice::ApplyFrequency()
{
    float fRatio = m_fPitch * m_fDoppler;

    fRatio = max(fRatio, XAUDIO2_MIN_FREQ_RATIO);
    fRatio = min(fRatio, c_fMaxFrequencyRatio);

    m_pvoice->SetFrequencyRatio(fRatio);
}

//
// The output matrix: unpositioned, a mono sound goes to the front left and
// right at full level (DirectSound's centre pan) and a stereo one to its
// sides; positioned, X3DAudio's pan scaled by DirectSound's distance and cone
// attenuation (a stereo sound is panned as one point, each side at half)
//

void XA2Voice::ApplyOutput()
{
    XA2Device* pdevice = m_pdevice;
    UINT32     nOut    = pdevice->m_nChannels;
    float      matrix[2 * XAUDIO2_MAX_AUDIO_CHANNELS] = {};

    if (nOut == 0 || nOut > XAUDIO2_MAX_AUDIO_CHANNELS) {
        return;
    }

    if (!(m_bSupport3D && m_b3D)) {
        if (nOut == 1) {
            matrix[0] = m_nChannels == 1 ? 1.0f : 0.5f;
            matrix[1] = 0.5f;
        } else if (m_nChannels == 1) {
            matrix[0] = 1.0f;                       // front left
            matrix[1] = 1.0f;                       // front right
        } else {
            matrix[0 * 2 + 0] = 1.0f;               // left to front left
            matrix[1 * 2 + 1] = 1.0f;               // right to front right
        }

        if (m_fDoppler != 1.0f) {
            m_fDoppler = 1.0f;
            ApplyFrequency();
        }
    } else {
        const PanListener& listener = m_bListenerRelative ? pdevice->m_listenerRelative : pdevice->m_listener;

        float pan[XAUDIO2_MAX_AUDIO_CHANNELS] = {};
        float fDoppler = X3DPan(pdevice->m_x3daudio, listener,
            ConvertVector(m_vectPosition), ConvertVector(m_vectVelocity),
            pdevice->m_fDopplerFactor, nOut, pan);

        //
        // DirectSound's attenuation
        //

        Vector vectToListener = m_bListenerRelative
            ? -m_vectPosition
            : pdevice->m_vectListenerPosition - m_vectPosition;
        float fDistance = vectToListener.Length();
        float fCone     = 0;

        if (fDistance > 0) {
            fCone = ConeGain(m_vectOrientation * vectToListener / fDistance,
                m_fInnerAngle, m_fOuterAngle, m_fOutsideGain);
        }

        float fAttenuation = DistanceAttenuation(fDistance, m_fMinimumDistance, pdevice->m_fRolloffFactor)
            * DbToAmplitude(fCone);

        for (UINT32 out = 0; out < nOut; out++) {
            if (m_nChannels == 1) {
                matrix[out] = pan[out] * fAttenuation;
            } else {
                matrix[out * 2 + 0] = pan[out] * fAttenuation * 0.5f;
                matrix[out * 2 + 1] = pan[out] * fAttenuation * 0.5f;
            }
        }

        if (m_fDoppler != fDoppler) {
            m_fDoppler = fDoppler;
            ApplyFrequency();
        }
    }

    m_pvoice->SetOutputMatrix(NULL, m_nChannels, nOut, matrix);
}

HRESULT XA2Voice::UpdateState(float fGain, float fPitch)
{
    if (fGain < -100 || fGain > 0) {
        ZAssert(false);
        return E_INVALIDARG;
    }

    if (!m_pvoice) {
        return E_UNEXPECTED;
    }

    if (m_fGain != fGain) {
        m_fGain = fGain;
        m_pvoice->SetVolume(DbToAmplitude(m_fGain));
    }

    if (m_fPitch != fPitch) {
        m_fPitch = fPitch;
        ApplyFrequency();
    }

    return S_OK;
}

HRESULT XA2Voice::UpdateState3D(
    const Vector& vectPosition,
    const Vector& vectVelocity,
    const Vector& vectOrientation,
    float fMinimumDistance,
    float fInnerAngle,
    float fOuterAngle,
    float fOutsideGain,
    bool b3D,
    bool bListenerRelative
    )
{
    if (!m_bSupport3D) {
        ZAssert(false);
        return E_NOTIMPL;
    }

    ZAssertIsUnitVector(vectOrientation);
    if ((fMinimumDistance <= 0)
        || (fInnerAngle < 0 || fInnerAngle > 360)
        || (fOuterAngle < 0 || fOuterAngle > 360)
        || (fOutsideGain < -100 || fOutsideGain > 0)
        )
    {
        ZAssert(false);
        return E_INVALIDARG;
    }

    if (!m_pvoice) {
        return E_UNEXPECTED;
    }

    m_vectPosition      = vectPosition;
    m_vectVelocity      = vectVelocity;
    m_vectOrientation   = vectOrientation;
    m_fMinimumDistance  = fMinimumDistance;
    m_fInnerAngle       = fInnerAngle;
    m_fOuterAngle       = fOuterAngle;
    m_fOutsideGain      = fOutsideGain;
    m_b3D               = b3D;
    m_bListenerRelative = bListenerRelative;

    // the listener moves every frame, so this is applied every time
    ApplyOutput();
    return S_OK;
}

#ifdef _DEBUG
ZString XA2Voice::DebugDump(const ZString& strIndent)
{
    return strIndent + "XA2Voice: "
        + (m_b3D ? " 3D" : " 2D")
        + (IsPlaying() ? " playing" : "")
        + (m_bLooping ? " looping" : "")
        + (m_bASR ? " ASR" : "")
        + (m_b3D ? ("\n" + strIndent + "  Pos("
                + StrFromFloat(m_vectPosition.X()) + ", " + StrFromFloat(m_vectPosition.Y()) + ", "
                + StrFromFloat(m_vectPosition.Z()) + ")")
                + (m_bListenerRelative ? " listener rel" : "")
            : ZString(""))
        + "\n";
}
#endif

};
