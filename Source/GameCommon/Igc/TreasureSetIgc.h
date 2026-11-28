/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:    TreasureSetIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CtreasureSetIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// TreasureSetIgc.h : Declaration of the CtreasureSetIGC

#ifndef __TREASURESETIGC_H_
#define __TREASURESETIGC_H_

/////////////////////////////////////////////////////////////////////////////
// CtreasureSetIGC
class CtreasureSetIGC : public ItreasureSetIGC
{
    public:
        CtreasureSetIGC(void)
        :
            m_data(NULL)
        {
        }

        ~CtreasureSetIGC(void)
        {
            delete [] (char*)m_data;
        }

    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
        void            Terminate(void) override
        {
            m_pMission->DeleteTreasureSet(this);
        }

        int             Export(void* data) const override;

        ObjectType      GetObjectType(void) const override
        {
            return OT_treasureSet;
        }

        ObjectID        GetObjectID(void) const override
        {
            return m_data->treasureSetID;
        }

    // ItreasureSetIGC
        const char*                 GetName(void) const override
        {
            return m_data->name;
        }
        bool                        GetZoneOnly(void) const override
        {
            return m_data->bZoneOnly;
        }

        void                        AddTreasureData(TreasureCode tc, ObjectID oid, unsigned char chance) override
        {
            if (m_data->nTreasureData == m_maxTreasureData)
            {
                //Not enough space to add one ... reallocate a larger array
                int oldSize = sizeof(DataTreasureSetIGC) + sizeof(TreasureData) *
                                                           m_maxTreasureData;
                const int c_increment = 10;
                int newSize = oldSize + sizeof(TreasureData) * c_increment;

                m_maxTreasureData += c_increment;

                DataTreasureSetIGC*    newData = (DataTreasureSetIGC*)(new char [newSize]);
                memcpy(newData, m_data, oldSize);
                delete [] (char*)m_data;

                m_data = newData;
            }

            TreasureData*   ptd = m_data->treasureData0() + m_data->nTreasureData;

            ptd->treasureCode = tc;
            ptd->treasureID = oid;
            ptd->chance = chance;

            m_totalTreasureChance += chance;
            m_data->nTreasureData++;
        }

        const TreasureData&         GetRandomTreasureData(void) const override
        {
            assert (m_totalTreasureChance >= 1);

            int r = randomInt(1, m_totalTreasureChance);

            TreasureData*   ptd = m_data->treasureData0();
            while (true)
            {
                r -= ptd->chance;

                if (r <= 0)
                    return *ptd;

                ptd++;

                assert (ptd < (m_data->treasureData0() + m_data->nTreasureData));
            }
        }

    private:
        ImissionIGC*            m_pMission;
        DataTreasureSetIGC*     m_data;
        short                   m_totalTreasureChance;
        short                   m_maxTreasureData;
};

#endif //__TREASURESETIGC_H_
