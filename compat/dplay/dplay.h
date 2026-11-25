//////////////////////////////////////////////////////////////////////////////
//
// dplay.h -- compile-only stand-in for the DirectPlay 4 header.
//
// DirectPlay is no longer in the Windows SDK. Allegiance's networking layer
// (FedMessaging in _Utility/Messages.cpp, NetworkGame in clintlib) is being
// replaced by a Winsock UDP transport in Phase 3 (see MODERNIZATION.md). Until
// then, this header only lets the existing code compile.
//
// It is deliberately NOT a working DirectPlay binding:
//   - Only the types, constants and methods this codebase uses are declared.
//     The interface layouts are not the DirectPlay ABI.
//   - CLSID_DirectPlay and every IID are GUID_NULL, so CoCreateInstance fails
//     with REGDB_E_CLASSNOTREG and the callers take their existing
//     "DirectPlay unavailable" error paths. No call can reach dplayx.dll.
//
// Delete this directory once Phase 3 lands.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ALLEGIANCE_COMPAT_DPLAY_H
#define ALLEGIANCE_COMPAT_DPLAY_H

#include <windows.h>
#include <objbase.h>

//
// GUIDs: all GUID_NULL on purpose (see above).
//

inline constexpr GUID CLSID_DirectPlay      = {};
inline constexpr GUID IID_IDirectPlay4A     = {};
inline constexpr GUID DPSPGUID_TCPIP        = {};

//
// Basic types
//

typedef DWORD  DPID;
typedef DWORD* LPDPID;

#define DPID_SYSMSG          0
#define DPID_ALLPLAYERS      0
#define DPID_SERVERPLAYER    1
#define DPID_UNKNOWN         0xFFFFFFFF

//
// Results
//

#define _FACDP  0x877
#define MAKE_DPHRESULT(code)  MAKE_HRESULT(1, _FACDP, code)

#define DP_OK                       S_OK
#define DPERR_ALREADYINITIALIZED    MAKE_DPHRESULT(5)
#define DPERR_BUFFERTOOSMALL        MAKE_DPHRESULT(30)
#define DPERR_INVALIDFLAGS          MAKE_DPHRESULT(120)
#define DPERR_INVALIDPLAYER         MAKE_DPHRESULT(150)
#define DPERR_NOMESSAGES            MAKE_DPHRESULT(190)
#define DPERR_UNAVAILABLE           MAKE_DPHRESULT(210)
#define DPERR_CONNECTING            MAKE_DPHRESULT(1000)

//
// Flags
//

#define DPOPEN_JOIN                     0x00000001
#define DPOPEN_CREATE                   0x00000002

#define DPSEND_GUARANTEED               0x00000001
#define DPSEND_ASYNC                    0x00000200
#define DPSEND_NOSENDCOMPLETEMSG        0x00000400

#define DPRECEIVE_ALL                   0x00000001

#define DPGET_REMOTE                    0x00000000
#define DPGET_LOCAL                     0x00000001
#define DPSET_REMOTE                    0x00000000
#define DPSET_LOCAL                     0x00000001

#define DPGETCAPS_GUARANTEED            0x00000001

#define DPMESSAGEQUEUE_SEND             0x00000001
#define DPMESSAGEQUEUE_RECEIVE          0x00000002

#define DPENUMSESSIONS_AVAILABLE        0x00000001
#define DPENUMSESSIONS_ALL              0x00000002
#define DPENUMSESSIONS_ASYNC            0x00000010
#define DPENUMSESSIONS_STOPASYNC        0x00000020

#define DPESC_TIMEDOUT                  0x00000001

#define DPPLAYERTYPE_GROUP              0x00000000
#define DPPLAYERTYPE_PLAYER             0x00000001

#define DPPLAYER_SERVERPLAYER           0x00000100

#define DPSESSION_KEEPALIVE             0x00000040
#define DPSESSION_NODATAMESSAGES        0x00000080
#define DPSESSION_SECURESERVER          0x00000100
#define DPSESSION_CLIENTSERVER          0x00001000
#define DPSESSION_DIRECTPLAYPROTOCOL    0x00002000
#define DPSESSION_OPTIMIZELATENCY       0x00008000

//
// System messages
//

#define DPSYS_CREATEPLAYERORGROUP       0x0003
#define DPSYS_DESTROYPLAYERORGROUP      0x0005
#define DPSYS_ADDPLAYERTOGROUP          0x0007
#define DPSYS_DELETEPLAYERFROMGROUP     0x0021
#define DPSYS_SESSIONLOST               0x0031
#define DPSYS_HOST                      0x0101
#define DPSYS_SETPLAYERORGROUPDATA      0x0102
#define DPSYS_SETPLAYERORGROUPNAME      0x0103
#define DPSYS_SECUREMESSAGE             0x0107
#define DPSYS_SENDCOMPLETE              0x010d

//
// Structures
//

