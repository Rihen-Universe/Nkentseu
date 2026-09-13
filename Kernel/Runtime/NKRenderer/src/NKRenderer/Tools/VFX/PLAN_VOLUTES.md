<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN VOLUTES — pré-enregistrement : la taille des tourbillons, EN CELLULES

**Écrit le 2026-09-12 (soir), AVANT la première ligne de code et AVANT la
première mesure.** Même raison d'être que `PLAN_GRILLE_MAC.md` et
`PLAN_ADVECTION_FLUX.md` : un critère écrit après la mesure ne juge plus rien.

---

## 0. LA QUESTION, ET D'OÙ ELLE VIENT

Le 07/09, en regardant les deux images du confinement, l'agent a écrit :

> « le panache est plus large et STRUCTURÉ, avec des bouffées internes visibles,
> **mais il ne montre pas encore de grosses volutes qui s'enroulent** (boîte de
> 0,5 m, caméra proche : les tourbillons entretenus restent de la taille de
> quelques cellules). »

La parenthèse est une **hypothèse mesurable**, pas une impression : elle prédit
que l'échelle des structures tourbillonnaires vaut **quelques cellules**. Deux
causes possibles, et elles ne se réparent pas de la même façon :

- **la BOÎTE (résolution spatiale)** : la source fait 0,06 m de rayon sur des
  cellules de 2 cm, soit **3 cellules** ; tout ce qui s'enroule autour d'un
  panache de 6 cellules de large fait, au mieux, quelques cellules. La taille
  des structures est fixée par la **physique** (la source, en mètres) et la
  grille est trop grosse pour la montrer. Remède : des cellules plus petites —
  un COÛT, pas un solveur ;
- **le SCHÉMA** : la dissipation numérique (semi-lagrangien) et le confinement
  (`epsilon · h`, qui injecte à l'échelle de la grille) fixent la taille des
  structures **à quelques cellules quelle que soit la grille**. Remède : un
  autre schéma.

**Si c'est la boîte, on s'arrête et on le dit.** Une limite comprise coûte
moins qu'un solveur réécrit.

---

## 1. 🎯 (n1) L'INSTRUMENT — une échelle de longueur, en cellules

### 1.1 Définition

Sur l'intérieur STRICT (la population de tous les témoins de vorticité), avec
`omega = rot(u)` **tel que le solveur le calcule** (au centre des cellules, à
partir des faces moyennées — la même valeur que celle que le confinement lit,
exposée par des accesseurs en lecture seule, sans rien changer à la physique) :

```
R_x(r) = somme_i  omega(i) · omega(i + r e_x)   /   somme_i  omega(i) · omega(i)
```

produit SCALAIRE des vecteurs vorticité (le signe compte : deux tourbillons
contra-rotatifs voisins sont **anti**-corrélés, ce qui est exactement ce qui
sépare « un tourbillon » de « une nappe uniforme »). Le dénominateur porte sur
toute la population stricte ; le numérateur sur les paires dont les deux
cellules sont dans la population stricte. `R_x(0) = 1` par construction.

L'échelle intégrale, en cellules :

```
L_x = somme_{r=0}^{r0-1}  ( R_x(r) + R_x(r+1) ) / 2
```

intégrée jusqu'au **premier passage par zéro** `r0` (interpolé linéairement
entre `r0-1` et `r0`). Si `R_x` ne passe jamais par zéro sur la demi-largeur du
domaine, l'instrument **REND FALSE** : la structure est plus grande que ce que
la boîte permet de mesurer, et fabriquer un chiffre serait mentir.

Même chose selon `z` (`L_z`). Les deux axes horizontaux — ceux qui traversent
le panache — sont publiés séparément, et la valeur retenue est leur moyenne :
`L = (L_x + L_z) / 2`.

### 1.2 Calibration : de L au DIAMÈTRE

Pour un tube de vorticité uniforme (cœur en rotation solide) de rayon `R`,
l'autocorrélation selon `x` est la fraction de recouvrement de deux disques
décalés de `r`, et son intégrale vaut **analytiquement** :

```
L = (4R/pi) · ( ∫_0^1 acos(s) ds  −  ∫_0^1 s·sqrt(1−s²) ds )
  = (4R/pi) · ( 1 − 1/3 )  =  8R / (3 pi)  ≈  0,8488 R
```

D'où le **diamètre calibré** que l'instrument publie :

