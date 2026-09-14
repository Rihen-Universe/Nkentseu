<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN DU SCHÉMA D'ADVECTION D'ORDRE SUPÉRIEUR — pré-enregistrement

**Écrit le 2026-09-13, AVANT la première ligne de code et AVANT la première
mesure.** Étape 6 du `PLAN_ADVECTION_FLUX.md`, ouverte par la décision de Rodolf
du matin du 13/09.

Références : Bram VAN LEER, « Towards the Ultimate Conservative Difference Scheme
V », *Journal of Computational Physics* 32, 1979, p. 101-136 ; Peter K. SWEBY,
« High Resolution Schemes Using Flux Limiters for Hyperbolic Conservation Laws »,
*SIAM Journal on Numerical Analysis* 21(5), 1984, p. 995-1011. Le schéma de base
reste le **donor-cell** de Courant, Isaacson & Rees 1952, déjà posé et mesuré.

---

## 0. POURQUOI CE LOT EXISTE, ET CE QU'IL REMPLACE

La question posée à Rodolf le 12/09 était un **arbitrage** : allumer
`advectFluxConservative` gagne la masse (−43,1 % → +0,0000 %) et coûte le détail
(`Tmax` divisé par **3,18** sur la scène (e), 550,5 K contre 1749,4 K).

**Rodolf a refusé l'arbitrage.** Sa raison, et elle est technique : payer 3,18 fois
le détail pour gagner la masse est un compromis qu'on *subit* ; un schéma d'ordre
supérieur *supprime* le compromis au lieu de le répartir.

**Et la mesure lui donnait déjà raison.** (g2) et (g3) ont fait varier la cible de
sous-cyclage de 0,40 à 1,20 : `Tmax` ne bouge que de **2,1 %** (539,2 → 550,5 K).
Le prix ne vient donc **pas** d'un réglage mal choisi, mais de la **diffusion du
premier ordre lui-même** — `D = (u·h/2)(1 − CFL)`. Une hypothèse a été éliminée,
et c'est ce qui autorise à dire que seul un ordre supérieur peut réduire ce
rapport.

---

## 1. CE QUI CHANGE — compté AVANT d'être promis

`PLAN_GRILLE_MAC.md` avait sous-estimé son portage (**37 sites dans 12 fonctions**
là où il en annonçait beaucoup moins). Le compte est donc fait **avant** :

### Le noyau — `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/VFX/`

| fichier | sites | quoi |
|---|---:|---|
| `NkFluidGrid.h` | 3 | l'énumération `NkFluidFluxLimiter`, le champ `advectFluxLimiter`, la déclaration de `AdvectFluxUnePasseLimitee` |
| `NkFluidGrid.cpp` | 2 | la nouvelle fonction, et **un seul** aiguillage dans `AdvectScalarFlux` |

**Cinq sites, deux fichiers.** C'est petit, et ce n'est pas un hasard : la grille
décalée et le bilan par face sont **déjà** posés. L'ordre supérieur n'est qu'un
**flux de plus sur la même face**.

### Le banc — `Applications/NkFluidGridProbe/src/`

| fichier | sites | quoi |
|---|---:|---|
| `advection_flux.cpp` | 3 aiguillages de scène + (h2) + (h1) + (h3) | les trois scènes (`ScenePanache`, `SceneMasse`, `SceneDixSecondes`) prennent le limiteur ; les contrôles neufs |
| `main.cpp` | 1 | le mode `NK_FLUID_MAC=8` |

---

## 2. CE QUI NE DOIT **PAS** CHANGER — et comment c'est garanti

> **Le défaut par défaut est l'ordre 1, et il doit rendre les chiffres
> d'aujourd'hui AU BIT.**

Ce n'est pas une intention, c'est une propriété de **construction** :
`AdvectFluxUnePasse` n'est **pas touchée**. Pas une ligne, pas un `+ 0.f`, pas une
condition ajoutée dans sa boucle intérieure. Le schéma d'ordre supérieur vit dans
une fonction **séparée**, et `AdvectScalarFlux` choisit l'une ou l'autre.

**Pourquoi cette forme plutôt qu'un `if` dans la boucle :** un test à l'intérieur
de la boucle intérieure oblige le compilateur à réordonner l'arithmétique, et
« bit-identique » cesse alors d'être vérifiable par `git diff`. Ici la preuve de
(h1)-négatif est **lisible dans le diff** avant même d'être mesurée.

Ne changent donc pas non plus :
`advectFluxConservative` reste **`false` par défaut** ; l'advection de la
**vitesse** reste semi-lagrangienne ; la projection, la flottabilité, le
confinement, la combustion et le rendu ne sont pas touchés.

---

## 3. LE SCHÉMA — écrit avant d'être codé

Sur la face `i` qui sépare les cellules `i-1` et `i`, de vitesse `u` :

```
c   = u * dt / h                       (Courant LOCAL de cette face)
phi_donneur = (u > 0) ? phi[i-1] : phi[i]

F_ordre1 = u * dt/h * phi_donneur                       (donor-cell, l'existant)

d   = phi[i] - phi[i-1]                (gradient LOCAL, en travers de la face)
du  = (u > 0) ? phi[i-1] - phi[i-2]                     (gradient AMONT)
             :  phi[i+1] - phi[i]

F_antidiffusif = 0.5 * |c| * (1 - |c|) * psi(du/d) * d

F = F_ordre1 + F_antidiffusif
dst[i-1] -= F ;  dst[i] += F
```

`F_ordre1 + F_antidiffusif` **sans** limiteur (`psi = 1`) est exactement le flux de
**Lax-Wendroff** — c'est ce qui donne l'ordre 2, et c'est ce qui oscille.

### La démonstration de l'identité, parce qu'elle ne se croit pas sur parole

Pour `u > 0` (`phi_donneur = phi[i-1]`, `c > 0`) :

```
F_LW − F_donor = 0.5·c·(phi[i-1]+phi[i]) − 0.5·c²·(phi[i]−phi[i-1]) − c·phi[i-1]
               = 0.5·c·(1 − c)·(phi[i] − phi[i-1])
```

Pour `u < 0` (`phi_donneur = phi[i]`, `c < 0`) :

```
F_LW − F_donor = −0.5·c·(1 + c)·(phi[i] − phi[i-1])
               = 0.5·|c|·(1 − |c|)·(phi[i] − phi[i-1])
```

**Les deux branches donnent la MÊME expression**, `0.5·|c|·(1−|c|)·d`. C'est ce qui
permet de l'écrire une seule fois, et c'est ce qui la rend vérifiable.

### Le limiteur, et la division qui ne se fait pas

`psi(r)·d` s'écrit **sans jamais diviser**, ce qui supprime la question du
dénominateur nul (un front raide donne `d = 0` très souvent) :

```
van Leer :  psi(r)·d = (du·|d| + |du|·d) / (|d| + |du|)      , 0 si |d|+|du| = 0
minmod   :  psi(r)·d = signe(d) · min(|d|, |du|)  si du·d > 0 , 0 sinon
superbee :  psi(r)·d = signe(d) · max( min(2|du|,|d|), min(|du|,2|d|) ) si du·d > 0
```

⚠️ **Aux extrema, `du` et `d` sont de signes opposés, et le numérateur de van Leer
s'annule EXACTEMENT** : `du·|d| + |du|·d = du·(−d) + du·d = 0`. Le schéma retombe
sur le donor-cell **là et seulement là**. C'est ce qui le rend borné — et c'est
aussi, mécaniquement, **ce qui l'empêchera de rendre 100 % du détail** : un pic de
température EST un extremum.

### La masse est conservée par construction, encore

Le flux antidiffusif est **retranché à une cellule et ajouté à l'autre**, comme le
flux d'ordre 1. Les faces de paroi ne sont **jamais** parcourues. La conservation
n'est donc pas une propriété du limiteur : elle est **indépendante** de `psi`.

⚠️ **C'est précisément pourquoi (h2) seul ne prouve rien** : le schéma **sans
limiteur**, qui fabrique des densités négatives, conservera la masse **tout
aussi exactement**. Voir § 5.

### Aux parois, le schéma retombe à l'ordre 1, et c'est voulu

`SetBoundary(0, …)` remplit les fantômes par **recopie** du voisin intérieur. Donc
à la première face intérieure, `du = phi[1] − phi[0] = 0` → `psi·d = 0` → ordre 1.
Un schéma d'ordre 2 qui extrapolerait au-delà de la paroi inventerait de la
matière que la paroi doit arrêter.

