# La triangulation des contours : Earcut

> Couche **Runtime** · NKFont · Transformer les **contours** d'un glyphe (un dessin fait de
> boucles fermées, avec des trous) en une **liste de triangles** remplissables par le GPU, via
> l'algorithme d'*ear-clipping* `NkEarcut`.

Un glyphe de police n'est pas une image : c'est un **dessin vectoriel**, une suite de contours
fermés. La lettre « o » a deux boucles — l'extérieur et le trou du milieu —, le « A » a son
triangle creux, le « B » en a deux. Le GPU, lui, ne sait dessiner qu'**une seule chose** :
des **triangles**. Entre les deux, il manque une étape : découper ces contours en triangles qui
remplissent exactement l'intérieur de la forme, trous compris. C'est précisément ce que fait
`NkEarcut` — il **triangule** un polygone défini par un contour extérieur et un nombre quelconque
de trous.

L'algorithme employé s'appelle l'*ear-clipping* (« découpe d'oreilles ») : on repère un sommet
« en oreille » — un coin convexe dont le triangle formé avec ses deux voisins ne contient aucun
autre point —, on le découpe en émettant ce triangle, puis on recommence sur le polygone réduit,
jusqu'à ce qu'il n'en reste qu'un. Les trous sont d'abord raccordés au contour extérieur par des
**ponts** (*bridges*), ce qui ramène un polygone à trous à un unique contour qu'on peut alors
découper d'un seul tenant.

Ce n'est **pas** un rasteriseur : `NkEarcut` ne produit aucun pixel, seulement des **indices** de
triangles. Ce n'est **pas** non plus un générateur de courbes : il prend des contours **déjà
aplatis** en segments droits (les courbes de Bézier du glyphe ont été échantillonnées en amont).
Et ce n'est **pas** un objet à instancier : c'est une **fonction libre, pure, sans état**.

- **Namespace public** : `nkentseu` (tout le reste est dans `nkentseu::detail`, interne).
- **Header (autonome, header-only)** : `#include "NKFont/NkEarcut.h"`

```cpp
NkVector<NkVector<NkVec2f>> polygon;
polygon.PushBack(outerContour);   // contour extérieur, CCW
polygon.PushBack(holeContour);    // trou, CW
auto indices = NkEarcut<float>(polygon);   // indices globaux, par triplets
```

---

## Le contrat d'entrée : winding et trous

Tout le sérieux de `NkEarcut` tient dans **la forme exacte de son entrée**. On lui passe un
**vecteur de contours**, chaque contour étant lui-même un vecteur de points 2D
(`NkVector<NkVector<NkVec2T<T>>>`). Le **premier** contour (`polygon[0]`) est le **contour
extérieur** ; tous les suivants (`polygon[1..]`) sont des **trous**.

La règle non négociable est le **sens de parcours** (*winding*) : le contour extérieur doit être
**anti-horaire (CCW)** — son aire signée positive en repère Y-up — et chaque trou doit être
**horaire (CW)**, aire négative. C'est cette opposition de sens qui permet à l'algorithme de
distinguer « plein » et « creux ».

Le piège : **rien n'est normalisé automatiquement**. `NkEarcut` ne retourne pas vos contours pour
vous — si un glyphe arrive avec le mauvais winding, c'est à **vous**, l'appelant, d'inverser
l'ordre des points **avant** l'appel. Un winding faux ne lève pas d'erreur : il produit
silencieusement des triangles incorrects, ou aucun.

```cpp
// Convention Y-up : aire > 0 = CCW (extérieur), aire < 0 = CW (trou).
// À l'appelant de retourner un contour dont le sens ne correspond pas.
```

> **En résumé.** `polygon[0]` = contour extérieur **CCW** (aire > 0) ; `polygon[1..]` = trous
> **CW** (aire < 0). Le winding est de la responsabilité de l'**appelant**, à corriger avant
> l'appel — aucune normalisation automatique, aucune erreur si c'est faux.

---

## La sortie : des indices globaux dans une flat-list

