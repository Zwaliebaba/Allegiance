//
// VirtualBuffer.h
//
// Classes representing virtual sounds that may or may not have a real
// XAudio2 voice associated with them at any given moment (the engine gives
// voices to the most important ones; see Xa2Engine.cpp).
//

namespace SoundEngine {

//
// A sound instance implementation representing a single virtual sound. This
// may or may not have an actual voice associated with it at any time - the
// sound engine controls that aspect of things.
//
class VirtualSoundBuffer : public ISoundInstance, public ISoundTweakable  SoundDebugImpl
{
protected:

    // An event source which is triggered when a sound is finished (often NULL)
    TRef<EventSourceImpl> m_peventsourceFinished;

    // the current buffer position of the sound
    BufferPositionTracker m_bufferPositionTracker;

    // The data source used to create this sound
    TRef<ISoundPCMData> m_pdata;

    // The current voice (possibly NULL)
    TRef<XA2Voice> m_pvoice;

    // the aproximate age of this sound in seconds, used for priority purposes
    float m_fAge;

    // has this sound started?
    bool m_bStarted;

    // has this sound stopped?
    bool m_bStopped;

    // are we trying to stop this sound?
    bool m_bRetryStop;

    // is the buffer currently playing?
    bool m_bBufferPlaying;

    // is the buffer currently audible?
    bool m_bAudible;

    // the dynamic priority of the sound.  The only meaning this number has is
    // in relation to other priorities, where the higher number is the higher
    // priority.  
    float m_fDynamicPriority;


    //
    // cached settings
    //

    // The current priority of the sound
    float m_fPriority;

    // The current gain of the sound
    float m_fGain;

    // the current pitch of the sound
    float m_fPitch;


    // prepare a voice with the given 3D support
    virtual HRESULT PrepareBuffer(XA2Device* pdevice, bool bSupport3D);

public:

    // Creates a virtual sound buffer for a normal sound, possibly looping
    VirtualSoundBuffer(ISoundPCMData* pdata, bool bLooping);

    // Creates a virtual sound buffer for an ASR sound
    VirtualSoundBuffer(ISoundPCMData* pdata, DWORD dwLoopOffset, DWORD dwLoopLength);


    //
    // Accessors
    //
        
    // returns true iff the buffer is no longer playing (and can be discarded)
    bool IsStopped()
    {
        return m_bStopped;
    };

    // Returns true if this has a playing physical buffer
    bool HasPlayingBuffer()
    {
        return m_bBufferPlaying;
    };

    // Returns true if this is audible at the moment
    bool IsAudible()
    {
        return m_bAudible;
    };

    // Gets the dynamic priority of the sound.  The only meaning this number 
    // has is in relation to other priorities, where the higher number is the
    // higher priority.  
    float GetDynamicPriority()
    {
        return m_fDynamicPriority;
    };



    // Recalculates the sounds position, loudness, whether it's playing, etc..
    virtual HRESULT Update(DWORD dwTimeElapsed, 
            const Vector& vectListenerPosition, float fRolloffFactor);

    // Creates and starts a voice for this sound
    virtual HRESULT StartBuffer(XA2Device* pdevice);

    // forcibly stops the voice (and frees it).
    HRESULT StopBuffer();


    //
    // ISoundTweakable interface
    //

    // Sets the gain, from 0 to -100 dB
    HRESULT SetGain(float fGain) override;

    // Sets the pitch shift, where 1.0 is normal, 0.5 is half of normal speed, 
    // and 2.0 is twice normal speed.  
    HRESULT SetPitch(float fPitch) override;

    // sets the priority - used as a addition to volume when choosing which 
    // sounds are most important to play.
    HRESULT SetPriority(float fPriority) override;


    //
    // ISoundInstance interface
    //

    // Stops the sound.  If bForceNow is true the sound will stop ASAP, 
    // possibly popping.  If it is false some sounds may play a trail-off 
    // sound or fade away.  
    HRESULT Stop(bool bForceNow = false) override;

