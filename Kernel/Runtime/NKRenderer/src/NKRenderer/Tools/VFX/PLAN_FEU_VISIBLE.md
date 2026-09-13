<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN — QUE LE FEU SE VOIE, DANS UNE FENÊTRE

**Écrit le 2026-09-13 après-midi, AVANT d'écrire le moindre chemin de rendu.**

Ce plan répond à la phrase de Rodolf devant les huit démos :

> *« le feu là, on n'a pas l'impression que c'est le feu, mais j'imagine que les
> paramètres vont nous dissuader du contraire. »*

**Son impression est juste, et aucun paramètre ne l'aurait changée** : ce qu'il a
regardé est `NK_VFX_PROBE`, une **fontaine de particules** qui n'utilise pas le
solveur de fluide. Le feu simulé — combustion, corps noir, confinement, grille MAC,
advection en flux — **n'a aucun hôte fenêtré**. Il ne produit que des PNG par
marche de rayon CPU.

---

## (q1) L'INVENTAIRE — ce qui existe déjà, lu dans le code

### Chemin ① — marche de rayon CPU vers une texture, mise à jour par image

**Ce qui existe : TOUT.** `NkFluidRaymarchRender` (Kajiya & Von Herzen 1984) rend
déjà le volume en RGBA8, sans GPU ni fenêtre. C'est lui qui produit les quatre
captures versionnées.

**Ce qu'il resterait à écrire** : créer une texture 2D RHI, y téléverser le buffer
à chaque image, et la dessiner en plein écran. Rien de conceptuel.

**Ses réglages qui gouvernent le coût, et ils existent déjà** :
`shadowMarch` (vraie par défaut), `shadowStepFactor`, `stepSize`, `width/height`.

### Chemin ② — marche de rayon GPU, en NkSL

**Ce qui existe, et c'est plus que je ne croyais :**

- **NKRHI sait créer une texture 3D** : `NkTextureDesc::Tex3D(w, h, d, fmt)`
  (`NkDescs.h:124`), avec l'implémentation DX11 (`CreateTexture3D`, SRV et UAV) et
  DX12 (`Texture3D.MipLevels`, UAV `WSize`) ;
- **NkSL sait déclarer et échantillonner une texture 3D** : le générateur GLSL émet
  `sampler3D` (`NkSLCodeGenGLSL.cpp:99`), le générateur HLSL émet `Texture3D` et
  `RWTexture3D<float4>` (`NkSLCodeGenHLSL.cpp:137, 186`).

**Ce qu'il resterait à écrire** : le nuanceur de marche de rayon lui-même, le
téléversement du champ de densité/température dans la texture 3D à chaque image, et
la passe plein écran. **C'est du travail réel, mais aucune brique ne manque.**

### Chemin ③ — particules pilotées par la grille

**Ce qui existe : TOUT aussi.** `NkVFXSystem` avec `NkEmitterDesc`, déjà câblé
derrière `NK_VFX_PROBE=1` dans `Demo3D.cpp:2621`, avec simulation CPU **ou GPU**
(`NkSimTarget`).

> ⚠️ **SON MENSONGE, ET IL DOIT ÊTRE DIT** : ce n'est **pas du volume**. Des
> particules colorées par la grille donnent une image de feu **plausible**, mais
> elles ne montrent ni l'absorption, ni la diffusion multiple, ni la profondeur
> optique. **C'est exactement ce que Rodolf regardait quand il a dit que ça ne
> ressemblait pas à du feu** — l'améliorer serait rendre le mensonge plus joli, pas
> montrer le solveur.

---

## ⚠️ LE LIEN AVEC LE LOT A, QUE JE N'AVAIS PAS VU AVANT DE LIRE LE CODE

`NkFluidRaymarchParams::emissionMinTemperature = 800 K`. **L'émission de corps noir
ne s'allume qu'au-dessus de 800 K.**

Or la bascule du lot A fait tomber le `Tmax` de la scène (e) de `1749,4` à
`571,7 K` — **sous le seuil**. Sur cette scène, après la bascule et **avant** le
re-réglage, **il n'y aurait plus aucune émission du tout** : le feu ne serait pas
« plus sombre », il serait **éteint**.

> **Le re-réglage de (p2) n'est donc pas un ajustement esthétique : c'est ce qui
> remet le panache au-dessus du seuil d'émission.** Viser 1500 K place la source
> à `1,9` fois ce seuil.

C'est aussi ce qui rend le témoin **(3.5)** à risque, comme annoncé au § 15 du
`PLAN_ORDRE_SUPERIEUR.md`.

---

