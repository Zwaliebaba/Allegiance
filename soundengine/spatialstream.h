//
// spatialstream.h: the hand-over between XAudio2 and Windows Spatial Sound
// for one sound (see xa2objects.cpp). XAudio2's audio thread writes the
// sound's processed mono samples, with its position and gain, after each
// pass of its effect chain; the spatial audio render thread reads one
// object buffer's worth per pass. Plain C++, so the unit tests can drive it.
//
// The two engines run on the same device clock but in separate passes, so
// the reader keeps a little in hand: it waits until two of its buffers are
// queued before it starts (and again after running dry), and if more than
// six queue up it skips the oldest, so the delay stays between about one
// and six buffers (10 to 60 ms at the usual 10 ms pass).
//

#pragma once

#include <atomic>
#include <cstring>
#include <vector>

namespace SoundEngine {

struct ObjectPose
{
    float x, y, z;      // Windows Spatial Sound's frame: meters from the listener, +x right, +y up, +z behind
    float fGain;        // amplitude, 0 to 1
};

class ObjectStream
{
    std::vector<float>    m_samples;
    unsigned              m_nMask;
    std::atomic<unsigned> m_nWrite;     // written by the producer
    std::atomic<unsigned> m_nRead;      // written by the consumer
    bool                  m_bPrimed;    // consumer only

    // the pose, as of the latest write (fields are read one by one: a pose
    // that tears between two writes is a pose in between, which is fine)
    std::atomic<float>    m_x, m_y, m_z, m_fGain;

public:
    static const unsigned c_nPrimeBuffers = 2;
    static const unsigned c_nMaxBuffers   = 6;

    explicit ObjectStream(unsigned nCapacity = 16384) :
        m_nWrite(0),
        m_nRead(0),
        m_bPrimed(false),
        m_x(0), m_y(0), m_z(-1), m_fGain(0)
    {
        unsigned nSize = 1024;

        while (nSize < nCapacity) {
            nSize *= 2;
        }

        m_samples.assign(nSize, 0.0f);
        m_nMask = nSize - 1;
    }

    unsigned GetCapacity() const
    {
        return (unsigned)m_samples.size();
    }

    // Consumer side, while no producer is writing: back to empty
    void Clear()
    {
        m_nRead.store(m_nWrite.load(std::memory_order_acquire), std::memory_order_release);
        m_bPrimed = false;
        m_fGain.store(0);
    }

    //
    // Producer: append samples (one or two interleaved channels, mixed to
    // mono), and set the pose. The producer only writes free space: if the
    // consumer has stalled and the ring is full, the new samples are
    // dropped (the pose still moves).
    //

    void Write(const float* pIn, unsigned nChannels, unsigned nFrames, const ObjectPose& pose)
    {
        unsigned nWrite = m_nWrite.load(std::memory_order_relaxed);
        unsigned nRead  = m_nRead.load(std::memory_order_acquire);

        m_x.store(pose.x, std::memory_order_relaxed);
        m_y.store(pose.y, std::memory_order_relaxed);
        m_z.store(pose.z, std::memory_order_relaxed);
        m_fGain.store(pose.fGain, std::memory_order_relaxed);

        if ((nWrite - nRead) + nFrames > GetCapacity()) {
            return;
        }

        for (unsigned i = 0; i < nFrames; i++) {
            float f = 0;

            if (pIn) {
                f = nChannels == 2 ? 0.5f * (pIn[2 * i] + pIn[2 * i + 1]) : pIn[i];
            }

            m_samples[(nWrite + i) & m_nMask] = f;
        }

        m_nWrite.store(nWrite + nFrames, std::memory_order_release);
    }

    ObjectPose GetPose() const
    {
        return ObjectPose{ m_x.load(std::memory_order_relaxed), m_y.load(std::memory_order_relaxed),
                           m_z.load(std::memory_order_relaxed), m_fGain.load(std::memory_order_relaxed) };
    }

    // Consumer: how much is queued
    unsigned GetQueued() const
    {
        return m_nWrite.load(std::memory_order_acquire) - m_nRead.load(std::memory_order_relaxed);
    }

    //
    // Consumer: fill one buffer of nFrames. Returns the frames that came
    // from the stream; the rest of the buffer is silence.
    //

    unsigned Read(float* pOut, unsigned nFrames)
    {
        unsigned nWrite  = m_nWrite.load(std::memory_order_acquire);
        unsigned nRead   = m_nRead.load(std::memory_order_relaxed);
        unsigned nQueued = nWrite - nRead;

        //
        // Too far behind: drop the oldest, keeping the prime amount
        //

        if (nQueued > c_nMaxBuffers * nFrames) {
            nRead   = nWrite - c_nPrimeBuffers * nFrames;
            nQueued = c_nPrimeBuffers * nFrames;
        }

        if (!m_bPrimed && nQueued >= c_nPrimeBuffers * nFrames) {
            m_bPrimed = true;
        }

        unsigned nCopy = 0;

        if (m_bPrimed) {
            nCopy = nQueued < nFrames ? nQueued : nFrames;

            for (unsigned i = 0; i < nCopy; i++) {
                pOut[i] = m_samples[(nRead + i) & m_nMask];
            }

            if (nCopy < nFrames) {
                m_bPrimed = false;      // ran dry: build up a margin again
            }
        }

        if (nCopy < nFrames) {
            std::memset(pOut + nCopy, 0, (nFrames - nCopy) * sizeof(float));
        }

        m_nRead.store(nRead + nCopy, std::memory_order_release);
        return nCopy;
    }
};

} // namespace SoundEngine