typedef struct DPNAME {
    DWORD   dwSize;
    DWORD   dwFlags;
    union { LPWSTR lpszShortName; LPSTR lpszShortNameA; };
    union { LPWSTR lpszLongName;  LPSTR lpszLongNameA;  };
} DPNAME, *LPDPNAME;
typedef const DPNAME* LPCDPNAME;

typedef struct DPSESSIONDESC2 {
    DWORD   dwSize;
    DWORD   dwFlags;
    GUID    guidInstance;
    GUID    guidApplication;
    DWORD   dwMaxPlayers;
    DWORD   dwCurrentPlayers;
    union { LPWSTR lpszSessionName; LPSTR lpszSessionNameA; };
    union { LPWSTR lpszPassword;    LPSTR lpszPasswordA;    };
    DWORD   dwReserved1;
    DWORD   dwReserved2;
    DWORD   dwUser1;
    DWORD   dwUser2;
    DWORD   dwUser3;
    DWORD   dwUser4;
} DPSESSIONDESC2, *LPDPSESSIONDESC2;
typedef const DPSESSIONDESC2* LPCDPSESSIONDESC2;

typedef struct DPCAPS {
    DWORD   dwSize;
    DWORD   dwFlags;
    DWORD   dwMaxBufferSize;
    DWORD   dwMaxQueueSize;
    DWORD   dwMaxPlayers;
    DWORD   dwHundredBaud;
    DWORD   dwLatency;
    DWORD   dwMaxLocalPlayers;
    DWORD   dwHeaderLength;
    DWORD   dwTimeout;
} DPCAPS, *LPDPCAPS;

typedef struct DPSECURITYDESC {
    DWORD   dwSize;
    DWORD   dwFlags;
    union { LPWSTR lpszSSPIProvider; LPSTR lpszSSPIProviderA; };
    union { LPWSTR lpszCAPIProvider; LPSTR lpszCAPIProviderA; };
    DWORD   dwCAPIProviderType;
    DWORD   dwEncryptionAlgorithm;
} DPSECURITYDESC, *LPDPSECURITYDESC;
typedef const DPSECURITYDESC* LPCDPSECURITYDESC;

typedef struct DPCREDENTIALS {
    DWORD   dwSize;
    DWORD   dwFlags;
    union { LPWSTR lpszUsername; LPSTR lpszUsernameA; };
    union { LPWSTR lpszPassword; LPSTR lpszPasswordA; };
    union { LPWSTR lpszDomain;   LPSTR lpszDomainA;   };
} DPCREDENTIALS, *LPDPCREDENTIALS;
typedef const DPCREDENTIALS* LPCDPCREDENTIALS;

typedef struct DPMSG_GENERIC {
    DWORD   dwType;
} DPMSG_GENERIC, *LPDPMSG_GENERIC;

typedef DPMSG_GENERIC DPMSG_HOST,        *LPDPMSG_HOST;
typedef DPMSG_GENERIC DPMSG_SESSIONLOST, *LPDPMSG_SESSIONLOST;

typedef struct DPMSG_CREATEPLAYERORGROUP {
    DWORD   dwType;
    DWORD   dwPlayerType;
    DPID    dpId;
    DWORD   dwCurrentPlayers;
    LPVOID  lpData;
    DWORD   dwDataSize;
    DPNAME  dpnName;
    DPID    dpIdParent;
    DWORD   dwFlags;
} DPMSG_CREATEPLAYERORGROUP, *LPDPMSG_CREATEPLAYERORGROUP;

typedef struct DPMSG_DESTROYPLAYERORGROUP {
    DWORD   dwType;
    DWORD   dwPlayerType;
    DPID    dpId;
    LPVOID  lpLocalData;
    DWORD   dwLocalDataSize;
    LPVOID  lpRemoteData;
    DWORD   dwRemoteDataSize;
    DPNAME  dpnName;
    DPID    dpIdParent;
    DWORD   dwFlags;
} DPMSG_DESTROYPLAYERORGROUP, *LPDPMSG_DESTROYPLAYERORGROUP;

typedef struct DPMSG_ADDPLAYERTOGROUP {
    DWORD   dwType;
    DPID    dpIdGroup;
    DPID    dpIdPlayer;
} DPMSG_ADDPLAYERTOGROUP, *LPDPMSG_ADDPLAYERTOGROUP;

typedef DPMSG_ADDPLAYERTOGROUP DPMSG_DELETEPLAYERFROMGROUP, *LPDPMSG_DELETEPLAYERFROMGROUP;

typedef struct DPMSG_SETPLAYERORGROUPDATA {
    DWORD   dwType;
    DWORD   dwPlayerType;
    DPID    dpId;
    LPVOID  lpData;
    DWORD   dwDataSize;
} DPMSG_SETPLAYERORGROUPDATA, *LPDPMSG_SETPLAYERORGROUPDATA;

