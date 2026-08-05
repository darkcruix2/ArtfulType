#!/usr/bin/env python
# Python 2.5 compatible icns builder for macOS 10.5 Leopard
# Builds classic it32/t8mk/il32/l8mk/is32/s8mk icns from a PNG.
import struct, os, sys, subprocess, tempfile

def run(cmd):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    return out, err, p.returncode

def png_to_rgba(png_path, size):
    # sips: scale and export as BMP
    tmp = tempfile.mktemp(suffix='.bmp')
    out, err, rc = run('sips -z %d %d "%s" --out "%s" --setProperty format bmp' % (size, size, png_path, tmp))
    if rc != 0 or not os.path.exists(tmp):
        print "sips failed:", err
        sys.exit(1)
    data = open(tmp, 'rb').read()
    os.unlink(tmp)

    px_offset = struct.unpack_from('<I', data, 10)[0]
    bmp_w     = struct.unpack_from('<i', data, 18)[0]
    bmp_h     = struct.unpack_from('<i', data, 22)[0]
    bpp       = struct.unpack_from('<H', data, 28)[0]
    flipped   = (bmp_h > 0)
    bmp_h     = abs(bmp_h)
    bpp_bytes = bpp / 8
    row_size  = ((bmp_w * bpp_bytes + 3) / 4) * 4

    pixels = []
    for row in range(bmp_h):
        src_row = (bmp_h - 1 - row) if flipped else row
        off = px_offset + src_row * row_size
        for col in range(bmp_w):
            base = off + col * bpp_bytes
            if bpp_bytes == 4:
                b, g, r, a = struct.unpack_from('BBBB', data, base)
            elif bpp_bytes == 3:
                b, g, r = struct.unpack_from('BBB', data, base)
                a = 255
            else:
                r = g = b = struct.unpack_from('B', data, base)[0]
                a = 255
            pixels.append((r, g, b, a))
    return pixels

def packbits_encode(channel):
    # Encode list of ints as PackBits literal runs (simplest valid encoding)
    out = ''
    i = 0
    while i < len(channel):
        chunk = channel[i:i+128]
        out += chr(len(chunk) - 1)  # literal run of N+1 bytes
        for v in chunk:
            out += chr(v)
        i += 128
    return out

def make_rgb_chunk(pixels, type_code):
    r_ch = [px[0] for px in pixels]
    g_ch = [px[1] for px in pixels]
    b_ch = [px[2] for px in pixels]

    body = ''
    if type_code == 'it32':
        body += '\x00\x00\x00\x00'   # it32 mandatory 4-byte header
    body += packbits_encode(r_ch)
    body += packbits_encode(g_ch)
    body += packbits_encode(b_ch)

    return type_code + struct.pack('>I', len(body) + 8) + body

def make_mask_chunk(pixels, type_code):
    alpha = ''.join(chr(px[3]) for px in pixels)
    return type_code + struct.pack('>I', len(alpha) + 8) + alpha

def build_icns(png_path, out_path):
    configs = [
        (128, 'it32', 't8mk'),
        (32,  'il32', 'l8mk'),
        (16,  'is32', 's8mk'),
    ]
    chunks = []
    for (size, rgb_code, mask_code) in configs:
        print "Generating %dx%d (%s/%s)..." % (size, size, rgb_code, mask_code)
        pixels = png_to_rgba(png_path, size)
        rgb  = make_rgb_chunk(pixels, rgb_code)
        mask = make_mask_chunk(pixels, mask_code)
        chunks.append(rgb)
        chunks.append(mask)
        print "  %d pixels, rgb=%d bytes, mask=%d bytes" % (len(pixels), len(rgb), len(mask))

    total = 8 + sum(len(c) for c in chunks)
    print "Writing %s (%d bytes)..." % (out_path, total)
    f = open(out_path, 'wb')
    f.write('icns')
    f.write(struct.pack('>I', total))
    for c in chunks:
        f.write(c)
    f.close()
    print "Done."

if len(sys.argv) != 3:
    print "Usage: python make_icns_bmp.py <input.png> <output.icns>"
    sys.exit(1)

build_icns(sys.argv[1], sys.argv[2])
