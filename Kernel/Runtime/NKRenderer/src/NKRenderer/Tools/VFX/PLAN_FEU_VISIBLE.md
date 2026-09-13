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
