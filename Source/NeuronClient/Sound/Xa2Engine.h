//
// xa2engine.h
//
// The XAudio2 implementation of the sound engine
//

namespace SoundEngine {

//
// A sound engine implementation using XAudio2 that handles basic resource
// management: of the sounds playing, the most important ones (by
// VirtualSoundBuffer::GetDynamicPriority) get a voice.
//
class XA2SoundEngine : public ISoundEngine, public ISoundBufferSource  SoundDebugImpl
{

    // XAudio2, its mastering voice and X3DAudio
    TRef<XA2Device> m_pdevice;

    // the listener used to render sound
    TRef<ISoundListener> m_plistener;

    // the rolloff factor, where 1.0f is the real-world rolloff
    float m_fRolloffFactor;

    // the doppler factor, where 1.0f is the real-world doppler
    float m_fDopplerFactor;

    // the distance factor, where 1.0f is the real-world distance factor
    float m_fDistanceFactor;

    // the room reverb's level, and the level it fades to
    float m_fReverb;
    float m_fReverbTarget;


    // the update event source, triggered on each call to update.
    TRef<IntegerEventSourceImpl> m_peventsourceUpdate;

    // a delegate buffer source for this (needed to avoid circular refs)
    TRef<ISoundBufferSource> m_pBufferSourceDelegate;

    // a list of currently playing virtual sounds
    typedef std::list<ZAdapt<TRef<VirtualSoundBuffer> > > VirtualSoundList;
    VirtualSoundList m_listActiveSounds;

    // the time of the last call to update
    DWORD m_dwLastUpdateTime;

    // the sound quality to use
    Quality m_quality;

    // the desired number of sounds to play at once.  This number may reserve 
    // a few voices for fading sounds out.
    int m_nNumBuffersDesired;

    // the maximum number of sounds we can play at once, including sounds that 
    // are fading in or out.
    int m_nNumBuffersMax;

    // gives the device the listener's position, velocity and orientation
    HRESULT UpdateListener();

    // provides a complete ordering of virtual sound buffers by priority.
    struct SoundPriorityCompare
    {
        bool operator () (VirtualSoundBuffer* psound1, VirtualSoundBuffer* psound2) const
        {
            return psound1->GetDynamicPriority() > psound2->GetDynamicPriority()
                || (psound1->GetDynamicPriority() == psound2->GetDynamicPriority()
                    && psound1 > psound2);
        }
    };

    // The opposite order (what std::not2 of the above gave; std::not2 is gone
    // in C++20)
    struct SoundPriorityCompareReversed
    {
        bool operator () (VirtualSoundBuffer* psound1, VirtualSoundBuffer* psound2) const
        {
            return !SoundPriorityCompare()(psound1, psound2);
        }
    };

    template <class T>
    struct RefToPtr
    {
        T* operator () (TRef<T>& rt)
        {
            return rt;
        }
    };

public:

    // Constructor - the real initialization is done in init.
    XA2SoundEngine();

    ~XA2SoundEngine();


    // Basic initialization.  This was pulled out of the constructor so that we
    // can return error values. Fails without an audio device.
    HRESULT Init();


    //
    // ISoundEngine Interface
    //

    // Rebuild the sound stage to reflect any recent changes in sound
    virtual HRESULT Update();

    // Gets a buffer source for this object (not guarenteed to keep the sound 
    // engine alive due to circular reference problems)
    virtual ISoundBufferSource* GetBufferSource();

    // Gets the number of virtual sound buffers that are playing at a given 
    // moment.  (no guarantees on how this number changes - it's just a perf.
    // number to use.)
    virtual HRESULT GetNumPlayingVirtualBuffers(int& nBuffers);


    // Sets a general quality of playback (CPU time vs. fidelity)
    HRESULT SetQuality(Quality quality);

    // Sets the listener to use for the current sounds
    virtual HRESULT SetListener(ISoundListener* plistener);

    // Sets the conversion from game units to meters
    virtual HRESULT SetDistanceFactor(float fMetersPerUnit);

    // Sets the rolloff factor, where 1.0 is the real world attenuation with 
    // distance, 2.0 is twice the attenuation of the real world, etc..
    virtual HRESULT SetRolloffFactor(float fRolloffFactor);

    // Sets the doppler factor, where 1.0 is real-world doppler
    virtual HRESULT SetDopplerFactor(float fDopplerFactor);

    // Sets how much of the room reverb is heard (fades to it)
    virtual HRESULT SetReverb(float fLevel);



    //
    // ISoundBufferSource
    //

    // Creates a static sound buffer of the given wave file.  If bLooping is 
    // true, the sound will loop until stopped.
    virtual HRESULT CreateStaticBuffer(TRef<ISoundInstance>& psoundNew, 
        ISoundPCMData* pcmdata, bool bLooping, ISoundPositionSource* psource = NULL);

    // Creates a sound buffer with a loop in the middle.  The sound will play
    // the start sound, play the loop sound until it gets a soft stop, then
    // play the rest of the sound.  
    virtual HRESULT CreateASRBuffer(TRef<ISoundInstance>& psoundNew, 
        ISoundPCMData* pcmdata, unsigned uLoopStart, unsigned uLoopLength, 
        ISoundPositionSource* psource = NULL);

    // Gets an event which fires each time update is called.  This can be used
    // for some of the trickier sounds that change with time.
    virtual IIntegerEventSource* GetUpdateEventSource();


    //
    // ISoundDebugDump
    //

#ifdef _DEBUG
    // return a human-readable description of the object, prepending
    // strIndent to the beginning of each line.  
    ZString DebugDump(const ZString& strIndent = "");
#endif
};

};