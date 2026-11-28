#include "pch.h"
#include "MessageCore.h"
#include "FedTransport.h"

#include <functional>
#include <memory>

//////////////////////////////////////////////////////////////////////////////
//
// Networking (_Utility): how FedMessaging packs a message, the session
// details and host:port parsing, and a real session over QUIC on the
// loopback: host, join, messages both ways, leave. See MODERNIZATION.md,
// "Networking: MsQuic replaces DirectPlay".
//
// The loopback test hosts on the game server's UDP port (47700), so it fails
// if a server is running on the same machine.
//
//////////////////////////////////////////////////////////////////////////////

ALLOC_MSG_LIST;

// Test messages, numbered past the game's
DEFINE_FEDMSG(CS, TEST_CHAT, 390)
    FM_VAR_ITEM(Text);
    FM_VAR_ITEM(Blob);
    FM_VAR_ITEM(Unused);
    int   n;
    short s;
END_FEDMSG

DEFINE_FEDMSG(S, TEST_TICK, 391)
    int n;
END_FEDMSG

namespace NetworkTests
{
    // {6A0C9C3E-1F4B-4E39-9B53-6F1D2C0A7E41}: not a lobby, so the game server's port
    const FedApplication c_appTest = { 0x7e57, 47700 };

    //
    // Records what FedMessaging reports
    //

    class RecordingSite : public IFedMessagingSite {
    public:
        std::vector<std::vector<BYTE>> m_messages;
        std::vector<std::string>       m_from;
        std::string                    m_strMessageBox;
        int                            m_cNew       = 0;
        int                            m_cDestroyed = 0;
        int                            m_cBadCRC    = 0;

        HRESULT OnAppMessage(FedMessaging* pthis, CFMConnection& cnxnFrom, FEDMESSAGE* pfm) override
        {
            m_messages.emplace_back((BYTE*)pfm, (BYTE*)pfm + pfm->cbmsg);
            m_from.push_back(cnxnFrom.GetName());
            return S_OK;
        }

        int OnMessageBox(FedMessaging* pthis, const char* strText, const char* strCaption, UINT nType) override
        {
            m_strMessageBox = strText;
            return 0;
        }

        HRESULT OnNewConnection(FedMessaging* pthis, CFMConnection& cnxn) override
        {
            m_cNew++;
            return S_OK;
        }

        HRESULT OnDestroyConnection(FedMessaging* pthis, CFMConnection& cnxn) override
        {
            m_cDestroyed++;
            return S_OK;
        }

        void OnBadCRC(FedMessaging* pthis, CFMConnection& cnxn, BYTE* pMsg, DWORD cbMsg) override
        {
            m_cBadCRC++;
        }

        template<class Message>
        const Message* Get(size_t index)
        {
            return (const Message*)m_messages[index].data();
        }
    };

    //
    // Calls ReceiveMessages on each until done() or the timeout
    //

    bool Pump(std::initializer_list<FedMessaging*> list, std::function<bool()> done, DWORD msTimeout = 10000)
    {
        DWORD timeStart = GetTickCount();

        while (!done()) {
            if (GetTickCount() - timeStart > msTimeout) {
                return false;
            }

            for (FedMessaging* pfm : list) {
                pfm->ReceiveMessages();
            }

            Sleep(5);
        }

        return true;
    }

    const BYTE c_blob[5] = { 1, 2, 3, 0, 5 };

