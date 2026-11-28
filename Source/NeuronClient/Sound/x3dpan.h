//
// x3dpan.h: X3DAudio's pan and doppler, behind plain types. x3dpan.cpp is the
// only file that includes x3daudio.h, whose Direct3D vector type must not
// meet the DirectX 7 types zlib.h brings in.
//

#pragma once

namespace SoundEngine {

const unsigned c_cbX3DHandle = 20;    // X3DAUDIO_HANDLE_BYTESIZE

// left-handed, as X3DAudio's (and DirectSound 3D's)
struct PanVector
{
    float x, y, z;
};

struct PanListener
{
    PanVector m_front;
    PanVector m_top;
    PanVector m_position;
    PanVector m_velocity;
};

// speed of sound in the units positions and velocities are in
bool X3DInitialize(unsigned dwChannelMask, float fSpeedOfSound, unsigned char handle[c_cbX3DHandle]);

// The output levels of a point sound (pan only: no distance attenuation),
// for nOutputChannels channels; returns the doppler frequency factor
float X3DPan(const unsigned char handle[c_cbX3DHandle], const PanListener& listener,
    const PanVector& vectPosition, const PanVector& vectVelocity, float fDopplerScaler,
    unsigned nOutputChannels, float* pfLevels);

}
