# Les conteneurs hétérogènes

> Couche **Foundation** · NKContainers · Regrouper des valeurs de **types différents** en un seul
> objet : la paire `NkPair` (exactement deux éléments) et le tuple `NkTuple` (un nombre quelconque,
> fixé à la compilation).

Les conteneurs séquentiels (`NkVector`, `NkList`…) rangent **plusieurs choses du même type**. Mais
il arrive souvent qu'on veuille l'inverse : **assembler quelques valeurs de types différents** en
un seul paquet — un identifiant *et* un score, un booléen de succès *et* le résultat, une position
*et* sa couleur *et* son indice. C'est exactement ce que résolvent `NkPair` et `NkTuple` : des
**agrégats hétérogènes de taille fixe**, l'alternative zéro-STL à `std::pair` et `std::tuple`.

La différence avec un conteneur séquentiel est profonde et tient en une phrase : ici, **le nombre
d'éléments et le type de chacun sont connus à la compilation**, et l'accès se résout *aussi* à la
compilation. On n'itère pas un tuple comme on itère un vecteur ; on en extrait le 0ᵉ, le 1ᵉʳ, le
2ᵉ élément — chacun avec son **propre type**. Conséquence directe : **aucune allocation dynamique**.
Tout est stocké **par valeur**, à la suite en mémoire, exactement comme les champs d'une `struct`
anonyme. Il n'y a donc **ni allocateur, ni invalidation, ni réagencement** à craindre — ces deux
types sont les plus simples de tout NKContainers sur le plan mémoire.

- **Namespace** : `nkentseu` (attention : **directement** `nkentseu`, pas `nkentseu::containers`
  comme les conteneurs séquentiels)
- **Headers** : `#include "NKContainers/Heterogeneous/NkPair.h"` ·
  `#include "NKContainers/Heterogeneous/NkTuple.h"`

---

## La paire : `NkPair`

