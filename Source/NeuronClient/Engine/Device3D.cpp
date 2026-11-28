#include "pch.h"

// compile this file for speed

#pragma optimize("t", on)
// #pragma optimize("s", on)

//////////////////////////////////////////////////////////////////////////////
//
// Clipping Planes
//
//////////////////////////////////////////////////////////////////////////////

const int g_maxCountPlanes = 8;

typedef BYTE Code;
const Code ClipFront  = 0x01;
const Code ClipBack   = 0x02;
const Code ClipLeft   = 0x04;
const Code ClipRight  = 0x08;
const Code ClipBottom = 0x10;
const Code ClipTop    = 0x20;
const Code ClipPlane  = 0x40;

//////////////////////////////////////////////////////////////////////////////
//
// Transformed and Lit Vertex
//
//////////////////////////////////////////////////////////////////////////////

class VertexTL {
public:
    HVector    m_pos;
    Color      m_color;
    Point      m_pointTexture;
    MeshIndex  m_index;
    Code       m_code;
    
    VertexTL()
    {
    }

    VertexTL(const VertexTL& v) :
        m_pos(v.m_pos),
        m_color(v.m_color),
        m_pointTexture(v.m_pointTexture),
        m_index(v.m_index),
        m_code(v.m_code)
    {
    }

    void Interpolate(const VertexTL& vertex0, const VertexTL& vertex1, float alpha)
    {
        float beta = 1 - alpha;

        m_pos = ::Interpolate(vertex0.m_pos, vertex1.m_pos, alpha);
        m_color =
            Color(
                alpha * vertex1.m_color.GetRed()   + beta * vertex0.m_color.GetRed()  ,
                alpha * vertex1.m_color.GetGreen() + beta * vertex0.m_color.GetGreen(),
                alpha * vertex1.m_color.GetBlue()  + beta * vertex0.m_color.GetBlue() ,
                alpha * vertex1.m_color.GetAlpha() + beta * vertex0.m_color.GetAlpha()
            );
        m_pointTexture = ::Interpolate(vertex0.m_pointTexture, vertex1.m_pointTexture, alpha);
    }

    void UpdateClipCode()
    {
        m_code = 0;

        if (m_pos.x < -m_pos.w) m_code |= ClipLeft;
        if (m_pos.x >  m_pos.w) m_code |= ClipRight;

        if (m_pos.y < -m_pos.w) m_code |= ClipBottom;
        if (m_pos.y >  m_pos.w) m_code |= ClipTop;

        if (m_pos.z <  0      ) m_code |= ClipFront;
        if (m_pos.z >  m_pos.w) m_code |= ClipBack;
    }

