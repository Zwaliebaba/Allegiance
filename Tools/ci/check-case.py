#!/usr/bin/env python3
"""
File names and their spelling (AGENTS.md R7).

MSVC opens an #include, and MSBuild a project entry, whatever its case, so a
wrongly spelled name builds on Windows and fails only on a case-sensitive file
system, or quietly survives until the next rename. This checks, over the whole
tree:

  1. every source file is named in PascalCase (.cpp, .h, .hlsl, .hlsli), apart
     from pch.h/pch.cpp (MSBuild's names), resource.h (the resource editor's)
     and the vendored Qhull sources;
  2. no two tracked files differ only in case;
  3. every #include of a file in the tree, in C++, HLSL and resource scripts,
     spells it as the file is spelled, folders included; the headers FXC
     generates from the shaders count as files in the tree;
  4. every ClCompile/ClInclude/FxCompile/None/ResourceCompile entry in a
     .vcxproj or .filters names a file with its exact case.

  python Tools/ci/check-case.py

Exit status: 0 clean, 1 findings.
"""

import collections
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SOURCE_EXTS = (".cpp", ".h", ".hlsl", ".hlsli")
INCLUDING_EXTS = SOURCE_EXTS + (".c", ".rc", ".rc2", ".RC")
PROJECT_EXTS = (".vcxproj", ".filters")
VENDORED = ("Tools/qhull/",)
EXEMPT_NAMES = {"pch.h", "pch.cpp", "resource.h"}
PASCAL = re.compile(r"^[A-Z][A-Za-z0-9]*\.(cpp|h|hlsl|hlsli)$")
INCLUDE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
ITEM_TAGS = {"ClCompile", "ClInclude", "FxCompile", "None", "ResourceCompile", "Image", "Text"}


def tracked():
    out = subprocess.run(["git", "ls-files"], cwd=ROOT, check=True, capture_output=True, text=True).stdout
    return out.split("\n")[:-1] if out.endswith("\n") else out.split("\n")


def main():
    files = [f for f in tracked() if f]
    exact = set(files)
    by_lower = collections.defaultdict(set)
    for f in files:
        by_lower[f.lower()].add(f)

    # The shaders' generated headers ($(IntDir)<shader>.h), found by name
    generated = {}
    for f in files:
        if f.endswith(".hlsl"):
            name = os.path.splitext(os.path.basename(f))[0] + ".h"
            generated[name.lower()] = name

    findings = []

    # 1. PascalCase names
    for f in files:
        name = os.path.basename(f)
        if f.startswith(VENDORED) or not f.startswith(("Source/", "Tests/", "Tools/")):
            continue
        if name.endswith(SOURCE_EXTS) and name.lower() not in EXEMPT_NAMES and not PASCAL.match(name):
            findings.append("%s: not a PascalCase file name (AGENTS.md R7)" % f)

    # 2. Names that differ only in case
    for lower, spellings in by_lower.items():
        if len(spellings) > 1:
            findings.append("%s: tracked in more than one case: %s" % (lower, ", ".join(sorted(spellings))))

    # 3. #include spellings
    for f in files:
        if not f.endswith(INCLUDING_EXTS) or f.startswith(VENDORED):
            continue
        try:
            with open(os.path.join(ROOT, f), encoding="latin-1") as handle:
                lines = handle.readlines()
        except OSError:
            continue
        here = os.path.dirname(f)
        for number, line in enumerate(lines, 1):
            m = INCLUDE.match(line)
            if not m:
                continue
            spelled = m.group(2).replace("\\", "/")
            # Candidates: the path as written from the including file's folder,
            # and any tracked file whose path ends with it
            relative = os.path.normpath(os.path.join(here, spelled)).replace("\\", "/")
            if relative in exact:
                continue
            if relative.lower() in by_lower:
                findings.append("%s:%d: #include %s, but the file is %s" % (f, number, spelled, sorted(by_lower[relative.lower()])[0]))
                continue
            suffix = "/" + spelled.lower().lstrip("./")
            matches = [t for t in files if ("/" + t.lower()).endswith(suffix)]
            if matches:
                if not any(("/" + t).endswith("/" + spelled.lstrip("./")) for t in matches):
                    findings.append("%s:%d: #include %s, but the file is %s" % (f, number, spelled, matches[0]))
                continue
            name = os.path.basename(spelled)
            if "/" not in spelled and name.lower() in generated and name != generated[name.lower()]:
                findings.append("%s:%d: #include %s, but the shader's header is %s" % (f, number, spelled, generated[name.lower()]))

    # 4. Project entries
    for f in files:
        if not f.endswith(PROJECT_EXTS):
            continue
        project_dir = os.path.dirname(f)
        root = ET.parse(os.path.join(ROOT, f)).getroot()
        for element in root.iter():
            tag = element.tag.split("}")[-1]
            include = element.get("Include")
            if tag not in ITEM_TAGS or not include or "$(" in include or "*" in include:
                continue
            path = os.path.normpath(os.path.join(project_dir, include.replace("\\", "/"))).replace("\\", "/")
            if path in exact:
                continue
            if path.lower() in by_lower:
                findings.append("%s: %s, but the file is %s" % (f, include, sorted(by_lower[path.lower()])[0]))
            else:
                findings.append("%s: %s names no tracked file" % (f, include))

    for line in findings:
        print(line)
    print("check-case: %d files, %d findings" % (len(files), len(findings)))
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
