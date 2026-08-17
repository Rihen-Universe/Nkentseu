# Spécification d'interface — "Aetherion Animate & FX"
### Document 4/6 — Version lisible par un humain (produit / UX)

> Extension du moteur "Aetherion Engine" (documents 1-3) vers une suite dédiée à l'**animation 3D avec squelette**, l'**animation 2D** (rig à os / cutout) et les **effets visuels (VFX)**, dans le même style visuel Unreal Engine 5. Ce document **réutilise sans les redécrire** : le système de thèmes Light/Dark (doc 1 §2), le système de docking (doc 1 §5), le Launcher, le Content Browser, le World Outliner et les composants génériques (doc 1 §19). Il ne détaille que ce qui est **nouveau ou spécifique** à l'animation et aux VFX.

---

## 0. Sommaire

1. Positionnement & rappel des briques réutilisées
2. Launcher — templates spécifiques
3. Workspaces (modes de travail) : Rigging, Animation, VFX, 2D
4. Skeleton Tree & Skeletal Mesh Editor
5. Physics Asset Editor (ragdoll / contraintes)
6. Animation Editor — Dope Sheet & Curve Editor
7. Animation Blueprint — State Machine & AnimGraph
8. Blend Spaces
9. Retargeting (réutilisation d'animation entre squelettes)
10. Montage Editor (composition de clips)
11. Motion Capture Import & Take Browser
12. 2D Rig Editor (bones + mesh deformation)
13. 2D Sprite Atlas Editor & Onion Skinning
14. VFX Editor — Emitter Stack & Node Graph
15. VFX Preview Viewport
16. VFX Library Browser
17. Transport bar globale de lecture
18. Export / Build spécifique animation-VFX
19. Préférences spécifiques
20. Glossaire des nouveaux composants

---

## 1. Positionnement & rappel des briques réutilisées

Aetherion Animate & FX partage la **même coquille d'éditeur** que le moteur de jeu (barre de titre, barre de menu, barre d'outils, docking, statusbar — voir doc 1 §4-5) et les **mêmes deux thèmes** (Light GitHub / Dark GitHub Pro, doc 1 §2). Sont réutilisés à l'identique :

- Launcher (doc 1 §3), avec des templates de projet différents (voir §2 ci-dessous)
- Content Browser + Asset Browser avancé (Reference Viewer, Size Map — doc 1 §9, §9bis)
- World Outliner (doc 1 §7) — utilisé pour les scènes contenant plusieurs personnages/effets
- Details Panel (doc 1 §8) — étendu avec de nouveaux types de propriétés (courbes, poids de skin — voir §4)
- Output Log, notifications, modales, menus contextuels (doc 1 §11, §18)
- Système de docking (doc 1 §5) — chaque nouvel écran ci-dessous est un panneau dockable comme les autres