## (q2) LE COÛT — ce qui est DÉJÀ mesuré, et ce qui manque

### Déjà mesuré, par la course complète du 13/09 (Release, CPU, un seul fil)

Marche de rayon **avec ombres** (`shadowMarch = true`), grille 25 × 80 × 25 :

```
 240 x 180    142,8 ms     375 575 échantillons + 1 111 754 d'ombre
 480 x 360    604,1 ms   1 501 910 échantillons + 4 440 358 d'ombre
 960 x 720   2 524,3 ms   6 008 558 échantillons + 17 760 032 d'ombre
```

⚠️ **Deux lectures du même montage donnent `518,0` et `604,1 ms`** (courses du
matin et de midi) : l'écart est la **charge de la machine**, pas le code. Un temps
de rendu sans sa machine n'est pas un nombre — je publie les deux.

**Extrapolation, à dire comme telle** : le coût est ~linéaire en pixels
(`3,5 µs/px` à 480 × 360). À **1280 × 720** (921 600 px), cela donne **~3,2 s par
image**. ⚠️ C'est une **extrapolation**, pas une mesure — elle sera mesurée.

**Et les ombres coûtent l'essentiel** : `4 440 358` échantillons d'ombre contre
`1 501 910` primaires, soit **~75 % du travail**. `shadowMarch = false` est donc le
premier levier, et il existe déjà.

### Coût de la SIMULATION, déjà mesuré

Scène (e), 25 × 50 × 25 = **31 250 cellules**, van Leer, Release, un fil :
**107,4 à 116,1 ms par pas** selon la course.

> ### ⚠️ CE QUE CES DEUX CHIFFRES DISENT DÉJÀ, ET QUI INVERSE L'AVERTISSEMENT
>
> On m'a mis en garde : *« une texture CPU mise à jour chaque image peut coûter plus
> cher que la simulation elle-même — sinon on croira que le fluide est lent alors
> que c'est le transfert. »*
>
> **Les deux sont lents, et dans cet ordre** : à 480 × 360 avec ombres, le rendu
> (`604 ms`) coûte **~5 fois** la simulation (`116 ms`). Ombres coupées, ils
> deviennent **comparables** (~150 contre ~116 ms). **Aucun des deux n'est
> négligeable, et aucune optimisation de l'un seul ne donnera une image fluide.**

### Ce qui manque encore, et sera mesuré

    (q2a) le rendu SANS ombres, aux memes trois definitions  -- le levier a 75 %
    (q2b) le cout du TELEVERSEMENT d une texture 2D RGBA8 par image, separement
    (q2c) la taille de la donnee a envoyer au GPU pour le chemin 2 :
          32 x 64 x 32 cellules x 4 octets = 256 Ko par champ et par image

⚠️ **Mesurer le rendu et le téléversement SÉPARÉMENT est une exigence, pas une
élégance** : additionnés, ils feraient accuser le solveur.

---

## (q3) LA RÈGLE DE CHOIX, écrite avant d'avoir les derniers chiffres

> **On écrit le chemin le MOINS CHER qui donne une image du VOLUME.**

Le chemin ③ est écarté **par sa définition, pas par son coût** : il ne montre pas
le volume, et c'est déjà ce que Rodolf a jugé insuffisant.

Entre ① et ② : **① si une image par marche CPU à basse définition, ombres coupées,
tient sous ~150 ms** — c'est le plus court à écrire et rien n'y manque. **② sinon**,
et il faudra dire que c'est un lot à part entière.

**Critères d'acceptation, fixés maintenant** : 120 images, code de sortie 0,
**0 `[ERR]`**, et **le volume se VOIT bouger** — mesuré par le pourcentage de pixels
qui diffèrent entre deux images, comparé à la gigue mesurée de la scène.
**Négatif** : sans `NK_FIRE_PROBE=1`, `Draw` et `Tris` **identiques**.

---

## (r) LES DEUX MESURES QUI MANQUAIENT — pré-enregistrement

**Écrit le 13/09 au soir, AVANT de coder le mode.** Ce sont les deux mesures que
**j'ai moi-même exigées** avant de recommander : *« sans ces deux-là, recommander ①
serait un pari, pas une conclusion. »*

### Ce qui est mesurable ici, et ce qui ne l'est pas

| | mesurable dans le banc ? |
|---|---|
| marche de rayon **avec** et **sans** ombres | **oui** — CPU pur |
| coût CPU de la mise en tampon d'une image RGBA8 | **oui** — c'est une recopie |
| **téléversement GPU** (map/unmap, DMA) | **NON** : le banc n'ouvre **aucun device** |

