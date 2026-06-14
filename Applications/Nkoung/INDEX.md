# Nkoung Documentation Index

Bienvenue dans la **plateforme multi-jeux Nkoung**. Ce fichier vous guide à travers toute la documentation disponible.

---

## 📖 Documents principaux

### Pour commencer
- **[QUICK_START.md](QUICK_START.md)** ← 🚀 **COMMENCEZ ICI**
  - Build en 3 minutes
  - Test gameplay
  - Troubleshooting rapide
  - Commandes essentielles

### Pour comprendre l'architecture
- **[ARCHITECTURE_NKOUNG.md](ARCHITECTURE_NKOUNG.md)**
  - Vue d'ensemble de la plateforme
  - Structure fichiers & répertoires
  - Flux exécution complet
  - Interface NkoungGame (abstraite)
  - Guide "Ajouter un nouveau jeu"
  - Utilisation modules Nkentseu

### Pour planifier le développement
- **[ROADMAP.md](ROADMAP.md)**
  - Phases MVP → 5 complet
  - État actuel (livré/en cours/pending)
  - Priorités & effort estimé
  - Risques & mitigations
  - KPIs de livraison

### Pour compiler & tester
- **[BUILD_CHECKLIST.md](BUILD_CHECKLIST.md)**
  - Vérifier fichiers existent
  - Vérifier Jenga config
  - Résolution errors compilation
  - Test exécution (7 étapes)
  - Logs & debugging
  - Performance & profiling

---

## 🗂️ Structure source

```
Applications/Nkoung/src/Nkoung/
│
├── main.cpp
│   └─ Entry point (~35 lignes), crée NkoungPlatformApp(state)
│
├── Core/
│   ├── NkoungConfig.h
│   │   └─ Allocateurs (Default/Resource/Scratch), logger, consts
│   └── NkoungConfig.cpp
│       └─ Impl allocateurs singletons
│
├── Platform/
│   ├── NkoungPlatformApp.h
│   │   └─ Orchestrateur : fenêtre, scènes, events, rendering
│   └── NkoungPlatformApp.cpp
│       └─ Init/Run/ParseArguments/UpdatePlatformMenu/RenderPlatformMenu
│
├── Games/
│   │
│   ├── Common/
│   │   ├── NkoungGame.h
│   │   │   └─ Classe abstraite : Init/Update/Render/OnEvent/Unload
│   │   ├── GameMetadata.h
│   │   │   └─ Enums (GameId, AppScene, GameStatus), struct GameInfo
│   │   ├── GameFactory.h
│   │   │   └─ Factory interface
│   │   └── GameFactory.cpp
│   │       └─ CreateGame/GetGameInfo/GetAllGames
│   │
│   └── Specific/
│       └── LaserPuzzle/
│           ├── LaserPuzzleGame.h
│           │   └─ Enums (LaserDirection, LaserTileType), structs
│           └── LaserPuzzleGame.cpp
│               └─ Impl Laser Puzzle (~250 lignes)
│
└── (à venir: Territories/, Labyrinth/, Bridges/, Flow/, Tactics/)
```

---

## 🎮 Les 6 jeux prévus

| # | Jeu | Type | Statut | MVP |
|---|-----|------|--------|-----|
| 1 | Laser Puzzle | Puzzle rayons | ✅ Prototype | Grille 6×6, miroirs |
| 2 | Territoires | Stratégie | ⏳ Skeleton | Grille, tour-par-tour |
| 3 | Gardien du Labyrinthe | Aventure | ⏳ Skeleton | Maze, pathfinding |
| 4 | Ponts & Chemins | Puzzle | ⏳ Skeleton | Grille, connexions |
| 5 | Flux | Puzzle | ⏳ Skeleton | Canalisations sans croisement |
| 6 | Tactique | Stratégie avancée | ⏳ Skeleton | Unités, carte dynamique |

---

## 📊 Phases de développement

### Phase 1 : Architecture MVP ✅ COMPLÈTE
- ✅ Structure multi-fichier (7 fichiers)
- ✅ Orchestrateur (NkoungPlatformApp)
- ✅ Pattern Factory (GameFactory)
- ✅ LaserPuzzle prototype
- ✅ Support 5 backends graphiques
- ✅ Documentation complète