C'est le cas le plus fréquent — **exactement deux valeurs liées** — et le plus simple. `NkPair<T1,
T2>` n'est qu'une `struct` à deux champs publics : `First` (de type `T1`) et `Second` (de type
`T2`), stockés **directement par valeur**, l'un derrière l'autre. On y accède sans cérémonie :

```cpp
nkentseu::NkPair<int, float> p1(42, 3.14f);
int   id    = p1.First;     // membre public, accès direct
float value = p1.Second;
```

Les deux types peuvent différer ou être identiques — `NkPair<int, int>` (une plage min/max),
`NkPair<bool, int>` (succès + valeur), `NkPair<NkVec3, NkColor>` (position + couleur). Le **layout
mémoire** est `[First : sizeof(T1)][Second : sizeof(T2)]`, sans aucune indirection : ce n'est
**pas** un nœud chaîné, **pas** un pointeur vers un bloc alloué — c'est aussi compact qu'un couple
de champs écrits à la main.

Plutôt que d'écrire les types à chaque fois, on laisse le compilateur les **déduire** avec la
factory `NkMakePair` :

```cpp
auto p2 = nkentseu::NkMakePair(100, 2.718);   // NkPair<int, double>, types déduits
```

Une subtilité importante de `NkMakePair` : il **decaye** les types (retire références et `const`)
avant de les stocker. Passer une `const std::string&` produit donc un `NkPair` qui contient une
*copie* `NkString`, pas une référence — c'est volontaire et évite les références pendantes, mais il
ne faut **pas** compter sur un type-référence dans le résultat.

> **En résumé.** `NkPair<T1, T2>` = deux valeurs de types potentiellement différents, stockées par
> valeur (zéro allocation), accessibles par les membres publics `First` / `Second`. Construisez
> directement (`NkPair<int, float> p(42, 3.14f)`) ou laissez `NkMakePair` déduire les types — en
> gardant à l'esprit qu'il les **decaye** (copie, jamais référence).

---

## Le tuple : `NkTuple`

Dès qu'il faut **plus de deux** valeurs hétérogènes — ou simplement un nombre variable — la paire
ne suffit plus. `NkTuple<Types...>` généralise l'idée à un **nombre quelconque d'éléments**, fixé à
la compilation : `NkTuple<int, float, const char*>` regroupe trois valeurs de trois types, comme un
tuple Python ou `std::tuple`.

```cpp
auto coords = nkentseu::NkMakeTuple(10, 20.5f, "point");   // NkTuple<int, float, const char*>
int         x     = nkentseu::NkGet<0>(coords);
float       y     = nkentseu::NkGet<1>(coords);
const char* label = nkentseu::NkGet<2>(coords);
```

On n'accède **pas** à un tuple par des membres nommés (il n'y a pas de `.Third`), mais par
**indice à la compilation** via `NkGet<Index>`. L'indice est vérifié au moment de la compilation
(`static_assert(Index < sizeof...(Types))`) : sortir des bornes est une **erreur de compilation**,
pas un plantage à l'exécution. C'est tout l'intérêt d'un agrégat de taille fixe — l'erreur est
attrapée avant même que le programme existe.

À l'intérieur, `NkTuple` est bâti de façon **récursive** : chaque niveau stocke son premier élément
(`mHead`) suivi d'un sous-tuple contenant le reste (`mTail`). Un `NkTuple<int, float, char>` est en
réalité un `int` suivi d'un `NkTuple<float, char>`, lui-même un `float` suivi d'un `NkTuple<char>`,
et ainsi de suite jusqu'au tuple vide `NkTuple<>` qui termine la chaîne. Cette construction n'a
**aucun coût mémoire** : les éléments se retrouvent **séquentiels par valeur**, comme les champs
d'une `struct`. Ce n'est **pas** une liste chaînée à l'exécution — la récursion est purement un
mécanisme de *compilation*, résolu et aplati avant de produire du code.

La taille se lit avec `NkTupleSize` (depuis une instance) ou `NkTupleSizeTrait<T>::value` (depuis le
type, en contexte template) :

```cpp
usize n = nkentseu::NkTupleSize(coords);                       // 3, résolu à la compilation
usize m = nkentseu::NkTupleSizeTrait<decltype(coords)>::value; // 3, depuis le type
```

> **En résumé.** `NkTuple<Types...>` = un nombre **fixe** de valeurs hétérogènes, accessibles par
> indice **à la compilation** avec `NkGet<Index>` (hors-bornes = erreur de compilation). Stockage
> récursif Head/Tail mais **aplati** et sans allocation. Construisez avec `NkMakeTuple` (types
> déduits et decayés) ; lisez la taille avec `NkTupleSize` ou `NkTupleSizeTrait`.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Le bloc move / forwarding / accès-par-indice est
conditionné par `#if defined(NK_CPP11)` (signalé ci-dessous) : en C++98/03, seules subsistent la
copie, les comparaisons, `NkGetFirst`/`NkGetSecond` et `NkMakePair(const&)`. Complexités entre
crochets.

### `NkPair<T1, T2>` — paire hétérogène

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Alias | `FirstType` (= `T1`), `SecondType` (= `T2`) | Types des deux membres. |
| Données | `First`, `Second` | Les deux valeurs (membres **publics**, accès direct). |
| Construction | `NkPair()`, `NkPair(first, second)`, copie, conversion `NkPair<U1,U2>` | Défaut (value-init) / par valeurs / copie / depuis types compatibles. |
| Construction *(NK_CPP11)* | move, move-conversion, `NkPair(U1&&, U2&&)` (forwarding) | Déplacement / déplacement-conversion / construction in-place par forwarding parfait. |
| Affectation | `operator=` (copie, copie-conversion) | Affectation par copie. |
| Affectation *(NK_CPP11)* | `operator=` (move, move-conversion) | Affectation par déplacement. |
| Modification | `Swap(other)` `[O(1)*]` | Échange membre-à-membre (`traits::NkSwap`). |
| Comparaison *(libre)* | `==` `!=` `<` `<=` `>` `>=` | Égalité ; ordre **lexicographique** (`First` puis `Second`). |
| Factory *(libre)* | `NkMakePair(first, second)` | Déduit les types (copie ; *(NK_CPP11)* surcharge forwarding **decayée**). |
| Accès *(libre)* | `NkGetFirst(p)`, `NkGetSecond(p)` (+ surcharges const) | Référence vers `First` / `Second`. |
| Accès indexé *(libre, NK_CPP11)* | `NkGet<Index>(p)` (non-const / const / rvalue) | Style `std::get`, `Index` ∈ {0, 1} vérifié par `static_assert`. |
| Méta *(NK_CPP11)* | `NkPairElement<Index, T1, T2>` | Trait : `Type` à l'indice + statics `Get`. |
| Échange *(libre)* | `NkSwap(lhs, rhs)` | Swap non-membre (ADL), délègue à `Swap`. |

