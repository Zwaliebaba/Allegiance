#ifndef _enginep_h_
#define _enginep_h_

//////////////////////////////////////////////////////////////////////////////
//
// Global hacks
//
//////////////////////////////////////////////////////////////////////////////

extern bool g_bMDLLog;
extern bool g_bWindowLog;

//////////////////////////////////////////////////////////////////////////////
//
// Direct Draw Asserts
//
//////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
    bool DDError(HRESULT hr, const char* pszCall, const char* pszFile, int line, const char* pszModule);
    bool DDSCallImpl(HRESULT hr, const char* pszCall, const char* pszFile, int line, const char* pszModule);

    #define DDCall(hr)  DDError(hr, #hr, __FILE__, __LINE__, __MODULE__)
    #define DDSCall(hr) DDSCallImpl(hr, #hr, __FILE__, __LINE__, __MODULE__)
#else
    bool DDSCallImpl(HRESULT hr);

    #define DDCall(hr) SUCCEEDED(hr)
    #define DDSCall(hr) SUCCEEDED(hr)
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Forward Declarations
//
//////////////////////////////////////////////////////////////////////////////

class PrivateContext;
class PrivateSurface;
class PrivateEngine;
class Rasterizer;
class D3D11Device;
class D3D11Target;

//////////////////////////////////////////////////////////////////////////////
//
// D3D11Device: the one Direct3D 11 device the engine renders and presents
// with. Created on first use, so tools that only convert surfaces (mdlc)
// never touch a GPU. Falls back to the WARP software adapter.
//
//////////////////////////////////////////////////////////////////////////////

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

class D3D11Device : public IObject {
public:
    virtual ID3D11Device*        GetDevice()        = 0;
    virtual ID3D11DeviceContext* GetContext()       = 0;
    virtual ZString              GetName()          = 0;
    virtual bool                 IsHardware()       = 0;

    //
    // Incremented whenever the device is re-created after a device removal;
    // holders of device objects compare it and re-create theirs.
    //

    virtual int                  GetGeneration()    = 0;
    virtual bool                 CheckDeviceLost(long hr) = 0; // true if lost (and re-created)

    //
    // Textures: a GPU copy of a surface, refreshed when the surface changes
    //

    virtual ID3D11ShaderResourceView* GetTexture(PrivateSurface* psurface) = 0;
    virtual void                      RemoveSurface(PrivateSurface* psurface) = 0;

    //
    // Presentation: draw rectSource of psurface into the window's client
    // area, scaled to fit with letterboxing, and present.
    //

    virtual void Present(HWND hwnd, PrivateSurface* psurface, const WinRect& rectSource, float gamma) = 0;
    virtual void ReleaseSwapChain(HWND hwnd) = 0;

    virtual int GetTotalVideoMemory() = 0;
};

TRef<D3D11Device> CreateD3D11Device();

//////////////////////////////////////////////////////////////////////////////
//
// Transform and lighting done in the rasterizer's vertex shader. Device3D
// fills this for each model-space draw; the formulas are those of its CPU
// lighting functions (device3d.cpp).
//
//////////////////////////////////////////////////////////////////////////////

enum TLMode {
    TLModeWhite,            // LightVertexCopy
    TLModeGlobalColor,      // LightVertexGlobalColor
    TLModeDirectional,      // LightVertex
    TLModeBidirectional,    // BidirectionalLightVertex
    TLModeMaterial,         // MaterialLightVertex
    TLModeVertexColor       // VertexL: the vertex's own colour
};

const int c_maxTLClipPlanes = 2;

struct TransformAndLight {
    float m_full[4][4];     // model -> the space Device3D clips in (rows)
    float m_post[4][4];     // that space -> Direct3D clip space (rows)
    float m_light[4];       // model-space light direction
    float m_color[4];       // bidirectional light colour
    float m_colorAlt[4];    // and the colour of the other side
    float m_global[4];      // global colour, or material diffuse
    float m_ambient;
    int   m_mode;           // TLMode
    int   m_countPlanes;    // extra clip planes beyond the frustum
    float m_planes[c_maxTLClipPlanes][4];
    float m_eye[4];         // model-space eye position (per-pixel specular)
    float m_zrev[4];        // model -> reversed clip-space z (1 at the near plane, 0 at the far), for float depth
    float m_depth[2];       // reversed depth -> view distance d = m_depth[1] / (m_depth[0] - depth); 0, 0 if not a perspective
};

