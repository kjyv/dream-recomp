# Rez

Per-game project for Rez (Japan), HDR-0178. Nothing from the disc is committed. See `.gitignore`.

## What you need

- Your own dump of the disc at `games/rez/disc/Rez (Japan).gdi`, with its track files next to it.
  A self-boot CD-R image (`.cdi`) also runs, but its `1ST_READ.BIN` usually has patches. Set
  `[disc] image` to the image you use.
- The disc extracted into `games/rez/extracted`:

  ```
  dcdisc extract "games/rez/disc/Rez (Japan).gdi" games/rez/extracted
  ```

- A 2 MB boot ROM image from your own console, given to the launcher with `--bios`. The title
  draws its memory-card messages with the system font in the boot ROM. Without it, those messages
  are blank.

## Local layout (gitignored)

```
games/rez/disc/       your dump: Rez (Japan).gdi and its tracks
games/rez/extracted/  output of `dcdisc extract` (IP.BIN, fs/, the boot binary)
```

## Committed here

- `rez.toml`: the per-game config (`docs/game-config.md`): load 0x8C010000 / link 0x0C010000,
  eight discovery seeds for the cases of a computed jump, and the four exception and interrupt
  handlers that the startup code copies to VBR
- `symbols.tsv`: 1,911 Katana SDK function names, found by matching the SDK libraries' code
  against the binary
- `CMakeLists.txt`: `dream_add_game(rez TITLE "Rez")`

## Code overlays

The game loads more code from the disc at run time. The config does not describe these files, so
they run on the development interpreter, and a release build stops at the first one. Each file
starts with a table of entry points, and the game loads it at one of two addresses:

| Address | Files |
| --- | --- |
| `0x8C300000` | `STGxx/STGxx.BIN`, `TITLE/TITLE.BIN`, `TITLE/SEGALOGO.BIN` and others |
| `0x8C380000` | `B_*/B_*.BIN`, `TITLE/AREASEL.BIN`, `TITLE/AUTOLOAD.BIN` and others |

## Running

```
cmake --build build --target rez_boot --parallel
build/games/rez/rez_boot --config games/rez/rez.toml \
    --bios dc_bios.bin --vmu rez.vmu --window
```

`--vmu` creates a blank, formatted memory card if the file does not exist. Without `--window`, the
launcher runs headless and prints a report.
