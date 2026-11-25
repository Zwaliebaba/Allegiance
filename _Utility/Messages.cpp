/* Messages.cpp
   Interface between WinTrek and FedSrv
   Owner: 

   FedMessaging runs on FedTransport (MsQuic, fedtransport.cpp); see
   MODERNIZATION.md, "Networking". The message packing below is unchanged.
*/

#include "pch.h"
#include "MessageCore.h"
#include "fedtransport.h"

const MsgClsPrio FedMessaging::c_mcpDefault = 1000;

static DWORD           g_idFrom = 0;
static bool            g_fHandlingMsg = false;
static CFMConnection * g_pcnxnFrom = NULL;
static LPBYTE          g_pbReceiveBuff = NULL;
static DWORD           g_dwMsgPacketSize = 0;

// Ok, this is kinda hacky, but it's too late to make wide-effect changes. The problem is that while in the OnAppMessage
// callback, they may delete the connection from which the message came. Then there may be another FEDMEESSAGE packed in 
// the same packet. In order to process any FEDMESSAGE, we need the pfsPlayer, which is no longer valid if we've deleted
// the connection, so we need to set a flag when a connection is deleted, so we know not to process any more FEDMESSAGEs
// that might be part of the same packet.
static bool g_fConnectionDeleted = false;

//
// IDs: the transport numbers its connections from 1, groups have a range of
// their own, and the local end of a session has a fixed ID
//

const DWORD c_idGroupFirst = 0x80000000;
const DWORD c_idMe         = 0x7fffffff;

//
// UDP ports per application; discovery answers on port + 1
//

const unsigned short c_portGameServer   = 47700;
const unsigned short c_portLobbyClients = 47710;
const unsigned short c_portLobbyServers = 47720;

static unsigned short PortForApplication(REFGUID guid)
{
  if (guid == FEDFREELOBBYCLIENTS_GUID || guid == FEDLOBBYCLIENTS_GUID)
    return c_portLobbyClients;

  if (guid == FEDFREELOBBYSERVERS_GUID || guid == FEDLOBBYSERVERS_GUID)
    return c_portLobbyServers;

  return c_portGameServer;
}

struct FedMessagingImpl
{
  FedTransport *              m_ptransport;   // while hosting or joined
  FedDiscovery *              m_pdiscovery;   // EnumSessions
  GUID                        m_guidEnum;
  bool                        m_fEnumStarted;
  std::vector<FedSessionInfo> m_sessions;     // what the last EnumSessions found

  FedMessagingImpl() :
    m_ptransport(NULL),
    m_pdiscovery(NULL),
    m_guidEnum(GUID_NULL),
    m_fEnumStarted(false)
  {
  }
};

void DumpMemory()
{
  _CrtDumpMemoryLeaks();
}

void whodidit()
{
  if (g_fHandlingMsg)
  {
    CB cb = ((FEDMESSAGE*)(g_pbReceiveBuff))->cbmsg;
    FEDMSGID id = ((FEDMESSAGE*)(g_pbReceiveBuff))->fmid;
    debugf("Currently handling message from %s(%u).\n"
           "cbmsg=%d, fmid=%d, total packet size=%d\n", 
           g_pcnxnFrom ? g_pcnxnFrom->GetName() : "<unknown>", g_idFrom,
           cb, id, g_dwMsgPacketSize);
  }
  else
    debugf("Not currently handling a message.\n");
}

/*-------------------------------------------------------------------------
 * VerifyMessages
 *-------------------------------------------------------------------------
  Purpose:
    Make sure that our output buffer adheres to the expected format of a 
    string of valid FEDMESSAGEs.
  
  Side Effects:
    Important: The default recipient no longer remains the default if 
               SendMessages is called with a non-NULL precip
 */
void VerifyMessage(LPBYTE pb, CB cbTotal)
{
#ifndef NO_MSG_CRC
  // <sigh> we can't do this as long as we have the hack to support non-crc clients, since we munge the message length on ack from logon to lobby
#else  
  int cMsgs = 0;
  CB cbRunning = 0;
  while (cbRunning < cbTotal)
  {
    FEDMESSAGE * pfm = (FEDMESSAGE *) pb;
    assert(pfm->cbmsg > 0);
    assert(pfm->fmid > 0);
    cbRunning += pfm->cbmsg;
    pb += pfm->cbmsg;
    cMsgs++;
  }
  assert(cbRunning == cbTotal);
#endif
}


FedMessaging::FedMessaging(IFedMessagingSite * pfmSite) :
  m_cMsgsOdometer(0),
  m_cBytesOdometer(0),
  m_pbFMNext(m_rgbbuffOutPacket),
  m_pfmSite(pfmSite),
  m_fConnected(false),
  m_fSessionCallback(false),
  m_precipDefault(NULL),
  m_fmGuaranteedDefault(FM_GUARANTEED),
  m_pcnxnServer(NULL),
  m_pgrpEveryone(NULL),
  m_pcnxnMe(NULL),
  m_fSecondaryOut(false),
  m_pbFMNextT(m_rgbbuffSecondaryOutPacket),
  m_guidInstance(GUID_NULL),
  m_guidApplication(GUID_NULL),
  m_timeMsgLast(Time::Now()),
  m_priority(c_mcpDefault),
  m_dwcbPacket(0),
  m_idGroupNext(c_idGroupFirst),
  m_pimpl(new FedMessagingImpl)
{
  assert (pfmSite);
}


