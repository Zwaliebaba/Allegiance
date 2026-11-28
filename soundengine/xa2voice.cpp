//
// xa2voice.cpp: the XAudio2 device and the source voice that plays one sound
//

#include "pch.h"
#include "soundbase.h"
#include "soundmath.h"
#include "x3dpan.h"
#include "xa2voice.h"
#include "spatializer.h"
#include "xa2objects.h"
#include "xa2spatial.h"

#include <memory>

namespace SoundEngine {

// the most a voice's pitch (with doppler) can raise its frequency
const float c_fMaxFrequencyRatio = 8.0f;

// the distance low-pass: XAudio2's state-variable filter, Butterworth (Q of 0.707)
const float c_fFilterOneOverQ = 1.4142f;

/////////////////////////////////////////////////////////////////////////////
//
// XA2Device
//
/////////////////////////////////////////////////////////////////////////////

XA2Device::XA2Device() :
    m_pxaudio2(NULL),
    m_pmaster(NULL),
    m_preverb(NULL),
    m_dwChannelMask(0),
    m_nChannels(0),
    m_nSampleRate(48000),
    m_bHeadphones(false),
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
    // the render thread stops (the voices, holding the device, are gone)
    m_pobjects = NULL;

    if (m_preverb) {
        m_preverb->DestroyVoice();
    }

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

    //
    // Windows Spatial Sound, if the player has turned it on (Windows Sonic
    // for Headphones, Dolby Atmos, DTS:X): positioned sounds become its
    // audio objects. The mastering voice then runs at the objects' rate, so
    // the voices' effects hand over samples at that rate.
    //

    if (ZSettings::GetDWORD("WindowsSpatialSound", 1) != 0) {
        m_pobjects = SpatialObjects::Create();
    }

    // fails without an audio device; the game then uses the dummy engine
    hr = m_pxaudio2->CreateMasteringVoice(&m_pmaster, XAUDIO2_DEFAULT_CHANNELS,
        m_pobjects ? m_pobjects->GetSampleRate() : XAUDIO2_DEFAULT_SAMPLERATE);
    if (FAILED(hr)) {
        debugf("No audio output (CreateMasteringVoice: %X)\n", hr);
        return hr;
    }

    XAUDIO2_VOICE_DETAILS details;
    m_pmaster->GetVoiceDetails(&details);
    m_nChannels   = details.InputChannels;
    m_nSampleRate = details.InputSampleRate;

    //
    // Stereo output is taken to be headphones: positioned sounds go through
    // the head model (spatializer.h). On stereo speakers its crosstalk is
    // wrong, so the HeadphoneSpatialization setting turns it off. More
    // channels (5.1, 7.1, and Windows Sonic or Dolby Atmos, which present
    // 7.1) keep X3DAudio's pan.
    //

    m_bHeadphones = m_nChannels == 2 && ZSettings::GetDWORD("HeadphoneSpatialization", 1) != 0;

    if (m_pobjects && m_nSampleRate != m_pobjects->GetSampleRate()) {
        debugf("Windows Spatial Sound not used: XAudio2 runs at %u Hz\n", m_nSampleRate);
        m_pobjects = NULL;
    }

    m_preverb = CreateReverbVoice(m_pxaudio2, m_nSampleRate, m_nChannels);

    m_pmaster->GetChannelMask(&m_dwChannelMask);

    if (m_dwChannelMask == 0) {
        m_dwChannelMask = m_nChannels == 1 ? SPEAKER_FRONT_CENTER : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
    }

    SetDistanceFactor(m_fDistanceFactor);

    debugf("XAudio2: %u output channels (mask %X) at %u Hz%s%s\n", m_nChannels, m_dwChannelMask, details.InputSampleRate,
        m_bHeadphones ? ", head model" : "", m_pobjects ? ", Windows Spatial Sound" : "");
    return S_OK;
}

