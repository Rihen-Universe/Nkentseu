# Pong Ultra Arena

Jeu **vitrine** du moteur **Nkentseu** : un Pong 1v1 moderne (splash animé, menus,
IA, power-ups, obstacles, particules, multijoueur LAN, audio), pensé pour démontrer
la couche graphique 2D **NKCanvas** + l'UI immediate-mode **NKUI** sur toutes les
plateformes cibles.

- **Roadmap / état d'avancement** : [ROADMAP.md](ROADMAP.md)
- **Game Design Document** : [docs/GDD_PONG_ULTRA_ARENA_v1.1.docx](docs/GDD_PONG_ULTRA_ARENA_v1.1.docx)
- **Maquettes** : [docs/01_splash_et_menus.html](docs/01_splash_et_menus.html) ·
  [docs/02_terrain_de_jeu_1v1.html](docs/02_terrain_de_jeu_1v1.html) ·
  [docs/03_assets_library.html](docs/03_assets_library.html)

---

## ⚠️ État actuel (2026-05-31) — à lire avant de lancer

Le projet est **en cours de migration** vers NKCanvas. Concrètement :

- **`Pong/` (ce dossier) = chantier de migration.** Son `src/Pong/Apps.cpp` est une
  **petite démo de test** (paddles, balle, scores) qui sert à **valider le rendu
  cross-backend** de NKCanvas — ce n'est PAS le jeu complet. Seul `Apps.cpp` est
  compilé ; le reste (`Game/`, `Net/`, `Audio/`, `UI/Scenes/`, `Render/`) est sur
  disque mais **hors-build**.
- **Le jeu complet et fonctionnel est dans [`../Pong2/`](../Pong2/)** (baseline qui
  tournait sur **Windows + Android** via l'ancien stack OpenGL maison). C'est la
  **référence** tant que la migration n'est pas finie.

**Au runtime, seul le backend OpenGL fonctionne** pour l'instant (Vulkan/DX11/DX12/
Software sont buggés — voir [ROADMAP.md](ROADMAP.md#bugs--quirks-connus)).

---

## Où Pong se situe dans le moteur

Nkentseu a **deux chemins de rendu** distincts :

| Chemin | Couche | Pour quoi |
|---|---|---|
| **2D** | **NKCanvas** (SFML-like) + **NKUI** (ImGui maison) | **Pong**, jeux 2D, démos |
| **3D** | **NKRenderer** → **NKRHI** | **Noge** (moteur 2D/3D) et **Nogee** (son éditeur) |

- **Pong est sur le chemin 2D** : rendu via NKCanvas, UI via NKUI.
- **Noge** = le moteur de jeu. **Nogee** = l'éditeur de Noge (Nogee *utilise* Noge).
  Tous deux rendent via **NKRenderer**, pas NKCanvas. NKUI y est aussi utilisé,
  mais **à travers NKRenderer**.

---

## Compiler & lancer

Depuis la racine du dépôt Nkentseu :

```bash
# Windows (par défaut), Debug
jenga build --target Pong

# Plateforme / config explicites
jenga build --target Pong --platform android --config Release
jenga build --target Pong --platform windows --config Debug
```

Binaire produit : `Build/Bin/<Config>-<System>/Pong/Pong.exe` (ou `.apk`, etc.).

### Choisir le backend graphique (`pong.config`)

Au premier lancement, un fichier **`pong.config`** est créé à côté de l'exécutable.
Éditable à la main, relu à chaque démarrage :

```
backend=opengl    # opengl | vulkan | dx11 | dx12 | software | auto
```

> 💡 **Mets `backend=opengl`** : c'est le seul backend qui marche au runtime
> actuellement. `auto` laisse le moteur choisir le meilleur backend supporté (avec
> chaîne de fallback) — mais il tombera sur les backends encore buggés.

---

## Commandes (démo actuelle)

| Touche | Action |
|---|---|
| `W` / `S` | Paddle gauche haut / bas |
| `↑` / `↓` | Paddle droit haut / bas |
| `Échap` | Quitter |

Sans input, les paddles passent en **auto-démo** (suivent la balle).

---

## Plateformes cibles

Windows · Linux (XLib & Wayland) · macOS · **Android** (minSdk 24, ABIs armeabi-v7a
/ arm64-v8a / x86 / x86_64) · HarmonyOS · iOS · Web (Emscripten WebGL2).

Multijoueur **LAN** (découverte UDP broadcast, port **7778**) — permissions réseau
déjà configurées côté Android/HarmonyOS dans [Pong.jenga](Pong.jenga).

---

## Arborescence

```
Pong/
├── ROADMAP.md, README.md
├── Pong.jenga              # build (compile seulement src/Pong/Apps.cpp pour l'instant)
├── docs/                   # GDD v1.1 + maquettes HTML
└── src/Pong/
    ├── Apps.cpp            # ENTRÉE ACTUELLE = démo de test backend (≠ jeu complet)
    ├── PongConfig.h        # sélection dynamique du backend (pong.config)
    ├── Game/               # [hors-build] PongApp, AIController, Obstacle/Particle/PowerUp
    ├── Render/             # [hors-build] ancien stack OpenGL maison (GLContext/GLRenderer2D/…)
    ├── Audio/              # [hors-build] AudioManager (NKAudio)
    ├── Net/                # [hors-build] NetworkDiscovery/Session, AfricaPlaces (LAN UDP)
    └── UI/                 # [hors-build] SceneManager + 12 scènes + Theme/UIScale
```

> Le jeu complet correspondant (compilé et jouable) vit dans [`../Pong2/`](../Pong2/).

---

## Licence

GPL-3.0 — voir [../../LICENSE](../../LICENSE). Éditeur : **Rihen Universe**.
