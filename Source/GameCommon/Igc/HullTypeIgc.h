/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    HullTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the ChullTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// HullTypeIgc.h : Declaration of the ChullTypeIGC

#ifndef __HULLTYPEIGC_H_
#define __HULLTYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// ChullTypeIGC
class       ChullTypeIGC : public IhullTypeIGC
{
    public:
        ChullTypeIGC(void)
        :
            m_data(NULL)
        {
        }

        ~ChullTypeIGC(void)
        {
            delete [] (char*)m_data;
        }

    public:
    // IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;

        int     Export(void* data) const override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_hullType;
        }

        ObjectID            GetObjectID(void) const override
        {
            return m_data->hullID;
        }

    // ItypeIGC
        const void*         GetData(void) const override
        {
            return m_data;
        }

    // IbuyableIGC
        const char*          GetModelName(void) const override
        {
            return m_data->modelName;
        }

        const char*          GetName(void) const override
        {
            return m_data->name;
        }
        const char*          GetDescription(void) const override
        {
            return m_data->description;
        }
        Money                GetPrice(void) const override
        {
            return m_data->price;
        }

        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_data->ttbmRequired;
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_data->ttbmEffects;
        }

        DWORD                   GetTimeToBuild(void) const override
        {
            return m_data->timeToBuild;
        }

        BuyableGroupID          GetGroupID(void) const override
        {
            return m_data->groupID;
        }

       // IhullTypeIGC
        float                GetMass(void) const override
        {
            return m_data->mass;
        }

        float               GetSignature(void) const override
        {
            return m_data->signature;
        }

        float                GetLength(void) const override
        {
            return m_data->length;
        }
        float                GetMaxSpeed(void) const override
        {
            return m_data->speed;
        }
        float                GetMaxTurnRate(Axis    axis) const override
        {
            assert (axis >= 0);
            assert (axis <= 3);
            return m_data->maxTurnRates[axis];
        }
        float                GetTurnTorque(Axis    axis) const override
        {
            assert (axis >= 0);
            assert (axis <= 3);
            return m_data->turnTorques[axis];
        }
        float                GetThrust(void) const override
        {
            return m_data->thrust;
        }
        float                GetSideMultiplier(void) const override
        {
            return m_data->sideMultiplier;
        }
        float                GetBackMultiplier(void) const override
        {
            return m_data->backMultiplier;
        }
        float                GetScannerRange(void) const override
        {
            return m_data->scannerRange;
        }

        float                GetMaxEnergy(void) const override
        {
            return m_data->maxEnergy;
        }
        float                GetRechargeRate(void) const override
        {
            return m_data->rechargeRate;
        }

        HitPoints            GetHitPoints(void) const override
        {
            return m_data->hitPoints;
        }
        DefenseTypeID       GetDefenseType(void) const override
        {
            return m_data->defenseType;
        }
        PartMask             GetPartMask(EquipmentType et, Mount  mountID) const override
        {
            assert (mountID >= 0);
            if (et == ET_Weapon)
            {
                return (mountID < m_data->maxWeapons)
                       ? (((HardpointData*)(((char*)m_data) + m_data->hardpointOffset))[mountID]).partMask
                       : 0;
            }
            else
            {
                return (mountID == 0) ? m_data->pmEquipment[et] : 0;
            }
        }

        short                GetCapacity(EquipmentType et) const override
        {
            short c;
            switch (et)
            {
                case ET_Magazine:
                    c = m_data->capacityMagazine;
                break;
                case ET_Dispenser:
                    c = m_data->capacityDispenser;
                break;
                case ET_ChaffLauncher:
                    c = m_data->capacityChaffLauncher;
                break;
                default:
                    c = 0;
            }

            return c;
        }

        bool                 CanMount(IpartTypeIGC* ppt, Mount  mountID) const override
        {
            assert (ppt);
            bool    bMountable;

            if (mountID < 0)
                bMountable = (mountID >= -c_maxCargo);
            else
            {
                EquipmentType   et = ppt->GetEquipmentType();
                bMountable = (ppt->GetPartMask() & ~GetPartMask(et, mountID)) == 0;
            }

            return bMountable;
        }            
        Mount                GetMaxWeapons(void) const override
        {
            return m_data->maxWeapons;
        }
        Mount                GetMaxFixedWeapons(void) const override
        {
            return m_data->maxFixedWeapons;
        }
        const HardpointData& GetHardpointData(Mount hardpointID) const override
        {
            assert ((hardpointID >= 0) && (hardpointID < m_data->maxWeapons));
            return ((HardpointData*)(((char*)m_data) + m_data->hardpointOffset))[hardpointID];
        }

        const char*          GetTextureName(void) const override
        {
            return m_data->textureName;
        }

        const char*          GetIconName(void) const override
        {
            return m_data->iconName;
        }

        HullAbilityBitMask   GetCapabilities(void) const override
        {
            return m_data->habmCapabilities;
        }

        bool                 HasCapability(HullAbilityBitMask habm) const override
        {
            return (m_data->habmCapabilities & habm) != 0;
        }

        const Vector&        GetCockpit(void) const override
        {
            return m_cockpit;
        }

        const Vector&        GetWeaponPosition(Mount mount) const override
        {
            assert (mount >= 0);
            assert (mount < m_data->maxWeapons);

            return m_positionWeapons[mount];
        }
        const Orientation&   GetWeaponOrientation(Mount mount) const override
        {
            assert (mount >= 0);
            assert (mount < m_data->maxWeapons);

            return m_orientationWeapons[mount];
        }

        float   GetMaxFuel(void) const override
        {
            return m_data->maxFuel;
        }

        float   GetECM(void) const override
        {
            return m_data->ecm;
        }

        float   GetRipcordSpeed(void) const override
        {
            return m_data->ripcordSpeed;
        }

        float   GetRipcordCost(void) const override
        {
            return m_data->ripcordCost;
        }

        short   GetMaxAmmo(void) const override
        {
            return m_data->maxAmmo;
        }

        IhullTypeIGC*   GetSuccessorHullType(void) const override
        {
            return m_phtSuccessor;
        }

        /*
        virtual const char*          GetPilotHUDName(void) const
        {
            return m_data->pilotHUDName;
        }
        virtual const char*          GetObserverHUDName(void) const
        {
            return m_data->observerHUDName;
        }
        */
    
        SoundID              GetInteriorSound(void) const override
        {
            return m_data->interiorSound;
        }
        SoundID              GetExteriorSound(void) const override
        {
            return m_data->exteriorSound;
        }

        SoundID              GetMainThrusterInteriorSound(void) const override
        {
            return m_data->mainThrusterInteriorSound;
        }
        SoundID              GetMainThrusterExteriorSound(void) const override
        {
            return m_data->mainThrusterExteriorSound;
        }

        SoundID              GetManuveringThrusterInteriorSound(void) const override
        {
            return m_data->manuveringThrusterInteriorSound;
        }
        SoundID              GetManuveringThrusterExteriorSound(void) const override
        {
            return m_data->manuveringThrusterExteriorSound;
        }
        const PartTypeListIGC*      GetPreferredPartTypes(void) const override
        {
            return &m_preferredPartTypes;
        }
        float GetScale() const override
        {
            return m_scale;
        }
        IObject*            GetIcon(void) const override
        {
            return m_poIcon;
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

    private:
        ImissionIGC*        m_pMission;
        TRef<IhullTypeIGC>  m_phtSuccessor;
        TRef<IObject>       m_poIcon;
        DataHullTypeIGC*    m_data;
        PartTypeListIGC     m_preferredPartTypes;
        Vector              m_cockpit;
        Vector              m_positionWeapons[c_maxMountedWeapons];
        Orientation         m_orientationWeapons[c_maxMountedWeapons];
        float               m_scale;

        int                 m_nLaunchSlots;
        Vector              m_positionLaunches[c_maxShipLaunchSlots];
        Vector              m_directionLaunches[c_maxShipLaunchSlots];

        int                 m_nLandSlots;
        int                 m_nCapLandSlots;
        int                 m_nLandPlanes[c_maxLandSlots];
        Vector              m_positionLandPlanes[c_maxShipLandSlots][c_maxLandPlanes];
        Vector              m_directionLandPlanes[c_maxShipLandSlots][c_maxLandPlanes];
};

#endif //__HULLTYPEIGC_H_
