#include "pch.h"
#include "alphacoverage.h"

//////////////////////////////////////////////////////////////////////////////
//
// Mip levels that keep a cut-out's coverage (Engine/alphacoverage.h): a thin
// strut that box filtering averages below alpha 0.5 must still be drawn at a
// distance, and the texels' colour must not change.
//
//////////////////////////////////////////////////////////////////////////////

namespace AlphaCoverageTests
{
    using namespace AlphaCoverage;

    static DWORD Texel(int r, int g, int b, int a)
    {
        return ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
    }

    //
    // An 8x8 image with a one-texel vertical strut of premultiplied grey,
    // and its 4x4 box-filtered level: the strut averages to alpha 128 in
    // half the 2x2 blocks' columns... exactly, 255 / 2 = 127, below 0.5
    //

    static std::vector<DWORD> Strut8()
    {
        std::vector<DWORD> v(64, 0);
        for (int y = 0; y < 8; y++) {
            v[y * 8 + 3] = Texel(100, 100, 100, 255);
        }
        return v;
    }

    static std::vector<DWORD> BoxFilter(const std::vector<DWORD>& src, int size)
    {
        int half = size / 2;
        std::vector<DWORD> dst(half * half);

        for (int y = 0; y < half; y++) {
            for (int x = 0; x < half; x++) {
                DWORD result = 0;
                for (int shift = 0; shift < 32; shift += 8) {
                    int sum = 0;
                    for (int dy = 0; dy < 2; dy++) {
                        for (int dx = 0; dx < 2; dx++) {
                            sum += (src[(y * 2 + dy) * size + x * 2 + dx] >> shift) & 0xff;
                        }
                    }
                    result |= (DWORD)(sum / 4) << shift;
                }
                dst[y * half + x] = result;
            }
        }
        return dst;
    }

    TEST_CLASS(Coverage)
    {
    public:
        TEST_METHOD(CutoutsAreZeroOrOpaqueOnly)
        {
            std::vector<DWORD> strut = Strut8();
            Assert::IsTrue(IsCutout(strut.data(), (int)strut.size()));

            std::vector<DWORD> opaque(16, Texel(1, 2, 3, 255));
            Assert::IsFalse(IsCutout(opaque.data(), (int)opaque.size()));

            strut[0] = Texel(10, 10, 10, 128);    // a soft texel: translucent art
            Assert::IsFalse(IsCutout(strut.data(), (int)strut.size()));
        }

        TEST_METHOD(BoxFilteringLosesAThinStrut)
        {
            // The premise: the filtered strut falls below the cut
            std::vector<DWORD> level1 = BoxFilter(Strut8(), 8);
            Assert::AreEqual(0.0f, AlphaCoverage::Coverage(level1.data(), (int)level1.size(), 1.0f));
        }

        TEST_METHOD(TheStrutSurvivesTheMipLevels)
        {
            std::vector<DWORD> level0 = Strut8();
            std::vector<std::vector<DWORD>> levels;
            levels.push_back(BoxFilter(level0, 8));
            levels.push_back(BoxFilter(levels[0], 4));

            Assert::IsTrue(PreserveCoverage(level0.data(), (int)level0.size(), levels));

            // 1 in 8 of the full-size texels is the strut: a quarter of the 4x4
            // level's (one column of four), half of the 2x2 level's
            float target = AlphaCoverage::Coverage(level0.data(), (int)level0.size(), 1.0f);
            Assert::AreEqual(0.125f, target);
            Assert::AreEqual(0.25f, AlphaCoverage::Coverage(levels[0].data(), (int)levels[0].size(), 1.0f));
            Assert::AreEqual(0.5f, AlphaCoverage::Coverage(levels[1].data(), (int)levels[1].size(), 1.0f));
        }

        TEST_METHOD(TheDecidingTexelsClearTheThreshold)
        {
            // Not exactly at 0.5, where alpha to coverage would cover half a pixel
            std::vector<DWORD> level0 = Strut8();
            std::vector<std::vector<DWORD>> levels;
            levels.push_back(BoxFilter(level0, 8));
            PreserveCoverage(level0.data(), (int)level0.size(), levels);

            for (DWORD texel : levels[0]) {
                int alpha = Alpha(texel);
                Assert::IsTrue(alpha == 0 || alpha >= 140, Wide("alpha " + std::to_string(alpha)).c_str());
            }
        }

        TEST_METHOD(ColourIsScaledWithAlpha)
        {
            // Premultiplied: the colour a texel stands for stays the same
            std::vector<DWORD> level0 = Strut8();
            std::vector<std::vector<DWORD>> levels;
            levels.push_back(BoxFilter(level0, 8));
            PreserveCoverage(level0.data(), (int)level0.size(), levels);

            for (DWORD texel : levels[0]) {
                int alpha = Alpha(texel);
                if (alpha > 0) {
                    int red = (texel >> 16) & 0xff;
                    Assert::AreEqual(100.0 / 255.0, (double)red / alpha, 0.02);
                }
            }
        }

        TEST_METHOD(LevelsThatAlreadyMatchAreUntouched)
        {
            // A solid block: its levels are as covered as the full image
            std::vector<DWORD> level0(64, 0);
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    level0[y * 8 + x] = Texel(50, 60, 70, 255);
                }
            }
            std::vector<std::vector<DWORD>> levels;
            levels.push_back(BoxFilter(level0, 8));
            std::vector<DWORD> before = levels[0];

            PreserveCoverage(level0.data(), (int)level0.size(), levels);
            Assert::IsTrue(before == levels[0]);
        }

        TEST_METHOD(ClassifiesOpaqueCutoutAndTranslucent)
        {
            // What decides the translucent pass: the texture's own alpha
            std::vector<DWORD> opaque(100, Texel(1, 2, 3, 255));
            Assert::IsTrue(Classify(opaque.data(), 100) == Kind::Opaque);

            std::vector<DWORD> cutout = opaque;
            cutout[0] = 0;
            Assert::IsTrue(Classify(cutout.data(), 100) == Kind::Cutout);

            // A re-saved cut-out picks up a few soft texels at its edges: still a cut-out
            cutout[1] = Texel(1, 1, 1, 200);
            Assert::IsTrue(Classify(cutout.data(), 100) == Kind::Cutout);

            // Glass: much of it in between
            std::vector<DWORD> glass(100, Texel(20, 30, 40, 96));
            Assert::IsTrue(Classify(glass.data(), 100) == Kind::Translucent);

            // A soft edge round an opaque middle, more than 1% of the image
            std::vector<DWORD> soft = opaque;
            for (int index = 0; index < 5; index++) {
                soft[index] = Texel(1, 1, 1, 128);
            }
            Assert::IsTrue(Classify(soft.data(), 100) == Kind::Translucent);
        }

        TEST_METHOD(TranslucentArtIsLeftAlone)
        {
            std::vector<DWORD> level0(64, Texel(60, 60, 60, 128));
            std::vector<std::vector<DWORD>> levels;
            levels.push_back(BoxFilter(level0, 8));
            std::vector<DWORD> before = levels[0];

            Assert::IsFalse(PreserveCoverage(level0.data(), (int)level0.size(), levels));
            Assert::IsTrue(before == levels[0]);
        }
    };
}