    TEST_CLASS(Packing)
    {
    public:
        // The ib/cb pairs start right after the header, the variable data
        // right after the fixed part, in order; cbmsg is the total
        TEST_METHOD(VariableFieldsFollowTheFixedPart)
        {
            RecordingSite                 site;
            std::unique_ptr<FedMessaging> pfm = std::make_unique<FedMessaging>(&site);
            BYTE                          buf[256];

            FMD_CS_TEST_CHAT* pchat = (FMD_CS_TEST_CHAT*)
                BEGIN_PFM_CREATE_PREALLOC(*pfm, buf, CS, TEST_CHAT)
                    FM_VAR_PARM("hello  ", CB_ZTS)
                    FM_VAR_PARM(c_blob, sizeof(c_blob))
                    FM_VAR_PARM(NULL, 0)
                END_PFM_CREATE

            Assert::IsTrue((BYTE*)pchat == buf);
            Assert::AreEqual((int)FM_CS_TEST_CHAT, (int)pchat->fmid);
            Assert::AreEqual((int)sizeof(FEDMESSAGE), (int)((BYTE*)&pchat->ibText - buf));

            // Trailing spaces are trimmed; the terminator is counted
            Assert::AreEqual((int)sizeof(FMD_CS_TEST_CHAT), (int)pchat->ibText);
            Assert::AreEqual(6, (int)pchat->cbText);
            AreEqualStr("hello", FM_VAR_REF(pchat, Text));

            Assert::AreEqual(pchat->ibText + 6, (int)pchat->ibBlob);
            Assert::AreEqual(5, (int)pchat->cbBlob);
            Assert::AreEqual(0, memcmp(FM_VAR_REF(pchat, Blob), c_blob, sizeof(c_blob)));

            Assert::AreEqual(0, (int)pchat->cbUnused);
            Assert::IsNull(FM_VAR_REF(pchat, Unused));

            Assert::AreEqual((int)sizeof(FMD_CS_TEST_CHAT) + 6 + 5, (int)pchat->cbmsg);
        }

        // An empty or null string is left out, so FM_VAR_REF gives null
        // (which is why ZString must take a null const char*)
        TEST_METHOD(EmptyStringsAreNull)
        {
            RecordingSite                 site;
            std::unique_ptr<FedMessaging> pfm = std::make_unique<FedMessaging>(&site);
            BYTE                          buf[256];

            FMD_CS_TEST_CHAT* pchat = (FMD_CS_TEST_CHAT*)
                BEGIN_PFM_CREATE_PREALLOC(*pfm, buf, CS, TEST_CHAT)
                    FM_VAR_PARM("", CB_ZTS)
                    FM_VAR_PARM(NULL, CB_ZTS)
                    FM_VAR_PARM(NULL, 0)
                END_PFM_CREATE

            Assert::AreEqual(0, (int)pchat->cbText);
            Assert::IsNull(FM_VAR_REF(pchat, Text));
            Assert::AreEqual(0, (int)pchat->cbBlob);
            Assert::AreEqual((int)sizeof(FMD_CS_TEST_CHAT), (int)pchat->cbmsg);
            AreEqualStr("", ZString(FM_VAR_REF(pchat, Text)));
        }

        // Only spaces trims to an empty string that is still sent (one byte),
        // unlike "", which is left out
        TEST_METHOD(QuirkSpacesOnlyIsAnEmptyString)
        {
            RecordingSite                 site;
            std::unique_ptr<FedMessaging> pfm = std::make_unique<FedMessaging>(&site);
            BYTE                          buf[256];

            FMD_CS_TEST_CHAT* pchat = (FMD_CS_TEST_CHAT*)
                BEGIN_PFM_CREATE_PREALLOC(*pfm, buf, CS, TEST_CHAT)
                    FM_VAR_PARM("   ", CB_ZTS)
                    FM_VAR_PARM(NULL, 0)
                    FM_VAR_PARM(NULL, 0)
                END_PFM_CREATE

            Assert::AreEqual(1, (int)pchat->cbText);
            AreEqualStr("", FM_VAR_REF(pchat, Text));
        }

        TEST_METHOD(AllocatedCopy)
        {
            RecordingSite                 site;
            std::unique_ptr<FedMessaging> pfm = std::make_unique<FedMessaging>(&site);

            BEGIN_PFM_CREATE_ALLOC(*pfm, pchat, CS, TEST_CHAT)
                FM_VAR_PARM("x", CB_ZTS)
                FM_VAR_PARM(NULL, 0)
                FM_VAR_PARM(NULL, 0)
            END_PFM_CREATE

            Assert::IsNotNull(pchat);
            Assert::AreEqual((int)sizeof(FMD_CS_TEST_CHAT) + 2, (int)pchat->cbmsg);
            AreEqualStr("x", FM_VAR_REF(pchat, Text));
            PFM_DEALLOC(pchat);
        }
    };

