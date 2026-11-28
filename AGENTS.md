# AGENTS.md — Engineering rules for Allegiance

Operating instructions for every agent (and human) writing code in this repository. Read this before writing a line.

Allegiance is a 2000 Microsoft game, open-sourced in 2004, being modernised to C++20, Direct3D 11, XAudio2 and MsQuic on x64. Nearly all of the tree predates every rule below. **The rules govern what you write, not what you find.**

What is authoritative, in order:

1. **This file**: conformance, meaning naming, layout and how to work here.
2. **[MODERNIZATION.md](MODERNIZATION.md)**: design, meaning what the target is, what each phase changed and why, the source layout and the library layering. Read the section your task touches before you start.
3. **The surrounding code**: for anything neither covers, match the file you are editing.

If you think a rule is wrong, or your task cannot be done without deviating, **say so in your report. Never deviate silently.**

The naming and layout rules are carried over from the sibling repositories Outpost.Warzone and Nomad-Commander, so that engine code can move between the trees without a rename pass. This repository makes one exception, an owner decision: constants are `UPPER_CASE` (R3).

---

## 1. Naming convention

| Kind | Convention | Example |
|---|---|---|
| Type (class, struct, enum, concept, alias) | `PascalCase` | `SceneTarget`, `ServiceCommand` |
| Function, method | `PascalCase` | `FlushDeferred()` |
| Member variable | `m_camelCase` | `m_sceneDepth`, `m_deviceLost` |
| Static member (mutable) | `sm_camelCase` | `sm_activeDevice` |
| Global, file-scope static | `g_camelCase` | `g_frameCount` |
| Parameter | `_camelCase` | `_fileName`, `_shipId` |
| Local | `camelCase` | `pageIndex` |
| Constant (`constexpr`, `const` at namespace or class scope) | `UPPER_CASE` | `MAX_TEXTURE_PAGES`, `SOFT_DISTANCE` |
| Enumerator | `PascalCase` | `ServiceCommand::Install` |
| Macro | `UPPER_CASE` | `DEFINE_FEDMSG` |
| Namespace | `PascalCase` | `AlphaCoverage`, `ServiceHost` |
| File (new) | `PascalCase.cpp` / `.h` | `ServiceHost.cpp` |

**This table governs code you write.** The legacy tree does not follow it. `m_pmodel`, `c_cMaxPlayers`, `s_szService`, `fHas`, `szName`, `ImodelIGC` and the lowercase file names are grandfathered, not exemplars. Do not extend those patterns into new code. Do not mass-rename them either, and do not rename one as a side effect of another change: a rename is its own commit, with its own reason.

### The rules behind the table

**R1: The leading underscore on parameters is deliberate.** It is legal C++. The reserved forms are `_Uppercase`, anything containing `__`, and `_lowercase` at global scope, and a parameter is never at global scope. Never introduce a reserved form: no `_Impl`, no `__helper`, and no file-scope `_cache` (use `g_cache` in an anonymous namespace).

**R2: A type name carries no prefix.** An interface is `ServiceApp`, not `IServiceApp`. No `CFoo`, `SFoo`, `EFoo`, `IFoo`, `FooBase`, `AbstractFoo`, and no `_t` suffixes. Name the concept, and let the concrete types say what they are. The game's `IshipIGC` family and the `IServiceApp` added during the modernisation are legacy.

**R3: Constants are `UPPER_CASE`.** `constexpr` variables, and `const` variables at namespace or class scope, follow this rule; `sm_` is for mutable statics only, which are rare and must document their thread-safety. An `UPPER_CASE` name shares its spelling space with macros, and `windows.h` defines many (`ERROR`, `DELETE`, `IGNORE`, `MAX_PATH`, `TRANSPARENT`, `OPAQUE`). Before you name a constant, check that the name is not a macro in the Win32 or CRT headers: a collision fails with an error that names neither. Qualified names (`AlphaCoverage::THRESHOLD`) do not help, because the preprocessor runs first.

**R4: Acronyms capitalise as words**: `QuicTransport`, `HdrTarget`, `MsaaSamples`. Identifiers from an external SDK keep that SDK's spelling (`ID3D11Device`, `DXGI_FORMAT`, `HRESULT`, `XAUDIO2_VOICE_DETAILS`, `QUIC_STATUS`) and are never renamed to fit.

