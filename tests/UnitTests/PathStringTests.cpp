#include "pch.h"

//////////////////////////////////////////////////////////////////////////////
//
// PathString (zlib/zstring.h): + is a path join, not std::string's +. The
// game builds its art and config paths with it.
//
//////////////////////////////////////////////////////////////////////////////

namespace PathStringTests
{
    TEST_CLASS(Parts)
    {
    public:
        TEST_METHOD(BackslashesBecomeSlashes)
        {
            AreEqualStr("a/b/c", PathString("a\\b\\c"));
            AreEqualStr("", PathString((PCC)nullptr));
        }

        TEST_METHOD(FilenameExtensionNameDirectory)
        {
            PathString path("dir/sub/file.ext");

            AreEqualStr("file.ext", path.GetFilename());
            AreEqualStr("ext", path.GetExtension());
            AreEqualStr("file", path.GetName());
            AreEqualStr("dir/sub", path.GetDirectory());
        }

        TEST_METHOD(NoDirectoryNoExtension)
        {
            PathString path("file");

            AreEqualStr("file", path.GetFilename());
            AreEqualStr("", path.GetExtension());
            AreEqualStr("file", path.GetName());
            AreEqualStr("", path.GetDirectory());
        }

        // The extension starts at the first dot
        TEST_METHOD(QuirkExtensionFromFirstDot)
        {
            PathString path("archive.tar.gz");

            AreEqualStr("tar.gz", path.GetExtension());
            AreEqualStr("archive", path.GetName());
        }

        TEST_METHOD(Drive)
        {
            AreEqualStr("c:", PathString("c:/foo").GetDrive());
            AreEqualStr("", PathString("foo/bar").GetDrive());
            AreEqualStr("", PathString("").GetDrive());
        }

        // A network path's "drive" loses the share name's last letter, and is
        // empty without a path after the share (original code; only the
        // root-relative join below uses GetDrive)
        TEST_METHOD(QuirkNetworkDrive)
        {
            AreEqualStr("//server/shar", PathString("//server/share/dir").GetDrive());
            AreEqualStr("", PathString("//server/share").GetDrive());
        }

        TEST_METHOD(URL)
        {
            AreEqualStr("file://c:/x", URLString(PathString("c:/x")));
        }
    };

    TEST_CLASS(Join)
    {
    public:
        TEST_METHOD(PlusIsAPathJoin)
        {
            AreEqualStr("a/b", PathString("a") + "b");
            AreEqualStr("c:/games/allegiance/artwork", PathString("c:/games/allegiance") + "artwork");
            AreEqualStr("c:/games/x", PathString("c:/games/") + "x");
        }

        TEST_METHOD(RelativeSegments)
        {
            AreEqualStr("c:/games/data/file.mdl", PathString("c:/games/allegiance") + "../data/file.mdl");
            AreEqualStr("c:/games/x", PathString("c:/games") + "./x");
            AreEqualStr("c:/a/b/c", PathString("c:/a") + "b\\c");
        }

        TEST_METHOD(AbsoluteReplaces)
        {
            AreEqualStr("d:/b", PathString("c:/a") + "d:/b");
            AreEqualStr("//srv/share", PathString("c:/a") + "//srv/share");
        }

        // A root-relative path keeps the drive, with a doubled slash
        TEST_METHOD(QuirkRootRelative)
        {
            AreEqualStr("c://x", PathString("c:/a/b") + "/x");
        }
    };
}
