//
// VirtualBuffer.cpp
//
// Classes representing virtual sounds that may or may not have a real
// XAudio2 voice associated with them at any given moment.
//

#include "pch.h"
#include "SoundBase.h"

#include "SoundMath.h"
#include "X3DPan.h"
#include "Xa2Voice.h"
#include "VirtualBuffer.h"

namespace SoundEngine {


/////////////////////////////////////////////////////////////////////////////
//
// VirtualSoundBuffer
//
/////////////////////////////////////////////////////////////////////////////


// prepare a voice with the given 3D support
HRESULT VirtualSoundBuffer::PrepareBuffer(XA2Device* pdevice, bool bSupport3D)
{
    HRESULT hr;

    if (m_pvoice == nullptr)
    {
        TRef<XA2Voice> pvoice = new XA2Voice();

        hr = pvoice->Init(pdevice, m_pdata, 
            !m_bufferPositionTracker.IsASR() && m_bufferPositionTracker.IsLooping(),
            m_bufferPositionTracker.IsASR(),
            m_bufferPositionTracker.GetLoopOffset(), 
            m_bufferPositionTracker.GetLoopLength(),
            bSupport3D);
        if (FAILED(hr)) return hr;

        m_pvoice = pvoice;
    }

    // update the voice's volume and pitch as needed.
    hr = m_pvoice->UpdateState(m_fGain, m_fPitch);
    if (ZFailed(hr)) return hr;

    return S_OK;
}


// Creates a virtual sound buffer for a normal sound, possibly looping
VirtualSoundBuffer::VirtualSoundBuffer(ISoundPCMData* pdata, bool bLooping) : 
    m_bufferPositionTracker(pdata->GetSize(), pdata->GetBytesPerSec(), pdata->GetBytesPerSample(), bLooping),
    m_pdata(pdata),

    m_bStarted(false),
    m_bStopped(false),
    m_bRetryStop(false),
    m_bBufferPlaying(false),
    m_bAudible(false),
    m_fAge(0),
    m_fPriority(1.0f),
    m_fGain(0.0f),
    m_fPitch(1.0f),
    m_fDynamicPriority(0.0f)
{
};


// Creates a virtual sound buffer for an ASR sound
VirtualSoundBuffer::VirtualSoundBuffer(ISoundPCMData* pdata, DWORD dwLoopOffset, DWORD dwLoopLength) : 
    m_bufferPositionTracker(pdata->GetSize(), pdata->GetBytesPerSec(), pdata->GetBytesPerSample(), dwLoopOffset, dwLoopLength),
    m_pdata(pdata),

