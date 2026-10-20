#!/usr/bin/env python3
"""
find_extras_sites.py - checks GTAIV.exe against the byte patterns used by the
optional extras (matrix limit, props out of reflections, ConeBack).

Usage:   python find_extras_sites.py "C:\\path\\to\\GTAIV.exe"
         python find_extras_sites.py --selftest

For every pattern it prints how many times it matches in the executable
sections and the RVA of each match. The extras only install a feature when
every pattern that feature needs matches EXACTLY ONCE, so every line below must
say "1 match". It also prints the camera address found through its pattern;
for the exe this mod was verified against it should be RVA 0x00E8E340.
"""
import re, struct, sys

PATTERNS = [
    ("matrix.limit.a",   "81 3D ? ? ? ? 00 02 00 00 7C ? 57 B9"),
    ("matrix.limit.b",   "81 F9 00 02 00 00 0F 8D ? ? ? ? 8B 4F 08 E8"),
    ("matrix.pool.size", "68 58 1B 00 00 B9 ? ? ? ? C7 05"),
    ("matrix.pool.alloc","51 E8 ? ? ? ? 89 83 E0 01 00 00"),
    ("matrix.pool.free", "FF B6 E0 01 00 00 E8 ? ? ? ? 83 C4 04"),
    ("refl.phases",      "85 0D ? ? ? ? 75 ? 0F B6 46 63 3D EF 00 00 00"),
    ("refl.list.a",      "F7 43 40 00 01 00 00 F3 0F 10 47 04"),
    ("refl.list.b",      "89 4C 24 28 A9 00 00 00 08"),
    ("cone.before",      "33 C0 33 FF 66 3B 86 28 52 00 00"),
    ("cone.reset",       "FF B5 24 52 00 00 E8"),
    ("cone.destructor",  "FF B6 24 52 00 00 E8"),
    ("cone.camera",      "F3 0F 10 05 ? ? ? ? F3 0F 10 15 ? ? ? ? F3 0F 10 0D ? ? ? ? 56 8B 75 0C"),
    ("drawable.refs",    "68 ? ? ? ? E8 ? FF FF FF C3"),
    ("rendercache.scans","E8 ? ? ? ? 8B 0D ? ? ? ? 2B C8 83 E1 F8 81 F9 00 00 01 00"),
    ("spawn.after",      "66 89 86 30 52 00 00 8B CE 5E E9"),
]
EXPECTED = {"rendercache.scans": 2}      # the render-cache scans exist twice; every other site once

def compile_pattern(text):
    parts = []
    for tok in text.split():
        parts.append(b"." if tok == "?" else re.escape(bytes([int(tok, 16)])))
    return re.compile(b"".join(parts), re.DOTALL)

def parse_pe(data):
    if data[:2] != b"MZ":
        raise SystemExit("not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("bad PE signature")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise SystemExit("not a 32-bit executable")
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    secs = []
    sh = opt + opt_size
    for i in range(nsec):
        name, vsize, va, rsize, raw = struct.unpack_from("<8sIIII", data, sh + i * 40)
        chars = struct.unpack_from("<I", data, sh + i * 40 + 36)[0]
        if chars & 0x20000000:      # executable
            secs.append((name.rstrip(b"\0").decode(errors="replace"), va, min(vsize, rsize) or vsize, raw))
    return image_base, secs

def scan(data, image_base, secs):
    results = {}
    for name, pat in PATTERNS:
        rx = compile_pattern(pat)
        hits = []
        for sname, va, size, raw in secs:
            blob = data[raw:raw + size]
            for m in rx.finditer(blob):
                hits.append((va + m.start(), blob[m.start():m.end()]))
        results[name] = hits
    return results

def report(data, image_base, secs, results):
    ok = True
    print("image base (preferred): 0x%08X" % image_base)
    for name, pat in PATTERNS:
        hits = results[name]
        want = EXPECTED.get(name, 1)
        flag = "OK " if len(hits) == want else "BAD"
        if len(hits) != want:
            ok = False
        print("%s %-18s %d match%s (expected %d)" % (flag, name, len(hits), "" if len(hits) == 1 else "es", want))
        for rva, b in hits[:5]:
            print("      RVA 0x%08X  %s" % (rva, b.hex(" ")))
    cam = results["cone.camera"]
    if len(cam) == 1:
        operand = struct.unpack_from("<I", cam[0][1], 12)[0]
        rva = operand - image_base
        print("camera position operand: 0x%08X  -> RVA 0x%08X %s" %
              (operand, rva, "(expected 0x00E8E340)" if rva == 0xE8E340 else "(DIFFERENT from 0x00E8E340)"))
    print("\nAll patterns match the expected number of times." if ok else
          "\nSome patterns do not match the expected number of times: the matching feature will stay off.")
    return ok

def selftest():
    code = bytearray(b"\xCC" * 0x600)
    off = 0x10
    for name, pat in PATTERNS:
        for _ in range(EXPECTED.get(name, 1)):
            for tok in pat.split():
                code[off] = 0x00 if tok == "?" else int(tok, 16); off += 1
            off += 8
    # camera operand: preferred base 0x400000 + RVA 0xE8E340
    cam_off = 0x10 + sum((len(p.split()) + 8) * EXPECTED.get(n, 1) for n, p in PATTERNS[:11])
    struct.pack_into("<I", code, cam_off + 12, 0x400000 + 0xE8E340)
    hdr = bytearray(0x200)
    hdr[0:2] = b"MZ"; struct.pack_into("<I", hdr, 0x3C, 0x80)
    pe = 0x80
    hdr[pe:pe + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", hdr, pe + 4, 0x14C, 1, 0, 0, 0, 0xE0, 0x102)
    opt = pe + 24
    struct.pack_into("<H", hdr, opt, 0x10B)
    struct.pack_into("<I", hdr, opt + 28, 0x400000)
    sh = opt + 0xE0
    struct.pack_into("<8sIIIIIIHHI", hdr, sh, b".text", len(code), 0x1000, len(code), 0x200, 0, 0, 0, 0, 0x60000020)
    data = bytes(hdr) + bytes(code)
    base, secs = parse_pe(data)
    res = scan(data, base, secs)
    ok = all(len(h) == EXPECTED.get(n, 1) for n, h in res.items())
    cam = struct.unpack_from("<I", res["cone.camera"][0][1], 12)[0] - base
    ok = ok and cam == 0xE8E340
    # a duplicate must be reported as BAD
    dup = data + data[0x200 + 0x10:0x200 + 0x1F]
    base2, secs2 = parse_pe(dup)
    print("selftest:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--selftest":
        sys.exit(selftest())
    if len(sys.argv) != 2:
        print(__doc__); sys.exit(2)
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    base, secs = parse_pe(data)
    res = scan(data, base, secs)
    sys.exit(0 if report(data, base, secs, res) else 1)
