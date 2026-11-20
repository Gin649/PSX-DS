#!/usr/bin/env python3
"""Gives a copy of a 64-bit little-endian .so a new soname, in place.

    set_soname.py LIB.so NEW_SONAME

The new name is written over the old one in the string table, so it can't
be longer than the old one. Each SwanStation copy in the test app gets its
own soname, so the Android loader never mistakes one for the other."""
import struct
import sys

DT_NULL, DT_STRTAB, DT_SONAME = 0, 5, 14
PT_LOAD, PT_DYNAMIC = 1, 2


def main(path, new):
    with open(path, "r+b") as f:
        data = bytearray(f.read())
        if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
            sys.exit("not a 64-bit little-endian ELF file")
        phoff, = struct.unpack_from("<Q", data, 0x20)
        phentsize, phnum = struct.unpack_from("<HH", data, 0x36)
        loads, dyn = [], None
        for i in range(phnum):
            p_type, _, p_offset, p_vaddr, _, p_filesz, _, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phentsize)
            if p_type == PT_LOAD:
                loads.append((p_vaddr, p_offset, p_filesz))
            elif p_type == PT_DYNAMIC:
                dyn = (p_offset, p_filesz)
        if not dyn:
            sys.exit("no dynamic section")

        def to_offset(vaddr):
            for v, o, n in loads:
                if v <= vaddr < v + n:
                    return vaddr - v + o
            sys.exit("address %x isn't in the file" % vaddr)

        strtab = soname = None
        for off in range(dyn[0], dyn[0] + dyn[1], 16):
            tag, val = struct.unpack_from("<qQ", data, off)
            if tag == DT_NULL:
                break
            if tag == DT_STRTAB:
                strtab = val
            elif tag == DT_SONAME:
                soname = val
        if strtab is None or soname is None:
            sys.exit("no soname to replace")
        at = to_offset(strtab) + soname
        end = data.index(b"\0", at)
        old = data[at:end].decode()
        enc = new.encode()
        if len(enc) > len(old):
            sys.exit("new soname %r is longer than %r" % (new, old))
        data[at:end] = enc + b"\0" * (len(old) - len(enc))
        f.seek(0)
        f.write(data)
        print("%s: soname %s -> %s" % (path, old, new))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
