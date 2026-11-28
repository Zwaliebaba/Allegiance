//////////////////////////////////////////////////////////////////////////////
//
// Mip levels that keep a cut-out's coverage
//
// A cut-out texture (alpha 0 or 255 at full size: a model texture with
// colour-keyed holes) is drawn opaque where its filtered alpha is at least
// 0.5, by the 50% cut or by alpha to coverage around 0.5. Its mip levels are
// box-filtered, which averages a thin strut or a lattice below 0.5, so the
// detail thins out and vanishes with distance. Scaling each level's texels
// so that the share of them at or above 0.5 matches the full-size image
// keeps the cut-out's shape at every distance (I. Castano, "Computing Alpha
// Mipmaps", 2010).
//
// The texels are premultiplied BGRA (alpha in the top byte), so colour is
// scaled with alpha and the colour each texel stands for is unchanged.
// Textures with partial alpha at full size are translucent, not cut-outs,
// and are left alone.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace AlphaCoverage
{
    const float c_threshold = 127.5f;   // alpha 0.5, in bytes

    inline int Alpha(DWORD texel)
    {
        return (int)(texel >> 24);
    }

    //
    // Every alpha 0 or 255, and some of each
    //

    inline bool IsCutout(const DWORD* ptexels, int count)
    {
        bool bTransparent = false;
        bool bOpaque      = false;

        for (int index = 0; index < count; index++) {
            int alpha = Alpha(ptexels[index]);

            if (alpha == 0) {
                bTransparent = true;
            } else if (alpha == 255) {
                bOpaque = true;
            } else {
                return false;
            }
        }

        return bTransparent && bOpaque;
    }

    //
    // The share of texels whose alpha, times scale, is at least the threshold
    //

    inline float Coverage(const DWORD* ptexels, int count, float scale)
    {
        if (count <= 0) {
            return 0;
        }

        int covered = 0;

        for (int index = 0; index < count; index++) {
            if (Alpha(ptexels[index]) * scale >= c_threshold) {
                covered++;
            }
        }

        return (float)covered / (float)count;
    }

    //
    // The smallest scale at which the coverage exceeds (bStrict) or reaches
    // the target. The coverage only grows with the scale, so a bisection
    // finds it; above c_maxScale a texel would need alpha under 16 to count,
    // which is noise, not shape.
    //

    const float c_maxScale = 8.0f;

    inline float FirstScale(const DWORD* ptexels, int count, float target, bool bStrict)
    {
        float low  = 0.0f;
        float high = c_maxScale;

        for (int step = 0; step < 24; step++) {
            float mid      = (low + high) * 0.5f;
            float coverage = Coverage(ptexels, count, mid);

            if (bStrict ? coverage <= target : coverage < target) {
                low = mid;
            } else {
                high = mid;
            }
        }

        return high;
    }

    //
    // The scale for a level: the coverage is a step function of it, so of
    // the scales that give the step closest to the target take the middle
    // one, which leaves the deciding texels clear of the threshold either
    // way (the smallest would put them exactly on it, where alpha to
    // coverage covers half a pixel). A level whose coverage already is
    // that step keeps its texels.
    //

    inline float FindScale(const DWORD* ptexels, int count, float target)
    {
        float first    = FirstScale(ptexels, count, target, false);
        float coverage = Coverage(ptexels, count, first);
        float next     = FirstScale(ptexels, count, coverage, true);

        if (first <= 1.0f && 1.0f < next) {
            return 1.0f;
        }

        //
        // (Not far past the first, though: where no further step comes, the
        // middle would be far up, and scaled colour saturates.)
        //

        float scale = (first + next) * 0.5f;
        return scale < first * 1.25f ? scale : first * 1.25f;
    }

    inline void Scale(DWORD* ptexels, int count, float scale)
    {
        for (int index = 0; index < count; index++) {
            DWORD texel  = ptexels[index];
            DWORD result = 0;

            for (int shift = 0; shift < 32; shift += 8) {
                float channel = (float)((texel >> shift) & 0xff) * scale + 0.5f;
                result |= (DWORD)(channel >= 255.0f ? 255 : (int)channel) << shift;
            }

            ptexels[index] = result;
        }
    }

    //
    // Rescale the mip levels below a cut-out's full-size image. Returns
    // whether the texture was a cut-out.
    //

    inline bool PreserveCoverage(const DWORD* plevel0, int count0, std::vector<std::vector<DWORD>>& vlevels)
    {
        if (vlevels.empty() || !IsCutout(plevel0, count0)) {
            return false;
        }

        float target = Coverage(plevel0, count0, 1.0f);

        for (std::vector<DWORD>& level : vlevels) {
            int   count = (int)level.size();
            float scale = FindScale(level.data(), count, target);

            if (scale != 1.0f) {
                Scale(level.data(), count, scale);
            }
        }

        return true;
    }
}