`NkEarcut` ne renvoie pas de points, mais un `NkVector<std::size_t>` d'**indices**, par paquets de
trois : chaque triplet est un triangle. La subtilité — et c'est la « correction v2 » notée dans
l'en-tête — est que ces indices sont **globaux**. Ils ne pointent pas dans tel ou tel contour,
mais dans un **tableau plat unique**, la concaténation de tous les contours dans l'ordre fourni :

```
[ outer | trou 1 | trou 2 | … ]
   0..              ↑           ↑
        outer.Size()   outer.Size()+trou1.Size()
```

Concrètement : l'indice `0` désigne le premier point du contour extérieur, l'indice
`outer.Size()` désigne le premier point du **trou 1**, et ainsi de suite. Vous pouvez donc
construire **un seul tableau de vertices** (extérieur suivi de tous les trous, dans le même ordre)
et l'indexer **directement** avec le retour de `NkEarcut` — c'est exactement la forme d'un
*index buffer* GPU.

Avant cette correction, les indices étaient locaux à chaque contour, ce qui obligeait à recoller
les morceaux. Désormais, le pont entre la triangulation et le rendu est immédiat.

> **En résumé.** Le retour est une liste plate d'indices, **3 par triangle**, **globaux** dans la
> concaténation `[outer | trous…]`. Construisez votre tableau de vertices dans le **même ordre** et
> indexez-le directement — c'est un index buffer prêt à l'emploi.

---

## Ce qui est filtré silencieusement

Deux situations font qu'un trou est **ignoré sans bruit**. D'abord, un trou de **moins de 3
points** : un segment ou un point ne délimite aucune aire, il est sauté. Ensuite, un trou
**flottant** : si son premier point n'est **pas à l'intérieur** du contour extérieur (test par
ray-casting), il est écarté — un trou hors de la forme n'a pas de sens.

Le détail à connaître : un trou ainsi écarté **ne consomme pas d'offset global**. L'indexation
reste donc cohérente avec une flat-list qui ne contiendrait **que** le contour extérieur et les
trous **réellement connectés**. Si, de votre côté, vous construisez votre tableau de vertices en y
mettant **tous** les contours d'entrée (y compris les flottants), vos indices et vos vertices se
désaligneront. La parade : aligner votre concaténation sur ce que `NkEarcut` a effectivement
retenu, ou s'assurer en amont qu'aucun contour n'est dégénéré ou flottant.

> **En résumé.** Trou < 3 points ou trou « flottant » (hors du contour extérieur) = **ignoré sans
> erreur**, et **sans consommer d'offset**. Construisez votre flat-list de vertices sur les seuls
> contours réellement retenus, sinon les indices se décalent.

---

## Aperçu de l'API

La **seule** entité publique de ce header est la fonction libre `NkEarcut<T>`. Tout le reste
(nœuds de liste chaînée, primitives géométriques, gestion des ponts) vit dans
`nkentseu::detail` : c'est l'implémentation, non destinée à l'usage direct.

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Triangulation | `NkEarcut<T = float>(polygon)` `→ NkVector<std::size_t>` | Triangule un contour extérieur + N trous par ear-clipping ; renvoie les indices globaux par triplets. Pas de `noexcept`, pas de `nodiscard`. |
| Entrée | `polygon : const NkVector<NkVector<NkVec2T<T>>>&` | `[0]` = extérieur (CCW), `[1..]` = trous (CW). Winding à normaliser par l'appelant. |
| Sortie | `NkVector<std::size_t>` | Indices globaux (3 = 1 triangle) dans la flat-list `[outer \| trous…]`. |
| Template | `T` | Type des coordonnées, **défaut `float`** ; points = `math::NkVec2T<T>`. |

*(Implémentation interne, `nkentseu::detail` — citée plus bas comme contexte uniquement, non
publique : `NkEarcutNode<T>`, `NkEarcutInsertNode`/`RemoveNode`/`DeleteList`, `NkEarcutArea`,
`NkEarcutPointInTriangle`, `NkEarcutIsEar`, `NkEarcutLinked`, `NkEarcutCreateListWithOffset`,
`NkEarcutFindBridge`, `NkEarcutConnectHole`, `NkEarcutPointInPolygon`.)*