FedMessaging::~FedMessaging()
{
  Shutdown();
  delete m_pimpl->m_pdiscovery;
  delete m_pimpl;
}


/*-------------------------------------------------------------------------
 * SetDefaultRecipient
 *-------------------------------------------------------------------------
  Purpose:
      Set default recipient, to be used if the out buffer overflows, or 
      NULL recipient is specified in SendMessages

      Important: The default recipient no longer remains the default if 
                 SendMessages is called with a NULL precip
 */
void FedMessaging::SetDefaultRecipient(CFMRecipient * precip, FMGuaranteed fmg)
{
  assert(0 == CbUsedSpaceInOutbox()); // should call this BEFORE you queue any messages
  m_precipDefault = precip;
  m_fmGuaranteedDefault = fmg;
}


/*-------------------------------------------------------------------------
 * GetDefaultRecipient
 *-------------------------------------------------------------------------
  Purpose:
      Get default recipient, and optionally other details about default recipient. 
      See SetDefaultRecipient

  Parameters:
      pfGuaranteedDefault:  output parameter to get guaranteed flag, or NULL if don't care
 */
CFMRecipient * FedMessaging::GetDefaultRecipient(FMGuaranteed * pfmg)
{
  if (pfmg)
    *pfmg = m_fmGuaranteedDefault;
  return m_precipDefault;
}


/*-------------------------------------------------------------------------
 * PFedMsgCreate
 *-------------------------------------------------------------------------
 * Purpose:
 *     Generate a FedMsg of specified type, and construct all variable-length parms
 * 
 * Parameters:
 *     pfm - Where to create the message (non-NULL). If NULL, memory is allocated for message
 *           Ignored if fQueueMsg==true
 * Returns:
 *     pointer to newly created message
 * 
 * Side Effects:
 *     Queues up the message if fQueueMsg is set, otherwise allocates new buffer for msg
 */
void * FedMessaging::PFedMsgCreate(bool fQueueMsg, BYTE * pbFMBuff, FEDMSGID fmid, CB cbfm, ...)
{
  va_list vl;
  BYTE * pBlobSrc, * pBlobDst; 
  BYTE * pbFM;

  assert (IsConnected());

StartOver: // if we overrun our buffer, need to start over after flushing it
  va_start(vl, cbfm); // make sure we have the last required parm

  if (fQueueMsg && (cbfm > CbFreeSpaceInOutbox()))
    SendToDefault(FM_FLUSH);
    
  //figure out where to compose this message
  pbFM =  fQueueMsg ? m_pbFMNext :
         (pbFMBuff  ? pbFMBuff : m_rgbbuffAlloc);

  ((FEDMESSAGE*)pbFM)->fmid = fmid;

  pBlobDst = pbFM + cbfm; // where we start putting the blobs
  IB * pib = (IB*)(pbFM + sizeof(FEDMESSAGE)); // relies on ib/cb pairs starting immediately after fm
  CB * pcb;
  CB cbBlob;

  while (FM_END_VAR_PARMS != (pBlobSrc = va_arg(vl, BYTE *)))
  {
    pcb = pib + 1;
    *pib = pBlobDst - pbFM;
    *pcb = 0;
    BYTE * pbLastNonSpace; // we trim the trailing whitespace too as a free added bonus
    cbBlob = (CB)va_arg(vl, UINT_PTR);   // see FM_VAR_PARM
    if (CB_ZTS == cbBlob)
    {
      if (pBlobSrc) // we have something for this parm (as opposed to an intentional NULL)
      {
        if (*pBlobSrc) // pointer to non-NULL string
        {
          int cbLen = lstrlen((char *)(pBlobSrc));
          pbLastNonSpace = pBlobSrc + cbLen - 1;
          while (cbLen > 0 && ' ' == *pbLastNonSpace)
          {
            pbLastNonSpace--;
            cbLen--;
          }

          if (fQueueMsg && (pBlobDst + cbLen > BuffOut() + GetBuffOutSize()))
          {
            // Need to send what was cued up, then move the current message (portion so far) to the head of the cue
            SendToDefault(FM_FLUSH);
            goto StartOver;
          }

          CopyMemory(pBlobDst, pBlobSrc, cbLen);
          *(pBlobDst + cbLen++) = 0;
          pBlobDst += cbLen;
          *pcb = cbLen;
        }
      }
    }
    else // length supplied
    {
      if (fQueueMsg && (pBlobDst + cbBlob > BuffOut() + GetBuffOutSize()))
      {
        // Need to send what was cued up, then move the current message (portion so far) to the head of the cue
        SendToDefault(FM_FLUSH);
        goto StartOver;
      }
      // if non-NULL pointer, then copy, otherwise, just reserve the space
      if (pBlobSrc)
        CopyMemory(pBlobDst, pBlobSrc, cbBlob);
      pBlobDst += cbBlob;
      *pcb = cbBlob;
    }
    
    pib += 2; // advance to next ib/cb pair
  } // while more blobs left
  va_end(vl);
  
  ((FEDMESSAGE *)pbFM)->cbmsg = pBlobDst - pbFM;
  if (fQueueMsg || !pbFMBuff) // if we're copying to pre-alloced pfm, then we're done
  {
    if (fQueueMsg)
    {
      m_pbFMNext = pBlobDst;
      // check for overrun buffer--relies on unsigned CB, otherwise check for free space >= 0
      assert(CbFreeSpaceInOutbox() <= GetBuffOutSize()); 
    }
    else // need to allocate space
    {
      // allocate a new blob to return the message in
      assert (((FEDMESSAGE *)pbFM)->cbmsg <= sizeof(m_rgbbuffAlloc));
      BYTE * pbMsg = (BYTE *) GlobalAllocPtr(GMEM_MOVEABLE, ((FEDMESSAGE *)pbFM)->cbmsg);
      CopyMemory(pbMsg, pbFM, ((FEDMESSAGE *)pbFM)->cbmsg);
      pbFM = pbMsg;
    }
  }
  assert(((FEDMESSAGE*)pbFM)->cbmsg > 0 && ((FEDMESSAGE*)pbFM)->fmid > 0);
  return pbFM;
}


