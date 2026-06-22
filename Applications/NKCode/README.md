# NKCode

> ⚠️ **Squelette documentaire — aucun code pour l'instant.** On construit NKCode **from scratch**,
> au-dessus de **Nkentseu** (rendu/UI) et de **Jenga** (projets/build).

**NKCode** est un **IDE cross-plateforme** bâti sur Nkentseu. Il permet de :

1. **Éditer du code texte** comme VSCode (multi-fichiers, coloration, navigation) ;
2. **Créer & construire des projets** via **Jenga** (génération de `.jenga`, build/run/debug) ;
3. **Programmer visuellement** sous **deux formes nodales** :
   - **Blocs façon Scratch** (emboîtables, accessibles),
   - **Nœuds façon Unreal Blueprint** (broches typées, flux exec + data).

Le visuel **se compile en code texte** (puis se construit par Jenga), comme Blueprint → C++ chez
Unreal. Les trois formes cohabitent dans un même projet.

**C'est faisable dès maintenant** : tout le socle GUI existe (NKWindow, NKCanvas, NKUI, NKFont,
NKEvent, NKFileSystem) et Jenga fournit le backend projets/build. Voir la **faisabilité détaillée**
et le design dans **[ARCHITECTURE.md](ARCHITECTURE.md)**. Feuille de route : **[ROADMAP.md](ROADMAP.md)**.

---

## Structure du projet

```
Applications/NKCode/
  ARCHITECTURE.md            # vision, faisabilité, design complet
  README.md                  # ce fichier
  ROADMAP.md                 # feuille de route par phases
  NKCode.jenga               # (à venir) fichier de build Jenga de l'app
  src/NKCode/
    Shell/                   # coquille : fenêtre, docking, palette de commandes, thème, réglages
    Editor/                  # éditeur de code texte : tampon, coloration, gouttière, recherche
    Project/                 # intégration Jenga : modèles, génération .jenga, build/run, explorateur
    Graph/                   # substrat visuel : modèle nœud/broche/lien + canvas + sérialisation
    Blocks/                  # langage par blocs façon Scratch (sur Graph)
    Blueprint/               # langage nodal façon Unreal Blueprint (sur Graph)
    Codegen/                 # compilation visuel -> code texte (+ build via Jenga)
    UIBuilder/               # concepteur d'UI par glisser-déposer (WYSIWYG, inspecteur NKReflection)
    Extensions/              # packages/plugins façon VSCode (+ packages projet via Jenga)
    Agents/                  # IA de dev façon Claude : assistant + sous-agents (texte ou graphe)
```

Chaque sous-système est décrit dans son `README.md`.

---

## Les sous-systèmes

| Sous-système | Rôle | Phase |
|--------------|------|-------|
| [Shell](src/NKCode/Shell/README.md) | Fenêtre, layout/docking, palette de commandes, thème, réglages. | 1 |
| [Editor](src/NKCode/Editor/README.md) | Éditeur de code texte (coloration, multi-curseurs, recherche). | 2 |
| [Project](src/NKCode/Project/README.md) | Intégration Jenga : créer/build/run un projet, explorateur, sortie. | 3 |
| [Graph](src/NKCode/Graph/README.md) | Moteur de graphe partagé (nœuds/broches/liens + canvas + sérialisation). | 4 |
| [Blueprint](src/NKCode/Blueprint/README.md) | Nœuds typés façon Unreal (flux exec + data), palette via NKReflection. | 5 |
| [Codegen](src/NKCode/Codegen/README.md) | Visuel → code texte → build Jenga (le graphe qui s'exécute). | 6 |
| [UIBuilder](src/NKCode/UIBuilder/README.md) | Concepteur d'UI **par glisser-déposer** (WYSIWYG) ; inspecteur via NKReflection ; codegen NKUI. | 7 |
| [Blocks](src/NKCode/Blocks/README.md) | Langage par blocs façon Scratch (même substrat Graph). | 8 |
| [Extensions](src/NKCode/Extensions/README.md) | Packages/plugins façon VSCode + packages projet via Jenga. | 9 |
| [Agents](src/NKCode/Agents/README.md) | IA de dev façon Claude : assistant + **sous-agents** (texte ou graphe). | 10 |

---

## Conventions
- `NkPascalCase`, namespace prévu `nkentseu::nkcode`.
- Rend via **NKUI/NKCanvas**, entrées via **NKEvent**, fichiers via **NKFileSystem**, build par
  **Jenga**. Le visuel = un **modèle Graph unique** ; Blocks et Blueprint en sont deux vues.

[Architecture complète](ARCHITECTURE.md) · [Roadmap](ROADMAP.md)
