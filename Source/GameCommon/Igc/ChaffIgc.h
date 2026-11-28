/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ChaffIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CchaffIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ChaffIgc.h : Declaration of the CchaffIGC

#ifndef __CHAFFIGC_H_
#define __CHAFFIGC_H_

#include    "ModelIgc.h"

class CchaffIGC : public TmodelIGC<IchaffIGC>
{
    public:
        CchaffIGC(void);
        ~CchaffIGC(void);

    public:
    // IbaseIGC
	    HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override;
	    void                Terminate(void) override;
        void                Update(Time now) override;

        ObjectType          GetObjectType(void) const override
        {
            return OT_chaff;
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

            TmodelIGC<IchaffIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddModel(this);

            Release();
        }

        virtual void                 SetVisibleF(bool   vf) const
        {
        }

        bool                 GetVisibleF(void) const override
        {
            return true;
        }
        void                 SetRender(unsigned char render) override
        {
        }

    // IchaffIGC

    private:
        DataChaffTypeIGC*  m_pChaffTypeData;
        Time               m_timeExpire;
};

#endif //__CHAFFIGC_H_
