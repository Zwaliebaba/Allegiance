//
// xa2spatial.h: the XAudio2 side of 3D sound for headphones and the station
// reverb (see MODERNIZATION.md, "Sound: 3D audio for headphones").
//

#pragma once

namespace SoundEngine {

//
// The head-model effect (spatializer.h) as an XAudio2 effect (XAPO): one
// or two channels in, two out. Its parameters are the direction of the
// sound in the listener's frame (x right, y up, z front); unpositioned
// sounds pass through.
//

struct SpatialEffectParameters
{
    float  x, y, z;
    UINT32 bSpatial;
};

// The new effect, with one reference for the caller
HRESULT CreateSpatialEffect(IUnknown** ppeffect);

//
// The reverb the game's sounds are sent to while docked: a submix voice
// with one input channel and XAudio2's reverb (xa2reverb.cpp), silent
// until its volume is raised. NULL if it couldn't be made.
//

IXAudio2SubmixVoice* CreateReverbVoice(IXAudio2* pxaudio2, UINT32 nSampleRate, UINT32 nOutputChannels);

}