/*-------------------------------------------------------------------------
 * PfmGetNext
 *-------------------------------------------------------------------------
 * Purpose:
 *    Get message after some provided message
 * 
 * Parameters:
 *    a message
 */
FEDMESSAGE * FedMessaging::PfmGetNext(FEDMESSAGE * pfm)
{
  BYTE * pfmNext = (BYTE*)pfm + pfm->cbmsg;
  assert(pfmNext >= BuffIn() && (BYTE*) pfm < (BuffIn()) + sizeof(m_rgbbuffInPacket));
  if (pfmNext - BuffIn() >= (int) PacketSize()) // no more messages in packet
  {
    assert((BYTE*)pfmNext == BuffIn() + PacketSize()); // should be exactly equal
    return NULL;
  }
  // Assert(!IsBadReadPointer(pfmNext, pfmNext->fm.cbfm))
  return (FEDMESSAGE *)pfmNext;
}


/*-------------------------------------------------------------------------
 * QueueExistingMsg
 *-------------------------------------------------------------------------
 * Purpose:
 *    Take a message that already exists, and queue it up for the outgoing packet
 * 
 * Parameters:
 *    pfm: the pre-existing, properly setup message
 */
void FedMessaging::QueueExistingMsg(const FEDMESSAGE * pfm)
{
  assert(m_fConnected);
  assert(pfm->cbmsg > 0);
  assert(pfm->fmid > 0);
  if (CbFreeSpaceInOutbox() < pfm->cbmsg) // overrun buffer--send it if we have a default recipient
  {
    assert(m_precipDefault);
    SendToDefault(FM_FLUSH);
  }
  CopyMemory(m_pbFMNext, pfm, pfm->cbmsg);
  m_pbFMNext += pfm->cbmsg;
  assert(CbFreeSpaceInOutbox() <= GetBuffOutSize()); 
}


/*-------------------------------------------------------------------------
 * GenericSend
 *-------------------------------------------------------------------------
 * Purpose:
 *    All messages go through this. Not used by FedMessaging users directly. Use SendMessages or ForwardMessage.
 *    A group send goes to each member.
 */
HRESULT FedMessaging::GenericSend(CFMRecipient * precip, const void * pv, CB cb, FMGuaranteed fmg)
{
  bool    fGuaranteed = FM_GUARANTEED == fmg;
  HRESULT hr          = S_OK;

#ifndef NO_MSG_CRC
    // add the crc to the end to minimize possible conflict of assumptions
    // We have to use our own buffer, so as to not overrun any input buffers ** :-( ** 
    char crcbuff[sizeof(m_rgbbuffOutPacket)];
    CopyMemory(crcbuff, pv, cb);
    int crc = MemoryCRC(crcbuff, cb);
    *(int*)(crcbuff + cb) = crc;
    pv = crcbuff;
    cb += sizeof(crc);
#endif

  FedTransport * ptransport = m_pimpl->m_ptransport;

  if (!ptransport)
    return E_FAIL;

  static CTempTimer tt("in FedTransport sends", 0.05f);
  tt.Start();

  CFMGroup * pgrp = precip->AsGroup();

  if (pgrp)
  {
    for (ListConnections::Iterator iter(*pgrp->GetMembers()); !iter.End(); iter.Next())
      ptransport->Send(iter.Value()->GetID(), pv, cb, fGuaranteed);
  }
  else
    hr = ptransport->Send(precip->GetID(), pv, cb, fGuaranteed);

  tt.Stop("to=%u, guaranteed=%d, hr=%x, 1st message (fmid)=%u", precip->GetID(), fGuaranteed, hr, 
          ((FEDMESSAGE*)pv)->fmid);

  m_pfmSite->OnMessageSent(this, precip, pv, cb, fmg);

#ifdef DEBUG
  if (FAILED(hr))
    debugf("Send to %s(%u) failed: 0x%x.\n", precip->GetName(), precip->GetID(), hr);
#endif // DEBUG

  return(hr);
}


