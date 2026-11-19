# Notes for AI agents working on this repo

Read `README.md` first; this file only covers what you would otherwise get wrong.

## What this is

A 32-bit ASI plugin (`src/VC100Checklist.cpp`) and a helper exe (`src/VC100Display.cpp`) for the classic PC
GTA Vice City with the **v1.0** `gta-vc.exe`. Single-file C++, Win32 only, no third-party libraries.

## Build and verify

- `build.cmd` builds both into `build\`. It must stay warning-clean at `/W3`.
- There are no automated tests. A change is verified by running the game:
  1. Copy the built files into the game's `scripts\` folder (the game must be closed; the files are locked while
     it runs).
  2. Launch the game **un-elevated**. If your shell is elevated, launch through Explorer
     (`explorer.exe "steam://rungameid/12110"`), otherwise the game inherits admin rights.
  3. A crash at startup shows a window titled `Unhandled Exception`; check the main window title before
     concluding it started. Windows also logs the faulting module under Application Error 1000.
  4. In gameplay, F7 writes `scripts\VC100Checklist.log`. The first line compares the checklist's point count
     with the game's own (`game progress X / 154, checklist counts Y / 154`). They must match on any save.
- You usually cannot see or drive the game. Say so; do not report visual behaviour as verified.

## Hard rules

- **Never commit game files or other people's mods:** no `gta-vc.exe`, `main.scm`, `.gxt`, audio, or third-party
  `.asi`/`.dll`. `.gitignore` covers the common ones.
- **Every address is for the v1.0 exe.** Take addresses from
  [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) (`plugin_vc/game_vc`), not from memory, and cite the
  symbol in a comment. Keep the startup check that refuses to hook any other exe.
- **Hooks are CALL-site patches that chain.** `HookCall` reads the existing target and calls it first, because
  other plugins hook the same sites. Do not replace it with a hard-coded original.
- **Keep the plugin read-only by default.** Anything that writes game state must be off unless enabled in the ini.

## The checklist table

- The `Items` table must add up to **154** progress points. If you add or change an entry, re-derive it:
  1. Get `vc/vc.json` from the [Sanny Builder library](https://github.com/sannybuilder/library).
  2. `python tools/scm.py <folder with vc.json> <path to main.scm> scm.pkl` disassembles the stock script.
  3. `python tools/prog.py scm.pkl` lists every `PLAYER_MADE_PROGRESS` call with the mission, its GXT key and the
     global variables set nearby. `tools/ctx.py scm.pkl <before> <after> <offset...>` prints the surrounding code.
  4. `tools/gxt.py` reads `american.gxt` to resolve mission names.
- "Flags" are **byte offsets** into the script's variable space (variable index x 4), read at `0x821280 + offset`.
- Hints that are not read from the game are written from memory. Mark them as such; do not present them as checked.

## Gotchas found the hard way

- **`LoadFromScriptsOnly=1`** in the Widescreen Fix's `global.ini`: plugins outside `scripts\` are not loaded.
- **Ultimate ASI Loader's window mode (`wndmode.ini`) hooks inside the game process.** It swallows
  `ChangeDisplaySettings` (returns success, changes nothing) and re-centers the game window within about 100 ms of
  any resize. That is why the resolution switch lives in a separate process. Don't retry in-process approaches.
- **The helper's mode change uses `CDS_FULLSCREEN`**, so Windows reverts it if the helper dies. Keep that.
- **`GetAsyncKeyState` needs the focus check** (`GameFocused`), or keys pressed in other apps toggle the panel.
- **Shell heredocs mangle backslashes** when generating C++ or Python. Write files with a file tool instead.
- **PowerShell functions return everything they output.** In test scripts, print with `Write-Host` and compare
  against an explicit value, or a "crashed" result reads as success.

## Scope

Keep it small. This is a helper for one game version, not a framework. Prefer documenting an established community
setup over adding a custom hack, and when a custom hack fails in testing, remove it rather than leaving it half-on.
