//////////////////////////////////////////////////////////////////////////////
//
// FedTransport on MsQuic, and the UDP session discovery beside it.
// See FedTransport.h and MODERNIZATION.md, "Networking".
//
// Built without the module's precompiled header: msquic.h needs Winsock 2,
// which can't be included after the plain <windows.h> the PCH brings in.
//
//////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>

#include <msquic.h>

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "FedTransport.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

namespace {

//////////////////////////////////////////////////////////////////////////////
//
// Wire format
//
// Stream: frames of [uint32 length][uint8 type][length - 1 bytes].
// Datagram: [uint8 type = Packet][packet].
//
// The hello carries the application ID (which session the client wants)
// and a protocol version; a mismatch is refused with a reason.
//
//////////////////////////////////////////////////////////////////////////////

const DWORD c_dwMagic           = 0x31514c41; // "ALQ1"
const DWORD c_dwProtocolVersion = 2; // 2: a 32-bit application ID in place of a GUID
const char  c_szAlpn[]          = "allegiance";

enum FrameType
{
    FrameHello   = 1,   // client -> server: magic, version, application ID, name
    FrameWelcome = 2,   // server -> client: session instance GUID
    FrameRefuse  = 3,   // server -> client: reason
    FramePacket  = 4    // either way: a FedMessaging packet
};

const DWORD c_cbMaxFrame = 64 << 10;

#pragma pack(push, 1)
struct HelloFrame
{
    DWORD m_dwMagic;
    DWORD m_dwVersion;
    DWORD m_idApplication;
    // followed by the zero-terminated name
};

struct ProbeFrame   // discovery request
{
    DWORD m_dwMagic;
    BYTE  m_type;   // 1
    DWORD m_idApplication;
};

struct ReplyFrame   // discovery reply
{
    DWORD          m_dwMagic;
    BYTE           m_type;   // 2
    DWORD          m_idApplication;
    GUID           m_guidInstance;
    unsigned short m_port;
    // followed by the zero-terminated session details
};
#pragma pack(pop)

const BYTE c_probeRequest = 1;
const BYTE c_probeReply   = 2;

//////////////////////////////////////////////////////////////////////////////
//
// MsQuic: msquic.dll is loaded on first use and shared by every transport in
// the process (client: game and lobby connections; server: players and the
// lobby). The registration is shared the same way.
//
//////////////////////////////////////////////////////////////////////////////

CRITICAL_SECTION       g_csQuic;
bool                   g_fcsQuic         = false;
int                    g_cQuicRefs       = 0;
HMODULE                g_hmsquic         = NULL;
const QUIC_API_TABLE*  g_pquic           = NULL;
HQUIC                  g_hregistration   = NULL;

void EnsureQuicLock()
{
    //
    // FedMessaging objects are globals (constructed before main), but
    // transports are only created from the game or server thread later on
    //

    if (!g_fcsQuic) {
        InitializeCriticalSection(&g_csQuic);
        g_fcsQuic = true;
    }
}

bool AcquireQuic(std::string& strError)
{
    EnsureQuicLock();
    EnterCriticalSection(&g_csQuic);

    if (g_cQuicRefs > 0) {
        g_cQuicRefs++;
        LeaveCriticalSection(&g_csQuic);
        return true;
    }

    g_hmsquic = LoadLibraryA("msquic.dll");

    if (g_hmsquic == NULL) {
        strError = "msquic.dll was not found next to the program.";
        LeaveCriticalSection(&g_csQuic);
        return false;
    }

    MsQuicOpenVersionFn pfnOpen = (MsQuicOpenVersionFn)GetProcAddress(g_hmsquic, "MsQuicOpenVersion");
    const void*         papi    = NULL;

    if (pfnOpen == NULL || QUIC_FAILED(pfnOpen(QUIC_API_VERSION_2, &papi))) {
        strError = "msquic.dll could not be initialised.";
        FreeLibrary(g_hmsquic);
        g_hmsquic = NULL;
        LeaveCriticalSection(&g_csQuic);
        return false;
    }

    g_pquic = (const QUIC_API_TABLE*)papi;

    QUIC_REGISTRATION_CONFIG config = { "Allegiance", QUIC_EXECUTION_PROFILE_LOW_LATENCY };

    if (QUIC_FAILED(g_pquic->RegistrationOpen(&config, &g_hregistration))) {
        strError = "MsQuic registration failed.";
        MsQuicCloseFn pfnClose = (MsQuicCloseFn)GetProcAddress(g_hmsquic, "MsQuicClose");
        if (pfnClose) pfnClose(g_pquic);
        g_pquic = NULL;
        FreeLibrary(g_hmsquic);
        g_hmsquic = NULL;
        LeaveCriticalSection(&g_csQuic);
        return false;
    }

    g_cQuicRefs = 1;
    LeaveCriticalSection(&g_csQuic);
    return true;
}

void ReleaseQuic()
{
    EnterCriticalSection(&g_csQuic);

    if (--g_cQuicRefs == 0) {
        //
        // RegistrationClose waits for every connection of the registration
        // to be closed, which the transports did before releasing
        //

        g_pquic->RegistrationClose(g_hregistration);
        g_hregistration = NULL;

        MsQuicCloseFn pfnClose = (MsQuicCloseFn)GetProcAddress(g_hmsquic, "MsQuicClose");
        if (pfnClose) pfnClose(g_pquic);
        g_pquic = NULL;

        FreeLibrary(g_hmsquic);
        g_hmsquic = NULL;
    }

    LeaveCriticalSection(&g_csQuic);
}

//
// Winsock, for the discovery sockets and address formatting
//

bool StartWinsock()
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

std::string FormatAddress(const SOCKADDR_INET& addr)
{
    char sz[INET6_ADDRSTRLEN] = { 0 };

    if (addr.si_family == AF_INET) {
        inet_ntop(AF_INET, (void*)&addr.Ipv4.sin_addr, sz, sizeof(sz));
    } else if (addr.si_family == AF_INET6) {
        if (IN6_IS_ADDR_V4MAPPED(&addr.Ipv6.sin6_addr)) {
            in_addr v4;
            memcpy(&v4, &addr.Ipv6.sin6_addr.u.Byte[12], sizeof(v4));
            inet_ntop(AF_INET, &v4, sz, sizeof(sz));
        } else {
            inet_ntop(AF_INET6, (void*)&addr.Ipv6.sin6_addr, sz, sizeof(sz));
        }
    }

    return sz;
}

//////////////////////////////////////////////////////////////////////////////
//
// Server certificate: self-signed, ECDSA P-256. The key is kept in the
// user's key store under a fixed name so it isn't regenerated every start;
// the certificate itself is made fresh in memory each time. Clients don't
// validate it yet (see MODERNIZATION.md).
//
//////////////////////////////////////////////////////////////////////////////

const wchar_t c_szKeyName[] = L"Allegiance Game Server";

PCCERT_CONTEXT CreateServerCertificate(std::string& strError)
{
    NCRYPT_PROV_HANDLE hprov = 0;
    NCRYPT_KEY_HANDLE  hkey  = 0;

    if (NCryptOpenStorageProvider(&hprov, MS_KEY_STORAGE_PROVIDER, 0) != ERROR_SUCCESS) {
        strError = "Could not open the key storage provider.";
        return NULL;
    }

    if (NCryptOpenKey(hprov, &hkey, c_szKeyName, 0, 0) != ERROR_SUCCESS) {
        hkey = 0;

        if (
               NCryptCreatePersistedKey(hprov, &hkey, NCRYPT_ECDSA_P256_ALGORITHM, c_szKeyName, 0, 0) != ERROR_SUCCESS
            || NCryptFinalizeKey(hkey, 0) != ERROR_SUCCESS
        ) {
            if (hkey) NCryptFreeObject(hkey);
            NCryptFreeObject(hprov);
            strError = "Could not create the server's key.";
            return NULL;
        }
    }

    //
    // Subject
    //

    const wchar_t* szSubject = L"CN=Allegiance Game Server";
    DWORD          cbName    = 0;

    CertStrToNameW(X509_ASN_ENCODING, szSubject, CERT_X500_NAME_STR, NULL, NULL, &cbName, NULL);
    std::vector<BYTE> name(cbName);
    CertStrToNameW(X509_ASN_ENCODING, szSubject, CERT_X500_NAME_STR, NULL, name.data(), &cbName, NULL);

    CERT_NAME_BLOB blobName = { cbName, name.data() };

    //
    // Server authentication EKU
    //

    LPSTR             rgszUsage[] = { (LPSTR)szOID_PKIX_KP_SERVER_AUTH };
    CERT_ENHKEY_USAGE usage       = { 1, rgszUsage };
    DWORD             cbUsage     = 0;

    CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, NULL, &cbUsage);
    std::vector<BYTE> usageEncoded(cbUsage);
    CryptEncodeObject(X509_ASN_ENCODING, X509_ENHANCED_KEY_USAGE, &usage, usageEncoded.data(), &cbUsage);

