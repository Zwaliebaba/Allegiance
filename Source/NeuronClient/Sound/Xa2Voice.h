//
// Xa2Voice.h: the XAudio2 device and the source voice that plays one sound
// (they replace the DirectSound 3D buffers; see MODERNIZATION.md, "Sound:
// XAudio2 replaces DirectSound")
//

namespace SoundEngine {

// game vectors are right-handed; DirectSound 3D and X3DAudio are left-handed
inline PanVector ConvertVector(const Vector& vect)
{
    return PanVector{ vect.X(), vect.Y(), -vect.Z() };
}

inline void ZAssertIsUnitVector(const Vector& vect)
{
#ifdef _DEBUG
    float fLengthSquared = vect.LengthSquared();
    ZAssert(fLengthSquared > (0.98f*0.98f));
    ZAssert(fLengthSquared < (1.02f*1.02f));
#endif
}


//
// The XAudio2 engine, its mastering voice, X3DAudio, and the listener the
// sound engine sets before each update. Each voice holds a reference, so the
// device goes after the last voice.
//

class SpatialObjects;

class XA2Device : public IObject
{
public:
    IXAudio2*               m_pxaudio2;
    IXAudio2MasteringVoice* m_pmaster;
    IXAudio2SubmixVoice*    m_preverb;          // the station reverb, or NULL
    DWORD                   m_dwChannelMask;
    UINT32                  m_nChannels;
    UINT32                  m_nSampleRate;
    bool                    m_bHeadphones;      // stereo output: positioned sounds go through the head model
    TRef<SpatialObjects>    m_pobjects;         // Windows Spatial Sound, when it's on
    unsigned char           m_x3daudio[c_cbX3DHandle];

    // set by the engine before the sounds update
    PanListener             m_listener;              // in the world
    Vector                  m_vectListenerPosition;  // the same, in game coordinates
    PanListener             m_listenerRelative;      // for listener-relative sounds: at the origin, at rest
    float                   m_fRolloffFactor;
    float                   m_fDopplerFactor;
    float                   m_fDistanceFactor;   // meters per game unit

    XA2Device();
    ~XA2Device();

    HRESULT Init();

    // X3DAudio takes the speed of sound in game units, which the distance
    // factor gives
    void SetDistanceFactor(float fMetersPerUnit);

    // The station reverb's level, 0 (off) to 1
    void SetReverbLevel(float fLevel);
};


//
// One XAudio2 source voice, playing one sound from a given position: the
// regions PlanVoiceRegions gives, all from one copy of the sound's data that
// the voices of that sound share. Volume, pitch and the 3D state are applied
// as they are set: the pan and doppler from X3DAudio, the distance and cone
// attenuation as DirectSound 3D computed them (SoundMath.h).
//

class XA2Voice : public ISoundObject  SoundDebugImpl
{
    TRef<XA2Device>                         m_pdevice;
    TRef<ISoundPCMData>                     m_pdata;
    std::shared_ptr<const std::vector<BYTE>> m_pbytes;
    IXAudio2SourceVoice*                    m_pvoice;

    unsigned m_nChannels;
    unsigned m_nOutChannels;    // into the output matrix: the effect's two, or the sound's own
    bool     m_bSpatial;        // the voice has the 3D effect (head model, spatial sound hand-over)
    int      m_nSlot;           // its Windows Spatial Sound slot, or -1; kept until the voice goes
    bool     m_bSlotTried;      // a slot was asked for once (one per voice)
    float    m_fFilter;         // the distance low-pass, as last set (XAudio2's radians)
    unsigned m_nBlockAlign;
    unsigned m_nLength;         // samples
    bool     m_bLooping;
    bool     m_bASR;
    unsigned m_nLoopStart;      // samples
    unsigned m_nLoopLength;     // samples
    bool     m_bSupport3D;

    std::vector<VoiceRegion> m_regions;     // submitted; the buffer contexts are index + 1
    bool                     m_bStarted;
    bool                     m_bStopped;    // stopped by Stop(true)

    // cached settings
    float  m_fGain;
    float  m_fPitch;
    float  m_fDoppler;
    bool   m_b3D;
    bool   m_bListenerRelative;
    float  m_fMinimumDistance;
    float  m_fInnerAngle, m_fOuterAngle, m_fOutsideGain;
    Vector m_vectPosition;
    Vector m_vectVelocity;
    Vector m_vectOrientation;

    HRESULT Submit(const VoiceRegion& region, bool bLast);
    void    ApplyFrequency();
    void    ApplyOutput();

public:
    XA2Voice();
    ~XA2Voice();

    // bASR: dwLoopStart and dwLoopLength (bytes) are the sustain loop
    HRESULT Init(XA2Device* pdevice, ISoundPCMData* pdata, bool bLooping,
        bool bASR, DWORD dwLoopStart, DWORD dwLoopLength, bool bSupport3D);

    // Starts the voice at the given position (bytes); bIsStopping: an ASR
    // sound that has already been stopped plays its release
    HRESULT Start(DWORD dwPosition, bool bIsStopping = false);

    // A forced stop ends it now; otherwise an ASR sound plays its release
    // (and any other sound stops now, as with DirectSound)
    HRESULT Stop(bool bForceNow = false);

    bool IsPlaying();

    // Sets the volume and pitch
    HRESULT UpdateState(float fGain, float fPitch);

    // Sets the 3D state; b3D false plays the sound unpositioned
    HRESULT UpdateState3D(
        const Vector& vectPosition,
        const Vector& vectVelocity,
        const Vector& vectOrientation,
        float fMinimumDistance,
        float fInnerAngle,
        float fOuterAngle,
        float fOutsideGain,
        bool b3D,
        bool bListenerRelative
        );

#ifdef _DEBUG
    ZString DebugDump(const ZString& strIndent = "");
#endif
};

};