    m_bStarted(false),
    m_bStopped(false),
    m_bRetryStop(false),
    m_bBufferPlaying(false),
    m_bAudible(false),
    m_fAge(0),
    m_fPriority(1.0f),
    m_fGain(0.0f),
    m_fPitch(1.0f),
    m_fDynamicPriority(0.0f)
{
};


// Recalculates the sounds position, loudness, whether it's playing, etc..
HRESULT VirtualSoundBuffer::Update(DWORD dwTimeElapsed, 
        const Vector& vectListenerPosition, float fRolloffFactor)
{
    HRESULT hr;
    bool bWasStopped = m_bStopped;

    // if we tried to stop the sound and failed, try again now
    if (m_bRetryStop && m_bBufferPlaying)
        m_pvoice->Stop(true);
    
    // Start the sound and calculate the end time, if we have not done so 
    // yet.
    if (!m_bStarted)
    {
        m_bStarted = true;
    }
    else
    {
        // update the play position
        m_bufferPositionTracker.AddTime((DWORD)(dwTimeElapsed * m_fPitch));
        m_fAge += dwTimeElapsed/1000.0f;
    }

    // if the voice is playing, it knows if we are still playing (voices,
    // unlike DirectSound buffers, are never lost)
    if (m_bBufferPlaying)
    {
        m_bStopped = !m_pvoice->IsPlaying();
    }

    // if we don't have a buffer playing, try to calculate if this sound 
    // would still be playing if it were audible.
    if (!m_bBufferPlaying)
    {
        m_bStopped = m_bufferPositionTracker.IsStopped();
    }
    else
    {
        // update the playing sound's volume and pitch, as needed.
        hr = m_pvoice->UpdateState(m_fGain, m_fPitch);
        if (ZFailed(hr)) return hr;
    }

    // if this has just stopped
    if (m_bStopped && !bWasStopped)
    {
        // free the voice to conserve resources
        m_pvoice = nullptr;
        m_bBufferPlaying = FALSE;

        // if there is an event source for the sound finishing, trigger it.
        if (m_peventsourceFinished != nullptr)
        {
            m_peventsourceFinished->Trigger();
        }
    }

    // set this sound to be audible if it is playing and has an attenuation
    // of less than 90dB.
    m_bAudible = !m_bStopped && (m_fGain > c_fMinAudible);

    // set the dynamic priority to be the aproximate volume plus the static
    // priority, with a 0.1 dB priority boost for physically playing sounds
    // and up to a 0.2 dB boost for sounds less than 2 seconds old.  This 
    // should help insure that, all other things being equal, a playing 
    // sound will continue playing and a younger sound will replace an 
    // identical older sound.  
    //
    // Repeating weapon sounds is an example of where this behavior is most
    // important.
    m_fDynamicPriority = m_fGain + m_fPriority
        + (m_bBufferPlaying ? 0.1f : 0)
        + max(0.0f, 1.0f * (2 - m_fAge));

    return S_OK;
};


// Creates and starts a voice for this sound
HRESULT VirtualSoundBuffer::StartBuffer(XA2Device* pdevice)
{
    HRESULT hr;

    // if we have a voice playing, we have nothing more to do
    if (m_bBufferPlaying)
    {
        ZAssert(false);  // I want to know about this
        return S_FALSE;
    }

    // A voice starts once, so a sound that is given a voice again gets a new
    // one, at the position the tracker has reached
    m_pvoice = nullptr;

    hr = PrepareBuffer(pdevice, false);
    if (FAILED(hr)) return hr;

    // start the sound.
    hr = m_pvoice->Start(m_bufferPositionTracker.GetPosition(), 
        m_bufferPositionTracker.IsStopping());
    if (FAILED(hr)) return hr;

    m_bBufferPlaying = true;

    return S_OK;
};


// forcibly stops the voice, and frees it. The tracker keeps the position
// (DirectSound's buffers reported theirs; the tracker's own differs only by
// the timing of the updates).
HRESULT VirtualSoundBuffer::StopBuffer()
{
    HRESULT hr;

    // if we don't have a voice playing, we have nothing to do
    if (!m_bBufferPlaying)
    {
        ZAssert(false);  // I want to know about this
        return S_FALSE;
    }
    
    hr = m_pvoice->Stop(true);
    if (ZFailed(hr)) return hr;

    m_pvoice = nullptr;
    m_bBufferPlaying = false;

    return S_OK;
}


// Sets the gain, from 0 to -100 dB
HRESULT VirtualSoundBuffer::SetGain(float fGain)
{
    if (fGain > 0)
    {
        ZAssert(false);
        return E_INVALIDARG;
    }

    m_fGain = max(fGain, -100.0f);

    return S_OK;
};


// Sets the pitch shift, where 1.0 is normal, 0.5 is half of normal speed, 
// and 2.0 is twice normal speed.  
HRESULT VirtualSoundBuffer::SetPitch(float fPitch)
{
    fPitch = max(fPitch, c_fMinFrequency / m_pdata->GetSampleRate());
    fPitch = min(fPitch, c_fMaxFrequency / m_pdata->GetSampleRate());

    m_fPitch = fPitch;

    return S_OK;
};

// sets the priority - used as a addition to volume when choosing which 
// sounds are most important to play.
HRESULT VirtualSoundBuffer::SetPriority(float fPriority) 
{
    m_fPriority = fPriority;
    return S_OK;
};


// Stops the sound.  If bForceNow is true the sound will stop ASAP, 
// possibly popping.  If it is false some sounds may play a trail-off 
// sound or fade away.  
HRESULT VirtualSoundBuffer::Stop(bool bForceNow)
{
    HRESULT hr;

    if (m_bBufferPlaying)
    {
        // REVIEW: fade out at higher quality settings?
        hr = m_pvoice->Stop(bForceNow);
        if (ZFailed(hr)) 
        {
            m_bRetryStop = true;
            return hr;
        }
    }

    m_bufferPositionTracker.Stop(bForceNow);

    return S_OK;
};


// returns S_OK if the sound is currently playing, S_FALSE otherwise.
HRESULT VirtualSoundBuffer::IsPlaying()
{
    return m_bStopped ? S_FALSE : S_OK;
};


// Gets an event which fires when the sound finishes playing (for any 
// reason)
IEventSource* VirtualSoundBuffer::GetFinishEventSource()
{
    // if we don't have an event source, create one.
    if (m_peventsourceFinished == nullptr)
    {
        m_peventsourceFinished = new EventSourceImpl();
    }

    return m_peventsourceFinished;
};


// Gets an interface for tweaking the sound, if supported, NULL otherwise.
TRef<ISoundTweakable> VirtualSoundBuffer::GetISoundTweakable()
{
    return this;
};


// Gets an interface for tweaking the sound, if supported, NULL otherwise.
TRef<ISoundTweakable3D> VirtualSoundBuffer::GetISoundTweakable3D()
{
    return nullptr;
};


#ifdef _DEBUG
// return a human-readable description of the object, prepending
// strIndent to the beginning of each line.  
ZString VirtualSoundBuffer::DebugDump(const ZString& strIndent)
{
    return strIndent + "VirtualSoundBuffer: Priority " + StrFromFloat(m_fDynamicPriority)
        + ", Age " + StrFromFloat(m_fAge) 
        + ", gain " + StrFromFloat(m_fGain) 
        + ", pitch " + StrFromFloat(m_fPitch) + "\n"
        + SoundDebugDump(m_pdata, strIndent + "  ")
        + ((m_pvoice) ? SoundDebugDump(m_pvoice, strIndent + "  ") : ZString());
}
#endif



/////////////////////////////////////////////////////////////////////////////
//
// VirtualSoundBuffer3D
//
/////////////////////////////////////////////////////////////////////////////

// constructs a simple 3D capable sound
VirtualSoundBuffer3D::VirtualSoundBuffer3D(ISoundPCMData* pdata, bool bLooping, 
        TRef<ISoundPositionSource> psource) :
    VirtualSoundBuffer(pdata, bLooping),
    m_pposSource(psource),