```
D = 2R = (3 pi / 4) · L  ≈  2,356 · L        (en cellules)
```

Ce facteur est dérivé, pas ajusté. Le contrôle positif le vérifie sur deux
rayons différents — s'il fallait un facteur différent pour chaque rayon,
l'instrument ne mesurerait pas une taille.

### 1.3 Contrôles de l'instrument — écrits avant, seuils fixés maintenant

| contrôle | montage | attendu | seuil |
|---|---|---|---|
| **POSITIF 1** | tube de rotation solide, axe `y`, rayon `R = 6` cellules, posé par les vitesses aux faces (le solveur calcule omega ; pas inerte) | `D = 12` cellules | ± 12 % (10,6 … 13,4) ; `L_x` et `L_z` à ± 5 % l'un de l'autre |
| **POSITIF 2 (échelle)** | même montage, `R = 3` cellules | `D = 6` cellules | ± 20 % (4,8 … 7,2) ; **rapport D(6)/D(3) dans [1,8 ; 2,2]** |
| **NÉGATIF 1 (bruit)** | vitesse = bruit blanc (LCG déterministe, graine dite) | corrélation détruite dès r = 1 | `D < 2,0` cellules |
| **NÉGATIF 2 (uniforme)** | rotation solide sur TOUT le domaine (omega uniforme) | `R(r)` ne passe jamais par zéro | l'instrument **rend false**, ne fabrique pas de point |
| **NÉGATIF 3 (vide)** | grille au repos, omega = 0 partout | dénominateur nul | **rend false** |

**⚠️ CORRECTION DE MONTAGE, datée du 12/09 après la PREMIÈRE course, les seuils
n'ayant pas bougé d'un chiffre.** Le tableau ci-dessus disait « tube de rotation
solide » sans dire ce qu'il y a DEHORS. Une rotation solide **tronquée** (vitesse
nulle hors du disque) n'est pas le champ de la dérivation § 1.2 : la vitesse y
saute de `Omega R` à 0 sur le bord, ce qui est une **nappe de vorticité de signe
opposé**, dont la circulation annule celle du disque et dont l'enstrophie domine.
Mesuré : D = 3,65 pour 12 attendu, 2,77 pour 6, rapport 1,32 — l'instrument
mesurait la nappe, et il avait raison. Le champ « omega uniforme dans un disque,
nul dehors » est le **tourbillon de Rankine** (irrotationnel `u_theta = Omega
R² / r` au-dehors) : c'est lui que les POSITIFS 1 et 2 posent désormais. Un
contrôle positif qui rougit sur un montage faux est exactement ce à quoi il sert.

Le POSITIF 2 est le seul qui prouve que l'instrument mesure une **taille** : un
instrument qui rend 12 à R = 6 pourrait rendre 12 à R = 3 (par exemple s'il
mesurait la boîte). Les tolérances sont plus larges à R = 3 parce qu'un disque
de 3 cellules de rayon est un disque grossier (± 1 cellule de bord sur 6).

---

## 2. LA SCÈNE — celle des deux images, à l'identique

Exactement `ConstruirePanache(g, false, 255, epsilon)` de `rendu.cpp`, la scène
des images `fumee_jet_sans_confinement_2026-09-07.png` et
`fumee_panache_confinement_2026-09-07.png` :

- boîte `[-0,25 ; 0,25] × [0 ; 1,6] × [-0,25 ; 0,25]` m, `h = 0,02` m → `25 × 80 × 25`
- `dt = 1/60`, **255 pas**, source continue : sphère `r = 0,06` m en `y = 0,05`,
  densité `7·dt`, `ΔT = 400·dt`, dissipations 0,5, `alpha = 0,25`, tolérance 1e-4
- **A** : `epsilon = 0` (le jet) ; **B** : `epsilon = 8` (le panache)

**Ce qu'on publie pour (n1)**, pour A et pour B :

- `D` en cellules **moyenné sur les 60 DERNIERS pas** (196 à 255) — un
  instantané peut tomber sur un creux ; la moyenne ne dépend pas de l'instant
  choisi. Le min et le max sur ces 60 pas sont publiés à côté ;
- `D` en mètres (`D · h`) et `D` rapporté au **diamètre de la source**
  (0,12 m), pour que le chiffre se lise sans la grille ;
- `L_x` et `L_z` séparément.

