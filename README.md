# Dynamic (Geode mod)

An input-timing analyzer for Geometry Dash that helps spot botted or macro-injected runs.

> **Important:** this is my own reconstruction. The mod shown in WhiteChocolate's video
> ("Top Players are cheating in Geometry Dash") was not released publicly at the request of the
> list team, so its exact internals are unknown. This project implements the general idea
> (detecting inputs that don't behave like human input) using standard statistics.
> It is not a copy of that mod and is not affiliated with it.

## How it works

1. Hooks real hardware input (`CCKeyboardDispatcher`, `CCTouchDispatcher`) and timestamps it.
2. Hooks the game's own click handling (`GJBaseGameLayer::handleButton`) and the frame start (`GJBaseGameLayer::update`).
3. Matches each in-game click to a recent hardware event and measures when that event arrived inside the frame.
4. On level completion it produces a report with these checks:
   - **Unmatched inputs:** in-game clicks with no keyboard/mouse/touch event behind them (macro/bot injection).
   - **Phase lock (Rayleigh test):** human input lands at random points in a frame; injected input does not.
   - **Constant delay:** an input-to-frame delay with almost no spread.
   - **Repeated intervals:** informational only, since spam sections can do this legitimately.

The result is a heuristic, not proof. Keyboard events for unrelated keys count as hardware input, which makes
the unmatched check conservative. Do not publicly accuse anyone based on this output alone.

## Settings

Show live HUD, show report on completion, save reports to disk (`<mod save dir>/reports/run_<timestamp>.txt`),
ignore practice mode, minimum clicks for a verdict, and the input match window in ms.

## Building

Requirements: [Geode SDK + CLI](https://docs.geode-sdk.org/getting-started/) with `GEODE_SDK` set,
CMake 3.21+, and a C++20 compiler (MSVC / clang-cl on Windows, Xcode on macOS, NDK for Android).

```
geode build
```

The output is `yourname.dynamic.geode`. Drop it into the `geode/mods` folder of your Geometry Dash install.
Or push this repo to GitHub: the included workflow builds for Windows, macOS, Android and iOS.

Before publishing, change `id` and `developer` in `mod.json`.

## Version support

Targets **Geometry Dash 2.2081** and **Geode 5.11.x**. For another GD version, change the `gd` and `geode` versions in `mod.json`
and check hook signatures against the [bindings](https://github.com/geode-sdk/bindings). For example, GD 2.2074 uses a
3-argument `dispatchKeyboardMSG` while 2.2081 adds a `double timestamp`.

## Layout

```
mod.json            mod metadata and settings
CMakeLists.txt      build config
src/main.cpp        hooks
src/Analyzer.hpp    statistics and report text
src/Hud.hpp         live HUD node
about.md            shown on the mod page
changelog.md
.github/workflows/  CI build
```