⚠️ **Je ne prétendrai donc pas mesurer le téléversement GPU.** Je mesure sa part
CPU et je **borne** l'autre par l'arithmétique du volume de données, en la disant
**borne** et non mesure. *Un banc qui n'a pas de GPU ne doit pas publier un chiffre
de GPU.*

### Les critères, écrits AVANT

    (r1) la marche SANS ombres, aux memes definitions
         les ombres pesent 4 440 358 echantillons contre 1 501 910 primaires,
         soit 74,7 % du travail -> le cout doit tomber vers 25,3 %
         PREDICTION a 480x360 : 604,1 x 0,253 = 153 ms, bande 140 a 190
    (r2) 1280x720 MESURE, pour remplacer mon extrapolation de ~3,2 s
         PREDICTION avec ombres : 3 220 ms (bande 2 800 a 3 700)
         PREDICTION sans ombres :   815 ms (bande   700 a   950)
    (r3) le cout CPU par image du tampon RGBA8 (1280x720x4 = 3 686 400 octets)
         PREDICTION : < 2 ms a TOUTE definition, donc au moins 100 fois moins
         cher que la marche -- le transfert n est PAS le probleme
    (r4) le cout de SIMULATION sur la grille du RENDU (25 x 80 x 25 = 50 000
         cellules), qui est ce qu une fenetre paierait a chaque image

### ⚠️ MA PRÉDICTION DE FOND — elle inverse le tableau une SECONDE fois

Ce matin, la mesure a inversé l'avertissement reçu : ce n'était pas le transfert
qui coûtait, c'était **le rendu**, cinq fois la simulation.

> **Je prédis que la seconde inversion est de sens opposé** : aux définitions où le
> rendu devient abordable (240 × 180 sans ombres, ~36 ms attendus), **c'est la
> SIMULATION qui domine** — ~150 à 250 ms sur 50 000 cellules, soit 4 à 7 fois le
> rendu.

**Conséquence si elle tient** : le chemin GPU (②) **ne sauverait pas l'affaire**,
puisqu'il n'accélère que le rendu. Le levier serait **la taille de la grille** ou
**une simulation GPU** — un autre lot, et il faudra le dire.

**VOLET NÉGATIF** : si le rendu sans ombres reste plus cher que la simulation à
toutes les définitions, ma prédiction est **fausse** et c'est **le rendu** qu'il
faut porter sur GPU. Les deux issues sont utiles ; une seule est vraie.

### Ce que ce lot décidera

**Si aucun chemin ne descend sous ~200 ms par image**, je le dis **avec les
chiffres** au lieu d'écrire une démo qui rame : *une vérité chiffrée vaut mieux
qu'une image saccadée.*

---

## (r) LE RÉSULTAT — ET LA DÉCISION : (q3) N'EST PAS ÉCRIT

**Mode `NK_FLUID_MAC=d`.** Grille du rendu, 25 × 80 × 25 = 50 000 cellules,
panache **établi** (255 pas), `epsilon = 8`, Release, un seul fil.

```
definition     AVEC ombres    SANS ombres   rapport   recopie RGBA8   octets
 240 x  180       251,5 ms        38,2 ms    0,152       0,309 ms     172 800
 480 x  360       958,2 ms       162,7 ms    0,170       1,102 ms     691 200
 960 x  720      3667,5 ms       669,3 ms    0,182       4,987 ms   2 764 800
1280 x  720      3969,6 ms       729,0 ms    0,184       7,868 ms   3 686 400

SIMULATION sur la MEME grille, panache etabli : 244,3 ms par pas

TEMPS D IMAGE du chemin CPU (sim + marche sans ombres + recopie) :
   240 x 180 :  282,8 ms  =  244,3 +  38,2 + 0,309   ->  3,54 images/s
   480 x 360 :  408,1 ms  =  244,3 + 162,7 + 1,102   ->  2,45 images/s
   960 x 720 :  918,6 ms  =  244,3 + 669,3 + 4,987   ->  1,09 images/s
  1280 x 720 :  981,2 ms  =  244,3 + 729,0 + 7,868   ->  1,02 images/s
```

### ⚠️ MON INSTRUMENT ÉTAIT FAUX, ET C'EST LA PREMIÈRE CHOSE À DIRE

Premier jet de (r3) : la recopie mesurée à **42,686 ms** pour 3,7 Mo, soit
**86 Mo/s**. J'écrivais la boucle avec `dst[k] = img[k]`, donc **par l'accesseur de
`NkVector`** : *je ne mesurais pas un transfert, je mesurais mon propre accesseur.*
Corrigé par pointeurs bruts — ce que fait un vrai téléversement : **7,868 ms**,
soit **5,4 fois moins**.

