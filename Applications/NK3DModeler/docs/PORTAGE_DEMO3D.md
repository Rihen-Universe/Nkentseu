# Portage de `Sandbox --demo=2` (Demo3D) vers NK3DModeler

Inventaire de travail. `Demo3D.cpp` fait 6907 lignes ; l'édition de maillage en
représente environ 35 %, le reste étant de la démo de rendu (GI voxel, ombres
virtuelles, cookies, enregistrement vidéo, 86 objets figés en dur) qui n'a pas
sa place dans un modeleur.

Légende : ✅ porté et câblé · ⚠️ porté mais **dormant** (aucun appelant) · ❌ absent

---

## 1. Navigation et caméra

| Fonctionnalité | Raccourci Demo3D | État |
|---|---|---|
| Orbite | milieu + glisser | ✅ |
| Orbite autour du **centroïde de la sélection** | milieu (si sélection) | ✅ |
| Déplacement (« grab ») | Maj + milieu | ✅ |
| Zoom | molette | ✅ |
| Déplacement vertical / horizontal | Maj / Ctrl + molette | ✅ |
| Retour auto en perspective sur orbite libre | — | ✅ |
| Vues axiales face/arrière, droite/gauche, dessus/dessous | pavé 1 / 3 / 7 (+Ctrl) | ⚠️ `Viewport3DAxisView` |
| Bascule orthographique | pavé 5 | ✅ |
| Recadrage sur la scène | *absent de Demo3D* | ⚠️ `Viewport3DFrameAll` |
| Mode **vol** (WASD, clic droit = regard, Maj = sprint) | `F` | ❌ |

## 2. Grille et affichage

