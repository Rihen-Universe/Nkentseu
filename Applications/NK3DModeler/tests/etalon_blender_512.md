# L'ÉTALON BLENDER 5.1.2 — grandeurs PRÉLEVÉES AU PIXEL

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen

Rodolf a fourni onze captures de **Blender 5.1.2** le 18/09 à 05:34 et 05:40. La référence
n'est donc plus une description : c'est une image fixe, qui ne bougera jamais.

> ⚠️ **ON N'EN TIRE PAS UNE COMPARAISON D'IMAGES.** Ni la même caméra, ni la même résolution,
> ni le même thème, ni le même moteur. Une comparaison pixel à pixel entre Blender et nous
> produirait un écart énorme qui ne dirait rien. **Ce qu'on en tire, ce sont des GRANDEURS** —
> couleurs, tailles, épaisseurs, règles d'occultation — et ce sont elles qui deviennent les
> attendus de **nos propres** témoins.

---

## 1. CE QUE CHAQUE CAPTURE MONTRE RÉELLEMENT

Vérifié une par une, pas supposé.

| capture | mode | sous-mode | sélection | ce qu'on voit |
|---|---|---|---|---|
| **053400** | Objet | — | rien | cube gris uni. **Aucun marqueur, aucun contour.** |
| **053409** | Objet | — | l'objet | **contour orange sur la SILHOUETTE seulement** ; l'intérieur reste gris, aucune arête interne |
| **053416** | Édition | **Face** | tout | **faces teintées d'orange translucide** (le gris transparaît), **arêtes orange vif**. Aucun point de sommet |
| **053421** | Édition | **Sommet** | **rien** | **sommets = carrés NOIRS pleins**, **arêtes = traits noirs fins**, faces **grises sans teinte** |
| **053427** | Édition | Sommet | **un seul** | le sommet est **BLANC** (actif), et **les trois arêtes qui y touchent sont orange en DÉGRADÉ**, s'estompant vers l'autre extrémité |
| **053433** | Édition | Sommet | **deux** | deux sommets clairs, et le dégradé orange court le long des arêtes concernées |
| **053440** | Édition | **Arête** | **une arête** | l'arête du dessus est **jaune/orange vif** sur toute sa longueur ; aucun point de sommet visible |
| **054024** | Édition | Sommet | tout | **faces orange translucide + arêtes orange vif + sommets orange**, et le menu `X` par-dessus |

*(053447, 053454 et 054016 sont détaillées au §5 — elles ont été lues depuis.)*

---

## 2. LES GRANDEURS, PRÉLEVÉES AU PIXEL

### 2.1 Sommets

| état | couleur mesurée | taille mesurée | liséré ? |
|---|---|---|---|
| **non sélectionné** | **(0, 0, 0)** — noir pur | amas plein d'environ **4 × 4 px** | **AUCUN** |
| **sélectionné** | **(255, 121, 0)** — orange vif | environ **4 × 4 px** | **AUCUN** |
| **actif** | blanc | idem | aucun |

⚠️ **DEUX ÉCARTS AVEC NOTRE IMPLÉMENTATION, ET ILS SONT NETS :**

1. **Blender ne dessine AUCUN liséré.** Nous traçons un carré sombre (alpha 0,9) **sous** un cœur
   coloré, `kLisereSupp = 0,7 px` de plus de chaque côté. Blender pose **un carré plein, un point
   c'est tout**. Notre liséré est donc une invention maison — elle a sa raison (lisibilité sur fond
   clair **et** sombre), mais **elle n'est pas Blender**, et le critère est Blender.
2. **La couleur du sélectionné diffère** : Blender **(255, 121, 0)**, nous
   **(255, 140, 13)** (`1.0, 0.55, 0.05`). Le vert et le bleu sont plus hauts chez nous — notre
   orange est plus **clair** et moins saturé.
3. **Le non sélectionné diffère aussi** : Blender **(0,0,0)** franc, nous **(5, 5, 8)**
   (`0.02, 0.02, 0.03`). Écart négligeable à l'œil, mais autant l'aligner.

### 2.2 Arêtes

