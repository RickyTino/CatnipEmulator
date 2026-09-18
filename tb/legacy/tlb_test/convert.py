#!/usr/bin/env python3
"""Convert a flat little-endian binary into one hex word per line,
matching the format of CatnipEmulator's func_ram.txt / perf_ram.txt."""
import struct
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: convert.py <input.bin> <output.txt>")
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    pad = (-len(data)) % 4
    if pad:
        data += b"\x00" * pad
    with open(sys.argv[2], "w") as out:
        for i in range(0, len(data), 4):
            out.write("%08x\n" % struct.unpack("<I", data[i:i + 4])[0])


if __name__ == "__main__":
    main()
