//
// spatializer.h: binaural rendering of one sound for headphones, from a
// parametric model of the head (no measured HRTF data). Plain C++, so the
// unit tests can drive it without XAudio2; xa2spatial.cpp runs it as an
// XAudio2 effect on each positioned voice when the output is stereo.
//
// The model, per ear:
//
//   - Interaural time difference: Woodworth's formula for a rigid sphere,
//     the far ear delayed by (a / c) (theta + sin theta), theta the lateral
//     angle (up to 0.66 ms).
//   - Head shadow: Brown and Duda's one-pole, one-zero filter, a high-
//     frequency boost of up to 6 dB facing the ear and a cut of up to 20 dB
//     on the far side (with the "bright spot" straight behind the ear).
//   - Pinna: Brown and Duda's five echoes, whose delays depend on azimuth
//     and elevation; they put the notches that tell up from down.
//   - A high shelf that dims the top end of sounds behind the listener, the
//     strongest front-back cue a head model without a measured pinna has.
//
// C. P. Brown and R. O. Duda, "A structural model for binaural sound
// synthesis", IEEE Transactions on Speech and Audio Processing 6(5), 1998.
//
// Directions are in the listener's frame: x right, y up, z front.
//

#pragma once

#include <cmath>
#include <vector>

namespace SoundEngine {

const int c_nPinnaEchoes = 5;

struct HeadDirection
{
    float x, y, z;
};

struct EarParams
{
    float m_fDelay;                         // samples (the interaural time difference)
    float m_fGain;                          // direct path
    float m_fPinnaDelay[c_nPinnaEchoes];    // samples after the direct path
    float m_fPinnaGain[c_nPinnaEchoes];
    float m_fShadowB0, m_fShadowB1, m_fShadowA1;  // head shadow: y = b0 x + b1 x1 - a1 y1
    float m_fRear;                          // high-shelf level, 1 in front
};

struct HeadParams
{
    EarParams m_ear[2];                     // 0 left, 1 right
};

namespace HeadModel {

const float c_fPi            = 3.14159265f;
const float c_fHeadRadius    = 0.0875f;    // meters
const float c_fSpeedOfSound  = 343.0f;     // meters per second
const float c_fShadowMin     = 0.1f;       // Brown and Duda's alpha min
const float c_fShadowAngle   = 150.0f / 180.0f * c_fPi;   // theta min
const float c_fPinnaMix      = 0.5f;       // the echoes at half Brown and Duda's strength: their full comb colours the sound
const float c_fRearCut       = 0.5f;       // the top end straight behind, relative to in front (-6 dB)
const float c_fRearShelfHz   = 4000.0f;
const float c_fOutputGain    = 0.7071f;    // a sound straight ahead about as loud as X3DAudio's centre pan

// Brown and Duda's pinna echoes, delays in samples at 44.1 kHz
const float c_fPinnaRho[c_nPinnaEchoes] = { 0.5f, -1.0f, 0.5f, -0.25f, 0.25f };
const float c_fPinnaA[c_nPinnaEchoes]   = { 1.0f,  5.0f, 5.0f,  5.0f,  5.0f };
const float c_fPinnaB[c_nPinnaEchoes]   = { 2.0f,  4.0f, 7.0f, 11.0f, 13.0f };
const float c_fPinnaD[c_nPinnaEchoes]   = { 1.0f,  0.5f, 0.5f,  0.5f,  0.5f };

inline float Clamp(float f, float fMin, float fMax)
{
    return f < fMin ? fMin : (f > fMax ? fMax : f);
}

inline HeadDirection Normalize(const HeadDirection& d)
{
    float fLength = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);

    if (fLength < 1e-6f) {
        return HeadDirection{ 0, 0, 1 };    // at the listener: straight ahead
    }