---

## 4. LES CRITÈRES, ÉCRITS AVANT LA MESURE

### (h1) LE DÉTAIL REVIENT — `Tmax / Tmax(référence semi-lagrangienne)`

Scène (e), **même course**, trois bras : semi-lagrangien (référence, aucun
verdict) · flux ordre 1 · flux limité. Configuration : Release, sans GPU, boîte
0,5 × 1 × 0,5 m, `h = 0,02 m`, `dt = 1/60 s`, 600 pas, cible CFL 0,90.

| | valeur |
|---|---|
| aujourd'hui | `550,5 / 1749,4 = 0,3147` — soit `1 / 3,18` |
| **seuil de réussite** | **`Tmax/ref ≥ 0,50`** |
| ma prédiction | **0,55 à 0,85**, soit une perte ramenée entre `/1,2` et `/1,8` |

**D'où vient le seuil 0,50, et pourquoi il n'est pas inventé ce matin :** c'est le
plancher que j'avais **proposé le 12/09 dans Q6** (« par exemple ≥ 50 % de la
référence ») sans qu'il soit appliqué. Le reprendre ici, c'est reprendre un chiffre
**déjà écrit avant de connaître la mesure**, pas en fabriquer un après.

**D'où vient la prédiction, calculée depuis le mécanisme :** un pic gaussien de
rayon `a` diffusé par `D` pendant `T` voit son amplitude tomber en 3D comme
`(a²/(a²+2DT))^{3/2}`. Le rapport mesuré 0,3147 donne `2·D₁·T = 1,146·a²`. Un
limiteur de van Leer annule l'erreur en `O(h)` **dans les régions lisses** et ne la
laisse que là où il écrête ; l'ordre de grandeur usuel est `D_eff ≈ D₁/4` à `D₁/8`.
Avec `D₁/5` : `2·D₂·T = 0,229·a²` → rapport `(1/1,229)^{3/2} = 0,73`.

⚠️ **La fourchette est ÉLARGIE exprès.** Q4, Q5 et Q6 ont produit **trois
sous-estimations d'ampleur de suite** (11,3 % prédit « de l'ordre du pourcent » ;
1e-7 prédit, zéro exact mesuré ; 20-60 % prédits, 109,75 % mesurés). Le sens et
l'ordre de grandeur étaient bons à chaque fois, **la fourchette trop étroite**.
J'élargis donc plutôt que de resserrer.

⚠️ **`Tmax/ref = 1` N'EST PAS L'OBJECTIF, et le dire est une question d'honnêteté.**
La référence semi-lagrangienne **perd 43,1 % de la masse** : son `Tmax` de 1749,4 K
est en partie l'artefact d'une concentration non conservative. Un transport
**parfait** ne rendrait donc pas 1749,4 K. La référence dit **d'où l'on part**, pas
où il faut arriver.

**VOLET NÉGATIF (obligatoire)** : le limiteur réglé sur `Ordre1` doit rendre
**exactement les chiffres d'aujourd'hui** — `Tmax = 550,5 K`, CFL max 2,45,
3 sous-pas. Si un seul bouge, le portage a touché ce qu'il promettait de ne pas
toucher, et **(h1) ne peut pas être lu**.

### (h2) LA MASSE TIENT

Montage de (f1) : fonction de courant, divergence MAC identiquement nulle, parois
fermées, aucune source, aucune dissipation, 20³ cellules, 200 pas, `dt = 1/120 s`.
**Seuil : dérive relative < 1e-05**, le même que (f1), jamais déplacé.

Prédiction : **zéro exact**, comme l'ordre 1 — la conservation ne dépend pas de
`psi`.

**VOLET NÉGATIF, ET IL EST OBLIGATOIRE** : le **contrôle positif (f1b)** — la part
de la masse qui a **changé de place** (variation L1, exigée > 5 %). « La masse se
conserve » est **trivialement vrai** d'un schéma qui ne transporte rien, et ce
montage l'aggrave : la bulle est posée au **point stationnaire** de la fonction de
courant. Sans (f1b), (h2) serait vert pour la pire des raisons.

### (h3) LA STABILITÉ NE RECULE PAS

La rupture de l'ordre 1 est **encadrée entre CFL 1,2433 et 1,2436** (dix
dichotomies, largeur 0,00027), **dans les unités du banc** — `MaxCFL` prend le max
des deux faces par axe là où la condition vraie porte sur la somme des flux
sortants, donc il **surestime**, d'un facteur qui dépend du champ.

Un schéma d'ordre supérieur **peut** déplacer cette rupture. **On la mesure, on ne
la suppose pas** : même dichotomie, même détecteur (la **densité négative**, pas la
masse — le schéma reste conservatif même instable), même filet coupé
(`advectMaxSubsteps = 1`, sinon on mesure le filet).

| | valeur |
|---|---|
| aujourd'hui | rupture ∈ [1,2433 ; 1,2436] |
| **seuil de réussite** | **rupture ≥ 0,90** dans les unités du banc |
| ma prédiction | van Leer ∈ [1,0 ; 1,25] |

**D'où vient le seuil 0,90 :** c'est la **cible de sous-cyclage en vigueur**, choisie
par (g3). Si la rupture passait sous elle, le sous-cyclage ne garantirait plus
rien et la cible devrait bouger — ce serait une **régression à rapporter**, pas à
cacher.

**VOLET NÉGATIF** : le schéma **sans limiteur** doit casser **plus tôt**, ou
produire des densités négatives sous la cible. S'il ne casse pas, mon détecteur ne
voit pas ce qu'il prétend voir, et **(h3) ne vaut rien**.

### (h4) RIEN D'AUTRE NE BOUGE

Ancrages, au dernier chiffre publié : enstrophie **1,458000**, (w2) **1,9961**,
(d) **0,081 cellule**, (b) **0,000026 %**.

⚠️ **Le COMPTE de contrôles, lui, DOIT augmenter, et le figer serait la faute.**
La course de référence de ce matin donne son compte et ses ROUGES ; (h2) ajoute
**3 contrôles nommés d'avance** au palier d'advection (la conservation limitée, son
contrôle positif, et le témoin « sans limiteur : conservatif ET faux »). Le compte
attendu est donc **celui de la référence + 3**, et **le nombre de ROUGES ne bouge
pas**. C'est exactement ce qui s'est passé pour (f) : 61 → 64 contrôles, 5 rouges
inchangés.

> Un compte figé à côté d'une liste est le motif que ce dépôt a **déjà payé**
> (`Captures/LISEZMOI.md`, « les 9 fichiers » qui ont vieilli en silence). On
> **compte**, on ne recite pas.

**VOLET NÉGATIF** : si un ancrage bouge, la promesse du § 2 est **fausse** — le
chemin par défaut a été touché — et le lot s'arrête là, quelle que soit la valeur
de (h1).

---

## 5. ⚠️ LE PIÈGE CENTRAL DE CE LOT, NOMMÉ AVANT DE LE RENCONTRER

**Un schéma d'ordre supérieur sans limiteur oscille, et fabrique des densités
négatives.** C'est la contrepartie exacte de l'ordre 2 : le théorème de Godunov dit
qu'aucun schéma linéaire d'ordre > 1 n'est monotone.

Le détecteur existe déjà, il est **exact et pas commode** : la **densité minimale**
vaut `0,000e+00` **exactement** dans tous les cas sains, et (g1) l'a vue distinguer
`−3,600e-04` de zéro à la quatrième décimale d'un CFL.

C'est pourquoi le variant **sans limiteur est CODÉ EXPRÈS**, et tourne comme
**témoin négatif** :

| ce qu'il doit montrer | pourquoi ça compte |
|---|---|
| masse conservée **exactement** | prouve que (h2) ne juge PAS la justesse |
| densité minimale **< 0** | prouve que le détecteur sait rendre autre chose que zéro |

> **Un témoin qui rend exactement zéro doit prouver qu'il peut rendre autre
> chose.** Ici, c'est le schéma non limité qui le prouve.

### ⚠️ LE CRITÈRE EST UN RAPPORT, PAS UN ZÉRO ABSOLU — et c'est écrit avant

Premier jet de ce plan : « le schéma limité doit rendre une densité minimale
`>= 0` ». **Corrigé AVANT la première mesure**, parce que ce seuil exigeait une
propriété que le schéma **ne prétend pas avoir** : la TVD de van Leer est un
résultat **1D**, et ce schéma est **multidimensionnel NON SPLITTÉ**, sans
transport de coin. De minuscules sous-dépassements y restent possibles, limiteur
ou non.