Ce qui est **propre à cette suite** : la notion de **Workspace** (bascule d'un ensemble de panneaux prédéfini selon la tâche, façon Maya/Blender "Layout switcher", superposée au système de docking libre existant).

---

## 2. Launcher — templates spécifiques

Dans l'assistant "Nouveau projet" (doc 1 §3.3), catégorie "Animation & VFX" ajoutée avec ses propres templates :

- **Personnage 3D vide** (squelette humanoïde standard préconfiguré, 1 mannequin de base)
- **Animation 2D vide** (rig à os 2D vierge)
- **VFX vide** (scène de preview neutre avec un émetteur exemple)
- **Bibliothèque de mouvements** (projet orienté mocap, dossiers Takes/ préconfigurés)

Chaque carte de template affiche en plus un badge indiquant le type de pipeline : `3D Squelette`, `2D Bones`, `VFX`.

---

## 3. Workspaces (modes de travail)

Un sélecteur de **Workspace** apparaît dans la barre d'outils principale, à droite du sélecteur de mode (doc 1 §4.3) : dropdown avec icône + libellé, ex. `Animation ▾`.

| Workspace | Panneaux affichés par défaut |
|---|---|
| **Rigging** | Viewport 3D, Skeleton Tree, Details (os sélectionné), Content Browser |
| **Animation** | Viewport 3D, Dope Sheet, Curve Editor, Skeleton Tree (compacte), Content Browser |
| **VFX** | Viewport VFX (preview temps réel), Emitter Stack, Node Graph VFX, Details |
| **2D** | Viewport 2D, 2D Rig Tree, Sprite Atlas, Dope Sheet 2D |

Changer de Workspace **recharge automatiquement la disposition de docking** associée (mécanisme identique à `Window > Layouts`, doc 1 §5), mais l'utilisateur peut ensuite librement redocker par-dessus ; ses changements sont mémorisés par Workspace.

Chaque Workspace a sa propre couleur d'accent discrète dans l'onglet du sélecteur (ex. Rigging = orange, Animation = bleu, VFX = violet, 2D = vert) — visible uniquement comme un petit liseré de 2px sous le nom du workspace actif, pour rester sobre.

---

## 4. Skeleton Tree & Skeletal Mesh Editor

Fenêtre/onglet dédiée à l'édition d'un squelette + son mesh associé.

### 4.1 Viewport (réutilise le Viewport 3D du doc 1 §6, avec ajouts)
- Affichage du squelette en overlay : os dessinés en lignes fines cyan, articulations en petites sphères cliquables, os sélectionné en surbrillance orange (`--selection-outline`)
- Toggle overlay dans la barre "Show ▾" : `Bones`, `Bone Names`, `Skin Weights (heatmap)`, `Mesh Wireframe`
- Mode heatmap de poids de skin : le mesh se colore en dégradé (bleu = poids 0, rouge = poids 1) selon l'os sélectionné dans le Skeleton Tree

### 4.2 Skeleton Tree (panneau dockable gauche)
- Arbre hiérarchique des os, identique visuellement au World Outliner (doc 1 §7) mais avec icône "os" spécifique
- Icône œil par os (masquer l'affichage d'un os dans le viewport), icône cadenas (verrouiller contre modification accidentelle)
- Recherche + filtre "Afficher seulement les os avec contraintes"
- Clic-droit sur un os : Ajouter un socket (point d'attache), Ajouter une contrainte IK, Renommer, Réinitialiser la pose

### 4.3 Details Panel — extensions spécifiques (au-delà du doc 1 §8)
- Nouvelle catégorie **"Bone"** : Longueur, Rotation locale/globale (3 champs X/Y/Z colorés comme le Transform standard), Translation Retargeting (dropdown : Animation / Skeleton / AnimScaled)
- Nouvelle catégorie **"Skin Weights"** : liste des os influençant le vertex/groupe sélectionné avec sliders de poids (0-1), somme totale affichée et surlignée en rouge si ≠ 1.0
- Nouvelle catégorie **"IK Constraint"** (si l'os en a une) : Type (Two Bone IK, FABRIK, Spline IK), cible (référence d'acteur/os), poids d'effecteur (slider)

### 4.4 Barre d'outils du Skeletal Mesh Editor
- Outils de sélection d'os (identiques aux gizmos standard mais limités à la rotation locale par défaut, car on pose le squelette)
- Bouton "Pose de référence" (retour à la T-pose/A-pose)
- Toggle "Édition en Local Space / World Space"
- Bouton "Créer une Physics Asset" (raccourci vers §5)

---

## 5. Physics Asset Editor (ragdoll / contraintes)

Éditeur dédié pour définir les corps physiques (capsules/boîtes/sphères de collision) attachés à chaque os, et les contraintes articulaires entre eux (ragdoll).

- **Viewport** : squelette affiché en fond estompé, formes de collision en semi-transparent bleu clair superposées à chaque os concerné, contraintes affichées comme des petits axes de charnière colorés à chaque articulation
- **Panneau gauche "Bodies"** : liste des corps physiques par os, avec icône de forme (capsule/boîte/sphère), toggle activé/désactivé
- **Panneau droit "Details"** : pour un corps sélectionné — Type de forme, Dimensions, Masse, Amortissement linéaire/angulaire ; pour une contrainte sélectionnée — Limites d'angle (swing1/swing2/twist) avec un mini-widget conique visuel montrant la plage de mouvement autorisée
- Barre d'outils : mode simulation physique en direct dans le viewport (bouton Play spécifique, distinct du Play principal), slider de gravité de test, bouton "Générer automatiquement les corps" (à partir du squelette, avec dialogue de confirmation des paramètres min/max de taille d'os concernés)

---

## 6. Animation Editor — Dope Sheet & Curve Editor

Remplace/étend le Sequencer générique (doc 1 §14) pour l'édition fine d'un clip d'animation sur un squelette.

### 6.1 Dope Sheet (vue par défaut)
- Colonne gauche : liste des os/courbes animées (identique structurellement à la colonne piste du Sequencer), avec un os parent repliable révélant ses sous-courbes (Position.X, Rotation.Y, etc.) seulement si "développer" est cliqué
- Zone droite : timeline avec des **losanges de keyframe groupés par os** (une seule rangée résumée par os, tous les canaux confondus) — permet de voir d'un coup d'œil où sont les poses clés sans détail excessif
- Notifications d'animation (Anim Notifies) affichées sur une piste séparée en haut de la timeline : petits triangles colorés avec icône (son = haut-parleur, effet = étincelle, événement custom = point d'exclamation), nommées en tooltip au survol