    m_b3D(false),
    m_bListenerRelative(false),
    m_bPlayingIn3D(true),
    m_fMinimumDistance(20.0f),
    m_fInnerAngle(360),
    m_fOuterAngle(360),
    m_fOutsideGain(0.0f),
    m_vectPosition(0,0,0),
    m_vectVelocity(0,0,0),
    m_vectOrientation(0,0,1)
{
};


// constructs an ASR 3D capable sound
VirtualSoundBuffer3D::VirtualSoundBuffer3D(ISoundPCMData* pdata, DWORD dwLoopOffset, 
        DWORD dwLoopLength, TRef<ISoundPositionSource> psource) :
    VirtualSoundBuffer(pdata, dwLoopOffset, dwLoopLength),
    m_pposSource(psource),

    m_b3D(false),
    m_bListenerRelative(false),
    m_bPlayingIn3D(true),
    m_fMinimumDistance(20.0f),
    m_fInnerAngle(360),
    m_fOuterAngle(360),
    m_fOutsideGain(0.0f),
    m_vectPosition(0,0,0),
    m_vectVelocity(0,0,0),
    m_vectOrientation(0,0,1)
{
};


// prepare a voice with 3D support
HRESULT VirtualSoundBuffer3D::PrepareBuffer(XA2Device* pdevice, bool bSupport3D)
{
    HRESULT hr;

    // do all of the 2D voice creation stuff.
    hr = VirtualSoundBuffer::PrepareBuffer(pdevice, true);
    if (FAILED(hr)) return hr;

    // update the voice's 3D state
    hr = m_pvoice->UpdateState3D(
        m_vectPosition, m_vectVelocity, m_vectOrientation, 
        m_fMinimumDistance, 
        m_fInnerAngle, m_fOuterAngle, m_fOutsideGain,
        m_bPlayingIn3D,
        m_bListenerRelative
        );
    if (ZFailed(hr)) return hr;

    return S_OK;
};


// Recalculates the sounds position, loudness, whether it's playing, etc..
HRESULT VirtualSoundBuffer3D::Update(DWORD dwTimeElapsed, 
        const Vector& vectListenerPosition, float fRolloffFactor)
{
    HRESULT hr;

    // if the source no longer exists, stop the sound.
    hr = m_pposSource->IsPlaying();
    if (ZFailed(hr)) return hr;
    bool bSourceIsValid = hr == S_OK;

    if (!bSourceIsValid)
    {
        Stop(true);
    }

    // most of what VirtualSoundBuffer does is perfectly fine
    hr = VirtualSoundBuffer::Update(dwTimeElapsed, vectListenerPosition,
        fRolloffFactor);
    if (ZFailed(hr)) return hr;

    if (bSourceIsValid)
    {
        // get the position of the source.
        hr = m_pposSource->GetPosition(m_vectPosition);
        if (ZFailed(hr)) return hr;

        // find out whether the position is listener relative
        hr = m_pposSource->IsListenerRelative();
        if (ZFailed(hr)) return hr;
        m_bListenerRelative = hr == S_OK;

        // optimization: don't do 3D processing if the source position is  
        // identical to the listener's position.
        m_bPlayingIn3D = m_b3D 
            && m_vectPosition 
                != (m_bListenerRelative ? Vector(0,0,0) : vectListenerPosition);

        if (m_bPlayingIn3D)
        {
            // get the velocity of the source
            hr = m_pposSource->GetVelocity(m_vectVelocity);
            if (ZFailed(hr)) return hr;

            // get the position of the listener relative to this source
            Vector vectRelListener = m_bListenerRelative 
                ? -m_vectPosition : (vectListenerPosition - m_vectPosition);

            // get the distance to the listener
            float fDistanceSquared = vectRelListener.LengthSquared();
        
            // calculate some useful constants
            float fMinimumDistanceSquared = m_fMinimumDistance * m_fMinimumDistance;
            float fMaximumDistanceSquared = fMinimumDistanceSquared
                * c_fMinToMaxDistanceRatio * c_fMinToMaxDistanceRatio;
        
            // if we are outside of the maximum distance for this sound
            if (fDistanceSquared > fMaximumDistanceSquared || !m_bAudible)
            {
                // this sound is silent
                m_bAudible = false;
                m_fDynamicPriority += -100;
            }
            else
            {
                // calculate how much softer the sound is (in dB) based on distance
                float fDistanceGain = min(0.0f, (float)(
                    fRolloffFactor * -10 * log(fDistanceSquared/fMinimumDistanceSquared)
                    ));

                // if this has a sound cone
                float fConeGain = 0;
                if (m_fOutsideGain != 0.0f && m_fInnerAngle != 360
                    && !m_bListenerRelative)  // TODO: HACK: we don't have the info 
                                              // to do the right thing for listener
                                              // relative sounds yet.
                {
                    const float fRadiansToDegrees = 57.2957795f;

                    // get the orientation of the source
                    hr = m_pposSource->GetOrientation(m_vectOrientation);
                    if (ZFailed(hr)) return hr;
                    ZAssertIsUnitVector(m_vectOrientation);

                    // calculate the angle between the source and the listener
                    // (using the identity v1 * v2 = |v1||v2|cos(angle) )
                    float fPhi = (float)acos(m_vectOrientation * vectRelListener 
                        / sqrt(fDistanceSquared)) * fRadiansToDegrees;

                    // adjust the cone attenuation to reflect this.
                    if (fPhi > m_fInnerAngle)
                    {
                        if (fPhi < m_fOuterAngle)
                        {
                            // do a linear fade between the outside and inside 
                            // gains.
                            fConeGain = m_fOutsideGain 
                                * (fPhi - m_fInnerAngle)
                                / (m_fOuterAngle - m_fInnerAngle);
                        }
                        else
                        {
                            fConeGain = m_fOutsideGain;
                        }
                    }
                }

                float fTotalGain = m_fGain + fDistanceGain + fConeGain;

                // update the audibility
                m_bAudible = fTotalGain > c_fMinAudible;

                // add in the distance and cone attenuations to the priority
                m_fDynamicPriority += fDistanceGain + fConeGain;
            }
        }
    };
    
    // if we have a buffer...
    if (m_bBufferPlaying)
    {
        // update the playing sound's 3D state, as needed.
        hr = m_pvoice->UpdateState3D(
            m_vectPosition, m_vectVelocity, m_vectOrientation, 
            m_fMinimumDistance, 
            m_fInnerAngle, m_fOuterAngle, m_fOutsideGain,
            m_bPlayingIn3D,
            m_bListenerRelative
            );
        if (ZFailed(hr)) return hr;
    }

    return S_OK;
}


// toggles 3D Positioning on and off for the given sound.
HRESULT VirtualSoundBuffer3D::Set3D(bool b3D)
{
    m_b3D = b3D;

    return S_OK;
};


// Sets the distance at which the sound will be at max volume.  This
// effects how quickly the sound drops off with distance.  
HRESULT VirtualSoundBuffer3D::SetMinimumDistance(float fMinimumDistance)
{
    // Any distance greater than 0 is OK.
    if (fMinimumDistance <= 0)
    {
        ZAssert(false);
        return E_INVALIDARG;
    }

    m_fMinimumDistance = fMinimumDistance;
    
    return S_OK;
};


// Sets a sound cone of size fInnerAngle (in degrees) where the volume is at 
// normal levels, outside of which it fades down by fOutsideGain 
// (range of 0 to -100 db) at fOuterAngle (degrees) and beyond.  
HRESULT VirtualSoundBuffer3D::SetCone(float fInnerAngle, float fOuterAngle, float fOutsideGain)
{
    // check the parameters
    if ((fInnerAngle < 0 || fInnerAngle > 360)
        || (fOuterAngle < 0 || fOuterAngle > 360)
        || (fOuterAngle < fInnerAngle)
        || (fOutsideGain < -100 || fOutsideGain > 0)
        )
    {
        ZAssert(false);
        return E_INVALIDARG;
    };

    m_fInnerAngle = fInnerAngle;
    m_fOuterAngle = fOuterAngle;
    m_fOutsideGain = fOutsideGain;

    return S_OK;
};


// Sets the gain, from 0 to -100 dB
HRESULT VirtualSoundBuffer3D::SetGain(float fGain)
{
    return VirtualSoundBuffer::SetGain(fGain);
};


// Sets the pitch shift, where 1.0 is normal, 0.5 is half of normal speed, 
// and 2.0 is twice normal speed.  
HRESULT VirtualSoundBuffer3D::SetPitch(float fPitch)
{
    return VirtualSoundBuffer::SetPitch(fPitch);
};


// sets the priority - used as a addition to volume when choosing which 
// sounds are most important to play.
HRESULT VirtualSoundBuffer3D::SetPriority(float fPriority) 
{
    return VirtualSoundBuffer::SetPriority(fPriority);
};


// Gets an interface for tweaking the sound, if supported, NULL otherwise.
TRef<ISoundTweakable> VirtualSoundBuffer3D::GetISoundTweakable()
{
    return VirtualSoundBuffer::GetISoundTweakable();
};

// Gets an interface for tweaking the sound, if supported, NULL otherwise.
TRef<ISoundTweakable3D> VirtualSoundBuffer3D::GetISoundTweakable3D()
{
    return this;
};


};