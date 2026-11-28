#ifndef _Geometry_h_
#define _Geometry_h_

//////////////////////////////////////////////////////////////////////////////
//
// WrapGeo, a Geo that wraps another geo
//
//////////////////////////////////////////////////////////////////////////////

class WrapGeo : public Geo {
public:
    WrapGeo(Geo* pgeo);
    WrapGeo(Geo* pgeo, Value* pvalue1);

    void SetGeo(Geo* pvalue) { SetChild(0, pvalue); }
    Geo* GetGeo() { return Geo::Cast(GetChild(0)); }

    //
    // WrapGeo members
    //

    virtual TRef<WrapGeo> Duplicate(Geo* pgeo);

    //
    // Geo members
    //

    Image* FindTexture() override;
    void Render(Context* pcontext) override;
    float GetRadius(const Matrix& mat = Matrix::GetIdentity()) override;
    void CallGroupGeoCallback(const Matrix& mat, GroupGeoCallback* pcallback) override;
    TRef<Geo> FoldTexture() override;
    TRef<Geo> RemoveMaterial() override;
    TRef<Geo> ApplyTransform(Transform* ptrans) override;
    int       GetTriangleCount() override;
    bool      AnyChildGroups(bool bHead) override;

    void RemoveCapture() override;
    bool HitTest(const Vector& vecOrigin, const Vector& vecRay, HitData3D& hitData, bool bCaptured) override;
    void MouseMove(const Vector& vec, bool bCaptured, bool bInside) override;
    void MouseEnter(const Vector& vec) override;
    void MouseLeave() override;
    MouseResult Button(const Vector& vec, int button, bool bCaptured, bool bInside, bool bDown) override;

    //
    // Value members
    //

