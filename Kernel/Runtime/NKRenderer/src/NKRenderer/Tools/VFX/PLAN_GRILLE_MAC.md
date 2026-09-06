<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen -->

# PLAN DE BASCULE VERS LA GRILLE DÉCALÉE (MAC) — pré-enregistrement

**Écrit le 2026-09-07 à 01h, AVANT la première ligne de code de la bascule et
AVANT la première mesure.** C'est sa seule raison d'être : un critère écrit après
la mesure ne juge plus rien, et une bascule qui change **tous** les chiffres du
banc sans avoir dit d'avance lesquels doivent changer rend impossible de
distinguer une **régression** d'un **déplacement légitime**.

Référence : Francis H. HARLOW & J. Eddie WELCH, « Numerical Calculation of
Time-Dependent Viscous Incompressible Flow of Fluid with Free Surface »,
*Physics of Fluids* **8** (1965), p. 2182-2189. C'est la grille qu'emploient
Fedkiw, Stam & Jensen 2001 ; le code de démonstration de Stam 1999, lui, est
colocalisé comme le nôtre aujourd'hui.

---

## 0. POURQUOI, EN UNE MESURE

Sur la grille colocalisée, la projection résout `lap(p) = -div` avec le Laplacien
**compact** (pas 1), mais la divergence **centrée** de la vitesse corrigée fait
apparaître un Laplacien de **pas 2**. Les deux opérateurs ne sont pas adjoints :
la grille se découple en sous-réseaux pair/impair — le mode « damier ».

**Ce n'est pas une hypothèse, c'est mesuré deux fois**, et la seconde fois avec le
confinement armé :

| | résidu du Poisson | `|div|·h/|u|` STRICT |
|---|---|---|
| ε = 0, borne 200 / 1e-4 | 3,175e-06 | **5,707591 %** |
| ε = 0, borne 4000 / 1e-8 | 3,120e-08 (÷ 102) | **5,706847 %** |
| ε = 8, borne 200 / 1e-4 | 3,556e-06 | **9,473474 %** |
| ε = 8, borne 4000 / 1e-8 | 4,414e-08 (÷ 81) | **9,472982 %** |

**Le résidu s'effondre de deux ordres de grandeur et le rapport ne bouge pas d'un
dix-millième.** Le remède ne sera donc jamais « plus de balayages ».

---

## 1. 🎯 LE CRITÈRE DÉCISIF DE LA BASCULE — écrit avant, et il ne bougera pas

Sur une grille décalée, l'opérateur de divergence (compact, sur les faces) et le
Laplacien de pression sont **adjoints exacts**. La divergence résiduelle après
projection cesse donc d'avoir un **plancher de discrétisation** : elle devient
bornée par la **tolérance du solveur**, et rien d'autre.

> **La contre-épreuve du plancher DOIT CHANGER DE VERDICT.** Aujourd'hui, resserrer
> la tolérance ne fait pas bouger le rapport. Après la bascule, elle **doit** le
> faire bouger.

**Seuil, fixé maintenant, sur la MÊME grille allégée et les MÊMES 40 pas que
`PlancherDuSolveur` :**

```
rapport(borne 200, 1e-4)  /  rapport(borne 4000, 1e-8)   >=  10
```

Aujourd'hui ce quotient vaut **1,00013** (ε = 0) et **1,00005** (ε = 8).
C'est un critère qui **ne peut pas être satisfait par la grille actuelle** — et
c'est ce qui en fait un critère et pas une décoration.

⚠️ **Ce critère ne dit rien de la MASSE.** L'advection reste semi-lagrangienne à
interpolation trilinéaire : la perte de masse de −45,9 % est **spatiale** (mesuré
le 05/09 : diviser le pas de temps par 4 ne la change pas). **La grille décalée
n'est pas censée la corriger**, et l'annoncer serait promettre ce qu'on ne livre
pas. Son correctif reste nommé ailleurs : advection **conservative en flux**
(Lentine, Aanjaneya & Fedkiw 2011).

---

## 2. ⚠️ LES CONTRÔLES D'INSTRUMENT, REFAITS DANS LE NOUVEAU REPÈRE

*« Sinon tu mesureras l'interpolation et pas la physique. »* Les six contrôles
actuels ne se transportent pas tels quels : trois vivent sur des champs
**scalaires** (qui ne bougent pas), trois sur la **vitesse** (qui déménage sur les
faces). Ce tableau dit, pour chacun, **ce qui doit rester identique au dernier
chiffre** et **ce qui doit être réécrit**.

