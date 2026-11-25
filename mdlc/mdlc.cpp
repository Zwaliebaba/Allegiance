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

    void DoConvert(const PathString& strInput, const PathString& strOutput)
    {
        printf(ZString("Converting image ") + strInput + "\n");

        //
        // Get the basenames for the bitmap files
        //

        ZString strName       = strInput.GetName().ToLower();
        ZString strOutputName = strOutput.GetName().ToLower();

        //
        // Load the bitmap
        //

        HBITMAP hbitmap = GetModeler()->LoadBitmap(strInput);
        if (hbitmap == NULL)
            Fail("cannot load " + strInput);
        TRef<Surface> psurface = GetEngine()->CreateSurface(hbitmap);
        ZVerify(::DeleteObject(hbitmap));

        //
        // Convert the bitmap to 16bpp
        //

        TRef<Surface> psurface16 =
            GetEngine()->CreateSurface(
                psurface->GetSize(),
                new PixelFormat(16, 0xf800, 0x07e0, 0x001f, 0x0000)
            );

        psurface16->BitBlt(WinPoint(0, 0), psurface);

        //
        // Create a namespace with the bitmap in it
        //

        TRef<INameSpace> pns = GetModeler()->CreateNameSpace(strOutputName, GetModeler()->GetNameSpace("model"));
        pns->AddMember(strOutputName, (Value*)new ConstantImage(psurface16, strName));

        //
        // Write out the namespace
        //

        Write(pns, strOutput);
        GetModeler()->UnloadNameSpace(strOutputName);
    }

    void DoCompressAnim(
        int xframes,
        int yframes,
        const PathString& strInput, 
        const PathString& strOutput
    ) {
        printf(ZString("Compressing animation ") + strInput + "\n");

        //
        // Get the basenames for the bitmap files
        //

        ZString strName       = strInput.GetName().ToLower();
        ZString strOutputName = strOutput.GetName().ToLower();

        //
        // Create the frame image
        //

        TRef<Number> pnumberFrame = new ModifiableNumber(0);

        TRef<Image> pimageFrame =
            CreateFrameImage(
                pnumberFrame,
                GetModeler()->LoadSurface(strInput, false),
                xframes,
                yframes
            );

        //
        // Create the name space
        //

        TRef<INameSpace> pns = 
            GetModeler()->CreateNameSpace(
                strOutputName, 
                GetModeler()->GetNameSpace("model")
            );

        pns->AddMember("frame", pnumberFrame);
        pns->AddMember("image", (Value*)pimageFrame);

        //
        // Write out the namespace
        //

        Write(pns, strOutput);
        GetModeler()->UnloadNameSpace(strOutputName);
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
            Fail("usage: mdlc [-fontdir dir] [-optimize | -convert | -compressanim x y | -info | -genheader | -batch] input [output]");

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
                    } else if (str == "compressanim") {
                        ZString strInput;
                        ZString strOutput;
                        float xframes;
                        float yframes;
                        if (token.IsNumber(xframes)) {
                            if (token.IsNumber(yframes)) {
                                if (token.IsString(strInput)) {
                                    if (token.IsString(strOutput)) {
                                        DoCompressAnim((int)xframes, (int)yframes, strInput, strOutput);
                                        return S_FALSE;
                                    }
                                }
                            }
                        }
                    } else if (str == "convert") {
                        ZString strInput;
                        ZString strOutput;
                        if (token.IsString(strInput)) {
                            if (token.IsString(strOutput)) {
                                DoConvert(strInput, strOutput);
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
