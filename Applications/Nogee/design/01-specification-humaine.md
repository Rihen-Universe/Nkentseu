# Spécification d'interface — Moteur de jeu "Aetherion Engine"
### Document 1/3 — Version lisible par un humain (produit / UX)

> Objectif : décrire, écran par écran, panneau par panneau, l'intégralité de l'interface d'un moteur de jeu de type Unreal Engine 5 : du launcher jusqu'au menu de build, en commençant par le pipeline 3D. Deux thèmes obligatoires : **Light (style GitHub)** et **Dark (style GitHub Dark Pro)**.

---

## 0. Sommaire

1. Principes généraux de design
2. Système de thèmes (Light / Dark)
3. Launcher
4. Coquille de l'éditeur (Editor Shell)
5. Système de docking
6. Viewport 3D
7. World Outliner (hiérarchie de scène)
8. Details Panel (propriétés)
9. Content Browser (miniatures d'assets)
9bis. Asset Browser avancé — Reference Viewer & Size Map
9ter. Project Navigator (arborescence système de fichiers)
9quater. Class Viewer / Blueprint Hierarchy
10. Place Actors / Palette d'objets
11. Output Log / Console
12. Éditeur de matériaux
13. Éditeur de Blueprint (visual scripting)
14. Sequencer / Timeline d'animation
15. World Settings & Project Settings
16. Préférences de l'éditeur
17. Build / Package / Platforms
18. Dialogues, notifications, menus contextuels
19. Glossaire des composants réutilisables

---

## 1. Principes généraux de design

- **Densité d'information élevée** : l'interface est un outil professionnel, pas une app grand public. Les zones cliquables restent petites (24–28px de hauteur), le texte est en 11–13px.
- **Tout est dockable** : chaque panneau (Outliner, Details, Content Browser, Viewport…) peut être détaché, redocké, empilé en onglets, ou mis en fenêtre flottante.
- **Cohérence des icônes** : un seul système d'icônes vectorielles (trait 1.5px, style outline, 16x16 ou 20x20), jamais de mélange de styles.
- **Feedback immédiat** : hover, focus, sélection, drag-over, erreur — chaque état a une couleur et une transition (120–150ms ease-out).
- **Clavier d'abord** : chaque action a un raccourci visible dans les menus et les tooltips.
- **Pas de blanc/noir pur** : toutes les couleurs de fond et de texte viennent de la palette GitHub (voir section 2), jamais de `#000000` ou `#ffffff` pur pour le texte.

---

## 2. Système de thèmes

### 2.1 Thème Light (style GitHub)

| Rôle | Couleur | Usage |
|---|---|---|
| Fond principal (canvas) | `#ffffff` | Fond du viewport, fond des fenêtres |
| Fond secondaire (subtle) | `#f6f8fa` | Fond des panneaux, barres d'outils |
| Fond inset | `#eaeef2` | Champs de saisie, zones creusées |
| Bordure par défaut | `#d0d7de` | Séparateurs, contours de panneaux |
| Bordure discrète | `#d8dee4` | Lignes de tableau, hairlines |
| Texte principal | `#1f2328` | Labels, titres |
| Texte secondaire | `#656d76` | Sous-titres, valeurs désactivées |
| Accent (sélection/liens) | `#0969da` | Item sélectionné, liens, focus ring |
| Succès | `#1a7f37` | Compilation OK, validation |
| Attention | `#9a6700` | Warning |
| Danger | `#d1242f` | Erreur, suppression |
| Violet (spécial/plugin) | `#8250df` | Tags Blueprint, éléments "événement" |

### 2.2 Thème Dark (style GitHub Dark Pro)

| Rôle | Couleur | Usage |
|---|---|---|
| Fond principal (canvas) | `#0d1117` | Fond du viewport |
| Fond secondaire (subtle) | `#161b22` | Panneaux, barres d'outils |
| Fond inset | `#010409` | Champs de saisie |
| Bordure par défaut | `#30363d` | Séparateurs |
| Bordure discrète | `#21262d` | Hairlines |
| Texte principal | `#e6edf3` | Labels, titres |
| Texte secondaire | `#8b949e` | Valeurs, sous-titres |
| Accent (sélection/liens) | `#2f81f7` | Sélection, focus ring |
| Succès | `#3fb950` | OK |
| Attention | `#d29922` | Warning |
| Danger | `#f85149` | Erreur |
| Violet (spécial/plugin) | `#a371f7` | Événements Blueprint |

### 2.3 Règles transverses

- Le viewport 3D garde toujours un fond neutre proche du noir/gris très foncé, **même en thème Light**, pour ne pas fausser la perception colorimétrique des rendus (comme Unreal). Seuls les chromes (barres, panneaux) suivent le thème.
- Un switch Light/Dark est disponible dans la barre de titre (icône soleil/lune) et dans Preferences.
- Le thème par défaut au premier lancement est **Dark**.

---

## 3. Launcher

Le Launcher est une fenêtre indépendante, plus petite que l'éditeur (1280x800 par défaut), lancée avant l'éditeur.

### 3.1 Écran de démarrage (Splash)
- Logo animé, barre de progression fine en bas, version du moteur affichée en petit texte gris.
- Durée : le temps du chargement des plugins et de la vérification des mises à jour.

### 3.2 Fenêtre principale du Launcher
Structure en sidebar gauche (240px) + zone de contenu :

**Sidebar gauche :**
- Avatar / nom de compte (haut)
- Navigation verticale : `Bibliothèque`, `Marketplace`, `Apprendre`, `Communauté`, `Paramètres`
- Sélecteur de version du moteur en bas (dropdown "Aetherion 5.4 ▾" + bouton "Installer une version")

**Onglet Bibliothèque (par défaut) :**
- En haut : bouton primaire `+ Nouveau projet`
- Barre de recherche + filtres (Tous / Récents / Favoris / Par moteur)
- Grille de cartes projets : miniature 16:9 du dernier rendu, nom du projet, version moteur utilisée, date de dernière ouverture, bouton "..." (Ouvrir l'emplacement, Dupliquer, Migrer vers une nouvelle version, Retirer de la liste, Supprimer)
- Double-clic sur une carte = ouverture du projet avec écran de chargement

**Onglet Marketplace :**
- Grille de packs (environnements, personnages, plugins), filtrable par catégorie/gratuit-payant, fiche produit en modal au clic.

### 3.3 Assistant "Nouveau projet"
Modal plein écran en plusieurs étapes (stepper horizontal en haut) :

1. **Catégorie** : Jeux / Films & Rendu / Architecture / Simulation — cartes larges avec icône
2. **Template** : grille de templates (Vide 3D, Third Person, First Person, Vehicule, Vide 2D, Top-down 2D…) avec toggle "Blueprint / C++"
3. **Paramètres cible** :
   - Cible : Bureau/Console vs Mobile/Tablette
   - Qualité graphique : Maximum vs Échelonnable
   - Ray tracing : activé/désactivé
   - Starter content : inclure/exclure
4. **Emplacement & nom** : champ chemin de dossier (avec bouton parcourir), champ nom du projet, espace disque disponible affiché
5. Bouton final `Créer le projet` (primaire, accent) — passe à l'écran de chargement de l'éditeur.

---

## 4. Coquille de l'éditeur (Editor Shell)

Fenêtre unique maximisée par défaut, structure verticale :

```
┌─────────────────────────────────────────────────────────┐
│ Barre de titre (menu + nom projet + contrôles fenêtre)   │
├─────────────────────────────────────────────────────────┤
│ Barre de menu (File Edit Window Tools Build Select Help) │
├─────────────────────────────────────────────────────────┤
│ Barre d'outils principale (Save | Source Ctrl | Play...) │
├──────────┬──────────────────────────────┬────────────────┤
│  Modes   │                              │                │
│  (icône  │        VIEWPORT 3D           │  Details Panel │
│  verti-  │        (zone dockable)       │  (dockable)    │
│  cale)   │                              │                │
├──────────┴──────────────────────────────┴────────────────┤
│  World Outliner (dock gauche/droite)   │  Content Browser │
├─────────────────────────────────────────────────────────┤
│ Barre de statut (FPS, mémoire, source control, messages) │
└─────────────────────────────────────────────────────────┘
```

### 4.1 Barre de menu
`File · Edit · Window · Tools · Build · Select · Actor · Help`

- **File** : Nouveau niveau, Ouvrir niveau, Enregistrer, Enregistrer sous, Importer, Exporter, Projet récents, Quitter
- **Edit** : Annuler/Rétablir, Couper/Copier/Coller, Dupliquer, Rechercher & remplacer, Préférences de l'éditeur, Paramètres du projet
- **Window** : liste de tous les panneaux dockables avec case à cocher, Layouts (Enregistrer la disposition, Charger, Réinitialiser), Plein écran
- **Tools** : Éditeur de matériaux, Éditeur de Blueprint, Sequencer, Landscape, Foliage, Migration d'assets
- **Build** : Compiler les niveaux (lighting), Compiler le code, Valider les assets, Ouvrir le menu de packaging
- **Select** : Tout sélectionner, Rien, Inverser, Sélectionner par type/tag
- **Actor** : Transformer, Grouper, Attacher, Convertir en Blueprint
- **Help** : Documentation, Raccourcis clavier, À propos

### 4.2 Barre d'outils principale
De gauche à droite :
- Icône Save (projet), icône Save All
- Contrôle Source Control (icône cadenas/branche + statut coloré)
- Groupe transform gizmo rapide (Sélection / Déplacer / Rotation / Échelle / Universel) — raccourcis Q W E R T
- Dropdown "Snap" (grille, angle, échelle) avec valeur numérique éditable
- **Bouton Play** central (icône triangle vert, très visible), avec dropdown pour choisir le mode (Play in Editor / Simulate / Standalone / sur device)
- Bouton Pause / Stop (actifs seulement en Play)
- Sélecteur de plateforme cible (icône console/PC/mobile + dropdown de config Debug/Development/Shipping)
- Icône Paramètres du projet
- Icône Éditeur de Blueprint (raccourci "Blueprints du niveau")
- Icône Cinématique (ouvre Sequencer)
- Icône notifications (cloche, badge rouge si erreurs de build)

### 4.3 Mode bar (barre verticale gauche, icônes 32x32)
`Sélection · Landscape · Foliage · Mesh Paint · Brush Editing (BSP) · Animation`
Un seul mode actif à la fois, change le contenu du Details Panel et les outils du viewport.

### 4.4 Barre de statut (bas de fenêtre)
- Zone gauche : messages de compilation en cours (spinner + texte), lien "Voir le log"
- Zone centre : nombre d'objets sélectionnés, coordonnées curseur monde
- Zone droite : FPS, temps de frame (ms), mémoire utilisée, icône source control avec branche courante

---

## 5. Système de docking

- Chaque panneau a un en-tête (28px) avec titre, icône, bouton "..." (menu : Fermer, Détacher, Masquer les onglets), bouton fermer.
- **Glisser un panneau** sur le bord d'un autre fait apparaître un overlay en croix (zones haut/bas/gauche/droite/centre) — déposer sur "centre" = empile en onglets, déposer sur un bord = split la zone (nouvelle colonne/ligne).
- Un panneau détaché devient une fenêtre flottante indépendante, toujours au-dessus de la fenêtre principale, redockable en la faisant glisser de nouveau.
- Onglets multiples dans une même zone : barre d'onglets scrollable horizontalement si trop nombreux, avec chevron ">>" de dépassement.
- Menu `Window > Layouts` permet d'enregistrer/charger des dispositions nommées (ex : "Layout Animation", "Layout Level Design").
- Layout par défaut = 5 zones : Viewport (centre), Outliner (droite-haut), Details (droite-bas), Content Browser (bas), Toolbar modes (gauche).

---

## 6. Viewport 3D

### 6.1 Overlay supérieur gauche (menus déroulants en ligne)
`Perspective ▾  |  Lit ▾ (view mode)  |  Show ▾ (show flags)  |  ⛶ (maximiser)`

- **Perspective ▾** : Perspective, Top, Bottom, Front, Back, Left, Right, Ortho
- **Lit ▾** (view modes) : Lit, Unlit, Wireframe, Detail Lighting, Lighting Only, Reflections, Player Collision, Buffer Visualization…
- **Show ▾** : cases à cocher pour Grid, Bounds, Collision, Fog, Post-processing, Sky, Particles, Landscape…

### 6.2 Overlay supérieur droit
- Icône caméra (FOV, vitesse de déplacement — slider au clic)
- Icône capture d'écran haute-résolution
- Icône "Immersive mode" (masque tous les chromes)

### 6.3 Overlay inférieur (statistiques, optionnel via Show Flags)
- FPS, Draw calls, Triangles, Mémoire GPU, en petit texte monospace semi-transparent, coin inférieur gauche.

### 6.4 Gizmos
- Gizmo de transformation (flèches XYZ colorées : rouge X, vert Y, bleu Z) avec anneaux de rotation et cubes d'échelle, taille constante à l'écran (non affectée par le zoom).
- Petit gizmo d'orientation (cube 3D cliquable) en haut à droite du viewport pour changer rapidement de vue.

### 6.5 Multi-viewport
- Bouton de layout (icône grille) : 1 vue plein cadre, 2 vues (H ou V), 4 vues (quad classique Top/Front/Side/Perspective). Chaque sous-viewport garde son propre overlay indépendant.

### 6.6 Sélection dans le viewport
- Clic simple = sélection, contour orange/jaune (`#f0883e` en dark, `#bc4c00` en light) autour du mesh sélectionné.
- Rectangle de sélection (drag) = multi-sélection.
- Menu contextuel clic-droit : Transformer vers, Éditer, Dupliquer, Créer un Blueprint depuis la sélection, Aligner avec la grille, Attacher à un parent, Isoler la sélection.

---

## 7. World Outliner (hiérarchie de scène)

Panneau dockable, structure :
- Barre de recherche en haut avec icône filtre (par type d'acteur, par tag, par couche/layer)
- Bouton "+" pour créer un nouveau dossier d'organisation
- Arbre hiérarchique avec :
  - Icône d'œil (visibilité) à gauche de chaque ligne
  - Icône de type d'objet (mesh, lumière, caméra, particules, son…)
  - Nom de l'acteur (double-clic pour renommer inline)
  - Colonne optionnelle "Type", "Layer", "Nb de triangles" (activables via l'en-tête de colonne, clic-droit)
- Glisser-déposer pour réorganiser la hiérarchie parent/enfant (indentation visuelle pendant le drag)
- Sélection multiple avec Ctrl/Shift, synchronisée avec la sélection dans le viewport
- Dossiers repliables/dépliables avec chevron

---

## 8. Details Panel (propriétés)

Panneau dockable affichant les propriétés du/des objet(s) sélectionné(s) :

- En-tête : nom de l'acteur (éditable), icône de type, bouton "Editer le Blueprint" si applicable
- Barre de recherche de propriété (filtre en direct dans toutes les catégories)
- Liste de **catégories repliables** (accordéons) : Transform, Rendering, Physics, Collision, Lighting, Tags, chaque catégorie custom des composants
- **Section Transform** toujours en haut, non repliable par défaut :
  - 3 champs numériques (X/Y/Z) pour Position, Rotation, Scale, chacun avec code couleur (rouge/vert/bleu) sur le label
  - Icône cadenas à côté de "Scale" pour lier/délier les 3 axes
  - Petite icône reset (flèche circulaire) apparaît au survol si la valeur diffère du défaut
- Liste de **composants** (Component hierarchy) en haut du panneau si l'acteur en a plusieurs, avec bouton "+ Ajouter un composant" (ouvre un menu recherche de type de composant)
- Chaque propriété : label à gauche (35% de largeur), champ éditable à droite ; types supportés : nombre, texte, booléen (toggle), couleur (swatch cliquable → color picker), enum (dropdown), référence d'asset (mini-thumbnail + nom, cliquable pour ouvrir dans Content Browser), courbe (mini-graphe inline)
- Propriétés modifiées par rapport à un Blueprint parent affichées en **gras avec une pastille jaune** à gauche (comme les diffs Unreal)

---

## 9. Content Browser (miniatures d'assets)

Panneau dockable en bas, structure horizontale :

**Colonne gauche (Sources, 220px) :**
- Arbre de dossiers du projet (Content/, Characters/, Maps/, Materials/…)
- Section "Favoris" épinglés en haut
- Section "Collections" (regroupements virtuels d'assets multi-dossiers)

**Zone centrale :**
- Fil d'ariane (breadcrumb) cliquable en haut : `Content > Characters > Hero`
- Barre de recherche + filtres par type d'asset (icônes toggle : Meshes, Materials, Textures, Blueprints, Sounds, Animations…)
- Slider de taille des miniatures (petit → grand)
- **Grille de miniatures** :
  - Carte carrée avec rendu 3D en aperçu (rotation automatique au survol pour les meshes, lecture au survol pour les sons/vidéos)
  - Nom de l'asset en dessous, tronqué avec ellipsis, tooltip complet au survol
  - Badge coin supérieur droit selon le type (icône colorée par catégorie : bleu=Blueprint, orange=Mesh, violet=Material, vert=Sound)
  - Overlay au survol : bouton aperçu rapide (loupe), bouton renommer
  - Sélection = bordure accent + fond légèrement teinté
  - Glisser-déposer direct vers le Viewport ou le World Outliner pour instancier
  - Clic-droit : Renommer, Dupliquer, Supprimer, Migrer, Créer une référence, Afficher dans l'explorateur système, Créer un Material Instance (si applicable)
- Bouton primaire "Importer" en haut à droite + bouton "+ Ajouter" (menu créer nouveau : dossier, Blueprint, Material, Level, Niveau de séquence…)

---

## 9bis. Asset Browser avancé — Reference Viewer & Size Map

Le Content Browser (section 9) est la vue "miniatures" quotidienne. Deux vues complémentaires, ouvertes depuis le clic-droit sur un asset (`Asset Actions > Reference Viewer` / `Size Map`), donnent une vue globale du projet :

### Reference Viewer (graphe de dépendances)
- Ouvre un onglet plein écran type graphe de nœuds (même moteur visuel que le Material Editor).
- Nœud central = l'asset choisi, en surbrillance accent.
- À gauche : tous les assets qui **référencent** celui-ci (parents / dépendants) — flèches entrantes.
- À droite : tous les assets **référencés par** celui-ci (dépendances) — flèches sortantes.
- Chaque nœud = mini-carte avec icône colorée par type (même code couleur que le Content Browser), nom, et badge "manquant" en rouge si l'asset référencé n'existe plus sur disque.
- Barre d'outils : profondeur de recherche (slider 1–10 niveaux), toggle "Afficher les références Editor-only", bouton "Localiser dans le Content Browser".
- Utilité affichée clairement : avant de supprimer/déplacer un asset, l'utilisateur voit immédiatement l'impact.

### Size Map (carte de poids)
- Treemap (rectangles proportionnels à la taille disque) démarrant depuis un asset ou un dossier.
- Couleur du rectangle = type d'asset (mêmes couleurs que Content Browser), taille du rectangle = poids en Mo.
- Survol = tooltip avec poids exact, type, chemin complet.
- Utile pour identifier les textures/meshes trop lourds avant packaging.

---

## 9ter. Project Navigator (arborescence système de fichiers)

Panneau dockable distinct du Content Browser : alors que le Content Browser montre les **chemins virtuels d'assets** (Content/...), le Project Navigator montre **l'arborescence réelle du projet sur disque**, utile pour les développeurs C++/scripts et la gestion de fichiers non-assets.

- Arbre de dossiers réel du projet :
  - `Source/` (fichiers .cpp/.h, icônes spécifiques par langage, structure Public/Private)
  - `Content/` (miroir simplifié du Content Browser, lecture seule ici)
  - `Config/` (fichiers .ini)
  - `Plugins/` (plugins installés, avec badge "Activé/Désactivé")
  - `Binaries/`, `Intermediate/`, `Saved/` — dossiers techniques, grisés par défaut avec toggle "Afficher les dossiers générés"
- Chaque fichier a une icône par extension (.cpp bleu, .h violet, .ini gris, .uasset selon type).
- Barre d'outils : bouton "Ouvrir dans l'IDE" (VS Code / Visual Studio / Rider selon config), bouton "Actualiser", champ recherche par nom de fichier.
- Clic-droit sur un fichier source : Ouvrir dans l'IDE, Ouvrir l'emplacement, Ajouter une nouvelle classe C++ (ouvre l'assistant "Nouvelle classe" avec choix de la classe parente et génération .h/.cpp).
- Ce panneau est masqué par défaut dans un projet 100% Blueprint (sans dossier Source significatif) et visible par défaut dès qu'un module C++ existe.

---

## 9quater. Class Viewer / Blueprint Hierarchy

Panneau/fenêtre dédiée à la hiérarchie des **classes** (différente de la hiérarchie de **scène** du World Outliner) :

- Arbre affichant l'arborescence d'héritage complète du moteur : `Object > Actor > Pawn > Character > BP_Hero`, `Actor > StaticMeshActor`, etc.
- Chaque nœud a une icône (C++ = icône engrenage bleu, Blueprint = icône plan bleu clair), et un badge si la classe est abstraite (grisé, non instanciable).
- Barre de recherche + toggle "Afficher seulement les classes Blueprint" / "Afficher les classes du moteur (C++)".
- Clic-droit sur une classe : Créer un Blueprint enfant, Ouvrir l'éditeur, Trouver toutes les références (ouvre le Reference Viewer filtré sur les instances de cette classe dans le niveau).
- Accessible depuis `Window > Class Viewer` et depuis un bouton dans l'assistant de création de Blueprint (`Choisir la classe parente`).

---

## 10. Place Actors / Palette d'objets

Panneau ou onglet accessible depuis la mode bar :
- Onglets horizontaux : Récent, Basique (Cube/Sphère/Plane/Light), Lumières, Visual Effects, Volumes, Géométrie, Tout
- Grille d'icônes glissables-déposables directement dans le viewport
- Barre de recherche filtrant tous les onglets à la fois

---

## 11. Output Log / Console

Panneau dockable, bas de fenêtre en général (onglet à côté de Content Browser) :
- Zone de texte monospace scrollable, une ligne par entrée
- Code couleur par sévérité : gris (Log info), jaune (Warning), rouge (Error), bleu (Command)
- Barre de filtre par catégorie (Blueprint, Physics, Rendering, Network…) avec cases à cocher
- Champ de saisie de commande console en bas avec autocomplétion
- Bouton "Effacer", bouton "Exporter le log", toggle "Auto-scroll"

---

## 12. Éditeur de matériaux

Fenêtre/onglet dédié plein écran dans la coquille de l'éditeur :

- **Zone centrale** : graphe de nœuds (node graph) sur fond quadrillé subtil, nœuds connectés par des câbles courbes colorés selon le type de donnée (blanc=vecteur, vert=scalaire, jaune=texture, gris=booléen)
- Nœud final "Material Output" toujours à droite, non supprimable
- Panneau gauche : palette de nœuds recherchable (Math, Texture, Paramètres, Utilitaires) par glisser-déposer ou double-clic
- Panneau droit haut : **preview 3D** de la sphère/cube/plan avec le matériau appliqué en temps réel, rotation à la souris
- Panneau droit bas : Details Panel du nœud sélectionné
- Barre d'outils : Apply, Search (rechercher un nœud dans le graphe), Zoom to fit, Live preview toggle
- Minimap en coin inférieur droit du graphe

---

## 13. Éditeur de Blueprint (visual scripting)

- **My Blueprint panel** (gauche) : liste des Variables, Fonctions, Macros, Event Graphs, Composants — chacun avec bouton "+"
- **Zone centrale** : Event Graph, nœuds connectés par câbles blancs (exécution, épais) et colorés (données, fins), nœuds Event en rouge/rose en tête de chaîne
- **Components panel** (en haut à gauche au-dessus de My Blueprint) : arbre des composants avec aperçu viewport miniature au-dessus (identique au Details Panel de l'éditeur principal)
- **Details Panel** (droite) : propriétés de la variable/nœud sélectionné
- Barre d'outils supérieure : Compile (icône avec badge succès/erreur), Save, Find in Blueprint, Class Settings, Class Defaults
- Onglets multiples : Event Graph, Construction Script, Functions individuelles, Macros

---

## 14. Sequencer / Timeline d'animation

- Zone gauche : liste des pistes (tracks) avec icônes par type (Transform, Visibilité, Matériau, Événement, Caméra), repliables si sous-pistes
- Zone droite : timeline horizontale avec règle temporelle (frames/secondes), keyframes en losanges cliquables/glissables
- Curseur de lecture (playhead) rouge vertical, draggable
- Barre d'outils : Play/Pause/Stop de la séquence, boutons Add Track, Add Camera, zoom temporel (slider), snapping aux frames
- Courbes d'animation en mode alternatif (bouton toggle "Curve Editor") affichant les interpolations en graphe

---

## 15. World Settings & Project Settings

Fenêtres modales/onglets pleine hauteur avec **sidebar de catégories à gauche** (comme des Preferences classiques) :
- World Settings : Gameplay (GameMode), Rendering (Lightmass), Physics, Navigation
- Project Settings : catégories Description, Maps & Modes, Input, Rendering, Packaging, Platforms (une sous-section par plateforme cible avec icône), Audio, Plugins
- Chaque catégorie affiche une longue liste de propriétés groupées en sections repliables, recherche globale en haut de la fenêtre filtrant toutes les catégories simultanément

---

## 16. Préférences de l'éditeur

Fenêtre modale, sidebar gauche de catégories : Général, Apparence (thème Light/Dark + accent color + taille de police UI), Niveau & Éditeur, Contrôles viewport (vitesse souris, inversion Y), Raccourcis clavier (tableau éditable recherche + colonne "Raccourci" cliquable pour ré-assigner), Source Control (Git/Perforce, credentials), Performance (auto-save intervalle, undo history depth)

---

## 17. Build / Package / Platforms

### 17.1 Menu Build (depuis la barre de menu)
- Compiler l'éclairage (qualité : Preview/Medium/High/Production, dialog avec estimation du temps)
- Compiler le code source (si C++), avec sortie dans l'Output Log
- Valider les assets manquants (rapport en modal listant les erreurs, double-clic pour localiser)

### 17.2 Dialogue de Packaging
Modal en plusieurs sections :
- Sélection de plateforme (icônes en haut : Windows, Mac, Linux, Android, iOS, consoles…)
- Configuration : Debug / Development / Shipping (radio buttons avec description courte de chacune)
- Options : Cook everything vs Cook seulement les maps du projet, compression, distribution build
- Chemin de sortie (champ + bouton parcourir)
- Bouton `Package Project` (primaire) lance le process
- **Progression** : barre de progression + log en direct dans un panneau extensible, étapes listées verticalement (Cooking content → Compiling → Staging → Packaging → Archiving) avec coche verte/spinner/croix rouge par étape
- Notification toast à la fin (succès avec bouton "Ouvrir le dossier" / échec avec bouton "Voir le log")

---

## 18. Dialogues, notifications, menus contextuels

- **Notifications toast** : coin inférieur droit, empilables, auto-dismiss 5s (sauf erreurs = restent jusqu'à fermeture manuelle), icône de sévérité + titre + description courte + action optionnelle
- **Dialogues modaux** : overlay semi-transparent sur toute la fenêtre (`rgba(0,0,0,0.5)`), boîte centrée, boutons alignés à droite (Annuler en texte simple, action primaire en bouton accent)
- **Menus contextuels** : apparaissent au point du clic-droit, items avec icône + label + raccourci aligné à droite, séparateurs entre groupes logiques, sous-menus avec flèche `▸`
- **Tooltips** : délai d'apparition 500ms, fond `bg-canvas-inset`, bordure fine, texte court + raccourci en gris si applicable

---

## 19. Glossaire des composants réutilisables

| Composant | Description |
|---|---|
| `PanelHeader` | En-tête de panneau dockable (titre, icône, actions) |
| `TreeView` | Arbre hiérarchique (Outliner, dossiers Content Browser) |
| `PropertyRow` | Ligne label + champ éditable du Details Panel |
| `ThumbnailCard` | Carte de miniature d'asset |
| `NodeGraphCanvas` | Canevas de graphe de nœuds (Material/Blueprint) |
| `Toolbar` | Barre d'icônes horizontale avec séparateurs |
| `TabStrip` | Barre d'onglets dockable |
| `Toast` | Notification flottante |
| `Modal` | Fenêtre de dialogue centrée |
| `StatusBar` | Barre d'état basse |
| `Stepper` | Assistant multi-étapes (New Project) |
| `ColorSwatch` | Pastille couleur cliquable ouvrant un color picker |

---

**Fin du document 1/3.** Voir `02-specification-claude.md` pour la spécification technique d'implémentation et `03-specification-banani.md` pour les prompts de génération écran par écran.