| état | couleur mesurée |
|---|---|
| **non sélectionnée** | **noir**, trait fin (1 à 2 px), qui s'estompe vers le gris aux extrémités |
| **sélectionnée** | **(255, 121, 0)** — la **même** couleur que le sommet sélectionné |

⚠️ **Notre arête sélectionnée est (255, 178, 33)** (`1.0, 0.70, 0.13`) — **différente de notre
propre sommet sélectionné** (255, 140, 13). **Blender emploie UNE SEULE couleur de sélection pour
les deux.** Nous en avons deux, sans qu'aucune ligne n'explique pourquoi.

⚠️ **ET LE DÉGRADÉ (053427).** Quand un seul sommet est sélectionné, Blender **dégrade** la couleur
le long des arêtes qui y touchent — orange au sommet, noir à l'autre bout. **Notre arête est
binaire.** C'est un écart de comportement, pas seulement de teinte ; **je le rapporte, je ne le
traite pas d'office.**

### 2.3 Remplissage de face sélectionnée

Mesuré sur **deux captures qui partagent la caméra** (053421 non sélectionné, 053416 sélectionné),
donc sur **les mêmes pixels** :

```
face gauche : gris (130,132,132) -> teinté (165,139,120)
face avant  : gris (114,114,115) -> teinté (155,126,104)
dessus      : gris (139,141,142) -> teinté (170,145,129)
```

⚠️ **ON NE PEUT PAS EN DÉDUIRE UN ALPHA, ET C'EST UN RÉSULTAT.** En résolvant
`teinté = a·C + (1−a)·gris` avec `C = (255,121,0)` sur deux points, on obtient des alphas **par
canal incompatibles** :

```
rouge 0,40     vert 0,30     bleu 0,07
```

Trois valeurs pour un seul alpha : **le mélange n'est donc PAS un alpha simple en espace sRGB.**
Blender mélange très probablement **en espace linéaire** puis encode. **Recopier « alpha = 0,36 »
comme nous le faisons ne reproduira pas cette teinte**, quel que soit le chiffre choisi, tant que
l'espace de mélange diffère.

> **C'est la même famille que « OpenGL n'encode pas en sRGB » :** une couleur translucide ne se
> transporte pas d'un moteur à l'autre sans son espace de mélange. **Ce qu'il faut retenir n'est
> pas un nombre, c'est que la teinte doit être réglée À L'ŒIL SUR NOTRE RENDU, pas recopiée.**

### 2.4 Contour d'objet (mode Objet, 053409)

**Orange vif sur la SILHOUETTE seulement** — pas d'arête interne, pas de teinte de surface.

---

## 3. ⚠️ LA RÈGLE D'OCCULTATION — le négatif, enfin OBSERVÉ

**053421**, cube en mode Sommet, rien de sélectionné : je compte **7 points sur 8**.

**Le huitième sommet — celui qui est DERRIÈRE le cube — n'apparaît pas.**

> **Blender cache les sommets occultés, et rayon X éteint c'est le comportement attendu.** C'est
> exactement le négatif posé par le coordinateur, et il n'est plus supposé : il est **observé dans
> la référence**.

**Et les sept visibles sont ENTIERS** — aucun n'est coupé en deux par la surface qui le porte.
C'est précisément ce que notre rendu fait et qui a valu la remarque de Rodolf.

**Donc le correctif doit obtenir les DEUX propriétés à la fois**, et l'une sans l'autre est un
échec :

| | attendu |
|---|---|
| **(B1)** | un sommet **visible** est dessiné **entier**, pas rogné par sa propre surface |
| **(B2)** | un sommet **occulté** n'est **pas** dessiné du tout |

Un biais qui règle (B1) en faisant traverser la géométrie casse (B2), et l'utilisateur
sélectionnerait des sommets qu'il ne voit pas.

---

## 4. CE QUI RESTE À VÉRIFIER DANS CES CAPTURES

- **le centre de face** : CONFIRMÉ au §5 sur deux captures — Blender n'en dessine aucun.
- **la taille exacte en px** dépend du facteur d'échelle de l'interface de Rodolf ; les 4 px mesurés
  valent **pour cette capture**, et c'est un ordre de grandeur, pas une constante à graver.

