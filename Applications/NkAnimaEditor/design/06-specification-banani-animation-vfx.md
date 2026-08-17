# Prompts de génération d'écrans — Aetherion Animate & FX
### Document 6/6 — Destiné à Banani

> Réutilise le même "Système de design" que `03-specification-banani.md` (section 0 de ce document, identique). Cette suite partage la coquille d'éditeur déjà générée (barre de titre/menu/toolbar/statusbar) — pour les écrans ci-dessous, ne régénère que le contenu central + panneaux dockables spécifiques, en gardant la même coquille visuelle que les écrans du document 3.

---

## 0. Système de design (identique au document 3 — à coller en préfixe de chaque prompt)

```
Style général : interface professionnelle de moteur de jeu / suite d'animation 3D, type Unreal Engine 5 / Maya.
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

Typographie : police sans-serif système, texte petit, labels de section en majuscules discrètes.
Icônes : style trait fin (outline, 1.5px), monochromes, jamais d'emoji.
Coins arrondis légers (4-6px) sur boutons/cartes, angles droits sur les panneaux dockables.
Le viewport 3D garde un fond gris très sombre (#1e1e1e) même en thème Light.
Boutons primaires = fond accent, texte clair. Boutons secondaires = contour fin, fond transparent.

Couleurs spécifiques à cette suite (fixes, ne suivent pas le thème) :
- os du squelette : cyan #3fd0e0, os sélectionné : orange #f0883e
- heatmap de poids de skin : dégradé bleu #1f6feb (poids 0) → rouge #f85149 (poids 1)
- courbes d'animation : rouge #f14c4c (X), vert #3fb950 (Y), bleu #2f81f7 (Z)
- câbles de flux de pose (AnimGraph) : violet #a371f7, plus épais que les câbles de données classiques
- onion skinning : bleu #2f81f7 (frames passées), orange #e3833b (frames futures)
```

---

## 1. Workspace Switcher — barre d'outils avec sélecteur

```
[Coller le Système de design]

Génère uniquement un fragment de barre d'outils horizontale (36px de haut), thème DARK, montrant à droite d'un groupe d'icônes existant un dropdown "Animation ▾" avec une icône de personnage stylisée devant le texte, et un fin liseré bleu de 2px sous tout le dropdown pour indiquer le workspace actif. À côté, montrer 3 autres dropdowns grisés/inactifs en fond pour suggérer "Rigging", "VFX", "2D" (juste leurs icônes, non actifs, sans liseré).
```

---

## 2. Skeletal Mesh Editor — Viewport avec squelette overlay

```
[Coller le Système de design]

Génère le Viewport 3D d'un éditeur de squelette de personnage, thème DARK, plein écran.

Fond gris très sombre avec grille de sol en perspective. Un personnage 3D humanoïde stylisé (silhouette simple, low-poly gris clair semi-transparent) au centre, debout en pose en T.

Superposé au personnage : un squelette en overlay — lignes fines cyan reliant des petites sphères aux articulations (épaule, coude, poignet, hanche, genou, cheville, colonne vertébrale, crâne), toutes bien visibles à travers le mesh semi-transparent. Un os au niveau de l'avant-bras droit est sélectionné : ses deux sphères d'articulation et la ligne entre elles sont surlignées en orange vif, avec un petit gizmo de rotation (anneau coloré) autour de l'articulation du coude.

Overlay en haut à gauche : menus déroulants "Perspective ▾", "Lit ▾", "Show ▾" (identiques au viewport standard). Overlay en haut à droite : icônes caméra/capture/plein écran + petit cube de navigation.

Barre d'outils juste au-dessus du viewport (dans le panneau) : icônes Sélection/Rotation locale (actif, surligné), bouton "Pose de référence", toggle "Local/World", bouton "Créer Physics Asset".
```

---

## 3. Skeleton Tree + Details Panel (Bone / Skin Weights)

