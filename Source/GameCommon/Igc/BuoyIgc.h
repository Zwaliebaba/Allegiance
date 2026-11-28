/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	BuoyIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CbuoyIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// BuoyIgc.h : Declaration of the CbuoyIGC

#ifndef __BUOYIGC_H_
#define __BUOYIGC_H_

#include    "ModelIgc.h"

class CbuoyIGC : public TmodelIGC<IbuoyIGC>
{
    public:
        CbuoyIGC(void);
        ~CbuoyIGC(void);

    public:
    // IbaseIGC
	    HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
	    void                Terminate(void) override;
        int                 Export(void* data) const override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_buoy;
        }

        ObjectID    GetObjectID(void) const override
        {
            return m_buoyID;
        }

    // ImodelIGC
        void                SetCluster(IclusterIGC* cluster) override
        {
            AddRef();

            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteModel(this);
            }

            TmodelIGC<IbuoyIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddModel(this);

            Release();
        }

        virtual void                 SetVisibleF(bool   vf) const
        {
        }

        bool                 GetVisibleF(void) const override
        {
            return (m_nConsumers > 0);
        }
        void                 SetRender(unsigned char render) override
        {
        }

    // IbuoyIGC
        void                 AddConsumer(void) override
        {
            m_nConsumers++;
        }
        void                 ReleaseConsumer(void) override
        {
            m_nConsumers--;
            assert (m_nConsumers >= 0);

            if (m_nConsumers == 0)
                Terminate();
        }

        BuoyType             GetBuoyType(void) override
        {
            return m_type;
        }

    private:
        short    m_nConsumers;
        BuoyID   m_buoyID;
        BuoyType m_type;
};

#endif //__BUOYIGC_H_
