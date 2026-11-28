#include "pch.h"
#include "Spatializer.h"

#include <vector>

//////////////////////////////////////////////////////////////////////////////
//
// The headphone spatializer (soundengine/Spatializer.h): the parametric head
// model's time and level differences between the ears, its front-back and
// elevation cues, and the renderer's stability. See MODERNIZATION.md,
// "Sound: 3D audio for headphones".
//
//////////////////////////////////////////////////////////////////////////////

using namespace SoundEngine;

namespace HeadModelTests
{
    const float c_fSampleRate = 48000;
    const float c_fPi         = 3.14159265f;

    // Each ear's level in dB for a sine from a direction, relative to the
    // sine itself, measured after the filters have settled
    void EarLevels(const HeadDirection& d, float fHz, double& dbLeft, double& dbRight)
    {
        Spatializer spatializer;
        spatializer.Init(c_fSampleRate);
        spatializer.SetDirection(d, true);

        const unsigned    nFrames = 4800;
        std::vector<float> in(nFrames);
        std::vector<float> out(2 * nFrames);
        double             fLeft  = 0;
        double             fRight = 0;
        double             fInput = 0;

        for (int block = 0; block < 10; block++) {
            for (unsigned i = 0; i < nFrames; i++) {
                in[i] = std::sin(2 * c_fPi * fHz * (block * nFrames + i) / c_fSampleRate);
            }

            spatializer.Process(in.data(), 1, out.data(), nFrames);

            if (block >= 2) {
                for (unsigned i = 0; i < nFrames; i++) {
                    fLeft  += out[2 * i] * out[2 * i];
                    fRight += out[2 * i + 1] * out[2 * i + 1];
                    fInput += in[i] * in[i];
                }
            }
        }

        dbLeft  = 10 * std::log10(fLeft / fInput);
        dbRight = 10 * std::log10(fRight / fInput);
    }

    TEST_CLASS(TimeDifference)
    {
    public:
        // Woodworth: (a / c) (pi / 2 + 1) at 90 degrees, 0.66 ms for an 8.75 cm head
        TEST_METHOD(NinetyDegreesIsWoodworth)
        {
            Assert::AreEqual(0.000656f, HeadModel::InterauralTimeDifference({ 1, 0, 0 }), 0.00001f);
            Assert::AreEqual(-0.000656f, HeadModel::InterauralTimeDifference({ -1, 0, 0 }), 0.00001f);
        }

        TEST_METHOD(NoneAheadBehindAboveBelow)
        {
            Assert::AreEqual(0.0f, HeadModel::InterauralTimeDifference({ 0, 0, 1 }), 1e-7f);
            Assert::AreEqual(0.0f, HeadModel::InterauralTimeDifference({ 0, 0, -1 }), 1e-7f);
            Assert::AreEqual(0.0f, HeadModel::InterauralTimeDifference({ 0, 1, 0 }), 1e-7f);
            Assert::AreEqual(0.0f, HeadModel::InterauralTimeDifference({ 0, -1, 0 }), 1e-7f);
        }

        // The far ear is the one delayed
        TEST_METHOD(FarEarDelayed)
        {
            HeadParams params = HeadModel::Compute({ 1, 0, 0 }, c_fSampleRate);

            Assert::AreEqual(0.000656f * c_fSampleRate, params.m_ear[0].m_fDelay, 0.5f);
            Assert::AreEqual(0.0f, params.m_ear[1].m_fDelay);
        }
    };

    TEST_CLASS(LevelDifference)
    {
    public:
        // A source on the right: the head shadows the left ear, strongly at
        // high frequencies and hardly at low ones
        TEST_METHOD(HeadShadowGrowsWithFrequency)
        {
            double dbLeftLow, dbRightLow, dbLeftHigh, dbRightHigh;
            EarLevels({ 1, 0, 0 }, 200, dbLeftLow, dbRightLow);
            EarLevels({ 1, 0, 0 }, 8000, dbLeftHigh, dbRightHigh);

            Assert::IsTrue(dbRightLow - dbLeftLow < 2, L"200 Hz: little difference");
            Assert::IsTrue(dbRightHigh - dbLeftHigh > 10, L"8 kHz: the far ear shadowed");
        }

        TEST_METHOD(MirrorImage)
        {
            double dbLeftA, dbRightA, dbLeftB, dbRightB;
            EarLevels({ 0.6f, 0.2f, 0.775f }, 3000, dbLeftA, dbRightA);
            EarLevels({ -0.6f, 0.2f, 0.775f }, 3000, dbLeftB, dbRightB);

            Assert::AreEqual(dbLeftA, dbRightB, 0.05);
            Assert::AreEqual(dbRightA, dbLeftB, 0.05);
        }

        // Low frequencies straight ahead come out at X3DAudio's centre pan
        // level (-3 dB per ear), so the change doesn't make the mix louder
        TEST_METHOD(AheadAtCentrePanLevel)
        {
            double dbLeft, dbRight;
            EarLevels({ 0, 0, 1 }, 300, dbLeft, dbRight);

            Assert::AreEqual(-3.0, dbLeft, 1.0);
            Assert::AreEqual(dbLeft, dbRight, 0.01);
        }
    };