---

## 5. LES DEUX DERNIÈRES CAPTURES — et elles ferment deux questions

| capture | mode | sous-mode | sélection | ce qu'on voit |
|---|---|---|---|---|
| **053447** | Édition | **Arête** | une arête | l'arête du dessus est **blanche/jaune vif** sur toute sa longueur. **AUCUN point de sommet** n'est dessiné |
| **053454** | Édition | **Face** | **une face** | la face avant est **teintée orange translucide**, son **contour est BLANC** (face active). **AUCUN point au barycentre** |
| **054016** | Objet | — | l'objet | **X n'ouvre PAS une liste** : une petite boîte **« Delete selected objects? »**, deux boutons côte à côte, **Delete** (bleu, mis en avant) et **Cancel** |

### ⚠️ DEUX ÉCARTS FERMES, ET ILS SONT STRUCTURELS

**(a) Blender ne dessine AUCUN point au centre des faces.** Ni en 053416 (toutes sélectionnées),
ni en 053454 (une seule). Une face se signale par **sa teinte** et, si elle est active, par **son
contour blanc** — jamais par un point. **Nous dessinons un carré au barycentre de chaque face**
(`kFaceDemi`, `kFaceDemiSel`, `kFaceDemiActif`). C'est une invention maison.

**(b) Blender ne dessine les marqueurs de sommet QUE dans le sous-mode Sommet.** En sous-mode
Arête (053447) comme en sous-mode Face (053416, 053454), **aucun point**. Notre masque
`editSelMask` fait déjà ça pour les sommets ; mais notre **point de centre de face** apparaît, lui,
en sous-mode Face — et il ne devrait pas exister du tout.

> **La face active se distingue par un CONTOUR BLANC, pas par un point.** C'est la règle que
> Blender applique, et c'est ce qu'il faudrait reproduire. Nous avons déjà un contour blanc pour la
> face active **en plus** du point : il suffirait de **retirer le point**.

---

## 6. LE MENU `X`, ENTRÉE PAR ENTRÉE

### Mode ÉDITION (054024) — Blender : **11 entrées, 4 groupes**

| # | Blender | chez nous (`NkModelerDeleteMenu.h`) | |
|---|---|---|---|
| 1 | Vertices | `Sommets` | ✅ |
| 2 | Edges | `Aretes` | ✅ |
| 3 | Faces | `Faces` | ✅ |
| 4 | Only Edges & Faces | `Seulement aretes et faces` | ✅ |
| 5 | Only Faces | `Seulement les faces` | ✅ |
| | *— séparateur —* | | |
| 6 | Dissolve Vertices | `Dissoudre les sommets` | ✅ |
| 7 | Dissolve Edges | `Dissoudre les aretes` | ✅ |
| 8 | Dissolve Faces | `Dissoudre les faces` | ✅ |
| | *— séparateur —* | | |
| 9 | Limited Dissolve | `Dissolution limitee` | ✅ |
| | *— séparateur —* | | |
| 10 | Collapse Edges & Faces | `Effondrer les aretes` | ✅ |
| 11 | **Edge Loops** | **— ABSENT —** | ❌ |

**10 sur 11.** Il manque **`Edge Loops`** — la suppression d'une boucle d'arêtes entière.

⚠️ **Et deux écarts de FORME, pas de contenu** :
- **nos entrées n'ont pas de séparateurs** : les quatre groupes de Blender sont aplatis en une
  liste unique de dix. Un menu sans groupe demande de lire les dix libellés pour en trouver un ;
- `Effondrer les aretes` traduit `Collapse Edges & **Faces**` : **le libellé perd les faces**. Soit
  l'opération ne les traite pas — et c'est un écart de comportement —, soit le nom ment.

### Mode OBJET (054016) — **ce n'est pas un menu**

**X en mode Objet ouvre une CONFIRMATION**, pas une liste : *« Delete selected objects? »*, avec
**Delete** mis en avant et **Cancel**. Deux comportements distincts pour la même touche, selon le
mode.

