#include "pch.h"
#include "SoundMath.h"

//////////////////////////////////////////////////////////////////////////////
//
// The sound engine's arithmetic (soundengine/SoundMath.h): gains,
// DirectSound 3D's distance and cone attenuation as the XAudio2 engine
// reproduces them, the XAudio2 buffers that play a sound from a position,
// and a virtual sound's position tracker. See MODERNIZATION.md, "Sound:
// XAudio2 replaces DirectSound".
//
//////////////////////////////////////////////////////////////////////////////

using namespace SoundEngine;

namespace SoundTests
{
    void AreClose(float expected, float actual, const wchar_t* message = nullptr)
    {
        Assert::AreEqual(expected, actual, 0.001f, message);
    }

    void IsRegion(const VoiceRegion& region, VoiceRegion::Part part, unsigned nBegin, unsigned nLength, bool bLoop)
    {
        Assert::AreEqual((int)part, (int)region.m_part, L"part");
        Assert::AreEqual(nBegin, region.m_nBegin, L"begin");
        Assert::AreEqual(nLength, region.m_nLength, L"length");
        Assert::AreEqual(bLoop, region.m_bLoop, L"loop");
    }

    TEST_CLASS(Gains)
    {
    public:
        TEST_METHOD(DecibelsToAmplitude)
        {
            AreClose(1.0f, DbToAmplitude(0));
            AreClose(0.5f, DbToAmplitude(-6.0206f));
            AreClose(0.1f, DbToAmplitude(-20));
            AreClose(0.0f, DbToAmplitude(-100));     // DSBVOLUME_MIN: silence
        }

        // DirectSound 3D: full up to the minimum distance, then the inverse
        // of the distance scaled by the rolloff, silent from the maximum
        TEST_METHOD(DistanceAttenuationAsDirectSound)
        {
            AreClose(1.0f, DistanceAttenuation(5, 10, 1));
            AreClose(1.0f, DistanceAttenuation(10, 10, 1));
            AreClose(0.5f, DistanceAttenuation(20, 10, 1));                 // -6 dB per doubling
            AreClose(0.25f, DistanceAttenuation(40, 10, 1));
            AreClose(1.0f / 3, DistanceAttenuation(20, 10, 2));
            AreClose(1.0f, DistanceAttenuation(1000, 10, 0));               // no rolloff
            AreClose(0.0f, DistanceAttenuation(10 * c_fMinToMaxDistanceRatio, 10, 1));
            Assert::IsTrue(DistanceAttenuation(10 * c_fMinToMaxDistanceRatio - 1, 10, 1) > 0);
        }

        // The distance low-pass: open up to the minimum distance, a quarter of
        // the way down at ten times it, never below the floor
        TEST_METHOD(DistanceCutoffFallsWithDistance)
        {
            AreClose(c_fDistanceCutoffOpen, DistanceCutoff(5, 10));
            AreClose(c_fDistanceCutoffOpen, DistanceCutoff(10, 10));
            Assert::AreEqual(20000.0f * std::pow(0.1f, 0.6f), DistanceCutoff(100, 10), 1.0f);
            Assert::IsTrue(DistanceCutoff(50, 10) > DistanceCutoff(100, 10));
            AreClose(c_fDistanceCutoffFloor, DistanceCutoff(10 * c_fMinToMaxDistanceRatio, 10));
            AreClose(c_fDistanceCutoffOpen, DistanceCutoff(100, 0));        // no minimum distance: unfiltered
        }

        // The angles are full cone widths, as DirectSound took them
        TEST_METHOD(ConeGainAsDirectSound)
        {
            const float c_fCos60 = 0.5f;

            AreClose(0.0f, ConeGain(-1, 360, 360, -20));                     // no cone
            AreClose(0.0f, ConeGain(-1, 90, 180, 0));                        // no outside attenuation
            AreClose(0.0f, ConeGain(1, 90, 180, -20));                       // on the axis
            AreClose(0.0f, ConeGain(0.70711f, 90, 180, -20));                // at the inner edge (45)
            AreClose(-20.0f * 15 / 45, ConeGain(c_fCos60, 90, 180, -20));    // between: linear in dB
            AreClose(-20.0f, ConeGain(0, 90, 180, -20));                     // at the outer edge (90)
            AreClose(-20.0f, ConeGain(-1, 90, 180, -20));                    // behind
            AreClose(-20.0f, ConeGain(c_fCos60, 90, 90, -20));               // no fade zone
            AreClose(0.0f, ConeGain(1.0001f, 90, 180, -20));                 // rounding past 1
        }
    };