    TEST_CLASS(Parsing)
    {
    public:
        TEST_METHOD(HostPort)
        {
            std::string    strHost;
            unsigned short port;

            FedParseHostPort("server.example", 47700, strHost, port);
            AreEqualStr("server.example", strHost);
            Assert::AreEqual(47700, (int)port);

            FedParseHostPort("10.0.0.2:5000", 47700, strHost, port);
            AreEqualStr("10.0.0.2", strHost);
            Assert::AreEqual(5000, (int)port);

            // A bad port gives the default; the host loses it either way
            FedParseHostPort("host:0", 47700, strHost, port);
            AreEqualStr("host", strHost);
            Assert::AreEqual(47700, (int)port);
            FedParseHostPort("host:70000", 47700, strHost, port);
            Assert::AreEqual(47700, (int)port);
            FedParseHostPort("host:abc", 47700, strHost, port);
            AreEqualStr("host", strHost);
            Assert::AreEqual(47700, (int)port);

            // An IPv6 literal (more than one colon) has no port
            FedParseHostPort("::1", 47700, strHost, port);
            AreEqualStr("::1", strHost);
            Assert::AreEqual(47700, (int)port);

            FedParseHostPort(nullptr, 47700, strHost, port);
            AreEqualStr("", strHost);
            Assert::AreEqual(47700, (int)port);
        }

        // As the server writes them (SetSessionDetails): Name\tValue\n...
        TEST_METHOD(SessionDetails)
        {
            FMSessionDesc desc(GUID_NULL, "GAM\tFriday Night Game\nPLR\t3\nLIM\t16\n");

            AreEqualStr("Friday Night Game", desc.GetGameName());
            Assert::AreEqual(3, desc.GetNumPlayers());
            Assert::AreEqual(16, desc.GetMaxPlayers());
        }

        TEST_METHOD(SessionDetailsSkipUnknownNames)
        {
            FMSessionDesc desc(GUID_NULL, "VER\t2\nGAM\tA\nXYZ\tignored\nPLR\t1\n");

            AreEqualStr("A", desc.GetGameName());
            Assert::AreEqual(1, desc.GetNumPlayers());
            Assert::AreEqual(1, desc.GetMaxPlayers());     // the default
        }

        TEST_METHOD(SessionDetailsEmpty)
        {
            FMSessionDesc desc(GUID_NULL, "");

            AreEqualStr("", desc.GetGameName());
            Assert::AreEqual(0, desc.GetNumPlayers());
            Assert::AreEqual(1, desc.GetMaxPlayers());
        }
    };

