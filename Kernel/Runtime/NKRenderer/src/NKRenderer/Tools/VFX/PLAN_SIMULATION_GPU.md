<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN — PORTER LA SIMULATION SUR GPU, LE SOLVEUR DE PRESSION D'ABORD

**Pré-enregistrement écrit le 2026-09-13 au soir, AVANT la moindre ligne de code,
et commité SEUL.** Un plan qui meurt avec la session n'a jamais été
pré-enregistré — quatre coupures réseau dans la journée l'ont rappelé.

---

## 0. POURQUOI CE LOT, ET POURQUOI CELUI-LÀ PLUTÔT QU'UN AUTRE

La mesure du 13/09 (`NK_FLUID_MAC=d`) a désigné le coupable **contre mon intuition
comme contre celle du coordinateur** :

```
grille 25 x 80 x 25 = 50 000 cellules, panache etabli, Release, un fil
  simulation                   244,3 ms par pas
  marche de rayon 240x180       38,2 ms  (ombres coupees)
  recopie du tampon              0,3 ms
  ------------------------------------------------
  rapport simulation / marche    6,40
```

> **Porter le RENDU sur GPU ferait passer de 3,54 à 4,09 images/s — `+15 %` pour un
> chantier entier.** On optimiserait ce qui ne coûte pas.

Et le conflit que ce lot doit lever est réel, mesuré, et n'a **aucune** solution
côté rendu :

> « Ça bouge en temps réel » et « on voit des volutes » **s'excluent** aux tailles
> de grille jouables sur CPU. Le chantier des volutes a mesuré des structures de
> **4-5 cm sur une source de 12 cm** ; une grille assez grossière pour tourner à
> 25 images/s (~6 000 cellules, soit `18 x 18 x 18`) n'a **plus aucune volute à
> montrer**.

---

## 1. ⚠️ L'ÉTAPE 0 EST UNE MESURE, PAS DU CODE — ET ELLE PEUT TUER LE LOT

**Je ne sais pas encore quelle part des `244,3 ms` est le solveur de pression.**
Tant que je ne le sais pas, promettre un gain est de la foi.

> **AMDAHL, et c'est le piège central de ce lot.** Si le solveur de pression pèse
> `60 %` du temps, un portage qui le rend **10 fois plus rapide** ne donne que
> `1 / (0,4 + 0,6/10) = 2,17x` sur le total. Un facteur 10 annoncé sur une partie
> n'est pas un facteur 10 sur le tout, et **c'est exactement le genre de chiffre
> qu'on se raconte avant de l'avoir mesuré.**

    (s0) LA VENTILATION DU PAS, mesuree AVANT tout portage
         combien de ms, sur les 244,3, vont a :
           - la projection (solveur de pression + ses balayages)
           - l advection des scalaires (flux van Leer, sous-cyclee)
           - l advection de la vitesse (semi-lagrangienne)
           - la flottabilite, le confinement, la combustion, la dissipation
         instrument : des chronos par phase, comme la ventilation deja faite
         ailleurs dans ce depot ; la somme des parts doit valoir le total
         mesure a 2 % pres, SINON l instrument ment et rien ne suit.

⚠️ **SI LA PROJECTION PÈSE MOINS DE 50 %, CE PLAN CHANGE DE CIBLE** — et je le dis
maintenant pour ne pas l'expliquer après. Le lot deviendrait *« porter l'advection
d'abord »*, ou *« porter tout le pas »*, et la justification « le solveur de
pression en premier » tomberait. **C'est une issue possible et elle est utile.**

---

## 2. LE CRITÈRE DÉCISIF, ÉCRIT AVANT — et il porte sur le TOTAL

Pas sur le solveur. Sur ce que Rodolf verra.

    (s1) CRITERE DECISIF : le PAS COMPLET, meme grille, meme scene, meme
         tolerance de pression, meme residu final -> gain >= 8x sur le TOTAL
         mesure comme un RAPPORT CPU/GPU sur la meme machine, jamais en ms
         absolues, et les deux courses dans la MEME execution

**D'où vient le `8x`, et pourquoi pas 10 ni 20 :** c'est ce qu'il faut, et rien de
plus, pour que le conflit tombe — le calcul est au § 3. Un seuil plus haut serait
une ambition ; celui-ci est une **exigence dérivée du besoin**.

