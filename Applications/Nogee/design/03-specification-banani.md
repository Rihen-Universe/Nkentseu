# Prompts de génération d'écrans — Aetherion Engine UI
### Document 3/3 — Destiné à Banani

> Chaque section ci-dessous est un **prompt autonome**, à copier-coller tel quel dans Banani, un écran à la fois. Génère toujours d'abord le "Système de design" (section 0) comme référence de style, puis chaque écran dans l'ordre. Répète le bloc "Système de design" en tête de chaque prompt d'écran si Banani ne conserve pas le contexte entre générations.

---

## 0. Système de design (à coller en préfixe de chaque prompt)

```
Style général : interface professionnelle de moteur de jeu 3D type Unreal Engine 5.
Densité élevée, texte petit (11-13px), pas d'espace perdu, look "outil pro" pas "app grand public".

Deux thèmes à produire, Light et Dark, façon GitHub / GitHub Dark Pro :

THÈME LIGHT :
- fond principal #ffffff, fond panneaux #f6f8fa, fond champs #eaeef2
- bordures #d0d7de, texte principal #1f2328, texte secondaire #656d76
- accent bleu #0969da, succès #1a7f37, alerte #9a6700, erreur #d1242f, violet #8250df

THÈME DARK :
- fond principal #0d1117, fond panneaux #161b22, fond champs #010409
- bordures #30363d, texte principal #e6edf3, texte secondaire #8b949e
- accent bleu #2f81f7, succès #3fb950, alerte #d29922, erreur #f85149, violet #a371f7

Typographie : police sans-serif système (type Segoe UI / Inter), texte petit, labels en majuscules discrètes pour les titres de section (letter-spacing léger).
Icônes : style trait fin (outline, 1.5px), monochromes, jamais d'emoji, jamais d'icônes 3D/skeuomorphes.
Coins arrondis légers (4-6px) sur boutons et cartes, pas d'arrondi sur les panneaux dockables (angles droits, comme Unreal/VSCode).
Le viewport 3D garde toujours un fond gris très foncé quasi-noir (#1e1e1e), même en thème Light — ne jamais le rendre blanc.
Boutons primaires = fond couleur accent, texte blanc/clair. Boutons secondaires = contour fin, fond transparent.
Toujours montrer une hiérarchie visuelle claire : barre de titre > barre de menu > barre d'outils > zone de travail dockée en panneaux > barre de statut.
```

---

## 1. Launcher — Écran Bibliothèque de projets

```
[Coller le Système de design]

Génère l'écran principal d'un launcher de moteur de jeu (comme Epic Games Launcher / Unreal), en thème DARK, résolution desktop 1280x800.

Layout :
- Sidebar verticale gauche (240px), fond légèrement plus sombre que le contenu : en haut un avatar rond + nom d'utilisateur, en dessous une navigation verticale avec icônes : Bibliothèque (actif, surligné en accent), Marketplace, Apprendre, Communauté, Paramètres. Tout en bas de la sidebar, un sélecteur de version du moteur "Aetherion 5.4 ▾" et un bouton secondaire "Installer une version".
- Zone de contenu principale : en haut, un titre "Bibliothèque" et un gros bouton primaire bleu "+ Nouveau projet" aligné à droite. En dessous, une barre de recherche avec icône loupe et des filtres en pilules (Tous / Récents / Favoris).
- Grille de 6 à 8 cartes de projets : chaque carte a une miniature 16:9 (image de rendu 3D abstraite, ex. paysage ou scène stylisée), le nom du projet en dessous, un sous-texte gris "Aetherion 5.4 · il y a 2 jours", et une icône "..." en haut à droite de la carte.
- Espacement généreux entre les cartes, coins arrondis 6px sur les cartes, légère ombre portée au survol d'une carte.
```

---

## 2. Launcher — Assistant Nouveau Projet (étape Template)

