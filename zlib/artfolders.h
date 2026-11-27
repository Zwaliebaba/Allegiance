#ifndef _artfolders_h_
#define _artfolders_h_

//////////////////////////////////////////////////////////////////////////////
//
// Art folders
//
// The art is in folders by kind (Artwork\Art.proj builds them; see
// MODERNIZATION.md, "Art folders"), but MDL files, the core files and the
// code name it by bare name ("fig02", "ires01bmp", "newtarget"). A file is
// looked for in the folders for its type, in order, then in the art root
// itself (where a file put by hand still works). ModelerImpl::GetFile and
// UTL::getFile use this.
//
//////////////////////////////////////////////////////////////////////////////

// The folders for a file type (the extension, without the dot, lower case),
// NULL-terminated; an empty list for types that live in the root
inline const char* const* ArtFolders(const char* szExtension)
{
    static const char* const s_textures[]  = { "Textures", "Models", NULL };
    static const char* const s_mdl[]       = { "Interface", "Models", "Fonts", NULL };
    static const char* const s_models[]    = { "Models", NULL };
    static const char* const s_interface[] = { "Interface", NULL };
    static const char* const s_audio[]     = { "Audio", NULL };
    static const char* const s_movies[]    = { "Movies", NULL };
    static const char* const s_data[]      = { "Data", NULL };
    static const char* const s_none[]      = { NULL };

    struct Type { const char* m_szExtension; const char* const* m_pszFolders; };

    static const Type s_types[] = {
        { "dds", s_textures  },     // images: textures, then model textures
        { "mdl", s_mdl       },     // UI, models, the font
        { "x",   s_models    },
        { "cvh", s_models    },     // collision hulls
        { "mml", s_interface },     // help pages
        { "wav", s_audio     },     // sound effects (music is in Music\, by path)
        { "mp4", s_movies    },
        { "igc", s_data      },     // core files
        { "rtf", s_data      },
    };

    if (szExtension && *szExtension == '.') {
        szExtension++;
    }

    for (const Type& type : s_types) {
        if (szExtension && _stricmp(type.m_szExtension, szExtension) == 0) {
            return type.m_pszFolders;
        }
    }

    return s_none;
}

inline bool ArtFileExists(const char* szPath)
{
    DWORD dwAttributes = GetFileAttributesA(szPath);

    return dwAttributes != INVALID_FILE_ATTRIBUTES && !(dwAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

#endif