    TEST_CLASS(VoiceRegions)
    {
    public:
        TEST_METHOD(Normal)
        {
            auto regions = PlanVoiceRegions(100, 0, false, false, 0, 0, false);
            Assert::AreEqual(1, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Once, 0, 100, false);

            regions = PlanVoiceRegions(100, 40, false, false, 0, 0, false);
            IsRegion(regions[0], VoiceRegion::Once, 40, 60, false);

            Assert::IsTrue(PlanVoiceRegions(100, 100, false, false, 0, 0, false).empty());
        }

        // The rest of the current pass, then all of it looped
        TEST_METHOD(Looping)
        {
            auto regions = PlanVoiceRegions(100, 30, true, false, 0, 0, false);
            Assert::AreEqual(2, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Once, 30, 70, false);
            IsRegion(regions[1], VoiceRegion::Loop, 0, 100, true);
        }

        // Attack 0-20, sustain loop 20-70, release 70-100
        TEST_METHOD(AttackSustainRelease)
        {
            auto regions = PlanVoiceRegions(100, 0, false, true, 20, 50, false);
            Assert::AreEqual(3, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Attack, 0, 20, false);
            IsRegion(regions[1], VoiceRegion::SustainLoop, 20, 50, true);
            IsRegion(regions[2], VoiceRegion::Release, 70, 30, false);

            // from inside the loop: the rest of the pass first
            regions = PlanVoiceRegions(100, 30, false, true, 20, 50, false);
            Assert::AreEqual(3, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Sustain, 30, 40, false);
            IsRegion(regions[1], VoiceRegion::SustainLoop, 20, 50, true);
            IsRegion(regions[2], VoiceRegion::Release, 70, 30, false);

            regions = PlanVoiceRegions(100, 80, false, true, 20, 50, false);
            Assert::AreEqual(1, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Release, 80, 20, false);
        }

        // A stopped ASR sound skips the sustain
        TEST_METHOD(AttackSustainReleaseStopping)
        {
            auto regions = PlanVoiceRegions(100, 5, false, true, 20, 50, true);
            Assert::AreEqual(2, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Attack, 5, 15, false);
            IsRegion(regions[1], VoiceRegion::Release, 70, 30, false);

            regions = PlanVoiceRegions(100, 30, false, true, 20, 50, true);
            Assert::AreEqual(1, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Once, 30, 70, false);
        }

        TEST_METHOD(EmptyPartsAreLeftOut)
        {
            // no release
            auto regions = PlanVoiceRegions(70, 0, false, true, 20, 50, false);
            Assert::AreEqual(2, (int)regions.size());
            IsRegion(regions[1], VoiceRegion::SustainLoop, 20, 50, true);

            // no attack
            regions = PlanVoiceRegions(100, 0, false, true, 0, 50, false);
            IsRegion(regions[0], VoiceRegion::Sustain, 0, 50, false);

            // no loop: once through
            regions = PlanVoiceRegions(100, 0, false, true, 20, 0, false);
            Assert::AreEqual(1, (int)regions.size());
            IsRegion(regions[0], VoiceRegion::Once, 0, 100, false);
        }
    };

    // 1000 bytes at 1000 bytes per second, 2 bytes per sample
    TEST_CLASS(PositionTracker)
    {
    public:
        TEST_METHOD(PlaysToTheEnd)
        {
            BufferPositionTracker tracker(1000, 1000, 2);

            tracker.AddTime(500);
            Assert::AreEqual(500u, tracker.GetPosition());
            Assert::IsFalse(tracker.IsStopped());

            tracker.AddTime(600);
            Assert::IsTrue(tracker.IsStopped());
        }

        TEST_METHOD(LoopsUntilStopped)
        {
            BufferPositionTracker tracker(1000, 1000, 2, true);

            tracker.AddTime(1500);
            Assert::AreEqual(500u, tracker.GetPosition());
            Assert::IsFalse(tracker.IsStopped());

            // Stop ends it at once. (It set the position to the loop length,
            // which a non-ASR sound never set: a stopped sound with no voice
            // could get one, and be heard, after it was stopped.)
            tracker.Stop();
            Assert::IsTrue(tracker.IsStopped());
        }

        TEST_METHOD(SustainLoops)
        {
            BufferPositionTracker tracker(1000, 1000, 2, 200, 500);

            tracker.AddTime(800);
            Assert::AreEqual(300u, tracker.GetPosition());   // 800 wraps to 200 + 100
            Assert::IsFalse(tracker.IsStopping());
        }

        // A stop in the sustain plays on through the release
        TEST_METHOD(SoftStopPlaysTheRelease)
        {
            BufferPositionTracker tracker(1000, 1000, 2, 200, 500);

            tracker.AddTime(300);
            tracker.Stop();
            Assert::IsTrue(tracker.IsStopping());
            Assert::IsFalse(tracker.IsStopped());

            tracker.AddTime(500);
            Assert::AreEqual(800u, tracker.GetPosition());
            tracker.AddTime(300);
            Assert::IsTrue(tracker.IsStopped());
        }

        // A stop in the attack goes from its end straight to the release
        TEST_METHOD(SoftStopInTheAttackSkipsTheSustain)
        {
            BufferPositionTracker tracker(1000, 1000, 2, 200, 500);

            tracker.AddTime(100);
            tracker.Stop();
            tracker.AddTime(200);
            Assert::AreEqual(800u, tracker.GetPosition());
        }

        // (It set the position to the loop length, so a forced stop left
        // an ASR sound playing.)
        TEST_METHOD(ForcedStopEndsAnASRSound)
        {
            BufferPositionTracker tracker(1000, 1000, 2, 200, 500);

            tracker.AddTime(300);
            tracker.Stop(true);
            Assert::IsTrue(tracker.IsStopped());
        }

        TEST_METHOD(PositionsAreWholeSamples)
        {
            BufferPositionTracker tracker(1000, 1000, 4);

            tracker.AddTime(7);
            Assert::AreEqual(4u, tracker.GetPosition());
        }

        // (It divided by the loop length.)
        TEST_METHOD(EmptyLoop)
        {
            BufferPositionTracker tracker(1000, 1000, 2, 200, 0);

            tracker.AddTime(500);
            Assert::AreEqual(500u, tracker.GetPosition());
        }
    };
}