    CERT_EXTENSION  extension  = { (LPSTR)szOID_ENHANCED_KEY_USAGE, FALSE, { cbUsage, usageEncoded.data() } };
    CERT_EXTENSIONS extensions = { 1, &extension };

    //
    // Where Schannel finds the private key
    //

    CRYPT_KEY_PROV_INFO keyInfo = {};
    keyInfo.pwszContainerName = (LPWSTR)c_szKeyName;
    keyInfo.pwszProvName      = (LPWSTR)MS_KEY_STORAGE_PROVIDER;
    keyInfo.dwProvType        = 0;

    CRYPT_ALGORITHM_IDENTIFIER algorithm = {};
    algorithm.pszObjId = (LPSTR)szOID_ECDSA_SHA256;

    //
    // Valid from yesterday (clock skew) for a year
    //

    SYSTEMTIME timeStart, timeEnd;
    FILETIME   ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart  = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    t.QuadPart -= 24ull * 3600 * 10000000;
    ft.dwLowDateTime  = t.LowPart;
    ft.dwHighDateTime = t.HighPart;
    FileTimeToSystemTime(&ft, &timeStart);
    t.QuadPart += 366ull * 24 * 3600 * 10000000;
    ft.dwLowDateTime  = t.LowPart;
    ft.dwHighDateTime = t.HighPart;
    FileTimeToSystemTime(&ft, &timeEnd);

    PCCERT_CONTEXT pcert =
        CertCreateSelfSignCertificate(
            hkey,
            &blobName,
            0,
            &keyInfo,
            &algorithm,
            &timeStart,
            &timeEnd,
            &extensions
        );

    NCryptFreeObject(hkey);
    NCryptFreeObject(hprov);

    if (pcert == NULL) {
        strError = "Could not create the server's certificate.";
    }

    return pcert;
}

//////////////////////////////////////////////////////////////////////////////
//
// QuicTransport
//
//////////////////////////////////////////////////////////////////////////////

class QuicTransport;

//
// One QUIC connection. Owned by the transport's peer map; deleted when MsQuic
// reports the connection's shutdown complete.
//

struct Peer
{
    QuicTransport*    m_ptransport;
    HQUIC             m_hconnection;
    HQUIC             m_hstream;
    DWORD             m_id;
    bool              m_fJoined;        // hello / welcome exchanged
    bool              m_fGone;          // shutdown reported to the game thread
    volatile bool     m_fSendDone;      // our side of the stream finished and acknowledged
    LONG              m_cbDatagramMax;  // 0 while datagrams can't be sent
    LONG              m_cSends;         // stream sends in flight
    LONG              m_cbQueued;       // stream bytes in flight
    std::vector<BYTE> m_vrecv;          // stream reassembly (MsQuic thread only)
    std::vector<BYTE> m_vhello;         // client: sent once the handshake completes
    std::string       m_strName;
    std::string       m_strReason;      // why the connection is closing
    char              m_szAddress[INET6_ADDRSTRLEN];

    Peer() :
        m_ptransport(NULL),
        m_hconnection(NULL),
        m_hstream(NULL),
        m_id(0),
        m_fJoined(false),
        m_fGone(false),
        m_fSendDone(false),
        m_cbDatagramMax(0),
        m_cSends(0),
        m_cbQueued(0)
    {
        m_szAddress[0] = '\0';
    }
};

//
// A send buffer: header, frame and the QUIC_BUFFER describing it, in one
// allocation freed when MsQuic is done with it
//

struct SendBuffer
{
    QUIC_BUFFER m_buffer;
    Peer*       m_ppeer;
    bool        m_fGuaranteed;
    DWORD       m_cb;
    BYTE        m_data[1];