```
[Coller le Système de design]

Génère une fenêtre modale plein écran "Nouveau projet", thème DARK, étape 2 d'un assistant (stepper).

En haut : un stepper horizontal avec 5 étapes "Catégorie · Template · Paramètres · Emplacement · Créer", l'étape "Template" est active (surlignée accent), les précédentes ont une coche verte.

Contenu : titre "Choisissez un template", sous-titre gris. Une grille de 6 cartes de templates larges (ratio 3:2) : "Vide 3D", "Third Person", "First Person", "Véhicule", "Vide 2D", "Top-down 2D" — chaque carte a une illustration abstraite représentative, un nom, et un petit toggle "Blueprint / C++" en bas de carte. La carte "Third Person" est sélectionnée (bordure accent bleu épaisse).

Bas de fenêtre : bouton secondaire "Précédent" à gauche, bouton primaire "Suivant" à droite.
```

---

## 3. Coquille de l'éditeur — Vue d'ensemble (layout par défaut)

```
[Coller le Système de design]

Génère la fenêtre principale complète d'un éditeur de moteur de jeu 3D, thème DARK, résolution 1920x1080, style très proche d'Unreal Engine 5.

Structure verticale de haut en bas :
1. Barre de titre fine (32px) : icône du moteur à gauche, nom du projet centré "MonProjet - Aetherion Editor", boutons de fenêtre (réduire/agrandir/fermer) à droite.
2. Barre de menu (30px) : File, Edit, Window, Tools, Build, Select, Actor, Help — texte simple, fond identique à la barre de titre.
3. Barre d'outils (36px) : icône Save, icône source control (branche git + point vert), séparateur, groupe de 5 icônes gizmo (flèche sélection, croix déplacer, cercle rotation, cube échelle, icône universelle) le 2e actif en surbrillance, séparateur, dropdown "Snap 10cm", au centre un gros bouton triangle vert "Play", à droite un dropdown plateforme "Windows (Development) ▾", une icône engrenage paramètres, une icône cloche notifications.

Zone de travail (le plus grand espace), divisée en panneaux dockables avec fines bordures :
- Colonne verticale d'icônes tout à gauche (32px de large) : icônes de modes (flèche sélection active en surbrillance, montagne pour landscape, arbre pour foliage, pinceau).
- Grand panneau central : le VIEWPORT 3D, fond gris très sombre presque noir, avec une scène 3D simple (quelques formes géométriques, une lumière, un sol en grille infinie perspective), overlay en haut à gauche avec des menus déroulants "Perspective ▾", "Lit ▾", "Show ▾", un petit cube de navigation 3D en haut à droite du viewport, un gizmo de déplacement coloré (rouge/vert/bleu) sur un objet sélectionné.
- Panneau droit haut, intitulé "World Outliner" : liste hiérarchique d'objets avec icônes (cube, lumière, caméra), icônes œil à gauche de chaque ligne, une barre de recherche en haut du panneau.
- Panneau droit bas, intitulé "Details" : liste de champs Transform (Position X/Y/Z en rouge/vert/bleu, Rotation, Scale), puis des sections repliables "Rendering", "Physics", "Collision".
- Panneau bas, occupant toute la largeur sous le viewport et l'outliner, avec des onglets "Content Browser" (actif) et "Output Log" : afficher le Content Browser avec un arbre de dossiers à gauche et une grille de miniatures carrées d'assets à droite (meshes orange, matériaux violets, blueprints bleus, sons verts — chaque miniature avec une icône colorée en coin).
4. Barre de statut fine (24px) en bas : à gauche "Prêt", au centre "3 objets sélectionnés", à droite "FPS: 144 · 512 Mo · main*".

Le tout avec des bordures fines (#30363d) séparant chaque panneau, en-têtes de panneaux avec petite icône + titre + bouton "..." à droite.
```

---

## 4. Viewport 3D — Gros plan avec overlays

