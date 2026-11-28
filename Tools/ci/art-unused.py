#!/usr/bin/env python3
#
# Lists the art files that nothing uses (MODERNIZATION.md, "Art folders").
# Report only: it never fails.
#
# It starts from what the game names directly and follows references,
# counting a file as used when a used thing names it:
#
#   - the code (client, server, mdlc, xmunge): every string literal, outside
#     comments; "<name>bmp" names an image. Of Source\GameApp\App\Artwork.h, only the
#     constants that are used. And the names it builds (CODE_BUILT);
#   - core files and maps (Data\*.igc) the code names: every name in them, as
#     a file of any type, and the images the code derives from one ("i", "l"
#     or "icon" + it; it + "loadoutpane", "hullinfo" or "lobby");
#   - MDL files: every name in them ("<name>bmp" an image, the rest other
#     files: sounds, models, namespaces);
#   - help pages (.mml): the pages and images their tags name (one-word fields);
#   - models (Artwork\Models.props): a used model's .x files (<name>.x,
#     _static, -low, _bound) and template, or its text MDL and .x files; the
#     textures a used .x file names (.ppm or .bmp: Source/NeuronClient/Engine/XFile.cpp);
#   - animations (Art.proj): the strip image; font.mdl: fontsource.mdl;
#     Music: tracks.lst and the tracks it lists.
#
# A name counts for every file of that name, whatever the folder, so the
# list errs toward keeping a file. Names the code builds at run time beyond
# the rules above are not seen: check a file before deleting it.
#
#   python Tools/ci/art-unused.py [repository root]

import os
import re
import sys
from collections import defaultdict

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..'))
ART = os.path.join(ROOT, 'Artwork')
FOLDERS = ['Audio', 'Data', 'Fonts', 'Interface', 'Models', 'Movies', 'Music', 'Textures', 'tools']
CODE_DIRS = ['Source', 'Tools/mdlc', 'Tools/xmunge']

IGC_PREFIXES = ['i', 'l', 'icon']                     # Loadout.cpp, Load.cpp, TeamPane.cpp
IGC_SUFFIXES = ['loadoutpane', 'hullinfo', 'lobby']   # Load.cpp, TeamScreen.cpp (civilization icon names)

# Names the code builds from pieces that are not a whole literal
CODE_BUILT = (
    ['globe%d' % n for n in range(1, 9)] +            # MapMakerIgc.cpp: a random map's backdrop, "globe" + 1..8
    ['invempty' + et for et in ('chaff', 'weapon', 'missile', 'mine', 'shield', 'cloak', 'ammo', 'afterburner')]
                                                      # InventoryImage.cpp: "invEmpty" + GetEquipmentTypeName
)

TOKEN = re.compile(r'[A-Za-z0-9_\-]+')

def read(path):
    return open(path, encoding='latin-1').read()

def strip_comments(text):
    return re.sub(r'(?<![:"\\])//[^\n]*', '', re.sub(r'/\*.*?\*/', '', text, flags=re.S))

#
# The files, by (folder, name, ext); a name in lower case, without the extension
#

files = {}
for folder in FOLDERS:
    path = os.path.join(ART, folder)
    if os.path.isdir(path):
        for f in os.listdir(path):
            name, ext = os.path.splitext(f)
            files[(folder, name.lower(), ext[1:].lower())] = os.path.join(path, f)

by_name = defaultdict(list)
for key in files:
    by_name[key[1]].append(key)

#
# What the art build makes from them
#

props = read(os.path.join(ART, 'Models.props'))
art_proj = read(os.path.join(ART, 'Art.proj'))
generated = {}                                     # generated MDL name -> the files it is built from

for name, template in re.findall(r'<ArtModel Include="([^"]+)"[^>]*Template="([^"]+)"', props):
    n = name.lower()
    generated[n] = [('Models', s, 'x') for s in (n, n + '_static', n + '-low', n + '_bound')] + [('tools', template.lower(), 'mdl')]
for name, text, xfiles in re.findall(r'<ArtTextModel Include="([^"]+)" Text="([^"]+)" XFiles="([^"]+)"', props):
    generated[name.lower()] = [('Models', text.lower(), 'mdl')] + [('Models', os.path.splitext(x)[0].lower(), 'x') for x in xfiles.split(';')]
for name in re.findall(r'<ArtAnimation Include="([^"]+)"', art_proj):
    generated[name.lower()] = [('Textures', name.lower(), 'dds')]
generated['font'] = [('Fonts', 'fontsource', 'mdl')]

#
# Reachability
#