**R5: Template parameters are `PascalCase`**: `T`, `Fn`, `Count`, `Ts...`.

**R6: Units belong in names; types do not.** `distanceMeters`, `durationMs`, `angleRadians` and `sizeBytes` are encouraged. Never encode the type: no `pDevice`, `szName`, `cItems`, `dwFlags`, `iCount`. That includes booleans: no `bVisible`, `fVisible`, `m_bEnabled`. A boolean reads as a predicate (`m_visible`, `m_deviceLost`, `isOpen`, `hasTarget`). The legacy Hungarian is exactly what this rule bans; it stays where it is and spreads nowhere.

**R7: A new file is named for its primary type**, in `PascalCase`, `.h` / `.cpp` only: no `.hpp`, `.cc`, `.inl`, and no upper-case extensions (`.CPP`, `.H`). Template implementations live in the header. The per-project `pch.h` / `pch.cpp` keep the name MSBuild expects. Existing files keep their names, lowercase ones included. The source layout in MODERNIZATION.md says which library and folder a file belongs in.

**R8: `m_` marks encapsulated state, not every field.** A class with invariants prefixes its private members `m_`. A public aggregate, such as a `Desc` config struct, a constant buffer or a vertex, uses plain `camelCase` fields so that brace initialisation reads naturally. The wire records (`FM_*`, declared through `DEFINE_FEDMSG`) are legacy and keep their spelling.

### Worked example: the target style

```cpp
// Source/NeuronClient/Engine/TexturePage.h
#pragma once

namespace TexturePages
{

constexpr UINT MAX_PAGES = 64;                               // R3: constant → UPPER_CASE

enum class PageFault : uint8_t { NotFound, BadFormat, OutOfVideoMemory };

/// One texture page held on the device. R2: no prefix on the type; R8: private state carries m_.
class TexturePage
{
public:
  struct Desc                                                // R8: aggregate → plain fields
  {
    UINT widthTexels;                                        // R6: unit in the name
    UINT heightTexels;
    DXGI_FORMAT format;                                      // R4: SDK spelling kept as-is
  };

  [[nodiscard]] static bool Create(ID3D11Device* _device,   // R1: _ on parameters
                                   const Desc& _desc,
                                   TexturePage& _outPage) noexcept;

  [[nodiscard]] UINT PageId() const noexcept { return m_pageId; }

private:
  ID3D11Texture2D* m_texture = nullptr;
  UINT m_pageId = 0;
  bool m_deviceLost = false;                                 // R6: no b or f
};

} // namespace TexturePages
```

### Enforcement

[`.clang-tidy`](.clang-tidy) is the machine-readable statement of the table, and the single source of truth for the option values.

**CI gates the lines a change adds.** The `naming` job in [`windows-build.yml`](.github/workflows/windows-build.yml) runs [`Tools/ci/check-naming.py`](Tools/ci/check-naming.py). The script runs clang-tidy's `readability-identifier-naming` on every translation unit the change touches, restricted to the added lines, and fails on any finding there. It reports declarations, not uses: a legacy name you call is fine, but a declaration on a line you write or change is held to the table. So:

- If you change a legacy function's signature line, its parameters on that line are checked. Either rename them (in that function: a small, local rename) or keep your edit off the declaration line.
- A name an external API dictates (a callback signature, a `WinMain` parameter, an SDK struct you redeclare) takes `// NOLINT(readability-identifier-naming)` on its line, with the reason.

Run it before you push, from a Developer PowerShell (it needs clang-tidy 19 or newer; Visual Studio ships one at `VC\Tools\Llvm\x64\bin`):

```
python Tools/ci/check-naming.py --base origin/main --clang-tidy "<VS>\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
```

Three rules the config cannot express, and that review therefore has to carry:

- **R2 (type prefixes)**: clang-tidy cannot ban a present prefix or suffix. Grep your declarations for `\b(class|struct|using)\s+[ICSE][A-Z]` and for a trailing `Base`, `Abstract` or `Impl`.
- **R6 (Hungarian)**: `camelBack` accepts `pDevice`, `szName` and `fVisible`, and `m_camelBack` accepts `m_pDevice`. Read your new names.
- **R7 (file names)**: nothing checks that a new file is `PascalCase`. Look at the name when you add one.