    TEST_CLASS(Loopback)
    {
    public:
        TEST_METHOD(HostJoinExchangeLeave)
        {
            RecordingSite                 siteServer;
            RecordingSite                 siteClient;
            std::unique_ptr<FedMessaging> pserver = std::make_unique<FedMessaging>(&siteServer);
            std::unique_ptr<FedMessaging> pclient = std::make_unique<FedMessaging>(&siteClient);

            HRESULT hr = pserver->HostSession(c_appTest, true, nullptr, false);
            Assert::IsTrue(SUCCEEDED(hr), Wide("HostSession: " + siteServer.m_strMessageBox).c_str());

            hr = pclient->JoinSession(c_appTest, "127.0.0.1:47700", "tester");
            Assert::IsTrue(SUCCEEDED(hr), Wide("JoinSession: " + siteClient.m_strMessageBox).c_str());
            Assert::IsTrue(pclient->GetSessionGuid() == pserver->GetSessionGuid(), L"the welcome carries the session instance");

            Assert::IsTrue(Pump({ pserver.get() }, [&] { return siteServer.m_cNew == 1; }), L"the server sees the client");
            Assert::AreEqual(1, pserver->GetConnectionCount());

            //
            // Client to server: one message with variable fields
            //

            BEGIN_PFM_CREATE(*pclient, pchat, CS, TEST_CHAT)
                FM_VAR_PARM("hello server", CB_ZTS)
                FM_VAR_PARM(c_blob, sizeof(c_blob))
                FM_VAR_PARM(NULL, 0)
            END_PFM_CREATE
            pchat->n = 42;
            pchat->s = -7;
            pclient->SendMessages(pclient->GetServerConnection(), FM_GUARANTEED, FM_FLUSH);

            Assert::IsTrue(Pump({ pserver.get() }, [&] { return siteServer.m_messages.size() == 1; }), L"the server gets the message");

            const FMD_CS_TEST_CHAT* pchatIn = siteServer.Get<FMD_CS_TEST_CHAT>(0);
            Assert::AreEqual((int)FM_CS_TEST_CHAT, (int)pchatIn->fmid);
            AreEqualStr("hello server", FM_VAR_REF(pchatIn, Text));
            Assert::AreEqual(0, memcmp(FM_VAR_REF(pchatIn, Blob), c_blob, sizeof(c_blob)));
            Assert::IsNull(FM_VAR_REF(pchatIn, Unused));
            Assert::AreEqual(42, pchatIn->n);
            Assert::AreEqual(-7, (int)pchatIn->s);
            AreEqualStr("tester", siteServer.m_from[0]);

            //
            // Server to everyone: two messages in one packet, in order
            //

            BEGIN_PFM_CREATE(*pserver, ptick1, S, TEST_TICK)
            END_PFM_CREATE
            ptick1->n = 1;
            BEGIN_PFM_CREATE(*pserver, ptick2, S, TEST_TICK)
            END_PFM_CREATE
            ptick2->n = 2;
            pserver->SendMessages(pserver->Everyone(), FM_GUARANTEED, FM_FLUSH);

            Assert::IsTrue(Pump({ pclient.get() }, [&] { return siteClient.m_messages.size() == 2; }), L"the client gets both");
            Assert::AreEqual((int)FM_S_TEST_TICK, (int)siteClient.Get<FMD_S_TEST_TICK>(0)->fmid);
            Assert::AreEqual(1, siteClient.Get<FMD_S_TEST_TICK>(0)->n);
            Assert::AreEqual(2, siteClient.Get<FMD_S_TEST_TICK>(1)->n);
            AreEqualStr("Server", siteClient.m_from[0]);

            Assert::AreEqual(0, siteServer.m_cBadCRC + siteClient.m_cBadCRC);

            //
            // The client leaves; the server drops the connection
            //

            pclient->Shutdown();
            Assert::IsFalse(pclient->IsConnected());
            Assert::IsTrue(Pump({ pserver.get() }, [&] { return siteServer.m_cDestroyed == 1; }), L"the server sees the client go");
            Assert::AreEqual(0, pserver->GetConnectionCount());
        }

        // Nobody listening: the join fails, and says so (within the 15 s
        // JoinServer waits at most)
        TEST_METHOD(JoinWithoutServerFails)
        {
            RecordingSite                 site;
            std::unique_ptr<FedMessaging> pclient = std::make_unique<FedMessaging>(&site);

            HRESULT hr = pclient->JoinSession(c_appTest, "127.0.0.1:47790", "tester");

            Assert::IsTrue(FAILED(hr));
            Assert::IsFalse(pclient->IsConnected());
            Assert::IsTrue(site.m_strMessageBox.find("Could not join") == 0, Wide(site.m_strMessageBox).c_str());
        }
    };
}
