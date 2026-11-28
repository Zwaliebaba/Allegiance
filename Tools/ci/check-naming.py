#!/usr/bin/env python3
"""
The naming gate (AGENTS.md section 1): clang-tidy's readability-identifier-naming
on the lines a change adds, and nowhere else.

The legacy tree predates the convention and is exempt from it, so a whole-tree
run would report tens of thousands of grandfathered names and say nothing about
the change. This runs clang-tidy on each translation unit the change touches,
with --line-filter set to the added lines, and fails on any naming finding
there. A declaration on a line you change is held to the rules; one you only
call is not (the check reports declarations, not uses).

Each file is checked with its own project's include directories and
preprocessor definitions, read from its .vcxproj and msbuild/Allegiance.Cpp.props,
through clang's MSVC driver (clang-cl finds Visual Studio and the Windows SDK
itself). Other clang diagnostics (MSVC-isms clang rejects) are counted but do
not fail the gate: they are not what it checks.

  python Tools/ci/check-naming.py --base origin/main
  python Tools/ci/check-naming.py --base HEAD~1 --clang-tidy "C:/.../clang-tidy.exe"

Exit status: 0 clean, 1 naming findings, 2 the gate could not run.
"""

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE_EXTS = (".cpp", ".h")
TREES = ("Source", "Tests", "Tools")
EXCLUDED = re.compile(r"(^|/)resource\.h$", re.IGNORECASE)
FINDING = re.compile(r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?:warning|error): (?P<msg>.*) \[readability-identifier-naming[^\]]*\]$")
CLANG_ERROR = re.compile(r": error: .*\[clang-diagnostic-error\]$")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout


def native(path):
    return path.replace("\\", os.sep).replace("/", os.sep)


# The tree's own files, for --header-filter: not the SDK, not the generated headers in out/
HEADERS = r"(Source|Tests|Tools)[/\\\\]"


def key(path):
    """A path as the gate compares it: '/' separated, and case-insensitive on Windows"""
    path = path.replace("\\", "/")
    return path.lower() if os.name == "nt" else path


def rel(path):
    return os.path.relpath(os.path.abspath(path), ROOT).replace("\\", "/")


#
# The lines the change adds, per file
#

def added_lines(base):
    out = git("diff", "-M", "-U0", "--no-color", "--diff-filter=AMR", base, "HEAD", "--", *TREES)
    ranges, current = {}, None
    for line in out.splitlines():
        if line.startswith("+++ "):
            name = line[4:]
            current = name[2:] if name.startswith("b/") else None
            if current and (not current.endswith(SOURCE_EXTS) or EXCLUDED.search(current)):
                current = None
        elif line.startswith("@@") and current:
            m = re.match(r"@@ -\S+ \+(\d+)(?:,(\d+))? @@", line)
            start, count = int(m.group(1)), int(m.group(2) or "1")
            if count:
                ranges.setdefault(current, []).append([start, start + count - 1])
    return ranges


#
# A file's project: its include directories, definitions and precompiled header
#

def find_project(path):
    d = os.path.dirname(os.path.join(ROOT, path))
    while len(d) >= len(ROOT):
        found = glob.glob(os.path.join(d, "*.vcxproj"))
        if found:
            return found[0]
        d = os.path.dirname(d)
    return None


def condition_applies(element, configuration):
    cond = element.get("Condition")
    return cond is None or ("'%s'" % configuration) in cond


def split_list(text):
    return [item.strip() for item in (text or "").split(";") if item.strip() and not item.strip().startswith("%(")]


