#include "pch.h"
#include "D3D11Engine.h"

//////////////////////////////////////////////////////////////////////////////
//
// Premultiplied "over" for 32-bit BGRA pixels: dest = source + dest * (1 - a)
//
//////////////////////////////////////////////////////////////////////////////

namespace {

inline DWORD BlendOver(DWORD source, DWORD dest)
{
    DWORD a = source >> 24;

    if (a == 255) {
        return source;
    } else if (a == 0) {
        return dest;
    }

    DWORD inv    = 255 - a;
    DWORD result = 0;

    for (int shift = 0; shift < 32; shift += 8) {
        DWORD c = ((source >> shift) & 0xff) + (((dest >> shift) & 0xff) * inv + 127) / 255;
        result |= (c > 255 ? 255 : c) << shift;
    }

    return result;
}

}

//////////////////////////////////////////////////////////////////////////////
//
// Surface
//
//////////////////////////////////////////////////////////////////////////////

class PrivateSurfaceImpl : public PrivateSurface {
public:
    //////////////////////////////////////////////////////////////////////////////
    //
    // Members
    //
    //////////////////////////////////////////////////////////////////////////////

    //
    // Debug Info
    //

    ZString                   m_strName;
    bool                      m_bDeviceFormat;
    bool                      m_bInContext;

    //
    // Surface Description
    //

    int                       m_id;
    WinPoint                  m_size;
    int                       m_pitch;
    TRef<PixelFormat>         m_ppf;
    BYTE*                     m_pbits;
    TRef<IObject>             m_pobjectMemory;

    bool                      m_bColorKey;
    Color                     m_colorKey;

    //
    // Device Format surfaces
    //

    TRef<PrivateEngine>       m_pengine;
    TRef<SurfaceSite>         m_psite;

    SurfaceType               m_stype;

    //
    // Format convertion
    //

    int                       m_idConverted;
    TRef<PixelFormat>         m_ppfConverted;
    TRef<PrivateSurfaceImpl>  m_psurfaceConverted;

    //
    // Drawing
    //

    TRef<PrivateContext>      m_pcontext;

    //
    // GPU copy (3D surfaces only) and which copy is current. At least one
    // always is; both are after an upload or a readback.
    //

    TRef<D3D11Target>         m_pgpu;
    bool                      m_bCpuCurrent;
    bool                      m_bGpuCurrent;

    //
    // Precomputed mip levels 1.. (SetMipLevels), dropped on the first write
    // to the pixels (a colour key change doesn't touch them)
    //

    std::vector<const BYTE*>  m_vmipLevels;
    TRef<IObject>             m_pobjectMipMemory;

    //
    // Transforms and clipping
    //

    WinPoint                  m_pointOffset;
    WinRect                   m_rectClip;
    WinPoint                  m_pointOffsetSave;
    WinRect                   m_rectClipSave;

    //////////////////////////////////////////////////////////////////////////////
    //
    //
    //
    //////////////////////////////////////////////////////////////////////////////