Le critère retenu est donc : le schéma limité doit rester **au moins 100 fois plus
près de zéro** que le non limité, et le non limité doit avoir réellement franchi
zéro (sinon le rapport serait `0/0`). Il teste ce qui est en jeu — *le limiteur
réduit-il le sous-dépassement d'un ordre de grandeur ?* — et non une propriété
empruntée à un autre théorème.

Un rouge obtenu contre un seuil qu'on n'a pas le droit d'exiger ne dit rien du
code ; il dit seulement que le seuil était faux.

---

## 6. CE QUE CE SCHÉMA NE CORRIGERA **PAS** — dit maintenant, pas après

1. **La quantité de mouvement.** Seuls les **scalaires** passent par le flux ; la
   **vitesse** reste advectée en semi-lagrangien. La chaîne
   `température → poussée → vitesse` traverse donc toujours un champ de vitesse
   non conservatif. Lentine-Aanjaneya-Fedkiw traitent les deux ; ce lot n'en fait
   qu'un.
2. **L'inconditionnelle stabilité.** Elle a été abandonnée avec le semi-lagrangien
   et ne revient pas. Le sous-cyclage reste **exigé**, pas optionnel.
3. **Le rapport `Tmax/ref = 1`.** Voir (h1) : la référence n'est pas une vérité.
4. **La perte à la paroi**, la divergence résiduelle, le contraste (2.1) — aucun
   n'est dans le chemin d'advection des scalaires.
5. **(c1), (v4) et (2.1)**, les trois témoins hors de leur régime qui attendent
   Rodolf : **je n'en touche aucun.**
6. **L'allumage de `advectFluxConservative`.** Il reste **éteint par défaut**.
   L'allumer est une décision de Rodolf, et elle attend le chiffre de (h1).

---

## 7. L'ORDRE DES GESTES

1. la course de **référence** sur le code de `main`, **avant** toute modification —
   elle donne le compte, les ROUGES et les ancrages auxquels (h4) se compare ;
2. le noyau : l'énumération, le champ, la fonction limitée ;
3. (h2) et son contrôle positif, **les moins chers** — s'ils sont rouges, le reste
   ne veut rien dire ;
4. (h1), le chiffre qui part à Rodolf ;
5. (h3), la dichotomie ;
6. (h4), la course complète **rejouée**, et le diff des verdicts.

**Aucun seuil de ce fichier ne sera déplacé après une mesure.** S'il devait l'être,
ce serait écrit comme un déplacement, avec sa raison, et la mesure d'avant
resterait publiée.

---

## 8. APRÈS LA MESURE — (h1) EST ROUGE, ET IL FAUT DIRE CE QU'IL ÉLIMINE

**Ajouté le 13/09 APRÈS la course de mode 8, et signalé comme tel.** Ce § ne
déplace aucun seuil du § 4 : il enregistre un résultat et **ouvre une enquête
nouvelle, qui rendra son propre pré-enregistrement avant de tourner.**

    schema             CFL max  sous-pas   vmax    Tmax    Tmax/ref   ms/pas
    SEMI-LAG (ref)       7,067      1     7,782   1749,4   1,000       104,5
    ordre 1              2,452      3     2,919    550,5   0,3147       84,7
    minmod               2,749      4     3,170    564,8   0,3229      106,6
    van Leer             2,806      4     3,222    571,7   0,3268      107,4
    superbee             2,904      4     3,292    580,6   0,3319      103,6
    SANS limiteur        2,754      4     3,189    704,9   0,4030      103,5

**Ma prédiction (0,55-0,85) est RÉFUTÉE, et de loin.** Le gain de van Leer sur
l'ordre 1 est de **3,8 % relatif** — la perte passe de `/3,18` à `/3,06`.

### Ce que le témoin négatif rend décisif

Le schéma **SANS limiteur** n'a **aucune diffusion numérique au premier ordre** —
c'est un Lax-Wendroff nu. Il plafonne à **0,4030**. Et ce plafond est lui-même
**une borne supérieure GONFLÉE** : sa densité minimale vaut `−2,428`, il fabrique
donc des valeurs hors de l'intervalle des données **dans les deux sens**, y compris
des **maximums qui n'existent pas**.

> **Conclusion mesurée : aucun schéma d'advection scalaire BORNÉ, à aucun ordre,
> n'atteint 0,50 sur cette scène.** Le prix ne vient donc **pas** de l'ordre du
> schéma scalaire. Le témoin que j'avais codé pour garder le détecteur est celui
> qui a tranché la question du lot.

Une **deuxième hypothèse tombe**, après celle du réglage de sous-cyclage éliminée
par (g2). Un lot qui ferme une piste vaut mieux qu'un lot qui la laisse ouverte —
mais il faut alors dire **où regarder ensuite**.

---

## 9. PRÉ-ENREGISTREMENT DE L'ENQUÊTE (i) — LA FUMÉE QUI PÈSE

**Écrit AVANT de coder le mode 9 et AVANT de le lancer.** ⚠️ **Ce n'est pas un
témoin : cette enquête ne rend AUCUN verdict**, comme `EnqueteBascule` et (g2).
Elle fait varier **un** paramètre pour départager deux causes ; une question posée
à un seul réglage ne peut pas trancher entre elles.

### L'hypothèse (H), et d'où elle vient

L'équation (8) de Fedkiw, Stam & Jensen 2001 porte **deux** termes :

```
f = − alpha · s · ŷ  +  beta · (T − T_ambiante) · ŷ
     ^^^^^^^^^^^^^^
     la FUMÉE PÈSE
```

Sur la scène (e), `buoyancyAlpha = 0,3` : le terme de fumée est **actif**. Or le
semi-lagrangien **perd 43,1 % de la masse de fumée** (mesuré en (f2)). Il perd
donc aussi **43 % du poids qui retient le panache**, monte plus vite
(`vmax 7,782` contre `3,2`), et concentre davantage sa chaleur.

**(H) : une part du gouffre sur `Tmax` ne serait pas un défaut du schéma
conservatif, mais la conséquence PHYSIQUEMENT JUSTE de garder la fumée que le
semi-lagrangien perd.** Si c'est vrai, le « prix » n'était pas entièrement un
prix : c'était en partie une **correction**.

### Le montage

Scène (e) inchangée par ailleurs, **quatre bras dans la MÊME course** :
`alpha = 0,3` (le défaut) et `alpha = 0` (le poids de la fumée coupé), croisés
avec semi-lagrangien et van Leer. Trois grandeurs par bras : `Tmax`, la **masse
de fumée finale**, et la **chaleur totale** `somme (T − T_amb)·h³`.

### Ce que j'attends, écrit avant de le voir

| si | alors |
|---|---|
| **(H) vraie** | à `alpha = 0`, `Tmax/ref` monte **nettement au-dessus de 0,3268** — je dis **> 0,50** |
| **(H) fausse** | `Tmax/ref` reste à **0,33 ± 0,03**, et la cause est ailleurs |

**VOLET NÉGATIF, obligatoire** : si les deux `Tmax` **absolus** bougent beaucoup
alors que leur **RAPPORT** ne bouge pas, `alpha` n'est **pas** la cause — j'aurais
seulement changé la scène. C'est le rapport qui répond, pas les valeurs.

⚠️ **La chaleur totale est le second instrument, et il peut me contredire
proprement** : si les deux schémas portent **la même chaleur totale** avec des
`Tmax` dans un rapport de 3, alors la différence est une pure **concentration** —
et ni `alpha`, ni l'ordre du schéma n'en seraient la cause, mais la manière dont
le semi-lagrangien **empile** la chaleur là où il ne conserve rien.

⚠️ **Vu ma série de trois sous-estimations puis d'une surestimation franche en
(h1), je n'accorde à ma propre attente que la valeur d'une cible à réfuter.**

---

## 10. LE RÉSULTAT DE (i) — (H) EST RÉFUTÉE, ET LE SECOND INSTRUMENT RÉPOND

    alpha   schéma            vmax      Tmax    Tmax/ref   masse fin.   chaleur (K.m^3)
     0,30   SEMI-LAGRANGIEN   7,782    1749,4   1,000 ref    0,256144      10,0607
     0,30   FLUX van Leer     3,222     571,7   0,3268       0,016781       1,1537
     0,00   SEMI-LAGRANGIEN   8,231    1721,5   1,000 ref    0,271373      10,5842
     0,00   FLUX van Leer     3,307     566,0   0,3287       0,016781       1,1537