### `NkTuple<Types...>` — tuple hétérogène de taille fixe

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Données | `mHead`, `mTail` (cas récursif) | Premier élément + sous-tuple du reste (stockage **séquentiel**). |
| Spécialisations | `NkTuple<>` (vide), `NkTuple<Head, Tail...>` (récursif) | Terminaison récursive / cas général. |
| Construction | `NkTuple()`, `NkTuple(head, tail...)`, copie, conversion | Défaut / par valeurs / copie / depuis tuple compatible. |
| Construction *(NK_CPP11)* | move, move-conversion, `explicit NkTuple(H&&, Args&&...)` (forwarding) | Déplacement / forwarding parfait (**`explicit`**, contrairement à `NkPair`). |
| Affectation | `operator=` (copie, copie-conversion) | Affectation par copie. |
| Affectation *(NK_CPP11)* | `operator=` (move, move-conversion) | Affectation par déplacement. |
| Comparaison *(membres)* | `==` `!=` `<` `<=` `>` `>=` | Récursif ; ordre **lexicographique** (méthodes membres, pas libres). |
| Modification | `Swap(other)` `[O(N)]` | Échange élément par élément (`NkSwap` + récursion). |
| Accès *(libre)* | `NkGet<Index>(t)` (non-const / const / rvalue) | Style `std::get`, `Index < sizeof...(Types)` vérifié par `static_assert`. |
| Taille *(libre)* | `NkTupleSize(t)`, `NkTupleSizeTrait<T>::value` | Nombre d'éléments depuis une **instance** / depuis le **type**. |
| Méta | `NkTupleElement<Index, NkTuple<...>>`, `NkTupleTypeAt<Index, Types...>` | Trait `Type`/`Get` à l'indice ; type à l'indice. |
| Factory *(libre)* | `NkMakeTuple(args...)` | Forwarding parfait, types **decayés**. |
| Échange *(libre)* | `NkSwap(lhs, rhs)` | Swap non-membre (ADL), délègue à `Swap`. |
| ⚠ Stub | `NkTupleConcat(t1, t2)` | **Non implémenté** : renvoie un tuple default-construit, **perd les valeurs**. |

(*) `O(1)` si les membres sont triviaux ; `O(n)` si ce sont eux-mêmes des conteneurs.

---

## Référence complète

Chaque élément repris en détail, avec sa complexité et ses usages dans les différents domaines du
moteur. Les éléments triviaux (constructeurs, affectations) sont décrits brièvement ; l'accès, les
comparaisons et les factories le sont à fond.

### Choisir : paire ou tuple ?

Le critère est trivial — **le nombre d'éléments** :

- **Exactement deux** valeurs liées → `NkPair`. Accès lisible par `First`/`Second`, factory
  `NkMakePair`. C'est l'immense majorité des « retours doubles » et des entrées clé/valeur.
- **Trois ou plus**, ou un nombre paramétré par un template variadique → `NkTuple`. Accès par
  indice `NkGet<i>`, factory `NkMakeTuple`.

Dans les deux cas : **taille fixe, types hétérogènes, zéro allocation, résolution à la
compilation**. Si vous vouliez plutôt un nombre **variable** d'éléments **du même type**, ce n'est
pas ici — c'est un [conteneur séquentiel](Sequential.md).

### `NkPair` à fond

