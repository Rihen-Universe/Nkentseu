# UnkenyEditor — fidélité à Unreal Engine 5

### Document 02 — ce que Rihen veut voir, panneau par panneau

> Demande de Rihen, 01/10/2026, après avoir essayé l'éditeur : *« on doit
> retravailler ça pour avoir réellement de l'Unreal 5 »*. Références qu'il a
> données (à ouvrir, pas à recopier dans le dépôt : images sous droits) :
>
> - les zones du Content Browser (UE 5.1) :
>   https://d1iv7db44yhgxn.cloudfront.net/documentation/images/70c4db53-ac9f-44f0-845e-33464b63940a/ue5_1-content-browser-areas.png
> - l'éditeur UE 5.8 complet (Place Actors, Outliner, Details, Content Browser) :
>   https://forums.unrealengine.com/t/ue-5-8-content-browser-not-interactive-and-plugins-missing-from-edit-menu/2760383
>   (captures : …/original/4X/5/1/d/51d846a40bb4295fee678597aeb5c13a136ff38f.png et
>   …/original/4X/8/f/6/8f6d65760903626638316ef26fba1da421a2be87.png sur d3kjluh73b9h9o.cloudfront.net)
> - les vignettes du Content Browser : https://www.reddit.com/r/unrealengine/comments/14ubhme/ue5_content_browser_thumbnails/
>
> Règle : **fidèle à la disposition et aux gestes d'Unreal**, appliqués à la 2D,
> avec les couleurs et le logo d'Unkeny. Rien de l'existant n'est détruit : on
> réorganise et on complète.

---

## 0. Portée : toute la famille, chacun avec sa touche

