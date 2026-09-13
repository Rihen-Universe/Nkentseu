<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN DE L'ADVECTION CONSERVATIVE EN FLUX — pré-enregistrement

**Écrit le 2026-09-12, AVANT la première ligne de code et AVANT la première
mesure.** Même raison d'être que `PLAN_GRILLE_MAC.md` : un critère écrit après la
mesure ne juge plus rien, et un schéma qui change **tous** les chiffres du banc
sans avoir dit d'avance lesquels ont le droit de changer rend impossible de
distinguer une **régression** d'un **prix consenti**.

Référence : Michael LENTINE, Mridul AANJANEYA & Ronald FEDKIW, « Mass and
Momentum Conservation for Fluid Simulation », *Symposium on Computer Animation*
2011. Le schéma de base est le **donor-cell** (décentrement amont) de Courant,
Isaacson & Rees, 1952.

---

## 0. POURQUOI MAINTENANT, ET PAS AVANT

**Un flux vit sur une FACE.** Tant que la grille était colocalisée, il n'existait
aucun endroit où poser `u · φ` : la vitesse et le scalaire habitaient le même
point, et « ce qui traverse la frontière entre deux cellules » n'avait pas de
support. La bascule MAC (commit `135bc76ec`) est donc le **prérequis** de ce lot,
et c'est elle qui le débloque.

C'est le dernier rouge **structurel** du banc : la masse. Mesuré et redit trois
fois, la perte est **SPATIALE** (diviser le pas de temps par 4 ne la change pas),
et son correctif était nommé depuis le 05/09 sans être fait.

---

## 1. 🎯 (f1) LE CONTRÔLE NÉGATIF — vert au premier jet, ou le portage est faux

**Montage, et il n'est pas négociable :** un champ de vitesse à divergence
**discrètement nulle**, des parois fermées, **aucune source**, aucune dissipation.
La masse doit alors se conserver **à l'epsilon machine**.

Le champ ne sera pas « à peu près » à divergence nulle : il sera **dérivé d'une
fonction de courant** posée sur les arêtes, ce qui rend la divergence MAC
**exactement** nulle, au bit près :

```
u(i,j) = -( psi(i,  j+1) - psi(i,j) ) / h
v(i,j) = +( psi(i+1,j  ) - psi(i,j) ) / h

div*h = (u(i+1,j)-u(i,j)) + (v(i,j+1)-v(i,j))  ==  0   identiquement
```

avec `psi = sin^2(pi x/L) · sin^2(pi y/L)`, nulle sur tout le bord — donc
`u · n = 0` aux parois, sans avoir à l'imposer.

**Pourquoi ce contrôle décide de tout :** un schéma en flux est conservatif **par
construction** — on calcule UN flux par face, on le retranche à une cellule et on
l'ajoute à l'autre. Si la masse n'est pas conservée sur ce montage, ce n'est pas
une question de précision : c'est que le portage est **faux** (un flux compté deux
fois, une face oubliée, un bord mal traité).

**Seuil, fixé maintenant :** dérive relative de masse **< 1e-5** sur 200 pas.
Ce n'est pas « zéro » parce que l'accumulation se fait en simple précision : chaque
flux apparaît deux fois avec des signes opposés, mais la somme de 200 pas × N
cellules en `float32` porte son propre arrondi. **1e-5 est le bruit de la simple
précision, pas une tolérance de confort** — le semi-lagrangien actuel, lui, perd
des **pourcents** sur ce même montage.

> **(f1) doit être ROUGE avant le portage et VERT après.** Le semi-lagrangien ne
> conserve pas la somme : son interpolation trilinéaire redistribue sans bilan.

---

## 2. ⚠️ (f2) LE PRIX, ÉCRIT AVANT D'ÊTRE MESURÉ

Le donor-cell d'ordre 1 est **franchement plus diffusif** que le semi-lagrangien.
Il va étaler la fumée. La question n'est donc pas « la masse se conserve-t-elle »
— elle se conservera, c'est une propriété du schéma — mais **à quel prix**.

> ⚠️ **Conserver la masse d'une fumée qui ne tourbillonne plus n'a AUCUNE
> valeur.** Rodolf veut voir du feu, pas un bilan comptable. Si la masse verdit en
> tuant le détail, **c'est un ÉCHEC, et il faut le dire** au lieu d'annoncer un
> témoin vert.

### 🔒 Ce qui n'a PAS le droit de bouger

| ancrage | valeur du 12/09 | pourquoi |
|---|---|---|
| **enstrophie, rotation solide : 1,458000** | 1,458000 m³/s² | ⚠️ **INCHANGÉ AU DERNIER CHIFFRE.** Il mesure `rot(u)` sur un champ **imposé**, sans aucun scalaire. L'advection en flux ne touche que densité, température et carburant : cet ancrage **ne peut pas** bouger. S'il bouge, c'est une régression, pas un prix |
| écart de l'enstrophie sur la rotation solide | **0,0000 %** | idem |
| **(b) divergence après projection** | 0,000026 % | ne dépend d'aucun scalaire |
| **(m1) et (m2)** | 1,192e-07 / 4,773e-08 / 2,000000 | ne dépendent d'aucun scalaire |
| masse 0,064000003, barycentre, rayon 0,08483 | — | compteurs, pas schémas |
| toute la chaîne colorimétrique | E, Wien, D65, rampe refusée | aucun rapport |
| **pont ECS** | 8/8 | aucun rapport |