/*-------------------------------------------------------------------------
 * SendToDefault
 *-------------------------------------------------------------------------
  Purpose:
      Send messages to default recipient

  Returns:
      Same as SendMessages

 */
int FedMessaging::SendToDefault(FMFlush fmf)
{
  return SendMessages(NULL, m_fmGuaranteedDefault, fmf);
}


/*-------------------------------------------------------------------------
 * SendMessages
 *-------------------------------------------------------------------------
  Purpose:
    Send queued-up messages
  
  Parameters:
    pvContext:  This is passed back to us when dp tell us it couldn't 
        deliver a message that we were tracking. FedSrv uses this as a pFSCon,
        and the client doesn't use it (yet).
    fflush: Whether you want to flush the queued message buffer
  
  Returns:
    bytes sent
  
  Side Effects:
    Important: The default recipient no longer remains the default if 
               SendMessages is called with a non-NULL precip
 */
int FedMessaging::SendMessages(CFMRecipient * precip, FMGuaranteed fmg, FMFlush fmf)
{
  assert(m_fConnected);
  // if they don't supply a recip, then they better have previously supplied one
  assert (precip || m_precipDefault);
  if (precip)
  {
    m_precipDefault = NULL;
  }
  else
  {
    precip = m_precipDefault;
    fmg    = m_fmGuaranteedDefault;
  }
  HRESULT hr;
  hr = S_OK;
  int cbToSend = CbUsedSpaceInOutbox();
  assert(cbToSend <= GetBuffOutSize());

  if (cbToSend > 0)
  {
    VerifyMessage(BuffOut(), cbToSend);
    hr = GenericSend(precip, BuffOut(), cbToSend, fmg);

    m_cMsgsOdometer++;
    m_cBytesOdometer += cbToSend;

    if (FM_FLUSH == fmf)
      m_pbFMNext = BuffOut();
  }
  return cbToSend;
}


/*-------------------------------------------------------------------------
 * CheckOdometer
 *-------------------------------------------------------------------------
 * Purpose:
 *    Tells you:
 *        How long it's been, 
 *        How many messages have been sent, and
 *        How many bytes have been sent
 *    since you've last called CheckOdometer. So obviously the first time you 
 *    call it you can ignore the return values
 * 
 * Parameters:    The return values
 * 
 * Side Effects:  Resets the odometer
 *
 * Returns:       Current time
 */
Time FedMessaging::CheckOdometer(float& flDTime, int& cMsgsOdometer, int& cBytesOdometer)
{
  Time timeNow    = Time::Now ();
  flDTime         = timeNow - m_timeOdometerStart;
  cMsgsOdometer   = m_cMsgsOdometer;
  cBytesOdometer  = m_cBytesOdometer;

  m_timeOdometerStart = timeNow;
  m_cMsgsOdometer     = 0;
  m_cBytesOdometer    = 0;
  return timeNow;
}


/*-------------------------------------------------------------------------
 * ReceiveMessages
 *-------------------------------------------------------------------------
 * Purpose:
 *    Handles everything the transport queued since the last call, and
 *    dispatches it to the site, on this (the game or server) thread
 */