**VOLET NÉGATIF, obligatoire** : le portage doit rendre **le même champ**. Critère :
sur une même scène et un même nombre de pas, `TotalMass`, `TotalHeat` et `Tmax`
doivent coïncider CPU/GPU à `1e-4` relatif — pas à l'epsilon machine (l'ordre de
sommation change sur GPU, et prétendre le contraire serait faux), mais assez près
pour que **(j1) et (k1) restent verts sur le chemin GPU**. Un solveur plus rapide
qui ne rend pas le même fluide n'est pas un solveur plus rapide.

---

## 3. LE NOMBRE QUE RODOLF POURRAIT SE PAYER — calculé AVANT

Budget de `40 ms` par image (25 images/s), dont la marche de rayon à 240 × 180,
ombres coupées : `38,2 ms` aujourd'hui sur CPU.

**Aujourd'hui, sans rien porter** : `40 − 38,2 = 1,8 ms` pour la simulation, soit
à `4,9 µs` par cellule **≈ 370 cellules**. Autant dire : **rien**.

Donc la marche doit descendre aussi. Deux leviers connus et déjà chiffrés : sur
GPU elle devient négligeable, ou sur CPU à `120 x 90` elle coûte ~`9,6 ms`
(extrapolation linéaire en pixels). Prenons **`4 ms` de rendu**, ce qui laisse
**`36 ms` de simulation** :

```
gain sur le pas   cout par cellule      cellules a 25 images/s     grille
     x 1 (CPU)        4,90 us                  7 300              ~19^3
     x 4              1,23 us                 29 000              ~31^3
     x 8              0,61 us                 59 000       25 x 80 x 25 ✔
     x16              0,31 us                118 000              ~49^3
     x32              0,15 us                235 000              ~62^3
```

> ### ⟹ **À `x8`, Rodolf se paie `59 000` cellules à 25 images/s — soit la grille
> `25 x 80 x 25` du panache actuel, celle qui MONTRE des volutes.**

**C'est de là que vient le seuil de (s1)**, et non d'un chiffre rond. En dessous de
`x8`, le conflit reste ; au-dessus, il est déjà levé et le surplus achète de la
résolution.

⚠️ **Extrapolation LINÉAIRE en cellules, et le solveur de pression ne l'honorera
pas forcément** : son nombre de balayages dépend du champ et de la tolérance, pas
seulement du compte de cellules. **C'est une cible chiffrée, pas une promesse** —
et (s0) est justement là pour la corriger avec des mesures.

---

## 4. CE QUI DOIT CHANGER, ET CE QUI NE DOIT PAS

### Ce qui change

| | |
|---|---|
| le solveur de pression | le Laplacien à 6 voisins sur grille MAC, en compute NkSL |
| le transfert | les champs montent une fois, redescendent **seulement si on les lit** |

**Ce qui existe déjà et que je n'aurai pas à écrire** — vérifié dans le code le
13/09 : NKRHI crée des **textures 3D** (`NkTextureDesc::Tex3D`, DX11
`CreateTexture3D` avec SRV **et UAV**, DX12 idem), et NkSL **génère** `sampler3D`
(GLSL) et `RWTexture3D<float4>` (HLSL). Le dépôt a par ailleurs déjà un témoin
d'atomiques GPU en NkSL (`NkGpuAtomicWitness`).

### Ce qui NE doit PAS changer

1. **Le chemin CPU reste, et reste le juge.** Le banc `NkFluidGridProbe` n'ouvre
   **aucun device** : ses 87 contrôles, (j1) et (k1) compris, doivent continuer à
   tourner **sans GPU**. Un portage qui rendrait le banc dépendant d'une carte
   graphique **détruirait l'appareil de preuve** — c'est-à-dire la seule chose qui
   a permis de trancher quoi que ce soit depuis le 05/09.
2. **Les deux défauts** (`advectFluxConservative`, `advectFluxLimiter`) gardent
   leur valeur et leur sens.
3. **Les seuils** de (j1), (k1), (f1), (h2) ne bougent pas.

---

## 5. CE QUE CE PORTAGE NE CORRIGERA PAS — dit maintenant

1. **La physique est inchangée.** Le même schéma, plus vite. **(v1) et (v6)
   resteront rouges** : que le confinement de vorticité ne concentre plus la
   vorticité sous transport conservatif est un **défaut de modèle**, pas de
   vitesse. *Aller plus vite ne rend pas un résultat juste.*