```
[Coller le Système de design]

Génère uniquement le panneau Viewport 3D en gros plan, thème DARK, occupant toute l'image.

Fond : dégradé gris très sombre façon studio (#1e1e1e à #141414), une grille de sol en perspective avec lignes fines gris clair semi-transparentes s'étendant vers l'horizon, quelques objets 3D simples posés dessus (un cube, une sphère, un cylindre) avec un éclairage doux et des ombres portées douces.

Overlays (superposés au rendu, fond semi-transparent sombre derrière le texte pour lisibilité) :
- Coin supérieur gauche : trois menus déroulants en ligne "Perspective ▾", "Lit ▾", "Show ▾", petites icônes devant chaque texte.
- Coin supérieur droit : icône caméra, icône capture d'écran, icône plein écran, et un petit cube 3D de navigation avec les faces "Top/Front/Right" visibles.
- Coin inférieur gauche : texte monospace petit et semi-transparent "FPS: 144 | Draw calls: 812 | Tris: 2.4M".
- Sur le cube sélectionné : un contour orange lumineux (#f0883e) et un gizmo de déplacement 3D avec 3 flèches colorées (rouge axe X, vert axe Y, bleu axe Z) partant du centre de l'objet, plus un petit arc de rotation gris fin autour.
```

---

## 5. World Outliner + Details Panel — Gros plan

```
[Coller le Système de design]

Génère deux panneaux côte à côte, thème LIGHT cette fois (style GitHub clair), fond blanc/gris très clair.

Panneau gauche "World Outliner" (titre en en-tête gris clair avec icône dossier) :
- Barre de recherche en haut avec icône loupe et icône filtre à droite.
- Liste hiérarchique indentée : "Environnement" (dossier, chevron ouvert) contenant "Sol" (icône plan), "Éclairage" (dossier) contenant "Lumière directionnelle" (icône soleil), "Lumière ponctuelle" (icône ampoule), puis à la racine "PersonnagePrincipal" (icône silhouette, ligne surlignée en bleu = sélectionnée), "Caméra_Cinématique" (icône caméra). Chaque ligne a une petite icône œil à gauche (certaines barrées = masqué) et le nom à droite.

Panneau droit "Details" :
- En-tête avec le nom "PersonnagePrincipal" en gras et une icône silhouette.
- Barre de recherche de propriété.
- Section "Transform" non repliable : trois lignes Position/Rotation/Scale, chacune avec 3 champs numériques X (label rouge) Y (label vert) Z (label bleu), petites flèches d'incrémentation, icône cadenas à côté de Scale.
- Sections repliables en dessous avec chevron : "Rendering" (fermée), "Physics" (fermée), "Collision" (fermée), "Tags" (fermée) — juste les en-têtes visibles avec un fond légèrement grisé et une bordure fine séparatrice.
- Une des lignes de propriété a le label en gras avec une petite pastille jaune à gauche (valeur modifiée par rapport au parent).
```

---

## 6. Content Browser — Gros plan grille de miniatures

```
[Coller le Système de design]

Génère le panneau Content Browser en gros plan, thème DARK.

En haut : fil d'ariane "Content > Characters > Hero", barre de recherche, icônes de filtre par type (mesh, matériau, texture, blueprint, son) sous forme de toggles, slider de taille des miniatures à droite, bouton primaire bleu "Importer" tout à droite.

Corps en deux colonnes :
- Colonne gauche fine (arbre de dossiers) : Content (racine), Characters (ouvert, surligné), Maps, Materials, Blueprints, Audio — icônes dossier.
- Zone principale : grille de 15-18 cartes miniatures carrées. Varier les types : quelques cartes avec rendu de personnage 3D stylisé (badge orange "Mesh" en coin), quelques sphères avec matériau chatoyant (badge violet "Material"), quelques icônes de nœud bleu (badge "Blueprint"), une icône d'onde sonore verte (badge "Sound"), une texture colorée (badge jaune "Texture"). Nom de fichier sous chaque carte, une carte sélectionnée avec bordure bleue accent et fond légèrement teinté bleu.
```

---

## 7. Éditeur de matériaux — Graphe de nœuds