**Membres et construction.** Les deux valeurs sont les champs publics `First` et `Second` ; on les
lit et on les écrit directement, sans accesseur. Le constructeur par défaut **value-initialise** les
deux membres (un `NkPair<int, float>()` part donc de `{0, 0.f}`), le constructeur paramétré copie
deux valeurs, et un constructeur **de conversion** templaté (`NkPair<U1, U2>`) permet de bâtir une
paire depuis une autre dont les types sont compatibles (membre-à-membre). Sous `NK_CPP11`
s'ajoutent le **déplacement** (via `traits::NkMove`), le déplacement-conversion (via
`traits::NkForward`), et surtout un constructeur **à forwarding parfait** `NkPair(U1&& first, U2&&
second)` qui construit les membres **in-place** — utile pour des types lourds qu'on ne veut pas
copier. Les alias `FirstType`/`SecondType` exposent les types des membres en contexte générique.

**`Swap` — l'échange.** `Swap(other)` permute membre à membre via `traits::NkSwap`. `O(1)` pour des
types triviaux, `O(n)` si les membres sont eux-mêmes des conteneurs (le swap conteneur est `O(1)`,
mais leurs sous-swaps comptent). La free function `NkSwap(a, b)` y délègue par ADL — c'est la forme
idiomatique.

**Comparaisons (free functions).** L'égalité compare les deux membres (`First == First && Second ==
Second`), et l'ordre est **lexicographique** : on compare d'abord `First`, et seulement en cas
d'égalité on départage par `Second`. C'est cet ordre qui rend `NkPair` directement utilisable comme
**clé composite** dans les conteneurs ordonnés. Cas d'usage par domaine :

- **Retour double** — la signature canonique d'une fonction qui rend deux choses : `NkPair<bool,
  int>` pour « succès + valeur », `NkPair<NkEntity, float>` pour « entité touchée + distance » d'un
  *raycast*, `NkPair<usize, usize>` pour une plage `[début, fin)`.
- **ECS / scène** — une clé d'association `(archétype, indice)` ; un couple `(composant, version)`
  pour invalider des caches.
- **Rendu** — `(largeur, hauteur)` d'une cible, `(min, max)` d'une plage de profondeur, `(texture,
  sampler)` formant une unité de liaison.
- **Physique / collision** — la **paire d'objets** d'un contact (`NkPair<BodyId, BodyId>`), naturel
  comme clé d'une table de contacts persistante ; `(temps d'impact, normale)` d'un test de balayage.
- **Animation** — `(image-clé, poids)` pour un mélange ; `(piste, valeur)` d'un échantillon.
- **Gameplay / IA** — `(score, identifiant)` d'un classement (l'ordre lexicographique trie d'abord
  par score) ; `(cible, priorité)` d'une décision.
- **Audio** — `(voix, gain)` d'un mixage ; `(échantillon, fréquence)` d'une source.
- **UI / 2D** — un point `(x, y)`, un intervalle `(début, longueur)` d'un texte sélectionné.
- **IO** — `(clé, valeur)` d'un fichier de configuration parcouru linéairement.

