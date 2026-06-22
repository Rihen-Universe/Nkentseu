# Guides Nkentseu — apprendre pas à pas (style SFML)

> Série de tutoriels pédagogiques pour **utiliser le moteur Nkentseu** afin de créer
> des projets enrichis, des **applications 2D** et des **jeux vidéo**.
> Esprit : comme la documentation de SFML — on part de zéro, on ajoute une brique à la
> fois, avec du **code réel et compilable**.
> Langue : français. Dernière mise à jour : 2026-06-20.

---

## À qui s'adressent ces guides

À quiconque veut écrire une application avec Nkentseu sans connaître les entrailles du
moteur. Chaque guide est autonome, va du plus simple au plus avancé, et renvoie aux
autres quand c'est utile.

## Parcours conseillé

| # | Guide | Ce que tu apprends |
|---|-------|--------------------|
| 0 | (ce fichier) | Mettre en place un projet, le point d'entrée, compiler avec Jenga |
| 1 | [NKMemory](01-NKMemory.md) | Allocateurs maison, `New`/`Delete`, smart pointers (la règle d'or mémoire) |
| 2 | [NKWindow](02-NKWindow.md) | Créer une fenêtre multi-plateforme, la boucle principale |
| 3 | [NKEvent](03-NKEvent.md) | Clavier, souris, tactile, manette, fermeture, focus |
| 4 | [NKImage](04-NKImage.md) | Charger/enregistrer des images (PNG, JPEG, SVG…) |
| 5 | [NKCanvas](05-NKCanvas.md) | **Rendu 2D** : sprites, formes, texte, transformations (le cœur 2D) |
| 6 | [NKAudio](06-NKAudio.md) | Sons, musique, volume, génération procédurale |
| 7 | [NKUI](07-NKUI.md) | Interface immédiate : boutons, panneaux, texte, thèmes |
| 8 | [NKNetwork](08-NKNetwork.md) | Réseau : transport, messages, multijoueur |
| 9 | [Projet 2D complet](09-Projet-2D-complet.md) | **Capstone** : un petit jeu de A à Z qui assemble tout |

> Les couches du moteur sont empilées : **Foundation** (NKMemory…) → **System** → **Runtime**
> (NKWindow, NKCanvas, NKAudio, NKImage, NKUI, NKEvent…) → **Engine/Applications**. Une couche
> n'utilise que celles en dessous. Voir `ARCHITECTURE.md` à la racine pour la vue d'ensemble.

---

## 0. Mettre en place un projet

### 0.1 Le point d'entrée

Nkentseu fournit un **point d'entrée portable** : tu écris `nkmain(...)` (et PAS `main()`),
le moteur s'occupe du `main`/`WinMain`/`android_main`/`emscripten` réel selon la plateforme.

```cpp
// main.cpp
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"      // fournit le vrai point d'entrée + nkmain()

using namespace nkentseu;

// Métadonnées de l'app (nom, version) — lues par le runtime au démarrage.
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "MonJeu";
    d.appVersion = "0.1.0";
    return d;
})());

// Ton vrai point d'entrée. `state` contient les arguments + l'état d'amorçage.
int nkmain(const NkEntryState& state) {
    // ... créer la fenêtre, lancer la boucle (voir guide NKWindow) ...
    return 0;
}
```

### 0.2 Forme minimale d'une application

Le squelette universel : créer une fenêtre, boucler tant qu'elle est ouverte, traiter les
événements, dessiner, recommencer. Les détails (fenêtre, événements, rendu) sont expliqués
dans les guides 2, 3 et 5. Vue d'ensemble :

```cpp
int nkmain(const NkEntryState& state) {
    // 1) Fenêtre (NKWindow)
    NkWindow window;
    NkWindowConfig cfg;
    cfg.title  = "Mon Application 2D";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!window.Create(cfg)) return -1;

    // 2) Cible de rendu 2D (NKCanvas) — voir guide 5
    //    renderer::NkRenderWindow rt(window, desc);

    // 3) Boucle principale
    while (window.IsOpen()) {
        // a) Événements (NKEvent) — voir guide 3
        while (NkEvent* ev = NkEvents().PollEvent()) {
            // réagir aux entrées / fermeture
        }
        // b) Mise à jour de la logique (dt)
        // c) Rendu (NKCanvas) — Clear, Draw..., Display
    }
    return 0;
}
```

> Selon la configuration, le runtime peut aussi gérer la boucle pour toi (modèle
> « application framework »). Le squelette ci-dessus est le modèle **direct**, le plus
> proche de SFML, utilisé par les jeux du dépôt (Pong, Mú…).

### 0.3 Compiler avec Jenga

Nkentseu se compile avec **Jenga** (descripteurs `*.jenga` en Python). Un projet déclare
ses dépendances de modules et Jenga propage includes + libs.

```python
# MonJeu.jenga (extrait)
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKWindow", "NKEvent", "NKCanvas", "NKImage", "NKAudio", "NKUI",
         "NKMemory", "NKCore", "NKMath", "NKLogger"],   # ce dont tu as besoin
        extra_includes=["src"],
    )
```

Commandes typiques :

```
jenga build                          # hôte par défaut, config Debug
jenga build --target MonJeu --config Release
jenga build --platform android       # ou windows/linux/web/harmonyos
```

> Chaque guide précise les modules à mettre dans `nkentseudependson` et les éventuels
> liens système par plateforme (ex. `OpenSLES` pour l'audio Android).

---

## Conventions de code (rappel)

- Types/classes : `NkPascalCase` ; interfaces : préfixe `NKI` ; méthodes : `PascalCase`.
- **Mémoire** : jamais de `new`/`delete`/`malloc` bruts → toujours **NKMemory** (guide 1).
- Zéro dépendance STL imposée : conteneurs/algorithmes maison (NKContainers, NKCore).

Bonne lecture — commence par le [guide NKMemory](01-NKMemory.md), puis
[NKWindow](02-NKWindow.md).