void XA2Device::SetReverbLevel(float fLevel)
{
    if (m_preverb) {
        m_preverb->SetVolume(fLevel);
    }
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
    m_nOutChannels(1),
    m_bSpatial(false),
    m_nSlot(-1),
    m_bSlotTried(false),
    m_fFilter(-1),
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

    // and with the spatial sound slot: no effect writes to it any more
    if (m_nSlot >= 0 && m_pdevice && m_pdevice->m_pobjects) {
        m_pdevice->m_pobjects->ReleaseSlot(m_nSlot);
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

    //
    // The voice sends to the master and to the station reverb. A 3D sound
    // gets the distance low-pass, and on headphones the head model, which
    // turns its one or two channels into two.
    //

    XAUDIO2_SEND_DESCRIPTOR sends[2] = {
        { 0, pdevice->m_pmaster },
        { 0, pdevice->m_preverb }
    };
    XAUDIO2_VOICE_SENDS voiceSends = { pdevice->m_preverb ? 2u : 1u, sends };
    UINT32              flags      = bSupport3D ? XAUDIO2_VOICE_USEFILTER : 0;

    IUnknown* peffect = NULL;

    if (bSupport3D && (pdevice->m_bHeadphones || pdevice->m_pobjects)
        && FAILED(CreateSpatialEffect(&peffect, pdevice->m_pobjects))
    ) {
        peffect = NULL;
    }

    XAUDIO2_EFFECT_DESCRIPTOR effect = { peffect, TRUE, 2 };
    XAUDIO2_EFFECT_CHAIN      chain  = { 1, &effect };

    HRESULT hr = pdevice->m_pxaudio2->CreateSourceVoice(&m_pvoice, &format, flags, c_fMaxFrequencyRatio,
        NULL, &voiceSends, peffect ? &chain : NULL);

    if (FAILED(hr) && peffect) {
        debugf("CreateSourceVoice with the head model failed: %X\n", hr);
        hr = pdevice->m_pxaudio2->CreateSourceVoice(&m_pvoice, &format, flags, c_fMaxFrequencyRatio,
            NULL, &voiceSends, NULL);
        peffect->Release();
        peffect = NULL;
    }

    if (FAILED(hr)) {
        debugf("CreateSourceVoice failed: %X\n", hr);
        m_pvoice = NULL;
        return hr;
    }

    // XAudio2 holds the effect now
    if (peffect) {
        peffect->Release();
    }

    m_bSpatial     = peffect != NULL;
    m_nOutChannels = m_bSpatial ? 2 : m_nChannels;

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
// A sound's direction in the listener's frame (x right, y up, z front), from
// X3DAudio's left-handed vectors
//

static HeadDirection ListenerDirection(const PanListener& listener, const PanVector& position)
{
    PanVector d     = { position.x - listener.m_position.x, position.y - listener.m_position.y, position.z - listener.m_position.z };
    PanVector front = listener.m_front;
    PanVector top   = listener.m_top;

    // right = top x front, in X3DAudio's left-handed frame
    PanVector right = {
        top.y * front.z - top.z * front.y,
        top.z * front.x - top.x * front.z,
        top.x * front.y - top.y * front.x
    };

    return HeadDirection{
        d.x * right.x + d.y * right.y + d.z * right.z,
        d.x * top.x   + d.y * top.y   + d.z * top.z,
        d.x * front.x + d.y * front.y + d.z * front.z
    };
}

//
// The output: unpositioned, a mono sound goes to the front left and right at
// full level (DirectSound's centre pan) and a stereo one to its sides.
// Positioned, it is attenuated by DirectSound's distance and cone
// attenuation, low-passed with distance, and either rendered by the head
// model (headphones) or panned by X3DAudio (a stereo sound as one point,
// each side at half). Every sound is also sent to the station reverb, at its
// attenuated level; the reverb voice's volume decides how much is heard.
//

void XA2Voice::ApplyOutput()
{
    XA2Device* pdevice = m_pdevice;
    UINT32     nOut    = pdevice->m_nChannels;
    UINT32     nIn     = m_nOutChannels;
    float      matrix[2 * XAUDIO2_MAX_AUDIO_CHANNELS] = {};
    float      fSend   = 1.0f;
    float      fCutoff = 0;     // Hz; 0: no filtering

    if (nOut == 0 || nOut > XAUDIO2_MAX_AUDIO_CHANNELS || nIn == 0 || nIn > 2) {
        return;
    }

    if (!(m_bSupport3D && m_b3D)) {
        if (m_bSpatial) {
            // the effect passes it through: mono to both ears, stereo as it is
            SpatialEffectParameters params = { 0, 0, 1, FALSE, -1, 0 };
            m_pvoice->SetEffectParameters(0, &params, sizeof(params));
        }

        if (nOut == 1) {
            matrix[0] = nIn == 1 ? 1.0f : 0.5f;
            matrix[1] = 0.5f;
        } else if (nIn == 1) {
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
        PanVector          position = ConvertVector(m_vectPosition);

        float pan[XAUDIO2_MAX_AUDIO_CHANNELS] = {};
        float fDoppler = X3DPan(pdevice->m_x3daudio, listener,
            position, ConvertVector(m_vectVelocity),
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

        //
        // Windows Spatial Sound: the sound gets an audio object the first
        // time it is positioned, if Windows has one to give; it keeps it
        // while it plays. One that is denied, or a renderer that has
        // stopped, plays the other ways.
        //

        SpatialObjects* pobjects = pdevice->m_pobjects;
        bool            bObject  = false;

        if (m_bSpatial && pobjects && !pobjects->IsFailed()) {
            if (!m_bSlotTried) {
                m_bSlotTried = true;
                m_nSlot      = pobjects->AcquireSlot();
            }

            bObject = m_nSlot >= 0 && !pobjects->IsSlotDenied(m_nSlot);
        }

        HeadDirection direction = ListenerDirection(listener, position);

        if (bObject) {
            //
            // Windows renders it: the effect hands it over at the voice's
            // volume and attenuation, and the master gets nothing from the
            // voice (the matrix stays zero); the reverb send stays
            //

            SpatialEffectParameters params = {
                direction.x, direction.y, direction.z, FALSE,
                m_nSlot, DbToAmplitude(m_fGain) * fAttenuation
            };
            m_pvoice->SetEffectParameters(0, &params, sizeof(params));
        } else if (m_bSpatial && pdevice->m_bHeadphones) {
            //
            // Headphones: the head model gives each ear its sound; the matrix
            // only attenuates (the output is stereo)
            //

            SpatialEffectParameters params = { direction.x, direction.y, direction.z, TRUE, -1, 0 };
            m_pvoice->SetEffectParameters(0, &params, sizeof(params));

            matrix[0 * 2 + 0] = fAttenuation;
            matrix[1 * 2 + 1] = fAttenuation;
        } else {
            if (m_bSpatial) {
                // the effect passes it through, two equal channels for the pan
                SpatialEffectParameters params = { 0, 0, 1, FALSE, -1, 0 };
                m_pvoice->SetEffectParameters(0, &params, sizeof(params));
            }

            for (UINT32 out = 0; out < nOut; out++) {
                if (nIn == 1) {
                    matrix[out] = pan[out] * fAttenuation;
                } else {
                    matrix[out * 2 + 0] = pan[out] * fAttenuation * 0.5f;
                    matrix[out * 2 + 1] = pan[out] * fAttenuation * 0.5f;
                }
            }
        }

        fSend   = fAttenuation;
        fCutoff = DistanceCutoff(fDistance, m_fMinimumDistance);

        if (m_fDoppler != fDoppler) {
            m_fDoppler = fDoppler;
            ApplyFrequency();
        }
    }

    m_pvoice->SetOutputMatrix(pdevice->m_pmaster, nIn, nOut, matrix);

    if (pdevice->m_preverb) {
        float send[2] = { fSend / nIn, fSend / nIn };
        m_pvoice->SetOutputMatrix(pdevice->m_preverb, nIn, 1, send);
    }

    //
    // The distance low-pass (a 3D voice has the filter); set when it moves
    // by more than a little, as it changes every frame
    //

    if (m_bSupport3D) {
        // XAudio2CutoffFrequencyToRadians, which xaudio2.h defines only with XAUDIO2_HELPER_FUNCTIONS
        float fFilter = fCutoff > 0 && fCutoff * 6.0f < pdevice->m_nSampleRate
            ? 2.0f * std::sin(3.14159265f * fCutoff / pdevice->m_nSampleRate)
            : (float)XAUDIO2_MAX_FILTER_FREQUENCY;

        if (m_fFilter < 0 || std::fabs(fFilter - m_fFilter) > 0.002f) {
            m_fFilter = fFilter;

            XAUDIO2_FILTER_PARAMETERS filter = { LowPassFilter, fFilter, c_fFilterOneOverQ };
            m_pvoice->SetFilterParameters(&filter);
        }
    }
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
