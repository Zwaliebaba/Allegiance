#include "pch.h"
#include "ArtFolders.h"

#include <filesystem>

//////////////////////////////////////////////////////////////////////////////
//
// The art folders (zlib/ArtFolders.h) and UTL::getFile, which the core files
// and collision hulls are opened with, on a temporary art folder. See
// MODERNIZATION.md, "Art folders".
//
//////////////////////////////////////////////////////////////////////////////

namespace ArtFolderTests
{
    std::string Folders(const char* szExtension)
    {
        std::string str;

        for (const char* const* psz = ArtFolders(szExtension); *psz; psz++) {
            str += str.empty() ? *psz : std::string(",") + *psz;
        }

        return str;
    }

    TEST_CLASS(Table)
    {
    public:
        TEST_METHOD(FoldersByType)
        {
            AreEqualStr("Textures,Models", Folders("dds"));
            AreEqualStr("Interface,Models,Fonts", Folders("mdl"));
            AreEqualStr("Models", Folders(".x"));
            AreEqualStr("Models", Folders("cvh"));
            AreEqualStr("Interface", Folders("mml"));
            AreEqualStr("Audio", Folders("WAV"));
            AreEqualStr("Movies", Folders("mp4"));
            AreEqualStr("Data", Folders(".igc"));
            AreEqualStr("", Folders("txt"));
            AreEqualStr("", Folders(nullptr));
        }
    };

    TEST_CLASS(GetFile)
    {
        std::filesystem::path m_root;

        void Touch(const std::filesystem::path& path)
        {
            std::filesystem::create_directories(path.parent_path());
            FILE* pfile = fopen(path.string().c_str(), "wb");
            Assert::IsNotNull(pfile);
            fclose(pfile);
        }

    public:
        TEST_METHOD_INITIALIZE(Setup)
        {
            char szTemp[MAX_PATH];
            GetTempPathA(MAX_PATH, szTemp);
            m_root = std::filesystem::path(szTemp) / ("artfolders" + std::to_string(GetCurrentProcessId()));
            std::filesystem::remove_all(m_root);
            std::filesystem::create_directories(m_root);
            UTL::SetArtPath(m_root.string().c_str());
        }

        TEST_METHOD_CLEANUP(Cleanup)
        {
            std::error_code ec;
            std::filesystem::remove_all(m_root, ec);
        }

        TEST_METHOD(FolderFirst)
        {
            Touch(m_root / "Data" / "core.igc");
            Touch(m_root / "core.igc");

            char sz[MAX_PATH];
            Assert::AreEqual(S_OK, UTL::getFile("core", ".igc", sz, false, false));
            AreEqualStr((m_root / "Data" / "core.igc").string().c_str(), sz);
        }

        TEST_METHOD(RootWhenNotInTheFolder)
        {
            Touch(m_root / "hull.cvh");

            char sz[MAX_PATH];
            Assert::AreEqual(S_OK, UTL::getFile("hull", ".cvh", sz, false, false));
            AreEqualStr((m_root / "hull.cvh").string().c_str(), sz);
        }

        // A file that is nowhere gets its folder's path, so writing it puts
        // it there; the name ends at a space, as before
        TEST_METHOD(MissingGetsTheFolderPath)
        {
            char sz[MAX_PATH];
            Assert::AreEqual(E_FAIL, UTL::getFile("newmap extra", ".igc", sz, false, true));
            AreEqualStr((m_root / "Data" / "newmap.igc").string().c_str(), sz);
        }
    };
}
