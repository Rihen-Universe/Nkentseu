# Architecture Nkoung - Plateforme de jeux 2D

## Vue d'ensemble

**Nkoung** est une plateforme multi-jeux permettant de sélectionner et jouer à plusieurs jeux 2D dans un même exécutable. L'architecture est modulaire, chaque jeu étant indépendant et pouvant être développé/maintenu séparément.

### Objectifs

- **Plateforme unifiée** : 6 jeux 2D accessibles depuis un menu central
- **Zéro-STL** : architecture en ligne avec Nkentseu (pas de `std::`)
- **Multi-fichier** : chaque jeu a son dossier et ses dépendances
- **Extensible** : ajouter un nouveau jeu = ajouter un dossier + implémenter NkoungGame

---

## Structure des répertoires

```
Applications/Nkoung/src/Nkoung/
├── main.cpp                           # Entry point
├── Core/
│   ├── NkoungConfig.h/cpp            # Config globale, allocateurs, logger
│   └── ...
├── Platform/
│   ├── NkoungPlatformApp.h/cpp       # Orchestrateur principal
│   └── ...
├── Games/
│   ├── Common/
│   │   ├── NkoungGame.h              # Classe abstraite jeu
│   │   ├── NkoungGameLevel.h         # Classe abstraite niveau
│   │   ├── GameMetadata.h            # Enums & infos jeux
│   │   ├── GameFactory.h/cpp         # Factory création jeux
│   │   └── ...
│   └── Specific/
│       ├── LaserPuzzle/
│       │   ├── LaserPuzzleGame.h/cpp # Impl Laser Puzzle
│       │   ├── Assets/
│       │   │   └── levels/           # JSON niveaux
│       │   └── ...
│       ├── Territories/
│       │   └── ...
│       └── ...
└── UI/
    ├── NkoungUITheme.h/cpp           # Thème personnalisé
    └── ...
```

---

## Flux d'exécution

### 1. Démarrage (`main.cpp`)

```cpp
int nkmain(const NkEntryState& state) {
    NkoungPlatformApp app;
    if (!app.Initialize(state)) return -1;
    return app.Run();
}
```

### 2. Initialisation (`NkoungPlatformApp::Initialize`)

- Parse les arguments (`--backend=dx11`, etc.)
- Crée la fenêtre NKWindow
- Initialise le contexte de rendu NKCanvas (NkRenderWindow)
- Initialise l'UI NKUI (contexte, backend canvas, polices)
- Affiche le menu de plateforme

### 3. Menu de plateforme

```
[Plateforme Nkoung v0.2.0]
┌─────────────────────────────────────────────┐
│ 1. Laser Puzzle          [Prototype ▶]     │
│ 2. Territoires           [À concevoir]      │
│ 3. Gardien du Labyrinthe [Prototype ▶]     │
│ 4. Ponts et Chemins      [À concevoir]      │
│ 5. Flux                  [À concevoir]      │
│ 6. Tactique              [À concevoir]      │
│                                              │
│ Entrée : lancer | Esc : quitter             │
└─────────────────────────────────────────────┘
```

Souris ou clavier :
- **Clic / Flèches + Entrée** : sélectionner & lancer un jeu
- **ESC** : quitter la plateforme

### 4. Scène de jeu

```cpp
NkoungGame* game = GameFactory::CreateGame(GameId::LaserPuzzle);
game->Init();
while (running) {
    game->Update(dt);
    game->Render(renderer);
    game->OnEvent(event);
}
game->Unload();
```

---

## Interface d'un jeu

Tout jeu Nkoung dérive de `NkoungGame` :

```cpp
class NkoungGame {
public:
    virtual bool Init() noexcept = 0;                    // Initialiser
    virtual void Update(float32 dt) noexcept = 0;        // Logique
    virtual void Render(NkRenderer2D&) noexcept = 0;     // Rendu
    virtual void OnEvent(NkEvent*) noexcept = 0;         // Événements
    virtual void Unload() noexcept = 0;                  // Nettoyer
    virtual const char* GetTitle() const noexcept = 0;   // Titre
    virtual bool WantExit() const noexcept = 0;          // Vouloir quitter
};
```

### Exemple : LaserPuzzleGame

```cpp
class LaserPuzzleGame : public NkoungGame {
    bool Init() noexcept override {
        return LoadTestLevel();
    }
    
    void Update(float32 dt) noexcept override {
        // Logique jeu
    }
    
    void Render(NkRenderer2D& renderer) noexcept override {
        // Dessiner grille, rayons, UI
    }
    
    void OnEvent(NkEvent* event) noexcept override {
        // Clic pour sélectionner, R pour tourner, ESC pour quitter
    }
};
```

---

## Configuration globale

### Allocateurs (`NkoungConfig.h`)

```cpp
namespace nkoung::memory {
    extern NkAllocator* gDefaultAllocator;      // Conteneurs, objets
    extern NkAllocator* gResourceAllocator;     // Textures, niveaux
    extern NkAllocator* gScratchAllocator;      // Scratch temporaire
    
    void InitializeAllocators() noexcept;
    void ShutdownAllocators() noexcept;
}
```

### Logger

```cpp
NKOUNG_LOG_INFO("Message");
NKOUNG_LOG_INFOF("Format: %d", value);
NKOUNG_LOG_WARN("Warning");
NKOUNG_LOG_ERROR("Error");
```

### Constantes globales

