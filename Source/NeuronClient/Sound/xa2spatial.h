//
// xa2spatial.h: the XAudio2 side of 3D sound for headphones and the station
// reverb (see MODERNIZATION.md, "Sound: 3D audio for headphones").
//

#pragma once

namespace SoundEngine {

class SpatialObjects;

//
// The effect on a positioned voice (an XAPO): one or two channels in, two
// out. Its parameters are the direction of the sound in the listener's
// frame (x right, y up, z front) and one of three ways to play it:
//
//   bSpatial   the head model renders it for headphones (spatializer.h)
//   nSlot >= 0 it goes to that Windows Spatial Sound object
//              (xa2objects.h) at fGain, and passes through for the reverb
//              send (the voice's master output is silent)
//   neither    it passes through (mono to both channels), for X3DAudio's
//              pan or an unpositioned sound
//

struct SpatialEffectParameters
{
    float  x, y, z;
    UINT32 bSpatial;
    INT32  nSlot;
    float  fGain;
};

// The new effect, with one reference for the caller. pobjects (or NULL) is
// the spatial sound renderer, which must outlive the effect.
HRESULT CreateSpatialEffect(IUnknown** ppeffect, SpatialObjects* pobjects);

//
// The reverb the game's sounds are sent to while docked: a submix voice
// with one input channel and XAudio2's reverb (xa2reverb.cpp), silent
// until its volume is raised. NULL if it couldn't be made.
//

IXAudio2SubmixVoice* CreateReverbVoice(IXAudio2* pxaudio2, UINT32 nSampleRate, UINT32 nOutputChannels);

}