    return HeadDirection{ d.x / fLength, d.y / fLength, d.z / fLength };
}

// The interaural time difference in seconds (positive: the source is to the right)
inline float InterauralTimeDifference(const HeadDirection& d)
{
    float fLateral = std::asin(Clamp(d.x, -1, 1));
    float fAbs     = std::fabs(fLateral);
    float fITD     = c_fHeadRadius / c_fSpeedOfSound * (fAbs + std::sin(fAbs));

    return fLateral >= 0 ? fITD : -fITD;
}

// Brown and Duda's head shadow alpha for the angle between the ear's axis
// and the source (0: facing the ear)
inline float ShadowAlpha(float fAngleFromEar)
{
    return (1 + c_fShadowMin / 2) + (1 - c_fShadowMin / 2) * std::cos(fAngleFromEar / c_fShadowAngle * c_fPi);
}

inline HeadParams Compute(const HeadDirection& dIn, float fSampleRate)
{
    HeadDirection d = Normalize(dIn);
    HeadParams    params;

    float fITD      = InterauralTimeDifference(d);
    float fAzimuth  = std::atan2(d.x, d.z);                 // 0 ahead, +-pi behind
    float fElevate  = std::asin(Clamp(d.y, -1, 1));
    float fPinnaCos = std::cos(fAzimuth / 2);
    float fScale    = fSampleRate / 44100.0f;
    float fRear     = d.z < 0 ? -d.z : 0;

    //
    // The echo gains sum to zero, so the pinna leaves low frequencies (where
    // most of the game's sound is) at the direct path's level, and only
    // colours the top end
    //

    float fNorm = c_fOutputGain;

    //
    // Head shadow, bilinear transform of (1 + alpha s / 2w0) / (1 + s / 2w0)
    // with w0 = c / a
    //

    float fK = fSampleRate / (c_fSpeedOfSound / c_fHeadRadius);

    for (int ear = 0; ear < 2; ear++) {
        EarParams& p    = params.m_ear[ear];
        float      fCos = ear == 1 ? d.x : -d.x;            // cosine of the angle from this ear's axis
        float      fAlpha = ShadowAlpha(std::acos(Clamp(fCos, -1, 1)));

        bool bFar = ear == 1 ? fITD < 0 : fITD > 0;

        p.m_fDelay = bFar ? std::fabs(fITD) * fSampleRate : 0;
        p.m_fGain  = fNorm;

        for (int k = 0; k < c_nPinnaEchoes; k++) {
            p.m_fPinnaDelay[k] = (c_fPinnaA[k] * fPinnaCos * std::sin(c_fPinnaD[k] * (c_fPi / 2 - fElevate)) + c_fPinnaB[k]) * fScale;
            p.m_fPinnaGain[k]  = c_fPinnaRho[k] * c_fPinnaMix * fNorm;
        }

        p.m_fShadowB0 = (1 + fAlpha * fK) / (1 + fK);
        p.m_fShadowB1 = (1 - fAlpha * fK) / (1 + fK);
        p.m_fShadowA1 = (1 - fK) / (1 + fK);
        p.m_fRear     = 1 - c_fRearCut * fRear;
    }

    return params;
}

} // namespace HeadModel


//
// One sound's binaural renderer: a short history of its (mono) input, and
// each ear's filter states. Parameters change smoothly across each block.
//

class Spatializer
{
    std::vector<float> m_history;
    unsigned           m_nMask;
    unsigned           m_nWrite;
    float              m_fSampleRate;
    float              m_fRearCoefficient;   // one-pole low-pass at the rear shelf frequency

    HeadParams         m_params;             // as of the end of the last block
    HeadParams         m_target;
    bool               m_bSpatial;
    bool               m_bHaveParams;

    struct EarState
    {
        float m_fShadowX1, m_fShadowY1;
        float m_fRearLow;
    } m_state[2];

    unsigned           m_nSilentFrames;      // since the input went silent

    float Read(float fDelay) const
    {
        // linear interpolation between the two samples around the delay
        int   nDelay = (int)fDelay;
        float fFrac  = fDelay - nDelay;
        float f0     = m_history[(m_nWrite - nDelay) & m_nMask];
        float f1     = m_history[(m_nWrite - nDelay - 1) & m_nMask];

        return f0 + (f1 - f0) * fFrac;
    }

    static float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

public:
    Spatializer() :
        m_nMask(0),
        m_nWrite(0),
        m_fSampleRate(48000),
        m_fRearCoefficient(0),
        m_bSpatial(false),
        m_bHaveParams(false),
        m_nSilentFrames(0)
    {
        Init(48000);
    }

    void Init(float fSampleRate)
    {
        m_fSampleRate = fSampleRate;

        //
        // History for the longest path: the time difference and the last
        // pinna echo, at the highest elevation term, plus interpolation
        //

        float    fMaxDelay = (0.0007f + 20.0f / 44100.0f) * fSampleRate + 4;
        unsigned nSize     = 64;

        while (nSize < (unsigned)fMaxDelay) {
            nSize *= 2;
        }

        m_history.assign(nSize, 0.0f);
        m_nMask = nSize - 1;

        m_fRearCoefficient = 1 - std::exp(-2 * HeadModel::c_fPi * HeadModel::c_fRearShelfHz / fSampleRate);

        Reset();
    }

