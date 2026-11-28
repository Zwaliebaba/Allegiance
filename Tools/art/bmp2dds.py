#!/usr/bin/env python3
"""
The one-time conversion of Artwork/256's BMP sources to the DDS files the repo
now holds as source art (see MODERNIZATION.md, "Textures: 32-bit DDS"), and the
tool to rebuild a model texture's mip levels after editing it.

    python Tools/art/bmp2dds.py convert Artwork/256          # every *.bmp -> *.dds
    python Tools/art/bmp2dds.py mips Artwork/256/foo.dds     # redo foo.dds's mip chain
    python Tools/art/bmp2dds.py premultiply Artwork/256/foo.dds
                                    # straight alpha -> premultiplied (and its mips)

The output is what mdlc -dds / -ddsmips wrote, byte for byte (the art build
checked that before the BMPs were deleted):

  * uncompressed 32-bit BGRA (DDS A8R8G8B8), rows top down;
  * a pixel is transparent (alpha 0, and RGB 0) where the game's RGB565
    conversion of it was 0: the colour key, near-black included;
  * textures that a model's .x file names (TextureFilename) get a mip chain,
    each level a 2x2 box filter of the one above, down to 1x1.

The images hold premultiplied alpha: each colour channel is already multiplied
by alpha, so RGB never exceeds alpha, and alpha 0 means RGB 0. The converted art
only has alpha 0 and 255, where that is the same as straight alpha. Editors
usually save straight alpha; run "premultiply" on such a file after saving it
(it rebuilds the mip chain if the file has one). For a model texture saved
without its mips, run "mips" afterwards. The box filter is the right one for
premultiplied pixels. A model texture's alpha decides how it is drawn: only 0 and
255 is a cut-out (antialiased, its mip levels keeping its coverage), any more than
a trace in between is translucent (glass, shields), drawn in the scene's
translucent pass; see MODERNIZATION.md, "Translucent surfaces". Soft alpha in UI
and HUD images blends as before.

Needs numpy.
"""

import os
import re
import struct
import sys

import numpy as np

# --------------------------------------------------------------------------
# BMP reading, as LoadImage(LR_CREATEDIBSECTION) + GetDIBits(32 bpp) see it
# --------------------------------------------------------------------------

def read_bmp(path):
    """Return (bgr, raw16): bgr is an (h, w, 3) uint8 array, top down; raw16 is
    the 16-bit 555 pixels for 16-bit files (else None)."""
    data = open(path, 'rb').read()
    if data[:2] != b'BM':
        raise ValueError(path + ': not a BMP file')
    offset = struct.unpack_from('<I', data, 10)[0]
    header = struct.unpack_from('<I', data, 14)[0]
    width, height, planes, bpp, compression = struct.unpack_from('<iiHHI', data, 18)
    colors_used = struct.unpack_from('<I', data, 46)[0] if header >= 40 else 0
    if compression != 0:
        raise ValueError(path + ': compressed BMPs are not handled')

    top_down = height < 0
    h = abs(height)
    w = width
    stride = (w * bpp + 31) // 32 * 4
    rows = np.frombuffer(data, dtype=np.uint8, count=stride * h, offset=offset).reshape(h, stride)
    if not top_down:
        rows = rows[::-1]

    raw16 = None
    if bpp == 24:
        bgr = rows[:, :w * 3].reshape(h, w, 3).copy()
    elif bpp == 8:
        count = colors_used or 256
        palette = np.frombuffer(data, dtype=np.uint8, count=count * 4, offset=14 + header).reshape(count, 4)
        bgr = palette[rows[:, :w], :3].copy()
    elif bpp == 16:
        raw16 = rows[:, :w * 2].copy().view('<u2').reshape(h, w)
        b5 = (raw16 & 0x1f).astype(np.uint32)
        g5 = ((raw16 >> 5) & 0x1f).astype(np.uint32)
        r5 = ((raw16 >> 10) & 0x1f).astype(np.uint32)
        # GDI's 555 -> 888 widening, as GetDIBits does it
        widen = lambda v: ((v << 3) | (v >> 2)).astype(np.uint8)
        bgr = np.stack([widen(b5), widen(g5), widen(r5)], axis=-1)
    else:
        raise ValueError('%s: %d-bit BMPs are not handled' % (path, bpp))

    return bgr, raw16


# --------------------------------------------------------------------------
# The colour key, as the engine converted to RGB565
# --------------------------------------------------------------------------

def keyed(bgr, raw16):
    """True where the pixel was 0 in RGB565."""
    if raw16 is not None:
        # BltConvert's 555 -> 565 shift
        return (raw16 & 0x7fff) == 0

    # PixelFormat::MakeColor / MakePixel in float32: int(v / 255 * size)
    def to(v, size):
        return (v.astype(np.float32) / np.float32(255) * np.float32(size)).astype(np.int32)

    b, g, r = bgr[..., 0], bgr[..., 1], bgr[..., 2]
    return (to(r, 31) == 0) & (to(g, 63) == 0) & (to(b, 31) == 0)