//////////////////////////////////////////////////////////////////////////////
//
// Rasterizer
//
//////////////////////////////////////////////////////////////////////////////

class Rasterizer : public IObject {
public:
    virtual void Terminate()                                            = 0;

    //
    // State
    //

    virtual void  SetShadeMode(ShadeMode shadeMode)                     = 0;
    virtual void  SetBlendMode(BlendMode blendMode)                     = 0;
    virtual void  SetTexture(Surface* psurfaceTexture)                  = 0;
    virtual void  SetWrapMode(WrapMode wrapMode)                        = 0;
    virtual void  SetCullMode(CullMode cullMode)                        = 0;
    virtual void  SetZTest(bool bZTest)                                 = 0;
    virtual void  SetZWrite(bool bZWrite)                               = 0;
    virtual void  SetLinearFilter(bool bLinearFilter)                   = 0;
    virtual void  SetPerspectiveCorrection(bool bPerspectiveCorrection) = 0;
    virtual void  SetDither(bool bDither)                               = 0;
    virtual void  SetColorKey(bool bColorKey)                           = 0;

    virtual void  BeginScene()                                          = 0;
    virtual void  EndScene()                                            = 0;
    virtual void  ClearZBuffer()                                        = 0;
    virtual void  SetClipRect(const Rect& rectClip)                     = 0;

    //
    // A 3D layer (Context::Begin3DLayer / End3DLayer), inside a scene. One
    // with a z-buffer is drawn into an HDR scene target and composited over
    // the surface when it ends.
    //

    virtual void  Begin3DLayer(bool bZBuffer)                           = 0;
    virtual void  End3DLayer()                                          = 0;

    //
    // Attributes
    //

    virtual Point GetSurfaceSize()                                      = 0;
    virtual bool  Has3DAcceleration()                                   = 0;

    //
    // Rendering
    //

