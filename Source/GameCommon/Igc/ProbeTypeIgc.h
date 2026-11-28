/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    ProbeTypeIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CprobeTypeIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ProbeTypeIgc.h : Declaration of the CprobeTypeIGC

#ifndef __PROBETYPEIGC_H_
#define __PROBETYPEIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CprobeTypeIGC
class CprobeTypeIGC : public IprobeTypeIGC
{
    #ifdef _DEBUG_CprobeTypeIGC_AddRef_Release
        public:
            DWORD __stdcall  AddRef(void)
            {
                DWORD dwRefs = IprobeTypeIGC::AddRef();
                debugf("CprobeTypeIGC::AddRef(): m_count = %d\n", dwRefs);
                return dwRefs;
            }

            DWORD __stdcall  Release(void)
            {
                DWORD dwRefs = IprobeTypeIGC::Release();
                debugf("CprobeTypeIGC::Release(): m_count = %d\n", dwRefs);
                return dwRefs;
            }
    #endif // _DEBUG

    public:
        CprobeTypeIGC(void)
        :
            m_projectileType(NULL)
        {
        }

        ~CprobeTypeIGC(void)
        {
            assert (m_projectileType == NULL);
        }

    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override
        {
            if (m_projectileType)
            {
                m_projectileType->Release();
                m_projectileType = NULL;
            }

            m_pMission->DeleteExpendableType(this);
        }
        void            Update(Time now) override
        {
        }

        int             Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_probeType;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data.expendabletypeID;
        }

    // ItypeIGC
        const void*     GetData(void) const override
        {
            return &m_data;
        }

    // IexpendableTypeIGC
        const LauncherDef*  GetLauncherDef(void) const override
        {
            return &(m_data.launcherDef);
        }
        EquipmentType       GetEquipmentType(void) const override
        {
            return ET_Dispenser;
        }
        float           GetLoadTime(void) const override
        {
            return m_data.loadTime;
        }
        float               GetMass(void) const override
        {
            return m_data.launcherDef.mass;
        }
        float           GetLifespan(void) const override
        {
            return m_data.lifespan;
        }

        float           GetSignature(void) const override
        {
            return m_data.signature;
        }
        HitPoints       GetHitPoints(void) const override
        {
            return m_data.hitPoints;
        }
        DefenseTypeID   GetDefenseType(void) const override
        {
            return m_data.defenseType;
        }
        char const*     GetName(void) const override
        {
            return m_data.launcherDef.name;
        }
        char const*     GetModelName(void) const override
        {
            return m_data.modelName;
        }
        char const*     GetModelTexture(void) const override
        {
            return m_data.textureName;
        }
        float           GetRadius(void) const override
        {
            return m_data.radius;
        }

        ExpendableAbilityBitMask    GetCapabilities(void) const override
        {
            return m_data.eabmCapabilities;
        }

        bool                        HasCapability(ExpendableAbilityBitMask eabm) const override
        {
            return (m_data.eabmCapabilities & eabm) != 0;
        }

        // IprobeTypeIGC
        float           GetScannerRange(void) const override
        {
            return m_data.scannerRange;
        }

        float               GetDtBurst(void) const override
        {
            return m_data.dtimeBurst;
        }

        float               GetDispersion(void) const override
        {
            return m_data.dispersion;
        }

        float               GetAccuracy(void) const override
        {
            return m_data.accuracy;
        }

        short               GetAmmo(void) const override
        {
            return m_data.ammo;
        }

        IprojectileTypeIGC* GetProjectileType(void) const override
        {
            return m_projectileType;
        }
        const Vector&       GetEmissionPt(void) const override
        {
            return m_emissionPt;
        }
        float               GetRipcordDelay(void) const override
        {
            return m_data.dtRipcord;
        }

        SoundID             GetAmbientSound(void) const override
        {
            return m_data.ambientSound;
        }

    private:
        ImissionIGC*        m_pMission;
        DataProbeTypeIGC    m_data;
        Vector              m_emissionPt;
        IprojectileTypeIGC* m_projectileType;
};

#endif //__PROBETYPEIGC_H_