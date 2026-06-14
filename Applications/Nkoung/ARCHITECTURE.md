# Architecture Nkoung 0.2.0 — Plateforme Multi-Jeux

## Vue d'ensemble

Nkoung est restructuré en une **plateforme professionnelle** permettant de :
1. Sélectionner et lancer des jeux 2D depuis un menu central
2. Jouer des jeux indépendants (chacun avec ses propres niveaux, règles, assets)
3. Revenir au menu et choisir un autre jeu
4. Étendre facilement avec de nouveaux jeux (implémentation d'une interface)

## Architecture en couches

```
Nkoung/
├─ src/Nkoung/
│  ├─ main.cpp                             # Point d'entrée, initialise NkoungPlatformApp
│  │
│  ├─ Core/
│  │  ├─ NkoungConfig.h/cpp                # Config globale, allocateurs, logger
│  │  └─ ...
│  │
│  ├─ Platform/
│  │  ├─ NkoungPlatformApp.h/cpp           # Orchestrateur principal (fenêtre, scènes, UI)
│  │  └─ ...
│  │
│  └─ Games/
│     ├─ Common/
│     │  ├─ NkoungGame.h                   # Classe abstraite (interface)
│     │  ├─ NkoungGameLevel.h              # Classe abstraite pour les niveaux
│     │  ├─ GameMetadata.h                 # Enum GameId, GameInfo, enum AppScene
│     │  └─ GameFactory.h/cpp              # Factory pour créer des jeux par ID
│     │
│     └─ Specific/
│        ├─ LaserPuzzle/
│        │  ├─ LaserPuzzleGame.h/cpp       # Implémentation du jeu Laser Puzzle
│        │  ├─ LaserRaySimulation.h        # (À implémenter) Sim rayon laser
│        │  ├─ LaserGUI.h/cpp              # (À implémenter) UI in-game
│        │  └─ Assets/
│        │     └─ levels*.json             # Données niveaux
│        │
│        ├─ Territories/
│        │  ├─ TerritoriesGame.h/cpp       # (À implémenter)
│        │  ├─ TerritoriesAI.h/cpp        
│        │  └─ Assets/
│        │
│        ├─ LabyouthGame.h/cpp             # (Existant, à refactoriser)
│        ├─ Bridges/... (à venir)
│        ├─ Flow/... (à venir)
│        └─ Tactics/... (à venir)
```

## Flot d'exécution

### 1. Démarrage (`main.cpp` → `nkmain()`)
```cpp
int nkmain(const NkEntryState& state) {
    NkoungPlatformApp app;
    if (!app.Initialize()) return -1;  // Init fenêtre, canvas, UI
    return app.Run();                   // Boucle principale
}
```

### 2. Initialisation (`NkoungPlatformApp::Initialize()`)
- Crée une fenêtre NkWindow
- Initialise NkRenderWindow (cible de rendu NKCanvas)
- Initialise NKUI (contexte + backend + font)
- Configure les callbacks d'événements
- Lance la scène PlatformMenu
- **Allocateurs** : initialisés via `memory::InitializeAllocators()`

### 3. Boucle principale (`NkoungPlatformApp::Run()`)
```
Tant que running && window.IsOpen() :
  ├─ Input : poll événements, update state
  ├─ Resize : détecte changement taille fenêtre
  ├─ Update : logique selon scène (Menu ou Jeu)
  ├─ Render :
  │  ├─ target.Clear() → reset rendu
  │  ├─ affiche Menu OU jeu → NkRenderer2D
  │  └─ target.Display() → présente
```

### 4. Scènes

#### Scène PlatformMenu (`AppScene::PlatformMenu`)
- Affiche liste des jeux avec descriptions
- Permet sélection + lancement via clique/Enter
- Gère quitter l'app

**Implémentation** : `NkoungPlatformApp::UpdatePlatformMenu()` + `RenderPlatformMenu()`

#### Scène GameScene (`AppScene::GameScene`)
- Crée instance du jeu via `GameFactory::CreateGame(id, allocator)`
- Appelle `game->Init()`
- Chaque frame : `game->Update(dt)` → `game->Render(renderer)` → `game->OnEvent(event)`
- Si `game->WantExit()` → retour au menu
- Libère jeu via `game->Unload()` (destructeur de NkUniquePtr)

**Implémentation** : `NkoungPlatformApp::UpdateGameScene()` + `RenderGameScene()`

## Interfaces clés

### `NkoungGame` (classe abstraite)
```cpp
class NkoungGame {
public:
    virtual bool Init() noexcept = 0;
    virtual void Update(float32 dt) noexcept = 0;
    virtual void Render(NkRenderer2D& renderer) noexcept = 0;
    virtual void OnEvent(NkEvent* event) noexcept = 0;
    virtual void Unload() noexcept = 0;
    virtual const char* GetTitle() const noexcept = 0;
    virtual bool WantExit() const noexcept = 0;
    // ... getters optionnels (niveau courant, progression, etc.)
};
```

**Chaque jeu dérive** de `NkoungGame` et implémente ces méthodes.

### `GameFactory` (factory pattern)
```cpp
static NkUniquePtr<NkoungGame> 
CreateGame(GameId id, NkAllocator* allocator = nullptr) noexcept;

static const GameInfo* GetGameInfo(GameId id) noexcept;
static nk_uint32 GetGameCount() noexcept;
static const GameInfo* GetAllGames() noexcept;
```

**Usage** :
```cpp
auto game = GameFactory::CreateGame(GameId::LaserPuzzle, memory::gDefaultAllocator);
if (game && game->Init()) {
    // play...
}
// Destruction automatique via NkUniquePtr
```

### `GameMetadata.h` (énums partagées)
```cpp
enum class GameId { LaserPuzzle, Territories, Labyrinth, ... };
enum class AppScene { PlatformMenu, GameScene };
enum class GameStatus { NotStarted, Prototype, AlphaBuild, ... };
struct GameInfo { GameId id, title, subtitle, description, status, playable };
```

## Prototype : LaserPuzzleGame

### Fonctionnalités MVP
- Grille 6×6 de tuiles (source, miroir, cible, mur, vide)
- Rayontrace laser partant d'une source
- Interaction : clic tuile → sélection + rotation (R)
- Victoire : rayon atteint cible
- Menu Escape pour quitter

### Structure
```cpp
class LaserPuzzleGame : public NkoungGame {
    nk_uint32 mGridWidth, mGridHeight;
    float32 mCellSize;
    NkVector<LaserTile> mGrid;      // État de la grille
    NkVector<LaserSegment> mLaserPath;  // Segments dessinés
    int32 mSelectedTileIndex;
    nk_uint32 mCurrentLevel, mMovesCount;
    
    // Methods
    bool LoadTestLevel() noexcept;
    void SimulateRay() noexcept;    // Calcule le trajet du laser
    int32 GetTileAtPosition(...) const noexcept;
    void RotateTile(int32 idx) noexcept;
};
```

### Format d'extension (pour les vrais niveaux)
```json
{
  "id": 1,
  "title": "Level 1 - Tutorial",
  "width": 6,
  "height": 6,
  "source": { "x": 0, "y": 0, "direction": "right" },
  "tiles": [
    {"pos": [0,0], "type": "source"},
    {"pos": [1,1], "type": "mirror", "orientation": 0},
    {"pos": [5,5], "type": "target"}
  ]
}
```

## Mémoire et allocateurs

### Stratégie
- **gDefaultAllocator** : objets jeux, conteneurs principaux
- **gResourceAllocator** : textures, polices, niveaux (lifecycle = scène)
- **gScratchAllocator** : temp work, reset après chaque frame/scène

### Pattern
```cpp
// Créer un conteneur
NkVector<MyType> list(memory::gDefaultAllocator);

// Créer un objet (placement new)
void* ptr = memory::gDefaultAllocator->Alloc(sizeof(MyType));
auto* obj = new(ptr) MyType(...);

// Libérer
obj->~MyType();
memory::gDefaultAllocator->Free(ptr);

// OU (via NkUniquePtr, plus sûr)
auto ptr = nkentseu::memory::NkUniquePtr<MyType>(
    new(allocator) MyType(...), 
    allocator
);
// Auto-destruction quand ptr sort de scope
```

## Input & Événements

### Mapping
- **NkMouseMoveEvent** → `mInput.mousePos`
- **NkMouseButtonPressEvent** → détecte clic, lance `game->OnEvent()`
- **NkKeyPressEvent** (Escape) → `game->WantExit()` ou `cmd.quit`
- **NkWindowCloseEvent** → `mRunning = false`

### Pour les jeux
```cpp
void LaserPuzzleGame::OnEvent(NkEvent* event) noexcept {
    if (auto* me = event->As<NkMouseButtonPressEvent>()) {
        int32 tileIdx = GetTileAtPosition(mInput.mousePos);
        mSelectedTileIndex = tileIdx;
    } else if (auto* kr = event->As<NkKeyPressEvent>()) {
        if (kr->GetKey() == NkKey::NK_KEY_R) RotateTile(mSelectedTileIndex);
        if (kr->GetKey() == NkKey::NK_KEY_ESCAPE) mWantExit = true;
    }
}
```

## Rendu

### Pipeline NKCanvas
```cpp
target.Clear(bgColor);                              // Efface
NkRenderer2D& r = target.GetRenderer2D();          // Get renderer

// Dessiner formes, textes, etc.
r.DrawFilledRect(...);
r.DrawCircle(...);
r.DrawLine(...);

target.Display();                                   // Affiche
```

### Détails d'implémentation LaserPuzzleGame::Render
1. Fond : rect rempli couleur
2. Grille : boucle y/x, pour chaque tuile
   - Rect couleur selon type (source=jaune, cible=vert, mur=gris)
   - Si sélectionné : highlight bleu
   - Contenu : petit dessin (cercle, miroir, etc.)
3. Rayon laser : segments ColorésBitmaps
4. État victoire : panel texte

## Extension : ajouter un nouveau jeu

### Étapes
1. Créer dossier `Games/Specific/MonJeu/`
2. Créer `MonJeuGame.h/cpp` dérivant `NkoungGame`
3. Implémenter les 5 méthodes virtuelles (Init, Update, Render, OnEvent, Unload)
4. (Optionnel) Créer sous-classes (MonJeuLevel, MonJeuAI, etc.)
5. Dans `GameFactory::CreateGame()`, ajouter case `GameId::MonJeu`:
   ```cpp
   case GameId::MonJeu:
       game = new(allocator->Alloc(...)) MonJeuGame();
       break;
   ```
6. Dans `GameMetadata.h`, ajouter enum + GameInfo dans kGameInfoTable

### Template minimal
```cpp
// MonJeuGame.h
class MonJeuGame : public NkoungGame {
public:
    bool Init() noexcept override;
    void Update(float32 dt) noexcept override;
    void Render(NkRenderer2D& r) noexcept override;
    void OnEvent(NkEvent* e) noexcept override;
    void Unload() noexcept override;
    const char* GetTitle() const noexcept override { return "Mon Jeu"; }
private:
    bool mWantExit = false;
};

// MonJeuGame.cpp
bool MonJeuGame::Init() noexcept {
    NKOUNG_LOG_INFO("[MonJeu] Initialisation");
    return true;  // ou false si erreur
}

void MonJeuGame::Update(float32 dt) noexcept { /* logique */ }
void MonJeuGame::Render(NkRenderer2D& r) noexcept { /* dessiner */ }
void MonJeuGame::OnEvent(NkEvent* e) noexcept { 
    if (auto* kr = e->As<NkKeyPressEvent>()) {
        if (kr->GetKey() == NkKey::NK_KEY_ESCAPE) mWantExit = true;
    }
}
void MonJeuGame::Unload() noexcept { NKOUNG_LOG_INFO("[MonJeu] Unload"); }
```

## Prochaines étapes

### Court terme (MVP)
- ✅ Architecture platform + factory
- ✅ Prototype LaserPuzzle jouable (grille 6×6, miroirs, rayon)
- ⏳ Menu UI détaillé (liste jeux, boutons lancer, descriptions)
- ⏳ Niveaux LaserPuzzle (20 niveaux min, données JSON)
- ⏳ Annuler/Refaire, progression, sauvegarde

### Moyen terme
- ⏳ Refactoriser Labyrinth → nouveau pattern NkoungGame
- ⏳ Implémenter Territories (grille stratégie au tour par tour)
- ⏳ Implémenter Flow (puzzle connexions)
- ⏳ Audio (SFX, musique via NKAudio)
- ⏳ UI avancée (pause in-game, menus imbriqués)

### Long terme
- ⏳ NKECS : pour jeux plus lourds (Tactics, Territories avancé)
- ⏳ Networking : multi-joueur (compétitif/coopératif)
- ⏳ Export mobile (Android APK, HarmonyOS HAP, Web)
- ⏳ Éditeur Nogee : créer/éditer niveaux visuellement

## Compilation

### Build Nkoung
```bash
cd /path/to/Nkentseu
jenga build --target Nkoung --config Release
```

### Exécuter
```bash
./Build/Bin/Release-Windows/Nkoung/Nkoung.exe [--backend=dx11|opengl|vulkan|dx12|sw]
```

### Débuguer (GDB)
```bash
gdb --batch -ex "run" --args ./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe
```

## Fichiers clés

| Fichier | Role | Status |
|---------|------|--------|
| `main.cpp` | Entry point | ✅ Livré |
| `Core/NkoungConfig.h/cpp` | Config globale | ✅ Livré |
| `Platform/NkoungPlatformApp.h/cpp` | Orchestrateur | ✅ Livré |
| `Games/Common/NkoungGame.h` | Interface jeu | ✅ Livré |
| `Games/Common/GameMetadata.h` | Enums partagées | ✅ Livré |
| `Games/Common/GameFactory.h/cpp` | Factory pattern | ✅ Livré |
| `Games/Specific/LaserPuzzle/*.h/cpp` | Laser Puzzle | ✅ Prototype |
| Autres jeux | À implémenter | ⏳ Backlog |

## Notes développement

### Code style Nkoung
- Noms `CamelCase` (classes/méthodes)
- Membres privés `mCamelCase`
- Macros `NKOUNG_UPPER_SNAKE` ou `NK_*`
- ZÉRO STL (utiliser NKContainers, NKMemory)
- Logs via `NKOUNG_LOG_*` macros

### Pièges communs
1. **Oublier d'appeler `game->Unload()`** → fuites mémoire. (Solution : NkUniquePtr gère auto)
2. **Mix allocateurs** → heap corruption. (Solution : toujours utiliser `memory::gDefaultAllocator`)
3. **Event non-handleés** → input manqué. (Solution : vérifier `OnEvent()` implémenté)
4. **Resize fenêtre** → crash DX12. (Solution : détecter changement + appeler `target.OnResize()`)

---

**Mainteneur** : Rihen  
**Version** : 0.2.0  
**Date** : 2026-06-09
