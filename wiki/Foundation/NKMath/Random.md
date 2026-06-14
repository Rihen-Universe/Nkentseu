# L'aléatoire

Le hasard est partout dans un jeu : disperser des particules, varier une couleur, lancer un
dé, faire surgir un ennemi à une position imprévisible. `NkRandom` est le générateur
pseudo-aléatoire du moteur. Sa particularité agréable : il ne se contente pas de tirer des
nombres, il sait tirer directement des **vecteurs**, des **couleurs**, des **matrices** —
les objets dont on a réellement besoin.

On y accède de deux façons équivalentes : par le singleton `NkRandom::Instance()`, ou par
l'alias global `NkRand`, plus court.

```cpp
#include "NKMath/NkRandom.h"
using namespace nkentseu::math;

float32 f = NkRand.NextFloat32();              // un flottant dans [0, 1)
NkVec2 pos{ NkRand.NextFloat32() * W,          // une position d'apparition
            NkRand.NextFloat32() * H };
NkColor c = NkRand.NextColor();                // une couleur aléatoire
```

---

## Tirer une valeur simple

Pour un scalaire, on choisit le type : `NextFloat32` pour un flottant, `NextInt32` /
`NextUInt32` / `NextUInt8` pour des entiers. Quand on veut un résultat **borné** — un dé
entre 1 et 6, un angle entre 0 et 360 — on passe par les versions à plage, `NextInRange` ou
les helpers `Range`/`RangeFloat`/`RangeInt`/`RangeUInt`, plutôt que de bricoler un modulo à
la main (qui introduit un biais de distribution).

```cpp
int32 de = NkRand.RangeInt(1, 6);   // un dé honnête, sans biais
```

## Tirer des objets composés

C'est là que `NkRandom` se distingue. `NextVec2/3/4` tirent un vecteur (une direction
aléatoire, une position), `NextColor`/`NextColorA`/`NextHSV` tirent une couleur (un effet,
une palette), et `NextMat2/3/4` tirent une matrice. On évite ainsi d'assembler composante
par composante, et le résultat est directement utilisable.

---

## Aperçu de l'API

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Accès | `NkRandom::Instance()` / `NkRand` | Générateur (singleton / alias global). |
| Scalaires | `NextFloat32()` | Flottant dans `[0, 1)`. |
| Scalaires | `NextInt32()` `NextUInt32()` `NextUInt8()` | Entiers (toute la plage du type). |
| Bornés | `NextInRange(...)` · `Range/RangeFloat/RangeInt/RangeUInt(min, max)` | Tirage borné, sans biais. |
| Vecteurs | `NextVec2()` `NextVec3()` `NextVec4()` | Vecteur aléatoire (direction, position). |
| Couleurs | `NextColor()` `NextColorA()` `NextHSV()` | Couleur aléatoire (avec/sans alpha, HSV). |
| Matrices | `NextMat2()` `NextMat3()` `NextMat4()` | Matrice aléatoire. |

---

## Référence complète

### Accès et tirages scalaires

On accède au générateur par le singleton `NkRandom::Instance()` ou l'alias global `NkRand`.
Le tirage de base, **`NextFloat32()`**, renvoie un flottant dans `[0, 1)` — la brique
universelle : un `< 0.3f` modélise « 30 % de chances », un `* W` étale sur une largeur.
`NextInt32`/`NextUInt32`/`NextUInt8` couvrent les entiers.

Dès qu'on veut un résultat **borné**, on passe par **`NextInRange`** ou les helpers
`Range`/`RangeInt`/`RangeFloat` plutôt que de bricoler un `% n` à la main — car le modulo
introduit un **biais** (les petites valeurs sortent légèrement plus souvent). Les domaines
sont innombrables :

- **Gameplay** : un coup critique (`NextFloat32() < tauxCritique`), un butin tiré dans une
  table, un délai d'apparition variable.
- **Effets** : un peu de *jitter* sur une position ou un timing pour casser la régularité.
- **Procédural** : choisir une tuile, une variation de mesh.

### Tirages composés — l'atout de NkRandom

Là où la plupart des générateurs s'arrêtent aux nombres, `NkRandom` tire directement les
objets dont on a besoin, ce qui évite de les assembler composante par composante :

- **`NextVec2/3/4`** — une **direction** aléatoire (la trajectoire d'une particule, l'errance
  d'une IA) ou une **position** (disperser des objets dans une zone).
- **`NextColor`/`NextColorA`/`NextHSV`** — une **couleur** aléatoire (un effet multicolore,
  une variation de teinte entre instances d'un même objet, une palette générée).
- **`NextMat2/3/4`** — une **transformation** aléatoire (orienter aléatoirement des rochers,
  des touffes d'herbe).

### Reproductibilité

Un générateur pseudo-aléatoire part d'une **graine** : à graine identique, suite identique.
Pour du gameplay **reproductible** — rejouer un replay à l'identique, écrire un test
déterministe, partager une « seed » de monde entre joueurs — initialisez le générateur avec
une graine connue au démarrage.

### Exemple

```cpp
// Disperser des balles à des positions et couleurs aléatoires (cf. démos NKCanvas)
for (int i = 0; i < count; ++i) {
    balls[i].pos = NkVec2{ NkRand.RangeFloat(0.f, W), NkRand.RangeFloat(0.f, H) };
    balls[i].vel = NkRand.NextVec2();        // direction aléatoire
    balls[i].col = NkRand.NextColor();
}
```

> **En résumé.** `NkRand` (ou `NkRandom::Instance()`) tire des scalaires (`NextFloat32`,
> `NextInt32`…), des valeurs **bornées** (`RangeInt`/`RangeFloat` — sans le biais du modulo),
> et surtout des objets prêts à l'emploi (`NextVec*`, `NextColor`, `NextMat*`). Pour un
> gameplay reproductible (replays, tests), initialisez le générateur avec une graine connue.

---

[← La géométrie](Geometry.md) · [Index NKMath](README.md) · [Récap NKMath](../NKMath.md)
