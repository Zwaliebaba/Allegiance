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
| `Zone` | Kept for now. `_Utility`, `clintlib`, `WinTrek` and `training` include `Zone.h` / `zauth.h`. Removed in Phase 2 together with the Zone auth code paths. |
| `Club` | Kept for now. `clintlib/pch.h` and `WinTrek/pch.h` include `Club/ClubMessages.h`. Removed with the Zone code paths. |
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
- Still to do: a vcpkg manifest (libjpeg-turbo, qhull, meshoptimizer) once the vendored copies are
  replaced.

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
| VC6 for-loop scope, the rest of Tier 1 (22 sites, including ones that compile but would now refer to a different variable) | Found with a scanner and hoisted the same way. `WinTrek/trekinput.cpp` read `indexJoystick` after its loop had finished; that bug is fixed separately (see below). | `engine`, `Igc`, `FedSrv`, `clintlib`, `training`, `WinTrek`, `cvh` |
| C3867: bare member-function name used as a pointer (about 420 sites, mostly `AddEventTarget(OnX, ...)` and rasterizer dispatch tables) | `&Class::Name`, found with a scanner that resolves the owning class | `engine`, `effect`, `WinTrek` |
| `d3drm.h` included only for a debug error-string table | Retained-mode entries and the include removed | `engine/dderror.cpp` |
| DirectInput 7 (`DirectInputCreate`, `IDirectInputDevice2`, `DIDEVTYPE_*`) not in the SDK | Ported to DirectInput 8. All DI8 game-controller types map to the engine's joystick. Unused dynamic-load path removed. `DIRECTINPUT_VERSION=0x0800` is set globally. | `engine/dinput.cpp` |
| C2712 in `ThingGeoImpl::~ThingGeoImpl` (`__try` in a destructor of a class with `TRef` members) | Guarded release moved into a static member that has no objects of its own. The members are released inside it, so the implicit member destructors afterwards do nothing. | `effect/thinggeo.cpp` |
| `dplay.h` / `dplobby.h` not in the SDK | Compile-only stand-ins in `compat/dplay/`. All GUIDs are null, so `CoCreateInstance` fails and the existing "DirectPlay unavailable" paths run. No call can reach `dplayx.dll` with a guessed ABI. Removed in Phase 3. | `Inc/messagecore.h` and users |
| Zone auth SDK (`zoneauth.h`, `zoneauthdecrypt.h`) not in the source release | `_Utility/zauth.cpp` reimplemented behind the same interface as "Zone unavailable": every authentication fails. The factories return objects rather than NULL because the callers never check for NULL. | `_Utility/zauth.cpp` |
| Debug `#define new new(__FILE__, __LINE__)` (`zlib/alloc.h`) breaks ATL's `new(std::nothrow)` | `push_macro`/`pop_macro` around the ATL include | `Inc/allegdb.h` |
| Original bug: when a hat direction is bound in the key-mapping UI, the stored joystick index was the leftover loop counter (always one past the last joystick). That broke the saved mapping, its "Joy N Hat ..." label and duplicate detection. | Use the joystick that axis 5 (the hat, which drives `m_ppboolHatButton`) is mapped to | `WinTrek/trekinput.cpp` |
| ICQ chat relay (`icqmapi.dll`, proprietary ICQ API, header not in the release, service gone) | Handler for `FM_S_ICQ_CHAT_ACK` made a no-op, which is what the original did when the DLL was missing. The hard-coded ICQ licence key is removed with it. | `WinTrek/WinTrek.cpp` |
| ATL 3 implementation files (`atlimpl.cpp`, `statreg.cpp`) included into precompiled headers | Removed; current ATL is header-only plus `atls.lib` | `FedSrv/pch.cpp`, `AGC/pch.cpp` |
| `<ZoneAuthDecrypt.h>` included but unused | Include removed | `FedSrv/pch.h` |
| Library pieces removed in C++17 (`std::bind1st`, `std::binary_function`) | `_HAS_AUTO_PTR_ETC=1` restores them for now; they get replaced with lambdas and plain function objects in the Phase 5 library cleanup | `soundengine`, `AGC`, `test/TCLib` |
| More default-int forms: `static IsXValid(...)`, `const c_numGrooveLevels = 3;` | Explicit `bool` / `int` | `Zone/Member.h`, `Zone/Squad.h`, `WinTrek/WinTrek.h` |
| `<assert.h>` pulled in by current CRT/ATL headers trips zlib's "own assert only" guard; the guard's `Don't` was also lexed as an unterminated character literal | `#undef assert` before `zlib.h` in TCLib; message reworded | `test/TCLib/pch.h`, `zlib/zassert.h` |
| qhull's `__STDC__` check: MSVC defines it only in standard C mode | `/std:c17` plus `/Zc:__STDC__` for qhull | `qhull/qhull.vcxproj` |
| zlib's global `swap(Type&, Type&)` template made every `std::sort` over a global-namespace type ambiguous (current algorithms call `swap` unqualified) | Replaced with `using std::swap;`, which does the same thing | `zlib/zmath.h` |
| `<typeinfo.h>` (pre-standard header) removed from the CRT | `<typeinfo>` | `test/`, `FedSrv/AdminUtil.h` |
| More typeless declarations (`static c = 0;`, `const cbData = ...;`) | Explicit `int` | `WinTrek/CharInfoScreen.cpp`, `WinTrek/ZoneSquad.cpp`, `AGC/AGCOrientation.cpp` |
| Member-function pointers the scanner could not attribute (inherited member, or taken through another object) | `&TeamPane::OnButtonSize`, `&ConsoleDataImpl::OnBackButton` | `WinTrek/teampane.cpp`, `WinTrek/console.cpp` |
| `new TEvent<ZString>>::Delegate(this)`: a stray `>` that VC6 accepted | `TEvent<ZString>::Delegate` | `WinTrek/leaderboardscreen.cpp` |
| zlib's `class IEventTarget` clashes with the DOM interface of the same name in the SDK's `mshtml.h`, which ATL's control/host headers pull in | Renamed to `IZEventTarget` (6 uses in 2 files) | `zlib/event.h`, `WinTrek/chatpane.cpp` |
| Dependent type names without `typename` in the connection-point macros (two-phase lookup) | `typedef typename ...` | `test/TCAtl/EventsCP.h`, `test/TCAtl/DualEventsCP.h` |
| `std::vector::begin()` used as a raw pointer (VC6's iterator was one) | `.data()` | `test/TCLib` |
| `__uuidof(StdComponentCategoriesMgr)`: the coclass declaration is gone from `comcat.h` | `CLSID_StdComponentCategoriesMgr` | `test/TCLib/TCLib.cpp` |
| zlib's debug `new` macro and the CRT `assert` against ATL in `TCAtl.h` and `AGC/pch.h` | Same `push_macro` / `#undef assert` treatment as before | `test/Inc/TCAtl.h`, `AGC/pch.h` |
| More `std::vector` iterators passed where ATL expects raw pointers (`CComEnum::Init`) | `.data()` / `.data() + size()` | `AGC/IAGCCollectionImpl.h`, `AGC/IAGCRangesImpl.h`, `AGC/TCStrings.cpp` |
| `TCLib.h` includes `zassert.h` after ATL may have defined the CRT `assert` | `#undef assert` first | `test/Inc/TCLib.h` |
