#include "pch.h"
#include "spatialstream.h"

#include <thread>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
//
// The hand-over from XAudio2 to a Windows Spatial Sound audio object
// (soundengine/spatialstream.h): the margin the reader builds up, running
// dry, falling behind, the mono mix, and a writer and reader on two threads.
// See MODERNIZATION.md, "Sound: 3D audio for headphones".
//
//////////////////////////////////////////////////////////////////////////////

using namespace SoundEngine;

namespace SpatialStreamTests
{
    const unsigned c_nFrames = 480;
    const ObjectPose c_pose  = { 1, 2, 3, 0.5f };

    // n frames counting up from first
    std::vector<float> Ramp(unsigned first, unsigned n)
    {
        std::vector<float> v(n);

        for (unsigned i = 0; i < n; i++) {
            v[i] = (float)(first + i);
        }

        return v;
    }

    TEST_CLASS(Margin)
    {
    public:
        // Nothing comes out until two buffers are queued, then it flows
        TEST_METHOD(WaitsForTwoBuffers)
        {
            ObjectStream       stream;
            std::vector<float> out(c_nFrames);

            stream.Write(Ramp(0, c_nFrames).data(), 1, c_nFrames, c_pose);
            Assert::AreEqual(0u, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual(0.0f, out[0]);

            stream.Write(Ramp(c_nFrames, c_nFrames).data(), 1, c_nFrames, c_pose);
            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual(0.0f, out[0]);
            Assert::AreEqual((float)(c_nFrames - 1), out[c_nFrames - 1]);

            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual((float)c_nFrames, out[0]);
        }

        // Running dry gives what there is, then silence, and waits for the
        // margin again
        TEST_METHOD(RunningDryRebuildsTheMargin)
        {
            ObjectStream       stream;
            std::vector<float> out(c_nFrames);

            stream.Write(Ramp(0, 2 * c_nFrames + 100).data(), 1, 2 * c_nFrames + 100, c_pose);
            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual(100u, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual((float)(2 * c_nFrames), out[0]);
            Assert::AreEqual(0.0f, out[100]);

            stream.Write(Ramp(0, c_nFrames).data(), 1, c_nFrames, c_pose);
            Assert::AreEqual(0u, stream.Read(out.data(), c_nFrames));
        }

        // More than six buffers behind: the oldest are dropped, two kept
        TEST_METHOD(FallingBehindDropsTheOldest)
        {
            ObjectStream       stream;
            std::vector<float> out(c_nFrames);
            unsigned           n = 10 * c_nFrames;

            stream.Write(Ramp(0, n).data(), 1, n, c_pose);
            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual((float)(n - 2 * c_nFrames), out[0]);
            Assert::AreEqual(c_nFrames, stream.GetQueued());
        }

        // Clear empties it and waits for the margin again
        TEST_METHOD(ClearEmpties)
        {
            ObjectStream       stream;
            std::vector<float> out(c_nFrames);

            stream.Write(Ramp(0, 3 * c_nFrames).data(), 1, 3 * c_nFrames, c_pose);
            stream.Clear();
            Assert::AreEqual(0u, stream.GetQueued());
            Assert::AreEqual(0.0f, stream.GetPose().fGain);
        }
    };

    TEST_CLASS(Contents)
    {
    public:
        TEST_METHOD(StereoIsMixedToMono)
        {
            ObjectStream stream;
            float        in[4] = { 1.0f, 0.0f, 0.25f, 0.75f };
            float        out[2];

            stream.Write(in, 2, 2, c_pose);
            stream.Write(in, 2, 2, c_pose);     // the margin for a two-frame read
            Assert::AreEqual(2u, stream.Read(out, 2));
            Assert::AreEqual(0.5f, out[0]);
            Assert::AreEqual(0.5f, out[1]);
        }

        TEST_METHOD(PoseIsTheLatest)
        {
            ObjectStream stream;
            float        in[1] = { 0 };

            stream.Write(in, 1, 1, ObjectPose{ 1, 1, 1, 1 });
            stream.Write(in, 1, 1, c_pose);

            ObjectPose pose = stream.GetPose();
            Assert::AreEqual(1.0f, pose.x);
            Assert::AreEqual(2.0f, pose.y);
            Assert::AreEqual(3.0f, pose.z);
            Assert::AreEqual(0.5f, pose.fGain);
        }

        // Silence in (no buffer) is written as zeros
        TEST_METHOD(SilenceIsZeros)
        {
            ObjectStream       stream;
            std::vector<float> out(c_nFrames, 1.0f);

            stream.Write(nullptr, 1, 2 * c_nFrames, c_pose);
            Assert::AreEqual(c_nFrames, stream.Read(out.data(), c_nFrames));
            Assert::AreEqual(0.0f, out[0]);
            Assert::AreEqual(0.0f, out[c_nFrames - 1]);
        }
    };

    TEST_CLASS(Threads)
    {
    public:
        // A writer and a reader on two threads: every buffer read is a run
        // of consecutive samples (nothing torn or reordered)
        TEST_METHOD(WriterAndReaderAgree)
        {
            ObjectStream      stream;
            std::atomic<bool> bDone(false);

            std::thread writer([&]() {
                unsigned next = 0;

                for (int pass = 0; pass < 2000; pass++) {
                    std::vector<float> v = Ramp(next, c_nFrames);
                    stream.Write(v.data(), 1, c_nFrames, c_pose);
                    next += c_nFrames;

                    if ((pass & 7) == 0) {
                        std::this_thread::yield();
                    }
                }

                bDone = true;
            });

            std::vector<float> out(c_nFrames);
            bool               bOrdered = true;
            unsigned           nTotal   = 0;

            while (!bDone || stream.GetQueued() >= c_nFrames) {
                unsigned n = stream.Read(out.data(), c_nFrames);

                for (unsigned i = 1; i < n; i++) {
                    bOrdered = bOrdered && out[i] == out[i - 1] + 1;
                }

                nTotal += n;
                std::this_thread::yield();
            }

            writer.join();

            Assert::IsTrue(bOrdered);
            Assert::IsTrue(nTotal > 0);
        }
    };
}
