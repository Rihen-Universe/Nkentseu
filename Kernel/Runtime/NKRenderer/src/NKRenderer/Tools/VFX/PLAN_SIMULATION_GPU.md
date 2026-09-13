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