    virtual void DrawTriangles(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void    DrawPoints(const VertexScreen* pvertex, int vcount)                                 = 0;

    //
    // Model-space geometry, transformed, lit and clipped in the vertex shader
    //

    virtual void DrawTriangles(const TransformAndLight& tl, const Vertex*  pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void DrawTriangles(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const TransformAndLight& tl, const Vertex*  pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const TransformAndLight& tl, const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
};

TRef<Rasterizer> CreateD3D11Rasterizer(PrivateSurface* psurface);

//////////////////////////////////////////////////////////////////////////////
//
// 3D Device
//
//////////////////////////////////////////////////////////////////////////////

class IDevice3D : public IObject {
public:
    //
    // Capabilities
    //

    virtual bool Has3DAcceleration() = 0;

    //
    // State
    //

    virtual bool GetClipping()                                         = 0;
    virtual const Matrix& GetMatrix()                                  = 0;
    virtual const Matrix& GetInverseModelMatrix()                      = 0;

    virtual void AddClipPlane(const Plane& plane)                      = 0;
    virtual void RemoveClipPlane(int indexRemove)                      = 0;

    virtual void SetMatrix(const Matrix& mat)                          = 0;
    virtual void SetPerspectiveMatrix(const Matrix& mat)               = 0;
    virtual void SetClipping(bool bClip)                               = 0;
    virtual void SetShadeMode(ShadeMode shadeMode)                     = 0;
    virtual void SetBlendMode(BlendMode blendMode)                     = 0;
    virtual void SetTexture(Surface* psurfaceTexture)                  = 0;
    virtual void SetMaterial(Material* pmaterial)                      = 0;
    virtual void SetWrapMode(WrapMode wrapMode)                        = 0;
    virtual void SetCullMode(CullMode cullMode)                        = 0;
    virtual void SetZTest(bool bZTest)                                 = 0;
    virtual void SetZWrite(bool bZWrite)                               = 0;
    virtual void SetLinearFilter(bool bLinearFilter)                   = 0;
    virtual void SetPerspectiveCorrection(bool bPerspectiveCorrection) = 0;
    virtual void SetDither(bool bDither)                               = 0;
    virtual void SetColorKey(bool bColorKey)                           = 0;
    virtual void SetLineWidth(float width)                             = 0;
    virtual void SetDeformation(Deformation* pdeform)                  = 0;

    virtual void SetGlobalColor(const Color& color)                                               = 0;
    virtual void DirectionalLight(const Vector& vec, const Color& color)                          = 0;
    virtual void BidirectionalLight(const Vector& vec, const Color& color, const Color& colorAlt) = 0;
    virtual void SetAmbientLevel(float level)                                                     = 0;

    //
    //
    //

    virtual void SetClipRect(const Rect& rectClip)                     = 0;
    virtual void BeginScene()                                          = 0;
    virtual void EndScene()                                            = 0;
    virtual void ClearZBuffer()                                        = 0;
    virtual void Begin3DLayer(bool bZBuffer)                           = 0;
    virtual void End3DLayer()                                          = 0;

    //
    // VertexBuffer
    //

    virtual VertexL*      GetVertexLBuffer(int count)      = 0;
    virtual VertexScreen* GetVertexScreenBuffer(int count) = 0;

    //
    // Rendering
    //

    virtual void DrawTriangles(const D3DVertex* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const D3DVertex* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void    DrawPoints(const D3DVertex* pvertex, int vcount)                                 = 0;

    virtual void DrawTriangles(const Vertex* psource, int count, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const Vertex* psource, int count, const MeshIndex* pindex, int icount) = 0;
    virtual void    DrawPoints(const Vertex* psource, int count)                                 = 0;

    virtual void DrawTriangles(const VertexL* psource, int count, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const VertexL* psource, int count, const MeshIndex* pindex, int icount) = 0;
    virtual void    DrawPoints(const VertexL* psource, int count                                ) = 0;

    virtual void DrawTriangles(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void     DrawLines(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) = 0;
    virtual void    DrawPoints(const VertexScreen* pvertex, int vcount)                                 = 0;

    //
    // Performance Counters
    //

    virtual int GetPerformanceCounter(Counter counter) = 0;
    virtual void ResetPerformanceCounters() = 0;
};

TRef<IDevice3D> CreateDevice3D(Rasterizer* prasterizer);

//////////////////////////////////////////////////////////////////////////////
//
// PrivateContext
//
//////////////////////////////////////////////////////////////////////////////

class PrivateContext : public Context {
public:
	//
	// Called from Surface
	//

    virtual void            BeginRendering() = 0;
    virtual void            EndRendering()   = 0;
    virtual PrivateSurface* GetSurface()     = 0;
};

TRef<PrivateContext> CreateContextImpl(PrivateSurface* psurface);

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

class BinarySurfaceInfo {
public:
    WinPoint m_size;
    int      m_pitch;
    DWORD    m_bitCount;
    DWORD    m_redMask;
    DWORD    m_greenMask;
    DWORD    m_blueMask;
    DWORD    m_alphaMask;
    bool     m_bColorKey;
};

//////////////////////////////////////////////////////////////////////////////
//
// PrivateSurface
//
//////////////////////////////////////////////////////////////////////////////

class PrivateSurface : public Surface {
public:
    //
    // Incremented on every write to the pixels; the D3D11 texture cache and
    // render targets compare it to know when their GPU copy is stale.
    //

    virtual int  GetChangeID()                      = 0;
    virtual void SetPixelFormat(PixelFormat* ppf)   = 0;

    //
    // GPU residency. A 3D surface (the window frame, 3D panes) keeps a GPU
    // copy once something draws into it; flags in the surface say which copy
    // is current, and each access brings the copy it needs up to date.
    //
    // BeginGpuDraw: the GPU copy, current and about to be drawn into (the CPU
    //     copy becomes stale). NULL if the surface can't use the GPU.
    // GetGpuSource: for reading the surface as a texture: its GPU copy made
    //     current, or NULL if it has none (use the texture cache).
    // MakeGpuResident: give the surface a GPU copy now (the presenter does
    //     this for the frame), keeping the CPU copy current too.
    //

    virtual bool                      UsesGpu()          = 0;
    virtual D3D11Target*              BeginGpuDraw()     = 0;
    virtual ID3D11ShaderResourceView* GetGpuSource()     = 0;
    virtual bool                      MakeGpuResident()  = 0;

    //
    // Mip levels below the surface itself, precomputed by the art build (a
    // DDS file with a mip chain) and uploaded with it by the texture cache.
    // Level n is the surface's size >> n (at least 1), in the surface's pixel
    // format, rows tightly packed. They describe the pixels as loaded: once
    // the surface is written to, GetMipLevelCount returns 1.
    //
    // SetMipLevels: plevels[0] is level 1; pobjectMemory keeps them alive.
    //

    virtual void        SetMipLevels(int count, const BYTE* const* plevels, IObject* pobjectMemory) = 0;
    virtual int         GetMipLevelCount()          = 0;   // including the surface (level 0)
    virtual const BYTE* GetMipLevel(int level)      = 0;   // 1 .. count - 1
};

//////////////////////////////////////////////////////////////////////////////
//
// PrivateSurface
//
//////////////////////////////////////////////////////////////////////////////

TRef<PrivateSurface> CreatePrivateSurface(
    PrivateEngine*  pengine,
    PixelFormat*    ppf,
    const WinPoint& size,
    SurfaceType     stype,
    SurfaceSite*    psite
);

TRef<PrivateSurface> CreatePrivateSurface(
          PrivateEngine*  pengine,
          PixelFormat*    ppf,
    const WinPoint&       size,
          int             pitch,
          BYTE*           pdata,
          IObject*        pobjectMemory
);

//////////////////////////////////////////////////////////////////////////////
//
// PrivateEngine
//
//////////////////////////////////////////////////////////////////////////////

class PrivateEngine : public Engine {
public:
    virtual void RemovePrivateSurface(PrivateSurface* psurface) = 0;

    //
    // The D3D11 device, created on first use. NULL if no device could be
    // created at all (not even WARP).
    //

    virtual D3D11Device* GetD3D11Device() = 0;

    virtual TRef<PrivateSurface> CreateCompatibleSurface(
        PrivateSurface* psurface, 
        const WinPoint& size, 
        SurfaceType     stype, 
        SurfaceSite*    psite
    ) = 0;

    //
    // Pixel Formats
    //

    virtual PixelFormat*      GetPrimaryPixelFormat() = 0;
    virtual TRef<PixelFormat> GetPixelFormat(
        int   bits,
        DWORD redMask,
        DWORD greenMask,
        DWORD blueMask,
        DWORD alphaMask
    ) = 0;
};

//////////////////////////////////////////////////////////////////////////////
//
// IEngineFont
//
//////////////////////////////////////////////////////////////////////////////

TRef<IEngineFont> CreateEngineFont(HFONT hfont);
TRef<IEngineFont> CreateEngineFont(IBinaryReaderSite* psite);

//////////////////////////////////////////////////////////////////////////////
//
// NameSpace stuff
//
//////////////////////////////////////////////////////////////////////////////

TRef<INameSpaceInfo> CreateNameSpaceInfo(INameSpace* pns, const ZString& str);

TRef<INameSpace> CreateNameSpace(const ZString& strName);
TRef<INameSpace> CreateNameSpace(const ZString& strName, INameSpace* pnsParent);
TRef<INameSpace> CreateNameSpace(const ZString& strName, INameSpaceList& parents);
TRef<INameSpace> CreateNameSpace(const ZString& strName, Modeler* pmodeler, ZFile* pfile);

TRef<INameSpace> CreateBinaryNameSpace(const ZString& strName, Modeler* pmodeler, ZFile* pfile);

//////////////////////////////////////////////////////////////////////////////
//
// Popup stuff
//
//////////////////////////////////////////////////////////////////////////////

class IPopupContainerPrivate : public IPopupContainer {
public:
    virtual void Initialize(Engine* pengine, RectValue* prectValue) = 0;
};

TRef<IPopupContainerPrivate> CreatePopupContainer();

#endif