    TEST_CLASS(FrontBackAndElevation)
    {
    public:
        // Behind sounds duller than ahead; the low end is the same
        TEST_METHOD(BehindLosesTheTopEnd)
        {
            double dbFrontLow, dbBackLow, dbFrontHigh, dbBackHigh, dbUnused;
            EarLevels({ 0, 0, 1 }, 500, dbFrontLow, dbUnused);
            EarLevels({ 0, 0, -1 }, 500, dbBackLow, dbUnused);
            EarLevels({ 0, 0, 1 }, 8000, dbFrontHigh, dbUnused);
            EarLevels({ 0, 0, -1 }, 8000, dbBackHigh, dbUnused);

            Assert::AreEqual(dbFrontLow, dbBackLow, 0.5, L"500 Hz");
            Assert::IsTrue(dbFrontHigh - dbBackHigh > 3, L"8 kHz");
        }

        // The pinna echoes move with elevation, so the spectrum does too
        TEST_METHOD(ElevationChangesTheSpectrum)
        {
            double dbBelow, dbLevel, dbUnused;
            float  fBelow = -60 * c_fPi / 180;
            EarLevels({ 0, std::sin(fBelow), std::cos(fBelow) }, 7000, dbBelow, dbUnused);
            EarLevels({ 0, 0, 1 }, 7000, dbLevel, dbUnused);

            Assert::IsTrue(std::fabs(dbBelow - dbLevel) > 2);
        }
    };

    TEST_CLASS(Renderer)
    {
    public:
        // An unpositioned sound is passed through: mono to both ears at full
        // level, stereo as it is
        TEST_METHOD(UnpositionedPassesThrough)
        {
            Spatializer spatializer;
            spatializer.Init(c_fSampleRate);
            spatializer.SetDirection({ 1, 0, 0 }, false);

            float mono[3]   = { 0.25f, -0.5f, 1.0f };
            float stereo[6] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f };
            float out[6];

            spatializer.Process(mono, 1, out, 3);
            Assert::AreEqual(0.25f, out[0]); Assert::AreEqual(0.25f, out[1]);
            Assert::AreEqual(1.0f, out[4]);  Assert::AreEqual(1.0f, out[5]);

            spatializer.Process(stereo, 2, out, 3);
            for (int i = 0; i < 6; i++) {
                Assert::AreEqual(stereo[i], out[i]);
            }
        }

        // A source spinning around the head (moving every block) stays
        // finite and bounded
        TEST_METHOD(MovingSourceStaysBounded)
        {
            Spatializer spatializer;
            spatializer.Init(c_fSampleRate);

            std::vector<float> in(480);
            std::vector<float> out(960);
            unsigned           seed = 1;
            float              fMax = 0;

            for (int block = 0; block < 2000; block++) {
                float a = block * 0.05f;
                spatializer.SetDirection({ std::sin(a), 0.3f * std::sin(0.7f * a), std::cos(a) }, true);

                for (float& f : in) {
                    seed = seed * 1664525 + 1013904223;
                    f = (seed >> 8) / 8388608.0f - 1;
                }

                spatializer.Process(in.data(), 1, out.data(), 480);

                for (float f : out) {
                    Assert::IsTrue(std::isfinite(f));
                    fMax = f > fMax ? f : (-f > fMax ? -f : fMax);
                }
            }

            Assert::IsTrue(fMax < 4);
        }

        // After the input stops, the output rings out within the tail
        TEST_METHOD(RingsOutAfterTheInputStops)
        {
            Spatializer spatializer;
            spatializer.Init(c_fSampleRate);
            spatializer.SetDirection({ 1, 0, 0 }, true);

            std::vector<float> in(480, 1.0f);
            std::vector<float> out(960);

            spatializer.Process(in.data(), 1, out.data(), 480);
            Assert::IsFalse(spatializer.IsQuiet());

            spatializer.Process(nullptr, 1, out.data(), spatializer.GetTailFrames());
            Assert::IsTrue(spatializer.IsQuiet());
        }
    };
}