used = set()                                       # file keys, and ('gen', name) for generated MDLs
any_names, image_names, other_names = set(), set(), set()
queue = []

def use(key):
    if key not in used and (key in files or key[0] == 'gen'):
        used.add(key)
        queue.append(key)

def name_any(n):
    any_names.add(n.lower())

def name_image(n):
    image_names.add(n.lower())

def name_other(n):
    other_names.add(n.lower())

def names_from_text(text, image_ok):
    # every token: "<name>bmp" an image; the rest other files (or any, from code)
    for t in TOKEN.findall(text):
        t = t.lower()
        if t.endswith('bmp') and len(t) > 3:
            name_image(t[:-3])
        (name_any if image_ok else name_other)(t)

def resolve():
    for n in list(any_names | image_names | other_names):
        for key in by_name.get(n, []):
            if key[2] == 'dds':
                if n in any_names or n in image_names:
                    use(key)
            elif n in any_names or n in other_names:
                use(key)
        if n in generated and (n in any_names or n in other_names):
            use(('gen', n))

# Roots: the code
artwork_h = os.path.join(ROOT, 'Source', 'GameApp', 'App', 'Artwork.h')
code = {}
for d in CODE_DIRS:
    for dirpath, _, fs in os.walk(os.path.join(ROOT, d)):
        for f in fs:
            if f.lower().endswith(('.cpp', '.h', '.inl', '.rc')):
                code[os.path.join(dirpath, f)] = strip_comments(read(os.path.join(dirpath, f)))

all_code = '\n'.join(t for f, t in code.items() if f != artwork_h)
for f, text in code.items():
    if f == artwork_h:
        for c, v in re.findall(r'#define\s+(\w+)\s+"([^"]+)"', text):
            if re.search(r'\b' + c + r'\b', all_code):
                names_from_text(v, True)
    else:
        for lit in re.findall(r'"((?:[^"\\\n]|\\.)*)"', text):
            names_from_text(lit.replace('\\\\', '\\'), True)

for n in CODE_BUILT:
    name_any(n)

# Music: the player reads Music\tracks.lst (Source/NeuronClient/Sound/RedbookSound.cpp)
use(('Music', 'tracks', 'lst'))

resolve()
while queue:
    while queue:
        key = queue.pop()
        if key[0] == 'gen':
            for dep in generated[key[1]]:
                use(dep)
            continue
        folder, name, ext = key
        path = files[key]
        if ext == 'igc':
            data = open(path, 'rb').read()
            for t in re.findall(rb'[A-Za-z0-9_\-]{2,}', data):
                t = t.decode().lower()
                name_any(t)
                for p in IGC_PREFIXES:
                    name_image(p + t)
                for s in IGC_SUFFIXES:
                    name_image(t + s)
        elif ext == 'mdl':
            names_from_text(strip_comments(read(path)), False)
        elif ext == 'mml':
            # <Tag|field|...>, tags nested in the last field; a field that
            # is one word may name a page or an image
            for fields in re.findall(r'<[A-Za-z]+((?:\|[^|<>]*)+)', read(path)):
                for field in fields.split('|')[1:]:
                    if TOKEN.fullmatch(field.strip()):
                        name_any(field.strip())
        elif ext == 'x':
            for t in re.findall(r'([A-Za-z0-9_\-]+)\.(?:ppm|bmp|dds)\b', read(path), flags=re.I):
                name_image(t)
        elif ext == 'lst':
            for line in read(path).splitlines():
                line = line.strip()
                if line:
                    use((folder, os.path.splitext(line)[0].lower(), os.path.splitext(line)[1][1:].lower()))
    resolve()

#
# Report
#

unused = sorted(k for k in files if k not in used)
unbuilt = sorted(n for n in generated if ('gen', n) not in used and re.search(r'Include="' + re.escape(n) + '"', props, re.I))
total = sum(os.path.getsize(files[k]) for k in unused)
by_folder = defaultdict(list)
for k in unused:
    by_folder[k[0]].append(k)

print(f'{len(unused)} of {len(files)} art files are not used ({total / 1048576:.1f} MB):')
if unbuilt:
    print(f'\nModels.props builds {len(unbuilt)} model(s) nothing uses:')
    for n in unbuilt:
        print(f'  {n}')
for folder in sorted(by_folder):
    ks = by_folder[folder]
    size = sum(os.path.getsize(files[k]) for k in ks)
    print(f'\n{folder} ({len(ks)}, {size / 1048576:.1f} MB):')
    for k in ks:
        print(f'  {os.path.basename(files[k])}')