### 🎯 Ce qui DOIT changer — c'est le but

| témoin | valeur du 12/09 | attendu |
|---|---|---|
| **(a) masse conservée** | **−43,1032 %** | **doit passer sous 1 %** — c'est tout l'objet du lot |
| **(v3) masse sous confinement** | −27,879 % / −41,840 % | doit s'effondrer aussi |

### 🔁 Ce qui a le droit de payer, et de combien — les bornes sont posées ICI

| témoin | valeur du 12/09 | plancher consenti |
|---|---|---|
| **enstrophie du PANACHE (A)** | **19,196880** | **>= 9,6** (perte de 50 % max). En dessous : le détail est tué, **ÉCHEC** |
| **(v6) concentration P(A)** | **4,078** | **>= 2,0**. En dessous, la vorticité ne se rassemble plus : **ÉCHEC** |
| **(2.1) contraste du rendu** | **999,00** ⚠️ *saturé* | **> 10**. ⚠️ 999 est une **valeur de saturation** (bords à 0,00 exactement), donc peu informative : c'est le plancher de 10 qui juge, pas la chute depuis 999 |
| **(d) transport** | **0,081 cellule** | **< 0,5 cellule**. Le donor-cell diffuse symétriquement, donc le barycentre ne doit pas dériver — seulement s'étaler |
| **(v2) rayon du panache** | × 2,29 | **>= 1,15** (le seuil déjà écrit) |
| **(2.2) atténuation** | 1,37 | **> 1,00** |
| coût (ms/pas) | 242,82 | **change, et ne se compare à rien** tant que la mesure n'est pas prise à charge machine égale |

### ⚠️ L'ORDRE DE MESURE, et c'est une condition de lisibilité

**Mesurer le PREMIER ORDRE NU — donor-cell sans limiteur, sans MacCormack —
AVANT d'ajouter quoi que ce soit.** Sinon on ne saura jamais ce que le limiteur a
rendu, ni ce qu'il a coûté. Le premier ordre est le **plancher de qualité** du
schéma ; tout ce qui vient après se juge par rapport à lui, pas par rapport au
semi-lagrangien.

---

## 3. ⚠️ (f3) LA STABILITÉ CHANGE DE NATURE — et il faut la nommer

Le semi-lagrangien de Stam est **inconditionnellement stable** : c'est sa raison
d'être, et c'est ce qu'on abandonne. Un flux explicite ne l'est pas. La condition
attendue est celle de Courant-Friedrichs-Lewy :

```
CFL = ( |u| + |v| + |w| ) · dt / h  <=  1
```

**Ce qu'il faut mesurer et ÉCRIRE : à quel `dt` ça casse**, et quel CFL cela
représente. On fait croître `dt` jusqu'à la rupture (NaN, ou masse qui part), et
on publie le dernier `dt` sain et le premier `dt` cassé, avec leur CFL.

> **Nommée, c'est une propriété connue du schéma. Tue, c'est une régression de
> robustesse.** La différence entre les deux est un tableau de trois lignes.

Et elle a une conséquence pratique qu'il faut dire : à `dt = 1/120 s` et
`h = 0,03 m`, une vitesse de **3,6 m/s** suffit à atteindre CFL = 1. La course (e)
monte à **7,782 m/s**. **Le schéma en flux pur ne passera donc pas (e) tel quel**,
et il faudra soit sous-cycler, soit le dire.

---

## 4. 🧭 L'ORDRE DES GESTES

1. **Ce pré-enregistrement, commité seul** — avant toute ligne de code.
2. **(f1) écrit et ROUGE** sur le schéma actuel. Un contrôle qui naîtrait vert ne
   dirait pas s'il juge le flux ou s'il juge que « ça compile ».
3. **Le donor-cell NU**, derrière un interrupteur éteint par défaut. (f1) doit
   virer au vert. **Rien d'autre n'est touché.**
4. **(f2) mesuré sur le premier ordre nu** — la course complète, confrontée ligne
   à ligne aux trois tableaux du § 2.
5. **(f3) le tableau de stabilité.**
6. **Seulement ensuite**, et seulement si le § 2 le justifie : un limiteur.

⚠️ **Chaque étape se commit séparément**, et l'étape 3 doit rendre son verdict
avant l'étape 6. **Couper au point où le dépôt est cohérent et jugé** : (f1) seul,
commité et rapporté, vaut mieux qu'un portage complet non mesuré.

---

## 5. ⛔ CE QUE CE PLAN NE PROMET PAS

- **Pas la vitesse.** Elle reste semi-lagrangienne ; ce lot ne touche que les
  scalaires. La quantité de mouvement conservative (Lentine 2011, § 4) est une
  autre question, et elle n'est pas ouverte ici.
- **Pas (c1) ni (v4).** Les deux reformulations proposées le 12/09 attendent
  Rodolf, et ce lot ne les attend pas — il ne les touche pas non plus.
- **Le seuil de (a) NE BOUGE PAS.** < 1 %, tel qu'il a toujours été écrit.
- **Pas le GPU, pas le temps réel.**
- Et il ne promet pas que le schéma **réussira** : il promet que le jour où il
  sera mesuré, **on saura lire le résultat** — y compris pour dire que le prix est
  trop cher.
