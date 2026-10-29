from dcdisc import open_image
from dcdisc.cdi import CdiImage
from dcdisc.ipbin import parse_ipbin, read_ipbin
from dcdisc.iso9660 import Iso9660

from conftest import CDI_GAME_LBA, write_cdi


def test_tracks_and_sessions(cdi_path):
    with open_image(cdi_path) as img:
        assert isinstance(img, CdiImage)
        assert [(t.number, t.lba, t.sector_size, t.is_data) for t in img.tracks] == [
            (1, 0, 2336, True), (2, CDI_GAME_LBA, 2336, True)]
        assert img.sessions == [[1], [2]]
        assert img.hd_track.number == 2
        assert img.read_sector(5) == bytes([5]) * 2048


def test_ipbin_and_filesystem(cdi_path, synthetic_files):
    files, _ = synthetic_files
    with open_image(cdi_path) as img:
        assert parse_ipbin(read_ipbin(img)).title == "SYNTHETIC TEST DISC"
        fs = Iso9660(img)
        for p, data in files.items():
            if p != "IP.BIN":
                assert fs.read_path(p) == data, p


def test_sector_layouts(tmp_path):
    user = [bytes([0x40 + i]) * 2048 for i in range(4)]
    path = tmp_path / "layouts.cdi"
    write_cdi(path, [[(0, 1, 2048, user)], [(1000, 1, 2352, user)], [(2000, 2, 2352, user)]])
    with open_image(str(path)) as img:
        assert [t.sector_size for t in img.tracks] == [2048, 2352, 2352]
        for base in (0, 1000, 2000):
            assert [img.read_sector(base + i) for i in range(4)] == user
