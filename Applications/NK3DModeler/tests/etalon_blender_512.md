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

- **le centre de face** : ⚠️ **ma conclusion était FAUSSE, voir le §9.**
- **la taille exacte en px** dépend du facteur d'échelle de l'interface de Rodolf ; les 4 px mesurés
  valent **pour cette capture**, et c'est un ordre de grandeur, pas une constante à graver.

---

## 5. LES DEUX DERNIÈRES CAPTURES — et elles ferment deux questions

| capture | mode | sous-mode | sélection | ce qu'on voit |
|---|---|---|---|---|
| **053447** | Édition | **Arête** | une arête | l'arête du dessus est **blanche/jaune vif** sur toute sa longueur. **AUCUN point de sommet** n'est dessiné |
| **053454** | Édition | **Face** | **une face** | la face avant est **teintée orange translucide**, son **contour est BLANC** (face active). *(Aucun point au barycentre **dans cette condition** — voir le §9, ce n'est PAS une absence générale)* |
| **054016** | Objet | — | l'objet | **X n'ouvre PAS une liste** : une petite boîte **« Delete selected objects? »**, deux boutons côte à côte, **Delete** (bleu, mis en avant) et **Cancel** |

### ⚠️ DEUX ÉCARTS FERMES, ET ILS SONT STRUCTURELS

**(a)** ⚠️ **CE QUI ETAIT ECRIT ICI ETAIT FAUX. Voir le §9.** J'y affirmais que Blender ne dessine
aucun point au centre des faces, et j'en tirais qu'il fallait retirer le nôtre. **Rodolf :
« Blender le fait aussi. »** Mon absence était **déduite de deux captures**, donc d'**une seule
condition d'affichage**.

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
| 11 | Edge Loops | `Boucles d'aretes` — **présent, grisé, avec son motif** | ✅ *(voir §11)* |

⚠️ **CE QUI ÉTAIT ÉCRIT ICI — « 10 sur 11, il manque Edge Loops » — ÉTAIT FAUX. Voir le §11.**

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

---

## 9. ⚠️ CORRECTION — « Blender ne dessine aucun point au centre des faces » ÉTAIT FAUX

**Rodolf : « Blender le fait aussi. »**

### Ce que j'avais fait, et pourquoi c'est une faute nommée

J'ai observé **deux captures** en sous-mode Face — l'une tout sélectionné (053416), l'autre une
seule face (053454) — je n'y ai vu **aucun point au barycentre**, et j'ai conclu **« Blender n'en
dessine aucun »**. J'ai même listé **« retrait du point de centre de face »** dans le travail
restant.

> **Deux captures ne montrent qu'UNE condition d'affichage.** Chez Blender, ces points dépendent du
> **sous-mode**, du **rayon X**, et de l'option de surimpression **« centres de face »** — qui peut
> être décochée. Une absence observée dans une condition n'est pas une absence.

**C'est exactement la faute que ce dépôt a déjà nommée** : *déduire une absence de l'absence d'un
appel*. Je l'ai refaite sur une image au lieu d'un appel.

⚠️ **ET ELLE ALLAIT COÛTER UNE FONCTION JUSTE.** Le lot que j'avais écrit était « retirer le
point ». **NE RIEN RETIRER.** Notre centre de face est **correct** ; ce qui manque, c'est de savoir
**dans quelles conditions** il doit paraître.

### Ce qu'il faut faire à la place

1. **relever les CONDITIONS** dans lesquelles Blender affiche les centres de face — sous-mode,
   rayon X, option de surimpression ;
2. **prélever leur taille et leur couleur** dans une capture où ils sont visibles ;
3. **reproduire les conditions**, pas seulement l'apparence.

**Capture manquante, et je la demande** : *sous-mode Face, avec et sans rayon X*, sur le cube par
défaut. Sans elle, je ne peux ni mesurer la couleur, ni la taille, ni la condition — et je ne
remplacerai pas cette mesure par une supposition.

### La leçon, pour la prochaine fois

> **Une absence ne se conclut jamais d'un échantillon.** Pour affirmer « X n'existe pas », il faut
> soit **épuiser les conditions**, soit **trouver dans le produit la ligne qui ne le dessine
> jamais**. Une image montre ce qui est ; elle ne montre pas ce qui n'aurait pas pu être.

---

## 10. ⚠️ LE « SEPTIÈME MARQUEUR » N'ÉTAIT PAS UN DÉFAUT — et mon attendu (B1) était FAUX

J'avais écrit : *rayon X éteint, 6 marqueurs entiers sur 7 ; le septième reste à 0 px, cause
inconnue.* Et j'en avais fait l'attendu **(B1) : 7 entiers sur 7**.

**J'ai regardé ce qu'il y a À CETTE ADRESSE, au lieu de supposer.**

```
adresse (636,404) : couleur alentour (124, 121, 112)   -- gris CLAIR
les six autres    : (46,43,39) (65,62,54) (58,52,33) (53,51,70) (71,68,64) (54,112,59)
```

**Le cube de renderdemo est un olive sombre (~55). La couleur à cette adresse est un gris clair à
124.** L'agrandissement le confirme sans ambiguïté : **une sphère de la scène se trouve DEVANT le
coin bas du cube.**

> **Le marqueur est donc caché par UN AUTRE OBJET, et c'est le comportement JUSTE.** Blender le
> cacherait aussi. Il n'y avait rien à corriger.

### Ce que ça change

**Mon attendu (B1) était mal formé** : il exigeait *« 7 entiers sur 7 »* en supposant que **le seul
occulteur possible était le cube lui-même**. La scène de renderdemo est peuplée — sphères, sol,
colonnes — et **une occultation par un tiers est un succès, pas un échec**.

**(B1) se réécrit** :

> **tout marqueur qui n'est occulté par aucune géométrie est dessiné ENTIER.**

Et avec cette formulation, **le correctif est complet** : les 6 marqueurs non occultés sont entiers
(14 à 42 px de cœur), et le septième est légitimement caché.

⚠️ **CE QUI M'A PRESQUE FAIT CONTINUER À CHERCHER.** J'avais déjà mesuré que **tripler le décalage
ne changeait rien**, et j'en avais tiré — correctement — que *« ce n'est pas un problème de
profondeur »*. Mais j'ai laissé la conclusion à *« cause inconnue »* au lieu d'aller regarder
l'image. **La réponse était visible à l'œil depuis le début.**

> **Quand un compteur dit « absent », la question suivante n'est pas « pourquoi le code ne le
> dessine-t-il pas » mais « qu'y a-t-il à cet endroit ». Une adresse, c'est fait pour aller voir.**

### L'attendu corrigé, pour la sonde

| | attendu |
|---|---|
| **(B1)** | tout marqueur **non occulté** est entier — **6 sur 6 aujourd'hui** ✅ |
| **(B1bis)** | ⚠️ **un marqueur occulté par un tiers reste caché** — c'est un **succès**. Une sonde qui exigerait 7/7 dans cette scène **rougirait sur du juste** |
| **(B2)** | le nombre d'adresses tracées reste **7**, pas 8 ✅ |
| **(B3)** | rayon X allumé : **7 sur 7** ✅ |
| **(B4)** | les adresses ne bougent pas de plus d'1 px ✅ |

---

## 11. ⚠️ SECONDE CORRECTION — « Edge Loops absent » ÉTAIT FAUX, ET POUR LA MÊME RAISON

J'avais écrit **« 10 entrées sur 11, il manque `Edge Loops` »**. **C'est faux.**

`NkModelerDeleteMenu.h` porte **les onze**, et `Edge Loops` s'y appelle **« Boucles d'aretes »**.
Mieux : les deux dernières sont **présentes, grisées, avec un motif nommé** —
*« pas encore ecrit : retirer une boucle d'aretes en recousant les faces »*. C'est **meilleur** que
ce que j'avais rapporté : l'entrée existe et **dit pourquoi** elle n'agit pas.

### D'où venait l'erreur

D'un `grep ... | head -25`. **La liste était tronquée à la vingt-cinquième ligne**, et les deux
dernières entrées tombaient juste après. **J'ai lu une troncature et j'en ai conclu une absence.**

> **C'est la DEUXIÈME fois en une soirée** : j'avais conclu « Blender ne dessine pas de centre de
> face » de deux captures (§9), et « `Edge Loops` n'existe pas » d'un `head -25`. **Deux absences
> déduites d'une vue partielle.**
>
> **Une absence ne se lit jamais dans un extrait.** Elle se prouve en **comptant** (`grep -c`, la
> taille du tableau) ou en **lisant la source entière**. Et quand elle porte sur du travail à faire,
> elle coûte double : on ajoute ce qui existe déjà, ou on retire ce qui est juste.

### Ce qui restait VRAI, et qui est maintenant LIVRÉ

**Les séparateurs manquaient bel et bien.** Le kit sait les dessiner — `NkCtxMenuDraw` a un
paramètre **`sepAfter`** — et nous lui passions `nullptr`. Les onze entrées s'affichaient en une
liste plate.

**Livré** : chaque entrée porte son **numéro de groupe**, et le trait s'en **dérive**.

⚠️ **Le groupe, pas le trait.** Stocker un booléen « trait après » aurait paru plus simple ; mais
insérer une entrée au milieu d'un groupe aurait alors **déplacé le trait d'une ligne sans que
personne le remarque**. Le groupe est l'information ; le trait en est la conséquence.
Et jamais de trait après la dernière : **un trait en bas de menu sépare le menu de rien**.

### Et le libellé tronqué, lui, était bien un défaut

`Collapse Edges & **Faces**` était traduit **« Effondrer les aretes »** — les faces disparaissaient
du nom. Corrigé en **« Effondrer les aretes et les faces »**. *(L'opération est de toute façon
« pas encore ecrit », mais un nom qui ment survit à son implémentation.)*

---

## 12. LE RÉSULTAT DU CORRECTIF, MESURÉ APRÈS L'ALIGNEMENT DES COULEURS

```
survie des marqueurs, rayon X éteint, dans le MODELEUR
   avant le décalage : 10 / 30  =  33 %
   après             : 27 / 34  =  79 %  (DirectX 11)
                       30 / 36  =  83 %  (OpenGL)
```

### Le plafond, dérivé — et il n'est pas 100 %

Rayon X **allumé**, les **24** sommets du cube sont dessinés, à **8** positions distinctes (notre
sommet EST un coin : trois se superposent à chaque coin). Rayon X **éteint**, le filtre
d'orientation n'en garde que **12**, à **7** positions.

**Le compte de pixels suit les POSITIONS, pas les sommets** : le plafond est donc **7/8 ≈ 87 %**,
pas 100 %.

> **Avant, nous étions à 38 % du plafond. Nous sommes à 93 %.**

⚠️ **Et je corrige ici mon propre raisonnement** : j'avais écrit que le plafond était **50 %**, en
comptant les **sommets** (12 sur 24) au lieu des **positions** (7 sur 8). Trois marqueurs
superposés ne couvrent pas trois fois plus de pixels qu'un seul. **Le plafond de 50 % était faux, et
il aurait fait passer 79 % pour un dépassement inexplicable.**

### ⚠️ ET L'INSTRUMENT EST REDESCENDU SOUS SON PLANCHER DE BRUIT, CÔTÉ ÉTALON

```
étalon : éteint 59 px / allumé 21 px  ->  281 %     (impossible)
(F) deux courses : allumé 21 puis 21  ·  ÉTEINT 59 puis 52   -> ROUGE
```

**281 % est impossible**, et (F) n'est plus vert. La cause est identifiable : la teinte de face
étant passée de α 0,36 à **0,03**, les comptes de l'étalon sont tombés de ~123 px à ~21 px —
**à quelques dizaines de pixels, une différence d'anticrénelage suffit à faire bouger le total**.

> **L'instrument n'est pas faux : il est devenu trop grossier pour ce qu'il mesure maintenant.**
> Ce n'est pas une régression du produit — le produit a rendu la teinte plus discrète, ce qui était
> l'objectif.

**Ce qui le réparerait, et c'est déjà écrit dans le banc** : la sonde doit **se calibrer sur
l'image** — prendre la couleur **aux adresses publiées par `[MARQPOS]`** dans la course rayon X
allumé, et s'en servir pour juger la course éteinte. Elle serait alors immunisée contre **tout**
changement de teinte, au lieu d'être recalibrée à chaque fois.

⚠️ **Je ne publie donc AUCUN chiffre de l'étalon issu de cette course.** Les chiffres du modeleur,
eux, tiennent : ils viennent d'un cadrage où les comptes restent à 30-36 px, au-dessus du plancher.

---

## 13. ⚠️ LE `X` DU MODE OBJET — **CE QUI SUIT ÉTAIT FAUX. Voir le §15.**

**Point 4 du lot. Vérifié dans la source, pas supposé.**

| | Blender (054016) | nous |
|---|---|---|
| `X` en mode Objet | une **confirmation** : *« Delete selected objects? »*, **Delete** (bleu, mis en avant) et **Cancel** | ⚠️ **une confirmation AUSSI** — mon « rien ne se passe » était faux, voir §15 |

**Ce que la lecture montre, avec les adresses** :

- `NkDemo3D.cpp:6889` — la touche `X` est traitée **à l'intérieur de `if (st->editMode)`**
  (ligne 6629). En mode Objet, **elle n'est jamais lue** ;
- `NkModelerViewport.h:2081` — le menu ne s'affiche que sous `editMode` ;
- `main.cpp:289` — **`objet.supprimer` EST pourtant lié à `X`** dans la table des raccourcis, et
  `NkModelerBrowser.h:1183` l'offre dans un menu contextuel ;
- **aucun site ne le dispatche depuis la touche.**

> **« Supprimer » est DÉCLARÉ dans la table des raccourcis du mode Objet et n'est LIVRÉ nulle
> part.** C'est la famille que ce dépôt nomme *déclarer n'est pas livrer* — et ici elle est
> visible par l'utilisateur : il lit le raccourci dans la table, il appuie, **rien**.

⚠️ **Ce qu'il ne faut PAS faire**, et c'est pour ça que je m'arrête là : **brancher `X` sur une
suppression directe**. Blender **demande confirmation** en mode Objet, et pour une raison qui se
comprend — en mode Édition on supprime une poignée de sommets qu'on peut annuler sans y penser ; en
mode Objet on supprime **un objet entier**. **Les deux comportements diffèrent volontairement.**

**Le lot, s'il est décidé** : une **confirmation** en mode Objet, pas une liste, avec le bouton de
suppression **mis en avant** et une annulation. **C'est un constat à rapporter à Rodolf**, pas une
décision que je prends seul.

---

## 14. LE CORRECTIF SUR LES QUATRE DORSAUX DE WINDOWS

Question de Rodolf : *« et sur Vulkan, DX12 et Metal ? »* — le chiffre publié valait pour **deux**
dorsaux sur cinq. `sonde_marqueurs_dorsaux.ps1`, même scène, même critère, caméra figée.

```
dorsal     retenu          allume   eteint   survie
opengl     OpenGL              36       30    83,3 %
vulkan     Vulkan              36       30    83,3 %
dx11       DirectX 11          34       27    79,4 %
dx12       DirectX 12          34       27    79,4 %
```

⚠️ **Le dorsal retenu est LU DANS LE JOURNAL, pas supposé.** Demander `vulkan` et obtenir DX11 par
un repli silencieux rendrait deux colonnes identiques et « prouverait » un accord qui n'existe pas.
Les quatre lignes confirment que c'est bien le dorsal demandé qui a tourné.

### L'écart de 3,9 points N'EST PAS une différence de rendu

Il était tentant de conclure « les API DirectX rognent un peu plus ». **C'est faux, et deux mesures
le montrent.**

**(a) La couleur du marqueur est IDENTIQUE sur les quatre** : **(231, 129, 0)**. L'hypothèse de
l'encodage — celle qui a déjà servi ici — est donc **écartée**.

**(b) Sur les GRANDS comptes, les quatre s'accordent à 0,2 point près.** En retirant la condition
de voisinage, le critère compte ~1 000 pixels au lieu de 30 :

```
             couleur seule        rapport
opengl       243 / 978            24,8 %
vulkan       251 / 1019           24,6 %
dx11         245 / 987            24,8 %
dx12         245 / 987            24,8 %
```

> **Le critère « avec voisinage » compte 27 à 36 éléments. Sur un compteur à 30, UN pixel vaut 3,3
> points.** Annoncer une différence de 3,9 points entre dorsaux sur ce compteur, c'est **prétendre
> voir un pixel** — et les 3,9 points disparaissent dès qu'on mesure sur mille.

**Issue retenue : (i), les quatre s'accordent.** C'est ce qu'on attend d'un décalage **calculé
avant l'envoi au dorsal** — une position monde, pas un état de pipeline. ⚠️ **Et l'accord ne prouve
pas le correctif** : il prouve que rien ne le contredit selon l'API. Ce qui le prouve, c'est le
passage de **33 % à ~80 %**.

### ⚠️ METAL : NON MESURABLE ICI, et déclaré comme tel

Metal n'existe que sur les plateformes Apple. Le kit le **refuse** sur Windows **avec son motif**
plutôt que de lancer autre chose en silence (`NkEditorGfxApiSupported` :
*« lancer une autre API a sa place serait un repli silencieux »*).

**Condition de réouverture** : une course sur **macOS**, par les **Actions GitHub** — le chemin que
le dépôt a déjà retenu pour cette plateforme.

**Le chiffre couvre donc QUATRE dorsaux sur cinq, et la sonde l'écrit à l'écran** pour qu'aucun
lecteur ne croie qu'il en couvre cinq.

---

## 15. ⚠️ TROISIÈME CORRECTION — « le `X` du mode Objet ne fait rien » ÉTAIT FAUX, et il est MESURÉ

Le §13 annonçait que `X` en mode Objet **ne faisait rien** et que « Supprimer » était *déclaré et
non livré*. **Les deux affirmations sont fausses**, et cette fois c'est une **mesure** qui le dit.

### La mesure, par un crochet qui emprunte le chemin de la touche

`NK_OBJ_SUPPR=<image>` pose le **même** `delK` que la touche — aucune injection clavier.

```
[nk3d] OBJ SUPPR : touche lue, cibles=1, confirmation=1
```

**Et la confirmation est PEINTE**, vérifiée sur la capture de la fenêtre de sonde :

> **« Supprimer "Cube.003" ? »** — bouton **Supprimer** (bleu, mis en avant) et **Annuler (Echap)**.

⚠️ **Elle NOMME l'objet.** Blender écrit *« Delete selected objects? »* ; nous écrivons le nom.
**C'est mieux, et il faut le garder.**

### Ce que le §13 avait manqué, et pourquoi

J'avais lu **`NkDemo3D.cpp`**, vu la touche `X` traitée à l'intérieur de `if (st->editMode)`, et
conclu qu'elle n'existait pas ailleurs. **Le raccourci vit dans un AUTRE fichier** —
`NkModelerHierarchy.h`, fonction `PaintSceneMenus` — et son commentaire dit même :
*« valables aussi la souris sur la vue 3D »*.

> **TROISIÈME absence conclue d'une vue partielle dans la même journée**, après le centre de face
> (deux captures) et `Edge Loops` (un `head -25`). **Celle-ci aurait coûté le plus cher** : on
> allait écrire une seconde porte pour une fonction qui en avait déjà une — exactement ce que le
> dépôt appelle *deux chemins pour un geste*.

### Ce qui est DÉJÀ conforme, point par point

| demandé | état |
|---|---|
| `X` branché en mode Objet | ✅ **mesuré** |
| une **confirmation**, pas une liste | ✅ **peinte**, et elle nomme l'objet |
| l'action **mise en avant** | ✅ fond d'accent sur « Supprimer » |
| **le zéro** : sans sélection, rien ne s'ouvre | ✅ **dans le code** (`if (st.delNodeCount > 0)`) — ⚠️ **non mesuré** : je n'ai pas de crochet pour vider la sélection, et je ne l'annonce donc pas comme prouvé |
| une **porte, pas un état armé** | ✅ le dialogue est **redéclaré à chaque image** dans `PaintSceneMenus` ; il ne peut pas rester armé sans être rendu |
| Échap annule | ✅ |

### ⚠️ CE QUI MANQUE VRAIMENT : L'ANNULATION

```cpp
void Demo3DHostDeleteNode(int32 node, bool withChildren) {
    nkvpDeleted[node] = true;
    if (node >= kNkvpFirstUser)
        nkvpUserKind[node - kNkvpFirstUser] = 0; // slot recyclable
    ...
}
```

**Aucune pile d'annulation n'est touchée. Supprimer un objet ne se défait pas.**

⚠️ **Et ce n'est pas un simple `Ctrl+Z` à brancher** : le slot est **immédiatement marqué
recyclable**. Une création ultérieure peut le reprendre — donc restaurer l'objet demande de
**retenir son contenu** (nature, transformation, maillage, parent, enfants), pas seulement de
lever un drapeau. **C'est le vrai lot**, et il est plus gros que « brancher la touche ».

**C'est le seul des cinq points demandés qui reste à faire.**