| contrôle | où vit son sujet | après la bascule |
|---|---|---|
| POSITIF du compteur de **masse** (0,064000003) | densité, **centres** | **identique au dernier chiffre** — ancrage |
| NÉGATIF « le bord n'est pas compté » | densité, centres | **identique** |
| POSITIF du **barycentre** (± 1 cellule) | densité, centres | **identique** |
| NÉGATIF du barycentre (grille vide → false) | — | **identique** |
| POSITIF du **rayon de giration** (gaussienne, écart 0,03 %) | densité, centres | **identique au dernier chiffre** — ancrage |
| NÉGATIFS du rayon (une cellule ; tranche vide) | densité, centres | **identiques** |
| POSITIF du compteur de **divergence** (`u = a·x` → `a·h`) | **vitesse → FACES** | **valeur attendue INCHANGÉE (0,150000 m/s), MONTAGE réécrit** : le champ se pose sur les faces x, pas aux centres. La différence compacte est **exacte** sur un champ linéaire, donc la valeur ne doit pas bouger — si elle bouge, c'est le montage qui est faux, pas la physique |
| NÉGATIF de la divergence (champ uniforme → 0) | vitesse → faces | **doit devenir exactement 0** (il l'est déjà) |
| POSITIF de l'**enstrophie** (rotation solide, écart **0,0000 %**) | **ω → ARÊTES** | **l'écart doit RESTER 0,0000 %** : le champ est linéaire, la différence compacte est exacte comme l'était la centrée. **Si l'écart cesse d'être nul, c'est l'instrument qui a bougé, pas la physique** — c'est la garde la plus sensible des sept |
| POSITIF de l'enstrophie, **valeur absolue** (1,458000 m³/s²) | ω → arêtes | ⚠️ **DOIT CHANGER** : la population comptée n'est plus la même (arêtes, pas centres). **À recalculer analytiquement AVANT de mesurer**, jamais à ajuster après |
| NÉGATIF de l'enstrophie (champ uniforme → 0) | ω → arêtes | **identique (0,000e+00)** |

### 🆕 Deux contrôles qui n'existent pas et qui doivent naître avec la bascule

**(m1) LE CONTRÔLE D'INTERPOLATION FACE → CENTRE.** C'est le chemin neuf le plus
dangereux : tout ce qui lit la vitesse au centre d'une cellule (flottabilité,
vorticité, statistiques, rendu, vent) passera désormais par une moyenne de deux
faces.
- **positif** : sur un champ **linéaire**, la moyenne des deux faces vaut
  **exactement** la valeur au centre → exiger l'égalité à l'epsilon machine ;
- **négatif** : sur un champ **quadratique**, l'erreur doit être **non nulle** et
  valoir `h²/8 · f''` → l'exiger, avec sa valeur analytique. *Sans ce second
  volet, le positif ne prouverait que « ça compile ».*

**(m2) LE CONTRÔLE POSITIF DU DAMIER — le seul qui prouve que la bascule a servi.**
Poser sur les faces un champ en damier (+1/−1 alterné) et exiger du compteur de
divergence une valeur **non nulle et connue**.
> Aujourd'hui, ce même champ rend **zéro** : la divergence centrée est **aveugle**
> au mode damier. C'est exactement l'aveuglement qu'on supprime.

C'est donc un critère qui **doit rougir sur la grille actuelle et verdir après** —
la seule forme qui distingue « j'ai changé de grille » de « j'ai changé de grille
pour quelque chose ».

---

## 3. ⚠️ CE QUI DOIT CHANGER DE VALEUR, ET POURQUOI — dit AVANT

*« Sans quoi on ne saura plus distinguer une régression d'un déplacement
légitime. »* Les valeurs de référence sont celles de la course du **2026-09-07**,
reproduites deux fois au dernier chiffre.

### 🔒 CE QUI NE DOIT PAS BOUGER (les ancrages)

Tout ce qui ne touche **aucune vitesse**. Si l'un de ceux-là change, c'est une
régression, pas un déplacement :

- les **7 contrôles scalaires** ci-dessus ;
- le contrôle **négatif du rendu** : densité nulle → **0 pixel** différent du fond
  sur 172 800 ;
- toute la **chaîne colorimétrique** : point blanc E (0,3331 / 0,3335), Wien
  **0,0079 %**, D65 (0,3134 / 0,3237), et le refus de la rampe linéaire (0,469) ;
- le **pont ECS** (`NkFluidEcsProbe`) : **8/8**, dont (e2) erreur cumulée
  **0,00e+00 m** et (e5)/(e6) le ramassage ;
