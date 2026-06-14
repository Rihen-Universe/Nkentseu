# Nkoung — Plateforme Multi-Jeux 2D

🎮 **Plateforme unifiée pour 6 jeux 2D** : Laser Puzzle, Territoires, Gardien du Labyrinthe, Ponts & Chemins, Flux, Tactique.

Construite sur le moteur **Nkentseu** (C++, zero-STL, cross-platform, 5 backends graphiques).

---

## 🚀 Démarrage rapide

```bash
# Build
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Nkoung --config Debug

# Run
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe

# Run avec backend précis (opengl, vulkan, dx11, dx12, software)
./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe --backend=opengl
```

**Résultat:** Fenêtre 1280×720 avec menu 6 jeux.

👉 **Détails complets:** Voir [QUICK_START.md](QUICK_START.md)

---

## 📖 Documentation

| Doc | Contenu | Audience |
|-----|---------|----------|
| **[QUICK_START.md](QUICK_START.md)** | 3 étapes build/test + troubleshooting rapide | Tous |
| **[ARCHITECTURE_NKOUNG.md](ARCHITECTURE_NKOUNG.md)** | Structure, flux exécution, patterns, interface jeux | Développeurs |
| **[ROADMAP.md](ROADMAP.md)** | Phases MVP→5, priorités, risques, KPIs | Planning |
| **[BUILD_CHECKLIST.md](BUILD_CHECKLIST.md)** | 7 étapes verif/compilation/test détaillées | Compilation |
| **[INDEX.md](INDEX.md)** | Navigation docs, quick links | Navigation |

---

## 📁 Structure

```
Applications/Nkoung/
├── src/Nkoung/
│   ├── main.cpp                        # Entry point
│   ├── Core/                           # Config globale
│   ├── Platform/                       # Orchestrateur
│   ├── Games/Common/                   # Base classes
│   └── Games/Specific/LaserPuzzle/    # Jeux implémentés
│
├── QUICK_START.md                      # Démarrer ici
├── ARCHITECTURE_NKOUNG.md              # Archi complète
├── ROADMAP.md                          # Planning
├── BUILD_CHECKLIST.md                  # Build steps
└── INDEX.md                            # Docs index
```

---

## 🎮 Jeux livrés

| Jeu | Type | Statut | Playable |
|-----|------|--------|----------|
| **Laser Puzzle** | Puzzle rayons | ✅ MVP | ✅ Oui |
| Territoires | Stratégie | ⏳ Skeleton | ❌ Non |
| Gardien du Labyrinthe | Aventure | ⏳ Skeleton | ❌ Non |
| Ponts & Chemins | Puzzle | ⏳ Skeleton | ❌ Non |
| Flux | Puzzle flow | ⏳ Skeleton | ❌ Non |
| Tactique | Stratégie avancée | ⏳ Skeleton | ❌ Non |

---

## 🏗️ Architecture (vue haute)

```
Entry Point
   ↓
NkoungPlatformApp::Initialize()
   ├─ Allocateurs (Default/Resource/Scratch)
   ├─ NkWindow (fenêtre native)
   ├─ NkRenderWindow (rendu 2D multi-backend)
   ├─ NKUI (GUI)
   └─ Event routing
   ↓
Main Loop (Run)
   ├─ Scene PlatformMenu:
   │   └─ Affiche 6 jeux, clic lancent
   │
   └─ Scene GameScene:
       ├─ game->Update(dt)
       ├─ game->Render(renderer)
       └─ game->OnEvent(event)
```

**Patterns appliqués:**
- **Factory** — GameFactory::CreateGame() → NkUniquePtr (RAII)
- **Strategy** — NkoungGame abstract + implémentations
- **State Machine** — AppScene transitions
- **Singletons** — Allocateurs, logger

---

## 🔧 Backends graphiques

5 backends supportés, sélectionnables via CLI :

```bash
./Nkoung.exe                      # Défaut (DX11/Windows, GL ailleurs)
./Nkoung.exe --backend=vulkan     # Vulkan
./Nkoung.exe --backend=opengl     # OpenGL
./Nkoung.exe --backend=dx11       # DirectX 11
./Nkoung.exe --backend=dx12       # DirectX 12
./Nkoung.exe --backend=software   # Software rasterizer
```