### 6.2 Curve Editor (bascule via un bouton toggle dans la barre d'outils)
- Remplace la zone timeline par un graphe de courbes (axes temps/valeur), une couleur par canal (rouge/vert/bleu pour X/Y/Z comme convention Transform)
- Sélection d'un ou plusieurs canaux à afficher via des cases à cocher dans la colonne gauche (au lieu des losanges)
- Poignées de tangente à chaque keyframe (type Bézier), draggables, avec un menu contextuel "Auto / Linéaire / Constant / Cassée" pour le type d'interpolation
- Barre d'outils du Curve Editor : Cadrer la sélection, Normaliser l'affichage, Filtre de lissage (bouton avec slider de force)

---

## 7. Animation Blueprint — State Machine & AnimGraph

Éditeur de graphe à deux niveaux, réutilise le moteur visuel `NodeGraphCanvas` (comme le Material Editor / Blueprint Editor du doc 1 §12-13) :

### 7.1 AnimGraph (niveau racine)
- Nœuds spécifiques : `Blend Poses by Bool`, `Layered Blend per Bone`, `State Machine` (nœud spécial, double-clic pour entrer dedans), `Output Pose` (nœud final, non supprimable, équivalent du Material Output)
- Câbles = flux de pose (couleur violette distincte des câbles de données classiques), épais comme les câbles d'exécution des Blueprints

### 7.2 State Machine (sous-graphe, ouvert en double-cliquant le nœud correspondant)
- Nœuds = états (rectangles arrondis avec un mini-aperçu du clip joué en icône), reliés par des flèches de transition
- Chaque flèche de transition cliquable ouvre un mini-graphe de condition (booléens/comparaisons) dans le panneau Details
- État "Entry" toujours présent en haut à gauche, non supprimable, point de départ visuel (rond plein)
- Barre d'onglets en haut du graphe pour naviguer entre AnimGraph racine et sous-state-machines ouvertes (breadcrumb cliquable, ex. `AnimGraph > Locomotion > Jump`)

### 7.3 Panneau gauche "My Blueprint" (identique doc 1 §13) + section additionnelle "Variables exposées" utilisées comme conditions de transition, avec badge indiquant si la variable est mise à jour côté C++/Blueprint externe.