```
[Coller le Système de design]

Génère deux panneaux côte à côte, thème LIGHT.

Panneau gauche "Skeleton Tree" : barre de recherche en haut, arbre hiérarchique indenté avec icône "os" devant chaque ligne : "Racine" > "Colonne" > "Torse" > "Épaule_D" > "Bras_D" (ligne surlignée bleu = sélectionnée) > "Avant-bras_D" > "Main_D". Chaque ligne a une icône œil à gauche et une icône cadenas discrète à droite (certaines verrouillées en gris).

Panneau droit "Details" : en-tête "Avant-bras_D" avec icône os. Section "Bone" non repliable : Longueur (champ numérique), Rotation locale avec 3 champs X (rouge) Y (vert) Z (bleu), dropdown "Translation Retargeting : Animation". Section repliable "Skin Weights" ouverte : liste de 3 os influençant avec sliders horizontaux de poids ("Avant-bras_D : 0.85", "Main_D : 0.10", "Bras_D : 0.05"), total "1.00" affiché en vert en bas de la section. Section repliable fermée "IK Constraint" en dessous.
```

---

## 4. Physics Asset Editor — ragdoll

```
[Coller le Système de design]

Génère l'écran Physics Asset Editor, thème DARK, plein écran.

Viewport central : le squelette humanoïde estompé en fond gris, avec des formes de collision en bleu clair semi-transparent superposées : capsules le long des bras/jambes/torse, une sphère sur la tête. Aux articulations, de petits widgets coniques colorés (jaune translucide) représentant les limites angulaires des contraintes (swing/twist).

Panneau gauche "Bodies" : liste avec icônes de forme (capsule/sphère/boîte) devant chaque nom d'os ayant un corps physique, toggles activé/désactivé à droite de chaque ligne.

Panneau droit "Details" : pour la capsule sélectionnée (avant-bras) — Type de forme (dropdown "Capsule"), Rayon, Longueur, Masse, Amortissement linéaire/angulaire (sliders). En dessous, section contrainte avec le mini-widget conique agrandi montrant visuellement la plage de mouvement.

Barre d'outils en haut : bouton "Simuler" (icône triangle, distincte du Play principal, couleur orange), slider "Gravité de test", bouton "Générer automatiquement les corps".
```

---

## 5. Dope Sheet — timeline d'animation par os

```
[Coller le Système de design]

Génère le panneau "Dope Sheet", thème DARK, plein écran horizontal.

Colonne gauche : liste des os avec chevrons repliables — "Colonne" (fermé), "Bras_D" (fermé), "Jambe_G" (ouvert, révélant en dessous indenté "Position.X", "Rotation.Y" en petit texte gris).

Zone droite : règle temporelle en haut ("0, 12, 24, 36, 48 frames"), une piste fine tout en haut avec des petits triangles colorés = Anim Notifies (un triangle bleu avec icône haut-parleur légendé "Pas_Son", un triangle orange avec icône étincelle légendé "Effet_Poussière"). En dessous, une rangée résumée par os avec des losanges de keyframe positionnés à différents instants (plus denses sur "Jambe_G"). Curseur de lecture rouge vertical traversant tout, positionné vers le tiers de la timeline.

Barre d'outils : bouton toggle "Dope Sheet / Curve Editor" (Dope Sheet actif), transport bar en dessous avec Play/Pause, frame actuelle "24 / 96", slider de vitesse.
```

---

## 6. Curve Editor — courbes d'animation

```
[Coller le Système de design]

Génère le panneau "Curve Editor", thème DARK, plein écran, remplaçant la zone timeline du Dope Sheet précédent par un graphe de courbes.

Axes fins gris (temps horizontal, valeur vertical). Trois courbes lisses superposées : une rouge (Position.X), une verte (Position.Y), une bleue (Position.Z), avec des points de keyframe en petits cercles blancs sur chaque courbe et de fines lignes de tangente (poignées) dépassant de part et d'autre de 2-3 keyframes sélectionnés, terminées par de petits carrés oranges.

Colonne gauche : cases à cocher pour afficher/masquer chaque canal (X coché rouge, Y coché vert, Z décoché gris).

Barre d'outils : "Cadrer la sélection", "Normaliser", slider "Lissage", et un petit menu contextuel flottant ouvert près d'un keyframe sélectionné listant "Auto / Linéaire / Constant / Cassée" (Auto surligné).
```

---

## 7. AnimGraph — State Machine