class Project:
    def __init__(self, vcxproj, configuration, unit_test_dir):
        self.path = vcxproj
        self.dir = os.path.dirname(vcxproj)
        self.name = os.path.splitext(os.path.basename(vcxproj))[0]
        self.macros = {
            "AllegianceRoot": ROOT + os.sep,
            "ProjectDir": self.dir + os.sep,
            "MSBuildProjectName": self.name,
            "Configuration": configuration,
            "Platform": "x64",
            "IntDir": os.path.join(ROOT, "out", "obj", "x64", configuration, self.name) + os.sep,
            "UnitTestDir": (unit_test_dir or "") + os.sep,
            "VCInstallDir": os.environ.get("VCINSTALLDIR", ""),
        }
        quic = glob.glob(os.path.join(ROOT, "packages", "Microsoft.Native.Quic.MsQuic.Schannel.*", "build", "native"))
        self.macros["MsQuicDir"] = (quic[0] + os.sep) if quic else ""

        self.includes, self.defines = [], []
        self.no_pch = set()
        self.files = []

        for sheet in (os.path.join(ROOT, "msbuild", "Allegiance.Cpp.props"), vcxproj):
            self._read(sheet, configuration)

    def expand(self, text):
        """A path with its macros expanded, or None where a macro is unknown
        here (a package that is not restored, say)"""
        missing = []

        def value(m):
            v = self.macros.get(m.group(1), "")
            if not v.strip(os.sep):
                missing.append(m.group(1))
            return v

        path = re.sub(r"\$\((\w+)\)", value, text)
        return None if missing else os.path.normpath(native(path))

    def _read(self, sheet, configuration):
        root = ET.parse(sheet).getroot()
        for element in root.iter():             # with or without the MSBuild namespace
            element.tag = element.tag.split("}")[-1]
        for group in root.findall("ItemDefinitionGroup"):
            if not condition_applies(group, configuration):
                continue
            for cl in group.findall("ClCompile"):
                for item in cl:
                    if not condition_applies(item, configuration):
                        continue
                    if item.tag == "AdditionalIncludeDirectories":
                        self.includes += [d for d in (self.expand(i) for i in split_list(item.text)) if d]
                    elif item.tag == "PreprocessorDefinitions":
                        self.defines += split_list(item.text)
        for group in root.findall("ItemGroup"):
            for cl in group.findall("ClCompile"):
                path = os.path.normpath(os.path.join(os.path.dirname(sheet), native(cl.get("Include"))))
                self.files.append(path)
                pch = cl.find("PrecompiledHeader")
                if pch is not None and pch.text == "NotUsing":
                    self.no_pch.add(path)


#
# The translation unit that compiles a changed file: the file itself, or for a
# header a .cpp of its project that includes it (its pch.cpp, failing that)
#

def unit_for(path, project):
    full = os.path.normpath(os.path.join(ROOT, path))
    if full.endswith(".cpp"):
        return full
    name = os.path.basename(full).lower()
    include = re.compile(r'#\s*include\s*["<]([^">]+)[">]')
    for cpp in project.files:
        try:
            with open(cpp, encoding="utf-8", errors="replace") as f:
                for line in f:
                    m = include.search(line)
                    if m and os.path.basename(m.group(1)).lower() == name:
                        return cpp
        except OSError:
            pass
    pch = os.path.join(project.dir, "pch.cpp")
    return pch if os.path.exists(pch) else None


def stub_shader_headers(project):
    """The shaders' headers come from FXC at build time; clang-tidy needs only
    their names, so empty arrays stand in when no build has run."""
    int_dir = project.macros["IntDir"]
    for hlsl in glob.glob(os.path.join(project.dir, "**", "*.hlsl"), recursive=True):
        name = os.path.splitext(os.path.basename(hlsl))[0]
        header = os.path.join(int_dir, name + ".h")
        if not os.path.exists(header):
            os.makedirs(int_dir, exist_ok=True)
            with open(header, "w") as f:
                f.write("#pragma once\nstatic const unsigned char g_%s[] = { 0 };\n" % name)


