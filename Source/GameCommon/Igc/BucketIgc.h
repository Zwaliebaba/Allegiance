/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    BucketIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CbucketIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// BucketIgc.h : Declaration of the CbucketIGC

#ifndef __BUCKETIGC_H_
#define __BUCKETIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CbucketIGC
class       CbucketIGC : public IbucketIGC
{
    public:
        CbucketIGC(void)
        :
            m_investedMilliseconds(0),
            m_investedMoney(0),
            m_lastPercentCompleteNotified(0),
            m_pbucketPredecessor(nullptr)
        {
        }

    public:
    // IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void    Terminate(void) override;

        void    Update(Time now) override
        {
            assert (now >= m_lastUpdate);

            if (m_investedMoney > 0)
            {
                DWORD   ttbms = m_timeToBuild * 1000;
                if (m_investedMilliseconds < ttbms)
                {
                    m_investedMilliseconds += now.clock() - m_lastUpdate.clock();

                    if ((m_investedMoney >= m_price) && (m_investedMilliseconds >= ttbms))
                    {
                        ForceComplete(now);
                    }

                    assert (m_price > 0);
                    {
                        DWORD   mim = 1000 * ((m_investedMoney * m_timeToBuild) / m_price);
                        if (m_investedMilliseconds > mim)
                            m_investedMilliseconds = mim;
                    }

                    {
                        int p = GetPercentComplete();
                        if (m_lastPercentCompleteNotified != p)
                        {
                            m_pMission->GetIgcSite()->BucketChangeEvent(c_bcPercentCompleteChange, this);
                            m_lastPercentCompleteNotified = p;
                        }
                    }
                }
            }
            m_lastUpdate = now;
        }

        int         Export(void* data) const override;

        ObjectType  GetObjectType(void) const override
        {
            return OT_bucket;
        }

        short                   GetObjectID(void) const override
        {
            const short c_bucketMultiplier = 0x7fff / (1 + OT_bucketEnd - OT_bucketStart);
            assert (c_bucketMultiplier >= 500);

            return m_data.buyable->GetObjectID() +
                   (m_data.buyable->GetObjectType() - OT_bucketStart) * c_bucketMultiplier;
        }

        ImissionIGC*    GetMission(void) const override
        {
            return m_pMission;
        }

    // ItypeIGC
        const void*   GetData(void) const override
        {
            return m_data.buyable->GetData();
        }

    // IbuyableIGC
        const char*          GetModelName(void) const override
        {
            return m_data.buyable->GetModelName();
        }

        const char*          GetIconName(void) const override
        {
            return m_data.buyable->GetIconName();
        }

        const char*          GetName(void) const override
        {
            return m_data.buyable->GetName();
        }
        const char*          GetDescription(void) const override
        {
            return m_data.buyable->GetDescription();
        }
        Money                GetPrice(void) const override
        {
            return m_price;
        }

        const TechTreeBitMask&  GetRequiredTechs(void) const override
        {
            return m_data.buyable->GetRequiredTechs();
        }

        const TechTreeBitMask&  GetEffectTechs(void) const override
        {
            return m_data.buyable->GetEffectTechs();
        }
    
        DWORD                   GetTimeToBuild(void) const override
        {
            return m_timeToBuild;
        }

        BuyableGroupID          GetGroupID(void) const override
        {
            return m_data.buyable->GetGroupID();
        }

        // IbucketIGC
        IbuyableIGC*            GetBuyable(void) const override
        {
            return m_data.buyable;
        }
        IsideIGC*               GetSide(void) const override
        {
            return m_data.side;
        }

        void                    SetPrice(Money m) override
        {
            assert (m_investedMoney == 0);
            assert (m > 0);
            m_price = m;
        }

        int                     GetPercentBought(void) const override
        {
            assert (m_price > 0);
            return ((100 * m_investedMoney) / m_price);
        }
        int                     GetPercentComplete(void) const override
        {
            //m_investedMilliseconds
            return m_investedMilliseconds / (10 * m_timeToBuild);
        }

