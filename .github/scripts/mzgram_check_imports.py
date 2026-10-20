#!/usr/bin/env python3
"""MZGram.exe needs nothing but Windows itself.

The release zips carry only MZGram.exe and Updater.exe, so every DLL the
executables import (also delay-loaded ones) must come with Windows. Qt links
every SQL driver it was built with into a static app: on the Windows x64
runners, where PostgreSQL is installed, that put libpq.dll into the imports
of MZGram.exe up to 1.0.4, and MZGram did not start on a computer without it.

Usage: mzgram_check_imports.py MZGram.exe [Updater.exe ...]
"""

import re
import struct
import sys

# DLLs that come with Windows 7 and later (the API sets with Windows 10 and
# later, resolved by the loader). Nothing else may be imported.
SYSTEM = {
    "advapi32.dll", "authz.dll", "avrt.dll", "bcrypt.dll",
    "bcryptprimitives.dll", "comctl32.dll", "comdlg32.dll", "crypt32.dll",
    "d3d9.dll", "d3d11.dll", "d3d12.dll",
    "dbghelp.dll", "dnsapi.dll", "dwmapi.dll", "dwrite.dll", "dxgi.dll",
    "gdi32.dll", "gdiplus.dll", "hid.dll", "icuuc.dll", "imm32.dll",
    "iphlpapi.dll", "kernel32.dll", "mf.dll", "mfplat.dll", "mfreadwrite.dll",
    "mpr.dll", "msimg32.dll", "ncrypt.dll", "netapi32.dll", "ntdll.dll",
    "odbc32.dll", "ole32.dll", "oleaut32.dll", "powrprof.dll", "propsys.dll",
    "psapi.dll", "rpcrt4.dll", "secur32.dll", "setupapi.dll", "shcore.dll",
    "shell32.dll", "shlwapi.dll", "uiautomationcore.dll", "user32.dll",
    "userenv.dll", "uxtheme.dll", "version.dll", "winhttp.dll", "winmm.dll",
    "winspool.drv", "wintrust.dll", "ws2_32.dll", "wtsapi32.dll",
}
API_SET = re.compile(r"^(api|ext)-ms-win-[a-z0-9-]+\.dll$")


def imports(data):
    """The DLL names a PE file imports, normal and delay-loaded."""
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE file")
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    dirs = opt + (112 if magic == 0x20B else 96)
    table = opt + opt_size
    spans = []
    for i in range(sections):
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, table + i * 40 + 8)
        spans.append((va, max(vsize, rsize), raw))

    def offset(rva):
        for va, size, raw in spans:
            if va <= rva < va + size:
                return rva - va + raw
        raise ValueError(f"rva {rva:#x} outside the sections")

    def name(rva):
        start = offset(rva)
        return data[start:data.index(b"\0", start)].decode("ascii")

    result = []
    for index, size, name_field in ((1, 20, 3), (13, 32, 1)):
        rva = struct.unpack_from("<I", data, dirs + index * 8)[0]
        if not rva:
            continue
        at = offset(rva)
        while True:
            fields = struct.unpack_from(f"<{size // 4}I", data, at)
            if not any(fields):
                break
            result.append(name(fields[name_field]))
            at += size
    return result


def foreign(names):
    return sorted(n for n in names if n.lower() not in SYSTEM and not API_SET.match(n.lower()))


def main(paths):
    failed = False
    for path in paths:
        with open(path, "rb") as file:
            names = imports(file.read())
        extra = foreign(names)
        if extra:
            failed = True
            print(f"FAIL: {path} imports {', '.join(extra)}, which Windows does not have")
        else:
            print(f"PASS: {path} imports only Windows DLLs ({len(names)})")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