    void Initialize()
    {
        m_pointOffset = WinPoint(0, 0);
        m_rectClip    = WinRect(WinPoint(0, 0), m_size);

        m_bColorKey   = false;
        m_bInContext  = false;

        m_colorKey    = Color(0, 0, 0);

        m_id          = 0;
        m_idConverted = -1;

        m_bCpuCurrent = true;
        m_bGpuCurrent = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // 
    //
    //////////////////////////////////////////////////////////////////////////////

    //
    // Every surface has its pixels in system memory. 3D surfaces also get a
    // GPU copy once something draws into them (see GPU residency below).
    //

    void AllocateSurface()
    {
        m_pitch = (m_size.X() * m_ppf->PixelBytes() + 3) & (~0x3);
        m_pbits = new BYTE[m_pitch * m_size.Y()];
        memset(m_pbits, 0, m_pitch * m_size.Y());
    }

    //
    // Surfaces loaded from binary MDL files point straight into a read-only
    // file mapping. Give them their own copy before the first write.
    //

    void MakeWritable()
    {
        if (m_pobjectMemory != NULL) {
            BYTE* pbits = new BYTE[m_pitch * m_size.Y()];
            memcpy(pbits, m_pbits, m_pitch * m_size.Y());
            m_pbits         = pbits;
            m_pobjectMemory = NULL;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // A device format surface
    //
    //////////////////////////////////////////////////////////////////////////////

    PrivateSurfaceImpl(
        PrivateEngine*  pengine,
        PixelFormat*    ppf,
        const WinPoint& size,
        SurfaceType     stype,
        SurfaceSite*    psite
    ) :
        m_pengine(pengine),
        m_ppf(ppf),
        m_size(size),
        m_bDeviceFormat(true),
        m_stype(stype),
        m_psite(psite),
        m_pbits(NULL)
    {
        Initialize();
        AllocateSurface();

        if (m_psite)
            m_psite->UpdateSurface(this);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Create a surface from a binary representation
    //
    //////////////////////////////////////////////////////////////////////////////

    PrivateSurfaceImpl(
              PrivateEngine*  pengine,
              PixelFormat*    ppf,
        const WinPoint&       size,
              int             pitch,
              BYTE*           pdata,
              IObject*        pobjectMemory
    ) :
        m_pengine(pengine),
        m_bDeviceFormat(false),
        m_stype(SurfaceType2D()),
        m_size(size),
        m_pobjectMemory(pobjectMemory),
        m_pitch(pitch),
        m_ppf(ppf),
        m_pbits(pdata)
    {
        Initialize();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Write a surface to a binary MDL file
    //
    //////////////////////////////////////////////////////////////////////////////
    
    void Write(ZFile* pfile) override
    {
        SyncCpu();

        BinarySurfaceInfo bsi;

        bsi.m_size      = m_size;
        bsi.m_pitch     = m_pitch;
        bsi.m_bitCount  = m_ppf->PixelBits();
        bsi.m_redMask   = m_ppf->RedMask();
        bsi.m_greenMask = m_ppf->GreenMask();
        bsi.m_blueMask  = m_ppf->BlueMask();
        bsi.m_alphaMask = m_ppf->AlphaMask();
        bsi.m_bColorKey = m_bColorKey;

        pfile->Write((void*)&bsi, sizeof(bsi));
        pfile->Write((void*)m_pbits, m_pitch * m_size.Y());
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // More constructors
    //
    //////////////////////////////////////////////////////////////////////////////

    TRef<Surface> CreateCompatibleSurface(const WinPoint& size, SurfaceType stype, SurfaceSite* psite) override
    {
        return m_pengine->CreateCompatibleSurface(this, size, stype, psite);
    }

    TRef<Surface> Copy() override
    {
        TRef<Surface> psurface = m_pengine->CreateCompatibleSurface(this, m_size, GetSurfaceType(), m_psite);

        //  : turn off color key while blting

        psurface->BitBlt(WinPoint(0, 0), this);
        return psurface;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Destructor
    //
    //////////////////////////////////////////////////////////////////////////////

    ~PrivateSurfaceImpl()
    {
        m_pengine->RemovePrivateSurface(this);
        if (m_pobjectMemory == NULL && m_pbits != NULL) {
            delete[] m_pbits;
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Site
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetSite(SurfaceSite* psite) override
    {
        m_psite = psite;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Device Specific Surface
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetPixelFormat(PixelFormat* ppf) override
    {
        ZAssert(m_pobjectMemory == NULL);
        DropMipLevels();

        m_pcontext      = NULL;

        if (m_pbits) {
            delete[] m_pbits;
            m_pbits = NULL;
        }

        m_ppf = ppf;
        AllocateSurface();
        DropGpu();

        if (m_psite) {
            m_psite->UpdateSurface(this);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Surface updates
    //
    //////////////////////////////////////////////////////////////////////////////

    void SurfaceChanged()
    {
        m_id++;
        if (m_id < 0) m_id = 0;
    }

    int GetChangeID() override
    {
        return m_id;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Mip levels
    //
    //////////////////////////////////////////////////////////////////////////////

    void SetMipLevels(int count, const BYTE* const* plevels, IObject* pobjectMemory) override
    {
        m_vmipLevels.assign(plevels, plevels + count);
        m_pobjectMipMemory = pobjectMemory;
        SurfaceChanged();   // the texture cache re-creates the texture with them
    }

    void DropMipLevels()
    {
        if (!m_vmipLevels.empty()) {
            m_vmipLevels.clear();
            m_pobjectMipMemory = NULL;
        }
    }

    int GetMipLevelCount() override
    {
        return 1 + (int)m_vmipLevels.size();
    }

    const BYTE* GetMipLevel(int level) override
    {
        ZAssert(level >= 1 && level < GetMipLevelCount());
        return m_vmipLevels[level - 1];
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Attributes
    //
    //////////////////////////////////////////////////////////////////////////////

    PixelFormat* GetPixelFormat() override
    {
        return m_ppf;
    }

    Palette* GetPalette() override
    {
        return NULL;
    }

    void SetName(const ZString& str) override
    {
        m_strName = str;
    }

    const ZString& GetName()
    {
        return m_strName;
    }

    Engine* GetEngine() override
    { 
        return m_pengine;               
    }

    const WinPoint& GetSize() override
    { 
        return m_size;                  
    }

    WinRect GetRect()
    {
        return 
            WinRect(
                WinPoint(0, 0),
                m_size
            );
    }

    SurfaceType GetSurfaceType() override 
    { 
        return m_stype;           
    }

    bool HasColorKey() override
    {
        return m_bColorKey;
    }

    const Color& GetColorKey() override
    {
        ZAssert(m_bColorKey);
        return m_colorKey;
    }

    void SetColorKey(const Color& color) override
    {
        m_bColorKey = true;
        m_colorKey = color;

        //
        // The texture cache bakes the key into the alpha channel
        //

        SurfaceChanged();

        if (m_psurfaceConverted) {
            m_psurfaceConverted->SetColorKey(color);
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Drawing context
    //
    //////////////////////////////////////////////////////////////////////////////

    Context* GetContext() override
    {
		ZAssert(!m_bInContext);

		m_bInContext = true;

        if (m_pcontext == NULL) {
            m_pcontext = CreateContextImpl(this);
        }

        if (m_pcontext->IsValid()) {
            //
            // Remove any offset or clipping
            //

            m_rectClipSave    = m_rectClip;
            m_rectClip        = WinRect(WinPoint(0, 0), m_size);
            m_pointOffsetSave = m_pointOffset;
            m_pointOffset     = WinPoint(0, 0);

            m_pcontext->BeginRendering();

            return m_pcontext;
        }

        m_pcontext   = NULL;
        m_bInContext = false;
        return NULL;
    }

    void ReleaseContext(Context* pcontext) override
    {
		ZAssert(m_bInContext);
        ZAssert(m_pcontext == pcontext);

		m_bInContext = false;

        m_pcontext->EndRendering();

        m_pointOffset = m_pointOffsetSave;
        m_rectClip    = m_rectClipSave;
    }

    PrivateSurface* GetConvertedSurface(PixelFormat* ppf)
    {
        if (ppf == m_ppf) {
            return this;
        } else if (ppf != m_ppfConverted || m_idConverted != m_id) {
            if (ppf != m_ppfConverted) {
                //
                // Create a surface with the requested pixel format
                //
            
                m_ppfConverted = ppf;

                m_psurfaceConverted =
                    new PrivateSurfaceImpl(
                        m_pengine,
                        ppf,
                        m_size,
                        m_stype,
                        NULL
                
                    );
                }

                if (HasColorKey()) {
                    m_psurfaceConverted->SetColorKey(GetColorKey());
                }

                m_idConverted = m_id - 1;
            }

            //
            // Do the bits need to be updated?
            //

            if (m_idConverted != m_id) {
                //
                // Do a conversion blt
                //

                m_psurfaceConverted->BltConvert(
                    WinPoint(0, 0),
                    this,
                    WinRect(WinPoint(0, 0), m_size)
                );

                m_idConverted = m_id;
        }

        return m_psurfaceConverted;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // GPU residency
    //
    // Only 3D surfaces use the GPU. Pixel access brings the CPU copy up to
    // date (a readback if the GPU copy is the current one); GPU drawing brings
    // the GPU copy up to date (an upload if the CPU copy is). 2D drawing into
    // a 3D surface is done on the GPU, so after the first frame the window
    // frame and the 3D panes are not copied in either direction.
    //
    //////////////////////////////////////////////////////////////////////////////

    D3D11Device* GetGpuDevice()
    {
        if (!m_bDeviceFormat || !m_stype.Test(SurfaceType3D())) {
            return NULL;
        }

        return m_pengine->GetD3D11Device();
    }

    void DropGpu()
    {
        m_pgpu        = NULL;
        m_bGpuCurrent = false;
        m_bCpuCurrent = true;
    }

    //
    // A GPU copy that is valid for the current device and size. If it had to
    // be (re)created its contents are undefined and the CPU copy is all there
    // is; after a device loss that copy may be a frame old.
    //

    D3D11Device* EnsureGpuTarget()
    {
        D3D11Device* pdevice = GetGpuDevice();

        if (pdevice == NULL) {
            return NULL;
        }

        if (m_pgpu == NULL) {
            m_pgpu = new D3D11Target();
        }

        bool bRecreated;

        if (!D3D11EnsureTarget(pdevice, m_pgpu, m_size, bRecreated)) {
            DropGpu();
            return NULL;
        }

        if (bRecreated) {
            m_bGpuCurrent = false;
            m_bCpuCurrent = true;
        }

        return pdevice;
    }

    void UploadToGpu(D3D11Device* pdevice)
    {
        D3D11Upload(pdevice, m_pgpu, m_pbits, m_pitch, m_ppf);
        m_bGpuCurrent = true;
    }

    //
    // bDiscard: the draw covers the whole surface, so there is no need to
    // upload first
    //

    D3D11Target* BeginGpuDraw(bool bDiscard)
    {
        DropMipLevels();

        D3D11Device* pdevice = EnsureGpuTarget();

        if (pdevice == NULL) {
            return NULL;
        }

        if (!m_bGpuCurrent && !bDiscard) {
            UploadToGpu(pdevice);
        }

        m_bGpuCurrent = true;
        m_bCpuCurrent = false;
        SurfaceChanged();

        return m_pgpu;
    }

    bool UsesGpu() override
    {
        return GetGpuDevice() != NULL;
    }

    D3D11Target* BeginGpuDraw() override
    {
        return BeginGpuDraw(false);
    }

    ID3D11ShaderResourceView* GetGpuSource() override
    {
        if (m_pgpu == NULL) {
            return NULL;
        }

        D3D11Device* pdevice = EnsureGpuTarget();

        if (pdevice == NULL) {
            return NULL;
        }

        if (!m_bGpuCurrent) {
            UploadToGpu(pdevice);
        }

        return m_pgpu->m_psrv;
    }

    bool MakeGpuResident() override
    {
        D3D11Device* pdevice = EnsureGpuTarget();

        if (pdevice == NULL) {
            return false;
        }

        if (!m_bGpuCurrent) {
            UploadToGpu(pdevice);
        }

        return true;
    }

    void SyncCpu()
    {
        if (m_bCpuCurrent) {
            return;
        }

        m_bCpuCurrent = true;

        D3D11Device* pdevice = m_pengine->GetD3D11Device();

        if (m_pgpu != NULL && pdevice != NULL && m_pgpu->IsValid(pdevice, m_size)) {
            MakeWritable();
            D3D11Readback(pdevice, m_pgpu, m_pbits, m_pitch, m_ppf);
        }
    }

    //
    // 2D on the GPU. A blit source is read through its own GPU copy if it has
    // one, otherwise through the texture cache.
    //

    bool GpuBlt(
        const WinRect&      rectTarget,
        PrivateSurfaceImpl* psurfaceSource,
        const WinRect&      rectSource,
        const WinRect&      rectScissor
    ) {
        if (psurfaceSource == this || !UsesGpu()) {
            return false;
        }

        D3D11Device*              pdevice = m_pengine->GetD3D11Device();
        D3D11KeyMode              keyMode = D3D11KeyNone;
        ID3D11ShaderResourceView* psrv    = psurfaceSource->GetGpuSource();

        if (psrv != NULL) {
            if (psurfaceSource->HasColorKey()) {
                keyMode = D3D11KeyBlack;
            }
        } else {
            psrv = pdevice->GetTexture(psurfaceSource);

            if (psurfaceSource->HasColorKey()) {
                keyMode = D3D11KeyAlpha;
            }
        }

        if (psrv == NULL) {
            return false;
        }

        D3D11Target* ptarget = BeginGpuDraw(false);

        if (ptarget == NULL) {
            return false;
        }

        D3D11Blit(pdevice, ptarget, rectTarget, psrv, psurfaceSource->GetSize(), rectSource, keyMode, rectScissor);
        return true;
    }

    bool GpuFill(const WinRect& rect, const Color& color)
    {
        if (!UsesGpu()) {
            return false;
        }

        bool bAll =
               rect.XMin() <= 0 && rect.YMin() <= 0
            && rect.XMax() >= m_size.X() && rect.YMax() >= m_size.Y();

        D3D11Target* ptarget = BeginGpuDraw(bAll);

        if (ptarget == NULL) {
            return false;
        }

        D3D11Fill(m_pengine->GetD3D11Device(), ptarget, rect, color);
        return true;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Surface Access calls
    //
    //////////////////////////////////////////////////////////////////////////////

    int GetPitch() override
    {
        return m_pitch;
    }

    const BYTE* GetPointer() override
    {
        SyncCpu();
        return m_pbits;
    }

    const BYTE* GetPointer(const WinPoint& point) override
    {
        return 
              GetPointer()
            + point.Y() * GetPitch()
            + point.X() * m_ppf->PixelBytes();
    }

    BYTE* GetWritablePointer() override
    {
        DropMipLevels();
        SyncCpu();
        SurfaceChanged();
        MakeWritable();
        m_bGpuCurrent = false;
        return m_pbits;
    }

    BYTE* GetWritablePointer(const WinPoint& point) override
    {
        return 
              GetWritablePointer()
            + point.Y() * GetPitch()
            + point.X() * m_ppf->PixelBytes();
    }

    void ReleasePointer() override
    {
    }

    Pixel GetPixel(const WinPoint& point) override
    {
        return m_ppf->GetPixel(GetPointer(point));
    }

    Color GetColor(const WinPoint& point) override
    {
        return m_ppf->MakeColor(GetPixel(point));
    }

    void SetPixel(const WinPoint& point, Pixel pixel) override
    {
        m_ppf->SetPixel(GetWritablePointer(point), pixel);
    }

    void SetColor(const WinPoint& point, const Color& color) override
    {
        SetPixel(point, m_ppf->MakePixel(color));
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Clipping and transforms
    //
    //////////////////////////////////////////////////////////////////////////////

    void Offset(const WinPoint& pointOffset) override
    {
        m_pointOffset += pointOffset;
    }

    const WinPoint& GetOffset() override
    {
        return m_pointOffset;
    }

    WinRect GetClipRect() override
    {
        WinRect rect = m_rectClip;
        rect.Offset(-m_pointOffset);
        return rect;
    }

    void SetClipRect(const WinRect& rectLocal) override
    {
        WinRect rectClip(rectLocal);
        rectClip.Offset(m_pointOffset);
        m_rectClip.Intersect(rectClip);
    }

    void RestoreClipRect(const WinRect& rectLocal) override
    {
        m_rectClip = rectLocal;
        m_rectClip.Offset(m_pointOffset);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // GDI calls
    //
    //////////////////////////////////////////////////////////////////////////////

    void DrawString(
        IEngineFont* pfont, 
        const Color& color, 
        const WinPoint& point, 
        const ZString& str
    ) override {
        pfont->DrawString(
            this, 
            point + m_pointOffset, 
            m_rectClip,
            str, 
            color
        );
    }

    void DrawStringWithShadow(
        IEngineFont* pfont, 
        const Color& color, 
        const Color& colorShadow,
        const WinPoint& point, 
        const ZString& str
    ) override {
        DrawString(pfont, colorShadow, point + WinPoint(1, 1), str);
        DrawString(pfont, color      , point                 , str);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Format converting blt
    //
    //////////////////////////////////////////////////////////////////////////////

    void BltConvert(
        const WinPoint&           point, 
              PrivateSurfaceImpl* psurfaceSource, 
        const WinRect&            rectSource
    ) {
        //
        // Source info
        //

        PixelFormat* ppfSource   = psurfaceSource->GetPixelFormat();
        const BYTE*  psource     = psurfaceSource->GetPointer(rectSource.Min());
        int          pitchSource = psurfaceSource->GetPitch();
        int          bytesSource = ppfSource->PixelBytes();

        //
        // Dest info
        //

        BYTE* pdest     = GetWritablePointer(point);
        int   bytesDest = m_ppf->PixelBytes();
        int   pitchDest = GetPitch();

        //
        // Do the appropriate blt
        //

        if (
               bytesSource == 2 
            && bytesDest   == 2
        ) {
            ZAssert(ppfSource->RedSize()  == 0x1f);
            ZAssert(ppfSource->BlueSize() == 0x1f);

            ZAssert(m_ppf->RedSize()  == 0x1f);
            ZAssert(m_ppf->BlueSize() == 0x1f);

            if (ppfSource->GreenSize() == 0x1f) {
                //
                // Convert 555 to 565
                //

                ZAssert(m_ppf->GreenSize() == 0x3f);

                for (int y = rectSource.YSize(); y > 0; y--) {
                    for (int x = 0; x < rectSource.XSize(); x++) {
                        WORD wSource = ((WORD*)psource)[x];
                        WORD wDest   = 
                              ((wSource & 0xffe0) << 1)
                            | (wSource & 0x1f);

                        ((WORD*)pdest)[x] = wDest;
                    }
                    psource += pitchSource;
                    pdest   += pitchDest;
                }
            } else {
                //
                // Convert 565 to 555
                //

                ZAssert(ppfSource->GreenSize() == 0x3f);
                ZAssert(m_ppf    ->GreenSize() == 0x1f);

                for (int y = rectSource.YSize(); y > 0; y--) {
                    for (int x = 0; x < rectSource.XSize(); x++) {
                        WORD wSource = ((WORD*)psource)[x];
                        WORD wDest   = 
                              ((wSource >> 1) & 0xffe0)
                            | (wSource & 0x1f);

                        ((WORD*)pdest)[x] = wDest;
                    }
                    psource += pitchSource;
                    pdest   += pitchDest;
                }
            }
        } else {
            //
            // Anything else goes through 8-bit BGRA, which keeps alpha (and
            // turns the colour key of a format without alpha into alpha 0)
            //

            for (int y = rectSource.YSize(); y > 0; y--) {
                for (int x = 0; x < rectSource.XSize(); x++) {
                    m_ppf->SetPixel(
                        pdest + x * bytesDest,
                        m_ppf->MakePixelBGRA(ppfSource->GetBGRA(ppfSource->GetPixel(psource + x * bytesSource)))
                    );
                }
                psource += pitchSource;
                pdest   += pitchDest;
            }
        }

        psurfaceSource->ReleasePointer();
        ReleasePointer();
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Blts
    //
    //////////////////////////////////////////////////////////////////////////////

    void UnclippedBltHandled(PrivateSurface* psurfaceSource, const WinRect& rect, const WinPoint& pointSource, BYTE* pdest)
    {
        const BYTE* psource     = psurfaceSource->GetPointer(pointSource);
        int         pitchSource = psurfaceSource->GetPitch();
        int         pitchDest   = GetPitch();
        int         scanSize    = rect.XSize() * m_ppf->PixelBytes();
        int         ysize       = rect.YSize();

        if (psurfaceSource->HasColorKey() && m_ppf->PixelBytes() == 4 && m_ppf->AlphaMask() == 0xff000000) {
            // 32-bit premultiplied alpha: blend over the destination (opaque
            // pixels are copied, alpha 0 ones, the colour key, left alone)
            int xpixels = scanSize / 4;

            for (int y = 0; y < ysize; y++) {
                const DWORD* psrc = (const DWORD*)psource;
                DWORD*       pdst = (DWORD*)pdest;

                for (int x = 0; x < xpixels; x++) {
                    pdst[x] = BlendOver(psrc[x], pdst[x]);
                }

                psource += pitchSource;
                pdest   += pitchDest;
            }
        } else if (psurfaceSource->HasColorKey() && m_ppf->PixelBytes() == 4) {
            // Copy the 32-bit pixels that are not the colour key (black)
            int xpixels = scanSize / 4;

            for (int y = 0; y < ysize; y++) {
                const DWORD* psrc = (const DWORD*)psource;
                DWORD*       pdst = (DWORD*)pdest;

                for (int x = 0; x < xpixels; x++) {
                    if ((psrc[x] & 0x00ffffff) != 0) {
                        pdst[x] = psrc[x];
                    }
                }

                psource += pitchSource;
                pdest   += pitchDest;
            }
        } else if (psurfaceSource->HasColorKey()) {
            ZAssert(m_ppf->PixelBytes() == 2);

            // Copy the 16-bit pixels that are not the color key (0)
            int xpixels = scanSize / 2;

            for (int y = 0; y < ysize; y++) {
                const WORD* psrc = (const WORD*)psource;
                WORD*       pdst = (WORD*)pdest;

                for (int x = 0; x < xpixels; x++) {
                    if (psrc[x] != 0) {
                        pdst[x] = psrc[x];
                    }
                }

                psource += pitchSource;
                pdest   += pitchDest;
            }
        } else {
            for (int y = 0; y < ysize; y++) {
                memcpy(pdest, psource, scanSize);
                psource += pitchSource;
                pdest   += pitchDest;
            }
        }
    }

    void UnclippedBlt(
        const WinRect& rect, 
        PrivateSurfaceImpl* psurfaceSourceOriginal, 
        const WinPoint& pointSource
    ) {
        ZAssert(rect.XMin() >= 0                                                       );
        ZAssert(rect.XMax() <= m_size.X()                                              );
        ZAssert(rect.YMin() >= 0                                                       );
        ZAssert(rect.YMax() <= m_size.Y()                                              );
        ZAssert(rect.XSize() + pointSource.X() <= psurfaceSourceOriginal->GetSize().X());
        ZAssert(rect.YSize() + pointSource.Y() <= psurfaceSourceOriginal->GetSize().Y());

        if (rect.XSize() == 0 || rect.YSize() == 0) {
            return;            
        }

        PrivateSurface* psurfaceSource = psurfaceSourceOriginal->GetConvertedSurface(m_ppf);
        ZAssert(psurfaceSource->GetPixelFormat() == m_ppf);

        {
            BYTE* pdest = GetWritablePointer(rect.Min());

            UnclippedBltHandled(psurfaceSource, rect, pointSource, pdest);

            psurfaceSource->ReleasePointer();
            ReleasePointer();
        }
    }

    void BitBlt(const WinPoint& point, Surface* psurfaceSourceArg, const WinRect& rectSourceArg) override
    {
        PrivateSurfaceImpl* psurfaceSource = (PrivateSurfaceImpl*)psurfaceSourceArg;

        //
        // Calculate the target rectangle
        //

        WinPoint origin = point + m_pointOffset;
        WinRect rectTarget(origin, origin + rectSourceArg.Size());

        //
        // Clip it to the surface
        //

        rectTarget.Intersect(m_rectClip);

        //
        // Only blt if the target isn't empty
        //

        if (!rectTarget.IsEmpty()) {
            //
            // adjust the source rect to match the target rect
            //

            WinPoint SourceOrigin = rectSourceArg.Min() + (rectTarget.Min() - origin);
            WinRect rectSource(SourceOrigin, SourceOrigin + rectTarget.Size());

            //
            // do the blt
            //

            if (!GpuBlt(rectTarget, psurfaceSource, rectSource, rectTarget)) {
                UnclippedBlt(rectTarget, psurfaceSource, rectSource.Min());
            }
        }

        SurfaceChanged();
    }

    void BitBlt(const WinPoint& point, Surface* psurfaceSource) override
    {
        BitBlt(
            point, 
            psurfaceSource, 
            WinRect(
                WinPoint(0, 0), 
                psurfaceSource->GetSize()
            )
        );
    }

    void BitBltFromCenter(const WinPoint& point, Surface* psurfaceSource) override
    {
        BitBlt(
            point - psurfaceSource->GetSize() / 2, 
            psurfaceSource
        );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // StretchBlts
    //
    // DirectDraw did these in hardware. The only caller is the video image,
    // which doubles the movie frame, so a nearest-neighbour CPU loop is enough.
    // Clipped to the surface (DirectDraw rejected rects outside it); honours
    // the source color key, which DDBLT_KEYSRC meant.
    //
    //////////////////////////////////////////////////////////////////////////////

    void UnclippedStretchBlt(const WinRect& rectTarget, PrivateSurfaceImpl* psurfaceSourceOriginal, const WinRect& rectSource)
    {
        PrivateSurface* psurfaceSource = psurfaceSourceOriginal->GetConvertedSurface(m_ppf);

        WinRect rectClipped = rectTarget;
        rectClipped.Intersect(WinRect(WinPoint(0, 0), m_size));

        if (rectClipped.IsEmpty()) {
            return;
        }

        int         bytes       = m_ppf->PixelBytes();
        bool        bColorKey   = psurfaceSourceOriginal->HasColorKey();
        bool        bAlpha      = bColorKey && bytes == 4 && m_ppf->AlphaMask() == 0xff000000;
        Pixel       pixelKey    = bColorKey ? m_ppf->MakePixel(psurfaceSourceOriginal->GetColorKey()) : Pixel::Create(0);
        const BYTE* psource     = psurfaceSource->GetPointer();
        int         pitchSource = psurfaceSource->GetPitch();
        BYTE*       pdestRow    = GetWritablePointer(rectClipped.Min());
        int         pitchDest   = GetPitch();

        for (int y = rectClipped.YMin(); y < rectClipped.YMax(); y++) {
            int ySource = rectSource.YMin() + (y - rectTarget.YMin()) * rectSource.YSize() / rectTarget.YSize();
            BYTE* pdest = pdestRow;

            for (int x = rectClipped.XMin(); x < rectClipped.XMax(); x++) {
                int xSource = rectSource.XMin() + (x - rectTarget.XMin()) * rectSource.XSize() / rectTarget.XSize();
                Pixel pixel = m_ppf->GetPixel(psource + ySource * pitchSource + xSource * bytes);

                if (bAlpha) {
                    *(DWORD*)pdest = BlendOver(pixel.Value(), *(DWORD*)pdest);
                } else if (!bColorKey || pixel.Value() != pixelKey.Value()) {
                    m_ppf->SetPixel(pdest, pixel);
                }

                pdest += bytes;
            }

            pdestRow += pitchDest;
        }

        psurfaceSource->ReleasePointer();
        ReleasePointer();
    }

    void BitBlt(const WinRect& rectTargetArg, Surface* psurfaceSourceArg, const WinRect& rectSource) override
    {
        PrivateSurfaceImpl* psurfaceSource = (PrivateSurfaceImpl*)psurfaceSourceArg;

        //
        // Calculate the target rectangle
        //

        WinRect rectTarget = rectTargetArg;
        rectTarget.Offset(m_pointOffset);

        // !!! this function is unclipped
        // !!! rectTarget.Intersect(m_rectClip);

        //
        // Only blt if the target isn't empty
        //

        if (!rectTarget.IsEmpty() && !rectSource.IsEmpty()) {
            if (!GpuBlt(rectTarget, psurfaceSource, rectSource, WinRect(WinPoint(0, 0), m_size))) {
                UnclippedStretchBlt(rectTarget, psurfaceSource, rectSource);
            }
        }

        SurfaceChanged();
    }

    void BitBlt(const WinRect& rectTarget, Surface* psurfaceSource) override
    {
        BitBlt(
            rectTarget,
            psurfaceSource, 
            WinRect(
                WinPoint(0, 0), 
                psurfaceSource->GetSize()
            )
        );
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Direct Draw Fills
    //
    //////////////////////////////////////////////////////////////////////////////

    void UnclippedFill(const WinRect& rect, Pixel pixel)
    {
        ZAssert(rect.XMin() >= 0         );
        ZAssert(rect.XMax() <= m_size.X());
        ZAssert(rect.YMin() >= 0         );
        ZAssert(rect.YMax() <= m_size.Y());

        {
            BYTE* pdest = GetWritablePointer(rect.Min());
            int   bytes = m_ppf->PixelBytes();
            int   pitch = GetPitch();

            for (int y = rect.YSize(); y > 0; y--) {
                if (bytes == 4) {
                    DWORD dwPixel = pixel.Value();

                    for (int x = 0; x < rect.XSize(); x++) {
                        ((DWORD*)pdest)[x] = dwPixel;
                    }
                } else if (bytes == 2) {
                    WORD wPixel = (WORD)pixel.Value();

                    for (int x = 0; x < rect.XSize(); x++) {
                        ((WORD*)pdest)[x] = wPixel;
                    }
                } else {
                    for (int x = 0; x < rect.XSize(); x++) {
                        m_ppf->SetPixel(pdest + x * bytes, pixel);
                    }
                }
                pdest += pitch;
            }

            ReleasePointer();
        }
    }

    void FillRect(const WinRect& rectArg, Pixel pixel) override
    {
        //
        // Offset the rect to the origin of the surface
        //

        WinRect rect = rectArg;
        rect.Offset(m_pointOffset);

        //
        // Clip it to the surface
        //

        rect.Intersect(m_rectClip);

        //
        // Only blt if the target isn't empty
        //

        if (!rect.IsEmpty()) {
            if (!GpuFill(rect, m_ppf->MakeColor(pixel))) {
                UnclippedFill(rect, pixel);
            }
        }

        SurfaceChanged();
    }

    //
    // A colour fill on the GPU keeps the full colour; on the CPU it is the
    // nearest pixel value, as before
    //

    void FillRect(const WinRect& rectArg, const Color& color) override
    {
        WinRect rect = rectArg;
        rect.Offset(m_pointOffset);
        rect.Intersect(m_rectClip);

        if (!rect.IsEmpty()) {
            if (!GpuFill(rect, color)) {
                UnclippedFill(rect, m_ppf->MakePixel(color));
            }
        }

        SurfaceChanged();
    }

    void FillSurface(Pixel pixel) override
    {
        FillRect(WinRect(-m_pointOffset, m_size - m_pointOffset), pixel);
    }

    void FillSurface(const Color& color) override
    {
        FillRect(WinRect(-m_pointOffset, m_size - m_pointOffset), color);
    }

    void FillSurfaceWithColorKey() override
    {
        FillSurface(m_colorKey);
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Used to create a surface with a certain pixel format
//
//////////////////////////////////////////////////////////////////////////////

TRef<PrivateSurface> CreatePrivateSurface(
    PrivateEngine*  pengine,
    PixelFormat*    ppf,
    const WinPoint& size,
    SurfaceType     stype,
    SurfaceSite*    psite
) {
    return new PrivateSurfaceImpl(pengine, ppf, size, stype, psite);
}

//////////////////////////////////////////////////////////////////////////////
//
// Used to create a surface from a memory mapped file
//
//////////////////////////////////////////////////////////////////////////////

TRef<PrivateSurface> CreatePrivateSurface(
          PrivateEngine*  pengine,
          PixelFormat*    ppf,
    const WinPoint&       size,
          int             pitch,
          BYTE*           pdata,
          IObject*        pobjectMemory
) {
    return new PrivateSurfaceImpl(pengine, ppf, size, pitch, pdata, pobjectMemory);
}