```
[Coller le Système de design]

Génère l'éditeur de matériaux (node graph), thème DARK, plein écran.

Fond du graphe : quadrillage de points très subtil sur fond #0d1117.

Nœuds (rectangles arrondis avec en-tête coloré selon la catégorie, corps sombre, petits cercles de connexion "pins" sur les bords gauche/droit) :
- À gauche, 3 nœuds "Texture Sample" (en-tête jaune-orangé) avec une miniature de texture dedans.
- Au centre, un nœud "Multiply" et un nœud "Lerp" (en-tête vert, juste des pins texte "A", "B", "Alpha").
- À droite, le nœud final "Material Output" (en-tête blanc/gris, plus large, pins "Base Color", "Metallic", "Roughness", "Normal", "Emissive"), non déplaçable, légèrement distinct visuellement (bordure plus épaisse).
- Câbles courbes reliant les pins, colorés selon le type de données (blanc pour vecteur, vert pour scalaire, jaune pour texture).

Panneau gauche fin : liste recherchable de nœuds disponibles par catégorie (Math, Texture, Paramètres).
Panneau droit haut : aperçu 3D d'une sphère avec le matériau appliqué, fond sombre neutre, reflets visibles.
Panneau droit bas : Details du nœud sélectionné (quelques champs simples).
Minimap miniature en bas à droite du graphe, petit rectangle indiquant la zone visible.
```

---

## 8. Éditeur de Blueprint — Event Graph

```
[Coller le Système de design]

Génère l'éditeur de Blueprint (visual scripting), thème DARK, plein écran, très similaire structurellement à l'éditeur de matériaux mais avec ces différences :

- Panneau gauche haut : "Components" avec un mini-viewport 3D montrant le mesh du personnage, en dessous une liste d'arbre de composants (Mesh, Capsule Collision, Camera, Spring Arm).
- Panneau gauche bas : "My Blueprint" avec sections "Variables" (liste avec pastilles colorées par type : bleu=boolean, vert=float, orange=vector) et "Functions", chacune avec un bouton "+".
- Zone centrale (Event Graph) : un nœud rose/rouge "Event BeginPlay" en haut à gauche avec un gros pin d'exécution blanc épais sortant vers la droite, connecté à un nœud "Print String", puis un nœud "Event Tick" plus bas connecté à un nœud "Add Movement Input". Les câbles d'exécution sont blancs et épais, les câbles de données sont fins et colorés.
- Barre d'outils en haut : bouton "Compile" avec une icône coche verte, bouton "Save", "Find in Blueprint", "Class Defaults".
- Panneau droit : Details du nœud/variable sélectionné.
```

---

## 8bis. Reference Viewer — Graphe de dépendances d'asset

```
[Coller le Système de design]

Génère l'écran "Reference Viewer" d'un moteur de jeu, thème DARK, plein écran, même style visuel que l'éditeur de matériaux (graphe de nœuds sur fond quadrillé sombre).

Un nœud central plus grand, entouré d'une bordure bleu accent épaisse, intitulé "BP_Hero" avec une icône plan bleu clair.
À gauche du nœud central : 3-4 petits nœuds reliés par des flèches pointant VERS le centre, intitulés "Niveau_Principal", "GameMode_Aventure", "WBP_HUD" (ce sont les éléments qui référencent BP_Hero).
À droite du nœud central : 5-6 petits nœuds reliés par des flèches partant DU centre, intitulés "SK_Hero" (badge orange Mesh), "AnimBP_Hero" (badge bleu), "M_Peau" (badge violet Material), "SFX_PasHero" (badge vert Sound), un nœud avec bordure rouge en pointillés et une icône d'avertissement intitulé "T_Texture_Manquante" (asset manquant).

Barre d'outils en haut : slider "Profondeur : 3", toggle "Afficher les références Editor-only", bouton "Localiser dans Content Browser".
Minimap en bas à droite.
```

---

## 8ter. Size Map — Treemap de poids disque

```
[Coller le Système de design]

Génère l'écran "Size Map", thème DARK, plein écran.

Un grand rectangle composé de sous-rectangles de tailles très variées façon "treemap" (mosaïque de blocs rectangulaires imbriqués, sans espace entre eux, bordures fines sombres). Colorer les blocs selon des catégories : blocs orange larges (Meshes, les plus gros), blocs jaunes moyens (Textures), blocs violets petits (Materials), blocs bleus petits (Blueprints), blocs verts fins (Sounds). Chaque bloc suffisamment grand affiche un nom de fichier et son poids en petit texte blanc centré ("T_Rock_Diffuse.png — 48 Mo").
En haut : fil d'ariane "Content > Environment", texte "Taille totale : 2.4 Go".
```

