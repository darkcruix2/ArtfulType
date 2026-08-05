#!/usr/bin/env python
"""
Build a macOS 10.5-compatible .icns file from a PNG source.
Uses only the old-style icon types (is32/s8mk, il32/l8mk, it32/t8mk)
that QuickDraw/IconServices on 10.5 understands.
"""
import struct
import os
import sys

# Try to import PIL/Pillow for PNG reading; fall back to AppKit if available
def load_png_rgba(path, size):
    try:
        import AppKit
        app = AppKit.NSApplication.sharedApplication()
        src = AppKit.NSImage.alloc().initWithContentsOfFile_(path)
        bmp = AppKit.NSBitmapImageRep.imageRepWithData_(src.TIFFRepresentation())
        dst = AppKit.NSBitmapImageRep.alloc().initWithBitmapDataPlanes_pixelsWide_pixelsHigh_bitsPerSample_samplesPerPixel_hasAlpha_isPlanar_colorSpaceName_bytesPerRow_bitsPerPixel_(
            None, size, size, 8, 4, True, False,
            AppKit.NSCalibratedRGBColorSpace, size*4, 32
        )
        AppKit.NSGraphicsContext.saveGraphicsState()
        AppKit.NSGraphicsContext.setCurrentContext_(
            AppKit.NSGraphicsContext.graphicsContextWithBitmapImageRep_(dst)
        )
        src.drawInRect_fromRect_operation_fraction_(
            AppKit.NSMakeRect(0, 0, size, size),
            AppKit.NSZeroRect,
            AppKit.NSCompositeSourceOver,
            1.0
        )
        AppKit.NSGraphicsContext.restoreGraphicsState()
        raw = dst.bitmapData()
        pixels = []
        for i in xrange(size * size):
            r = ord(raw[i*4+0])
            g = ord(raw[i*4+1])
            b = ord(raw[i*4+2])
            a = ord(raw[i*4+3])
            pixels.append((r, g, b, a))
        return pixels
    except Exception as e:
        print "AppKit approach failed:", e
        sys.exit(1)

def make_raw_argb(pixels, size):
    """
    Old 'it32'/'il32'/'is32' format: raw 32-bit ARGB (big-endian).
    Actually the raw format for it32 is: 4-byte header of zeros, then
    RLE-compressed channel data per channel (A, R, G, B).
    For simplicity we write uncompressed: each channel as raw bytes.
    il32/is32 are just raw XRGB (no alpha channel in the RGB chunk).
    """
    # For it32 (128x128) and il32 (32x32), is32 (16x16):
    # The RGB data is in 'PackBits' RLE per channel.
    # Simplest: write uncompressed (no RLE) which is valid if run-length > 128.
    # Actually uncompressed is signalled by a literal run: 0x80..0xFF prefix byte
    # means "copy next N+1 bytes verbatim". 
    # Simplest valid encoding: split into 128-byte chunks, each prefixed with 0x7F (copy 128).
    
    channels = [[], [], []]  # R, G, B
    for (r, g, b, a) in pixels:
        channels[0].append(r)
        channels[1].append(g)
        channels[2].append(b)
    
    def packbits_literal(data):
        """Encode data as PackBits literal runs (simplest possible)."""
        out = bytearray()
        i = 0
        while i < len(data):
            chunk = data[i:i+128]
            out.append(len(chunk) - 1)  # 0..127 = literal run of N+1 bytes
            out.extend(chunk)
            i += 128
        return bytes(out)
    
    rgb_data = bytearray()
    # it32 starts with 4 zero bytes as magic
    if size == 128:
        rgb_data.extend(b'\x00\x00\x00\x00')
    for ch in channels:
        rgb_data.extend(packbits_literal(ch))
    
    return bytes(rgb_data)

def make_mask(pixels, size):
    """8-bit alpha mask ('t8mk', 'l8mk', 's8mk')."""
    return bytes(bytearray([a for (r,g,b,a) in pixels]))

def build_icns(png_path, out_path):
    # Icon sizes and their type codes
    configs = [
        (128, 'it32', 't8mk'),
        (32,  'il32', 'l8mk'),
        (16,  'is32', 's8mk'),
    ]
    
    chunks = []
    for (size, rgb_type, mask_type) in configs:
        print "Processing %dx%d..." % (size, size)
        pixels = load_png_rgba(png_path, size)
        
        rgb_data = make_raw_argb(pixels, size)
        mask_data = make_mask(pixels, size)
        
        # Each chunk: 4-byte OSType + 4-byte length (includes 8-byte header)
        rgb_chunk = rgb_type.encode('ascii') + struct.pack('>I', len(rgb_data) + 8) + rgb_data
        mask_chunk = mask_type.encode('ascii') + struct.pack('>I', len(mask_data) + 8) + mask_data
        
        chunks.append(rgb_chunk)
        chunks.append(mask_chunk)
    
    total_len = 8 + sum(len(c) for c in chunks)
    
    with open(out_path, 'wb') as f:
        f.write(b'icns')
        f.write(struct.pack('>I', total_len))
        for chunk in chunks:
            f.write(chunk)
    
    print "Written:", out_path, "(%d bytes)" % total_len

build_icns('/tmp/at_icon.png', os.path.expanduser('~/ArtfulType-ObjC/ArtfulType.icns'))
