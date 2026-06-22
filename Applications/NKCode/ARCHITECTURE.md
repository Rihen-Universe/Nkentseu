# NKCode — Architecture

> ⚠️ **Statut : squelette documentaire.** Cette application ne contient pour l'instant **aucun
> code** — uniquement l'architecture, le `README` et la `ROADMAP`. On la construit **from scratch**,
> au-dessus de **Nkentseu** (rendu/UI) et de **Jenga** (projets/build).

---

## 1. Ce qu'est NKCode

**NKCode** est un **environnement de développement** (IDE) bâti sur Nkentseu et Jenga. Il poursuit
trois objectifs :

1. **Éditer du code texte** comme VSCode (multi-fichiers, coloration, navigation).
2. **Créer et construire des projets** en s'appuyant sur **Jenga** (génération de `.jenga`,
   build/run/debug via la CLI).
3. **Programmer visuellement**, sous **deux formes nodales** complémentaires :
   - **Blocs façon Scratch** — emboîtables, pour débuter / prototyper / le scripting accessible.
   - **Nœuds façon Unreal Blueprint** — broches typées, flux d'exécution + flux de données, pour
     la logique de jeu / le gameplay visuel.

Les trois formes ne sont pas cloisonnées : le **visuel se compile en code texte** (et inversement
on peut visualiser/éditer la structure), de sorte qu'un même projet mêle texte, blocs et nœuds.

---

## 2. Pourquoi c'est possible maintenant (faisabilité)

NKCode n'invente pas de brique fondamentale : il **assemble** ce que Nkentseu et Jenga offrent déjà.

| Besoin d'un IDE | Fourni par | État |
|-----------------|-----------|------|
| Fenêtre cross-plateforme | **NKWindow** (13 backends) | ✅ prod |
| Dessin 2D (UI, canvas de nœuds) | **NKCanvas** (software + GPU) | ✅ prod |
| Widgets / UI immédiate / draw lists | **NKUI** | ✅ |
| Polices (monospace pour le code) | **NKFont** | ✅ |
| Entrées (clavier/souris/tactile) | **NKEvent** | ✅ |
| Fichiers (ouvrir/sauver, projet) | **NKFileSystem** | ✅ |
| **Projets & build** (DSL, CLI, générateurs, outillage éditeur) | **Jenga** | ✅ |
| Sérialisation des graphes / modèle projet | **NKSerialization** + **NKReflection** | 🟡 à maturer |

