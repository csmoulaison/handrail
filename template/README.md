# __NAME__

A game built on the [handrail](https://github.com/csmoulaison/handrail) engine, which lives in `handrail/` as a git submodule.

## Building

You need a C compiler, the Vulkan SDK (for headers and `glslangValidator`), and CMake (to build FreeType on the first bootstrap).

- **Linux:** `sh build.bat`, then `cd bin && ./__NAME__`. Needs gcc, X11, and ALSA headers.
- **Windows:** `build.bat`, then `cd bin` and `__NAME__.exe`. Build from a Visual Studio developer prompt, so `cl` is on the PATH.

Run the game from `bin/`. It loads its game library, and writes `log.txt`, relative to the working directory.

`build.bat` takes an optional target and mode (`build.bat dynamic release`):

| Target | What it does |
| --- | --- |
| `bootstrap` | Builds the asset step (`code/prebuild.c`) into a program |
| `static` | Runs the asset step, then builds the executable with the asset pack inside it |
| `dynamic` | Builds the game library (`code/game.c`) |
| `all` | All three, in that order |
| `clean` | Deletes `bin/` and `build/` |

## Hot reload

The game's code is a library that the executable reloads whenever it changes. With the game running, edit `code/game.c` and run `build.bat dynamic`. Game state lives in `game_memory` and survives the reload.

Rebuild with `all` after changing assets, shaders, or `code/prebuild.c`.

## Layout

```
code/
  config.h      Settings every translation unit agrees on: log layers, profiling, engine limit overrides
  prebuild.c    The asset step: packs shaders, textures, and fonts, and writes code/generated/
  game.c        The game: game_init, game_update, game_audio_callback
  draw.c        The renderer front end: turns draw calls into GPU instances
  shaders/      GLSL for draw.c's instances, compiled by prebuild
  generated/    Written by prebuild. Don't edit.
assets/         Source assets prebuild reads
handrail/       The engine (submodule)
```

The comments in each file explain what it does and where in `handrail/code/handrail/` to read more.

## Where to go next

- **Drawing more kinds of things:** add a kind to `DrawInstanceKind` in `code/draw.c` and a case for it in each shader.
- **Input actions and gamepads:** `handrail/code/handrail/input.h`
- **Fixed-timestep simulation:** `handrail/code/handrail/timestep.h`
- **Immediate-mode UI and debug views:** `handrail/code/handrail/ui.h`, `debug_view.h`
- **Meshes, sprites, sound:** `handrail/code/handrail/media/`

For all of these working together (3D meshes, UI, a debug overlay, input rebinding), see the handrail game template: https://github.com/csmoulaison/handrail_game_template