HRESULT FedMessaging::ReceiveMessages()
{
  assert(!g_fHandlingMsg);

  FedTransport * ptransport = m_pimpl->m_ptransport;

  if (!ptransport)
    return S_OK;

  ptransport->Poll(); // servers answer discovery probes here

  g_fHandlingMsg = true;

  FedTransportEvent event;

  while (IsConnected() && ptransport->PopEvent(event))
  {
    switch (event.m_type)
    {
      case FedTransportEvent::PeerJoined:
      {
        m_pfmSite->OnSysMessage(this); // just for tracking
        CFMConnection * pcnxn = CreateConnection(event.m_strText.c_str(), event.m_idPeer);
        char szRemoteAddress[16];
        GetIPAddress(*pcnxn, szRemoteAddress);
        debugf("New connection %s(%u) from %s, which gives us %d connections\n", 
               pcnxn->GetName(), pcnxn->GetID(), szRemoteAddress, m_listCnxns.GetCount());
      }
      break;

      case FedTransportEvent::Packet:
      {
        CFMConnection * pcnxnFrom = GetConnectionFromId(event.m_idPeer);
        DWORD           cbPacket  = (DWORD)event.m_data.size();

        if (!pcnxnFrom)
        {
          debugf("Warning: Ignoring a packet from connection %u, which has no CFMConnection\n", event.m_idPeer);
          break;
        }

        if (cbPacket > sizeof(m_rgbbuffInPacket) || cbPacket < sizeof(FEDMESSAGE)
#ifndef NO_MSG_CRC
            + sizeof(int)
#endif
           )
        {
          debugf("Warning: Ignoring a %u-byte packet from %s(%u)\n", cbPacket, pcnxnFrom->GetName(), pcnxnFrom->GetID());
          break;
        }

        CopyMemory(m_rgbbuffInPacket, event.m_data.data(), cbPacket);
        m_dwcbPacket      = cbPacket;
        g_pbReceiveBuff   = m_rgbbuffInPacket;
        g_idFrom          = event.m_idPeer;
        g_dwMsgPacketSize = cbPacket;
        g_pcnxnFrom       = pcnxnFrom;

        DispatchPacket(pcnxnFrom);

        m_timeMsgLast = Time::Now();
      }
      break;

      case FedTransportEvent::PeerGone:
      {
        m_pfmSite->OnSysMessage(this);
        CFMConnection * pcnxn = GetConnectionFromId(event.m_idPeer);
        debugf("Connection %s(%u) closed: %s\n", pcnxn ? pcnxn->GetName() : "<gone>", event.m_idPeer, event.m_strText.c_str());

        // if the server goes, the session is over
        if (pcnxn && pcnxn == m_pcnxnServer)
        {
          m_pfmSite->OnSessionLost(this);
          Shutdown();
        }
        else if (pcnxn)
          DeleteConnection(*pcnxn);
      }
      break;

      case FedTransportEvent::SendFailed:
      {
        m_pfmSite->OnSysMessage(this);
        CFMConnection * pcnxn = GetConnectionFromId(event.m_idPeer);
        debugf("A guaranteed message was not delivered to %s(%u).\n", pcnxn ? pcnxn->GetName() : "<gone>", event.m_idPeer);

        if (pcnxn)
          m_pfmSite->OnMessageNAK(this, Time::Now().clock(), pcnxn);
      }
      break;

      default: // Joined and JoinRefused only matter while joining (JoinServer)
      break;
    }
  }

  g_idFrom = 0;
  g_pcnxnFrom = NULL;
  g_fHandlingMsg = false;
  return S_OK;
}


/*-------------------------------------------------------------------------
 * DispatchPacket
 *-------------------------------------------------------------------------
 * Purpose:
 *    Check the packet in m_rgbbuffInPacket and hand each FEDMESSAGE in it to the site
 */
void FedMessaging::DispatchPacket(CFMConnection * pcnxnFrom)
{
  FEDMESSAGE * pfm = (FEDMESSAGE *) m_rgbbuffInPacket;
  int cMsgs = 0;

#ifndef NO_MSG_CRC
  int crc = MemoryCRC(m_rgbbuffInPacket, PacketSize());
  if (crc != *(int*)(m_rgbbuffInPacket + PacketSize()))
  {
    // ACK! Failed CRC. WTF??? ...<sigh> but it happens
    m_pfmSite->OnBadCRC(this, *pcnxnFrom, m_rgbbuffInPacket, m_dwcbPacket);
    return;
  }
#endif

  static CTempTimer tt("spent looping through message queue", .1f);
  tt.Start();
  g_fConnectionDeleted = false;
  while (pfm && !g_fConnectionDeleted)
  {
    if(pfm->fmid > 0 && pfm->cbmsg >= sizeof(FEDMESSAGE) && pfm->cbmsg <= PacketSize() &&
       (BYTE*)pfm + pfm->cbmsg <= m_rgbbuffInPacket + PacketSize())
    {
      m_pfmSite->OnAppMessage(this, *pcnxnFrom, pfm);
      pfm = PfmGetNext(pfm);
      cMsgs++;
    }
    else
    {
      // ACK! Hacker?
      whodidit();
#ifndef NO_MSG_CRC
      // we'll just call it a bad crc, even though it's not quite the same thing
      m_pfmSite->OnBadCRC(this, *pcnxnFrom, m_rgbbuffInPacket, m_dwcbPacket);
#endif
      break;
    }
  }
  tt.Stop();
}


/*-------------------------------------------------------------------------
 * HostSession
 *-------------------------------------------------------------------------
 * Purpose:
 *    Start a session and be the host (server): listen on the application's
 *    port and answer discovery probes on the next one
 */
HRESULT FedMessaging::HostSession(GUID guidApplication, bool fKeepAlive, HANDLE hEventServer, bool fProtocol)
{
  assert(!m_fConnected);
  Shutdown();

  std::string    strError;
  FedTransport * ptransport = CreateFedTransport(hEventServer, strError);

  if (!ptransport)
  {
    m_pfmSite->OnMessageBox(this, ("Networking is unavailable: " + strError).c_str(), "Allegiance", MB_OK);
    return E_FAIL;
  }

  GUID guidInstance;
  CoCreateGuid(&guidInstance);

  unsigned short port = PortForApplication(guidApplication);
  HRESULT        hr   = ptransport->Listen(guidApplication, guidInstance, port);

  if (FAILED(hr))
  {
    delete ptransport;

    char szMsg[256];
    wsprintf(szMsg, "The server could not listen on UDP port %u. Another server may be using it, "
                    "or QUIC is unavailable (it needs Windows 11 or Windows Server 2022 or later).", port);
    m_pfmSite->OnMessageBox(this, szMsg, "Allegiance", MB_OK);
    return hr;
  }

  m_pimpl->m_ptransport = ptransport;
  m_guidApplication     = guidApplication;
  m_guidInstance        = guidInstance;
  m_fConnected          = true;

  m_pcnxnMe = new CFMConnection(this, "<Server connection>", c_idMe);
  ZVerify(m_pgrpEveryone = CreateGroup("Everyone"));
  ResetOutBuffer();

  debugf("Hosting on UDP port %u (discovery %u)\n", port, port + 1);
  return S_OK;
}