---

## 8quater. Project Navigator — Arborescence de fichiers projet

```
[Coller le Système de design]

Génère le panneau "Project Navigator", thème LIGHT (style GitHub clair), ressemblant à l'explorateur de fichiers d'un IDE (type VS Code / Visual Studio Solution Explorer).

Barre d'outils en haut : icône "Ouvrir dans l'IDE", icône actualiser, barre de recherche, toggle "Afficher les dossiers générés" (désactivé).

Arbre de fichiers :
- "Source" (dossier ouvert, icône dossier bleu) contenant "MonJeu" (sous-dossier) avec "Public" et "Private", fichiers à l'intérieur : "Hero.h" (icône violette), "Hero.cpp" (icône bleue), "GameMode.h", "GameMode.cpp".
- "Content" (dossier fermé, icône dossier orange).
- "Config" (dossier fermé) avec en dessous visibles "DefaultEngine.ini", "DefaultGame.ini" (icônes grises fichier texte).
- "Plugins" (dossier fermé) avec un badge vert "3 activés" à côté.
- Tout en bas, grisés et en italique : "Binaries", "Intermediate", "Saved" (dossiers techniques peu visibles).

Style clairement plus "développeur/code" que le Content Browser (moins de miniatures, plus de texte et d'icônes de fichiers simples).
```

---

## 8quinquies. Class Viewer — Hiérarchie de classes

```
[Coller le Système de design]

Génère l'écran "Class Viewer", thème DARK, panneau plein écran ou grande fenêtre.

En haut : barre de recherche, deux toggles "Toutes les classes" / "Blueprint seulement" (le premier actif).

Arbre hiérarchique indenté représentant l'héritage :
- "Object" (racine, icône engrenage gris)
  - "Actor" (icône engrenage bleu)
    - "Pawn"
      - "Character"
        - "BP_Hero" (icône plan bleu clair, ligne surlignée = sélectionnée)
        - "BP_Ennemi_Base" (icône plan bleu clair)
          - "BP_Ennemi_Gobelin" (indenté encore, icône plan bleu clair)
    - "StaticMeshActor" (icône engrenage bleu)
    - "Actor" grisé en italique avec mention "(Abstrait)" à côté d'une des entrées.

Panneau droit optionnel : aperçu miniature de la classe sélectionnée (silhouette 3D simple) avec boutons "Créer un Blueprint enfant", "Ouvrir l'éditeur", "Trouver toutes les références".
```

---

## 9. Sequencer — Timeline d'animation

```
[Coller le Système de design]

Génère le panneau Sequencer (timeline d'animation cinématique), thème DARK, plein écran horizontal.

Colonne gauche (pistes) : liste verticale avec icônes — "Caméra_Cinématique" (icône caméra, ligne parente), sous elle indentées "Transform", "Field of View" ; puis "PersonnagePrincipal" avec sous-pistes "Transform", "Animation", "Matériau_Visage".

Zone droite : règle temporelle horizontale en haut (graduations en secondes/frames "00:00, 00:05, 00:10..."), pour chaque piste une ligne horizontale avec des losanges (keyframes) positionnés à différents instants, reliés par une ligne fine indiquant l'interpolation. Un curseur de lecture vertical rouge (playhead) traverse toutes les pistes à environ 1/3 de la timeline.

Barre d'outils en haut de la zone timeline : boutons Play/Pause/Stop, bouton "+ Track", bouton "+ Caméra", slider de zoom temporel à droite.
```

---

## 10. Fenêtre Paramètres du projet

