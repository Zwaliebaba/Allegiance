/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	ModelIgc.h
**
**  Author: 
**
**  Description:
**      Header for the CmodelIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// ModelIgc.h : Declaration of the CmodelIGC

#ifndef __MODELIGC_H_
#define __MODELIGC_H_

template<class T> class TmodelIGC;

class   CmodelIGC
{
    public:
        CmodelIGC(ImodelIGC*    model);
        ~CmodelIGC(void);

        Time                GetMyLastUpdate(void) const
        {
            return m_lastUpdate;
        }

        void                SetMyLastUpdate(Time  lastUpdate)
        {
            m_lastUpdate = lastUpdate;
        }

        ImissionIGC*        GetMyMission(void) const
        {
            return m_pMission;
        }

        void                SetMyMission(ImissionIGC* pMission)
        {
            assert (m_side == NULL);
            assert (m_cluster == NULL);

            m_pMission = pMission;
        }

        void                MakeStatic(void)
        {
            assert ((m_attributes & c_mtStatic) == 0);
            assert ((m_attributes & c_mtHitable) == 0);
            assert ((m_attributes & c_mtCastRay) != 0);

            m_pHitTest->SetID(c_htidStaticObject);
            m_pHitTest->SetStaticF(true);

            assert (m_cluster);

            m_attributes &= ~c_mtCastRay;
            m_attributes |= c_mtStatic | c_mtHitable;

            m_cluster->MakeModelStatic(m_model);
        }
        void                MakeUnhitable(void)
        {
            assert ((m_attributes & c_mtStatic) == 0);
            assert ((m_attributes & c_mtHitable) != 0);
            assert ((m_attributes & c_mtCastRay) != 0);

            assert (m_cluster);

            m_attributes &= ~(c_mtCastRay | c_mtHitable);

            m_cluster->MakeModelUnhitable(m_model);
        }

    private:
        void                SetMyAttributes(ModelAttributes newVal);

    public:

    // IbaseIGC
        HRESULT         Initialize(ImissionIGC* pMission, Time now, const void*  data, int dataSize)
        {
			m_lastUpdate = now;
            m_pMission = pMission;

            return S_OK;
        }

        void            Terminate(void);

        void            Update(Time now);

    // ImodelIGC
        virtual void        LoadCVH(const char*     pszFileName,
                                    const char*     pszIconName,
                                    ModelAttributes attributes,
                                    HitTestShape    htsDefault = c_htsSphere);
        HRESULT         LoadMine(const char* textureName, float strength, float radius, const char* iconName, ModelAttributes attributes);
        HRESULT         LoadWarp(const char* textureName, const char* iconName, ModelAttributes attributes);
        HRESULT         LoadEffect(const Color& color, ModelAttributes attributes);

        HRESULT         Load(int                options,
                             const char*        model,
                             const char*        texture,
                             const char*        icon,
                             ModelAttributes    attributes);

        bool            GetVisibleF(void) const
        {
            return m_visibleF;
        }
        void            SetVisibleF(bool visibleF);
        void            SetRender(unsigned char render);

        bool            SeenBySide(IsideIGC*   side) const
        {
            if (!m_pThingSite)
                return false;
            return m_pThingSite->GetSideVisibility(side);
        }

        void            UpdateSeenBySide(void)
        {
            m_pThingSite->UpdateSideVisibility(m_model, m_cluster);
        }
        void            SetSideVisibility(IsideIGC* side,
                                          bool      fVisible)
        {
            m_pThingSite->SetSideVisibility(side, fVisible);
        }

        ThingSite*      GetThingSite(void) const
        {
            return m_pThingSite;
        }
        void            FreeThingSite(void)
        {
            if (m_pHitTest)
            {
                if (m_cluster)
                {
                    m_cluster->FreeThingSite(m_attributes, m_model, m_pHitTest);
                }

                m_pHitTest->Release();
                m_pHitTest = nullptr;

                assert (m_pThingSite);
                m_pThingSite->Terminate();
                m_pThingSite = nullptr;
            }
            else
                assert (!m_pThingSite);
        }

        const Vector&   GetPosition(void) const
        {
            return m_pHitTest->GetPosition();
        }
        void            SetPosition(const Vector& newVal);