---

## 2. Layout and formatting

[`.clang-format`](.clang-format) is the layout for **new files**: 2-space indent, 140 columns, Allman braces, pointer and reference bound left, includes never reordered. [`.editorconfig`](.editorconfig) covers what clang-format does not: CRLF, the encodings, the final newline, trailing whitespace and the non-C++ formats.

The tree is not formatted to it, and will not be (owner decision). Most of it is 4-space, with K&R and Allman braces mixed. So:

- **A new file** follows `.clang-format`, and may be run through clang-format whole.
- **In an existing file, match the file**: its indent, its brace style, its comment style. Do not run clang-format over it, not even over "your" lines: a 2-space block inside a 4-space function is worse than either. Your editor applies `.editorconfig`'s 2-space indent, so check the indentation of what you type in a 4-space file.
- **No repo-wide reformat, no bulk include reordering, no drive-by renames.** A diff whose reviewable content is buried in whitespace churn will be rejected.
- **Encodings.** Five sources and two resource scripts are Latin-1 (`.editorconfig` lists them). Leave them as they are; do not re-encode a file as a side effect.
- Do not silence a diagnostic with `#pragma warning(disable: ...)` to make a build pass. Fix the cause, or report it.

---

## 3. Build and verify

x64 only, toolset v145 (Visual Studio 2026), C++20 with `/permissive-`, MultiByte character set: the code is ANSI and uses the A-suffixed Win32 APIs. There is no CMake. If a build error tempts you to change the toolset, lower the language standard or switch a project's conformance mode, stop and report instead.

MODERNIZATION.md, under "Building", has the commands: the solution, the MsQuic restore, the unit tests and the art. The project files share one layout. A library lists its own folders, then those of the libraries it may use, and nothing else, which is what enforces the layering. When you add, remove or move a source file, edit the owning `.vcxproj` **and** its `.filters`.

CI (`.github/workflows/windows-build.yml`) is the authority:

- `build`: Debug and Release, each project on its own so that one failure does not hide another, then the six unit-test DLLs and the mdlc smoke test.
- `asan`: the libraries, the tests and mdlc under AddressSanitizer.
- `art`: the art build, with the tools from `build`.
- `naming`: the naming gate (section 1).

**A green build says nothing about whether the game draws, sounds or plays.** For anything touching rendering, input, audio, networking or art loading, run it. A cloud container has no GPU and no MSVC, so there a change is at best syntax-checked and built by CI. Say so.

**Report what you actually did.** "Builds in CI, not run" and "ran it and checked X" are different claims. Never imply the second when you did only the first.

---

## 4. Working rules

**Stay in scope.** Do what the task asks. Adjacent legacy code that offends you is not part of the task: note it in your report and move on.

**Keep MODERNIZATION.md true.** If your change completes, alters or invalidates something it describes, update it in the same change. Figures in it are measured, not estimated: if you quote a new one, say how you measured it.

**Prove code is dead before deleting it.** Grep the whole tree, including the `.vcxproj`, `.filters`, `.rc` and the art's `.mdl` sources, before concluding that nothing references a symbol.

**Commits and PRs.** Branch off `main`. Make small, focused commits, each with an imperative subject that describes the change, not the process. All CI jobs must be green before a merge. Never commit build output (`out/`, `packages/`), `.vs/` or `.user` files.

---

## 5. Before you hand work back

- [ ] Naming conforms to section 1: `_` on parameters, `m_` on class state, no Hungarian, no `I`/`C`/`Base` affixes, no Win32 macro names among the `UPPER_CASE` constants. `Tools/ci/check-naming.py` passes.
- [ ] Only the lines the task required were changed: no reformatting, no drive-by renames.
- [ ] New, removed or moved files are in the `.vcxproj` **and** `.filters` of every project involved.
- [ ] It builds in CI: say which jobs you saw green.
- [ ] If it touches rendering, input, audio, networking or art loading: it was **run**, or you said plainly that it was not.
- [ ] MODERNIZATION.md is updated if the change moved something it describes.
- [ ] Your report states plainly what you verified, what you assumed, and any rule here you had to bend.
