#ifndef _fedtransport_h_
#define _fedtransport_h_

//////////////////////////////////////////////////////////////////////////////
//
// FedTransport: the network layer under FedMessaging (Messages.cpp), on
// MsQuic. See MODERNIZATION.md, "Networking: MsQuic replaces DirectPlay".
//
// One QUIC connection per client. Guaranteed packets travel in order on one
// bidirectional stream; unguaranteed ones as QUIC datagrams, or on the stream
// if they are larger than the connection's datagram limit.
//
// MsQuic calls back on its own threads. Those callbacks only queue events;
// FedMessaging pops them on the game thread (PopEvent).
//
// This header deliberately uses no Winsock or MsQuic types: it is included
// by Messages.cpp, which is built with the module's precompiled header.
//
//////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

struct FedTransportEvent
{
    enum Type
    {
        PeerJoined,     // server: a client's hello arrived (m_idPeer, m_strText = name)
        Joined,         // client: the server's welcome arrived (m_guid = session instance)
        JoinRefused,    // client: refused, or the connection failed first (m_strText = reason)
        Packet,         // a FedMessaging packet from m_idPeer (m_data)
        PeerGone,       // the connection to m_idPeer has closed (m_strText = reason)
        SendFailed      // a guaranteed send to m_idPeer was cancelled
    };

    Type              m_type;
    DWORD             m_idPeer;
    GUID              m_guid;
    std::string       m_strText;
    std::vector<BYTE> m_data;

    FedTransportEvent() :
        m_type(Packet),
        m_idPeer(0),
        m_guid()
    {
    }
};

class FedTransport
{
public:
    virtual ~FedTransport() {}

    //
    // Server: accept connections for guidApplication on the UDP port, and
    // answer discovery probes on port + 1 with the instance and details.
    //

    virtual HRESULT Listen(REFGUID guidApplication, REFGUID guidInstance, unsigned short port) = 0;
    virtual void    SetSessionDetails(const char* szDetails) = 0;
    virtual void    Poll() = 0;     // answers discovery probes; call regularly

    //
    // Client: connect to one server. The result arrives as Joined or
    // JoinRefused; the connection's peer ID is returned now.
    //

    virtual HRESULT Connect(REFGUID guidApplication, const char* szHost, unsigned short port, const char* szName, DWORD* pidPeer) = 0;

    //
    // Both
    //

    virtual HRESULT Send(DWORD idPeer, const void* pv, DWORD cb, bool fGuaranteed) = 0;
    virtual void    ClosePeer(DWORD idPeer) = 0;
    virtual bool    PopEvent(FedTransportEvent& event) = 0;
    virtual bool    WaitForEvent(DWORD ms) = 0;

    virtual bool    GetPeerAddress(DWORD idPeer, char* szAddress, int cch) = 0;
    virtual bool    GetPeerStats(DWORD idPeer, DWORD* pmsRtt, DWORD* pcSends, DWORD* pcbQueued) = 0;
    virtual void    GetTotals(DWORD* pcSends, DWORD* pcbQueued, DWORD* pcEvents) = 0;
};

//
// NULL if MsQuic can't be loaded or initialised; strError says why.
// hEventSignal, if not NULL, is set whenever an event is queued.
//

FedTransport* CreateFedTransport(HANDLE hEventSignal, std::string& strError);

//////////////////////////////////////////////////////////////////////////////
//
// Session discovery (client side of the probe FedTransport::Poll answers)
//
//////////////////////////////////////////////////////////////////////////////

struct FedSessionInfo
{
    GUID           m_guidInstance;
    std::string    m_strDetails;    // as SetSessionDetails set it
    std::string    m_strHost;       // where the reply came from
    unsigned short m_port;          // the session's QUIC port
    DWORD          m_timeSeen;
};

class FedDiscovery
{
public:
    virtual ~FedDiscovery() {}

    //
    // Probe szHost (NULL or empty: broadcast on the LAN) on the discovery
    // port for sessions of guidApplication. Replaces any earlier search.
    //

    virtual void Start(REFGUID guidApplication, const char* szHost, unsigned short portDiscovery) = 0;

    //
    // Collect replies, re-probe now and then, and return the sessions that
    // answered recently.
    //

    virtual void Poll(std::vector<FedSessionInfo>& sessions) = 0;
};

FedDiscovery* CreateFedDiscovery();

//
// "host" or "host:port"
//

void FedParseHostPort(const char* sz, unsigned short portDefault, std::string& strHost, unsigned short& port);

#endif