---

## 8. Blend Spaces

Éditeur 1D ou 2D pour interpoler entre plusieurs clips selon des paramètres (ex. vitesse + direction).

- **Grille centrale** : plan cartésien avec axes nommés/étiquetés par l'utilisateur (ex. X = "Vitesse", Y = "Direction"), échantillons d'animation placés en points sur la grille (icône diamant + nom du clip)
- Glisser-déposer un asset d'animation depuis le Content Browser directement sur la grille pour l'ajouter comme échantillon à une position donnée
- Un point de preview mobile (curseur en croix) draggable sur la grille, avec le viewport 3D à côté qui joue en temps réel le blend résultant
- Panneau droit : liste des échantillons avec leurs coordonnées éditables numériquement, bouton "Trianguler" (visualise les zones d'interpolation par triangles semi-transparents entre échantillons voisins)
- Toggle 1D/2D en haut (1D = liste linéaire horizontale au lieu d'un plan)

---

## 9. Retargeting (réutilisation d'animation entre squelettes)

Assistant/fenêtre dédiée pour appliquer une animation créée sur un squelette source à un squelette cible différent (morphologie différente).

- Deux colonnes côte à côte : arbre du squelette source (gauche) et arbre du squelette cible (droite), avec des lignes de connexion visuelles entre les os correspondants (auto-détectées par nom, éditables manuellement par glisser-déposer d'un os à l'autre)
- Os non mappés surlignés en rouge/orange dans les deux arbres
- Bouton "Auto-mapper par nom" et "Auto-mapper par proximité de squelette"
- Preview côte à côte de deux petits viewports (source qui joue l'animation originale, cible qui joue le résultat retargeté) synchronisés sur la même timeline
- Réglages de "Retarget Pose" par os problématique (offset de rotation correctif) accessible dans le Details Panel

---

## 10. Montage Editor (composition de clips)

Timeline permettant d'assembler plusieurs clips d'animation bout à bout ou en sections, avec des branches (utile pour des combos d'attaque par exemple).

- Plusieurs pistes horizontales ("Slots") pouvant contenir des segments de clip (blocs rectangulaires colorés avec le nom du clip, redimensionnables/déplaçables comme un montage vidéo classique)
- Marqueurs de section nommés au-dessus de la timeline (ex. "Windup", "Impact", "Recovery"), déplaçables, utilisés comme points d'entrée pour déclencher le montage depuis le gameplay
- Piste de "Anim Notifies" partagée, identique à celle du Dope Sheet (§6.1)
- Courbes de blend en fondu (petit triangle diagonal dessiné aux jonctions entre deux segments qui se chevauchent, représentant le blend in/out)

---

## 11. Motion Capture Import & Take Browser

- **Take Browser** : variante du Content Browser (doc 1 §9) filtrée sur les fichiers de capture importés (FBX/BVH/C3D), miniature = aperçu vidéo de référence si disponible, sinon icône squelette animé
- **Import Wizard** modal : mapping des marqueurs mocap vers le squelette cible (liste à deux colonnes comme le Retargeting §9), options de nettoyage automatique (lissage, suppression de gigue/jitter avec slider de force), aperçu avant/après dans deux viewports côte à côte
- Une fois importé, chaque Take apparaît comme un clip d'animation standard éditable dans le Dope Sheet (§6)

---

## 12. 2D Rig Editor (bones + mesh deformation)

Éditeur pour le rig 2D à squelette (type Spine/DragonBones), dans le Workspace "2D".

- **Viewport 2D** : personnage en illustration à plat (composé de plusieurs images/calques), fond de type damier de transparence léger, grille 2D en overlay désactivable
- Squelette 2D en overlay : os représentés comme des formes en losange allongé (convention type Spine), articulations = points cliquables, os sélectionné surligné orange
- Chaque image/calque du personnage associée à un ou plusieurs os (liste de "Slots" dans un panneau dédié à droite, glisser une image depuis le Content Browser sur un slot pour l'assigner)
- Mode "Mesh" (bascule d'outil) : superpose une grille de déformation (maillage triangulé) sur une image, avec des points de contrôle pondérés à un ou plusieurs os (comme le skin weight 3D mais en 2D), heatmap de poids identique au §4.1
- Barre d'outils : outils Créer un os, Créer un maillage, Assigner un slot, Pondérer (weight paint 2D avec pinceau et taille/force de brosse)

---

## 13. 2D Sprite Atlas Editor & Onion Skinning

### 13.1 Sprite Atlas Editor
- Grande zone centrale affichant la texture atlas complète, avec des rectangles de découpe superposés (chaque sprite individuel entouré d'un cadre fin, nom au survol)
- Panneau droit : liste des sprites détectés/définis avec miniature, nom, dimensions en pixels, point de pivot éditable visuellement (petite croix déplaçable dans la sprite agrandie)
- Outils : détection automatique de sprites (slicing par grille régulière ou par détection de contours/alpha), édition manuelle du rectangle de découpe par glisser des poignées

### 13.2 Onion Skinning (overlay du Viewport 2D, activable dans "Show ▾")
- Poses des frames précédentes affichées en superposition semi-transparente teintée (dégradé du bleu vers transparent pour le passé, orange vers transparent pour le futur), nombre de frames avant/après réglable via un petit slider dans la barre d'outils du viewport
- Toggle indépendant pour n'afficher que le contour (silhouette) plutôt que la pose complète, pour réduire l'encombrement visuel

---

## 14. VFX Editor — Emitter Stack & Node Graph

Éditeur de systèmes de particules/effets visuels, façon Niagara.

### 14.1 Emitter Stack (panneau gauche, vue par défaut)
- Colonne verticale de **cartes d'émetteurs** empilées (un système VFX = plusieurs émetteurs superposés), chaque émetteur = une carte pliable avec :
  - En-tête : nom, icône de type de particule (sprite/mesh/ruban), toggle activé/désactivé, statistiques rapides (nb de particules actives)
  - Corps déplié : liste de **modules** empilés verticalement dans un ordre d'exécution fixe par catégorie (Spawn, Initialize, Update, Render), chaque module = une ligne avec icône, nom, toggle, et une pastille indiquant s'il a des paramètres exposés
  - Bouton "+ Ajouter un module" en bas de chaque catégorie, ouvrant une recherche par nom/catégorie
- Glisser-déposer pour réordonner les modules à l'intérieur d'une catégorie (l'ordre affecte le résultat, comme une pile de calques)

### 14.2 Node Graph VFX (onglet alternatif, pour l'édition avancée d'un module)
- Réutilise `NodeGraphCanvas` (doc 1 §12), avec une palette de nœuds spécifique VFX (Bruit de Perlin, Courbe, Collision, Force, Attribut de particule)
- Utile pour créer des modules custom au-delà de la bibliothèque standard de l'Emitter Stack

### 14.3 Details Panel — extension VFX
- Nouvelle catégorie par module sélectionné : liste de paramètres avec, en plus des types standards (doc 1 §8), un type **Distribution** (constante / plage aléatoire min-max / courbe dans le temps) sélectionnable via un petit dropdown à côté de chaque champ concerné

---

## 15. VFX Preview Viewport

Variante du Viewport 3D standard (doc 1 §6), avec ajouts spécifiques :

- Overlay supérieur droit additionnel : compteur de particules actives en temps réel, bouton "Reset simulation" (icône flèche circulaire), bouton "Isoler cet émetteur" (masque les autres émetteurs du système pour debug)
- Fond de preview configurable : damier de transparence, environnement HDRI simple, ou noir uni (dropdown dédié)
- Curseur de vitesse de simulation (0.1x à 2x) en overlay bas, pour ralentir/accélérer l'observation sans changer le comportement réel en jeu
- Grille de sol optionnelle avec échelle métrique visible (utile pour juger la taille réelle d'un effet)

---

## 16. VFX Library Browser

Variante filtrée du Content Browser (doc 1 §9), avec :
- Filtre par défaut sur les types "Système VFX", "Émetteur", "Module"
- Miniature = aperçu vidéo en boucle courte de l'effet (lecture au survol, comme les sons dans le Content Browser standard)
- Tags additionnels visibles en petit sous le nom : catégorie d'effet (Feu, Eau, Magie, Impact, Ambiance) sous forme de petites pilules colorées

---

## 17. Transport bar globale de lecture

Barre fine (36px) toujours visible sous la barre d'outils principale dès qu'un Workspace Animation/VFX/2D est actif (indépendante du Sequencer/Dope Sheet qui peut être fermé) :

- Boutons : Aller au début, Image précédente, Play/Pause, Image suivante, Aller à la fin, toggle Boucle
- Champ numérique éditable "Frame actuelle / Frame totale" (ex. `24 / 120`)
- Slider de vitesse de lecture (0.25x, 0.5x, 1x, 2x) en dropdown compact
- Toggle "Temps réel" vs "Frame par frame" (utile pour juger le rendu final vs debug précis)

---

## 18. Export / Build spécifique animation-VFX

Étend le menu Build (doc 1 §17) :
- **Export FBX/Alembic** : dialogue avec choix du/des clip(s), option "Bake toutes les courbes" (aplati les contraintes/IK en keyframes brutes), plage de frames, taux d'échantillonnage
- **Export vers moteur de jeu** : si utilisé en compagnon d'Aetherion Engine, bouton direct "Envoyer vers le projet de jeu" avec sélection du projet cible (réutilise la liste de projets du Launcher) et mapping automatique des chemins d'assets
- **Rapport de compression d'animation** : modal listant chaque clip avec sa taille avant/après compression et un curseur de tolérance d'erreur global

---

## 19. Préférences spécifiques

Ajout dans Editor Preferences (doc 1 §16), nouvelle catégorie "Animation & VFX" :
- Couleur des os par défaut dans le viewport, épaisseur des lignes de squelette
- Nombre de frames d'onion skinning par défaut
- Comportement du Curve Editor (tangentes auto par défaut : Auto/Linéaire/Plate)
- Limite du nombre de particules affichées en preview avant avertissement de performance

---

## 20. Glossaire des nouveaux composants

| Composant | Description |
|---|---|
| `WorkspaceSwitcher` | Dropdown de bascule entre dispositions prédéfinies (Rigging/Animation/VFX/2D) |
| `SkeletonTree` | Arbre d'os, variante de `TreeView` |
| `BoneWeightSlider` | Slider de poids de skin par os, avec somme validée |
| `DopeSheetTrack` | Rangée résumée de keyframes par os |
| `CurveEditorCanvas` | Graphe de courbes d'animation avec tangentes éditables |
| `StateMachineCanvas` | Variante de `NodeGraphCanvas` pour états/transitions |
| `BlendSpaceGrid` | Plan 1D/2D d'échantillons d'animation |
| `RetargetMappingList` | Double liste avec lignes de correspondance os↔os |
| `EmitterStackCard` | Carte pliable d'émetteur VFX avec modules empilés |
| `DistributionField` | Champ de propriété à 3 modes (constante/aléatoire/courbe) |
| `TransportBar` | Barre de lecture globale (play/frame/vitesse) |
| `OnionSkinOverlay` | Superposition semi-transparente des poses voisines dans le viewport 2D |

---

**Fin du document 4/6.** Voir `05-specification-claude-animation-vfx.md` pour la spécification technique d'implémentation et `06-specification-banani-animation-vfx.md` pour les prompts de génération écran par écran.