    void UpdateClipCode(const Plane& plane, Code code)
    {
        if (plane.Distance(m_pos) <= 0) {
            m_code |= code;
        }
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Device3D
//
//////////////////////////////////////////////////////////////////////////////

class Device3D : public IDevice3D {
    //
    // Types
    //

    typedef void (Device3D::*PFNClip)(VertexTL* pvertex, int vcount, const MeshIndex* pindex, int icount);
    typedef void (Device3D::*PFNDraw)(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount);
    typedef void (Device3D::*PFNVertex)(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw);
    typedef void (Device3D::*PFNVertexL)(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw);

    //
    // Interfaces
    //

    TRef<Rasterizer>    m_prasterizer;

    //
    // Current state
    //

    Point                m_sizeSurface;
    Rect                 m_rectClip;
    Rect                 m_rectClipScreen;
    bool                 m_bClip;

    bool                 m_bZTest;
    bool                 m_bZWrite;
    ShadeMode            m_shadeMode;
    BlendMode            m_blendMode;

    PFNVertexL           m_pfnTransform;
    PFNVertex            m_pfnLightVertex;
    PFNVertexL           m_pfnLightVertexL;
    PFNDraw              m_pfnDrawLines;

    Matrix               m_mat;
    Matrix               m_matPerspective;
    Matrix               m_matFull;
    Matrix               m_matInverse;
    bool                 m_bUpdateMatFull;
    bool                 m_bUpdateLighting;
    bool                 m_bUpdatePointers;
    bool                 m_bUpdateInverse;

    TRef<Material>       m_pmaterial;
    TRef<Deformation>    m_pdeform;

    //
    // Line drawing attributes
    //

    float                m_lineWidth;

    //
    // Lighting
    //

    float                m_brightAmbient;
    Vector               m_vecLight;
    Vector               m_vecLightWorld;
    Color                m_color;
    Color                m_colorAlt;
    Color                m_colorGlobal;
    bool                 m_bBidirectional;

    //
    // Clipping
    //

    float m_scalex;
    float m_scaley;
    float m_xoffset;
    float m_yoffset;

    int   m_countPlanes;
    Plane m_pplane[g_maxCountPlanes];

    //
    // Temporary vertex and index buffers
    //

    VertexL*      m_pvertexLBuffer;
    int           m_countVertexLBuffer;

    VertexTL*     m_pvertexTLBuffer;
    int           m_countVertexTLBuffer;

    VertexScreen* m_pvertexScreenBuffer;
    int           m_countVertexScreenBuffer;
    int           m_indexVertexScreenBuffer;

    MeshIndex*    m_pindexBuffer;
    int           m_countIndexBuffer;
    int           m_indexIndexBuffer;

    //
    // Performance counters
    //

    #ifdef EnablePerformanceCounters
        int m_countPoints;
        int m_countLines;
        int m_countTriangles;
        int m_countMeshes;
        int m_countObjects;
        int m_countObjectsRendered;
        int m_countMatrixLoads;
    #endif

public:
    //////////////////////////////////////////////////////////////////////////////
    //
    // Constructor
    //
    //////////////////////////////////////////////////////////////////////////////

    Device3D(Rasterizer* prasterizer) :
        m_prasterizer(prasterizer),

        m_pvertexLBuffer(nullptr),
        m_countVertexLBuffer(0),

        m_pvertexTLBuffer(nullptr),
        m_countVertexTLBuffer(0),

        m_pvertexScreenBuffer(nullptr),
        m_countVertexScreenBuffer(0),
        m_indexVertexScreenBuffer(0),

        m_pindexBuffer(nullptr),
        m_countIndexBuffer(0),
        m_indexIndexBuffer(0),

        m_bClip(true),
        m_shadeMode(ShadeModeGouraud),
        m_bUpdateMatFull(true),
        m_bUpdateLighting(true),
        m_bUpdatePointers(true),
        m_brightAmbient(0.0f),
        m_vecLightWorld(1, 0, 0),
        m_color(Color::White()),
        m_colorAlt(Color::White()),
        m_colorGlobal(Color::White()),

        m_pfnDrawLines(&Device3D::DrawLinesRasterizer),
        m_lineWidth(0.5f)
    {
        m_sizeSurface = prasterizer->GetSurfaceSize();
        m_mat.SetIdentity();
        m_matPerspective.SetIdentity();

        //
        // Setup the clipping planes
        //

        m_countPlanes = 6;
        m_pplane[0] = Plane(HVector( 0, 0, 1,  0));
        m_pplane[1] = Plane(HVector( 0, 0, 1, -1));
        m_pplane[2] = Plane(HVector( 1, 0, 0,  1));
        m_pplane[3] = Plane(HVector(-1, 0, 0,  1));
        m_pplane[4] = Plane(HVector( 0, 1, 0,  1));
        m_pplane[5] = Plane(HVector( 0,-1, 0,  1));

        //
        // Initialize Performance counters
        //

        #ifdef EnablePerformanceCounters
            ResetPerformanceCounters();
        #endif
    }

    bool IsValid() override
    {
        return m_prasterizer->IsValid();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Performance counters
    //
    //////////////////////////////////////////////////////////////////////////////

    #ifdef EnablePerformanceCounters
        void ResetPerformanceCounters() override
        {
            m_countPoints          = 0;
            m_countLines           = 0;
            m_countTriangles       = 0;
            m_countMeshes          = 0;
            m_countObjects         = 0;
            m_countObjectsRendered = 0;
            m_countMatrixLoads     = 0;
        }

        int GetPerformanceCounter(Counter counter) override
        {
            switch (counter) {
                case CounterPoints          : return m_countPoints         ;
                case CounterLines           : return m_countLines          ;
                case CounterTriangles       : return m_countTriangles      ;
                case CounterMeshes          : return m_countMeshes         ;
                case CounterObjects         : return m_countObjects        ;
                case CounterObjectsRendered : return m_countObjectsRendered;
                case CounterMatrixLoads     : return m_countMatrixLoads    ;
            }

            ZError("Unknown counter");
            return 0;
        }
    #endif

    //////////////////////////////////////////////////////////////////////////////
    //
    // Destructor
    //
    //////////////////////////////////////////////////////////////////////////////

    ~Device3D()
    {
        if (m_pvertexLBuffer) {
            free(m_pvertexLBuffer);
        }

        if (m_pvertexTLBuffer) {
            free(m_pvertexTLBuffer);
        }

        if (m_pvertexScreenBuffer) {
            free(m_pvertexScreenBuffer);
        }

        if (m_pindexBuffer) {
            free(m_pindexBuffer);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Capabilities
    //
    //////////////////////////////////////////////////////////////////////////////

    bool Has3DAcceleration() override
    {
        return m_prasterizer->Has3DAcceleration();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Device
    //
    //////////////////////////////////////////////////////////////////////////////

    void BeginScene() override
    {
        m_prasterizer->BeginScene();
    }

    void EndScene() override
    {
        m_prasterizer->EndScene();
    }

    void ClearZBuffer() override
    {
        m_prasterizer->ClearZBuffer();
    }

    void Begin3DLayer(bool bZBuffer) override
    {
        m_prasterizer->Begin3DLayer(bZBuffer);
    }

    void End3DLayer() override
    {
        m_prasterizer->End3DLayer();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // View Rect
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetClipRect(const Rect& rectClip) override
    {
        m_rectClip       = rectClip;
        m_rectClipScreen =
            Rect(
                rectClip.XMin(),
                m_sizeSurface.Y() - rectClip.YMax(),
                rectClip.XMax(),
                m_sizeSurface.Y() - rectClip.YMin()

            );

        m_bUpdateMatFull = true;
        m_prasterizer->SetClipRect(m_rectClipScreen);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void DirectionalLight(const Vector& vec, const Color& color) override
    {
        m_vecLightWorld  = m_mat.TransformDirection(vec);
        m_bBidirectional = false;
    }

    void BidirectionalLight(const Vector& vec, const Color& color, const Color& colorAlt) override
    {
        m_vecLightWorld  = m_mat.TransformDirection(vec);
        m_color          = color;
        m_colorAlt       = colorAlt;
        m_bBidirectional = true;
    }

    void SetAmbientLevel(float level) override
    {
        m_brightAmbient = level;
    }

    void SetGlobalColor(const Color& color) override
    {
        m_colorGlobal = color;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    const Matrix& GetFullMatrix()
    {
        if (m_bUpdateMatFull) {
            m_bUpdateMatFull = false;

            m_matFull = m_mat;
            m_matFull.Multiply(m_matPerspective);

            // !!! In D3D pixels centers are on integer boundaries
            //     The device clips to integer boundaries
            //     subtract 0.25 to move the first row of pixels centers into the clip rect

            if (m_bClip) {
                Point pointCenter = m_rectClip.Center();

                //
                // move view rect into NDC
                //

                float scalex = 0.5f * m_rectClip.XSize();
                float scaley = 0.5f * m_rectClip.YSize();

                m_matFull.Translate(
                    Vector(
                        -pointCenter.X(),
                        -pointCenter.Y(),
                        0
                    )
                );
                m_matFull.Scale(
                    Vector(
                        1 / scalex,
                        1 / scaley,
                        1.0f
                    )
                );

                //
                // these are the scales and translates to go from NDC back to the screen
                //

                m_xoffset = pointCenter.X(); // !!! - 0.25f;
                m_yoffset = m_sizeSurface.Y() - pointCenter.Y(); // !!! - 0.25f;

                m_scalex =  0.5f * m_rectClip.XSize();
                m_scaley = -0.5f * m_rectClip.YSize();
            } else {
                //
                // Flip the y coordinate
                //

                m_matFull.Translate(Vector(0, -m_sizeSurface.Y(), 0));
                m_matFull.Scale(Vector(1, -1, 1));
            }
        }

        return m_matFull;
    }

    void UpdateLighting()
    {
        if (m_bUpdateLighting) {
            m_bUpdateLighting = false;

            m_vecLight = GetInverseModelMatrix().TransformDirection(m_vecLightWorld);
            m_vecLight.SetNormalize();
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Update Pointers
    //
    //////////////////////////////////////////////////////////////////////////////

    void UpdatePointers()
    {
        if (m_bUpdatePointers) {
            m_bUpdatePointers = false;

            bool bClip = m_bClip || m_countPlanes > 6;

            if (m_pdeform != nullptr) {
                bClip = true;
                m_pfnTransform = &Device3D::TransformDeformClip;
            } else if (bClip) {
                m_pfnTransform = &Device3D::TransformClip;
            } else {
                m_pfnTransform = &Device3D::TransformNoClip;
            }

            m_pfnLightVertexL = m_pfnTransform;

            switch (m_shadeMode) {
                case ShadeModeNone:
                case ShadeModeCopy:
                    if (bClip) {
                        m_pfnLightVertex = &Device3D::LightVertexCopy;
                    } else {
                        m_pfnLightVertex = &Device3D::TransformNoClipNoLight;
                    }
                    break;

                case ShadeModeGlobalColor:
                    m_pfnLightVertex = &Device3D::LightVertexGlobalColor;
                    break;

                case ShadeModeFlat:
                case ShadeModeGouraud:
                    if (m_pmaterial) {
                        m_pfnLightVertex = &Device3D::MaterialLightVertex;
                    } else if (m_bBidirectional) {
                        m_pfnLightVertex = &Device3D::BidirectionalLightVertex;
                    } else {
                        if (bClip) {
                            m_pfnLightVertex = &Device3D::LightVertex;
                        } else {
                            m_pfnLightVertex = &Device3D::TransformAndLightNoClip;
                        }
                    }
                    break;

                default:
                    ZError("Invalid ShadeMode");
            }

            //
            // Lines
            //

            if (
                   (m_lineWidth == 0.5f)
                && (
                       m_prasterizer->Has3DAcceleration()
                    || (
                           (m_lineWidth == 0.5f)
                        && (!m_bZTest)
                        && (!m_bZWrite)
                        && (m_shadeMode == ShadeModeFlat)
                        && (m_blendMode == BlendModeSource)
                       )
                   )
            ) {
                m_pfnDrawLines = &Device3D::DrawLinesRasterizer;
            } else {
                m_pfnDrawLines = &Device3D::DrawLinesWithPolygons;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Get State
    //
    //////////////////////////////////////////////////////////////////////////////

    bool GetClipping() override
    {
        return m_bClip;
    }

    const Matrix& GetMatrix() override
    {
        return m_mat;
    }

    const Matrix& GetInverseModelMatrix() override
    {
        if (m_bUpdateInverse) {
            m_bUpdateInverse = false;
            m_matInverse.SetInverse(m_mat);
        }

        return m_matInverse;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Set State
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetMatrix(const Matrix& mat) override
    {
        m_mat             = mat;
        m_bUpdateMatFull  = true;
        m_bUpdateLighting = true;
        m_bUpdateInverse  = true;
    }

    void SetPerspectiveMatrix(const Matrix& mat) override
    {
        m_matPerspective = mat;
        m_bUpdateMatFull = true;
    }

    void SetClipping(bool bClip) override
    {
        m_bClip = bClip;
        m_bUpdateMatFull  = true;
        m_bUpdatePointers = true;
    }

    void RemoveClipPlane(int indexRemove) override
    {
        ZAssert(indexRemove >= 0 && indexRemove + 6 < m_countPlanes);

        for(int index = m_countPlanes - indexRemove - 1; index < m_countPlanes - 1; index++) {
            m_pplane[index] = m_pplane[index + 1];
        }

        m_countPlanes--;
    }

    void AddClipPlane(const Plane& plane) override
    {
        ZAssert(m_countPlanes < g_maxCountPlanes);

        //
        // transform the plane into eye coordinates
        //

        const Matrix& mat = GetFullMatrix();
        Matrix mati;

        mati.SetInverse(mat);
        mati.Transpose();

        HVector p  = plane.GetHVector();
        HVector pp = mati.Transform(p);

        m_pplane[m_countPlanes] = Plane(pp);

        m_countPlanes++;
    }

    void SetShadeMode(ShadeMode shadeMode) override
    {
        m_shadeMode = shadeMode;
        m_bUpdatePointers = true;
        m_prasterizer->SetShadeMode(shadeMode);
    }

    void SetMaterial(Material* pmaterial) override
    {
        m_pmaterial = pmaterial;
        m_bUpdatePointers = true;
    }

    void SetBlendMode(BlendMode blendMode) override
    {
        m_blendMode = blendMode;
        m_bUpdatePointers = true;
        m_prasterizer->SetBlendMode(blendMode);
    }

    void SetTexture(Surface* psurfaceTexture) override
    {
        m_prasterizer->SetTexture(psurfaceTexture);
    }

    void SetWrapMode(WrapMode wrapMode) override
    {
        m_prasterizer->SetWrapMode(wrapMode);
    }

    void SetCullMode(CullMode cullMode) override
    {
        m_prasterizer->SetCullMode(cullMode);
    }

    void SetZTest(bool bZTest) override
    {
        m_bZTest = bZTest;
        m_bUpdatePointers = true;
        m_prasterizer->SetZTest(bZTest);
    }

    void SetZWrite(bool bZWrite) override
    {
        m_bZWrite = bZWrite;
        m_bUpdatePointers = true;
        m_prasterizer->SetZWrite(bZWrite);
    }

    void SetLinearFilter(bool bLinearFilter) override
    {
        m_prasterizer->SetLinearFilter(bLinearFilter);
    }

    void SetPerspectiveCorrection(bool bPerspectiveCorrection) override
    {
        m_prasterizer->SetPerspectiveCorrection(bPerspectiveCorrection);
    }

    void SetDither(bool bDither) override
    {
        m_prasterizer->SetDither(bDither);
    }

    void SetColorKey(bool bColorKey) override
    {
        m_prasterizer->SetColorKey(bColorKey);
    }

    void SetLineWidth(float width) override
    {
        m_lineWidth = 0.5f * width;
    }

    void SetDeformation(Deformation* pdeform) override
    {
        m_pdeform = pdeform;
        m_bUpdatePointers = true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Buffers
    //
    //////////////////////////////////////////////////////////////////////////////

    //
    //  , may want to grow to more than count
    //

    void SizeVertexLBuffer(int count)
    {
        if (m_countVertexLBuffer < count) {
            m_countVertexLBuffer = count;
            m_pvertexLBuffer = (VertexL*)realloc(m_pvertexLBuffer, sizeof(VertexL) * count);
        }
    }

    void SizeVertexTLBuffer(int count)
    {
        if (m_countVertexTLBuffer < count) {
            m_countVertexTLBuffer = count;
            m_pvertexTLBuffer = (VertexTL*)realloc(m_pvertexTLBuffer, sizeof(VertexTL) * count);
        }
    }

    void SizeVertexScreenBuffer(int count)
    {
        if (m_countVertexScreenBuffer < count) {
            m_pvertexScreenBuffer = (VertexScreen*)realloc(m_pvertexScreenBuffer, sizeof(VertexScreen) * count);
            for (int index = m_countVertexScreenBuffer; index < count; index++) {
                m_pvertexScreenBuffer[index].specular = 0;
            }
            m_countVertexScreenBuffer = count;
        }
    }

    void SizeIndexBuffer(int count)
    {
        if (m_countIndexBuffer < count) {
            m_countIndexBuffer = count;
            m_pindexBuffer = (MeshIndex*)realloc(m_pindexBuffer, sizeof(MeshIndex) * count);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Vertex Buffers
    //
    //////////////////////////////////////////////////////////////////////////////

    VertexL* GetVertexLBuffer(int count) override
    {
        SizeVertexLBuffer(count);
        return m_pvertexLBuffer;
    }

    VertexScreen* GetVertexScreenBuffer(int count) override
    {
        SizeVertexScreenBuffer(count);
        return m_pvertexScreenBuffer;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void StoreTriangle(MeshIndex index1, MeshIndex index2, MeshIndex index3)
    {
        SizeIndexBuffer(m_indexIndexBuffer + 3);

        m_pindexBuffer[m_indexIndexBuffer    ] = index1;
        m_pindexBuffer[m_indexIndexBuffer + 1] = index2;
        m_pindexBuffer[m_indexIndexBuffer + 2] = index3;

        m_indexIndexBuffer += 3;
    }

    void StoreLine(MeshIndex index1, MeshIndex index2)
    {
        SizeIndexBuffer(m_indexIndexBuffer + 2);

        m_pindexBuffer[m_indexIndexBuffer    ] = index1;
        m_pindexBuffer[m_indexIndexBuffer + 1] = index2;

        m_indexIndexBuffer += 2;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClampVertex(VertexScreen& vertex)
    {
        if (vertex.x < m_rectClipScreen.XMin()) {
            vertex.x = m_rectClipScreen.XMin();
        } else if (vertex.x > m_rectClipScreen.XMax()) {
            vertex.x = m_rectClipScreen.XMax();
        }

        if (vertex.y < m_rectClipScreen.YMin()) {
            vertex.y = m_rectClipScreen.YMin();
        } else if (vertex.y > m_rectClipScreen.YMax()) {
            vertex.y = m_rectClipScreen.YMax();
        }

        if (vertex.z < 0) {
            vertex.z = 0;
        } else if (vertex.z > 1) {
            vertex.z = 1;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    MeshIndex ConvertVertex(VertexTL& vertex)
    {
        SizeVertexScreenBuffer(m_indexVertexScreenBuffer + 1);

        vertex.m_index = m_indexVertexScreenBuffer;
        VertexScreen* pvertex = m_pvertexScreenBuffer + m_indexVertexScreenBuffer;

        float rhw  = 1.0f / vertex.m_pos.w;

        pvertex->x        = m_scalex * rhw * vertex.m_pos.x + m_xoffset;
        pvertex->y        = m_scaley * rhw * vertex.m_pos.y + m_yoffset;
        pvertex->z        = rhw * vertex.m_pos.z;
        pvertex->qw       = rhw;
        pvertex->u        = vertex.m_pointTexture.X();
        pvertex->v        = vertex.m_pointTexture.Y();
        pvertex->color    = MakeD3DCOLOR(vertex.m_color);
        pvertex->specular = 0;

        ClampVertex(*pvertex);

        m_indexVertexScreenBuffer++;

        return vertex.m_index;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    MeshIndex TranslateIndex(VertexTL& vertex)
    {
        if (vertex.m_index == 0xffff) {
            return ConvertVertex(vertex);
        }

        return vertex.m_index;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    int TranslateIntersection(
        const Plane&    plane,
        const VertexTL& vertex0, 
        const VertexTL& vertex1 
    ) {
        VertexTL vertex;

        CalculateIntersection(plane, vertex0, vertex1, vertex);
        return ConvertVertex(vertex);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Plane
    //
    //////////////////////////////////////////////////////////////////////////////

    void CalculateIntersection(
        const Plane&     plane,
        const VertexTL&  vertex0,
        const VertexTL&  vertex1,
              VertexTL&  vertexOut
    ) {
        float alpha = plane.Intersect(vertex0.m_pos, vertex1.m_pos);

        vertexOut.Interpolate(vertex0, vertex1, alpha);
        vertexOut.m_index = 0xffff;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip a polygon to a plane
    //
    //////////////////////////////////////////////////////////////////////////////

    int ClipPolygonToPlane(
        const Plane&     plane,
              Code       bit,
              VertexTL*  pvertexIn,
              int        vcountIn,
              VertexTL*  pvertexOut
    ) {
        int  vcount         = 0;
        int  vindexPrevious = vcountIn - 1;
        Code codePrevious   = pvertexIn[vindexPrevious].m_code & bit;

        for (int vindex = 0; vindex < vcountIn; vindex++) {
            Code code = pvertexIn[vindex].m_code & bit;

            if ((code ^ codePrevious) != 0) {
                CalculateIntersection(
                    plane,
                    pvertexIn[vindexPrevious],
                    pvertexIn[vindex],
                    pvertexOut[vcount]
                );

                pvertexOut[vcount].UpdateClipCode();
                if (m_countPlanes > 6) {
                    Code code = ClipPlane;

                    for (int index = 6; index < m_countPlanes; index++, code <<= 1) {
                        pvertexOut[vcount].UpdateClipCode(m_pplane[index], code);
                    }
                }
                vcount++;
            }

            if (code == 0) {
                pvertexOut[vcount] = pvertexIn[vindex];
                vcount++;
            }

            ZAssert(vcount <= 8);

            codePrevious = code;
            vindexPrevious = vindex;
        }

        return vcount;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip a polygon to a set of planess
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipPolygonToPlanes(
        const Plane* pplane,
              int        pcount,
              Code       codeOr,
        const VertexTL&  vertex0,
        const VertexTL&  vertex1,
        const VertexTL&  vertex2
    ) {
        VertexTL pverticesIn[3] = 
            { 
                vertex0, 
                vertex1, 
                vertex2 
            };
        VertexTL pverticesOut0[8];
        VertexTL pverticesOut1[8];

        VertexTL* pvertexIn    = pverticesIn;
        VertexTL* pvertexOut   = pverticesOut0;
        VertexTL* pvertexOther = pverticesOut1;

        int  vcount = 3;
        Code bit    = 1;
        int  pindex = 0;

        while (pindex < pcount) {
            if ((codeOr & bit) != 0) {
                vcount = ClipPolygonToPlane(pplane[pindex], bit, pvertexIn, vcount, pvertexOut);

                pvertexIn    = pvertexOut;
                pvertexOut   = pvertexOther;
                pvertexOther = pvertexIn;
            }

            bit *= 2;
            pindex++;
        }

        //
        // Translate the vertices to screen coordinates
        //

        MeshIndex aindex[7];
        int index;

        for(index = 0; index < vcount; index++) {
            aindex[index] = TranslateIndex(pvertexIn[index]);
        }

        //
        // Form all of the triangle fans
        //

        for(index = 1; index < vcount - 1; index++) {
            StoreTriangle(
                aindex[0],
                aindex[index],
                aindex[index + 1]
            );
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipPolygonToPlane(
        const Plane&     plane,
              Code       code0,
              Code       code1,
              Code       code2,
              VertexTL&  vertex0,
              VertexTL&  vertex1,
              VertexTL&  vertex2
    ) {
        if (code0 != 0) {
            if (code1 != 0) {
                // p0 and p1 outside

                StoreTriangle(
                    TranslateIntersection(plane, vertex2, vertex0),
                    TranslateIntersection(plane, vertex1, vertex2),
                    TranslateIndex(vertex2)
                );
            } else if (code2 != 0) {
                // p0 and p2 outside

                StoreTriangle(
                    TranslateIntersection(plane, vertex0, vertex1),
                    TranslateIndex(vertex1),
                    TranslateIntersection(plane, vertex2, vertex1)
                );
            } else {
                // p0 outside

                int index0 = TranslateIntersection(plane, vertex0, vertex1);
                int index2 = TranslateIndex(vertex2);

                StoreTriangle(
                    index0,
                    TranslateIndex(vertex1),
                    index2
                );

                StoreTriangle(
                    index0,
                    index2,
                    TranslateIntersection(plane, vertex0, vertex2)
                );
            }
        } else {
            if (code1 != 0) {
                if (code2 != 0) {
                    // p1 and p2 outside

                    StoreTriangle(
                        TranslateIndex(vertex0),
                        TranslateIntersection(plane, vertex0, vertex1),
                        TranslateIntersection(plane, vertex2, vertex0)
                    );
                } else {
                    // p1 outside

                    int index0 = TranslateIndex(vertex0);
                    int index2 = TranslateIntersection(plane, vertex1, vertex2);

                    StoreTriangle(
                        index0,
                        TranslateIntersection(plane, vertex0, vertex1),
                        index2
                    );

                    StoreTriangle(
                        index0,
                        index2,
                        TranslateIndex(vertex2)
                    );
                }
            } else {
                // p2 outside

                int index0 = TranslateIndex(vertex0);
                int index2 = TranslateIntersection(plane, vertex1, vertex2);

                StoreTriangle(
                    index0,
                    TranslateIndex(vertex1),
                    index2
                );

                StoreTriangle(
                    index0,
                    index2,
                    TranslateIntersection(plane, vertex2, vertex0)
                );
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip Triangles
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipTriangles(VertexTL* pvertex, int vcountIn, const MeshIndex* pindex, int icountIn)
    {
        //
        // Clip each triangle
        //

        for(int index = 0; index < icountIn; index += 3) {
            int index0 = pindex[index + 0];
            int index1 = pindex[index + 1];
            int index2 = pindex[index + 2];

            Code code0 = pvertex[index0].m_code;
            Code code1 = pvertex[index1].m_code;
            Code code2 = pvertex[index2].m_code;

            Code codeAnd = code0 & code1 & code2;

            if (codeAnd == 0) {
                Code codeOr  = code0 | code1 | code2;

                if (codeOr == 0) {
                    //
                    // Not clipped
                    //

                    StoreTriangle(
                        TranslateIndex(pvertex[index0]),
                        TranslateIndex(pvertex[index1]),
                        TranslateIndex(pvertex[index2])
                    );
                } else {
                    //
                    // Partially clipped
                    //

                    Code bit = 1;
                    int pindex = 0;

                    while (
                           (pindex < m_countPlanes)
                        && (codeOr != bit)
                    ) {
                        pindex++;
                        bit *= 2;
                    }

                    if (pindex < m_countPlanes) {
                        //
                        // Clipped by a single plane
                        //

                        ClipPolygonToPlane(
                            m_pplane[pindex],
                            code0,
                            code1,
                            code2,
                            pvertex[index0],
                            pvertex[index1],
                            pvertex[index2]
                        );
                    } else {
                        //
                        // Clipped by multiple planes
                        //

                        ClipPolygonToPlanes(
                            m_pplane,
                            m_countPlanes,
                            codeOr,
                            pvertex[index0],
                            pvertex[index1],
                            pvertex[index2]
                        );
                    }
                }
            } else {
                //
                // Completely clipped
                //

                int i = 0;
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip a line to a set of planess
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipLineToPlanes(
        const Plane* pplane,
              int        pcount,
              Code       codeOr,
        const VertexTL&  vertex0,
        const VertexTL&  vertex1
    ) {
        float t0 = 0;
        float t1 = 1;

        int  vcount = 3;
        Code bit    = 1;
        int  pindex = 0;

        while (pindex < pcount) {
            if ((codeOr & bit) != 0) {
                float t = pplane[pindex].Intersect(vertex0.m_pos, vertex1.m_pos);

                if ((vertex0.m_code & bit) != 0) {
                    if (t > t0) {
                        t0 = t;
                    }
                } else {
                    if (t < t1) {
                        t1 = t;
                    }
                }
            }

            bit *= 2;
            pindex++;
        }

        if (t0 < t1) {
            VertexTL vertexT0; vertexT0.Interpolate(vertex0, vertex1, t0);
            VertexTL vertexT1; vertexT1.Interpolate(vertex0, vertex1, t1);

            StoreLine(
                ConvertVertex(vertexT0),
                ConvertVertex(vertexT1)
            );
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip Lines
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipLines(VertexTL* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        for (int index = 0; index < icount; index += 2) {
            int index0 = pindex[index + 0];
            int index1 = pindex[index + 1];

            Code code0 = pvertex[index0].m_code;
            Code code1 = pvertex[index1].m_code;

            Code codeAnd = code0 & code1;

            if (codeAnd == 0) {
                Code codeOr  = code0 | code1;

                if (codeOr == 0) {
                    //
                    // Not clipped
                    //

                    StoreLine(
                        TranslateIndex(pvertex[index0]),
                        TranslateIndex(pvertex[index1])
                    );
                } else {
                    //
                    // Clipped
                    //

                    ClipLineToPlanes(
                        m_pplane,
                        m_countPlanes,
                        codeOr,
                        pvertex[index0],
                        pvertex[index1]
                    );
                }
            } else {
                //
                // Completely clipped
                //
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clip Points
    //
    //////////////////////////////////////////////////////////////////////////////

    void ClipPoints(VertexTL* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        for(int index = 0; index < vcount; index++) {
            if (pvertex[index].m_code == 0) {
                ConvertVertex(pvertex[index]);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Transform with deformation and clipping
    //
    //////////////////////////////////////////////////////////////////////////////

    void TransformDeformClip(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexTLBuffer(vcount);

        Matrix mat = m_mat;
        m_mat.SetIdentity();
        m_bUpdateMatFull = true;
        const Matrix& matFull = GetFullMatrix();

        for (int index = 0; index < vcount; index++) {
            Vector vecLocal = Vector(pvertex[index].x, pvertex[index].y, pvertex[index].z);

            HVector hvec(
                m_pdeform->Deform(
                    mat.Transform(
                        vecLocal
                    )
                )
            );

            m_pvertexTLBuffer[index].m_pos          = matFull.Transform(hvec);
            m_pvertexTLBuffer[index].m_pointTexture = Point(pvertex[index].u, pvertex[index].v);
            m_pvertexTLBuffer[index].m_color        = 
                Color(
                    pvertex[index].r,
                    pvertex[index].g,
                    pvertex[index].b,
                    pvertex[index].a
                );
            m_pvertexTLBuffer[index].m_index = 0xffff;

            m_pvertexTLBuffer[index].UpdateClipCode();
        }

        if (m_countPlanes > 6) {
            for (int index = 0; index < vcount; index++) {
                Code code = ClipPlane;

                for (int iplane = 6; iplane < m_countPlanes; iplane++, code <<= 1) {
                    m_pvertexTLBuffer[index].UpdateClipCode(m_pplane[iplane], code);
                }
            }
        }

        m_indexVertexScreenBuffer = 0;
        m_indexIndexBuffer = 0;

        (this->*pfnClip)(m_pvertexTLBuffer, vcount, pindex, icount);

        if (m_indexVertexScreenBuffer != 0) {
            (this->*pfnDraw)(
                m_pvertexScreenBuffer, 
                m_indexVertexScreenBuffer, 
                m_pindexBuffer, 
                m_indexIndexBuffer
            );
        }

        m_mat = mat;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Transform with clipping
    //
    //////////////////////////////////////////////////////////////////////////////

    void TransformClip(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexTLBuffer(vcount);

        const Matrix& mat = GetFullMatrix();

        for (int index = 0; index < vcount; index++) {
            HVector hvec(
                pvertex[index].x,
                pvertex[index].y,
                pvertex[index].z,
                1
            );

            m_pvertexTLBuffer[index].m_pos          = mat.Transform(hvec);
            m_pvertexTLBuffer[index].m_pointTexture = Point(pvertex[index].u, pvertex[index].v);
            m_pvertexTLBuffer[index].m_color        = 
                Color(
                    pvertex[index].r,
                    pvertex[index].g,
                    pvertex[index].b,
                    pvertex[index].a
                );
            m_pvertexTLBuffer[index].m_index = 0xffff;

            m_pvertexTLBuffer[index].UpdateClipCode();
        }

        if (m_countPlanes > 6) {
            for (int index = 0; index < vcount; index++) {
                Code code = ClipPlane;

                for (int iplane = 6; iplane < m_countPlanes; iplane++, code <<= 1) {
                    m_pvertexTLBuffer[index].UpdateClipCode(m_pplane[iplane], code);
                }
            }
        }

        m_indexVertexScreenBuffer = 0;
        m_indexIndexBuffer = 0;

        (this->*pfnClip)(m_pvertexTLBuffer, vcount, pindex, icount);

        if (m_indexVertexScreenBuffer != 0) {
            (this->*pfnDraw)(
                m_pvertexScreenBuffer, 
                m_indexVertexScreenBuffer, 
                m_pindexBuffer, 
                m_indexIndexBuffer
            );
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Special case Transform and light without clipping
    //
    //////////////////////////////////////////////////////////////////////////////

    void TransformAndLightNoClip(
        const Vertex* pvertex, 
        int vcount, 
        const MeshIndex* pindex, 
        int icount, 
        PFNClip pfnClip, 
        PFNDraw pfnDraw
    ) {
        SizeVertexScreenBuffer(vcount);

        UpdateLighting();
        const Matrix& mat = GetFullMatrix();

        for (int index = 0; index < vcount; index++) {
            const float x = pvertex[index].x;
            const float y = pvertex[index].y;
            const float z = pvertex[index].z;

            const float rhw = 1.0f / (mat[3][0] * x + mat[3][1] * y + mat[3][2] * z + mat[3][3]);

            m_pvertexScreenBuffer[index].x  = rhw * (mat[0][0] * x + mat[0][1] * y + mat[0][2] * z + mat[0][3]);
            m_pvertexScreenBuffer[index].y  = rhw * (mat[1][0] * x + mat[1][1] * y + mat[1][2] * z + mat[1][3]);
            m_pvertexScreenBuffer[index].z  = rhw * (mat[2][0] * x + mat[2][1] * y + mat[2][2] * z + mat[2][3]);
            m_pvertexScreenBuffer[index].qw = rhw;
            m_pvertexScreenBuffer[index].u  = pvertex[index].u;
            m_pvertexScreenBuffer[index].v  = pvertex[index].v;

            {
                float fbright =
                    255.0f * (
                          pvertex[index].nx * m_vecLight.X()
                        + pvertex[index].ny * m_vecLight.Y()
                        + pvertex[index].nz * m_vecLight.Z()
                        + m_brightAmbient
                    );

                int bright = MakeInt(fbright);

                if (bright > 255) {
                    bright = 255;
                } else if (bright < 0) {
                    bright = 0;
                }

                m_pvertexScreenBuffer[index].color = 0xff000000 | bright | (bright << 8) | (bright << 16);
            }
        }

        (this->*pfnDraw)(m_pvertexScreenBuffer, vcount, pindex, icount);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Tranform without clipping
    //
    //////////////////////////////////////////////////////////////////////////////

    void TransformNoClip(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexScreenBuffer(vcount);

        const Matrix& mat = GetFullMatrix();

        for (int index = 0; index < vcount; index++) {
            const float x = pvertex[index].x;
            const float y = pvertex[index].y;
            const float z = pvertex[index].z;

            const float rhw = 1.0f / (mat[3][0] * x + mat[3][1] * y + mat[3][2] * z + mat[3][3]);

            m_pvertexScreenBuffer[index].x  = rhw * (mat[0][0] * x + mat[0][1] * y + mat[0][2] * z + mat[0][3]);
            m_pvertexScreenBuffer[index].y  = rhw * (mat[1][0] * x + mat[1][1] * y + mat[1][2] * z + mat[1][3]);
            m_pvertexScreenBuffer[index].z  = rhw * (mat[2][0] * x + mat[2][1] * y + mat[2][2] * z + mat[2][3]);
            m_pvertexScreenBuffer[index].qw = rhw;
            m_pvertexScreenBuffer[index].u  = pvertex[index].u;
            m_pvertexScreenBuffer[index].v  = pvertex[index].v;

            m_pvertexScreenBuffer[index].color = 
                MakeD3DCOLOR(
                    Color(
                        pvertex[index].r,
                        pvertex[index].g,
                        pvertex[index].b,
                        pvertex[index].a
                    )
                );
        }

        (this->*pfnDraw)(m_pvertexScreenBuffer, vcount, pindex, icount);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Tranform without clipping or lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void TransformNoClipNoLight(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexScreenBuffer(vcount);

        const Matrix& mat = GetFullMatrix();

        for (int index = 0; index < vcount; index++) {
            const float x = pvertex[index].x;
            const float y = pvertex[index].y;
            const float z = pvertex[index].z;

            const float rhw = 1.0f / (mat[3][0] * x + mat[3][1] * y + mat[3][2] * z + mat[3][3]);

            m_pvertexScreenBuffer[index].x  = rhw * (mat[0][0] * x + mat[0][1] * y + mat[0][2] * z + mat[0][3]);
            m_pvertexScreenBuffer[index].y  = rhw * (mat[1][0] * x + mat[1][1] * y + mat[1][2] * z + mat[1][3]);
            m_pvertexScreenBuffer[index].z  = rhw * (mat[2][0] * x + mat[2][1] * y + mat[2][2] * z + mat[2][3]);
            m_pvertexScreenBuffer[index].qw = rhw;
            m_pvertexScreenBuffer[index].u  = pvertex[index].u;
            m_pvertexScreenBuffer[index].v  = pvertex[index].v;
        }

        (this->*pfnDraw)(m_pvertexScreenBuffer, vcount, pindex, icount);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Copy Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void LightVertexCopy(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexLBuffer(vcount);

        for (int index = 0; index < vcount; index++) {
            m_pvertexLBuffer[index].x = pvertex[index].x;
            m_pvertexLBuffer[index].y = pvertex[index].y;
            m_pvertexLBuffer[index].z = pvertex[index].z;
            m_pvertexLBuffer[index].u = pvertex[index].u;
            m_pvertexLBuffer[index].v = pvertex[index].v;
            m_pvertexLBuffer[index].r = 1;
            m_pvertexLBuffer[index].g = 1;
            m_pvertexLBuffer[index].b = 1;
            m_pvertexLBuffer[index].a = 1;
        }

        (this->*m_pfnTransform)(m_pvertexLBuffer, vcount, pindex, icount, pfnClip, pfnDraw);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Copy Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void LightVertexGlobalColor(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        SizeVertexLBuffer(vcount);

        float r = m_colorGlobal.R();
        float g = m_colorGlobal.G();
        float b = m_colorGlobal.B();
        float a = m_colorGlobal.A();

        for (int index = 0; index < vcount; index++) {
            m_pvertexLBuffer[index].x = pvertex[index].x;
            m_pvertexLBuffer[index].y = pvertex[index].y;
            m_pvertexLBuffer[index].z = pvertex[index].z;
            m_pvertexLBuffer[index].u = pvertex[index].u;
            m_pvertexLBuffer[index].v = pvertex[index].v;
            m_pvertexLBuffer[index].r = r;
            m_pvertexLBuffer[index].g = g;
            m_pvertexLBuffer[index].b = b;
            m_pvertexLBuffer[index].a = a;
        }

        (this->*m_pfnTransform)(m_pvertexLBuffer, vcount, pindex, icount, pfnClip, pfnDraw);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Bi-Directional Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    float Saturate(float value)
    {
        if (value > 1) {
            return 1;
        } else if (value < 0) {
            return 0;
        }

        return value;
    }

    void BidirectionalLightVertex(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        UpdateLighting();

        SizeVertexLBuffer(vcount);

        for (int index = 0; index < vcount; index++) {
            m_pvertexLBuffer[index].x = pvertex[index].x;
            m_pvertexLBuffer[index].y = pvertex[index].y;
            m_pvertexLBuffer[index].z = pvertex[index].z;
            m_pvertexLBuffer[index].u = pvertex[index].u;
            m_pvertexLBuffer[index].v = pvertex[index].v;

            float bright =
                  pvertex[index].nx * m_vecLight.X()
                + pvertex[index].ny * m_vecLight.Y()
                + pvertex[index].nz * m_vecLight.Z();

            if (bright >= 0) {
                //bright += m_brightAmbient;
                /*
                bright = Saturate(bright);

                m_pvertexLBuffer[index].r = bright * m_color.R();
                m_pvertexLBuffer[index].g = bright * m_color.G();
                m_pvertexLBuffer[index].b = bright * m_color.B();
                */

                m_pvertexLBuffer[index].r = Saturate(bright * m_color.R() + m_brightAmbient);
                m_pvertexLBuffer[index].g = Saturate(bright * m_color.G() + m_brightAmbient);
                m_pvertexLBuffer[index].b = Saturate(bright * m_color.B() + m_brightAmbient);
            } else {
                bright = /*m_brightAmbient*/ - bright;
                /*
                bright = Saturate(bright);

                m_pvertexLBuffer[index].r = bright * m_colorAlt.R();
                m_pvertexLBuffer[index].g = bright * m_colorAlt.G();
                m_pvertexLBuffer[index].b = bright * m_colorAlt.B();
                */

                m_pvertexLBuffer[index].r = Saturate(bright * m_colorAlt.R() + m_brightAmbient);
                m_pvertexLBuffer[index].g = Saturate(bright * m_colorAlt.G() + m_brightAmbient);
                m_pvertexLBuffer[index].b = Saturate(bright * m_colorAlt.B() + m_brightAmbient);
            }

            m_pvertexLBuffer[index].a = 1;
        }

        (this->*m_pfnTransform)(m_pvertexLBuffer, vcount, pindex, icount, pfnClip, pfnDraw);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Directional Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void LightVertex(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        UpdateLighting();

        SizeVertexLBuffer(vcount);

        for (int index = 0; index < vcount; index++) {
            m_pvertexLBuffer[index].x = pvertex[index].x;
            m_pvertexLBuffer[index].y = pvertex[index].y;
            m_pvertexLBuffer[index].z = pvertex[index].z;
            m_pvertexLBuffer[index].u = pvertex[index].u;
            m_pvertexLBuffer[index].v = pvertex[index].v;

            float bright =
                  pvertex[index].nx * m_vecLight.X()
                + pvertex[index].ny * m_vecLight.Y()
                + pvertex[index].nz * m_vecLight.Z()
                + m_brightAmbient;

            if (bright > 1) {
                bright = 1;
            } else if (bright < 0) {
                bright = 0;
            }

            m_pvertexLBuffer[index].r = bright;
            m_pvertexLBuffer[index].g = bright;
            m_pvertexLBuffer[index].b = bright;
            m_pvertexLBuffer[index].a = 1;
        }

        (this->*m_pfnTransform)(m_pvertexLBuffer, vcount, pindex, icount, pfnClip, pfnDraw);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Directional Lighting
    //
    //////////////////////////////////////////////////////////////////////////////

    void MaterialLightVertex(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount, PFNClip pfnClip, PFNDraw pfnDraw)
    {
        UpdateLighting();

        SizeVertexLBuffer(vcount);

        float r = m_pmaterial->GetDiffuse().GetRed();
        float g = m_pmaterial->GetDiffuse().GetGreen();
        float b = m_pmaterial->GetDiffuse().GetBlue();
        float a = m_pmaterial->GetDiffuse().GetAlpha();

        for (int index = 0; index < vcount; index++) {
            m_pvertexLBuffer[index].x = pvertex[index].x;
            m_pvertexLBuffer[index].y = pvertex[index].y;
            m_pvertexLBuffer[index].z = pvertex[index].z;
            m_pvertexLBuffer[index].u = pvertex[index].u;
            m_pvertexLBuffer[index].v = pvertex[index].v;

            float bright =
                  pvertex[index].nx * m_vecLight.X()
                + pvertex[index].ny * m_vecLight.Y()
                + pvertex[index].nz * m_vecLight.Z()
                + m_brightAmbient;

            if (bright > 1) {
                bright = 1;
            } else if (bright < 0) {
                bright = 0;
            }

            m_pvertexLBuffer[index].r = r * bright;
            m_pvertexLBuffer[index].g = g * bright;
            m_pvertexLBuffer[index].b = b * bright;
            m_pvertexLBuffer[index].a = a;
        }

        (this->*m_pfnTransform)(m_pvertexLBuffer, vcount, pindex, icount, pfnClip, pfnDraw);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Draw lines using triangles
    //
    //////////////////////////////////////////////////////////////////////////////

    void StoreVertex(
        VertexScreen& pvertex,
        float    x,
        float    y,
        float    z,
        float    qw,
        D3DCOLOR color,
        float    u,
        float    v
    ) {
        pvertex.x        = x;
        pvertex.y        = y;
        pvertex.z        = z;
        pvertex.qw       = qw;
        pvertex.color    = color;
        pvertex.specular = 0;
        pvertex.u        = u;
        pvertex.v        = v;
    }

    void DrawLinesWithPolygons(const VertexScreen* pvertexInArg, int vcount, const MeshIndex* pindexInArg, int icount)
    {
        bool bUsingVertexBuffer = (pvertexInArg == m_pvertexScreenBuffer);
        bool bUsingIndexBuffer  = (pindexInArg  == m_pindexBuffer       );

        //
        // Allocate space for the triangles
        //

        SizeIndexBuffer(icount + icount * 3);

        VertexScreen* pvertexOut = GetVertexScreenBuffer(vcount + icount * 2) + vcount;
        MeshIndex*    pindexOut  = m_pindexBuffer + icount;

        //
        // Figure out which source index point to use
        //

        const VertexScreen* pvertexIn = bUsingVertexBuffer ? m_pvertexScreenBuffer : pvertexInArg;
        const MeshIndex*    pindexIn  = bUsingIndexBuffer  ? m_pindexBuffer        : pindexInArg ;

        //
        // Build all of the triangles
        //

        Vector vecUp(0, 0, 1);
        VertexScreen* pvertex  = pvertexOut;
        MeshIndex*    pindex   = pindexOut;
        int           indexOut = 0;

        for (int index = 0; index < icount; index += 2) {
            const VertexScreen& vertex1 = pvertexIn[pindexIn[index    ]];
            const VertexScreen& vertex2 = pvertexIn[pindexIn[index + 1]];

            if (
                   vertex1.x != vertex2.x 
                || vertex1.y != vertex2.y
            ) {
                Vector vecForward(vertex2.x - vertex1.x, vertex2.y - vertex1.y, 0);
                Vector vecLeft = CrossProduct(vecUp, vecForward);
                vecLeft = vecLeft.Normalize() * m_lineWidth;

                pvertex[0]          = vertex1;
                pvertex[0].x        = vertex1.x + vecLeft.X();// * vertex1.qw;
                pvertex[0].y        = vertex1.y + vecLeft.Y();// * vertex1.qw;
                pvertex[0].z        = vertex1.z;
                pvertex[0].qw       = vertex1.qw;
                pvertex[0].u        = vertex1.u;
                pvertex[0].v        = 0;
                pvertex[0].color    = vertex1.color;
                pvertex[0].specular = vertex1.specular;

                pvertex[1]          = vertex1;
                pvertex[1].x        = vertex1.x - vecLeft.X();// * vertex1.qw;
                pvertex[1].y        = vertex1.y - vecLeft.Y();// * vertex1.qw;
                pvertex[1].z        = vertex1.z;
                pvertex[1].qw       = vertex1.qw;
                pvertex[1].u        = vertex1.u;
                pvertex[1].v        = 1;
                pvertex[1].color    = vertex1.color;
                pvertex[1].specular = vertex1.specular;

                pvertex[2]          = vertex2;
                pvertex[2].x        = vertex2.x + vecLeft.X();// * vertex2.qw;
                pvertex[2].y        = vertex2.y + vecLeft.Y();// * vertex2.qw;
                pvertex[2].z        = vertex2.z;
                pvertex[2].qw       = vertex2.qw;
                pvertex[2].u        = vertex2.u;
                pvertex[2].v        = 0;
                pvertex[2].color    = vertex2.color;
                pvertex[2].specular = vertex2.specular;

                pvertex[3]          = vertex2;
                pvertex[3].x        = vertex2.x - vecLeft.X();// * vertex2.qw;
                pvertex[3].y        = vertex2.y - vecLeft.Y();// * vertex2.qw;
                pvertex[3].z        = vertex2.z;
                pvertex[3].qw       = vertex2.qw;
                pvertex[3].u        = vertex2.u;
                pvertex[3].v        = 1;
                pvertex[3].color    = vertex2.color;
                pvertex[3].specular = vertex2.specular;

                pvertex += 4;

                pindex[0] = indexOut + 0;
                pindex[1] = indexOut + 1;
                pindex[2] = indexOut + 2;
                pindex[3] = indexOut + 1;
                pindex[4] = indexOut + 3;
                pindex[5] = indexOut + 2;
                pindex   += 6;
                indexOut += 4;
            }
        }

        //
        // Update the performance counter
        //

        #ifdef EnablePerformanceCounters
            m_countTriangles += icount;
        #endif

        //
        // Render them
        //

        m_prasterizer->DrawTriangles(pvertexOut, pvertex - pvertexOut, pindexOut, pindex - pindexOut);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Screen Vertices
    //
    //////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////////
    //
    // Transform and lighting in the vertex shader
    //
    // Model-space triangles and one-pixel lines go to the rasterizer as they
    // are, with the matrices and lighting below; the shader does what the
    // Transform* and Light* functions above do. Deformations (a per-vertex
    // callback), wide lines (built in screen space) and points (plotted
    // without texture, blending or z) keep the CPU path.
    //
    //////////////////////////////////////////////////////////////////////////////

    bool UseShaderTL()
    {
        return m_pdeform == nullptr;
    }

    void BuildTransformAndLight(TransformAndLight& tl, bool bNormal)
    {
        const Matrix& matFull = GetFullMatrix();

        //
        // Full matrix: model to the space the CPU clipper works in, which is
        // where the extra clip planes are kept
        //

        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                tl.m_full[row][col] = matFull[row][col];
            }
        }

        //
        // Then to DirectX 6 screen pixels (y down): the clip rect's NDC
        // mapped back with the scales and offsets GetFullMatrix computed, or
        // already screen pixels when clipping is off
        //

        float screen[4][4] = {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        };

        if (m_bClip) {
            screen[0][0] = m_scalex;
            screen[0][3] = m_xoffset;
            screen[1][1] = m_scaley;
            screen[1][3] = m_yoffset;
        }

        //
        // Then to Direct3D clip space for the whole surface. The half-pixel
        // offset puts DirectX 6 pixel centres (integers) on Direct3D 11's
        // (half-integers), as Raster.hlsli does for screen vertices.
        //

        float w = m_sizeSurface.X();
        float h = m_sizeSurface.Y();

        float clip[4][4] = {
            { 2 / w,  0,     0, 1 / w - 1 },
            { 0,     -2 / h, 0, 1 - 1 / h },
            { 0,      0,     1, 0         },
            { 0,      0,     0, 1         }
        };

        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                float sum = 0;

                for (int k = 0; k < 4; k++) {
                    sum += clip[row][k] * screen[k][col];
                }

                tl.m_post[row][col] = sum;
            }
        }

        //
        // Extra clip planes
        //

        tl.m_countPlanes = m_countPlanes - 6 < c_maxTLClipPlanes ? m_countPlanes - 6 : c_maxTLClipPlanes;

        for (int index = 0; index < c_maxTLClipPlanes; index++) {
            if (index < tl.m_countPlanes) {
                const HVector& plane = m_pplane[6 + index].GetHVector();
                tl.m_planes[index][0] = plane.x;
                tl.m_planes[index][1] = plane.y;
                tl.m_planes[index][2] = plane.z;
                tl.m_planes[index][3] = plane.w;
            } else {
                tl.m_planes[index][0] = 0;
                tl.m_planes[index][1] = 0;
                tl.m_planes[index][2] = 0;
                tl.m_planes[index][3] = 1;
            }
        }

        //
        // Lighting, chosen as UpdatePointers chooses the CPU function
        //

        for (int index = 0; index < 4; index++) {
            tl.m_light[index]    = 0;
            tl.m_color[index]    = 1;
            tl.m_colorAlt[index] = 1;
            tl.m_global[index]   = 1;
        }

        tl.m_ambient = m_brightAmbient;

        //
        // Reversed depth for the scene targets' float depth buffers: clip z
        // is w at the near plane and 0 at the far one. Built from the
        // perspective's constants and the full matrix's w row (exactly minus
        // the eye-space z) in double, not as w - z per vertex, which would
        // cancel at the far plane and lose the precision reversed-Z is for.
        //
        // With P's z row (0, 0, a, b) and w row (0, 0, -1, 0):
        // zrev = w - z = -(1 + a) * zeye - b = (1 + a) * wrow . p - b.
        // GetFullMatrix's clip-rect mapping only touches the x and y rows.
        //

        if (m_matPerspective[3][2] != 0) {
            double a = m_matPerspective[2][2];
            double b = m_matPerspective[2][3];

            for (int col = 0; col < 4; col++) {
                tl.m_zrev[col] = (float)((1.0 + a) * matFull[3][col]);
            }

            tl.m_zrev[3] = (float)((1.0 + a) * matFull[3][3] - b);

            //
            // The other way, for soft particles: depth = (1 + a) - b / d, so
            // d = b / ((1 + a) - depth). (Screen vertices' conventional depth,
            // reversed, is the same function of d.)
            //

            tl.m_depth[0] = (float)(1.0 + a);
            tl.m_depth[1] = (float)b;
        } else {
            tl.m_depth[0] = 0;
            tl.m_depth[1] = 0;

            for (int col = 0; col < 4; col++) {
                tl.m_zrev[col] = (float)((double)matFull[3][col] - (double)matFull[2][col]);
            }
        }

        //
        // The eye is the origin of the space m_mat maps into
        //

        {
            Vector eye = GetInverseModelMatrix().Transform(Vector(0, 0, 0));

            tl.m_eye[0] = eye.X();
            tl.m_eye[1] = eye.Y();
            tl.m_eye[2] = eye.Z();
            tl.m_eye[3] = 1;
        }

        if (!bNormal) {
            tl.m_mode = TLModeVertexColor;
            return;
        }

        switch (m_shadeMode) {
            case ShadeModeGlobalColor:
                tl.m_mode      = TLModeGlobalColor;
                tl.m_global[0] = m_colorGlobal.R();
                tl.m_global[1] = m_colorGlobal.G();
                tl.m_global[2] = m_colorGlobal.B();
                tl.m_global[3] = m_colorGlobal.A();
                break;

            case ShadeModeFlat:
            case ShadeModeGouraud:
                UpdateLighting();

                tl.m_light[0] = m_vecLight.X();
                tl.m_light[1] = m_vecLight.Y();
                tl.m_light[2] = m_vecLight.Z();

                if (m_pmaterial) {
                    Color diffuse  = m_pmaterial->GetDiffuse();
                    tl.m_mode      = TLModeMaterial;
                    tl.m_global[0] = diffuse.GetRed();
                    tl.m_global[1] = diffuse.GetGreen();
                    tl.m_global[2] = diffuse.GetBlue();
                    tl.m_global[3] = diffuse.GetAlpha();
                } else if (m_bBidirectional) {
                    tl.m_mode        = TLModeBidirectional;
                    tl.m_color[0]    = m_color.R();
                    tl.m_color[1]    = m_color.G();
                    tl.m_color[2]    = m_color.B();
                    tl.m_colorAlt[0] = m_colorAlt.R();
                    tl.m_colorAlt[1] = m_colorAlt.G();
                    tl.m_colorAlt[2] = m_colorAlt.B();
                } else {
                    tl.m_mode = TLModeDirectional;
                }
                break;

            default:
                tl.m_mode = TLModeWhite;
                break;
        }
    }

    void DrawLinesRasterizer(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        m_prasterizer->DrawLines(pvertex, vcount, pindex, icount);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Screen Vertices
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawTriangles(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        m_prasterizer->DrawTriangles(pvertex, vcount, pindex, icount);
    }

    void DrawLines(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        UpdatePointers();
        (this->*m_pfnDrawLines)(pvertex, vcount, pindex, icount);
    }

    void DrawPoints(const VertexScreen* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        m_prasterizer->DrawPoints(pvertex, vcount);
    }

    void DrawPoints(const VertexScreen* pvertex, int vcount) override
    {
        m_prasterizer->DrawPoints(pvertex, vcount);
    }

    void DrawTriangles(const D3DVertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        ZAssert(false);
    }

    void DrawLines(const D3DVertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        ZAssert(false);
    }

    void DrawPoints(const D3DVertex* pvertex, int vcount, const MeshIndex* pindex, int icount)
    {
        ZAssert(false);
    }

    void DrawPoints(const D3DVertex* pvertex, int vcount) override
    {
        ZAssert(false);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawTriangles(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        #ifdef EnablePerformanceCounters
            m_countTriangles += icount / 3;
        #endif

        if (UseShaderTL()) {
            TransformAndLight tl;
            BuildTransformAndLight(tl, true);
            m_prasterizer->DrawTriangles(tl, pvertex, vcount, pindex, icount);
            return;
        }

        UpdatePointers();
        (this->*m_pfnLightVertex)(pvertex, vcount, pindex, icount, &Device3D::ClipTriangles, &Device3D::DrawTriangles);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawLines(const Vertex* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        #ifdef EnablePerformanceCounters
            m_countLines += icount - 1;
        #endif

        UpdatePointers();

        if (UseShaderTL() && m_pfnDrawLines == &Device3D::DrawLinesRasterizer) {
            TransformAndLight tl;
            BuildTransformAndLight(tl, true);
            m_prasterizer->DrawLines(tl, pvertex, vcount, pindex, icount);
            return;
        }
        (this->*m_pfnLightVertex)(pvertex, vcount, pindex, icount, &Device3D::ClipLines, m_pfnDrawLines);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawPoints(const Vertex* pvertex, int vcount) override
    {
        #ifdef EnablePerformanceCounters
            m_countPoints += vcount;
        #endif

        UpdatePointers();
        (this->*m_pfnLightVertex)(pvertex, vcount, nullptr, 0, &Device3D::ClipPoints, &Device3D::DrawPoints);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Draw Colored Triangles
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawTriangles(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        #ifdef EnablePerformanceCounters
            m_countTriangles += icount / 3;
        #endif

        if (UseShaderTL()) {
            TransformAndLight tl;
            BuildTransformAndLight(tl, false);
            m_prasterizer->DrawTriangles(tl, pvertex, vcount, pindex, icount);
            return;
        }

        UpdatePointers();
        (this->*m_pfnLightVertexL)(pvertex, vcount, pindex, icount, &Device3D::ClipTriangles, &Device3D::DrawTriangles);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Draw Colored Lines
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawLines(const VertexL* pvertex, int vcount, const MeshIndex* pindex, int icount) override
    {
        #ifdef EnablePerformanceCounters
            m_countLines += icount - 1;
        #endif

        UpdatePointers();

        if (UseShaderTL() && m_pfnDrawLines == &Device3D::DrawLinesRasterizer) {
            TransformAndLight tl;
            BuildTransformAndLight(tl, false);
            m_prasterizer->DrawLines(tl, pvertex, vcount, pindex, icount);
            return;
        }
        (this->*m_pfnLightVertexL)(pvertex, vcount, pindex, icount, &Device3D::ClipLines, m_pfnDrawLines);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Draw Colored Points
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawPoints(const VertexL* pvertex, int vcount) override
    {
        #ifdef EnablePerformanceCounters
            m_countPoints += vcount;
        #endif

        UpdatePointers();
        (this->*m_pfnLightVertexL)(pvertex, vcount, nullptr, 0, &Device3D::ClipPoints, &Device3D::DrawPoints);
    }
};

TRef<IDevice3D> CreateDevice3D(Rasterizer* prasterizer)
{
    return new Device3D(prasterizer);
}