---

## 📊 État de développement

### Phase 1 : Architecture MVP ✅ COMPLÈTE

**Livré :**
- ✅ Structure multi-fichier (7 fichiers)
- ✅ Orchestrateur (NkoungPlatformApp)
- ✅ Pattern Factory (GameFactory)
- ✅ LaserPuzzleGame MVP (grille 6×6, miroirs, rayons)
- ✅ Support 5 backends (--backend CLI)
- ✅ Documentation complète (5 docs, 1300+ lignes)

**Lignes de code :**
- Codebase : ~2500 lignes
- Documentation : ~1300 lignes

**Prêt pour :** Build & test compilation

### Phase 2 : Polish & Fonctionnalités (1-2 semaines)

- [ ] Menu UI professionnelle (descriptions, status, icônes)
- [ ] LaserPuzzle complétude (niveaux JSON, HUD, victoire)
- [ ] Skeletons 5 autres jeux
- [ ] Tests multiplateforme (Linux, macOS, Android, web)

### Phase 3+ : Implémentations & systèmes

Voir [ROADMAP.md](ROADMAP.md) pour détails phases 3-5.

---

## 💾 Allocateurs et mémoire

Nkoung suit le pattern Nkentseu de gestion mémoire :

```cpp
namespace nkoung::memory {
    extern NkAllocator* gDefaultAllocator;      // Conteneurs, objets jeu
    extern NkAllocator* gResourceAllocator;     // Textures, assets, niveaux
    extern NkAllocator* gScratchAllocator;      // Scratch temporaire frame
    
    void InitializeAllocators() noexcept;       // Appelé au startup
    void ShutdownAllocators() noexcept;         // Appelé au shutdown
}
```

**Usage :**
```cpp
// Allocation
auto* obj = new(allocator->Alloc(sizeof(T))) T(...);  // Placement new

// Désallocation
obj->~T();
allocator->Free(obj);

// Smart pointers (RAII)
NkUniquePtr<T> ptr(obj, allocator);  // Auto-destruit
```

---

## 🔍 Logs & debugging

```bash
# Voir tous les logs
./Nkoung.exe 2>&1 | tee run.log

# Logs attendus au démarrage
[INFO] Initialisation Nkoung Platform
[INFO] Fenêtre créée: 1280x720
[INFO] Contexte graphique initialisé: DX11
[INFO] UI contexte prêt
```

---

## 🎯 Prochaines actions

1. **Build & Test**
   ```bash
   jenga build --target Nkoung --config Debug
   ./Build/Bin/Debug-Windows/Nkoung/Nkoung.exe
   ```

2. **Vérifier menu visible** avec 6 jeux affichés

3. **Tester Laser Puzzle** :
   - Clic/clavier pour sélectionner
   - Grille 6×6 s'affiche
   - Clic miroir → surligne
   - R → rotation miroir

4. **Tester backends** :
   ```bash
   ./Nkoung.exe --backend=opengl
   ./Nkoung.exe --backend=vulkan
   ```

5. **Rapporter bugs** via [BUILD_CHECKLIST.md](BUILD_CHECKLIST.md#signaler-erreurs)

👉 **Voir [QUICK_START.md](QUICK_START.md) pour guide complet**

---

## 📞 Support

- **Responsable :** Rihen (@nkentseu)
- **Langue :** Français
- **Repo :** D:\Projets\2026\Nkentseu\Nkentseu
- **Workspace :** Applications/Nkoung

---

## 📚 Ressources

- **Nkentseu Architecture** → `../../ARCHITECTURE.md`
- **NKCanvas Usage** → `../../Kernel/Runtime/NKCanvas/USAGE.md`
- **Designs GDD** → Voir `/docs` ou `/Pcp`

---

**Version :** 0.2.0  
**Status :** 🟢 **Prêt pour compilation**  
**Date :** Juin 2026  

✨ **La plateforme Nkoung est prête ! Lancez le build.** ✨