    static SendBuffer* Create(Peer* ppeer, BYTE type, const void* pv, DWORD cb, bool fStream, bool fGuaranteed)
    {
        DWORD       cbHeader = (fStream ? sizeof(DWORD) : 0) + 1;
        SendBuffer* psb      = (SendBuffer*)malloc(sizeof(SendBuffer) + cbHeader + cb);

        if (psb == NULL) {
            return NULL;
        }

        BYTE* p = psb->m_data;

        if (fStream) {
            DWORD cbFrame = 1 + cb;
            memcpy(p, &cbFrame, sizeof(cbFrame));
            p += sizeof(cbFrame);
        }

        *p++ = type;

        if (cb) {
            memcpy(p, pv, cb);
        }

        psb->m_buffer.Length = cbHeader + cb;
        psb->m_buffer.Buffer = psb->m_data;
        psb->m_ppeer         = ppeer;
        psb->m_fGuaranteed   = fGuaranteed;
        psb->m_cb            = cbHeader + cb;
        return psb;
    }
};

class QuicTransport : public FedTransport
{
public:
    CRITICAL_SECTION         m_cs;      // the maps, the event queue, the session details
    HANDLE                   m_hevent;  // signalled when an event is queued (WaitForEvent)
    HANDLE                   m_heventUser;
    bool                     m_fQuic;
    bool                     m_fServer;     // Listen was called

    HQUIC                    m_hconfigServer;
    HQUIC                    m_hconfigClient;
    HQUIC                    m_hlistener;
    PCCERT_CONTEXT           m_pcert;

    DWORD                    m_idApplication;
    GUID                     m_guidInstance;
    unsigned short           m_port;

    std::map<DWORD, Peer*>   m_peers;
    DWORD                    m_idNext;
    std::deque<FedTransportEvent> m_events;

    //
    // Discovery responder (server)
    //

    SOCKET                   m_socketDiscovery;
    std::string              m_strDetails;

    QuicTransport(HANDLE hEventSignal) :
        m_heventUser(hEventSignal),
        m_fQuic(false),
        m_fServer(false),
        m_hconfigServer(NULL),
        m_hconfigClient(NULL),
        m_hlistener(NULL),
        m_pcert(NULL),
        m_idApplication(0),
        m_guidInstance(),
        m_port(0),
        m_idNext(1),
        m_socketDiscovery(INVALID_SOCKET)
    {
        InitializeCriticalSection(&m_cs);
        m_hevent = CreateEvent(NULL, FALSE, FALSE, NULL);
        StartWinsock();
    }

    ~QuicTransport()
    {
        Close();

        if (m_socketDiscovery != INVALID_SOCKET) {
            closesocket(m_socketDiscovery);
        }

        WSACleanup();
        CloseHandle(m_hevent);
        DeleteCriticalSection(&m_cs);
    }

    bool Init(std::string& strError)
    {
        if (!AcquireQuic(strError)) {
            return false;
        }

        m_fQuic = true;
        return true;
    }

    //
    // Shut every connection down and wait for MsQuic to finish with them
    //