2. **`Tmax` restera ce qu'il est** — il dépend de la source et du transport, pas du
   processeur.
3. **Les quatre témoins hors de leur régime** — (c1), (v4), (2.1) et la GARDE de
   (a) — ne seront pas reformulés par ce lot.
4. **La quantité de mouvement ne sera pas davantage conservée** : seuls les
   scalaires passent par le flux, et cela ne change pas.
5. **Le `0,13 K` créé par la divergence résiduelle** (mesuré en (p2-)) **ne
   disparaîtra pas** : il vient de la forme conservative de l'advection de `T`, pas
   de la machine. Il pourrait même **grandir** si la tolérance de pression était
   relâchée pour aller plus vite — **c'est un piège à surveiller**, et un critère
   de plus : `Tmax` à débit nul ne doit pas s'éloigner davantage de `300,0000 K`.

---

## 6. L'ORDRE DES GESTES

1. **(s0) la ventilation du pas** — une mesure, aucun portage. Elle peut changer la
   cible du lot, et c'est pour cela qu'elle est première ;
2. le **témoin d'équivalence** CPU/GPU sur un champ analytique, AVANT le solveur :
   un Laplacien dont on connaît la réponse ;
3. le **solveur de pression** en compute, tolérance et résidu **inchangés** ;
4. **(s1)** le rapport sur le pas complet, et le volet négatif (même champ) ;
5. la **marche de rayon** GPU **seulement ensuite**, et seulement si (s1) passe —
   sinon elle n'achèterait toujours que `+15 %`.

**Aucun seuil de ce fichier ne sera déplacé après une mesure.** S'il devait l'être,
ce serait écrit comme un déplacement, avec sa raison, et la mesure d'avant
resterait publiée.

---

## 7. PRÉ-ENREGISTREMENT DÉTAILLÉ DE (s0) — écrit AVANT d'instrumenter

**Écrit le 13/09 au soir, AVANT la moindre ligne d'instrumentation**, après avoir
seulement **lu** `Step()`.

### Les phases, relevées dans le code et rangées en DEUX familles

C'est ce rangement qui portera le résultat, et il est fait **avant** de mesurer.

**① PHYSIQUE — ce qu'un moteur paierait vraiment :**

```
Combust · AddBuoyancy · ComputeVorticity (1re) · AddVorticityConfinement
AddWind · AdvectVelocity · PROJECT · MaxCFL + sous-cyclage
AdvectScalar x3 (densite, temperature, carburant) · dissipation
```

**② INSTRUMENTATION — ce que le banc paie pour SE JUGER :**

```
MeasureDivergence x3  (avant, apres, apres STRICT)
MeasureVelocity · ComputeVorticity (2e, refaite « pour l appelant »)
TotalMass + TotalHeat + TotalFuel + le balayage de Tmax
```

> ⚠️ **CETTE SECONDE FAMILLE EST UNE HYPOTHÈSE DANGEREUSE, ET IL FAUT LA NOMMER
> MAINTENANT.** `Step()` contient **trois** parcours de divergence, **deux** calculs
> de vorticité et **quatre** réductions sur toute la grille. **Si elle pèse lourd,
> alors les `244,3 ms` publiés comme « coût de simulation » sont EN PARTIE le coût
> du solveur qui SE MESURE LUI-MÊME** — et un moteur qui n'a pas besoin de ces
> témoins ne paierait pas ce prix. **Cela changerait la lecture de tout le lot B**,
> y compris le rapport `6,40`.

### Ma prédiction, calculée depuis le mécanisme

La course complète mesure **`268,3` balayages SOR par pas** (`omega 1,9005`). Sur
50 000 cellules, cela fait **~13,4 millions** de mises à jour par pas, là où
l'instrumentation fait ~10 parcours, soit **~0,5 million** : un rapport de **27
pour 1** en nombre d'accès.

    projection                 50 a 75 %   <- ma prediction principale
    advection des scalaires    15 a 30 %   (3 champs, sous-cyclee, van Leer)
    instrumentation             3 a 10 %
    tout le reste             moins de 10 %

### LA GARDE, et elle peut invalider la mesure entière

    (s0g) la SOMME des parts doit valoir le TOTAL a 2 % pres
          sinon une phase m echappe, ou mes chronos mesurent autre chose que ce
          qu ils nomment -- et AUCUN pourcentage ne serait lisible