**`NkMakePair`, `NkGetFirst`/`NkGetSecond`, `NkGet<Index>`.** `NkMakePair` épargne l'écriture des
types : la version `const&` copie, et (sous `NK_CPP11`) une surcharge à forwarding parfait
**decaye** les types pour le stockage (références et `const` retirés). `NkGetFirst(p)` /
`NkGetSecond(p)` renvoient une référence vers le membre correspondant (avec surcharges `const`).
Sous `NK_CPP11`, l'accès **par indice** `NkGet<0>(p)` / `NkGet<1>(p)` mime `std::get` : un
`static_assert(Index < 2)` interdit tout indice ≥ 2 à la compilation, et la surcharge rvalue
(`NkGet<i>(NkMove(p))`) permet d'**extraire par déplacement**. Le trait `NkPairElement<Index, T1,
T2>` est le rouage interne : il expose le `Type` à l'indice et les statics `Get`.

### `NkTuple` à fond

**Structure récursive, coût nul.** Le template primaire `NkTuple<Types...>` n'est qu'une
**déclaration** ; seules deux spécialisations sont définies. `NkTuple<>` est le **tuple vide**, qui
termine la récursion : son `operator==` rend toujours `true`, son `operator<` toujours `false`, et
son `Swap` est un no-op. Le cas récursif `NkTuple<Head, Tail...>` stocke `Head mHead;` suivi d'un
`NkTuple<Tail...> mTail;`. Les éléments se retrouvent donc **contigus par valeur** — la récursion
est un artifice de *compilation*, aplati dans le code généré, sans surcoût mémoire ni indirection à
l'exécution.

**Construction.** Le constructeur par défaut value-initialise récursivement ; le constructeur
paramétré `NkTuple(const Head& head, const Tail&... tail)` reçoit toutes les valeurs et délègue la
queue au sous-tuple ; un constructeur de **conversion** accepte un tuple de types compatibles. Sous
`NK_CPP11` viennent le déplacement, le déplacement-conversion, et un constructeur à forwarding
parfait `explicit NkTuple(H&& head, Args&&... tail)`. **Détail à retenir** : ce constructeur
forwarding est marqué **`explicit`** (alors que celui de `NkPair` ne l'est pas) — une conversion
implicite vers un `NkTuple` depuis une liste d'arguments n'aura donc pas lieu silencieusement.

**Accès par indice `NkGet<Index>`.** C'est la seule manière de lire un élément. `NkGet<0>(t)`,
`NkGet<1>(t)`, etc., chacun rendant une référence du **bon type**. Un `static_assert(Index <
sizeof...(Types))` garantit que tout dépassement est une **erreur de compilation**. Trois surcharges
existent : non-const, const, et (sous `NK_CPP11`) rvalue pour l'**extraction par move**
(`NkGet<0>(NkMove(t))`). En coulisses, `NkTupleElement<Index, NkTuple<...>>` descend récursivement
de `mHead` en `mTail` jusqu'à l'indice voulu (et `NkTupleTypeAt` calcule le type à une position
donnée) — toute cette navigation est **résolue à la compilation**, donc gratuite à l'exécution.

**Comparaisons (membres) et `Swap`.** Particularité par rapport à `NkPair` : pour `NkTuple` les six
opérateurs sont des **méthodes membres**, pas des free functions. L'égalité compare `mHead` puis
récursivement `mTail` ; l'ordre est **lexicographique** (on départage élément par élément, du
premier au dernier). `Swap(other)` échange `mHead` via `traits::NkSwap` puis récurse sur `mTail` —
complexité `O(N)` (chaque swap individuel est `O(1)`).

**Taille.** `NkTupleSize(t)` rend `sizeof...(Types)` depuis une instance (l'équivalent de `len()` en
Python, résolu à la compilation) ; `NkTupleSizeTrait<TupleType>::value` donne la même chose à partir
du **type** seul, ce qui est indispensable en contexte template (`NkTupleSizeTrait<decltype(t)>::
value`).

Cas d'usage par domaine — `NkTuple` brille dès qu'on veut **plus de deux** valeurs hétérogènes ou un
nombre **paramétré** :

- **Retour multiple riche** — `NkTuple<bool, int, const char*>` pour « succès + code + message » ;
  `NkTuple<NkVec3, NkVec3, float>` pour « point + normale + distance » d'une intersection.
- **ECS / scène** — une **clé composite** à plusieurs champs (`NkTuple<int, usize>` comme identité
  combinée), exploitable grâce à l'ordre lexicographique dans `NkMap`/`NkSet`.
- **Rendu** — un état de pipeline empaqueté (`NkTuple<NkShader*, NkBlendMode, NkCullMode>`) servant
  de clé de cache ; les paramètres groupés d'un tirage.
- **Physique** — le résultat complet d'un balayage (`NkTuple<bool, float, NkVec3, BodyId>` : touché,
  temps, normale, corps).
- **Animation** — un échantillon multi-canal `(temps, position, rotation, échelle)` lu d'une piste.
- **Gameplay / IA** — un n-uplet de décision `(action, cible, score, coût)` produit par un
  planificateur ; une entrée de table de transition d'état.
- **Audio** — la description groupée d'une source `(buffer, gain, hauteur, position)`.
- **UI / 2D** — une couleur empaquetée `NkTuple<u8, u8, u8, u8>`, un rectangle `(x, y, w, h)`.
- **IO / GPU** — la signature d'un format de sommet décrite comme un tuple de types d'attributs,
  résolue à la compilation.

**`NkMakeTuple`.** La factory de choix : `NkMakeTuple(args...)` déduit et **decaye** les types pour
le stockage (références et `const` retirés). Elle repose sur le forwarding parfait, donc sur
`NK_CPP11`. Comme pour `NkMakePair`, ne comptez **pas** sur des types-références dans le résultat.

### Le piège `NkTupleConcat`

`NkTupleConcat(t1, t2)` **n'est pas opérationnel**. Sa signature promet de concaténer deux tuples en
un `NkTuple<Types1..., Types2...>`, mais son corps renvoie un tuple **default-construit** (un
`// TODO` marque l'implémentation manquante) : il **ignore `t1` et `t2` et perd toutes leurs
valeurs**. C'est le seul élément réellement défaillant de cette famille — ne l'utilisez pas tant
qu'il n'est pas implémenté ; concaténez manuellement avec `NkMakeTuple(NkGet<0>(t1), …,
NkGet<0>(t2), …)`.

