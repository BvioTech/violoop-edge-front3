#!/usr/bin/env python3
"""Writes assets/away/studio-probe.hdr, the light probe qml/AwayPage.qml uses.

A stand-in until the designer's own HDRs (visor-studio.hdr and friends) are
in the repo: a dark studio with a large soft box overhead and upright strip
lights around the horizon, so the mascot's dark metal shell and glossy hands
have something to reflect. Equirectangular, Radiance RGBE, no RLE.

Usage: make-away-probe.py [output.hdr]
"""
import math
import os
import sys

W, H = 256, 128

# (longitude of centre in turns, half-width in turns, radiance RGB)
STRIPS = [
    (0.00, 0.035, (2.6, 2.9, 3.2)),
    (0.25, 0.050, (4.2, 4.4, 4.6)),
    (0.50, 0.035, (2.6, 2.9, 3.2)),
    (0.75, 0.050, (3.4, 3.8, 4.4)),
]


def smooth(edge0, edge1, x):
    t = min(1.0, max(0.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3 - 2 * t)


def radiance(u, v):
    """u: longitude 0..1, v: 0 at the zenith, 1 at the nadir."""
    # Walls: dim cool grey, falling off towards the floor.
    wall = 0.16 * (1 - smooth(0.5, 0.75, v)) + 0.035
    r, g, b = wall * 0.92, wall * 0.97, wall * 1.06
    # Overhead soft box.
    top = 3.4 * (1 - smooth(0.10, 0.24, v))
    r, g, b = r + top * 0.96, g + top * 0.98, b + top
    # Strip lights, from a bit above the horizon up to the soft box.
    band = smooth(0.22, 0.28, v) * (1 - smooth(0.50, 0.58, v))
    for centre, half, colour in STRIPS:
        d = abs((u - centre + 0.5) % 1.0 - 0.5)
        k = band * (1 - smooth(half * 0.6, half, d))
        r, g, b = r + colour[0] * k, g + colour[1] * k, b + colour[2] * k
    return r, g, b


def rgbe(r, g, b):
    m = max(r, g, b)
    if m < 1e-32:
        return b"\0\0\0\0"
    mant, exp = math.frexp(m)
    scale = mant * 256.0 / m
    return bytes((min(255, int(r * scale)), min(255, int(g * scale)), min(255, int(b * scale)), exp + 128))


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "assets", "away", "studio-probe.hdr")
    data = bytearray(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n" % (H, W))
    for y in range(H):
        for x in range(W):
            data += rgbe(*radiance((x + 0.5) / W, (y + 0.5) / H))
    with open(out, "wb") as f:
        f.write(data)


if __name__ == "__main__":
    main()
