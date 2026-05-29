# Songoo Refactoring to Nkentseu/NKECS

## Overview

Songoo has been completely refactored to match Pong's professional architecture:

- **Zero external dependencies** - All from Nkentseu ecosystem (NKWindow, NKContext, NKFont, NKImage, NKAudio, NKUI)
- **POO Architecture** - Scene-based UI system with SceneManager
- **Cross-Platform Ready** - Build for Windows + Android via Jenga (Songoo.jenga)
- **Clean Separation** - Core (SongooGame), Scenes (UI flow), Systems (Game logic), Rendering, Audio

## Project Structure

```
src/Songoo/
├── Apps.cpp                           # Entry point (nkmain)
├── Core/
│   ├── SongooGame.h/cpp              # Main application class
│   └── SongooTypes.h                 # Game enums, constants, types
├── Game/
│   ├── Board/
│   │   ├── SongooBoard.h/cpp         # Mancala logic
│   │   ├── [Pit.h, Player.h, etc]   # TODO: Components/structs
│   ├── AI/                            # TODO: AI systems
│   └── Systems/                       # TODO: Game systems
├── Render/
│   ├── GLRenderer2D.h/cpp            # 2D rendering wrapper
│   ├── FontAtlas.h/cpp               # Text rendering
│   └── [BoardRenderer.h, etc]        # TODO: Specialized renderers
├── Audio/
│   ├── AudioManager.h/cpp            # NKAudio integration
│   └── AudioEvents.h                 # TODO: Event types
├── UI/
│   ├── Scene.h                       # Base scene interface
│   ├── SceneManager.h/cpp            # Scene stack management
│   ├── AppContext.h                  # Shared context POD
│   ├── Theme.h                       # Colors, visual constants
│   ├── UIScale.h                     # Responsive design helpers
│   └── Scenes/
│       ├── SplashScene.h/cpp         # Loading/splash
│       ├── MainMenuScene.h/cpp       # Menu (TODO: full UI)
│       ├── GameplayScene.h/cpp       # TODO: Main game loop
│       ├── GameOverScene.h/cpp       # TODO: Results screen
│       ├── SettingsScene.h/cpp       # TODO: Options menu
│       └── StoryScene.h/cpp          # TODO: Intro story
└── Resources/
    └── ResourceManager.h/cpp         # TODO: Asset loading cache
```

## Jenga Build Configuration

**Songoo.jenga** configured for:
- **Windows** (clang-mingw toolchain)
- **Android** (NDK, min SDK 24, target SDK 34)
- All dependencies resolved via Jenga links (no hardcoded paths)
- Assets from `Resources/Songoo/` packaged as APK assets on Android

Build commands:
```bash
jenga build --platform windows --config Release      # Windows
jenga build --platform android --config Release      # Android APK
```

## Implementation Status

### ✅ Completed
- [x] Folder structure created
- [x] Songoo.jenga configuration
- [x] Core types & enums (SongooTypes.h)
- [x] Scene framework (Scene.h, SceneManager, AppContext)
- [x] SongooGame (main app orchestrator)
- [x] GLRenderer2D & FontAtlas (wrapper stubs)
- [x] AudioManager (wrapper stubs)
- [x] SplashScene + MainMenuScene (basic flow)
- [x] Apps.cpp (nkmain entry point)
- [x] SongooBoard (Mancala logic skeleton)
- [x] Theme & UIScale helpers

### 🔄 In Progress
- [ ] Complete SongooBoard logic (seeding, captures, rules)
- [ ] Implement remaining scenes (Gameplay, GameOver, Settings, Story)
- [ ] GLRenderer2D/FontAtlas full integration with Nkentseu RHI
- [ ] Audio sample loading & playback
- [ ] Touch input handling for Android
- [ ] Game systems (AI, animations, particles)

### ⚠️ TODO (Phase 2)
- [ ] Full NKECS integration for gameplay entities
- [ ] AI difficulty levels
- [ ] Network/Multiplayer (future)
- [ ] Visual polish (animations, particles, effects)
- [ ] Settings persistence (JSON config)

## Key Architecture Decisions

1. **No STL or External Libs** - Uses Nkentseu containers (NkVector, etc.)
2. **Scene-Based UI** - Matches Pong pattern, familiar to team
3. **AppContext POD** - Simple passthrough, avoids deep parameter chains
4. **Lazy Initialization** - Audio, GL resources init only when needed
5. **Memory via NkMemory** - nk_new/nk_delete for consistent allocation

## Next Steps

1. Implement Gameplay Scene logic
2. Wire GLRenderer2D to actual rendering (integrate with NKContext)
3. Load resources (textures, fonts, audio) from Resources/Songoo/
4. Test Windows build first
5. Iterate render/audio stubs to real implementations
6. Build & test Android APK
7. Polish & shipping prep

## Notes for Developers

- All scenes inherit from Scene base class and implement lifecycle methods
- AppContext is rebuilt each frame with fresh pointers (safe, simple)
- SongooGame owns all subsystems; scenes are ephemeral
- No global state except logger (inherited from Nkentseu)
- Follow Pong patterns for consistency (identical folder org, naming, patterns)

---

**Status**: Foundation complete, ready for gameplay implementation.  
**Target**: Full playable game on Windows + Android by [TBD]
