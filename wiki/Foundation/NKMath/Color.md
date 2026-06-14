# La couleur

Une couleur paraît simple — du rouge, du vert, du bleu — jusqu'à ce qu'on réalise qu'on en a
besoin sous trois formes différentes selon ce qu'on en fait. Pour **stocker** et **afficher**,
on veut un format compact (8 bits par canal). Pour **calculer** (mélanger, éclairer), on veut
des flottants, sous peine de perdre en précision à chaque opération. Et pour **manipuler
artistiquement** (changer la teinte, la luminosité), le modèle RGB est peu pratique — c'est
le modèle HSV qu'on veut. NKMath fournit les trois, et les conversions entre elles.

---

## NkColor — la couleur 8 bits (stockage, rendu)

`NkColor` range quatre composantes `uint8` (rouge, vert, bleu, alpha), de 0 à 255. C'est le
format compact du **rendu** — c'est d'ailleurs lui qu'on aliase en `NkColor2D` côté NKCanvas.

```cpp
NkColor orange{ 255, 130, 70, 255 };   // opaque par défaut (alpha = 255)
NkColor bg = NkColor::Transparent;
```

Pour le GPU, on empaquette les quatre octets en un seul entier 32 bits — mais l'ordre des
octets compte, et c'est une source d'erreur classique. `ToU32()` produit l'ordre **ABGR**
(celui qu'attendent les vertices NKUI), tandis que `ToUint32A()` produit l'ordre **RGBA**.
Choisissez selon ce qu'attend le consommateur.

NKMath fournit aussi un petit nuancier de couleurs prêtes — `White`, `Black`, `Transparent`,
`Gray`, `Red`, `Green`, `Blue` — et `FromName` pour obtenir une couleur par son nom.

---

## NkColorF — la couleur flottante (calculs)

Dès qu'on **calcule** sur des couleurs, on passe en `NkColorF` : quatre `float32` entre 0 et
1. Pourquoi ? Parce que mélanger, additionner, multiplier des couleurs 8 bits accumule des
erreurs d'arrondi (chaque opération quantifie sur 256 niveaux) ; en flottant, le résultat
reste précis jusqu'à la reconversion finale.

```cpp
NkColorF base = orange.ToColorF();
NkColorF tinted = base * NkColorF{ 0.8f, 0.8f, 1.f, 1.f };   // teinte bleutée
NkColorF mix = a.Lerp(b, 0.5f);                               // dégradé à mi-chemin
```

C'est sur `NkColorF` que vivent les opérations riches : les opérateurs arithmétiques
(`+`, `-`, `*`, `/`, avec une autre couleur ou un scalaire) pour mélanger et moduler, `Lerp`
pour un dégradé fluide, `Darken`/`Lighten` pour assombrir/éclaircir, `WithAlpha` pour ne
changer que la transparence, et `ToGrayscale` pour désaturer. Une fois le calcul fini, on
reconvertit en `NkColor` avec `ToColor()` pour le stockage ou l'affichage.

---

## NkHSV — la couleur artistique (teinte / saturation / valeur)

Pour ajuster une couleur « comme un artiste » — décaler la teinte vers le rouge, baisser la
saturation, monter la luminosité — le modèle HSV est le bon outil, parce que ces trois
réglages y sont des axes séparés (ce qu'ils ne sont pas en RGB). On y passe via `ToHSV()` /
`FromHSV`, on ajuste, on revient.

---

## Aperçu de l'API

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| **NkColor** (8 bits) | `NkColor(r, g, b, a = 255)` | Construction (opaque par défaut). |
| Empaquetage | `ToU32()` / `ToUint32A()` | Vers entier 32 bits **ABGR** / **RGBA** (GPU). |
| Conversion | `ToColorF()` · `ToHSV()` / `FromHSV(...)` | Vers flottant / HSV. |
| Nuancier | `White` `Black` `Transparent` `Gray` `Red` `Green` `Blue` · `FromName(name)` | Couleurs prêtes. |
| **NkColorF** (flottant) | `NkColorF(r, g, b, a = 1.f)` | Construction (composantes `[0, 1]`). |
| Arithmétique | `+` `-` `*` `/` (couleur/scalaire, `+=`…) · `==` `!=` | Mélange / comparaison. |
| Manipulation | `Lerp(other, t)` | Dégradé fluide. |
| Manipulation | `Darken(a)` / `Lighten(a)` | Assombrir / éclaircir. |
| Manipulation | `WithAlpha(a)` | Changer la transparence. |
| Manipulation | `ToGrayscale()` / `ToGrayscaleWithAlpha()` | Désaturer. |
| Conversion | `ToColor()` · `operator NkVector4f` / `NkVector3f` | Vers 8 bits / vecteur. |
| **NkHSV** | `struct` h/s/v · `+` `-` | Teinte/saturation/valeur, combinaison. |

---

## Référence complète

### NkColor — stocker et envoyer au GPU

`NkColor` se construit par ses quatre octets (`NkColor(r, g, b, a)`, opaque par défaut). Son
rôle premier est d'être **transmise au GPU**, et c'est là qu'intervient l'empaquetage en
entier 32 bits — avec un piège d'ordre d'octets : **`ToU32()`** produit l'ordre **ABGR**
(celui des vertices NKUI et de nombreux GPU), **`ToUint32A()`** l'ordre **RGBA**. Se tromper
d'ordre donne des couleurs inversées (du rouge qui vire au bleu). Le nuancier prêt à l'emploi
(`White`, `Black`, `Red`…) et `FromName` couvrent les besoins courants — couleurs de thème
d'UI, valeurs par défaut. Pour *calculer* sur une couleur, on passe d'abord en flottant avec
`ToColorF()`.

### NkColorF — calculer sur les couleurs

C'est sur `NkColorF` que vivent les opérations intéressantes, parce que le flottant ne perd
pas de précision au fil des calculs. Les **opérateurs arithmétiques** mélangent : l'addition
combine des contributions lumineuses (deux lampes qui éclairent un même point), la
multiplication module (teinter une texture par une couleur, appliquer une ombre). Au-delà,
quelques méthodes nommées correspondent à des besoins très concrets :

- **`Lerp(other, t)`** — le **dégradé** : une barre de vie qui passe du vert au rouge en
  fonction des PV, un fondu de couleur, un ciel qui change du jour à la nuit, un dégradé dans
  une UI.
- **`Darken(amount)` / `Lighten(amount)`** — les **états d'interface** : un bouton survolé
  s'éclaircit, pressé s'assombrit ; sert aussi à dériver toute une palette à partir d'une
  couleur de base.
- **`WithAlpha(alpha)`** — les **fondus** : faire apparaître ou disparaître un élément en ne
  touchant qu'à sa transparence, sans recalculer sa couleur.
- **`ToGrayscale()` / `ToGrayscaleWithAlpha()`** — les **effets** : un écran de pause grisé,
  un mode désaturé, un personnage « assommé » en noir et blanc.

Une fois le calcul terminé, `ToColor()` reconvertit en `NkColor` 8 bits pour l'affichage, et
`operator NkVector4f`/`NkVector3f` donne une vue vecteur pratique pour passer la couleur à un
shader (un uniform `vec4`).

### NkHSV — ajuster artistiquement

Le modèle RGB est parfait pour la machine mais peu intuitif pour un humain : « rendre cette
couleur plus chaude » ne correspond à aucun réglage simple en RGB. Le modèle **HSV** sépare au
contraire la **teinte** (la couleur elle-même), la **saturation** (son intensité) et la
**valeur** (sa luminosité) en trois axes indépendants. On y passe via `ToHSV()`/`FromHSV`,
on ajuste l'axe voulu, on revient. Usages : générer une **palette** harmonieuse (mêmes
saturation/valeur, teintes réparties), faire un **color grading** (réchauffer/refroidir une
scène), ou un effet d'arc-en-ciel (faire défiler la teinte). Les opérateurs `+`/`-` combinent
deux HSV.

### Exemple

```cpp
#include "NKMath/NkColor.h"
using namespace nkentseu::math;

NkColor  ui   = NkColor::Blue;
NkColorF c    = ui.ToColorF();              // passer en flottant pour calculer
NkColorF dark = c.Darken(0.2f);              // 20 % plus sombre
NkColorF fade = c.WithAlpha(0.5f);           // semi-transparent
NkColor  out  = c.Lerp(NkColorF{1,1,1,1}, 0.3f).ToColor();   // 30 % vers le blanc
uint32   gpu  = out.ToU32();                 // empaqueté pour le GPU (ABGR)
```

> **En résumé.** Trois formes : `NkColor` (8 bits) pour stocker/afficher, `NkColorF`
> (flottant) pour calculer sans perte (c'est là que vivent `Lerp`/`Darken`/`Lighten`/
> `WithAlpha`), `NkHSV` pour les ajustements artistiques. Calculez en `NkColorF`, reconvertissez
> en `NkColor` à la fin. Attention à l'ordre d'octets : `ToU32()` = ABGR, `ToUint32A()` = RGBA.

---

[← Les fonctions](Functions.md) · [Index NKMath](README.md) · [La géométrie →](Geometry.md)