⚠️ **Et une garde de perturbation** : ajouter ~15 lectures d'horloge par pas doit
rester invisible devant `244 ms`. Si le total s'écarte de plus de 2 % de celui
mesuré **avant** instrumentation, **c'est l'instrument qui change ce qu'il
mesure**, et je le dirai.

### ⚠️ MA PROPRE RÈGLE, QUE JE M'INTERDIS DE NÉGOCIER

> **Si la projection pèse moins de 50 % du pas, je CHANGE DE CIBLE et je le dis** —
> même si cela contredit tout ce que nous supposons depuis ce matin, **y compris le
> titre de ce plan**.

Et le cas le plus gênant est déjà écrit : **si l'instrumentation pèse lourd, le
premier gain n'est pas un portage GPU du tout** — c'est de ne plus faire tourner
trois mesures de divergence dans un moteur qui n'en a pas besoin. **Ce serait un
résultat qui rend ce lot inutile, et il vaut mieux le savoir avant d'investir.**

---

## 8. LE RÉSULTAT DE (s0) — LE TITRE DE CE PLAN EST RÉFUTÉ

**Mode `NK_FLUID_MAC=e`.** Grille `25 x 80 x 25 = 50 000` cellules, panache établi,
30 pas, Release, un fil.

```
phase                            ms / pas      part
PROJECTION (solveur pression)       85,09    38,76 %
advection des scalaires (x3)        34,60    15,76 %
advection de la VITESSE             63,80    29,06 %
vorticite (1re, pour le confin.)     7,77     3,54 %
confinement de vorticite             4,35     1,98 %
flottabilite                         1,19     0,54 %
combustion                           0,00     0,00 %
vent                                 0,00     0,00 %
CFL + sous-cyclage                   3,53     1,61 %
dissipation                          0,54     0,25 %
-----------------------------------------------------
PHYSIQUE (somme)                   200,87    91,51 %
INSTRUMENTATION (le banc)           18,64     8,49 %
TOTAL mesure                       219,51   100,00 %
```

### ⟹ LA RÈGLE S'APPLIQUE, ET ELLE TUE LE TITRE

**La projection pèse `38,76 %`, donc MOINS de 50 %.** La règle du § 7 était écrite
avant, et je ne la négocie pas :

> **« LE SOLVEUR DE PRESSION D'ABORD » EST RÉFUTÉ PAR LA MESURE.**
> AMDAHL sur le chiffre mesuré : un facteur 10 sur la projection **seule** donne
> `1 / (0,6124 + 0,03876) = ` **`1,54x`** sur le pas complet. Loin des `8x`
> qu'exige (s1).

### Ma prédiction est réfutée, et je sais pourquoi

Je prédisais **50 à 75 %** pour la projection. Deux erreurs, toutes deux
nommables :

1. **J'ai pris le nombre de balayages SOR de la MAUVAISE SCÈNE.** Les `268,3` que
   je citais viennent du palier masse/divergence ; **cette scène-ci en fait `96`**
   au dernier pas. Mon calcul de `13,4 M` mises à jour était donc presque trois
   fois trop grand. *Un chiffre repris d'une autre course n'est pas une mesure de
   celle-ci.*
2. **Je n'avais pas listé l'advection de la VITESSE comme candidate** — je l'avais
   noyée dans « tout le reste, moins de 10 % ». Elle pèse **`29,06 %`**. Le
   semi-lagrangien y paie, par composante et par cellule, un rebours RK2 **et deux
   interpolations trilinéaires** ; trois composantes sur 50 000 cellules, cela se
   voit.

**Les deux autres parts sont dans leurs bandes** : advection des scalaires
`15,76 %` (prédit 15-30), instrumentation `8,49 %` (prédit 3-10).

### ⚠️ MA GARDE (s0g) EST VERTE, ET ELLE EST PLUS FAIBLE QUE JE NE L'AI ÉCRITE

Écart `0,0000`. Mais **ce vert est presque trivial, et il faut le dire** : les
jalons se **chaînent**, donc la somme des parts partitionne l'intervalle **par
construction**. Cette garde ne peut attraper qu'un trou entre le premier et le
dernier jalon — **elle ne peut PAS attraper une phase mal étiquetée**, dont le
temps serait simplement attribué au mauvais nom.