def bgra_from_bmp(path):
    bgr, raw16 = read_bmp(path)
    h, w, _ = bgr.shape
    out = np.empty((h, w, 4), dtype=np.uint8)
    out[..., :3] = bgr
    out[..., 3] = 255
    out[keyed(bgr, raw16)] = 0
    return out


# --------------------------------------------------------------------------
# Mip levels and the DDS file (Source/NeuronClient/Engine/dds.cpp's layout)
# --------------------------------------------------------------------------

def mip_chain(level0):
    levels = [level0]
    while levels[-1].shape[0] > 1 or levels[-1].shape[1] > 1:
        src = levels[-1].astype(np.uint32)
        h, w = src.shape[:2]
        hn, wn = max(1, h // 2), max(1, w // 2)
        y0 = np.minimum(2 * np.arange(hn), h - 1); y1 = np.minimum(2 * np.arange(hn) + 1, h - 1)
        x0 = np.minimum(2 * np.arange(wn), w - 1); x1 = np.minimum(2 * np.arange(wn) + 1, w - 1)
        s = (src[y0][:, x0] + src[y0][:, x1] + src[y1][:, x0] + src[y1][:, x1] + 2) // 4
        levels.append(s.astype(np.uint8))
    return levels


DDSD = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000      # caps, height, width, pitch, pixelformat


def dds_bytes(levels):
    h, w = levels[0].shape[:2]
    flags, mips, caps = DDSD, 0, 0x1000
    if len(levels) > 1:
        flags |= 0x20000                        # mipmap count
        mips = len(levels)
        caps |= 0x8 | 0x400000                  # complex, mipmap
    header = struct.pack('<7I', 124, flags, h, w, w * 4, 0, mips) + b'\0' * 44
    header += struct.pack('<8I', 32, 0x40 | 0x1, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000)
    header += struct.pack('<5I', caps, 0, 0, 0, 0)
    return b'DDS ' + header + b''.join(l.tobytes() for l in levels)


def read_dds_level0(path):
    data = open(path, 'rb').read()
    if data[:4] != b'DDS ':
        raise ValueError(path + ': not a DDS file')
    h, w = struct.unpack_from('<II', data, 12)
    return np.frombuffer(data, dtype=np.uint8, count=w * h * 4, offset=128).reshape(h, w, 4).copy()


# --------------------------------------------------------------------------
# Which bitmaps are model textures
# --------------------------------------------------------------------------

def model_textures(folder):
    rx = re.compile(r'TextureFilename\s*\{\s*"([^"]+)"', re.IGNORECASE)
    names = set()
    for f in os.listdir(folder):
        if f.lower().endswith('.x'):
            text = open(os.path.join(folder, f), encoding='latin-1').read()
            names.update(os.path.splitext(m)[0].lower() for m in rx.findall(text))
    return names


def premultiply(level):
    a = level[..., 3:4].astype(np.uint32)
    out = level.copy()
    out[..., :3] = ((level[..., :3].astype(np.uint32) * a + 127) // 255).astype(np.uint8)
    return out


def dds_level_count(path):
    data = open(path, 'rb').read(128)
    flags, mips = struct.unpack_from('<I', data, 8)[0], struct.unpack_from('<I', data, 28)[0]
    return mips if (flags & 0x20000) and mips > 1 else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'convert':
        folder = argv[2]
        textures = model_textures(folder)
        count = mips = 0
        for f in sorted(os.listdir(folder)):
            name, ext = os.path.splitext(f)
            if ext.lower() != '.bmp':
                continue
            level0 = bgra_from_bmp(os.path.join(folder, f))
            levels = mip_chain(level0) if name.lower() in textures else [level0]
            open(os.path.join(folder, name + '.dds'), 'wb').write(dds_bytes(levels))
            count += 1
            mips += len(levels) > 1
        print('%d DDS files, %d with mip levels' % (count, mips))
    elif len(argv) == 3 and argv[1] == 'premultiply':
        level0 = premultiply(read_dds_level0(argv[2]))
        levels = mip_chain(level0) if dds_level_count(argv[2]) > 1 else [level0]
        open(argv[2], 'wb').write(dds_bytes(levels))
        print(argv[2] + ': premultiplied' + (', mip levels rebuilt' if len(levels) > 1 else ''))
    elif len(argv) == 3 and argv[1] == 'mips':
        level0 = read_dds_level0(argv[2])
        open(argv[2], 'wb').write(dds_bytes(mip_chain(level0)))
        print(argv[2] + ': mip levels rebuilt')
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
