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
| Networking | Own Winsock UDP transport behind the existing `FedMessaging` interface, replacing DirectPlay 4. Built in Phase 3 with a loopback / packet-loss test harness that runs in CI. |
| Scope | Tier 1 is built. Tier 2 stays in the repo, outside the solution. Tier 3 is deleted once nothing in Tier 1 or 2 references it. |
| Verification | GitHub Actions on `windows-2025` (VS 2026). `.github/workflows/windows-build.yml`. |

## Module tiers

**Tier 1 (built):** `zlib` (Microsoft's base library, not the compression library), `guids`, `_Utility`,
`engine`, `effect`, `soundengine`, `Igc`, `clintlib`, `training`, `WinTrek` (client); `mdlc`, `xmunge`,
`cvh`, `qhull` (art tools); `FedSrv` as a standalone LAN server without SQL.

**Tier 2 (parked):** `lobby`, `AGC`, `AllSrvUI`, `srvconfig`, `cliconfig`.

**Tier 3 (delete):**

| Module | Status |
|---|---|
| `MSRGuard`, `SymGuard`, `patch`, `fsmon`, `dreamcast` | Deleted. |
| `Zone` | Kept for now. `_Utility`, `clintlib`, `WinTrek` and `training` include `Zone.h` / `zauth.h`. Removed in Phase 2 together with the Zone auth code paths. |
| `Club` | Kept for now. `clintlib/pch.h` and `WinTrek/pch.h` include `Club/ClubMessages.h`. Removed with the Zone code paths. |
| `sentinal`, `fedperf` | Kept for now. `FedSrv` compiles `sentinal/config.cpp` and `fedperf/cntrmap.cpp` / `collect.cpp`. |
| `AutoUpdate` | Kept for now. Referenced by `lobby/pch.h` (Tier 2). |
| `test` | Kept for now. `AGC` depends on `test/TCAtl`, `TCObj` and `TCLib`. |

### Open issue: FedSrv depends on AGC

13 of the 23 `.cpp` files in `FedSrv` use AGC (the COM automation / admin event layer), and the
server compiles the `sentinal` and `fedperf` sources listed above. A standalone LAN server therefore
needs one of two things: AGC moves into Tier 1, or the admin/COM layer is stripped out of FedSrv. This
gets decided when FedSrv is ported.

## Findings that correct the first analysis

- The message-compiler sources exist. They use a `.msg` extension: `FedSrv/fedmsg.msg`,
  `sentinal/sentmsg.msg`, `lobby/lobbymsg.msg`, `Club/AllClubMsg.msg`. They need an `mc.exe` build
  step, not a reconstruction.
- `ddraw.h` and `d3d.h` (Direct3D 7-era immediate mode) are still in the Windows SDK; the first CI
  run resolved them. Compatibility shims are only needed for headers that really are gone:
  `d3drm.h`, `dplay.h` and `dplobby.h`.
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
- Still to do: projects for the remaining Tier 1 modules, an `mc.exe` build rule for `.msg` files, MIDL
  for the `.idl` files, and a vcpkg manifest (libjpeg-turbo, qhull, meshoptimizer).

### Phase 2: make it compile

- Two-phase lookup and `typename`, string-literal constness, for-loop scope, secure-CRT fixes.
- Old ATL 3 code (`statreg.cpp`, `atlimpl.cpp`, `CComModule`) moved to the current ATL module classes.
- Compile-time shims for the DirectX headers that are no longer in the SDK (`d3drm.h`, `dplay.h`,
  `dplobby.h`), so everything links before behaviour changes. The shims are written for this project, not vendored from
  old SDKs, and they are deleted as Phase 3 replaces each API.
- Zone and Club code paths removed. `/permissive-` turned on.

### Phase 3: make it run

| Subsystem | Today | Target |
|---|---|---|
| Rendering | DirectDraw 4 + Direct3D 3, MMX software rasterizer | D3D11 backend; a few shaders emulate the fixed-function pipeline (lights, materials, fog, colour-key alpha) |
| Input | DirectInput 2/7 | DirectInput 8 |
| Audio | DirectSound / DS3D | Unchanged at first; XAudio2 later |
| Networking | DirectPlay 4 inside `FedMessaging` | Own Winsock UDP layer: reliable-ordered and unreliable channels, fragmentation, timeouts, broadcast LAN discovery replacing `EnumSessions` |

The DirectPlay surface used by `FedMessaging` is about 15 calls: `Open`, `Close`, `EnumSessions`,
`GetSessionDesc`, `SetSessionDesc`, `CreatePlayer`, `DestroyPlayer`, `GetPlayerData`, `SendEx`,
`Receive`, `CancelMessage`, `CreateAddress` and `InitializeConnection`. Groups are already implemented
inside `FedMessaging`.

### Phase 4: art pipeline

`Art.proj` replaces the nmake/awk/sed/batch chain (`Artwork/256/makefile`, `makegeo.mak`,
`Artwork/tools/*.bat`, `*.awk`). It reads the existing manifests (`bitmap.txt`, `copymdl.txt`, the
geometry list in `makegeo.mak`) and builds incrementally through Inputs and Outputs.

| Step | Original | Replacement |
|---|---|---|
| Images | `mdlc -convert` (24-bit BMP to 16-bit MDL) | Same, rebuilt |
| Animations | `mdlc -compressanim` | Same |
| Flatten / extract / bounds | `xmunge` on Direct3D Retained Mode | `xmunge` on the engine's own `.x` parser (`engine/xfile.cpp`) |
| Levels of detail | MSR progressive-mesh binaries + `xmunge -reduce` | meshoptimizer at the original ratios (0.5 and 0.01) |
| Collision hulls | `cvh` + qhull | Same; qhull from vcpkg |
| Model compile | `sed` templates + `mdlc -optimize` | Template substitution in MSBuild, then `mdlc` |

`mdlc` links the whole engine, so art conversion is gated on the engine building and running. The repo
has no reference output, so Stage A is verified by loading and rendering in game, unless original
retail `.mdl` files are available to compare against.

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

## Conformance fix log

Fixes are made per CI round, and each change keeps the original runtime behaviour.

| Pattern | Fix | Sites |
|---|---|---|
| VC6 for-loop scope (loop variable used after the loop) | Declare the variable before the loop | `zlib/zstring.cpp` |
| Default-int declarations (`static Foo(`, `operator =` with no return type) | Explicit `void` / `bool` / `T&` return types | `_Utility/listwrappers.h`, `engine/value.h`, `engine/context.h`, `soundengine/ds3dutil.h`, `clintlib/badwords.cpp`, `WinTrek/WinTrek.cpp` |
| `CastMember(X);` with no such macro (VC6 read it as an undefined `int` member function) | Removed | `engine/geometry.h`, `engine/image.h` |
| `abs(float)` ambiguous: the float overloads now live in `<cmath>` | `zlib.h` includes `<cmath>` and `<cstdlib>` | `zlib` |
| C2712: `__try` in a function that needs object unwinding (`/EHsc`) | Body moved into `Win32MainBody`; `Win32Main` keeps only the SEH frame. Also `delete` changed to `delete[]` for the `new[]` spacer | `zlib/Win32app.cpp` |
| VC6 for-loop scope, the rest of Tier 1 (22 sites, including ones that compile but would now refer to a different variable) | Found with a scanner and hoisted the same way. `WinTrek/trekinput.cpp` reads `indexJoystick` after its loop has finished; that latent bug is kept as-is for now. | `engine`, `Igc`, `FedSrv`, `clintlib`, `training`, `WinTrek`, `cvh` |
| C3867: bare member-function name used as a pointer (about 420 sites, mostly `AddEventTarget(OnX, ...)` and rasterizer dispatch tables) | `&Class::Name`, found with a scanner that resolves the owning class | `engine`, `effect`, `WinTrek` |
| `d3drm.h` included only for a debug error-string table | Retained-mode entries and the include removed | `engine/dderror.cpp` |
| DirectInput 7 (`DirectInputCreate`, `IDirectInputDevice2`, `DIDEVTYPE_*`) not in the SDK | Ported to DirectInput 8. All DI8 game-controller types map to the engine's joystick. Unused dynamic-load path removed. `DIRECTINPUT_VERSION=0x0800` is set globally. | `engine/dinput.cpp` |
| C2712 in `ThingGeoImpl::~ThingGeoImpl` (`__try` in a destructor of a class with `TRef` members) | Guarded release moved into a static member that has no objects of its own. The members are released inside it, so the implicit member destructors afterwards do nothing. | `effect/thinggeo.cpp` |
| `dplay.h` / `dplobby.h` not in the SDK | Compile-only stand-ins in `compat/dplay/`. All GUIDs are null, so `CoCreateInstance` fails and the existing "DirectPlay unavailable" paths run. No call can reach `dplayx.dll` with a guessed ABI. Removed in Phase 3. | `Inc/messagecore.h` and users |
| Zone auth SDK (`zoneauth.h`, `zoneauthdecrypt.h`) not in the source release | `_Utility/zauth.cpp` reimplemented behind the same interface as "Zone unavailable": every authentication fails. The factories return objects rather than NULL because the callers never check for NULL. | `_Utility/zauth.cpp` |
| Debug `#define new new(__FILE__, __LINE__)` (`zlib/alloc.h`) breaks ATL's `new(std::nothrow)` | `push_macro`/`pop_macro` around the ATL include | `Inc/allegdb.h` |