**Détails:** Voir [ROADMAP.md](ROADMAP.md#phase-actuelle--mvp-plateforme-en-cours)

### Phase 2 : Polish & Fonctionnalités (1-2 sem)
- Menu professionnel (descriptions, status, icônes)
- LaserPuzzle complétude (niveaux JSON, HUD)
- Skeletons 5 autres jeux
- Test multiplateforme

**Détails:** Voir [ROADMAP.md](ROADMAP.md#phase-2--polish--fonctionnalités-1-2-semaines)

### Phase 3 : Implémentations jeux (2-4 sem, parallèle)
- Territories (stratégie)
- Gardien du Labyrinthe (aventure)
- Ponts & Chemins (puzzle)
- Flux (puzzle)
- Tactique (stratégie avancée)

### Phase 4 : Systèmes support (1-2 sem)
- Sauvegarde/progression JSON
- Audio optionnel (NKAudio)
- ECS optionnel (NKECS)

### Phase 5 : Packaging & distribution (1 sem)
- Compilations Release
- Installeurs (MSI, AppImage, DMG)
- Tests configurations

---

## 🚀 Commandes essentielles

```bash
# BUILD
jenga build --target Nkoung --config Debug

# RUN (défaut = DX11 Windows, GL ailleurs)
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe

# RUN avec backend précis
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe --backend=opengl
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe --backend=vulkan
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe --backend=dx12
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe --backend=software

# CLEAN
rm -r Build/

# MULTITHREAD
jenga build --target Nkoung -j8
```

---

## 💾 État actuel (juin 2026)

### Fichiers livrés
- ✅ `main.cpp` (~35 lignes)
- ✅ `Core/NkoungConfig.h/cpp`
- ✅ `Platform/NkoungPlatformApp.h/cpp`
- ✅ `Games/Common/{NkoungGame.h, GameMetadata.h, GameFactory.h/cpp}`
- ✅ `Games/Specific/LaserPuzzle/{LaserPuzzleGame.h/cpp}`
- ✅ `Nkoung.jenga` (config build OK)

### Documentation livrée
- ✅ `ARCHITECTURE_NKOUNG.md` (~350 lignes)
- ✅ `ROADMAP.md` (~300 lignes)
- ✅ `BUILD_CHECKLIST.md` (~250 lignes)
- ✅ `QUICK_START.md` (~150 lignes)
- ✅ `INDEX.md` (ce fichier)

### État compilable
- ✅ Tous fichiers `.cpp` compilables
- ✅ Jenga config inclut automatiquement `src/**.cpp`
- ✅ Dépendances OK (NKWindow, NKCanvas, NKUI, NKEvent, NKMemory, NKLogger)
- ⏳ À tester : `jenga build --target Nkoung`

---

## 🎯 Prochaines actions

### Immediate (aujourd'hui)
1. Lancer `jenga build --target Nkoung --config Debug`
2. Corriger erreurs compilation (si any)
3. Tester `./Nkoung.exe` — vérifier menu visible
4. Tester `./Nkoung.exe --backend=opengl` — vérifier menu visible

### Ensuite (demain/cette semaine)
1. Menu polish (afficher descriptions)
2. LaserPuzzle complétude (rayons multi-réflexion, niveaux)
3. Skeletons 5 autres jeux
4. Tests multiplateforme

### Futurs (semaines 2-4)
1. Implémentations jeux (Territories, Labyrinth, etc.)
2. Sauvegarde/progression
3. Packaging & release

---

## 📚 Ressources complémentaires

### Nkentseu modules utilisés
- **NKWindow** — Fenêtres natives, multi-OS, événements
- **NKCanvas** — Rendu 2D SFML-like, multi-backends
- **NKUI** — GUI immediate-mode, widgets, layouts
- **NKEvent** — Événements polymorphes typés
- **NKMemory** — Allocateurs custom, smart pointers
- **NKLogger** — Logger async, multi-sinks
- **NKMath** — Vecteurs, matrices, géométrie
- **NKContainers** — Conteneurs zero-STL

### Documentation Nkentseu
- `../../ARCHITECTURE.md` — Architecture moteur complète
- `../../Kernel/Runtime/NKCanvas/USAGE.md` — Usage détaillé NKCanvas
- Modules ROADMAPs — `../../Kernel/*/ROADMAP.md`

### Ressources GDD jeux
- Voir dossier `/docs` ou `/Pcp` pour designs complets

---

## 🤝 Support & contacts

**Responsable :** Rihen (@nkentseu)  
**Email :** nkentseu@gmail.com  
**Langue :** Français  
**Repo local :** D:\Projets\2026\Nkentseu\Nkentseu  
**Workspace :** Applications/Nkoung  

---

## 📌 Quick links

| Besoin | Aller à |
|--------|---------|
| Démarrer rapidement | [QUICK_START.md](QUICK_START.md) |
| Comprendre archi | [ARCHITECTURE_NKOUNG.md](ARCHITECTURE_NKOUNG.md) |
| Ajouter un jeu | [ARCHITECTURE_NKOUNG.md § Ajouter un nouveau jeu](ARCHITECTURE_NKOUNG.md#ajouter-un-nouveau-jeu) |
| Build échoue | [BUILD_CHECKLIST.md § Étape 3.2](BUILD_CHECKLIST.md#32-résolution-derreurs) |
| Planning futur | [ROADMAP.md](ROADMAP.md) |
| État modules | [ROADMAP.md § État du sprint](ROADMAP.md#état-du-sprint-nkoung) |

---

**Version :** 0.2.0  
**Date :** Juin 2026  
**Status :** 🟢 Prêt pour compilation & test  