⚠️ **À VÉRIFIER CHEZ NOUS, ET JE N'AI PAS PU LE FAIRE** (pas de fenêtre dans le temps restant) :
notre `X` en mode Objet ouvre-t-il une confirmation, la même liste, ou rien ? **C'est un constat à
rapporter à Rodolf**, pas un lot décidé d'office.

---

## 7. LA BARRE D'OUTILS DE GAUCHE CHANGE AVEC LE MODE

Relevé par Rodolf, et confirmé sur les captures — **compté sur l'image, pas supposé** :

| mode | outils visibles dans la colonne de gauche |
|---|---|
| **Objet** (053400, 053409) | **9** icônes |
| **Édition** (053421, 053447…) | **21** icônes |

La colonne du mode Édition ajoute tout le bloc de modélisation (extruder, insérer, biseauter,
couteau, poly build, spin, lisser, aimanter…) sous les outils communs de transformation.

> **Ce n'est pas un lot pour maintenant.** C'est un constat à ne pas perdre : **la barre d'outils
> est une propriété DU MODE**, comme le sont déjà chez nous les sous-modes et les espaces. Le jour
> où on l'implémente, la question n'est pas « quels outils mettre » mais « de quel mode cette
> liste dépend-elle », et la réponse est déjà écrite dans notre mémoire : *les espaces sont les
> modes*.

---

## 8. ⚠️ LE DÉFAUT EST ENFIN CHIFFRÉ — et par un verdict DISCRET

Le code publie l'adresse écran de chaque marqueur tracé (`[MARQPOS]`). On ne cherche donc plus un
marqueur dans une image : **on va voir à une adresse connue**. Les adresses viennent **du code**,
pas d'une recherche dans l'image — aucune circularité.

Course sur `renderdemo`, sous-mode Sommet, tout sélectionné, caméra figée. **12 marqueurs tracés,
à 7 positions distinctes à l'écran** — exactement **les 7 coins visibles d'un cube**, le huitième
étant occulté. *(12 et non 7 parce que notre sommet EST un coin : un cube en a 24, et trois coins
se superposent à chaque position.)*

**Attendu dérivé** : un cœur de `2 × 1,8 = 3,6 px` de côté vaut environ **13 px**. On exige donc
**≥ 9 px** pour dire « entier ».

```
rayon X ALLUMÉ   7 ENTIERS sur 7      (27, 15, 35, 44, 20, 19, 31 px)
rayon X ÉTEINT   5 ENTIERS sur 7      (0, 7, 30, 42, 14, 22, 41 px)
                                       ^  ^
                                       |  +-- ROGNÉ  (7 px sur ~13)
                                       +----- ABSENT (0 px : avalé entier)
```

> **Deux marqueurs sur sept sont abîmés quand le rayon X est éteint : un rogné, un entièrement
> avalé par la surface.** C'est ça, « pas la même forme ».

### L'ATTENDU DU CORRECTIF, ÉCRIT AVANT DE L'ÉCRIRE

| | attendu | d'où il vient |
|---|---|---|
| **(B1)** | rayon X éteint : **7 ENTIERS sur 7** | Blender montre ses 7 coins visibles **entiers** (053421). Aujourd'hui : **5** |
| **(B2)** | **le nombre d'adresses tracées reste 7, pas 8** | le coin occulté ne doit **pas** apparaître. Un biais qui le ferait surgir aurait désactivé le test de profondeur au lieu de décaler — et l'utilisateur sélectionnerait un sommet qu'il ne voit pas |
| **(B3)** | rayon X allumé : **toujours 7 sur 7** | le correctif ne doit rien casser de ce qui marchait |
| **(B4)** | les adresses **ne bougent pas** de plus de **1 px** | on décale le long du **rayon de vue** : la projection écran ne doit pas changer. Si elle change, le décalage est dans la mauvaise direction |

⚠️ **(B1) et (B2) s'opposent, et c'est tout le sujet.** Obtenir l'un en perdant l'autre n'est pas un
correctif. **Les deux se mesurent dans la même course**, ce qui interdit de n'en regarder qu'un.
