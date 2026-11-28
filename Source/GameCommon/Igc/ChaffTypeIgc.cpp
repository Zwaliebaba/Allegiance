/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ChaffTypeIgc.cpp
**
**  Author: 
**
**  Description:
**      Implementation of the CchaffTypeIGC class. This file was initially created by
**  the ATL wizard for the core object.
**
**  History:
*/
// ChaffTypeIgc.cpp : Implementation of CchaffTypeIGC
#include "pch.h"
#include "ChaffTypeIgc.h"

/////////////////////////////////////////////////////////////////////////////
// CchaffTypeIGC
HRESULT     CchaffTypeIGC::Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize)
{
    assert (pMission);
    m_pMission = pMission;

    HRESULT hr = S_OK;

    if (data && (dataSize == sizeof(DataChaffTypeIGC)))
    {
        m_data = *((DataChaffTypeIGC*)data);

        pMission->AddExpendableType(this);
    }
    else
        hr = E_INVALIDARG;

    return hr;
}

int         CchaffTypeIGC::Export(void* data) const
{
    if (data)
        *((DataChaffTypeIGC*)data) = m_data;

    return sizeof(DataChaffTypeIGC);
}