def compiler_args(project, driver, extra):
    if driver == "cl":
        args = ["--driver-mode=cl", "/std:c++20", "/EHsc", "/permissive-", "/TP", "/D_MBCS"]
        args += ["/D" + d for d in project.defines]
        args += ["/I" + i for i in project.includes if i]
    else:
        args = ["-x", "c++", "-std=c++20", "-fms-extensions"]
        args += ["-D" + d for d in project.defines]
        args += ["-I" + i for i in project.includes if i]
    return args + extra


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", required=True, help="the revision the change is measured against")
    parser.add_argument("--clang-tidy", default=shutil.which("clang-tidy") or "clang-tidy")
    parser.add_argument("--configuration", default="Debug")
    parser.add_argument("--unit-test-dir", help="VC\\Auxiliary\\VS\\UnitTest of the Visual Studio installation")
    parser.add_argument("--driver", choices=["cl", "gnu"], default="cl" if os.name == "nt" else "gnu")
    parser.add_argument("--extra-arg", action="append", default=[], help="passed to the compiler (repeatable)")
    parser.add_argument("--config", default=os.path.join(ROOT, ".clang-tidy"), help="clang-tidy 19 or newer reads the repository's own")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    try:
        ranges = added_lines(args.base)
    except subprocess.CalledProcessError as e:
        print("check-naming: git diff against %s failed: %s" % (args.base, e.stderr.strip()))
        return 2

    if not ranges:
        print("check-naming: no C++ lines added since %s" % args.base)
        return 0

    #
    # Which lines count is decided here, not by clang-tidy's --line-filter: its
    # JSON names every changed file, and a large change (a rename across the
    # tree) makes it longer than a Windows command line may be. clang-tidy
    # reports every naming finding in the tree's own files, and only those on
    # added lines are kept.
    #

    canary = os.path.join("out", "check-naming", "Canary.cpp").replace(os.sep, "/")
    os.makedirs(os.path.join(ROOT, os.path.dirname(canary)), exist_ok=True)
    with open(os.path.join(ROOT, canary), "w") as f:
        f.write("int CanaryNotAGlobalName = 0;\n")
    wanted = {key(f): r for f, r in ranges.items()}
    wanted[key(canary)] = [[1, 1]]

    def on_added_line(file, line):
        return any(start <= line <= end for start, end in wanted.get(key(rel(file)), []))

    projects, units = {}, {}
    for path in sorted(ranges):
        vcxproj = find_project(path)
        if not vcxproj:
            print("check-naming: %s belongs to no project; not checked" % path)
            continue
        if vcxproj not in projects:
            projects[vcxproj] = Project(vcxproj, args.configuration, args.unit_test_dir)
            stub_shader_headers(projects[vcxproj])
        project = projects[vcxproj]
        unit = unit_for(path, project)
        if not unit:
            print("check-naming: no translation unit compiles %s; not checked" % path)
            continue
        units.setdefault(unit, project)

    def tidy(unit, compiler):
        cmd = [args.clang_tidy, unit, "--quiet",
               "--config-file=" + args.config,
               "--checks=-*,readability-identifier-naming",
               "--warnings-as-errors=-*",
               "--header-filter=" + HEADERS,
               "--"] + compiler
        if args.verbose:
            print(" ".join('"%s"' % c if " " in c else c for c in cmd))
        return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")

    #
    # The canary: a known violation through the same filter. If clang-tidy does
    # not report it, the gate cannot see anything and must not say "clean".
    #

    canary_args = ["--driver-mode=cl", "/TP"] if args.driver == "cl" else ["-x", "c++"]
    result = tidy(os.path.join(ROOT, canary), canary_args)
    output = (result.stdout + result.stderr).splitlines()
    if not any((m := FINDING.match(line.strip())) and "CanaryNotAGlobalName" in line and on_added_line(m.group("file"), int(m.group("line")))
               for line in output):
        print("check-naming: the canary was not reported, so the gate cannot see findings; clang-tidy said:")
        print("\n".join(output)[-2000:] or "(nothing)")
        return 2

    findings, broken = set(), 0
    for unit, project in sorted(units.items()):
        result = tidy(unit, compiler_args(project, args.driver, args.extra_arg))
        errors, recognised, first_error = 0, False, ""
        for line in (result.stdout + result.stderr).splitlines():
            m = FINDING.match(line.strip())
            if m:
                if on_added_line(m.group("file"), int(m.group("line"))):
                    findings.add((rel(m.group("file")), int(m.group("line")), int(m.group("col")), m.group("msg")))
                recognised = True
            elif CLANG_ERROR.search(line):
                errors += 1
                recognised = True
                if errors == 1:
                    first_error = line.strip()

        # A failure that is neither a finding nor a compile error is clang-tidy
        # itself not running (a bad config, a missing binary): never "clean"
        if result.returncode != 0 and not recognised:
            print("check-naming: clang-tidy failed on %s:" % rel(unit))
            print((result.stdout + result.stderr).strip()[-2000:])
            return 2
        if errors:
            broken += 1
            print("check-naming: %s: clang reported %d compile errors (not part of the gate), the first: %s" % (rel(unit), errors, first_error))

    print("check-naming: %d files changed, %d translation units checked" % (len(ranges), len(units)))
    if broken:
        print("check-naming: %d of them did not parse cleanly under clang; declarations clang could not see were not checked" % broken)

    for file, line, col, msg in sorted(findings):
        print("::error file=%s,line=%d,col=%d::%s (AGENTS.md section 1)" % (file, line, col, msg))
    if findings:
        print("check-naming: %d naming findings on added lines" % len(findings))
        return 1

    print("check-naming: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