---

## Référence complète

Le module n'a qu'une fonction publique, mais elle condense plusieurs idées. On la détaille à fond,
puis on éclaire la mécanique interne (sans l'exposer comme API) pour comprendre les coûts et les
limites.

### `NkEarcut<T>` — la fonction de triangulation

```cpp
template <typename T = float>
NkVector<std::size_t> NkEarcut(const NkVector<NkVector<math::NkVec2T<T>>>& polygon);
```

Le paramètre template `T` est le type des coordonnées (**`float` par défaut**) ; les points sont
des `math::NkVec2T<T>` qu'on lit par `.x` / `.y`. La fonction n'est ni `noexcept` ni `nodiscard`,
mais sa valeur de retour — les indices — est son **unique** résultat utile : ne l'ignorez jamais.

**Déroulé interne, étape par étape.**

1. **Garde précoce.** Si `polygon.IsEmpty()` ou si `polygon[0].Size() < 3`, la fonction retourne
   un vecteur **vide** : un contour de moins de trois points ne délimite aucune surface.
2. **Liste extérieure.** Le contour extérieur devient une liste doublement chaînée circulaire, en
   sens direct, avec un offset global de 0.
3. **Trous.** Pour chaque trou (`h ≥ 1`) : ignoré s'il a moins de 3 points ; ignoré s'il est
   « flottant » (son premier point hors du contour extérieur) ; sinon créé en **ordre inversé**
   (CW → CCW dans la liste) avec son offset global, puis raccordé au ring extérieur par un **pont**.
   L'incrément `globalOffset += polygon[h].Size()` n'a lieu **qu'après** le pont — les `continue`
   (trou trop petit ou flottant) sautent l'incrément, d'où la cohérence d'indexation décrite plus
   haut.
4. **Découpe.** Ear-clipping sur la liste fusionnée (extérieur + trous connectés).
5. **Libération.** La liste chaînée interne est entièrement détruite avant le retour.
6. **Retour.** Le vecteur de triangles.

**Convention d'aire (Y-up).** L'aire signée d'un triangle `< 0` indique un coin **CCW** (oreille
valide pour un polygone CCW) ; `> 0` indique **CW** (invalide). C'est pourquoi le contour
extérieur est attendu CCW.

**Coût.** L'ear-clipping est naïf : globalement **O(n²)** sur le nombre de sommets, la recherche
de ponts étant elle-même brute-force en `O(n_outer × n_hole)`. Pour des glyphes (quelques dizaines
à quelques centaines de sommets), c'est négligeable et fait une fois par glyphe, à la mise en
cache. Un **garde-fou** `maxIter = 16000` borne la boucle de découpe : au-delà d'environ 16000
itérations, la triangulation s'arrête — pertinent uniquement sur des polygones extrêmement denses.

Les usages, par domaine :

- **UI / 2D & texte** — le cœur du métier : transformer les contours d'un glyphe en triangles
  pour un rendu de texte **vectoriel** (zoomable sans crénelage, contrairement à un atlas
  bitmap). Plus largement, remplir n'importe quelle forme 2D dessinée par contours — icônes,
  badges, formes de canvas, secteurs d'un graphique.
- **Outils / éditeur** — afficher la silhouette pleine d'un caractère ou d'un logo importé en SVG,
  générer la géométrie de remplissage d'un tracé dessiné à la main dans un éditeur de formes.
- **Rendu** — produire un **index buffer** directement consommable : le retour, par triplets
  d'indices globaux, est exactement la forme attendue par un `glDrawElements` / `DrawIndexed`,
  apposé sur le tableau plat des vertices `[outer | trous…]`.
- **GPU / géométrie** — base d'une **extrusion 3D** : on triangule la face avant d'une lettre,
  on duplique pour la face arrière, et on relie les contours pour le côté — du texte 3D à partir
  d'une police 2D.
- **Gameplay** — remplir une forme libre définie par des points (zone de jeu, terrain découpé,
  région de minimap) pour la dessiner en plein.