| Fonctionnalité | Raccourci | État |
|---|---|---|
| Grille infinie | `F1` | ✅ (case « Grille ») |
| Lignes mineures / majeures séparément | `F2` / `F3` | ❌ (une seule case) |
| Axes du shader de grille | `F4` | ✅ (case « Repère d'axes ») |
| Opacité du plan − / + | `F11` / `F12` | ❌ |
| Axes X/Y/Z en vraies lignes 3D | permanent | ❌ |
| **6 modes d'affichage** : PBR, matcap unlit, normale, UV, AO, filaire | `Z` | ⚠️ 4 modes seulement |
| **MatCap — 30 préréglages** | `M` | ❌ (l'interface a le combo, rien derrière) |
| Source de couleur unlit : matériau / gris / personnalisée | `B` | ❌ |
| Filaire n-gon persistant (sans diagonales) | auto | ❌ |
| **X-ray** (voir et sélectionner à travers) | `Alt+Z` | ⚠️ `Viewport3DSetXray` |
| Liseré de sélection (silhouette) | auto | ❌ |
| Cage de boîte englobante | env | ✅ (via le gizmo) |
| VSync | `V` | ❌ |

## 3. Sélection

| Fonctionnalité | Raccourci | État |
|---|---|---|
| Entrée / sortie du mode édition | `TAB` | ✅ (par le combo) |
| Sous-modes sommet / arête / face | `1` `2` `3` | ✅ (exclusifs) |
| Sous-modes **combinés** | Maj+`1/2/3` | ❌ |
| Clic : sommet (14 px) / arête (12 px) / face (rayon) | clic gauche | ✅ |
| Ajouter / basculer | Maj / Ctrl + clic | ✅ |
| Tout sélectionner / désélectionner | `A` / `Alt+A` | ⚠️ `Viewport3DSelectAll` |
| **Boucle d'arêtes / anneau de faces** | Alt + clic | ⚠️ `Viewport3DSelectLoopAt` |
| Rectangle | `B` puis glisser | ⚠️ `Viewport3DSelectRect` |
| Lasso | Ctrl + glisser | ⚠️ `Viewport3DSelectLasso` |
| Cercle (peinture, molette = rayon) | `C` | ⚠️ `Viewport3DSelectCircle` |
| Élément **actif** : sommet, arête | dernier cliqué | ✅ |
| Élément actif : **face** | dernier cliqué | ❌ |
| **Ordre de sélection** (`selOrder`) | auto | ❌ — sans lui, Merge First/Last est faux |
| Propagation aux sommets coïncidents | auto | ❌ |
| Croître / rétrécir, par similarité, lié, inverser | — | ❌ **et absent de `NkEditMesh`** |

## 4. Gizmo et transformations

| Fonctionnalité | Raccourci | État |
|---|---|---|
| Déplacer / tourner / redimensionner / combiné | `G` `R` `S` `C` | ✅ (par l'outil) |
| Effacer translation / rotation / échelle | Alt+`G`/`R`/`S` | ❌ |
| Orientations global / local / normal | `,` | ✅ pour les deux premières |
| Repère **normal calculé depuis l'élément** | auto | ❌ |
| **5 modes de pivot** : médian, boîte, curseur 3D, origines individuelles, actif | `.` | ⚠️ `Viewport3DSetGizmoPivot` |
| **Verrous d'axe X / Y / Z pendant le glissement** | `X` `Y` `Z` maintenus | ❌ (`gin.lockAxis` jamais renseigné) |
| Aimantation temporaire | Ctrl maintenu | ✅ |
| Aimantation persistante | Maj+TAB | ✅ (les trois bascules) |
| **Curseur 3D** : placement, dessin, pivot | Maj + clic droit | ❌ **entièrement absent** |
| Saisie numérique (`G X 2.5 ↵`) | — | ❌ **et absent de Demo3D** |

## 5. Opérations d'édition

| Opération | Raccourci | Paramètres | État |
|---|---|---|---|
| **Extrusion** (face > arête > sommet, décalage zéro) | `E` | région / individuel | ✅ |
| Supprimer les faces | `X` | — | ✅ |
| **Dissoudre** (contextuel V/E/F) | `Ctrl+X` | mode | ✅ |
| **Souder** — 6 modes | `M` / `Maj+M` | centre, premier, dernier, au curseur, effondrer, par distance | ✅ (mode passé en argument) |
| Créer une face (n-gon) | `F` | — | ✅ |
| Créer une arête filaire | — | — | ❌ (`MakeEdgeFromSelected` existe) |
| **Subdiviser** | `W` / `Maj+W` | coupes 1..4 | ✅ |
| **Catmull-Clark** | — | niveaux | ❌ (seulement via modificateur) |
| **Loop cut** | `Ctrl+R` | coupes 1..5, glissement | ✅ (sans glissement) |
| **Bisect / couteau** | `K` + 2 clics | plan | ❌ |
| **Biseau d'arête / de sommet** | `Ctrl+B` / `Ctrl+Maj+B` | largeur, segments 1..16 | ✅ |
| **Inset** | `I` / `Maj+I` / `Alt+I` | épaisseur, profondeur, individuel | ✅ |
| **Séparer les arêtes (rip)** | `V` | écartement | ❌ |
| **Révolution (spin)** | `J` | centre = curseur 3D, axe, angle, pas | ❌ |
| **Sphériser** | `Maj+Alt+S` | facteur 0..2 | ❌ |
| **Gonfler / dégonfler** | `Ctrl+Alt+S` | décalage signé | ❌ |
| Lissage doux / plat | `Maj+S` / `Maj+F` | sélection seule ou tout | ❌ |
| Recalcul des normales | implicite | — | ✅ |
| Quadification à l'entrée en édition | implicite | seuil coplanaire | ✅ |
| **Édition proportionnelle** (6 atténuations) | — | rayon | ❌ **et absent de Demo3D** |
| **Symétrie de maillage** (X/Y/Z) | — | tolérance | ❌ **et absent de Demo3D** |
| Glissement d'arête / de sommet autonome | — | — | ❌ **et absent partout** |
| Booléens | — | — | ❌ **et absent partout** |

## 6. Pile de modificateurs — **absente à 100 %**

`NkModifierStack` est autonome et prêt : `Add / Remove / MoveUp / MoveDown /
SetEnabled / Duplicate / ApplyToBase / Evaluate`, plus une table `NkModParam`
qui décrit chaque paramètre (nom, type, bornes) — donc une interface
**générique** est possible, sans copier la liste à la main.

**17 types**, avec leurs paramètres :

| # | Type | Paramètres |
|---|---|---|
| 0 | **Miroir** | axe X/Y/Z, souder, distance de soudure |
| 1 | **Réseau** | nombre, décalage |
| 2 | **Surface de subdivision** | niveaux, simple ou Catmull-Clark |
| 3 | **Solidifier** | épaisseur, décalage, bordure |
| 4 | **Trianguler** | sommets minimum |
| 5 | **Souder** | distance |
| 6 | **Biseau** | largeur, segments |
| 7 | **Vis** | pas, angle, hauteur, axe |
| 8 | **Séparation d'arêtes** | angle |
| 9 | **Décimer** | angle (mode planaire) |
| 10 | **Construction** | ratio (animable) |
| 11 | **Masque** | inverser |
| 12 | **Projeter** | sphère / cylindre / cube, facteur, rayon |
| 13 | **Déformation simple** | torsion / courbure / effilement / étirement, angle, facteur, axe |
| 14 | **Lisser** | facteur, répétitions |
| 15 | **Onde** | hauteur, largeur, phase, axe |
| 16 | **Lissage par angle** | angle (« auto smooth ») |

L'évaluation est **non destructive** : la cage éditée reste la base, la pile
produit la géométrie affichée. « Appliquer » est le seul geste destructif.

## 7. Système modal (aperçu temps réel) — **absent à 100 %**

Huit opérations ont un aperçu : biseau d'arête, biseau de sommet, inset, loop
cut, révolution, extrusion, sphériser, gonfler. La souris règle un paramètre
continu, la molette un paramètre entier, le clic gauche confirme, Échap ou clic
droit annule.

Le cadre est **générique** — il ne faut pas porter huit fonctions mais un seul
mécanisme : instantané au lancement, restauration puis ré-application à chaque
image (donc jamais cumulatif), verrou souris unique qui neutralise tous les
autres consommateurs, et une seule entrée d'historique à la confirmation.

Détail à ne pas perdre : un **chemin allégé « positions seules »** évite de
recréer les tampons GPU quand la topologie ne change pas (sphériser, gonfler,
glissement du loop cut).

## 8. Annulation, journal, sessions

| Fonctionnalité | État |
|---|---|
| Annuler / rétablir (limite 64) | ⚠️ `Viewport3DUndo/Redo` dormants |
| Journal de commandes (17 opérations sérialisables) | ✅ alimenté |
| Sauvegarde / chargement de session `.nkmec` | ❌ |
| Rejeu pas-à-pas | ❌ |

## 9. Décimation et retopologie

**Absentes de Demo3D**, mais les modules existent dans le moteur —
`NkMeshDecimate` (QEM) et `NkMeshRetopo` (champ transverse), écrits et testés
plus tôt dans ce projet. Le modificateur « Décimer » de la pile n'est qu'une
dissolution d'arêtes coplanaires, pas de la QEM.

## 10. Objets, scène, lumières

Demo3D a **86 objets figés en dur** et aucun moyen d'en ajouter : ce n'est pas
un modèle à suivre. Ce qui est à reprendre :

| Fonctionnalité | État |
|---|---|
| Primitives (sphère, plan, cube) | ❌ |
| Ajout / suppression / duplication / hiérarchie | ❌ **et absent de Demo3D** |
| Persistance du maillage édité par objet | ❌ (un seul objet ici) |
| **Gizmos de lumière** (`NkLightGizmo`), pick, manipulation | ❌ |
| Matériaux éditables | ❌ |

## 11. Rendu

Ombres (5 qualités de PCF, biais, adoucissement), GI voxel à un rebond,
enregistrement vidéo H.264/MJPEG, captures, panneau de débogage des ombres :
tout cela appartient à une démo de rendu. À reprendre plus tard et
sélectivement — les **ombres** ont un sens dans un modeleur, la GI voxel non.

## 12. Pilotage headless

Demo3D expose ~70 variables `NK_*` qui permettent de rejouer n'importe quel
scénario sans souris. C'était son infrastructure de test. Rien d'équivalent
côté modeleur — à considérer le jour où il faudra des tests de non-régression
sur l'édition.

---

## Ordre de portage

1. **Brancher le clavier** sur les fonctions déjà écrites. Une quinzaine
   d'entre elles sont codées et dorment faute d'appelant : c'est le meilleur
   rapport valeur/effort du chantier.
2. **Curseur 3D et les 5 pivots** — prérequis de la révolution, de « souder au
   curseur » et de « sphériser ».
3. **Système modal** — le cadre, pas les huit opérations.
4. **Pile de modificateurs** — interface générique pilotée par `NkModParam`.
5. Opérations manquantes : bisect, rip, révolution, sphériser, gonfler,
   lissage doux/plat.
6. Sessions et rejeu.
7. Multi-objets, primitives, lumières manipulables.
