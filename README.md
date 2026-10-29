# DSXTriggers

NVSE plugin for **Fallout: New Vegas** that automatically maps the weapon in the player's hands to adaptive trigger profiles for a **DualSense** controller through **DSX** over UDP.

> lightweight, native x86 plugin — no CRT dependency, no external runtime, no configuration file required.

## Features

- automatic weapon-type detection from the Fallout: New Vegas process
- adaptive right-trigger profiles for melee, pistols, rifles, automatic weapons, shotguns, sniper rifles, energy weapons and explosives
- RGB status color sent alongside the trigger profile
- automatic profile refresh when the equipped weapon or relevant weapon properties change
- periodic refresh to recover from missed UDP packets
- automatic trigger reset when the DLL is unloaded
- communicates with DSX at `127.0.0.1:6969`
- built as a 32-bit Windows DLL for the classic Fallout: New Vegas runtime

## Requirements

- Fallout: New Vegas
- xNVSE / NVSE-compatible plugin loader
- DualSense controller
- DSX listening on UDP port `6969`
- the game runtime targeted by this plugin: `1.4.0.525`

## Installation

1. Install xNVSE.
2. Install and run DSX with the DualSense connected.
3. Copy `DSXTriggers.dll` into the game's NVSE plugins directory:

```text
Fallout New Vegas/Data/nvse/plugins/DSXTriggers.dll
```

4. Launch the game.
5. Equip different weapons and the right trigger profile should update automatically.

The supplied DLL is already compiled as **PE32 / x86**.

## How it works

The plugin runs a small worker thread that periodically reads the player's current weapon data from the New Vegas process. It derives a trigger profile from weapon type, automatic-fire flags, projectile count, sight FOV and fire rate, then sends the resulting DSX instruction JSON to the local DSX UDP endpoint.

The plugin intentionally keeps the implementation small and dependency-free. Network communication uses Winsock directly and the source is written in C without the C runtime.

## Trigger profiles

| Weapon / category | Trigger behavior | Status color |
|---|---|---|
| Melee | soft trigger | white |
| Energy weapons | adaptive / automatic or semi-auto | green |
| Shotguns | semi-auto resistance | orange/red |
| Automatic weapons | automatic trigger | red |
| Sniper rifles | semi-auto resistance | blue |
| Rifles | semi-auto resistance | orange |
| Pistols | semi-auto resistance | yellow/orange |
| Minigun / flamethrower | automatic trigger | red |
| Grenade launcher | semi-auto resistance | orange |
| Grenades / mines / thrown weapons | resistance | yellow |

These profiles are implementation defaults and can be refined as weapon-specific testing progresses.

## Source

`DSXTriggers.c` contains the complete plugin source. The memory offsets are specific to the targeted Fallout: New Vegas executable and should not be assumed to work with other game builds.

## Building

The source targets **32-bit MinGW/GCC** and uses direct Win32/Winsock imports. A typical build environment needs a Windows-compatible MinGW toolchain and the Winsock library.

Example:

```bash
i686-w64-mingw32-gcc -m32 -shared -nostdlib -Wl,--kill-at -o DSXTriggers.dll DSXTriggers.c -lws2_32 -lkernel32
```

Depending on the toolchain, the exact linker flags may need adjustment. The provided binary is the reference build for this repository.

## Notes

This plugin writes to the DSX UDP endpoint only; it does not modify game saves or game assets. The memory addresses used by the plugin are hard-coded for the supported executable version.

## License

This repository currently contains original project code and a prebuilt binary. Add the license that matches how you want to distribute and modify the project before publishing derivative builds.
