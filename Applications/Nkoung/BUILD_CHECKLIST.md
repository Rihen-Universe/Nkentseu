# Nkoung BUILD & COMPILATION CHECKLIST

## Étape 1 : Vérifier les fichiers

```bash
# À partir de: d:\Projets\2026\Nkentseu\Nkentseu\

# Vérifier tous les .cpp/.h existent
Applications/Nkoung/src/Nkoung/
├── main.cpp                                  ? Existe
├── Core/NkoungConfig.h/cpp                   ? Existe
├── Platform/NkoungPlatformApp.h/cpp          ? Existe
├── Games/Common/
│   ├── NkoungGame.h                          ? Existe
│   ├── GameMetadata.h                        ? Existe
│   └── GameFactory.h/cpp                     ? Existe
└── Games/Specific/LaserPuzzle/
    └── LaserPuzzleGame.h/cpp                 ? Existe
```

**Script rapide (PowerShell):**
```powershell
$files = @(
    "Applications/Nkoung/src/Nkoung/main.cpp",
    "Applications/Nkoung/src/Nkoung/Core/NkoungConfig.h",
    "Applications/Nkoung/src/Nkoung/Core/NkoungConfig.cpp",
    "Applications/Nkoung/src/Nkoung/Platform/NkoungPlatformApp.h",
    "Applications/Nkoung/src/Nkoung/Platform/NkoungPlatformApp.cpp",
    "Applications/Nkoung/src/Nkoung/Games/Common/NkoungGame.h",
    "Applications/Nkoung/src/Nkoung/Games/Common/GameMetadata.h",
    "Applications/Nkoung/src/Nkoung/Games/Common/GameFactory.h",
    "Applications/Nkoung/src/Nkoung/Games/Common/GameFactory.cpp",
    "Applications/Nkoung/src/Nkoung/Games/Specific/LaserPuzzle/LaserPuzzleGame.h",
    "Applications/Nkoung/src/Nkoung/Games/Specific/LaserPuzzle/LaserPuzzleGame.cpp"
)

foreach ($f in $files) {
    if (Test-Path $f) { Write-Host "✅ $f" } 
    else { Write-Host "❌ MANQUANT: $f" }
}
```

---

## Étape 2 : Vérifier Jenga file

**Fichier:** `Applications/Nkoung/Nkoung.jenga`

**Points à vérifier:**

1. **Tous les .cpp inclus dans `files()`**
   ```python
   files([
       "src/Nkoung/main.cpp",
       "src/Nkoung/Core/NkoungConfig.cpp",
       "src/Nkoung/Platform/NkoungPlatformApp.cpp",
       "src/Nkoung/Games/Common/GameFactory.cpp",
       "src/Nkoung/Games/Specific/LaserPuzzle/LaserPuzzleGame.cpp",
   ])
   ```

2. **Dépendances correctes**
   ```python
   dependson(["NKWindow", "NKEvent", "NKCanvas", "NKUI", "NKMemory", "NKLogger", "NKMath", "NKContainers"])
   ```

3. **Platform headers (Windows)**
   ```python
   if system() == "windows":
       libs(["user32", "kernel32", ...])  # Au besoin
   ```

---

## Étape 3 : Build

### 3.1 Clean build

```bash
cd D:\Projets\2026\Nkentseu\Nkentseu

# Nettoyer
rm -r Build/

# Build clean
jenga build --target Nkoung --config Debug --platform windows
```

### 3.2 Résolution d'erreurs

#### Erreur : "file not found: Core/NkoungConfig.cpp"

**Cause:** Jenga ne voit pas le fichier  
**Fix:** 
- Vérifier path relatif dans `files([...])`
- Vérifier fichier existe vraiment
- Vérifier no typos dans filename

#### Erreur : "undefined reference to 'NkoungConfig::GetLogger()'"

**Cause:** .cpp pas compilé ou linker erreur  
**Fix:**
- Vérifier .cpp dans `files([])`
- Vérifier pas d'erreur compilation sur le .cpp
- Vérifier pas de C++17 vs C++20 mismatch

#### Erreur : "include 'NKWindow/...' not found"

**Cause:** Dépendance manquante ou path wrong  
**Fix:**
- Ajouter `dependson(["NKWindow"])` dans jenga
- Vérifier `#include` path correct (relatif à Kernel/)
- Ex: `#include "NKWindow/NkWindow.h"`

#### Erreur : "cannot create render window: unknown API"

**Cause:** NkGraphicsApi enum pas reconnu  
**Fix:**
- Vérifier NKRHI headers incluent NkGraphicsApi enum
- Chercher : `enum class NkGraphicsApi { NK_GFX_API_VULKAN, ... }`
- Vérifier path include correct

---

## Étape 4 : Test exécution

### 4.1 Lancer défaut (avec backend par défaut)

```bash
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe

# Résultat attendu:
# - Fenêtre 1280x720 "Nkoung v0.2.0"
# - Menu avec 6 jeux
# - Texte blanc sur fond sombre
```