**Ce qu'elle prouve réellement** : aucune fraction du pas n'échappe au comptage.
**Ce qu'elle ne prouve pas** : que chaque nom porte bien le temps qu'il annonce.
Pour cela il faudrait muter une phase (la doubler, et voir sa part doubler) —
**ce n'est pas fait**, et je le dis plutôt que de laisser ce vert valoir plus qu'il
ne vaut.

### ⚠️ LA GARDE DE PERTURBATION EST NOYÉE DANS LA CHARGE MACHINE

Je voulais vérifier que l'instrumentation ne déplace pas le total de plus de 2 %.
**Je ne peux pas** : la même mesure, **avant** instrumentation, avait déjà donné
`413,9` puis `244,3 ms` selon la charge — et elle donne `219,51` maintenant.
**L'écart de charge (70 %) écrase l'effet cherché (2 %).** Je ne conclus donc rien
sur la perturbation, au lieu d'attribuer à mon instrument une baisse qui vient de
la machine.

### ==> LA NOUVELLE CIBLE, calculée depuis la mesure

    x10 sur la projection SEULE            -> 1,54x    (insuffisant)
    x10 sur les DEUX advections (44,82 %)  -> 1,67x    (insuffisant)
    x10 sur TOUTE la physique (91,51 %)    -> 5,88x    (encore insuffisant)
    x20 sur toute la physique              -> 7,66x    (tout juste sous 8x)

> **Aucun portage PARTIEL n'atteint les `8x`.** Il faut le **pas complet**, et même
> ainsi `x10` ne suffit pas.

### ⚠️ ET LE VERROU QUE PERSONNE N'AVAIT VU : L'INSTRUMENTATION

Pour `8x`, il faut un pas de `219,51 / 8 = 27,44 ms`. Or **l'instrumentation seule
coûte `18,64 ms`**.

> **Si elle reste sur CPU, elle consomme `68 %` du budget total à elle seule**, et
> il ne resterait que `8,8 ms` pour TOUTE la physique — soit un facteur `22,8`
> exigé sur elle. **Les `8x` sont inatteignables tant que les trois mesures de
> divergence, les deux vorticités et les quatre réductions tournent à chaque pas.**

**C'est le résultat le plus actionnable du lot, et il ne coûte aucun GPU** : un
moteur n'a pas besoin de ces témoins à chaque image. Les rendre **optionnels**
(un drapeau de `NkFluidGridParams`) rendrait `18,64 ms` immédiatement, soit
**`1,09x`** — petit seul, mais **il débloque le plafond** au-dessus.

⚠️ **Je ne le fais PAS dans ce lot** : le banc a besoin de ces témoins, et les
rendre optionnels veut dire décider **qui** les allume. C'est une décision, pas une
optimisation, et elle appartient à Rodolf.

---

## 9. LA RÈGLE DES TÉMOINS, TRANCHÉE — et ce qu'elle rend RÉELLEMENT

> **Dans le BANC, les témoins restent TOUJOURS allumés, SANS interrupteur** — c'est
> lui le juge, et **un juge qui peut fermer les yeux ne juge plus**. Aucun mode,
> aucune variable d'environnement n'expose `temoinsMesure`.
>
> **Sur le chemin TEMPS RÉEL, ils sont ÉTEINTS — et c'est l'EXTINCTION qui
> s'annonce**, dans le bandeau ET dans le journal, jamais l'allumage. *Un réglage
> qu'il faut penser à armer se fait oublier ; un bandeau qui dit « témoins
> éteints » se voit.*

### ⚠️ `MeasureVelocity` N'EST PAS UNE MESURE — trouvé en LISANT, avant de couper

Son nom ment. Quand une cellule dépasse `maxSpeed`, elle **multiplie les six
vitesses de face** par `lim/s` : c'est un **filet de sécurité**, donc de la
**physique**. La couper aurait changé le **champ**, pas seulement le rapport — le
contrôle négatif du banc l'aurait dit, mais **après**. Elle reste donc allumée
partout, avec le balayage de `Tmax` que le bandeau affiche.

### Ce que l'extinction rend, MESURÉ et non supposé

```
INSTRUMENTATION totale              19,96 ms    8,64 %
  dont EXTINGUIBLE (temps reel)     14,58 ms    6,31 %   <- l economie REELLE
  dont GARDE (filet + Tmax)          5,38 ms    2,33 %   <- ne peut pas partir
TOTAL du pas                       231,10 ms
```