/*-------------------------------------------------------------------------
 * Shutdown
 *-------------------------------------------------------------------------
 * Purpose:
 *    Shutdown an open session. It is assumed that logoff, etc. has already been done.
 *    Closing the transport delivers what is still queued before the connections close.
 */
void FedMessaging::Shutdown()
{
  static CTempTimer tt("In FedMessaging::Shutdown", 0.1f);
  tt.Start();

  if (m_pimpl->m_ptransport)
  {
    delete m_pimpl->m_ptransport;
    m_pimpl->m_ptransport = NULL;
  }

  if (m_pcnxnMe)
  {
    delete m_pcnxnMe;
    m_pcnxnMe = NULL;
  }

  if (m_pcnxnServer)
  {
    delete m_pcnxnServer;
    m_pcnxnServer = NULL;
  }

  ResetOutBuffer();
  m_guidInstance = GUID_NULL;
  m_fConnected  = false;
  tt.Stop();
}


/*-------------------------------------------------------------------------
 * JoinSession
 *-------------------------------------------------------------------------
 * Purpose:
 *    Become a client in a session
 * 
 * Parameters:
 *    what application, what server (host or host:port; empty to take the
 *    first one that answers on the LAN), and the player's name
 */
HRESULT FedMessaging::JoinSession(GUID guidApplication, const char * szServer, const char * szCharName)
{
  std::string    strHost;
  unsigned short port;

  FedParseHostPort(szServer, PortForApplication(guidApplication), strHost, port);

  if (strHost.empty())
  {
    //
    // Find one on the LAN, as DirectPlay's broadcast enumeration did
    //

    FedDiscovery * pdiscovery = CreateFedDiscovery();
    std::vector<FedSessionInfo> sessions;
    DWORD timeStart = GetTickCount();

    pdiscovery->Start(guidApplication, "", port + 1);

    do
    {
      Sleep(250);
      pdiscovery->Poll(sessions);
    } while (sessions.empty() && GetTickCount() - timeStart < 5000);

    delete pdiscovery;

    if (sessions.empty())
      return E_FAIL;

    strHost = sessions[0].m_strHost;
    port    = sessions[0].m_port;
  }

  return JoinServer(guidApplication, strHost.c_str(), port, szCharName);
}


/*-------------------------------------------------------------------------
 * FedMessaging.JoinSessionInstance
 *-------------------------------------------------------------------------
  Purpose:
      Join a session that EnumSessions has found
 */
HRESULT FedMessaging::JoinSessionInstance(GUID guidInstance, const char * szName)
{
  for (size_t index = 0; index < m_pimpl->m_sessions.size(); index++)
  {
    const FedSessionInfo & info = m_pimpl->m_sessions[index];

    if (info.m_guidInstance == guidInstance)
      return JoinServer(m_pimpl->m_guidEnum, info.m_strHost.c_str(), info.m_port, szName);
  }

  debugf("JoinSessionInstance: that session has not answered recently\n");
  return E_FAIL;
}


/*-------------------------------------------------------------------------
 * JoinServer
 *-------------------------------------------------------------------------
  Purpose:
      Connect, and wait for the server's welcome (or refusal), as the
      DirectPlay join did before it returned
 */
HRESULT FedMessaging::JoinServer(GUID guidApplication, const char * szHost, unsigned short port, const char * szName)
{
  static CTempTimer tt("in FedMessaging::JoinServer", 0.5f);
  tt.Start();

  Shutdown();

  std::string    strError;
  FedTransport * ptransport = CreateFedTransport(NULL, strError);

  if (!ptransport)
  {
    m_pfmSite->OnMessageBox(this, ("Networking is unavailable: " + strError).c_str(), "Allegiance", MB_OK);
    tt.Stop();
    return E_FAIL;
  }

  DWORD       idServer  = 0;
  HRESULT     hr        = ptransport->Connect(guidApplication, szHost, port, szName, &idServer);
  std::string strReason = "the server did not answer";

  if (SUCCEEDED(hr))
  {
    hr = E_FAIL;

    DWORD             timeStart = GetTickCount();
    FedTransportEvent event;

    for (;;)
    {
      if (ptransport->PopEvent(event))
      {
        if (FedTransportEvent::Joined == event.m_type)
        {
          m_guidInstance = event.m_guid;
          hr = S_OK;
          break;
        }

        if (FedTransportEvent::JoinRefused == event.m_type)
        {
          strReason = event.m_strText;
          break;
        }

        continue; // nothing else comes before the welcome
      }

      if (GetTickCount() - timeStart > 15000)
        break;

      ptransport->WaitForEvent(100);
    }
  }
  else
  {
    FedTransportEvent event;

    if (ptransport->PopEvent(event) && FedTransportEvent::JoinRefused == event.m_type)
      strReason = event.m_strText;
  }

  if (FAILED(hr))
  {
    debugf("Joining %s:%u failed: %s\n", szHost, port, strReason.c_str());
    delete ptransport;

    char szMsg[512];
    _snprintf(szMsg, sizeof(szMsg) - 1, "Could not join the session on %s: %s.", szHost, strReason.c_str());
    szMsg[sizeof(szMsg) - 1] = '\0';
    m_pfmSite->OnMessageBox(this, szMsg, "Allegiance", MB_OK);

    tt.Stop();
    return hr;
  }

  m_pimpl->m_ptransport = ptransport;
  m_guidApplication     = guidApplication;
  m_fConnected          = true;

  m_pcnxnMe     = new CFMConnection(this, szName, c_idMe);
  m_pcnxnServer = new CFMConnection(this, "Server", idServer);
  ResetOutBuffer();

  tt.Stop();
  return S_OK;
}


