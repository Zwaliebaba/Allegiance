//
// Xa2Engine.cpp
//
// The XAudio2 implementation of the sound engine. See MODERNIZATION.md,
// "Sound: XAudio2 replaces DirectSound".
//

#include "pch.h"
#include "SoundBase.h"

#include "SoundMath.h"
#include "X3DPan.h"
#include "Xa2Voice.h"
#include "VirtualBuffer.h"
#include "Xa2Engine.h"
#include "SoundUtil.h"

namespace SoundEngine {

/////////////////////////////////////////////////////////////////////////////
//
// DefaultListener
//
/////////////////////////////////////////////////////////////////////////////

// a listener who simply sits at the origin
class DefaultListener : public ISoundListener
{
public:

    //
    // ISoundPositionSource
    //

    // Gets the position of the sound in space
    HRESULT GetPosition(Vector& vectPosition)
    {
        vectPosition = Vector(0, 0, 0);
        return S_OK;
    };

    // Gets the velocity of the sound in space
    HRESULT GetVelocity(Vector& vectVelocity)
    {
        vectVelocity = Vector(0, 0, 0);
        return S_OK;
    };

    // Gets the orientation of the sound in space, used for sound cones.
    virtual HRESULT GetOrientation(Vector& vectOrientation)
    {
        vectOrientation = Vector(0, 0, 1);
        return S_OK;
    };

    // Returns S_OK if the position, velocity and orientation reported are 
    // relative to the listener, S_FALSE otherwise.  
    virtual HRESULT IsListenerRelative()
    {
        return S_FALSE; // it would be useless if it was "listener relative"
    };

    // Returns S_OK if this source is still playing the sound, S_FALSE 
    // otherwise.  This might be false if a sound emitter is destroyed, for 
    // example, in which case the sound might fade out.  Once it returns
    // S_FALSE once, it should never return S_OK again.
    virtual HRESULT IsPlaying()
    {
        return S_OK;
    };
    
    //
    // ISoundListener 
    //