    void Close()
    {
        if (!m_fQuic) {
            return;
        }

        if (m_hlistener) {
            g_pquic->ListenerClose(m_hlistener);   // waits for the listener to stop
            m_hlistener = NULL;
        }

        //
        // Closing a connection discards stream data not yet sent, and the
        // last guaranteed messages (a logoff, say) matter: end each stream
        // gracefully first and give the other side a moment to acknowledge,
        // as FedMessaging used to wait for DirectPlay's send queue to drain
        //

        EnterCriticalSection(&m_cs);
        for (std::map<DWORD, Peer*>::iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
            Peer* ppeer = it->second;

            if (ppeer->m_hstream && ppeer->m_fJoined) {
                g_pquic->StreamShutdown(ppeer->m_hstream, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL, 0);
            } else {
                ppeer->m_fSendDone = true;
            }
        }
        LeaveCriticalSection(&m_cs);

        DWORD timeStart = GetTickCount();

        for (;;) {
            bool fDone = true;

            EnterCriticalSection(&m_cs);
            for (std::map<DWORD, Peer*>::iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
                fDone = fDone && it->second->m_fSendDone;
            }
            LeaveCriticalSection(&m_cs);

            if (fDone || GetTickCount() - timeStart > 2000) {
                break;
            }

            Sleep(10);
        }

        EnterCriticalSection(&m_cs);
        for (std::map<DWORD, Peer*>::iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
            g_pquic->ConnectionShutdown(it->second->m_hconnection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
        }
        LeaveCriticalSection(&m_cs);

        //
        // Peers are deleted from their SHUTDOWN_COMPLETE callbacks
        //

        timeStart = GetTickCount();

        for (;;) {
            EnterCriticalSection(&m_cs);
            bool fEmpty = m_peers.empty();
            LeaveCriticalSection(&m_cs);

            if (fEmpty || GetTickCount() - timeStart > 5000) {
                break;
            }

            Sleep(10);
        }

        if (m_hconfigServer) {
            g_pquic->ConfigurationClose(m_hconfigServer);
            m_hconfigServer = NULL;
        }

        if (m_hconfigClient) {
            g_pquic->ConfigurationClose(m_hconfigClient);
            m_hconfigClient = NULL;
        }

        if (m_pcert) {
            CertFreeCertificateContext(m_pcert);
            m_pcert = NULL;
        }

        ReleaseQuic();
        m_fQuic = false;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Configuration
    //
    //////////////////////////////////////////////////////////////////////////////

    HQUIC OpenConfiguration(bool fServer, std::string& strError)
    {
        QUIC_SETTINGS settings = {};

        settings.IdleTimeoutMs                   = 30000;
        settings.IsSet.IdleTimeoutMs             = TRUE;
        settings.HandshakeIdleTimeoutMs          = 10000;
        settings.IsSet.HandshakeIdleTimeoutMs    = TRUE;
        settings.KeepAliveIntervalMs             = 5000;
        settings.IsSet.KeepAliveIntervalMs       = TRUE;
        settings.DatagramReceiveEnabled          = TRUE;
        settings.IsSet.DatagramReceiveEnabled    = TRUE;
        settings.PeerBidiStreamCount             = fServer ? 1 : 0;
        settings.IsSet.PeerBidiStreamCount       = TRUE;

        QUIC_BUFFER alpn = { (uint32_t)(sizeof(c_szAlpn) - 1), (uint8_t*)c_szAlpn };
        HQUIC       hconfig = NULL;

        if (QUIC_FAILED(g_pquic->ConfigurationOpen(g_hregistration, &alpn, 1, &settings, sizeof(settings), NULL, &hconfig))) {
            strError = "MsQuic configuration failed.";
            return NULL;
        }

        QUIC_CREDENTIAL_CONFIG credential = {};

        if (fServer) {
            m_pcert = CreateServerCertificate(strError);

            if (m_pcert == NULL) {
                g_pquic->ConfigurationClose(hconfig);
                return NULL;
            }

            credential.Type               = QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT;
            credential.Flags              = QUIC_CREDENTIAL_FLAG_NONE;
            credential.CertificateContext = (QUIC_CERTIFICATE*)m_pcert;
        } else {
            //
            // Servers have self-signed certificates; see MODERNIZATION.md
            //

            credential.Type  = QUIC_CREDENTIAL_TYPE_NONE;
            credential.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
        }

        QUIC_STATUS status = g_pquic->ConfigurationLoadCredential(hconfig, &credential);

        if (QUIC_FAILED(status)) {
            char sz[200];
            wsprintfA(sz, "Loading the TLS credential failed (0x%08x). QUIC needs TLS 1.3 from Windows: Windows 11 or Windows Server 2022 or later.", status);
            strError = sz;
            g_pquic->ConfigurationClose(hconfig);
            return NULL;
        }

        return hconfig;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Events
    //
    //////////////////////////////////////////////////////////////////////////////

    void QueueEvent(FedTransportEvent& event)
    {
        EnterCriticalSection(&m_cs);
        m_events.push_back(FedTransportEvent());
        std::swap(m_events.back(), event);
        LeaveCriticalSection(&m_cs);

        SetEvent(m_hevent);

        if (m_heventUser) {
            SetEvent(m_heventUser);
        }
    }

    void QueueSimpleEvent(FedTransportEvent::Type type, DWORD idPeer, const std::string& strText = std::string())
    {
        FedTransportEvent event;
        event.m_type    = type;
        event.m_idPeer  = idPeer;
        event.m_strText = strText;
        QueueEvent(event);
    }

    bool PopEvent(FedTransportEvent& event) override
    {
        EnterCriticalSection(&m_cs);
        bool fAny = !m_events.empty();

        if (fAny) {
            std::swap(event, m_events.front());
            m_events.pop_front();
        }

        LeaveCriticalSection(&m_cs);
        return fAny;
    }

    bool WaitForEvent(DWORD ms) override
    {
        EnterCriticalSection(&m_cs);
        bool fAny = !m_events.empty();
        LeaveCriticalSection(&m_cs);

        if (fAny) {
            return true;
        }

        return WaitForSingleObject(m_hevent, ms) == WAIT_OBJECT_0;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // Peers
    //
    //////////////////////////////////////////////////////////////////////////////

    Peer* AddPeer(HQUIC hconnection)
    {
        Peer* ppeer = new Peer();
        ppeer->m_ptransport  = this;
        ppeer->m_hconnection = hconnection;

        EnterCriticalSection(&m_cs);
        ppeer->m_id = m_idNext++;
        m_peers[ppeer->m_id] = ppeer;
        LeaveCriticalSection(&m_cs);

        //
        // Remote address, for GetIPAddress
        //

        QUIC_ADDR addr = {};
        uint32_t  cb   = sizeof(addr);

        if (QUIC_SUCCEEDED(g_pquic->GetParam(hconnection, QUIC_PARAM_CONN_REMOTE_ADDRESS, &cb, &addr))) {
            lstrcpynA(ppeer->m_szAddress, FormatAddress(addr).c_str(), sizeof(ppeer->m_szAddress));
        }

        return ppeer;
    }

    void SendFrame(Peer* ppeer, BYTE type, const void* pv, DWORD cb, QUIC_SEND_FLAGS flags = QUIC_SEND_FLAG_NONE)
    {
        if (ppeer->m_hstream == NULL) {
            return;
        }

        SendBuffer* psb = SendBuffer::Create(ppeer, type, pv, cb, true, true);

        if (psb == NULL) {
            return;
        }

        InterlockedIncrement(&ppeer->m_cSends);
        InterlockedExchangeAdd(&ppeer->m_cbQueued, psb->m_cb);

        if (QUIC_FAILED(g_pquic->StreamSend(ppeer->m_hstream, &psb->m_buffer, 1, flags, psb))) {
            InterlockedDecrement(&ppeer->m_cSends);
            InterlockedExchangeAdd(&ppeer->m_cbQueued, -(LONG)psb->m_cb);
            free(psb);
        }
    }

    void Shutdown(Peer* ppeer, const char* szReason)
    {
        if (ppeer->m_strReason.empty()) {
            ppeer->m_strReason = szReason;
        }

        g_pquic->ConnectionShutdown(ppeer->m_hconnection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
    }

    //
    // A complete frame from the stream (MsQuic thread)
    //

    void OnFrame(Peer* ppeer, BYTE type, const BYTE* pb, DWORD cb)
    {
        switch (type) {
            case FrameHello:
            {
                if (!m_fServer || ppeer->m_fJoined) {
                    return;
                }

                const HelloFrame* phello = (const HelloFrame*)pb;

                if (cb < sizeof(HelloFrame) || phello->m_dwMagic != c_dwMagic) {
                    Shutdown(ppeer, "not an Allegiance client");
                    return;
                }

                const char* szRefuse = NULL;

                if (phello->m_dwVersion != c_dwProtocolVersion) {
                    szRefuse = "The server runs a different version of Allegiance.";
                } else if (phello->m_idApplication != m_idApplication) {
                    szRefuse = "The server is not hosting that kind of session.";
                }

                if (szRefuse) {
                    SendFrame(ppeer, FrameRefuse, szRefuse, lstrlenA(szRefuse) + 1, QUIC_SEND_FLAG_FIN);
                    ppeer->m_strReason = szRefuse;
                    return;
                }

                const char* szName = (const char*)(phello + 1);
                DWORD       cchMax = cb - sizeof(HelloFrame);

                ppeer->m_strName.assign(szName, strnlen(szName, cchMax));
                ppeer->m_fJoined = true;

                SendFrame(ppeer, FrameWelcome, &m_guidInstance, sizeof(m_guidInstance));

                FedTransportEvent event;
                event.m_type    = FedTransportEvent::PeerJoined;
                event.m_idPeer  = ppeer->m_id;
                event.m_strText = ppeer->m_strName;
                QueueEvent(event);
            }
            break;

            case FrameWelcome:
            {
                if (m_fServer || ppeer->m_fJoined || cb < sizeof(GUID)) {
                    return;
                }

                ppeer->m_fJoined = true;

                FedTransportEvent event;
                event.m_type   = FedTransportEvent::Joined;
                event.m_idPeer = ppeer->m_id;
                memcpy(&event.m_guid, pb, sizeof(GUID));
                QueueEvent(event);
            }
            break;

            case FrameRefuse:
            {
                if (m_fServer) {
                    return;
                }

                std::string strReason((const char*)pb, strnlen((const char*)pb, cb));
                ppeer->m_strReason = strReason;
                QueueSimpleEvent(FedTransportEvent::JoinRefused, ppeer->m_id, strReason);
                Shutdown(ppeer, strReason.c_str());
            }
            break;

            case FramePacket:
                QueuePacket(ppeer, pb, cb);
            break;
        }
    }

    void QueuePacket(Peer* ppeer, const BYTE* pb, DWORD cb)
    {
        //
        // Packets only count once the hello and welcome have been exchanged
        //

        if (!ppeer->m_fJoined || cb == 0) {
            return;
        }

        FedTransportEvent event;
        event.m_type   = FedTransportEvent::Packet;
        event.m_idPeer = ppeer->m_id;
        event.m_data.assign(pb, pb + cb);
        QueueEvent(event);
    }

    void OnStreamData(Peer* ppeer, const QUIC_BUFFER* pbuffers, uint32_t count)
    {
        std::vector<BYTE>& v = ppeer->m_vrecv;

        for (uint32_t index = 0; index < count; index++) {
            v.insert(v.end(), pbuffers[index].Buffer, pbuffers[index].Buffer + pbuffers[index].Length);
        }

        size_t offset = 0;

        while (v.size() - offset >= sizeof(DWORD)) {
            DWORD cbFrame;
            memcpy(&cbFrame, &v[offset], sizeof(cbFrame));

            if (cbFrame == 0 || cbFrame > c_cbMaxFrame) {
                v.clear();
                Shutdown(ppeer, "bad frame");
                return;
            }

            if (v.size() - offset - sizeof(DWORD) < cbFrame) {
                break;
            }

            const BYTE* pframe = &v[offset + sizeof(DWORD)];
            OnFrame(ppeer, pframe[0], pframe + 1, cbFrame - 1);
            offset += sizeof(DWORD) + cbFrame;
        }

        v.erase(v.begin(), v.begin() + offset);
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // MsQuic callbacks
    //
    //////////////////////////////////////////////////////////////////////////////

    static QUIC_STATUS QUIC_API StreamCallback(HQUIC hstream, void* pcontext, QUIC_STREAM_EVENT* pevent)
    {
        Peer*          ppeer      = (Peer*)pcontext;
        QuicTransport* ptransport = ppeer->m_ptransport;

        switch (pevent->Type) {
            case QUIC_STREAM_EVENT_RECEIVE:
                ptransport->OnStreamData(ppeer, pevent->RECEIVE.Buffers, pevent->RECEIVE.BufferCount);
            break;

            case QUIC_STREAM_EVENT_SEND_COMPLETE:
            {
                SendBuffer* psb = (SendBuffer*)pevent->SEND_COMPLETE.ClientContext;

                if (psb) {
                    InterlockedDecrement(&ppeer->m_cSends);
                    InterlockedExchangeAdd(&ppeer->m_cbQueued, -(LONG)psb->m_cb);

                    if (pevent->SEND_COMPLETE.Canceled && psb->m_fGuaranteed && ppeer->m_fJoined && !ppeer->m_fGone) {
                        ptransport->QueueSimpleEvent(FedTransportEvent::SendFailed, ppeer->m_id);
                    }

                    free(psb);
                }
            }
            break;

            case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
            case QUIC_STREAM_EVENT_PEER_SEND_ABORTED:
                ptransport->Shutdown(ppeer, "the other side closed the connection");
            break;

            case QUIC_STREAM_EVENT_SEND_SHUTDOWN_COMPLETE:
                ppeer->m_fSendDone = true;
            break;

            case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
                //
                // The connection's SHUTDOWN_COMPLETE closes the stream handle
                //

                ppeer->m_fSendDone = true;
            break;

            default:
            break;
        }

        return QUIC_STATUS_SUCCESS;
    }

    static QUIC_STATUS QUIC_API ConnectionCallback(HQUIC hconnection, void* pcontext, QUIC_CONNECTION_EVENT* pevent)
    {
        Peer*          ppeer      = (Peer*)pcontext;
        QuicTransport* ptransport = ppeer->m_ptransport;

        switch (pevent->Type) {
            case QUIC_CONNECTION_EVENT_CONNECTED:
                //
                // Client: open the stream and say hello
                //

                if (!ptransport->m_fServer && ppeer->m_hstream == NULL) {
                    if (
                           QUIC_FAILED(g_pquic->StreamOpen(hconnection, QUIC_STREAM_OPEN_FLAG_NONE, StreamCallback, ppeer, &ppeer->m_hstream))
                        || QUIC_FAILED(g_pquic->StreamStart(ppeer->m_hstream, QUIC_STREAM_START_FLAG_NONE))
                    ) {
                        if (ppeer->m_hstream) {
                            g_pquic->StreamClose(ppeer->m_hstream);
                            ppeer->m_hstream = NULL;
                        }

                        ptransport->Shutdown(ppeer, "could not open a stream");
                    } else {
                        ptransport->SendFrame(ppeer, FrameHello, ppeer->m_vhello.data(), (DWORD)ppeer->m_vhello.size());
                    }
                }
            break;

            case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED:
            {
                //
                // Server: the client's one bidirectional stream
                //

                if (ppeer->m_hstream == NULL) {
                    ppeer->m_hstream = pevent->PEER_STREAM_STARTED.Stream;
                    g_pquic->SetCallbackHandler(ppeer->m_hstream, (void*)StreamCallback, ppeer);
                } else {
                    g_pquic->StreamClose(pevent->PEER_STREAM_STARTED.Stream);
                }
            }
            break;

            case QUIC_CONNECTION_EVENT_DATAGRAM_STATE_CHANGED:
                InterlockedExchange(
                    &ppeer->m_cbDatagramMax,
                    pevent->DATAGRAM_STATE_CHANGED.SendEnabled ? pevent->DATAGRAM_STATE_CHANGED.MaxSendLength : 0
                );
            break;

            case QUIC_CONNECTION_EVENT_DATAGRAM_RECEIVED:
            {
                const QUIC_BUFFER* pbuffer = pevent->DATAGRAM_RECEIVED.Buffer;

                if (pbuffer->Length > 1 && pbuffer->Buffer[0] == FramePacket) {
                    ptransport->QueuePacket(ppeer, pbuffer->Buffer + 1, pbuffer->Length - 1);
                }
            }
            break;

            case QUIC_CONNECTION_EVENT_DATAGRAM_SEND_STATE_CHANGED:
                if (QUIC_DATAGRAM_SEND_STATE_IS_FINAL(pevent->DATAGRAM_SEND_STATE_CHANGED.State)) {
                    free(pevent->DATAGRAM_SEND_STATE_CHANGED.ClientContext);
                }
            break;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT:
                if (ppeer->m_strReason.empty()) {
                    QUIC_STATUS status = pevent->SHUTDOWN_INITIATED_BY_TRANSPORT.Status;

                    if (status == QUIC_STATUS_CONNECTION_IDLE) {
                        ppeer->m_strReason = "the connection timed out";
                    } else if (status == QUIC_STATUS_UNREACHABLE) {
                        ppeer->m_strReason = "the server is unreachable";
                    } else if (status == QUIC_STATUS_CONNECTION_REFUSED) {
                        ppeer->m_strReason = "the connection was refused";
                    } else {
                        char sz[64];
                        wsprintfA(sz, "network error 0x%08x", status);
                        ppeer->m_strReason = sz;
                    }
                }
            break;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER:
                if (ppeer->m_strReason.empty()) {
                    ppeer->m_strReason = "the other side closed the connection";
                }
            break;

            case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
                ptransport->OnShutdownComplete(ppeer);
            break;

            default:
            break;
        }

        return QUIC_STATUS_SUCCESS;
    }

    void OnShutdownComplete(Peer* ppeer)
    {
        //
        // Tell the game thread, unless the connection never got as far as
        // the welcome, in which case a client hears JoinRefused instead
        //

        if (ppeer->m_fJoined) {
            ppeer->m_fGone = true;
            QueueSimpleEvent(FedTransportEvent::PeerGone, ppeer->m_id, ppeer->m_strReason);
        } else if (!m_fServer) {
            QueueSimpleEvent(
                FedTransportEvent::JoinRefused,
                ppeer->m_id,
                ppeer->m_strReason.empty() ? std::string("the connection failed") : ppeer->m_strReason
            );
        }

        EnterCriticalSection(&m_cs);
        m_peers.erase(ppeer->m_id);
        LeaveCriticalSection(&m_cs);

        if (ppeer->m_hstream) {
            g_pquic->StreamClose(ppeer->m_hstream);
        }

        if (ppeer->m_hconnection) {
            g_pquic->ConnectionClose(ppeer->m_hconnection);
        }

        delete ppeer;
    }

    static QUIC_STATUS QUIC_API ListenerCallback(HQUIC hlistener, void* pcontext, QUIC_LISTENER_EVENT* pevent)
    {
        QuicTransport* ptransport = (QuicTransport*)pcontext;

        if (pevent->Type != QUIC_LISTENER_EVENT_NEW_CONNECTION) {
            return QUIC_STATUS_SUCCESS;
        }

        HQUIC hconnection = pevent->NEW_CONNECTION.Connection;
        Peer* ppeer       = ptransport->AddPeer(hconnection);

        g_pquic->SetCallbackHandler(hconnection, (void*)ConnectionCallback, ppeer);

        QUIC_STATUS status = g_pquic->ConnectionSetConfiguration(hconnection, ptransport->m_hconfigServer);

        if (QUIC_FAILED(status)) {
            //
            // Returning a failure makes MsQuic close the connection itself.
            // The peer is left for a SHUTDOWN_COMPLETE that may still arrive
            // (a few bytes, and only when the configuration is unusable).
            //

            EnterCriticalSection(&ptransport->m_cs);
            ptransport->m_peers.erase(ppeer->m_id);
            LeaveCriticalSection(&ptransport->m_cs);
            ppeer->m_hconnection = NULL;
            ppeer->m_fGone       = true;
        }

        return status;
    }

    //////////////////////////////////////////////////////////////////////////////
    //
    // FedTransport
    //
    //////////////////////////////////////////////////////////////////////////////

    HRESULT Listen(DWORD idApplication, REFGUID guidInstance, unsigned short port)
    {
        std::string strError;

        m_idApplication = idApplication;
        m_guidInstance    = guidInstance;
        m_port            = port;
        m_fServer         = true;

        m_hconfigServer = OpenConfiguration(true, strError);

        if (m_hconfigServer == NULL) {
            OutputDebugStringA(("FedTransport: " + strError + "\n").c_str());
            return E_FAIL;
        }

        if (QUIC_FAILED(g_pquic->ListenerOpen(g_hregistration, ListenerCallback, this, &m_hlistener))) {
            return E_FAIL;
        }

        QUIC_BUFFER alpn = { (uint32_t)(sizeof(c_szAlpn) - 1), (uint8_t*)c_szAlpn };
        QUIC_ADDR   addr = {};
        QuicAddrSetFamily(&addr, QUIC_ADDRESS_FAMILY_UNSPEC);
        QuicAddrSetPort(&addr, port);

        if (QUIC_FAILED(g_pquic->ListenerStart(m_hlistener, &alpn, 1, &addr))) {
            g_pquic->ListenerClose(m_hlistener);
            m_hlistener = NULL;
            return E_FAIL;
        }

        //
        // Discovery responder on the next port
        //

        m_socketDiscovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (m_socketDiscovery != INVALID_SOCKET) {
            sockaddr_in sin = {};
            sin.sin_family      = AF_INET;
            sin.sin_port        = htons(port + 1);
            sin.sin_addr.s_addr = htonl(INADDR_ANY);

            u_long fNonBlocking = 1;

            if (
                   bind(m_socketDiscovery, (sockaddr*)&sin, sizeof(sin)) != 0
                || ioctlsocket(m_socketDiscovery, FIONBIO, &fNonBlocking) != 0
            ) {
                closesocket(m_socketDiscovery);
                m_socketDiscovery = INVALID_SOCKET;
            }
        }

        return S_OK;
    }

    void SetSessionDetails(const char* szDetails)
    {
        EnterCriticalSection(&m_cs);
        m_strDetails = szDetails ? szDetails : "";
        LeaveCriticalSection(&m_cs);
    }

    void Poll()
    {
        if (m_socketDiscovery == INVALID_SOCKET) {
            return;
        }

        for (;;) {
            char        buffer[512];
            sockaddr_in sinFrom;
            int         cbFrom = sizeof(sinFrom);
            int         cb     = recvfrom(m_socketDiscovery, buffer, sizeof(buffer), 0, (sockaddr*)&sinFrom, &cbFrom);

            if (cb == SOCKET_ERROR) {
                break;
            }

            const ProbeFrame* pprobe = (const ProbeFrame*)buffer;

            if (
                   cb < (int)sizeof(ProbeFrame)
                || pprobe->m_dwMagic != c_dwMagic
                || pprobe->m_type != c_probeRequest
                || pprobe->m_idApplication != m_idApplication
            ) {
                continue;
            }

            EnterCriticalSection(&m_cs);
            std::string strDetails = m_strDetails;
            LeaveCriticalSection(&m_cs);

            std::vector<BYTE> reply(sizeof(ReplyFrame) + strDetails.size() + 1);
            ReplyFrame* preply = (ReplyFrame*)reply.data();
            preply->m_dwMagic         = c_dwMagic;
            preply->m_type            = c_probeReply;
            preply->m_idApplication = m_idApplication;
            preply->m_guidInstance    = m_guidInstance;
            preply->m_port            = m_port;
            memcpy(preply + 1, strDetails.c_str(), strDetails.size() + 1);

            sendto(m_socketDiscovery, (const char*)reply.data(), (int)reply.size(), 0, (sockaddr*)&sinFrom, cbFrom);
        }
    }

    HRESULT Connect(DWORD idApplication, const char* szHost, unsigned short port, const char* szName, DWORD* pidPeer)
    {
        std::string strError;

        m_idApplication = idApplication;

        if (m_hconfigClient == NULL) {
            m_hconfigClient = OpenConfiguration(false, strError);

            if (m_hconfigClient == NULL) {
                OutputDebugStringA(("FedTransport: " + strError + "\n").c_str());
                QueueSimpleEvent(FedTransportEvent::JoinRefused, 0, strError);
                return E_FAIL;
            }
        }

        HQUIC hconnection = NULL;
        Peer* ppeer       = new Peer();
        ppeer->m_ptransport = this;

        if (QUIC_FAILED(g_pquic->ConnectionOpen(g_hregistration, ConnectionCallback, ppeer, &hconnection))) {
            delete ppeer;
            return E_FAIL;
        }

        ppeer->m_hconnection = hconnection;

        EnterCriticalSection(&m_cs);
        ppeer->m_id = m_idNext++;
        m_peers[ppeer->m_id] = ppeer;
        LeaveCriticalSection(&m_cs);

        lstrcpynA(ppeer->m_szAddress, szHost, sizeof(ppeer->m_szAddress));

        //
        // The hello goes out on the stream once the handshake completes
        // (QUIC_CONNECTION_EVENT_CONNECTED)
        //

        std::string strName = szName ? szName : "";
        ppeer->m_vhello.resize(sizeof(HelloFrame) + strName.size() + 1);
        HelloFrame* phello = (HelloFrame*)ppeer->m_vhello.data();
        phello->m_dwMagic         = c_dwMagic;
        phello->m_dwVersion       = c_dwProtocolVersion;
        phello->m_idApplication = idApplication;
        memcpy(phello + 1, strName.c_str(), strName.size() + 1);

        if (QUIC_FAILED(g_pquic->ConnectionStart(hconnection, m_hconfigClient, QUIC_ADDRESS_FAMILY_UNSPEC, szHost, port))) {
            //
            // The connection shuts down and reports JoinRefused
            //

            Shutdown(ppeer, "the server address could not be used");
        }

        if (pidPeer) {
            *pidPeer = ppeer->m_id;
        }

        return S_OK;
    }

    HRESULT Send(DWORD idPeer, const void* pv, DWORD cb, bool fGuaranteed)
    {
        HRESULT hr = E_FAIL;

        EnterCriticalSection(&m_cs);

        std::map<DWORD, Peer*>::iterator it = m_peers.find(idPeer);

        if (it != m_peers.end() && it->second->m_fJoined && !it->second->m_fGone) {
            Peer* ppeer = it->second;

            if (!fGuaranteed && cb + 1 <= (DWORD)ppeer->m_cbDatagramMax) {
                SendBuffer* psb = SendBuffer::Create(ppeer, FramePacket, pv, cb, false, false);

                if (psb) {
                    if (QUIC_SUCCEEDED(g_pquic->DatagramSend(ppeer->m_hconnection, &psb->m_buffer, 1, QUIC_SEND_FLAG_NONE, psb))) {
                        hr = S_OK;
                    } else {
                        free(psb);
                    }
                }
            } else if (ppeer->m_hstream) {
                SendBuffer* psb = SendBuffer::Create(ppeer, FramePacket, pv, cb, true, fGuaranteed);

                if (psb) {
                    InterlockedIncrement(&ppeer->m_cSends);
                    InterlockedExchangeAdd(&ppeer->m_cbQueued, psb->m_cb);

                    if (QUIC_SUCCEEDED(g_pquic->StreamSend(ppeer->m_hstream, &psb->m_buffer, 1, QUIC_SEND_FLAG_NONE, psb))) {
                        hr = S_OK;
                    } else {
                        InterlockedDecrement(&ppeer->m_cSends);
                        InterlockedExchangeAdd(&ppeer->m_cbQueued, -(LONG)psb->m_cb);
                        free(psb);
                    }
                }
            }
        }

        LeaveCriticalSection(&m_cs);
        return hr;
    }

    void ClosePeer(DWORD idPeer)
    {
        EnterCriticalSection(&m_cs);

        std::map<DWORD, Peer*>::iterator it = m_peers.find(idPeer);

        if (it != m_peers.end()) {
            //
            // Its PeerGone was asked for, so don't report it again
            //

            it->second->m_fJoined = false;
            Shutdown(it->second, "closed");
        }

        LeaveCriticalSection(&m_cs);
    }

    bool GetPeerAddress(DWORD idPeer, char* szAddress, int cch)
    {
        bool fFound = false;

        EnterCriticalSection(&m_cs);

        std::map<DWORD, Peer*>::iterator it = m_peers.find(idPeer);

        if (it != m_peers.end()) {
            lstrcpynA(szAddress, it->second->m_szAddress, cch);
            fFound = true;
        }

        LeaveCriticalSection(&m_cs);
        return fFound;
    }

    bool GetPeerStats(DWORD idPeer, DWORD* pmsRtt, DWORD* pcSends, DWORD* pcbQueued)
    {
        bool fFound = false;

        EnterCriticalSection(&m_cs);

        std::map<DWORD, Peer*>::iterator it = m_peers.find(idPeer);

        if (it != m_peers.end()) {
            Peer* ppeer = it->second;

            if (pmsRtt) {
                QUIC_STATISTICS_V2 stats = {};
                uint32_t           cb    = sizeof(stats);

                *pmsRtt =
                    QUIC_SUCCEEDED(g_pquic->GetParam(ppeer->m_hconnection, QUIC_PARAM_CONN_STATISTICS_V2, &cb, &stats))
                        ? stats.Rtt / 1000
                        : 0;
            }

            if (pcSends)   *pcSends   = ppeer->m_cSends;
            if (pcbQueued) *pcbQueued = ppeer->m_cbQueued;
            fFound = true;
        }

        LeaveCriticalSection(&m_cs);
        return fFound;
    }

    void GetTotals(DWORD* pcSends, DWORD* pcbQueued, DWORD* pcEvents)
    {
        DWORD cSends = 0, cbQueued = 0;

        EnterCriticalSection(&m_cs);

        for (std::map<DWORD, Peer*>::iterator it = m_peers.begin(); it != m_peers.end(); ++it) {
            cSends   += it->second->m_cSends;
            cbQueued += it->second->m_cbQueued;
        }

        if (pcEvents) *pcEvents = (DWORD)m_events.size();

        LeaveCriticalSection(&m_cs);

        if (pcSends)   *pcSends   = cSends;
        if (pcbQueued) *pcbQueued = cbQueued;
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Discovery (client)
//
//////////////////////////////////////////////////////////////////////////////

class Discovery : public FedDiscovery
{
public:
    SOCKET                      m_socket;
    DWORD                       m_idApplication;
    sockaddr_in                 m_sinTarget;
    bool                        m_fTarget;
    DWORD                       m_timeProbe;
    std::vector<FedSessionInfo> m_sessions;

    Discovery() :
        m_socket(INVALID_SOCKET),
        m_idApplication(0),
        m_fTarget(false),
        m_timeProbe(0)
    {
        StartWinsock();
    }

    ~Discovery()
    {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
        }

        WSACleanup();
    }

    void Start(DWORD idApplication, const char* szHost, unsigned short portDiscovery) override
    {
        m_idApplication = idApplication;
        m_sessions.clear();
        m_fTarget = false;

        if (m_socket == INVALID_SOCKET) {
            m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (m_socket == INVALID_SOCKET) {
                return;
            }

            u_long fNonBlocking = 1;
            BOOL   fBroadcast   = TRUE;
            ioctlsocket(m_socket, FIONBIO, &fNonBlocking);
            setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (const char*)&fBroadcast, sizeof(fBroadcast));
        }

        memset(&m_sinTarget, 0, sizeof(m_sinTarget));
        m_sinTarget.sin_family = AF_INET;
        m_sinTarget.sin_port   = htons(portDiscovery);

        if (szHost == NULL || szHost[0] == '\0') {
            m_sinTarget.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            m_fTarget = true;
        } else {
            addrinfo  hints = {};
            addrinfo* presult = NULL;
            hints.ai_family   = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            if (getaddrinfo(szHost, NULL, &hints, &presult) == 0 && presult) {
                m_sinTarget.sin_addr = ((sockaddr_in*)presult->ai_addr)->sin_addr;
                m_fTarget = true;
                freeaddrinfo(presult);
            }
        }

        SendProbe();
    }

    void SendProbe()
    {
        if (m_socket == INVALID_SOCKET || !m_fTarget) {
            return;
        }

        ProbeFrame probe;
        probe.m_dwMagic         = c_dwMagic;
        probe.m_type            = c_probeRequest;
        probe.m_idApplication = m_idApplication;

        sendto(m_socket, (const char*)&probe, sizeof(probe), 0, (sockaddr*)&m_sinTarget, sizeof(m_sinTarget));
        m_timeProbe = GetTickCount();
    }

    void Poll(std::vector<FedSessionInfo>& sessions)
    {
        DWORD timeNow = GetTickCount();

        if (m_socket != INVALID_SOCKET) {
            for (;;) {
                char        buffer[2048];
                sockaddr_in sinFrom;
                int         cbFrom = sizeof(sinFrom);
                int         cb     = recvfrom(m_socket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&sinFrom, &cbFrom);

                if (cb == SOCKET_ERROR) {
                    break;
                }

                const ReplyFrame* preply = (const ReplyFrame*)buffer;

                if (
                       cb < (int)sizeof(ReplyFrame)
                    || preply->m_dwMagic != c_dwMagic
                    || preply->m_type != c_probeReply
                    || preply->m_idApplication != m_idApplication
                ) {
                    continue;
                }

                buffer[cb] = '\0';

                char szHost[INET_ADDRSTRLEN] = { 0 };
                inet_ntop(AF_INET, &sinFrom.sin_addr, szHost, sizeof(szHost));

                FedSessionInfo* pinfo = NULL;

                for (size_t index = 0; index < m_sessions.size(); index++) {
                    if (m_sessions[index].m_guidInstance == preply->m_guidInstance) {
                        pinfo = &m_sessions[index];
                    }
                }

                if (pinfo == NULL) {
                    m_sessions.push_back(FedSessionInfo());
                    pinfo = &m_sessions.back();
                    pinfo->m_guidInstance = preply->m_guidInstance;
                }

                pinfo->m_strDetails = (const char*)(preply + 1);
                pinfo->m_strHost    = szHost;
                pinfo->m_port       = preply->m_port;
                pinfo->m_timeSeen   = timeNow;
            }
        }

        //
        // Forget sessions that stopped answering; probe again every second
        //

        for (size_t index = m_sessions.size(); index-- > 0; ) {
            if (timeNow - m_sessions[index].m_timeSeen > 10000) {
                m_sessions.erase(m_sessions.begin() + index);
            }
        }

        if (timeNow - m_timeProbe > 1000) {
            SendProbe();
        }

        sessions = m_sessions;
    }
};

} // namespace

//////////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////////

FedTransport* CreateFedTransport(HANDLE hEventSignal, std::string& strError)
{
    QuicTransport* ptransport = new QuicTransport(hEventSignal);

    if (!ptransport->Init(strError)) {
        delete ptransport;
        return NULL;
    }

    return ptransport;
}

FedDiscovery* CreateFedDiscovery()
{
    return new Discovery();
}

void FedParseHostPort(const char* sz, unsigned short portDefault, std::string& strHost, unsigned short& port)
{
    strHost = sz ? sz : "";
    port    = portDefault;

    //
    // host:port, but not an IPv6 literal (more than one colon)
    //

    size_t colon = strHost.find(':');

    if (colon != std::string::npos && strHost.find(':', colon + 1) == std::string::npos) {
        int value = atoi(strHost.c_str() + colon + 1);

        if (value > 0 && value < 65536) {
            port = (unsigned short)value;
        }

        strHost.erase(colon);
    }
}
