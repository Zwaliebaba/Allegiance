//////////////////////////////////////////////////////////////////////////////
//
// dplobby.h -- compile-only stand-in for the DirectPlay 4 lobby header.
// See dplay.h in this directory: not a working DirectPlay binding, removed in
// Phase 3. DirectPlayLobbyCreate always fails with DPERR_UNAVAILABLE.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ALLEGIANCE_COMPAT_DPLOBBY_H
#define ALLEGIANCE_COMPAT_DPLOBBY_H

#include "dplay.h"

inline constexpr GUID IID_IDirectPlayLobby3A = {};
inline constexpr GUID DPAID_INet             = {};

typedef BOOL (FAR PASCAL* LPDPENUMADDRESSCALLBACK)(
    REFGUID guidDataType, DWORD dwDataSize, LPCVOID lpData, LPVOID lpContext);

struct IDirectPlayLobby3A : public IUnknown {
    STDMETHOD(CreateAddress)(REFGUID guidSP, REFGUID guidDataType, LPCVOID lpData, DWORD dwDataSize, LPVOID lpAddress, LPDWORD lpdwAddressSize) PURE;
    STDMETHOD(EnumAddress)(LPDPENUMADDRESSCALLBACK lpEnumAddressCallback, LPCVOID lpAddress, DWORD dwAddressSize, LPVOID lpContext) PURE;
};

typedef IDirectPlayLobby3A  IDirectPlayLobby;
typedef IDirectPlayLobby3A  IDirectPlayLobbyA;
typedef IDirectPlayLobby3A  IDirectPlayLobby2;
typedef IDirectPlayLobby3A  IDirectPlayLobby2A;
typedef IDirectPlayLobby3A  IDirectPlayLobby3;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBY;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBYA;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBY2;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBY2A;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBY3;
typedef IDirectPlayLobby3A* LPDIRECTPLAYLOBBY3A;

inline HRESULT WINAPI DirectPlayLobbyCreate(LPGUID, IDirectPlayLobby3A** lplpDPL, IUnknown*, LPVOID, DWORD)
{
    if (lplpDPL) *lplpDPL = NULL;
    return DPERR_UNAVAILABLE;
}

#endif
