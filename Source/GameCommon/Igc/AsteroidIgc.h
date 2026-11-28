/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	AsteroidIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CasteroidIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// AsteroidIgc.h : Declaration of the CasteroidIGC

#ifndef __ASTEROIDIGC_H_
#define __ASTEROIDIGC_H_

#include    "ModelIgc.h"

class   CasteroidIGC : public TmodelIGC<IasteroidIGC>
{
    public:
        CasteroidIGC(void)
        :
            m_fraction(1.0f)
        {
        }

    // IbaseIGC
	    HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override
        {
            if (m_pbuildingEffect)
            {
                //The building effect gets terminated along with the asteroid
                m_pbuildingEffect->Terminate();
                assert (m_pbuildingEffect == NULL);
            }

            GetMyMission()->DeleteAsteroid(this);
            TmodelIGC<IasteroidIGC>::Terminate();
        }
        int     Export(void*    data) const override;

        void    Update(Time now) override
        {
            if (((m_asteroidDef.aabmCapabilities & c_aabmMineHe3) != 0) &&
                (m_asteroidDef.ore < m_asteroidDef.oreMax))
            {
                float   dOre = (now - GetMyLastUpdate()) * GetMyMission()->GetFloatConstant(c_fcidHe3Regeneration);

                dOre *= (m_asteroidDef.oreMax - m_asteroidDef.ore);
                m_asteroidDef.ore += dOre;
            }

            TmodelIGC<IasteroidIGC>::Update(now);

            //If there is an active building effect ... call the building effect to do the 
            //update so that ... if the building effect completes ... it is being done from
            //the asteroid update
            if (m_pbuildingEffect)
                m_pbuildingEffect->AsteroidUpdate(now);
        }

        ObjectType  GetObjectType(void) const override
        {
            return OT_asteroid;
        }

        ObjectID    GetObjectID(void) const override
        {
            return m_asteroidDef.asteroidID;
        }

    // ImodelIGC
        void    SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            //Overrride the model's cluster method so that we can maintain the cluster's asteroid list
            //(as well as letting the model maintain its model list)
            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteAsteroid(this);
            }

            TmodelIGC<IasteroidIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddAsteroid(this);

            Release();
        }

    // IdamageIGC
        DamageResult        ReceiveDamage(DamageTypeID            type,
                                                  float                   amount,
                                                  Time                    timeCollision,
                                                  const Vector&           position1,
                                                  const Vector&           position2,
                                                  ImodelIGC*              launcher) override
        {
            DamageResult    dr = c_drHullDamage;

            amount *= GetMyMission()->GetDamageConstant(type, c_defidAsteroid);

            if (amount < 0.0f)
            {
                m_fraction -= amount / float(m_asteroidDef.hitpoints);
                if (m_fraction > 1.0f)
                    m_fraction = 1.0f;
            }
            else if (amount != 0.0f)
            {
                float   oldFraction = m_fraction;

                assert (m_asteroidDef.hitpoints != 0);
                m_fraction -= amount / float(m_asteroidDef.hitpoints);

                GetMission()->GetIgcSite()->PlaySoundEffect(rockHitSound, this,
                    GetOrientation().TimesInverse(position2 - position1));

                if (m_fraction <= 0.0f)
                {
                    m_fraction = 0.0f;
                    if (oldFraction > 0.0f)
                    {
                        GetMyMission()->GetIgcSite()->KillAsteroidEvent(this, true);
                        dr = c_drKilled;
                    }
                }
                
                return ((type & c_dmgidNoDebris) == 0);
            }

            return dr;
        }
        float   GetFraction(void) const override
        {
            return m_fraction;
        }
        void    SetFraction(float newVal) override
        {
            m_fraction = newVal;
        }

        float               GetHitPoints(void) const override
        {
            return m_fraction * float(m_asteroidDef.hitpoints);
        }

    // IasteroidIGC
        float   GetOre(void) const override
        {
            return m_asteroidDef.ore;
        }
        void    SetOre(float    newVal) override
        {
            /* NYI -- re-enable if He3 does not regenerate
            if ((newVal == 0.0f) &&
                ((m_asteroidDef.aabmCapabilities & (c_aabmMineHe3 | c_aabmMineGold)) != 0))
            {
                m_asteroidDef.aabmCapabilities &= ~(c_aabmMineHe3 | c_aabmMineGold);
                char    bfr[c_cbName];
                Bounded::Copy(bfr, GetName());
                SetSecondaryName(bfr);

                const AsteroidDef&  ad = IasteroidIGC::GetTypeDefaults(m_asteroidDef.aabmCapabilities);

                SetIcon(GetMyMission()->GetIgcSite()->LoadRadarIcon(ad.iconName));
            }
            */

            m_asteroidDef.ore = newVal;
        }
        float   MineOre(float    newVal) override
        {
            if (m_asteroidDef.ore < newVal)
            {
                newVal = m_asteroidDef.ore;
                m_asteroidDef.ore = 0.0f;

                /* NYI -- re-enable if He3 does not regenerate
                GetMyMission()->GetIgcSite()->DrainAsteroidEvent(this);
                */
            }
            else
                m_asteroidDef.ore -= newVal;

            return newVal;
        }

        AsteroidAbilityBitMask  GetCapabilities(void) const override
        {
            return m_pbuildingEffect ? 0 : m_asteroidDef.aabmCapabilities;
        }
        bool                    HasCapability(AsteroidAbilityBitMask aabm) const override
        {
            return m_pbuildingEffect ? false : ((m_asteroidDef.aabmCapabilities & aabm) != 0);
        }

        IbuildingEffectIGC*     GetBuildingEffect(void) const override
        {
            return m_pbuildingEffect;
        }
        void                    SetBuildingEffect(IbuildingEffectIGC* pbe) override
        {
            if (m_pbuildingEffect)
            {
                assert (pbe == NULL);
                m_pbuildingEffect->MakeUnhitable();
            }

            m_pbuildingEffect = pbe;
        }

    private:
        AsteroidDef                 m_asteroidDef;
        float                       m_fraction;
        TRef<IbuildingEffectIGC>    m_pbuildingEffect;
};

#endif //__ASTEROIDIGC_H_
