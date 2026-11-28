/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    WeaponIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CweaponIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// WeaponIgc.h : Declaration of the CweaponIGC

#ifndef __WEAPONIGC_H_
#define __WEAPONIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CweaponIGC
class CweaponIGC : public IweaponIGC
{
    public:
        CweaponIGC(void);

    // IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;
        void    Update(Time now) override
        {

            if (m_mountedFraction < 1.0f)
            {
                float dt = now - m_ship->GetLastUpdate();

                m_mountedFraction += dt * m_pMission->GetFloatConstant(c_fcidMountRate);
                if (m_mountedFraction < 1.0f)
                {
                    m_fFiringShot = false;
                    m_fFiringBurst = false;
                    return;
                }

                IIgcSite*   pigc = GetMission()->GetIgcSite();
                pigc->PlayNotificationSound(mountedSound, m_ship);
                pigc->PostNotificationText(m_ship, false, "%s ready.", GetPartType()->GetName());
                m_mountedFraction = 1.0f;
            }

            if ((!m_fActive) && fIsWeaponSelected())
                Activate();

            if (m_fActive)
            {
                FireWeapon(now);
            }
            else
            {
                m_fFiringShot = false;
                m_fFiringBurst = false;
            }
        }

        ObjectType  GetObjectType(void) const override
        {
            return OT_weapon;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

    // IpartIGC
        EquipmentType    GetEquipmentType(void) const override
        {
            return ET_Weapon;
        }

        IpartTypeIGC*    GetPartType(void) const override
        {
            return m_partType;
        }

        IshipIGC*        GetShip(void) const override
        {
            return m_ship;
        }
        void             SetShip(IshipIGC* newVal, Mount mount) override;

        Mount            GetMountID(void) const override
        {
            return m_mountID;
        }
        void             SetMountID(Mount newVal) override;

        bool             fActive(void) const override
        {
            return m_fActive;
        }

        void             Activate(void) override
        {
            assert (m_ship);

            if (!m_fActive)
            {
                m_ship->ChangeSignature(m_typeData->signature);

                m_nextFire = m_ship->GetLastUpdate() + m_typeData->dtimeBurst;

                m_fActive = true;
            }
        }
        void             Deactivate(void) override
        {
            assert (m_ship);

            if (m_fActive)
            {
                m_ship->ChangeSignature(-m_typeData->signature);
                m_fActive = false;
            }

            m_fFiringBurst = false;
            m_fFiringShot = false;  // REVIEW: this may lead to a silent shot
        }
        float            GetMass(void) const override
        {
            return m_typeData->mass;
        }

        Money            GetPrice(void) const override
        {
            return m_typeData->price;
        }

        float           GetMountedFraction(void) const override
        {
            return m_mountedFraction;
        }

        void            SetMountedFraction(float f) override
        {
            m_mountedFraction = f;
        }

        void            Arm(void) override
        {
            m_mountedFraction = 1.0f;
            m_nextFire = m_ship->GetLastUpdate();
        }

    // IweaponIGC
        IprojectileTypeIGC*  GetProjectileType(void) const override
        {
            return m_projectileType;
        }
        short                GetAmmoPerShot(void) const override
        {
            return m_typeData->cAmmoPerShot;
        }

        float               GetLifespan(void) const override
        {
            float   ls = m_projectileType->GetLifespan();
            if (m_ship && (m_typeData->energyPerShot != 0.0f))
                ls *= m_ship->GetSide()->GetGlobalAttributeSet().GetAttribute(c_gaLifespanEnergy);

            return ls;
        }

        const Vector&       GetPosition(void) const override
        {
            return *m_pposition;
        }

        float               GetDtBurst(void) const override
        {
            return m_typeData->dtimeBurst;
        }

        void                 SetGunner(IshipIGC* pship) override
        {
            Deactivate();

            if (m_pshipGunner)
                m_pshipGunner->Release();

            m_pshipGunner = pship;

            if (pship)
            {
                assert (m_ship);
                assert (pship->GetParentShip() == m_ship);
                pship->AddRef();

                assert (m_mountID >= 0);
                assert (pship->GetParentShip());
                assert (pship->GetParentShip()->GetHullType()->GetHardpointData(m_mountID).bFixed == false);
            }
        }

        IshipIGC*            GetGunner(void) const override
        {
            return m_pshipGunner;
        }

        bool                 fFiringShot(void) const override
        {
            return m_fFiringShot;
        }

        bool                 fFiringBurst(void) const override
        {
            return m_fFiringBurst;
        }

        SoundID              GetActivateSound(void) const override
        {
            return m_typeData->activateSound;
        }

        SoundID              GetSingleShotSound(void) const override
        {
            return m_typeData->singleShotSound;
        }

        SoundID              GetBurstSound(void) const override
        {
            return m_typeData->burstSound;
        }

    private:
        bool                        fIsWeaponSelected(void) const
        {
            bool    fSelected;
            if (m_ship->GetHullType()->GetHardpointData(m_mountID).bFixed)
            {
                int stateM = m_ship->GetStateM();

                fSelected = ((stateM & allWeaponsIGC) ||
                             ((stateM & oneWeaponIGC) &&
                              (m_mountID == ((stateM & selectedWeaponMaskIGC) >> selectedWeaponShiftIGC)))) &&
                            (m_ship->GetRipcordModel() == NULL);
            }
            else
            {
                fSelected = m_pshipGunner && (m_pshipGunner->GetStateM() & weaponsMaskIGC);
            }

            return fSelected;
        }

        void                        FireWeapon(Time   now);

        ImissionIGC*                m_pMission;

        IshipIGC*                   m_pshipGunner;

        const DataWeaponTypeIGC*    m_typeData;
        IpartTypeIGC*               m_partType;
        IshipIGC*                   m_ship;

        IprojectileTypeIGC*         m_projectileType;
        Time                        m_nextFire;

        const Vector*               m_pposition;
        const Orientation*          m_porientation;
        float                       m_mountedFraction;

        Mount                       m_mountID;
        bool                        m_fActive;
        bool                        m_fFiringShot;
        bool                        m_fFiringBurst;
};

#endif //__WEAPONIGC_H_
