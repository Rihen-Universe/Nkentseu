# Songo'o — Guide d'intégration dans le repo Nkentseu

## Structure finale dans le workspace Nkentseu

```
Nkentseu-main/
├── Applications/
│   └── Songo/                       ← COPIER ICI le contenu de songo_v2/
│       ├── Songo.jenga
│       └── src/
│           ├── Apps.cpp
│           └── Songoo/
│               ├── Game/
│               │   ├── GameTypes.h
│               │   ├── SongooBoard.h / .cpp
│               │   └── SongooGame.h / .cpp
│               ├── Audio/
│               │   └── AudioManager.h / .cpp
│               ├── Render/
│               │   └── SafeArea.h
│               └── UI/
│                   ├── Theme.h
│                   ├── UIScale.h
│                   ├── AppContext.h
│                   ├── Scene.h
│                   ├── SceneManager.h / .cpp
│                   └── Scenes/
│                       ├── RihenIntroScene.h / .cpp
│                       ├── NogeIntroScene.h / .cpp
│                       ├── StoryScene.h / .cpp
│                       ├── MainMenuScene.h / .cpp
│                       ├── GameplayScene.h / .cpp
│                       ├── GameOverScene.h / .cpp
│                       ├── OptionsScene.h / .cpp
│                       └── CreditsScene.h / .cpp
│
└── Resources/
    └── Songo/                        ← CRÉER CE DOSSIER
        ├── assets/
        │   ├── Background.png        ← fond du plateau (INCHANGÉ)
        │   ├── hand.png              ← main (INCHANGÉ)
        │   ├── Icon.png              ← icône app (INCHANGÉ)
        │   ├── Songo'oIcon.png       ← fond menu (INCHANGÉ)
        │   ├── trou0.png … trou15.png   ← textures trous (INCHANGÉ)
        │   ├── story_01.png … story_06.png  ← histoire (INCHANGÉ)
        │   ├── fonts/
        │   │   └── LiberationSans-Bold.ttf  ← AJOUTER
        │   └── animrihen/
        │       └── rihen_00000.png … rihen_00155.png  ← renommer depuis RIHEN LOGO_XXXXX.png
        └── audio/                    ← INCHANGÉ
            ├── background.mp3
            ├── credit.mp3
            ├── deposit.mp3
            ├── pickup.mp3
            ├── tambour.mp3
            └── songo2.mp3
```

---

## Étape 1 — Intégrer dans Nkentseu.jenga

Ouvrir `Nkentseu-main/Nkentseu.jenga` et ajouter :

```python
with include("Applications/Songo/Songo.jenga"):
    pass
```

---

## Étape 2 — Renommer les frames du logo

Les frames originales sont nommées `RIHEN LOGO_XXXXX.png` (avec espace).
Nkentseu Songo attend `rihen_NNNNN.png` (sans espace, de 00000 à 00155).

**Linux/macOS :**
```bash
cd Resources/Songo/assets/animrihen/
i=0
for f in "RIHEN LOGO_"*.png; do
    printf -v n "%05d" $i
    mv "$f" "rihen_${n}.png"
    ((i++))
done
```

**Windows (PowerShell) :**
```powershell
cd Resources\Songo\assets\animrihen
$i = 0
Get-ChildItem "RIHEN LOGO_*.png" | Sort-Object Name | ForEach-Object {
    $n = $i.ToString("D5")
    Rename-Item $_.Name "rihen_$n.png"
    $i++
}
```

---

## Étape 3 — Fichiers manquants à ajouter

| Fichier | Où l'obtenir |
|---------|-------------|
| `GLRenderer2D.h/.cpp` | Copier depuis `Applications/Songoo/src/Songoo/Render/` |
| `Texture2D.h/.cpp` | Copier depuis `Applications/Songoo/src/Songoo/Render/` |
| `GLContext.h/.cpp` | Copier depuis `Applications/Songoo/src/Songoo/Render/` |
| `FontAtlas.h/.cpp` | Copier depuis `Applications/Songoo/src/Songoo/Render/` |
| `SafeArea.cpp` | Copier depuis `Applications/Songoo/src/Songoo/Render/` |
| `LiberationSans-Bold.ttf` | Ubuntu : `/usr/share/fonts/truetype/liberation/` |

Ces fichiers sont **identiques** entre Songoo et Songo — ils font partie de
l'infrastructure de rendu partagée. Il suffit de les copier.

---

## Étape 4 — Build

```bash
# Desktop Windows
jenga build --platform windows --config Release

# Desktop Linux X11
jenga build --platform linux --linux-backend x11 --config Release

# Desktop Linux Wayland
jenga build --platform linux --linux-backend wayland --config Release

# Android (génère un APK)
jenga build --platform android --config Release

# Installer sur un appareil connecté via ADB
adb install Build/Bin/Release-Android/Songo/Songo.apk

# Web / WASM
jenga build --platform web --config Release
```

---

## Ce qui a changé par rapport à la conversion v1

| Aspect | v1 (conversion SDL3→NKRHI abstraite) | v2 (basé sur Songoo du repo) |
|--------|--------------------------------------|------------------------------|
| API RHI | NKRHI hypothétique (`NkICommandBuffer`) | `GLRenderer2D` réel (confirmé) |
| Audio | SFML3 conservée | **NKAudio natif** (pas de SFML3) |
| Architecture | `NkApplication + NkLayer` | `NkWindow + boucle manuelle + SceneManager` |
| Intro logo | Thread simple | **Worker thread asynchrone** (DrainQueue) |
| Scènes | Monolithique dans GameLayer | **Pile de scènes** (Push/Pop/Replace) |
| Android | config supposée | **Copie exacte de Songoo.jenga** |
| Fonts | NkUIFont (NKUI) | **FontAtlas** (GLRenderer2D natif) |

---

## Points d'attention signalés

| Point | Détail |
|-------|--------|
| `GLRenderer2D`, `Texture2D`, `GLContext`, `FontAtlas` | À copier depuis `Applications/Songoo/src/Songoo/Render/` — ces classes ne sont pas dans ce ZIP car elles sont déjà dans le repo |
| `SafeArea::From()` | L'implémentation `.cpp` est dans le repo Songoo — la copier |
| `NkMouseButtonReleaseEvent` | Vérifier le nom exact dans `NKEvent/NkMouseEvent.h` du repo |
| `NkTouchEndEvent` / `NkTouchPoint` | Vérifier dans `NKEvent/NkTouchEvent.h` |
| `AudioGenerator`, `AudioLoader`, `AudioEngine` | Vérifier les noms exacts dans `NKAudio/NKAudio.h` |
| `nkentseudependson()` dans Songo.jenga | Fonction helper définie dans `jengaconfig.py` — présente dans le workspace racine |
