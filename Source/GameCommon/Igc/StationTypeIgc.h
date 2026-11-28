/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    StationTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CstationTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// StationTypeIgc.h : Declaration of the CstationTypeIGC

#ifndef __STATIONTYPEIGC_H_
#define __STATIONTYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CstationTypeIGC
class       CstationTypeIGC : public IstationTypeIGC
{
    public:
        CstationTypeIGC(void)
        :
            m_pstSuccessor(nullptr)
        {
        }

        ~CstationTypeIGC(void)
        {
        }

        /*
        DWORD __stdcall  AddRef(void)
        {
            return IstationTypeIGC::AddRef();
        }

        DWORD __stdcall  Release(void)
        {
            return IstationTypeIGC::Release();
        }
        */

    public:
    // IbaseIGC
        HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void                Terminate(void) override;

        int                 Export(void* data) const override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_stationType;
        }

        ObjectID            GetObjectID(void) const override
        {
            return m_data.stationTypeID;
        }

    // ItypeIGC
        const void*         GetData(void) const override
        {
            return &m_data;
        }

    // IbuyableIGC
        const char*          GetModelName(void) const override
        {
            return m_data.modelName;
        }

        const char*          GetName(void) const override
        {
            return m_data.name;
        }
        const char*          GetDescription(void) const override
        {
            return m_data.description;
        }
        Money                GetPrice(void) const override
        {
            return m_data.price;
        }
        BuyableGroupID       GetGroupID(void) const override 
        { 
            return m_data.groupID; 
        };

        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_data.ttbmRequired;
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_data.ttbmEffects;
        }

        DWORD                   GetTimeToBuild(void) const override
        {
            return m_data.timeToBuild;
        }

        // IstationTypeIGC
        float               GetSignature(void) const override
        {
            return m_data.signature;
        }

        float                GetRadius(void) const override
        {
            return m_data.radius;
        }

        float                GetScannerRange(void) const override
        {
            return m_data.scannerRange;
        }

        HitPoints            GetMaxArmorHitPoints(void) const override
        {
            return m_data.maxArmorHitPoints;
        }

        DefenseTypeID        GetArmorDefenseType(void) const override
        {
            return m_data.defenseTypeArmor;
        }

        HitPoints                GetMaxShieldHitPoints(void) const override
        {
            return m_data.maxShieldHitPoints;
        }

        DefenseTypeID        GetShieldDefenseType(void) const override
        {
            return m_data.defenseTypeShield;
        }

        float                GetArmorRegeneration(void) const override
        {
            return m_data.armorRegeneration;
        }

        float                GetShieldRegeneration(void) const override
        {
            return m_data.shieldRegeneration;
        }

        Money                GetIncome(void) const override
        {
            return m_data.income;
        }

        const TechTreeBitMask&  GetLocalTechs(void) const override
        {
            return m_data.ttbmLocal;
        }

        StationAbilityBitMask   GetCapabilities(void) const override
        {
            return m_data.sabmCapabilities;
        }
        bool                            HasCapability(StationAbilityBitMask sabm) const override
        {
            return (m_data.sabmCapabilities & sabm) != 0;
        }
        const char*             GetTextureName(void) const override
        {
            return m_data.textureName;
        }
        const char*             GetBuilderName(void) const override
        {
            return m_data.builderName;
        }
        const char*             GetIconName(void) const override
        {
            return m_data.iconName;
        }

        IstationTypeIGC*        GetSuccessorStationType(const IsideIGC*   pside) override
        {
            //The successor can be built if:
            //    we have one, and
            //    the required bits are a subset of the side tech bits and the required bits for this station
            //    (on the logic that, if we have this station, we had the tech bits at one time)
            if (m_pstSuccessor != nullptr)
            {
                if (m_pstSuccessor->GetRequiredTechs() <= (m_data.ttbmRequired |
                                                           pside->GetDevelopmentTechs()))
                    return m_pstSuccessor->GetSuccessorStationType(pside);
            }

            return this;
        }
        AsteroidAbilityBitMask   GetBuildAABM(void) const override
        {
            return m_data.aabmBuild;
        }

        int                     GetLaunchSlots(void) const override
        {
            return m_nLaunchSlots;
        }
        const Vector&           GetLaunchPosition(int   slotID) const override
        {
            assert (slotID >= 0);
            assert (slotID < m_nLaunchSlots);

            return m_positionLaunches[slotID];
        }
        const Vector&           GetLaunchDirection(int   slotID) const override
        {
            assert (slotID >= 0);
            assert (slotID < m_nLaunchSlots);

            return m_directionLaunches[slotID];
        }

        int                     GetLandSlots(void) const override
        {
            return m_nLandSlots;
        }
        int                     GetCapLandSlots(void) const override
        {
            return m_nCapLandSlots;
        }
        int                     GetLandPlanes(int   slotID) const override
        {
            assert (slotID >= 0);
            assert (slotID < m_nLandSlots);

            return m_nLandPlanes[slotID];
        }
        const Vector&           GetLandPosition(int slotID, int planeID) const override
        {
            assert (slotID >= 0);
            assert (slotID < m_nLandSlots);
            assert (planeID >= 0);
            assert (planeID <= m_nLandPlanes[slotID]);

            return m_positionLandPlanes[slotID][planeID];
        }
        const Vector&           GetLandDirection(int slotID, int planeID) const override
        {
            assert (slotID >= 0);
            assert (slotID < m_nLandSlots);
            assert (planeID >= 0);
            assert (planeID <= m_nLandPlanes[slotID]);

            return m_directionLandPlanes[slotID][planeID];
        }

        SoundID                 GetInteriorSound() const override
        {
            return m_data.interiorSound;
        }
        SoundID                 GetInteriorAlertSound() const override
        {
            return m_data.interiorAlertSound;
        }
        SoundID                 GetExteriorSound() const override
        {
            return m_data.exteriorSound;
        }

        SoundID                 GetConstructorNeedRockSound() const override
        {
            return m_data.constructorNeedRockSound;
        }

        SoundID                 GetConstructorUnderAttackSound() const override
        {
            return m_data.constructorUnderAttackSound;
        }

        SoundID                 GetConstructorDestroyedSound() const override
        {
            return m_data.constructorDestroyedSound;
        }

        SoundID                 GetCompletionSound() const override
        {
            return m_data.completionSound;
        }

        SoundID                 GetUnderAttackSound() const override
        {
            return m_data.underAttackSound;
        }

        SoundID                 GetCriticalSound() const override
        {
            return m_data.criticalSound;
        }

        SoundID                 GetDestroyedSound() const override
        {
            return m_data.destroyedSound;
        }

        SoundID                 GetCapturedSound() const override
        {
            return m_data.capturedSound;
        }

        SoundID                 GetEnemyCapturedSound() const override
        {
            return m_data.enemyCapturedSound;
        }

        SoundID                 GetEnemyDestroyedSound() const override
        {
            return m_data.enemyDestroyedSound;
        }

        StationClassID          GetClassID() const override
        {
            return m_data.classID;
        }

        IdroneTypeIGC*              GetConstructionDroneType(void) const override
        {
            return m_pdtConstruction;
        }

    private:
        ImissionIGC*        m_pMission;
        DataStationTypeIGC  m_data;

        IdroneTypeIGC*      m_pdtConstruction;

        IstationTypeIGC*    m_pstSuccessor;

        int                 m_nLaunchSlots;
        Vector              m_positionLaunches[c_maxLaunchSlots];
        Vector              m_directionLaunches[c_maxLaunchSlots];

        int                 m_nLandSlots;
        int                 m_nCapLandSlots;
        int                 m_nLandPlanes[c_maxLandSlots];
        Vector              m_positionLandPlanes[c_maxLandSlots][c_maxLandPlanes];
        Vector              m_directionLandPlanes[c_maxLandSlots][c_maxLandPlanes];
};

#endif //__STATIONTYPEIGC_H_