typedef struct DPMSG_SETPLAYERORGROUPNAME {
    DWORD   dwType;
    DWORD   dwPlayerType;
    DPID    dpId;
    DPNAME  dpnName;
} DPMSG_SETPLAYERORGROUPNAME, *LPDPMSG_SETPLAYERORGROUPNAME;

typedef struct DPMSG_SECUREMESSAGE {
    DWORD   dwType;
    DWORD   dwFlags;
    DPID    dpIdFrom;
    LPVOID  lpData;
    DWORD   dwDataSize;
} DPMSG_SECUREMESSAGE, *LPDPMSG_SECUREMESSAGE;

typedef struct DPMSG_SENDCOMPLETE {
    DWORD   dwType;
    DPID    idFrom;
    DPID    idTo;
    DWORD   dwFlags;
    DWORD   dwPriority;
    DWORD   dwTimeout;
    LPVOID  lpvContext;
    DWORD   dwMsgID;
    HRESULT hr;
    DWORD   dwSendTime;
} DPMSG_SENDCOMPLETE, *LPDPMSG_SENDCOMPLETE;

//
// Callbacks
//

typedef BOOL (FAR PASCAL* LPDPENUMSESSIONSCALLBACK2)(
    LPCDPSESSIONDESC2 lpThisSD, LPDWORD lpdwTimeOut, DWORD dwFlags, LPVOID lpContext);

//
// Interface: only the methods this codebase calls.
//

struct IDirectPlay4A : public IUnknown {
    STDMETHOD(AddPlayerToGroup)(DPID idGroup, DPID idPlayer) PURE;
    STDMETHOD(Close)() PURE;
    STDMETHOD(CreateGroup)(LPDPID lpidGroup, LPDPNAME lpGroupName, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(CreatePlayer)(LPDPID lpidPlayer, LPDPNAME lpPlayerName, HANDLE hEvent, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(DeletePlayerFromGroup)(DPID idGroup, DPID idPlayer) PURE;
    STDMETHOD(DestroyGroup)(DPID idGroup) PURE;
    STDMETHOD(DestroyPlayer)(DPID idPlayer) PURE;
    STDMETHOD(EnumSessions)(LPDPSESSIONDESC2 lpsd, DWORD dwTimeout, LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2, LPVOID lpContext, DWORD dwFlags) PURE;
    STDMETHOD(GetCaps)(LPDPCAPS lpDPCaps, DWORD dwFlags) PURE;
    STDMETHOD(GetGroupData)(DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(GetPlayerAddress)(DPID idPlayer, LPVOID lpAddress, LPDWORD lpdwAddressSize) PURE;
    STDMETHOD(GetPlayerCaps)(DPID idPlayer, LPDPCAPS lpPlayerCaps, DWORD dwFlags) PURE;
    STDMETHOD(GetPlayerData)(DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(GetSessionDesc)(LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(Open)(LPDPSESSIONDESC2 lpsd, DWORD dwFlags) PURE;
    STDMETHOD(Receive)(LPDPID lpidFrom, LPDPID lpidTo, DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(SetGroupData)(DPID idGroup, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(SetPlayerData)(DPID idPlayer, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(SetSessionDesc)(LPDPSESSIONDESC2 lpSessDesc, DWORD dwFlags) PURE;
    STDMETHOD(InitializeConnection)(LPVOID lpConnection, DWORD dwFlags) PURE;
    STDMETHOD(SecureOpen)(LPCDPSESSIONDESC2 lpsd, DWORD dwFlags, LPCDPSECURITYDESC lpSecurity, LPCDPCREDENTIALS lpCredentials) PURE;
    STDMETHOD(SendEx)(DPID idFrom, DPID idTo, DWORD dwFlags, LPVOID lpData, DWORD dwDataSize, DWORD dwPriority, DWORD dwTimeout, LPVOID lpContext, LPDWORD lpdwMsgID) PURE;
    STDMETHOD(GetMessageQueue)(DPID idFrom, DPID idTo, DWORD dwFlags, LPDWORD lpdwNumMsgs, LPDWORD lpdwNumBytes) PURE;
    STDMETHOD(CancelMessage)(DWORD dwMsgID, DWORD dwFlags) PURE;
};

typedef IDirectPlay4A  IDirectPlay;
typedef IDirectPlay4A  IDirectPlay2;
typedef IDirectPlay4A  IDirectPlay2A;
typedef IDirectPlay4A  IDirectPlay3;
typedef IDirectPlay4A  IDirectPlay3A;
typedef IDirectPlay4A  IDirectPlay4;
typedef IDirectPlay4A* LPDIRECTPLAY;
typedef IDirectPlay4A* LPDIRECTPLAY2;
typedef IDirectPlay4A* LPDIRECTPLAY2A;
typedef IDirectPlay4A* LPDIRECTPLAY3;
typedef IDirectPlay4A* LPDIRECTPLAY3A;
typedef IDirectPlay4A* LPDIRECTPLAY4;
typedef IDirectPlay4A* LPDIRECTPLAY4A;

#endif
