# Allegiance modernisation

Goal: port the 2004 Microsoft Research source drop of Allegiance to a Visual Studio 2026 / MSBuild
solution that compiles, runs on current Windows, and builds its art from source.

## Decisions

| Topic | Decision |
|---|---|
| Baseline | Strictly clean port of this repository. No code or fixes taken from community forks. |
| Toolchain | Visual Studio 2026, platform toolset v145, `.slnx` solution, Win32 first, x64 later. |
| Renderer | New Direct3D 11 backend behind the engine's existing `Engine` / `Surface` / rasterizer seam. DirectDraw 4, Direct3D 3 and the MMX software rasterizer are removed. |
| Art | Stage A: rebuild the original runtime format (binary MDL, 16-bit RGB565 images) through an MSBuild art project. Stage B: new MDL version with 32-bit / BC7 DDS textures generated from the 24-bit source BMPs. |
| Level of detail | Reduced LOD meshes are dropped. They were a 1999 performance optimisation (the models are hundreds to a few thousand triangles). Every LOD slot uses the full-detail mesh, which removes `xmunge -reduce`, the MSR progressive-mesh binaries and meshoptimizer. |
| Networking | MsQuic (Schannel build) behind the existing `FedMessaging` interface, replacing DirectPlay 4. This replaced the first plan of an own Winsock UDP layer; with QUIC providing reliability and loss recovery, no loopback / packet-loss harness was built, and LAN play is verified by hand. Windows 11 or Server 2022 and later. |
| Settings | JSON files instead of the registry: `settings.json` per user for the client, `server.json` for all users for the server, or either next to the executable. A small built-in reader and writer (`zlib/zsettings.*`), no third-party JSON library. Registry values are not imported. The only registry use left is Windows' own: COM and service registration, and performance-counter IDs. |
| Scope | Tier 1 is built. Tier 2 stays in the repo, outside the solution. Tier 3 is deleted once nothing in Tier 1 or 2 references it. |
| Verification | GitHub Actions on `windows-2025` (VS 2026). `.github/workflows/windows-build.yml`. |

## Module tiers