- le **registre** : (r3) débit **× 1,9991**, (r4) **0,00e+00 m**, (r8), (r9) ;
- **(w2) le rapport des masses : 1,9966.** La division par la masse ne dépend pas
  de la discrétisation — si ce chiffre bouge, c'est le branchement du vent sur les
  faces qui est faux.

### 🔁 CE QUI DOIT CHANGER — avec le sens attendu quand je peux le prédire, et le mot « je ne prédis pas » quand je ne peux pas

| témoin | valeur du 07/09 | ce qui doit se passer |
|---|---|---|
| **(b) divergence stricte après projection** | **0,417057 %** | **DOIT S'EFFONDRER** et devenir **fonction de la tolérance** (§ 1). C'est le but de la bascule |
| **le plancher du solveur** | rapport constant (**quotient 1,00013**) | **DOIT devenir > 10** — le critère décisif |
| **(v4) divergence sous confinement** | 0,39252 % → **6,32962 %** | **DOIT s'effondrer aussi** : c'est toute la thèse, la divergence × 16 vient de la grille et pas du confinement. **Si elle ne s'effondre pas, ma thèse est fausse et il faut le dire** |
| **(a) masse conservée** | **−45,8798 %** | **change, sens NON PRÉDIT.** La vitesse du rebours vient maintenant d'une interpolation de faces. Je ne prédis pas le signe du déplacement — l'inventer serait pire que l'ignorer |
| **(v3) masse sous confinement** | −29,331 % / −42,491 % | idem, **sens non prédit** |
| **(c1) Boussinesq** | écart **0,0002 %** | **doit rester < 1 %**. ⚠️ S'il passe de 0,0002 % à quelque chose de visible, **c'est l'interpolation face → centre qu'on mesure, pas la loi** — d'où (m1) |
| **(d) transport** | **0,070 cellule** | **doit rester ≈ 0,07** : un champ uniforme est exactement représentable sur les faces et son interpolation est exacte. **S'il empire, c'est l'interpolation** |
| **enstrophie, valeurs absolues** (A 11,4899 · B 5,0291 · C 12,1555) | | **changent** : autre population (arêtes). ⚠️ **Ce sont les RAPPORTS qui doivent survivre**, pas les valeurs — (v6) **× 1,384** et (v6b) **× 0,422** |
| **(v2) rayon du panache** (× 1,85) et **(i1) rayon en pixels** (× 2,46) | | **changent** : la scène n'évolue plus pareil. **Les deux doivent rester > 1,15**, et **(i1) doit rester du même ordre que (v2)** — deux instruments indépendants qui divergeraient seraient un signal |
| **paliers ② et ③** (74,02 · 1,34 · les tiers froid/chaud) | | **changent** : ce sont des scènes, pas des lois. Leurs **contrôles**, eux, sont dans les ancrages |
| **coût (ms/pas)** | 130-230 | **change**, et **ne se compare à rien** tant que la mesure n'est pas prise à charge machine égale |

---

## 4. 🧭 L'ORDRE DES GESTES

1. **(m1) et (m2) d'abord, seuls** — les deux contrôles neufs, avant que la
   moindre équation ne bouge. Ils se vérifient sur des champs analytiques et ne
   dépendent d'aucun solveur.
2. **Le stockage et les bords** : `u` sur les faces x `(nx+1)·ny·nz`, `v` sur les
   faces y, `w` sur les faces z ; les scalaires **ne bougent pas**.
3. **La projection** — c'est elle qui porte tout le gain. Mesurer le critère
   décisif du § 1 **ici**, avant de toucher à l'advection.
4. **L'advection**, chaque composante rebroussée depuis **sa** face.
5. **Le confinement et le vent** re-branchés sur les faces ; re-vérifier
   **(w2) = 1,9966** et **(v6) / (v6b)**.
6. **Rejouer les 57 + 8 témoins** et confronter, ligne à ligne, aux deux listes du
   § 3. *Tout écart qui n'est pas dans la seconde liste est une régression.*

⚠️ **Chaque étape se commit séparément**, et l'étape 3 doit rendre son verdict
avant l'étape 4 : si le critère décisif ne tombe pas, il ne sert à rien de porter
l'advection.

---

## 5. ⛔ CE QUE CE PLAN NE PROMET PAS

- **Pas la masse.** La bascule ne corrige pas l'advection ; annoncer le contraire
  serait promettre ce qu'on ne livre pas.
- **Pas le GPU.** Le portage NKRHI/NkSL reste nommé et non commencé.
- **Pas le temps réel.**
- Et il ne promet pas que la bascule **réussira** : il promet que **le jour où
  elle sera mesurée, on saura lire le résultat**, parce que les seuils et la
  liste des déplacements légitimes étaient écrits avant.