### Pourquoi une liste chaînée circulaire (contexte interne)

L'ear-clipping retire des sommets **un par un** au milieu du polygone. Sur un tableau, chaque
retrait coûterait un décalage `O(n)` ; sur une **liste doublement chaînée circulaire**, c'est
`O(1)` — on rebranche deux pointeurs. C'est pourquoi `detail` modélise chaque contour par un cycle
de nœuds `NkEarcutNode<T>` (coordonnées `x`/`y`, **index global** `i`, voisins `prev`/`next`). Les
nœuds y sont alloués par `new`/`delete` (heap CRT) — un détail d'implémentation **invisible** de
l'API publique : tous les nœuds d'un appel sont créés puis libérés dans ce même appel. Seul le
résultat `NkVector<std::size_t>` suit la gestion mémoire de `NkVector` (donc NKMemory).

### Comment les trous sont raccordés (contexte interne)

Un polygone à trous n'est pas directement « découpable » : il faut d'abord en faire un contour
**unique**. `detail` y parvient par des **ponts**. `NkEarcutFindBridge` cherche, en brute-force, la
paire de sommets (un sur l'extérieur, un sur le trou) la **plus proche** (distance euclidienne au
carré). `NkEarcutConnectHole` fusionne alors le trou dans le ring extérieur via un **double pont**
— et la « correction v2 » du header utilise `holeLast = holeBridge->prev` (et non `->next`) pour le
pont retour, ce qui évitait une boucle infinie dans le ring. Le test `NkEarcutPointInPolygon`
(ray-casting, epsilon `1e-8` au dénominateur) sert à écarter les trous flottants avant même de
chercher un pont. Détecter une oreille valide repose sur `NkEarcutArea` (aire signée) et
`NkEarcutPointInTriangle` (inclusion, bords inclus), combinés dans `NkEarcutIsEar`.

### Ownership, pureté et threading

`NkEarcut` est une **fonction pure sans état persistant** : aucune `Create`/`Destroy` à appeler,
aucune ressource à fermer. Toute la mémoire intermédiaire (la liste chaînée) est allouée puis
libérée **dans le même appel**. Comme il n'y a aucun état global, la fonction est **ré-entrante et
thread-safe** tant que chaque appel travaille sur ses propres données : on peut trianguler
plusieurs glyphes en parallèle sur un pool de threads sans synchronisation. Seule règle à ne pas
oublier : la valeur de retour — les indices — est le **seul** résultat, à ne pas jeter faute de
`nodiscard`.

---

### Exemple récapitulatif

```cpp
#include "NKFont/NkEarcut.h"
using namespace nkentseu;
using nkentseu::math::NkVec2f;

// 1. Préparer les contours du glyphe (déjà aplatis en segments).
//    Extérieur CCW, trou CW — winding normalisé par l'appelant.
NkVector<NkVector<NkVec2f>> polygon;
polygon.PushBack(outerContour);   // CCW (aire > 0)
polygon.PushBack(holeContour);    // CW  (aire < 0)  ← le creux du « o »

// 2. Trianguler : indices globaux, par triplets.
NkVector<std::size_t> indices = NkEarcut<float>(polygon);

// 3. Construire UN tableau de vertices plat, dans le même ordre que les contours.
NkVector<NkVec2f> verts;
for (const auto& p : outerContour) verts.PushBack(p);
for (const auto& p : holeContour)  verts.PushBack(p);   // seulement les trous retenus

// 4. indices[] indexe directement verts[] : prêt pour un index buffer GPU.
for (std::size_t k = 0; k + 2 < indices.Size(); k += 3) {
    const NkVec2f& a = verts[indices[k + 0]];
    const NkVec2f& b = verts[indices[k + 1]];
    const NkVec2f& c = verts[indices[k + 2]];
    EmitTriangle(a, b, c);   // ou : remplir un index buffer et glDrawElements
}
```

---

[← Index NKFont](README.md) · [Récap NKFont](../NKFont.md) · [Couche Runtime](../README.md)
