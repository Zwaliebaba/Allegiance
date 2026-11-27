//
// soundmath.h
//
// The sound engine's arithmetic, without XAudio2: gains, DirectSound 3D's
// distance and cone attenuation (which the XAudio2 engine reproduces, so the
// mix sounds as it did), the XAudio2 buffers that play a sound from a given
// position, and the position tracker of a virtual sound. Header-only, so the
// unit tests (tests/UnitTests/SoundTests.cpp) use it directly.
//

#pragma once

#include <cmath>
#include <vector>

namespace SoundEngine {

// the minimum audible sound we care about (in dB)
const float c_fMinAudible = -60.0f;

// The conversion ratio from min distance to max distance.
const float c_fMinToMaxDistanceRatio = 4000.0f;

// DirectSound's frequency range (DSBFREQUENCY_MIN, DSBFREQUENCY_MAX), which
// a sound's pitch is still clamped to
const float c_fMinFrequency = 100.0f;
const float c_fMaxFrequency = 200000.0f;


//
// A gain in dB (0 to -100, as DirectSound's volume) as an amplitude factor;
// -100 dB is silence, as DSBVOLUME_MIN was
//

inline float DbToAmplitude(float fGain)
{
    if (fGain <= -100.0f) {
        return 0.0f;
    }

    return powf(10.0f, fGain / 20.0f);
}


//
// DirectSound 3D's distance attenuation, as an amplitude factor: full volume
// up to the minimum distance, then the inverse of the distance (half the
// amplitude, -6 dB, per doubling for a rolloff factor of 1), and silence from
// the maximum distance on (the buffers had DSBCAPS_MUTE3DATMAXDISTANCE; the
// maximum is c_fMinToMaxDistanceRatio times the minimum)
//

inline float DistanceAttenuation(float fDistance, float fMinimumDistance, float fRolloffFactor)
{
    if (fDistance >= fMinimumDistance * c_fMinToMaxDistanceRatio) {
        return 0.0f;
    }

    if (fDistance <= fMinimumDistance) {
        return 1.0f;
    }

    return fMinimumDistance / (fMinimumDistance + fRolloffFactor * (fDistance - fMinimumDistance));
}


//
// DirectSound 3D's sound cone, in dB. fCosAngle is the cosine of the angle
// between the cone's direction and the direction from the sound to the
// listener; the angles are the cone's full widths in degrees, as DirectSound
// took them. Inside the inner cone 0 dB, outside the outer cone fOutsideGain,
// and a linear fade (in dB) between them.
//

inline float ConeGain(float fCosAngle, float fInnerAngle, float fOuterAngle, float fOutsideGain)
{
    if (fInnerAngle >= 360.0f || fOutsideGain == 0.0f) {
        return 0.0f;
    }

    fCosAngle = fCosAngle < -1.0f ? -1.0f : (fCosAngle > 1.0f ? 1.0f : fCosAngle);

    float fAngle      = acosf(fCosAngle) * 57.2957795f;
    float fInnerHalf  = fInnerAngle / 2;
    float fOuterHalf  = fOuterAngle / 2;

    if (fAngle <= fInnerHalf) {
        return 0.0f;
    }

    if (fAngle >= fOuterHalf) {
        return fOutsideGain;
    }

    return fOutsideGain * (fAngle - fInnerHalf) / (fOuterHalf - fInnerHalf);
}


//
// The XAudio2 buffers that play a sound from nPosition (all in samples). Each
// is a region of the sound's one copy of its data:
//
//   normal sound      Once: the position to the end
//   looping sound     Once, then Loop: all of it, repeated until stopped
//   ASR sound         Attack: to the loop start; Sustain: the rest of a loop
//                     pass (if starting inside the loop); SustainLoop: the
//                     loop, repeated until ExitLoop; Release: after the loop
//   stopping ASR      Attack then Release (a stopped sound skips the sustain,
//                     as BufferPositionTracker has it), or Once from inside or
//                     after the loop
//
// Empty regions are left out. An ASR sound with an empty loop plays Once.
//

struct VoiceRegion
{
    enum Part { Once, Loop, Attack, Sustain, SustainLoop, Release };

    Part     m_part;
    unsigned m_nBegin;
    unsigned m_nLength;
    bool     m_bLoop;
};

inline std::vector<VoiceRegion> PlanVoiceRegions(
    unsigned nLength,
    unsigned nPosition,
    bool     bLooping,
    bool     bASR,
    unsigned nLoopStart,
    unsigned nLoopLength,
    bool     bStopping
) {
    std::vector<VoiceRegion> regions;

    auto add = [&](VoiceRegion::Part part, unsigned nBegin, unsigned nEnd, bool bLoop) {
        if (nEnd > nBegin) {
            regions.push_back({ part, nBegin, nEnd - nBegin, bLoop });
        }
    };

    if (nPosition >= nLength) {
        return regions;
    }

    if (!bASR || nLoopLength == 0) {
        add(VoiceRegion::Once, nPosition, nLength, false);

        if (bLooping && !bASR && !bStopping) {
            add(VoiceRegion::Loop, 0, nLength, true);
        }

        return regions;
    }

    unsigned nLoopEnd = nLoopStart + nLoopLength;

    if (bStopping) {
        if (nPosition < nLoopStart) {
            add(VoiceRegion::Attack, nPosition, nLoopStart, false);
            add(VoiceRegion::Release, nLoopEnd, nLength, false);
        } else {
            add(VoiceRegion::Once, nPosition, nLength, false);
        }

        return regions;
    }

    if (nPosition < nLoopStart) {
        add(VoiceRegion::Attack, nPosition, nLoopStart, false);
        add(VoiceRegion::SustainLoop, nLoopStart, nLoopEnd, true);
    } else if (nPosition < nLoopEnd) {
        add(VoiceRegion::Sustain, nPosition, nLoopEnd, false);
        add(VoiceRegion::SustainLoop, nLoopStart, nLoopEnd, true);
    }

    add(VoiceRegion::Release, nPosition < nLoopEnd ? nLoopEnd : nPosition, nLength, false);

    return regions;
}


//
// Tracks a buffer position over the lifetime of a virtual sound.  This is
// designed to lighter-weight than an actual sound buffer, because the sound
// it represents may never be played.  It's designed to be accurate, however,
// so that a buffer created using this information can start at the appropriate
// point.  Positions are in bytes.
//

class BufferPositionTracker
{
    // the current position in the sound
    unsigned m_dwPosition;

