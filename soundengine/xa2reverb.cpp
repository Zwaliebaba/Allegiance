//
// xa2reverb.cpp: the reverb submix voice for the station screens (see
// xa2spatial.h). XAudio2's reverb with the I3DL2 "large room" preset, fully
// wet; the voice starts silent and the engine raises its volume while the
// player is docked.
//

#include "pch.h"
#include "xa2spatial.h"

#include <xaudio2fx.h>

namespace SoundEngine {

IXAudio2SubmixVoice* CreateReverbVoice(IXAudio2* pxaudio2, UINT32 nSampleRate, UINT32 nOutputChannels)
{
    IUnknown* preverb = NULL;

    if (FAILED(XAudio2CreateReverb(&preverb))) {
        debugf("XAudio2CreateReverb failed\n");
        return NULL;
    }

    //
    // The reverb takes one or two channels and gives one, two or six; a
    // stereo reverb serves every output (more channels get it at the front)
    //

    XAUDIO2_EFFECT_DESCRIPTOR descriptor = { preverb, TRUE, nOutputChannels >= 2 ? 2u : 1u };
    XAUDIO2_EFFECT_CHAIN      chain      = { 1, &descriptor };

    IXAudio2SubmixVoice* pvoice = NULL;
    HRESULT hr = pxaudio2->CreateSubmixVoice(&pvoice, 1, nSampleRate, 0, 0, NULL, &chain);
    preverb->Release();

    if (FAILED(hr)) {
        debugf("Reverb submix voice: %X\n", hr);
        return NULL;
    }

    XAUDIO2FX_REVERB_I3DL2_PARAMETERS i3dl2  = XAUDIO2FX_I3DL2_PRESET_LARGEROOM;
    XAUDIO2FX_REVERB_PARAMETERS       native;

    ReverbConvertI3DL2ToNative(&i3dl2, &native);
    native.WetDryMix = 100;     // the voice carries only the reverb; the dry sound goes to the master

    pvoice->SetEffectParameters(0, &native, sizeof(native));
    pvoice->SetVolume(0);

    return pvoice;
}

}
