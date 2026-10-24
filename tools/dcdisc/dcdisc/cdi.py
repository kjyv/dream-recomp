"""DiscJuggler (.cdi) reader.

A CDI is one file: the tracks' sectors back to back, then a descriptor table, then 8 bytes giving
the format version and where the table is. Version 3.5 counts the table's position back from the
end of the file, older versions from the start. Self-boot Dreamcast CD-Rs are written as CDIs.

Each track stores its pregap in front of its sectors. Track LBAs in the table are absolute disc
LBAs, so a multi-session image's later sessions keep their real addresses; the game's filesystem
is in the last session's data track. Data tracks are mostly mode 2 form 1 stored as 2336 bytes
(subheader, user data, EDC/ECC, without sync and header).

The layout follows cdirip, which every CDI tool since has copied.
"""

from __future__ import annotations

import struct
from typing import BinaryIO

from .image import DiscImage, Track

CDI_V2 = 0x80000004
CDI_V3 = 0x80000005
CDI_V35 = 0x80000006

_TRACK_MARK = bytes([0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF])
_SECTOR_SIZES = {0: 2048, 1: 2336, 2: 2352, 4: 2448}


class CdiImage(DiscImage):
    def __init__(self, path: str):
        self.path = path
        self.tracks: list[Track] = []
        self.offsets: dict[int, int] = {}  # track number -> file offset of its first sector
        self.modes: dict[int, int] = {}    # track number -> 0 audio, 1 mode 1, 2 mode 2
        self.sessions: list[list[int]] = []
        self._f: BinaryIO = open(path, "rb")
        try:
            self._parse()
        except Exception:
            self._f.close()
            raise

    def _u(self, size: int) -> int:
        data = self._f.read(size)
        if len(data) != size:
            raise ValueError("CDI descriptor table is truncated")
        return int.from_bytes(data, "little")

    def _skip(self, n: int) -> None:
        self._f.seek(n, 1)

    def _parse(self) -> None:
        f = self._f
        f.seek(0, 2)
        length = f.tell()
        if length < 8:
            raise ValueError("file too small for a CDI")
        f.seek(length - 8)
        version, header_offset = struct.unpack("<II", f.read(8))
        if version not in (CDI_V2, CDI_V3, CDI_V35):
            raise ValueError(f"unknown CDI version 0x{version:08X}")
        f.seek(length - header_offset if version == CDI_V35 else header_offset)

        position = 0
        number = 0
        for _ in range(self._u(2)):
            session: list[int] = []
            for _ in range(self._u(2)):
                if self._u(4) != 0:
                    self._skip(8)  # extra data written by DiscJuggler 3.00.780 and later
                if f.read(10) != _TRACK_MARK or f.read(10) != _TRACK_MARK:
                    raise ValueError(f"CDI track descriptor without start mark at {f.tell()}")
                self._skip(4)
                self._skip(self._u(1))  # the image's file name
                self._skip(11 + 4 + 4)
                if self._u(4) == 0x80000000:
                    self._skip(8)  # DiscJuggler 4
                self._skip(2)
                pregap = self._u(4)
                sectors = self._u(4)
                self._skip(6)
                mode = self._u(4)
                self._skip(12)
                lba = self._u(4)
                total = self._u(4)
                self._skip(16)
                size_code = self._u(4)
                if size_code not in _SECTOR_SIZES or mode > 2:
                    raise ValueError(f"CDI track {number + 1}: sector size code {size_code}, "
                                     f"mode {mode}")
                size = _SECTOR_SIZES[size_code]
                self._skip(29)
                if version != CDI_V2:
                    self._skip(5)
                    if self._u(4) == 0xFFFFFFFF:
                        self._skip(78)
                number += 1
                self.tracks.append(Track(number, lba, sectors, size, mode != 0))
                self.offsets[number] = position + pregap * size
                self.modes[number] = mode
                session.append(number)
                position += total * size
            self.sessions.append(session)
            self._skip(4 + 8)
            if version != CDI_V2:
                self._skip(1)
        if not self.tracks:
            raise ValueError("CDI has no tracks")
        if position > length:
            raise ValueError(f"CDI tracks need {position} bytes, the file has {length}")

    @property
    def hd_track(self) -> Track:
        """The first data track of the last session: a self-boot disc boots from there."""
        for session in reversed(self.sessions):
            for number in session:
                t = self.tracks[number - 1]
                if t.is_data:
                    return t
        raise ValueError("image has no data track")

    def read_raw_sector(self, lba: int) -> bytes:
        t = self.track_for_lba(lba)
        self._f.seek(self.offsets[t.number] + (lba - t.lba) * t.sector_size)
        data = self._f.read(t.sector_size)
        if len(data) != t.sector_size:
            raise IOError(f"short read at LBA {lba}")
        return data

    def read_sector(self, lba: int) -> bytes:
        raw = self.read_raw_sector(lba)
        t = self.track_for_lba(lba)
        if t.sector_size == 2048:
            return raw
        if t.sector_size == 2336:  # mode 2 without sync and header: 8 bytes of subheader first
            return raw[8:8 + 2048]
        if t.sector_size in (2352, 2448):
            start = 24 if self.modes[t.number] == 2 else 16
            return raw[start:start + 2048]
        raise ValueError(f"unexpected stored sector size {t.sector_size}")

    def close(self) -> None:
        self._f.close()
