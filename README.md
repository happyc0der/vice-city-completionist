# Vice City Completionist

An in-game 100% checklist for the classic PC version of **Grand Theft Auto: Vice City**, plus the setup notes for a
stable, modern-Windows install to play a completionist run without looking things up.

It was put together in one day for one machine. It works there; read [Limitations](#limitations) before relying on it.

## What's in here

| Part | What it is |
|---|---|
| `src/VC100Checklist.cpp` | An ASI plugin. Reads the running game's completion flags and shows what is still missing for 100%. |
| `src/VC100Display.cpp` | A tiny helper that switches the desktop resolution while the game runs (optional, for borderless window mode). |
| `config/VC100Checklist.ini` | Example settings. |
| `tools/` | The Python scripts used to work out which script variable each of the game's 154 progress points sets. |
| [Setup guide](#setup-guide) | The mod list and settings this was built and tested with. |

No game files and no third-party mods are included.

## The checklist

Press **F7** in game.

| Key | Action |
|---|---|
| F7 | Show or hide the panel |
| F5 / F6 | Previous / next page |
| Page Up / Page Down | Scroll |
| F8 | Mark the next remaining task of this page on the radar and the pause map (pink square) |

Pages: Summary, Story missions, Asset and phone missions, Properties, Odd jobs, Races and challenges, Collectibles.

- Each page lists only what is **left**, with a line saying where to go or what is required: the cars on each
  Sunshine Autos list, the stadium's opening hours, property prices, your best paramedic level so far.
- It updates the moment the game awards a progress point; nothing is read from save files.
- The F8 marker works for anything with a fixed start point: properties, street races, the stadium, the shooting
  range, RC vans, chopper checkpoints, Cone Crazy, PCJ Playground, the dirt track.
- Every time the panel opens it writes `VC100Checklist.log` next to the plugin: every item with its state, and the
  checklist's own point count beside the game's. If those two numbers differ, something is mapped wrong.

Hidden packages, rampages, unique stunt jumps and store robberies are shown as counts only. To see them on the map,
use [Collectibles on Radar](https://github.com/kong78/collectibles-on-radar-gta-vc), which this was designed to sit
next to.

### How it knows

Vice City's mission script calls `PLAYER_MADE_PROGRESS` 154 times; that total is the 100%. `tools/scm.py`
disassembles the stock `main.scm`, `tools/prog.py` lists every one of those calls with the variable the script sets
next to it, and the plugin reads those variables from the script's memory (`0x821280 + offset`) along with a few
stats. The table in `VC100Checklist.cpp` adds up to exactly 154.

## Install

Requirements:

- GTA Vice City for PC with the **v1.0 `gta-vc.exe`** (the Steam and Rockstar Launcher versions need a downgrade).
- The **stock `main.scm`** (no total conversions or script mods).
- An ASI loader, for example [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader); the
  Widescreen Fix ships one.

Steps:

1. Download `VC100Checklist.asi` (and `VC100Display.exe` if you want the resolution switch) from
   [Releases](../../releases), or [build them](#build).
2. Put them, with `config/VC100Checklist.ini`, in the folder your ASI loader loads from. With the Widescreen Fix that
   is the game's `scripts` folder.
3. Start the game, load or start a game, press F7.

If the exe is not v1.0 the plugin does nothing and writes `not loaded: this is not the v1.0 gta-vc.exe` to its log.

To remove it, delete the three files.

## Build

Windows, MSVC x86 (Visual Studio or the free Build Tools, 2019 or 2022):

```
build.cmd
```

Output goes to `build\`. No dependencies beyond the Windows SDK; the plugin does not use plugin-sdk.

## Setup guide

This is the install the plugin was written for. Links go to the authors' own pages.

1. **Game:** classic Vice City from Steam. Launch it once before changing anything.
2. **Downgrade to 1.0:** [TJGM's guide](https://tjgm.dev/guides/gtavc/How-to-Downgrade-EVERY-Version-of-GTA-Vice-City/).
   Keep a copy of the original exe. Do not run Steam's "Verify integrity of game files" afterwards.
3. **[SilentPatch](https://github.com/CookiePLMonster/SilentPatch/releases/latest)** and its DDraw component.
   In `SilentPatchVC.ini`, `ShowPropertyBlips=1` puts purchasable properties on the map.
4. **[Widescreen Fix](https://thirteenag.github.io/wfp#gtavc)** (the released build). Its `global.ini` sets
   `LoadFromScriptsOnly=1`, so every `.asi` has to go in `scripts\`.
5. **[Collectibles on Radar](https://github.com/kong78/collectibles-on-radar-gta-vc)**. Its default toggle key is
   F12, which is Steam's screenshot key; change `on_off_key` (F9 = `120`).
6. **This plugin.**

Settings that matter:

- **Frame Limiter on (30 fps).** The speedrun leaderboard requires it and the game's physics assume it. A crash
  during a rampage happened here at 60 fps with an unreleased build of Fusion Fix; it has not come back at 30.
- **The game's own mouse controls** ("Standard"). No camera mods.
- **Alt+Tab:** exclusive fullscreen blocks it. Create an empty `wndmode.ini` next to `gta-vc.exe` to turn on
  Ultimate ASI Loader's borderless window mode. The window is then as big as the game's resolution; if that is
  lower than your desktop's, set `[Window] DesktopWidth` / `DesktopHeight` in `VC100Checklist.ini` to the game's
  resolution and the helper switches the desktop to match while you play.

Things that did not work, so you don't repeat them:

- **GInput together with the unreleased Fusion Fix:** crash at startup.
- **The unreleased Fusion Fix (free camera, high-fps fixes):** the free camera swings behind the player when
  firing, and a crash followed. Wait for an official release.
- **Stretching the borderless window to the screen:** the loader's window mode re-centers it within milliseconds.
- **Changing the display mode from inside the game process:** the loader's window mode swallows the call.

What speedrunners run, for comparison: the stock 1.0 game, Frame Limiter on, and
[SpeedrunSilentPatch](https://ssp.gta.run/vice-city/) as the only mod
([rules](https://www.speedrun.com/gtavc), [allowed tools](https://www.speedrun.com/gtavc/forums/w0b2j)).

## Limitations

- **v1.0 exe only.** Every address is hard-coded for it. Not 1.1, not the Steam exe, not the Definitive Edition.
- **Stock `main.scm` only.** The flags are byte offsets into the original script's variables. Any mod that
  recompiles the script shifts them and the checklist will show nonsense. The log's two point counts are the check.
- **Tested on one PC** (Windows 11, English game, 16:9) and not through a full playthrough. The flag mapping was
  derived from the script, not confirmed by completing each task. A fresh save reads 1 of 154 here, which is correct.
- **Place hints are written from memory**, not read from the game: whose office, which district, the car lists.
  Names of story missions, property prices and marker positions do come from the game's files.
- **English text only.** Mission names are read from the game's text file and non-ASCII characters show as `?`.
- **The panel is not drawn in menus or cutscenes**, only during gameplay with the HUD on.
- **No marker for story missions** (the game already shows those) or for collectibles (use Collectibles on Radar).
- **The resolution switch changes your whole desktop** while the game runs. Other windows can move or resize, and
  alt-tabbed apps are at the lower resolution too. It is off unless you set it.
- **`AimTurnsPlayer` is an untested experiment** written against a build that was then removed. Leave it off.
- **`FillScreen` does not work** with Ultimate ASI Loader's window mode.
- **Speedrun leaderboards do not allow this plugin** during runs.

## Credits

Built on what others worked out: [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) for the 1.0 memory layout,
the [Sanny Builder library](https://github.com/sannybuilder/library) for the script command definitions,
[Collectibles on Radar](https://github.com/kong78/collectibles-on-radar-gta-vc) for the radar drawing approach,
[SilentPatch](https://github.com/CookiePLMonster/SilentPatch),
[Widescreen Fix and Ultimate ASI Loader](https://github.com/ThirteenAG), and
[PCGamingWiki](https://www.pcgamingwiki.com/wiki/Grand_Theft_Auto:_Vice_City).

Written with Claude Code.

## License

MIT. See [LICENSE](LICENSE). Grand Theft Auto: Vice City is a trademark of Rockstar Games; this project is not
affiliated with or endorsed by Rockstar Games or Take-Two Interactive.
