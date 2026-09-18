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

*(053447 et 053454 restent à lire ; 054016 est le menu `X` du mode Objet.)*

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

- **053447** et **053454** : pas encore lues.
- **le centre de face** : en sous-mode Face (053416), je ne distingue **aucun point au barycentre**.
  Nous en dessinons un. À confirmer sur une capture non sélectionnée en sous-mode Face.
- **la taille exacte en px** dépend du facteur d'échelle de l'interface de Rodolf ; les 4 px mesurés
  valent **pour cette capture**, et c'est un ordre de grandeur, pas une constante à graver.