### 4.2 Tester backend command-line

```bash
# OpenGL
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe --backend=opengl
# Résultat: même menu

# Vulkan
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe --backend=vulkan
# Résultat: même menu

# DirectX 12
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe --backend=dx12
# Résultat: même menu

# Software
.\Build\Bin\Debug-Windows\Nkoung\Nkoung.exe --backend=software
# Résultat: fenêtre noire (software renderer lent)
```

### 4.3 Test gameplay

```bash
./Nkoung.exe

# Menu apparaît
# Clic sur "Laser Puzzle"
# Résultat attendu:
#   - Grille 6x6 s'affiche
#   - Couleurs: cellules bleu sombre, grille ligne grise
#   - Source: cercle vert (haut gauche)
#   - Cible: cercle orange (bas droite)
#   - Miroirs: lignes blanches

# Interactions:
#   - Clic sur miroir → sélection highlight bleu clair
#   - Touche R → miroir tourne
#   - Touche ESC → retour menu

# Bug check:
#   - Pas de crash
#   - Pas de lag/freeze
#   - FPS stable (~60 Hz)
```

---

## Étape 5 : Logs & debugging

### 5.1 Activer logs détaillés

Vérifier que [NkoungConfig.cpp](src/Nkoung/Core/NkoungConfig.cpp) initialise logger :

```cpp
static NkLogger& gLogger = NkLogger::GetDefaultInstance();

NkLogger& nkoung::GetLogger() noexcept {
    return gLogger;
}
```

### 5.2 Vérifier logs au démarrage

Console output attendu:
```
[INFO] [Nkoung] Initialisation Nkoung Platform
[INFO] [Nkoung] Fenêtre créée: 1280x720
[INFO] [Nkoung] Contexte graphique initializado: DX11
[INFO] [Nkoung] UI contexte prêt
```

### 5.3 Logs gameplay LaserPuzzle

```
[INFO] Initialisation Laser Puzzle
[INFO] Niveau chargé: 6x6
[INFO] Laser Puzzle déchargé
```

---

## Étape 6 : Performance & profiling

### 6.1 FPS counter

Doit afficher dans titre fenêtre : `Nkoung v0.2.0 | FPS: 60`

Code (si implémenté dans NkoungPlatformApp::Run):
```cpp
static float32 fpsCounter = 0.f;
fpsCounter += dt;
if (fpsCounter >= 0.5f) {
    uint32 fps = static_cast<uint32>(1.f / mClock.GetDeltaTime());
    mWindow.SetTitle(NkFormat("Nkoung v0.2.0 | FPS: %u", fps));
    fpsCounter = 0.f;
}
```

### 6.2 Memory usage

Vérifier pas de memory leak :
- Ouvrir Task Manager
- Observer RAM du process au lancement
- Au clic menu → jeu : RAM stable (pas de croissance non-linéaire)
- Retour menu : RAM revient (Unload appelé)

### 6.3 CPU usage

Pour debug lent :
```powershell
# Ouvrir Process Explorer (Sysinternals)
# Regarder Stack Trace pendant freeze
# Chercher boucle infinies, allocations massives
```

---

## Étape 7 : Signaler erreurs

**Format bug report:**
```
**Platform:** Windows / Linux / macOS
**Backend:** OpenGL / Vulkan / DX11 / DX12 / Software
**Jenga config:** Debug / Release
**Erreur:**
- Message exact console
- Call stack si applicable
- Repro steps

**Attendu:**
Menu visible avec 6 jeux / Laser Puzzle playable / etc.

**Actuel:**
Crash / écran noir / lag / etc.
```

---

## Checkpoints validation finale

| Checkpoint | Critère | Statut |
|-----------|---------|--------|
| Files exist | Tous 11 fichiers présents | ❌ À vérifier |
| Build complet | `jenga build` sans erreur | ❌ À essayer |
| Lancement | Exe lance sans crash | ❌ À tester |
| Menu visible | 6 jeux listés | ❌ À tester |
| CLI backend | `--backend=opengl` OK | ❌ À tester |
| Laser Puzzle | Grille visible, R rotates | ❌ À tester |
| Retour menu | ESC → menu | ❌ À tester |
| Performance | 60 FPS stable | ❌ À tester |

---

## Prochaines actions

1. ✅ Créer tous fichiers
2. ❌ Vérifier Jenga file inclut tous .cpp
3. ❌ Lancer `jenga build --target Nkoung --config Debug`
4. ❌ Tester `./Nkoung.exe` et gameplay
5. ❌ Reporter bugs le cas échéant

---

## Support & contacts

- **Responsable Nkoung:** Rihen (@nkentseu)
- **Langue:** Français
- **Repo:** D:\Projets\2026\Nkentseu\Nkentseu
- **Documentation:** ARCHITECTURE_NKOUNG.md, ROADMAP.md