    // get the "up" vector for the listener
    virtual HRESULT GetUpDirection(Vector& vectUp)
    {
        vectUp = Vector(0, 1, 0);
        return S_OK;
    }
};



/////////////////////////////////////////////////////////////////////////////
//
// XA2SoundEngine
//
/////////////////////////////////////////////////////////////////////////////

// gives the device the listener's position, velocity and orientation
HRESULT XA2SoundEngine::UpdateListener()
{
    HRESULT hr;
    Vector  vectPosition, vectVelocity, vectOrientation, vectUp;

    hr = m_plistener->GetPosition(vectPosition);
    if (ZFailed(hr)) return hr;

    hr = m_plistener->GetVelocity(vectVelocity);
    if (ZFailed(hr)) return hr;

    hr = m_plistener->GetOrientation(vectOrientation);
    if (ZFailed(hr)) return hr;
    ZAssertIsUnitVector(vectOrientation);

    hr = m_plistener->GetUpDirection(vectUp);
    if (ZFailed(hr)) return hr;
    ZAssertIsUnitVector(vectUp);

    PanListener& listener = m_pdevice->m_listener;
    listener.m_position = ConvertVector(vectPosition);
    listener.m_velocity = ConvertVector(vectVelocity);
    listener.m_front    = ConvertVector(vectOrientation);
    listener.m_top      = ConvertVector(vectUp);

    m_pdevice->m_vectListenerPosition = vectPosition;

    m_pdevice->m_fRolloffFactor = m_fRolloffFactor;
    m_pdevice->m_fDopplerFactor = m_fDopplerFactor;

    return S_OK;
};


// Constructor - the real initialization is done in init.
XA2SoundEngine::XA2SoundEngine() :
    m_quality(midQuality),
    m_fRolloffFactor(1.0f),
    m_fDopplerFactor(1.0f),
    m_fDistanceFactor(1.0f),
    m_fReverb(0.0f),
    m_fReverbTarget(0.0f),
    m_nNumBuffersDesired(16),
    m_nNumBuffersMax(24)
{
    m_plistener = new DefaultListener();
    m_peventsourceUpdate = new IntegerEventSourceImpl();
    m_dwLastUpdateTime = timeGetTime(); 
    m_pBufferSourceDelegate = new SoundBufferSourceDelegate(this);
}


XA2SoundEngine::~XA2SoundEngine()
{
    m_peventsourceUpdate = NULL;

    // the voices go with the sounds; the device with the last of them
    m_listActiveSounds.clear();
    m_pdevice = NULL;
}


// Basic initialization.  This was pulled out of the constructor so that we
// can return error values.
HRESULT XA2SoundEngine::Init()
{
    TRef<XA2Device> pdevice = new XA2Device();

    HRESULT hr = pdevice->Init();
    if (FAILED(hr)) return hr;

    m_pdevice = pdevice;
    m_pdevice->SetDistanceFactor(m_fDistanceFactor);

    return SetQuality(midQuality);
}


// Rebuild the sound stage to reflect any recent changes in sound
HRESULT XA2SoundEngine::Update()
{
    HRESULT hr;
    Vector vectListenerPosition;
    DWORD dwUpdateTime = timeGetTime();
    DWORD dwUpdatePeriod = dwUpdateTime - m_dwLastUpdateTime; 

    // fire the update event.
    m_peventsourceUpdate->Trigger(dwUpdatePeriod);

    typedef std::vector<VirtualSoundBuffer*> TempSoundList;

    // get the current position of the listener, to use for future 
    // calculations, and give the voices the listener before they update
    hr = m_plistener->GetPosition(vectListenerPosition);
    if (ZFailed(hr)) return hr;

    hr = UpdateListener();
    if (ZFailed(hr)) return hr;

    
    // Walk the list of active sounds and call update for each one, 
    // removing the sounds which have ended, keeping track of which sounds
    // have playing buffers
    
    TempSoundList listPrevSounds;
    listPrevSounds.reserve(m_nNumBuffersMax);

    VirtualSoundList::iterator iterSound;
    for (iterSound = m_listActiveSounds.begin();
        iterSound != m_listActiveSounds.end();)
    {
        hr = (*iterSound)->Update(dwUpdatePeriod, vectListenerPosition, 
            m_fRolloffFactor);
        if (ZFailed(hr)) return hr;

        // if this is no longer playing, remove it from the list.
        if ((*iterSound)->IsStopped())
        {
            iterSound = m_listActiveSounds.erase(iterSound);
        }
        else
        {
            // if the sound has an active buffer, keep track of it.
            if ((*iterSound)->HasPlayingBuffer())
                listPrevSounds.push_back(
                    (TRef<VirtualSoundBuffer>)(*iterSound)
                    );
            ++iterSound;
        }
    }


    // get the n most important sounds that we wish to play
    TempSoundList listNewSounds;
    if (!m_listActiveSounds.empty())
    {
        // create a list of all of the sounds
        TempSoundList listAllSounds(m_listActiveSounds.size());
        std::transform(m_listActiveSounds.begin(), m_listActiveSounds.end(), 
            listAllSounds.begin(), RefToPtr<VirtualSoundBuffer>());

        // turn it into a priority queue
        std::make_heap(listAllSounds.begin(), listAllSounds.end(), SoundPriorityCompareReversed());

        // get the m_nNumBuffersDesired most important sounds by popping 
        // them off of the queue
        listNewSounds.reserve(m_nNumBuffersDesired);
        for (int nSound = 0; nSound < min(m_nNumBuffersDesired, listAllSounds.size()); nSound++)
        {
            std::pop_heap(listAllSounds.begin(), listAllSounds.end() - nSound, 
                SoundPriorityCompareReversed());

            VirtualSoundBuffer* pbuffer = (*(listAllSounds.end() - nSound - 1));
            if (pbuffer->IsAudible())
                listNewSounds.push_back(pbuffer);
        }
    }

   
    // Compare the previously playing sounds and the list of new sounds we
    // want to play, and start and stop the sound buffers as needed to 
    // achieve this.
    {
        // sort the old list by priority
        std::sort(listPrevSounds.begin(), listPrevSounds.end(), SoundPriorityCompare());

        // find the first sound which is not in the new list
        TempSoundList::iterator iterDead = 
            (listNewSounds.empty()) ? listPrevSounds.begin() :
            std::find_if(listPrevSounds.begin(), listPrevSounds.end(), 
                [plast = *(listNewSounds.end() - 1)](VirtualSoundBuffer* psound) { return SoundPriorityCompare()(plast, psound); });

        // stop all of the old buffers 
        // REVIEW: should fade out, if we have buffers.
        for (; iterDead != listPrevSounds.end(); iterDead++)
        {
            hr = (*iterDead)->StopBuffer();
            if (ZFailed(hr)) return hr;
        }

        // Start any new sounds            
        for (TempSoundList::iterator iterNew = listNewSounds.begin();
            iterNew != listNewSounds.end(); ++iterNew)
        {
            if (!(*iterNew)->HasPlayingBuffer())
            {
                hr = (*iterNew)->StartBuffer(m_pdevice);
                if (FAILED(hr))
                {
                    // Silently fail
                    debugf("Error during update while starting sound: %X\n", hr);
                }
            }
        };
    }

    //
    // The room reverb fades in and out over about a second
    //

    if (m_fReverb != m_fReverbTarget && m_pdevice)
    {
        float fStep = dwUpdatePeriod / 1000.0f;

        m_fReverb = m_fReverb < m_fReverbTarget
            ? min(m_fReverb + fStep, m_fReverbTarget)
            : max(m_fReverb - fStep, m_fReverbTarget);

        m_pdevice->SetReverbLevel(m_fReverb);
    }

    m_dwLastUpdateTime = dwUpdateTime;

    return S_OK;
}


// Gets the number of virtual sound buffers that are playing at a given 
// moment.  (no guarantees on how this number changes - it's just a perf.
// number to use.)
HRESULT XA2SoundEngine::GetNumPlayingVirtualBuffers(int& nBuffers)
{
    nBuffers = m_listActiveSounds.size();

    return S_OK;
}


// Sets a general quality of playback (CPU time vs. fidelity): how many sounds
// play at once, as DirectSound's software mixing had it. (The primary buffer
// formats and the 3D algorithms of the DirectSound engine have no
// counterpart: XAudio2 mixes at the device's format.)
HRESULT XA2SoundEngine::SetQuality(Quality quality)
{
    HRESULT hr;

    // stop all playing voices; the next update starts the ones that fit
    VirtualSoundList::iterator iterSound;
    for (iterSound = m_listActiveSounds.begin();
        iterSound != m_listActiveSounds.end(); ++iterSound)
    {
        if ((*iterSound)->HasPlayingBuffer())
        {
            hr = (*iterSound)->StopBuffer();
            if (ZFailed(hr)) return hr;
        }
    }
    
    switch (quality)
    {
    case minQuality:
        m_nNumBuffersDesired = 8;
        m_nNumBuffersMax = 8;
        break;

    case midQuality:
    default:
        m_nNumBuffersDesired = 16;
        m_nNumBuffersMax = 24;
        break;

    case maxQuality:
        m_nNumBuffersDesired = 24;
        m_nNumBuffersMax = 32;
        break;
    }

    m_quality = quality;

    return S_OK;
};


// Sets the listener to use for the current sounds
HRESULT XA2SoundEngine::SetListener(ISoundListener* plistener)
{
    if (plistener == NULL)
    {
        ZAssert(false);
        return E_POINTER;
    }

    m_plistener = plistener;

    return S_OK;
}


// Sets the conversion from game units to meters
HRESULT XA2SoundEngine::SetDistanceFactor(float fMetersPerUnit)
{
    if (fMetersPerUnit <= 0)
    {
        ZAssert(false);
        return E_INVALIDARG;
    };

    m_fDistanceFactor = fMetersPerUnit;

    if (m_pdevice)
        m_pdevice->SetDistanceFactor(fMetersPerUnit);

    return S_OK;
}


// Sets the rolloff factor, where 1.0 is the real world attenuation with 
// distance, 2.0 is twice the attenuation of the real world, etc..
HRESULT XA2SoundEngine::SetRolloffFactor(float fRolloffFactor)
{
    // DirectSound's range (DS3D_MINROLLOFFFACTOR, DS3D_MAXROLLOFFFACTOR)
    if (fRolloffFactor < 0.0f || fRolloffFactor > 10.0f)
    {
        ZAssert(false);
        return E_INVALIDARG;
    };

    m_fRolloffFactor = fRolloffFactor;
    return S_OK;
}


// Sets the doppler factor, where 1.0 is real-world doppler
HRESULT XA2SoundEngine::SetDopplerFactor(float fDopplerFactor)
{
    // DirectSound's range (DS3D_MINDOPPLERFACTOR, DS3D_MAXDOPPLERFACTOR)
    if (fDopplerFactor < 0.0f || fDopplerFactor > 10.0f)
    {
        ZAssert(false);
        return E_INVALIDARG;
    };

    m_fDopplerFactor = fDopplerFactor;
    return S_OK;
}


// Sets how much of the room reverb is heard, 0 to 1
HRESULT XA2SoundEngine::SetReverb(float fLevel)
{
    if (fLevel < 0.0f || fLevel > 1.0f)
    {
        ZAssert(false);
        return E_INVALIDARG;
    }

    m_fReverbTarget = fLevel;
    return S_OK;
}


// Gets a buffer source for this object (not guarenteed to keep the sound 
// engine alive due to circular reference problems)
ISoundBufferSource* XA2SoundEngine::GetBufferSource()
{
    return m_pBufferSourceDelegate;
};


// Creates a static sound buffer of the given wave file.  If bLooping is 
// true, the sound will loop until stopped.
HRESULT XA2SoundEngine::CreateStaticBuffer(TRef<ISoundInstance>& psoundNew, 
    ISoundPCMData* pcmdata, bool bLooping, ISoundPositionSource* psource)
{
    TRef<VirtualSoundBuffer> pvirtualsound;

    if (psource != NULL)
    {
        pvirtualsound = new VirtualSoundBuffer3D(pcmdata, bLooping, psource);
    }
    else
    {
        pvirtualsound = new VirtualSoundBuffer(pcmdata, bLooping);
    }

    m_listActiveSounds.push_back(pvirtualsound);
    psoundNew = pvirtualsound;

    return S_OK;
}


// Creates a sound buffer with a loop in the middle.  The sound will play
// the start sound, play the loop sound until it gets a soft stop, then
// play the rest of the sound.  
HRESULT XA2SoundEngine::CreateASRBuffer(TRef<ISoundInstance>& psoundNew, 
    ISoundPCMData* pcmdata, unsigned uLoopStart, unsigned uLoopLength, 
    ISoundPositionSource* psource)
{
    // check the parameters
    if (pcmdata == NULL)
    {
        ZAssert(false);
        return E_POINTER;
    }

    // make sure the loop is contained in the buffer
    if (uLoopStart + uLoopLength > pcmdata->GetSize())
    {
        ZAssert(false);
        return E_INVALIDARG;
    }

    TRef<VirtualSoundBuffer> pvirtualsound;

    if (psource != NULL)
    {
        pvirtualsound = new VirtualSoundBuffer3D(pcmdata, uLoopStart, uLoopLength, psource);
    }
    else
    {
        pvirtualsound = new VirtualSoundBuffer(pcmdata, uLoopStart, uLoopLength);
    }

    m_listActiveSounds.push_back(pvirtualsound);
    psoundNew = pvirtualsound;

    return S_OK;
}


// Gets an event which fires each time update is called.  This can be used
// for some of the trickier sounds that change with time.
IIntegerEventSource* XA2SoundEngine::GetUpdateEventSource()
{
    return m_peventsourceUpdate;
};


#ifdef _DEBUG
// return a human-readable description of the object, prepending
// strIndent to the beginning of each line.  
ZString XA2SoundEngine::DebugDump(const ZString& strIndent)
{
    ZString strResult = strIndent + "XA2SoundEngine: \n";

    // Walk the list of active sounds and dump each one
    VirtualSoundList::iterator iterSound;
    for (iterSound = m_listActiveSounds.begin();
        iterSound != m_listActiveSounds.end(); ++iterSound)
    {
        strResult += SoundDebugDump(
            (ISoundInstance*)(TRef<VirtualSoundBuffer>&)(*iterSound), 
            strIndent + "  ");
    }

    return strResult;
}
#endif

};