⚠️ **Et même corrigé, il reste pessimiste** : 469 Mo/s pour une boucle octet par
octet, là où un `memcpy` réel atteint plusieurs Go/s. **Le vrai rapport est donc
AU-DESSUS de celui que je publie.**

### (r3) est ROUGE, et le seuil n'est PAS déplacé

`93×` mesuré contre **`100×`** exigé. Il échoue **de 7 %**. Je ne le bouge pas.
Mais la conclusion qu'il testait — *le transfert n'est pas le problème* — **tient
quand même**, et pour une raison qui se dit : le rouge vient de ce que **mon
instrument surestime encore la recopie**, pas de ce que le transfert coûterait.

### (r4) VERT — et c'est le chiffre qui décide

**Rapport simulation / marche à 240 × 180 : `6,40`.** Ma prédiction de fond
annonçait **4 à 7** : elle tient.

> **Porter le rendu sur GPU ne sauverait pas l'affaire.** Si la marche devenait
> **gratuite**, le temps d'image passerait de `282,8` à `244,6 ms` — de **3,54 à
> 4,09 images/s**, soit **+15 %**. On optimiserait ce qui ne coûte pas.

### ⚠️ UNE PRÉDICTION JUSTE POUR DEUX RAISONS FAUSSES QUI SE COMPENSENT

(r1) à 480 × 360 : prédit **153 ms**, bande 140-190, mesuré **162,7** — dans la
bande. **Mais le calcul était faux deux fois** : je partais d'une base de
`604,1 ms` (c'est `958,2`) et d'une fraction d'ombres de `0,253` (c'est `0,170`).
`604,1 × 0,253 = 153` et `958,2 × 0,170 = 163` : **les deux erreurs se
compensent**. *Une prédiction juste par compensation n'est pas une prédiction
validée*, et le dire vaut mieux que d'encaisser le point.

### Ce que la course complète permet d'ISOLER, et que ce mode seul ne pouvait pas

À `epsilon = 0`, même scène, **avant** et **après** la bascule conservative :

```
480 x 360 avec ombres    604,1 ms  ->  495,5 ms     (-18 %)
echantillons d ombre    4 440 358  ->  2 569 360    (-42 %)
echantillons primaires  1 501 910  ->  1 511 118    (inchanges)
```

**La bascule a rendu le rendu MOINS cher**, pas plus : le panache est plus lisse,
donc les rayons atteignent la coupure de transmittance plus tôt. **Le facteur 3,7
entre `958,2` et `495,5` est donc le CONFINEMENT DE VORTICITÉ, pas le schéma.**
Deux choses avaient changé ; sans la course complète je n'aurais pas pu attribuer.

⚠️ **Variance de machine à nommer** : la même mesure de simulation a donné
**413,9 ms** puis **244,3 ms** — la première course tournait pendant deux `jenga`
d'un autre arbre. **Je publie les deux.** Un temps sans sa charge n'est pas un
nombre.

### ==> LA DÉCISION : je n'écris PAS (q3)

**Aucun des deux chemins ne donne une image montrable**, et le GPU n'y changerait
rien :

| | temps d'image | images/s |
|---|---|---|
| chemin ① CPU, 240 × 180 | 282,8 ms | **3,54** |
| chemin ② GPU (marche **gratuite**, hypothèse haute) | 244,6 ms | **4,09** |

**Le levier n'est ni le rendu ni le transfert : c'est la TAILLE DE LA GRILLE.**
À `244,3 ms` pour 50 000 cellules, soit **4,9 µs par cellule** :

    ~20 000 cellules  ->  ~98 ms de simulation  ->  ~7 images/s avec la marche
    ~6 000 cellules   ->  ~29 ms                ->  ~25 images/s

⚠️ **Extrapolation LINÉAIRE, et le solveur de pression ne l'honorera pas
forcément** : son nombre de balayages dépend du champ, pas seulement du compte de
cellules. C'est une piste chiffrée, **pas une mesure**.

Une grille de 6 000 cellules, c'est `18 × 18 × 18`. **Trop grossier pour montrer un
panache** — le chantier des volutes a mesuré que les structures font 4-5 cm sur une
source de 12 cm. **Il y a donc un vrai conflit entre « ça bouge en temps réel » et
« on voit des volutes », et il n'est pas résolu par un choix de renderer.**

**Ce qu'il faudrait, et c'est un lot à part entière** : porter la **SIMULATION** sur
GPU (le solveur de pression en premier), pas le rendu.
