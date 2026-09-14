# -*- coding: utf-8 -*-
# Verification INDEPENDANTE du compteur de pixels du banc : decodeur PNG ecrit
# ici, zlib de la bibliotheque standard, aucun code partage avec NkSequenceCheck.
# Si les deux comptes coincident, ce n'est pas parce qu'ils se trompent ensemble.
import struct
import sys
import zlib


def decode_png(path):
    d = open(path, "rb").read()
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "pas un PNG"
    pos, idat, w = 8, b"", None
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos + 4])[0]
        typ = d[pos + 4:pos + 8]
        data = d[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", data[:10])
            assert depth == 8, "profondeur %d non geree" % depth
        elif typ == b"IDAT":
            idat += data
        elif typ == b"IEND":
            break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    nch = {0: 1, 2: 3, 4: 2, 6: 4}[ctype]
    stride = w * nch
    out = bytearray(stride * h)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(nch, stride):
                line[i] = (line[i] + line[i - nch]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - nch] if i >= nch else 0
                c = prev[i - nch] if i >= nch else 0
                b = prev[i]
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, nch, bytes(out)


def compte_non_magenta(path):
    w, h, nch, px = decode_png(path)
    n = 0
    for i in range(w * h):
        r, g, b = px[i * nch], px[i * nch + 1], px[i * nch + 2]
        if not (r == 255 and g == 0 and b == 255):
            n += 1
    return w, h, n


if __name__ == "__main__":
    for f in sys.argv[1:]:
        w, h, n = compte_non_magenta(f)
        nom = f.replace("\\", "/").split("/")[-2] + "/" + f.replace("\\", "/").split("/")[-1]
        print("%-34s %dx%d  pixels non-magenta = %d" % (nom, w, h, n))