    void Reset()
    {
        for (float& f : m_history) {
            f = 0;
        }

        m_nWrite        = 0;
        m_state[0]      = EarState{ 0, 0, 0 };
        m_state[1]      = EarState{ 0, 0, 0 };
        m_nSilentFrames = (unsigned)m_history.size();
        m_bHaveParams   = false;
    }

    // bSpatial false: an unpositioned sound, passed through (mono to both
    // ears at full level, stereo as it is)
    void SetDirection(const HeadDirection& d, bool bSpatial)
    {
        m_bSpatial = bSpatial;

        if (bSpatial) {
            m_target = HeadModel::Compute(d, m_fSampleRate);

            if (!m_bHaveParams) {
                m_params      = m_target;
                m_bHaveParams = true;
            }
        }
    }

    // How long the output keeps ringing after the input stops
    unsigned GetTailFrames() const
    {
        return (unsigned)m_history.size();
    }

    // Whether everything has rung out (the output would be silence)
    bool IsQuiet() const
    {
        return m_nSilentFrames >= GetTailFrames();
    }

    //
    // pIn: nFrames frames of nInChannels (1 or 2) interleaved floats, or
    // NULL for silence; pOut: nFrames stereo frames
    //

    void Process(const float* pIn, unsigned nInChannels, float* pOut, unsigned nFrames)
    {
        if (!m_bSpatial || !m_bHaveParams) {
            for (unsigned i = 0; i < nFrames; i++) {
                float l = pIn ? pIn[i * nInChannels] : 0;
                float r = pIn ? pIn[i * nInChannels + nInChannels - 1] : 0;

                pOut[2 * i + 0] = l;
                pOut[2 * i + 1] = r;

                // keep the history current, so turning spatial on doesn't
                // play stale samples
                m_nWrite = (m_nWrite + 1) & m_nMask;
                m_history[m_nWrite] = nInChannels == 2 ? 0.5f * (l + r) : l;
            }

            m_nSilentFrames = pIn ? 0 : m_nSilentFrames + nFrames;
            return;
        }

        const HeadParams from = m_params;
        const HeadParams to   = m_target;

        for (unsigned i = 0; i < nFrames; i++) {
            float x = 0;

            if (pIn) {
                x = nInChannels == 2
                    ? 0.5f * (pIn[2 * i] + pIn[2 * i + 1])
                    : pIn[i];
            }

            m_nWrite = (m_nWrite + 1) & m_nMask;
            m_history[m_nWrite] = x;

            float t = (float)(i + 1) / nFrames;

            for (int ear = 0; ear < 2; ear++) {
                const EarParams& a = from.m_ear[ear];
                const EarParams& b = to.m_ear[ear];
                EarState&        s = m_state[ear];

                float fDelay = Lerp(a.m_fDelay, b.m_fDelay, t);
                float y      = Read(fDelay) * Lerp(a.m_fGain, b.m_fGain, t);

                for (int k = 0; k < c_nPinnaEchoes; k++) {
                    y += Read(fDelay + Lerp(a.m_fPinnaDelay[k], b.m_fPinnaDelay[k], t))
                        * Lerp(a.m_fPinnaGain[k], b.m_fPinnaGain[k], t);
                }

                // head shadow
                float b0 = Lerp(a.m_fShadowB0, b.m_fShadowB0, t);
                float b1 = Lerp(a.m_fShadowB1, b.m_fShadowB1, t);
                float a1 = Lerp(a.m_fShadowA1, b.m_fShadowA1, t);
                float z  = b0 * y + b1 * s.m_fShadowX1 - a1 * s.m_fShadowY1;

                s.m_fShadowX1 = y;
                s.m_fShadowY1 = std::fabs(z) < 1e-20f ? 0 : z;

                // rear high shelf: the low band as it is, the high band dimmed
                s.m_fRearLow += m_fRearCoefficient * (z - s.m_fRearLow);
                if (std::fabs(s.m_fRearLow) < 1e-20f) s.m_fRearLow = 0;

                float fRear = Lerp(a.m_fRear, b.m_fRear, t);

                pOut[2 * i + ear] = s.m_fRearLow + fRear * (z - s.m_fRearLow);
            }
        }

        m_params        = to;
        m_nSilentFrames = pIn ? 0 : m_nSilentFrames + nFrames;
    }
};

} // namespace SoundEngine