```cpp
namespace nkoung::globals {
    static constexpr uint32 DEFAULT_WINDOW_WIDTH = 1280;
    static constexpr uint32 DEFAULT_WINDOW_HEIGHT = 720;
    static constexpr float32 TARGET_FPS = 60.0f;
    extern NkString gDataPath;  // Chemin dynamique pour assets
}
```

---

## Ajouter un nouveau jeu

### 1. Créer le dossier du jeu

```
Games/Specific/MonJeu/
├── MonJeuGame.h/cpp
├── MonJeuLevel.h/cpp (optionnel)
├── Assets/
│   └── levels/          # JSON, images, etc.
└── ...
```

### 2. Implémenter `MonJeuGame : public NkoungGame`

```cpp
class MonJeuGame : public NkoungGame {
    bool Init() noexcept override;
    void Update(float32 dt) noexcept override;
    void Render(NkRenderer2D&) noexcept override;
    void OnEvent(NkEvent*) noexcept override;
    void Unload() noexcept override;
    const char* GetTitle() const noexcept override { return "Mon Jeu"; }
};
```

### 3. Enregistrer dans `GameFactory`

1. Ajouter l'enum dans `GameMetadata.h` :
   ```cpp
   enum class GameId {
       LaserPuzzle,
       MonJeu,  // ← NOUVEAU
       Count
   };
   ```

2. Ajouter la GameInfo dans `GameFactory.cpp` :
   ```cpp
   static const GameInfo kGameInfoTable[] = {
       // ... LaserPuzzle ...
       {
           GameId::MonJeu,
           "Mon Jeu",
           "Subtitle",
           "Description.",
           GameStatus::Prototype,
           true  // playable
       }
   };
   ```

3. Implémenter la création dans `GameFactory::CreateGame` :
   ```cpp
   case GameId::MonJeu:
       game = new(allocator->Alloc(sizeof(MonJeuGame))) MonJeuGame();
       break;
   ```

### 4. Compiler

```bash
jenga build --target Nkoung
```

---

## Architectures possibles pour chaque jeu

### Option 1 : Simple (stateful)
- Une seule instance du jeu
- Niveau unique ou boucle linéaire
- Exemple : **Laser Puzzle** MVP

### Option 2 : Avec niveaux
- Manager de niveaux (`NkoungGameLevel`)
- Sauvegardes de progression
- Exemple : **Territories**, **Ponts**

### Option 3 : Avec ECS (futur)
- Intégration optionnelle de NKECS
- Utile pour les jeux plus lourds
- Exemple : **Tactique avancée**

---

## Utilisation des modules Nkentseu

### NKWindow
- Créer fenêtre : `mWindow.Create(cfg)`
- Gestion événements : `NkEvents().PollEvent()`

### NKCanvas
- Rendu 2D : `NkRenderWindow` + `NkRenderer2D`
- Formes : rectangles, cercles, lignes, triangles

### NKUI
- Menu de plateforme utilise `NkUIContext`
- Les jeux peuvent aussi l'utiliser pour menus in-game

### NKEvent
- Polymorphe : `event->Is<T>()`, `event->As<T>()`
- Catégories : `NK_CAT_INPUT | NK_CAT_KEYBOARD`

### NKMemory
- Allocateurs typés
- Smart pointers : `NkUniquePtr`, `NkSharedPtr`

---

## Compilation & exécution

### Build

```bash
jenga build --target Nkoung --config Debug
jenga build --target Nkoung --config Release
```

### Exécution

```bash
# Défaut (DX11 sur Windows, OpenGL ailleurs)
./Nkoung.exe

# Forcer Vulkan
./Nkoung.exe --backend=vulkan

# Forcer OpenGL
./Nkoung.exe --backend=opengl

# Forcer DirectX 11
./Nkoung.exe --backend=dx11

# Forcer Software
./Nkoung.exe --backend=software
```

---

## État actuel (juin 2026)

### Livré
- ✅ Architecture multi-jeux
- ✅ Orchestrateur plateforme (NkoungPlatformApp)
- ✅ Menu de sélection basique
- ✅ LaserPuzzleGame prototype (grille 6×6, miroirs)
- ✅ Support backends (DX11, OpenGL, Vulkan, DX12, Software)

### En cours
- 🔶 LaserPuzzle : complétude rayons, niveaux JSON
- 🔶 UI menu profesionnelle (listes, icônes)

### À venir
- ⏳ Territories : jeu complet
- ⏳ Autres jeux (Bridges, Flow, Tactics, etc.)
- ⏳ Système de sauvegarde/progression
- ⏳ Intégration optionnelle NKECS pour jeux lourds

---

## Notes de développement

### Mémoire
- Chaque jeu alloue via `memory::gResourceAllocator` pour ses assets
- GameFactory alloue les instances via `memory::gDefaultAllocator`
- À la fin du jeu : `game->Unload()` + destruction automate smartptr

### Resize & DPI
- NkRenderWindow gère le resize automatiquement
- La plateforme détecte les changements de taille et appelle `OnResize()`

### Events
- La plateforme collecte tous les events
- En mode menu : routing vers HandlePlatformMenuEvent
- En mode jeu : routing vers `game->OnEvent()`

### Threading
- Single-threaded par défaut (NKWindow thread polling)
- Les allocateurs et logger thread-safe si nécessaire

---

## Ressources

- [ARCHITECTURE.md](../../ARCHITECTURE.md) — Architecture Nkentseu complète
- [Kernel/Runtime/NKCanvas/USAGE.md](../../Kernel/Runtime/NKCanvas/USAGE.md) — Usage NKCanvas
- [GDDs jeux](./docs/) — Spécifications design détaillées
