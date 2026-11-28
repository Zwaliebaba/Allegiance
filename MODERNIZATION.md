# Allegiance modernisation

Goal: port the 2004 Microsoft Research source drop of Allegiance to a Visual Studio 2026 / MSBuild
solution that compiles, runs on current Windows, and builds its art from source.

## Decisions

| Topic | Decision |
|---|---|
| Baseline | Strictly clean port of this repository. No code or fixes taken from community forks. |
| Toolchain | Visual Studio 2026, platform toolset v145, `.slnx` solution, x64 only (Win32 was built alongside until x64 had been played, then dropped). |
| Renderer | New Direct3D 11 backend behind the engine's existing `Engine` / `Surface` / rasterizer seam. DirectDraw 4, Direct3D 3 and the MMX software rasterizer are removed. |
| Art | Stage A: rebuild the original runtime format (binary MDL, 16-bit RGB565 images) through an MSBuild art project (done). Stage B: images as uncompressed 32-bit BGRA DDS files, converted once from the 24-bit source BMPs, which they replace as the source art; the colour key baked into alpha, mip levels for model textures; no MDL wrapper per image. Fonts stay in `font.mdl`; BC7 compression is not used (lossy, and UI code reads pixels on the CPU). |
| Level of detail | Reduced LOD meshes are dropped. They were a 1999 performance optimisation (the models are hundreds to a few thousand triangles). Every LOD slot uses the full-detail mesh, which removes `xmunge -reduce`, the MSR progressive-mesh binaries and meshoptimizer. |
| Networking | MsQuic (Schannel build) behind the existing `FedMessaging` interface, replacing DirectPlay 4. This replaced the first plan of an own Winsock UDP layer; with QUIC providing reliability and loss recovery, no loopback / packet-loss harness was built, and LAN play is verified by hand. Windows 11 or Server 2022 and later. |
| Settings | JSON files instead of the registry: `settings.json` per user for the client, `server.json` for all users for the server, or either next to the executable. A small built-in reader and writer (`NeuronCore/Base/zsettings.*`), no third-party JSON library. Registry values are not imported. The only registry use left is the Windows service entry, if the server is installed as one. |
| AGC | Removed. Its event system became a small plain C++ module in the server that writes a daily log file (`logs\server-<date>.log`); `LogAllEvents` in `server.json` adds the game events (chat, kills, logins) to the default set. No database or Windows event log output. |
| Server administration | The DCOM admin objects (`IAdminServer` and the rest) are removed with no replacement for now; a server is configured through `server.json`. A later admin protocol will run on the MsQuic listener. Training mission 7, the one in-repo user of the admin objects, starts its server as a child process. Windows performance counters are dropped. |
| Scope | Tier 1 is built. Tier 2 stayed in the repo, outside the solution, until the dead code cleanup deleted it. Tier 3 is deleted once nothing in Tier 1 or 2 references it. |
| Verification | GitHub Actions on `windows-2025` (VS 2026). `.github/workflows/windows-build.yml`. |

## Module tiers