**Tier 1 (built):** `zlib` (Microsoft's base library, not the compression library), `guids`, `_Utility`,
`engine`, `effect`, `soundengine`, `Igc`, `clintlib`, `training`, `WinTrek` (client); `mdlc`, `xmunge`,
`cvh`, `qhull` (art tools); `FedSrv`, built as the original standalone LAN server flavour `AllSrv32`
(`ALLSRV_STANDALONE`); `AGC` and the helper libraries it needs from `test/` (`TCLib`); `sharemem`.

`xmunge` is not built yet. It is written entirely against Direct3D Retained Mode (frames, mesh
builders, progressive meshes), which Windows no longer has. Making it compile would mean writing a
D3DRM stand-in only to discard it, so it is rewritten in Phase 4 on the engine's own `.x` parser plus
meshoptimizer.

**Tier 2 (parked):** `lobby`, `AllSrvUI`, `srvconfig`, `cliconfig`.

**Tier 3 (delete):**

| Module | Status |
|---|---|
| `MSRGuard`, `SymGuard`, `patch`, `fsmon`, `dreamcast` | Deleted. |
| `Zone`, `Club` | Deleted in Phase 2 with the Zone and Club code paths (see Phase 2). |
| `sentinal`, `fedperf` | Deleted. Only the VS.NET 2003 `Server.vcproj` compiled them into the server; the original nmake build never did. |
| `AutoUpdate` | Kept for now. Referenced by `lobby/pch.h` (Tier 2). |
| `test` | Kept: `AGC` and `FedSrv` build on `test/TCLib`, `test/TCAtl` and `test/Inc`, so these are effectively Tier 1 support code rather than tests. |

### FedSrv and AGC

13 of the 23 `.cpp` files in `FedSrv` use AGC (the COM automation / admin event layer). Decision: AGC
moves into Tier 1 and is ported with the server, keeping the admin and event features intact.

## Findings that correct the first analysis

- The message-compiler sources exist. They use a `.msg` extension: `FedSrv/fedmsg.msg`,
  `sentinal/sentmsg.msg`, `lobby/lobbymsg.msg`, `Club/AllClubMsg.msg`. They need an `mc.exe` build
  step, not a reconstruction.
- `ddraw.h` and `d3d.h` (Direct3D 7-era immediate mode) are still in the Windows SDK; the first CI
  run resolved them. Compatibility shims are only needed for headers that really are gone:
  `d3drm.h`, `dplay.h` and `dplobby.h`.
- `test/Inc/ScrRun.idl` and `test/TCAtl/VersionInfoImpl.idl` are in the repo. The 2004
  `VSNet.Readme` listed them as missing, and the first analysis repeated that without checking.
- The original server build compiled neither `sentinal` nor `fedperf`; only the VS.NET 2003
  `.vcproj` did. Its `ALLSRV_STANDALONE` flavour (`AllSrv32`) is the LAN server.
- AGC's event definitions are generated from `AGC/AGCEvents.xml` by stylesheets in Microsoft's
  pre-standard `WD-xsl` dialect with embedded JScript. Only MSXML 3 runs those, so the build keeps
  the original `cscript XMLXForm.js` step (`AllegianceXslt` in `Directory.Build.targets`).
- `cvh` does not link qhull; it runs a `qhull` executable, so qhull is built as a console tool.
- `zlib/zlib.h` includes `ddraw.h` and `d3d.h` even though nothing in `zlib` uses DirectX. Every
  module inherits DirectX through it. These includes move into `engine` during the D3D11 port, once
  the downstream modules that rely on the transitive include have been found.

## Phases

### Phase 0: scope and baseline (done in the first commit)

- Deleted the unreferenced Tier 3 modules.
- Deleted `Inc/COMDEF.H`, `COMIP.H` and `COMUTIL.H`, VC6-era copies that shadowed the Windows SDK
  headers.
- Deleted the VS.NET 2003 `Allegiance.sln` and the `.vcproj` files of the modules now ported. The
  `.dsp` files and nmake makefiles stay as reference until each module is ported.

### Phase 1: MSBuild skeleton (in progress)

- `Directory.Build.props`: toolset v145, Windows SDK 10.0, MultiByte character set.
- `msbuild/Allegiance.Cpp.props`: shared compiler settings (static CRT, C++17, `/EHsc`, `/W3`, shared
  include order taken from the original `makefile.inc`). Output goes to `out/`, which git ignores.
- `Allegiance.slnx` with `.vcxproj` files for `zlib`, `guids`, `_Utility`, `engine` and `effect`. Source
  lists are taken from the original makefiles; files the original build excluded (`dcbase.cpp`,
  `dialog.cpp`, `dcdevice3d.cpp`, `efsound.cpp`, `panes.cpp`, `engine/scan`, `engine/exec`) stay
  excluded.
- Projects for all remaining Tier 1 modules (except `xmunge`, see above), generated from the
  original makefiles' source lists, include paths and defines.
- `Directory.Build.targets`: `AllegianceXslt` (AGC's XML to IDL/MC/RC2/headers) and
  `AllegianceMessage` (`mc.exe` for `.msg`/`.mc`). MIDL uses MSBuild's built-in `Midl` items.
- Generated files shared between projects go to `out/gen/<Platform>/<Configuration>/<module>/`.
  `AGCGuid` (the original `AGCGuid.lib`) owns AGC's code generation, so it builds before every
  project that includes the AGC headers.
- Resource scripts: the makefiles concatenated module `.rc` + generated message `.rc` +
  `#define FLAVOR` + `Inc/VerRes.rc` with `copy`/`type`. Each executable now has a small
  `*.build.rc` that `#include`s the same pieces.
- `_WIN32_WINNT`/`WINVER` are set to Windows 10 globally; current ATL refuses the original 0x0400.
- Still to do: a vcpkg manifest (libjpeg-turbo) once the vendored copy is replaced. qhull stays
  vendored: `cvh` needs only the qhull executable, and the art pipeline no longer needs meshoptimizer.

### Phase 2: make it compile

- Two-phase lookup and `typename`, string-literal constness, for-loop scope, secure-CRT fixes.
- Old ATL 3 code (`statreg.cpp`, `atlimpl.cpp`, `CComModule`) moved to the current ATL module classes.
- Compile-time shims for the DirectX headers that are no longer in the SDK (`d3drm.h`, `dplay.h`,
  `dplobby.h`), so everything links before behaviour changes. The shims are written for this project, not vendored from
  old SDKs, and they are deleted as Phase 3 replaces each API (`compat/dplay` went with the MsQuic networking).
- Zone and Club code paths removed. The MSN Gaming Zone services they talked to no longer exist, and
  nothing built ran them: `USEAUTH` (Zone authentication) was never defined, and `AllSrv32` is the
  standalone server, which compiles out every `!ALLSRV_STANDALONE` Zone-server path. Removed:
  - the Club client: `ConnectToClub` and its connection, the Club messages (`Club/ClubMessages.h`),
    the character info, squads, leaderboard and Zone events screens (`WinTrek/CharInfoScreen.cpp`,
    `SquadsScreen.cpp`, `leaderboardscreen.cpp`, `zoneeventsscreen.cpp`, `ZoneSquad.*`) and their art;
  - Zone authentication: `_Utility/zauth.cpp`, the server's ticket check, the `AuthServer` setting, the
    `-noauth` and `-zone` switches, and the Zone events in `AGC/AGCEvents.xml` (2007-2009, 2023, 6023);
  - the Zone variant of the games list (zone icon, event-details download from `ZoneEventDetailsURL`);
  - the `Zone/` and `Club/` directories, and the Zone/Club keys in the client config file
    (`ZAuth`, `ClubLobby`, `Club`, `ClubMessageURL`, `ZoneEventsURL`, `ZoneEventDetailsURL`,
    `PassportUpdateURL`, `ZoneAuthGUID`, `ClubMessageCRC`, `UsePassport`).

  The Internet button now opens a plain lobby screen (`WinTrek/LobbyScreen.cpp`, formerly
  `ZoneClubScreen.cpp`): config download, auto-update and message of the day, then the free lobby. The
  logon popup asks only for a call sign. Main menu: LAN, Internet and Training.

  Protocol: `MSGVER` is 199 (Zone ticket out of `FM_C_LOGONREQ`, `bClubGame` out of `MissionParams`) and
  `LOBBYVER` is 9 (ticket out of `FM_C_LOGON_LOBBY`). Old and new clients, servers and lobbies do not
  interoperate, and an old client can't even reach a new lobby's auto-update: it sends the old
  `FM_C_LOGON_LOBBY` layout, which a new lobby misreads (`FM_C_LOGON_LOBBY_OLD` covers only much older
  clients). That is acceptable because nothing speaking the 2004 protocol is left to stay compatible with:
  the Zone lobbies are gone.

  If characters and persistent multiplayer come back, the removed screens are a starting point: the last
  commit that has them is `84f18ae` (`git checkout 84f18ae -- WinTrek/CharInfoScreen.cpp ...`, plus their
  MDL and bitmaps from `Artwork/256`). They need a new server behind them; the Club server ran on SQL
  Server and Zone authentication.

  The parked Tier 2 code (`lobby`, `cliconfig`, and the pigs under `test/`) still has Zone references and
  is not built; `lobby/pch.h` includes the deleted `zauth.h`.
- `/permissive-` turned on (not done yet).
- The old build system is deleted (190 files): the nmake makefiles, `makefile.inc`/`makeexe.inc`/`makedep.inc`,
  the VC6 `.dsp`/`.dsw` and VS.NET `.vcproj`/`.sln` files, `VSNet.Readme`, and `build/`, the 1999 build-lab
  scripts (build numbering, BVT scripts, setup packaging, all hard-wired to `%FEDROOT%`). Git history keeps
  them for anyone reviving a Tier 2 module.

### Phase 3: make it run

| Subsystem | Today | Target |
|---|---|---|
| Rendering | DirectDraw 4 + Direct3D 3 (DirectX 6), software rasterizer | D3D11 backend. Stage 1 (below): a D3D11 rasterizer with the engine's CPU transform and lighting. Stage 2: transform and lighting in shaders |
| Input | DirectInput 2/7 | DirectInput 8 |
| Audio | DirectSound / DS3D | Unchanged at first; XAudio2 later |
| Music | Audio-CD tracks through MCI (`redbooksound.cpp`) | Same `IDiskPlayer` interface, playing the WAVs from `Artwork/redbook` (track order from `tracks.lst`) |
| Networking | DirectPlay 4 inside `FedMessaging` | MsQuic: a reliable ordered stream and unreliable datagrams per connection, plus a UDP probe for LAN discovery (see "Networking" below) |
| Settings | `HKLM\Software\Microsoft\Microsoft Games\Allegiance\1.0` (and `...\Server`) | A JSON file per program (see "Settings" below) |

The DirectPlay surface used by `FedMessaging` is about 15 calls: `Open`, `Close`, `EnumSessions`,
`GetSessionDesc`, `SetSessionDesc`, `CreatePlayer`, `DestroyPlayer`, `GetPlayerData`, `SendEx`,
`Receive`, `CancelMessage`, `CreateAddress` and `InitializeConnection`. Groups are already implemented
inside `FedMessaging`.

#### Rendering, stage 1: D3D11 rasterizer

Decisions:

- **Scope.** Stage 1 keeps the engine's CPU transform, lighting and clipping (`device3d.cpp`), and replaces only what sits under it.
- **Removed.** DirectDraw 4, Direct3D 3 and the software rasterizer. The D3D11 WARP adapter covers machines without a GPU.
- **Fullscreen.** A borderless window covering the monitor. The game renders at its chosen resolution, scaled to fit and letterboxed.
- **Verification.** Manual testing; CI only builds.

What the map of the old code showed:

- **2D is already all CPU.** Text is drawn by `font.cpp` writing 16-bit pixels. Point blits and fills are CPU loops. No code outside the engine touches DirectDraw or writes pixels through a pointer.
- **DirectDraw held only three things:**
  - the window render target, or the fullscreen back buffer;
  - texture copies;
  - a GDI source for `mdlc`'s BMP import.
- **The 3D path is small.** It is DirectX 6 `IDirect3DDevice3` with pre-transformed `D3DTLVERTEX` vertices and one texture stage, using DECAL or MODULATE. Cut-outs use the colour key, and there are three blend modes. There is no fog, specular, alpha test, multi-texture or mip-mapping. Textures were capped at 256x256.
- **The software rasterizer was not just a fallback.** Every 3D surface without the `Video` bit used it, including the off-screen panes: loadout and inventory ship previews, popups and some UI.
- **Windowed mode needed a 16-bit desktop.** On a 32-bit desktop the game always switched to exclusive fullscreen.
- **There is no restore logic to preserve.** The engine rebuilt everything after a device change, and its device-dependant registry was never used.

Design:

| Piece | Old | New |
|---|---|---|
| Surfaces | System memory, or DirectDraw "video" surfaces; locks and conversions between them | Always system memory (565). Read-only MDL mappings are copied on first write |
| 3D | `D3DRasterizer` (HAL) or `SoftwareRasterizer`, per surface | `d3d11raster.cpp` for every 3D surface. At `BeginScene` the CPU pixels are uploaded if they changed; at `EndScene` the result is read back if anything was drawn. The Context already ends the scene before each 2D operation |
| State | DirectX 6 render states | HLSL in `engine/shaders`, compiled by `FxCompile` in CI. Covers DECAL/MODULATE, flat/Gouraud, colour-key discard, the three blend modes, z test/write, cull, scissor, and a Z clear limited to the clip rect. Vertices are shifted half a pixel to keep DirectX 6 pixel centres |
| Textures | DirectDraw texture surfaces, re-`Load`ed when changed | `D3D11Device` cache: BGRA8 with the colour key in alpha, refreshed when the surface's change counter moves, full size |
| Presentation | `Flip` (fullscreen) or `Blt` to the primary surface through a clipper (windowed) | A flip-model swap chain per window. The frame surface is drawn scaled and letterboxed into the client area (1:1 windowed), with vsync |
| Fullscreen | `SetDisplayMode` at 16 bpp in exclusive mode | A borderless topmost window on the monitor. The fullscreen size picks the render resolution from the original mode list, limited to the monitor |
| Gamma | DirectDraw gamma ramp | The same curve in the presentation pixel shader |
| Device loss | `TestCooperativeLevel`, full rebuild | `DXGI_ERROR_DEVICE_REMOVED`/`RESET` re-creates the device. Nothing is lost, because surfaces live in system memory |
| Tools | `mdlc` initialised DirectDraw with `-software` | The engine creates the D3D11 device only on first use, so `mdlc` needs no GPU. BMP import uses `GetDIBits`. The 1,405 24-bit sources get identical pixels. The 37 16-bit (555) sources are read at their own depth, so the 565 conversion is unchanged. The 7 8-bit sources now come out in their palette colours; the old conversion ignored palettes (zero colour masks), so they almost certainly came out black |
| Movies | DirectShow `IAMMultiMediaStream` into a DirectDraw surface | Stubbed: the video image reports the movie as finished. Media Foundation playback is a follow-up |

Deleted:

- `ddsurface.cpp`, `dddevice.cpp`, `d3ddevice.cpp`, `raster.cpp`, `softraster.cpp` and `palette.cpp`.
- The unbuilt `fliphelp.*`, `ddstruct.*`, `dcdevice3d.cpp`, `engine/scan` and `engine/exec`.

`ddraw.lib` is no longer linked. `zlib.h` includes only `d3dtypes.h`, for the vertex and colour structs that are still used everywhere.

Known gaps to check when testing:

- The 2D/3D synchronisation costs a full-frame upload and readback around each 3D layer. At 800x600 this should be a few milliseconds, but it is unmeasured. Stage 2 removes it (below).
- Pixel-exactness against the DirectX 6 output is unverified (half-pixel rule, colour-key edges with linear filtering).
- Movies are skipped.

#### Rendering, stage 2: GPU-resident frame, transform and lighting in shaders

Decisions:

- **Scope.** The frame and the off-screen 3D panes stay on the GPU. 2D drawing into them (blits, fills,
  text) becomes GPU draws, and transform, lighting and clipping move into the vertex shader. Stage 2 is
  about structure and headroom; the game already runs smoothly.
- **Colour depth.** Render targets are 32-bit (BGRA8). Art is still converted from its 565 sources, so it
  looks the same, but shading and blending no longer band.
- **Resolution.** The 4:3 modes stay, scaled and letterboxed. Native or widescreen rendering is a UI layout
  project of its own.
- **Verification.** Manual testing; CI only builds. The frame-rate overlay (Alt+F) gains a line with the
  per-frame uploads, readbacks, texture refreshes and draw calls, next to the existing milliseconds per
  frame, so the effect is measurable.

What stage 1 left in place:

- Every 3D layer uploaded the whole 565 frame at `BeginScene` and read it back at `EndScene`.
- The Context draws `FillRect` through the 3D path but blits images (`DrawImage`) on the CPU, ending the
  scene first. A screen that mixes the two paid an upload and a readback for every switch.
- `Present` uploaded the finished frame once more.

Design:

| Piece | Stage 1 | Stage 2 |
|---|---|---|
| Surface storage | System memory only; the rasterizer kept a GPU copy for the length of a scene | A surface that the GPU has rendered into keeps a GPU copy (BGRA8 render target and shader resource, plus a z-buffer on demand) next to its 565 pixels. Two flags say which copy is current |
| Synchronisation | Upload at `BeginScene`, readback at `EndScene` | Lazy, per access. Reading the pixels (`GetPointer`, `GetPixel`, `Save`, `Write`, a CPU blit from it) reads back if only the GPU copy is current. Writing them (`GetWritablePointer`, `SetPixel`) makes the CPU copy current. A GPU draw uploads first if only the CPU copy is current |
| 2D into a GPU-resident surface | CPU loops | Blits are textured quads with point sampling (exact pixels); the colour key discards. Fills are solid quads; `FillSurface` is a clear. Text draws quads from a per-font glyph atlas built from the 1-bit glyphs. Stretch blits are scaled quads. Clipping is the scissor rect |
| 2D into a CPU surface | CPU loops | Unchanged. Surfaces that never take a 3D draw (art, 2D-only panes) never get a GPU copy, so `mdlc` still needs no GPU |
| Blit and texture sources | Texture cache, refreshed from the 565 pixels by change counter | The same cache for CPU surfaces. A GPU-resident source is used directly through its shader resource view, with no copy |
| Presentation | Upload the frame, then draw it scaled | Draw the frame's own GPU copy scaled |
| 3D vertices | CPU transform, lighting and clipping; screen-space vertices uploaded per draw | `Vertex` and `VertexL` geometry goes to the vertex shader in model space with the full matrix and lighting in a constant buffer. The rasterizer clips against the clip-rect viewport.Extra clip planes (beyond the six frustum planes, at most two) become `SV_ClipDistance`. Vertices are streamed through the dynamic vertex buffer each draw: meshes are small, and a per-mesh GPU cache would need ownership the geometry API doesn't give |
| Lighting | `LightVertex`, `BidirectionalLightVertex`, `MaterialLightVertex`, global colour, copy | The same formulas per vertex in HLSL, chosen by a mode constant: directional with ambient, two-sided with an alternate colour, material diffuse, global colour, white |
| Kept on the CPU | | Deformation (`TransformDeformClip`, the only per-vertex callback), lines wider than one pixel (`DrawLinesWithPolygons`, built in screen space) and points (the rasterizer plots them without texture, blending or z). All produce screen-space vertices for the stage 1 path |

Device loss drops the GPU copies. A surface whose only current copy was on the GPU keeps its last CPU
pixels; for the frame that is the previous frame, which is redrawn anyway.

Differences to look for when testing:

- Colours no longer quantise to 565 in 3D, blends or colour fills (by design).
- A pane drawn with a colour key treats as transparent anything that would have been black in 565, which
  is what the readback into 565 used to do.
- Text cells on the GPU cover each glyph's whole bytes, as the unclipped CPU path did. The CPU path for a
  glyph cut by the left or right clip edge only drew its first eight columns (an `ibit & 0x7 == 0`
  precedence slip); the GPU draws the whole glyph up to the clip edge.
- Stretch blits sample at pixel centres, which can pick a neighbouring source pixel compared with the
  integer CPU loop. Only the (stubbed) movie image uses them.

#### Networking: MsQuic replaces DirectPlay

Decisions:

- **Transport.** [MsQuic](https://github.com/microsoft/msquic), the Schannel build. One QUIC connection per
  client: a bidirectional stream carries the guaranteed messages in order, QUIC datagrams (RFC 9221) carry
  the unguaranteed ones. Congestion control, loss recovery, fragmentation and keep-alives are MsQuic's.
- **Windows.** The Schannel build needs TLS 1.3 from Windows, so Windows 11 or Server 2022 and later.
  Windows 10 is out of support (October 2025); it would need the OpenSSL build.
- **Server identity.** QUIC requires TLS, so every server has a certificate: `AllSrv32` creates a
  self-signed one at startup (ECDSA P-256, key kept under a fixed name in the user's key store so it is not
  regenerated every time). Clients don't validate it for now. Traffic is encrypted, but the server isn't
  authenticated. Pinning a fingerprint published by the lobby is the planned next step.
- **Delivery.** The `Microsoft.Native.Quic.MsQuic.Schannel` NuGet package (packages.config, restored by
  Visual Studio and by CI into `packages/`). `msquic.dll` is copied next to the exes and loaded at run
  time, so the game still starts without it (networking then reports why it can't).
- **Verification.** CI builds it; LAN play is tested by hand.

What FedMessaging used DirectPlay for, and what replaces it:

| DirectPlay | Replacement |
|---|---|
| `Open(DPOPEN_CREATE)`, server player | `HostSession`: a MsQuic listener on the session's port, plus the discovery socket |
| `Open(DPOPEN_JOIN)`, `CreatePlayer` | `JoinSession`: connect, open the stream, send a hello (application GUID, protocol version, player name); the server answers with a welcome (session instance) or a refusal. The call waits for that, as the DirectPlay join did |
| `DPSYS_CREATEPLAYERORGROUP` / `DESTROYPLAYERORGROUP` | A hello creates the `CFMConnection` (`OnNewConnection`); connection shutdown deletes it, or ends the session on a client |
| `SendEx` guaranteed / unguaranteed | A length-prefixed frame on the stream / a datagram. An unguaranteed packet bigger than the connection's datagram limit goes on the stream |
| Groups (`CreateGroup`, `AddPlayerToGroup`, group sends) | Membership lists kept by FedMessaging; a group send goes to each member. A deleted connection leaves every group, as with DirectPlay |
| `DPSYS_SENDCOMPLETE` failure → `OnMessageNAK` | A guaranteed send cancelled by MsQuic (the connection is going away) |
| `EnumSessions` | A UDP probe, broadcast on the LAN or sent to a named server, answered with the session instance, its QUIC port and the details `SetSessionDetails` sets (game name, players, limit). Calls are polled, as before: each call reports what has answered so far |
| `GetPlayerAddress`, `GetPlayerCaps`, `GetMessageQueue` | Remote address, RTT and bytes in flight from MsQuic's connection statistics |
| `dplobby.h` addresses, `CheckVersion`, `killsvr.exe` | Gone |

Threading: MsQuic calls back on its own threads. Those callbacks only queue events (new peer, packet,
peer gone, send failed); `FedMessaging::ReceiveMessages` drains the queue on the game thread, so the
`IFedMessagingSite` callbacks run where they always did.

Ports (UDP): game servers 47700 (QUIC) and 47701 (discovery); the lobby's client side 47710/47711 and its
server side 47720/47721. A server address may name another port as `host:port`.

#### Settings: a JSON file replaces the registry

Every setting the game and the server kept under `HKLM\Software\Microsoft\Microsoft Games\Allegiance\1.0`
now lives in a JSON file. Writing to HKLM needs administrator rights, which a process with the default
`asInvoker` manifest doesn't have, so saved preferences were silently lost for normal users. A file is
also easier to inspect, edit, copy and reset.

| Program | File | Where |
|---|---|---|
| Client (`Allegiance.exe`) | `settings.json` | `%LOCALAPPDATA%\Allegiance`, created on the first write |
| Server (`AllSrv32.exe`) | `server.json` | `%ProgramData%\Allegiance`, so the service account and an administrator see the same file |
| Either, portable | the same name | next to the executable, if a file of that name is there (an empty `{}` is enough). It then takes precedence |

The file is one JSON object. Values keep their old registry names and sit at the top level, except for two
sections:

```json
{
  "CombatXSize": 1024,
  "Music": 1,
  "CharacterName": "Pilot",
  "SquadMemberships": { "Pilot": [12, 40] },
  "EventLog": { "EnabledNTEvents": "default" }
}
```

Numbers stand in for `REG_DWORD`, so flags stay 0 or 1. `true`/`false` and numeric strings (`"1"`, `"0x10"`)
also read as numbers, so hand edits are forgiving. Strings are UTF-8 in the file and the ANSI code page in
the game, as before. The squad memberships per character were a `REG_BINARY` array of IDs and are now a
JSON array. The AGC event logger, configured through `IAGCEventLoggerPrivate::Initialize`, now gets the
settings file's path instead of a registry key, and keeps its five values in the `EventLog` section.

`zlib/zsettings.h` is the interface: `ZSettingsFile` for one file, and the `ZSettings` functions for the
process's own file. It is a small reader and writer (strict RFC 8259 on input, a UTF-8 byte order mark
allowed), without ZString or the rest of zlib, so AGC uses it too. Every write re-reads the file, applies the
change, and writes it back to a temporary file that is then renamed over the original, under a named mutex
per file. The server process and the AGC logger inside it, or two clients, don't lose each other's changes,
and a crash mid-write leaves the old file. Reads come from the copy last read. A file that doesn't parse
is ignored (the defaults apply), a debugger trace says where it failed, and it is kept as `<name>.bad` when
the next write replaces it.

The server makes `server.json` its settings file first thing in `main`. `InitSql` runs earlier, during
static initialisation (`BEGIN_SQL_DEF` in `queries.h`), so it opens the file by name instead.

Behaviour that changed:

- Without its registry key the server skipped its whole settings block, so `nUpdatesPerSecond`,
  `fWantInt3`, `fProtocol`, `fDoublePrecision` and `fTimeout` stayed zero (static initialisation) and
  `LocalAddress` stayed empty. It now always applies the documented defaults: 10 updates per second, the
  flags on, and the host name as the local address.
- Existing registry values are not imported. A fresh file starts from the defaults, and the old key
  can be deleted by hand.

Removed with the registry code, because nothing used them: `zlib/zreg.h`, `Inc/regkey.h` (the service
name moved to `FedSrv/serversettings.h`), `UTL::GetPathFromReg`, `ZString::GetProfileString` (a `win.ini`
read, which Windows maps to the registry), `WinTrek/netinfo.cpp` and the intro screen's check that
used it (a Windows 9x probe for network cards with known-bad drivers, through `HKEY_DYN_DATA`, which NT
doesn't have, so it always passed), `CheckForAllGuard` (launched `AllGuard.exe`, which isn't in the
repository, from the registry's art path), the `TCPropBagOnRegKey` COM class (registry-backed property
bags; in AGC's IDL but not in its object map), and TCLib's registry-switched lock tracing
(`_ENABLE_TRACE_LOCKS`, never defined).

What stays in the registry is Windows' own: COM class, AppID and type library registration for AGC and
the server (`.rgs` files, `-RegServer`), the service entry, the event-log source, and the performance
counter IDs `sharemem` reads from the service's `Performance` key.

The parked Tier 2 tools `cliconfig`, `srvconfig` and `AllSrvUI` exist to edit the old registry values.
Nothing reads those values any more; reviving one of these tools means porting it to `zsettings.h`.

### Phase 4: art pipeline

`Artwork/Art.proj` replaces the nmake/awk/sed/batch chain (`Artwork/256/makefile`, `makegeo.mak`,
`Artwork/tools/*.bat`, `*.awk`, `sed.exe`), which has been deleted. It reads `bitmap.txt` and
`copymdl.txt` unchanged, and `Artwork/256/Models.props`, a mechanical conversion of `makegeo.mak`'s
model list. It builds incrementally through Inputs and Outputs.

| Step | Original | Replacement |
|---|---|---|
| Images | `mdlc -convert` (24-bit BMP to 16-bit MDL) | Same, rebuilt |
| Animations | `mdlc -compressanim` | Same |
| Flatten / extract / bounds | `xmunge` on Direct3D Retained Mode | `xmunge` rewritten as portable C++17 on its own text `.x` reader (done) |
| Levels of detail | MSR progressive-mesh binaries + `xmunge -reduce` | Dropped: every LOD slot uses the full-detail mesh |
| Collision hulls | `cvh` + qhull | Same; qhull vendored and built in the solution |
| Model compile | `sed` templates + `mdlc -optimize` | Template substitution in MSBuild, then one `mdlc -batch` |

`mdlc` links the whole engine, so art conversion is gated on the engine building and running. The repo
has no reference output, so Stage A is verified by loading and rendering in game, unless original
retail `.mdl` files are available to compare against.

#### Hard-coded paths in the art tools

The original art build assumed a fixed `%FEDROOT%` layout (`%FEDROOT%\src\artwork\...`,
`%FEDROOT%\objs\debug\<tool>\<tool>.exe`, `%FEDROOT%\src\tools\build\awk.exe`, `%FEDROOT%\src\bin\qhull`).

| Where | Treatment |
|---|---|
| `cvh/main.cpp`: `system("%fedroot%\\src\\bin\\qhull ...")` (2 sites) | Fixed in code. Uses `qhull.exe` next to `cvh.exe` (same build output directory) or `ALLEGIANCE_QHULL`, and quotes paths so spaces work |
| `cvh/main.cpp`: debug hull mesh written to `..\artbuild\<name>_m.x` | Fixed in code. Written to the working directory. The original crashed (`fprintf` to NULL) when that directory did not exist |
| `Artwork/tools/*.bat`, `*.awk`, `Artwork/*/makefile`, `makegeo.mak` (about 70 path lines; `awk.exe` was never in the release; `sed.exe` is bundled) | Replaced by `Art.proj`, whose paths are relative to `$(AllegianceRoot)` and the build output. Deleted. |
| `Artwork/tools/pmesh*/convpm.bat` + MSR progressive-mesh binaries | Deleted with the reduced LODs |
| Art data (`.x`, `.mdl`) | Checked: no absolute texture or include paths |

#### Art.proj status

Done:

- Bitmaps: the 1,447 entries in `bitmap.txt`.
- The six animations.
- The font.
- The copied files: `copymdl.txt` plus the `wav`, `rtf`, `avi`, `igc` and `mml` wildcards.
- The 223 template models. Each gets its collision hull through `xmunge`, `cvh` and qhull, then is compiled with `mdlc -optimize`. 45 of them have separate `_bound.x` hull pieces.
- The 16 hand-written text-MDL models.

Every step is incremental. A run with nothing changed does nothing, and touching one bitmap reconverts only that bitmap.

How it was checked:

- Locally, `xmunge`, `cvh` and qhull are built natively and run for real; `mdlc` is replaced by a stand-in that only creates the output files.
- The real `mdlc` conversion runs only in CI.
- `fig02pit` is left out because its source is missing.

Each model's template refers to its `.x` files by absolute path in `out\obj\Artwork\models\`. That lets `mdlc` run in `out\Artwork\` without the intermediates. It has to run there because textures are looked up on the art path, and a missing texture is a fatal error (`ModelerImpl::GetFile` asserts).

`ops1` and `ops2` are in `makegeo.mak`, and their textures `ops1.bmp` and `ops2.bmp` are in the drop. But the textures were never listed in `bitmap.txt`, so the original build could not have produced these two models. Both lines are now added to `bitmap.txt`.

Two `mdlc` changes support this:

- `-batch <file>` runs one command per line in a single process. A Debug `mdlc` spends about 2.4 s starting the engine, so one process per bitmap would take about an hour. Each converted namespace is unloaded after it is written, which keeps memory flat in the 32-bit process.
- Failures now exit with code 1. `ModelerSite::Error` used to call `_exit(0)`, and output files that could not be written were skipped without an error, so the original build could not notice a failed conversion.

#### xmunge

The rewrite keeps `-flatten`, `-extract` and `-bound` and matches Direct3D Retained Mode's behaviour:

- It loads only the first top-level frame.
- An animated frame's matrix is replaced by scale · rotation · translation from its keys. Missing key types are identity, rotations use slerp, and times clamp to the key range.
- `-bound` pieces are written in model space.

These rules were worked out against the art, and a separate reference implementation produces the same vertices on all 579 models in `Artwork/256`. The reader accepts the eight hand-edited files with unbalanced braces (`beam.x`, `bullet.x`, `circle.x`, …) the way D3DRM did.

#### Art directory cleanup

The original build only gathers art from `Artwork/256`, through these inputs:

- `bitmap.txt`, `copymdl.txt` and `fontsource.mdl`.
- The sources named in `makegeo.mak` (`<geo>.x`, `_static.x`, `_bound.x`, text MDLs).
- Wildcard copies of `*.wav`, `*.rtf`, `*.avi`, `*.igc` and `*.mml`.

The client reads only that output directory. Everything else was deleted, and git history keeps it:

- `Artwork/old256`, `cadet` (art for the separate Cadet product, never integrated), `ship`, `sounds`, `soundpac1`, `training`, `team2.igc` and `team4.igc`.
- About 800 files in `Artwork/256`:
  - prebuilt `.mdl` and intermediate `.x` files that `makegeo.mak` regenerates;
  - bitmaps and models that no manifest names;
  - `artwork_new/`, `icons/` and stray scripts and notes.
- The progressive-mesh tools and unused templates and scripts in `Artwork/tools`.

A before-and-after check confirms that all 3,186 inputs the manifests need are still present. That check covers every model's optional `_static.x`, `-low.x` and `_bound.x` sources. The first version of the cleanup missed them: `makegeo.mak` names each model as `$(@B)`, so a regex read the literal `$(@B)` instead of the name, and 234 sources were deleted. They have been restored.

Kept on purpose:

- `Artwork/redbook`: the soundtrack. The game plays music as audio-CD tracks through MCI (`soundengine/redbooksound.cpp`), so the port has no music without a CD. A Phase 3 follow-up replaces the CD player with file playback from these WAVs.
- The 41 files below. They are referenced by name from code or a shipped MDL, but the original build never put them in the art output, so they were missing at runtime even in 2004. They are latent bugs to check when the client first runs: either the referencing code is dead, or the file should be added to a manifest. Also, `makegeo.mak` builds `fig02pit.mdl` from `fig02pit.x`, but no `fig02pit.x` source exists.

  `BtnMissionDetails.bmp`, `BtnMount.bmp`, `BtnReject.bmp`, `BtnReqPos.bmp`, `BtnSell.bmp`, `BtnTeamRoom.bmp`, `btnCharInfo2Zone.bmp`, `btncombo.bmp`, `btnmain.bmp`, `btnmaingames.bmp`, `btnpicksector.bmp`, `cmdformation.bmp`, `concmdbardisplay.bmp`, `conenergyweaponsdisplay.bmp`, `confabsel.bmp`, `conmissilesdisplay.bmp`, `conordersdisplay.bmp`, `conradarasteroids.bmp`, `conradarships.bmp`, `conradarstations.bmp`, `conradartarget.bmp`, `conradartreasures.bmp`, `conselectiondisplay.bmp`, `conshrunkcmdbardisplay.bmp`, `console.mdl`, `constationdisplay.bmp`, `conteambarsel.bmp`, `conweaponsdisplay.bmp`, `credits.bmp`, `hudgauge.bmp`, `loadout1.mdl`, `loadout11.mdl`, `loadout3.mdl`, `loadout4.mdl`, `loadout5.mdl`, `loadout9.mdl`, `loadoutbomber.mdl`, `loadouttacticalfighter.mdl`, `messageoftheday.mdl`, `techtree.mdl`, `temp.mdl`

#### Smoke test

CI runs `tools/ci/mdlc-smoke.ps1` after the build. It converts one bitmap and one animation, and
compiles `fontsource.mdl`. This establishes whether `mdlc`, which starts the engine with its software
DirectDraw path, runs on a headless runner. The first run crashed during engine start-up: `FontImpl::GetSizes` asserted on a zero-width glyph, which current GDI reports for some characters. The assert is relaxed to reject only negative widths; the Debug client used the same path.

### Phase 5: optional

x64, C++20, replacing `ZString` and the custom containers, XAudio2, a D3D12 backend, Tier 2 modules.

## Building

Open `Allegiance.slnx` in Visual Studio 2026 (Desktop development with C++, plus the C++ ATL and MFC
components) and build `Debug|x86`. From the command line:

```
msbuild Allegiance.slnx -p:Configuration=Debug -p:Platform=x86
```

CI builds each library on its own (`-p:BuildProjectReferences=false`), so one failure doesn't hide the
errors of the others. The error summary is on the workflow run page, and the full logs are attached to
the run.

MsQuic comes from NuGet (`_Utility/packages.config`, into `packages\`). Visual Studio restores it on
build; from the command line, restore first:

```
msbuild _Utility\Utility.vcxproj -t:restore -p:RestorePackagesConfig=true -p:SolutionDir=%CD%\
```

The build copies `msquic.dll` next to the exes. Networking needs Windows 11 or Windows Server 2022 or
later (QUIC's TLS 1.3 comes from Windows), and UDP 47700-47701 open for a game server.

Then build the art with the tools from that build:

```
msbuild Artwork\Art.proj -p:Configuration=Debug
```

The art goes to `out\Artwork\`, and `out\bin\Win32\<Configuration>\Artwork` is made a directory
junction to it, because that is where `Allegiance.exe` and `AllSrv32.exe` look for their art
(`-p:ArtLink=false` skips the junction). The client uses a different location only when
`HKLM\SOFTWARE\WOW6432Node\Microsoft\Microsoft Games\Allegiance\1.0` has an `ArtPath` value, and the
server only when the `Server` key below it does (the game is 32-bit, hence `WOW6432Node`). In CI, a
separate `art` job does this with the tools from the build job, and attaches the result to the run as
the `art-Debug` artifact.

`font.mdl` holds pre-rendered glyphs, so the fonts `fontsource.mdl` names must be available when it
is built. Verdana and Tahoma come with Windows, but Eurostile does not, and Windows quietly
substitutes Courier New for it (the menu titles then look garbled). `Art.proj` passes the fonts in
`$(ArtFontDir)` (default `bin\`, which is gitignored: Eurostile is not freely licensed) to
`mdlc -fontdir`, which loads them for that process only, so nothing has to be installed. Put
`eurosti.ttf` and `eurostib.ttf` there, or set `-p:ArtFontDir=<folder>`. When a font is still
missing, the build prints `mdlc : warning : font '...' is not installed, Windows substituted '...'`.
The CI art job has no fonts, so its `font.mdl` uses the substitute.

## Conformance fix log

Fixes are made per CI round, and each change keeps the original runtime behaviour.

| Pattern | Fix | Sites |
|---|---|---|
| VC6 for-loop scope (loop variable used after the loop) | Declare the variable before the loop | `zlib/zstring.cpp` |
| Default-int declarations (`static Foo(`, `operator =` with no return type) | Explicit `void` / `bool` / `T&` return types | `_Utility/listwrappers.h`, `engine/value.h`, `engine/context.h`, `soundengine/ds3dutil.h`, `clintlib/badwords.cpp`, `WinTrek/WinTrek.cpp` |
| `CastMember(X);` with no such macro (VC6 read it as an undefined `int` member function) | Removed | `engine/geometry.h`, `engine/image.h` |
| `abs(float)` ambiguous: the float overloads now live in `<cmath>` | `zlib.h` includes `<cmath>` and `<cstdlib>` | `zlib` |
| C2712: `__try` in a function that needs object unwinding (`/EHsc`) | Body moved into `Win32MainBody`; `Win32Main` keeps only the SEH frame. Also `delete` changed to `delete[]` for the `new[]` spacer | `zlib/Win32app.cpp` |
| VC6 for-loop scope, the rest of Tier 1 (22 sites, including ones that compile but would now refer to a different variable) | Found with a scanner and hoisted the same way. `WinTrek/trekinput.cpp` read `indexJoystick` after its loop had finished; that bug is fixed separately (see below). | `engine`, `Igc`, `FedSrv`, `clintlib`, `training`, `WinTrek`, `cvh` |
| C3867: bare member-function name used as a pointer (about 420 sites, mostly `AddEventTarget(OnX, ...)` and rasterizer dispatch tables) | `&Class::Name`, found with a scanner that resolves the owning class | `engine`, `effect`, `WinTrek` |
| `d3drm.h` included only for a debug error-string table | Retained-mode entries and the include removed | `engine/dderror.cpp` |
| DirectInput 7 (`DirectInputCreate`, `IDirectInputDevice2`, `DIDEVTYPE_*`) not in the SDK | Ported to DirectInput 8. All DI8 game-controller types map to the engine's joystick. Unused dynamic-load path removed. `DIRECTINPUT_VERSION=0x0800` is set globally. | `engine/dinput.cpp` |
| C2712 in `ThingGeoImpl::~ThingGeoImpl` (`__try` in a destructor of a class with `TRef` members) | Guarded release moved into a static member that has no objects of its own. The members are released inside it, so the implicit member destructors afterwards do nothing. | `effect/thinggeo.cpp` |
| `dplay.h` / `dplobby.h` not in the SDK | Compile-only stand-ins in `compat/dplay/`. All GUIDs are null, so `CoCreateInstance` fails and the existing "DirectPlay unavailable" paths run. No call can reach `dplayx.dll` with a guessed ABI. Removed in Phase 3, when MsQuic replaced DirectPlay. | `Inc/messagecore.h` and users |
| Zone auth SDK (`zoneauth.h`, `zoneauthdecrypt.h`) not in the source release | `_Utility/zauth.cpp` reimplemented behind the same interface as "Zone unavailable": every authentication fails. The factories return objects rather than NULL because the callers never check for NULL. | `_Utility/zauth.cpp` (deleted with the Zone code) |
| Debug `#define new new(__FILE__, __LINE__)` (`zlib/alloc.h`) breaks ATL's `new(std::nothrow)` | `push_macro`/`pop_macro` around the ATL include | `Inc/allegdb.h` |
| Original bug: when a hat direction is bound in the key-mapping UI, the stored joystick index was the leftover loop counter (always one past the last joystick). That broke the saved mapping, its "Joy N Hat ..." label and duplicate detection. | Use the joystick that axis 5 (the hat, which drives `m_ppboolHatButton`) is mapped to | `WinTrek/trekinput.cpp` |
| ICQ chat relay (`icqmapi.dll`, proprietary ICQ API, header not in the release, service gone) | Handler for `FM_S_ICQ_CHAT_ACK` made a no-op, which is what the original did when the DLL was missing. The hard-coded ICQ licence key is removed with it. | `WinTrek/WinTrek.cpp` |
| ATL 3 implementation files (`atlimpl.cpp`, `statreg.cpp`) included into precompiled headers | Removed; current ATL is header-only plus `atls.lib` | `FedSrv/pch.cpp`, `AGC/pch.cpp` |
| `<ZoneAuthDecrypt.h>` included but unused | Include removed | `FedSrv/pch.h` |
| Library pieces removed in C++17 (`std::bind1st`, `std::binary_function`) | `_HAS_AUTO_PTR_ETC=1` restores them for now; they get replaced with lambdas and plain function objects in the Phase 5 library cleanup | `soundengine`, `AGC`, `test/TCLib` |
| More default-int forms: `static IsXValid(...)`, `const c_numGrooveLevels = 3;` | Explicit `bool` / `int` | `Zone/Member.h`, `Zone/Squad.h` (both deleted with the Zone code), `WinTrek/WinTrek.h` |
| `<assert.h>` pulled in by current CRT/ATL headers trips zlib's "own assert only" guard; the guard's `Don't` was also lexed as an unterminated character literal | `#undef assert` before `zlib.h` in TCLib; message reworded | `test/TCLib/pch.h`, `zlib/zassert.h` |
| qhull's `__STDC__` check: MSVC defines it only in standard C mode | `/std:c17` plus `/Zc:__STDC__` for qhull | `qhull/qhull.vcxproj` |
| zlib's global `swap(Type&, Type&)` template made every `std::sort` over a global-namespace type ambiguous (current algorithms call `swap` unqualified) | Replaced with `using std::swap;`, which does the same thing | `zlib/zmath.h` |
| `<typeinfo.h>` (pre-standard header) removed from the CRT | `<typeinfo>` | `test/`, `FedSrv/AdminUtil.h` |
| More typeless declarations (`static c = 0;`, `const cbData = ...;`) | Explicit `int` | `WinTrek/CharInfoScreen.cpp`, `WinTrek/ZoneSquad.cpp` (both deleted with the Club screens), `AGC/AGCOrientation.cpp` |
| Member-function pointers the scanner could not attribute (inherited member, or taken through another object) | `&TeamPane::OnButtonSize`, `&ConsoleDataImpl::OnBackButton` | `WinTrek/teampane.cpp`, `WinTrek/console.cpp` |
| `new TEvent<ZString>>::Delegate(this)`: a stray `>` that VC6 accepted | `TEvent<ZString>::Delegate` | `WinTrek/leaderboardscreen.cpp` (deleted with the Club screens) |
| zlib's `class IEventTarget` clashes with the DOM interface of the same name in the SDK's `mshtml.h`, which ATL's control/host headers pull in | Renamed to `IZEventTarget` (6 uses in 2 files) | `zlib/event.h`, `WinTrek/chatpane.cpp` |
| Dependent type names without `typename` in the connection-point macros (two-phase lookup) | `typedef typename ...` | `test/TCAtl/EventsCP.h`, `test/TCAtl/DualEventsCP.h` |
| `std::vector::begin()` used as a raw pointer (VC6's iterator was one) | `.data()` | `test/TCLib` |
| `__uuidof(StdComponentCategoriesMgr)`: the coclass declaration is gone from `comcat.h` | `CLSID_StdComponentCategoriesMgr` | `test/TCLib/TCLib.cpp` |
| zlib's debug `new` macro and the CRT `assert` against ATL in `TCAtl.h` and `AGC/pch.h` | Same `push_macro` / `#undef assert` treatment as before | `test/Inc/TCAtl.h`, `AGC/pch.h` |
| More `std::vector` iterators passed where ATL expects raw pointers (`CComEnum::Init`) | `.data()` / `.data() + size()` | `AGC/IAGCCollectionImpl.h`, `AGC/IAGCRangesImpl.h`, `AGC/TCStrings.cpp` |
| `TCLib.h` includes `zassert.h` after ATL may have defined the CRT `assert` | `#undef assert` first | `test/Inc/TCLib.h` |
| zlib's "no other assert" `#error` versus current CRT/ATL headers that include `<assert.h>` themselves. The earlier per-file `#undef assert` patches were order-dependent and broke when `<assert.h>` arrived after `zassert.h`. | Reverted. `zassert.h` now re-establishes zlib's `assert` at the end of every inclusion, outside its include guard, instead of failing | `zlib/zassert.h` |
| `COM_INTERFACE_ENTRY2(IPersistStream, IPersistStreamInit)`: current ATL static-asserts that the second argument derives from the first | `COM_INTERFACE_ENTRY_IID(IID_IPersistStream, ...)`, the standard ATL idiom, which exposes the same layout-compatible vtable (10 maps) | `AGC`, `test/TCAtl` |
| `::CrossProduct`: an inline friend that only argument-dependent lookup can find | Namespace-scope declaration added | `zlib/vector.h` |
| Address of a returned temporary (`&::CrossProduct(...)`) | Local variable | `AGC/AGCVector.cpp` |
| Hard-coded `..\..\Extern\sbn\include\msxml.h` | `<msxml.h>` from the Windows SDK | `Inc/XMLPtrs.h` |
| Indented typeless `const` declarations (the earlier scan missed indented lines) | `const int` | `FedSrv/FedSrv.CPP` |
| Deferred to the `/permissive-` switch: `&CComVariant(x)` (address of a class temporary) is still accepted in permissive mode with warning C4238 | none yet | `AGC/AGCCommand.cpp`, `AGC/AGCVector.cpp`, `AGC/IAGCRangesImpl.h` |
| `_com_ptr_t = CComPtr<T>`: the template assignment cannot deduce through the conversion | Assign the raw `.p`, which `_com_ptr_t` QueryInterfaces as before | `AGC/IAGCRangesImpl.h` |
| Current ATL's `GetPropertyMap()` returns `const ATL_PROPMAP_ENTRY*` | Property-map helpers take `const` (none of them write through it) | `test/TCAtl/PropertyClass.*` |
| The server linked AGC's import library, which the original build never did (only `AGCGuid.lib`) | Reference kept for build order only (`LinkLibraryDependencies=false`) | `FedSrv/AllSrv32.vcxproj` |
| Project-generation error (mine): `SWMRG.CPP` was marked "no precompiled header" because its first `#include` lines are inside a comment. Compiling `pch.h` separately duplicated the non-inline `IsAllSrvRunning()` that MIDL emits from a `cpp_quote` | Back on the precompiled header, as in the original makefile | `FedSrv/AllSrv32.vcxproj` |
