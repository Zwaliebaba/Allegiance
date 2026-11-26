//////////////////////////////////////////////////////////////////////////////
//
// MDL Compiler
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// The main entry point
//
//////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "dds.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////////
//
// ModelerSite
//
//////////////////////////////////////////////////////////////////////////////

class ModelerSiteImpl : public ModelerSite {
public:
    void Error(const ZString& str)
    {
        printf(str);
        printf("\n");
        fflush(stdout);
        _exit(1);   // was 0, which hid failures from the build
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// MDLC Application
//
//////////////////////////////////////////////////////////////////////////////

class MDLCApp : public EffectApp {
public:
    static void Fail(const ZString& str)
    {
        printf("mdlc: " + str + "\n");
        fflush(stdout);
        _exit(1);
    }

    // Engine warnings (e.g. a substituted font) go to the build log, in the
    // "origin : warning : text" form MSBuild picks up; other debug output stays
    // in the debugger.
    void DebugOutput(const char* psz)
    {
        EffectApp::DebugOutput(psz);
        if (strncmp(psz, "warning: ", 9) == 0)
            printf("mdlc : warning : %s", psz + 9);
    }

    // -fontdir <dir>: make the fonts in <dir> available to this process only,
    // so font.mdl gets the faces fontsource.mdl names without installing them.
    static void LoadFonts(const ZString& strDir)
    {
        int count = 0;
        const char* patterns[] = { "*.ttf", "*.otf", "*.fon" };
        for (int i = 0; i < ARRAYSIZE(patterns); i++) {
            WIN32_FIND_DATA data;
            HANDLE hfind = FindFirstFile(strDir + "\\" + patterns[i], &data);
            if (hfind == INVALID_HANDLE_VALUE)
                continue;
            do {
                if (AddFontResourceEx(strDir + "\\" + data.cFileName, FR_PRIVATE, NULL) != 0)
                    count++;
                else
                    printf("mdlc : warning : could not load font %s\\%s\n", (PCC)strDir, data.cFileName);
            } while (FindNextFile(hfind, &data));
            FindClose(hfind);
        }
        printf("mdlc: loaded %d fonts from %s\n", count, (PCC)strDir);
    }

    static void Write(INameSpace* pns, const ZString& strOutput)
    {
        TRef<ZFile> pfile = new ZWriteFile(strOutput);
        if (!pfile->IsValid())
            Fail("cannot write " + strOutput);
        pns->WriteToBinaryFile(pfile);
    }

    void DoCompile(const PathString& strInput, const PathString& strOutput)
    {
        //
        // Get the basename
        //

        ZString strName       = strInput.GetName();
        ZString strOutputName = strOutput.GetName();

        //
        // Load an mdl file
        //

        TRef<INameSpace> pns = GetModeler()->GetNameSpace(strInput);

        if (pns) {
            //
            // Save the file in mdl format
            //

            Write(pns, strOutput);

            //
            // Load the compiled mdl file to make sure there are no errors.
            //

            GetModeler()->UnloadNameSpace(strOutput);
            TRef<INameSpace> pnsReloaded = GetModeler()->GetNameSpace(strOutput);
            ZAssert(pnsReloaded != NULL);
            GetModeler()->UnloadNameSpace(strOutput);
            GetModeler()->UnloadNameSpace(strInput);
        }
    }

    void DoInfo(const ZString& strInput)
    {
        TRef<INameSpace> pns = GetModeler()->GetNameSpace(strInput);

        if (pns) {
            TRef<Geo> pgeo; CastTo(pgeo, pns->FindMemberLocal("object"));
            TRef<LODGeo> plodGeo; CastTo(plodGeo, pgeo);

            if (plodGeo) {
                printf(plodGeo->GetInfo());
                return;
            }

            printf("mdlc: Unrecognized object type. No info avaliable");
        }
    }

    void DoGenHeader(const ZString& strInput, const ZString& strOutput)
    {
        TRef<INameSpace> pns = GetModeler()->GetNameSpace(strInput);

        if (pns) {
            TRef<ZFile> pfile = new ZWriteFile(strOutput);

            pns->WriteTypeHeader(pfile);
        }
    }

    void DoOptimize(const ZString& strInput, const ZString& strOutput)
    {
        printf(ZString("Optimizing ") + strInput + "\n");

        //
        // Load an mdl file
        //

        TRef<INameSpace> pns = GetModeler()->GetNameSpace(strInput);

        if (pns) {
            //
            // Optimize it
            //

            TRef<Number> pnumber = Number::Cast(pns->FindMember("frame"));
            TRef<Geo>    pgeo    = Geo::Cast(pns->FindMember("object"));

            TRef<INameSpace> pnsThing = OptimizeThingGeo(strOutput, pgeo, pnumber);

            if (pnsThing) {
                //
                // Save the file in mdl format
                //

                Write(pnsThing, strOutput + ".mdl");

                //
                // Load the optimized mdl file to make sure there are no errors.
                //

                GetModeler()->UnloadNameSpace(strOutput);
                TRef<INameSpace> pnsReloaded = GetModeler()->GetNameSpace(strOutput);
                ZAssert(pnsReloaded != NULL);
                GetModeler()->UnloadNameSpace(strOutput);
                GetModeler()->UnloadNameSpace(strInput);
            } else {
                Fail("cannot optimize " + strInput);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // -dds / -ddsmips: a bitmap to a 32-bit BGRA DDS file (engine/dds.h)
    //
    // The colours keep the source's full depth (the image MDL files that
    // mdlc -convert made cut them to RGB565). The colour key goes into alpha
    // exactly as the game applied it: a pixel is transparent where its RGB565
    // conversion is 0. Those pixels get RGB 0 as well, so "alpha 0" and
    // "black" stay the same set of pixels, as the engine expects.
    //
    // -ddsmips adds a mip chain (2x2 box filter down to 1x1), for textures
    // used on 3D models.
    //
    //////////////////////////////////////////////////////////////////////////////

    void DoConvertDDS(const PathString& strInput, const PathString& strOutput, bool bMips)
    {
        printf(ZString("Converting image ") + strInput + (bMips ? " (with mip levels)\n" : "\n"));

        HBITMAP hbitmap = GetModeler()->LoadBitmap(strInput);
        if (hbitmap == NULL)
            Fail("cannot load " + strInput);

        //
        // The key: the bitmap converted to RGB565, as for the MDL files
        //

        TRef<Surface> psurface = GetEngine()->CreateSurface(hbitmap);
        WinPoint      size     = psurface->GetSize();

        TRef<Surface> psurface16 =
            GetEngine()->CreateSurface(
                size,
                new PixelFormat(16, 0xf800, 0x07e0, 0x001f, 0x0000)
            );

        psurface16->BitBlt(WinPoint(0, 0), psurface);

        //
        // The colours, as 32-bit BGRA rows, top down
        //

        std::vector<DWORD> vpixels(size.X() * size.Y());

        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = size.X();
        bmi.bmiHeader.biHeight      = -size.Y();
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC hdc   = ::CreateCompatibleDC(NULL);
        int lines = ::GetDIBits(hdc, hbitmap, 0, size.Y(), vpixels.data(), &bmi, DIB_RGB_COLORS);
        ::DeleteDC(hdc);
        ZVerify(::DeleteObject(hbitmap));

        if (lines != size.Y())
            Fail("cannot read the pixels of " + strInput);

        const BYTE* p16     = psurface16->GetPointer();
        int         pitch16 = psurface16->GetPitch();

        for (int y = 0; y < size.Y(); y++) {
            const WORD* pkey = (const WORD*)(p16 + y * pitch16);
            DWORD*      pd   = vpixels.data() + y * size.X();

            for (int x = 0; x < size.X(); x++) {
                pd[x] = pkey[x] == 0 ? 0 : ((pd[x] & 0x00ffffff) | 0xff000000);
            }
        }

        psurface16->ReleasePointer();

        //
        // Mip levels
        //

        std::vector<std::vector<DWORD>> vlevels;
        vlevels.push_back(vpixels);

        if (bMips) {
            int w = size.X();
            int h = size.Y();

            while (w > 1 || h > 1) {
                const std::vector<DWORD>& vsource = vlevels.back();
                int wNext = w > 1 ? w / 2 : 1;
                int hNext = h > 1 ? h / 2 : 1;

                std::vector<DWORD> vnext(wNext * hNext);

                for (int y = 0; y < hNext; y++) {
                    int y0 = (2 * y)     < h ? 2 * y     : h - 1;
                    int y1 = (2 * y + 1) < h ? 2 * y + 1 : h - 1;

                    for (int x = 0; x < wNext; x++) {
                        int x0 = (2 * x)     < w ? 2 * x     : w - 1;
                        int x1 = (2 * x + 1) < w ? 2 * x + 1 : w - 1;

                        DWORD p[4] = {
                            vsource[y0 * w + x0], vsource[y0 * w + x1],
                            vsource[y1 * w + x0], vsource[y1 * w + x1]
                        };

                        DWORD result = 0;

                        for (int shift = 0; shift < 32; shift += 8) {
                            DWORD sum = 2;
                            for (int i = 0; i < 4; i++) {
                                sum += (p[i] >> shift) & 0xff;
                            }
                            result |= (sum / 4) << shift;
                        }

                        vnext[y * wNext + x] = result;
                    }
                }

                vlevels.push_back(vnext);
                w = wNext;
                h = hNext;
            }
        }

        std::vector<const DWORD*> vpointers;
        for (const std::vector<DWORD>& v : vlevels) {
            vpointers.push_back(v.data());
        }

        if (!WriteDDS(PCC(ZString(strOutput)), size.X(), size.Y(), vpointers))
            Fail("cannot write " + strOutput);
    }

    HRESULT Initialize(const ZString& strCommandLine)
    {
        // Unbuffered, so the last line printed before a crash shows how far a
        // batch got (stdout is a pipe under MSBuild, hence fully buffered).
        setvbuf(stdout, NULL, _IONBF, 0);

        if (FAILED(EffectApp::Initialize(strCommandLine + " -software")))
            Fail("engine initialization failed");

        GetModeler()->SetSite(new ModelerSiteImpl());

        //
        // -batch <file>: run each line of the file as a command line, so a
        // whole art build pays for engine start-up once.
        //

        {
            PCC pcc = strCommandLine;
            CommandLineToken token(pcc, strCommandLine.GetLength());
            ZString str;
            ZString strFile;

            if (token.IsMinus(str) && str == "batch") {
                if (!token.IsString(strFile))
                    Fail("usage: mdlc -batch <file>");

                FILE* f = fopen(strFile, "r");
                if (f == NULL)
                    Fail("cannot open " + strFile);

                char line[4096];
                int count = 0;
                while (fgets(line, sizeof(line), f)) {
                    int length = (int)strlen(line);
                    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r' || line[length - 1] == ' '))
                        line[--length] = 0;
                    if (length == 0 || line[0] == '#')
                        continue;
                    if (FAILED(RunCommand(ZString(line))))
                        Fail(ZString("bad command in ") + strFile + ": " + line);
                    count++;
                }
                fclose(f);
                printf("mdlc: %d commands\n", count);
                return S_FALSE;
            }
        }

        if (FAILED(RunCommand(strCommandLine)))
            Fail("usage: mdlc [-fontdir dir] [-optimize | -dds | -ddsmips | -info | -genheader | -batch] input [output]");

        return S_FALSE;
    }

    HRESULT RunCommand(const ZString& strCommandLine)
    {
        {
            //
            // Parse the command line
            //

            PCC pcc = strCommandLine;
            CommandLineToken token(pcc, strCommandLine.GetLength());

            while (token.MoreTokens()) {
                ZString str;

                if (token.IsMinus(str)) {
                    if (str == "fontdir") {
                        ZString strDir;
                        if (!token.IsString(strDir))
                            return E_FAIL;
                        LoadFonts(strDir);
                        continue;   // the command itself follows
                    } else if (str == "optimize") {
                        ZString strInput;
                        ZString strOutput;
                        if (token.IsString(strInput)) {
                            if (token.IsString(strOutput)) {
                                DoOptimize(strInput, strOutput);
                                return S_FALSE;
                            }
                        }
                    } else if (str == "dds" || str == "ddsmips") {
                        ZString strInput;
                        ZString strOutput;
                        if (token.IsString(strInput)) {
                            if (token.IsString(strOutput)) {
                                DoConvertDDS(strInput, strOutput, str == "ddsmips");
                                return S_FALSE;
                            }
                        }
                    } else if (str == "info") {
                        ZString strInput;
                        if (token.IsString(strInput)) {
                            DoInfo(strInput);
                            return S_FALSE;
                        }
                    } else if (str == "genheader") {
                        ZString strInput;
                        ZString strOutput;
                        if (token.IsString(strInput)) {
                            if (token.IsString(strOutput)) {
                                DoGenHeader(strInput, strOutput);
                                return S_FALSE;
                            }
                        }
                    }
                } else {
                    ZString strInput;
                    ZString strOutput;
                    if (token.IsString(strInput)) {
                        if (token.IsString(strOutput)) {
                            DoCompile(strInput, strOutput);
                            return S_FALSE;
                        }
                    }

                    return E_FAIL;                        
                }
            }
        }

        return E_FAIL;
    }

    void Terminate()
    {
        EffectApp::Terminate();
    }
} g_app;