```
[Coller le Système de design]

Génère une fenêtre modale "Paramètres du projet", thème LIGHT, plein écran avec overlay sombre semi-transparent autour.

Sidebar gauche de catégories (fond gris clair) : Description, Maps & Modes (active, surlignée bleu), Input, Rendering, Packaging, Platforms (avec sous-icônes Windows/Mac/Android/iOS en dessous), Audio, Plugins — chaque item avec petite icône.

Zone de contenu droite : titre "Maps & Modes", barre de recherche globale en haut à droite de la fenêtre. En dessous, des sections avec labels à gauche et champs à droite : "Default GameMode" (dropdown), "Editor Startup Map" (champ avec icône asset), "Game Default Map" (champ avec icône asset), section repliable "Local Multiplayer" en dessous (fermée par défaut, juste l'en-tête visible avec chevron).

Bas de la fenêtre modale : pas de boutons Annuler/OK visibles (paramètres sauvegardés automatiquement, juste un petit texte gris "Sauvegardé automatiquement" en bas à droite).
```

---

## 11. Dialogue de Packaging (Build)

```
[Coller le Système de design]

Génère une fenêtre modale "Package Project", thème DARK, taille moyenne centrée avec overlay sombre autour.

Haut de la modale : titre "Package Project" et bouton fermer (X) à droite.

Contenu :
- Ligne d'icônes de plateformes en haut (Windows surlignée/sélectionnée en bleu, Mac, Linux, Android, iOS en gris non sélectionné).
- Trois boutons radio horizontaux "Debug / Development / Shipping" avec Development sélectionné, petit texte gris sous chacun décrivant l'usage.
- Case à cocher "Cook everything" et "Compresser les fichiers".
- Champ "Dossier de sortie" avec bouton "Parcourir" à droite.

Bas de la modale : une liste verticale d'étapes de progression avec icônes d'état — "Cooking content" (coche verte), "Compiling" (spinner animé bleu, en cours), "Staging" (icône horloge grise, en attente), "Packaging" (grise, en attente), "Archiving" (grise, en attente) — reliées par une fine ligne verticale. Une barre de progression globale en dessous à 42%.
Bouton primaire bleu "Package Project" en bas à droite (désactivé/grisé pendant le process en cours), bouton secondaire "Annuler".
```

---

## 12. Composants isolés — Notifications & menu contextuel

```
[Coller le Système de design]

Génère une planche de composants UI isolés sur fond neutre, thème DARK :

1. Une notification toast : coin arrondi, icône coche verte à gauche, titre "Compilation réussie" en gras, sous-texte gris "0 erreur, 2 avertissements", bouton "Voir le log" en lien bleu, bouton fermer (x) en haut à droite du toast.
2. Une deuxième toast au-dessus, variante erreur : icône croix rouge, titre "Échec du build", sous-texte "3 erreurs détectées".
3. Un menu contextuel (clic-droit) : liste verticale d'items avec icône + label + raccourci clavier aligné à droite en gris ("Dupliquer  Ctrl+D", "Supprimer  Suppr", séparateur fin, "Créer un Blueprint depuis la sélection", "Isoler la sélection ▸" avec flèche de sous-menu).
4. Un tooltip simple : petit rectangle fond sombre, texte "Déplacer (W)" avec le raccourci en gris clair.

Disposer les 4 éléments espacés sur la planche, chacun avec une légère ombre portée pour indiquer qu'ils flottent au-dessus de l'interface.
```

---

## Notes d'usage pour Banani

- Générer le **thème Dark en priorité** (thème par défaut du produit), puis dupliquer chaque écran en variante Light en réutilisant le même prompt avec la palette Light substituée.
- Ne jamais laisser Banani interpréter librement la palette : toujours coller les codes hexadécimaux exacts de la section 0.
- Si Banani propose des arrondis prononcés ou un style "grand public" (larges espacements, gros boutons colorés), le corriger explicitement dans le prompt suivant avec "réduire la taille des éléments, augmenter la densité, style outil professionnel type IDE, pas d'app mobile".
- Générer les écrans dans l'ordre du sommaire (1 → 12, avec les variantes 8bis/8ter/8quater/8quinquies juste après le Content Browser) : les écrans 3 à 6 partagent la même coquille (chrome), la cohérence visuelle sera plus facile à obtenir en les enchaînant.