**(H) est RÉFUTÉE proprement** : couper le poids de la fumée déplace le rapport de
`0,3268` à `0,3287` — **0,6 %**. Le volet négatif que j'avais écrit est exactement
ce qui s'est produit : les `Tmax` absolus bougent un peu, **le rapport ne bouge
pas**. `alpha` n'est pas la cause.

### ⚠️ MAIS LA COLONNE « CHALEUR » DIT AUTRE CHOSE, ET C'EST LA VRAIE RÉPONSE

> **Le semi-lagrangien finit la course avec 15,3 fois plus de FUMÉE et 8,7 fois
> plus de CHALEUR que le schéma conservatif.**

Et la chaîne qui l'établit ne repose sur aucune supposition :

1. **La masse finale du schéma en flux est IDENTIQUE à six décimales pour
   `alpha = 0,30` et `alpha = 0,00`** (`0,016781`), alors que le champ de vitesse,
   lui, change (`vmax 3,222 → 3,307`). C'est la **signature d'une conservation
   exacte** : la masse totale ne vaut plus que *ce qui est injecté moins ce qui est
   dissipé*, deux quantités que l'écoulement ne touche pas.
2. **Celle du semi-lagrangien, elle, DÉPEND de l'écoulement** : `0,256144` contre
   `0,271373`, soit 5,9 % d'écart pour un seul paramètre changé.
3. Or sur cette scène les deux schémas reçoivent **la même injection**
   (`EmitSphere`, même position, même débit, même nombre de pas) et subissent **la
   même dissipation** (multiplicative, appliquée au champ entier). Le total correct
   est donc celui du schéma conservatif — **par construction, et (f1)/(h2) le
   prouvent à `0,000e+00` de dérive**.

**Conséquence : les 15,3× et les 8,7× ne sont pas de la matière que le schéma en
flux PERD. C'est de la matière que le semi-lagrangien FABRIQUE.** C'est le défaut
connu du semi-lagrangien près d'une source ré-injectée : quand l'écoulement
diverge, plusieurs cellules remontent leur trajectoire vers la *même* cellule
source et en prennent chacune une copie pleine.

### Ce que cela fait à la lecture de (h1)

`Tmax/ref = 0,3268` ne dit pas « le schéma conservatif a perdu le détail ». Il dit
**« la référence porte 8,7 fois trop de chaleur »**.

Et le rapport des deux rapports le montre :

```
chaleur : le semi-lagrangien en porte  8,72 fois plus
Tmax    : il n'atteint qu'un pic       3,06 fois plus haut
          ————————————————————————————————————————————
à chaleur ÉGALE, le schéma conservatif est 2,85 fois PLUS CONCENTRÉ
```

> **Le « prix » mesuré le 12/09 n'était pas une perte de détail. C'était le
> retrait d'une chaleur qui n'aurait jamais dû exister.** Et c'est pour cela
> qu'aucun schéma d'advection, à aucun ordre, ne pouvait le racheter : il n'y
> avait rien à racheter.

### ⚠️ CE QUI N'EST PAS ENCORE MESURÉ, et je le dis plutôt que de l'omettre

La chaîne ci-dessus est une **déduction corroborée**, pas une mesure directe. La
mesure qui la transformerait en fait est simple et **n'est pas faite** : compter
analytiquement la masse injectée (`n cellules × débit × dt × h³`, sommée sur 600
pas, multipliée par le facteur de dissipation) et la comparer aux **deux** totaux.
Si elle tombe sur `0,016781`, la création par le semi-lagrangien est **mesurée** et
non plus déduite.

⚠️ Et **`Tmax` reste ce qui gouverne la couleur du corps noir**. Dire que 1749,4 K
est un artefact ne rend pas 571,7 K joli à l'écran : cela déplace la question de
« quel schéma d'advection » vers « quelle source, quel `beta`, quelle dissipation
de température » — un réglage de scène, pas un défaut de solveur. **Cette
décision-là appartient à Rodolf**, et je ne la prends pas.

---

## 11. PRÉ-ENREGISTREMENT DE (j1) — LE COMPTAGE ANALYTIQUE

**Écrit le 13/09, AVANT de coder le mode 10 et AVANT de le lancer.** C'est la
mesure dont j'ai moi-même écrit au § 10 qu'elle **manquait** : sans elle, « le
semi-lagrangien fabrique » reste une déduction.

### ⚠️ LA RÈGLE DE CE LOT, ET ELLE EST TOUT LE LOT

> **Le nombre attendu ne se demande JAMAIS au solveur qu'on juge.** Il se calcule
> **À LA MAIN**, dans le banc, depuis les paramètres d'injection et de scène.

Le banc recompte donc lui-même `Nx, Ny, Nz` par `ceil((max − min)/h)`, et
**recompte lui-même** les cellules de la sphère source en refaisant le test
géométrique. Il ne lit du solveur **que la masse qu'il juge**.

⚠️ **Et il GARDE ce recomptage** : si ses `Nx, Ny, Nz` ne sont pas ceux de la
grille, le calcul à la main porterait sur une **autre grille** et le verdict serait
faux tout en ayant l'air juste. Ce contrôle rougit explicitement.

### Le modèle, écrit avant d'être codé

Ordre réel d'un tour de boucle, **lu dans `Step()`** (`NkFluidGrid.cpp`) :

```
EmitSphere      ->  M <- M + A          (avant Step, entre deux pas)
Step : advection                         (le schéma en flux CONSERVE M)
       dissipation  ->  M <- M * f       f = exp(-densityDissipation * dt)
```

d'où, après `n` pas :

```
        n                              1 - f^n
M_n =   S  A * f^(n-k+1)   =   A * f * ---------
       k=1                              1 - f
```

avec `A = N_cellules * débit * dt * h³`, la masse injectée à chaque pas.

**L'advection ne peut pas en retirer** : les faces de paroi ne sont jamais
parcourues, et `TotalMass` ne somme que l'**intérieur** — vérifié dans le code,
pas supposé.

### LE NOMBRE, calculé à la main AVANT la course

Scène (e) : boîte `0,5 × 1 × 0,5 m`, `h = 0,02` → `25 × 50 × 25` ; sphère source
centrée `(0 ; 0,05 ; 0)`, rayon `0,05` ; débit `6` ; `dt = 1/60` ; 600 pas ;
`densityDissipation = 0,2`.

Les centres de cellule tombent sur des multiples de `h` autour du centre de la
sphère, donc le test `dx² + dy² + dz² <= r²` devient, en unités de `h` :
`a² + b² + c² <= 6,25` avec `a, b, c` entiers — soit `<= 6`. Le dénombrement des
triplets entiers donne :

```
somme 0 : 1 · somme 1 : 6 · somme 2 : 12 · somme 3 : 8
somme 4 : 6 · somme 5 : 24 · somme 6 : 24        ->  N = 81 cellules
```

⚠️ **Aucune ambiguïté de virgule flottante** : le dernier accepté vaut
`6 h² = 0,0024` et le premier refusé `7 h² = 0,0028`, contre un seuil de `0,0025`.
Le test n'est pas sur un fil.

```
A     = 81 * 6 * (1/60) * (0,02)³ = 6,48e-05
f     = exp(-0,2/60)              = 0,9966722
f^600 = exp(-2)                   = 0,1353353
M_600 = 6,48e-05 * 0,9966722 * (1 - 0,1353353) / (1 - 0,9966722)
```

> ### ⟹ **M attendue = 0,016781** — et je l'écris ICI, avant la course.

⚠️ **Cette mesure n'est PAS aveugle, et le dire fait partie du travail** : la
valeur `0,016781` a déjà été relevée sur le solveur par l'enquête (i). Ce que (j1)
apporte n'est donc pas la surprise d'un chiffre, c'est que ce chiffre **existe
maintenant deux fois, par deux chemins qui ne se parlent pas** — une arithmétique
de papier et une course. Le calcul ci-dessus a été fait **avant** d'écrire une
ligne du mode 10, et il tombe sur la valeur mesurée ; c'est cette coïncidence-là
qui est le résultat, et elle serait invérifiable si le nombre n'était pas écrit
d'avance.

