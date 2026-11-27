#!/usr/bin/env python3
#
# Checks that every art reference resolves in the art folders (MODERNIZATION.md,
# "Art folders"), the way the client and server look files up (zlib/artfolders.h:
# a bare name, in the folders for its type, then the art root):
#
#   - no file name is in two folders that are searched for the same type
#     (one would hide the other);
#   - the MDL files: ImportWave sounds, images ("<name>bmp"), "use" imports;
#   - the core files (Data\*.igc): ship, station, drone, missile, probe, mine,
#     chaff and projectile models, icons, textures, and the investment-screen
#     images ("i" + the model name) of stations (but the flag pedestal, which
#     is never offered), drones and research;
#   - the code: images, namespaces and models named by a string literal, and
#     the artwork.h constants that are used.
#
# It reads the sources (Artwork\<folder>) plus what Artwork\Art.proj generates
# (the models of Models.props, the animation MDLs, font.mdl).
#
#   python tools/ci/art-check.py [repository root]
#
# Exit code 1 lists the problems.

import os
import re
import struct
import sys
from collections import defaultdict

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..'))
ART = os.path.join(ROOT, 'Artwork')

# zlib/artfolders.h
FOLDERS = {
    'dds': ['Textures', 'Models'],
    'mdl': ['Interface', 'Models', 'Fonts'],
    'x':   ['Models'],
    'cvh': ['Models'],
    'mml': ['Interface'],
    'wav': ['Audio'],
    'mp4': ['Movies'],
    'igc': ['Data'],
    'rtf': ['Data'],
}

problems = []

def problem(text):
    problems.append(text)

#
# What each folder will hold: its files, and what the art build adds
#

contents = defaultdict(lambda: defaultdict(set))     # folder -> ext -> names (lower case)

for folder in set(f for fs in FOLDERS.values() for f in fs) | {'Music'}:
    path = os.path.join(ART, folder)
    if os.path.isdir(path):
        for f in os.listdir(path):
            name, ext = os.path.splitext(f)
            contents[folder][ext[1:].lower()].add(name.lower())

props = open(os.path.join(ART, 'Models.props'), encoding='utf-8-sig').read()
models = [m.lower() for m in re.findall(r'<ArtModel Include="([^"]+)"', props)]
textmodels = [m.lower() for m in re.findall(r'<ArtTextModel Include="([^"]+)"', props)]
contents['Models']['mdl'].update(models + textmodels)
contents['Models']['cvh'].update(models)
art_proj = open(os.path.join(ART, 'Art.proj'), encoding='utf-8-sig').read()
contents['Interface']['mdl'].update(m.lower() for m in re.findall(r'<ArtAnimation Include="([^"]+)"', art_proj))
contents['Fonts']['mdl'].add('font')

def resolves(name, ext):
    return any(name.lower() in contents[f][ext] for f in FOLDERS.get(ext, []))

#
# The same name in two folders searched for one type
#

for ext, folders in FOLDERS.items():
    seen = {}
    for folder in folders:
        for name in contents[folder][ext]:
            if name in seen:
                problem(f'{name}.{ext} is in both {seen[name]} and {folder}; the {folder} one is never found')
            else:
                seen[name] = folder

#
# Namespaces the code creates (MDL "use" of them needs no file)
#

code_files = []
for d in ['WinTrek', 'effect', 'engine', 'clintlib', 'training', 'Igc', 'zlib', 'soundengine', '_Utility']:
    for dirpath, _, files in os.walk(os.path.join(ROOT, d)):
        code_files += [os.path.join(dirpath, f) for f in files if f.lower().endswith(('.cpp', '.h'))]

code = {f: open(f, encoding='latin-1').read() for f in code_files}
code_namespaces = set()
for text in code.values():
    code_namespaces.update(n.lower() for n in re.findall(r'CreateNameSpace\s*\(\s*"([^"]+)"', text))

#
# MDL files
#

def strip_comments(text):
    return re.sub(r'//[^\n]*', '', re.sub(r'/\*.*?\*/', '', text, flags=re.S))

for folder in ['Interface', 'Models', 'Fonts']:
    path = os.path.join(ART, folder)
    for f in sorted(os.listdir(path)):
        if not f.lower().endswith('.mdl'):
            continue
        text = strip_comments(open(os.path.join(path, f), encoding='latin-1').read())
        where = f'{folder}/{f}'
        for name in re.findall(r'ImportWave\(\s*"([^"]+)"', text):
            if not resolves(name, 'wav'):
                problem(f'{where}: sound {name}.wav not found')
        for name in re.findall(r'"([A-Za-z0-9_\-]+)bmp"', text):
            if not resolves(name, 'dds'):
                problem(f'{where}: image {name}.dds not found')
        for name in re.findall(r'\buse\s+"([^"]+)"', text):
            if name.lower() not in code_namespaces and not resolves(name, 'mdl'):
                problem(f'{where}: use "{name}": no {name}.mdl, and the code creates no such namespace')