**La prédiction inscrite** (l'hypothèse du 07/09) : `D(B)` vaut **entre 2 et 8
cellules**. Ce n'est pas un vert/rouge — c'est ce que la phrase « quelques
cellules » veut dire, chiffré avant de mesurer, pour qu'on sache si la phrase
était juste.

---

## 3. 🔬 (n2) BOÎTE OU SCHÉMA — deux expériences, une règle de décision

### (n2a) — littéralement ce qui est demandé : la boîte à nombre de cellules constant

Boîte divisée par 2 dans les trois dimensions : `[-0,125 ; 0,125] × [0 ; 0,8] ×
[-0,125 ; 0,125]` m, `h = 0,01` m → **toujours 25 × 80 × 25**. La source reste
**la même en mètres** (`r = 0,06` m en `y = 0,05`) : c'est l'objet physique, on
ne le change pas. ⚠️ **Confondant dit d'avance** : les parois latérales passent
de 0,25 m à 0,125 m de l'axe — le panache les sent. C'est pour ça que (n2a) ne
décide pas seule.

### (n2b) — l'expérience propre : h seul change

Même boîte de 0,5 × 1,6 × 0,5 m, même source, même `dt`, même nombre de pas,
même `epsilon = 8` ; **seul `h` passe de 0,02 à 0,01 m** → `50 × 160 × 50 =
400 000 cellules`. Une seule chose change. Coût attendu : ~8 fois plus de
cellules et un Poisson plus long, soit quelques minutes par course — dit après.
(`dt` n'est PAS divisé : le semi-lagrangien est inconditionnellement stable et
diviser `dt` changerait une seconde chose ; le CFL réellement vu est publié.)

### La règle de décision — fixée maintenant, sur (n2b), scène B

```
r = D_cellules(h = 1 cm) / D_cellules(h = 2 cm)
```

- **r ≥ 1,6** → la taille des structures est fixée par la **physique** (elle
  reste ~constante en MÈTRES : `D·h` à ± 25 %) et la grille de 2 cm était trop
  grosse pour la montrer → **C'EST LA BOÎTE (la résolution)**. On s'arrête, on
  chiffre ce que coûterait la résolution qui montrerait des volutes, et on ne
  touche pas au solveur ;
- **r ≤ 1,3** → la taille reste **accrochée à la grille** (quelques cellules
  quel que soit `h`, donc `D·h` divisé par ~2) → **C'EST LE SCHÉMA**. On le dit,
  on nomme le correctif, on ne l'écrit pas dans ce lot ;
- **entre 1,3 et 1,6** → **indéterminé**, dit tel quel. Aucun solveur n'est
  écrit dans ce cas non plus.

(n2a) est publiée à côté avec son propre `r`, et si les deux `r` ne disent pas
la même chose, c'est le confondant des parois qui parle — on le dira.

---

## 4. ⛔ (n3) LES CONTRÔLES NÉGATIFS — un témoin qui ne peut pas rougir ne témoigne pas

| contrôle | montage | attendu |
|---|---|---|
| **(n3a) déterminisme** | scène B jouée deux fois | `D` **identique au dernier chiffre** (le solveur n'a aucun aléa) |
| **(n3b) invariance par translation** | scène B, boîte ET source déplacées de `(+1,3 ; +0,2 ; −0,7)` m | `D` identique à **1e-4 relatif** (les coordonnées monde changent, la physique non ; un instrument qui lirait le monde au lieu de la grille rougirait ici) |
| **(n3c) fidélité de la copie** | la scène reconstruite par ce lot, à ses paramètres par défaut, contre `ConstruirePanache` de `rendu.cpp` **appelée telle quelle** | `D` final **identique au dernier chiffre** — une copie qui dérive est le piège classique, et ce contrôle le voit |
| **(n3d) la caméra** | — | **par construction** : l'instrument ne lit aucun pixel. Dit, pas éprouvé : on n'écrit pas un témoin qui ne peut pas rougir. |

---

## 5. CE QUE CE LOT NE FAIT PAS

- **Aucune ligne du solveur ne change.** Seuls des accesseurs en lecture sur
  `omega` sont ajoutés à `NkFluidGrid.h`, et `ConstruirePanache` / `EcrirePng`
  de `rendu.cpp` cessent d'être `static` pour être réutilisées.
- **Aucun verdict sur « ça tournoie à l'œil ».** Si (n2b) rend un panache à
  1 cm, son image est rendue **avec la même caméra** que les deux du 07/09 et
  versionnée comme aide à l'œil — elle ne prouve rien de plus que ce que la
  mesure dit.
- **Aucun GPU**, aucune fenêtre, aucun device : banc CPU, comme tout ce banc.

## 5bis. ADDENDUM DATÉ — 2026-09-12, 20h30, APRÈS la première course de (n1) et AVANT toute mesure de ce qui suit

**Ce que la première course a appris, et que (n1) ne pouvait pas dire.** Sur la
scène B, (n1) rend **D = 2,20 cellules** à h = 2 cm et **2,33** à h = 1 cm
(n2a) : le premier zéro de la corrélation tombe à r ≈ 2 dans les deux cas. Ce
que l'instrument voit sur un panache, c'est la **nappe de cisaillement** qui
l'entoure — et dans un solveur sans viscosité, une nappe de vorticité est
**aussi mince que la grille le permet**, à toute résolution. Une nappe qui
s'enroule (une volute) reste une nappe mince : ses spires successives sont de
même signe et séparées par le pas de la spirale, mais (n1) s'arrête au premier
zéro, avant de les voir. **(n1) mesure l'épaisseur de la nappe, pas le diamètre
de l'enroulement.** Il reste écrit tel quel, avec ce constat à côté ; il ne se
réécrit pas — il se DOUBLE.

**(n1') LE SECOND INSTRUMENT — le critère Q.** Hunt, Wray & Moin, « Eddies,
streams, and convergence zones in turbulent flows », CTR Summer Program 1988 :

```
Q = ½ ( |Omega|² − |S|² ),   Omega = partie antisymétrique du gradient de u,
                              S     = partie symétrique
```

`Q > 0` là où la **rotation domine le cisaillement** : le cœur d'un tourbillon.
Dans un cisaillement simple (une nappe droite, un jet), `|Omega| = |S|` et
`Q = 0` ; dans un enroulement, l'intérieur des spires tourne en bloc et `Q > 0`
sur tout le cœur. C'est **exactement** la différence entre « un jet » et « une
volute », et c'est ce que (n1) ne sépare pas. Le gradient de `u` est pris au
centre des cellules avec le MÊME pochoir que `ComputeVorticity` (faces
moyennées, différences centrées), population STRICTE.

**Définition du diamètre.** Les cellules avec `Q ≥ 0,01 · Q_max` (seuil relatif
fixé ICI, pour que le bruit des zones quasi immobiles ne relie pas tout) sont
regroupées en **composantes connexes** (6-voisinage). Pour chaque composante :
volume `V` (cellules) et étendue verticale `nj` (rangées) → section horizontale
moyenne `A = V / nj` → `d = 2 · sqrt(A / pi)`. **Exact sur un cylindre.** Le
diamètre publié est la moyenne des `d` **pondérée par l'enstrophie** de chaque
composante (un gros cœur pèse plus qu'une miette), avec à côté : le `d` de la
composante la plus lourde, le nombre de composantes, la fraction de
l'enstrophie stricte portée par les cellules `Q > 0`. Si la composante la plus
lourde **touche une paroi LATÉRALE** (x ou z) de la population stricte,
l'instrument **rend false** : `d` dérive de la section HORIZONTALE, et seule une
paroi latérale la tronque. Le sol et le plafond ne tronquent pas la section —
le tube des contrôles positifs traverse d'ailleurs toute la hauteur, et le
panache naît au sol ; toucher le sol ou le plafond est **publié comme un
drapeau**, pas comme un refus. (Précision écrite avant la première mesure de
(n1'), pas après.)

**Contrôles, seuils fixés maintenant :**

| contrôle | montage | attendu |
|---|---|---|
| POSITIF 1 | Rankine R = 6 (cœur en rotation solide, `Q = Omega² > 0` dedans, irrotationnel `Q < 0` dehors) | `d = 12 ± 10 %`, **une** composante |
| POSITIF 2 | Rankine R = 3 | `d = 6 ± 20 %` ; rapport d(6)/d(3) dans [1,8 ; 2,2] |
| NÉGATIF 1 | bruit blanc | **aucune structure cohérente** : soit `d < 2` (miettes), soit **false** parce que l'amas de `Q > 0` PERCOLE jusqu'aux parois — en 3D, un site sur trois suffit à percoler, et le bruit en retient plus. Rouge si l'instrument rend un `d ≥ 2` valide : il aurait fabriqué un tourbillon dans du bruit. |
| NÉGATIF 2 | rotation uniforme sur tout le domaine | la composante touche les parois latérales → **false** |
| NÉGATIF 3 | grille vide | **false** |
| NÉGATIF 4 (le décisif) | **cisaillement simple** `u = a·z`, tout le domaine | `Q = 0` partout → aucune cellule retenue → **false**. Un instrument qui verrait un tourbillon dans un cisaillement pur ne pourrait pas séparer un jet d'une volute. |

**Même règle de décision** que § 3, appliquée à `r' = d(1 cm) / d(2 cm)` sur la
scène B : `r' ≥ 1,6` → boîte ; `r' ≤ 1,3` → schéma ; entre → indéterminé. Les
deux instruments sont publiés côte à côte ; s'ils ne disent pas la même chose,
c'est dit, et c'est (n1') qui parle des volutes, parce que lui seul distingue
rotation et cisaillement — (n1) parle de la nappe.

**(n3b), rouge à la première course (écart 5,8 % au lieu de 1e-4) : il reste
rouge tel qu'écrit.** La cause est à MESURER, pas à supposer : si la source
discrétisée diffère entre les deux repères (arrondi float32 de `bmin + (i−0,5)h`
sur le bord de la sphère), les deux courses ne sont pas le même problème discret
et l'écart mesure la sensibilité de l'écoulement, pas l'instrument. Contrôle
ajouté : nombre de cellules dont la densité diffère après le PREMIER pas, dans
les deux repères, et la masse injectée. Si ce nombre est nul, la cause est
ailleurs et il faudra la chercher.

## 5ter. NOTE DATÉE — 2026-09-12, 21h15 : (n2b) de la première course RÉFUTE la prémisse du § 5bis

Le § 5bis a été écrit pendant la première course, après (n1) et (n2a), **avant**
que (n2b) ne rende son chiffre. Sa prémisse — « une nappe de vorticité est aussi
mince que la grille le permet, à toute résolution, donc (n1) rendra ~2 cellules
quel que soit h » — **est fausse, et c'est (n2b) qui le dit** : à h = 1 cm dans
la même boîte, (n1) rend **4,77 cellules** (min 4,04, max 6,31 sur 60 pas), pas
2. En mètres : **0,0440 → 0,0477 m, × 1,08**. La taille que (n1) mesure est
fixée par la physique, pas par la grille ; à 2 cm elle tombait à 2,2 cellules
parce que 0,044 m font 2,2 cellules de 2 cm, et c'est tout.

**Conséquences, dans l'ordre :**

1. **Le verdict de (n1) sous la règle du § 3 est celui de la première course
   et il tient** : r = 4,769 / 2,201 = **2,167 ≥ 1,6 → C'EST LA BOÎTE.**
   (n2a) disait 1,059 — le confondant des parois, annoncé au § 3, a parlé : en
   divisant la boîte par deux avec une source de 0,12 m de diamètre, les parois
   à 0,125 m de l'axe écrasent le panache (D en mètres y tombe à 0,023).
2. **(n1') reste, avec ses contrôles et sa règle**, parce qu'il mesure autre
   chose que (n1) — la rotation contre le cisaillement — et qu'un second
   instrument indépendant vaut par ce qu'il sépare, pas par la raison qu'on
   avait de l'écrire. Mais la raison écrite au § 5bis était fausse et reste là,
   barrée par ce paragraphe, pour qu'on ne la recopie pas.
3. Une hypothèse écrite entre deux mesures de la même course est une
   hypothèse, pas un résultat : elle a été datée, elle a été réfutée par le
   chiffre suivant, et c'est exactement ce qu'on attend d'elle.

## 6. LA COUPE

**(n1) seule, mesurée et commitée, est un résultat complet.** (n2) et (n3)
viennent après, dans cet ordre : (n3) d'abord (ils sont courts et ils gardent
(n1)), puis (n2a), puis (n2b) qui coûte.

## 7. COMMENT REJOUER

```
jenga build --target NkFluidGridProbe --config Release
NK_FLUID_VOLUTES=1 ./Build/Bin/Release-Windows/NkFluidGridProbe/NkFluidGridProbe.exe
```

Le mode `NK_FLUID_VOLUTES=1` ne joue que ce lot. Les contrôles de l'instrument
et (n1) sont aussi joués par la course complète (sans le mode), pour que
l'instrument reste gardé ; (n2b) ne l'est que sous le mode, à cause de son coût.
