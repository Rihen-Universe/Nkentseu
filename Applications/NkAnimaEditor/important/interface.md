# Spécification Complète des Interfaces Utilisateur
# Application d'Animation 3D Physiquement Correcte avec Pilotage IA
> Inspirée de Cascadeur + Blender — Jeux Vidéo & Cinéma

---

## Table des Matières

1. [Vue d'ensemble de l'interface principale](#1-vue-densemble-de-linterface-principale)
2. [Viewport 3D Principal](#2-viewport-3d-principal)
3. [Timeline & Dope Sheet](#3-timeline--dope-sheet)
4. [Graph Editor (Courbes F-Curve)](#4-graph-editor-courbes-f-curve)
5. [Panneau de Hiérarchie / Outliner](#5-panneau-de-hiérarchie--outliner)
6. [Panneau Propriétés](#6-panneau-propriétés)
7. [Viewport d'Animation (Animation Preview)](#7-viewport-danimation-animation-preview)
8. [Éditeur de Rig & Armature](#8-éditeur-de-rig--armature)
9. [Interface IA (AI Director Panel)](#9-interface-ia-ai-director-panel)
10. [Physics Simulation Panel](#10-physics-simulation-panel)
11. [Motion Library & Asset Browser](#11-motion-library--asset-browser)
12. [Render & Output Panel](#12-render--output-panel)
13. [Node Editor (Shader / Compositing / Rigging)](#13-node-editor-shader--compositing--rigging)
14. [Camera & Cinématique Panel](#14-camera--cinématique-panel)
15. [Audio & Sync Panel](#15-audio--sync-panel)
16. [Workspace Switcher & Layouts](#16-workspace-switcher--layouts)
17. [Raccourcis Clavier Globaux](#17-raccourcis-clavier-globaux)
18. [Actions Souris Globales](#18-actions-souris-globales)
19. [Barre de Menus Principale](#19-barre-de-menus-principale)
20. [Barre d'Outils Contextuelle (Header Toolbar)](#20-barre-doutils-contextuelle-header-toolbar)
21. [HUD & Overlays Viewport](#21-hud--overlays-viewport)
22. [Gizmos & Manipulateurs](#22-gizmos--manipulateurs)
23. [Fenêtres Modales & Dialogues](#23-fenêtres-modales--dialogues)

---

## 1. Vue d'ensemble de l'interface principale

### Description Générale

L'interface principale adopte une disposition modulaire à zones redimensionnables et ré-assemblables, similaire à Blender mais optimisée pour le flux de travail animation-physique-IA. Chaque zone est indépendante et peut accueillir n'importe quel éditeur.

### Structure par Défaut (Layout "Animation Film")

```
┌──────────────────────────────────────────────────────────────────────┐
│  MENU BAR  │  FILE  EDIT  VIEW  OBJECT  ANIMATION  PHYSICS  AI  RENDER  HELP  │
├────────────────────────────────────────────────────────┬─────────────┤
│                                                        │             │
│                   VIEWPORT 3D PRINCIPAL                │  OUTLINER   │
│                      (zone centrale)                   │  HIERARCHY  │
│                                                        │             │
│                                                        ├─────────────┤
│                                                        │             │
│                                                        │ PROPRIÉTÉS  │
│                                                        │   PANEL     │
├────────────────┬───────────────────────────────────────┤             │
│   DOPE SHEET   │          GRAPH EDITOR / F-CURVES      │             │
│   / TIMELINE   │                                       │             │
├────────────────┴───────────────────────────────────────┴─────────────┤
│  TRANSPORT BAR  [|◄] [◄] [▶/▏▏] [►] [►|]   FPS: 24  FRAME: 001/250 │
└──────────────────────────────────────────────────────────────────────┘
```

### Zones Redimensionnables

Chaque bordure entre zones peut être glissée pour redimensionner. Un clic droit sur une bordure offre : **Diviser horizontalement**, **Diviser verticalement**, **Fusionner zones**, **Dupliquer zone**.

### Thèmes

- **Dark Pro** (défaut cinéma) — fond #1a1a1a, accents cyan #00d4ff
- **Dark Game** — fond #1c1c1c, accents orange #ff6a00
- **Light Studio** — fond #d4d4d4 pour environnements lumineux
- **High Contrast** — accessibilité

---

## 2. Viewport 3D Principal

### Description

Le cœur de l'application. Zone de visualisation et d'interaction 3D temps réel avec rendu physiquement correct (PBR), simulation de corps rigides/mous, et overlays d'animation (trajectoires, forces, centres de masse).

### Zones de l'Interface Viewport

#### Header du Viewport (bandeau supérieur)

```
[Mode▼] [Shading▼] [Viewport Shading Icons] [Overlay▼] [Gizmo▼] [Proportional▼] [Snap▼]
```

- **Mode Selector** (déroulant, raccourci `Tab` pour basculer) :
  - Object Mode
  - Edit Mode
  - Pose Mode
  - Weight Paint Mode
  - Vertex Paint Mode
  - Physics Debug Mode
  - Camera Mode

- **Shading** (déroulant) :
  - Wireframe `Alt+Z`
  - Solid `Z`
  - Material Preview `M`
  - Rendered `Ctrl+M`
  - Physics Debug (forces, collisions, masses)

- **Overlays** (déroulant panneau flottant) :
  - Afficher trajectoires d'os
  - Afficher centres de masse
  - Afficher vecteurs de force
  - Afficher zones de contact IK
  - Afficher contraintes
  - Grid et axes
  - Motion paths
  - Onion skinning (peaux d'oignon)

#### Gizmo de Navigation (coin supérieur droit)

Sphère de navigation XYZ cliquable + icônes :
- 🔍 Zoom (molette ou `+`/`-`)
- 👁 Perspective/Orthographique `Numpad 5`
- 🎥 Caméra active `Numpad 0`
- 🏠 Home View `Home`

#### Panneau Latéral (N-Panel, `N` pour afficher/masquer)

Panneau coulissant depuis la droite du viewport, onglets :

- **Transform** — position XYZ, rotation XYZ, échelle XYZ, dimensions
- **Item** — propriétés de l'objet sélectionné
- **Tool** — options de l'outil actif
- **View** — paramètres de caméra de vue, clipping, FOV
- **Physics** — aperçu rapide des propriétés physiques de l'objet
- **AI** — suggestions IA contextuelles pour la pose actuelle
- **Motion** — trajectoire, vitesse, accélération de l'objet sélectionné

#### Toolbar (T-Panel, `T` pour afficher/masquer)

Barre verticale gauche avec outils contextuels selon le mode :

**Object Mode :**
- ↖ Sélection (boîte, cercle, lasso) `W`
- ✋ Move `G`
- 🔄 Rotate `R`
- ↔ Scale `S`
- 📐 Transform (move+rotate+scale combinés)
- 📏 Measure
- 📌 Annotate

**Pose Mode :**
- 🦴 Select Bone
- ✋ Move Bone `G`
- 🔄 Rotate Bone `R`
- 💪 IK Drag (inverse kinematics interactive) `I`
- 🎯 FK Mode `F`
- ⚖ Physics Balance Assist
- 🤖 AI Pose Suggest `Ctrl+Alt+P`
- 📐 Pose Symmetrize `X→Y`

### Interactions Souris dans le Viewport

| Action | Résultat |
|--------|----------|
| Clic gauche | Sélectionner |
| Shift + Clic gauche | Ajouter à la sélection |
| Clic droit | Menu contextuel |
| Molette | Zoom |
| Clic molette (maintenu) | Orbite |
| Shift + Clic molette | Pan |
| Ctrl + Clic molette | Zoom précis |
| Double clic sur objet | Entrer en Edit/Pose Mode |
| Clic gauche maintenu + drag | Déplacer (si outil Move actif) |
| Alt + Clic gauche | Sélection en boucle (edge loop) |
| Ctrl + Clic gauche | Sélection en chemin |
| Clic gauche sur fond | Désélectionner tout |

### Raccourcis Clavier Viewport 3D

| Raccourci | Action |
|-----------|--------|
| `G` | Grab/Move (puis X/Y/Z pour contraindre l'axe) |
| `R` | Rotate (puis X/Y/Z) |
| `S` | Scale (puis X/Y/Z) |
| `G` → `X` → valeur | Déplacer exactement sur X |
| `A` | Sélectionner tout / Désélectionner tout |
| `Alt+A` | Désélectionner tout |
| `B` | Box Select |
| `C` | Circle Select |
| `Ctrl+I` | Inverser la sélection |
| `H` | Masquer la sélection |
| `Alt+H` | Afficher tout |
| `F` | Remplir (fill) |
| `Tab` | Basculer Edit/Object Mode |
| `Ctrl+Tab` | Sélectionner le mode (pie menu) |
| `Numpad 1` | Vue face |
| `Numpad 3` | Vue côté |
| `Numpad 7` | Vue dessus |
| `Numpad 0` | Vue caméra |
| `Numpad 5` | Basculer perspective/ortho |
| `Numpad .` | Zoom sur sélection |
| `Home` | Tout afficher |
| `Z` | Pie menu shading |
| `N` | Panneau latéral |
| `T` | Toolbar |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `X` | Supprimer |
| `M` | Déplacer dans collection |
| `Ctrl+J` | Joindre objets |
| `Alt+G` | Réinitialiser position |
| `Alt+R` | Réinitialiser rotation |
| `Alt+S` | Réinitialiser échelle |
| `I` | Insérer keyframe (menu pie) |
| `Alt+I` | Supprimer keyframe |
| `Shift+I` | Insérer keyframe pour tous les canaux |
| `Ctrl+Alt+I` | Insérer keyframe guidé par IA |
| `/` | Isoler la sélection (Local View) |
| `Ctrl+Numpad 0` | Définir objet comme caméra active |
| `Ctrl+Alt+C` | Copier la pose |
| `Ctrl+Alt+V` | Coller la pose |
| `Ctrl+Alt+Shift+V` | Coller la pose miroir |
| `P` | Créer groupe de contrainte IK |
| `Ctrl+P` | Parent |
| `Alt+P` | Supprimer parent |
| `Shift+D` | Dupliquer |
| `Alt+D` | Dupliquer lié |
| `Ctrl+D` | Dupliquer et simuler physique |
| `F12` | Rendu du frame courant |
| `Ctrl+F12` | Rendu de l'animation |
| `Ctrl+Shift+A` | Appliquer suggestion IA complète |
| `Ctrl+Shift+P` | Ouvrir AI Physics Check |

### Onion Skinning (Peaux d'Oignon)

Panneau flottant activé depuis Overlays ou `Ctrl+Shift+O` :
- Nombre de frames avant/après
- Opacité des ghosts
- Couleur avant (bleu) / après (orange)
- Mode : absolu (frames fixes) ou relatif (frames autour du courant)
- Afficher seulement les keyframes

---

## 3. Timeline & Dope Sheet

### Description

Panneau inférieur combiné en deux modes : **Timeline** pour la navigation temporelle globale, **Dope Sheet** pour la gestion des keyframes et canaux d'animation. Peut aussi afficher le **NLA Editor** (Non-Linear Animation).

### Onglets du Panneau

```
[Timeline] [Dope Sheet] [NLA Editor] [Action Editor]
```

### Timeline

Interface de navigation temporelle globale.

#### Header Timeline

```
[◄◄] [◄] [▶/▏▏] [►] [►►]  |  Frame: [001]  Start: [001]  End: [250]  FPS: [24 ▼]  |  [⏱ Vitesse]  [🔒 Lock]
```

- **Transport Controls** : premier frame, précédent, play/pause, suivant, dernier
- **Champ Frame** : éditable, `Ctrl+Clic` pour saisie numérique
- **Start / End** : plage de lecture, clic droit pour options de plage
- **FPS Selector** : 12, 24, 25, 30, 48, 60, 120 fps + personnalisé
- **Vitesse de lecture** : x0.25, x0.5, x1, x2 (pour prévisualisation)
- **Lock to Frame Range** : verrouille la tête de lecture dans la plage

#### Corps de la Timeline

- Barre de frames avec marqueurs temporels
- **Keyframes** : losanges colorés selon le canal :
  - Jaune = keyframe position
  - Vert = keyframe rotation
  - Bleu = keyframe échelle
  - Orange = keyframe IK
  - Rouge = keyframe physique
  - Violet = keyframe IA
- **Tête de lecture** : ligne bleue verticale, glissable
- **Marqueurs** (`M` pour ajouter) : drapeaux nommés sur la timeline
- **Plages de preview** (`P` + drag) : zone de lecture en boucle
- **Audio waveform** (si audio chargé) : forme d'onde affichée en arrière-plan

#### Interactions Souris Timeline

| Action | Résultat |
|--------|----------|
| Clic gauche | Déplacer la tête de lecture |
| Drag sur timeline | Scrubbing (navigation rapide) |
| Clic gauche sur keyframe | Sélectionner keyframe |
| Shift + Clic | Ajouter à la sélection |
| Drag sur keyframe | Déplacer le keyframe |
| Ctrl + Drag | Snap au frame entier |
| G | Déplacer keyframes sélectionnés |
| S | Scaler keyframes sélectionnés |
| X | Supprimer keyframes |
| M | Ajouter marqueur |
| Ctrl+M | Renommer marqueur |
| Molette | Zoom horizontal |
| Ctrl+Molette | Zoom vertical |
| Clic molette + drag | Pan |

### Dope Sheet

Vue détaillée par objet et canal.

#### Structure du Dope Sheet

```
▼ 🦴 Armature_Perso          ●●●○     ●●        ●
  ▼ 🔴 os_hanche_G           ●  ●     ● ●       ●
      📐 Rotation X           ●  ●     ● ●       ●
      📐 Rotation Y           ●         ●
      📐 Rotation Z             ●       ●        ●
  ▼ 🔴 os_epaule_D            ●   ●       ●      ●
▼ 📷 Camera.001              ●              ●
▼ 💡 Lumière_principale       ●    ●
```

- Arborescence repliable/dépliable
- Icônes de type (armature, mesh, caméra, lumière, etc.)
- Canaux cliquables pour sélection individuelle
- Keyframes repositionnables par glisser

#### Header Dope Sheet

```
[Dope Sheet ▼] [🔍 Filtre] [🔒 Only Selected] [⬛ Summary] [Sort▼] [Sync]
```

- **Filtre** : chercher par nom de canal/objet
- **Only Selected** : n'affiche que l'objet sélectionné dans le viewport
- **Summary** : réduit tout en une seule ligne de résumé
- **Sort** : par nom, par type, par ordre de création

#### Raccourcis Dope Sheet

| Raccourci | Action |
|-----------|--------|
| `A` | Sélectionner tous les keyframes |
| `B` | Box select keyframes |
| `G` | Déplacer keyframes sélectionnés |
| `S` | Scaler keyframes |
| `X` | Supprimer keyframes |
| `Ctrl+C` | Copier keyframes |
| `Ctrl+V` | Coller keyframes |
| `Shift+D` | Dupliquer keyframes |
| `I` | Insérer keyframe |
| `T` | Changer le type d'interpolation (pie menu) |
| `V` | Changer le type de handle bezier |
| `H` | Masquer le canal |
| `Alt+H` | Afficher tous les canaux |
| `Ctrl+G` | Grouper les canaux sélectionnés |
| `Tab` | Basculer Action Editor |
| `Ctrl+Shift+F` | Frame (zoomer sur la sélection) |

### NLA Editor (Non-Linear Animation)

Vue pour assembler et mixer des clips d'animation.

#### Concept

Chaque action est représentée comme un **clip** (bloc coloré). Les clips peuvent être :
- Placés sur des pistes indépendantes
- Mis en boucle (loop)
- Étirés / compressés en temps
- Mixés par blend (fondu entre clips)
- Empilés (piste inférieure = priorité, sauf mode blend)

#### Interface NLA

```
▼ 🦴 Armature_Perso
  📽 [Action: Marche_01  ██████████]  [Action: Course_02  ████]
  📽                                          [Action: Saut_01  ██]
  🤖 [AI Motion: Atterrissage_gen  ████████]
```

- Pistes glissables verticalement
- Clips redimensionnables (poignées latérales)
- Blend In/Out : zones de transparence aux extrémités des clips
- Clic droit sur clip → **Unlink**, **Duplicate**, **Set Blend Mode**

---

## 4. Graph Editor (Courbes F-Curve)

### Description

Éditeur de courbes d'animation (F-Curves) pour un contrôle précis de l'interpolation entre keyframes. Essentiel pour obtenir des animations naturelles et non mécaniques.

### Interface

#### Header

```
[Graph Editor ▼] [🔍 Filtre] [Only Selected] [Normalize] [Auto Clamping] [Handle Type▼]
```

#### Corps

Espace 2D avec :
- **Axe X** : temps (frames)
- **Axe Y** : valeur du canal (position, rotation, etc.)
- **Courbes colorées** : une par canal (rouge=X, vert=Y, bleu=Z)
- **Keyframes** : points sur les courbes, avec handles Bezier ajustables
- **Handles** : tangentes de contrôle (vecteurs bleus)

#### Types de Handles (T dans le viewport)

- **Auto** — Blender calcule automatiquement la tangente (lissage naturel)
- **Auto Clamped** — Auto mais évite le dépassement de valeur
- **Vector** — Transition angulaire (linéaire)
| **Aligned** — handles alignés mais longueur indépendante
- **Free** — handles totalement libres
- **Ease In** — accélération progressive
- **Ease Out** — décélération progressive
- **Ease In-Out** — accélération puis décélération

#### Types d'Interpolation (T sur keyframe)

- **Constant** — saut brutal (états booléens)
- **Linear** — interpolation droite
- **Bezier** — lissage naturel (défaut)
- **Bounce** — rebond physique
- **Elastic** — élastique
- **Sine / Quad / Cubic / Quart / Quint / Expo / Circ** — easing functions

#### Raccourcis Graph Editor

| Raccourci | Action |
|-----------|--------|
| `A` | Sélectionner toutes les courbes/keyframes |
| `B` | Box select |
| `G` | Déplacer keyframes |
| `S` | Scaler |
| `R` | Rotation du groupe de keyframes |
| `T` | Changer interpolation |
| `V` | Changer type de handle |
| `H` | Masquer la courbe |
| `Alt+H` | Afficher toutes les courbes |
| `N` | Panneau latéral (valeurs exactes) |
| `Ctrl+G` | Activer/désactiver normalisation |
| `Home` | Zoomer sur tout |
| `Numpad .` | Zoomer sur la sélection |
| `Ctrl+Clic` | Ajouter un keyframe sur la courbe |
| `Ctrl+L` | Lier les handles |

#### Panneau Latéral Graph Editor (N)

Onglets :
- **View** : plage affichée, grille
- **Curve** : nom du canal, couleur, extrapolation
- **Key** : valeur exacte du keyframe sélectionné, frame, handle type
- **Drivers** : pilotes d'expression (lier une propriété à une autre)
- **AI Suggest** : la courbe idéale suggérée par l'IA en overlay

---

## 5. Panneau de Hiérarchie / Outliner

### Description

Arborescence de tous les objets de la scène. Équivalent du panneau Scène/Hiérarchie de Blender ou de l'Outliner de Cascadeur. Permet la sélection, l'organisation, le masquage et le renaming.

### Interface

#### Header

```
[🔍 Recherche        ] [🎬 Scene ▼] [⚙ Options]
```

#### Corps

```
▼ 📁 Scène_Principale
  ▼ 📁 Personnages
    ▼ 🦴 Armature_Hero        👁 🎬 🔒
      ├ 🟠 Mesh_Corps
      ├ 🟠 Mesh_Tete
      └ 🟠 Mesh_Vetements
    ▼ 🦴 Armature_PNJ_01      👁 🎬 🔒
  ▼ 📁 Environnement
    ├ 🟠 Sol
    ├ 🟠 Mur_G
    └ 🟠 Mur_D
  ▼ 📁 Cameras
    ├ 📷 Camera_Master
    └ 📷 Camera_Close
  ▼ 📁 Lumières
    ├ 💡 Sun
    └ 💡 Fill_Light
  ▼ 📁 Physics
    └ 🔵 RigidBody_Debris_01
```

#### Icônes de Visibilité (par ligne)

- 👁 **Visibilité viewport** (clic pour basculer)
- 🎬 **Visibilité rendu** (clic pour exclure du rendu)
- 🔒 **Verrouillage** (empêche la sélection accidentelle)
- ⚙ **Icône type** (mesh, armature, caméra, lumière, collection…)

#### Raccourcis Outliner

| Raccourci | Action |
|-----------|--------|
| `Clic` | Sélectionner l'objet |
| `Shift+Clic` | Sélection multiple |
| `Clic sur ▶` | Déplier/replier l'arborescence |
| `A` | Déplier tout |
| `Ctrl+A` | Replier tout |
| `Ctrl+Clic` | Sélectionner récursivement |
| `F2` | Renommer l'élément |
| `X` | Supprimer (avec confirmation) |
| `M` | Déplacer dans une collection |
| `Shift+M` | Lier à une collection |
| `G` | Glisser-déposer pour réordonner |
| `H` | Masquer / Afficher |
| `Ctrl+H` | Masquer récursivement |
| `Ctrl+F` | Filtrer / Chercher |

#### Filtre de l'Outliner

Panneau déroulant `⚙ Options` :
- Afficher : Mesh, Armature, Camera, Light, Curve, Empty, Force Field, Collection, Constraint
- Trier : par nom, par type, par date de création
- Recherche : insensible à la casse, wildcards

---

## 6. Panneau Propriétés

### Description

Panneau vertical droit avec onglets pour accéder à toutes les propriétés de l'objet sélectionné, de la scène, du matériau, de la physique, etc.

### Onglets (icônes verticaux)

#### 🌐 Propriétés de Scène

- **Unités** : métriques / impériales, facteur d'échelle
- **Gravité** : vecteur XYZ (défaut 0, -9.81, 0)
- **Plage de temps** : frame start/end, FPS
- **Audio** : piste audio de référence
- **Keying Sets** : ensembles de propriétés à keyer simultanément

#### 🌍 Propriétés Monde

- **Environnement HDRI** : image HDR pour l'éclairage ambiant
- **Couleur de fond** : couleur ou image
- **Brouillard** : distance, densité, couleur
- **Ambient Occlusion** : intensité, rayon, samples

#### 🎥 Propriétés Caméra (si sélectionnée)

- Focal length (mm), F-Stop, Sensor Size
- Clipping Near/Far
- DOF (Depth of Field) — distance de mise au point
- Shake/Handheld — bruit de caméra simulé
- Motion Blur — intensité, shutter angle

#### 🟠 Propriétés Objet

- **Transform** : position, rotation, échelle
- **Relations** : parent, collection
- **Visibilité** : viewport, rendu
- **Motion Blur** : objet spécifique
- **Ligne de contour** : pour rendu stylisé

#### 🦴 Propriétés Armature

- Mode d'affichage des os (Octahedron, Stick, B-Bone, Envelope, Wire)
- Afficher : Axes, Noms, In Front
- **Pose Library** : bibliothèque de poses enregistrées
- **B-Bones** : courbe d'os (segments, ease in/out)

#### ⚙ Propriétés d'Os (en Pose Mode)

- **Contraintes** : liste des contraintes (IK, Copy Rotation, Limit Rotation…)
  - Ajouter contrainte : `Ctrl+Shift+C`
  - Chaque contrainte affiche : cible, influence (0-1), curseur d'activation
- **B-Bone** : segments, ease in/out, courbure
- **Inverse Kinematics** : si l'os est une cible IK
- **Custom Shape** : forme custom d'affichage de l'os

#### 🔵 Propriétés Physique

Voir section dédiée [§10](#10-physics-simulation-panel).

#### 🔴 Propriétés Particules

- Systèmes de particules (cheveux, tissus, poussière)
- Émission, vélocité, forces
- Collision

#### 🟡 Propriétés Contraintes Objet

- Liste des contraintes d'objet (Copy Location, Track To, Floor…)
- Mêmes options que contraintes d'os

#### 🟢 Propriétés Matériau

- Matériaux assignés au mesh (liste)
- Shader simplifié : albedo, roughness, metallic, normal
- Lien vers le Node Editor pour édition avancée
- **Skin Shader** (SSS) pour les personnages
- **Cloth Shader** pour les tissus

#### 💎 Propriétés Shader Data

- UV Maps
- Color Attributes
- Shape Keys (morphing)

---

## 7. Viewport d'Animation (Animation Preview)

### Description

Fenêtre secondaire dédiée à la prévisualisation de l'animation avec vue split multi-angles, comparaison avant/après, et outils d'analyse de mouvement (trajectoires, courbes de vitesse, analyse d'énergie).

### Modes du Viewport Animation

#### Mode Split (2 ou 4 vues)

```
┌─────────────────┬─────────────────┐
│   Vue Caméra    │   Vue Côté      │
│   (rendu film)  │   (technique)   │
├─────────────────┼─────────────────┤
│   Vue Face      │  Vue Perspective│
│   (technique)   │   (preview)     │
└─────────────────┴─────────────────┘
```

- Chaque vue peut avoir son propre shading
- Synchronisation des têtes de lecture
- Freeze Frame indépendant par vue

#### Mode Comparaison A/B

- Vue splitée verticalement : animation actuelle (A) vs référence (B)
- B peut être : un rendu précédent, une vidéo de référence, une animation IA
- Déplacement du split par glisser
- Synchronisation du playback

#### Onglets du Viewport Animation

- **Preview** — visualisation simple
- **Trajectory** — courbes de trajectoire des effecteurs et os clés
- **Speed Graph** — courbe de vitesse (magnitude du vecteur vitesse) sur le temps
- **Energy** — énergie cinétique et potentielle (physique)
- **Reference** — vidéo/image de référence en overlay (rotoscoping)
- **AI Overlay** — projection de la suggestion IA en overlay transparent

#### Outils d'Analyse

- **Trajectoire** : cliquer sur un os/objet → afficher sa trajectoire dans l'espace
  - Éditer la trajectoire directement (drag de points)
  - Baking de la trajectoire → keyframes
- **Centre de masse global** : affichage du centre de masse du personnage complet
- **Ligne d'action** : outil de Cascadeur — flèche principale de la force de mouvement
- **Vecteurs de vitesse** : flèches aux effecteurs indiquant direction et magnitude
- **Analyse d'anticipation** : couleurs indiquant les frames d'anticipation/follow-through (rouge = anticipation, vert = action, bleu = follow-through)

#### Raccourcis Viewport Animation

| Raccourci | Action |
|-----------|--------|
| `Space` | Play/Pause |
| `←` / `→` | Frame précédent / suivant |
| `Shift+←` | 10 frames en arrière |
| `Shift+→` | 10 frames en avant |
| `Ctrl+←` | Keyframe précédente |
| `Ctrl+→` | Keyframe suivante |
| `Home` | Revenir au frame de début |
| `End` | Aller au frame de fin |
| `L` | Boucler la plage de préview |
| `Ctrl+T` | Afficher la trajectoire de l'os sélectionné |
| `Ctrl+E` | Afficher graphe d'énergie |
| `Ctrl+V` | Afficher vecteurs de vitesse |
| `F` | Freeze Frame (figer cette vue) |
| `R` | Mode Référence (charger vidéo) |
| `Shift+R` | Aligner la référence sur le frame courant |

---

## 8. Éditeur de Rig & Armature

### Description

Interface dédiée à la création, édition et configuration des armatures (squelettes) et des systèmes de rigging. Inspiré de Blender mais avec des helpers physiques intégrés.

### Modes

- **Edit Mode (Armature)** — créer et positionner les os
- **Pose Mode** — animer les os
- **Rig Setup Mode** — configurer IK, contraintes, contrôleurs custom

### Interface Edit Mode Armature

#### Outils Spécifiques (Toolbar)

- 🦴 **Add Bone** `Shift+A`
- ✂ **Extrude Bone** `E`
- 🔗 **Subdivide Bone** (diviser un os en plusieurs)
- 🎯 **Set Root / Tip** (redéfinir orientation)
- 🔄 **Roll Bone** `Ctrl+R` (rotation d'axe)
- 📐 **Align Bones** (aligner sur un axe mondial/local)
- 🔀 **Mirror Bones** `Shift+Ctrl+M`

#### Panneau Latéral Edit Mode

- **Bone Name** : F2 ou double clic pour renommer
- **Roll** : rotation de l'os sur son axe
- **Inherit Rotation** : héritage de la rotation parent
- **Connected** : os connecté au parent (snap automatique)
- **Deform** : cet os déforme-t-il le mesh ?
- **B-Bone Settings** : courbe, segments, ease

#### Rig Setup Mode

Interface spéciale pour :
- **IK Chains** : définir les chaînes IK, le pole target, le nombre de segments
- **FK/IK Switch** : slider de blending FK/IK par chaîne
- **Stretch Bones** : os élastiques avec contrôle de volume
- **Spline IK** : colonne vertébrale, queue sur courbe NURBS
- **Custom Controls** : assigner des shapes custom aux os contrôleurs
- **Proxy Rig** : rig simplifié pour animation rapide, lié au rig complet

#### Outils Auto-Rig

- **Auto-Rig Humanoïde** : placement automatique via détection de landmarks
  1. Importer le mesh
  2. Cliquer "Detect Humanoid"
  3. Ajuster les 12 points clés sur le mesh
  4. Générer l'armature
  5. Skin automatique (vertex weights)
- **Auto-Rig Quadrupède**
- **Auto-Rig Créature Custom** : basé sur un template éditables
- **Retarget** : transférer une animation d'une armature vers une autre

#### Raccourcis Edit Mode Armature

| Raccourci | Action |
|-----------|--------|
| `E` | Extruder un os |
| `Shift+A` | Ajouter un os |
| `Ctrl+R` | Roll de l'os |
| `Ctrl+N` | Recalculer le roll automatiquement |
| `F` | Connecter les os sélectionnés |
| `Alt+F` | Déconnecter les os |
| `Ctrl+Alt+A` | Aligner les os |
| `Shift+Ctrl+M` | Mirror par rapport à un axe |
| `L` | Sélectionner une chaîne d'os connectés |
| `Shift+L` | Ajouter une chaîne à la sélection |
| `[` | Sélectionner le parent |
| `]` | Sélectionner les enfants |
| `Ctrl+[` | Sélectionner tous les parents |
| `Ctrl+]` | Sélectionner tous les enfants |
| `P` | Séparer les os (nouveau rig) |
| `Ctrl+P` | Parent (connexion) |
| `Alt+P` | Supprimer parent |
| `H` | Masquer les os |
| `Shift+H` | Masquer les os non-sélectionnés |
| `Alt+H` | Afficher tout |

---

## 9. Interface IA (AI Director Panel)

### Description

Interface dédiée aux fonctionnalités d'intelligence artificielle. Panneau flottant ou ancré, permettant de piloter l'animation par IA, générer des poses, compléter des animations, analyser la qualité physique, et suggérer des corrections.

### Sous-Panneaux

#### 9.1 AI Pose Assistant

Fenêtre principale de suggestion de pose.

```
┌─────────────────────────────────────────────┐
│  🤖 AI POSE ASSISTANT                       │
├─────────────────────────────────────────────┤
│  Mode: [Suggest ▼]  Cible: [Personnage ▼]  │
├─────────────────────────────────────────────┤
│  Prompt textuel:                            │
│  ┌─────────────────────────────────────────┐│
│  │ "personnage qui saute pour attraper    ││
│  │  une balle au dessus de lui"           ││
│  └─────────────────────────────────────────┘│
│  [🎙 Vocal]  [📷 Photo Ref]  [🎬 Vidéo Ref] │
├─────────────────────────────────────────────┤
│  Suggestions (4 variantes):                 │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐      │
│  │ V1 ✓│ │  V2  │ │  V3  │ │  V4  │      │
│  │[img] │ │[img] │ │[img] │ │[img] │      │
│  └──────┘ └──────┘ └──────┘ └──────┘      │
│  Variante sélectionnée: 1                   │
├─────────────────────────────────────────────┤
│  Influence: [████████░░] 80%               │
│  Conserver: [☑ Contacts Sol] [☑ Mains]    │
│  [Appliquer]  [Bake en Keyframe]  [Rejeter]│
└─────────────────────────────────────────────┘
```

- **Mode** : Suggest (proposer), Complete (compléter un mouvement), Correct (corriger la physique), Mirror (miroir gauche/droite), Interpolate (remplir les inbetweens)
- **Cible** : tous les personnages, ou un personnage spécifique
- **Prompt** : texte libre décrivant l'action désirée
- **Vocal** : dictée vocale → prompt texte (microphone)
- **Photo Ref** : importer une photo comme référence de pose
- **Vidéo Ref** : analyser une vidéo (motion capture simplifié)
- **Variantes** : 4 propositions visuelles, sélectionnable par clic ou flèches `←/→`
- **Influence** : blend entre la pose actuelle et la suggestion IA (slider 0-100%)
- **Conserver** : points d'ancrage à ne pas déplacer (pieds au sol, mains en contact)

#### 9.2 AI Motion Completion

Outil pour compléter automatiquement une animation partielle.

```
┌─────────────────────────────────────────────┐
│  🤖 AI MOTION COMPLETION                   │
├─────────────────────────────────────────────┤
│  Animation source:                          │
│  Frame A: [001] → Frame B: [048]           │
│  Frames à compléter: [015] à [032]         │
├─────────────────────────────────────────────┤
│  Style: [Naturel ▼]  Énergie: [████░░] 70%│
│  Respecter la physique: [☑]               │
│  Maintenir les contacts: [☑]              │
├─────────────────────────────────────────────┤
│  [🎬 Prévisualiser]  [✅ Appliquer]        │
└─────────────────────────────────────────────┘
```

#### 9.3 AI Physics Check

Analyse la correction physique de l'animation.

```
┌─────────────────────────────────────────────┐
│  ⚖ AI PHYSICS CHECK                        │
├─────────────────────────────────────────────┤
│  Frame courante: 042                        │
│  Score physique: ██████████░░ 78%          │
├─────────────────────────────────────────────┤
│  Problèmes détectés:                        │
│  ⚠ [F.032] Centre de masse instable        │
│  ⚠ [F.038] Pied droit flottant (0.3cm)    │
│  ✅ [F.042] Contact mains correct          │
│  ⚠ [F.045-50] Rotation hanche irréaliste  │
├─────────────────────────────────────────────┤
│  [🔧 Corriger automatiquement]             │
│  [🔧 Corriger frame par frame]             │
│  [📊 Rapport complet]                       │
└─────────────────────────────────────────────┘
```

- Score global de réalisme physique
- Liste des problèmes par frame avec gravité
- Correction automatique (modifie les keyframes concernés)
- Rapport exportable en PDF/CSV

#### 9.4 AI Motion Style Transfer

Transférer le style d'une animation vers une autre.

```
┌─────────────────────────────────────────────┐
│  🎨 AI STYLE TRANSFER                      │
├─────────────────────────────────────────────┤
│  Source style: [Charger animation...]      │
│  Cible: [Animation courante]               │
├─────────────────────────────────────────────┤
│  Paramètres:                                │
│  Timing:   [████████░░] 80%               │
│  Amplitude:[██████░░░░] 60%               │
│  Style:    [████░░░░░░] 40%               │
├─────────────────────────────────────────────┤
│  [Prévisualiser]  [Appliquer]              │
└─────────────────────────────────────────────┘
```

#### 9.5 AI Mocap Cleanup

Nettoyer les données de motion capture brutes.

```
┌─────────────────────────────────────────────┐
│  🧹 AI MOCAP CLEANUP                       │
├─────────────────────────────────────────────┤
│  Source: [Charger BVH/FBX...]             │
│  Filtrage bruit:    [████████░░] 80%      │
│  Fixation pieds:   [☑] Seuil: [0.5cm]   │
│  Smoothing global: [████░░░░░░] 40%      │
│  Conserver spikes: [☐]                    │
│  Retargeter vers:  [Armature_Hero ▼]     │
├─────────────────────────────────────────────┤
│  [Analyser]  [Nettoyer]  [Appliquer]      │
└─────────────────────────────────────────────┘
```

#### Raccourcis AI Panel

| Raccourci | Action |
|-----------|--------|
| `Ctrl+Alt+P` | Ouvrir AI Pose Assistant |
| `Ctrl+Alt+C` | Lancer AI Physics Check |
| `Ctrl+Alt+M` | AI Motion Completion |
| `Ctrl+Alt+S` | AI Style Transfer |
| `Ctrl+Alt+G` | Générer variante IA (1 clic) |
| `Ctrl+Shift+A` | Appliquer la suggestion IA active |
| `←/→` | Naviguer entre les variantes IA |
| `Escape` | Rejeter la suggestion IA |
| `Ctrl+Z` | Annuler l'application IA |
| `Ctrl+Alt+V` | Afficher/masquer l'overlay IA dans le viewport |

---

## 10. Physics Simulation Panel

### Description

Interface complète pour configurer et simuler la physique dans l'application. Inspiré de Cascadeur pour la physique des personnages et de Blender pour la simulation générale.

### Types de Simulation

#### 10.1 Rigid Body (Corps Rigides)

Panneau dans Propriétés > Physique > Rigid Body

```
┌─────────────────────────────────────────┐
│  🔵 RIGID BODY                         │
├─────────────────────────────────────────┤
│  Type: [Active ▼]  (Active/Passive)    │
│  Masse: [10.5 kg]                      │
│  Friction: [0.8]  Restitution: [0.2]   │
├─────────────────────────────────────────┤
│  Collision Shape:                       │
│  [Box ▼]  (Box, Sphere, Capsule,       │
│           Convex Hull, Mesh, Compound)  │
│  Margin: [0.04 m]                      │
├─────────────────────────────────────────┤
│  Linear Damping:  [████░░] 0.04       │
│  Angular Damping: [████░░] 0.10       │
└─────────────────────────────────────────┘
```

#### 10.2 Soft Body (Corps Mous)

- **Objectif** : chairs, jellys, tissus organiques mous
- **Goal** : attraction vers la forme originale (0=totalement mou, 1=rigide)
- **Edges** : raideur des arêtes, amortissement
- **Face** : pression interne (gonflement)
- **Self Collision** : collision avec lui-même

#### 10.3 Cloth (Tissu)

```
┌─────────────────────────────────────────┐
│  🧵 CLOTH SIMULATION                   │
├─────────────────────────────────────────┤
│  Preset: [Coton ▼]  (Soie, Cuir, Denim)│
├─────────────────────────────────────────┤
│  Tension:     [████████░░] 15.0       │
│  Compression: [██████░░░░] 15.0       │
│  Shear:       [████░░░░░░]  5.0       │
│  Bending:     [██░░░░░░░░]  0.5       │
├─────────────────────────────────────────┤
│  Collisions: [☑]  Self: [☑]          │
│  Distance: [0.02m]  Friction: [5.0]  │
├─────────────────────────────────────────┤
│  Pinning: [Vertex Group: Pin ▼]       │
├─────────────────────────────────────────┤
│  [▶ Simuler]  [⏸ Pause]  [🔄 Reset]  │
│  [💾 Bake]  [🗑 Delete Bake]          │
└─────────────────────────────────────────┘
```

#### 10.4 Fluide

- **Type** : Domaine, Émetteur, Obstacle, Récepteur
- **Domaine** : résolution, viscosité, gravité
- **Bake** : calculer la simulation et stocker les données
- **Rendu** : Surface, Spray, Foam, Bulles

#### 10.5 Character Physics (Physique Personnage)

Module spécifique aux personnages animés (inspiré de Cascadeur) :

```
┌─────────────────────────────────────────┐
│  🦴 CHARACTER PHYSICS                  │
├─────────────────────────────────────────┤
│  Poids total: [75.0 kg]               │
│  Taille: [1.80 m]                     │
├─────────────────────────────────────────┤
│  ▼ Segments de masse                   │
│  Tête:         [6.0 kg]  [5%]         │
│  Torse:       [25.0 kg]  [33%]        │
│  Bassin:      [12.0 kg]  [16%]        │
│  Bras G/D:    [4.0 kg each]           │
│  Avant-bras:  [2.5 kg each]           │
│  Mains:       [0.6 kg each]           │
│  Cuisses:     [9.5 kg each]           │
│  Jambes:      [4.5 kg each]           │
│  Pieds:       [1.2 kg each]           │
├─────────────────────────────────────────┤
│  Afficher:                             │
│  [☑] Centre de masse global           │
│  [☑] Centre de masse par segment      │
│  [☑] Polygone de support (base)       │
│  [☑] Vecteur de force principale      │
│  [☐] Ellipsoïdes d'inertie            │
├─────────────────────────────────────────┤
│  Auto-balance:  [☐] (mode IA)         │
│  Seuil alerte: [5°] déséquilibre      │
└─────────────────────────────────────────┘
```

#### 10.6 Bake Panel (Cuisson des Simulations)

```
┌─────────────────────────────────────────┐
│  💾 BAKE SIMULATION                    │
├─────────────────────────────────────────┤
│  Frame start: [001]  End: [250]        │
│  Cache: [/cache/sim_001/]             │
│  Format: [BPhys ▼]                    │
├─────────────────────────────────────────┤
│  [▶ Bake]         Progression: 0%     │
│  [▶ Bake Sound]                       │
│  [🗑 Delete All]                       │
└─────────────────────────────────────────┘
```

#### Raccourcis Physics Panel

| Raccourci | Action |
|-----------|--------|
| `Ctrl+Shift+B` | Lancer le Bake |
| `Alt+B` | Supprimer le Bake |
| `Ctrl+Alt+B` | Bake de la sélection uniquement |
| `Ctrl+Shift+C` | Ajouter contrainte physique |
| `P` | Activer la physique sur l'objet |
| `Alt+P` | Désactiver la physique |

---

## 11. Motion Library & Asset Browser

### Description

Bibliothèque centralisée de mouvements, poses, assets 3D, matériaux, HDRI et effets. Interface de drag & drop pour intégrer rapidement les assets dans la scène.

### Structure de l'Interface

#### Header Asset Browser

```
[📁 Bibliothèques ▼] [🔍 Rechercher...] [🏷 Tags] [⚙ Vue: Grille/Liste] [⬆ Import] [+ Nouveau]
```

#### Panneau Gauche — Arborescence des Sources

```
▼ 📁 Bibliothèques Locales
  ├ 🏃 Motions
  ├ 🤸 Poses
  ├ 🟠 Meshes
  ├ 🎨 Matériaux
  └ 🌅 HDRI
▼ 📁 Projet Courant
  ├ Actions
  └ Poses
▼ ☁ Bibliothèques Cloud
  ├ Mixamo (via plugin)
  ├ Sketchfab (via plugin)
  └ Marketplace IA
```

#### Zone Centrale — Grille d'Assets

Chaque asset affiché avec :
- Vignette (preview image ou vidéo)
- Nom
- Tags
- Durée (pour les animations)
- Note de qualité physique (⭐ 1-5 étoiles)
- Indicateur IA (🤖 si généré par IA)

#### Panneau Droit — Détail de l'Asset

```
┌───────────────────────────────────┐
│  [Preview Animation]              │
├───────────────────────────────────┤
│  Nom: Marche_Athlétique_01       │
│  Durée: 32 frames (24fps)        │
│  OS: 65 os (Humanoid Standard)   │
│  Tags: marche, athlétique, sport │
│  Physique: ⭐⭐⭐⭐⭐            │
│  IA: ☑ Validé physiquement       │
├───────────────────────────────────┤
│  [👁 Prévisualiser sur rig]       │
│  [📥 Importer]  [🔗 Lier]       │
│  [➕ Ajouter à NLA]              │
└───────────────────────────────────┘
```

#### Drag & Drop

- Glisser une animation depuis l'Asset Browser vers le viewport → applique sur l'armature sélectionnée
- Glisser une pose → applique sur le frame courant
- Glisser un mesh → importe dans la scène
- Glisser vers la NLA Editor → ajoute comme clip

#### Raccourcis Asset Browser

| Raccourci | Action |
|-----------|--------|
| `Ctrl+F` | Chercher |
| `G` | Vue grille |
| `L` | Vue liste |
| `Enter` | Prévisualiser l'asset sélectionné |
| `I` | Importer l'asset |
| `Alt+I` | Lier l'asset (pas copier) |
| `Ctrl+T` | Ajouter des tags |
| `S` | Sauvegarder l'asset courant |
| `Shift+D` | Dupliquer l'asset |
| `X` | Supprimer l'asset |
| `Ctrl+E` | Exporter l'asset |

---

## 12. Render & Output Panel

### Description

Interface de rendu pour les productions jeu vidéo (temps réel, bake) et film (rendu haute qualité, couches, compositing). Supporte plusieurs moteurs de rendu.

### Moteurs de Rendu Disponibles

- **EEVEE Next** — rendu temps réel haute qualité (jeux, previz)
- **Cycles X** — rendu path-tracing (film, publicité)
- **Radeon ProRender** — alternatif GPU
- **Export Game Engine** — Unity, Unreal Engine (FBX/USD)

### Interface Render Panel

#### Onglet Rendu

```
┌─────────────────────────────────────────┐
│  🎬 RENDER ENGINE: [Cycles X ▼]        │
├─────────────────────────────────────────┤
│  Résolution: [1920] x [1080]  [100%]   │
│  Aspect: [16:9 ▼]  Lock: [☑]          │
│  FPS: [24]  Subframes: [0]             │
├─────────────────────────────────────────┤
│  Frame Range: [001] à [250]            │
│  Step: [1]  (rendre 1 frame sur X)    │
├─────────────────────────────────────────┤
│  Samples: [256]  (Cycles)             │
│  Denoising: [☑] [Intel Open Image ▼] │
│  Seed: [0] [Anim: ☑]                 │
├─────────────────────────────────────────┤
│  Motion Blur: [☑]  Shutter: [0.5]    │
│  DOF: [☑] (depuis caméra)             │
└─────────────────────────────────────────┘
```

#### Onglet Output

```
┌─────────────────────────────────────────┐
│  📂 Output: [/renders/scene_001/####]  │
│  Format: [EXR Multi-couches ▼]         │
│  Compression: [ZIP]  Quality: [100%]   │
├─────────────────────────────────────────┤
│  ▼ Couches (Render Passes)             │
│  [☑] Combined  [☑] Depth  [☑] Normal │
│  [☑] Diffuse   [☑] Specular          │
│  [☑] Shadow    [☑] AO                │
│  [☑] Motion Vector (pour comp)        │
├─────────────────────────────────────────┤
│  [F12 Rendu Frame]  [Ctrl+F12 Animer] │
└─────────────────────────────────────────┘
```

#### Onglet Export Game Engine

```
┌─────────────────────────────────────────┐
│  🎮 EXPORT GAME ENGINE                 │
├─────────────────────────────────────────┤
│  Format: [FBX ▼]  (FBX, USD, glTF)   │
│  Cible: [Unreal Engine 5 ▼]           │
├─────────────────────────────────────────┤
│  [☑] Armature  [☑] Animations        │
│  [☑] Matériaux [☑] Textures (baked)  │
│  [☑] LODs      [☑] Collisions        │
├─────────────────────────────────────────┤
│  Échelle: [1.00]  Axe Up: [Z ▼]      │
├─────────────────────────────────────────┤
│  [📤 Exporter la scène]               │
│  [📤 Exporter la sélection]           │
└─────────────────────────────────────────┘
```

#### Raccourcis Render

| Raccourci | Action |
|-----------|--------|
| `F12` | Rendre le frame courant |
| `Ctrl+F12` | Rendre l'animation |
| `Escape` | Annuler le rendu |
| `F11` | Afficher le dernier rendu |
| `Ctrl+Alt+F12` | Rendre en bake (physique → textures) |

---

## 13. Node Editor (Shader / Compositing / Rigging)

### Description

Éditeur de nœuds polyvalent utilisé pour les shaders, le compositing post-rendu, et le rigging procédural.

### Modes du Node Editor

```
[Shader Editor] [Compositing] [Geometry Nodes] [Rig Nodes]
```

#### Shader Editor

Interface de création de matériaux par nœuds.

- **Nœuds d'entrée** : Texture Coordinate, Vertex Color, Geometry, Camera Data
- **Nœuds textures** : Image Texture, Noise, Voronoi, Wave, Gradient, Magic
- **Nœuds couleur** : Mix, RGB Curves, Hue/Sat, Color Ramp
- **Nœuds vecteur** : Normal Map, Bump, Displacement
- **Nœuds BSDF** : Principled BSDF, Diffuse, Specular, Translucent, Glass, SSS
- **Nœuds de sortie** : Material Output, AOV Output

Raccourcis Node Editor :

| Raccourci | Action |
|-----------|--------|
| `Shift+A` | Ajouter un nœud |
| `Ctrl+Shift+A` | Chercher un nœud |
| `X` | Supprimer nœud(s) |
| `G` | Déplacer nœud(s) |
| `Ctrl+G` | Grouper les nœuds |
| `Alt+G` | Dégrouper |
| `Tab` | Entrer/sortir d'un groupe |
| `H` | Masquer les prises non-connectées |
| `Ctrl+T` | Ajouter un nœud Texture Coordinate + Mapping |
| `F` | Connecter deux prises (frame selection) |
| `Ctrl+L` | Lier les nœuds par leur nom |
| `M` | Mute (désactiver temporairement) |
| `Ctrl+Shift+Clic` | Prévisualiser la sortie d'un nœud |

#### Compositing

Post-traitement après rendu :

- **Input** : couches EXR, Image, Movie, Mask, Scene
- **Transformations** : Crop, Scale, Flip, Translate, Rotate, Stabilize 2D
- **Filtres** : Blur, Sharpen, Despeckle, Glare, Lens Distortion, Vignette
- **Couleur** : Color Balance, Curves, Exposure, Hue Correction, Tone Map
- **Keying** : Chroma Key, Color Key, Difference Matte
- **Output** : Composite, File Output, Viewer

#### Rig Nodes (Rigging Procédural)

Interface de rigging par nœuds (pour rigs complexes et procéduraux) :

- **Bone Input/Output** : nœuds de lecture/écriture des transformations d'os
- **IK Solver** : nœud de résolution IK paramétrable
- **Math** : opérations vectorielles, matrices
- **Physics Constraint** : nœuds de contrainte physique
- **AI Rig** : nœud d'assistance IA pour pose dynamique

---

## 14. Camera & Cinématique Panel

### Description

Interface dédiée à la gestion des caméras, à la cinématographie virtuelle et aux mouvements de caméra pour le cinéma.

### Propriétés Caméra

```
┌─────────────────────────────────────────┐
│  📷 CAMERA: Camera_Master              │
├─────────────────────────────────────────┤
│  ▼ Optique                             │
│  Focal Length: [50mm]  [Preset: ▼]    │
│  F-Stop: [2.8]  (aperture)            │
│  Sensor: [36mm Full Frame ▼]           │
│  Shift X: [0]  Shift Y: [0]           │
├─────────────────────────────────────────┤
│  ▼ Profondeur de champ                 │
│  Focus Object: [os_tete ▼]            │
│  Focus Distance: [3.50m]              │
│  Bokeh: [Disc ▼]  Blades: [6]         │
├─────────────────────────────────────────┤
│  ▼ Mouvement                           │
│  Shake: [0.5]  Frequency: [12Hz]      │
│  Follow Path: [☐]  [Chemin: ▼]       │
│  Track To: [os_tete ▼]                │
├─────────────────────────────────────────┤
│  ▼ Cinématographie IA                  │
│  Style: [Film Américain ▼]            │
│  [🤖 Auto-Frame le personnage]        │
│  [🤖 Suggérer angle cinématique]      │
└─────────────────────────────────────────┘
```

### Éditeur de Chemin Caméra

- Création de chemins NURBS/Bezier pour les mouvements de caméra
- Vitesse sur le chemin : constante, par keyframes, ou par courbe de vitesse
- Alignement automatique de la caméra sur le chemin

### Séquenceur de Coupes (Cut Manager)

```
┌─────────────────────────────────────────────────────────┐
│  🎬 CUT MANAGER                                         │
├────────────┬────────────┬────────────┬──────────────────┤
│ Cam_Master │ Cam_Close  │ Cam_POV    │ Cam_Top          │
│ ████████   │     ████   │  ██████    │       ████       │
│ 001-048    │  049-072   │ 073-120    │    121-160       │
└────────────┴────────────┴────────────┴──────────────────┘
```

- Timeline des coupes de caméra (cuts)
- Drag pour repositionner les coupes
- Clic droit → Transition (Cut, Fade, Dissolve)
- Prévisualisation en temps réel du montage

---

## 15. Audio & Sync Panel

### Description

Interface pour charger une piste audio, synchroniser l'animation, et utiliser le son comme guide pour le rythme des mouvements.

### Interface Audio

```
┌─────────────────────────────────────────┐
│  🎵 AUDIO                              │
├─────────────────────────────────────────┤
│  Piste: [son_reference.wav]  [📂]      │
│  Volume: [████████░░] 0.8             │
│  Offset: [+3 frames]                   │
│  Waveform: [☑ Afficher dans Timeline] │
├─────────────────────────────────────────┤
│  ▼ Synchronisation                     │
│  Sync Mode: [AV-sync ▼]               │
│  (Free, Frame Drop, AV-sync)           │
├─────────────────────────────────────────┤
│  ▼ Analyse Audio (IA)                  │
│  [🤖 Détecter les beats]              │
│  [🤖 Détecter les accents phonétiques]│
│  [📌 Créer marqueurs sur beats]       │
└─────────────────────────────────────────┘
```

- Détection automatique des beats → marqueurs dans la timeline
- Détection phonétique pour le lip-sync → marqueurs de visèmes
- Waveform visible en arrière-plan dans la timeline

---

## 16. Workspace Switcher & Layouts

### Description

L'application propose des espaces de travail (workspaces) prédéfinis pour les différentes phases de production. Accessible via la barre d'onglets en haut.

### Onglets de Workspace

```
[Layout] [Modeling] [Rigging] [Animation] [Physics] [AI Director] [VFX] [Render] [Compositing]
```

#### Layout (Vue Générale)

- Viewport 3D principal centré
- Outliner droite
- Propriétés droite (compact)
- Timeline bas

#### Modeling

- Viewport 3D grand format
- Outliner réduit
- Propriétés mesh détaillées
- N-Panel actif (mesures, overlay)

#### Rigging

- Viewport 3D (mode Pose)
- Outliner avec focus Armature
- Panneau propriétés os (détaillé)
- Small timeline bas

#### Animation (Principal)

- Viewport 3D (70% hauteur)
- Dope Sheet (30% hauteur)
- Graph Editor en onglet du Dope Sheet
- Outliner compact droite
- Propriétés droite

#### Physics

- Viewport 3D (mode Physics Debug)
- Physics Panel droite (étendu)
- Timeline bas avec indications bake
- Character Physics Panel

#### AI Director

- Viewport 3D (avec overlays IA)
- AI Director Panel droite (étendu)
- Animation Preview (petit, comparaison A/B)
- Timeline bas

#### VFX

- Viewport 3D
- Node Editor (Geometry Nodes / Shader)
- Asset Browser bas
- Particules/Fluides dans Propriétés

#### Render

- Viewport 3D (mode Rendered)
- Render Panel droite
- Output Panel
- Camera Panel

#### Compositing

- Node Editor (Compositing) - zone principale
- Image Viewer
- File Browser bas

### Création de Workspace Personnalisé

- Clic droit sur un onglet → **Dupliquer Workspace**
- Renommer, réorganiser les zones
- Sauvegarder le workspace personnalisé
- Exporter/Importer des configurations

---

## 17. Raccourcis Clavier Globaux

### Navigation & Interface

| Raccourci | Action |
|-----------|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Ctrl+S` | Sauvegarder le projet |
| `Ctrl+Shift+S` | Sauvegarder sous |
| `Ctrl+N` | Nouveau projet |
| `Ctrl+O` | Ouvrir un projet |
| `Ctrl+W` | Fermer le projet |
| `Ctrl+Q` | Quitter l'application |
| `F1` | Aide contextuelle |
| `F2` | Renommer l'élément actif |
| `F3` | Chercher une action (Command Palette) |
| `F4` | Menu File (contextuel) |
| `F5` | Rafraîchir / recalculer |
| `F8` | Recharger les scripts |
| `F9` | Panneau de dernière action (Adjust Last) |
| `F11` | Afficher le dernier rendu |
| `F12` | Rendre le frame courant |
| `Ctrl+F12` | Rendre l'animation |
| `Ctrl+Alt+S` | Sauvegarder une copie |
| `Tab` | Basculer mode Objet / Edit |
| `Ctrl+Tab` | Sélecteur de mode (pie menu) |
| `~` | Pie menu vue (Face, Côté, Dessus...) |
| `Space` | Play/Pause animation |
| `Shift+Space` | Ouvrir le menu d'actions |
| `` ` `` | Pie menu navigation viewport |
| `Ctrl+Space` | Maximiser la zone sous le curseur |
| `Ctrl+Alt+Space` | Voir en fullscreen |
| `Ctrl+Alt+Q` | Quad View (4 viewports) |

### Sélection

| Raccourci | Action |
|-----------|--------|
| `Clic gauche` | Sélectionner |
| `Shift+Clic` | Ajouter/retirer de la sélection |
| `Ctrl+Clic` | Sélection de chemin |
| `Alt+Clic` | Sélection de boucle |
| `A` | Sélectionner tout / Désélectionner |
| `Alt+A` | Désélectionner tout |
| `B` | Box Select |
| `C` | Circle Select (taille avec molette) |
| `Ctrl+I` | Inverser la sélection |
| `L` | Sélectionner connecté (sous le curseur) |
| `Ctrl+L` | Sélectionner connecté (sélectionné) |
| `Shift+G` | Sélectionner similaire |
| `[` | Sélectionner parent |
| `]` | Sélectionner enfants |
| `Ctrl+NumPad+` | Expand sélection |
| `Ctrl+NumPad-` | Réduire sélection |

### Transformation

| Raccourci | Action |
|-----------|--------|
| `G` | Déplacer |
| `G, X` | Déplacer sur axe X |
| `G, Y` | Déplacer sur axe Y |
| `G, Z` | Déplacer sur axe Z |
| `G, Shift+X` | Déplacer sur plan YZ |
| `G, Shift+Y` | Déplacer sur plan XZ |
| `G, Shift+Z` | Déplacer sur plan XY |
| `G, X, 2.5` | Déplacer de 2.5 sur X |
| `R` | Rotation |
| `R, X` | Rotation autour de X |
| `R, X, 45` | Rotation de 45° autour de X |
| `R, R` | Rotation libre (trackball) |
| `S` | Scale |
| `S, X` | Scale sur X |
| `S, X, 2` | Doubler la taille sur X |
| `S, Shift+X` | Scale sur plan YZ |
| `Alt+G` | Reset position |
| `Alt+R` | Reset rotation |
| `Alt+S` | Reset scale |
| `Ctrl+A` | Appliquer les transformations (menu) |
| `Ctrl+M` | Mirror (menu axe) |
| `Shift+Ctrl+Alt+C` | Set Origin (menu) |

### Animation

| Raccourci | Action |
|-----------|--------|
| `I` | Insérer keyframe (menu pie) |
| `Alt+I` | Supprimer keyframe |
| `Shift+I` | Insérer keyframe tous canaux |
| `K` | Insérer keyframe (outil visuel) |
| `J` | Frame précédente |
| `L` | Frame suivante |
| `Ctrl+J` | Keyframe précédente |
| `Ctrl+L` | Keyframe suivante |
| `Shift+Ctrl+E` | Extraire les poses comme actions séparées |
| `Ctrl+Alt+C` | Copier pose |
| `Ctrl+Alt+V` | Coller pose |
| `Ctrl+Alt+Shift+V` | Coller pose miroir |
| `Ctrl+E` | Propaguer la pose (appliquer aux keyframes suivants) |
| `Alt+P` | Effacer les rotations d'os |
| `Alt+G` | Effacer la position d'os |
| `Alt+S` | Effacer le scale d'os |
| `Shift+W` | Pie menu pose spéciale (rest, T-pose, A-pose) |

### Physique & IA

| Raccourci | Action |
|-----------|--------|
| `Ctrl+Alt+P` | Ouvrir AI Pose Assistant |
| `Ctrl+Alt+C` | AI Physics Check |
| `Ctrl+Alt+M` | AI Motion Completion |
| `Ctrl+Shift+A` | Appliquer suggestion IA |
| `Ctrl+Shift+P` | AI Physics Auto-correct |
| `Ctrl+Shift+B` | Bake simulation physique |
| `Ctrl+Shift+G` | Afficher/masquer le centre de masse |
| `Ctrl+Shift+V` | Afficher/masquer vecteurs de vitesse |
| `Ctrl+Shift+T` | Afficher/masquer trajectoires |

---

## 18. Actions Souris Globales

### Souris Standard (3 boutons + molette)

| Action Souris | Zone | Résultat |
|---------------|------|----------|
| **Clic Gauche** | Viewport | Sélectionner objet/os |
| **Clic Gauche maintenu** | Viewport | Déplacer si outil actif |
| **Double Clic Gauche** | Viewport | Entrer Edit/Pose Mode |
| **Clic Droit** | Viewport | Menu contextuel |
| **Clic Droit** | Os sélectionné | Menu spécifique os |
| **Molette Haut** | Viewport | Zoom avant |
| **Molette Bas** | Viewport | Zoom arrière |
| **Clic Molette maintenu** | Viewport | Orbite (rotation vue) |
| **Shift + Clic Molette** | Viewport | Pan (translation vue) |
| **Ctrl + Clic Molette** | Viewport | Zoom précis |
| **Clic Gauche** | Timeline | Déplacer tête de lecture |
| **Drag** | Timeline | Scrubbing rapide |
| **Clic Gauche** | Keyframe | Sélectionner keyframe |
| **Drag** | Keyframe | Déplacer keyframe |
| **Double Clic** | Keyframe | Éditer la valeur exacte |
| **Clic Molette** | Timeline | Pan horizontal |
| **Molette** | Timeline | Zoom horizontal |
| **Ctrl + Molette** | Timeline | Zoom vertical |
| **Clic Gauche** | Courbe F-Curve | Sélectionner point |
| **Ctrl + Clic Gauche** | Courbe F-Curve | Ajouter keyframe sur courbe |
| **Drag Handle** | F-Curve | Ajuster tangente Bezier |
| **Drag** | Bordure de zone | Redimensionner zones |
| **Clic Droit** | Bordure de zone | Diviser / Fusionner zones |
| **Clic Gauche** | Nœud | Sélectionner nœud |
| **Drag Port** | Node Editor | Créer connexion |
| **Drag corps nœud** | Node Editor | Déplacer nœud |
| **Molette** | Node Editor | Zoom |
| **Clic Molette** | Node Editor | Pan |
| **Clic Gauche** | Outliner item | Sélectionner |
| **Clic sur 👁** | Outliner | Basculer visibilité |
| **Double Clic** | Outliner | Renommer |
| **Drag** | Outliner | Réordonner / changer parent |

### Tablette Graphique (Wacom / Huion)

| Action | Résultat |
|--------|----------|
| Stylet clic | Sélectionner / dessiner |
| Stylet maintenu + drag | Orbite (comme clic molette) |
| Bouton 1 stylet | Clic droit (menu contextuel) |
| Bouton 2 stylet | Undo |
| Pression stylet | Intensité peinture de poids |
| Tilt stylet | Direction du coup de pinceau |
| Expresskey (configurable) | Raccourcis personnalisés |
| Touch ring | Zoom / rotation vue |

### Souris avec Boutons Additionnels

| Bouton | Action assignable |
|--------|-------------------|
| Bouton latéral 4 | Undo |
| Bouton latéral 5 | Redo |
| Bouton latéral (configurables) | Play/Pause, Frame suivant, etc. |

---

## 19. Barre de Menus Principale

### FILE

- **New** `Ctrl+N` — nouveau projet (sous-menu : Animation Film, Game Character, Physics Scene, Vide)
- **Open** `Ctrl+O` — ouvrir un projet
- **Open Recent** — derniers projets
- **Append** — importer des éléments d'un autre fichier
- **Link** — lier des éléments d'un autre fichier
- **Import** — FBX, OBJ, GLTF, BVH, Alembic, USD, SVG
- **Export** — FBX, OBJ, GLTF, BVH, Alembic, USD, FBX Game Engine
- **Save** `Ctrl+S`
- **Save As** `Ctrl+Shift+S`
- **Save Copy** `Ctrl+Alt+S`
- **Versions** — historique des versions du fichier
- **Preferences** — paramètres globaux de l'application
- **Quit** `Ctrl+Q`

### EDIT

- **Undo** `Ctrl+Z`
- **Redo** `Ctrl+Shift+Z`
- **Undo History** — liste des actions annulables
- **Repeat Last** `Shift+R`
- **Repeat History** `Ctrl+Alt+R`
- **Find** `F3` — chercher une commande
- **Operator Search** — recherche d'opérateurs par nom
- **Preferences** — réglages de l'éditeur
- **Lock Interface** — verrouiller toute modification accidentelle

### VIEW

- **Toggle Fullscreen** `Ctrl+Alt+Space`
- **Toggle Quad View** `Ctrl+Alt+Q`
- **Area** — diviser, joindre, dupliquer les zones
- **Workspace** — gérer les workspaces
- **Sidebar** `N` — afficher/masquer le panneau latéral
- **Toolbar** `T` — afficher/masquer la toolbar
- **Overlays** — gérer les overlays viewport
- **Shading** — changer le mode de shading
- **Viewport Render** — aperçu rendu

### OBJECT

- **Add** `Shift+A` — ajouter un objet (mesh, armature, caméra, lumière, empty, etc.)
- **Select All** `A`
- **Deselect All** `Alt+A`
- **Invert Selection** `Ctrl+I`
- **Transform** — sous-menu (move, rotate, scale, mirror)
- **Set Origin** — options d'origine
- **Apply** `Ctrl+A` — appliquer les transformations
- **Relations** — parent, group, collection
- **Duplicate** `Shift+D`
- **Duplicate Linked** `Alt+D`
- **Delete** `X`
- **Join** `Ctrl+J`
- **Visibility** — masquer, montrer, verrouiller
- **Convert** — convertir mesh, curve, nurbs
- **Animation** — sous-menu (insert key, clear keys, bake)
- **Rigid Body** — ajouter/supprimer physics
- **Constraints** — ajouter contrainte

### ANIMATION

- **Play** `Space`
- **Jump to Start** `Shift+Left`
- **Jump to End** `Shift+Right`
- **Previous Frame** `Left`
- **Next Frame** `Right`
- **Previous Keyframe** `Down`
- **Next Keyframe** `Up`
- **Insert Keyframe** `I`
- **Delete Keyframe** `Alt+I`
- **Keyframe Type** — changer le type de keyframe
- **Bake Action** — convertir la simulation en keyframes
- **Apply as Rest Pose** — définir la pose actuelle comme rest pose
- **Apply Pose Library** — bibliothèque de poses
- **AI** — sous-menu AI Director (voir §9)
- **NLA** — ouvrir l'éditeur NLA
- **Motion Paths** — afficher/calculer les trajectoires

### PHYSICS

- **Add Rigid Body** `Ctrl+Shift+R`
- **Add Soft Body**
- **Add Cloth**
- **Add Fluid** — domaine, émetteur, obstacle
- **Add Force Field** — vent, vortex, turbulence, gravity
- **Add Collision**
- **Bake All** `Ctrl+Shift+B`
- **Delete All Bakes**
- **Character Physics** — ouvrir le panneau physique personnage
- **Physics Debug** — activer le mode debug visuel

### AI

- **AI Pose Assistant** `Ctrl+Alt+P`
- **AI Physics Check** `Ctrl+Alt+C`
- **AI Motion Completion** `Ctrl+Alt+M`
- **AI Style Transfer**
- **AI Mocap Cleanup**
- **AI Auto-Rig** — rigging automatique
- **AI Cinematic Camera** — suggestions caméra
- **AI Settings** — paramètres du moteur IA (modèle, serveur)
- **Train Custom Model** — entraîner sur des données custom

### RENDER

- **Render Image** `F12`
- **Render Animation** `Ctrl+F12`
- **Render Audio** — rendu de la piste audio
- **View Render** `F11`
- **View Animation** `Ctrl+F11`
- **Cancel Render** `Escape`
- **Render Settings** — ouvrir le panneau de rendu
- **Slot** — gérer les slots de rendu (comparaison A/B)

### HELP

- **Manual** — documentation en ligne
- **Keyboard Shortcut Map** — carte des raccourcis
- **Tutorials** — tutoriels intégrés
- **Release Notes**
- **Report Bug**
- **About**

---

## 20. Barre d'Outils Contextuelle (Header Toolbar)

Chaque éditeur possède son propre header. Voici les éléments communs et spécifiques.

### Header Global (toujours présent)

```
[🔧 Mode ▼] [Workspace Tabs: Layout | Modeling | Rigging | Animation | Physics | AI | Render]
```

### Header Viewport 3D (Pose Mode)

```
[Pose Mode ▼] | [👁 Overlays ▼] [🎨 Shading ▼] [📐 Proportional ▼] [🔄 Transform Orientation ▼] [📌 Pivot ▼] [🧲 Snap ▼]
```

Chaque élément ouvre un panneau déroulant :

**Proportional Editing ▼**
- Activé / Désactivé `O`
- Mode : Sphérique, Racine, Linéaire, Sharp, Sphere, Random, Inverse Square
- Taille : modifiable par molette pendant la transformation

**Transform Orientation ▼**
- Global, Local, Normal, Gimbal, View, Custom

**Pivot ▼**
- Active Element, Individual Origins, Median Point, 3D Cursor, Bounding Box

**Snap ▼**
- Incrément, Vertex, Edge, Face, Volume, Node, Frame (timeline)
- Options : Align to rotation, Project onto surface, Backface culling

---

## 21. HUD & Overlays Viewport

### Informations Affichées (HUD)

Coin inférieur gauche du viewport :

```
Frame: 042 / 250   |   Objets: 3 sélectionnés / 47 total
Mode: Pose Mode    |   Verts: 12,450   Faces: 24,900
FPS: 24.0          |   Simulation: ✅ Actif
```

### Overlays Disponibles (panneau Overlays)

#### Section Guides

- **Floor Grid** — grille de sol (taille, subdivisions, opacité)
- **Axes X, Y, Z** — afficher les axes monde
- **3D Cursor** — afficher le curseur 3D
- **Annotations** — afficher les annotations dessinées

#### Section Objets

- **Extra** — afficher les extras (origines, noms, axes)
- **Relationship Lines** — lignes parent-enfant
- **Outline Selected** — contour orange des objets sélectionnés
- **Bone Colors** — couleur des os par groupe

#### Section Géométrie

- **Wireframe** — opacité du wireframe en solid mode
- **Face Orientation** — colorer selon orientation des faces (bleu=front, rouge=back)
- **Vertex Normals** — afficher les normales de vertex
- **Face Normals** — afficher les normales de face

#### Section Animation Spécifique

- **Motion Paths** — trajectoires des os/objets
- **Onion Skinning** — frames fantômes
- **Center of Mass** — centre de masse global
- **Per-Segment CoM** — centre de masse par segment du corps
- **Support Polygon** — polygone de support au sol
- **Force Vectors** — vecteurs de force
- **Velocity Vectors** — vecteurs de vitesse aux effecteurs
- **Contact Zones** — zones de contact pied/sol
- **AI Suggestion Overlay** — projection de la pose IA en transparence

---

## 22. Gizmos & Manipulateurs

### Gizmo de Transformation

Affiché sur l'objet/os sélectionné, contrôlable par clic-drag :

- **Axe X** (rouge) — déplacer/rotation/scale sur X
- **Axe Y** (vert) — déplacer/rotation/scale sur Y
- **Axe Z** (bleu) — déplacer/rotation/scale sur Z
- **Plan XY** (carré bleu) — déplacer sur plan XY
- **Plan XZ** (carré vert) — déplacer sur plan XZ
- **Plan YZ** (carré rouge) — déplacer sur plan YZ
- **Centre** (blanc) — transformation libre tous axes

Affichage : les 3 gizmos (Move, Rotate, Scale) peuvent être actifs simultanément.

### Gizmo IK (en Pose Mode avec chaîne IK active)

- **Sphère IK cible** : drag pour repositionner la cible IK en temps réel
- **Pole Vector** : drag du triangle pour orienter la chaîne
- **Reach Circle** : anneau indiquant la limite d'extension de la chaîne

### Gizmo de Navigation (coin supérieur droit)

- Sphère XYZ orientable (clic pour aligner la vue)
- Boutons : Perspective/Ortho, Camera, Reset

### Gizmo Centre de Masse (Physics Overlay)

- Sphère jaune = centre de masse global
- Ligne verticale → projection au sol
- Polygone au sol = polygone de support
- Couleur : vert (stable), orange (limite), rouge (instable)

---

## 23. Fenêtres Modales & Dialogues

### Dialogue "Adjust Last Operation" (F9)

Panneau flottant apparaissant après chaque opération, permettant d'ajuster les paramètres de la dernière action.

```
┌───────────────────────────────────────┐
│  Déplacer                             │
├───────────────────────────────────────┤
│  X: [0.500m]                         │
│  Y: [0.000m]                         │
│  Z: [1.200m]                         │
│  Contrainte: [Axe X]                 │
│  Mode: [Global]                      │
└───────────────────────────────────────┘
```

### Dialogue de Confirmation

Pour les opérations destructives (supprimer, écraser, etc.) :

```
┌────────────────────────────────────┐
│  ⚠ Supprimer 3 objets ?           │
│  Cette action ne peut être annulée │
│  après la sauvegarde.              │
├────────────────────────────────────┤
│  [Annuler]           [Supprimer]   │
└────────────────────────────────────┘
```

### Dialogue Préférences (Ctrl+,)

```
┌────────────────────────────────────────────────────┐
│  PRÉFÉRENCES                                       │
├──────────┬─────────────────────────────────────────┤
│ Interface│  Thème: [Dark Pro ▼]                   │
│ Keymap   │  Langue: [Français ▼]                  │
│ Entrées  │  DPI: [Auto ▼]  Resolution Scale: [1.0]│
│ Addons   │  ─────────────────────────────────────  │
│ Système  │  Résolution du curseur: [Normal ▼]     │
│ Fichiers │  Info Bulles: [☑] Raccourcis: [☑]     │
│ Sauveg.  │  Splash Screen: [☑]                   │
│ IA       │  ─────────────────────────────────────  │
│ Render   │  Animation:                            │
│          │  FPS par défaut: [24 ▼]               │
│          │  Durée par défaut: [250 frames]        │
└──────────┴─────────────────────────────────────────┘
```

**Onglet IA :**
- Modèle IA : local / cloud
- URL du serveur IA : `http://localhost:8080`
- Clé API : `[••••••••]`
- Qualité des suggestions : Rapide / Équilibré / Haute Qualité
- Confidentialité : envoyer les poses au cloud (opt-in)

### Command Palette (F3)

Barre de recherche universelle pour toutes les actions :

```
┌────────────────────────────────────┐
│  🔍 Chercher une action...        │
├────────────────────────────────────┤
│  > Insérer keyframe (All Channels) │
│  > AI Pose Assistant               │
│  > Bake Rigid Body Physics         │
│  > Render Animation                │
│  > Set Active Camera               │
│  ...                               │
└────────────────────────────────────┘
```

- Navigation par flèches ↑↓
- Validation par Entrée
- Affiche le raccourci correspondant à droite
- Filtrage en temps réel

### Pie Menus (menus circulaires)

Activés par des raccourcis, affichent 8 options maximum en cercle autour du curseur :

**Pie Menu Shading (Z) :**
```
           [Solid]
    [Material]    [Wireframe]
[Rendered]            [Overlay+]
    [Physics]     [Debug]
           [Camera]
```

**Pie Menu Keyframe (I) :**
```
              [Location]
[LocRotScale]     [Rotation]
 [All Props]         [Scale]
    [Custom]     [VisualLocRot]
              [Available]
```

**Pie Menu Vue (~) :**
```
           [Dessus]
    [Côté G]    [Face]
[Bas]               [Derrière]
    [Côté D]    [Camera]
           [User]
```

---

## Annexe A — Flux de Travail Recommandés

### Film : Animation de Personnage

1. **Workspace Rigging** → Créer/importer et configurer l'armature
2. **Workspace Layout** → Placer le personnage dans la scène, configurer l'éclairage et les caméras
3. **Workspace AI Director** → Utiliser AI Pose Assistant pour les poses clés
4. **Workspace Animation** → Affiner sur la Dope Sheet et les F-Curves
5. **Workspace Physics** → Vérifier et corriger la physique, tissu, cheveux
6. **Workspace Render** → Configurer Cycles X et lancer le rendu

### Jeu Vidéo : Cycle de Mouvements

1. **Workspace Rigging** → Rig humanoid standard
2. **Workspace AI Director** → Générer la base du mouvement (marche, course, saut)
3. **Workspace Animation** → Affiner, créer les loops propres
4. **NLA Editor** → Organiser les clips
5. **Export** → FBX / glTF vers Unity ou Unreal Engine

### Motion Capture : Cleanup Pipeline

1. **Import BVH/C3D** → Importer les données mocap brutes
2. **AI Mocap Cleanup** → Filtrage automatique du bruit
3. **Retarget** → Adapter au rig du personnage
4. **Workspace Animation** → Corrections manuelles
5. **AI Physics Check** → Valider la correction physique
6. **Export** → Selon destination

---

## Annexe B — Conventions de Nommage Recommandées

| Élément | Convention | Exemple |
|---------|------------|---------|
| Armature | `ARM_NomPerso` | `ARM_Hero` |
| Os | `os_zone_G/D` | `os_avant_bras_G` |
| Action | `ACT_NomPerso_action` | `ACT_Hero_Marche01` |
| Caméra | `CAM_role` | `CAM_Master`, `CAM_CloseUp` |
| Collection | `COL_categorie` | `COL_Personnages` |
| Mesh | `MSH_NomObjet` | `MSH_Corps_Hero` |
| Matériau | `MAT_NomMaterial` | `MAT_Peau_Hero` |
| Rendu Layer | `RL_description` | `RL_Personnage_Avant` |

---

*Document de spécification UI — Application Animation 3D Physiquement Correcte + IA*
*Version 1.0 — Couvre les phases de conception initiale*
*Inspiré de Cascadeur, Blender, Maya, et les meilleures pratiques de l'industrie*