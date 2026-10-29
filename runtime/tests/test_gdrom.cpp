#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "dream/runtime/gdrom/disc.h"

#include "doctest.h"

using namespace dream::gdrom;

namespace {
const char* kFixture = DREAM_DCDISC_FIXTURES "/synthetic.chd";
}

TEST_CASE("chd: the synthetic GD-ROM fixture has the layout dcdisc measured") {
    std::string err;
    auto disc = open_disc(kFixture, err);
    REQUIRE_MESSAGE(disc != nullptr, err);
    CHECK(disc->type() == DiscType::GdRom);
    REQUIRE(disc->tracks().size() == 3);
    CHECK(disc->tracks()[0].lba == 0);
    CHECK(disc->tracks()[0].sectors == 300);
    CHECK(!disc->tracks()[0].data);  // the synthetic disc puts an audio track first
    CHECK(disc->tracks()[1].lba == 300);
    CHECK(disc->tracks()[1].sectors == 60);
    CHECK(disc->tracks()[1].data);
    CHECK(disc->tracks()[2].lba == 45000);
    CHECK(disc->tracks()[2].sectors == 79);
    CHECK(disc->hd_track()->number == 3);
    CHECK(disc->leadout_lba() == 45079);
    // IP.BIN sits in the first sector of the game area.
    std::uint8_t sector[2048];
    REQUIRE(disc->read_user(45000, sector));
    CHECK_EQ(std::memcmp(sector, "SEGA SEGAKATANA ", 16), 0);
    // Primary volume descriptor at LBA 45016.
    REQUIRE(disc->read_user(45016, sector));
    CHECK(sector[0] == 1);
    CHECK_EQ(std::memcmp(sector + 1, "CD001", 5), 0);
    CHECK(!disc->read_user(45079, sector));  // past the last track
    CHECK(!disc->read_user(400, sector));    // gap between the areas
}

TEST_CASE("chd: the Katana TOC lists tracks per area with FADs") {
    std::string err;
    auto disc = open_disc(kFixture, err);
    REQUIRE(disc);
    std::uint32_t toc[102];
    disc->toc(0, toc);
    CHECK(toc[0] == (0x01u << 24 | 150u));             // track 1: audio, FAD 150
    CHECK(toc[1] == (0x41u << 24 | 450u));             // track 2: data, FAD 450
    CHECK(toc[2] == 0xFFFFFFFFu);                      // not in this area
    CHECK(toc[99] == (0x01u << 24 | (1u << 16)));      // first track 1
    CHECK(toc[100] == (0x41u << 24 | (2u << 16)));     // last track 2
    CHECK(toc[101] == (0x41u << 24 | (360u + 150u)));  // lead-out after track 2
    disc->toc(1, toc);
    CHECK(toc[0] == 0xFFFFFFFFu);
    CHECK(toc[2] == (0x41u << 24 | 45150u));
    CHECK(toc[99] == (0x41u << 24 | (3u << 16)));
    CHECK(toc[100] == (0x41u << 24 | (3u << 16)));
    CHECK(toc[101] == (0x41u << 24 | (45079u + 150u)));
}

TEST_CASE("disc: unsupported and missing images report errors") {
    std::string err;
    CHECK(open_disc("nothing.iso", err) == nullptr);
    CHECK(err.find("unsupported") != std::string::npos);
    CHECK(open_disc("/nonexistent/x.chd", err) == nullptr);
    CHECK(!err.empty());
}

namespace {

// A DiscJuggler 3.5 image with two sessions of mode 2 data in 2336-byte sectors: a dummy track at
// LBA 0, and the game at `game_lba`, whose sectors hold their LBA in their first user word. Laid
// out the way cdirip reads it, with a 150-sector pregap in front of each track.
std::filesystem::path write_cdi(std::uint32_t game_lba, std::uint32_t sectors) {
    const auto path = std::filesystem::temp_directory_path() / "dream_test_synthetic.cdi";
    std::vector<std::uint8_t> body, table;
    auto u16 = [&](std::uint32_t v) {
        table.push_back(static_cast<std::uint8_t>(v));
        table.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    auto u32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) table.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
    };
    auto zeros = [&](std::size_t n) { table.insert(table.end(), n, 0); };
    const std::uint8_t mark[10] = {0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF};
    const std::uint32_t pregap = 150;
    u16(2);
    for (const std::uint32_t lba : {0u, game_lba}) {
        u16(1);
        body.insert(body.end(), pregap * 2336, 0);
        for (std::uint32_t i = 0; i < sectors; ++i) {
            std::uint8_t sector[2336] = {};
            const std::uint32_t v = lba + i;
            std::memcpy(sector + 8, &v, 4);
            body.insert(body.end(), sector, sector + sizeof sector);
        }
        u32(0);
        table.insert(table.end(), mark, mark + 10);
        table.insert(table.end(), mark, mark + 10);
        zeros(4);
        table.push_back(0);  // no file name
        zeros(11 + 4 + 4);
        u32(0);
        zeros(2);
        u32(pregap);
        u32(sectors);
        zeros(6);
        u32(2);  // mode 2
        zeros(12);
        u32(lba);
        u32(pregap + sectors);
        zeros(16);
        u32(1);  // 2336-byte sectors
        zeros(29 + 5);
        u32(0);
        zeros(4 + 8 + 1);
    }
    const std::uint32_t from_end = static_cast<std::uint32_t>(table.size() + 8);
    u32(0x80000006u);
    u32(from_end);
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    std::fwrite(body.data(), 1, body.size(), f);
    std::fwrite(table.data(), 1, table.size(), f);
    std::fclose(f);
    return path;
}

}  // namespace

TEST_CASE("cdi: a self-boot image is presented as the GD-ROM its game was made for") {
    const auto path = write_cdi(11702, 40);
    std::string err;
    auto disc = open_disc(path, err);
    REQUIRE_MESSAGE(disc != nullptr, err);
    CHECK(disc->type() == DiscType::GdRom);
    REQUIRE(disc->tracks().size() == 3);
    CHECK(disc->tracks()[0].lba == 0);
    CHECK(disc->tracks()[0].sectors == 40);
    CHECK(disc->tracks()[1].sectors == 0);  // the placeholder for the second low-density track
    CHECK(disc->tracks()[2].number == 3);
    CHECK(disc->tracks()[2].lba == 11702);
    CHECK(disc->hd_track()->number == 3);
    std::uint8_t sector[2048];
    std::uint32_t word = 0;
    REQUIRE(disc->read_user(11702 + 7, sector));
    std::memcpy(&word, sector, 4);
    CHECK(word == 11702 + 7);
    REQUIRE(disc->read_user(3, sector));
    std::memcpy(&word, sector, 4);
    CHECK(word == 3);
    CHECK(!disc->read_user(11702 + 40, sector));
    std::uint32_t toc[102];
    disc->toc(1, toc);
    CHECK(toc[2] == (0x41u << 24 | (11702u + 150u)));
    CHECK(toc[99] == (0x41u << 24 | (3u << 16)));
    CHECK(toc[101] == (0x41u << 24 | (11742u + 150u)));
    disc.reset();
    std::filesystem::remove(path);
}