#
# Core files
#

def cstr(data, offset, size):
    return data[offset:offset + size].split(b'\0')[0].decode('latin-1').lower()

BUYABLE = {29: 'hull type', 30: 'part type', 31: 'station type', 32: 'development', 33: 'drone type'}
OBJECT = {22: 'projectile type', 23: 'missile type', 24: 'mine type', 25: 'probe type', 26: 'chaff type'}

# DataStationTypeIGC.sabmCapabilities, and c_sabmPedestal: the flag pedestal,
# which the server places itself in a capture-the-flag game; it is never
# offered on the investment screen (it requires a tech nothing grants)
STATION_ABILITIES = 452
PEDESTAL = 0x400

for f in sorted(contents['Data']['igc']):
    data = open(os.path.join(ART, 'Data', f + '.igc'), 'rb').read()
    if len(data) < 8:
        continue
    size = struct.unpack_from('<i', data, 4)[0]
    if size + 8 != len(data):
        continue                                   # not a core file (a map)
    p, end = 8, 8 + size
    while p < end:
        otype, rsize = struct.unpack_from('<hi', data, p)
        rec = data[p + 6:p + 6 + rsize]
        p += 6 + rsize
        where = f'Data/{f}.igc'
        if otype in BUYABLE:                       # DataBuyableIGC: modelName[14] at 8, iconName[13] at 22
            kind = BUYABLE[otype]
            model, icon = cstr(rec, 8, 14), cstr(rec, 22, 13)
            if icon and not resolves(icon, 'dds'):
                problem(f'{where}: {kind} icon {icon}.dds not found')
            if otype in (29, 31) and model and not resolves(model, 'mdl'):
                problem(f'{where}: {kind} model {model}.mdl not found')
            pedestal = otype == 31 and struct.unpack_from('<H', rec, STATION_ABILITIES)[0] & PEDESTAL
            if otype in (31, 32, 33) and not pedestal and model and model.isalnum() and not resolves('i' + model, 'dds'):
                problem(f'{where}: {kind} {model}: investment image i{model}.dds not found')
        elif otype in OBJECT:                      # DataObjectIGC: modelName[13] at 24, textureName[13] at 37
            kind = OBJECT[otype]
            model, texture = cstr(rec, 24, 13), cstr(rec, 37, 13)
            if model and model[0].isalpha() and not resolves(model, 'mdl'):
                problem(f'{where}: {kind} model {model}.mdl not found')
            if texture and not resolves(texture, 'dds'):
                problem(f'{where}: {kind} texture {texture}.dds not found')

#
# Code: literal names, and the artwork.h constants in use
#

artwork_h = code.get(os.path.join(ROOT, 'WinTrek', 'artwork.h'), '')
constants = dict(re.findall(r'#define\s+(AWF_\w+)\s+"([^"]+)"', artwork_h))
all_code = '\n'.join(t for f, t in code.items() if not f.endswith('artwork.h'))
used = {c: v for c, v in constants.items() if re.search(r'\b' + c + r'\b', all_code)}

for f, text in code.items():
    text = strip_comments(text)
    where = os.path.relpath(f, ROOT)
    for name in re.findall(r'Load(?:Image|Surface)\s*\(\s*"([a-z0-9_]+)bmp"', text):
        if not resolves(name, 'dds'):
            problem(f'{where}: image {name}.dds not found')
    for name in re.findall(r'(?:GetNameSpace|LoadGeo)\s*\(\s*"([^"]+)"', text):
        if name.lower() not in code_namespaces and not resolves(name, 'mdl'):
            problem(f'{where}: {name}.mdl not found')

for c, v in sorted(used.items()):
    if v.endswith('bmp') and not resolves(v[:-3], 'dds'):
        problem(f'WinTrek/artwork.h: {c} names {v[:-3]}.dds, which is not found')

#
# Report
#

counts = ', '.join(f'{folder} {sum(len(n) for n in contents[folder].values())}' for folder in sorted(contents))
print(f'Art folders: {counts}')

if problems:
    print(f'{len(problems)} problem(s):')
    for p in problems:
        print('  ' + p)
    sys.exit(1)

print('Every reference resolves.')
