//
// x3dpan.cpp: X3DAudio's pan and doppler (see x3dpan.h). Built without the
// precompiled header.
//

#include <windows.h>
#include <xaudio2.h>
#include <x3daudio.h>

#include "x3dpan.h"

namespace SoundEngine {

static_assert(sizeof(X3DAUDIO_HANDLE) == c_cbX3DHandle, "X3DAUDIO_HANDLE_BYTESIZE");

static X3DAUDIO_VECTOR Convert(const PanVector& vect)
{
    X3DAUDIO_VECTOR result;
    result.x = vect.x;
    result.y = vect.y;
    result.z = vect.z;
    return result;
}

bool X3DInitialize(unsigned dwChannelMask, float fSpeedOfSound, unsigned char handle[c_cbX3DHandle])
{
    return SUCCEEDED(X3DAudioInitialize(dwChannelMask, fSpeedOfSound, handle));
}

float X3DPan(const unsigned char handle[c_cbX3DHandle], const PanListener& listener,
    const PanVector& vectPosition, const PanVector& vectVelocity, float fDopplerScaler,
    unsigned nOutputChannels, float* pfLevels)
{
    // a flat volume curve: the distance attenuation is DirectSound's
    // (soundmath.h), applied by the voice
    static X3DAUDIO_DISTANCE_CURVE_POINT s_points[2] = { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
    static X3DAUDIO_DISTANCE_CURVE       s_curve     = { s_points, 2 };

    X3DAUDIO_LISTENER x3dlistener = {};
    x3dlistener.OrientFront = Convert(listener.m_front);
    x3dlistener.OrientTop   = Convert(listener.m_top);
    x3dlistener.Position    = Convert(listener.m_position);
    x3dlistener.Velocity    = Convert(listener.m_velocity);

    X3DAUDIO_EMITTER emitter = {};
    emitter.OrientFront         = { 0.0f, 0.0f, 1.0f };
    emitter.OrientTop           = { 0.0f, 1.0f, 0.0f };
    emitter.Position            = Convert(vectPosition);
    emitter.Velocity            = Convert(vectVelocity);
    emitter.ChannelCount        = 1;
    emitter.CurveDistanceScaler = 1.0f;
    emitter.DopplerScaler       = fDopplerScaler;
    emitter.pVolumeCurve        = &s_curve;

    X3DAUDIO_DSP_SETTINGS dsp = {};
    dsp.SrcChannelCount     = 1;
    dsp.DstChannelCount     = nOutputChannels;
    dsp.pMatrixCoefficients = pfLevels;

    X3DAudioCalculate(handle, &x3dlistener, &emitter,
        X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER, &dsp);

    return dsp.DopplerFactor;
}

}