        bool                    GetCompleteF(void) const override
        {
            return m_investedMilliseconds >= (1000 * m_timeToBuild);
        }

        void                    ForceComplete(Time now) override
        {
            assert (m_data.side->CanBuy(m_data.buyable));

            switch(m_data.buyable->GetObjectType())
            {
                case OT_development:
                {
                    m_pMission->GetIgcSite()->DevelopmentCompleted(this, (IdevelopmentIGC*)(m_data.buyable), now);

                    m_investedMilliseconds = 1000 * m_timeToBuild;
                    m_investedMoney = m_price;
                }
                break;
                case OT_droneType:
                {
                    //Find a station where it can be built
                    IstationIGC*    pstation;
                    {
                        StationLinkIGC*    psl = m_data.side->GetStations()->first();
                        while (true)
                        {
                            assert (psl);
                            if (psl->data()->CanBuy(m_data.buyable))
                            {
                                pstation = psl->data();
                                break;
                            }

                            psl = psl->next();
                        }
                    }
                    m_pMission->GetIgcSite()->DroneTypeCompleted(this, pstation, (IdroneTypeIGC*)(m_data.buyable), now);

                    m_investedMoney = 0;
                    m_investedMilliseconds = 0;
                }
                break;
                case OT_stationType:
                {
                    //Find a station where it can be built
                    IstationIGC*    pstation;
                    {
                        StationLinkIGC*    psl = m_data.side->GetStations()->first();
                        while (true)
                        {
                            assert (psl);
                            if (psl->data()->CanBuy(m_data.buyable))
                            {
                                pstation = psl->data();
                                break;
                            }

                            psl = psl->next();
                        }
                    }
                    m_pMission->GetIgcSite()->StationTypeCompleted(this, pstation, (IstationTypeIGC*)(m_data.buyable), now);

                    m_investedMoney = 0;
                    m_investedMilliseconds = 0;
                }
                break;

                case OT_hullType:
                {
                    m_pMission->GetIgcSite()->HullTypeCompleted(m_data.side, (IhullTypeIGC*)(m_data.buyable));

                    m_investedMoney = 0;
                    m_investedMilliseconds = 0;
                }
                break;
                case OT_partType:
                {
                    m_pMission->GetIgcSite()->PartTypeCompleted(m_data.side, (IpartTypeIGC*)(m_data.buyable));

                    m_investedMoney = 0;
                    m_investedMilliseconds = 0;
                }
                break;
                default:
                    assert (false);
            }
        }

        DWORD                   GetTime(void) const override
        {
            return m_investedMilliseconds;
        }

        Money                   GetMoney(void) const override
        {
            return m_investedMoney;
        }

        void                    SetTimeAndMoney(DWORD dwTime, Money money) override
        {
            m_investedMilliseconds = dwTime;
            m_investedMoney = money;

            m_pMission->GetIgcSite()->BucketChangeEvent(c_bcMoneyChange, this);
        }
        Money                    AddMoney(Money m) override
        {
            {
                Money leftover = m_price - m_investedMoney - m;
                if (leftover < 0)
                    m += leftover;
            }

            m_investedMoney += m;
            m_pMission->GetIgcSite()->BucketChangeEvent(c_bcMoneyChange, this);

            return m;
        }
        void                    SetEmpty(void) override
        {
            m_investedMoney = 0;
            m_investedMilliseconds = 0;

            m_pMission->GetIgcSite()->BucketChangeEvent(c_bcEmptied, this);
        }

        ObjectType              GetBucketType(void) const override
        {
            return m_data.buyable->GetObjectType();
        }
        IbucketIGC*             GetPredecessor(void) const override
        {
            return m_pbucketPredecessor;
        }
    private:
        ImissionIGC*        m_pMission;
        DataBucketIGC       m_data;

        Time                m_lastUpdate;

        int                 m_lastPercentCompleteNotified;
        DWORD               m_investedMilliseconds;
        DWORD               m_timeToBuild;
        Money               m_investedMoney;
        Money               m_price;
        IbucketIGC*         m_pbucketPredecessor;
};

#endif //__BUCKETIGC_H_
