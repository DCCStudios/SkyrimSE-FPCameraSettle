# FP Camera Settle

Adds first-person camera inertia/settle effects to Skyrim Special Edition. The camera responds to player actions with spring-based physics, creating a more dynamic and immersive first-person experience.

## Features

- **Action-based camera inertia** for:
  - Walking & Running (all directions with smooth blending)
  - Sprinting
  - Jumping & Landing
  - Sneaking & Un-sneaking
  - Taking hits & Hitting enemies
  - Arrow/Bolt release (bow & crossbow)

- **Separate settings for weapon drawn vs sheathed states**
- **Per-action configurable parameters:**
  - Enable/disable each action
  - Intensity multiplier (0-10x)
  - Spring stiffness & damping
  - Position & rotation impulses
  - Blend time for smooth impulse application

- **In-game configuration menu** via SKSE Menu Framework
  - Edit Mode toggle for live preview
  - Copy settings between actions
  - Save/Load to INI

- **Performance optimized** with configurable physics substeps
- **Hot reload** support for INI changes

## Requirements

### For Users
- Skyrim Special Edition (1.6.x)
- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/96499) (for in-game menu)

### For Building
- Visual Studio 2022 (v143 toolset)
- CMake 3.21+
- vcpkg (will be configured automatically)

## Building from Source

### 1. Clone the Repository

```bash
git clone --recurse-submodules https://github.com/DCCStudios/SkyrimSE-FPCameraSettle.git
cd SkyrimSE-FPCameraSettle
```

If you already cloned without submodules:
```bash
git submodule update --init --recursive
```

### 2. Configure with CMake

```bash
cmake --preset release
```

This will:
- Download and build vcpkg dependencies (spdlog, simpleini, etc.)
- Configure the project for Release build

### 3. Build

```bash
cmake --build build/release --config Release
```

The compiled plugin will be output to:
```
Compile/SKSE/Plugins/FPCameraSettle.dll
```

### 4. Install

Copy the contents of `Compile/` to your Skyrim `Data/` folder:
```
Data/
└── SKSE/
    └── Plugins/
        ├── FPCameraSettle.dll
        └── FPCameraSettle.ini
```

## Configuration

### In-Game Menu
1. Install SKSE Menu Framework
2. Open the SKSE Menu Framework overlay (default: F2)
3. Navigate to "FP Camera Settle" > "Settings"
4. Enable **Edit Mode** to modify settings with live preview
5. Adjust settings and click "Save to INI" to persist

### INI File
Settings are stored in `Data/SKSE/Plugins/FPCameraSettle.ini`

Key sections:
- `[General]` - Master toggle and global intensity
- `[WeaponState]` - Separate multipliers for drawn/sheathed
- `[Performance]` - Physics substeps (1-8)
- `[ActionName_Drawn]` / `[ActionName_Sheathed]` - Per-action settings

## Project Structure

```
├── src/
│   ├── main.cpp           # Plugin entry point, hooks
│   ├── CameraSettle.cpp/h # Core spring physics & action detection
│   ├── Settings.cpp/h     # INI loading/saving
│   ├── Menu.cpp/h         # SKSE Menu Framework UI
│   └── PCH.h              # Precompiled header
├── extern/
│   └── CommonLibSSE/      # CommonLibSSE-NG (submodule)
├── CMakeLists.txt
├── vcpkg.json             # Dependencies
└── FPCameraSettle.ini     # Default configuration
```

## Dependencies

- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) - SKSE plugin development library
- [spdlog](https://github.com/gabime/spdlog) - Logging
- [SimpleIni](https://github.com/brofield/simpleini) - INI file handling
- [SKSE Menu Framework](https://github.com/ArranzCNL/SKSEMenuFramework) - In-game UI (header-only)

## License

MIT License - See [LICENSE](LICENSE) for details.

## Credits

- CharmedBaryon for CommonLibSSE-NG
- The SKSE team
- ArranzCNL for SKSE Menu Framework