```
[Coller le Système de design]

Génère l'écran "AnimGraph / State Machine", thème DARK, plein écran, même style que l'éditeur de Blueprint (graphe de nœuds).

Breadcrumb en haut du graphe : "AnimGraph > Locomotion" cliquable.

Nœuds = états, rectangles arrondis avec une mini-vignette d'aperçu (icône silhouette en pose) et un nom : "Entry" (rond plein noir/blanc, point de départ, en haut à gauche), relié par une flèche à "Idle" (état actif, bordure bleu accent épaisse), qui a des flèches bidirectionnelles vers "Marche" et "Course", elle-même reliée à "Saut". Chaque flèche de transition est une ligne fine avec une petite étiquette au milieu indiquant une condition résumée ("Vitesse > 0.1").

En bas à gauche, un panneau plus petit "My Blueprint" avec une liste de variables (pastilles colorées : bleu=bool "EstEnCourse", vert=float "Vitesse").
Panneau droit "Details" : condition de transition sélectionnée avec un mini-graphe de comparaison simple ("Vitesse" > "0.1").
```

---

## 8. Blend Space — grille 2D

```
[Coller le Système de design]

Génère l'écran "Blend Space", thème LIGHT, plein écran.

Zone centrale : un grand carré avec grille fine, axe horizontal étiqueté "Direction (-180 à 180)", axe vertical étiqueté "Vitesse (0 à 600)". Plusieurs points en forme de losange bleu positionnés dedans, chacun avec une petite étiquette de nom de clip à côté ("Idle" au centre-bas, "Marche_Avant" en haut au centre, "Course_Avant" tout en haut, "Marche_Diag_D" décalé à droite). Des triangles semi-transparents gris fins relient les losanges voisins (triangulation visible). Un curseur en forme de croix rouge est positionné entre "Marche_Avant" et "Marche_Diag_D", représentant le point de preview actuel.

Panneau droit : liste des échantillons avec leurs coordonnées X/Y éditables en petits champs numériques, bouton "Trianguler" en haut.
Un petit viewport de preview en bas à droite montrant le personnage jouant l'animation blendée résultante.
```

---

## 9. Retargeting — mapping de squelettes

```
[Coller le Système de design]

Génère l'écran "Retargeting", thème DARK, plein écran, deux colonnes.

Colonne gauche : arbre du squelette source, titre "Squelette Source (Mannequin_A)". Colonne droite : arbre du squelette cible, titre "Squelette Cible (Mannequin_B)", structure similaire mais noms légèrement différents. Entre les deux arbres, des lignes fines courbes reliant chaque os correspondant (comme des câbles de node graph mais horizontaux). Deux os sont surlignés en orange avec un contour pointillé rouge des deux côtés = non mappés, sans ligne de connexion.

En haut : boutons "Auto-mapper par nom", "Auto-mapper par proximité".
En bas : deux petits viewports côte à côte avec le même personnage en pose légèrement différente, sous-titrés "Animation source" et "Résultat retargeté", synchronisés par une transport bar commune en dessous.
```

---

## 10. Montage Editor — composition de clips

```
[Coller le Système de design]

Génère l'écran "Montage Editor", thème DARK, plein écran horizontal.

3 pistes horizontales empilées ("Slot Défaut", "Slot Réaction", "Slot Effets"). Sur la première piste, deux blocs rectangulaires colorés bout à bout avec un léger chevauchement en diagonale (triangle de fondu visible à la jonction) : bloc bleu "Attaque_Windup" suivi d'un bloc orange "Attaque_Impact". Au-dessus de la timeline, des marqueurs de section verticaux nommés "Windup", "Impact", "Recovery" avec des étiquettes.
Une piste fine tout en haut avec des triangles de Anim Notifies, identique visuellement au Dope Sheet.
Transport bar en bas.
```

---

## 11. 2D Rig Editor

```
[Coller le Système de design]

Génère l'écran "2D Rig Editor", thème LIGHT, plein écran.

Viewport central : fond légèrement quadrillé façon damier de transparence, un personnage 2D en illustration plate stylisée vu de face (plusieurs formes/calques distincts : tête, torse, bras, jambes, comme un personnage cutout). Superposé : un squelette 2D en losanges allongés cyan reliant les articulations du personnage, un os du bras surligné en orange.

Panneau droit "Slots" : liste avec icône image devant chaque nom ("Slot_Tête" → "tete_illustration.png", "Slot_Bras_D" → "bras_droit.png", une ligne vide "Slot_Chapeau" → non assigné, en gris pointillé).

Barre d'outils en haut : icônes "Créer un os", "Créer un maillage", "Assigner un slot", "Pondérer (weight paint)" avec un slider de taille de brosse visible à côté (actif).
```

---

