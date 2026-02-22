# VirtualStage

## What is this?

VirtualStage is a cross-platform 3D visualization and layout tool for designing virtual LED/projection screen setups. Built on **openFrameworks 0.12.0**, it lets stage designers create, arrange, and preview multiple virtual screens in 3D space with live video feeds via Syphon (macOS) or Spout (Windows).

### Key capabilities
- Create and transform (position, rotate, scale, curve) virtual screens in 3D
- Receive live video textures from other applications (Resolume, etc.) via Syphon/Spout
- Import Resolume Advanced Output XML presets for automatic layout generation
- Per-screen input mapping editor with crop and polygon masking
- Save/load projects as JSON
- Designer mode (editing) and View mode (clean preview)

## Architecture

```
ofApp  (main controller — handles input, rendering, UI, project I/O)
 ├── Scene          (scene graph: screen collection + server directory)
 │    ├── vector<ScreenObject>   (the virtual screens)
 │    └── Syphon/Spout server list
 ├── Gizmo          (3D translate/rotate/scale manipulation handles)
 ├── PropertiesPanel (ofxGui inspector for selected screen properties)
 └── ofEasyCam      (3D camera navigation)
```

### Data flow
1. `Scene` owns all `ScreenObject` instances and the available video server list
2. `ofApp` delegates rendering, picking, and serialization to `Scene`
3. `Gizmo` operates on the currently selected `ScreenObject` via `ofApp`
4. `PropertiesPanel` bidirectionally syncs parameters with the selected `ScreenObject`
5. Each `ScreenObject` independently manages its Syphon/Spout client for texture reception

## Project structure

```
VirtualStage/
├── src/
│   ├── main.cpp              (12 lines)   Entry point, GLFW window setup
│   ├── ofApp.h/cpp           (71+1018)    Main app: modes, rendering, UI, I/O
│   ├── ScreenObject.h/cpp    (91+593)     Virtual screen: mesh, texture, transform
│   ├── Scene.h/cpp           (82+331)     Scene graph, picking, server directory
│   ├── Gizmo.h/cpp           (40+205)     3D manipulation gizmo
│   ├── PropertiesPanel.h/cpp (51+115)     GUI property editor
│   └── win_byte_fix.h        (17)         Windows SDK std::byte conflict fix
├── addons.make               ofxSyphon + ofxGui (macOS)
├── addons.make.win           ofxSpout + ofxGui (Windows)
├── config.make               OF build configuration
├── Makefile                  Build script + macOS bundle rules
├── .github/workflows/
│   └── build-windows.yml     CI/CD: builds macOS + Windows, creates releases
├── VirtualStage.xcodeproj/   Xcode project (macOS)
├── bin/                      Output binaries
└── of.icns                   App icon
```

## Key classes

### ofApp (`src/ofApp.h/cpp`)
Main controller. Two modes: **Designer** (3D editor with grid, gizmo, panels) and **View** (clean fullscreen preview). Handles:
- Keyboard shortcuts (Tab=mode, W/E/R=gizmo, A=add, Del=delete, M=mapping editor)
- Resolume XML import (`loadResolumeXml`) — parses slices/polygons from Advanced Output
- Input mapping editor — 2D crop UI with snap-to-grid and 8-point resize handles
- Project save/load as JSON (Cmd+S / Cmd+O)
- Server list panel, toolbar, status bar rendering

### ScreenObject (`src/ScreenObject.h/cpp`)
Individual virtual screen. Supports three mesh types:
- **Flat** — `ofPlanePrimitive` for standard rectangular screens
- **Curved** — Custom `ofVboMesh` for cylindrical arc screens (-180 to +180 degrees)
- **Polygon** — Tessellated `ofVboMesh` for non-rectangular masked shapes

Manages its own Syphon/Spout client, texture crop rectangle (0-1 normalized), and full JSON serialization.

### Scene (`src/Scene.h/cpp`)
Owns the collection of `ScreenObject`s. Provides:
- Add/remove screens with auto-incrementing IDs
- Ray-based picking with depth sorting
- Server directory (Syphon announcements or Spout polling every 1s)
- Project serialization/deserialization with camera state
- Source auto-reconnection on project load

### Gizmo (`src/Gizmo.h/cpp`)
3D manipulation widget with translate/rotate/scale modes. Uses 2D screen-space projection for picking (point-to-line distance). Gizmo size scales with camera distance.

### PropertiesPanel (`src/PropertiesPanel.h/cpp`)
ofxGui-based inspector panel. Exposes position, rotation, scale, curvature, and input mapping crop parameters with bidirectional sync to the selected `ScreenObject`.

## Platform abstraction

Platform-specific code is isolated via `#ifdef`:
```cpp
#ifdef TARGET_OSX
    ofxSyphonClient / ofxSyphonServerDirectory
#elif defined(TARGET_WIN32)
    SpoutReceiver / ofxSpout polling
#endif
```
The `win_byte_fix.h` header resolves `std::byte` conflicts between GCC 15+ and Windows SDK.

## Dependencies

| Addon/Library | Platform | Purpose |
|---|---|---|
| ofxSyphon | macOS | Receive textures from other apps |
| ofxSpout | Windows | Receive textures from other apps |
| ofxGui | Both | Parameter panels and UI controls |
| ofxJson (OF core) | Both | Project save/load |
| ofxXml (OF core) | Both | Resolume XML preset parsing |
| GLFW | Both | Windowing (via ofAppGLFWWindow) |
| GLM | Both | 3D math |

## Building

### macOS
```bash
make Release -j$(sysctl -n hw.ncpu)
make bundle   # embeds Syphon.framework into .app
```
Or open `VirtualStage.xcodeproj` in Xcode.

### Windows (MSYS2/MinGW64)
```bash
# Requires openFrameworks for MSYS2 and ofxSpout addon
make Release -j$(nproc)
```

### CI/CD
GitHub Actions (`.github/workflows/build-windows.yml`) builds both platforms automatically. Pushes to tags matching `v*` create GitHub Releases with platform-specific zips.

## Keyboard shortcuts

| Key | Action |
|---|---|
| Tab | Toggle Designer/View mode |
| W / E / R | Gizmo: Translate / Rotate / Scale |
| A | Add new screen |
| Delete | Delete selected screen |
| M | Open input mapping editor |
| L | Import Resolume XML |
| Cmd+S | Save project |
| Cmd+O | Open project |
| 1-9 | Assign video source to selected screen |
| Cmd+1/2/3/4 | Camera presets (front/top/3-4/reset) |
| F | Toggle always-on-top |

## Code conventions
- C++11/17, openFrameworks style (camelCase methods, member variables without prefix)
- Platform-specific blocks use `#ifdef TARGET_OSX` / `#elif defined(TARGET_WIN32)`
- JSON for serialization (`ofJson` / nlohmann::json)
- All source files in flat `src/` directory (no subdirectories)
- ~2600 lines of application code total
