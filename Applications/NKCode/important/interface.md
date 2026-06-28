# Spécification Complète des Interfaces Utilisateur
# IDE Généraliste Avancé — Multi-Langages avec IA, Débogueur Visuel, Collaboration & Intégration Moteur
> Style hybride Visual Studio + JetBrains + Interface Custom
> Version 1.0

---

## Table des Matières

1. [Vue d'ensemble de l'interface principale](#1-vue-densemble-de-linterface-principale)
2. [Éditeur de Code (Code Editor)](#2-éditeur-de-code-code-editor)
3. [Explorateur de Projet (Project Explorer)](#3-explorateur-de-projet-project-explorer)
4. [Terminal Intégré](#4-terminal-intégré)
5. [Panneau de Débogage Visuel](#5-panneau-de-débogage-visuel)
6. [Interface IA (AI Assistant Panel)](#6-interface-ia-ai-assistant-panel)
7. [Gestionnaire de Git & Contrôle de Version](#7-gestionnaire-de-git--contrôle-de-version)
8. [Collaboration Temps Réel (Live Collab)](#8-collaboration-temps-réel-live-collab)
9. [Gestionnaire de Packages & Extensions](#9-gestionnaire-de-packages--extensions)
10. [Intégration Moteur de Jeu (Game Engine Bridge)](#10-intégration-moteur-de-jeu-game-engine-bridge)
11. [Intégration Applications Graphiques & Simulation](#11-intégration-applications-graphiques--simulation)
12. [Panneau de Problèmes & Diagnostics](#12-panneau-de-problèmes--diagnostics)
13. [Recherche Globale (Search & Replace)](#13-recherche-globale-search--replace)
14. [Gestionnaire de Tâches & Build System](#14-gestionnaire-de-tâches--build-system)
15. [Profiler & Analyseur de Performance](#15-profiler--analyseur-de-performance)
16. [Éditeur de Paramètres & Préférences](#16-éditeur-de-paramètres--préférences)
17. [Vue Split & Multi-Éditeurs](#17-vue-split--multi-éditeurs)
18. [Palette de Commandes (Command Palette)](#18-palette-de-commandes-command-palette)
19. [Status Bar (Barre de Statut)](#19-status-bar-barre-de-statut)
20. [Barre de Menus Principale](#20-barre-de-menus-principale)
21. [Workspaces & Layouts](#21-workspaces--layouts)
22. [Raccourcis Clavier Globaux](#22-raccourcis-clavier-globaux)
23. [Actions Souris Globales](#23-actions-souris-globales)
24. [Fenêtres Modales & Dialogues](#24-fenêtres-modales--dialogues)
25. [Système de Notifications & Toasts](#25-système-de-notifications--toasts)
26. [Thèmes & Personnalisation Visuelle](#26-thèmes--personnalisation-visuelle)

---

## 1. Vue d'ensemble de l'interface principale

### Description Générale

L'interface adopte une architecture en zones ancrables et détachables (dockable panels), inspirée de Visual Studio pour la robustesse et la profondeur des outils, de JetBrains pour l'intelligence contextuelle, et d'une interface custom pour les intégrations moteur et graphique. Toutes les zones peuvent être redimensionnées, détachées en fenêtres flottantes, ou réorganisées par glisser-déposer.

### Structure par Défaut (Layout "Développement")

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  MENU BAR │ File  Edit  View  Go  Run  Debug  Git  AI  Tools  Window  Help  │
├──────────────────────────────────────────────────────────────────────────────┤
│ ACTIVITY │                                                                    │
│  BAR     │  TABS: [main.cpp ×] [renderer.h ×] [shader.glsl ×] [CMake ×] …  │
│          ├────────────────────────────────────────┬──────────────────────────┤
│ 📁 Expl. │                                        │                          │
│          │         ÉDITEUR DE CODE                │    AI ASSISTANT          │
│ 🔍 Rech. │            (zone centrale)             │       PANEL              │
│          │                                        │                          │
│ 🔀 Git   │                                        ├──────────────────────────┤
│          │                                        │                          │
│ 🐛 Debug │                                        │    OUTLINE /             │
│          │                                        │    STRUCTURE             │
│ 🤖 AI    ├────────────────────────────────────────┤                          │
│          │  PANNEAU INFÉRIEUR (onglets):          │                          │
│ 🔧 Build │  [Terminal] [Problèmes] [Sortie] [Debug│] [Profiler] [Git Log]   │
│          │                                        │                          │
│ 🔌 Moteur│                                        │                          │
└──────────┴────────────────────────────────────────┴──────────────────────────┤
│  STATUS BAR : Branch: main  |  Erreurs: 0  Warnings: 3  |  UTF-8  |  C++   │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Activity Bar (Barre d'Activité Verticale)

Barre d'icônes sur le bord gauche, cliquable pour ouvrir/fermer chaque panneau latéral :

|
 Icône 
|
 Panneau 
|
 Raccourci 
|
|
-------
|
---------
|
-----------
|
|
 📁 
|
 Explorateur de Projet 
|
`Ctrl+Shift+E`
|
|
 🔍 
|
 Recherche Globale 
|
`Ctrl+Shift+F`
|
|
 🔀 
|
 Contrôle de Version (Git) 
|
`Ctrl+Shift+G`
|
|
 🐛 
|
 Débogage 
|
`Ctrl+Shift+D`
|
|
 🤖 
|
 AI Assistant 
|
`Ctrl+Shift+A`
|
|
 🔧 
|
 Build & Tâches 
|
`Ctrl+Shift+B`
|
|
 📊 
|
 Profiler 
|
`Ctrl+Shift+R`
|
|
 🔌 
|
 Moteur / Intégrations 
|
`Ctrl+Shift+M`
|
|
 🧩 
|
 Extensions & Packages 
|
`Ctrl+Shift+X`
|
|
 ⚙ 
|
 Paramètres 
|
`Ctrl+,`
|

Chaque icône peut afficher un badge de notification (nombre d'erreurs, de pull requests, etc.).

### Zones Redimensionnables

- **Bordures** : glisser pour redimensionner
- **Clic droit sur la bordure** : Masquer panneau, Réinitialiser la taille, Séparer en fenêtre
- **Drag & Drop** : faire glisser un panneau par son titre pour le repositionner
- **Double clic sur la bordure** : réinitialiser à la taille par défaut

---

## 2. Éditeur de Code (Code Editor)

### Description

Zone centrale de l'IDE. Supporte l'édition multi-onglets, le split horizontal et vertical, la coloration syntaxique pour tous les langages, l'IntelliSense avancé, les diagnostics inline, et les suggestions IA en temps réel.

### Zones de l'Éditeur

#### Tab Bar (Barre d'Onglets)

```
[main.cpp ×] [renderer.h] [shader.glsl •] [package.json] [+ Nouvel onglet]   [⊞ Split ▼]
```

- `•` (point) = fichier modifié non sauvegardé
- `×` = bouton fermer (apparaît au survol)
- Clic molette = fermer l'onglet
- Clic droit sur onglet → **Fermer**, **Fermer les autres**, **Fermer à droite**, **Copier le chemin**, **Révéler dans l'explorateur**, **Ouvrir dans un terminal**, **Épingler l'onglet**, **Split Vertical**, **Split Horizontal**
- Onglets épinglés : affichés avec une icône 📌, non fermables accidentellement
- Glisser un onglet → réordonner ou déplacer vers une autre zone split
- `Ctrl+Tab` → naviguer entre les onglets ouverts (liste déroulante MRU)

#### Gouttière Gauche (Gutter)

Bande verticale à gauche du code, de gauche à droite :

- **Numéros de ligne** (clic = sélectionner la ligne entière)
- **Breakpoints** (clic = ajouter/supprimer un breakpoint)
  - Point rouge = breakpoint actif
  - Point creux = breakpoint désactivé
  - Point jaune = breakpoint conditionnel
  - Point bleu = tracepoint (log sans arrêt)
- **Fold/Unfold** (▶/▼ sur les blocs pliables)
- **Indicateurs Git** (bande colorée) :
  - Vert = ligne ajoutée
  - Bleu = ligne modifiée
  - Rouge (triangle) = ligne supprimée
- **Suggestions IA** (icône 🤖 sur les lignes avec suggestion disponible)
- **Couverture de code** (bande verte/rouge si coverage activé)

#### Zone d'Édition Principale

- **Coloration syntaxique** : themes customisables (défaut : One Dark Pro, Dracula, GitHub Light)
- **Guides d'indentation** : lignes verticales subtiles
- **Bracket Matching** : surlignage des accolades/parenthèses correspondantes
- **Minimap** (coin droit) : vue miniaturisée du fichier, glissable
  - Clic → naviguer rapidement
  - Afficher les erreurs/modifications sur la minimap
  - Masquer : `Ctrl+Shift+\`
- **Scrollbar** avec indicateurs colorés (erreurs=rouge, avertissements=jaune, recherche=orange, breakpoint=rouge vif)
- **Curseur multiple** : `Alt+Clic` pour ajouter un curseur, `Ctrl+Alt+↑/↓` pour curseurs colonne

#### Hover Documentation

Au survol d'un symbole (après 500ms) :
```
┌──────────────────────────────────────────────────────┐
│  🔵 Renderer::drawMesh(Mesh* mesh, Shader* shader)  │
│  Rend un mesh avec le shader spécifié.              │
│                                                      │
│  @param mesh    Le mesh à rendre                    │
│  @param shader  Le shader à utiliser                │
│  @returns void                                       │
│                                                      │
│  [Aller à la définition]  [Trouver les références] │
│  [🤖 Expliquer avec l'IA]                          │
└──────────────────────────────────────────────────────┘
```

#### Inline Diagnostics

Sous les lignes d'erreur, un message inline apparaît :
```cpp
int result = myFunc(x, "hello");
//                  ^^^^^^^^^
//  ❌ Erreur: argument 2 de type 'const char*' incompatible avec 'int'
//  💡 Correction suggérée: (int)"hello" → utiliser atoi()  [Appliquer]
```

- `Ctrl+.` → Quick Fix / Correction rapide
- `F8` → Erreur suivante
- `Shift+F8` → Erreur précédente

#### IntelliSense & Autocomplétion

Déclenchée automatiquement (ou `Ctrl+Space`) :

```
renderer.draw█
┌──────────────────────────────────────────────┐
│ 🔵 drawMesh(Mesh*, Shader*)    méthode      │
│ 🔵 drawLine(Vec3, Vec3, Color) méthode      │
│ 🔵 drawDebugBox(AABB)          méthode      │
│ 🟡 drawCallCount               propriété    │
│ 🤖 drawMesh(currentMesh, activeShader) ← IA│
└──────────────────────────────────────────────┘
```

Types d'entrées :
- 🔵 Méthodes / Fonctions
- 🟡 Propriétés / Variables
- 🟢 Classes / Types
- 🔴 Keywords
- 🟣 Snippets
- 🤖 Suggestions IA (contextuelles)
- 📦 Imports suggérés

#### Suggestions IA Inline (Ghost Text)

Style Copilot/Cursor : le texte suggéré apparaît en gris après le curseur :

```cpp
void Renderer::init() {
    // Initialiser Vulkan
    VkInstanceCreateInfo createInfo{}; █
    // createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;  ← suggestion IA en gris
```

- `Tab` → Accepter la suggestion complète
- `Ctrl+→` → Accepter mot par mot
- `Escape` → Rejeter
- `Alt+]` → Suggestion suivante
- `Alt+[` → Suggestion précédente

#### Raccourcis Éditeur de Code

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+S`
|
 Sauvegarder 
|
|
`Ctrl+Shift+S`
|
 Sauvegarder tous les fichiers 
|
|
`Ctrl+Z`
|
 Undo 
|
|
`Ctrl+Shift+Z`
|
 Redo 
|
|
`Ctrl+C`
|
 Copier 
|
|
`Ctrl+X`
|
 Couper 
|
|
`Ctrl+V`
|
 Coller 
|
|
`Ctrl+A`
|
 Tout sélectionner 
|
|
`Ctrl+D`
|
 Sélectionner l'occurrence suivante du mot 
|
|
`Ctrl+Shift+L`
|
 Sélectionner toutes les occurrences 
|
|
`Ctrl+F`
|
 Chercher dans le fichier 
|
|
`Ctrl+H`
|
 Chercher & Remplacer dans le fichier 
|
|
`Ctrl+G`
|
 Aller à la ligne n° 
|
|
`Ctrl+P`
|
 Ouvrir un fichier (Quick Open) 
|
|
`Ctrl+Shift+P`
|
 Palette de commandes 
|
|
`F12`
|
 Aller à la définition 
|
|
`Alt+F12`
|
 Peek définition (inline, sans quitter le fichier) 
|
|
`Shift+F12`
|
 Trouver toutes les références 
|
|
`Ctrl+Shift+O`
|
 Aller à un symbole dans le fichier 
|
|
`Ctrl+T`
|
 Aller à un symbole dans le projet 
|
|
`F2`
|
 Renommer le symbole (refactoring) 
|
|
`Ctrl+.`
|
 Quick Fix / Action de code 
|
|
`Ctrl+/`
|
 Commenter / Décommenter la ligne 
|
|
`Ctrl+Shift+/`
|
 Commenter le bloc sélectionné 
|
|
`Alt+↑`
|
 Déplacer la ligne vers le haut 
|
|
`Alt+↓`
|
 Déplacer la ligne vers le bas 
|
|
`Shift+Alt+↑`
|
 Dupliquer la ligne vers le haut 
|
|
`Shift+Alt+↓`
|
 Dupliquer la ligne vers le bas 
|
|
`Ctrl+L`
|
 Sélectionner la ligne entière 
|
|
`Ctrl+Shift+K`
|
 Supprimer la ligne 
|
|
`Ctrl+Enter`
|
 Insérer une ligne en dessous 
|
|
`Ctrl+Shift+Enter`
|
 Insérer une ligne au-dessus 
|
|
`Tab`
|
 Indenter / Accepter suggestion 
|
|
`Shift+Tab`
|
 Désindenter 
|
|
`Ctrl+]`
|
 Indenter la sélection 
|
|
`Ctrl+[`
|
 Désindenter la sélection 
|
|
`Ctrl+Shift+[`
|
 Plier le bloc 
|
|
`Ctrl+Shift+]`
|
 Déplier le bloc 
|
|
`Ctrl+K Ctrl+0`
|
 Plier tout 
|
|
`Ctrl+K Ctrl+J`
|
 Déplier tout 
|
|
`Alt+Clic`
|
 Ajouter un curseur 
|
|
`Ctrl+Alt+↑`
|
 Ajouter curseur en haut 
|
|
`Ctrl+Alt+↓`
|
 Ajouter curseur en bas 
|
|
`Escape`
|
 Sortir du mode multi-curseur 
|
|
`Ctrl+Space`
|
 Déclencher l'autocomplétion 
|
|
`Ctrl+Shift+Space`
|
 Afficher la signature de fonction 
|
|
`F8`
|
 Aller à l'erreur suivante 
|
|
`Shift+F8`
|
 Aller à l'erreur précédente 
|
|
`Ctrl+K Ctrl+I`
|
 Afficher la documentation hover 
|
|
`Ctrl+Shift+M`
|
 Ouvrir le panneau Problèmes 
|
|
`Ctrl+`
` `
|
 Ouvrir le terminal 
|
|
`Ctrl+\`
|
 Split l'éditeur 
|
|
`Ctrl+W`
|
 Fermer l'onglet courant 
|
|
`Ctrl+Shift+T`
|
 Rouvrir l'onglet fermé 
|
|
`Ctrl+Tab`
|
 Onglet suivant (MRU) 
|
|
`Ctrl+Shift+Tab`
|
 Onglet précédent 
|
|
`Ctrl+K Ctrl+S`
|
 Ouvrir le panneau des raccourcis 
|
|
`Alt+Z`
|
 Activer/désactiver le word wrap 
|
|
`Ctrl+K Z`
|
 Mode Zen (plein écran sans distractions) 
|

#### Menu Contextuel (Clic Droit dans l'éditeur)

- Aller à la définition `F12`
- Aller à la déclaration
- Aller à l'implémentation
- Trouver les références `Shift+F12`
- Trouver toutes les implémentations
- Peek → Définition / Références / Implémentations
- Renommer le symbole `F2`
- Refactorer → Extraire fonction, Extraire variable, Inline, Déplacer vers fichier
- Quick Fix `Ctrl+.`
- Formater le document `Ctrl+Shift+I` / `Alt+Shift+F`
- Formater la sélection
- **🤖 IA** → Expliquer ce code, Corriger les bugs, Optimiser, Générer des tests, Documenter
- Changer le langage du fichier
- Copier le chemin relatif
- Révéler dans l'explorateur
- Ouvrir dans le terminal
- Comparer avec → (autre fichier, HEAD, clipboard)

---

## 3. Explorateur de Projet (Project Explorer)

### Description

Arborescence hiérarchique du projet avec gestion des fichiers, multi-racines, filtres, prévisualisations, et intégration Git (statut des fichiers colorés).

### Interface

#### Header

```
EXPLORATEUR                                     [⊕ Nouveau] [⊞ Collapse] [⚙]
──────────────────────────────────────────────────────────────
```

#### Corps

```
▼ 📁 MON_PROJET (racine)
  ▼ 📁 src
    ▼ 📁 core
      ├ 📄 main.cpp              [M]   ← modifié (git)
      ├ 📄 renderer.cpp          [M]
      ├ 📄 renderer.h
      └ 📄 physics.cpp           [U]   ← non-tracké
    ▼ 📁 shaders
      ├ 📄 basic.vert            [A]   ← ajouté (git)
      └ 📄 basic.frag
    ▼ 📁 ui
      └ 📄 mainwindow.cpp
  ▼ 📁 assets
    ▼ 📁 meshes
      └ 📄 player.fbx
    └ 📁 textures
  ▼ 📁 tests
    └ 📄 test_renderer.cpp
  ├ 📄 CMakeLists.txt
  ├ 📄 .gitignore
  └ 📄 README.md
▼ 📁 LIBS_EXTERNES (racine secondaire)
  └ 📁 vulkan-sdk
```

#### Indicateurs Git (lettres colorées)

|
 Indicateur 
|
 Couleur 
|
 Signification 
|
|
------------
|
---------
|
---------------
|
|
`M`
|
 Orange 
|
 Modifié 
|
|
`A`
|
 Vert 
|
 Ajouté (staged) 
|
|
`U`
|
 Cyan 
|
 Untracked (nouveau) 
|
|
`D`
|
 Rouge 
|
 Supprimé 
|
|
`C`
|
 Jaune 
|
 Conflit de merge 
|
|
`R`
|
 Bleu 
|
 Renommé 
|
|
`!`
|
 Rouge 
|
 Ignoré (.gitignore) 
|

#### Actions sur les Fichiers (Clic Droit)

- **Nouveau fichier** `Ctrl+N`
- **Nouveau dossier**
- **Renommer** `F2`
- **Supprimer** `Delete`
- **Déplacer** (drag & drop ou Cut/Paste)
- **Copier le chemin** / **Copier le chemin relatif**
- **Révéler dans l'explorateur système** `Ctrl+Shift+R`
- **Ouvrir dans le terminal**
- **Comparer avec HEAD** (Git)
- **Blame Git** (voir qui a modifié chaque ligne)
- **Annuler les modifications** (Git discard)
- **Ajouter au .gitignore**
- **Propriétés** (taille, permissions, dates)
- **🤖 IA → Générer fichier similaire**, **Analyser le fichier**, **Créer des tests**

#### Toolbar de l'Explorateur

- `⊕` **Nouveau fichier** `Ctrl+N`
- `📁` **Nouveau dossier**
- `↺` **Rafraîchir**
- `⊞` **Tout replier**
- `🔍` **Filtrer les fichiers** (champ de filtre inline)
- `👁` **Afficher/masquer les fichiers exclus**

#### Raccourcis Explorateur

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+N`
|
 Nouveau fichier 
|
|
`Ctrl+Shift+N`
|
 Nouveau dossier 
|
|
`F2`
|
 Renommer 
|
|
`Delete`
|
 Supprimer 
|
|
`Ctrl+C`
|
 Copier 
|
|
`Ctrl+X`
|
 Couper 
|
|
`Ctrl+V`
|
 Coller 
|
|
`Ctrl+D`
|
 Dupliquer 
|
|
`Enter`
|
 Ouvrir le fichier 
|
|
`Space`
|
 Prévisualiser le fichier (peek) 
|
|
`Ctrl+Clic`
|
 Sélection multiple 
|
|
`Shift+Clic`
|
 Sélection de plage 
|
|
`Alt+Clic`
|
 Ouvrir dans un split 
|
|
`Ctrl+Alt+R`
|
 Révéler dans l'explorateur système 
|
|
`Ctrl+K Ctrl+O`
|
 Ouvrir le dossier comme racine 
|
|
`Ctrl+R`
|
 Ajouter un dossier à l'espace de travail 
|

#### Barre de Filtre Inline

En tapant dans l'explorateur (sans raccourci), un filtre s'active en haut :
```
🔍 renderer█
```
Filtre en temps réel sur les noms de fichiers. `Escape` pour annuler.

---

## 4. Terminal Intégré

### Description

Terminal complet intégré dans le panneau inférieur. Supporte plusieurs shells (bash, zsh, PowerShell, cmd, fish), plusieurs instances simultanées en onglets, et une intégration profonde avec l'IDE (liens cliquables vers les erreurs, autocomplétion des commandes, intégration IA).

### Interface Terminal

#### Header du Terminal

```
TERMINAL    [bash ×] [bash] [pwsh] [+]    [⊞ Split] [🗑 Fermer] [⬆ Agrandir] [⤢]
```

- Onglets multiples avec le nom du shell
- `+` → Nouveau terminal (dans le même shell par défaut, ou sélection dans un déroulant)
- `⊞ Split` → Diviser le terminal horizontalement (deux terminaux côte à côte)
- `⤢` → Détacher en fenêtre flottante

#### Corps du Terminal

Terminal ANSI complet avec :
- Coloration syntaxique des commandes
- **Liens cliquables** : chemins de fichiers, URLs, numéros de ligne d'erreur
- **Sélection de texte** → copie automatique dans le presse-papiers
- Défilement avec scrollbar et molette
- Support Unicode et emoji
- **Historique** illimité avec scroll

#### Barre de Commandes IA (au-dessus du terminal)

```
🤖 [Décrire ce que vous voulez faire...                    ] [Exécuter] [Ctrl+K]
```

- Entrée en langage naturel → génère la commande shell correspondante
- Prévisualisation de la commande avant exécution
- `Ctrl+K` → activer/désactiver l'assistant IA terminal

#### Raccourcis Terminal

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
| `Ctrl+`` ` `` | Ouvrir/fermer le terminal |
| `Ctrl+Shift+`` ` `` | Nouveau terminal |
| `Ctrl+Shift+5` | Split terminal |
| `Ctrl+PgUp` | Onglet terminal précédent |
| `Ctrl+PgDn` | Onglet terminal suivant |
| `Ctrl+C` | Interrompre le processus courant |
| `Ctrl+D` | Fermer le terminal (EOF) |
| `Ctrl+L` | Effacer l'écran |
| `Ctrl+U` | Effacer la ligne courante |
| `Ctrl+A` | Début de la ligne |
| `Ctrl+E` | Fin de la ligne |
| `↑` / `↓` | Historique des commandes |
| `Ctrl+R` | Recherche dans l'historique |
| `Ctrl+Shift+C` | Copier la sélection |
| `Ctrl+Shift+V` | Coller |
| `Alt+Clic` | Repositionner le curseur |
| `Ctrl+Shift+F` | Chercher dans le terminal |
| `Ctrl+K` | Commande IA |

---

## 5. Panneau de Débogage Visuel

### Description

Débogueur graphique avancé inspiré de Visual Studio, avec support multi-langages (C/C++, Python, JavaScript, Rust, Go, Java, C#…), visualisation de la mémoire, des threads, de la pile d'appels, des variables, et des points d'arrêt avancés. Intégration spéciale pour le débogage de shaders GPU et de code de simulation.

### Sous-Panneaux du Débogueur

#### 5.1 Barre d'Outils de Débogage

Affichée en haut quand une session de débogage est active :

```
▶ [Continue F5] [⏸ Pause] [⏹ Stop] | [↷ Step Over F10] [↘ Step Into F11] [↗ Step Out Shift+F11] | [↺ Restart] | [🔥 Hot Reload] | Session: Debug (x64) ▼
```

- **Continue** `F5` — reprendre l'exécution jusqu'au prochain breakpoint
- **Pause** — suspendre l'exécution à la ligne courante
- **Stop** `Shift+F5` — arrêter la session
- **Step Over** `F10` — exécuter la ligne sans entrer dans les appels
- **Step Into** `F11` — entrer dans l'appel de fonction
- **Step Out** `Shift+F11` — sortir de la fonction courante
- **Restart** `Ctrl+Shift+F5` — relancer la session
- **Hot Reload** `Ctrl+Shift+H` — injecter les modifications sans redémarrer

#### 5.2 Panneau Variables

```
┌──────────────────────────────────────────────────────┐
│  VARIABLES                                           │
├──────────────────────────────────────────────────────┤
│  ▼ Locales                                          │
│    mesh          Mesh*      0x7f3a1b2c  📌          │
│    ▼ mesh→                                          │
│      vertexCount int        12450                   │
│      indexCount  int        24900                   │
│      ▶ vertices  float*     0x7f3a2000              │
│    shader        Shader*    0x7f4c0010              │
│    result        bool       true                    │
│  ▼ Globales                                         │
│    g_frameCount  uint64     1042                    │
│    g_deltaTime   float      0.016667                │
│  ▼ Watch (Surveillance)                             │
│    mesh→verts[0] Vec3       {0.5, 1.2, -0.3}       │
│    [+ Ajouter expression à surveiller]             │
└──────────────────────────────────────────────────────┘
```

- Clic sur `▶` → développer les pointeurs/structures
- Double clic sur une valeur → **modifier à la volée**
- Clic droit → **Ajouter à la surveillance**, **Copier la valeur**, **Afficher en hexadécimal**, **Afficher en binaire**, **Visualiser** (ouvre un visualiseur custom)
- 📌 → Épingler une variable (reste visible même hors de portée)
- Coloration : modifié depuis le step précédent = **orange**

#### 5.3 Panneau Watch (Surveillance)

```
┌──────────────────────────────────────────────────────┐
│  WATCH                                              │
├──────────────────────────────────────────────────────┤
│  Expression                    Valeur    Type       │
│  mesh→vertexCount              12450     int        │
│  transform.position            {1,0,0}   Vec3       │
│  sizeof(Renderer)              2048      size_t     │
│  frameTime * 1000.0f           16.667    float      │
│  [Saisir une expression...]                         │
└──────────────────────────────────────────────────────┘
```

#### 5.4 Pile d'Appels (Call Stack)

```
┌──────────────────────────────────────────────────────┐
│  PILE D'APPELS              Thread: Main [0] ▼      │
├──────────────────────────────────────────────────────┤
│  ► Renderer::drawMesh()    renderer.cpp:145  🔵     │
│    Scene::render()         scene.cpp:89             │
│    GameLoop::tick()        gameloop.cpp:62          │
│    main()                  main.cpp:23              │
│  [Code externe masqué]                              │
└──────────────────────────────────────────────────────┘
```

- Clic sur un frame → naviguer vers ce contexte (variables locales mises à jour)
- `🔵` = frame courant
- Clic droit → **Exécuter jusqu'ici**, **Définir comme frame courant**, **Copier**, **Charger les symboles**

#### 5.5 Panneau Threads

```
┌──────────────────────────────────────────────────────┐
│  THREADS                                            │
├──────────────────────────────────────────────────────┤
│  ► [0] Main Thread            En pause              │
│    [1] Render Thread          En cours              │
│    [2] Physics Thread         En cours              │
│    [3] Audio Thread           En attente            │
│    [4] AI Worker Thread       En cours              │
└──────────────────────────────────────────────────────┘
```

- Clic → basculer vers ce thread (call stack mis à jour)
- Clic droit → **Geler/Dégeler le thread**, **Nommer le thread**

#### 5.6 Points d'Arrêt (Breakpoints Panel)

```
┌──────────────────────────────────────────────────────┐
│  POINTS D'ARRÊT             [☑ Tout activer] [🗑]   │
├──────────────────────────────────────────────────────┤
│  ● renderer.cpp:145                       ☑         │
│  ◐ scene.cpp:89  (Conditionnel)           ☑         │
│    Condition: frameCount > 100            │         │
│    Nombre d'hits: 3                       │         │
│  ◈ physics.cpp:212  (Tracepoint)          ☑         │
│    Message: "Step {frameCount}"           │         │
│  ◉ Exceptions C++    Toutes              ☑         │
│  ◉ Exceptions C++    Non gérées          ☑         │
└──────────────────────────────────────────────────────┘
```

Types de breakpoints :
- `●` Standard — pause inconditionnelle
- `◐` Conditionnel — pause si l'expression est vraie
- `◈` Tracepoint — log un message sans pause
- `◉` Exception — pause sur les exceptions d'un type donné
- `◆` Compteur de hits — pause après N occurrences
- `◇` Dépendant d'une autre variable — pause si une variable change

#### 5.7 Visualiseur de Mémoire

```
┌──────────────────────────────────────────────────────────────────────┐
│  MÉMOIRE    Adresse: [0x7f3a2000         ]  [Aller]  Format: [Hex▼] │
├──────────────────────────────────────────────────────────────────────┤
│  0x7f3a2000  3F 80 00 00  00 00 00 00  3F 80 00 00  │  ?...  ?...  │
│  0x7f3a2010  00 00 00 00  BE CC CC CD  3F 4C CC CD  │  ....  ?L..  │
│  0x7f3a2020  40 00 00 00  00 00 00 00  40 00 00 00  │  @...  @...  │
└──────────────────────────────────────────────────────────────────────┘
```

#### 5.8 Débogage GPU & Shaders

Panneau spécial pour le débogage des shaders (GLSL, HLSL, WGSL) :

```
┌──────────────────────────────────────────────────────┐
│  🎮 GPU DEBUGGER                                    │
├──────────────────────────────────────────────────────┤
│  Pipeline active: ForwardPass                       │
│  Draw Call: #1042  Triangles: 24900                 │
├──────────────────────────────────────────────────────┤
│  Pixel Sélectionné: (640, 400)                      │
│  Valeurs interpolées:                               │
│    position  vec4  (0.5, 1.2, -0.3, 1.0)           │
│    normal    vec3  (0.0, 1.0, 0.0)                  │
│    texcoord  vec2  (0.5, 0.5)                       │
├──────────────────────────────────────────────────────┤
│  Étapes du pipeline:                                │
│  [Vertex ✅] [Tessellation] [Geometry] [Fragment ✅]│
│  [Compute]                                          │
├──────────────────────────────────────────────────────┤
│  [📸 Capture Frame]  [⏸ Step Shader]  [📊 Stats]  │
└──────────────────────────────────────────────────────┘
```

#### Raccourcis Débogage

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`F5`
|
 Démarrer / Continuer 
|
|
`Shift+F5`
|
 Arrêter 
|
|
`Ctrl+Shift+F5`
|
 Redémarrer 
|
|
`F9`
|
 Basculer breakpoint sur la ligne 
|
|
`Ctrl+F9`
|
 Désactiver le breakpoint 
|
|
`Shift+F9`
|
 Breakpoint conditionnel 
|
|
`F10`
|
 Step Over 
|
|
`F11`
|
 Step Into 
|
|
`Shift+F11`
|
 Step Out 
|
|
`Ctrl+F10`
|
 Exécuter jusqu'au curseur 
|
|
`Alt+F9`
|
 Définir l'instruction suivante 
|
|
`Ctrl+Shift+H`
|
 Hot Reload 
|
|
`Ctrl+Alt+B`
|
 Ouvrir panneau Breakpoints 
|
|
`Ctrl+Alt+W`
|
 Ouvrir Watch 
|
|
`Ctrl+Alt+C`
|
 Ouvrir Call Stack 
|
|
`Ctrl+Alt+T`
|
 Ouvrir Threads 
|
|
`Ctrl+Alt+M`
|
 Ouvrir Memory 
|

---

## 6. Interface IA (AI Assistant Panel)

### Description

Panneau latéral droit dédié à l'IA, inspiré de GitHub Copilot Chat et Cursor. Supporte le chat multi-tours, la génération de code, l'analyse, le refactoring, la génération de tests, et une intégration profonde avec le contexte du projet.

### Sous-Panneaux IA

#### 6.1 Chat IA

```
┌─────────────────────────────────────────────────────┐
│  🤖 AI ASSISTANT          Modèle: [GPT-4o ▼]  [⚙] │
├─────────────────────────────────────────────────────┤
│  Contexte: renderer.cpp:145  📎  [+Ajouter]        │
├─────────────────────────────────────────────────────┤
│                                                     │
│  👤 Comment optimiser cette boucle de rendu         │
│     pour réduire les draw calls ?                  │
│                                                     │
│  🤖 Pour réduire les draw calls, voici trois       │
│     techniques :                                   │
│                                                     │
│     1. **Instanced Rendering** — Regroupez les     │
│        meshes identiques :                         │
│     ```cpp                                         │
│     glDrawElementsInstanced(GL_TRIANGLES,          │
│       indexCount, GL_UNSIGNED_INT, 0,              │
│       instanceCount);                              │
│     ```                                            │
│     2. **Batching** — Fusionnez les vertex        │
│        buffers compatibles...                      │
│                                                    │
│  [📋 Copier] [💾 Insérer dans l'éditeur] [🔁 Retry]│
├─────────────────────────────────────────────────────┤
│  [📎] [/cmd] Poser une question...          [Envoyer]│
└─────────────────────────────────────────────────────┘
```

- **Contexte** : fichiers, sélections ou symboles ajoutés comme contexte
- **Modèle** : sélectionner le modèle IA (local via Ollama, cloud via API)
- **Commandes slash** (`/`) :
  - `/explain` — expliquer le code sélectionné
  - `/fix` — corriger les bugs
  - `/optimize` — optimiser les performances
  - `/test` — générer des tests unitaires
  - `/doc` — générer la documentation
  - `/refactor` — refactorer le code
  - `/review` — revue de code
  - `/commit` — générer un message de commit
  - `/search` — chercher dans la codebase
  - `/new` — créer un nouveau fichier/composant

#### 6.2 Génération de Code

```
┌─────────────────────────────────────────────────────┐
│  🤖 GÉNÉRATION DE CODE                             │
├─────────────────────────────────────────────────────┤
│  Description:                                       │
│  ┌─────────────────────────────────────────────────┐│
│  │ Une classe Renderer Vulkan avec support multi-  ││
│  │ thread, triple buffering et validation layers   ││
│  └─────────────────────────────────────────────────┘│
│                                                     │
│  Langage: [C++ ▼]  Style: [Mon projet ▼]          │
│  Fichier cible: [renderer.h ▼]                    │
│                                                     │
│  [🎲 Générer]  [🔄 Régénérer]                     │
│                                                     │
│  Options:                                           │
│  [☑] Inclure les commentaires                     │
│  [☑] Générer les tests associés                   │
│  [☑] Respecter les conventions du projet          │
│  [☐] Mode verbeux (explication incluse)           │
└─────────────────────────────────────────────────────┘
```

#### 6.3 Revue de Code IA

```
┌─────────────────────────────────────────────────────┐
│  🔍 REVUE DE CODE IA                               │
├─────────────────────────────────────────────────────┤
│  Scope: [Fichier courant ▼]                        │
│  Focus: [☑] Bugs  [☑] Performance  [☑] Sécurité  │
│         [☑] Lisibilité  [☐] Architecture           │
├─────────────────────────────────────────────────────┤
│  [▶ Analyser]                                      │
├─────────────────────────────────────────────────────┤
│  Résultats (renderer.cpp):                         │
│                                                     │
│  ⚠ Ligne 145: Fuite mémoire potentielle           │
│    `vertexBuffer` alloué sans `delete` correspondant│
│    [Voir]  [Corriger auto]                         │
│                                                     │
│  💡 Ligne 189: Optimisation possible               │
│    Utiliser `std::move` au lieu de copie           │
│    [Voir]  [Appliquer]                             │
│                                                     │
│  ✅ Gestion des erreurs : Correcte                 │
│  ✅ Thread safety : OK (mutexes présents)          │
└─────────────────────────────────────────────────────┘
```

#### 6.4 Raccourcis IA

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+A`
|
 Ouvrir/fermer le chat IA 
|
|
`Ctrl+I`
|
 IA inline (sur la sélection courante) 
|
|
`Ctrl+Shift+I`
|
 Générer du code à la position du curseur 
|
|
`Alt+\`
|
 Déclencher les suggestions inline manuellement 
|
|
`Ctrl+Alt+/`
|
 Expliquer la sélection 
|
|
`Ctrl+Alt+F`
|
 Fix la sélection avec l'IA 
|
|
`Ctrl+Alt+T`
|
 Générer des tests pour la sélection 
|
|
`Ctrl+Alt+D`
|
 Documenter la sélection 
|
|
`Ctrl+Alt+O`
|
 Optimiser la sélection 
|
|
`Ctrl+Alt+R`
|
 Revue de code IA (fichier courant) 
|
|
`Escape`
|
 Fermer / rejeter la suggestion IA 
|

---

## 7. Gestionnaire de Git & Contrôle de Version

### Description

Interface graphique complète pour Git (et autres VCS : SVN, Mercurial via plugins), incluant la gestion des commits, branches, merges, rebase, stash, blame, historique, et diff visuel.

### Sous-Panneaux Git

#### 7.1 Vue Principale Git

```
┌─────────────────────────────────────────────────────┐
│  SOURCE CONTROL                   [↻ Sync] [⋯]    │
├─────────────────────────────────────────────────────┤
│  Branch: ██ feature/vulkan-renderer  [⊞ New] [↕]  │
├─────────────────────────────────────────────────────┤
│  Message de commit:                                 │
│  ┌─────────────────────────────────────────────────┐│
│  │ feat(renderer): add Vulkan backend              ││
│  │                                                 ││
│  │ - Implémente VkInstance et VkDevice             ││
│  └─────────────────────────────────────────────────┘│
│  [🤖 Générer message] [✅ Commit] [Commit & Push]  │
├─────────────────────────────────────────────────────┤
│  ▼ Staged Changes (3)                              │
│    ✅ renderer.cpp    [M]  [-]                     │
│    ✅ renderer.h      [M]  [-]                     │
│    ✅ CMakeLists.txt  [M]  [-]                     │
│  ▼ Changes (5)                                     │
│    📄 shader.glsl     [M]  [+] [↺]               │
│    📄 physics.cpp     [U]  [+] [↺]               │
│    📄 scene.cpp       [M]  [+] [↺]               │
│    📄 main.cpp        [M]  [+] [↺]               │
│    📄 test.cpp        [U]  [+] [↺]               │
│  ▼ Stashed (1)                                     │
│    📦 stash@{0}: WIP on main                      │
└─────────────────────────────────────────────────────┘
```

- `[+]` → Stager le fichier
- `[-]` → Déstaguer le fichier
- `[↺]` → Annuler les modifications
- `✅` → Fichier stagé
- Clic sur le fichier → ouvrir le diff

#### 7.2 Diff Visuel

Vue deux colonnes (ou inline) des différences :

```
┌──────────────────────────┬──────────────────────────┐
│  renderer.cpp (HEAD)     │  renderer.cpp (Working)  │
├──────────────────────────┼──────────────────────────┤
│  145  void drawMesh() {  │  145  void drawMesh() {  │
│  146    bind(mesh);      │  146    bind(mesh);      │
│  ──────────────────────  │  147    validateState(); │ ← AJOUTÉ
│  147    draw(shader);    │  148    draw(shader);    │
│  148  }                  │  149  }                  │
└──────────────────────────┴──────────────────────────┘
  [◀ Précédent changement]              [Suivant ▶]
```

- Lignes ajoutées : fond vert
- Lignes supprimées : fond rouge
- Lignes modifiées : fond bleu/orange
- `Ctrl+Alt+←/→` → naviguer entre les changements
- `Ctrl+Alt+Enter` → Stager le changement courant
- `Ctrl+Alt+Backspace` → Annuler le changement courant

#### 7.3 Gestionnaire de Branches

```
┌─────────────────────────────────────────────────────┐
│  BRANCHES                          [+ Nouvelle]     │
├─────────────────────────────────────────────────────┤
│  ▼ 📍 Locale                                       │
│    ► main                          ↓ 0  ↑ 2       │
│    ★ feature/vulkan-renderer       ↓ 5  ↑ 12      │ ← Courante
│      fix/memory-leak               ↓ 2  ↑ 1       │
│  ▼ 🌐 Distante (origin)                            │
│      origin/main                                   │
│      origin/develop                                │
│  ▼ 🏷 Tags                                         │
│      v1.0.0                                        │
│      v1.1.0-beta                                   │
└─────────────────────────────────────────────────────┘
```

- `↓/↑` → nombre de commits derrière/devant origin
- Clic droit → **Checkout**, **Merge dans la branche courante**, **Rebase**, **Supprimer**, **Pousser**, **Comparer avec la branche courante**

#### 7.4 Vue Historique (Git Log)

```
┌─────────────────────────────────────────────────────────────────────────┐
│  HISTORIQUE                        [🔍] [🌿 Branch: All ▼] [Auteur ▼]  │
├────┬──────────────────────────────────────────────────────┬─────────────┤
│Gph │  Message                                             │ Auteur      │
├────┼──────────────────────────────────────────────────────┼─────────────┤
│ ●  │  feat(renderer): add Vulkan backend              4h  │ Vous        │
│ │  │  chore: update dependencies                     1j  │ Alice       │
│ ●  │  fix: corrige la fuite mémoire dans Scene       2j  │ Vous        │
│ │  │  refactor: sépare le renderer en modules        3j  │ Bob         │
│ ●  │  initial commit                                 5j  │ Vous        │
└────┴──────────────────────────────────────────────────────┴─────────────┘
```

- Graphe de branches coloré à gauche
- Clic sur un commit → détails (diff, auteur, date, hash, tags)
- Clic droit → **Checkout**, **Cherry-pick**, **Revert**, **Reset**, **Créer une branche ici**, **Créer un tag**

#### Raccourcis Git

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+G`
|
 Ouvrir le panneau Git 
|
|
`Ctrl+K Ctrl+G`
|
 Stager le fichier courant 
|
|
`Ctrl+K Ctrl+U`
|
 Déstaguer le fichier courant 
|
|
`Ctrl+K Ctrl+C`
|
 Commit (avec message) 
|
|
`Ctrl+K Ctrl+P`
|
 Push 
|
|
`Ctrl+K Ctrl+F`
|
 Fetch 
|
|
`Ctrl+K Ctrl+L`
|
 Pull 
|
|
`Ctrl+K Ctrl+Z`
|
 Stash 
|
|
`Ctrl+K Ctrl+A`
|
 Appliquer le stash 
|
|
`Ctrl+Shift+Alt+D`
|
 Ouvrir le diff du fichier courant 
|
|
`Ctrl+Shift+Alt+H`
|
 Ouvrir l'historique du fichier courant 
|
|
`Ctrl+Shift+Alt+B`
|
 Git Blame du fichier courant 
|

---

## 8. Collaboration Temps Réel (Live Collab)

### Description

Fonctionnalité de collaboration en temps réel, similaire à VS Live Share, permettant à plusieurs développeurs de coder ensemble, partager des sessions de débogage, des terminaux, et des annotations.

### Interface Live Collab

#### Panneau Live Collab

```
┌─────────────────────────────────────────────────────┐
│  🌐 LIVE COLLAB                                    │
├─────────────────────────────────────────────────────┤
│  Session: mon-projet-vulkan                         │
│  Rôle: 👑 Hôte                                     │
│  Lien: [collab://abc123def   ] [📋 Copier] [QR]    │
├─────────────────────────────────────────────────────┤
│  Participants (3):                                  │
│  🟢 👤 Vous (Hôte)              renderer.cpp:145   │
│  🟢 👤 Alice                    shader.glsl:22     │
│  🟡 👤 Bob (En attente)                            │
│  [+ Inviter]                                       │
├─────────────────────────────────────────────────────┤
│  Ressources partagées:                              │
│  [☑] Fichiers en lecture  [☑] Fichiers en écriture │
│  [☑] Terminal             [☑] Session de débogage  │
│  [☐] Serveurs locaux      [☑] Focus Follow         │
├─────────────────────────────────────────────────────┤
│  [⏸ Pause partage]  [🚪 Quitter]  [⚙ Permissions] │
└─────────────────────────────────────────────────────┘
```

#### Indicateurs dans l'Éditeur

- **Curseur coloré** de chaque participant visible dans le code
- **Sélection colorée** par participant
- **Tooltip au survol** du curseur : nom du participant
- **Bandeau de suivi** : "Suivre Alice" → la vue se synchronise sur Alice

#### Chat de Session

```
┌─────────────────────────────────────────────────────┐
│  💬 CHAT DE SESSION                                │
├─────────────────────────────────────────────────────┤
│  Alice: regarde la ligne 145, je pense que         │
│         le problème est là                         │
│  Vous: oui tu as raison, on peut utiliser          │
│        VkDestroyBuffer ici                         │
│  Bob: je rejoins dans 2 min                       │
├─────────────────────────────────────────────────────┤
│  [Écrire un message...              ] [Envoyer]    │
└─────────────────────────────────────────────────────┘
```

#### Raccourcis Live Collab

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+L`
|
 Démarrer / rejoindre une session 
|
|
`Ctrl+Shift+Alt+L`
|
 Arrêter la session 
|
|
`Ctrl+Shift+F`
|
 Suivre un participant (focus follow) 
|
|
`Ctrl+Shift+Alt+F`
|
 Arrêter de suivre 
|
|
`Ctrl+Shift+Alt+T`
|
 Partager un terminal 
|
|
`Ctrl+Shift+Alt+D`
|
 Partager la session de débogage 
|

---

## 9. Gestionnaire de Packages & Extensions

### Description

Marketplace intégré pour les extensions, plugins, et packages de langages. Similaire au marketplace VS Code mais avec des catégories spécifiques aux intégrations moteur.

### Interface

#### Header

```
EXTENSIONS          [🔍 Rechercher extensions...    ]  [☁ Marketplace] [📦 Installées]
```

#### Résultats de Recherche / Liste

```
┌─────────────────────────────────────────────────────────────────────┐
│  📦 CMake Tools                    ⭐4.8  📥2.1M  ✅ Installée     │
│  Intégration CMake complète avec IntelliSense et débogage          │
│  [Désinstaller]  [⚙ Configurer]                                    │
├─────────────────────────────────────────────────────────────────────┤
│  📦 GLSL Language Support          ⭐4.6  📥890K                   │
│  Coloration syntaxique, IntelliSense et validation GLSL/HLSL       │
│  [Installer]                                                        │
├─────────────────────────────────────────────────────────────────────┤
│  📦 Mon Moteur Bridge              ⭐—    📥—    🔌 Local          │
│  Intégration native avec votre moteur de jeu/simulation            │
│  [Configurer]  [📚 Documentation]                                  │
└─────────────────────────────────────────────────────────────────────┘
```

#### Catégories (Filtre Gauche)

- Tous
- Langages (C/C++, Python, Rust, Go, JavaScript, GLSL, HLSL, WGSL…)
- Débogueurs
- Intégrations IA
- Moteurs de jeu (Unity, Unreal, Godot, Mon Moteur)
- Build Systems (CMake, Make, Ninja, Meson)
- Linters & Formatters
- Thèmes & Icons
- Collaboration
- Visualiseurs & Profilers

---

## 10. Intégration Moteur de Jeu (Game Engine Bridge)

### Description

Interface d'intégration bidirectionnelle avec les moteurs de jeu (Unity, Unreal Engine 5, Godot) et votre propre moteur. Permet de recompiler, relancer, déboguer et synchroniser le code sans quitter l'IDE.

### Interface Game Engine Bridge

#### Panneau Principal

```
┌─────────────────────────────────────────────────────┐
│  🔌 ENGINE BRIDGE                                  │
├─────────────────────────────────────────────────────┤
│  Moteur connecté: [Mon Moteur v2.1 ▼]  🟢 Connecté │
│  Projet moteur: /projects/game/           [📂]     │
├─────────────────────────────────────────────────────┤
│  ▼ Actions Rapides                                  │
│  [▶ Build & Run]  [🔨 Build Only]  [⏹ Arrêter]    │
│  [🔥 Hot Reload Script]  [🔄 Sync Assets]          │
├─────────────────────────────────────────────────────┤
│  ▼ Configuration                                    │
│  Target: [Debug ▼]  Platform: [Windows x64 ▼]     │
│  Scene: [MainScene.scene ▼]                        │
├─────────────────────────────────────────────────────┤
│  ▼ Logs Moteur (temps réel)                        │
│  [INFO]  Renderer initialisé (Vulkan 1.3)          │
│  [INFO]  Scene chargée: MainScene                  │
│  [WARN]  Texture manquante: wood_normal.png        │
│  [ERR ]  Script Lua: erreur ligne 42               │
│  [Filtrer...                    ] [🗑 Effacer]     │
├─────────────────────────────────────────────────────┤
│  ▼ Profil d'exécution                              │
│  FPS: 144  Frame: 16.67ms  GPU: 8.2ms  CPU: 4.1ms │
│  Draw Calls: 1042  Triangles: 2.4M  VRAM: 512MB   │
└─────────────────────────────────────────────────────┘
```

#### Onglet Assets Moteur

```
┌─────────────────────────────────────────────────────┐
│  ASSETS MOTEUR                [↻ Sync] [+ Importer] │
├─────────────────────────────────────────────────────┤
│  ▼ 📁 Meshes                                       │
│    ├ 🟠 player.fbx         ✅ Compilé  [Modifier]  │
│    └ 🟠 enemy.obj          ⚠ Dépassé  [Recompiler] │
│  ▼ 📁 Shaders                                      │
│    ├ 📄 basic.vert         ✅ Compilé              │
│    └ 📄 pbr.frag           🔄 En cours...          │
│  ▼ 📁 Scripts                                      │
│    └ 📄 player_controller.lua                      │
└─────────────────────────────────────────────────────┘
```

#### Intégration Unreal Engine 5

```
┌─────────────────────────────────────────────────────┐
│  UNREAL ENGINE 5                                   │
├─────────────────────────────────────────────────────┤
│  Projet: /UE5/MyGame/MyGame.uproject              │
│  Plugin IDE: ✅ Installé (v2.3)                   │
├─────────────────────────────────────────────────────┤
│  [▶ Ouvrir dans UE5]  [🔨 Compiler le C++]        │
│  [🔥 Live Coding]     [📦 Package le jeu]          │
├─────────────────────────────────────────────────────┤
│  Live Coding: [☑ Activé]                          │
│  Patches: 3 appliqués dans la session              │
└─────────────────────────────────────────────────────┘
```

#### Raccourcis Engine Bridge

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+F5`
|
 Build & Run dans le moteur 
|
|
`Ctrl+Shift+F5`
|
 Arrêter le moteur 
|
|
`Ctrl+Alt+F5`
|
 Build only 
|
|
`Ctrl+Shift+H`
|
 Hot Reload script/code 
|
|
`Ctrl+Alt+S`
|
 Synchroniser les assets 
|
|
`Ctrl+Alt+L`
|
 Ouvrir les logs moteur 
|
|
`Ctrl+Shift+M`
|
 Ouvrir le panneau Engine Bridge 
|

---

## 11. Intégration Applications Graphiques & Simulation

### Description

Interface de connexion avec vos applications graphiques custom (renderer 3D, simulateur physique, application de visualisation de données) via un protocole de communication IPC/socket. Permet le débogage live de shaders, la visualisation des données de simulation en temps réel, et l'injection de code à chaud.

### Interface App Integration Panel

#### Connexion & État

```
┌─────────────────────────────────────────────────────┐
│  🖥 APP INTEGRATION                                │
├─────────────────────────────────────────────────────┤
│  Applications connectées:                           │
│  🟢 Mon Renderer 3D     PID: 4821    [Déboguer]    │
│  🟢 Simulateur Physique PID: 4822    [Déboguer]    │
│  🔴 App Visualisation   Non lancée  [▶ Lancer]     │
│  [+ Connecter une app]  [⚙ Configurer protocole]  │
├─────────────────────────────────────────────────────┤
│  Protocole: [TCP Socket: 6543 ▼]                   │
│  Auto-reconnect: [☑]  Timeout: [5000ms]            │
└─────────────────────────────────────────────────────┘
```

#### Débogage Shader Live

```
┌─────────────────────────────────────────────────────┐
│  🎨 SHADER LIVE DEBUGGER                           │
├─────────────────────────────────────────────────────┤
│  App: Mon Renderer 3D                              │
│  Shader actif: pbr.frag (Fragment Shader)          │
├─────────────────────────────────────────────────────┤
│  Uniform Values (temps réel):                      │
│  lightDir     vec3   (0.577, 0.577, -0.577)       │
│  albedo       vec3   (0.8, 0.2, 0.2)              │
│  roughness    float  0.35                          │
│  metallic     float  0.0                           │
│  ─────────────────────────────────────────────────  │
│  Pixel sous le curseur: (640, 400)                 │
│  gl_FragCoord: (640.5, 400.5, 0.9, 1.0)          │
│  Output color: (0.82, 0.31, 0.18, 1.0)            │
├─────────────────────────────────────────────────────┤
│  [📸 Capture] [⏸ Pause rendu] [🔄 Recharger shader]│
└─────────────────────────────────────────────────────┘
```

#### Visualiseur de Données de Simulation

```
┌─────────────────────────────────────────────────────┐
│  📊 SIMULATION DATA VIEWER                         │
├─────────────────────────────────────────────────────┤
│  App: Simulateur Physique  Step: 1042  dt: 0.016s  │
├─────────────────────────────────────────────────────┤
│  Entités actives: 1,204                            │
│  Contacts: 348  Contraintes: 892                   │
├─────────────────────────────────────────────────────┤
│  Inspecter entité: [ID: 42           ]  [Aller]   │
│  ▼ Entité #42                                     │
│    position   vec3   (1.5, 0.0, 2.3)             │
│    velocity   vec3   (0.1, -9.8, 0.0)            │
│    mass       float  5.0                          │
│    AABB       AABB   min(-0.5,-0.5) max(0.5,0.5) │
├─────────────────────────────────────────────────────┤
│  [📈 Graphique temps réel]  [💾 Enregistrer]      │
│  [⏸ Pause sim]  [↷ Step unique]  [▶ Reprendre]   │
└─────────────────────────────────────────────────────┘
```

#### Hot Reload Graphique

```
┌─────────────────────────────────────────────────────┐
│  🔥 HOT RELOAD                                     │
├─────────────────────────────────────────────────────┤
│  Mode: [Auto (à la sauvegarde) ▼]                 │
│                                                     │
│  Derniers rechargements:                           │
│  ✅ pbr.frag           il y a 2s                  │
│  ✅ basic.vert         il y a 45s                 │
│  ❌ compute.comp       Erreur compilation          │
│      "Ligne 32: undeclared identifier 'g_Data'"   │
├─────────────────────────────────────────────────────┤
│  [🔄 Forcer rechargement] [⚙ Options]              │
└─────────────────────────────────────────────────────┘
```

---

## 12. Panneau de Problèmes & Diagnostics

### Description

Panneau centralisé de tous les diagnostics (erreurs, avertissements, infos) provenant des compilateurs, linters, analyseurs statiques, et de l'IA.

### Interface

#### Header

```
PROBLÈMES   ❌ 3 Erreurs  ⚠ 12 Avertissements  ℹ 5 Infos    [🔍 Filtre]  [Fichier courant ☑]
```

#### Liste des Problèmes

```
┌──────────────────────────────────────────────────────────────────────┐
│  ▼ renderer.cpp  (2 erreurs, 3 avertissements)                      │
│  ❌ L.145 C.12  use of undeclared identifier 'VkBuffer2'  [clang]   │
│  ❌ L.212 C.5   expected ';' after return statement       [clang]   │
│  ⚠  L.89  C.3   unused variable 'result'                 [clang]   │
│  ⚠  L.134 C.8   implicit conversion loses precision      [clang]   │
│  ▼ shader.glsl  (1 avertissement)                                   │
│  ⚠  L.22  C.1   texture sampler unused                   [glsl]    │
│  ▼ IA  (3 suggestions)                                              │
│  💡 renderer.cpp L.145  Fuite mémoire possible            [AI]      │
│  💡 scene.cpp L.89  Peut être optimisé avec std::move     [AI]      │
└──────────────────────────────────────────────────────────────────────┘
```

- Clic sur un problème → naviguer vers la ligne dans l'éditeur
- Clic droit → **Quick Fix**, **Ignorer ce diagnostic**, **Ignorer ce type**, **Copier**
- Filtre par : fichier courant, workspace, type (erreur/avertissement/info/IA)
- Tri par : fichier, sévérité, position

---

## 13. Recherche Globale (Search & Replace)

### Description

Recherche et remplacement dans l'ensemble du projet (ou un sous-ensemble), avec support des expressions régulières, des filtres de fichiers, et des previews inline.

### Interface

#### Panneau de Recherche

```
┌─────────────────────────────────────────────────────┐
│  RECHERCHE                                         │
├─────────────────────────────────────────────────────┤
│  🔍 [drawMesh                           ] [Aa] [.*] │
│  🔄 [drawMeshInstanced                  ] [☑ Préserver casse] │
│  📁 [src/**/*.cpp, src/**/*.h           ]           │
│  🚫 [build/**, node_modules/**          ]           │
├─────────────────────────────────────────────────────┤
│  [Tout remplacer]  [Prévisualiser]                  │
├─────────────────────────────────────────────────────┤
│  42 résultats dans 8 fichiers                       │
│                                                     │
│  ▼ renderer.cpp  (15 occurrences)                  │
│    L.89:   void Renderer::drawMesh(Mesh* m) {      │
│    L.145:      drawMesh(currentMesh, shader);      │
│    L.201:  // Appel drawMesh optimisé              │
│  ▼ scene.cpp  (7 occurrences)                      │
│    L.45:   renderer.drawMesh(mesh, mat);           │
└─────────────────────────────────────────────────────┘
```

Options :
- `Aa` → Sensible à la casse
- `.*` → Expressions régulières
- `[abc]` → Correspondance de mot entier
- Filtre d'inclusion : globs (ex: `**/*.cpp`)
- Filtre d'exclusion : globs

#### Raccourcis Recherche

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+F`
|
 Ouvrir la recherche globale 
|
|
`Ctrl+Shift+H`
|
 Ouvrir la recherche & remplacement global 
|
|
`F4`
|
 Résultat suivant 
|
|
`Shift+F4`
|
 Résultat précédent 
|
|
`Ctrl+Enter`
|
 Ouvrir tous les résultats 
|
|
`Ctrl+Alt+Enter`
|
 Tout remplacer 
|
|
`Escape`
|
 Fermer la recherche 
|

---

## 14. Gestionnaire de Tâches & Build System

### Description

Interface de configuration et d'exécution des tâches de build, de test, et de déploiement (CMake, Make, Gradle, npm, cargo, etc.).

### Interface Build Panel

#### Panneau Tâches

```
┌─────────────────────────────────────────────────────┐
│  BUILD & TÂCHES                                    │
├─────────────────────────────────────────────────────┤
│  Configuration: [Debug ▼]  Platform: [x64 ▼]       │
│  [🔨 Build]  [🧹 Clean]  [🔨 Rebuild]  [▶ Run]    │
├─────────────────────────────────────────────────────┤
│  ▼ Tâches Définies                                  │
│  ▶ Build Debug (CMake)        `cmake --build .`    │
│  ▶ Build Release (CMake)      `cmake --build . -R` │
│  ▶ Run Tests                  `ctest --verbose`    │
│  ▶ Format Code                `clang-format -i`    │
│  ▶ Generate Docs              `doxygen`            │
│  ▶ Package                    `cpack`              │
│  [+ Nouvelle tâche]  [⚙ tasks.json]               │
├─────────────────────────────────────────────────────┤
│  Progression: ████████████░░░░░  75%               │
│  [12/16] Compilation renderer.cpp...               │
│  Temps écoulé: 00:45  Estimé restant: 00:15        │
└─────────────────────────────────────────────────────┘
```

#### Sortie du Build

```
┌─────────────────────────────────────────────────────┐
│  SORTIE (Build)                [🔍] [🗑] [⟳]       │
├─────────────────────────────────────────────────────┤
│  [00:00:01] Démarrage build Debug...               │
│  [00:00:02] -- Configuring done                    │
│  [00:00:05] [  6%] Building CXX object renderer.o  │
│  [00:00:12] [12%] Building CXX object scene.o      │
│  ⚠ renderer.cpp:89:3: warning: unused variable     │
│  [00:00:45] [100%] Linking CXX executable game     │
│  ✅ Build succeeded in 45.3s                       │
└─────────────────────────────────────────────────────┘
```

#### Raccourcis Build

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+B`
|
 Build (tâche par défaut) 
|
|
`Ctrl+Shift+Alt+B`
|
 Sélectionner la tâche de build 
|
|
`Ctrl+F7`
|
 Compiler le fichier courant uniquement 
|
|
`F7`
|
 Build tout le projet 
|
|
`Ctrl+Shift+Alt+C`
|
 Clean 
|
|
`Ctrl+Shift+Alt+R`
|
 Rebuild 
|
|
`Ctrl+F5`
|
 Run sans débogage 
|
|
`F5`
|
 Run avec débogage 
|
|
`Escape`
|
 Annuler le build en cours 
|

---

## 15. Profiler & Analyseur de Performance

### Description

Outil de profiling intégré pour analyser les performances CPU et GPU, détecter les goulots d'étranglement, et visualiser l'utilisation mémoire.

### Interface Profiler

#### Vue Timeline Profiler

```
┌────────────────────────────────────────────────────────────────────────┐
│  PROFILER         [▶ Enregistrer]  [⏸ Pause]  [💾 Sauvegarder]       │
├──────────────┬─────────────────────────────────────────────────────────┤
│  CPU          │ ████░░░░░░░░░░░░░░░░░░  Frame: 16.67ms                │
│  Main Thread  │ [Update 2.1ms][Render 8.3ms][Physics 4.2ms][░░░░2.1ms]│
│  Render Thread│ [Submit 1.2ms][████████DrawCalls 6.8ms░░░░░░]         │
│  GPU          │ [░░░░Vertex 3.2ms░░][░░░░░░Fragment 9.8ms░░░░░░]      │
│  Memory       │ RAM: 1.2GB / 16GB   VRAM: 512MB / 8GB                │
├──────────────┴─────────────────────────────────────────────────────────┤
│  Zoom: [────────────────────●────────────────────]                     │
└────────────────────────────────────────────────────────────────────────┘
```

#### Vue Flame Graph

```
┌────────────────────────────────────────────────────────────────────────┐
│  FLAME GRAPH  (CPU Main Thread, Frame 1042)                           │
├────────────────────────────────────────────────────────────────────────┤
│  GameLoop::tick()                                         16.67ms 100%│
│  ├─ Scene::update()                                        2.10ms  13%│
│  │  ├─ PhysicsWorld::step()                               1.80ms  11%│
│  │  └─ ScriptSystem::update()                             0.30ms   2%│
│  ├─ Renderer::render()                                     8.30ms  50%│
│  │  ├─ CullingSystem::cull()                              0.50ms   3%│
│  │  ├─ Renderer::sortDrawCalls()                          0.20ms   1%│
│  │  └─ Renderer::submitDrawCalls()        ████████████    7.60ms  46%│ ← Goulot!
│  └─ AudioSystem::update()                                 0.10ms   1%│
└────────────────────────────────────────────────────────────────────────┘
```

- Clic sur une barre → zoom dans cette fonction
- Double clic → aller au code source
- Couleurs : rouge = critique, orange = élevé, vert = normal, bleu = attente

#### Vue Mémoire

```
┌─────────────────────────────────────────────────────┐
│  MÉMOIRE                    Snapshot: [Prendre]     │
├─────────────────────────────────────────────────────┤
│  Heap: 450MB  Stack: 2MB  Mapped: 800MB            │
├─────────────────────────────────────────────────────┤
│  Top Allocateurs:                                   │
│  TextureManager        245MB  █████████░░░░  55%   │
│  MeshBuffer            120MB  ████░░░░░░░░░  27%   │
│  PhysicsWorld           45MB  █░░░░░░░░░░░░  10%   │
│  AudioBuffer            20MB  ░░░░░░░░░░░░░   4%   │
├─────────────────────────────────────────────────────┤
│  Fuites détectées: 3                               │
│  ⚠ TextureCache: 12MB non libérés                 │
│  [🔍 Localiser]                                    │
└─────────────────────────────────────────────────────┘
```

---

## 16. Éditeur de Paramètres & Préférences

### Description

Interface de configuration globale de l'IDE, similaire aux Settings de VS Code avec une vue JSON et une vue graphique.

### Interface Settings

```
┌──────────────────────────────────────────────────────────────────────┐
│  PARAMÈTRES                               [🔍 Chercher...    ] [{}] │
├────────────────┬─────────────────────────────────────────────────────┤
│  ▼ 🖥 Éditeur  │  Éditeur › Taille de police                        │
│    Police      │  [14               ]                               │
│    Indentation │  ─────────────────────────────────────────────     │
│    Curseur     │  Éditeur › Famille de police                       │
│    Minimap     │  [JetBrains Mono, Fira Code, Consolas  ]          │
│  ▼ 🎨 Thème   │  ─────────────────────────────────────────────     │
│  ▼ ⌨ Keymap   │  Éditeur › Tabulation                              │
│  ▼ 🤖 IA      │  Taille: [4  ]  [☑] Insérer des espaces           │
│  ▼ 🐛 Débogage│  ─────────────────────────────────────────────     │
│  ▼ 🔀 Git     │  Éditeur › Formatage automatique                   │
│  ▼ 🔌 Moteur  │  [☑] Formater à la sauvegarde                     │
│  ▼ 🌐 Collab  │  [☑] Formater au collage                          │
│  ▼ 🏗 Build   │  ─────────────────────────────────────────────     │
│  ▼ ⚙ Système  │  Éditeur › Suggestions inline IA                  │
└────────────────┤  [☑] Activer les suggestions inline              │
                 │  Délai: [300ms  ]                                │
                 │  Modèle: [claude-sonnet-4-6 ▼]                  │
                 └─────────────────────────────────────────────────┘
```

- Vue `{}` JSON → éditer directement le fichier de configuration
- Scopes : Utilisateur (global), Workspace (projet courant), Dossier
- Badges **Modifié** sur les paramètres non-par-défaut
- Lien **Réinitialiser** sur chaque paramètre modifié
- **Sync** : synchronisation des paramètres via compte en ligne

---

## 17. Vue Split & Multi-Éditeurs

### Description

Système de division de l'éditeur pour travailler sur plusieurs fichiers simultanément.

### Modes Split

#### Split Vertical (par défaut, `Ctrl+\`)

```
┌──────────────────────┬──────────────────────┐
│  renderer.cpp        │  renderer.h          │
│  145: drawMesh() {   │  42: void drawMesh(  │
│  146:   bind(mesh);  │       Mesh* m,       │
│  147:   draw(shad);  │       Shader* s);    │
└──────────────────────┴──────────────────────┘
```

#### Split Horizontal (`Ctrl+K Ctrl+\`)

```
┌──────────────────────────────────────────────┐
│  main.cpp                                    │
│  23: gameLoop.run();                         │
├──────────────────────────────────────────────┤
│  gameloop.cpp                                │
│  62: scene.render();                         │
└──────────────────────────────────────────────┘
```

#### Grille 2x2 (`Ctrl+K Ctrl+G`)

```
┌───────────────────┬───────────────────┐
│  renderer.cpp     │  renderer.h       │
├───────────────────┼───────────────────┤
│  shader.glsl      │  scene.cpp        │
└───────────────────┴───────────────────┘
```

#### Diff Editor (`Ctrl+K D`)

Vue comparaison côte à côte de deux fichiers ou deux versions du même fichier.

#### Raccourcis Split

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+\`
|
 Split vertical 
|
|
`Ctrl+K Ctrl+\`
|
 Split horizontal 
|
|
`Ctrl+K Ctrl+G`
|
 Grille 2x2 
|
|
`Ctrl+1/2/3/4`
|
 Mettre le focus sur la zone n° 
|
|
`Ctrl+K Ctrl+←/→`
|
 Déplacer l'éditeur courant vers la zone précédente/suivante 
|
|
`Ctrl+K ←/→`
|
 Changer de zone active 
|
|
`Ctrl+K W`
|
 Fermer tous les éditeurs de la zone 
|
|
`Ctrl+K M`
|
 Maximiser la zone courante 
|

---

## 18. Palette de Commandes (Command Palette)

### Description

Accès universel à toutes les commandes de l'IDE, similaire à `Ctrl+Shift+P` de VS Code / `Ctrl+Shift+A` de JetBrains.

### Interface

```
┌────────────────────────────────────────────────────────────┐
│  > format█                                        Ctrl+P   │
├────────────────────────────────────────────────────────────┤
│  📄  Formater le document               Alt+Shift+F        │
│  📄  Formater la sélection             Ctrl+K Ctrl+F       │
│  📄  Formater à la sauvegarde (Toggle)                     │
│  ⚙   Ouvrir la configuration Prettier                     │
│  🤖  IA: Formater avec explication                        │
│  ──────────────────────────────────────────────────────    │
│  📄  Récents: renderer.cpp                                 │
│  📄  Récents: shader.glsl                                  │
└────────────────────────────────────────────────────────────┘
```

#### Préfixes de la Palette

|
 Préfixe 
|
 Mode 
|
|
---------
|
------
|
|
 (vide) 
|
 Ouvrir un fichier (Quick Open) 
|
|
`>`
|
 Commande 
|
|
`@`
|
 Aller à un symbole dans le fichier 
|
|
`#`
|
 Aller à un symbole dans le projet 
|
|
`:`
|
 Aller à la ligne n° 
|
|
`?`
|
 Aide (liste des préfixes) 
|
|
`!`
|
 Afficher les tâches de build 
|
|
`AI:`
|
 Commande IA 
|
|
`git:`
|
 Commande Git 
|
|
`ext:`
|
 Commande d'extension 
|

---

## 19. Status Bar (Barre de Statut)

### Description

Barre d'information en bas de la fenêtre, cliquable pour accéder aux fonctionnalités correspondantes.

### Structure

```
[ 🌿 main  ↓0 ↑2 ]  [ ❌ 3  ⚠ 12 ]  [  Ln 145, Col 12  ]  [ UTF-8 ]  [ LF ]  [ C++ ]  [ clang-format ]  [ 🤖 IA: Actif ]  [ 🟢 Moteur ]  [ 🌐 Collab: 2 ]
```

|
 Élément 
|
 Clic 
|
 Raccourci 
|
|
---------
|
------
|
-----------
|
|
`🌿 main ↓0 ↑2`
|
 Checkout / branches 
|
`Ctrl+Shift+G`
|
|
`❌ 3 ⚠ 12`
|
 Ouvrir le panneau Problèmes 
|
`Ctrl+Shift+M`
|
|
`Ln 145, Col 12`
|
 Aller à la ligne 
|
`Ctrl+G`
|
|
`UTF-8`
|
 Changer l'encodage 
|
 — 
|
|
`LF`
|
 Changer les fins de ligne (LF/CRLF/CR) 
|
 — 
|
|
`C++`
|
 Changer le langage du fichier 
|
 — 
|
|
`clang-format`
|
 Changer le formateur 
|
 — 
|
|
`🤖 IA: Actif`
|
 Activer/désactiver l'IA 
|
`Ctrl+Shift+Alt+A`
|
|
`🟢 Moteur`
|
 Ouvrir Engine Bridge 
|
`Ctrl+Shift+M`
|
|
`🌐 Collab: 2`
|
 Ouvrir Live Collab 
|
`Ctrl+Shift+L`
|

Couleurs de la status bar :
- **Bleu** → mode normal
- **Orange** → mode débogage actif
- **Violet** → mode Live Collab actif
- **Rouge** → erreur critique

---

## 20. Barre de Menus Principale

### FILE

- **New File** `Ctrl+N`
- **New Folder** `Ctrl+Shift+N`
- **New Project** — (templates : C++/CMake, Python, Rust, Game, Simulation, Vide)
- **Open File** `Ctrl+O`
- **Open Folder** `Ctrl+K Ctrl+O`
- **Open Workspace** — ouvrir un fichier `.workspace`
- **Open Recent** — derniers fichiers / projets
- **Save** `Ctrl+S`
- **Save As** `Ctrl+Shift+S`
- **Save All** `Ctrl+K S`
- **Auto Save** — (Off, After Delay, On Focus Change, On Window Change)
- **Revert File** — annuler toutes les modifications depuis la dernière sauvegarde
- **Share** — partager le projet (Live Collab, GitHub Gist)
- **Export** — exporter le projet (ZIP, tarball)
- **Preferences** `Ctrl+,`
- **Quit** `Ctrl+Q`

### EDIT

- **Undo** `Ctrl+Z`
- **Redo** `Ctrl+Shift+Z`
- **Cut** `Ctrl+X`
- **Copy** `Ctrl+C`
- **Paste** `Ctrl+V`
- **Paste Special** → (Coller sans formatage, Coller comme texte brut)
- **Find** `Ctrl+F`
- **Find Next** `F3`
- **Find Previous** `Shift+F3`
- **Replace** `Ctrl+H`
- **Find in Files** `Ctrl+Shift+F`
- **Replace in Files** `Ctrl+Shift+H`
- **Select All** `Ctrl+A`
- **Expand Selection** `Shift+Alt+→`
- **Shrink Selection** `Shift+Alt+←`
- **Add Cursor Above** `Ctrl+Alt+↑`
- **Add Cursor Below** `Ctrl+Alt+↓`
- **Column Selection** `Shift+Alt+Drag`
- **Toggle Line Comment** `Ctrl+/`
- **Toggle Block Comment** `Shift+Alt+A`
- **Format Document** `Alt+Shift+F`
- **Format Selection** `Ctrl+K Ctrl+F`
- **Fold All** `Ctrl+K Ctrl+0`
- **Unfold All** `Ctrl+K Ctrl+J`
- **Snippets** → gérer les snippets personnalisés

### VIEW

- **Command Palette** `Ctrl+Shift+P`
- **Open View** → ouvrir n'importe quel panneau
- **Appearance** → Thème, Thème d'icônes, Zoom (+/-), Plein écran
- **Editor Layout** → Single, 2 colonnes, 3 colonnes, 2 lignes, Grille, Flip, Même groupe
- **Minimap** → Activer/Désactiver
- **Breadcrumbs** → Fil d'Ariane (chemin du symbole courant)
- **Word Wrap** `Alt+Z`
- **Whitespace** → afficher les espaces/tabulations
- **Indentation Guides**
- **Activity Bar** → afficher/masquer
- **Status Bar** → afficher/masquer
- **Panel** → afficher/masquer le panneau inférieur
- **Sidebar** → afficher/masquer le panneau latéral
- **Zen Mode** `Ctrl+K Z` → masquer tout sauf l'éditeur
- **Centered Layout** → centrer l'éditeur

### GO

- **Back** `Alt+←`
- **Forward** `Alt+→`
- **Go to File** `Ctrl+P`
- **Go to Symbol in File** `Ctrl+Shift+O`
- **Go to Symbol in Workspace** `Ctrl+T`
- **Go to Definition** `F12`
- **Go to Declaration**
- **Go to Type Definition**
- **Go to Implementation** `Ctrl+F12`
- **Go to References** `Shift+F12`
- **Go to Line/Column** `Ctrl+G`
- **Go to Bracket** `Ctrl+Shift+\`
- **Next Problem** `F8`
- **Previous Problem** `Shift+F8`
- **Next Change** (Git) `Alt+F3`
- **Previous Change** (Git) `Shift+Alt+F3`

### RUN

- **Run** `Ctrl+F5`
- **Start Debugging** `F5`
- **Stop** `Shift+F5`
- **Restart** `Ctrl+Shift+F5`
- **Step Over** `F10`
- **Step Into** `F11`
- **Step Out** `Shift+F11`
- **Continue** `F5`
- **Toggle Breakpoint** `F9`
- **New Breakpoint** → Conditionnel, Logpoint, Compteur de hits, Exception
- **Enable All Breakpoints**
- **Disable All Breakpoints**
- **Remove All Breakpoints**
- **Hot Reload** `Ctrl+Shift+H`
- **Run Task** → lancer une tâche définie
- **Run Build Task** `Ctrl+Shift+B`
- **Run Test** `Ctrl+Shift+T`

### DEBUG

- **Attach to Process** — déboguer un processus existant par PID
- **Configurations** — gérer les configurations de débogage (launch.json)
- **Open Debug Console**
- **Show Call Stack**
- **Show Variables**
- **Show Watch**
- **Show Breakpoints**
- **Show Threads**
- **GPU Debugger** — ouvrir le débogueur GPU

### GIT

- **Initialize Repository**
- **Clone Repository**
- **Commit** `Ctrl+K Ctrl+C`
- **Push** `Ctrl+K Ctrl+P`
- **Pull** `Ctrl+K Ctrl+L`
- **Fetch** `Ctrl+K Ctrl+F`
- **Sync** (Push + Pull)
- **Stage All Changes**
- **Unstage All**
- **Discard All Changes**
- **Create Branch**
- **Switch Branch**
- **Merge Branch**
- **Rebase Branch**
- **Stash Changes**
- **Apply Stash**
- **View History**
- **View Blame**
- **View Diff**
- **Pull Request** → créer/gérer les PR (GitHub, GitLab, Bitbucket)

### AI

- **Toggle AI Assistant** `Ctrl+Shift+A`
- **AI Chat** → ouvrir le chat IA
- **Explain Selection** `Ctrl+Alt+/`
- **Fix Selection** `Ctrl+Alt+F`
- **Generate Code** `Ctrl+Shift+I`
- **Generate Tests** `Ctrl+Alt+T`
- **Generate Documentation** `Ctrl+Alt+D`
- **Optimize Selection** `Ctrl+Alt+O`
- **Code Review** `Ctrl+Alt+R`
- **Generate Commit Message** `Ctrl+K Ctrl+M`
- **AI Terminal** `Ctrl+K`
- **AI Settings** → modèle, API, confidentialité
- **Training Custom Model** → adapter le modèle sur votre codebase

### TOOLS

- **Extensions** `Ctrl+Shift+X`
- **Package Manager** → gérer les dépendances (vcpkg, conan, npm, pip, cargo…)
- **Snippets** → gérer les snippets
- **Tasks** → gestionnaire de tâches
- **Keybindings** `Ctrl+K Ctrl+S`
- **Color Theme** `Ctrl+K Ctrl+T`
- **Icon Theme**
- **Profiler** → ouvrir le profiler
- **Memory Analyzer**
- **Network Inspector** → inspecter les requêtes réseau de l'app
- **Regex Tester** → outil de test de regex inline
- **JSON Viewer** → visualiser/formater du JSON
- **Diff Tool** → comparer deux fichiers quelconques
- **Checksum** → calculer le checksum d'un fichier
- **Developer Tools** → ouvrir les devtools de l'IDE lui-même

### WINDOW

- **New Window** `Ctrl+Shift+N`
- **Close Window** `Ctrl+Shift+W`
- **Close All Editors**
- **Close Folder**
- **Split Editor** `Ctrl+\`
- **Split Editor Down** `Ctrl+K Ctrl+\`
- **Toggle Panel** `Ctrl+J`
- **Toggle Sidebar** `Ctrl+B`
- **Toggle Activity Bar**
- **Switch to Editor n°** `Ctrl+1-9`
- **Layouts** — sauvegarder / restaurer des layouts
- **Move Window to Screen** → multi-écrans

### HELP

- **Documentation** `F1`
- **Keyboard Shortcuts Reference** `Ctrl+K Ctrl+R`
- **Interactive Tutorial**
- **Release Notes**
- **Community Forum**
- **Report Issue**
- **Check for Updates**
- **Toggle Developer Tools** `Ctrl+Shift+I`
- **About**

---

## 21. Workspaces & Layouts

### Description

Layouts prédéfinis pour les différents contextes de travail, commutables instantanément.

### Layouts Disponibles

|
 Layout 
|
 Raccourci 
|
 Description 
|
|
--------
|
-----------
|
-------------
|
|
**
Code
**
|
`Ctrl+K 1`
|
 Éditeur + Explorateur + Terminal. Concentration maximale sur le code. 
|
|
**
Debug
**
|
`Ctrl+K 2`
|
 Éditeur + Call Stack + Variables + Watch + Terminal débogage. 
|
|
**
Git
**
|
`Ctrl+K 3`
|
 Diff Viewer + Historique + Branches + Terminal. 
|
|
**
AI
**
|
`Ctrl+K 4`
|
 Éditeur + Chat IA grand format + Outline. 
|
|
**
Build
**
|
`Ctrl+K 5`
|
 Éditeur + Sortie build + Tâches + Terminal. 
|
|
**
Profiler
**
|
`Ctrl+K 6`
|
 Flame Graph + Timeline + Mémoire + Éditeur. 
|
|
**
Engine
**
|
`Ctrl+K 7`
|
 Éditeur + Engine Bridge + Assets moteur + Logs moteur. 
|
|
**
Collab
**
|
`Ctrl+K 8`
|
 Éditeur + Chat Collab + Participants + Commentaires. 
|
|
**
Zen
**
|
`Ctrl+K Z`
|
 Éditeur seul, plein écran, sans distractions. 
|
|
**
Custom
**
|
 — 
|
 Layout sauvegardé par l'utilisateur. 
|

### Gestion des Workspaces (Projets Multi-Racines)

Un workspace (`.code-workspace`) regroupe plusieurs dossiers racines :

```json
{
  "folders": [
    { "path": "/projects/mon-moteur", "name": "Moteur" },
    { "path": "/projects/mon-jeu",    "name": "Jeu" },
    { "path": "/libs/math-lib",       "name": "Math Lib" }
  ],
  "settings": {
    "editor.fontSize": 14
  }
}
```

Chaque racine apparaît distinctement dans l'Explorateur avec sa couleur propre.

---

## 22. Raccourcis Clavier Globaux

### Navigation Générale

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+P`
|
 Ouvrir rapidement un fichier 
|
|
`Ctrl+Shift+P`
|
 Palette de commandes 
|
|
`Ctrl+,`
|
 Paramètres 
|
|
`Ctrl+K Ctrl+S`
|
 Raccourcis clavier 
|
|
`Ctrl+B`
|
 Afficher/masquer la sidebar 
|
|
`Ctrl+J`
|
 Afficher/masquer le panneau inférieur 
|
|
`Ctrl+Shift+E`
|
 Explorateur de projet 
|
|
`Ctrl+Shift+F`
|
 Recherche globale 
|
|
`Ctrl+Shift+G`
|
 Git 
|
|
`Ctrl+Shift+D`
|
 Débogage 
|
|
`Ctrl+Shift+X`
|
 Extensions 
|
|
`Ctrl+Shift+A`
|
 AI Assistant 
|
| `Ctrl+`` ` `` | Terminal |
| `Ctrl+Shift+`` ` `` | Nouveau terminal |
| `Ctrl+K Z` | Mode Zen |
| `Ctrl+Shift+Alt+N` | Nouveau projet |
| `F11` | Plein écran |
| `Ctrl+Shift+N` | Nouvelle fenêtre |
| `Alt+←` | Retour (historique de navigation) |
| `Alt+→` | Avant (historique de navigation) |

### Éditeur de Code

(Voir section §2 pour la liste complète)

### Débogage

(Voir section §5 pour la liste complète)

### Git

(Voir section §7 pour la liste complète)

### Build

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+B`
|
 Build 
|
|
`F5`
|
 Run avec débogage 
|
|
`Ctrl+F5`
|
 Run sans débogage 
|
|
`Shift+F5`
|
 Arrêter 
|
|
`Ctrl+Shift+F5`
|
 Redémarrer 
|

### IA

|
 Raccourci 
|
 Action 
|
|
-----------
|
--------
|
|
`Ctrl+Shift+A`
|
 Chat IA 
|
|
`Ctrl+I`
|
 IA inline (sélection) 
|
|
`Ctrl+Alt+/`
|
 Expliquer 
|
|
`Ctrl+Alt+F`
|
 Fixer 
|
|
`Ctrl+Alt+T`
|
 Générer tests 
|
|
`Ctrl+Alt+D`
|
 Documenter 
|
|
`Ctrl+Alt+O`
|
 Optimiser 
|
|
`Tab`
|
 Accepter suggestion inline 
|
|
`Escape`
|
 Rejeter suggestion 
|

---

## 23. Actions Souris Globales

### Éditeur de Code

|
 Action Souris 
|
 Résultat 
|
|
---------------
|
----------
|
|
 Clic gauche 
|
 Placer le curseur 
|
|
 Double clic 
|
 Sélectionner le mot 
|
|
 Triple clic 
|
 Sélectionner la ligne 
|
|
 Quadruple clic 
|
 Sélectionner tout 
|
|
 Clic gauche + drag 
|
 Sélectionner le texte 
|
|
 Alt + clic 
|
 Ajouter un curseur 
|
|
 Alt + drag 
|
 Sélection en colonne (box selection) 
|
|
 Ctrl + clic 
|
 Aller à la définition du symbole 
|
|
 Ctrl + drag 
|
 Copier (et non déplacer) 
|
|
 Clic sur numéro de ligne 
|
 Sélectionner la ligne 
|
|
 Clic sur gouttière 
|
 Ajouter / supprimer un breakpoint 
|
|
 Clic droit sur gouttière 
|
 Options de breakpoint 
|
|
 Survol d'un symbole 
|
 Documentation hover (après délai) 
|
|
 Clic molette sur un onglet 
|
 Fermer l'onglet 
|
|
 Drag d'un onglet 
|
 Réordonner / déplacer vers split 
|
|
 Molette 
|
 Scroller verticalement 
|
|
 Ctrl + molette 
|
 Changer la taille de la police 
|
|
 Shift + molette 
|
 Scroller horizontalement 
|
|
 Clic sur la scrollbar 
|
 Naviguer (position absolue) 
|
|
 Drag sur la minimap 
|
 Naviguer dans le fichier 
|
|
 Clic droit 
|
 Menu contextuel 
|
|
 Clic droit sur l'onglet 
|
 Options de l'onglet 
|

### Explorateur de Projet

|
 Action Souris 
|
 Résultat 
|
|
---------------
|
----------
|
|
 Clic gauche 
|
 Sélectionner 
|
|
 Double clic 
|
 Ouvrir le fichier 
|
|
 Clic droit 
|
 Menu contextuel 
|
|
 Drag 
|
 Déplacer le fichier / Réordonner 
|
|
 Ctrl + clic 
|
 Sélection multiple 
|
|
 Shift + clic 
|
 Sélection de plage 
|
|
 Alt + clic 
|
 Ouvrir dans un split 
|
|
 Clic sur 
`▶`
|
 Déplier / replier 
|

### Panneau Inférieur (Terminal, Sortie, etc.)

|
 Action Souris 
|
 Résultat 
|
|
---------------
|
----------
|
|
 Clic sur lien fichier 
|
 Ouvrir le fichier à la ligne 
|
|
 Ctrl + clic sur URL 
|
 Ouvrir dans le navigateur 
|
|
 Drag de la bordure supérieure 
|
 Redimensionner le panneau 
|
|
 Double clic sur la bordure 
|
 Hauteur par défaut 
|

### Débogueur

|
 Action Souris 
|
 Résultat 
|
|
---------------
|
----------
|
|
 Clic sur variable 
|
 Sélectionner 
|
|
 Double clic sur valeur 
|
 Modifier la valeur 
|
|
 Clic droit sur variable 
|
 Options (Watch, Copier, Visualiser) 
|
|
 Clic sur frame de la call stack 
|
 Naviguer vers ce contexte 
|
|
 Drag d'un breakpoint 
|
 Déplacer le breakpoint 
|
|
 Clic droit sur breakpoint 
|
 Options (Conditionnel, Désactiver) 
|

---

## 24. Fenêtres Modales & Dialogues

### Dialogue Quick Open (`Ctrl+P`)

```
┌───────────────────────────────────────────────────────┐
│  renderer█                                            │
├───────────────────────────────────────────────────────┤
│  📄  renderer.cpp            src/core/              │
│  📄  renderer.h              src/core/              │
│  📄  renderer_vulkan.cpp     src/backends/          │
│  📄  renderer_test.cpp       tests/                 │
└───────────────────────────────────────────────────────┘
```

### Dialogue New Project

```
┌───────────────────────────────────────────────────────┐
│  NOUVEAU PROJET                                      │
├───────────────────────────────────────────────────────┤
│  Template:                                           │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐       │
│  │ C++    │ │ Python │ │  Rust  │ │  Game  │       │
│  │ CMake  │ │ Flask  │ │ Cargo  │ │ C++/GL │       │
│  └────────┘ └────────┘ └────────┘ └────────┘       │
│  ┌────────┐ ┌────────┐ ┌────────┐                   │
│  │  Web   │ │  Data  │ │ Vide   │                   │
│  │ TS/React│ │ Jupyter│ │        │                   │
│  └────────┘ └────────┘ └────────┘                   │
├───────────────────────────────────────────────────────┤
│  Nom:         [MonProjet                ]            │
│  Emplacement: [/home/user/projects/     ] [📂]       │
│  VCS:         [Git ▼]  [☑ Init commit  ]            │
│  [☑] Ouvrir dans la fenêtre courante               │
├───────────────────────────────────────────────────────┤
│  [Annuler]                         [Créer le projet] │
└───────────────────────────────────────────────────────┘
```

### Dialogue Keybindings (`Ctrl+K Ctrl+S`)

```
┌───────────────────────────────────────────────────────────────────┐
│  RACCOURCIS CLAVIER         [🔍 Chercher...        ]  [{}]       │
├───────────────────────────────────────────────────────────────────┤
│  Commande                  Raccourci    Quand                    │
│  Aller à la définition     F12          editorHasDefinition      │
│  Trouver les références    Shift+F12    editorHasReference       │
│  Formater le document      Alt+Shift+F  editorTextFocus          │
│  Build                     Ctrl+Shft+B  —                        │
│  ─────────────────────────────────────────────────────────────   │
│  [+ Ajouter un raccourci]  [Réinitialiser tout]                 │
└───────────────────────────────────────────────────────────────────┘
```

Double clic sur un raccourci → enregistrer une nouvelle combinaison.

### Panel "Adjust Last Action" (après une opération)

```
┌──────────────────────────────────┐
│  Renommer le symbole             │
├──────────────────────────────────┤
│  Ancien: drawMesh                │
│  Nouveau: [drawMeshInstanced   ] │
│  Scope: [☑] Tout le projet      │
│         [☐] Fichier courant     │
│  [☑] Prévisualiser les changes  │
├──────────────────────────────────┤
│  [Annuler]          [Appliquer] │
└──────────────────────────────────┘
```

### Dialogue "Konflit de Merge" (Git)

```
┌────────────────────────────────────────────────────────────────┐
│  ⚠ CONFLIT DE MERGE : renderer.cpp                           │
├────────────────────────────────────────────────────────────────┤
│  [✅ Accepter Entrant]  [✅ Accepter Actuel]  [✅ Les deux]   │
│  [📝 Éditer manuellement]  [🤖 Résoudre avec l'IA]           │
├──────────────────────┬─────────────────────────────────────────┤
│  <<<< HEAD (Actuel)  │  >>>> feature/vulkan (Entrant)         │
│  void drawMesh() {   │  void drawMesh(Shader* s) {            │
│    bind(mesh);       │    validateShader(s);                  │
│    draw();           │    bind(mesh, s);                      │
│  }                   │    draw();                             │
│                      │  }                                     │
└──────────────────────┴─────────────────────────────────────────┘
```

---

## 25. Système de Notifications & Toasts

### Toasts (Notifications Temporaires)

Apparaissent en bas à droite, disparaissent après 4s (ou au clic) :

```
┌──────────────────────────────────────────┐
│  ✅ Build réussi en 12.4s  [Voir sortie] │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│  ❌ 3 erreurs de compilation  [Voir]     │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│  🤖 Suggestion IA disponible  [Voir]    │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│  🔥 Hot Reload appliqué (pbr.frag)      │
└──────────────────────────────────────────┘

┌──────────────────────────────────────────┐
│  🌐 Alice a rejoint la session Collab   │
└──────────────────────────────────────────┘
```

### Centre de Notifications

Clic sur l'icône 🔔 en bas à droite → liste de toutes les notifications :
- Filtre : Toutes, Erreurs, Build, IA, Collab, Moteur
- Marquer comme lue, supprimer, configurer les notifications

---

## 26. Thèmes & Personnalisation Visuelle

### Thèmes Inclus

|
 Thème 
|
 Style 
|
 Fond 
|
|
-------
|
-------
|
------
|
|
**
Dark Pro
**
|
 Sombre professionnel 
|
#
1e1e1e 
|
|
**
One Dark
**
|
 Atom One Dark 
|
#
282c34 
|
|
**
Dracula
**
|
 Violet/Cyan 
|
#
282a36 
|
|
**
Monokai Pro
**
|
 Coloré, contrasté 
|
#
2d2a2e 
|
|
**
GitHub Dark
**
|
 GitHub officiel 
|
#
0d1117 
|
|
**
GitHub Light
**
|
 Clair/GitHub 
|
#
ffffff 
|
|
**
Solarized Dark
**
|
 Tons chauds sombres 
|
#
002b36 
|
|
**
Solarized Light
**
|
 Tons chauds clairs 
|
#
fdf6e3 
|
|
**
High Contrast
**
|
 Accessibilité 
|
#
000000 
|
|
**
Custom
**
|
 Configuré par l'utilisateur 
|
 — 
|

### Personnalisation des Couleurs (`settings.json`)

```json
"workbench.colorCustomizations": {
  "editor.background": "#1a1a2e",
  "editor.selectionBackground": "#00d4ff33",
  "activityBar.background": "#16213e",
  "sideBar.background": "#0f3460",
  "statusBar.background": "#e94560"
}
```

### Polices Recommandées

- **JetBrains Mono** — ligatures, excellent pour IDE (recommandée)
- **Fira Code** — ligatures populaires
- **Cascadia Code** — Microsoft, bonne lisibilité
- **Monaspace** — GitHub, plusieurs variantes
- **Iosevka** — très fine et dense

### Icônes de Fichiers

Jeu d'icônes sélectionnable : Material Icons, VS Code Icons, Fluent Icons, Custom.

---

## Annexe A — Fichiers de Configuration

### Structure des Fichiers de Config

```
.ide/
├── settings.json          ← Paramètres du workspace
├── tasks.json             ← Tâches de build
├── launch.json            ← Configurations de débogage
├── extensions.json        ← Extensions recommandées
├── keybindings.json       ← Raccourcis custom
└── snippets/
    ├── cpp.json
    └── glsl.json
```

### Exemple `launch.json` (C++ Débogage)

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug Game (x64)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/game",
      "args": ["--dev-mode"],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [],
      "externalConsole": false,
      "preLaunchTask": "Build Debug",
      "MIMode": "gdb",
      "miDebuggerPath": "/usr/bin/gdb"
    },
    {
      "name": "Attach to Mon Moteur",
      "type": "cppdbg",
      "request": "attach",
      "processId": "${command:pickProcess}"
    }
  ]
}
```

### Exemple `tasks.json`

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build Debug",
      "type": "shell",
      "command": "cmake --build ${workspaceFolder}/build --config Debug",
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": ["$gcc"]
    },
    {
      "label": "Run Tests",
      "type": "shell",
      "command": "ctest --verbose --test-dir ${workspaceFolder}/build",
      "group": "test"
    }
  ]
}
```

---

## Annexe B — Intégration SDK Custom (Protocole)

Pour connecter votre propre moteur ou application graphique à l'IDE :

### Protocole JSON-RPC sur TCP (port 6543 par défaut)

```json
// IDE → Application : Recharger un shader
{
  "jsonrpc": "2.0",
  "method": "hotReload",
  "params": {
    "type": "shader",
    "path": "assets/shaders/pbr.frag",
    "content": "..."
  },
  "id": 1
}

// Application → IDE : Données de profiling
{
  "jsonrpc": "2.0",
  "method": "profilerData",
  "params": {
    "fps": 144,
    "frameTimeMs": 6.9,
    "gpuTimeMs": 4.2,
    "drawCalls": 1042
  }
}

// Application → IDE : Log
{
  "jsonrpc": "2.0",
  "method": "log",
  "params": {
    "level": "error",
    "message": "Shader compile error: line 32",
    "file": "assets/shaders/compute.comp",
    "line": 32
  }
}
```

### SDK Disponibles

- **C++ Header-only** : `#include <ide_bridge.h>` → 3 lignes pour connecter
- **Python** : `pip install my-ide-bridge`
- **Lua** : `require("ide_bridge")`
- **Rust** : `ide_bridge = "0.1"` dans Cargo.toml

---

## Annexe C — Flux de Travail Recommandés

### Développement C++ avec CMake

1. `Ctrl+Shift+N` → Nouveau projet (template C++/CMake)
2. Coder dans l'éditeur avec IntelliSense clang
3. `Ctrl+Shift+B` → Build
4. `F5` → Run avec débogage
5. `F9` sur une ligne → Breakpoint
6. `F10/F11` → Step Over / Into
7. `Ctrl+I` sur une fonction complexe → IA explique / optimise

### Workflow Moteur Custom

1. Lancer l'application depuis l'IDE (`Ctrl+F5`)
2. L'app se connecte automatiquement via SDK
3. Modifier un shader dans l'éditeur
4. `Ctrl+S` → Save → Hot Reload automatique dans l'app
5. Voir les logs de l'app dans Engine Bridge
6. Clic sur une erreur de log → naviguer vers la ligne de code

### Collaboration sur un Bug Critique

1. `Ctrl+Shift+L` → Démarrer une session Collab
2. Partager le lien avec l'équipe
3. `F5` → Démarrer le débogage (partagé)
4. L'équipe voit les breakpoints et les variables en temps réel
5. Chat dans la session pour se coordonner
6. `Ctrl+Alt+R` → Code review IA en parallèle

---

*Document de spécification UI — IDE Généraliste Avancé*
*Version 1.0 — Multi-Langages, IA intégrée, Débogage Visuel, Collaboration, Intégration Moteur*
*Inspiré de Visual Studio, JetBrains, VS Code — avec intégrations custom*

---

# NKCode — État d'implémentation & roadmap (mapping de cette spec)

> Cette spec décrit la **cible** (vision complète). Ci-dessous, la traduction en
> **chantiers concrets** pour NKCode (IDE de Nkentseu), 100 % **NKGui** (rendu
> identique sur toutes les plateformes, zéro widget natif), piloté par **Jenga**.
>
> Principe directeur : on implémente d'abord le **socle d'un IDE C/C++ utilisable**
> (workspaces, projets, build, édition, sortie, terminal), puis on monte vers les
> sections avancées (IA, débogueur visuel, collab, engine bridge) section par section.

Légende : ✅ fait · 🚧 en cours · ⬜ à faire

## Socle déjà en place
- ✅ **Fenêtre principale** (§1, §20) : barre de titre custom, barre de menus, barre
  d'outils de **build** (workspace/projet/system/config/arch/tests), barre d'état,
  barre d'activité, zone dockable.
- ✅ **Éditeur de code** (§2) : multi-onglets, coloration syntaxique, sélection,
  copier/couper/coller, menu contextuel, formatage. (sans IntelliSense/IA/minimap)
- ✅ **Explorateur** (§3) : arbre de fichiers en **pur NKGui**. (sans Git/filtre/DnD)
- ✅ **Terminal intégré** (§4) : shell interactif réel (ConPTY), WSL2 détecté, Unicode.
- ✅ **Sortie/Problèmes** (§12 partiel) : console colorée par niveau, progression.
- ✅ **Palette de commandes** (§18). ✅ **Status bar** (§19). ✅ **Préférences/Thèmes** (§16, §26).
- ✅ **Polices** externes (data/fonts) + fallback Unicode.

## En cours
- 🚧 **Écran de démarrage plein écran** (nouveau, façon « page de démarrage » VS) :
  s'affiche **maximisé, à la place de l'éditeur**, tant qu'aucun workspace n'est
  chargé. Actions Ouvrir un dossier / Ouvrir un workspace / Nouveau workspace +
  **liste des récents** (workspaces déjà ouverts avec l'IDE, persistée). Une fois un
  workspace choisi → bascule vers l'éditeur.

## Roadmap NKCode (ordre indicatif)
| Item | Interface (spec) | Description |
|------|------------------|-------------|
| **#5** | §24, §3 | **Éditeur de Propriétés** projet/workspace (lecture/écriture `.jenga` via UI) |
| **#3** | §14 | **Déploiement** : `jenga package`/`deploy` + installateur `.jng`, par système |
| **#2** | §10/§14 | Combo **Appareil** contextuel (Android/iOS) + détection ADB |
| **#4** | §10 | Panneau **Émulateurs** (créer/lancer/supprimer) |
| **#6** | §3 | Vue **« solution »** dans l'explorateur (workspace → projets → cibles) |
| **#7** | §13 | **Recherche/Remplacement** (fichier + global) |
| **#8** | §12 | Panneau **Problèmes** cliquables (build → fichier:ligne) |
| **#9** | §7 | **Contrôle de version Git** (statut, diff, commit, branches) |
| **#10** | §5 | **Débogueur** intégré (gdb/lldb : breakpoints, pile, variables, mémoire) |
| **#11** | §16, §22 | **Préférences étendues** (éditeur, terminal, **raccourcis**, build) |
| **#12** | §9 | **Paquets/dépendances** (lié aux paquets transitifs Jenga `dependson`) |
| **#13** | §1 | **i18n** de l'interface (FR/EN/中文) |
| **#14** | §25 | **Notifications** + file de tâches |
| **#15** | §17 | Vue **split** / multi-éditeurs |
| **#16** | §6 | **Assistant IA** (chat, génération, revue) — modèle local/cloud |
| **#17** | §10, §11 | **Engine Bridge** Nkentseu + débogage shader/sim live (IPC) |
| **#18** | §8 | **Collaboration** temps réel (live share) |
| **#19** | §15 | **Profiler** & analyseur de performance |
| **#20** | §2 | **IntelliSense** (clangd/LSP), hover, go-to-def, refactor |

> Les sections IA/Débogueur/Collab/Engine Bridge (§5–§11) sont les plus lourdes et
> viennent **après** un IDE C/C++ pleinement utilisable (socle + #2–#9).