**Tier 1 (built):** `zlib` (Microsoft's base library, not the compression library), `guids`, `_Utility`,
`engine`, `effect`, `soundengine`, `Igc`, `clintlib`, `training`, `WinTrek` (client); `mdlc`, `xmunge`,
`cvh`, `qhull` (art tools); `FedSrv`, built as the original standalone LAN server flavour `AllSrv32`
(`ALLSRV_STANDALONE`; the database-backed Zone server code it compiled out has since been deleted) with
the helper library it needs from `test/` (`TCLib`). (`sharemem`, the
performance-counter library, and `AGC` are deleted; see "Server administration" and "AGC removed".)

`xmunge` is not built yet. It is written entirely against Direct3D Retained Mode (frames, mesh
builders, progressive meshes), which Windows no longer has. Making it compile would mean writing a
D3DRM stand-in only to discard it, so it is rewritten in Phase 4 on the engine's own `.x` parser plus
meshoptimizer.

**Tier 2 (parked, then deleted):** `lobby`, `AllSrvUI`, `srvconfig`, `cliconfig`. They drove the registry, DCOM and AGC
interfaces that are gone, so they could not be revived without a rewrite, and the dead code cleanup deleted them
(see "Dead code cleanup"). The lobby protocol headers the game still uses moved to `Inc/`.

**Tier 3 (delete):**

| Module | Status |
|---|---|
| `MSRGuard`, `SymGuard`, `patch`, `fsmon`, `dreamcast` | Deleted. |
| `Zone`, `Club` | Deleted in Phase 2 with the Zone and Club code paths (see Phase 2). |
| `sentinal`, `fedperf` | Deleted. Only the VS.NET 2003 `Server.vcproj` compiled them into the server; the original nmake build never did. |
| `AutoUpdate` | Deleted in the dead code cleanup, with `lobby`. The code that drove it went later (see "Dead code cleanup"). |
| `test` | Kept: `FedSrv` builds on `test/TCLib`, `test/TCAtl` and `test/Inc` (as `AGC` did), so these are effectively Tier 1 support code rather than tests. |

### FedSrv and AGC

13 of the 23 `.cpp` files in `FedSrv` use AGC (the COM automation / admin event layer). Decision: AGC
moves into Tier 1 and is ported with the server, keeping the admin and event features intact. (Later
the DCOM admin objects were removed, and then AGC itself; see "Server administration" and "AGC removed".)

## Source layout

Since the restructure the code is six static libraries, two executables and a test DLL per library.
The phase notes below keep the module and file names of their time (`zlib`, `_Utility`, `WinTrek`,
`d3d11raster.cpp`, ...); this table says where each module went, and "Conventions" how the files were renamed.

| Library | Used by | Folders | Was |
|---|---|---|---|
| `NeuronCore` | everything | `Base`, `Utility` | `zlib` (but `main.h`), `_Utility` (but `allegdb.cpp`), `Inc/messagecore.h` (and `guids`, since deleted) |
| `NeuronClient` | client, mdlc | `Engine` (with `shaders`), `Effect`, `Sound`, `App` | `engine`, `effect`, `soundengine`, `zlib/main.h` |
| `NeuronServer` | server | `Base`, `Service` | `FedSrv/SWMRG.*`, the generic half of `FedSrv/allsrvmodule.*` (and `test/TCLib`, `test/TCAtl`, `test/Inc`, since deleted) |
| `GameCommon` | client, server | `Igc`, `Protocol`, `Version` | `Igc`, the session kinds (were `guids`), `Inc/Messages.h`, `messagesAll.h`, `Parts.h`, `trainingserver.h`, `VerRes.rc`, `SlmVer.h` |
| `GameLogic` | server | `Server` | `FedSrv` (but its resources and `main()`), `Inc/messagesLS.h` (and the SQL layer, since deleted) |
| `GameApp` | client | `App`, `ClientLib`, `Training` | `WinTrek` (but its resources and `main()`), `clintlib`, `training`, `Inc/messagesLC.h`, `quickchat.h` |

Everything is under `Source/<Library>/`; the executables are `Source/Client` (`Allegiance.exe`: `main.cpp`,
the resource scripts, the icon, what was `Lang/Usa/allegiance`) and `Source/Server` (`AllSrv32.exe`). The
tests are `Tests/<Library>.Tests`, the art tools and CI scripts `Tools/` (`mdlc`, `cvh`, `xmunge`, `qhull`,
`ci`, `art`). There is no shared `Inc` folder.

**Layering.** NeuronCore depends on nothing; NeuronClient and NeuronServer on NeuronCore; GameCommon on
NeuronCore; GameApp on GameCommon and NeuronClient; GameLogic on GameCommon and NeuronServer. It is
enforced by the include directories: a project lists its own folders, then those of the libraries it
may use, and nothing else (`msbuild/Allegiance.Cpp.props` has no shared include directories), so a
library cannot include a header above its layer. Two compromises remain. `Utility/utility.h` carries
some game constants. Effect, the game's visual effects, depends only on the engine and is part of
NeuronClient, because mdlc needs it to compile the art.

**Session kinds.** The DirectPlay application GUIDs (`guids.h`) named the kind of session a host offers,
and NeuronCore mapped them to ports. They are now `FedApplication`s, a 32-bit ID and a port, which the
game defines in `GameCommon/Protocol/sessions.h` (`c_appGameServer`, `c_appGameServerPrivate`,
`c_appLobbyClients`, `c_appLobbyServers`) and passes to `HostSession`, `JoinSession` and `EnumSessions`.
The hello and discovery frames carry the ID, so the transport's protocol version is 2: a build from before
is refused with "The server runs a different version of Allegiance". Four of the GUIDs had no users. The
rest of `guids.cpp` instantiated OLE DB's GUIDs for the server's SQL layer (`allegdb`, `srvqueries.h`),
which no longer reached a database: the logon used one query class as a data holder, now the plain
`LogonDetails` (`FedSrv.h`), and the rest went with it, with the two database error events.

**ATL removed.** The server reached ATL and the 1999 TCLib and TCAtl libraries built on it (about 10,900
lines) for two things: `TCUserAccount`, with which `-install user password` gave the account the right
to log on as a service, and one `TCAutoCriticalSection`. The first is now `Service/accountrights.*` in
NeuronServer (`LookupAccountName` and LSA), the second NeuronCore's `ZAutoCriticalSection`; TCLib, TCAtl,
the ATL includes and their `new`-macro workarounds, and the `statreg.h` of the COM registration are
gone, so building needs neither the ATL nor the MFC component. The rest of the server's way of running
(install and remove, the service control dispatcher, the console's Q/P/C, `-install`, `-remove` and
`-service`) moved from `FedSrv/allsrvmodule.cpp` to `Service/servicehost.*` in NeuronServer, behind
`IServiceApp`, which the game server implements (`allsrvmodule.*`). One change of behaviour:
`-install <user>` without a password is now a usage error; it used to install the service as
LocalSystem and ignore the account.

**Precompiled headers.** One per library, `Source/<Library>/pch.h`, the union of what its sources'
precompiled headers held before (GameApp's is WinTrek's plus what ClientLib and Training added).
GameLogic's is `GameLogic.h`, which the server's `main.cpp` includes too. Compiled without it:
`fedtransport.cpp` (msquic.h wants Winsock 2 before `windows.h`), `x3dpan.cpp` (X3DAudio's Direct3D
types) and the executables' `main.cpp`.

**Linking.** The client's application object (`g_trekImpl`, `GameApp/App/trekapp.cpp`) registers
itself when it is constructed and nothing refers to it, so the linker would leave it out of a static
library. The executables link their game library with `/WHOLEARCHIVE`, which keeps every object, as
when those sources were the executable's own.

**Headers.** Deleted: 29 headers that no source reaches through `#include` (most of TCLib's unused
templates, the engine's `stdafx.h`, `xfile.h`, `panewindow.h` and a second `framedata.h`, WinTrek's
`TrekDlgs.h`, `hudimage.h`, `offline.h`, `sectorvw.h`, `speech.h`, effect's `debrisgeo.h`), the unused
`CBitArray`, and 20 `#include` lines of headers the source's precompiled header already includes (a
source's own header stays). The old projects' `WINTREK`, `FEDSRV`, `IGC_SHIP`, `igc_static`, `_USRDLL`,
`DLL` and `_CONSOLE` defines are gone; nothing tested them except `VerRes.rc`, which marked
`Allegiance.exe` as a DLL because of `DLL`.

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
- AGC's event definitions were generated from `AGC/AGCEvents.xml` by stylesheets in Microsoft's
  pre-standard `WD-xsl` dialect with embedded JScript. Only MSXML 3 runs those, so the build kept
  the original `cscript XMLXForm.js` step (`AllegianceXslt`). Both went with AGC; the definitions are
  now `FedSrv/serverevents_defs.h`.
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

### Phase 1: MSBuild skeleton (done)

- `Directory.Build.props`: toolset v145, Windows SDK 10.0, MultiByte character set.
- `msbuild/Allegiance.Cpp.props`: shared compiler settings (static CRT, C++20 since Phase 5, C++17 before, `/EHsc`, `/W3`, shared
  include order taken from the original `makefile.inc`). Output goes to `out/`, which git ignores.
- `Allegiance.slnx` with `.vcxproj` files for `zlib`, `guids`, `_Utility`, `engine` and `effect`. Source
  lists are taken from the original makefiles; files the original build excluded (`dcbase.cpp`,
  `dialog.cpp`, `dcdevice3d.cpp`, `efsound.cpp`, `panes.cpp`, `engine/scan`, `engine/exec`) stay
  excluded.
- Projects for all remaining Tier 1 modules (except `xmunge`, see above), generated from the
  original makefiles' source lists, include paths and defines.
- `Directory.Build.targets`: `AllegianceMessage` (`mc.exe` for `.msg`/`.mc`), and `AllegianceXslt`
  (AGC's XML to IDL/MC/RC2/headers) until AGC was removed. MIDL uses MSBuild's built-in `Midl` items.
- Generated files shared between projects go to `out/gen/<Platform>/<Configuration>/<module>/`.
  `AGCGuid` (the original `AGCGuid.lib`) owned AGC's code generation, so it built before every
  project that included the AGC headers (removed with AGC).
- Resource scripts: the makefiles concatenated module `.rc` + generated message `.rc` +
  `#define FLAVOR` + `Inc/VerRes.rc` with `copy`/`type`. Each executable now has a small
  `*.build.rc` that `#include`s the same pieces.
- `_WIN32_WINNT`/`WINVER` are set to Windows 10 globally; current ATL refuses the original 0x0400.
- No package manager for third-party code: the vendored libjpeg went with the dead code cleanup (nothing
  built used it), and qhull stays vendored: `cvh` needs only the qhull executable, and the art pipeline no
  longer needs meshoptimizer. MsQuic is the one NuGet package.

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
  `ZoneClubScreen.cpp`): config download and message of the day, then the free lobby (the auto-update step
  it also ran was removed later, see "Dead code cleanup"). The
  logon popup asks only for a call sign. Main menu: LAN, Internet and Training.

  Protocol: `MSGVER` is 199 (Zone ticket out of `FM_C_LOGONREQ`, `bClubGame` out of `MissionParams`) and
  `LOBBYVER` is 9 (ticket out of `FM_C_LOGON_LOBBY`). Old and new clients, servers and lobbies do not
  interoperate, and an old client can't even reach a new lobby's auto-update: it sends the old
  `FM_C_LOGON_LOBBY` layout, which a new lobby misreads (`FM_C_LOGON_LOBBY_OLD` covers only much older
  clients). That is acceptable because nothing speaking the 2004 protocol is left to stay compatible with:
  the Zone lobbies are gone. (Removing auto-update later took `MSGVER` to 200 and `LOBBYVER` to 10.)

  If characters and persistent multiplayer come back, the removed screens are a starting point: the last
  commit that has them is `84f18ae` (`git checkout 84f18ae -- WinTrek/CharInfoScreen.cpp ...`, plus their
  MDL and bitmaps from `Artwork/256`). They need a new server behind them; the Club server ran on SQL
  Server and Zone authentication.

  The parked Tier 2 code (`lobby`, `cliconfig`, and the pigs under `test/`) still has Zone references and
  is not built; `lobby/pch.h` includes the deleted `zauth.h`.
- `/permissive-` turned on (`ConformanceMode` in `msbuild/Allegiance.Cpp.props`); the fixes are in the
  conformance fix log below.
- C++20 (`/std:c++20`, Phase 5); its fixes are in the same log.
- The old build system is deleted (190 files): the nmake makefiles, `makefile.inc`/`makeexe.inc`/`makedep.inc`,
  the VC6 `.dsp`/`.dsw` and VS.NET `.vcproj`/`.sln` files, `VSNet.Readme`, and `build/`, the 1999 build-lab
  scripts (build numbering, BVT scripts, setup packaging, all hard-wired to `%FEDROOT%`). Git history keeps
  them for anyone reviving a Tier 2 module.

### Phase 3: make it run

| Subsystem | Today | Target |
|---|---|---|
| Rendering | DirectDraw 4 + Direct3D 3 (DirectX 6), software rasterizer | D3D11 backend. Stage 1 (below): a D3D11 rasterizer with the engine's CPU transform and lighting. Stage 2: transform and lighting in shaders. Stage 3: HDR, MSAA, bloom, per-pixel lighting and native widescreen |
| Input | DirectInput 2/7 | DirectInput 8 |
| Audio | DirectSound / DS3D | Done: XAudio2 2.9 (in Windows 10 and later) with X3DAudio for the pan and doppler; on headphones a parametric head model, plus a distance low-pass and station reverb (see "Sound: 3D audio for headphones" below) |
| Music | Audio-CD tracks through MCI (`redbooksound.cpp`) | Done: the same `IDiskPlayer` interface plays the WAVs from `Artwork/Music` (copied to `out\Artwork\Music` by `Art.proj`; CD track n is line n - 1 of `tracks.lst`) through the Media Foundation Media Engine, audio only. The music gain sets the player's volume instead of the system mixer's CD line |
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
| Surfaces | System memory, or DirectDraw "video" surfaces; locks and conversions between them | Always system memory (565 then; 32-bit BGRA since "Soft alpha"). Read-only MDL mappings are copied on first write |
| 3D | `D3DRasterizer` (HAL) or `SoftwareRasterizer`, per surface | `d3d11raster.cpp` for every 3D surface. At `BeginScene` the CPU pixels are uploaded if they changed; at `EndScene` the result is read back if anything was drawn. The Context already ends the scene before each 2D operation |
| State | DirectX 6 render states | HLSL in `engine/shaders`, compiled by `FxCompile` in CI. Covers DECAL/MODULATE, flat/Gouraud, colour-key discard, the three blend modes, z test/write, cull, scissor, and a Z clear limited to the clip rect. Vertices are shifted half a pixel to keep DirectX 6 pixel centres |
| Textures | DirectDraw texture surfaces, re-`Load`ed when changed | `D3D11Device` cache: BGRA8 with the colour key in alpha, refreshed when the surface's change counter moves, full size |
| Presentation | `Flip` (fullscreen) or `Blt` to the primary surface through a clipper (windowed) | A flip-model swap chain per window. The frame surface is drawn scaled and letterboxed into the client area (1:1 windowed), with vsync |
| Fullscreen | `SetDisplayMode` at 16 bpp in exclusive mode | A borderless topmost window on the monitor. The fullscreen size picks the render resolution from the original mode list, limited to the monitor |
| Gamma | DirectDraw gamma ramp | The same curve in the presentation pixel shader |
| Device loss | `TestCooperativeLevel`, full rebuild | `DXGI_ERROR_DEVICE_REMOVED`/`RESET` re-creates the device. Nothing is lost, because surfaces live in system memory |
| Tools | `mdlc` initialised DirectDraw with `-software` | The engine creates the D3D11 device only on first use, so `mdlc` needs no GPU. BMP import uses `GetDIBits`. The 1,405 24-bit sources get identical pixels. The 37 16-bit (555) sources are read at their own depth, so the 565 conversion is unchanged. The 7 8-bit sources now come out in their palette colours; the old conversion ignored palettes (zero colour masks), so they almost certainly came out black |
| Movies | DirectShow `IAMMultiMediaStream` into a DirectDraw surface | Media Foundation (later; see "Movies" below): a source reader decodes RGB32 frames into a system-memory surface, and an audio-only Media Engine plays the sound and serves as the clock |

Deleted:

- `ddsurface.cpp`, `dddevice.cpp`, `d3ddevice.cpp`, `raster.cpp`, `softraster.cpp` and `palette.cpp`.
- The unbuilt `fliphelp.*`, `ddstruct.*`, `dcdevice3d.cpp`, `engine/scan` and `engine/exec`.

`ddraw.lib` is no longer linked. `zlib.h` includes only `d3dtypes.h`, for the vertex and colour structs that are still used everywhere.

Known gaps to check when testing:

- The 2D/3D synchronisation costs a full-frame upload and readback around each 3D layer. At 800x600 this should be a few milliseconds, but it is unmeasured. Stage 2 removes it (below).
- Pixel-exactness against the DirectX 6 output is unverified (half-pixel rule, colour-key edges with linear filtering).
- Movies are skipped (played again since; see "Movies").

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
| Surface storage | System memory only; the rasterizer kept a GPU copy for the length of a scene | A surface that the GPU has rendered into keeps a GPU copy (BGRA8 render target and shader resource, plus a z-buffer on demand) next to its CPU pixels. Two flags say which copy is current |
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

#### Rendering, stage 3: modern look and native widescreen

Decisions:

- **Direction.** A visibly modernised look, not a faithful one, and one code path: no switches and no "classic" preset.
- **Widescreen.** The combat view renders at the monitor's native size. The camera is Hor+, widening horizontally up to 21:9 and pillarboxed beyond that. The 800x600 menu screens are scaled to the screen height and pillarboxed.
- **Verification.** CI builds. The visual changes were tested together, by hand, at the end.

| Piece | Stage 2 | Stage 3 |
|---|---|---|
| Fullscreen size | The 4:3 mode list (640x480 to 1600x1200) that fits the monitor; 800x600 by default | The same list plus the monitor's native size, cut to 21:9 if it is wider (the presenter pillarboxes it); with no saved preference the combat view uses the native size (`CombatFullscreenXSize`/`YSize` 0) |
| Field of view | Horizontal FOV, so a wider view cut the top and bottom | The game's FOV values stay those of a 4:3 view. A wider view keeps that view's vertical field and widens horizontally up to 21:9 (`HorPlusFOV` in `WinTrek.cpp`); the blip angle-to-pixel factor uses the widened field. The HUD needs no change: `dialog.mdl` anchors its dialogs to the screen sides |
| Presentation | Bilinear scaling to fit, vsync, DXGI's default frame queue | Sharp bilinear: whole-number scaling with square pixels, filtering only the rest, so 800x600 screens stay crisp when pillarboxed. A frame-latency waitable swap chain with a maximum latency of 1; the presenter waits on it after each present |
| Texture filtering | Bilinear with the baked mip chains | 16x anisotropic |
| 3D layers | Drawn straight into the surface's BGRA8 GPU copy, in gamma space | A 3D layer with a z-buffer (`Context::Begin3DLayer`/`End3DLayer`, passed down through `Device3D` to the rasterizer) draws into a scene target: RGBA16F in linear light with a D32F depth buffer, 4x MSAA on hardware with at least 1 GB of video memory (one sample otherwise, and on WARP). At the end of the layer the scene is resolved, bloomed, tonemapped, gamma-encoded and blended over the surface (premultiplied, through the clip rects the layer drew in). Scene targets are pooled by size. Layers without a z-buffer still draw directly |
| Depth | D24S8, conventional (0 near, 1 far), less-equal | In a scene, D32F with reversed depth (1 near, 0 far), greater-equal, cleared to 0, which spreads the float's precision evenly over distance instead of spending it near the camera. Model-space geometry gets its reversed z straight from the model-space position: `Device3D` builds that row (`m_zrev`) in double from the perspective's constants and the full matrix's w row, because computing w - z per vertex would cancel at the far plane and lose the gain. Screen vertices, whose depth the CPU computes conventionally, are flipped (1 - z) in the vertex shader and keep their own precision. Layers without a scene keep D24S8 and conventional depth |
| Colour space | Art and colours used as they are | In a scene, textures, vertex colours and authored colours (global colour, material diffuse, light colours) are decoded from gamma 2.2; light intensities (the directional term, ambient) are linear. The composite encodes with 1/2.2 |
| Coverage | Additive draws accumulated alpha | In a scene, `BlendModeAdd` leaves alpha alone: light in front of nothing stays see-through and is added to what is behind, as when it was drawn straight into the frame |
| Tonemap | None (values clipped at 1) | Unchanged up to a knee of 0.9, then a smooth roll-off towards white. White art under full light comes out at 0.95 in linear light (249 of 255 encoded) |
| Bloom | None | A dual-filter chain of half-size levels (down to about 16 pixels) on the resolved scene: the first downsample keeps what is over 0.9 (soft knee 0.5), the upsamples add each level into the one above, and the composite adds the top level at a strength of 0.35. Lit hulls stay under the threshold; stacked additive light (engines, weapons, explosions, alephs) glows |
| Lighting | Per vertex (Gouraud), in `tl.hlsli` | Per pixel for `Vertex` geometry (`tl_ps.hlsl`): the same directional, bidirectional and material formulas for the interpolated normal, plus, in a scene, a Blinn-Phong highlight (power 24, strength 0.35) in the light's colour, faded out towards the terminator. `Device3D` adds the model-space eye position to the transform-and-light constants. Flat shading uses the first vertex's normal. `VertexL` geometry keeps its vertex colours |
| Overlay | Uploads, readbacks, texture refreshes, 2D and 3D draws | Also the scene targets composited per frame |

The constants (knee, bloom threshold and strength, specular power and strength, the MSAA memory floor) are
at the top of `engine/d3d11raster.cpp` and `engine/shaders/tl_ps.hlsl`, chosen without seeing the result,
and are the first thing to tune.

How it was checked: CI builds it and runs the unit tests and the art build, which exercise none of it. Locally, clang
syntax-checked the engine against the mingw-w64 Direct3D 11 headers, and glslang parsed every shader as HLSL;
neither is FXC or MSVC.

To check when testing:

- Combat at native size on 16:9 and 21:9 (and 32:9, which should be pillarboxed to 21:9): the HUD at the
  screen edges, the target blips and the round radar, mouse picking, zoom (the FOV keys), the command view.
- Menu screens: pillarboxed and sharp at 1080p, 1440p and 4K.
- The look: hull shading and highlights, engine and weapon glow, explosions, alephs, cloaked ships
  (blended draws), the loadout and hangar ship previews (panes keyed on black), the target display.
- MSAA edges on hulls and on colour-keyed cut-outs. Alpha-to-coverage is not used, so cut-outs stay aliased.
- Frame pacing: input lag and smoothness, and that vsync still holds the frame rate.
- Device reset (for example a driver update or switching GPU) during combat.
- Depth: distant stations, asteroids and ships against each other (no z-fighting), and decals, particles
  and engine trails (screen vertices, flipped depth) against hulls; nothing should disappear or draw through.

Known gaps:

- **HUD scale.** The HUD and fonts are drawn at their pixel size, so at 4K they are small. Scaling the UI is a separate change.
- **Competitive balance.** Hor+ shows 21:9 players more of the sector than 4:3 players.
- **Not done.** Tearing and VRR without vsync, and specular or emissive maps (the art has none).

#### Sound: 3D audio for headphones

The XAudio2 port kept DirectSound 3D's model: X3DAudio pans each positioned sound over the output speakers
and gives its doppler, and DirectSound's distance and cone attenuation (`soundengine/soundmath.h`) scale it.
On 5.1 and 7.1 speakers that already places sounds in front and behind. On headphones, the usual way to play,
it only pans left and right: a ship behind sounded like one in front, and above like below.

Decisions:

- **Output.** Headphones, and Windows Spatial Sound: with Windows Sonic for Headphones, Dolby Atmos or DTS:X turned on, positioned sounds become its dynamic audio objects (`ISpatialAudioClient`), with height. Otherwise a stereo output gets the head model below.
- **No measured HRTF.** The game ships no third-party HRTF data. The one set reachable when this was written (MIT KEMAR, from libmysofa) carries "No license provided" in its SOFA metadata. A parametric head model replaces it.
- **Extras.** A distance low-pass, and reverb while docked. No occlusion.
- **Verification.** Unit tests for the model and the distance curve. Listening by hand at the end.

| Piece | Before | Now |
|---|---|---|
| Stereo output (headphones) | X3DAudio's stereo pan (left-right only) | Each 3D voice runs the head model (`soundengine/spatializer.h`) as an XAudio2 effect (`xa2spatial.cpp`, a plain `IXAPO` without `xapobase.lib`). It takes the voice's one or two channels and gives two, from the sound's direction in the listener's frame (the camera), which `XA2Voice::ApplyOutput` sets every update. The output matrix then only attenuates. Unpositioned sounds pass through: mono to both ears at full level, stereo as it is |
| The head model | | Woodworth's interaural time difference for an 8.75 cm sphere (up to 0.66 ms); Brown and Duda's head-shadow filter (one pole, one zero; up to +6 dB at high frequencies facing the ear, down to -20 dB on the far side); Brown and Duda's five pinna echoes for elevation, at half their strength (their full comb colours the sound) and summing to unit gain at low frequencies; and a high shelf at 4 kHz that dims sounds behind the listener by up to 6 dB, the strongest front-back cue a model without a measured pinna has. Straight ahead, low frequencies come out at X3DAudio's centre-pan level (-3 dB per ear). Parameters move linearly across each block, so a moving source doesn't click |
| Windows Spatial Sound on | X3DAudio's 7.1 pan into the virtual 7.1 device | Each positioned sound is a dynamic audio object (`soundengine/xa2objects.cpp`). XAudio2 still decodes, loops, pitches, dopplers and low-passes it: the voice's effect, the last step before the volume, hands the result to the object through a lock-free ring (`spatialstream.h`), with the sound's direction (Windows' frame, +z behind, at a meter) and its gain (the voice's volume and attenuation, as Windows doesn't attenuate). The voice's master output stays silent; its reverb send doesn't. A render thread in the multithreaded apartment owns everything that touches `ISpatialAudioClient`: activation on the default endpoint, a mono float object format, up to 32 dynamic objects, and each pass's buffers, positions and volumes. The mastering voice runs at the objects' rate. The ring keeps two to six object buffers in hand (10 to 60 ms), dropping the oldest beyond six |
| Spatial sound fallback | | A voice asks for an object the first time it is positioned and keeps its slot until it is destroyed (after `DestroyVoice`, so no effect still writes to a slot another voice reuses). A sound Windows has no object for (other applications may hold them), or every sound once the stream stops (the endpoint changes, spatial sound is turned off), plays the other ways: the head model on stereo, X3DAudio's pan otherwise. `"WindowsSpatialSound": 0` turns it off. Unpositioned sounds always stay in XAudio2's mix |
| More channels | X3DAudio's pan | Unchanged for 5.1 and 7.1 speakers without spatial sound |
| Stereo speakers | | The head model's crosstalk is wrong on speakers. `"HeadphoneSpatialization": 0` in `settings.json` goes back to X3DAudio's stereo pan |
| Distance | Attenuation only | Also XAudio2's low-pass (state-variable, Butterworth) on 3D voices: open up to the sound's minimum distance, then 20 kHz times (minimum / distance)^0.6 (about 5 kHz at ten times it), never below 2 kHz (`DistanceCutoff`) |
| Stations | Dry | Every voice also sends to a reverb submix voice (XAudio2's reverb, the I3DL2 "large room" preset, fully wet), at its attenuated level. `ISoundEngine::SetReverb` sets the submix level, which fades over about a second; the client sets 0.35 while the ship is docked and 0 in space |

The constants (pinna strength, rear cut, output gain, the distance curve, the reverb preset and level) are
at the top of `spatializer.h` and `soundmath.h`, and in `xa2reverb.cpp` and `WinTrek.cpp`, chosen without
listening, and are the first thing to tune.

How it was checked: CI builds it and runs the unit tests (`HeadModelTests.cpp`: the time difference, the
level difference rising with frequency, the mirror image, the centre-pan level, front-back, elevation, the
pass-through, a moving source, the tail; `SpatialStreamTests.cpp`: the ring's margin, running dry, falling
behind, the mono mix, the pose, and a writer and a reader on two threads; `SoundTests.cpp`: the distance
curve). They were also run natively on Linux, the ring's under ThreadSanitizer. The XAPO, the reverb and the
spatial sound renderer were checked against the mingw-w64 headers where they have the types, not by running
them.

To check when listening, on headphones:

- A ship circling you: it should move around your head, with behind duller than in front, not jump from side to side.
- Above and below (for example in the command view, or climbing past a station): a difference, if subtle. Elevation is the model's weakest cue.
- No clicks or zipper noise when sounds or the camera move fast, and no change in loudness overall compared with before.
- Distant explosions and engines duller than near ones.
- Docking: the hangar and station sounds gain a room, and launching returns to dry space.
- With `"HeadphoneSpatialization": 0`: the old stereo pan.

With Windows Sonic for Headphones (or Atmos or DTS:X) turned on:

- The debug log should report "Windows Spatial Sound: n dynamic objects at 48000 Hz" and "XAudio2: ... Windows Spatial Sound".
- Sounds around and above and below you, with a clearer height than the head model; no gaps or crackle when many sounds play (a sound Windows has no object for falls back to the pan, which may sound different).
- Latency: weapons fire against the visual, which should feel as before (the hand-over adds 10 to 60 ms).
- Turning spatial sound off, or unplugging the headphones, mid-game: the sounds keep playing, through the head model or the pan (sounds started before the change keep their voice's way until they end).
- With `"WindowsSpatialSound": 0`: the head model or the pan, as without spatial sound.

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
| `Open(DPOPEN_JOIN)`, `CreatePlayer` | `JoinSession`: connect, open the stream, send a hello (application ID, protocol version, player name); the server answers with a welcome (session instance) or a refusal. The call waits for that, as the DirectPlay join did |
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
server side 47720/47721 (`GameCommon/Protocol/sessions.h`). A server address may name another port as `host:port`.

The LAN dialog's server box (in the original, commented out) is back: type a host, `host:port` or an
address and press Find or Enter, and the dialog asks that server instead of broadcasting, which reaches
servers beyond the LAN; empty asks the LAN again. The list is shorter to make room for it.

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
  "SquadMemberships": { "Pilot": [12, 40] }
}
```

Numbers stand in for `REG_DWORD`, so flags stay 0 or 1. `true`/`false` and numeric strings (`"1"`, `"0x10"`)
also read as numbers, so hand edits are forgiving. Strings are UTF-8 in the file and the ANSI code page in
the game, as before. The squad memberships per character were a `REG_BINARY` array of IDs and are now a
JSON array. (The AGC event logger kept its configuration in an `EventLog` section until AGC was removed;
see "AGC removed".)

`zlib/zsettings.h` is the interface: `ZSettingsFile` for one file, and the `ZSettings` functions for the
process's own file. It is a small reader and writer (strict RFC 8259 on input, a UTF-8 byte order mark
allowed), without ZString or the rest of zlib, so any module can use it. Every write re-reads the file, applies the
change, and writes it back to a temporary file that is then renamed over the original, under a named mutex
per file. Two processes, or two objects in one process, writing the same file don't lose each other's changes,
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

What stays in the registry is Windows' own: the service entry, if the server is installed as a service.
The server's own COM registration, its event-log source and the performance counters went with the DCOM
admin layer, and AGC's COM registration with AGC (see "Server administration" and "AGC removed" below).

The parked Tier 2 tools `cliconfig`, `srvconfig` and `AllSrvUI` exist to edit the old registry values.
Nothing reads those values any more; reviving one of these tools means porting it to `zsettings.h`.

#### Server administration: the DCOM admin layer is gone

AllSrv was also a DCOM server. It registered `IAdminSession`, `IAdminServer`, `IAdminGames`,
`IAdminGame`, `IAdminUser(s)` and `IAdminShip` as a local server, with `CoInitializeSecurity` set to a
NULL DACL (everyone may connect) at packet-level authentication, and pushed AGC events to connected
sessions. `AllSrvUI` (Tier 2, not built) was the admin tool on top of it. Nothing outside this repository
uses the interfaces, and the only user inside it was the client's training mission 7. The layer is
removed: the `Admin*` objects, `AllSrvModuleIDL.idl` and its type library, the `.rgs` registration
scripts, `-RegServer`/`-UnregServer`/`-reregister`/`-Embedding`, the MTA keep-alive thread that held the
class objects, and the NULL-DACL `CoInitializeSecurity`. A later admin protocol will run on the MsQuic
listener; until then a server is configured through `server.json`.

How the server runs now:

| Command | Does |
|---|---|
| `AllSrv32` | Runs in the console. `Q` quits, `P`/`C` pause and continue, as before |
| `AllSrv32 -install [user password]` | Installs the Windows service `AllSrv32` (automatic start) with `-service` on its command line; administrator rights needed |
| `AllSrv32 -remove` | Removes the service (it must be stopped) |
| `AllSrv32 -service` | What the service control manager runs; not for interactive use |
| `AllSrv32 -training <civ> -stopevent <handle>` | The client's training server (below) |

Hosting changed with it, because games used to come from the admin objects. Without an admin tool
the console server hosted the public session and tried the long-dead Zone lobby, the client's LAN
browser looks only for the LAN session (`FEDSRV_STANDALONE_PRIVATE_GUID`, now `c_appGameServerPrivate`), and the `Game1`, `Game2`, ...
games from `server.json` were only created after a lobby connection succeeded. Now the server hosts the
LAN session and creates the `server.json` games at startup; `"PublicLobby": 1` restores the public
session and lobby. Games from `server.json` are never deleted: after a round they go back to "not
started" (unless `bAllowRestart` is off) and wait for players, so a dedicated server keeps running. The
code that shut a standalone server down when its last game was deleted now only stops a training server;
in practice it never fired, because the only games ever deleted were lobby-created ones that nobody joined,
and only the database server (never built, and removed in the dead code cleanup) took those.

Training mission 7 ("live" training against the Bios) used the admin objects to have COM start a local
AllSrv32, create the game, start it once the player had joined, and kill it afterwards. The client now
starts `AllSrv32.exe` from its own folder itself, as a hidden child process in a job object (so it dies
with the client), with `-training <civ> -stopevent <handle>`. The server hosts the LAN session with that
one game (`Inc/trainingserver.h` names it), starts it when the player has joined, and shuts down when the
client sets the inherited event (the client does that when training ends, and the job object covers a
client that crashes). If the server exits before it can be joined, for
example because another server holds the port, the client says so. The rest of the flow (finding the
server on 127.0.0.1, joining as "Cadet") is unchanged.

The Windows performance counters (`sharemem`, `Inc/sharemem.h`, the `AllSrv` counter object) are deleted.
The server still keeps the same statistics in memory (`g.serverCounters`, `FedSrv/counters.h`), for the
admin protocol to report later.

AGC, still an in-process COM server at this point, was removed next (below).

#### AGC removed: server events go to a log file

AGC (Active Game Core, about 17,000 lines plus 3,300 lines of IDL) was a COM object model over the game
(games, ships, stations, sectors, teams and their collections), an event system, and an event logger
that wrote events to the Windows event log and to a database. With the admin layer gone, the only
part anything used was the event system: the server raised about 40 kinds of event (start and stop,
lobby connection, logins, chat, kills, hack detection, errors), and the logger was the only listener.
Nothing in the client used AGC. AGC registered itself as a COM server on first use (writing HKCR, so
the server needed administrator rights once).

All of `AGC/`, its two projects (`AGC`, `AGCGuid`), `Inc/AGCIDL.idl`, the XSLT build step and the
server's COM start-up (`CoInitialize`, the ATL module) are gone. In their place:

| Piece | Now |
|---|---|
| Event definitions | `FedSrv/serverevents_defs.h`: the IDs, names, severities and message formats, converted once from `AGCEvents.xml` (64 events; the pigs' were dropped). Edited directly from now on |
| Raising an event | `g_events.TriggerEvent` / `TriggerContextEvent` (`FedSrv/serverevents.h`), with the same arguments as before: (name, `VT_` type, value) triplets. The values are taken by their C++ type through a variadic template, so a wrong `VT_` tag can no longer misread the argument list |
| Output | `logs\server-<date>.log` next to the settings file (`%ProgramData%\Allegiance\logs`, or next to the executable for a portable install), one line per event: time, severity, name, context, message, in UTF-8. Warnings and errors also go to the console |
| Which events | The 44 that went to the Windows event log by default are logged; `"LogAllEvents": 1` in `server.json` logs all of them, which adds chat, kills, sector changes and logins. The per-event enable ranges and their `EventLog` section are gone |
| Errors | An event of severity Error still ends the process, as AGC did (in a debug build it first breaks into an attached debugger if `fWantInt3` is on) |
| Database logging | Dropped (it needed an OLE DB connection string that nothing set) |
| Ship events | `CAGCSite` became `FedSrv/eventsite.h`; the "ship killed" positions are text instead of `AGCVector` objects |
| Game parameters | `gameparamdata.h` moved to `FedSrv/` |

ATL was still included for a few helpers (`CComBSTR`, `test/TCLib`, `test/TCAtl`), with no ATL module,
COM server, COM class or COM registration of the repository's own left in the built code (the client
still uses DirectX's COM interfaces, which need none of that). Since the restructure ATL is gone
altogether; see "Source layout", "ATL removed".

#### Movies

The movies (`msr_games`, `logo1`, `logo2`; 320x240 at 15 fps, 6 to 13 seconds, 8-bit or 16-bit PCM sound)
were Indeo 4 and Indeo 5 AVIs, which Windows 10 and 11 can no longer decode (the Indeo codecs were removed
from Windows). They were converted once, with ffmpeg 6.1 (`libx264 -profile:v main -crf 18`, AAC 128 kb/s),
to MP4 files that Media Foundation plays, about a third of the size; the AVIs were deleted.
`engine/videoimage.cpp` decodes the video with a source reader into an engine surface and draws it scaled
to fit the screen, keeping the aspect ratio; the sound plays through an audio-only Media Engine whose
position the frames follow. A key press still skips a movie. `intro.avi`, the intro movie that played
after them, was on the game CD and is not in the repository; the client no longer looks for it (nor for
a CD: the `CDPath` setting is gone), and the intro screen's "Intro" button, which replayed it, is
removed, with its image.

#### Dead code cleanup

Before the x64 and `/permissive-` work, so those ports only touch code that is used:

| What | How |
|---|---|
| Directories no project builds and nothing in the build includes (about 100,000 lines): the Tier 2 tools and `lobby`; `cadet`, `drones`, `edit`, `engineapp`, `engineapptemplate`, `fedsrvdll`, `jpeg`, `mdl`, `mdledit`, `me`, `mesh2pl`, `offline`, `qh2mesh`, `shadowswitcher`, `soundtest`, `techtree`, `AutoUpdate`, `Reloader`, `test/TCDeploy`, `test/TCObj`, `test/TCGuid`; `SrvHost` (a VB6 front end for the DCOM server), `Setup` (1999 redistributables and Zone DLLs), `AllGuard` | Deleted. What "included" means was worked out by resolving every `#include` through each project's own include directories, not by file name: the build reads only `lobby`'s three message headers (moved to `Inc/`) and `offline/mission.h`, which was entirely `#if 0` |
| Source files in built directories that no project compiles and nothing includes (63), e.g. most of `test/TCAtl`, the client's old offline mode, Dreamcast's `zlib/dcbase.cpp` | Deleted |
| The database-backed Zone server compiled out of `AllSrv32` (`!ALLSRV_STANDALONE`: ODBC queries, DB logging, lobby-created games, IGC files built from the database) | Removed with `unifdef -DALLSRV_STANDALONE`; the define is gone. The AllegDB query classes stay, as data holders the standalone server uses (later replaced by `LogonDetails` and deleted; see "Source layout") |
| `#if 0` blocks and the Dreamcast port's branches in built code | Removed with `unifdef -k -UDREAMCAST`; the vendored qhull is left as it is |
| The auto-update system, which needed the deleted `AutoUpdate.exe` and `Reloader.exe` and a lobby that sends `FM_L_AUTO_UPDATE_INFO`: the client's FTP art downloader (`clintlib/AutoDownload.*`), its progress popup and dialog art (`AutoDownloadDialog.mdl` and three textures), the lobby screen's `FilelistCRC`/`FilelistSize`/`FilelistSite`/`FilelistDirectory` check, the start-up `MoveInProgress` check and the `-checkfiles`, `-noautoupdate` and `-reloaded` switches; the server's `AutoUpdate.exe` download and launch from the lobby config (`FedSrv.CPP`) and the never-raised `CantWatchArt` event; the FTP session in `zlib/FTPSession.*` (the HTTP session stays, for the config file and the message of the day) | Deleted. `crcFileList` left `FM_C_LOGONREQ` (`MSGVER` 200) and `FM_C_LOGON_LOBBY`, and `FM_L_AUTO_UPDATE_INFO` and `FM_C_LOGON_LOBBY_OLD` were deleted (`LOBBYVER` 10). The version-mismatch messages no longer tell players to auto-update or delete `filelist.txt` |
| Leftover non-source files nothing builds, includes or reads (a second pass, resolving `#include`s and `.rc` file references per project as above): the nmake `makeexes`; `zlib/doit.bat` (a hand-run `cl` line list) and `zlib/float/math.asm` (x86 assembly no project assembles); `Inc/Utility.hxx` (a forwarder `_Utility/Utility.h` never reaches, since it finds its own copy first); the old client resource script `Lang/Usa/WinTrek/WinTrek.rc` and its icon (the client builds `Lang/Usa/allegiance/allegiance.rc`); `FedSrv/Status.htm` (a FrontPage status page scripting the removed DCOM admin objects) and `FedSrv/regini.txt` (registry keys for the 1999 server setup); `WinTrek/offline.map` and `offline.sql` (data for the deleted offline mode); the Source Insight project files `training/training.CF/PI/PR/PS/WK` | Deleted |

`database/` and `databasepcg/` stayed at first, as the game-balance data (ships, parts, civilisations)
the static core `.igc` files were generated from. They were deleted later with the 1999 training
mission specifications (`training/specification`, `training/spec.html`): nothing builds or reads any
of them, the `.igc` files in `Artwork` are the game's source of truth, and the data was a SQL Server
bulk-copy export (with `bcp.exe` and `osql.exe` bundled) for the Zone database the server no longer has.
They remain in the git history.

#### x64

The game is 64-bit only. x64 was first added next to Win32 (both built in CI); Win32 was dropped in a
second change, which also removed the x87-only code (`_controlfp`, the server's `fDoublePrecision`
setting) and switched the art tools, CI and `Art.proj` to `out\bin\x64\`. The two builds could play
together: the network messages, the static core (`.igc`) structures and the binary MDL format hold no
pointers or pointer-sized fields, so their layout is the same on both.

| What | Change |
|---|---|
| Inline assembly (MSVC x64 has none) | Replaced with C++ for both platforms: the 1-bit font blit (`engine/font.cpp`), the colour-keyed and plain 16-bit blits (`engine/surface.cpp`), `update_crc` (`_Utility/CRC.cpp`), `__debugbreak()` for `int 3`. Where a C version was already there (`MakeD3DCOLOR`, the CPU transform-and-light path in `engine/device3d.cpp`, `Matrix::Transform`), it is now the only version; `USEASM` and `FLOATASM` are gone. `MakeInt` uses `lrintf`, which rounds to nearest like the x87 `fistp` it replaces (the old non-assembly `MakeInt` did not compile). |
| x87 precision | `_controlfp(_PC_53)` (client start-up, and the server's `fDoublePrecision` setting, now ignored) is gone: x64 has no x87 precision control, and float maths is SSE2. The `HitTest.cpp` trick that forced a float out of an x87 register is removed for the same reason. |
| Pointers kept in 32-bit fields | IGC's `SetPrivateData`/`GetPrivateData` (ship, side, cluster, mission) and the connection's private data, which hold the server's `CFS*` and the client's `PlayerInfo` pointers, are `DWORD_PTR`. The client list panes' item data (`LONG_PTR`), the message builder's blob lengths (`FM_VAR_PARM` passes them pointer-sized), window subclassing (`SetWindowLongPtr(GWLP_WNDPROC)`), `TlsValue`, qhull's `ptr_intT` (`size_t`), and a dozen single casts (handles, `ShellExecute` results, pointer differences measured in `DWORD`s) |
| Debug allocator | `operator new(size_t, file, line)` (it took `unsigned int`, so no `new` compiled on x64). `ZFree` frees a client block directly instead of reading the block type from a copy of the CRT's block header, whose layout changed with the Universal CRT. The unused `zlib/fixdelete.h`, which replaced the global `operator delete` the same way, is deleted |
| Warnings | C4302, C4311 and C4312 (a pointer cut to 32 bits, or a 32-bit value made a pointer) are errors. MSVC also reports them for constants, which a clang x64 pass did not: `c_htidStaticObject` and two `-1` markers now go through `INT_PTR` |
| MsQuic | The x64 `msquic.dll` is copied next to the exes |
| Stack depth | A `TList` freed its nodes recursively (each node's reference to the next released it), one level per element. With x64's larger frames, the Debug `mdlc` overflowed its 1 MB stack converting the models. A node now frees the rest of the chain in a loop |

`AllSrv32` keeps its name (exe, project and Windows service name), so an installed service and scripts
that start it keep working.

### Phase 4: art pipeline

`Artwork/Art.proj` replaces the nmake/awk/sed/batch chain (`Artwork/256/makefile`, `makegeo.mak`,
`Artwork/tools/*.bat`, `*.awk`, `sed.exe`), which has been deleted. It read `bitmap.txt` and
`copymdl.txt` unchanged, and `Models.props`, a mechanical conversion of `makegeo.mak`'s
model list. It builds incrementally through Inputs and Outputs. (The art was one flat `Artwork/256`
folder until "Art folders" below; the sections up to there describe it as it was then.)

| Step | Original | Replacement |
|---|---|---|
| Images | `mdlc -convert` (24-bit BMP to 16-bit MDL) | `mdlc -dds` / `-ddsmips` (32-bit BGRA DDS), see "Textures: 32-bit DDS" |
| Animations | `mdlc -compressanim` (precompressed 16-bit frames in a binary MDL) | A generated text MDL that builds the animation from the strip's DDS when loaded |
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
- `fig02pit` is left out: `makegeo.mak` had a rule for it, but its source never existed (the 2004 drop had
  only `fig02pit.old.x`, an abandoned 3D cockpit for `fig02`), and no code or MDL ever loaded it.

Each model's template refers to its `.x` files by absolute path in `out\obj\Artwork\models\`. That lets `mdlc` run in `out\Artwork\` without the intermediates. It has to run there because textures are looked up on the art path, and a missing texture is a fatal error (`ModelerImpl::GetFile` asserts).

`ops1` and `ops2` are in `makegeo.mak`, and their textures `ops1.bmp` and `ops2.bmp` are in the drop. But the textures were never listed in `bitmap.txt`, so the original build could not have produced these two models. Both lines are now added to `bitmap.txt`.

Two `mdlc` changes support this:

- `-batch <file>` runs one command per line in a single process. A Debug `mdlc` spends about 2.4 s starting the engine, so one process per bitmap would take about an hour. Each converted namespace is unloaded after it is written, which keeps memory flat (it mattered in the 32-bit process).
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
- Wildcard copies of `*.wav`, `*.rtf`, `*.avi` (now `*.mp4`, see "Movies"), `*.igc` and `*.mml`.

The client reads only that output directory. Everything else was deleted, and git history keeps it:

- `Artwork/old256`, `cadet` (art for the separate Cadet product, never integrated), `ship`, `sounds`, `soundpac1`, `training`, `team2.igc` and `team4.igc`.
- About 800 files in `Artwork/256`:
  - prebuilt `.mdl` and intermediate `.x` files that `makegeo.mak` regenerates;
  - bitmaps and models that no manifest names;
  - `artwork_new/`, `icons/` and stray scripts and notes.
- The progressive-mesh tools and unused templates and scripts in `Artwork/tools`.

A before-and-after check confirms that all 3,186 inputs the manifests need are still present. That check covers every model's optional `_static.x`, `-low.x` and `_bound.x` sources. The first version of the cleanup missed them: `makegeo.mak` names each model as `$(@B)`, so a regex read the literal `$(@B)` instead of the name, and 234 sources were deleted. They have been restored.

Kept on purpose:

- `Artwork/redbook` (now `Artwork/Music`): the soundtrack. The game plays music as audio-CD tracks through MCI (`soundengine/redbooksound.cpp`), so the port has no music without a CD. A Phase 3 follow-up replaces the CD player with file playback from these WAVs.
- `Artwork/256` held twelve MDL files that `copymdl.txt` never listed (`console`, `loadout1`, `3`, `4`,
  `5`, `9`, `11`, `loadoutbomber`, `loadouttacticalfighter`, `messageoftheday`, `techtree`, `temp`).
  An earlier note here called them missing at runtime; none is used, and they are deleted, with 25 that
  did ship and were never loaded either:
  - `console`: the seven UI files that `use "console"` get the namespace `console.cpp` builds in code;
    the file is an older UI definition.
  - The per-hull loadout layouts (`loadout<hull id>`) and per-class ones (`loadoutbomber`,
    `loadoutscout`, …): the loadout screen reads the members `loadout<hull id>` of `loadout.mdl`, which
    all are `loadoutDefault`, and never loads these files.
  - `messageoftheday` (the lobby screen loads a downloaded `publicmessageoftheday`), `techtree` (the
    pre-core-file tech tree; it uses functions that no longer exist), `temp` (a scratch copy of
    `quickchat.mdl`'s structures).

  Also removed: the "game starting" screen (`gamestartingscreen.cpp` and `.mdl`). Nothing switched to
  `ScreenIDGameStarting`, the code looked up a `quitButtonPane` the MDL never defined, and its
  background image never existed. And the loadout screen without a team asked for `loadoutpanemp` and
  `hullinfomp`, which lacked the `bmp` that image names end in; they are the generic `loadoutpane` and
  `hullinfo` images now.

  The 29 images the original build also never shipped (`BtnMissionDetails`, `hudgauge` and the rest) do
  ship since the move to DDS sources ("Textures: 32-bit DDS").

#### Art folders

The art is in folders by kind, in the sources and in `out\Artwork` alike:

| Folder | Holds | Source |
|---|---|---|
| `Audio` | Sound effects and voices (878 WAVs) | copied |
| `Music` | The soundtrack (13 WAVs, `tracks.lst`) | copied (hard links) |
| `Textures` | Images that are not model textures: UI, HUD, icons, effects, decals, planets, backgrounds (1,182 DDS) | copied |
| `Models` | The 223 template models' and 16 text models' `.x` sources and the 230 textures their `.x` files name; the output adds each model's `.mdl` and `.cvh` | `mdlc -optimize`, `xmunge` + `cvh` + qhull |
| `Fonts` | `fontsource.mdl`, compiled to `font.mdl`; the TTFs it needs (not in git) | `mdlc` |
| `Interface` | The UI: screens, panes, cursors, sound definitions (72 MDL files), the six animation MDLs the build writes, and the 254 help pages (`.mml`) | copied |
| `Movies` | The opening movies | copied |
| `Data` | The core files and training maps (`.igc`) | copied |

The folders are the manifests: `bitmap.txt` and `copymdl.txt` listed exactly what the folders hold, so
they are gone. `Models.props` (at `Artwork/`) still lists the models, since each has a template and a
frame.

Nothing names a folder. MDL files, the core files and the code still say `"fig02"`, `"ires01bmp"`,
`"newtarget"`; `zlib/artfolders.h` lists the folders a file of each type is looked for in, and
`ModelerImpl::GetFile` (MDL, images, `.x`, help pages; `Modeler::GetArtFile` for sounds and movies)
and `UTL::getFile` (core files and hulls, for the client and the server) look in those, then in the
art root, so a file put there by hand still works:

| Type | Folders |
|---|---|
| `.dds` | `Textures`, `Models` |
| `.mdl` | `Interface`, `Models`, `Fonts` |
| `.x`, `.cvh` | `Models` |
| `.mml` | `Interface` |
| `.wav` | `Audio` (music is read from `Music` by path) |
| `.mp4` | `Movies` |
| `.igc` | `Data` |

A name with a folder in it, or an absolute path, is taken as it is. `mdlc` looks art up from its
working directory unless given `-artpath <dir>`; the model batches run in `out\Artwork\Models`, where
the outputs go, with `-artpath out\Artwork`, since optimizing a model also loads effect images (the
lights' strobe) from `Textures`.

The art CI job runs `tools/ci/art-check.py` first. It resolves every reference the way the client does,
against the source folders plus what the build generates: the MDL files' sounds, images and `use`
imports (namespaces the code creates count), the core files' models, icons, textures and investment
images, and the images, namespaces and models the code names; and it fails when a name is in two
folders searched for one type, where one would hide the other. Every reference resolves. The one station
type without an investment-screen image, the Bios "Platform" (`biosplat`), is the flag pedestal
(`c_sabmPedestal`): the server places it in a capture-the-flag game and it is never offered, so
the check skips pedestals.

`tools/ci/art-unused.py` lists the art files nothing uses; it only reports, and the art job puts the
list in its summary. It starts from the code (client, server, `mdlc`, `xmunge`: every string literal
outside comments, the used `artwork.h` constants, and the names the code builds, such as a random
map's `globe1`–`globe8` and `invEmpty` + an equipment type) and follows what is named: the core
files and maps the code names (every name in them, plus the `i`/`l`/`icon` images and civilization
suffixes the code derives), MDL files, help-page tags, a used model's `.x` files and template from
`Models.props`, the textures a used `.x` file names (`.ppm` or `.bmp`, which `engine/xfile.cpp`
maps to the image), the animation strips, and `Music\tracks.lst`. A name counts for every file of
that name, so the list errs toward keeping a file; a name the code assembles some other way is not
seen, so a listed file is to be checked before it is deleted. It also lists the `Models.props`
entries no one uses.

The first report listed 637 of 3,135 files (50 MB), checked by hand against every mention of each
name, and they were removed: 56 models no core file, map or code names (among them the weapons
`wep01`, `wep03`, `wep05`, `wep08`, `wep10`, `wep11`, `wep13`, `wep17`, the civilizations' flag
platforms and the `*ops1` fighters) with their `.x` files, textures, investment and loadout images
and `Models.props` entries; Zone-era and commented-out interface images; 71 help pages no page links
to, `splashscreen.mdl` and `stndialog.mdl`; 40 sounds no sound definition imports; and `eula.rtf`,
which nothing shows (so `.rtf` is no longer an art type). A core file or map from elsewhere that
names one of these models would no longer load.

Changed with the move:

- The `SoundPack` override (a folder next to `Artwork` that replaced sounds file by file) is gone;
  nothing ships one, and replacing a file in `Audio` does the same.
- The player's key bindings (`inputmap1.mdl`) are player data: they are saved next to `settings.json`
  and read from there. Until they are first saved there, the copy in the art folder, where the client
  used to save them, is read, so existing bindings carry over. `defaultinputmap.mdl` stays with the art.
- `selected.mdl` is deleted: nothing loaded it, and the `selection` namespace it imported exists
  nowhere.

#### Smoke test

CI runs `tools/ci/mdlc-smoke.ps1` after the build. It writes two DDS images (one with mip levels) and an
animation strip, checks the DDS files, compiles `fontsource.mdl`, and loads the animation through a
text MDL. This establishes whether `mdlc`, which starts the engine with its software
DirectDraw path, runs on a headless runner. The first run crashed during engine start-up: `FontImpl::GetSizes` asserted on a zero-width glyph, which current GDI reports for some characters. The assert is relaxed to reject only negative widths; the Debug client used the same path.

#### Textures: 32-bit DDS

The images are DDS files, uncompressed 32-bit BGRA (DDS `A8R8G8B8`), which any DDS reader opens. They
are the source art in `Artwork/256`: the 1,413 BMP sources (146.7 MB) were converted once with
`tools/art/bmp2dds.py` and deleted, as the movies were converted to MP4. The art build copies the
images `bitmap.txt` lists. They keep the BMPs' full 24-bit depth; the image MDL files the build used to
make (`mdlc -convert`) had cut them to RGB565.

The conversion was checked: before the BMPs were deleted, a CI step compared every one of the 1,413 DDS
files byte for byte with what `mdlc` (`-dds` / `-ddsmips`, removed since) wrote from the same BMP.

| Topic | Decision |
|---|---|
| Format | Uncompressed BGRA8. BC7 was considered: a quarter of the size, but lossy, an extra tool (texconv) in the art build, and UI code reads pixels on the CPU (hit testing, CPU-composed panes), which would need a decoder |
| Transparency | The game's only transparency was a colour key: pixels that are 0 in RGB565 (pure black, and near-black up to about RGB 8,4,8) are transparent where an MDL file asks for the key (`ImportImage("name", true)`). The conversion baked exactly that set into alpha: alpha 0 there, and RGB 0 too, so "alpha 0" and "black" stay the same pixels. 1.4 million near-black pixels in 635 bitmaps are transparent only because of the 565 rounding; keying on pure black instead would have made them visible |
| When alpha applies | Only where the MDL file asks for the key, as before. An image loaded without it is drawn opaque, alpha ignored, so bitmaps used both ways (with and without the key) look as they did |
| Model textures | The 230 images that a model's `.x` file names (`TextureFilename`) have a mip chain (2x2 box filter, down to 1x1) baked into the file. All are power-of-two sized. Distant models no longer shimmer; their textures are a third larger. The `.x` files still name them `<name>.bmp`, which the engine maps to `<name>.dds` |
| Images never built | The 29 BMPs that `bitmap.txt` did not list (referenced by name from code or MDL, so missing at runtime since 2004; see "Art directory cleanup") were converted too and added to the list, so they now ship. Untested: they appear wherever the code asks for them |
| Editing an image | Use any editor that writes uncompressed 32-bit DDS (Paint.NET, GIMP). Alpha is premultiplied ("Soft alpha" below); editors save straight alpha, so run `python tools/art/bmp2dds.py premultiply Artwork/Textures/<name>.dds` (or `Artwork/Models/` for a model texture) after saving (it also rebuilds an existing mip chain). For a model texture saved without its mips, rebuild them: `python tools/art/bmp2dds.py mips Artwork/Models/<name>.dds` |
| Animations | The six strips are DDS images too. Each gets a small text MDL (`use "model"; frame = ModifiableNumber(0); image = FrameImage(ImportImage("<name>bmp", false), frame, x, y);`) that compresses the frames when it loads, with the code `mdlc -compressanim` ran at build time |
| Fonts | Stay in `font.mdl` (glyph bitmaps plus metrics, not textures) |

How the engine reads them:

- `Modeler::GetNameSpace` loads an image namespace `<name>bmp` from `<name>.dds` when there is one
  (`engine/dds.*`), so every way an image is reached works unchanged: `ImportImage` in text MDL, model
  textures, and the imports in binary model MDL files. An image MDL file from an older art build is
  still read if there is no DDS file next to it.
- The image is tagged with its namespace, so `mdlc -optimize` writes models that import their textures
  by name. `INameSpace::AddMember` does that only in debug builds; a release `mdlc` would otherwise
  embed the pixels in every model.
- The surface points into the file mapping, like the image MDL files did. Its mip levels go to the
  texture cache with it and are dropped on the first write to the pixels.
- The texture cache takes alpha from the image when it is colour keyed, and makes it opaque otherwise.
- The engine's CPU-side surfaces are 32-bit BGRA with alpha too, the same pixel format object as the
  images, so drawing an image into one copies it unconverted ("Soft alpha").

Nothing reads BMP files any more: `mdlc -convert`, `-compressanim`, `-dds` and `-ddsmips` are removed,
and so are `Modeler::LoadBitmap` and `Engine::CreateSurface(HBITMAP)`. The smoke test checks the format
and the key rule in three of the DDS sources (one a model texture with mip levels) and loads an
animation from a DDS strip.

#### Soft alpha

Partial alpha works in the engine; the art doesn't use it yet (its alpha is 0 or 255), so nothing looks
different until an image is authored with soft edges.

| Topic | Decision |
|---|---|
| Alpha form | Premultiplied, in the DDS files and everywhere after. The converted art already is: its transparent pixels are RGB 0, so the baked mip levels (a box filter over those pixels) are premultiplied averages. Treating the files as straight alpha would have meant premultiplying in the texture cache, which darkens the edge texels of every model texture's mip levels a second time. Editors save straight alpha; `bmp2dds.py premultiply` converts |
| Where it applies | Wherever an image is colour keyed, as before: UI images (blits, on the GPU and on the CPU), and images drawn through the 3D path with blending (`BlendModeSourceAlpha`, `BlendModeAdd`: HUD indicators, radar, loadout, effect images), where the texture's alpha multiplies the vertex alpha. Opaque 3D draws (model textures, `BlendModeSource`) still cut out at alpha 50%, which is the old colour key for 0/255 art, with no sorting of transparent parts (since then antialiased with alpha to coverage; see "Alpha beyond the cut-out") |
| Hit testing | A keyed image catches clicks where its alpha is at least 50%, so faint glows and shadows don't |
| CPU surfaces | `GetPrimaryPixelFormat` is 32-bit BGRA with alpha (it was RGB565). Fills, text, conversions, the GPU readback and the colour-keyed blit (now a premultiplied "over") handle it; the fill asserted 16-bit, which an `AnimatedImage` of a DDS strip with more than one frame hit since the DDS change. `Surface::Save` (16-bit BMP, no callers) is removed |
| Render targets | A pane's GPU copy holds premultiplied alpha as coverage. Fills, text and opaque blits replace pixels (alpha 1); keyed blits blend over them, colour and alpha; opaque 3D draws write alpha 1, blended ones accumulate it. A fill with the colour key (black, in 565 terms) writes alpha 0, so a keyed pane cleared that way starts transparent and keeps soft edges drawn into it |
| Legacy black key | Panes were keyed on black: whatever was drawn black, or left black, was transparent when the pane was composited. That still holds, but only for opaque pixels: when a render target is the source of a keyed blit or a 3D texture, an opaque pixel that is black in 565 terms is transparent, and the readback to the CPU turns it into alpha 0. Translucent dark pixels (shadows) survive. On the CPU, `MakePixel` gives a colour that is black in 565 terms alpha 0 |

Visible differences, for 0/255 art:

- Images drawn through the 3D path with blending and linear filtering: their filtered edge texels used to
  be cut at alpha 50%; now they blend, so those edges are anti-aliased rather than stair-stepped.
- Translucent 3D content drawn into a keyed pane (over its cleared, transparent background) stays
  translucent when the pane is composited; it used to come out opaque and darkened, since only black
  was transparent.
- CPU-composed surfaces (the hangar, the LOD scroll pane) now keep 24-bit colour instead of 565.
- CPU text: glyphs clipped at the left or right edge lost their pixels beyond the first eight columns
  (`ibit & 0x7 == 0` parses as `ibit & (0x7 == 0)`); fixed. The GPU text path was not affected.

#### Alpha beyond the cut-out

The art's alpha is all 0 or 255 (720 colour-keyed textures, 256 opaque), and all translucency in 3D is
the effects' (glows, trails, bolts, alephs, explosions: mostly additive). This makes what there is look
better and prepares for translucent art.

| Piece | What it does |
|---|---|
| Alpha to coverage | An opaque cut-out (a colour-keyed model texture) in a 4x multisampled scene: the filtered alpha, sharpened to a ramp a pixel wide around 0.5, is the share of the pixel's samples it covers, so its edges are antialiased like a triangle's and stay where the 50% cut put them. The blend state takes the stored alpha from the blend factor (1), not the shader. Without MSAA (GPUs with under 1 GB) the 50% cut stays (`raster.hlsli`, `m_pblendSceneCoverage`) |
| No dark fringe | An opaque cut-out shows its texel's colour, not its premultiplied colour: a filtered edge texel at alpha 0.6 held 60% of its colour, and was drawn as that, a dark outline round every cut-out |
| Coverage-preserving mip levels | Box-filtered mip levels average a thin strut below alpha 0.5, so cut-out detail thinned out and vanished with distance. At upload each level of a cut-out (full-size alpha only 0 and 255) is scaled so its share of texels at or above 0.5 matches the full-size image, the colour with the alpha, as the texels are premultiplied; of the scales that give that, the middle one, so the deciding texels are clear of 0.5 (`Engine/alphacoverage.h`, tested). The DDS files are unchanged; translucent art is left alone |
| Per-sample tonemapping | A multisampled scene is composited from its samples, each tonemapped and encoded before they are averaged (`scene_ps_ms.hlsl`): a glow at ten times white next to a dark hull used to be averaged first, which smeared the edge white. The box resolve stays, for the bloom |
| Blended draws after the opaque ones | In a 3D layer's scene a blended draw that tests depth is recorded (its state and a copy of its vertices) and drawn once the layer's opaque geometry is in: when the layer ends, or before the z-buffer is cleared or something is drawn without a depth test, which keeps its place. Replayed in order (Context's sorted effects are back to front), without writing depth. Drawn in place, a translucent surface's depth hid the opaque geometry drawn after it behind it |
| Soft particles | The replay binds the scene's depth read-only and as a texture (feature level 11: typeless depth, a read-only view and a texture view), and a replayed blended fragment fades out over `c_softDistance` (3 world units) in front of the geometry behind it, the nearest of the pixel's samples, so glows and trails no longer cut hard lines into hulls. Reversed depth is (1 + a) - b / d for screen and model-space vertices alike, so Device3D hands the rasterizer (1 + a, b) to get view distances back |

To check by eye (nothing in CI renders): the edges of cut-out parts (station struts, antennae, the
lattice textures) antialiased at all distances and no longer outlined in dark; thin cut-out detail
still there far away; engine glows and explosions fading into hulls instead of cutting into them, and
3 units the right distance for that; and nothing drawn blended in a z-buffered 3D layer (the HUD's
3D parts) now in front of or behind what it should not be, the one risk of moving those draws.

#### Translucent surfaces

For translucent art to come (glass, energy shields, holograms): a texture's alpha decides. Only 0
and 255 (with at most 1% of texels in between, the noise a re-saved cut-out picks up) is a
cut-out; more than that in between is translucent (`AlphaCoverage::Classify`, at upload). An opaque
draw of a translucent texture in a 3D layer's scene goes to the translucent pass with the
"over"-blended draws (`BlendModeSourceAlpha`); nothing has to be marked in the model.

| Piece | What it does |
|---|---|
| Technique | Weighted blended order-independent transparency (McGuire and Bavoil, 2013). During the replay of the scene's blended draws (see "Alpha beyond the cut-out") the translucent ones draw, in any order, into two extra targets with the scene's samples: an accumulation target (RGBA16F: premultiplied colour and alpha, weighted by depth, the paper's equation 10: nearer surfaces weigh up to 3000 times more) and a revealage target (R16F: what still shows through, from 1, times 1 - alpha). A composite (`oit_composite_ps.hlsl`) divides out the weights and lays the average over the scene by 1 - revealage, before the additive effects (so light shows over glass) and before the tonemap |
| What it gets right | Translucent surfaces in any order, within one model and between models, intersecting ones included, with no sorting; they fade into what is behind them like the soft particles |
| What it approximates | Where several near-opaque layers stack, their colours mix by weight instead of the front one winning outright; nearer layers weigh more. Fine for glass and shields; not for a stack of opaque-looking decals |
| Cost | The two targets are made the first time a scene has a translucent draw: about 150 MB at 2560x1440 with 4x MSAA. Feature level 11 (as the soft particles); below it translucent draws are blended in their order, unsorted |
| Lighting | Translucent model surfaces are lit like the others, without the specular highlight (it is for opaque surfaces) |

Authoring: save the texture with straight alpha, then `python Tools/art/bmp2dds.py premultiply
<file>.dds` (it rebuilds the mip chain if there is one). To try it before there is art, give an
existing model texture a uniform alpha of about 50% that way and fly past the model: it should be
see-through from every side, its far side visible through its near side, with no flicker as the
view turns and nothing behind it lost; then two such models overlapping, and one in front of an
engine glow (the glow shows over it).

### Phase 5: optional

C++20, replacing `ZString` and the custom containers, unit tests, XAudio2 (done, in Phase 3's "Sound"), a D3D12 backend, a new lobby service.

#### C++20 (done)

`LanguageStandard` is `stdcpp20` for every project. The switch by itself changes no behaviour; it is the
base for the library cleanup (`std::span`, `<format>`, concepts for `TRef`). Few fixes were needed,
since `/permissive-` had already done the conformance work:

- C++20 also tries `b == a` for `a == b`. `TRef` compared only against `Type*`, so `TRef == TRef` had two
  equally good candidates, `(TRef, Type*)` and its reversal. `TRef` now has exact `TRef`-to-`TRef`
  `==` and `!=`.
- The library pieces removed in C++17 and C++20 (`std::bind1st`, `std::not2`, `std::binary_function`,
  `std::binary_negate`) are replaced with lambdas and plain function objects, so `_HAS_AUTO_PTR_ETC=1` is
  gone.
- Using the value of an assignment to a `volatile` is deprecated; the two loops that did it are rewritten.
- `IntItemIDWrapper` compared its value with the other wrapper through a conversion, which the reversed
  `operator==` made ambiguous for MSVC (clang resolved it); it compares the two values directly now.

How it was checked: every project was swept with clang (`-std=c++17` and `-std=c++20`, with C++20's
compatibility and deprecation warnings on), and the second run showed nothing the first did not. Clang
uses libstdc++, so the MSVC STL's C++20 removals (negators, `result_of`, allocator members, the
`<ciso646>`-style headers, `uncaught_exception`) were checked by search; only the negators were used.
No identifier is a new keyword (`concept`, `requires`, `char8_t`, the coroutine keywords), and no
`u8` literals are used.

#### Library cleanup: containers and ZString (done)

Decided scope:

| Piece | Treatment |
|---|---|
| `TVector` (value array with copy-on-write over file mappings) | `std::vector`. Done |
| `TArray` | `std::array`. Done |
| `TStack` | Its only use, the MDL evaluator's operand stack, is a small `ObjectStack` class over `std::vector` (`engine/objectstack.h`): the factories use `Pop()`'s value in expressions about 300 times, and `std::stack::pop` returns nothing. A popped object stays referenced by its slot until a later push reuses it, as with `TStack`: many factories keep a plain pointer to what they popped, and an unnamed object has no other reference. The first version released the slot on `Pop`, a use-after-free that surfaced as the smoke test's binary-MDL reload crashing in `FrameImage` once the heap layout changed. Done |
| `TList`, `TMap` | Kept. They are reference-counted objects, and `TList`'s sink parameter is how UI list panes get change notifications; replacing them is a redesign, not a swap |
| `Slist_utl` / `Slink_utl` (IGC) | Kept. Intrusive lists whose members unlink themselves on destruction; game-object lifetime depends on them |
| `ZString` | To `std::string`, in steps: first its number constructors become explicit (done) (`"Score " + n` formats a number with `ZString` but appends one `char` with `std::string`), then one library group per PR, with explicit `.c_str()` where `ZString` converted to `const char*` silently |

How the container change was made: `TVector` was first replaced by a shim deriving from
`std::vector` whose every old member was `[[deprecated]]`, so a clang sweep of all projects listed
each call site (462). A script rewrote them from those exact locations (`GetCount()` becomes
`(int)size()`, since `engine` builds with warnings as errors; `Set(i, v)` becomes `[i] = v`; and so
on); the constructors and the few `Find`/`Remove`/`Insert` calls were done by hand. Then the shim was
deleted, so anything left is a compile error, not a silent change. The one trap that would have
compiled is `TVector(countAlloc, count)`, which `std::vector` reads as `count` copies of a value; all
20 uses (equal arguments, or a reserve of an empty vector) were rewritten first.

Behaviour differences:

- A mesh loaded from a binary MDL file copies its vertices and indices; `TVector` could point into the
  file mapping, `std::vector` can't. Load time and memory grow by the size of the geometry.
- Element bounds checks: `TVector` asserted in debug builds; the MSVC debug STL does the same.
- An empty `TVector` could be assigned `NULL` (through its `int` constructor); the one place that did
  calls `clear()`.

ZString's number constructors (`ZString(int)`, `ZString(float)`, `ZString(bool)`) are explicit (done).
Implicitly they formatted numbers wherever a `ZString` was expected, and `std::string` would silently
turn the same code into appending one `char`. The compiler listed the 23 places that relied on it
(score and team-pane columns, a mission's lives, a sector map count, an MDL number-to-string value,
vector and sound-buffer debug strings); each now says `ZString(n)`. One was a bug: the video screen's
CD path was meant to get a `/` appended, and got `"47"` (the char's code, through `ZString(int)`).
With the constructors explicit, `ZString + int` would compile as pointer arithmetic through the
`const char*` conversion (before, it was ambiguous and never compiled), so `ZString`/`int` `operator+`
is deleted.

The move to `std::string` runs in four steps, so the two types are never mixed (a library-by-library
switch would need conversions at every interface, since `ZString` is in all of them):

1. `ZString` gets `std::string`'s names (`size()`, `empty()`, `c_str()`, `data()`, `clear()`), and its
   own operations become free functions in `zlib/zstring.h` that keep their exact semantics: int
   indices with -1 for not found, and the quirks (`StrReverseFind` of a string searches forwards, as
   `ReverseFind` did; `StrLeftOf` with no match returns the delimiter set). Numbers format through
   `StrFromInt` / `StrFromFloat` / `StrFromBool` (`"%g"` for floats, not `std::to_string`'s `"%f"`), and
   `StrRepeat(ch, n)` replaces `ZString(ch, n)`, whose argument order `std::string` reverses.
2. Call sites move to those names, one library group per PR, by a script driven by a clang sweep in
   which the old members are marked deprecated (locally only). zlib, engine and mdlc: done (181 sites); effect and soundengine: done (50); Igc, clintlib and training: done (15); WinTrek and FedSrv: done (197).
3. The implicit conversion to `const char*` goes; every remaining use gets `.c_str()` (done, about
   230 sites, from the same kind of sweep, with clang's notes saying which argument of a call was the
   string). Two uses meant something else: `if (cfgInfo.strTrainingURL)` was always true (the
   conversion never gives null), so the "can't find the download address" message could never show;
   it tests `!empty()` now. And the sonic-chat code kept a pointer into the `ZString` that
   `GetSonicChatText` returns by value, which only stayed valid because the copy shared the stored
   string's data; it keeps its own copy for the function now, as `std::string` will need.
4. `ZString` becomes `std::string` (done): `class ZString : public std::string` with nothing but a
   null-safe `const char*` constructor and assignment (null gives the empty string, as before;
   `std::string`'s is undefined, and network message fields read with `FM_VAR_REF` are null when
   empty, 53 of them), and `+=` / `+` with an `int` deleted, since `std::string`'s `+=` would append
   one char. Everything that takes a `const std::string&` takes a `ZString`; the name can go per site
   later. The reference-counted data, the static file-mapping strings and the members are gone; the
   `Str*` functions are now written on `std::string`, with the same semantics (where `ZString` read
   past the end for an out-of-range count, they clamp). Other effects:
   - Copies are real copies. Every stored `c_str()` was checked in step 3 to point into a named
     string, not a temporary.
   - `PathString + "name"` stays a path join (a `PathString + const char*` overload; it would have
     been ambiguous with `std::string`'s `+`), and `PathString` takes a `std::string`.
   - MDL struct members of type `String` live in a zeroed byte buffer: they are constructed in place
     when read and destroyed explicitly (`std::construct_at` / `std::destroy_at`, since debug builds
     `#define new`), where the old code assigned into the zeroes and cleared the pointer.
   - `std::string`'s `[]` asserts in debug builds past `size()`; `PathString::GetDrive` read `[1]` of
     possibly shorter strings and checks the length now.
   - Two `_TRACE`-only uses of the old constructors (`zlib/Win32app.cpp`) that no build compiles were
     converted too.

#### override everywhere (done)

clang-tidy's `modernize-use-override` fix-its over every translation unit (destructors left alone): 4,043
declarations gained `override` and 1,869 lost the `virtual` it makes redundant, so a function that stops
overriding, because a base signature changed, is a compile error instead of a silent new virtual. The sweep is
one commit marked `Mechanical:` (AGENTS.md section 1). One fix-it was left out: `TmodelIGC<T>::MakeUnhitable`
overrides for some `T` only. One fix-it that was not the check's (a compile error's typo correction, from parsing
with mingw-w64's headers) slipped in and was reverted; the sweeps now apply their own check's fix-its only.

clang's `-Woverloaded-virtual` over the result listed 20 functions that hide a base virtual of the same name.
Most are deliberate overloads. Five were meant to override and never did:

| Function | What it broke |
|---|---|
| `BaseClient::DamageStationEvent`, `KillStationEvent` | They took a `leakage` argument that `IIgcSite` no longer passes, so the site's empty defaults ran: no hull or shield sound for a station hit in your sector, and offline a destroyed station was neither blown up nor removed |
| `Slideshow`'s picture `HitTest` | Without `bCaptured`, `Image::HitTest` on the picture's empty bounds answered, and a click never reached the `Button` that advances the slideshow |
| A menu item's `MouseLeave` | It took a point `IMouseInput`'s does not; the menu's handler is empty, so nothing changed |
| `CprojectileIGC::Export` | Not `const`, as `IbaseIGC`'s is; projectiles are not exported through the base today |

Not fixed, for a decision: `CbuoyIGC::SetVisibleF` and `CchaffIGC::SetVisibleF` are empty, meant to keep
buoys and chaff out of the client's visibility pass, but `const` by mistake, so they do not override, and
the pass hides and shows them like everything else. Fixing it would make them always visible, a
fog-of-war change to try in play first.

#### NULL to nullptr (done)

clang-tidy's `modernize-use-nullptr` fix-its: 3,571 null pointer constants (`NULL`, and 45 literal `0`s used
as pointers) on 3,419 lines are `nullptr`, one `Mechanical:` commit per library (NeuronCore, NeuronClient,
NeuronServer, GameCommon, GameLogic, GameApp, and the executables, tests and tools together). A script checked
that every changed line differs from the old one only in those constants. `NULL` inside macro definitions, and
where an integer is meant, is left alone, as the check leaves it; so are the few files clang could not parse
on Linux (those using Media Foundation, X3DAudio and MsQuic headers mingw-w64 lacks).

#### Bounded strings (done)

`strcpy`, `lstrcpy`, `strcat`, `sprintf`, `wsprintf`, `_snprintf`, `_vsnprintf`, `vsprintf` and `strncpy` are
gone from `Source` and `Tests`: 213 calls go through `Bounded::Copy`, `Append`, `Format` and `FormatList`
(`NeuronCore/Base/BoundedString.h`, reached through `ZLib.h`). They take the destination as an array, so the
bound is the array's own and a pointer destination does not compile without a stated size. What does not fit is
cut off and the result always terminated (cut off rather than `strcpy_s`'s abort, since names, chat and game
settings come off the network and a long one must not stop the server); `Copy` zero-fills the rest of the buffer
as `strncpy` did, so a record sent whole carries no stale memory. A script rewrote the calls with array
destinations (212); the 23 with pointer destinations were sized by hand, two of them in Debug-only code.
Left alone: `lstrcpyn` (already bounded), the cvh art tool (it does not link NeuronCore) and vendored Qhull.
The logon handler, for one, already checked its fields' lengths before copying; the bounds make that the rule
rather than each caller's care. What they turned up:

- `FormatErrorMessage` (the FTP/HTTP session) wrote the server's response text at the end of its 1 KB error
  buffer but gave it the size of the whole buffer.
- `SetSessionDetails` built the server's published game list in a static 10 KB buffer with no bound and no
  terminator, so a shorter list than the last kept the tail of the longer one: clients could be shown games that
  had ended.
- A console readout appended a space and a name to a full 100-byte buffer, over its terminator.
- `FedSrvApp::AsyncFileOut` formatted a 19-character time prefix, with its terminator, into the text's length
  plus 19 bytes.
- `FileCRC`'s error buffer contract (100 + `MAX_PATH`) is a constant, `FILE_ERROR_SIZE`, which `UTL::SaveFile`
  shares.

`UTL::SearchAndReplace` (no callers, a destination with no size, and replacements that can lengthen the result)
and the unbounded `Strcpy` / `Strncpy` wrappers are deleted.

#### Ownership: unique_ptr where it is clear (in progress)

Owned pointers become `std::unique_ptr` where one object plainly owns another; the rest of the tree's
`delete`s (about 150: game objects whose lifetime IGC's intrusive lists manage, messages allocated as byte
buffers, nodes freed by `Terminate` methods) are left for case-by-case work.

- **Training.** A mission is a tree of conditions and actions, each node owning the nodes passed to its
  constructor: 34 members in 23 classes, and the action, condition, goal and wait-condition lists, hold
  `unique_ptr`. The constructors still take raw pointers, so the missions that build the trees are unchanged.
  Two places that replaced a held object without freeing it now free it, and a second `Stop` no longer leaks
  the parked mission tree. `TrainingMission`'s destructor frees its conditions before the player leaves the
  mission, as before: they point at the ships and stations that leaving tears down.
- **The client.** `MissionInfo`'s mission definition, and `PlayerInfo`'s scores, which were allocated with
  `new[]` and freed with `delete`. (`BaseClient::m_pMissionInfo` stays a raw pointer: it is a public member,
  and changing its declaration would hold its legacy name to the convention, a rename across three files.)
- **Pimpls.** `ZSettingsFile` and `FedMessaging`.
- **Array deletes.** Seven `new[]` allocations were freed with `delete` (found by matching every `delete`
  against its pointer's allocation): the training loadout purchase buffer, missile launch data, the server's
  leader vote count, DirectInput's data format and state buffer, a font's glyph data, and the frame image's
  XOR buffer.

Not converted: `SuspendedSoundFinishedCondition` (owns its sound action only when told to), `g_pMission`
(nulled before its `delete` on purpose), `CmissionIGC::m_pStatic` (owned only in some missions) and
`CFSMission::m_pMission` (terminated, in order, before it is freed).

#### Unit tests and AddressSanitizer

Each library has a Microsoft CppUnitTest project, `Tests/<Library>.Tests` (a DLL that links the
library and those below it; Test Explorer in Visual Studio runs them, CI runs all six in one
`vstest.console` run). NeuronCore's and NeuronClient's tests are what was `tests/UnitTests`, which
covered what the library cleanup could break without the game noticing at once:

- `ZString`: null `const char*` gives the empty string; `ZString + int`, `int + ZString`, `+= int` and
  the conversion to `const char*` don't compile (checked with `static_assert`s on concepts, so a
  regression fails the build).
- The `Str*` helpers: indices, not-found, out-of-range counts, `"%g"` number formatting, tokens. The
  inherited quirks have tests named `Quirk*`, so changing one is deliberate: `StrReverseFind` of a
  string searches forwards, `StrLeftOf` without a match gives the delimiters, and the reverse char
  search at index 0 starts at the terminator.
- The scrambler, with a known vector: the scrambled form is stored and sent, so its tables must not
  change.
- `PathString` joins (`..`, `.`, backslashes, drive letters, network paths). Two original bugs are
  pinned as quirks, not fixed, since nothing depends on the broken cases: a network path's
  `GetDrive` drops the share name's last letter (and is empty with nothing after the share), and
  joining a root-relative path gives `c://x`.
- Sound (`NeuronClient/Sound/soundmath.h`): gains, the distance and cone attenuation, the XAudio2 buffers
  for every kind of sound and starting point, and the position tracker (see "Sound").
- `ObjectStack` (moved to `engine/objectstack.h`, now `NeuronClient/Engine`, so it can be tested without the engine): order,
  growth, and that a popped object lives until its slot is reused, the use-after-free fixed above.
- Networking (NeuronCore's `Utility`): how `PFedMsgCreate` lays out a message (the `ib`/`cb` pairs right after
  the header, the variable data after the fixed part, trailing spaces trimmed, an empty string left
  out so `FM_VAR_REF` gives null; a string of only spaces is sent as an empty one, pinned as a
  quirk), `FedParseHostPort`, and the session details `FMSessionDesc` reads. And a loopback session
  over real QUIC: `HostSession`, `JoinSession` to `127.0.0.1:47700`, a message with variable fields
  from client to server, two messages in one packet from the server to `Everyone`, no bad CRCs, and
  the server dropping the connection when the client shuts down; plus a join with nobody listening,
  which must fail with a message. The loopback test uses the game server's port, so it fails while
  a server runs on the same machine (and Windows may ask whether the test host may use the network
  the first time). For these tests `PFedMsgCreate` asserts a session only when it queues the
  message: building one into a buffer never needed it, as the macros' comment says.

Not covered here: loading MDL needs an initialised engine (`EffectApp`), so the text-to-binary MDL
round trip stays in the mdlc smoke test, which runs under ASan too.

The restructure added tests for the other four libraries:

- NeuronServer: the server's single-writer, multiple-reader guard (`SWMRG`: readers share, a writer
  excludes readers and writers, the last reader lets a writer in; the waits that must fail are made
  from another thread, since the writer's mutex is recursive) and TCLib's `TCReplaceText` (in place,
  counting only, truncation, the case-insensitive form).
- GameCommon: IGC's object type bits, which cluster warning a side's assets raise (and that
  invulnerable stations ignore bombers and transports), and `solveForImpact`, the lead a shot needs.
- GameLogic: the server event table, which is edited by hand since AGC's XML went: IDs and names
  unique, every `%Name%` closed, every `%.Field%` one the log knows.
- GameApp: the chat and name censor, including text with characters above 0x7F, which used to index
  its lower-case table with a negative `char`. (`LoadBadWords`, which loads the word list from
  `allbad.dll`, is never called, so the game doesn't censor anything today.)

The test projects define `ZLIB_SKIP_NEW_REDEF`, so the debug `#define new` stays out of the framework's
headers. `msquic.dll`, which the NeuronCore build copies to the output directory, is loaded at run
time.

The `asan` CI job builds the six libraries, their tests and mdlc in Release with
`/fsanitize=address` (`-p:EnableASAN=true`; Debug's `/RTC` checks don't combine with it) and runs the
unit tests and the mdlc smoke test, which loads text and binary MDL (the code that had the
use-after-free). Before that it checks that `mdlc.exe` and the test DLLs import the ASan runtime, so a clean
run can't come from uninstrumented binaries. First run: 41 tests and the smoke test clean. The ASan runtime DLL comes from the Visual Studio installation and is put on `PATH`;
a report goes to stderr, which the smoke script keeps in `out/art-smoke/<step>.log.err` (attached to
the run as `asan-logs`).

## Building

Open `Allegiance.slnx` in Visual Studio 2026 (Desktop development with C++; ATL and MFC are not
needed) and build `Debug|x64`. From the command line:

```
msbuild Allegiance.slnx -p:Configuration=Debug -p:Platform=x64
```

The output goes to `out\bin\x64\<Configuration>\`.

CI builds each project on its own (`-p:BuildProjectReferences=false`), so one failure doesn't hide the
errors of the others. The error summary is on the workflow run page, and the full logs are attached to
the run.

The unit tests (`Tests/<Library>.Tests`) run from Test Explorer, or from a Developer Command Prompt:

```
vstest.console out\bin\x64\Debug\NeuronCore.Tests.dll out\bin\x64\Debug\NeuronClient.Tests.dll out\bin\x64\Debug\NeuronServer.Tests.dll out\bin\x64\Debug\GameCommon.Tests.dll out\bin\x64\Debug\GameLogic.Tests.dll out\bin\x64\Debug\GameApp.Tests.dll /Platform:x64
```

MsQuic comes from NuGet (`Source/NeuronCore/packages.config`, into `packages\`). Visual Studio restores it on
build; from the command line, restore first:

```
msbuild Source\NeuronCore\NeuronCore.vcxproj -t:restore -p:RestorePackagesConfig=true -p:SolutionDir=%CD%\
```

The build copies `msquic.dll` (for the build's platform) next to the exes. Networking needs Windows 11 or Windows Server 2022 or
later (QUIC's TLS 1.3 comes from Windows), and UDP 47700-47701 open for a game server.

Then build the art with the tools from that build:

```
msbuild Artwork\Art.proj -p:Configuration=Debug
```

The art goes to `out\Artwork\`, in the folders "Art folders" describes, and `out\bin\x64\<Configuration>\Artwork` is made a directory
junction to it, because that is where `Allegiance.exe` and `AllSrv32.exe` look for their art
(`-p:ArtLink=false` skips the junction). Building `Allegiance.exe` or `AllSrv32.exe` also makes that
junction in its own output directory if it is missing and `out\Artwork` exists (`Directory.Build.targets`),
so a build made after the art still finds it. Elsewhere, set `ArtPath` in `settings.json` and `server.json`. In CI, a
separate `art` job does this with the tools from the build job, and attaches the result to the run as
the `art-Debug` artifact.

`font.mdl` holds pre-rendered glyphs, so the fonts `fontsource.mdl` names must be available when it
is built. Verdana and Tahoma come with Windows, but Eurostile does not, and Windows quietly
substitutes Courier New for it (the menu titles then look garbled). `Art.proj` passes the fonts in
`$(ArtFontDir)` (default `Artwork\Fonts\`, where `*.ttf` are gitignored: Eurostile is not freely licensed) to
`mdlc -fontdir`, which loads them for that process only, so nothing has to be installed. Put
`eurosti.ttf` and `eurostib.ttf` there, or set `-p:ArtFontDir=<folder>`. When a font is still
missing, the build prints `mdlc : warning : font '...' is not installed, Windows substituted '...'`.
The CI art job has no fonts, so its `font.mdl` uses the substitute.

### Conventions

New code follows the naming and layout rules in [AGENTS.md](AGENTS.md), carried over from the sibling
repositories with one exception (constants are `UPPER_CASE`). The legacy tree is exempt and is not renamed
or reformatted. CI's `naming` job runs clang-tidy's identifier-naming check on the lines a change adds,
through `Tools/ci/check-naming.py`. The four files with upper-case extensions (`FedSrv.CPP/.H`,
`SWMRG.CPP/.H`) were renamed to `.cpp`/`.h`, because a case-sensitive `*.cpp` glob missed them.

The source files are the exception to "not renamed": all of them were then renamed to `PascalCase` (R7),
acronyms as words, 468 files: `afterburnerIGC.cpp` is `AfterburnerIgc.cpp`, `CRC.h` is `Crc.h`, `d3d11raster.cpp`
is `D3D11Raster.cpp`, `raster_ps.hlsl` is `RasterPs.hlsl` (and its byte array `g_RasterPs`). The casing came from
the type a file declares where one matches its name, and from its words otherwise. `pch.*`, `resource.h` and the
vendored Qhull keep their names. **The notes in this document keep the file names of their time**, as they keep
the module names: a name here matches today's file case-insensitively, without its underscores. MSVC opens an
`#include` whatever its case, so 259 includes and project entries were spelled wrongly before the rename and still
built; `Tools/ci/check-case.py`, in the same CI job, now holds every file name and every spelling of it to the
file on disk.

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
| Library pieces removed in C++17 (`std::bind1st`, `std::binary_function`) | `_HAS_AUTO_PTR_ETC=1` restored them until C++20; then replaced with lambdas and plain function objects, and the define removed | `soundengine`, `AGC` (removed), `test/TCLib` |
| C++20: `std::not2` / `std::binary_negate` removed | `SoundPriorityCompareReversed`, and small function objects for `ci_greater` | `soundengine/ds3dengine.*`, `test/TCLib/tstring.h` |
| C++20: `TRef == TRef` ambiguous with the reversed `(Type*, TRef)` candidate | Exact `TRef`/`TRef` `==` and `!=` | `zlib/tref.h` |
| C++20: `IntItemIDWrapper`'s comparisons compared `m_t` with the wrapper itself, so the conversion to `T` competed with the reversed member `operator==` (C2666; clang chose one, MSVC did not) | Compare `m_t` with the other wrapper's `m_t` | `_Utility/listwrappers.h` |
| C++20: value of an assignment to a `volatile` used in a loop condition (deprecated) | Comma expression, then the test | `zlib/FTPSession.cpp` |
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
| `&CComVariant(x)` (address of a class temporary), which permissive mode accepted with warning C4238 | Went with AGC, the only code that did it | (`AGC/` removed) |
| `/permissive-`: a member of a dependent base class named without `this->` | `this->` | `test/TCLib/AutoHandle.h` (`DECLARE_TCPtr`), `engine/value.h` (`TModifiableValue`) |
| `/permissive-`: a dependent nested type without `typename` | `typename` | `zlib/event.h` |
| `/permissive-`: a template calls `::GetString` / `::Write` / `::GetFunctionName` qualified, which finds only the overloads declared before it (two-phase lookup) | Called through `ValueGetString` / `ValueWrite` / `ValueGetFunctionName`, global forwarders that call unqualified, so argument-dependent lookup finds later overloads (`Matrix`, fonts) at instantiation; the `float` and `bool` overloads are declared ahead of them | `engine/value.h` |
| `/permissive-`: a non-dependent use of a class defined later (`TLookup` uses `IObjectPair`) | `IObjectPair` moved from `mdl.h` into `value.h`, ahead of `TLookup` | `engine/value.h`, `engine/mdl.h` |
| Empty member-initializer list (`TArray(TValue pvalue[count]) :`) | `:` removed | `zlib/tarray.h` (removed since; `std::array`) |
| `and`, `or`, `not`, `bit_and`, `bit_or` defined as macros; the first three are keywords | Macros removed; the code uses the standard alternative tokens, `bitand` / `bitor` included | `training/pch.h` and four `training/*.cpp` |
| Extra qualification on an in-class member declaration (`void PaneImage::Render()` inside `PaneImage`) | Qualification removed | `engine/paneimage.cpp`, `engine/model.h`, `FedSrv/BitArray.h`, `test/TCLib/UtilityThread.cpp`, `test/TCLib/WorkerThread.cpp` |
| `/permissive-`: a template's base-class members used unqualified, in classes that use many of them | Using-declarations and typedefs for the base's members | `zlib/point.h` (`TPoint`), `zlib/rect.h` (`TRect`), `_Utility/listwrappers.h` (`TListListWrapper`, `TMapListWrapper`) |
| `/permissive-`: the same, a few calls each | `this->` | `effect/listpane.cpp`, `engine/modeler.cpp`, `engine/value.h` (`TWrapValue`), `WinTrek/consoledata.h` |
| `TRef<Derived>` returned or passed as `TRef<Base>`: two user-defined conversions (`TRef<Derived>` → `Derived*` → `TRef<Base>`), which only permissive mode allowed | A converting constructor and `operator=` on `TRef`, constrained to pointer-convertible types | `zlib/tref.h` |
| String literals are `const` (`/Zc:strictStrings`) | The pointers, tables and parameters that hold literals are `const char*` (found with clang's `-Wwritable-strings`); OLE DB literals passed as `BSTR` are cast, as permissive mode did implicitly | `zlib/alloc.h` (debug `new` takes `__FILE__`), `Inc/messagecore.h`, `Inc/allegdb.h`, `zlib/FTPSession.cpp`, `clintlib/AutoDownload.*` (since deleted), `clintlib/clintlib.*`, `clintlib/badwords.cpp`, `FedSrv/FedSrv.CPP`, `WinTrek/WinTrek.cpp`, `WinTrek/Treki.h`, `engine/dderror.cpp`, `test/TCAtl/UserAcct.*`, and a few more |
| A temporary bound to a non-const reference | A named local (or a `const&` parameter) | `engine/bspgeo.cpp`, `effect/efpane.cpp`, `test/Inc/TCLib.h` |
| Address of a temporary (`&(RECT)rect`) | A named local | `zlib/window.h`, `engine/font.cpp` |
| `?:` with a `ZString` on one side and a `const char*` on the other: ambiguous | The other side made explicit; in `fsmission.cpp` it stays a pointer to the member string, not to a temporary | `FedSrv/fsmission.cpp`, `WinTrek/WinTrek.cpp`, `WinTrek/teamscreen.cpp`, `soundengine/ds3dvirtualbuffer.cpp`, `engine/enginewindow.cpp` |
| `ZString((long)x)` matched three constructors | `ZString((int)x)` | `WinTrek/WinTrek.cpp`, `WinTrek/loadout.cpp` |
| `return false;` from a function returning `TRef<Geo>` (`false` is not a null pointer constant) | `return NULL;` | `engine/xfile.cpp` |
| A message pointer compared `>= 0` (always true) | `!= NULL` | `FedSrv/FedSrv.CPP` |
| `_com_ptr_t = CComPtr<T>`: the template assignment cannot deduce through the conversion | Assign the raw `.p`, which `_com_ptr_t` QueryInterfaces as before | `AGC/IAGCRangesImpl.h` |
| Current ATL's `GetPropertyMap()` returns `const ATL_PROPMAP_ENTRY*` | Property-map helpers take `const` (none of them write through it) | `test/TCAtl/PropertyClass.*` |
| The server linked AGC's import library, which the original build never did (only `AGCGuid.lib`) | Reference kept for build order only (`LinkLibraryDependencies=false`) | `FedSrv/AllSrv32.vcxproj` |
| Project-generation error (mine): `SWMRG.CPP` was marked "no precompiled header" because its first `#include` lines are inside a comment. Compiling `pch.h` separately duplicated the non-inline `IsAllSrvRunning()` that MIDL emits from a `cpp_quote` | Back on the precompiled header, as in the original makefile | `FedSrv/AllSrv32.vcxproj` |
