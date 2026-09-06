#!/usr/bin/env python3
"""Decode a VIDYUT protocol stream into PNGs so frames can be eyeballed."""
import json, sys, zlib, struct, os

def png(path, w, h, rgb_rows):
    raw = b"".join(b"\x00" + row for row in rgb_rows)
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)

def main(stream_path, out_prefix):
    w = h = 0
    pal = [(0, 0, 0)] * 16
    fb = None
    n = 0
    last = None
    for line in open(stream_path, "r", errors="replace"):
        if not line.startswith("@vidyut "):
            continue
        msg = json.loads(line[8:])
        t = msg.get("t")
        if t == "init":
            w, h = msg["w"], msg["h"]
            fb = bytearray(w * h)
        elif t == "pal":
            v = msg["rgb"]
            pal = [(v[i*3], v[i*3+1], v[i*3+2]) for i in range(16)]
        elif t == "frame":
            hexs = msg["rle"]
            fb = bytearray(w * h)
            pos = 0
            i = 0
            while i < len(hexs):
                run = int(hexs[i:i+4], 16)
                val = int(hexs[i+4:i+6], 16)
                fb[pos:pos+run] = bytes([val]) * run
                pos += run
                i += 6
            assert pos == w * h, f"frame covers {pos} of {w*h} pixels"
            rows = []
            for y in range(h):
                row = bytearray()
                for x in range(w):
                    r, g, b = pal[fb[y*w + x] & 15]
                    row += bytes((r, g, b))
                rows.append(bytes(row))
            last = f"{out_prefix}_{n:03d}.png"
            png(last, w, h, rows)
            n += 1
    print(f"{n} frame(s); last = {last}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
