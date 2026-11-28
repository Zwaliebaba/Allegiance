#include "pch.h"
#include "ServiceHost.h"

//////////////////////////////////////////////////////////////////////////////
//
// The server's service switches (Service/ServiceHost.h): AllSrv32
// [-install [user password] | -remove | -service]; anything else is the
// program's own.
//
//////////////////////////////////////////////////////////////////////////////

namespace ServiceArgsTests
{
    template <size_t N>
    static ServiceArgs Parse(const char * (&rgsz)[N])
    {
        return ParseServiceArgs((int)N, const_cast<char **>(rgsz));
    }

    TEST_CLASS(Switches)
    {
    public:
        TEST_METHOD(NoArgumentsRunsInTheConsole)
        {
            const char * rgsz[] = { "AllSrv32" };
            Assert::IsTrue(Parse(rgsz).m_command == ServiceCommand::Console);
        }

        TEST_METHOD(InstallAsLocalSystem)
        {
            const char * rgsz[] = { "AllSrv32", "-install" };
            ServiceArgs args = Parse(rgsz);
            Assert::IsTrue(args.m_command == ServiceCommand::Install);
            Assert::IsNull(args.m_szUser);
            Assert::IsNull(args.m_szPassword);
        }

        TEST_METHOD(InstallAsAnAccount)
        {
            const char * rgsz[] = { "AllSrv32", "/INSTALL", "DOMAIN\\server", "secret" };
            ServiceArgs args = Parse(rgsz);
            Assert::IsTrue(args.m_command == ServiceCommand::Install);
            Assert::AreEqual("DOMAIN\\server", args.m_szUser);
            Assert::AreEqual("secret", args.m_szPassword);
        }

        TEST_METHOD(AnAccountWithoutItsPasswordIsAMistake)
        {
            const char * rgsz[] = { "AllSrv32", "-install", "server" };
            Assert::IsTrue(Parse(rgsz).m_command == ServiceCommand::Other);
        }

        TEST_METHOD(RemoveAndUninstall)
        {
            const char * rgsz1[] = { "AllSrv32", "-remove" };
            const char * rgsz2[] = { "AllSrv32", "-Uninstall" };
            const char * rgsz3[] = { "AllSrv32", "-remove", "now" };
            Assert::IsTrue(Parse(rgsz1).m_command == ServiceCommand::Remove);
            Assert::IsTrue(Parse(rgsz2).m_command == ServiceCommand::Remove);
            Assert::IsTrue(Parse(rgsz3).m_command == ServiceCommand::Other);
        }

        TEST_METHOD(ServiceOnlyAlone)
        {
            const char * rgsz1[] = { "AllSrv32", "-service" };
            const char * rgsz2[] = { "AllSrv32", "-service", "x" };
            Assert::IsTrue(Parse(rgsz1).m_command == ServiceCommand::Service);
            Assert::IsTrue(Parse(rgsz2).m_command == ServiceCommand::Other);
        }

        TEST_METHOD(TheProgramsOwnSwitchesPassThrough)
        {
            const char * rgsz1[] = { "AllSrv32", "-training", "1" };
            const char * rgsz2[] = { "AllSrv32", "install" };
            Assert::IsTrue(Parse(rgsz1).m_command == ServiceCommand::Other);
            Assert::IsTrue(Parse(rgsz2).m_command == ServiceCommand::Other);
        }
    };
}