> **L'économie n'est pas de `18,64 ms` comme nous l'attendions : elle est de
> `14,58 ms`.** Les `5,38 ms` restants sont le filet et le `Tmax`. **Annoncer 18,6
> aurait été promettre ce qu'on ne peut pas rendre.**

**Sur la sonde temps réel** (65 536 cellules) : `sim 147,1 à 154,8 ms` et
`227,1 à 236,4 ms` par image, contre `153,5 à 196,0` et `245,7 à 301,6` avant, avec
`TEMOINS ETEINTS : OUI` dans chaque ligne de journal.
⚠️ **Le signe est bon, la magnitude n'est PAS séparable de la charge machine** : le
même total a déjà varié de 70 % d'une course à l'autre. **Le chiffre qui vaut est
`msMesuresEteintes`**, mesuré dans la **même** course que son total.

### Le contrôle négatif du banc : VERT

`87 contrôles, 7 ROUGES`, **rouges identiques au texte**, ancrages au dernier
chiffre — enstrophie `1,458000`, (w2) `2,0015`, (d) `0,019 cellule`, (b)
`0,000029 %`. **Le banc n'a rien éteint**, et c'est ce que la règle exige.

### ⟹ LA CIBLE, MISE À JOUR AVEC LES VRAIS CHIFFRES

Le plafond des `8x` demandait un pas de `27,44 ms`. Avec l'extinction, le CPU passe
de `231,10` à `216,52 ms` — soit **`1,07x`** — et le budget restant pour la physique
devient `27,44 − 5,38 = 22,06 ms` au lieu de `8,8`.

    exige sur la PHYSIQUE pour tenir les 8x :  200,87 / 22,06 = x 9,1
    (contre x 22,8 avant l extinction)

> **L'extinction ne fait pas gagner `1,07x` : elle fait passer l'exigence de
> `x22,8` à `x9,1`.** C'est là toute sa valeur — elle **débloque le plafond**, elle
> n'accélère presque rien.

Et la cible reste celle de (s0) : **le pas COMPLET, ou rien.**

```
x10 sur la projection SEULE (39 %)    -> 1,54x    insuffisant
x10 sur les DEUX advections (44,8 %)  -> 1,67x    insuffisant
x10 sur TOUTE la physique             -> 5,88x    insuffisant
x20 sur TOUTE la physique             -> 7,66x    tout juste sous 8x
```

### ⚠️ CE QUE ÇA REPRÉSENTE COMME CHANTIER — dit honnêtement

Porter **tout le pas**, ce n'est pas porter un solveur :

1. le **solveur de pression** — et **le SOR séquentiel ne se parallélise pas tel
   quel**. Il faut passer à un Jacobi rouge-noir, c'est-à-dire **changer
   d'algorithme, pas seulement de processeur** : le nombre de balayages changera,
   **et le résultat aussi** ;
2. l'**advection de la vitesse** semi-lagrangienne (29 %) et ses interpolations
   trilinéaires ;
3. l'**advection en flux** des scalaires (16 %), **sous-cyclée** — un nombre de
   passes qui dépend du champ, donc décidé sur le GPU ou synchronisé à chaque pas ;
4. vorticité, confinement, flottabilité, dissipation (≈ 6 %) ;
5. et le **transfert** : les champs montent une fois et ne redescendent que
   lorsqu'on les lit — or **le banc les lit à chaque pas**, donc le banc CPU et le
   chemin GPU ne peuvent pas être le même code.

**Le point dur n'est pas le nombre de lignes, c'est le § 1** : un Jacobi rouge-noir
ne rend pas le même champ qu'un SOR séquentiel à la même tolérance. **Le volet
négatif de (s1) — même masse, même chaleur, même `Tmax` à `1e-4` — est donc le vrai
risque du chantier, pas la performance.**

> **Estimation honnête : ce n'est pas un lot, c'est un chantier de la taille de la
> bascule MAC ou de l'advection en flux** — plusieurs jours, avec son propre banc
> d'équivalence CPU/GPU à écrire **avant** le premier nuanceur. **Et il peut échouer
> sur (s1)-négatif plutôt que sur la vitesse.**

**Si Rodolf préfère renoncer, (s0) lui aura fait économiser ce chantier** — et
c'était exactement sa raison d'être.