    // the length of the buffer
    unsigned m_dwLength;

    // the sample rate of the sound, in bytes per second.
    unsigned m_dwBytesPerSec;

    // a mask of valid position bits (since a position must start on a sample
    // boundary)
    unsigned m_dwPositionMask;

    // true iff end has been called on this
    bool m_bEnding;

    // true if this is looping (non-ASR)
    bool m_bLooping;

    // true iff this is an ASR sound.
    bool m_bASR;

    // the starting position and length of the sustain loop (ASR)
    unsigned m_dwLoopStart;
    unsigned m_dwLoopLength;

public:

    // Create a position tracker for a sound which plays until it reaches the
    // end (for non-looping sounds) or has stop called upon it.
    BufferPositionTracker(unsigned dwLength, unsigned dwBytesPerSec, unsigned dwBytesPerSample, bool bLooping = false) :
        m_dwPosition(0),
        m_dwLength(dwLength),
        m_dwBytesPerSec(dwBytesPerSec),
        m_dwPositionMask(~(dwBytesPerSample - 1)),
        m_bEnding(false),
        m_bLooping(bLooping),
        m_bASR(false),
        m_dwLoopStart(0),
        m_dwLoopLength(0)
    {
    }

    // Creates a position tracker for an ASR buffer.
    BufferPositionTracker(unsigned dwLength, unsigned dwBytesPerSec, unsigned dwBytesPerSample,
            unsigned dwLoopStart, unsigned dwLoopLength) :
        m_dwPosition(0),
        m_dwLength(dwLength),
        m_dwBytesPerSec(dwBytesPerSec),
        m_dwPositionMask(~(dwBytesPerSample - 1)),
        m_bEnding(false),
        m_bLooping(false),
        m_bASR(true),
        m_dwLoopStart(dwLoopStart),
        m_dwLoopLength(dwLoopLength)
    {
    }


    //
    // Events that the buffer tracker needs to know about
    //

    // the sound has been given a stop. A forced stop, or any stop of a
    // non-ASR sound, ends it now; an ASR sound plays its release.
    void Stop(bool bForceNow = false)
    {
        if (!m_bASR || bForceNow) {
            m_dwPosition = m_dwLength;
        }

        m_bEnding = true;
    }

    // the given number of miliseconds have elapsed
    void AddTime(unsigned dwElapsedTime)
    {
        unsigned dwElapsedBytes = (unsigned)(dwElapsedTime * (unsigned long long)m_dwBytesPerSec / 1000);

        if (!m_bASR) {
            m_dwPosition += dwElapsedBytes;

            // if we've passed the end...
            if (m_dwPosition > m_dwLength) {
                // loop for a looping sound
                if (m_bLooping && !m_bEnding) {
                    m_dwPosition %= m_dwLength;
                } else {
                    // stop the position at the end of the sound.
                    m_dwPosition = m_dwLength;
                }
            }
        } else if (m_bEnding) {
            bool bWasInAttack = m_dwPosition < m_dwLoopStart;

            m_dwPosition += dwElapsedBytes;

            // if we were in the attack portion, make sure we jump
            // directly to the release portion of the wave.
            if (bWasInAttack && (m_dwPosition > m_dwLoopStart)) {
                m_dwPosition += m_dwLoopLength;
            }

            // stop the position at the end of the sound.
            if (m_dwPosition > m_dwLength) {
                m_dwPosition = m_dwLength;
            }
        } else {
            m_dwPosition += dwElapsedBytes;

            // loop if appropriate
            if (m_dwLoopLength != 0 && m_dwPosition >= m_dwLoopStart + m_dwLoopLength) {
                m_dwPosition = ((m_dwPosition - m_dwLoopStart) % m_dwLoopLength) + m_dwLoopStart;
            }
        }
    }

    // Resets the position to the given one, presumably retrieved from a
    // playing buffer.
    void SetPosition(unsigned dwPosition)
    {
        m_dwPosition = dwPosition < m_dwLength ? dwPosition : m_dwLength;
    }


    //
    // Info a SoundInstance may need.
    //

    // true iff this has stopped playing.
    bool IsStopped() const { return m_dwPosition >= m_dwLength; }

    // is the sound in the process of stopping?
    bool IsStopping() const { return m_bASR && m_bEnding; }

    // what is the current offset in the source buffer?
    unsigned GetPosition() const { return m_dwPosition & m_dwPositionMask; }

    bool     IsASR()         const { return m_bASR; }
    unsigned GetLoopOffset() const { return m_dwLoopStart; }
    unsigned GetLoopLength() const { return m_dwLoopLength; }
    bool     IsLooping()     const { return m_bLooping; }
};

}