### Les critères, écrits AVANT

    (j1)  le CONSERVATIF colle a l analytique     ecart relatif < 1e-3
          prediction : quelques 1e-5
    (j1)  le SEMI-LAGRANGIEN s en ecarte          facteur > 5
    (j1b) NEGATIF : une injection UNIQUE, sans dissipation
          -> masse CONSTANTE sur 600 pas, ecart a l analytique NUL
    (j1c) MUTATION : debit attendu fausse de +1 %  -> (j1) DOIT rougir

**D'où vient le seuil `1e-3`, et pourquoi il n'est pas plus serré :** le solveur
travaille en `float32` et multiplie 600 fois par un `f` lui-même arrondi en
`float32`, alors que le comptage à la main utilise la double précision. Le biais
systématique de cet écart de `f` se compose sur 600 pas — de l'ordre de quelques
`1e-5`. Un seuil de `1e-6` ne mesurerait plus la conservation mais la différence
de précision entre deux façons d'écrire la même exponentielle. **Et la marge reste
de quatre ordres de grandeur** face à l'écart du semi-lagrangien (un facteur 15,
soit 1 400 %).

**Pourquoi (j1b) n'est PAS « débit = 0 ».** Couper l'injection donnerait
`0 = 0` : un **témoin nul**, exactement le piège que ce banc traque depuis (f1b).
L'injection **unique** garde un nombre **non nul** à atteindre, et la dissipation
coupée rend l'attendu **exact** au lieu d'approché — la masse doit alors rester
**rigoureusement constante** pendant 600 pas.

### ⚠️ CE QUE JE CONCLURAIS SI LE CONSERVATIF NE COLLAIT PAS NON PLUS

Écrit **avant** la mesure, pour ne pas être tenté de l'expliquer après :

> **Si l'écart du conservatif dépasse lui aussi `1e-3`, alors AUCUNE des deux
> courses ne porte la bonne quantité de matière, et TOUTE la chaîne de déduction
> de Q10 tombe** — y compris la phrase « la référence porte 8,7 fois trop de
> chaleur », qui redeviendrait une hypothèse.