## 12. Sprite Atlas Editor

```
[Coller le Système de design]

Génère l'écran "Sprite Atlas Editor", thème DARK, plein écran.

Zone centrale : une grande texture atlas (mosaïque de sprites de personnage colorés, façon planche de sprites) avec des rectangles de découpe fins blancs superposés sur chaque sprite individuel, une petite croix rouge (pivot) visible au centre-bas d'un sprite agrandi/sélectionné, poignées de redimensionnement carrées aux coins du rectangle sélectionné.

Panneau droit : liste de sprites détectés avec miniature carrée, nom, dimensions en pixels ("64x64px").
Barre d'outils en haut : bouton "Découpage automatique" avec dropdown "Par grille / Par détection alpha", champs de taille de grille.
```

---

## 13. Onion Skinning — overlay viewport 2D

```
[Coller le Système de design]

Génère le Viewport 2D en gros plan, thème DARK, montrant l'effet d'onion skinning.

Un personnage 2D cutout en pose de course, dessiné net et opaque au centre (frame actuelle). Autour de lui, en superposition semi-transparente : deux silhouettes légèrement décalées teintées bleu clair de plus en plus transparentes vers la gauche (poses des 2 frames précédentes), et deux silhouettes teintées orange clair de plus en plus transparentes vers la droite (poses des 2 frames suivantes) — donnant une impression de mouvement en éventail.

Overlay bas : petit slider "Frames avant : 2 / Frames après : 2", toggle "Silhouette seule" (désactivé).
```

---

## 14. VFX Editor — Emitter Stack

```
[Coller le Système de design]

Génère l'écran VFX Editor, thème DARK, plein écran, structure en 3 colonnes.

Colonne gauche "Emitter Stack" : 2 cartes d'émetteurs empilées verticalement. Première carte "Émetteur_Flammes" (dépliée, icône flamme) : catégories internes en sous-sections colorées — en-tête vert "Spawn" avec une ligne de module "Spawn Rate" (toggle actif), en-tête bleu "Initialize" avec "Initial Velocity" et "Initial Color", en-tête jaune "Update" avec "Gravity Force" (toggle désactivé, ligne grisée), en-tête violet "Render" avec "Sprite Renderer". Bouton "+ Ajouter un module" discret sous chaque catégorie. Deuxième carte "Émetteur_Étincelles" repliée (juste l'en-tête visible avec un chevron fermé et "142 particules actives" en petit texte).

Colonne centrale : viewport de preview VFX avec un effet de flammes stylisé (formes oranges/jaunes semi-transparentes montantes) sur fond damier de transparence, overlay "312 particules actives" en haut à droite, bouton "Reset simulation", slider de vitesse de simulation en bas.

Colonne droite "Details" : paramètres du module "Spawn Rate" sélectionné avec un champ de type Distribution montrant un petit dropdown "Constante / Plage aléatoire / Courbe" à côté de la valeur, actuellement sur "Plage aléatoire" avec deux champs Min/Max.
```

---

## 15. VFX Library Browser

```
[Coller le Système de design]

Génère le "VFX Library Browser", thème DARK, variante du Content Browser.

Grille de miniatures carrées, chacune montrant un aperçu figé d'un effet visuel différent (flammes oranges, éclaboussure d'eau bleue, étincelles magiques violettes, impact de poussière grise, aura d'ambiance verte). Sous chaque miniature : le nom de l'effet et 1-2 petites pilules colorées de tag ("Feu", "Impact"). Une miniature a une icône de lecture (triangle play) semi-transparente au survol pour indiquer l'aperçu vidéo en boucle.

Barre du haut : filtres toggle "Système / Émetteur / Module", barre de recherche.
```

---

## Notes d'usage pour Banani

- Générer d'abord les écrans 2 à 10 (pipeline 3D squelette/animation) dans l'ordre, puis 11-13 (2D), puis 14-15 (VFX) — ils partagent de moins en moins de structure visuelle entre groupes.
- Pour chaque écran, si Banani régénère aussi la barre de titre/menu/toolbar générale, vérifier qu'elle reste identique aux écrans du document 3 (même moteur de jeu, même produit) — sinon la recoller manuellement en post-traitement.
- Comme pour le document 3 : toujours coller les codes hexadécimaux exacts, corriger explicitement toute dérive vers un style "grand public" (arrondis excessifs, espacements larges).
