# Nkoung QUICK START — Guide de démarrage rapide

## 🚀 Lancer le projet en 3 minutes

### Étape 1 : Build

```bash
cd D:\Projets\2026\Nkentseu\Nkentseu

# Compiler pour la première fois (5-10 min)
jenga build --target Nkoung --config Debug --platform windows
```

**Résultat attendu:**
- ✅ Pas d'erreur compilation
- ✅ Exe créé: `Build/Bin/Debug-Windows/Nkoung/Nkoung.exe`

**Si erreur:** Voir [BUILD_CHECKLIST.md → Étape 3.2](BUILD_CHECKLIST.md#32-résolution-derreurs)

### Étape 2 : Lancer

```bash
# Défaut (DX11 sur Windows)
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe

# Ou forcer backend
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe --backend=opengl
```

**Résultat attendu:**
- ✅ Fenêtre 1280×720 s'ouvre
- ✅ Menu affiche 6 jeux
- ✅ Texte blanc sur fond sombre

### Étape 3 : Tester Laser Puzzle

```
Menu de plateforme affiche:
┌─────────────────────────────────────────────┐
│ 1. Laser Puzzle          [Prototype ▶]     │
│ 2. Territoires           [À concevoir]      │
│ ... (4 autres)                              │
└─────────────────────────────────────────────┘

Clic sur "Laser Puzzle" ou appui Entrée:
  → Grille 6×6 s'affiche
  → Cellules bleu sombre avec lignes grises
  → Cercle vert (source) en haut gauche
  → Cercle orange (cible) en bas droite
  → Miroirs (lignes blanches)

Interactions:
  - Clic miroir → surligne en bleu clair
  - Touche R → miroir tourne
  - Touche ESC → retour menu

Quitter menu:
  - Touche ESC ou Alt+F4
```

---

## 🐛 Troubleshooting rapide

| Symptôme | Cause probable | Fix |
|----------|----------------|-----|
| Build fails: "file not found" | .cpp pas trouvé | Vérifier path du fichier |
| Build fails: "undefined reference" | .cpp pas compilé | Vérifier `files()` dans Jenga |
| Exe ne lance pas | DLL manquante | Vérifier `Path` env, lancer via VS |
| Menu noir / texte invisible | Contexte GPU pas créé | Vérifier logs, tester `--backend=software` |
| Clic menu ne fait rien | Event dispatch bug | Vérifier [NkoungPlatformApp.cpp](src/Nkoung/Platform/NkoungPlatformApp.cpp) |

---

## 📊 Architecture schématique

```
main.cpp
   ↓
NkoungPlatformApp::Initialize(NkEntryState)
   ├─ NkoungConfig::InitializeAllocators()
   ├─ ParseArguments() → backend selection
   ├─ NkWindow::Create()
   ├─ NkRenderWindow::Initialize()
   └─ NKUI Setup
   ↓
NkoungPlatformApp::Run() — main loop
   ├─ Event polling
   ├─ Dispatch to scene (PlatformMenu or GameScene)
   ├─ Update(dt)
   ├─ Render()
   └─ Repeat until exit

Menu scene:
  Clic jeu → GameFactory::CreateGame() → LaunchGame()
  Scène devient GameScene
  
Game scene:
  game->Update(dt)
  game->Render(renderer)
  game->OnEvent(event)
  ESC → ReturnToPlatformMenu()
```

---

## 📁 Structure fichiers

```
Applications/Nkoung/
├── Nkoung.jenga                    # Build config (auto includes src/**.cpp)
├── ARCHITECTURE_NKOUNG.md          # Docs archi complètes
├── ROADMAP.md                      # Phases futurs & planning
├── BUILD_CHECKLIST.md              # Checklist détaillée compilation
├── QUICK_START.md                  # Ce fichier
│
└── src/Nkoung/
    ├── main.cpp                    # Entry point (~35 lignes)
    │
    ├── Core/
    │   ├── NkoungConfig.h/cpp      # Globals (allocateurs, logger, consts)
    │
    ├── Platform/
    │   ├── NkoungPlatformApp.h/cpp # Orchestrateur (scenes, events, rendering)
    │
    ├── Games/Common/
    │   ├── NkoungGame.h            # Base class virtuelle
    │   ├── GameMetadata.h          # Enums & infos jeux
    │   ├── GameFactory.h/cpp       # Factory création jeux
    │
    ├── Games/Specific/LaserPuzzle/
    │   ├── LaserPuzzleGame.h/cpp   # Impl Laser Puzzle
    │
    └── (à venir: Territories/, Labyrinth/, etc.)
```

---

## 🎮 Commandes utiles

```bash
# Build optimisé (Release)
jenga build --target Nkoung --config Release

# Build multithread
jenga build --target Nkoung -j8

# Clean + rebuild
rm -r Build/ && jenga build --target Nkoung

# Compiler autres backends
jenga build --target Nkoung --platform linux
jenga build --target Nkoung --platform macos
jenga build --target Nkoung --platform android
jenga build --target Nkoung --platform web

# Debug avec GDB (Linux)
gdb --args ./Nkoung.exe --backend=opengl
(gdb) run
```

---

## 🔍 Logs & debugging

**Voir output logs:**
```bash
# Logs toujours affichés
./Nkoung.exe 2>&1 | tee nkoung.log

# Logs verbose (si compilé avec DEBUG)
./Nkoung.exe --log-level=trace
```

**Logs attendus au démarrage:**
```
[INFO] Initialisation Nkoung Platform
[INFO] Fenêtre créée: 1280x720
[INFO] Contexte graphique initialisé: DX11
[INFO] UI contexte prêt
```

**Logs LaserPuzzle:**
```
[INFO] Initialisation Laser Puzzle
[INFO] Niveau chargé: 6x6
```

---

## 📈 Prochaines étapes (après test OK ✅)

1. **Menu polish** — afficher descriptions jeux, status
2. **LaserPuzzle complétude** — multi-réflexions, niveaux JSON
3. **Ajouter skeletons 5 autres jeux** — fichiers vides compilent
4. **Implémentations jeux** — commencer Territories & Co.

Voir [ROADMAP.md](ROADMAP.md) pour détails.

---

## 📞 Support rapide

**Responsable:** Rihen (@nkentseu)  
**Emails:** Logs compilent ? → `jenga build --target Nkoung -j8`  
**Repo local:** D:\Projets\2026\Nkentseu\Nkentseu  
**Docs:** ARCHITECTURE_NKOUNG.md (archi complète)  

---

## ✨ C'est prêt ! 

La nouvelle architecture Nkoung est **complète et prête au build**.  
Tous les fichiers sont en place, Jenga config OK, documentation à jour.

**Prochaine action:** Lancer `jenga build --target Nkoung` et rapporter les erreurs !

💡 **Tip:** Garder ce QUICK_START.md ouvert pendant le build/test — il a les troubleshoots les plus courants.
