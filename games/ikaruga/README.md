# Ikaruga

Per-game project for Ikaruga (Japan), T38706M. Nothing from the disc is committed; see
`.gitignore`.

## What you need

- Your own dump of the disc at `games/ikaruga/disc/Ikaruga (Japan).chd`.
- The disc extracted into `games/ikaruga/extracted`:

  ```
  dcdisc extract "games/ikaruga/disc/Ikaruga (Japan).chd" games/ikaruga/extracted
  ```

- A 2 MB boot ROM image from your own console, given to the launcher with `--bios`. The title
  draws its memory-card messages with the system font in the boot ROM. Without it, those messages
  are blank.

## Local layout (gitignored)

```
games/ikaruga/disc/       your dump: Ikaruga (Japan).chd
games/ikaruga/extracted/  output of `dcdisc extract` (IP.BIN, fs/, the boot binary)
```

## Committed here

- `ikaruga.toml`: the per-game config (`docs/game-config.md`): load 0x8C010000 / link 0x0C010000,
  two discovery seeds, and the four exception and interrupt handlers that the startup code copies
  to VBR
- `symbols.tsv`: 1,717 Katana SDK function names, found by matching the SDK libraries' code
  against the binary
- `CMakeLists.txt`: `dream_add_game(ikaruga TITLE "Ikaruga")`

## Running

```
cmake --build build --target ikaruga_boot --parallel
build/games/ikaruga/ikaruga_boot --config games/ikaruga/ikaruga.toml \
    --bios dc_bios.bin --vmu ikaruga.vmu --window
```

`--vmu` creates a blank, formatted memory card if the file does not exist. Without `--window`, the
launcher runs headless and prints a report.
