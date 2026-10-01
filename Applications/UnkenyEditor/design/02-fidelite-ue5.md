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

**Gestes obligatoires** : sélectionner fichiers **et dossiers** ; **couper / copier
/ coller** (Ctrl+X / C / V) ; **dupliquer** ; **renommer** (F2) ; **supprimer**
(Suppr, avec confirmation et références) ; **glisser-déposer** d'un dossier vers un
autre (menu « Déplacer ici / Copier ici » comme Unreal) ; **glisser** un asset dans
le viseur pour le **poser** ; **déposer** des fichiers ou des dossiers depuis
l'explorateur du système = importer ; clic droit sur un asset, un dossier, le
vide : menus complets. Le sélecteur de fichiers est **celui de NKEditorKit**
(jamais un nouveau).

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