CFMConnection * FedMessaging::GetConnectionFromId(DWORD id)
{
  for (ListConnections::Iterator iter(m_listCnxns); !iter.End(); iter.Next()) 
  {
    if (iter.Value()->GetID() == id)
      return iter.Value();
  }

  if (m_pcnxnServer && m_pcnxnServer->GetID() == id)
    return m_pcnxnServer;

  if (m_pcnxnMe && m_pcnxnMe->GetID() == id)
    return m_pcnxnMe;

  return NULL;
}


HRESULT FedMessaging::GetIPAddress(CFMConnection & cnxn, char szRemoteAddress[16])
{
  char szAddress[64];

  szRemoteAddress[0] = '\0';

  if (!m_pimpl->m_ptransport || !m_pimpl->m_ptransport->GetPeerAddress(cnxn.GetID(), szAddress, sizeof(szAddress)))
    return E_FAIL;

  lstrcpyn(szRemoteAddress, szAddress, 16);
  return S_OK;
}


HRESULT FedMessaging::GetSendQueue(DWORD * pcMsgs, DWORD * pcBytes)
{
  DWORD cMsgs = 0, cBytes = 0;

  if (m_pimpl->m_ptransport)
    m_pimpl->m_ptransport->GetTotals(&cMsgs, &cBytes, NULL);

  if (pcMsgs)  *pcMsgs  = cMsgs;
  if (pcBytes) *pcBytes = cBytes;
  return S_OK;
}


HRESULT FedMessaging::GetReceiveQueue(DWORD * pcMsgs, DWORD * pcBytes)
{
  DWORD cEvents = 0;

  if (m_pimpl->m_ptransport)
    m_pimpl->m_ptransport->GetTotals(NULL, NULL, &cEvents);

  if (pcMsgs)  *pcMsgs  = cEvents;
  if (pcBytes) *pcBytes = 0;
  return S_OK;
}


HRESULT FedMessaging::GetConnectionSendQueue(CFMConnection * pcnxn, DWORD * pcMsgs, DWORD * pcBytes)
{
  if (!pcnxn || !m_pimpl->m_ptransport || !m_pimpl->m_ptransport->GetPeerStats(pcnxn->GetID(), NULL, pcMsgs, pcBytes))
    return E_FAIL;

  return S_OK;
}


HRESULT FedMessaging::GetLinkDetails(CFMConnection * pcnxn, OUT DWORD * pdwHundredbpsG, OUT DWORD * pdwmsLatencyG, 
                                                              OUT DWORD * pdwHundredbpsU, OUT DWORD * pdwmsLatencyU)
{
  DWORD msRtt = 0;

  if (!pcnxn || !m_pimpl->m_ptransport || !m_pimpl->m_ptransport->GetPeerStats(pcnxn->GetID(), &msRtt, NULL, NULL))
    return E_FAIL;

  if (pdwHundredbpsG) *pdwHundredbpsG = 0;
  if (pdwmsLatencyG)  *pdwmsLatencyG  = msRtt / 2;
  if (pdwHundredbpsU) *pdwHundredbpsU = 0;
  if (pdwmsLatencyU)  *pdwmsLatencyU  = msRtt / 2;
  return S_OK;
}


/*-------------------------------------------------------------------------
 * EnumSessions
 *-------------------------------------------------------------------------
  Purpose:
      Look for sessions, and report (OnSessionFound) those that answered so
      far. Call it repeatedly, as callers did with DirectPlay's.
  Parameters:
      szServer: a host (or host:port) to ask, "" to broadcast on the LAN,
                NULL to keep polling the current search
 */