        const Vector&   GetVelocity(void) const
        {
            return m_pHitTest->GetVelocity();
        }
        void            SetVelocity(const Vector& newVal)
        {
            assert (newVal * newVal >= 0.0f);
            m_pHitTest->SetVelocity(newVal);
        }

        const Orientation&  GetOrientation(void) const
        {
            return m_pHitTest->GetOrientation();
        }
        void                SetOrientation(const Orientation& newVal);

        const Rotation&     GetRotation(void) const
        {
            return m_rotation;
        }
        void                SetRotation(const Rotation& newVal)
        {
            m_rotation = newVal;
        }
        float               GetRadius(void) const
        {
            return m_pHitTest->GetRadius();
        }
        void                SetRadius(float newVal)
        {
            assert (newVal != 0.0f);
            m_pHitTest->SetRadius(newVal);
            m_pThingSite->SetRadius(newVal);
        }

        ModelAttributes     GetAttributes(void) const
        {
            return m_attributes;
        }

        IsideIGC*           GetSide(void) const
        {
            return m_side;
        }
        void                SetSide(IsideIGC* newVal)
        {
            IsideIGC*   psideOld = m_side;

            if (m_side)
                m_side->Release();

            m_side = newVal;

            if (m_side)
                m_side->AddRef();

            if (m_cluster)
            {
                //Warning ... this will probably cause a few assertion failures
                //till we get the sequence exactly right but we should never change
                //the side of an object that is in a cluster unless we are changing
                //from a legal side to a legal side.
                //assert (newVal);  //Can't do this ... killing a station first sets its side to NULL
                assert (psideOld);

                //Reset the thing site's view of which cluster the model is in
                //(the cluster did not change but some of the internal state for
                //the thing site has to change to reflect the change in side)
                //This also updates the side visibility for the object.
                m_pThingSite->SetCluster(m_model, m_cluster);
            }
        }

        float               GetMass(void) const
        {
            return m_mass;
        }
        void                SetMass(float    newVal)
        {
            m_mass = newVal;
        }

        IclusterIGC*        GetCluster(void) const
        {
            return m_cluster;
        }
        void                SetCluster(IclusterIGC* newVal);

        const char*         GetName(void) const
        {
            return m_name;
        }
        void                SetName(const char* newVal)
        {
            UTL::putName(m_name, newVal);
        }
        void                SetSecondaryName(const char* newVal)
        {
            assert (strlen(newVal) < c_cbName-1);
            m_name[0] = '\0';

            strcpy(m_name+1, newVal);
        }

        void                SetSignature(float    newVal)
        {
            m_signature = newVal;
        }
        float               GetSignature(void) const
        {
            return m_signature;
        }
        void                ChangeSignature(float delta)
        {
            m_signature += delta;
        }

        void                SetTexture(const char* pszTextureName)
        {
            m_pThingSite->SetTexture(pszTextureName);
        }

        HRESULT                 LoadDecal(const char*      textureName,
                                          const char*      iconName,
                                          const Color&     color,
                                          bool             bDirectional,
                                          float            width,
                                          ModelAttributes  attributes,
                                          HitTestShape     htsDefault = c_htsPoint);

        void                    Move(float t)
        {
            m_pHitTest->Move(t);
            m_pThingSite->SetPosition(m_pHitTest->GetPosition());
        }
        void                    Move(void)
        {
            m_pHitTest->Move();
            m_pThingSite->SetPosition(m_pHitTest->GetPosition());
        }
        void                    SetBB(Time tStart, Time tStop, float dT)
        {
            m_pHitTest->SetBB(tStart, tStop, dT);
        }
        HitTest*                GetHitTest(void) const
        {
            return m_pHitTest;
        }

        virtual IObject*            GetIcon(void) const
        {
            return m_poIcon;
        }
        virtual void                SetIcon(IObject* picon)
        {
            m_poIcon = picon;
        }

        virtual const DamageBucketList*  GetDamageBuckets(void) const
        {
            return &m_damageBuckets;
        }
        virtual void                     AddDamageBucket(DamageBucket*   pdb)
        {
            assert (m_pMission);
            m_damageBuckets.last(pdb);
        }
        virtual void                     DeleteDamageBucket(DamageBucket*   pdb)
        {
            DamageBucketLink*   pdbl = m_damageBuckets.find(pdb);
            assert (pdbl);
            delete pdbl;
        }

