#!/usr/bin/env python3
"""Losslessly optimize PNG and PNG-backed ICO assets using only Python stdlib."""
import argparse
from pathlib import Path
import struct
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"


def chunks(data):
    if not data.startswith(SIGNATURE):
        raise ValueError("Expected a PNG image")
    offset = 8
    result = []
    while offset < len(data):
        size = struct.unpack_from(">I", data, offset)[0]
        tag = data[offset + 4:offset + 8]
        body = data[offset + 8:offset + 8 + size]
        if zlib.crc32(tag + body) != struct.unpack_from(">I", data, offset + 8 + size)[0]:
            raise ValueError("Invalid PNG checksum")
        result.append((tag, body))
        offset += size + 12
    return result


def paeth(a, b, c):
    p = a + b - c
    distances = abs(p - a), abs(p - b), abs(p - c)
    return a if distances[0] <= min(distances[1:]) else b if distances[1] <= distances[2] else c


def pixels(parts):
    width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", parts[0][1])
    if (depth, color, compression, filtering, interlace) != (8, 6, 0, 0, 0):
        raise ValueError("This optimizer supports non-interlaced 8-bit RGBA icons")
    encoded = zlib.decompress(b"".join(body for tag, body in parts if tag == b"IDAT"))
    stride = width * 4
    rows = []
    previous = bytes(stride)
    for y in range(height):
        kind = encoded[y * (stride + 1)]
        row = bytearray(encoded[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = row[x - 4] if x >= 4 else 0
            b = previous[x]
            c = previous[x - 4] if x >= 4 else 0
            predictor = (0, a, b, (a + b) // 2, paeth(a, b, c))[kind]
            row[x] = (row[x] + predictor) & 255
        rows.append(bytes(row))
        previous = row
    return rows, encoded


def optimize_png(data):
    parts = chunks(data)
    rows, original = pixels(parts)
    filtered = bytearray()
    previous = bytes(len(rows[0]))
    for row in rows:
        candidates = [bytes([0]) + row]
        for kind in (1, 2, 4):
            values = bytearray(len(row))
            for x, value in enumerate(row):
                a = row[x - 4] if x >= 4 else 0
                b = previous[x]
                c = previous[x - 4] if x >= 4 else 0
                predictor = a if kind == 1 else b if kind == 2 else paeth(a, b, c)
                values[x] = (value - predictor) & 255
            candidates.append(bytes([kind]) + values)
        filtered.extend(min(candidates, key=lambda candidate: sum(min(v, 256 - v) for v in candidate[1:])))
        previous = row
    payload = min((zlib.compress(original, 9), zlib.compress(filtered, 9)), key=len)
    output = bytearray(SIGNATURE)
    written = False
    for tag, body in parts:
        if tag in (b"tEXt", b"zTXt", b"iTXt", b"tIME", b"eXIf"):
            continue
        if tag == b"IDAT":
            if written:
                continue
            body = payload
            written = True
        output.extend(struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body)))
    # Compare all decoded RGBA values, including alpha and hidden RGB values.
    if pixels(chunks(output))[0] != rows:
        raise ValueError("PNG pixel verification failed")
    return bytes(output) if len(output) < len(data) else data


def optimize_ico(data):
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if (reserved, kind) != (0, 1):
        raise ValueError("Expected a Windows ICO")
    entries, frames = [], []
    for i in range(count):
        entry = data[6 + 16 * i:22 + 16 * i]
        size, offset = struct.unpack_from("<II", entry, 8)
        frame = data[offset:offset + size]
        entries.append(entry[:8])
        frames.append(optimize_png(frame) if frame.startswith(SIGNATURE) else frame)
    output = bytearray(struct.pack("<HHH", 0, 1, count))
    offset = 6 + 16 * count
    for entry, frame in zip(entries, frames):
        output.extend(entry + struct.pack("<II", len(frame), offset))
        offset += len(frame)
    return bytes(output) + b"".join(frames)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    for path in parser.parse_args().paths:
        source = path.read_bytes()
        result = optimize_ico(source) if path.suffix.lower() == ".ico" else optimize_png(source)
        if len(result) < len(source):
            path.write_bytes(result)
        print(f"{path}: {len(source):,} -> {min(len(source), len(result)):,} bytes; identical RGBA pixels")