HRESULT FedMessaging::EnumSessions(GUID guidApplication, const char * szServer)
{
  m_fSessionCallback = true;

  if (!m_pimpl->m_pdiscovery)
    m_pimpl->m_pdiscovery = CreateFedDiscovery();

  if (szServer || !m_pimpl->m_fEnumStarted || guidApplication != m_pimpl->m_guidEnum)
  {
    std::string    strHost;
    unsigned short port;

    FedParseHostPort(szServer, PortForApplication(guidApplication), strHost, port);

    m_pimpl->m_pdiscovery->Start(guidApplication, strHost.c_str(), port + 1);
    m_pimpl->m_guidEnum     = guidApplication;
    m_pimpl->m_fEnumStarted = true;
  }

  m_pimpl->m_pdiscovery->Poll(m_pimpl->m_sessions);

  for (size_t index = 0; index < m_pimpl->m_sessions.size(); index++)
  {
    FMSessionDesc desc(m_pimpl->m_sessions[index].m_guidInstance, m_pimpl->m_sessions[index].m_strDetails.c_str());

    if (m_fSessionCallback)
      m_pfmSite->OnSessionFound(this, &desc);
  }

  return S_OK;
}


void FedMessaging::SetSessionDetails(char * szDetails)
{
  if (m_pimpl->m_ptransport)
    m_pimpl->m_ptransport->SetSessionDetails(szDetails);
}


CFMConnection * FedMessaging::CreateConnection(const char * szName, DWORD id) // after the physical connection has already been established
{
  static CTempTimer tt("in CreateConnection", .01f);
  tt.Start();
  CFMConnection * pcnxn = new CFMConnection(this, szName, id);
  m_listCnxns.PushFront(pcnxn);
  m_pgrpEveryone->AddConnection(this, pcnxn); // they stay in Everyone until the connection is deleted
  m_pfmSite->OnNewConnection(this, *pcnxn);
  tt.Stop();
  return pcnxn;
}


void FedMessaging::DeleteConnection(CFMConnection & cnxn)
{
  static CTempTimer tt("in DeleteConnection", .01f);
  tt.Start();
  m_pfmSite->OnDestroyConnection(this, cnxn);
  tt.Stop();

  m_listCnxns.Remove(&cnxn);

  //
  // A deleted connection leaves every group, as DirectPlay players did
  //

  for (ListGroups::Iterator iter(m_listGroups); !iter.End(); iter.Next())
    iter.Value()->m_listMembers.Remove(&cnxn);

  if (m_pimpl->m_ptransport)
    m_pimpl->m_ptransport->ClosePeer(cnxn.GetID());

  cnxn.Delete(this);
}


CFMConnection::CFMConnection(FedMessaging * pfm, const char * szName, DWORD id) : // only FedMessaging::CreateConnection can create these things
    CFMRecipient(szName, id),
    m_cAbsentCount(0),
    m_dwTimeLastComplete(0),
    m_dwPrivate(0)
{
  debugf("New CFMConnection: id=%u, name=%s, this=%p\n", id, szName, this);
}


void CFMConnection::Delete(FedMessaging * pfm) // basically the destructor, but with a parameter
{
  debugf("Deleting connection: id=%u, name=%s, this=%p, private=%p\n", 
          GetID(), GetName(), this, (void*)m_dwPrivate);
  delete this;
  g_fConnectionDeleted = true;
}


void CFMGroup::AddConnection(FedMessaging * pfm, CFMConnection * pcnxn)
{
  if (!m_listMembers.Find(pcnxn))
    m_listMembers.PushEnd(pcnxn);
}


void CFMGroup::DeleteConnection(FedMessaging * pfm, CFMConnection * pcnxn)
{
  m_listMembers.Remove(pcnxn);
}


CFMGroup::CFMGroup(FedMessaging * pfm, const char * szName) :
    CFMRecipient(szName, pfm->m_idGroupNext++)
{
}


void CFMGroup::Delete (FedMessaging * pfm) 
{
  delete this;
}


/*-------------------------------------------------------------------------
 * FMSessionDesc.FMSessionDesc
 *-------------------------------------------------------------------------
  Purpose:
      Crack the name/value pairs in the session details and populate the members
  Side Effects:
      This is kinda of bad coupling, since the server packs this, and the messaging layer unpacks. 
 */
FMSessionDesc::FMSessionDesc(REFGUID guidInstance, const char * szDetails)
{
  char * szName = NULL;
  char * szValue = NULL;
  m_nNumPlayers = 0;
  m_nMaxPlayers = 1;
  m_guidInstance = guidInstance;

  std::vector<char> details(szDetails, szDetails + lstrlen(szDetails) + 1);

  szName = strtok(details.data(), "\t");
  while(szName)
  {
    if (!lstrcmp(szName, "GAM"))
    {
      szValue = strtok(NULL, "\n");
      m_strGameName = szValue;
    }
    else if (!lstrcmp(szName, "PLR"))
    {
      szValue = strtok(NULL, "\n");
      m_nNumPlayers = szValue ? (short)atoi(szValue) : 0;
    }
    else if (!lstrcmp(szName, "LIM"))
    {
      szValue = strtok(NULL, "\n");
      m_nMaxPlayers = szValue ? (short)atoi(szValue) : 1;
    }
    else // throw away any remainder
    {
      szValue = strtok(NULL, "\n");
    }
    szName = strtok(NULL, "\t");
  }
}
