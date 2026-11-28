#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// IGC's pure rules (Igc/common.cpp): the object type bits, the cluster
// warning a side's assets raise, and the lead a shot needs.
//
//////////////////////////////////////////////////////////////////////////////

namespace IgcTests
{
    TEST_CLASS(Typebits)
    {
    public:
        TEST_METHOD(EachTargetableTypeHasItsBit)
        {
            Assert::AreEqual((int)c_ttShip,     GetTypebits(OT_ship));
            Assert::AreEqual((int)c_ttStation,  GetTypebits(OT_station));
            Assert::AreEqual((int)c_ttMissile,  GetTypebits(OT_missile));
            Assert::AreEqual((int)c_ttMine,     GetTypebits(OT_mine));
            Assert::AreEqual((int)c_ttProbe,    GetTypebits(OT_probe));
            Assert::AreEqual((int)c_ttAsteroid, GetTypebits(OT_asteroid));
            Assert::AreEqual((int)c_ttWarp,     GetTypebits(OT_warp));
            Assert::AreEqual((int)c_ttTreasure, GetTypebits(OT_treasure));
            Assert::AreEqual((int)c_ttBuoy,     GetTypebits(OT_buoy));
        }

        TEST_METHOD(TheBitsAreDistinct)
        {
            int seen = 0;
            for (ObjectType ot = 0; ot <= OT_modelEnd; ot++)
            {
                int tt = c_ttTypebits[ot];
                Assert::AreEqual(0, seen & tt, Wide("object type " + std::to_string(ot)).c_str());
                seen |= tt;
            }
        }
    };

    TEST_CLASS(ClusterWarnings)
    {
    public:
        TEST_METHOD(NothingIsNoThreat)
        {
            Assert::AreEqual((int)c_cwNoThreat, (int)GetClusterWarning(0, false));
            Assert::AreEqual((int)c_cwNoThreat, (int)GetClusterWarning(c_amFighter | c_amStation, false));
        }

        TEST_METHOD(TheWorstThreatWins)
        {
            AssetMask am = c_amStation | c_amMiner | c_amEnemyFighter | c_amEnemyBomber | c_amEnemyAPC;
            Assert::AreEqual((int)c_cwStationCaptureThreat, (int)GetClusterWarning(am, false));
            Assert::AreEqual((int)c_cwStationThreatened,    (int)GetClusterWarning((AssetMask)(am & ~c_amEnemyAPC), false));
            Assert::AreEqual((int)c_cwMinerThreatened,      (int)GetClusterWarning(c_amMiner | c_amEnemyFighter, false));
            Assert::AreEqual((int)c_cwEnemyFighterInCluster, (int)GetClusterWarning(c_amEnemyFighter, false));
        }

        TEST_METHOD(InvulnerableStationsFearNoBombers)
        {
            Assert::AreEqual((int)c_cwNoThreat, (int)GetClusterWarning(c_amStation | c_amEnemyAPC, true));
            Assert::AreEqual((int)c_cwNoThreat, (int)GetClusterWarning(c_amStation | c_amEnemyBomber, true));
        }

        TEST_METHOD(EveryWarningHasText)
        {
            for (ClusterWarning cw = 0; cw < c_cwMax; cw++)
            {
                const char* sz = GetClusterWarningText(cw);
                Assert::IsNotNull(sz);
                Assert::IsTrue(*sz != '\0');
            }
            Assert::AreEqual("No threat", GetClusterWarningText(c_cwNoThreat));
            Assert::AreEqual("Station at risk of capture", GetClusterWarningText(c_cwStationCaptureThreat));
        }
    };

    TEST_CLASS(Impact)
    {
    public:
        TEST_METHOD(StationaryTarget)
        {
            Vector direction;
            float t = solveForImpact(Vector(100.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 0.0f), 10.0f, 0.0f, &direction);
            Assert::AreEqual(10.0f, t, 1e-4f);
            Assert::AreEqual(1.0f, direction.X(), 1e-5f);
            Assert::AreEqual(0.0f, direction.Y(), 1e-5f);
        }

        TEST_METHOD(TheTargetsRadiusShortensTheFlight)
        {
            // The shot hits when it is a radius away: 90 units at 10 per second
            Vector direction;
            float t = solveForImpact(Vector(100.0f, 0.0f, 0.0f), Vector(0.0f, 0.0f, 0.0f), 10.0f, 10.0f, &direction);
            Assert::AreEqual(9.0f, t, 1e-4f);
        }

        TEST_METHOD(InsideTheRadiusHitsNow)
        {
            Vector direction;
            Assert::AreEqual(0.0f, solveForImpact(Vector(0.0f, 5.0f, 0.0f), Vector(1.0f, 0.0f, 0.0f), 10.0f, 10.0f, &direction));
            Assert::AreEqual(1.0f, direction.Y(), 1e-5f);
        }

        TEST_METHOD(AFasterTargetMovingAwayCannotBeHit)
        {
            Vector direction;
            Assert::AreEqual(FLT_MAX, solveForImpact(Vector(100.0f, 0.0f, 0.0f), Vector(20.0f, 0.0f, 0.0f), 10.0f, 0.0f, &direction));
        }

        TEST_METHOD(LeadsACrossingTarget)
        {
            // Target 100 ahead crossing at 6, shot at 10: |(100, 6t)| = 10t gives t = 12.5
            Vector direction;
            float t = solveForImpact(Vector(100.0f, 0.0f, 0.0f), Vector(0.0f, 6.0f, 0.0f), 10.0f, 0.0f, &direction);
            Assert::AreEqual(12.5f, t, 1e-3f);
            Assert::AreEqual(0.8f, direction.X(), 1e-4f);
            Assert::AreEqual(0.6f, direction.Y(), 1e-4f);
        }
    };
}
