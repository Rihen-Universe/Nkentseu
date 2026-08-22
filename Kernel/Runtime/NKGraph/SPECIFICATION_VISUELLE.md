# Spécification visuelle de l'éditeur nodal

> Écrit le 2026-08-22 à la demande de Rodolf : *« définir tous les types de nœud
> possibles avec leurs différents éléments, même les nœuds custom, et aussi tu
> donnes les couleurs des types atomiques et des types composés, je veux la
> totale — pense aussi aux tableaux, aux dictionnaires […] sans oublier le cas
> des textures et des matériaux […] même les commentaires et les blocks. »*
>
> Compagnon de `CATALOGUE_NOEUDS.md` (ce qui existe), de
> `DESIGN_EDITEUR_NODAL.md` (la référence de style) et de
> `ELEMENTS_A_DESSINER.md` (l'inventaire). **Ce document-ci est celui qui
> tranche**, ou qui dit explicitement qu'il ne tranche pas.

---

## 0. Comment lire ce document — trois niveaux, jamais mélangés

Chaque affirmation porte une étiquette. **C'est la règle la plus importante du
document** : une spécification qui mélange l'observé et l'inventé se fait
appliquer comme si tout était observé.

| étiquette | ce que ça veut dire |
|---|---|
| **VU** | relevé dans une référence, avec le nom du fichier. Ce n'est pas une opinion. |
| **DÉCIDÉ** | arbitrage déjà rendu par Rodolf. Non rouvert ici. |
| **PROPOSÉ** | ma proposition, **avec sa raison**. Modifiable. |
| 🔴 **NON TRANCHÉ** | aucune référence ne le montre et je n'ai pas de raison défendable. **Ne pas le deviner au codage.** |

Le relevé brut, image par image, est dans `echanges/design.questions.md`, entrée
`Q1`. Ce document en est la **synthèse** ; il ne le remplace pas.

### Périmètre du corpus

12 références distinctes dans `references/` (13 fichiers, dont
`Screenshot 2026-07-17 174628.png` qui est **le doublon exact** de
`PRINCIPALE_flux_automatisation.png`). `planche_01_noeuds.svg/.png` est **notre
production**, pas une référence. `images (1).jpg` **ne montre aucun éditeur
nodal** — c'est une capture de l'éditeur Unreal avec ses 7 zones numérotées ;
je la traite comme une contrainte de cohérence avec l'inspecteur, pas comme une
référence de style (question posée en `Q1`).

---

## 1. Le socle géométrique

### 1.1 Les trois valeurs du fond — **VU**, et mesurées

Échantillonnées sur `PRINCIPALE_flux_automatisation.png` (couleur dominante
d'une zone pleine, pas un pixel isolé) :

| rôle | valeur | mesure |
|---|---|---|
| fond du canevas | `#121212` | dominante sur 120 × 60 px de fond nu |
| corps du nœud | **`#212121`** | dominante sur 120 × 10 px de corps nu |
| contrôle (champ, bouton, bandeau cliquable) | **`#2B2B2B`** | dominante du champ `Enter path…` et du bandeau `+ Click to add data blocks` |
| bandeau de section repliable | `#262626` | dominante du bandeau `1 Variables` |
| en-tête neutre (nœud qui n'exécute pas) | `#2B2B2B` | dominante de l'en-tête `Evaluate` |

> ✅ **Le `#212121` choisi par Rodolf est exactement le corps de la référence
> principale.** Son dessin est déjà aligné ; il n'y a rien à arbitrer.

⚠️ **Correction à `ELEMENTS_A_DESSINER.md` A9.** Ce document annonce un champ
« en creux `#1b1b20`, plus sombre que le corps ». **La mesure dit le contraire** :
le champ est `#2B2B2B`, soit **10 niveaux au-dessus** du corps. La hiérarchie de
la référence est **montante** — fond `#121212` < corps `#212121` < contrôle
`#2B2B2B` — et elle a une raison : sur un fond aussi sombre, un creux plus sombre
que le corps se confond avec le fond dès qu'on dézoome. **PROPOSÉ : adopter la
hiérarchie montante et corriger A9.** (Question ouverte en `Q1`, point 1.)

### 1.2 Rayons, épaisseurs, pas — **DÉCIDÉ** + **VU**

| élément | valeur | source |
|---|---|---|
| rayon du **corps** | **0** | DÉCIDÉ (Rodolf) — appuyé par `174000` et `174227`, qui sont à rayon 0 |
| rayon de l'**en-tête** | **5** (coins hauts seulement) | DÉCIDÉ (Rodolf) |
| filet du corps | 1 px, `#33333C` | PROPOSÉ — il faut *une* séparation avec le fond quand deux nœuds se touchent |
| filet d'exécution sous l'en-tête | **2,5 px** | DÉCIDÉ |
| grille | **points** de 1 px, pas 22 px, `#1C1C1C` | VU (principale : points, pas ~11 px à son échelle) |

⚠️ **La grille est faite de POINTS, pas de lignes** — VU sur la principale.
`174227`, `174000`, `images (2)` et `images (5)` utilisent des lignes ; la
principale gagne, c'est elle le style.

### 1.3 La règle de contraste qui commande tout le reste — **PROPOSÉ**

**Trois plans, jamais plus** : fond, corps, contrôle. Tout élément nouveau doit
se ranger dans l'un des trois. Une quatrième valeur de gris introduit une
hiérarchie que personne ne peut lire — c'est exactement le défaut de
`83576832-…png`, qui a deux teintes de corps *et* deux teintes de colonne, et
qui est la référence la plus difficile à lire du corpus.

---

## 2. Anatomie complète du nœud — tous les micro-éléments

De haut en bas. Chaque ligne est un élément vectoriel dessinable.

```
  ┌───────────────────────────────────────────┐
  │ ▼  Multiplier                          ?  │  ← en-tête (catégorie)
  │    Maths                                  │  ← sous-titre (famille)
  ├═══════════════════════════════════════════┤  ← filet d'exécution
  │  ░░░░░░░░░ aperçu ░░░░░░░░░░░░░░░░░░░░░░  │  ← bloc d'aperçu (escamotable)
  │  ▾ Section                                │  ← en-tête de section
  │ ▌ 1.0 Valeur A          [ 0.500 ]         │  ← rangée de prise
  │ ▌ 1.0 Valeur B             branchée       │  ← rangée branchée (champ retiré)
  │      + ajouter une entrée                 │  ← rangée d'ajout
  │                        Résultat  1.0 ▐    │  ← rangée de sortie
  │  dernière évaluation : 3.5                │  ← ligne d'état
  └───────────────────────────────────────────┘
```

### 2.1 L'en-tête

| micro-élément | spécification | niveau |
|---|---|---|
| **bande** | hauteur 21 px (28 px avec sous-titre), pleine, **couleur = catégorie** | DÉCIDÉ |
| **marqueur de repli `▼`** | triangle plein, 8 px, **collé à gauche dans l'en-tête**, avant le titre. `▼` = déplié, `▶` = replié | VU (`images (3)`, `images (4)` : sur *tous* les nœuds) |
| **titre** | 12,5 px semi-gras, clair, aligné à gauche après le `▼` | VU (principale) |
| **sous-titre** | 9,5 px, **50 % d'opacité**, sur une seconde ligne sous le titre. Porte la **famille** (`Maths`, `Transform`) quand le titre porte l'**opération** (`Multiplier`, `GetWorldX`) | VU (`174227`) |
| **`?` d'aide** | cercle discret 11 px, **toujours à l'extrême droite** | VU (principale : `Evaluate`, `If`) |
| **pictogramme de famille** | 12 px, optionnel, **entre le `▼` et le titre** | VU (`174000` le met à droite, `174057` à gauche). **PROPOSÉ : à gauche** — à droite il entre en conflit avec le `?` et avec la prise de sortie d'en-tête |
| **prise de sortie sur l'en-tête** | pour un nœud à **sortie unique**, la sortie se pose à l'extrême droite de l'en-tête | VU (`images (3)`, `images (4)`) — **PROPOSÉ : on ne le reprend pas** (voir § 3.4) |

**Le sous-titre n'est pas décoratif.** `Multiplier / Maths` et `Multiplier /
Vecteur` sont deux nœuds différents que le seul titre confond. C'est la raison
pour laquelle `174227` le porte sur *tous* ses nœuds.

### 2.2 Le filet d'exécution — **DÉCIDÉ**

Trait plein de 2,5 px collé sous l'en-tête, sur toute la largeur :

- **orange `#F79A28`** — le nœud **exécute** (il a au moins une prise d'exécution) ;
- **pétrole `#0A555F`** — le nœud **n'exécute pas** (il calcule).

⚠️ **Un nœud de matériau porte donc TOUJOURS le filet pétrole**, sans exception
(§ 6.4). C'est le seul indice qui reste quand on a dézoomé au point de ne plus
lire les prises, et c'est ce qui permet la lecture en diagonale d'un blueprint.

### 2.3 Le corps

| micro-élément | spécification | niveau |
|---|---|---|
| **bloc d'aperçu** | pleine largeur moins 8 px de marge, **immédiatement sous le filet, AVANT la première rangée**. Escamotable : quand il disparaît, les rangées remontent et le nœud raccourcit d'autant | VU (`images (4)`, les deux états côte à côte) |
| **en-tête de section** | bandeau `#262626` pleine largeur, `▾`/`▸` à gauche, libellé, **comptage optionnel à gauche du libellé** (`1 Variables`) | VU (principale) |
| **rangée** | hauteur 24 px. Étiquette à gauche, contrôle aligné à droite | VU |
| **rangée d'ajout** | `+ ajouter une entrée`, texte centré atténué sur bandeau `#2B2B2B` | VU (principale : `+ Click to add data blocks`) |
| **ligne d'état** | 10 px, atténuée, **en pied de corps**, hors des rangées. Porte la dernière valeur évaluée | VU (`83576832` : `Last Evaluation: True`, `null`) |
| **poignée de redimensionnement** | triangle strié 8 px au coin bas-droit, **uniquement sur les nœuds redimensionnables** (aperçu, commentaire, cadre) | VU (`83576832`) |

🔴 **NON TRANCHÉ — la largeur du nœud.** Aucune référence ne dit si elle est
fixe, si elle s'adapte au plus long libellé, ou si elle est redimensionnable à la
souris. Les trois existent dans le corpus. **À décider avant le codage**, parce
que ça change la sérialisation (une largeur libre doit être enregistrée).

### 2.4 Les contrôles de rangée — la liste complète

Tous sont alignés à droite, dans **une seule colonne de contrôle**. C'est ce qui
permet à l'œil de descendre le nœud en ligne droite.

| # | contrôle | apparence | niveau |
|---|---|---|---|
| 1 | **champ numérique** | rectangle `#2B2B2B`, rayon 2, texte 10,5 px aligné à droite | VU |
| 2 | **champ de texte** | idem, texte aligné à gauche, invite atténuée (`Entrer un chemin…`) | VU (principale) |
| 3 | **liste déroulante** | idem + `▾` collé à droite | VU (`83576832` : `Equal ▾`) |
| 4 | **nuancier de couleur** | rectangle plein 36 × 14, filet 1 px clair. **Damier sous la couleur si l'alpha < 1** | VU (`images (3)`, `images (4)` : rectangle cyan plein) ; damier **PROPOSÉ** — sans lui, un noir opaque et un transparent sont identiques |
| 5 | **case à cocher** | carré 12 × 12, filet 1,5 px ; coché = carré plein + `✓`. **Occupe la colonne de contrôle**, pas une colonne à part | VU (`images (3)` : `Invert ▢` ; `images (4)` : `Invert Absor ▢`) |
| 6 | **valeur en lecture seule** | **texte gris `#6A6A6A` aligné à droite, SANS cadre et SANS fond** | VU (`images (3)`, `images (4)`, `origami`) |
| 7 | **bouton à glyphe** | carré 18 × 18 `#2B2B2B`, glyphe centré | VU (principale : bouton chaîne) |
| 8 | **bouton pleine largeur** | bandeau `#2B2B2B`, texte centré | VU (`83576832` : `Run`, `Step`) |
| 9 | **champ multiligne** | zone `#2B2B2B` de hauteur libre, coloration syntaxique | VU (principale) |
| 10 | **poignée de réordonnancement** | 6 points en 2 × 3, **à GAUCHE de l'étiquette** | VU (principale) |
| 11 | **barre d'édition** | dégradé, courbe, rampe — occupe la largeur (§ 7.5) | VU (`planche_01`), pas dans le corpus |

⚠️ **6 est le micro-élément le plus facile à rater et le plus utile.** Une
**valeur affichée** et un **champ éditable** ne doivent pas se ressembler : le
champ a un fond, la valeur n'en a pas. Dans `images (4)`, `Roughness 0.001` n'est
pas éditable sur le nœud — il faut le panneau. Le dessin le dit sans un mot.

---

## 3. Les prises

### 3.1 Géométrie — **DÉCIDÉ**, et confirmée par la mesure

**Rectangle à cheval sur le bord, la moitié dehors.** Format **17 × 63** (ratio
1 : 3,7), rayon 2.

> **VU** : la principale utilise des barres verticales à coins arrondis, ~5 × 14
> px à son échelle (ratio 1 : 2,8), **exactement la même famille de forme**. Le
> choix de Rodolf est plus élancé mais du même dessin. Ce n'est pas une
> invention.

**Le fil part du milieu du bord extérieur, horizontalement** — VU sur la
principale (trois paires vérifiées).

### 3.2 Les deux familles de forme — **DÉCIDÉ**

| | forme | pourquoi |
|---|---|---|
| **valeur** | rectangle 17 × 63, rayon 2 | |
| **exécution** | même rectangle **terminé par une pointe** vers la droite | DÉCIDÉ (Rodolf a tranché contre le triangle) |

⚠️ **Il faut le dire franchement : aucune référence ne montre le rectangle à
pointe.** Le corpus propose **quatre** formes d'exécution différentes — barre
colorée identique aux données (principale), double chevron `»` (`174000`),
triangle plein `▶` (`174057`, `174227`), carré-dans-carré (`83576832`). Le
rectangle à pointe est un **ajout assumé**, et il est meilleur que les quatre
pour une raison précise : **il appartient à la même famille de forme que la
prise de valeur** (donc l'œil apprend un seul objet) **tout en s'en distinguant
par la silhouette** (donc il reste lisible en vision déficiente, où la couleur
seule échoue).

### 3.3 Creux et plein — **DÉCIDÉ**

**Creux = non branché. Plein = branché.**

> **VU trois fois, de trois façons différentes** : anneau creux → disque plein
> (`images (3)`, `images (4)`, `83576832`) ; prise sombre → prise vive
> (`origami`) ; et **Octane fait les deux à la fois** — la prise passe de creuse
> à pleine **et** l'étiquette passe du gris à l'orange.

**PROPOSÉ : on reprend les deux signaux ensemble**, comme Octane. Raison : la
prise fait 17 px de large ; à 55 % de zoom, creux et plein ne se distinguent
plus. La couleur de l'étiquette, elle, tient un palier de plus.

| état | prise | étiquette |
|---|---|---|
| non branchée | contour 2 px de la couleur du type, intérieur `#212121` | `#C8CCD4` |
| branchée | rectangle plein de la couleur du type | **`#F79A28`** |

⚠️ **Et une entrée branchée perd son champ de saisie** — DÉCIDÉ. La valeur
saisie n'a plus de sens quand un fil la remplace ; la laisser visible fait croire
qu'elle compte encore. C'est ce que fait Blender, et c'est ce que fait Octane
(`Absorption`, `Scattering` et `Medium` n'ont aucune valeur affichée).

### 3.4 Où vivent les prises — **DÉCIDÉ**

| | position |
|---|---|
| **entrée d'exécution** | **toujours unique**, sur le bord gauche de **l'en-tête** |
| **sortie d'exécution unique** | bord droit de l'en-tête, avec son libellé dans la bande |
| **sorties d'exécution multiples** | **dans le corps**, une rangée chacune, dès qu'il y en a plus d'une |
| **entrées de valeur** | bord gauche du corps, une par rangée |
| **sorties de valeur** | bord droit du corps, une par rangée |

> **VU** : `174227` met exactement ça — `▶` sur le bord gauche de l'en-tête,
> `Continue ▶` sur le bord droit de la même bande. C'est la référence la plus
> proche de notre cible blueprint.

**PROPOSÉ : la sortie de VALEUR ne se pose jamais sur l'en-tête**, contrairement
à Octane (`images (3)`, `images (4)`). Raison : l'en-tête est déjà occupé par le
`▼`, le titre, le sous-titre et le `?` ; et surtout, **si la valeur pouvait vivre
sur l'en-tête, l'en-tête cesserait d'être le lieu réservé à l'exécution** — or
c'est précisément ce qui permet de lire la ligne de vie en diagonale.

⚠️ **Une rangée peut porter une entrée à gauche ET une sortie à droite** — VU
(`174227` : `Value A [champ] … Set Var [champ] ■`). **PROPOSÉ : autorisé mais
jamais imposé** ; utile pour `Écrire variable` qui prend une valeur et la
renvoie.

### 3.5 La pastille de type — **DÉCIDÉ** (couleur **et** glyphe)

Petit rectangle 19 × 15, rayon 2, fond = couleur du type à 22 % d'opacité,
glyphe court centré en 8,5 px.

**Position — VU sur la principale** : **à gauche de l'étiquette pour une entrée**
(`123 Data`), **à droite pour une sortie** (`Result 123`). Autrement dit :
**toujours du côté de la prise.** Même règle pour la valeur affichée dans
`origami` (`8 Option` à gauche pour une sortie, `Bounciness 5` à droite pour une
entrée) — c'est une convention robuste, elle place l'information sur le chemin de
l'œil qui suit le fil.

---

## 4. Les types de donnée — couleur, glyphe, et la raison chiffrée

### 4.1 Le problème, mesuré avant d'être résolu

`CATALOGUE_NOEUDS.md` § 5quinquies pose la question : *« douze couleurs
discernables, c'est beaucoup »*. **Ce n'est pas « beaucoup », c'est impossible,
et on peut le chiffrer.**

J'ai construit un jeu de 16 couleurs (une par type), puis mesuré **toutes les
paires** en distance CIEDE2000, dans quatre conditions : vision normale,
**protanopie**, **deutéranopie**, **tritanopie** (simulation Viénot 1999).
Résultat : **plancher de 0,89** — `réel` et `couleur` sont **indiscernables** en
protanopie. Six autres paires sous 3.

Puis j'ai cherché, par sélection gloutonne max-min sur une grille de 1 080
couleurs candidates (toutes de clarté L\* entre 58 et 88, donc lisibles sur le
corps `#212121`), **le plus grand nombre de couleurs mutuellement distinctes**.
La courbe tombe vite :

| nombre de couleurs | distance minimale garantie |
|---|---|
| 2 | 53,0 |
| 3 | 13,2 |
| 4 | 12,9 |
| 5 | 11,4 |
| 6 | 10,5 |
| 7 | 9,4 |
| 8 | 9,4 |
| 9 | 8,4 |

**Le coude est à 6.** Au-delà, on paie chaque couleur supplémentaire par une paire
qu'on ne peut plus séparer.

> **Conclusion mesurée : on ne peut pas avoir plus de six ou sept couleurs de
> prise. Donc la couleur porte la FAMILLE, et le glyphe porte le TYPE EXACT.**
> Ce n'est pas un compromis esthétique, c'est ce que la mesure autorise.

Un second essai le confirme par la négative : donner à chaque membre d'une
famille sa propre clarté (vec2 clair → transformation sombre) fait **rechuter le
plancher inter-familles à 2,2** — le vert sombre de `matrice` rejoint l'orange
d'`exécution` en deutéranopie. **Une famille = une seule couleur, plate.**

### 4.2 La palette — **PROPOSÉ**, plancher mesuré à **11,0**

| famille | couleur | ce qu'elle regroupe | raison |
|---|---|---|---|
| **exécution** | **`#F79A28`** | l'ordre d'exécution | DÉCIDÉ — orange Rihen, et déjà la couleur du filet d'exécution : le même signal partout |
| **nombre** | **`#17B2EB`** | réel, entier, booléen | ce sont les trois types **inter-convertibles** : un booléen vaut 0 ou 1, un entier est un réel. Même famille de conversion = même couleur |
| **géométrie** | **`#C0EB81`** | vec2, vec3, vec4, matrice, transformation | tout ce qui a des **composantes** et vit dans l'espace |
| **texte** | **`#F2559B`** | chaîne | seul type dont on ne fait aucune arithmétique — il mérite d'être seul |
| **apparence** | **`#D9B6A3`** | couleur, texture, matériau, shader | tout ce qui **décrit une surface**. Terre cuite : c'est la matière |
| **référence** | **`#81EBEB`** | objet, entité, actif | ce qui **désigne** au lieu de contenir |
| **quelconque** | **`#9AA3AD`** | générique, résolu au branchement | gris neutre : **l'absence de type doit ressembler à l'absence de couleur** |

**Validation, tous modes de vision confondus :**

- **plancher = 11,0** (`texte` / `quelconque`, en protanopie) ;
- paire suivante 12,1, puis 13,3 — la distribution est régulière, il n'y a pas
  une paire limite isolée ;
- **contraste minimal sur le corps `#212121` = 5,0 : 1** (`texte`), tous les
  autres au-dessus de 6,3 : 1. Une prise de 17 px de large reste visible.
- `exécution` contre `géométrie` tombe à 11,4 en deutéranopie — **mais ces deux-là
  ne se confondent jamais, parce que leur prise n'a pas la même forme.** La
  couleur est un signal redondant pour ce couple, pas le seul.

⚠️ **Le rouge d'erreur** (`#E4443C`, § 11.1) est à **7,1** de `texte` en
tritanopie. C'est faible — mais l'erreur n'apparaît **jamais sur une prise** :
elle vit sur le filet et l'en-tête, à une tout autre échelle, avec un `!`. **À
surveiller si l'erreur descendait un jour au niveau de la prise.**

### 4.3 Les types atomiques

| type | famille / couleur | glyphe | ce qu'il montre non branché |
|---|---|---|---|
| **réel** | nombre `#17B2EB` | `1.0` | champ numérique, 3 décimales |
| **entier** | nombre `#17B2EB` | `12` | champ numérique, sans décimale |
| **booléen** | nombre `#17B2EB` | `V/F` | **case à cocher** (contrôle 5), jamais un champ |
| **texte** | texte `#F2559B` | `abc` | champ de texte, invite atténuée |

**PROPOSÉ — pourquoi `1.0` et `12` et pas `f` et `i`** : le glyphe montre **la
forme de la donnée**, pas son nom savant. Un artiste lit `1.0` sans avoir appris
ce qu'est un flottant. Même raison pour `V/F` plutôt que `bool`.

### 4.4 Les types composés

| type | famille / couleur | glyphe | ce qu'il montre non branché |
|---|---|---|---|
| **vec2** | géométrie `#C0EB81` | `XY` | 2 champs sur une ligne (plié) |
| **vec3** | géométrie `#C0EB81` | `XYZ` | 3 champs sur une ligne (plié) |
| **vec4** | géométrie `#C0EB81` | `XYZW` | 4 champs (plié) |
| **couleur** | apparence `#D9B6A3` | `RVB` | nuancier + damier si alpha |
| **matrice** | géométrie `#C0EB81` | `M4` | **rien** — pas de champ (§ 4.5) |
| **transformation** | géométrie `#C0EB81` | `TRS` | 3 lignes pliées : position, rotation, échelle |
| **tableau** | *couleur du contenu* | `[ ]` | § 5 |
| **dictionnaire** | *couleur du contenu* | `{ }` | § 5 |
| **référence d'objet** | référence `#81EBEB` | `OBJ` | nom de l'objet, ou `— aucun —` |
| **texture** | apparence `#D9B6A3` | `TEX` | § 6 |
| **matériau** | apparence `#D9B6A3` | `MAT` | § 6 |
| **shader** | apparence `#D9B6A3` | `SH` | **rien** — un shader ne se saisit pas |
| **quelconque** | quelconque `#9AA3AD` | `?` | **rien** tant qu'il n'est pas résolu |

⚠️ **`couleur` n'est PAS dans la famille géométrie, alors qu'un `vec3` et une
couleur RVB ont le même contenu.** C'est délibéré, et c'est déjà écrit dans
`CATALOGUE_NOEUDS.md` § 2 : *« une couleur a un espace colorimétrique, pas un
vecteur »*. Les brancher l'un sur l'autre sans le dire est la source d'erreur la
plus classique d'un graphe de matériau — un vecteur normalisé interprété comme du
sRGB. **Deux familles de couleur différentes rendent la faute visible avant le
rendu.**

### 4.5 Les types qui n'affichent rien — **PROPOSÉ**

`matrice`, `shader`, `quelconque` non résolu : **rangée avec sa prise et son
étiquette, colonne de contrôle vide.**

Raison : une matrice 4 × 4 saisie à la main sur un nœud, ce sont **seize champs**
— le nœud devient plus haut que l'écran pour une donnée que personne n'écrit à
la main. **PROPOSÉ : la matrice se produit (par un nœud `Composer transformation`)
ou se branche, elle ne se saisit pas.** Si Rodolf veut la saisie, elle va dans le
panneau latéral, jamais sur le nœud.

### 4.6 Plier / déplier une donnée composée — **DÉCIDÉ** (rappel)

`CATALOGUE_NOEUDS.md` § 5ter distingue **plier/déplier** (affichage, une seule
prise, aucun lien nouveau) de **séparer/recombiner** (structure, trois prises,
trois liens possibles). **La règle qui les sépare : est-ce que le nombre de liens
possibles change ?**

Ce que le dessin doit rendre visible, et qui n'est nulle part encore :

- **plié** → `Position  [0.0] [1.0] [0.0]` — **une** prise en face de la rangée ;
- **déplié** → `Position` puis trois sous-rangées `X` `Y` `Z` **indentées de
  12 px, sans prise propre**, et **toujours une seule prise**, centrée en face du
  libellé `Position` ;
- **séparé** → trois rangées de plein rang `Position X`, `Position Y`,
  `Position Z`, **chacune avec sa prise**, et **plus aucune prise sur
  `Position`**.

⚠️ **L'indentation sans prise est le signal.** C'est elle qui dit « je regarde le
contenu » plutôt que « j'ai trois branchements ». Sans elle, déplié et séparé sont
indistinguables — et l'utilisateur croit pouvoir brancher `Y` seul.

🔴 **NON TRANCHÉ, et déjà signalé au catalogue** : que devient un lien branché sur
`Position` quand on sépare ? Unreal le **casse**. L'alternative (le router vers
les trois) rend la recombinaison ambiguë. **C'est un arbitrage de modèle, pas de
dessin** — mais le dessin doit savoir lequel montrer.

---

## 5. Les tableaux et les dictionnaires — en détail

C'est la partie que Rodolf a nommée en premier, et c'est la moins couverte par le
corpus. **Une seule image du corpus montre quelque chose qui y ressemble** :
`origami_patcher.webp`, où `Option Picker` empile `174`, `120`, `58` sous sa
première rangée, sans étiquette, en gris atténué. Elle ne montre **ni la
longueur, ni le type contenu, ni le repli**. Tout le reste de cette section est
**PROPOSÉ**.

### 5.1 Le principe : un tableau n'est pas un type, c'est une FORME

**PROPOSÉ, et c'est la décision structurante de cette section.**

Un tableau de réels reste bleu `#17B2EB`. Un tableau de couleurs reste terre
cuite `#D9B6A3`. **La couleur dit toujours ce qu'il y a dedans ; c'est la forme
de la prise qui dit combien.**

Trois raisons, dans l'ordre de force :

1. **La mesure de § 4.1 l'impose.** Six couleurs disponibles ; en donner une au
   « tableau » et une au « dictionnaire » coûterait deux familles de type, et on
   perdrait quand même l'information « tableau **de quoi** ».
2. `CATALOGUE_NOEUDS.md` § 5bis l'exige explicitement : *« il faut pouvoir
   montrer le type contenu »*. Une couleur propre au tableau le rend impossible.
3. C'est **composable** : tableau de tableaux, dictionnaire de couleurs, tableau
   de références — la forme se compose, la couleur non.

### 5.2 L'apparence de la prise

| | dessin de la prise 17 × 63 | glyphe de pastille |
|---|---|---|
| **scalaire** | rectangle plein d'un seul tenant | `1.0` |
| **tableau** | **trois segments empilés**, séparés par deux fentes de 3 px au fond du corps | `[1.0]` |
| **dictionnaire** | **grille 2 × 3** : les trois segments, plus une fente verticale au milieu (la clé à gauche, la valeur à droite) | `{1.0}` |

Le glyphe **encadre celui du contenu** : `[1.0]` se lit « tableau de réels »,
`{abc}` « dictionnaire de textes », `[[1.0]]` « tableau de tableaux de réels ».
On lit la structure sans la nommer.

⚠️ **Ce que ça donne au dézoom, et c'est le point faible à assumer** : à 55 %,
les fentes de 3 px disparaissent — **un tableau redevient visuellement un
scalaire**. Le fil, lui, tient un palier de plus (§ 5.3). **PROPOSÉ : c'est
acceptable**, parce qu'à 55 % on ne branche plus, on lit la structure ; et parce
que la seule alternative — une couleur propre — coûte plus cher (§ 5.1).

### 5.3 L'apparence du fil

| | trait |
|---|---|
| **valeur** | trait simple, 2 px, couleur du type |
| **tableau** | **trait double** — deux traits de 1,4 px espacés de 2,5 px |
| **dictionnaire** | **trait double dont le trait inférieur est pointillé** (3-3) |
| **exécution** | trait simple, **3,5 px**, toujours orange, jamais coloré par un type |

Raison du double trait plutôt qu'un trait plus épais : **l'épaisseur est déjà
prise** par l'exécution. Deux traits parallèles restent lisibles à 55 % là où une
différence d'épaisseur ne l'est plus, et ils disent visuellement « plusieurs ».

Raison du pointillé pour le dictionnaire : il faut un troisième état sur le même
axe, et **le pointillé est déjà, ailleurs, le signe de « en cours / incomplet »**
(le fil en cours de tirage). ⚠️ **Collision assumée** — mais le fil de tirage est
gris et suit le curseur ; il ne se confond pas avec un fil posé et coloré. **Si
Rodolf juge la collision trop risquée, la solution de repli est un troisième
trait plutôt qu'un pointillé** ; je ne l'ai pas prise parce que trois traits de
1,4 px font 6 px de large et se confondent avec un fil d'exécution.

### 5.4 Le nœud qui PRODUIT un tableau

**PROPOSÉ — forme n° 9 du § 7.** Un nœud de calcul dont le corps est une **liste
de rangées à charge variable** :

```
┌──────────────────────────────┐
│ ▼ Construire tableau      ?  │
├══════════════════════════════┤  filet pétrole
│ ▌ 1.0  0                     │   ← élément 0, prise + valeur
│ ▌ 1.0  1                     │
│ ▌ 1.0  2                     │
│        + ajouter un élément  │
│  3 éléments                  │   ← ligne d'état
│              tableau  [1.0] ▐│
└──────────────────────────────┘
```

- **chaque élément est une vraie prise** — il peut être branché individuellement.
  Même choix que le `ColorRamp` de `planche_01`, où chaque arrêt est une prise ;
- la rangée d'ajout et le `−` par ligne pilotent la longueur ;
- ⚠️ **repli obligatoire au-delà de 8 éléments** : les rangées 9 et suivantes sont
  remplacées par une **rangée `… et 492 autres`**, dépliable. `CATALOGUE_NOEUDS.md`
  § 5bis pose l'exigence (*« un tableau de 500 entrées détruit le canevas »*) ;
  **8 est PROPOSÉ**, pour que le nœud reste plus court que le `Principled` déplié.

**Le dictionnaire** a le même nœud avec **deux colonnes** : `clé` (champ de texte)
et `valeur` (prise + contrôle). **La clé n'a jamais de prise** — PROPOSÉ, raison :
une clé branchée rendrait la structure du dictionnaire imprévisible à
l'édition, donc non compilable pour un matériau.

### 5.5 Le nœud qui PARCOURT un tableau

**PROPOSÉ — forme n° 10.** C'est un nœud d'exécution, et **sa silhouette doit
être reconnaissable entre toutes**, parce qu'il est le seul à avoir **deux
sorties d'exécution de nature différente** :

```
┌────────────────────────────────────┐
│ ▶ ▼ Pour chaque                 ?  │  ← entrée d'exéc. sur l'en-tête
│      Boucle                        │
├════════════════════════════════════┤  filet ORANGE
│ ▌[1.0] éléments                    │
│                    corps de boucle ▶│  ← exéc., une fois PAR élément
│                         élément 1.0▐│  ← la valeur courante
│                          indice  12▐│
│                      terminé       ▶│  ← exéc., une seule fois, à la fin
│  0 / 3                             │  ← ligne d'état pendant l'exécution
└────────────────────────────────────┘
```

⚠️ **`corps de boucle` et `terminé` doivent se distinguer sans lire l'étiquette**
— c'est la faute la plus commune du débutant en blueprint. **PROPOSÉ : une ligne
vide de 8 px entre les deux**, plus le fait que `terminé` est la dernière rangée.
Ce n'est pas énorme ; **je n'ai pas mieux, et je préfère le dire.**
🔴 **NON TRANCHÉ** — si Rodolf veut un signal plus fort (une teinte de rangée, un
glyphe de boucle sur la prise), c'est son appel.

### 5.6 🔴 Ce qui se passe quand on branche un tableau sur une entrée scalaire

**La question la plus importante de cette section, et le corpus n'en dit rien.**
Trois politiques existent dans l'industrie, et elles ne se ressemblent pas :

| politique | qui la pratique | conséquence |
|---|---|---|
| **A — refus sec** | Unreal | la prise s'éteint pendant le tirage, le lien ne se fait pas |
| **B — adaptation implicite** | Houdini, Blender (geometry nodes) | le nœud s'exécute N fois, une par élément. Puissant, et **invisible** |
| **C — refus utile** | — | on refuse, **et on propose le nœud qui répare** |

**PROPOSÉ : C.** Voici ce que ça donne, précisément :

1. pendant le tirage, la prise scalaire s'affiche **incompatible** (éteinte, 30 %
   d'opacité, § 11.4) ;
2. **si l'utilisateur relâche quand même dessus**, on n'annule pas le geste : on
   ouvre la **recherche de nœud filtrée** (§ 12.1) sur les seuls nœuds qui vont
   de `[T]` vers `T` — `Élément à l'indice`, `Premier`, `Réduire`, `Pour chaque` ;
3. le nœud choisi **s'insère entre les deux** et les deux liens se font.

Raison du refus plutôt que de l'adaptation : **le graphe est partagé entre le
matériau et le blueprint** (`CATALOGUE_NOEUDS.md` § 7). L'adaptation implicite
est défendable dans un graphe qui s'évalue ; elle ne l'est pas dans un graphe
**qui se compile vers NkSL** — une boucle implicite de longueur inconnue à la
compilation n'a pas de traduction en shader. **Une même règle doit valoir dans les
deux mondes**, donc c'est la plus stricte qui gagne.

Raison du « refus **utile** » plutôt que du refus sec : le refus sec laisse
l'utilisateur devant un lien qui ne se fait pas, sans lui dire pourquoi ni quoi
faire. **Le moment où l'éditeur en sait le plus sur l'intention est exactement
celui où on relâche le bouton** — le gâcher est un luxe.

### 5.7 Les autres nœuds de tableau — inventaire

Nommés dans `CATALOGUE_NOEUDS.md` § 5bis, ils suivent tous des formes déjà
définies, aucun ne demande un dessin propre :

| nœud | forme | filet |
|---|---|---|
| `Longueur` | transformateur (§ 7.2) | pétrole |
| `Élément à l'indice` | transformateur | pétrole |
| `Ajouter` / `Retirer` | exécution (§ 7.7) | orange |
| `Filtrer` / `Trier` | transformateur à charge variable | pétrole |
| `Clés` / `Valeurs` (dictionnaire) | séparateur (§ 7.4) | pétrole |
| `Contient la clé` | transformateur → booléen | pétrole |

---

## 6. Textures et matériaux

### 6.1 Ce que la référence montre — **VU**, et c'est décisif

`images (4).jpg` est **la seule image du corpus qui montre le même graphe dans
deux états**, légendés `NODE PREVIEW ON` et `NODE PREVIEW OFF`. Relevé :

- **ON** : le nœud `OctSpecular1` porte, **entre l'en-tête et la première
  rangée**, une **vignette carrée** en pleine largeur moins une marge, montrant
  une sphère rendue **sur un damier** ;
- **OFF** : la vignette disparaît **entièrement**, les rangées remontent contre
  l'en-tête, le nœud **raccourcit d'autant**. Largeur, couleurs, ordre des
  rangées : identiques ;
- `images (3)` montre la même chose sur une **texture** : `ImageTexture` porte la
  vignette de l'image chargée, plus haute que large.

**Conclusions VU, sans interprétation** : l'aperçu est un **bloc de corps
escamotable en pleine largeur**, pas une décoration d'en-tête, pas une infobulle.
Et dans cette image, il se **bascule globalement** (tous les nœuds ensemble),
pas nœud par nœud.

⚠️ **Ce que la référence ne montre PAS** : ni la taille, ni le chemin, ni
l'espace colorimétrique n'apparaissent nulle part sur le nœud. Rodolf les demande
explicitement. **Tout le § 6.2 est donc PROPOSÉ.**

### 6.2 Le nœud de TEXTURE — **PROPOSÉ**

```
┌────────────────────────────────────────┐
│ ▼ Texture image                     ?  │
│    Texture                             │
├════════════════════════════════════════┤  filet PÉTROLE (jamais orange)
│  ┌──────────────────────────────────┐  │
│  │                                  │  │  ← aperçu, 96 px de haut,
│  │        (l'image, recadrée)       │  │    damier sous l'alpha
│  │                                  │  │
│  └──────────────────────────────────┘  │
│  murs/beton_albedo.png             ⋯   │  ← chemin, ÉLIDÉ PAR LA GAUCHE
│  2048 × 2048 · RVBA8 · sRVB            │  ← carte d'identité, une ligne
│ ▌ XY  Coordonnées        branchée      │
│ ▌ 1.0 Gamma               [ 2.200 ]    │
│                    Couleur   RVB ▐     │
│                    Alpha     1.0 ▐     │
└────────────────────────────────────────┘
```

| élément | spécification | raison |
|---|---|---|
| **aperçu** | 96 px de haut, largeur du corps − 16, **image recadrée au centre** (jamais déformée), **damier 8 px sous l'alpha** | VU pour le principe ; le damier parce que sans lui un PNG noir opaque et un PNG transparent sont le même rectangle noir |
| **chemin** | 10 px, `#8A8A8A`, **élidé par la GAUCHE** (`⋯beton_albedo.png`) | l'information utile d'un chemin est **à la fin**. Élider par la droite cache le nom du fichier — c'est l'erreur la plus courante |
| **carte d'identité** | une seule ligne, 9,5 px, `#6A6A6A` : `taille · format · espace colorimétrique` | **trois faits sur une ligne**, parce qu'ils se lisent ensemble ou pas du tout |
| **espace colorimétrique** | `sRVB` ou `linéaire`, **en toutes lettres** | ⚠️ c'est le seul champ qui **change silencieusement le rendu**. Une normal map en sRVB donne un éclairage faux sans aucune erreur. Il doit être lisible **sans clic** |
| **filet** | **pétrole, toujours** | § 6.4 |

⚠️ **PROPOSÉ : quand l'espace colorimétrique est incohérent avec l'usage** (une
texture sRVB branchée sur une entrée `Normale`), **la ligne d'identité passe en
`#E4443C` et le nœud prend l'état « avertissement » (§ 11.2)** — pas l'état
erreur : le graphe reste valide, il est seulement probablement faux. C'est le
seul cas du document où le dessin porte un jugement métier ; il le mérite,
parce que c'est le bug le plus cher et le plus silencieux d'un graphe de
matériau.

### 6.3 Le nœud de MATÉRIAU — **PROPOSÉ**

Même bloc d'aperçu, **mais une sphère rendue avec le matériau, sur damier** — VU
sur `images (4)`.

Différences avec la texture :

- **l'aperçu est un rendu, donc il a un coût.** PROPOSÉ : il se recalcule **à la
  fin d'un geste**, jamais pendant qu'on tire un curseur, et il affiche un
  **voile de 30 % + un point qui pulse au coin haut-droit** tant qu'il n'est pas
  à jour. Sans cet état, un aperçu périmé ment.
- **pas de chemin, pas de taille** : un matériau n'est pas un fichier au sens où
  la texture l'est. **PROPOSÉ : à la place, `4 textures · 12 nœuds`** — ce qu'il
  coûte, qui est l'information qu'on cherche vraiment sur un matériau.
- ⚠️ **la sortie `Surface` porte le type `shader`** (`SH`, terre cuite), **et
  rien d'autre ne s'y branche** — `CATALOGUE_NOEUDS.md` § 5quater.6 demandait
  « une forme de prise qui n'appartienne qu'à elle ». **PROPOSÉ : ce n'est PAS
  une forme propre, c'est la forme normale avec la couleur apparence et le
  glyphe `SH`.** Raison : une septième forme de prise pour un seul type est un
  vocabulaire cher pour un cas que le refus de branchement gère déjà (la prise
  s'éteint pendant le tirage, § 11.4). Si Rodolf veut la forme propre, c'est
  facile à ajouter — mais je ne la propose pas.

### 6.4 🔴 La règle absolue — **DÉCIDÉ**

**Un graphe de matériau n'a JAMAIS de fil d'exécution. Jamais.**

Conséquences pour le dessin, toutes obligatoires :

1. **tout nœud de matériau porte le filet pétrole** — un filet orange dans un
   graphe de matériau est un bug, pas un cas ;
2. **aucune prise à pointe** n'apparaît dans un éditeur de matériau ;
3. la bibliothèque de nœuds **ne propose pas** les familles à exécution quand le
   graphe est un matériau — elles ne sont pas grisées, **elles sont absentes** ;
4. l'entrée d'exécution étant sur l'en-tête, **l'en-tête d'un nœud de matériau
   n'a jamais de prise à gauche** : son bord gauche d'en-tête est net. C'est
   gratuit, et ça se voit à 25 % de zoom.

### 6.5 L'aperçu au dézoom — **PROPOSÉ**

Rodolf pose la question explicitement. Réponse par palier (§ 12.2) :

| palier | ce que devient l'aperçu | raison |
|---|---|---|
| **100 %** | vignette complète, 96 px | |
| **55 %** | **vignette conservée, réduite à 48 px**, tout le reste du corps s'efface | ⚠️ **c'est le seul élément du nœud qui GAGNE en importance relative quand on dézoome** : à 55 % on ne lit plus « Texture image », mais on reconnaît une image de brique en un coup d'œil. L'aperçu **est** l'étiquette, une fois le texte perdu |
| **25 %** | **la vignette devient la couleur moyenne de l'image**, en aplat sur tout le nœud | une image de 48 px réduite à 12 px est du bruit. Sa moyenne, non : une planche de bois reste brune, un ciel reste bleu, et **la lecture d'un graphe de matériau dézoômé se fait exactement là-dessus** |

C'est la seule exception à la règle « à 25 %, le nœud est un rectangle de la
couleur de sa catégorie » — et elle se justifie : pour une texture, **la couleur
moyenne est plus informative que la catégorie**, qu'on connaît déjà par la forme.

---

## 7. Toutes les formes de nœud

Les huit du catalogue (§ 5quater), plus les six que Rodolf demande. **La forme
suit la connectivité, jamais le nom** — c'est pour ça qu'il y en a quatorze et
pas cinquante-quatre.

| # | forme | côté gauche | côté droit | filet | exemple |
|---|---|---|---|---|---|
| 1 | **source** | **vide** | 1..n valeurs | pétrole | `RGB`, `Valeur`, `Coordonnées de texture` |
| 2 | **transformateur** | n valeurs | 1 valeur | pétrole | `Maths`, `Mélanger`, `Borner` |
| 3 | **puits** | n valeurs | **vide** | pétrole | `Sortie de matériau` |
| 4 | **séparateur** | 1..2 valeurs | n valeurs | pétrole | `Séparer XYZ`, `Texture image` |
| 5 | **charge variable** | n valeurs | 1 valeur | pétrole | `ColorRamp`, `Courbe` |
| 6 | **surface** | ~20 valeurs, en sections | 1 shader | pétrole | `Principled` |
| 7 | **exécution** | exéc. + valeurs | exéc. + valeurs | **orange** | `Si / Sinon`, `Écrire` |
| 8 | **événement** | **aucune entrée d'exéc.** | 1 exéc. + valeurs | **orange** | `Au démarrage`, `À chaque image` |
| 9 | **producteur de tableau** | n éléments | 1 tableau | pétrole | `Construire tableau` (§ 5.4) |
| 10 | **itérateur** | exéc. + tableau | **2 exéc.** + valeurs | **orange** | `Pour chaque` (§ 5.5) |
| 11 | **aperçu** | 1 valeur | 0..1 valeur | pétrole | `Texture image`, `Matériau` (§ 6) |
| 12 | **groupe** | ses entrées exposées | ses sorties exposées | selon son contenu | § 7.6 |
| 13 | **custom** | selon la déclaration | selon la déclaration | selon | § 8 |
| 14 | **inerte** | — | — | — | commentaire, cadre, relais (§ 9) |

### 7.1 La source — le côté gauche vide

**Ce qui la fait reconnaître de loin, c'est le vide à gauche**, pas son contenu.
⚠️ **PROPOSÉ : ne jamais dessiner de marge gauche « au cas où ».** Le corps
commence au ras du texte. Un nœud source doit avoir **un bord gauche parfaitement
net** — c'est un signal gratuit qu'on perdrait par simple symétrie de mise en page.

### 7.2 Le transformateur — la liste d'opération

**VU** (`174227` : `Multiply / Math` en en-tête + sous-titre). Deux emplacements
possibles pour l'opération :

- **dans le sous-titre de l'en-tête** — le titre devient `Multiplier`, le
  sous-titre `Maths` ;
- **en première rangée**, dans une liste déroulante pleine largeur.

**PROPOSÉ : le sous-titre**, et voici pourquoi c'est le meilleur des deux.
⚠️ `CATALOGUE_NOEUDS.md` § 5quater.2 note que **changer l'opération change le
nombre d'entrées** (une addition en prend 2, un `borner` en prend 3). Si
l'opération est en première rangée, **le nœud change de hauteur sous le curseur
au moment même où on clique** — la rangée qu'on visait s'est déplacée. Dans
l'en-tête, la liste ne bouge jamais.

### 7.3 Le puits — plus imposant, et unique

**PROPOSÉ** : `Sortie de matériau` prend l'en-tête **orange Rihen `#F79A28`**
(le seul de tout l'éditeur) et **un corps 20 % plus large** que les autres.
Raison : il est unique par graphe et il en est la fin ; le rendre trouvable au
premier coup d'œil dans `images (2)` — trente nœuds — vaut cette exception.

⚠️ **PROPOSÉ : un second nœud de sortie dans le même graphe est une ERREUR**
(§ 11.1), pas un avertissement. Deux fins possibles, c'est un graphe qui ne
compile pas.

### 7.4 Le séparateur — deux sorties qu'on ne doit pas confondre

`CATALOGUE_NOEUDS.md` § 5quater.4 pose le problème : `Texture image` sort
**couleur ET alpha**, et on branche l'alpha en croyant brancher la couleur.

**PROPOSÉ** : les deux sorties sont **de familles de couleur différentes** —
`Couleur` en apparence `#D9B6A3` (glyphe `RVB`), `Alpha` en nombre `#17B2EB`
(glyphe `1.0`). **Le problème se résout tout seul par le typage**, sans règle
spéciale : elles ne se ressemblent déjà pas. C'est le meilleur genre de solution
— celle qui n'ajoute rien.

### 7.5 Le nœud à charge variable — l'éditeur EST le nœud

`ColorRamp`, `Courbe`. **PROPOSÉ**, et le corpus n'en montre aucun.

- **la barre de dégradé occupe la largeur** du corps moins 16 px, hauteur 20 px ;
- **chaque arrêt est une prise** (déjà dans `planche_01`) : sa position **et** sa
  couleur sont pilotables ;
- **au dézoom 55 %, les poignées d'arrêt disparaissent, la barre reste** — elle
  devient un aperçu. Même logique que § 6.5 : **l'éditeur graphique dégrade en
  aperçu, il ne disparaît pas.** C'est ce qui distingue un nœud à charge variable
  d'un nœud ordinaire quand le texte est perdu ;
- **à 25 %, la barre devient la couleur moyenne du dégradé.**

### 7.6 Le nœud de GROUPE — 🔴 le plus mal couvert

`ELEMENTS_A_DESSINER.md` B8 le marque déjà ⚠️. **Aucune référence du corpus ne
montre un sous-graphe replié.** Le seul indice est le bouton `Project` en haut à
gauche de `images (5)`, qui est un **fil d'Ariane** — donc le concept existe chez
eux, mais l'image ne montre pas le nœud.

Ce que je peux proposer avec une raison, et ce que je ne peux pas :

**PROPOSÉ :**
- l'en-tête porte **un pictogramme de pile** (deux rectangles décalés) à gauche
  du titre — c'est le seul élément qui dit « il y a quelque chose dedans » ;
- **double-clic pour entrer**, et le fil d'Ariane apparaît en haut du canevas
  (`Matériau mur ▸ Bruit de surface`) — VU comme mécanisme dans `images (5)` ;
- **les entrées et sorties du groupe sont celles qu'on a exposées**, et elles
  portent leur **nom public** ;
- le filet est **orange si le groupe contient au moins un nœud qui exécute**,
  pétrole sinon. Raison : sinon le repli **cache la ligne de vie**, ce qui est
  exactement ce qu'un repli ne doit jamais faire.

🔴 **NON TRANCHÉ :**
- **entre-t-on dans le groupe sur place, ou dans un onglet ?** Les deux existent
  et ce n'est pas un choix de dessin — c'est un choix de fenêtrage NKEditorKit ;
- **le groupe montre-t-il un aperçu de son contenu** (une miniature du
  sous-graphe) ? Séduisant, et je n'ai **aucune référence** ni aucune mesure pour
  dire si c'est lisible à 200 × 120 px.

### 7.7 Le nœud d'exécution et l'événement

**VU** (`174227`, `174000`, `174057`) et **DÉCIDÉ** pour les positions (§ 3.4).

Ce qui distingue l'**événement** du nœud d'exécution ordinaire, et qui doit se
voir sans lire : **il n'a aucune entrée d'exécution**, donc **le bord gauche de
son en-tête est net**. C'est le symétrique exact du nœud source (§ 7.1), sur
l'autre rangée. **Un événement, c'est un nœud source de temps.**

**PROPOSÉ** : et parce que c'est *un début*, l'événement porte en plus **le coin
haut-gauche de son en-tête arrondi à 12 px au lieu de 5** — une seule courbe, qui
dit « ça commence ici ». C'est le seul écart de géométrie que je propose dans tout
le document, et il ne coûte rien.

⚠️ `Si / Sinon` a **deux sorties d'exécution**. `CATALOGUE_NOEUDS.md` § 5quater.7
exige qu'elles se distinguent **par l'étiquette ET la position, jamais par la
seule couleur** — elles sont toutes les deux orange. **Elles descendent donc dans
le corps** (§ 3.4), `Vrai` puis `Faux`, dans cet ordre, toujours.

---

## 8. Les nœuds CUSTOM — et surtout : quand l'éditeur ne sait rien

C'est la demande de Rodolf la plus tournée vers l'avenir, et le corpus la couvre
mieux qu'on ne pourrait l'espérer : **`node-based-…webp` contient un `Example
node`** — trois entrées `input1..3`, deux sorties `output1..2`, aucun contenu,
aucune valeur, aucun type. **C'est exactement le squelette nu d'un nœud dont
l'éditeur ne sait rien.**

### 8.1 Les trois niveaux de connaissance — **PROPOSÉ**

L'éditeur ne connaît pas « les nœuds custom » en bloc. Il en connaît **trois
degrés**, et le dessin doit les distinguer, parce qu'ils n'engagent pas la même
confiance.

| niveau | ce que l'éditeur sait | ce qu'il dessine |
|---|---|---|
| **A — déclaré** | nom, catégorie, prises typées, valeurs par défaut, aide | **un nœud normal.** Rien ne le distingue d'un nœud livré, et c'est le but |
| **B — déclaré partiellement** | les prises et leurs noms, **pas les types** | prises en **`quelconque` gris `#9AA3AD`**, glyphe `?`, aucun champ de saisie |
| **C — inconnu** | rien qu'un nom de type lu dans le fichier | § 8.3 |

**Raison de séparer B et C** : B est un nœud qui **marchera** — il lui manque du
confort. C est un nœud **qui ne peut pas s'exécuter**. Les confondre, c'est soit
alarmer sur B, soit rassurer sur C.

### 8.2 Ce qu'une déclaration doit fournir — **PROPOSÉ**

Pour atteindre le niveau A, l'auteur d'un nœud déclare, dans cet ordre
d'importance :

1. **le nom affiché et la catégorie** → titre, sous-titre, couleur d'en-tête ;
2. **s'il exécute** → couleur du filet. ⚠️ **Non déductible** : un nœud sans
   prise d'exécution *déclarée* pourrait quand même avoir des effets de bord.
   **C'est une déclaration, pas une inférence** ;
3. **ses prises** : nom, type, sens, **et pour une entrée, sa valeur par défaut**
   — sans elle, la rangée non branchée n'a rien à afficher ;
4. **son texte d'aide** → le `?` de l'en-tête. Sans lui, **PROPOSÉ : le `?` n'est
   pas dessiné du tout**, plutôt qu'un `?` qui ouvre le vide ;
5. **facultatif** : pictogramme, largeur préférée, sections de repli.

⚠️ **Ce que l'auteur ne choisit JAMAIS** : la couleur des prises (elle vient du
type), la géométrie, les rayons, l'aspect des états. **Un nœud custom qui pourrait
choisir ses couleurs détruirait le code couleur pour tout le monde** — un seul
auteur suffit. C'est le point à écrire dans l'API avant qu'elle n'existe, pas
après.

### 8.3 🔴 Le nœud INCONNU — ce que l'éditeur affiche quand il ne sait rien

Le cas arrive **tout le temps** : on ouvre un `.nkgraph` produit avec un module
absent, une version antérieure, un greffon désinstallé.

**PROPOSÉ** :

```
┌────────────────────────────────────┐
│ ▼ studio.fx.Tourbillon             │  ← en-tête GRIS RAYÉ à 45°
├════════════════════════════════════┤  ← filet gris, ni orange ni pétrole
│ ▌ ?  entree_0        (contour tireté)│
│ ▌ ?  entree_1                       │
│                       sortie_0  ? ▐ │
│  type inconnu — les liens sont      │
│  conservés                          │
└────────────────────────────────────┘
```

| choix | raison |
|---|---|
| **en-tête gris rayé à 45°** | il ne peut pas emprunter la couleur d'une catégorie qu'on ne connaît pas, et un gris uni le ferait passer pour un nœud de calcul |
| **filet gris** — ni orange ni pétrole | ⚠️ **on ne sait pas s'il exécute.** Lui donner du pétrole serait une affirmation fausse. **Le filet doit avoir un troisième état « je ne sais pas »** — c'est une conséquence de ce nœud, et personne ne l'avait vue |
| **prises en contour tireté** | ni creux (« non branché ») ni plein (« branché ») : **elles peuvent être branchées et on ne sait pas si le type est bon** |
| **les noms de prise viennent du FICHIER** | c'est tout ce qu'on a, et c'est déjà beaucoup |
| **les liens sont conservés et redessinés** | 🔴 **la règle la plus importante de ce paragraphe.** Un éditeur qui laisse tomber les liens d'un nœud inconnu **détruit le travail à la simple ouverture du fichier**, et le désastre est silencieux : on sauvegarde, et c'est perdu. Le dessin doit rendre évident que **rien n'a été jeté** |
| **ligne d'état explicite** | l'utilisateur doit savoir que c'est récupérable, pas cassé |

⚠️ **Un nœud inconnu n'est PAS un nœud en erreur.** Le rouge dit « ce graphe est
faux » ; ici le graphe est probablement juste, c'est **l'éditeur** qui est
incomplet. Utiliser le rouge d'erreur pousserait l'utilisateur à supprimer le
nœud — exactement le geste qui perd le travail.

### 8.4 Le nœud INDISPONIBLE — le rang interdit

`CATALOGUE_NOEUDS.md` § 3.3 : `Light Path`, `Raycast`, `Ambient Occlusion`,
`Curves Info`, `Particle Info` supposent un tracé de rayons ; notre moteur
rastérise. `ELEMENTS_A_DESSINER.md` D6 le laisse ⚠️.

**PROPOSÉ — et il faut le distinguer de tout le reste** :

- **dans la bibliothèque** : présent, en **50 % d'opacité**, avec sa raison en
  seconde ligne (`nécessite un tracé de rayons`). **Présent, parce qu'un nœud
  absent se cherche indéfiniment ; un nœud refusé se comprend une fois.**
- **posé sur le canevas** (cas d'un fichier importé) : **corps hachuré à 45° en
  `#3A3A44`**, en-tête à 50 %, **prises intactes et fils conservés**.
- **la raison est écrite dans le nœud**, pas dans une infobulle : `ce moteur
  rastérise`.

⚠️ **Trois états gris à ne jamais confondre**, et c'est le piège de cette
section :

| état | ce qu'il dit | signal |
|---|---|---|
| **désactivé** (§ 11.3) | *l'utilisateur* l'a éteint | corps à 45 %, **fil traversant en pointillé** |
| **inconnu** (§ 8.3) | *l'éditeur* ne le connaît pas | **rayures d'en-tête** + filet gris |
| **indisponible** (§ 8.4) | *le moteur* ne peut pas | **hachures de corps** + raison écrite |

Trois causes, trois responsables, trois remèdes. **Un seul gris pour les trois
rendrait l'éditeur inutilisable exactement au moment où l'utilisateur a besoin
d'aide.**

---

## 9. Commentaires et blocs

### 9.1 Le commentaire libre — 🔴 rien dans le corpus

⚠️ **À dire franchement : aucune des douze références ne montre un commentaire
libre posé sur le canevas.** `node-based-…webp` montre des paragraphes de texte,
mais **logés dans le corps d'un nœud**, pas flottants.

**PROPOSÉ**, en assumant que c'est une invention :

- **du texte posé sur le fond, sans corps, sans filet, sans fond** — le
  commentaire n'est pas un nœud et ne doit pas y ressembler ;
- 13 px, `#8A8A8A`, **aligné à gauche**, retour à la ligne à une largeur qu'on
  redimensionne par une poignée (§ 2.3) ;
- **au survol seulement**, un rectangle `#1A1A1A` à 60 % apparaît derrière pour
  qu'on sache où cliquer ;
- **il ne se branche à rien**, ne se replie pas, **et il passe DEVANT les fils,
  DERRIÈRE les nœuds** ;
- au dézoom **25 %, il disparaît entièrement** — c'est le seul objet qu'on
  efface complètement. Raison : illisible, il ne serait plus qu'une tache grise
  qui brouille la lecture de structure, et **c'est justement la lecture qu'on
  fait à 25 %**.

### 9.2 Le cadre — **VU**, et richement

`node-based-…webp` est la seule référence à en montrer un, et elle en montre
plus que prévu :

| micro-élément | ce qui est VU |
|---|---|
| **remplissage** | vert semi-transparent, ~8 % |
| **filet extérieur** | plein, 1,5 px, de la teinte du cadre |
| **second filet intérieur** | **pointillé**, à ~8 px du bord |
| **bandeau de titre** | plein, en haut, texte sombre sur la teinte |
| **compteur** | **`7 nodes`, aligné à droite du bandeau** |
| **teinte des nœuds contenus** | ⚠️ **les nœuds à l'intérieur prennent la teinte du cadre** (filet et en-tête verdis) alors que ceux du dehors restent bleus |

**PROPOSÉ pour la suite :**

- **le cadre passe DERRIÈRE tout** — nœuds et fils ;
- **il se déplace avec son contenu** : tirer le bandeau déplace le cadre **et**
  les nœuds ; tirer le corps du cadre **ne déplace que le cadre** (donc on peut
  cadrer autre chose). ⚠️ **Ces deux gestes doivent avoir deux curseurs
  différents** ;
- **l'appartenance est géométrique, pas déclarée** : un nœud déposé dans le
  rectangle en fait partie ; sorti, il n'en fait plus partie. Raison : une
  appartenance déclarée finit toujours par mentir quand on déplace les choses,
  et il faudrait alors la montrer, ce qui coûte un signal de plus.
- 🔴 **NON TRANCHÉ : reprend-on la teinture des nœuds contenus ?** C'est le
  micro-élément le plus fort de la référence, **et il entre en conflit direct
  avec notre en-tête = catégorie**. Teindre un nœud en vert parce qu'il est dans
  un cadre vert **écrase l'information de catégorie** — la seule qui survit au
  dézoom (§ 12.2). **PROPOSÉ : on ne teinte QUE le filet du corps, jamais
  l'en-tête.** Mais c'est un vrai arbitrage, et il revient à Rodolf.
- **le compteur `12 nœuds` est repris** : gratuit, et il dit tout de suite si un
  nœud est tombé du cadre sans qu'on le voie.
- **repli du cadre** : le bandeau seul reste, avec `▸ Éclairage · 12 nœuds`.
  🔴 **NON TRANCHÉ** : que deviennent les fils qui entraient dans le cadre ? Ils
  doivent aboutir quelque part sur le bandeau, et je n'ai aucune référence.

### 9.3 Le relais (reroute)

**VU** — la puce `body.tests` de la principale, avec une découverte : elle porte
**une entrée à gauche ET une sortie à droite**, plus un **bloc-icône plein
`#2E2770` sur toute la hauteur à gauche**. Ce n'est donc pas un simple point sur
un fil : **c'est une valeur nommée qui passe.**

**PROPOSÉ — deux objets, pas un**, parce que le corpus en montre deux et qu'ils
ne servent pas à la même chose :

| | dessin | rôle |
|---|---|---|
| **relais nu** | **un rectangle 17 × 17** de la couleur du type, rayon 2, **sans corps, sans texte** | ranger un fil. Il ne dit rien qu'on ne sache déjà |
| **relais nommé** (la puce) | pilule à coins 13 px, bloc-icône coloré à gauche, libellé, prise d'entrée et prise de sortie | poser une source près de son consommateur, à l'autre bout du graphe |

⚠️ **Le relais nu est un carré, pas la prise 17 × 63.** Raison : il n'est à
cheval sur aucun bord — il **est** le point. Lui donner la silhouette d'une prise
ferait chercher le nœud auquel il appartient.

---

## 10. Les fils

| famille | trait | source |
|---|---|---|
| **valeur** | courbe de Bézier, **2 px**, couleur du type | VU (principale) |
| **exécution** | même courbe, **3,5 px**, **toujours orange**, jamais colorée par un type | DÉCIDÉ |
| **tableau** | **double**, 2 × 1,4 px, écart 2,5 px | PROPOSÉ (§ 5.3) |
| **dictionnaire** | double, trait inférieur **pointillé** | PROPOSÉ (§ 5.3) |
| **en cours de tirage** | **pointillé gris**, suit le curseur | VU comme convention, pas dans le corpus |

**VU sur la principale, et à reprendre tel quel :**

- **la couleur du fil est celle de la prise dont il part** — vérifié sur trois
  paires (violet → violet, ambre → ambre, blanc → blanc). **Le fil n'a pas de
  couleur propre.** ⚠️ Corollaire : quand les deux bouts sont de familles
  différentes (une conversion déclarée, § 11.5), **PROPOSÉ : dégradé de la
  couleur de départ vers celle d'arrivée** — c'est ce que semble faire
  `174000` (un fil vire de l'orange au vert entre deux prises de couleurs
  différentes), sans que je puisse l'affirmer d'une seule image ;
- **le fil sort horizontalement du bord** avant de s'infléchir. Non négociable :
  c'est ce qui rend la prise de départ identifiable quand dix fils partent du
  même nœud ;
- **les fils passent DERRIÈRE les nœuds.**

**PROPOSÉ — la cohabitation des deux familles**, qui est la question ouverte n° 2
de `CATALOGUE_NOEUDS.md` § 5quinquies. Elles se distinguent par **trois signaux
simultanés** :

1. **la couleur** — l'orange d'exécution n'est jamais une couleur de donnée ;
2. **l'épaisseur** — 3,5 contre 2 ;
3. **le point d'attache** — l'exécution part de l'**en-tête**, la donnée du
   **corps**. ⚠️ **C'est le plus fort des trois** : les fils d'exécution vivent
   sur une bande horizontale, les fils de donnée en dessous. **La ligne de vie
   d'un blueprint devient une ligne géométrique**, et c'est ce qui permet de le
   lire en diagonale.

`83576832` fait mieux sur un point qu'il faut nommer : ses fils d'exécution sont
des **polylignes à angles** quand ses fils de donnée sont des courbes. **Un
quatrième signal, gratuit, et qui survit au noir et blanc.** 🔴 **NON TRANCHÉ** :
c'est un écart de style avec la principale, qui n'a que des courbes.

🔴 **NON TRANCHÉ — deux cas que `ELEMENTS_A_DESSINER.md` C5 et C6 laissent déjà
ouverts, et que je ne sais pas mieux trancher :**
- **fil sélectionné** — halo ? éclaircissement ? Je n'ai aucune référence.
- **croisement** — dessus, dessous, ou saut ? Le saut est le plus lisible et le
  plus cher à calculer ; sans mesure, je ne le recommande pas.

---

## 11. Tous les états

### 11.1 Erreur — **PROPOSÉ**

- filet du corps `#E4443C`, 1,5 px ; corps teinté `#2A1A1A` ;
- **`!` dans l'en-tête**, à gauche du `?` ;
- ⚠️ **la raison est écrite DANS le nœud**, en pied, `#E4443C`, 10 px :
  `fichier introuvable : textures/mur_albedo.png`.

**La raison lisible sans survol n'est pas un confort.** `CATALOGUE_NOEUDS.md`
l'exige deux fois, et `images (2)` montre pourquoi : dans un graphe de trente
nœuds, **survoler trente nœuds pour trouver celui qui a un problème est une
minute perdue à chaque fois.**

### 11.2 Avertissement — **PROPOSÉ** (état nouveau)

Ni dans le corpus, ni dans nos documents. **Il manque, et le § 6.2 le prouve** :
une texture sRVB branchée sur une entrée `Normale` produit un graphe **valide et
faux**. L'erreur serait un mensonge (ça compile), le silence aussi (ça ne marche
pas).

- filet `#F79A28` **pointillé** (l'orange déjà connu, mais en pointillé pour ne
  pas se confondre avec le filet d'exécution plein) ;
- `⚠` dans l'en-tête ; raison en pied, `#F79A28`.

### 11.3 Désactivé — **PROPOSÉ**

Corps et en-tête à **45 % d'opacité**, texte à 40 %. **Les prises et les fils
restent à pleine opacité** — sinon on croit que le nœud est déconnecté alors
qu'il est seulement éteint.

⚠️ **Signal supplémentaire, et il porte tout le sens** : **le fil qui le traverse
passe en pointillé de part en part.** Un nœud désactivé dans une chaîne ne coupe
pas la chaîne, il la laisse passer ; le pointillé le dit à distance, sans lire le
nœud.

### 11.4 Survolé, sélectionné, actif

`ELEMENTS_A_DESSINER.md` D7 laisse le survol ⚠️. Le corpus, lui, répond — et il
répond mieux que prévu, parce qu'il montre **trois** états là où nos documents
en prévoyaient deux.

| état | dessin | source |
|---|---|---|
| **survolé** | **filet du corps éclairci** (`#33333C` → `#5A5A68`). **Jamais le corps** | PROPOSÉ — teinter le corps au survol fait « clignoter » le canevas quand on le traverse à la souris |
| **sélectionné** | **bordure `#F79A28` de 1,6 px sur tout le nœud, en-tête comprise**, épousant les rayons (0 en bas, 5 en haut) | **VU** (`images (3)` : bordure rouge-orange ; `174057` : liseré bleu ; `node-based` : liseré bleu). **Trois références, trois couleurs, une seule convention : la bordure.** |
| **actif** | ⚠️ **en-tête ÉCLAIRCI de 15 %**, en plus de la bordure de sélection | **VU** (`images (3)` : un seul nœud a l'en-tête bleu clair, ses jumeaux l'ont bleu sombre, et tous portent la bordure rouge) |

⚠️ **`sélectionné` et `actif` sont deux états différents, et le corpus le prouve.**
Nos documents ne les distinguaient pas. Un éditeur qui ne les sépare pas **ne
sait pas à qui envoyer le clavier** quand douze nœuds sont sélectionnés.

**DÉCIDÉ** (rappel) : la sélection est **une bordure, jamais une teinte du
corps**. Raison confirmée par le corpus : la teinte du corps est déjà prise par
l'erreur et le désactivé.

### 11.5 Pendant le tirage d'un fil

C'est, d'après `CATALOGUE_NOEUDS.md` § 2, *« le retour visuel le plus utile de
tout l'éditeur »*, et **aucune référence ne le montre.**

| prise | dessin |
|---|---|
| **compatible** | halo clair de 1,5 px, couleur du type, à 60 % |
| **convertible** (conversion déclarée dans ce sens) | halo **pointillé**, couleur à 35 % |
| **incompatible** | **éteinte** : `#3A3A44`, 30 % d'opacité |

⚠️ **La distinction compatible / convertible n'est pas un raffinement.**
`CATALOGUE_NOEUDS.md` § 2 le pose : *« réel → couleur existe, couleur → réel
n'existe pas »*. **Les conversions sont dirigées** : si le dessin ne montre que
deux états, l'utilisateur apprend une règle symétrique qui est fausse, et il la
découvre en tirant le fil dans l'autre sens.

**PROPOSÉ** : pendant le tirage, **les nœuds entiers dont aucune prise n'est
compatible passent à 50 %.** Raison : dans un graphe de trente nœuds
(`images (2)`), éteindre des prises de 17 px ne se voit pas ; éteindre des nœuds,
oui. **Le retour doit être visible à l'échelle où on travaille, pas à celle où on
dessine la spécification.**

### 11.6 Le graphe invalide doit rester dessinable — **DÉCIDÉ** (rappel)

`CATALOGUE_NOEUDS.md` § 6 : *« un éditeur passe son temps dans ces états. »*

| cas | dessin |
|---|---|
| **lien pendant** | fil qui s'arrête en l'air, terminé par un **petit disque creux** |
| **cycle en cours de construction** | **les fils du cycle passent en `#E4443C`**, tous ensemble — le cycle se voit comme une boucle rouge, pas comme un nœud fautif |
| **entrée obligatoire vide** | **prise creuse cerclée de `#E4443C`**, et le nœud passe en avertissement (§ 11.2), pas en erreur |

---

## 12. Le canevas

### 12.1 La recherche de nœud filtrée

`ELEMENTS_A_DESSINER.md` E1, ⬜. **PROPOSÉ**, et c'est le pivot du § 5.6.

Quand on tire un fil dans le vide, le panneau s'ouvre au curseur, **filtré par le
type de la prise d'origine**, et son en-tête dit le filtre **en toutes lettres** :

```
  ┌──────────────────────────────────┐
  │ 🔍 _________________             │
  │ qui acceptent   [1.0] tableau de │  ← le filtre, écrit
  │                       réels    ✕ │  ← et retirable
  ├──────────────────────────────────┤
  │  Pour chaque            Boucle   │
  │  Longueur               Tableau  │
  │  Élément à l'indice     Tableau  │
  └──────────────────────────────────┘
```

⚠️ **Le filtre doit être écrit et retirable.** Un filtre invisible transforme
« ce nœud n'existe pas » en conclusion, alors que la vraie phrase est « ce nœud
n'accepte pas ce type ». C'est la même faute que la face n° 8 du `CLAUDE.md`
parent — **un résultat négatif sans son périmètre est une rumeur.**

### 12.2 Le dézoom — trois paliers

**DÉCIDÉ** dans `ELEMENTS_A_DESSINER.md` D11. Ce que j'ajoute, c'est **la mesure
qui justifie les seuils** : `images (2)` et `images (5)` sont deux références
prises précisément à ces échelles, et elles montrent ce qui survit.

| palier | ce qui reste | ce qui disparaît |
|---|---|---|
| **100 %** | tout | — |
| **55 %** | en-tête, titre, étiquettes de rangée, prises, filet, **aperçus (réduits)**, **barres d'édition** | **valeurs et champs de saisie** |
| **25 %** | **un rectangle de la couleur de la catégorie**, le filet exécution/pétrole, les fils, **la couleur moyenne d'un aperçu** | tout le texte, toutes les prises, les commentaires |

**La mesure qui le fonde** : dans `images (2)` — un vrai graphe de trente nœuds —
**les libellés sont déjà illisibles alors que les couleurs d'en-tête et les
couleurs de fil restent parfaitement lisibles.** Dans `images (5)`, plus dézoomé
encore, **seuls les fils structurent l'image**.

> ⚠️ **Conséquence, et c'est l'argument qui commande toute la palette** : à 25 %,
> **la couleur d'en-tête et la couleur de fil sont TOUT ce qu'il reste.** C'est
> pour ça que les couleurs de catégorie doivent être fortement distinctes, et
> c'est pour ça que le § 4.1 refuse d'en inventer seize.

**PROPOSÉ — l'ordre de disparition** : *valeurs → étiquettes → prises → titre*.
Raison : on dézoome pour lire la **structure**, jamais les nombres. Ce qui sert à
la structure part en dernier.

### 12.3 Le reste du canevas

| élément | état |
|---|---|
| **sélection au lasso** | rectangle pointillé `#F79A28` pendant le glissé — PROPOSÉ |
| **fil d'Ariane de sous-graphe** | bandeau en haut à gauche — **VU** (`images (5)` : bouton `Project`) |
| **aide contextuelle** | ligne de texte libre en haut du canevas — **VU** (`node-based-…webp`) ; **PROPOSÉ : oui, elle ne coûte rien et remplace un tutoriel** |
| **menu contextuel** | trois contenus (fond / nœud / fil) — ⬜, PROPOSÉ inchangé |
| **minicarte** | 🔴 **NON TRANCHÉ** (E3) |
| **barre d'outils** | 🔴 **NON TRANCHÉ** (E4) |

---

## 13. Récapitulatif des 🔴 NON TRANCHÉS

**À lire avant tout codage.** Chacun de ces points sera décidé en passant s'il
n'est pas décidé exprès — et décidé en passant, il sera incohérent.

| # | question | où | pourquoi ça coûte |
|---|---|---|---|
| 1 | **largeur du nœud** : fixe, adaptative ou redimensionnable ? | § 2.3 | une largeur libre doit être **sérialisée** |
| 2 | **séparer une struct casse-t-il le lien** existant ? | § 4.6 | touche le modèle et l'annulation |
| 3 | **fil de dictionnaire** : pointillé (collision avec le tirage) ou triple trait ? | § 5.3 | signal ambigu si mal choisi |
| 4 | **`corps de boucle` / `terminé`** : un signal plus fort qu'une ligne vide ? | § 5.5 | la faute n° 1 du débutant |
| 5 | **shader** : forme de prise propre, ou couleur + glyphe ? | § 6.3 | une septième forme de prise |
| 6 | **groupe** : sur place ou en onglet ? aperçu du contenu ? | § 7.6 | choix de fenêtrage, pas de dessin |
| 7 | **cadre** : teinte-t-il ses nœuds ? | § 9.2 | ⚠️ **entre en conflit avec l'en-tête = catégorie** |
| 8 | **cadre replié** : où aboutissent les fils entrants ? | § 9.2 | aucune référence |
| 9 | **fil sélectionné** : halo ou éclaircissement ? | § 10 | déjà ⚠️ dans C5 |
| 10 | **croisement de fils** : dessus, dessous, saut ? | § 10 | déjà ⚠️ dans C6 |
| 11 | **fils d'exécution en polylignes** à angles, ou en courbes comme la principale ? | § 10 | écart de style assumé ou non |
| 12 | **minicarte** et **barre d'outils** | § 12.3 | déjà ⚠️ dans E3, E4 |
| 13 | **champ de saisie** : plus clair (mesuré sur la principale) ou plus sombre (A9 actuel) ? | § 1.1 | ⚠️ **contredit un document déjà écrit** |
| 14 | **`images (1)`** : cohérence avec l'inspecteur, ou autre attente ? | § 0 | je refuse de deviner |

---

## 14. Les planches

| planche | contenu |
|---|---|
| `references/planche_01_noeuds.svg` | **existante** — les formes de base dans le style de la principale |
| `references/planche_02_types.svg` | les 7 familles, les 16 types, pastilles, prises, **tableaux et dictionnaires** |
| `references/planche_03_formes.svg` | les 14 formes de nœud, dont custom, groupe, itérateur, aperçu |
| `references/planche_04_etats.svg` | tous les états, le tirage, les trois gris, les trois paliers de dézoom |
| `references/planche_05_matieres.svg` | textures et matériaux, aperçu ON/OFF, commentaire, cadre, relais |

Rendu PNG : `msedge --headless=new --screenshot --window-size=L,H fichier.svg`.

---

## 15. Ce que ce document N'A PAS fait

Par honnêteté, et pour que personne ne croie le contraire :

1. **Il n'a mesuré aucune lisibilité réelle.** Les distances de couleur sont
   calculées, pas observées sur un écran. La première planche rendue en PNG et
   regardée à 55 % et à 25 % peut contredire le § 12.2 — **et c'est elle qui
   aura raison.**
2. **Il n'a pas vérifié que le modèle NKGraph porte ces notions.** Le tableau, le
   dictionnaire, le nœud inconnu, le drapeau « exécute » d'un nœud custom, la
   largeur sérialisée : je les ai spécifiés **visuellement** sans lire
   `NkNodeGraph.h`. **Plusieurs n'existent probablement pas encore dans le
   modèle**, et le dire est plus utile que de le supposer.
3. **Il n'a pas décidé le sens de lecture.** Tout est écrit pour un graphe
   **horizontal** (la principale l'est). `secondaire_prises_verticales.png`
   montre l'autre monde, et `DESIGN_EDITEUR_NODAL.md` § 2 déconseille de mélanger
   les deux. **Rien ici ne l'interdit formellement** — mais rien ne le prévoit
   non plus.