Rihen, 01/10 : *« ce n'est pas seulement valable pour UnkenyEditor, mais aussi pour
Nogee, NkAnimaEditor, PV3DE, NK3DModeler ; mais chacun doit apporter une touche
particulière à cette interface »*. Images de référence de Rihen (hors dépôt, sous
droits) : `C:\Users\rihen\Documents\Projects\References\UE5\`.

**Où écrire** : dans **NKEditorKit**, jamais en copie dans chaque application. Le
socle existe : `Components/NkContentBrowserModel.h` (+ `NkContentBrowserDraw.cpp`,
`NkTreeViewModel.h`, `NkTabStripModel.h`, `NkEditorInspector.h`, `NkTheme.h`),
écrit comme **modèle neutre + jetons + variantes + greffes**, déjà consommé par
NKUIDesign et NKCraft (derrière `NK_KIT_BROWSER=1`). La spécification de famille
« Aetherion » (style UE5, thèmes clair et sombre, docking, Content Browser et
Outliner partagés) est dans `Applications/Nogee/design/` (documents 1 à 3) et
`Applications/NkAnimaEditor/design/04` à `06`.

**Directive de Rodolf du 30/08, toujours valable** : la variante par défaut est le
mixte Unreal + Aetherion ; une application ne bascule sur le composant partagé
qu'une fois la **parité avec son navigateur historique atteinte et validée sur
capture** ; les navigateurs existants (`NKCraft/Shell/NkModelerBrowser.h`,
`Nogee/Panels/ContentBrowserPanel.cpp`, `Nogee/Panels/AssetBrowser.cpp`) restent
intacts jusque-là.

**La touche de chacun** (proposition, à valider par Rihen) :

| application | sa touche |
|---|---|
| **UnkenyEditor** (jeux 2D) | formes 2D et acteurs de jeu dans *Placer des acteurs* ; aperçus de sprites et de scènes 2D ; logo Unkeny (tuiles / rebond) |
| **Nogee** (jeux 3D, Noge) | primitives 3D, lumières et volumes 3D ; aperçus rendus des maillages et matériaux |
| **NkAnimaEditor** (animation, VFX) | frise temporelle au premier plan ; assets d'animation, squelettes, contrôleurs `.nkanimctl` avec aperçu animé au survol |
| **PV3DE** (patient virtuel) | les **cas** (`.nkcase`) et les patients comme assets ; palette plus calme ; mention « simulation pédagogique » |
| **NKCraft** (modélisation) | l'araignée ; vignettes rendues des maillages ; générateur de créatures |

**Ordre** : le composant partagé d'abord, adopté par **UnkenyEditor** en premier
(c'est lui que Rihen teste), puis les autres applications, une par une, chacune
après validation sur capture.

## 1. Ce que Rihen a constaté (01/10)

1. Le navigateur de contenu : **on ne peut pas sélectionner les dossiers** ; pas de
   copier / coller / couper, pas de **déplacer** par glisser, pas de **poser** des
   fichiers ou des dossiers ; à gauche il n'y a que « Acteur ».
2. L'**entité active** (case dans l'en-tête de Détails) n'est pas visible : le
   moteur la porte (commit `a567dd01c`), l'interface pas encore.
3. Il faut l'**icône carrée en haut à gauche** qui couvre la ligne des menus et
   celle des onglets, comme le logo rond d'Unreal.
4. Le **panneau Détails** doit être dessiné comme celui d'Unreal.
5. Il faut des **acteurs de base** à poser, comme le panneau *Place Actors*
   d'Unreal : des formes (rectangle, cercle, triangle, étoile, polygone…) et des
   acteurs de jeu.

## 2. La disposition générale (UE 5.8)

| zone | Unreal | Unkeny |
|---|---|---|
| coin haut gauche | logo rond sur **deux lignes** (menus + onglet de niveau) | logo Unkeny carré, sombre et clair (choix du tour 2 en attente, sinon provisoire) |
| ligne 1 | menus `File Edit Window Tools Build Platforms Select Actor Help`, nom du projet à droite, boutons de fenêtre | idem en français |
| ligne 2 | onglet du niveau (« Untitled ») | onglets de scène |
| barre d'outils | Enregistrer, *Selection Mode*, **Ajouter** (acteur), Blueprints, cinématiques, **Jouer / Pause / Stop / Éjecter**, ⋮ | idem, sans ce qui n'existe pas encore |
| gauche | **Place Actors** (voir § 4) | nouveau panneau « Placer des acteurs » |
| centre | viseur et sa barre (outils, accrochages 10 / 10° / 0,25, Perspective, Lit, affichage) | déjà en place (barre flottante) |
| droite haut | **Outliner** : colonnes œil, étoile, *Item Label*, *Type* ; dossiers ; recherche ; « 8 actors (1 selected) » | compléter : colonne Type, dossiers, ligne d'état |
| droite bas | **Details** (voir § 5) | à redessiner |
| bas | **Content Browser** ancrable (voir § 3) | à refaire |
| barre d'état | *Content Drawer*, *Output Log*, *Cmd* + console, à droite état de sauvegarde, contrôle de version | ajouter la barre d'état |

## 3. Le Content Browser (les 7 zones d'UE 5.1)

1. **Barre** : `+ Ajouter` (créer : dossier, scène, prefab, contrôleur d'animation,
   matériau 2D…), `Importer`, `Tout enregistrer`, **précédent / suivant**, **fil
   d'Ariane** cliquable (`Tout > Contenu > …`), verrou, réglages.
2. **Sources** (volet gauche, redimensionnable) : *Favoris* (repliable, avec
   recherche) ; **le projet** en arborescence de dossiers à flèches (repliable,
   avec recherche) ; un dossier se **sélectionne**, se **renomme**, se **glisse**.
3. **Collections** (repliable, `+`, recherche, compteur par collection).
4. **Filtres** par type (Scène, Prefab, Image, Son, Animation…).
5. **Recherche** + menu de filtres + **tri**.
6. **Vue des assets** : **vignettes** (aperçu réel pour images et scènes, icône par
   type sinon), **bande de couleur du type** sous la vignette, nom, type en petit ;
   sélection simple, **Ctrl** et **Maj** ; double-clic = ouvrir ; vide =
   « Aucun résultat. Déposez des fichiers ici ou faites un clic droit pour créer du
   contenu. » ; **ligne d'état** « N éléments (M sélectionnés) » ; taille des
   vignettes réglable.
7. **Réglages** de la vue (taille, colonnes, afficher les dossiers…).

> **État au 01/10 (étape 1, branche `ue5/navigateur-partage`)** — fait dans le
> composant PARTAGÉ de NKEditorKit (variante `unreal`, `NkContentBrowserUnreal.cpp`)
> et adopté par le tiroir « Contenu » : les sept zones ; sources *Favoris*, projet
> (arbre « Contenu » + catalogue « Acteurs »), *Collections* avec compteur, chacune
> repliable avec sa loupe ; puces de type ; recherche, tri (nom, date, taille,
> type ; sens) ; vraies vignettes des images, icônes Unkeny des autres natures ;
> sélection simple / Ctrl / Maj / cadre ; double-clic ; état vide ; ligne d'état ;
> taille des vignettes (Réglages, Ctrl+molette) ; verrou ; précédent / suivant.
> **Partiel** : pas de colonnes réglables (la vue liste existe), pas encore de
> miniature RENDUE d'une scène ou d'un prefab (une icône Unkeny à la place).

### 3.1 Les cartes de fichiers et de dossiers

Rihen, 01/10 : *« même les cartes de fichier et de dossier doivent tendre vers
Unreal, avec nos propres touches »*. Références d'images à venir de Rihen (dossier
hors dépôt : `C:\Users\rihen\Documents\Projects\References\UE5\`).

- **Carte d'asset (Unreal)** : vignette carrée sur fond sombre, **bande fine de la
  couleur du type** juste sous la vignette, nom sur deux lignes au plus (coupé
  proprement), type en petit et en gris en bas ; survol = fond éclairci ;
  sélection = fond bleu plein ; plusieurs tailles de vignette.
- **Carte de dossier (Unreal)** : grande icône de dossier (sa **couleur se choisit**
  par dossier, comme Unreal), nom dessous ; même survol, même sélection.
- **Nos touches** : les couleurs de la famille (bleu d'état, ambre pour la
  sélection secondaire et le glisser), le logo et les icônes de types d'Unkeny
  (scène, prefab, contrôleur d'animation, image, son, police, matériau 2D), des
  coins à peine arrondis, et de **vrais aperçus** : l'image elle-même, la première
  image d'une animation, une miniature rendue de la scène ou du prefab.

**Gestes obligatoires** : sélectionner fichiers **et dossiers** ; **couper / copier
/ coller** (Ctrl+X / C / V) ; **dupliquer** ; **renommer** (F2) ; **supprimer**
(Suppr, avec confirmation et références) ; **glisser-déposer** d'un dossier vers un
autre (menu « Déplacer ici / Copier ici » comme Unreal) ; **glisser** un asset dans
le viseur pour le **poser** ; **déposer** des fichiers ou des dossiers depuis
l'explorateur du système = importer ; clic droit sur un asset, un dossier, le
vide : menus complets. Le sélecteur de fichiers est **celui de NKEditorKit**
(jamais un nouveau).

> **État au 01/10 (étape 1)** — fait : sélection des fichiers ET des dossiers ;
> Ctrl+X / C / V, Ctrl+D, F2 en place, Suppr avec confirmation et « Cité par » ;
> glisser vers un dossier (grille, rail, favori) → « Déplacer ici / Copier ici »,
> toute la sélection voyage ; image et prefab glissés dans le viseur = sprite ou
> instance ; dépôt de fichiers ET dossiers de l'OS = import ; clic droit complet
> (asset, dossier, vide, collection) ; couleur de dossier (Unreal « Set Color »),
> favoris, collections, gardés dans `Contenu/.nknavigateur`. Le disque passe par
> `NkContentBrowserDisque.h` (confiné au Contenu, jamais d'écrasement, corbeille).
> Témoins e51 à e56 du banc de l'éditeur. **Partiel** : un dépôt de l'OS vise une
> carte de dossier de la grille, pas encore une rangée du rail.

## 4. Placer des acteurs (Place Actors)

Panneau à onglets verticaux à icône, comme Unreal : **Favoris, Récents, Base,
Lumières, Formes, Effets, Volumes, Tout**, avec recherche, liste à icônes,
**glisser vers le viseur** pour poser.

- **Formes** (maillages 2D, avec collision et couleur réglables) : rectangle (et
  carré), cercle, ellipse, triangle, **étoile** (nombre de branches), **polygone
  régulier** (nombre de côtés), capsule, ligne / chaîne, polygone libre.
- **Base** : Acteur vide, Point de départ du joueur, Caméra, Personnage (contrôleur
  de personnage), Corps mou, Sprite, Texte, Son, Déclencheur (zone).
- **Lumières** (si l'éclairage 2D est actif) : lumière ponctuelle, projecteur,
  ambiante.
- **Effets** : feu, fumée, émetteur de particules.
- **Volumes** : zone de déclenchement, zone de physique (vent, eau…), limites de
  caméra.

> **État au 01/10 (branche `unkeny/formes-2d-placer-acteurs`)** — fait : le panneau
> à gauche (`NkEditeurPlacer.cpp`), ses huit onglets, la recherche, les favoris et
> récents, clic = au centre de la vue, glisser = au point lâché (un volume lâché sur
> une entité devient son collisionneur). Les **formes 2D** sont un composant du
> moteur (`Unkeny/Scene/NkUnkenyFormes.h`, dessin dans `NkDessinerScene`) avec leur
> collisionneur assorti ; collisionneurs polygone / chaîne / rotation, **calques de
> collision** (Fenêtre > Réglages du projet : collision), poignées dans la vue et
> **aimant** (sommets, arêtes, faces ; V maintenue). Base : acteur vide, sprite,
> personnage, caisse, balle, corps mou, son, déclencheur. Lumières : ponctuelle,
> projecteur, directionnelle. Volumes : déclencheur et volumes bloquants.
> **Pas fait, faute de moteur** : point de départ du joueur, caméra entité, texte
> de scène, zones de physique (vent, eau) et limites de caméra — Unkeny ne les porte
> pas encore. Bancs FORMES (28) et FORMES EDITEUR (22) ; captures
> `References\Captures\formes\`.

## 5. Le panneau Détails (dessin d'Unreal)

- **En-tête** : icône du type, **nom** de l'acteur, **case « active »** (Unity
  `GameObject.active` : éteint = ni dessiné, ni simulé, ni scripté, en édition
  comme en jeu ; distinct de l'œil de l'Outliner), `+ Ajouter` (composant),
  boutons d'aide et de verrou.
- **Arbre des composants** de l'acteur (`Sol (Instance)` → composants).
- **Recherche** dans les propriétés + boutons d'affichage.
- **Filtres par catégorie** en pastilles : Général, Acteur, Physique, Rendu,
  Animation, Audio, Tout.
- **Catégories repliables** (triangle), titres en gras, lignes `nom | valeur`,
  colonne des noms alignée.
- **Transform** : `Position / Rotation / Échelle` avec liste déroulante (relatif /
  absolu), champs **X / Y avec liseré rouge / vert** à gauche, **verrou** de
  l'échelle, flèche de **réinitialisation** à droite de chaque ligne modifiée ;
  *Mobilité* en boutons segmentés.
- **Références d'assets** : vignette + liste déroulante + boutons « parcourir » et
  « utiliser la sélection ».
- **Avancé** repliable sous chaque catégorie.

## 6. Méthode

- Une étape à la fois, dans cet ordre : **3** (Content Browser), **5** (Détails, y
  compris la case « active »), **4** (Placer des acteurs et formes), **2** (icône
  du coin, barre d'état, colonnes de l'Outliner).
- À chaque étape : témoins au banc de l'éditeur (avec contre-épreuve), et une
  **capture hors écran** montrée à Rihen avant de passer à la suivante.
- Ne jamais piloter la vraie souris de Rihen.
