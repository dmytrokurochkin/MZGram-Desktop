#!/usr/bin/env python3
"""Tests for mzgram_check_imports.py: run with python3 test_mzgram_check_imports.py"""

import struct
import unittest

import mzgram_check_imports as check


def pe(dlls, delayed=()):
    """A minimal PE32+ file importing dlls (and delay-loading delayed): one
    section holding the import table, the delay-load table and the names."""
    section_rva, section_raw = 0x1000, 0x200
    descriptors = 20 * (len(dlls) + 1)
    delay_size = 32 * (len(delayed) + 1) if delayed else 0
    names = b""
    offsets = []
    delay_offsets = []
    for dll in dlls:
        offsets.append(section_rva + descriptors + delay_size + len(names))
        names += dll.encode() + b"\x00"
    for dll in delayed:
        delay_offsets.append(section_rva + descriptors + delay_size + len(names))
        names += dll.encode() + b"\x00"
    body = b"".join(struct.pack("<5I", 0, 0, 0, rva, 0) for rva in offsets) + bytes(20)
    if delayed:
        body += b"".join(struct.pack("<8I", 1, rva, 0, 0, 0, 0, 0, 0) for rva in delay_offsets) + bytes(32)
    body += names
    header = bytearray(section_raw)
    struct.pack_into("<I", header, 0x3C, 0x40)
    header[0x40:0x44] = b"PE\x00\x00"
    opt_size = 240
    struct.pack_into("<HHIIIHH", header, 0x44, 0x8664, 1, 0, 0, 0, opt_size, 0)
    opt = 0x40 + 24
    struct.pack_into("<H", header, opt, 0x20B)
    struct.pack_into("<II", header, opt + 112 + 8, section_rva, len(body))
    if delayed:
        struct.pack_into("<II", header, opt + 112 + 13 * 8, section_rva + descriptors, delay_size)
    struct.pack_into("<IIII", header, opt + opt_size + 8, len(body), section_rva, len(body), section_raw)
    return bytes(header) + body


class ImportsTest(unittest.TestCase):
    def test_reads_the_import_table(self):
        self.assertEqual(["KERNEL32.dll", "libpq.dll"], check.imports(pe(["KERNEL32.dll", "libpq.dll"])))

    def test_only_windows_dlls_pass(self):
        self.assertEqual([], check.foreign(["KERNEL32.dll", "api-ms-win-core-heap-l1-1-0.dll", "ODBC32.dll", "bcryptprimitives.dll"]))
        self.assertEqual(["Qt6Core.dll", "libpq.dll"], check.foreign(["USER32.dll", "libpq.dll", "Qt6Core.dll"]))

    def test_reads_the_delay_load_table(self):
        file = pe(["KERNEL32.dll"], ["icu.dll", "dwmapi.dll"])
        self.assertEqual((["KERNEL32.dll"], ["icu.dll", "dwmapi.dll"]), check.import_tables(file))
        self.assertEqual(["KERNEL32.dll", "icu.dll", "dwmapi.dll"], check.imports(file))

    # icu.dll came with Windows 10 1903, after the oldest Windows MZGram runs
    # on: delay-loaded it waits for the check that Windows has it, imported
    # the usual way MZGram would not start on 1809.
    def test_icu_only_when_delay_loaded(self):
        self.assertEqual([], check.foreign(["KERNEL32.dll"], ["icu.dll"]))
        self.assertEqual(["icu.dll"], check.foreign(["KERNEL32.dll", "icu.dll"], []))
        self.assertEqual(["libpq.dll"], check.foreign(["KERNEL32.dll"], ["libpq.dll"]))


if __name__ == "__main__":
    unittest.main()