    // returns S_OK if the sound is currently playing, S_FALSE otherwise.
    HRESULT IsPlaying() override;

    // Gets an event which fires when the sound finishes playing (for any 
    // reason)
    IEventSource* GetFinishEventSource() override;

    // Gets an interface for tweaking the sound, if supported, NULL otherwise.
    TRef<ISoundTweakable> GetISoundTweakable() override;

    // Gets an interface for tweaking the sound, if supported, NULL otherwise.
    TRef<ISoundTweakable3D> GetISoundTweakable3D() override;


    //
    // ISoundDebugDump
    //

#ifdef _DEBUG
    // return a human-readable description of the object, prepending
    // strIndent to the beginning of each line.  
    ZString DebugDump(const ZString& strIndent = "");
#endif
};


class VirtualSoundBuffer3D : public VirtualSoundBuffer, public ISoundTweakable3D
{
private:

    //
    // cached settings
    //

    // The source of this sound, position-wise
    TRef<ISoundPositionSource> m_pposSource;

    // is 3D on at the moment?
    bool m_b3D;

    // have we decided to play in 3D at the moment?
    bool m_bPlayingIn3D;

    // the current maximum distance of the sound
    float m_fMinimumDistance;

    // the current sound cone
    float m_fInnerAngle, m_fOuterAngle, m_fOutsideGain;

    // the last known position of the sound
    Vector m_vectPosition;

    // the last known velocity of the sound
    Vector m_vectVelocity;

    // the last known orientation of the sound
    Vector m_vectOrientation;

    // whether or not the last known info is relative to the listener
    bool m_bListenerRelative;

public:

    // constructs a simple 3D capable sound
    VirtualSoundBuffer3D(ISoundPCMData* pdata, bool bLooping, 
            TRef<ISoundPositionSource> psource);

    // constructs an ASR 3D capable sound
    VirtualSoundBuffer3D(ISoundPCMData* pdata, DWORD dwLoopOffset, 
            DWORD dwLoopLength, TRef<ISoundPositionSource> psource);

    //
    // VirtualSoundBuffer overides
    //


protected:

    // prepare a voice with the given 3D support
    HRESULT PrepareBuffer(XA2Device* pdevice, bool bSupport3D) override;

public:

    // Recalculates the sounds position, loudness, whether it's playing, etc..
    HRESULT Update(DWORD dwTimeElapsed, 
            const Vector& vectListenerPosition, float fRolloffFactor) override;


    //
    // ISoundTweakable3D interface
    //

    // toggles 3D Positioning on and off for the given sound.
    HRESULT Set3D(bool b3D) override;

    // Sets the distance at which the sound will be at max volume.  This
    // effects how quickly the sound drops off with distance.  
    HRESULT SetMinimumDistance(float fMinimumDistance) override;

    // Sets a sound cone of size fInnerAngle (in degrees) where the volume is at 
    // normal levels, outside of which it fades down by fOutsideGain 
    // (range of 0 to -100 db) at fOuterAngle (degrees) and beyond.  
    HRESULT SetCone(float fInnerAngle, float fOuterAngle, float fOutsideGain) override;


    //
    // ISoundTweakable interface
    //

    // Sets the gain, from 0 to -100 dB
    HRESULT SetGain(float fGain) override;

    // Sets the pitch shift, where 1.0 is normal, 0.5 is half of normal speed, 
    // and 2.0 is twice normal speed.  
    HRESULT SetPitch(float fPitch) override;

    // sets the priority - used as a addition to volume when choosing which 
    // sounds are most important to play.
    HRESULT SetPriority(float fPriority) override;

    // Gets an interface for tweaking the sound, if supported, NULL otherwise.
    TRef<ISoundTweakable> GetISoundTweakable() override;

    // Gets an interface for tweaking the sound, if supported, NULL otherwise.
    TRef<ISoundTweakable3D> GetISoundTweakable3D() override;
};

};