    TRef<Value> Fold() override;
    ZString GetFunctionName() override { return "WrapGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// DecalGeo
//
//////////////////////////////////////////////////////////////////////////////

class DecalGeo : public Geo {
private:
    Color  m_color;
    Vector m_vecPosition;
    Vector m_vecForward;
    Vector m_vecRight;
    float  m_scale;
    float  m_angle;

public:
    DecalGeo(
        Image* pimage,
        const Color& color,
        const Vector& position,
        const Vector& forward,
        const Vector& right,
        float scale,
        float angle
    );

    void SetImage(Image* pvalue) { SetChild(0, pvalue); }
    Image* GetImage() { return Image::Cast(GetChild(0)); }

    float GetAngle()
    {
        return m_angle;
    }

    void SetAngle(float angle)
    {
        m_angle = angle;
    }

	const Color& GetColor()
	{
		return m_color;
	}

	void SetColor(const Color& color)
	{
		m_color = color;
	}

    const Vector& GetPosition()
    {
        return m_vecPosition;
    }

    void SetPosition(const Vector& vec)
    {
        m_vecPosition = vec;
    }

    void SetForward(const Vector& vec)
    {
        m_vecForward = vec;
    }

    const Vector& GetForward()
    {
        return m_vecForward;
    }

    void SetRight(const Vector& vec)
    {
        m_vecRight = vec;
    }

    const Vector& GetRight()
    {
        return m_vecRight;
    }

    float GetScale()
    {
        return m_scale;
    }

    void SetScale(float scale)
    {
        m_scale = scale;
    }

    void Render(Context* pcontext) override;

    int GetTriangleCount() override
    {
        return 2;
    }

    //
    // Value members
    //

    ZString GetFunctionName() override { return "DecalGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class VisibleGeo : public WrapGeo {
private:
    bool m_bVisible;

public:
    DefineCastMember(VisibleGeo);
    VisibleGeo(Geo* pgeo);

    void SetVisible(bool bVisible);
    bool IsVisible()
    {
        return m_bVisible;
    }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "VisibleGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class ClipGeo : public WrapGeo {
private:
    bool m_bClip;

public:
    DefineCastMember(ClipGeo);
    ClipGeo(Geo* pgeo);

    bool GetClip()
    {
        return m_bClip;
    }

    void SetClip(bool bClip);
    bool IsClip()
    {
        return m_bClip;
    }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "ClipGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class ClipPlaneGeo : public WrapGeo {
private:
    Plane m_plane;

public:
    DefineCastMember(ClipPlaneGeo);
    ClipPlaneGeo(Geo* pgeo, const Plane& plane);

    void SetClipPlane(const Plane& plane);

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "ClipPlaneGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class TransformGeo : public WrapGeo {
public:
    TransformGeo(Geo* pgeo, Transform* ptrans);

    void SetTransform(Transform* pvalue) { SetChild(1, pvalue); }
    Transform* GetTransform() { return Transform::Cast(GetChild(1)); }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;
    float GetRadius(const Matrix& mat = Matrix::GetIdentity()) override;
    void CallGroupGeoCallback(const Matrix& mat, GroupGeoCallback* pcallback) override;

    //
    // Value members
    //

    TRef<Value> Fold() override;
    ZString GetFunctionName() override { return "TransformGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class MaterialGeo : public WrapGeo {
private:
    TRef<Material> m_pmaterial;
    bool           m_bOverride;

public:
    MaterialGeo(Geo* pgeo, Material* pmaterial, bool bOverride = false);

    void SetMaterial(Material* pvalue) { SetChild(1, pvalue); }
    Material* GetMaterial() { return Material::Cast(GetChild(1)); }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;
    TRef<Geo> RemoveMaterial() override;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "MaterialGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class CullModeGeo : public WrapGeo {
private:
    CullMode m_mode;
    bool     m_bOveride;

public:
    CullModeGeo(Geo* pgeo, CullMode mode, bool bOveride = false) :
        WrapGeo(pgeo),
        m_mode(mode),
        m_bOveride(bOveride)
    {
    }

    void SetCullMode(CullMode mode) { m_mode = mode; }
    CullMode GetCullMode() { return m_mode; }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override
    {
        return new CullModeGeo(pgeo, m_mode, m_bOveride);
    }

    //
    // Geo members
    //

    void Render(Context* pcontext) override
    {
        pcontext->SetCullMode(m_mode, m_bOveride);
        GetGeo()->Render(pcontext);
    }

    //
    // Value members
    //

    ZString GetFunctionName() override { return "CullModeGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

class TextureGeo : public WrapGeo {
private:
    bool m_bOverride;

public:
    TextureGeo(Geo* pgeo, Image* pimageTexture, bool bOverride = false);

    void SetTexture(Image* pvalue) { SetChild(1, pvalue); }
    Image* GetTexture() { return Image::Cast(GetChild(1)); }

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;
    TRef<Geo>     FoldTexture() override;

    //
    // Geo members
    //

    Image* FindTexture() override;
    void Render(Context* pcontext) override;
    TextureGeo* GetTextureGeo() override;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "TextureGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// Timed Geo
//
//////////////////////////////////////////////////////////////////////////////

class TimedGeo : public WrapGeo, public EventSourceImpl {
private:
    float m_lifespan;

    void Evaluate() override;

    Number* GetTime() { return Number::Cast(GetChild(1)); }

public:
    TimedGeo(Geo* pgeo, Number* ptime, float lifespan);

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;
};

//////////////////////////////////////////////////////////////////////////////
//
// Group Geo
//
//////////////////////////////////////////////////////////////////////////////

class GroupGeo : public Geo {
protected:
    GroupGeo() : Geo() {}
    GroupGeo(Value* pvalue0) : Geo(pvalue0) {}
    GroupGeo(Value* pvalue0, Value* pvalue1) : Geo(pvalue0, pvalue1) {}

public:
    static TRef<GroupGeo> Create();
    static TRef<GroupGeo> Create(Geo* pgeo);
    static TRef<GroupGeo> Create(Geo* pgeo, Geo* pgeo2);
    static TRef<GroupGeo> Create(Geo* pgeo, Geo* pgeo2, Geo* pgeo3);

    virtual void           SetName(const ZString& strName) = 0;
    virtual const ZString& GetName()                       = 0;
    virtual void           AddGeo(Geo* pgeo)               = 0;
    virtual void           RemoveGeo(Geo* pgeo)            = 0;
    virtual ValueList*     GetList()                       = 0;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "GroupGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// LODGeo
//
//////////////////////////////////////////////////////////////////////////////

class LODGeo : public WrapGeo {
protected:
    LODGeo(Geo* pgeo) : WrapGeo(pgeo) {}

public:
    static TRef<LODGeo> Create(Geo* pgeo);

    //
    // LODGeo members
    //

    virtual void AddGeo(Geo* pgeo, float radius) = 0;
    virtual ZString GetInfo() = 0;

    //
    // Value members
    //

    ZString GetFunctionName() override { return "LODGeo"; }
};

//////////////////////////////////////////////////////////////////////////////
//
// Callback Geo
//
//////////////////////////////////////////////////////////////////////////////

class CallbackGeo : public WrapGeo, IGeoCallback {
public:
    CallbackGeo(Geo* pgeo);

    //
    // WrapGeo members
    //

    TRef<WrapGeo> Duplicate(Geo* pgeo) override;

    //
    // Geo members
    //

    void Render(Context* pcontext) override;
    void RenderCallback(Context* pcontext) override;
};

//////////////////////////////////////////////////////////////////////////////
//
// Blend Add Callback Geo
//
//////////////////////////////////////////////////////////////////////////////

TRef<Geo> CreateBlendGeo(Geo* pgeo, BlendMode blendMode);

#endif