Dans ce cas je ne cherche pas un coupable dans le solveur : je vérifie **d'abord
mon propre comptage** (l'ordre injection/dissipation, le dénombrement `N = 81`,
l'unité de `TotalMass`), parce qu'un instrument neuf qui contredit deux mesures
anciennes est **plus souvent faux** que les deux mesures. **Et je le publierais
comme un résultat, pas comme un incident.**

### Ce que (j1) ne fait pas

Il ne mesure **que la masse de fumée**. Le facteur 8,7 sur la **chaleur** n'est pas
recompté ici : la température subit un rappel vers l'ambiante
(`T <- T_amb + (T - T_amb)*f`) et une combustion éteinte sur cette scène, ce qui en
fait un second modèle — à écrire séparément si Rodolf le demande.
Et il **n'allume rien** : `advectFluxConservative` reste `false`,
`advectFluxLimiter` reste `Ordre1`.

---

## 12. LE RÉSULTAT DE (j1) — LA DÉDUCTION EST DEVENUE UN FAIT

**Mode `NK_FLUID_MAC=a`, 5 contrôles, 0 ROUGE.**

```
COMPTAGE À LA MAIN : grille 25 x 50 x 25 ; 81 cellules dans la sphère ;
A = 6,480000e-05 par pas ; f = exp(-0,2/60) = 0,996672216 ; M = 0,016781081

schéma              masse finale    écart relatif à l'analytique   facteur
ANALYTIQUE (main)    0,016781081    —                              1,000
FLUX ordre 1         0,016781075    3,855e-07                      1,000
FLUX van Leer        0,016781075    3,855e-07                      1,000
SEMI-LAGRANGIEN      0,256143659    1,426e+01                     15,264
```

**Le dénombrement de papier — 81 — est celui que le banc retrouve. Le nombre écrit
d'avance — 0,016781 — est celui que le solveur conservatif rend, à
`3,9e-07` près.** Sept ordres de grandeur séparent les deux schémas de leur
référence commune.

> **Le semi-lagrangien ne PERD pas de la matière sur cette scène : il en
> FABRIQUE.** Ce n'est plus une déduction, c'est un écart mesuré contre un nombre
> calculé **hors de lui**.

Les trois gardes tiennent : la **grille recomptée** est celle du solveur
(`25 × 50 × 25`) ; le **négatif non nul** (injection unique, dissipation coupée)
rend `0,000064800` attendu contre `0,000064800` mesuré, avec une amplitude sur
toute la course de **`0,000e+00` exactement** ; et la **mutation** à `+1 %` de
débit fait passer l'écart de `3,855e-07` à `9,901e-03` — **le compteur sait
rougir.**

### Ma prédiction rate, et pour la première fois dans l'autre sens

J'attendais « quelques `1e-5` », en raisonnant sur le `float32` du solveur composé
sur 600 pas contre la double précision du comptage. Mesuré : **`3,9e-07`**, deux
ordres de grandeur mieux. Après trois sous-estimations d'ampleur (Q4, Q5, Q6) puis
une **surestimation** franche en (h1), c'est la première fois que je me trompe en
étant **trop pessimiste sur la précision**.

Le seuil `1e-3` **n'est pas déplacé** — il reste celui du § 11. Je note seulement
qu'il était quatre ordres de grandeur plus large que nécessaire, et que je l'aurais
su en regardant (f1), qui rendait déjà `0,000e+00` exact.

### Le cas que j'avais écrit d'avance n'a pas eu lieu

Le § 11 prévoyait quoi conclure **si le conservatif ne collait pas non plus** :
toute la chaîne de Q10 serait tombée. Elle ne tombe pas. **L'avoir écrit avant
reste ce qui rend ce vert lisible** — un critère qui ne pouvait pas échouer n'aurait
rien jugé.

---

## 13. PRÉ-ENREGISTREMENT DE (k1) — LE COMPTAGE DE LA CHALEUR

**Écrit le 13/09, AVANT de coder le mode `b` et AVANT de le lancer.**

### Pourquoi, et ce n'est pas la symétrie

La masse prouve que le semi-lagrangien **fabrique de la matière**. Mais l'arbitrage
que Rodolf doit rendre porte sur `Tmax`, donc sur la **chaleur**. Tant que le
facteur **8,7** reste une déduction, *« la référence porte 8,7 fois trop de
chaleur »* est une **inférence sur le nombre qui décide**, alors que le `3,9e-07` a
été obtenu sur l'autre.

### ⚠️ D'ABORD LA QUESTION QU'ON M'A POSÉE : la chaleur EST-ELLE conservative ici ?

Il m'est demandé de **dire non plutôt que de forcer un modèle**. Je réponds
**oui**, et je nomme les **trois préconditions** qui le rendent vrai — lues dans le
code, pas supposées. **Si l'une tombe, le modèle tombe avec elle.**

1. **L'advection conserve `somme T`.** Le schéma en flux est un télescopage sur les
   faces **intérieures** ; les faces de paroi ne sont jamais parcourues. Donc
   `somme T` sur l'intérieur est conservée **exactement**, et comme le nombre de
   cellules intérieures est constant, `H = somme (T − T_amb) · h³` l'est aussi.
2. **La dissipation est une MULTIPLICATION sur `H`, pas sur `T`.** Le code écrit
   `T <- T_amb + (T − T_amb) · f` : c'est exactement `H <- H · f`, avec
   `f = exp(−(temperatureDissipation + coolingRate) · dt)`. C'est ce qui rend la
   même récurrence qu'en (j1) applicable — et ce n'est **pas** une coïncidence de
   forme, c'est la définition d'un rappel exponentiel vers l'ambiante.
3. **La combustion est ÉTEINTE sur la scène (e).** `Combust` sort immédiatement si
   `burnRate <= 0`, et la scène ne le règle pas (défaut `0`). C'est le **seul**
   autre endroit du solveur qui écrit dans `mTemperature`. La flottabilité **LIT**
   `T` et écrit dans la **vitesse** ; la projection ne touche pas `T`.

Et `Reset()` met `T = T_amb` **partout**, donc `H` part de **zéro** — ce que je ne
suppose pas : **je le mesure** (voir (k0)).

> ⚠️ **LA RÉSERVE, et elle est réelle.** Advecter `T` en forme **conservative**
> (`dT/dt + div(u·T) = 0`) n'est physiquement juste que parce que la projection rend
> `div u` quasi nul ; la forme physique est `dT/dt + u·grad T = 0`. La
> **conservation discrète de `somme T` est exacte quoi qu'il arrive** — c'est un
> télescopage — mais **ce qu'elle conserve n'est le bon objet que dans la mesure où
> `div u` est nul**, et le témoin (b) en donne la taille : `0,000026 %`. Je compte
> donc une quantité que le schéma conserve exactement, **en sachant que son sens
> physique repose sur un autre témoin que le mien.**

### LE NOMBRE, calculé à la main AVANT la course

Même récurrence qu'en (j1) — **la même fonction**, avec d'autres constantes, et
c'est précisément parce que les trois préconditions ci-dessus sont vraies :

```
EmitSphere  ->  H <- H + A_T          A_T = N * debitT * dt * h³
Step        ->  advection (conserve), puis H <- H * f_T
```

Scène (e) : `N = 81` cellules (le **même** dénombrement qu'en (j1), refait),
`débitT = 900 K`, `dt = 1/60`, `h = 0,02`, 600 pas,
`temperatureDissipation = 0,5`, `coolingRate = 0`.

```
A_T     = 81 * 900 * (1/60) * (0,02)³ = 9,720e-03   par pas
f_T     = exp(-0,5/60)                = 0,99170129
f_T^600 = exp(-5)                     = 0,00673795
H_600   = 9,720e-03 * 0,99170129 * (1 - 0,00673795) / (1 - 0,99170129)
```

> ### ⟹ **H attendue = 1,15372 K·m³** — écrite ICI, avant la course.

⚠️ **Comme en (j1), la mesure n'est pas aveugle** : l'enquête (i) a déjà relevé
`1,1537` sur le solveur. Ce que (k1) apporte n'est pas la surprise d'un chiffre,
c'est que ce chiffre **existe deux fois, par deux chemins qui ne se parlent pas**.

### Les critères, écrits AVANT

    (k0)  la SOURCE injecte exactement ce que je compte
          apres UN EmitSphere et AUCUN pas : H = 9,720e-03 et M = 6,480e-05
          -> separe « la source injecte ce que je crois » de « le transport conserve »
    (k1)  le CONSERVATIF colle a l analytique     ecart relatif < 1e-3
          prediction : entre 1e-6 et 1e-4, donc PLUS LACHE que le 3,9e-07 de (j1)
    (k1)  le SEMI-LAGRANGIEN s en ecarte          facteur > 5
    (k1b) NEGATIF : injection UNIQUE, dissipation thermique COUPEE
          -> H CONSTANTE sur 600 pas, egale a A_T = 9,720e-03, NON NULLE
    (k1c) MUTATION : debit thermique attendu fausse de +1 %  -> (k1) DOIT rougir

### ⚠️ POURQUOI JE PRÉDIS PLUS LÂCHE QU'EN (j1) — un MÉCANISME, pas une prudence

La densité est stockée autour de **zéro**, où le `float32` est d'une finesse
extrême. La température est stockée autour de **300 K**, et la grandeur qui compte
est `T − 300`. Or près de 300, le quantum du `float32` vaut **`3,05e-05` K** : pour
une cellule à `T = 300,001`, l'excès `0,001` porte déjà **1,5 % d'erreur
relative** — et le solveur refait cette soustraction-recomposition **600 fois**
(`T <- T_amb + (T − T_amb)·f`).

**C'est une perte de chaleur par QUANTIFICATION, dans le solveur lui-même**, que le
champ de masse ne subit pas. Je m'attends donc à un écart plus grand qu'en (j1) —
et **si je me trompe encore en étant trop pessimiste**, ce sera la deuxième fois de
suite, et il faudra en tirer que ce banc est plus précis que je ne le crois.

**Le seuil reste `1e-3`**, le même qu'en (j1) : la séparation d'avec le
semi-lagrangien est un facteur **8,7**, soit **770 %** — trois ordres de grandeur de
marge même au seuil.

**Pourquoi (k1b) n'est pas un `0 = 0`** : couper la source donnerait `H = 0`
partout, un témoin nul. Une injection **unique**, dissipation thermique **coupée**,
laisse un nombre **non nul et exact** (`9,720e-03`) que `H` doit tenir
**rigoureusement constant** pendant 600 pas — et je borne **toute la course**, pas
la seule valeur finale.

### ⚠️ CE QUE JE CONCLURAIS SI LE CONSERVATIF COLLAIT SUR LA MASSE MAIS PAS SUR LA CHALEUR

Écrit **avant** la mesure, pour ne pas l'expliquer après.

**Ce ne pourrait PAS être le transport.** `mDensity` et `mTemperature` passent par
**le même `AdvectScalar`**, avec le même schéma et le même `bnd = 0` : un
télescopage qui conserve l'un conserve l'autre. Un écart sur la chaleur seule
**accuserait donc mes trois préconditions**, dans cet ordre :

1. **la combustion** serait active malgré `burnRate = 0` — le contrôle (k0) et un
   relevé de `TotalFuel` le diraient ;
2. **la dissipation** ne serait pas le rappel multiplicatif que j'ai lu — je
   relirais le bloc, et la relecture serait la mesure ;
3. **un quatrième écrivain de `mTemperature`** existerait, que je n'ai pas trouvé
   en cherchant. C'est l'hypothèse la plus intéressante, et la plus probable si les
   deux premières tombent.

Et si aucune des trois ne rend compte de l'écart, alors **la chaleur n'est pas une
quantité conservée de ce schéma**, le facteur **8,7 reste une déduction**, et le lot
devient : *délimiter ce qui se compte, et dire ce que `Tmax` mesure vraiment.*
**Je publierais cela comme le résultat**, pas comme un incident — et ce serait un
résultat plus lourd que le vert que je vise.

### ⚠️ LA DÉLIMITATION, VRAIE MÊME SI (k1) EST VERT

Elle doit être écrite **maintenant**, parce qu'un vert ne l'effacerait pas.

> **`Tmax` n'est PAS la chaleur.** `H` est une **intégrale**, `Tmax` un **maximum
> ponctuel**. Aucun schéma ne conserve un maximum ponctuel, et **aucun calcul à la
> main ne peut prédire `Tmax`** : il dépend de la manière dont la chaleur est
> **répartie**, pas seulement de sa quantité.

Conséquence, et elle borne exactement ce que ce lot peut prétendre :

- (k1) mesure **le dénominateur** de l'argument de Q10 — *la référence porte-t-elle
  vraiment 8,7 fois trop de chaleur ?* — et **rien d'autre** ;
- (k1) **ne dit pas** ce que `Tmax` « devrait » valoir, ni que `571,7 K` est la
  bonne valeur. Il dit que `1749,4 K` est atteint **avec 8,7 fois trop de chaleur**,
  donc qu'il ne peut pas servir de cible ;
- **ce que `Tmax` mesure vraiment**, sur cette scène : la température de la cellule
  la plus chaude, c'est-à-dire **la concentration de la chaleur au voisinage
  immédiat de la source**, là où le semi-lagrangien recopie la cellule source dans
  plusieurs cellules aval. C'est une grandeur de **répartition**, gouvernée par le
  schéma **et** par le réglage de la source — et c'est pourquoi la question
  « re-régler la scène (e) » est la bonne question suivante, et qu'elle appartient à
  Rodolf.

### Ce que (k1) n'allume pas

`advectFluxConservative` reste `false`, `advectFluxLimiter` reste `Ordre1`. Les
contrôles vivent dans le mode `b`, **hors de la course complète** (trois scènes de
600 pas), comme (j1) : le compte de la course complète reste **87 / 7**.

---

## 14. LE RÉSULTAT DE (k1) — LA CHALEUR SE COMPTE, ET LE 8,7 EST MESURÉ

**Mode `NK_FLUID_MAC=b`, 5 contrôles, 0 ROUGE.**

```
COMPTAGE À LA MAIN : 81 cellules ; A_T = 9,719999e-03 par pas ;
f_T = 0,991701293 ; H attendue = 1,153720232 K·m³

schéma              chaleur (K·m³)   écart à l'analytique   facteur    Tmax
ANALYTIQUE (main)     1,153720232    —                       1,000       —
FLUX ordre 1          1,153720617    3,338e-07               1,000     550,5
FLUX van Leer         1,153721809    1,367e-06               1,000     571,7
SEMI-LAGRANGIEN      10,060710907    7,720e+00               8,720    1749,4
```

**Le `1,15372` écrit d'avance est celui que le solveur conservatif rend.** Les
trois préconditions du § 13 tiennent donc : la chaleur au-dessus de l'ambiante
**est** une quantité conservée de ce schéma.

> **Le facteur 8,720 cesse d'être une déduction.** La référence de (h1) porte bien
> une chaleur **qu'aucune source n'a fournie**, et c'est désormais mesuré contre un
> nombre calculé **hors du solveur**.

### ⚠️ MA PRÉDICTION TOMBE DANS SON PROPRE INTERVALLE — la première fois

Prédit : **`1e-6` à `1e-4`**, plus lâche qu'en (j1), pour un **mécanisme nommé**
(la quantification du `float32` autour de 300 K). Mesuré : **`1,367e-06`**, dans
l'intervalle, à son bord inférieur. Après trois sous-estimations, une
surestimation et un excès de pessimisme, **c'est la première prédiction de cette
série qui tombe juste** — et elle y tombe parce qu'elle était **adossée à un
mécanisme** au lieu d'être une fourchette de confort.

### Le mécanisme est visible, isolé par le contrôle négatif

Le **même** montage négatif, joué sur les deux champs, sépare les deux effets :

```
(j1b) masse   : amplitude sur 600 pas = 0,000e+00   EXACTEMENT
(k1b) chaleur : amplitude sur 600 pas = 1,648e-05
```

**La masse est rigoureusement constante ; la chaleur dérive de `1,6e-05`.** C'est
exactement la quantification annoncée : la densité vit autour de **zéro**, où le
`float32` est fin ; la température vit autour de **300 K**, où son quantum vaut
`3,05e-05` K, et le solveur refait `T <- T_amb + (T − T_amb)·f` **600 fois**. Le
contrôle négatif ne se contente donc pas de verdir : **il mesure l'effet qu'il
était censé exclure.**

Second détail du même ordre : van Leer s'écarte **4 fois plus** que l'ordre 1
(`1,367e-06` contre `3,338e-07`). Le flux antidiffusif ajoute de l'arithmétique par
face, donc des arrondis — la conservation reste exacte **en algèbre**, pas en
`float32`.

### (k0), le contrôle qui manquait à (j1)

```
grille neuve                        H = 0,000e+00   (Reset met T = T_amb : MESURÉ)
après UN EmitSphere, AUCUN pas      H = 0,009720000  contre 0,009719999  (1,122e-07)
                                    M = 0,000064800  contre 0,000064800  (4,628e-08)
```

Il **sépare** « la source injecte ce que je crois » de « le transport conserve » —
deux choses que (j1) jugeait **ensemble**. Les deux attendus sont **non nuls et
exacts**, donc ce n'est pas un témoin nul ; et il valide **rétroactivement** le
dénombrement `N = 81` sur lequel reposait (j1).

### La délimitation du § 13 reste vraie, et ce vert ne l'efface pas

`Tmax` n'est pas la chaleur. Ce lot mesure **le dénominateur** de l'argument de
Q10 — *et rien d'autre*. Il ne dit pas que `571,7 K` est la bonne valeur ; il dit
que `1749,4 K` est atteint **avec 8,72 fois la chaleur injectée**, donc que ce
nombre **ne peut pas servir de cible**. Ce que `Tmax` mesure ici est la
**concentration** de la chaleur au voisinage de la source — une grandeur de
**répartition**, gouvernée par le schéma *et* par le réglage de la scène.

---

## 15. PRÉ-ENREGISTREMENT DE (p) — ALLUMER, PUIS RE-RÉGLER LA SCÈNE

**Écrit le 13/09 après-midi, AVANT de changer la moindre valeur par défaut.**

### (p1) Ce qui change, et ce qui ne doit pas

Deux valeurs par défaut dans `NkFluidGridParams`, **et rien d'autre** :

```
advectFluxConservative   false  ->  true
advectFluxLimiter        Ordre1 ->  VanLeer
```

**Aucun mode du banc n'est affecté**, et c'est vérifiable : (f1), (f2), (f3), (g1),
(g2), (h), (i), (j1) et (k1) posent **explicitement** ces deux champs sur leurs
grilles. Seuls les paliers qui laissent les défauts — masse/divergence, vorticité,
vent, ECS, volutes, rendu — basculent. **C'est exactement le but.**

### ⚠️ CE QUI ÉTAIT CALÉ SUR LA CHALEUR INVENTÉE — nommé AVANT de courir

C'est la partie qui m'est demandée, et la seule qui puisse me contredire.

**Certains de basculer au VERT, par mécanisme et non par espoir :**

| témoin | aujourd'hui | pourquoi il DOIT basculer |
|---|---|---|
| **(a)** masse, boîte close | ROUGE, `-43,1032 %` | le flux conserve **par construction** ; (f1), (j1) et (k1) le prouvent à `1e-7` |
| **(v3)** la masse n'empire pas | ROUGE, A `-27,9 %` / B `-41,8 %` | les deux dérives tombent à ~0, donc `B - A` ~ 0 point, sous les 5 exigés |

**Certain de NE PAS bouger, et c'est le contrôle de ma propre annonce :**

| ancrage | valeur | pourquoi il est immunisé |
|---|---|---|
| enstrophie | **1,458000** | contrôle **positif** sur une rotation solide **SANS AUCUN scalaire** : aucun champ advecté n'y entre |

**Attendus qui VONT bouger** — ils étaient mesurés sur un champ de vitesse nourri
par la chaleur inventée :

- **(d) `0,081 cellule`** et **(w1)/(w2) `1,9961`** : le centroïde d'un schéma
  **conservatif** en vitesse uniforme avance à **exactement `u`**. Je prédis donc
  qu'ils restent **VERTS**, `(d)` à `<= 0,081` et `(w2)` à `2,00 ± 0,02`, avec des
  déplacements absolus différents ;
- **(b) `0,000026 %`** : la poussée change, donc le champ de vitesse change. Reste
  vert, **valeur différente** ;
- **(c2)** et **(e)** : restent verts, avec `vmax` qui tombe de `7,782` à ~`3,2`.

**Les DEUX que je désigne comme à RISQUE de devenir ROUGES :**

1. **(2.1) le contraste de la colonne**, aujourd'hui **SATURÉ** (bords à `0,00`).
   Le donor-cell **diffuse** : si la fumée atteint les bords de l'image, le rapport
   chute et peut passer sous `1,5` ;
2. **(3.5) la couleur du corps noir**, aujourd'hui `Tmax 1836 K` sur la scène de
   combustion. Si `Tmax` y tombe comme il est tombé sur (e), l'émission s'effondre
   et le compte de pixels émissifs avec elle.

**Mon annonce chiffrée : 7 ROUGES -> 5**, bande honnête **5 à 7** selon que les deux
à risque basculent. **Si un témoin que je n'ai pas nommé change de couleur, mon
analyse était fausse**, et c'est ce résultat-là qu'il faudra publier.

⚠️ **Je ne toucherai PAS à la colorimétrie du corps noir pour rattraper l'image.**
Si la flamme est sombre à 1500 K, c'est une question d'exposition ou de rendu —
**je le dirai, je ne le compenserai pas.**

### (p2) LA CIBLE PHYSIQUE, et sa source, écrites AVANT la première course

⚠️ **D'abord une honnêteté de vocabulaire** : la scène (e) **n'est pas une
flamme**. Elle n'a **aucun carburant** (`burnRate = 0`, `fuel = 0`) : c'est un
**panache de fumée chaude** entretenu par une source de chaleur. Son `Tmax` est la
température de la **source**, pas une température de combustion. La scène qui
brûle vraiment est celle du palier ③ (`ConstruirePanache(avecFeu = true)`, avec
`burnRate = 9` et `heatPerFuel = 900`).

Cela dit, la cible demandée est celle d'un feu, et elle est légitime : (e) est la
scène sur laquelle tout le chantier a mesuré.

> ### CIBLE : `Tmax = 1500 K`, bande d'acceptation **[1350 ; 1650] K** (± 10 %)

**Source** : la zone de flamme continue d'un feu de nappe d'hydrocarbure se situe
vers **1200-1500 K** (Drysdale, *An Introduction to Fire Dynamics*) ; les mesures
de McCaffrey (1979) sur l'axe d'un panache de feu donnent un plateau de l'ordre de
**1100-1200 K** dans la zone continue. Je prends **1500 K**, haut de cette plage et
milieu de la bande `1200-1900 K` que le lot nomme. **Le dépôt porte déjà cet ordre
de grandeur** : le commentaire de `ConstruirePanache(avecFeu)` calcule un équilibre
à ~1760 K et le qualifie d'« ordre de grandeur d'une VRAIE flamme ».

### La RÈGLE de réglage, écrite avant d'en connaître le résultat

**Je n'agis que sur le DÉBIT D'INJECTION** (`EmitSphere`, le terme `K/s`), **jamais
sur un facteur cosmétique appliqué à `T`**, jamais sur `beta`, jamais sur la
dissipation.

    echelle geometrique sur le debit thermique, a partir des 900 K/s actuels :
        x1  x2  x4  x6  x8  x12  x16
    REGLE : on retient le PLUS PETIT debit dont le Tmax tombe dans [1350 ; 1650] K.
    Si aucun n y tombe, on publie la courbe et on dit que la cible est hors
    d atteinte par ce levier -- on n en change pas.

**Pourquoi le plus petit** : un débit plus grand que nécessaire monte le CFL, donc
le nombre de sous-pas, donc le coût — et rien d'autre.

### Ma prédiction, calculée depuis le mécanisme

Linéairement, atteindre `1500 K` depuis `571,7 K` demanderait
`(1500 − 300) / (571,7 − 300) = 4,417`. **Mais le couplage l'interdit** : plus de
chaleur donne plus de poussée, donc plus de vitesse, donc plus de transport —
`Tmax` croît **moins vite que le débit**.

> **Je prédis un facteur entre 6 et 12**, donc la ligne retenue sera `×8` ou `×12`.

**VOLET NÉGATIF, obligatoire** : à **débit thermique nul**, `Tmax` doit valoir
**exactement l'ambiante, 300,0 K** — rien n'injecte, et la dissipation ne fait que
rappeler vers l'ambiante. Et la ladder doit être **strictement monotone** : un
débit qui ne déplace pas `Tmax` signifierait que je ne tourne pas le bon bouton.

### (p3) La masse et la chaleur restent exactes APRÈS re-réglage

(j1) et (k1) rejoués **sur la scène neuve**, mêmes seuils (`< 1e-3`), sans les
déplacer.

⚠️ **ET UNE DETTE QUE JE PAIE DANS CE LOT** : le débit vit aujourd'hui **en dur à
deux endroits** — dans `SceneDixSecondes` et dans les deux comptages analytiques.
Changer l'un sans l'autre ferait **mentir le compteur en silence**, et il
verdirait. Le débit devient donc **une constante nommée**, lue par la scène **et**
par le comptage. C'est la seule façon que le calcul à la main ne puisse pas dériver
de la scène qu'il juge.

---

## 16. LE RÉSULTAT DE (p) — MON ANNONCE TIENT SUR LES CHIFFRES, ET RATE SUR LA FRAGILITÉ

### (p2) L'échelle, et la règle appliquée mécaniquement

```
facteur  débit K/s     Tmax     vmax  CFL max  sous-pas  ms/pas
x 0.0          0.0    300.1    0.200    0.312         1    79.5
x 1.0        900.0    571.7    3.222    2.806         4   107.5
x 2.0       1800.0    733.6    4.158    3.573         4   113.3
x 4.0       3600.0    992.3    5.293    4.676         6   139.6
x 6.0       5400.0   1212.0    6.078    5.331         6   130.7
x 8.0       7200.0   1406.1    6.694    5.882         7   148.9
x12.0      10800.0   1770.5    7.677    6.661         8   160.5
x16.0      14400.0   2090.4    8.443    7.281         9   180.4
```

`×6` rend `1212,0 K` (hors bande), `×8` rend `1406,1 K` (dedans). **RETENU : ×8,
soit 7200 K/s.** La prédiction annonçait `×6` à `×12` : **elle tient**. Le linéaire
aurait dit `×4,417` ; l'écart est le couplage poussée/transport, annoncé.

### ⚠️ (p2-) LE CONTRÔLE NÉGATIF EST ROUGE, ET IL A TROUVÉ QUELQUE CHOSE

À débit thermique **nul**, `Tmax = 300,1328 K` au lieu de `300,0000`. **Le seuil
n'est pas déplacé.** Et le contrôle d'isolement tranche la cause :

```
avec source de masse (donc un écoulement)   vmax 0,200   Tmax 300,1328
AUCUNE injection (donc AUCUN écoulement)    vmax 0,0000   Tmax 300,0000  EXACT
```

> **C'est la réserve écrite au § 13 AVANT (k1), et elle cesse d'être théorique.**
> Le flux transporte `T` **comme une densité** : le bilan d'une cellule vaut
> `−T·(div u)·dt`. Comme `div u` n'est nul qu'à `1e-5` près, **un écoulement suffit
> à créer de la chaleur que personne n'a injectée** — ici `0,13 K`, soit `0,044 %`
> de l'ambiante.

⚠️ **Cela ne contredit PAS (k1)**, et la cohérence est instructive : le terme
parasite est de **signe variable** et se compense presque dans l'intégrale, donc
`H` reste juste à `1,6e-06` — pendant que `Tmax`, qui est un **maximum ponctuel**,
en garde la trace. **C'est la démonstration expérimentale de la délimitation du
§ 13 : `Tmax` n'est pas la chaleur.**

### (p3) La masse et la chaleur restent exactes après re-réglage

```
CHALEUR, scène re-réglée (7200 K/s) :
ANALYTIQUE (main)     9,229761858    —
FLUX ordre 1          9,229775429    1,470e-06
FLUX van Leer         9,229776382    1,574e-06
SEMI-LAGRANGIEN      46,925403595    4,084e+00   x 5,084
```

Le comptage à la main **a suivi la scène** : c'est la dette de la constante nommée,
payée et vérifiée.

⚠️ **ET UN SEUIL QUI PASSE DE JUSTESSE, que je signale plutôt que d'en profiter** :
le facteur du semi-lagrangien tombe de **8,720 à 5,084**, pour un seuil écrit à
**5,0**. Il passe, mais de peu — et il aurait échoué à un débit plus élevé. **Je ne
le déplace pas** ; je dis qu'il a été écrit pour l'ancienne scène et qu'il est
désormais **marginal**. Observation associée, sans prétendre l'expliquer : plus la
source est chaude, **moins** le semi-lagrangien fabrique proportionnellement.

### ⚠️ (p1) LE COMPTE : 87 contrôles, 7 ROUGES — ET MON ANALYSE ÉTAIT FAUSSE

**Ce qui tient :**

| annoncé | mesuré |
|---|---|
| (a) masse → VERT | **VERT** (`+0,0000 %`) |
| (v3) masse → VERT | **VERT** |
| enstrophie `1,458000` inchangée | **`1,458000`** |
| (d) `<= 0,081 cellule` | **`0,019`** |
| (w2) `2,00 ± 0,02` | **`2,0015`** |
| bande de ROUGES `5 à 7` | **7** |

**Ce qui ne tient pas, et c'est le résultat à publier.** J'avais écrit : *« si un
témoin que je n'ai pas nommé change de couleur, mon analyse était fausse ».*
**Deux** ont changé, et **aucun des deux que j'avais nommés n'a bougé** :

| témoin | ce que j'avais dit | ce qui s'est passé |
|---|---|---|
| **(2.1)** contraste | **à risque** | VERT, centre `223,14` |
| **(3.5)** corps noir | **à risque** | VERT, `6 152 px` émissifs |
| **(v6)** concentration | *non nommé* | **ROUGE** : `A 4,339 -> B 4,169`, `x 0,961` (seuil `1,050`) |
| **GARDE de (a)** paroi | *non nommé* | **ROUGE** : masse paroi `2,130e-05` (seuil `1,512e-07`) |

**Et la GARDE de (a) est un quatrième témoin HORS DE SON RÉGIME**, après (c1),
(v4) et (2.1). Sa raison d'être était de vérifier que le vert de (a) n'était pas
obtenu parce que le semi-lagrangien **détruisait** la fumée au contact de la paroi
(mesuré `−100 %` le 05/09). Avec le schéma en flux, **aucun flux ne traverse une
paroi** : toucher le mur est devenu **inoffensif**, et (a) lui-même le prouve en
étant vert. Le garde protège donc contre un danger que le correctif a supprimé.
**Je ne le touche pas** — je le nomme, et la décision revient à Rodolf.

**(v6), lui, est un vrai résultat négatif** : avec un transport conservatif, le
confinement de vorticité **ne concentre plus** la vorticité sur cette scène
(`×0,961` au lieu de `×1,156`). Avec (v1) (`×0,49`, seuil `×2,00`), cela fait
**deux témoins du confinement qui disent qu'il ne fait plus son travail** — et
c'est un chantier, pas un détail.
