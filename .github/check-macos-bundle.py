"""Check that Mach-O dependencies resolve inside the app or to macOS libraries."""

import struct
import sys
from pathlib import Path


def commands(data):
    magic = data[:4]
    fat = {
        b"\xca\xfe\xba\xbe": (">", False),
        b"\xbe\xba\xfe\xca": ("<", False),
        b"\xca\xfe\xba\xbf": (">", True),
        b"\xbf\xba\xfe\xca": ("<", True),
    }
    if magic in fat:
        endian, wide = fat[magic]
        count = struct.unpack_from(endian + "I", data, 4)[0]
        for index in range(count):
            entry = 8 + index * (32 if wide else 20)
            offset, size = struct.unpack_from(endian + ("QQ" if wide else "II"), data, entry + 8)
            yield from commands(data[offset:offset + size])
        return
    headers = {
        b"\xce\xfa\xed\xfe": ("<", 28),
        b"\xcf\xfa\xed\xfe": ("<", 32),
        b"\xfe\xed\xfa\xce": (">", 28),
        b"\xfe\xed\xfa\xcf": (">", 32),
    }
    if magic not in headers:
        return
    endian, offset = headers[magic]
    count = struct.unpack_from(endian + "I", data, 16)[0]
    for _ in range(count):
        command, size = struct.unpack_from(endian + "II", data, offset)
        if command in (0xC, 0x80000018, 0x8000001F, 0x80000023, 0x8000001C):
            start = offset + struct.unpack_from(endian + "I", data, offset + 8)[0]
            value = data[start:offset + size].split(b"\0", 1)[0].decode()
            yield command == 0x8000001C, value
        offset += size


def check_bundle(app):
    app = app.resolve()
    executable_dir = app / "Contents/MacOS"
    main = executable_dir / "salvium-wallet-gui"
    if not main.is_file():
        raise SystemExit(f"Missing GUI executable: {main}")
    binaries = {}
    for path in app.rglob("*"):
        if path.is_file() and not path.is_symlink():
            entries = list(commands(path.read_bytes()))
            if entries:
                binaries[path] = entries
    main_rpaths = [(value, main) for is_rpath, value in binaries[main] if is_rpath]

    def expand(value, binary):
        return Path(value.replace("@loader_path", str(binary.parent))
                    .replace("@executable_path", str(executable_dir))).resolve()

    missing = []
    for binary, entries in binaries.items():
        rpaths = [(value, binary) for is_rpath, value in entries if is_rpath] + main_rpaths
        for is_rpath, value in entries:
            if is_rpath or value.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if value.startswith("@rpath/"):
                candidates = [expand(prefix, owner) / value[len("@rpath/"):]
                              for prefix, owner in rpaths]
            else:
                candidates = [expand(value, binary)]
            if not any(path.resolve().is_relative_to(app) and path.is_file() for path in candidates):
                missing.append(f"{binary.relative_to(app)}: {value}")
    if missing:
        raise SystemExit("Unbundled library dependencies:\n" + "\n".join(missing))
    print(f"Verified bundled dependencies for {len(binaries)} Mach-O files")


if __name__ == "__main__":
    check_bundle(Path(sys.argv[1]))