    private:
        ImissionIGC*        m_pMission;
        Time                m_lastUpdate;

        TRef<ThingSite>     m_pThingSite;
        TRef<IObject>       m_poIcon;
        IsideIGC*           m_side;

        DamageBucketList    m_damageBuckets;

        ImodelIGC*          m_model;        //pointer for anything that needs a pointer to the "model" & not the class implementation
        Rotation            m_rotation;
        IclusterIGC*        m_cluster;
        float               m_mass;
        float               m_signature;
        HitTest*            m_pHitTest;
        char                m_name[c_cbName];
        ModelAttributes     m_attributes;
        bool                m_visibleF;
        bool                m_selectedF;
        bool                m_decalF;
        unsigned char       m_render;
};

template<class T> class TmodelIGC : public T
{
    public:
        TmodelIGC(void)
        :
            m_model(this)
        {
        }

        //Note ... these methods are not part of the ImodelIGC interface (& not virtual either)
        Time                GetMyLastUpdate(void) const
        {
            return m_model.GetMyLastUpdate();
        }

        void                SetMyLastUpdate(Time  lastUpdate)
        {
            m_model.SetMyLastUpdate(lastUpdate);
        }

        ImissionIGC*        GetMyMission(void) const
        {
            return m_model.GetMyMission();
        }

        void                SetMyMission(ImissionIGC* pMission)
        {
            m_model.SetMyMission(pMission);
        }

        void                MakeStatic(void)
        {
            m_model.MakeStatic();
        }

        void                MakeUnhitable(void)
        {
            m_model.MakeUnhitable();
        }

    // IbaseIGC
        HRESULT Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize) override
        {
            return m_model.Initialize(pMission, now, data, dataSize);
        }
        void    Terminate(void) override
        {
            m_model.Terminate();
        }
        void    Update(Time now) override
        {
            m_model.Update(now);
        }

        ImissionIGC*        GetMission(void) const override
        {
            return m_model.GetMyMission();
        }

    // ImodelIGC
        Time                 GetLastUpdate(void) const override
        {
            return m_model.GetMyLastUpdate();
        }
        void                 SetLastUpdate(Time now) override
        {
            m_model.SetMyLastUpdate(now);
        }

        void        SetMission(ImissionIGC* pMission) override
        {
            m_model.SetMyMission(pMission);
        }
        void        LoadCVH(const char*     pszFileName,
                                    const char*     pszIconName,
                                    ModelAttributes attributes,
                                    HitTestShape    htsDefault = c_htsSphere) override
        {
            m_model.LoadCVH(pszFileName, pszIconName, attributes, htsDefault);
        }
        HRESULT              LoadEffect(const Color& color, ModelAttributes attributes) override
        {
            return m_model.LoadEffect(color, attributes);
        }
        HRESULT              LoadMine(const char* textureName, float strength, float radius, const char* icon, ModelAttributes attributes) override
        {
            return m_model.LoadMine(textureName, strength, radius, icon, attributes);
        }
        HRESULT              LoadWarp(const char* textureName, const char* icon, ModelAttributes attributes) override
        {
            return m_model.LoadWarp(textureName, icon, attributes);
        }

        HRESULT              Load(int                  options,
                                          const char*          model,
                                          const char*          texture,
                                          const char*          icon,
                                          ModelAttributes      attributes) override
        {
            return m_model.Load(options, model, texture, icon, attributes);
        }

        bool                 GetVisibleF(void) const override
        {
            return m_model.GetVisibleF();
        }
        void                 SetVisibleF(bool visibleF) override
        {
            m_model.SetVisibleF(visibleF);
        }
        void                 SetRender(unsigned char render) override
        {
            m_model.SetRender(render);
        }

        bool                 SeenBySide(IsideIGC*  side) const override
        {
            return m_model.SeenBySide(side);
        }
        void                 UpdateSeenBySide(void) override
        {
            m_model.UpdateSeenBySide();
        }
        void                 SetSideVisibility(IsideIGC* side, bool fVisible) override
        {
            m_model.SetSideVisibility(side, fVisible);
        }

        ThingSite*            GetThingSite(void) const override
        {
            return m_model.GetThingSite();
        }

        void                  FreeThingSite(void) override
        {
            m_model.FreeThingSite();
        }

        const Vector&        GetPosition(void) const override
        {
            return m_model.GetPosition();
        }
        void                 SetPosition(const Vector& newVal) override
        {
            m_model.SetPosition(newVal);
        }

        const Vector&        GetVelocity(void) const override
        {
            return m_model.GetVelocity();
        }
        void                 SetVelocity(const Vector& newVal) override
        {
            m_model.SetVelocity(newVal);
        }

        const Orientation&   GetOrientation(void) const override
        {
            return m_model.GetOrientation();
        }
        void                 SetOrientation(const Orientation& newVal) override
        {
            m_model.SetOrientation(newVal);
        }

        const Rotation&      GetRotation(void) const override
        {
            return m_model.GetRotation();
        }
        void                 SetRotation(const Rotation& newVal) override
        {
            m_model.SetRotation(newVal);
        }
        float                GetRadius(void) const override
        {
            return m_model.GetRadius();
        }
        void                 SetRadius(float newVal) override
        {
            m_model.SetRadius(newVal);
        }

        ModelAttributes      GetAttributes(void) const override
        {
            return m_model.GetAttributes();
        }

        IsideIGC*            GetSide(void) const override
        {
            return m_model.GetSide();
        }
        void                 SetSide(IsideIGC* newVal) override
        {
            m_model.SetSide(newVal);
        }

        float                GetMass(void) const override
        {
            return m_model.GetMass();
        }
        void                 SetMass(float newVal) override
        {
            m_model.SetMass(newVal);
        }

        IclusterIGC*         GetCluster(void) const override
        {
            return m_model.GetCluster();
        }
        void                 SetCluster(IclusterIGC* newVal) override
        {
            m_model.SetCluster(newVal);
        }

        char const*          GetName(void) const override
        {
            return m_model.GetName();
        }
        void                 SetName(const char* newVal) override
        {
            m_model.SetName(newVal);
        }
        void                SetSecondaryName(const char* newVal) override
        {
            m_model.SetSecondaryName(newVal);
        }
        float                GetSignature(void) const override
        {
            return m_model.GetSignature();
        }
        void                 SetSignature(float newVal) override
        {
            m_model.SetSignature(newVal);
        }

        void                 ChangeSignature(float delta) override
        {
            m_model.ChangeSignature(delta);
        }

        void                SetTexture(const char* pszTextureName) override
        {
            m_model.SetTexture(pszTextureName);
        }

        HRESULT              LoadDecal(const char*      textureName,
                                               const char*      iconName,
                                               const Color&     color,
                                               bool             bDirectional,
                                               float            width,
                                               ModelAttributes  attributes,
                                               HitTestShape     htsDefault = c_htsPoint) override
        {
            return m_model.LoadDecal(textureName, iconName, color, bDirectional, width, attributes, htsDefault);
        }

        void                 Move(float t) override
        {
            m_model.Move(t);
        }
        void                 Move(void) override
        {
            m_model.Move();
        }
        void                 SetBB(Time tStart, Time tStop, float dt) override
        {
            m_model.SetBB(tStart, tStop, dt);
        }
        HitTest*             GetHitTest(void) const override
        {
            return m_model.GetHitTest();
        }

        void                 HandleCollision(Time       timeCollision,
                                                     float                  tCollision,
                                                     const CollisionEntry&  entry,
                                                     ImodelIGC* pModel) override
        {
            //Should never be called
            assert (false);
        }

        IObject*            GetIcon(void) const override
        {
            return m_model.GetIcon();
        }

        void                    SetIcon(IObject* picon) override
        {
            m_model.SetIcon(picon);
        }

        const DamageBucketList*  GetDamageBuckets(void) const override
        {
            return m_model.GetDamageBuckets();
        }
        void                     AddDamageBucket(DamageBucket*   pdb) override
        {
            m_model.AddDamageBucket(pdb);
        }
        void                     DeleteDamageBucket(DamageBucket*   pdb) override
        {
            m_model.DeleteDamageBucket(pdb);
        }

    private:
        CmodelIGC   m_model;
};

#endif //__MODELIGC_H_