### Le socle commun

- **Stockage par valeur, zéro allocation.** Ni `NkPair` ni `NkTuple` n'utilisent d'allocateur :
  leurs éléments vivent **dans l'objet**, comme des champs de `struct`. Aucune invalidation, aucun
  réagencement, aucune allocation dynamique interne — contrairement aux conteneurs séquentiels.
- **Résolution à la compilation.** Le nombre d'éléments et leur type sont connus du compilateur ;
  l'accès (`First`/`Second`, `NkGet<Index>`) et la taille (`NkTupleSize`) se résolvent **avant
  l'exécution**. Un indice hors-bornes est une **erreur de compilation**.
- **`NkMakePair`/`NkMakeTuple` decayent.** Les factories retirent références et `const` des types
  stockés (`traits::NkDecay_t`). Voulu, mais à connaître : on ne stocke jamais une référence.
- **Ordre lexicographique.** Les deux types se comparent membre par membre, du premier au dernier —
  d'où leur emploi naturel comme **clés** dans les conteneurs ordonnés ([Associative](Associative.md)).
- **Move semantics (NK_CPP11).** Construction, affectation et extraction par déplacement transfèrent
  des membres lourds sans copie ; tout ce pan est gardé par `#if defined(NK_CPP11)`.
- **Ce qui n'existe pas (encore).** Pas de *structured bindings* C++17 (`auto [a, b] = pair;` n'est
  **pas** câblé : pas de spécialisation `tuple_size`/`tuple_element` standard), pas de `NkApply` /
  `NkVisit` / `NkTupleSlice`, pas d'intégration `NkVariant`. Ces extensions sont mentionnées comme
  « recommandées » dans les en-têtes mais **ne sont pas implémentées**.

---

### Exemple

```cpp
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKContainers/Heterogeneous/NkTuple.h"
using namespace nkentseu;

// NkPair : retour double "succès + valeur", lu par les membres publics.
NkPair<bool, int> parsed = NkMakePair(true, 42);   // types déduits → NkPair<bool, int>
if (parsed.First) {
    int value = parsed.Second;
}

// NkPair comme clé composite : l'ordre lexicographique en fait une clé de NkMap/NkSet.
NkPair<int, int> range(0, 100);                    // (début, fin) : compare début puis fin

// NkTuple : un résultat riche à plusieurs champs hétérogènes.
auto hit = NkMakeTuple(true, 3.5f, "wall");        // NkTuple<bool, float, const char*>
bool        touched  = NkGet<0>(hit);              // accès par indice, vérifié à la compilation
float       distance = NkGet<1>(hit);
const char* surface  = NkGet<2>(hit);
usize       fields   = NkTupleSize(hit);           // 3, résolu à la compilation

// Échange idiomatique (ADL) sur l'une ou l'autre famille.
NkSwap(range, range);                              // délègue à range.Swap(...)
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Strings →](Strings.md)