**À construire** (rien de bloquant) : un **widget éditeur de texte**, un **moteur de graphe**
(nœuds/broches/liens + canvas), les **compilateurs visuel→code**, et les **services de langage**
(coloration d'abord, complétion/LSP ensuite).

> **En résumé.** Le socle GUI (NKWindow/NKCanvas/NKUI/NKFont/NKEvent/NKFileSystem) + Jenga (build)
> sont présents et éprouvés (Nkoung le prouve). NKCode est un **gros projet multi-phases**, mais
> **démarrable dès aujourd'hui** — on bâtit l'éditeur texte + l'intégration Jenga d'abord, puis les
> couches visuelles.

---

## 3. Place dans la pile

```
┌───────────────────────────────────────────────────────────┐
│  NKCode (cette app)                                        │
│    Shell · Editor · Project · Graph · Blocks · Blueprint · │
│    Codegen                                                 │
├───────────────────────────────────────────────────────────┤
│  Jenga (CLI + DSL .jenga + générateurs + outillage IDE)   │  ← backend projets/build
├───────────────────────────────────────────────────────────┤
│  Nkentseu — Runtime : NKCanvas, NKUI, NKFont, NKImage      │
│            System  : NKFileSystem, NKSerialization,        │
│                      NKReflection, NKThreading             │
│            (NKWindow, NKEvent)                             │
│            Foundation : NKMemory, NKContainers, NKMath     │
└───────────────────────────────────────────────────────────┘
```

NKCode **rend** son interface avec NKCanvas/NKUI, **lit/écrit** les fichiers via NKFileSystem,
**sérialise** ses graphes via NKSerialization, et **délègue à Jenga** tout ce qui touche aux
projets (création, build, run). C'est une **application** Nkentseu, comme Nkoung — pas un module
moteur.

---

## 4. Les sous-systèmes

| Sous-système | Rôle |
|--------------|------|
| **Shell** | La coquille : fenêtre, boucle, **layout/docking** des panneaux, **palette de commandes**, réglages, thème. |
| **Editor** | L'**éditeur de code texte** : tampon, curseur/sélection multi-curseurs, **coloration syntaxique**, gouttière, minimap, recherche. |
| **Project** | L'**intégration Jenga** : modèles de projet, génération/édition de `.jenga`, **build/run/debug** via la CLI, explorateur de fichiers, panneau de sortie. |
| **Graph** | Le **substrat de programmation visuelle** partagé : modèle **nœud / broche / lien**, le **canvas** (pan/zoom, sélection, connexion), undo/redo, **sérialisation** (NKReflection/NKSerialization). |
| **Blocks** | Le langage **par blocs façon Scratch** : palette de blocs, **emboîtement** (snapping), catégories, au-dessus de Graph. |
| **Blueprint** | Le langage **nodal façon Unreal Blueprint** : **broches typées**, **flux d'exécution** (pins blancs) + **flux de données** (pins typés), palette de nœuds (idéalement **auto-générée depuis NKReflection**). |
| **Codegen** | La **compilation visuel → code texte** (Blocks/Blueprint → C++/script) et l'exécution (génération + build via Jenga, ou interprétation par une petite VM). |
| **UIBuilder** | Le **concepteur d'UI par glisser-déposer** (WYSIWYG façon Qt Designer/UMG) : palette, canvas, hiérarchie, **inspecteur via NKReflection**, layout responsive, sérialisation `.nkui` + codegen NKUI, liaison événements → logique. |
| **Extensions** | Le **système de packages/plugins** (façon VSCode) : extensions d'éditeur (commandes, panneaux, langages, **nœuds**, thèmes) chargées en natif/scripté, + **packages de projet** via Jenga. |
| **Agents** | Le **système de développement IA** (façon Claude) : assistant + **sous-agents** parallèles, **en texte ou en graphe** (orchestration visuelle sur le substrat Graph). LLM externe, puis **local via NKAI**. |

Détail de chacun dans `src/NKCode/<Sous-système>/README.md`.

> **Deux multiplicateurs.** `Extensions` ouvre NKCode (chacun ajoute commandes/langages/**nœuds**) ;
> `Agents` met une IA de dev dans l'IDE. Leur synergie avec le reste : un agent peut **piloter
> NKCode via ses outils**, et un pipeline d'agents se **câble visuellement** avec le même éditeur de
> nœuds que le gameplay (substrat `Graph`). NKAI fournira plus tard les **modèles locaux**.

---

## 5. Les trois formes de code et le pont visuel↔texte

Le cœur conceptuel de NKCode est qu'une **même logique** peut s'exprimer en texte, en blocs ou en
nœuds, et que **le visuel se compile en texte** :

```
   Blocs (Scratch)  ─┐
                     ├─►  Modèle de GRAPHE (Graph)  ──►  Codegen  ──►  Code texte (Editor)  ──►  Jenga build
   Nœuds (Blueprint)─┘
```

- **Graph** est le modèle commun : Blocks et Blueprint en sont deux **présentations** (deux palettes
  + deux façons de relier). Cela évite de réimplémenter deux moteurs.
- **Codegen** traduit le graphe en code (C++ pour Blueprint, un script lisible pour Blocks), que
  **Jenga** compile ensuite — exactement le pipeline d'Unreal (Blueprint → C++ → build).
- À terme, le sens **texte → visuel** (parser un fichier en graphe) est l'objectif inverse, plus
  difficile, repoussé.

---

## 6. Modèle de données & persistance

- **Workspace** → un dossier ouvert ; **Project** → un projet Jenga (`.jenga` + sources) ;
  **Document** → un fichier ouvert (texte, ou graphe `.nkblocks` / `.nkbp`).
- Les **graphes** (Blocks/Blueprint) sont sérialisés via **NKSerialization**, leur schéma décrit par
  **NKReflection** (d'où l'intérêt de maturer ce module). Les fichiers texte restent du texte.
- L'**état de session** (panneaux, onglets ouverts) est persisté pour rouvrir là où on s'était
  arrêté.

---

## 7. Trajectoire (résumé — détail dans la ROADMAP)

```
1. Coquille : fenêtre NKUI + docking + palette de commandes + thème
2. Éditeur texte : tampon + coloration + ouvrir/sauver (NKFileSystem)
3. Intégration Jenga : créer un projet (modèle) + build/run + panneau de sortie
4. Graph : moteur de nœuds/broches/liens + canvas (pan/zoom/connexion) + sérialisation
5. Blueprint : nœuds typés + flux exec/data ; palette auto depuis NKReflection
6. Codegen : Blueprint → code → build via Jenga (le premier graphe qui s'exécute)
7. Blocks : langage par blocs façon Scratch sur le même substrat Graph
8. Polissage : complétion/LSP, debug, multi-curseurs, thèmes, extensions
```

Premier jalon visible : **éditer un fichier et le compiler via Jenga** depuis NKCode (étapes 1→3).
Jalon « waouh » : **un graphe Blueprint qui se compile et s'exécute** (étape 6).

---

## 8. Conventions

- Application Nkentseu : `NkPascalCase`, namespace prévu `nkentseu::nkcode` (ou `nkcode`).
- Rend via **NKUI/NKCanvas** (comme Nkoung) ; entrées via **NKEvent** ; fichiers via
  **NKFileSystem**. Construit par **Jenga** (`NKCode.jenga`).
- Le visuel s'appuie sur un **modèle Graph unique** ; Blocks et Blueprint en sont des vues.
- Codegen produit du texte que **Jenga** compile — NKCode ne réimplémente pas de compilateur C++.

---

[Sous-systèmes & présentation](README.md) · [Roadmap](ROADMAP.md)
