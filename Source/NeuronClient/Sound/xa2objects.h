//
// xa2objects.h: Windows Spatial Sound (ISpatialAudioClient) for positioned
// sounds, when the player has turned on Windows Sonic for Headphones, Dolby
// Atmos or DTS:X. Each sound gets a dynamic audio object; XAudio2 still
// decodes, pitches and filters it, and the voice's effect hands the result
// over through an ObjectStream (spatialstream.h). See MODERNIZATION.md,
// "Sound: 3D audio for headphones".
//

#pragma once

#include "spatialstream.h"

namespace SoundEngine {

class SpatialObjects : public IObject
{
public:
    // NULL if spatial sound isn't on (or isn't available)
    static TRef<SpatialObjects> Create();

    // The audio objects' sample rate, which the mastering voice must run at
    virtual UINT32 GetSampleRate() = 0;

    //
    // Slots, one per sound, used from the game thread:
    //   AcquireSlot   a free slot (-1 if none, or if spatial sound has
    //                 failed); the render thread then asks Windows for an
    //                 audio object
    //   IsSlotDenied  Windows had no object for it (others may be using
    //                 them): the sound should be released and played
    //                 another way
    //   ReleaseSlot   once the voice's effect has stopped writing (the voice
    //                 is destroyed, or no longer writes to the slot)
    //   IsFailed      the stream has stopped (the device changed, spatial
    //                 sound was turned off): every sound should play
    //                 another way
    //

    virtual int           AcquireSlot()           = 0;
    virtual bool          IsSlotDenied(int nSlot) = 0;
    virtual void          ReleaseSlot(int nSlot)  = 0;
    virtual bool          IsFailed()              = 0;

    // The slot's stream, written by the voice's effect on XAudio2's thread
    virtual ObjectStream* GetStream(int nSlot) = 0;
};

}
