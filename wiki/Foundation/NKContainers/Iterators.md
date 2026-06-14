# Les itérateurs et la liste d'initialisation

> Couche **Foundation** · NKContainers · Parcourir une séquence sans savoir *de quoi elle est
> faite* : l'itérateur générique `NkIterator` (et son cousin const, son adaptateur inverse, ses
> traits et ses algorithmes), et la **vue en lecture seule** `NkInitializerList` qui transporte une
> séquence littérale sans allouer.

Dès qu'on écrit une fonction qui doit **traverser une collection** — sommer des échantillons,
appliquer une transformation à chaque sommet, chercher une entité — on bute sur une question : faut-il
écrire une version pour `NkVector`, une autre pour `NkList`, une troisième pour un tableau C brut ? La
réponse, depuis des décennies, est l'**itérateur** : un petit objet qui sait *avancer* sur une
séquence et *donner accès* à l'élément courant, en cachant complètement la structure qui le porte.
On écrit l'algorithme **une seule fois**, en termes de « début » et de « fin », et il marche sur tout
ce qui sait produire ces deux bornes. C'est exactement le principe de la STL — mais ici, **zéro
dépendance STL**, **zéro allocation**, tout `inline` et `noexcept`.

`NkIterator<T>` n'est, au fond, **rien d'autre qu'un pointeur emballé**. Sa seule donnée interne est
un `T* mPtr` ; `++` l'avance, `*` le déréférence. Ce n'est **pas** un conteneur, ce n'est **pas** un
objet lourd, et ce n'est **pas** un itérateur « sûr » qui vérifie ses bornes — il ne valide rien, par
choix de performance. Sa vraie richesse est ailleurs : un système de **catégories** (tags) qui décide,
à la compilation, quelles opérations existent. Un itérateur de tableau peut sauter en `O(1)`
(`it + 5`) ; un itérateur de liste chaînée ne le peut pas. Plutôt que d'exposer une arithmétique qui
planterait, NKContainers **désactive ces opérations par SFINAE** selon la catégorie — l'erreur est
attrapée par le compilateur, pas à l'exécution.

`NkInitializerList<T>`, lui, répond à un besoin voisin mais distinct : passer **une liste littérale**
(`{1, 2, 3}`) à une fonction ou à un constructeur, **sans allouer**. C'est une *vue* — deux pointeurs,
début et fin — sur une séquence contiguë dont **on ne possède pas** la mémoire. L'équivalent maison de
`std::initializer_list`, mais maîtrisé de bout en bout.

- **Namespace** : `nkentseu` (les types sont déclarés directement dans `nkentseu`)
- **Header parapluie** : `#include "NKContainers/Iterators/NkIterator.h"` et
  `#include "NKContainers/Iterators/NkInitializerList.h"`

> **Note.** Les déclarations effectives vivent dans `nkentseu`. Certains commentaires et macros des
> headers (`NK_FOREACH`, `NK_WHILEACH`) référencent `nkentseu::containers::GetBegin/GetEnd/…` : c'est
> une incohérence apparente des fichiers eux-mêmes ; le code réel reste sous `nkentseu`.

---

## Les catégories : ce que l'itérateur a *le droit* de faire

Avant de parcourir quoi que ce soit, il faut comprendre les **tags de catégorie**. Un itérateur
n'offre pas les mêmes opérations selon la structure qu'il parcourt, et c'est cette hiérarchie qui le
formalise :

```
NkInputIteratorTag            (lire, avancer)
   └─ NkForwardIteratorTag    (+ relire plusieurs fois)
        └─ NkBidirectionalIteratorTag   (+ reculer : --)
             └─ NkRandomAccessIteratorTag   (+ sauter : + - [] <)
```

`NkOutputIteratorTag` est **à part** (écriture seule, ne dérive de rien). La chaîne d'héritage n'est
pas décorative : c'est **elle** qui pilote l'activation SFINAE. `operator--` n'existe qu'à partir de
**Bidirectional** ; toute l'arithmétique (`+`, `-`, `+=`, `-=`, `[]`, `<`…) n'existe qu'en
**RandomAccess**. Si vous fabriquez un itérateur pour une liste chaînée, vous le déclarez en
`NkForwardIteratorTag` — et le compilateur **refusera** un `it + 3` qui n'aurait aucun sens.

Le défaut de `NkIterator<T, Category>` est `NkRandomAccessIteratorTag`, car son usage premier est de
parcourir un tableau contigu — où sauter coûte `O(1)`.

> **En résumé.** La catégorie est un *contrat de capacités* vérifié à la compilation. Input < Forward
> < Bidirectional < RandomAccess. Reculer (`--`) demande Bidirectional ; sauter (`+`, `[]`, `<`)
> demande RandomAccess. On choisit la catégorie selon ce que la structure sait vraiment faire.

---

## L'itérateur : `NkIterator` et `NkConstIterator`

`NkIterator<T>` est l'objet de parcours **mutable** : il enveloppe un `T* mPtr` unique et le promène.
On le déréférence (`*it`, `it->membre`), on l'avance (`++it`), on le compare (`it != end`). En
RandomAccess, on l'indexe (`it[3]`), on le décale (`it + 5`), on mesure une distance (`b - a`), on le
trie (`a < b`). Toutes ces opérations sont **`O(1)`** et `noexcept` — ce n'est qu'un pointeur, après
tout.

```cpp
int arr[5] = { 10, 20, 30, 40, 50 };
NkIterator<int> it(arr);          // pointe sur arr[0]
NkIterator<int> end(arr + 5);     // une case APRÈS le dernier
for (; it != end; ++it)
    total += *it;                 // 10 + 20 + 30 + 40 + 50
```

`NkConstIterator<T>` est la version **lecture seule** : son pointeur interne est `const T*`, son
`Reference` est `const T&`. La subtilité utile, c'est qu'un `NkIterator` se **convertit
implicitement** en `NkConstIterator` (mais jamais l'inverse) :

```cpp
NkIterator<int>      mut(arr);
NkConstIterator<int> ci = mut;    // conversion implicite : on n'écrira pas à travers ci
```

Ce n'est **pas** une copie défensive ni un objet « sûr » : aucune des deux versions ne vérifie ses
bornes ni la validité du pointeur. Déréférencer un itérateur en fin de séquence, ou indexer
hors-bornes, est un **comportement indéfini** — c'est le prix de la vitesse, assumé.

> **En résumé.** `NkIterator<T>` = pointeur mutable emballé ; `NkConstIterator<T>` = sa version
> lecture seule, vers laquelle le mutable se convertit tout seul. Déréférencer, avancer, comparer
> partout ; reculer/sauter/indexer selon la catégorie. **Aucune** vérification de bornes.

---

## Parcourir à l'envers : `NkReverseIterator`

Parfois on veut traverser **du dernier vers le premier** : dépiler dans l'ordre inverse d'empilement,
dessiner des calques de l'arrière vers l'avant, appliquer un *undo*. Plutôt que de réécrire la boucle,
on **adapte** l'itérateur existant : `NkReverseIterator<It>` enveloppe un itérateur et **inverse son
sens** — `++` recule sous le capot, `--` avance.

Le détail à connaître, et la source classique de confusion : un reverse iterator se construit à partir
de l'itérateur **placé après le dernier élément** (la borne `end`), et son `*` lit l'élément
**précédent** (`auto tmp = mIt; return *--tmp;`). C'est ce décalage d'un cran qui fait que `RBegin`
pointe « juste après la fin réelle » tout en donnant le dernier élément quand on le lit. Ses
comparaisons d'ordre (`<`, `>`…) sont elles aussi **inversées** par rapport à l'itérateur sous-jacent,
pour que l'ordre de parcours reste cohérent.

> **En résumé.** `NkReverseIterator` retourne le sens d'un itérateur : on le construit depuis la borne
> `end`, son `*` lit l'élément d'avant, et ses comparaisons d'ordre sont inversées. Idéal pour
> parcourir de la fin au début sans dupliquer la logique.

---

## La liste d'initialisation : `NkInitializerList`

`NkInitializerList<T>` résout un problème d'**ergonomie d'API** : comment écrire
`f({1, 2, 3})` proprement, sans que `f` n'alloue, et sans dépendre de la STL. La réponse est une
**vue non-propriétaire** : deux pointeurs, `const T* mBegin` et `const T* mEnd` (16 octets), qui
encadrent une séquence contiguë **dont on ne possède pas la mémoire**. C'est un *emprunt* — l'appelant
garantit que les données vivent assez longtemps.

```cpp
void SetUniformBlock(NkInitializerList<float> values);   // une vue, pas une copie
SetUniformBlock(NkMakeInitializerList<float>(0.f, 1.f, 0.5f));
```

On la parcourt comme un conteneur (`Begin`/`End`, `RBegin`/`REnd`), on l'indexe (`list[i]`), on lit
ses extrémités (`Front`, `Back`), sa taille (`Size`, `Empty`), son pointeur (`Data`). Et grâce à des
fonctions libres `begin`/`end`, le `range-based for` fonctionne directement :

```cpp
for (auto v : list)   // OK : begin(list)/end(list) renvoient des const T*
    Apply(v);
```

Ce n'est **pas** un conteneur : elle ne possède rien, ne libère rien, ne se redimensionne pas. Le
**piège mortel** est le *dangling* : ne jamais retourner ou stocker une `NkInitializerList` qui pointe
vers un tableau local — dès que ce tableau meurt, la vue pointe dans le vide.

> **En résumé.** `NkInitializerList<T>` = deux pointeurs `[begin, end)`, une **vue lecture seule
> non-propriétaire** sur une séquence contiguë. Parfaite en *paramètre* de fonction. Zéro allocation,
> aucun bounds-check — et **jamais** survivre aux données qu'elle référence.

---

## Aperçu de l'API

La liste de **tous** les éléments publics. Complexités entre crochets ; le détail (formule, usages)
suit dans la « Référence complète ». Les opérations marquées **[RA]** n'existent qu'en RandomAccess,
**[Bi+]** qu'à partir de Bidirectional.

### Catégories et traits

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Tags | `NkInputIteratorTag`, `NkForwardIteratorTag`, `NkBidirectionalIteratorTag`, `NkRandomAccessIteratorTag` | Hiérarchie de capacités (héritage Input→…→RandomAccess). |
| Tags | `NkOutputIteratorTag` | Écriture seule (indépendant). |
| Introspection | `NkIteratorTraits<It>` | Expose `ValueType`/`Pointer`/`Reference`/`DifferenceType`/`IteratorCategory`. |
| Spécialisations | `NkIteratorTraits<T*>`, `<const T*>`, `<NkIterator>`, `<NkConstIterator>`, `<NkInitializerList<T>>` | Traits prêts pour pointeurs bruts, itérateurs et liste (catégorie RandomAccess). |
| Détection | `NkIsIterator<T>` / `NkIsIterator_v<T>` | Vrai pour `T*`, `const T*`, `NkIterator`, `NkConstIterator`. |

### `NkIterator<T, Category>` / `NkConstIterator<T, Category>`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkIterator()`, `NkIterator(Pointer)` | Par défaut (`nullptr`) / depuis un pointeur. |
| Conversion | `NkConstIterator(const NkIterator<U>&)` | Mutable → const, implicite (SFINAE `!NK_IS_CONST_V(U)`). |
| Accès | `operator*` `[O(1)]`, `operator->` `[O(1)]` | Déréférencement / accès membre. |
| Avance | `++` / `++(int)` | Avancer (toutes catégories). |
| Recul | `--` / `--(int)` **[Bi+]** | Reculer. |
| Arithmétique | `+` `-` `+=` `-=` `[]` `[RA]`, `it - it` (distance) `[RA]` | Décalage, indexation, distance. |
| Comparaison | `==` `!=` · `== nullptr` `!= nullptr` | Égalité (toutes catégories) + test à `nullptr`. |
| Comparaison | `<` `<=` `>` `>=` **[RA]** | Ordre. |
| Hétérogène (const) | `== / !=` avec `NkIterator<T>` | Comparer const ↔ mutable. |

### `NkReverseIterator<Iterator>`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkReverseIterator()`, `NkReverseIterator(Iterator)` | Vide / depuis un itérateur (typiquement `end`). |
| Accès | `operator*` `[O(1)]`, `operator->` | Lit l'élément **précédent** (`--tmp`). |
| Avance/recul | `++` / `++(int)` (recule mIt) · `--` / `--(int)` **[Bi+]** | Sens inversé. |
| Arithmétique | `+` `-` `+=` `-=` `[]` `[RA]`, `it - it` `[RA]` | Décalage inversé, indexation `*(mIt-(offset+1))`. |
| Comparaison | `==` `!=` · `<` `<=` `>` `>=` **[RA]** (ordre **inversé**) | Égalité / ordre retourné. |

### `NkInitializerList<T>`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkInitializerList()`, `(first, last)`, `(data, size)`, `(const T(&)[N])`, `(std::initializer_list<T>)` | Vide / plage / pointeur+taille / tableau statique / interop STL. |
| Itération | `Begin`/`End`, `CBegin`/`CEnd`, `RBegin`/`REnd`, `CRBegin`/`CREnd` | Itérateurs avant / const / inverses. |
| Accès | `Data` `[O(1)]`, `Size` `[O(1)]`, `Empty` `[O(1)]`, `Front`, `Back`, `operator[]` | Pointeur / taille / vide ? / extrémités / indexé (sans bounds-check). |
| Comparaison | `operator==` `[O(n)]`, `operator!=` | Égalité élément par élément (early-exit). |
| Assignation | `operator=` (copie / `std::initializer_list` × 2) `[O(1)]` | Rebind des deux pointeurs. |

### Helpers et fonctions libres

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Bornes | `GetBegin`/`GetEnd` (tableau `T[N]` ou conteneur, const inclus) | Début/fin déduits — tableau (`+N`) ou délégation à `Begin()`/`End()`. |
| Navigation | `NkAdvance(it, n)` `[O(1) RA]`, `NkDistance(a, b)` `[O(1)]`, `NkNext(it, n=1)`, `NkPrev(it, n=1)` | Avancer en place / distance / copie décalée avant / arrière. |
| Parcours | `NkForeach`, `NkForeachIndexed`, `NkForeachWithBreak`, `NkWhileach` `[O(n)]` | Appliquer une opération sur chaque élément (avec index / arrêt / condition). |
| Liste | `NkMakeInitializerList(...)` (variadique / tableau / pointeur+taille / vide / `std::initializer_list`) | Fabriques de `NkInitializerList` (ou de helper temporaire). |
| Helper | `NkInitializerListHelper<T, N>` (+ spécialisation `<T, 0>`) | Buffer **owning temporaire** → convertible en `NkInitializerList`. |
| Range-for | `begin(list)` / `end(list)` | `const T*` pour `for (auto x : list)`. |
| Macros | `NK_FOREACH(...)`, `NK_WHILEACH(...)` | Sucre syntaxique de parcours (surcharge variadique). |

---

## Référence complète

Chaque élément est repris ici. Les pièces triviales (construction, opérateurs) sont brèves ; les
mécanismes structurants — catégories, SFINAE, vue non-propriétaire, algorithmes — sont traités **à
fond**, avec leurs usages dans les domaines du temps réel.

### Les tags de catégorie et `NkIteratorTraits`

Les cinq tags (`NkInputIteratorTag`, `NkOutputIteratorTag`, `NkForwardIteratorTag`,
`NkBidirectionalIteratorTag`, `NkRandomAccessIteratorTag`) sont des **structures vides** : leur seul
rôle est leur **type** et leur chaîne d'héritage. C'est un mécanisme classique de *tag dispatching* —
on choisit une implémentation selon la catégorie, à coût nul à l'exécution.

`NkIteratorTraits<It>` est la couche d'**introspection** qui rend les algorithmes génériques :
plutôt que d'écrire `It::ValueType` partout (ce qui casserait sur un pointeur brut `T*`, qui n'a pas de
membres), on passe par `NkIteratorTraits<It>::ValueType`. Les spécialisations couvrent les cas
réels : `T*` et `const T*` (catégorie RandomAccess, `DifferenceType = ptrdiff`), `NkIterator`,
`NkConstIterator`, et `NkInitializerList<T>`. C'est ce qui permet à `NkDistance` ou `NkForeach` de
fonctionner aussi bien sur un `NkIterator` que sur un `int*`.

- **ECS / scène** — un système écrit en termes de `NkIteratorTraits` parcourt indifféremment un
  tableau de composants contigu ou un itérateur d'archétype, sans surcharge.
- **Rendu** — un algorithme de transformation de sommets s'applique tel quel à un `NkVector<NkVertex>`
  ou à un `NkVertex*` brut venu d'un mapping GPU.
- **IO** — lire une plage d'octets `[begin, end)` sans se soucier de savoir si la source est un tableau
  ou un conteneur.

### `NkIterator` à fond

L'itérateur mutable est un **pointeur déguisé** : une seule donnée, `T* mPtr`, et toutes les
opérations en `O(1)`. Construit par défaut, il vaut `nullptr` (utile comme sentinelle) ; construit
depuis un pointeur, il pointe dessus. `operator*` rend `T&`, `operator->` permet `it->membre`.

Le cœur du design est la **gradation par catégorie** :

- **Toutes catégories** : `++`, `++(int)`, `==`, `!=`, et la comparaison à `nullptr`.
- **Bidirectional et plus** : `--`, `--(int)` — reculer n'a de sens que si la structure le permet.
- **RandomAccess seulement** : `+`, `-`, `+=`, `-=`, `operator[]`, la **distance** `b - a`, et l'ordre
  `<`, `<=`, `>`, `>=`.

Si vous tentez `it + 3` sur un itérateur Forward, **le code ne compile pas** — l'erreur est attrapée
au bon endroit, pas masquée par un comportement faux. Usages typiques :

- **Rendu** — boucler sur un tampon de sommets/indices : `for (auto it = GetBegin(verts); it != GetEnd(verts); ++it)`,
  avec saut indexé `it[i]` pour piocher un sommet précis.
- **ECS** — parcours linéaire d'un pool de composants ; la distance `end - begin` donne le compte
  exact en `O(1)`.
- **Physique / animation** — itérer sur des particules, des keyframes, en avançant pas à pas.
- **Audio** — balayer un buffer d'échantillons contigu (RandomAccess) pour appliquer un gain.

### `NkConstIterator` à fond

Même mécanique, mais **lecture seule** : `mPtr` est `const T*`, `Reference` est `const T&`. La pièce
qui fait toute la différence pratique est la **conversion implicite** depuis `NkIterator<U>` (gardée
par SFINAE `!NK_IS_CONST_V(U)`) : on passe librement un itérateur mutable là où un const est attendu
(`NkConstIterator<int> ci = mut;`), exactement comme un `T*` se convertit en `const T*`. L'inverse est
interdit — on ne *récupère* pas la mutabilité par accident.

`NkConstIterator` offre en plus des **comparaisons hétérogènes** (`==`, `!=`) avec l'itérateur
mutable, pour qu'une boucle mélangeant les deux (`while (constIt != mutableEnd)`) compile sans friction.

- **Rendu / GPU** — exposer une vue lecture seule d'un mesh à un système qui ne doit pas le muter.
- **UI / 2D** — parcourir une liste de widgets pour les dessiner sans risquer de la modifier en
  cours de route.
- **Réseau / IO** — itérer sur un buffer reçu, immuable par contrat.

### `NkReverseIterator` à fond

L'adaptateur inverse enveloppe un itérateur (`Iterator mIt`) et **renverse son sens de marche** : son
`++` décrémente `mIt`, son `--` (Bidirectional+) l'incrémente. Le décalage d'un cran — `*` lit
`*--tmp`, donc l'élément *avant* la position interne — est la convention qui rend l'adaptateur correct
quand on le construit depuis la borne `end`. Son arithmétique RandomAccess est elle aussi miroir
(`operator+` fait `mIt - offset`), et ses comparaisons d'ordre sont **inversées** (`<` renvoie
`mIt > other.mIt`).

- **Rendu 2D / UI** — dessiner des calques ou une pile de fenêtres du fond vers l'avant.
- **Gameplay** — rejouer un historique d'actions à l'envers (*undo*), parcourir un chemin du but vers
  le départ.
- **Physique** — itérer une liste de contacts dans l'ordre inverse de résolution.

### `GetBegin` / `GetEnd` — obtenir les bornes

Ces fonctions libres uniformisent l'obtention des bornes. Sur un **tableau brut** `T (&)[N]`, elles
déduisent la taille à la compilation et renvoient `NkIterator<T>(array)` / `NkIterator<T>(array + N)`
— plus besoin d'écrire `+ N` à la main ni de risquer une erreur de taille. Sur un **conteneur**, elles
**délèguent** à `container.Begin()` / `container.End()` (et leurs surcharges `const`). C'est la clé qui
permet aux algorithmes et aux macros de fonctionner indifféremment sur un tableau C ou sur un conteneur
maison. Coût `O(1)`.

### `NkAdvance`, `NkDistance`, `NkNext`, `NkPrev` — naviguer

Ces utilitaires expriment la navigation de façon générique, en passant par `NkIteratorTraits` :

- `NkAdvance(it, n)` déplace l'itérateur **en place** de `n` crans (`it += n`) — `O(1)` en
  RandomAccess.
- `NkDistance(first, last)` renvoie le nombre d'éléments entre deux itérateurs (`last - first`), `O(1)`
  pour un accès aléatoire — pratique pour compter sans boucler.
- `NkNext(it, n = 1)` renvoie une **copie** avancée de `n`, sans toucher l'original ; `NkPrev(it,
  n = 1)` une copie reculée. On les utilise pour viser « l'élément suivant » dans une expression sans
  perturber l'itérateur courant.

Usages : sauter à un sommet/échantillon décalé (rendu, audio), calculer le nombre d'entités dans une
plage (ECS), regarder l'élément suivant d'une liste de keyframes sans avancer le curseur principal
(animation).

### `NkForeach`, `NkForeachIndexed`, `NkForeachWithBreak`, `NkWhileach` — appliquer

Le cœur algorithmique : appliquer une opération à chaque élément, **une fonction écrite une fois** pour
toutes les séquences. Tous en `O(n)`.

- `NkForeach(begin, end, op)` appelle `op(*it)` sur chaque élément ; une **surcharge conteneur**
  (`NkForeach(container, op)`) déduit les bornes via `GetBegin`/`GetEnd`.
- `NkForeachIndexed(begin, end, op)` passe en plus l'**index** `nk_size i` : `op(*it, i)` — pour
  remplir un tampon GPU indexé, numéroter des lignes d'UI, écrire à un offset.
- `NkForeachWithBreak(begin, end, op, breakCond)` applique `op` puis **s'arrête** dès que
  `breakCond(*it)` est vrai — recherche du premier élément satisfaisant une condition, traitement
  jusqu'à un marqueur.
- `NkWhileach(begin, end, cond, op)` boucle **tant que** `cond(*begin)` reste vrai (et qu'on n'a pas
  atteint la fin) ; surcharge conteneur disponible — utile pour consommer un préfixe homogène d'une
  séquence (tokens d'un même type, runs d'une RLE).

Usages transversaux : transformer en masse des composants (ECS), appliquer un gain échantillon par
échantillon (audio), mettre à jour chaque particule (physique), dessiner chaque glyphe (UI/2D),
parcourir des octets jusqu'à un délimiteur (IO).

### `NkIsIterator` — détecter un itérateur

`NkIsIterator<T>` (et son raccourci `NkIsIterator_v<T>`) répondent « ce type est-il un itérateur ? » :
faux par défaut, vrai pour `T*`, `const T*`, `NkIterator` et `NkConstIterator`. C'est un outil de
**métaprogrammation** pour contraindre des templates (n'accepter qu'un vrai itérateur) ou choisir une
surcharge — la même philosophie que les traits de [Type-traits](Utilities.md), appliquée aux
itérateurs.

### `NkInitializerList` à fond

La vue littérale est **deux pointeurs** `[mBegin, mEnd)` et **rien d'autre** : pas de taille stockée
(elle se déduit `mEnd - mBegin`), pas de propriété de la mémoire. Tous ses membres sont `constexpr` +
`noexcept` + inline.

**Construire.** Cinq façons : vide ; depuis une plage `(first, last)` ; depuis `(data, size)` ; depuis
un tableau statique `(const T(&)[N])` (la taille est déduite) ; et depuis un `std::initializer_list`
pour **interopérer** avec la syntaxe `{…}` native du langage.

**Parcourir.** `Begin`/`End` (qui rendent des `NkConstIterator<T>`), plus les variantes const
(`CBegin`/`CEnd`) et inverses (`RBegin`/`REnd`, `CRBegin`/`CREnd`). Et grâce aux `begin`/`end` libres,
le `range-based for` marche directement.

**Lire.** `Data()` (pointeur début ou `nullptr`), `Size()` (`mEnd - mBegin`), `Empty()`, `Front()`
(`*mBegin`), `Back()` (`*(mEnd-1)`), `operator[](i)` (`mBegin[i]`). **Aucun bounds-check** : `Front`,
`Back`, `[]` sur une liste vide ou hors-bornes sont du comportement indéfini.

**Comparer.** `operator==` est `O(n)` : même taille **et** égalité élément par élément, avec
*early-exit* à la première différence ; `operator!=` en est la négation. **Assigner** rebinde les deux
pointeurs en `O(1)` (depuis une autre liste ou un `std::initializer_list`).

Usages :

- **Rendu / GPU** — passer un petit bloc de constantes (`{r, g, b, a}`, une matrice aplatie) à une
  fonction de configuration de pipeline, sans allouer.
- **UI / 2D** — déclarer un jeu d'items de menu, une palette de couleurs, en littéral.
- **Gameplay** — énumérer des directions cardinales, un set d'états autorisés, en argument d'appel.
- **Audio** — fournir une courte table de coefficients de filtre.

Le **piège** reste le même partout : non-propriétaire ⇒ ne jamais survivre aux données pointées (pas
de `return` d'une liste vers un tableau local ; préférer un `static const` ou des données à durée de
vie suffisante).

### `NkInitializerListHelper` et `NkMakeInitializerList` — fabriquer

Comment construire une `NkInitializerList` à partir d'**arguments** (`NkMakeInitializerList<float>(1, 2,
3)`) sans STL ? Via un **helper owning temporaire** : `NkInitializerListHelper<T, N>` contient un
buffer interne `T storage[N]` qui **copie** les arguments, et se **convertit** implicitement en
`NkInitializerList<T>` pointant sur ce buffer. La spécialisation `<T, 0>` gère la liste vide.

Les fabriques `NkMakeInitializerList` couvrent tous les cas (`[[nodiscard]] constexpr noexcept`) : la
variadique `(Args&&...)` qui passe par le helper (`{static_cast<T>(args)...}`), la version tableau
`(const T(&)[N])`, la version `(data, size)`, la version vide, et l'interop `(std::initializer_list)`.

**Le piège critique** : la variadique renvoie un helper avec **buffer interne temporaire**. À
consommer **immédiatement dans l'appel** — `f(NkMakeInitializerList<int>(1, 2, 3))` est correct, mais
`auto bad = NkMakeInitializerList<int>(1, 2, 3);` puis usage différé donne une vue *dangling* (le
buffer temporaire est détruit). Les versions tableau / pointeur+taille, elles, pointent sur des données
que **vous** possédez : pas de helper, pas de piège.

### Les macros `NK_FOREACH` et `NK_WHILEACH`

Sucre syntaxique optionnel (hors namespace), à surcharge variadique :

- `NK_FOREACH(container, value)` ou `NK_FOREACH(container, qualifier, value)` (ex.
  `NK_FOREACH(vec, const auto&, x)`) — déroule une boucle de parcours via `GetBegin`/`GetEnd`.
- `NK_WHILEACH(container, value, condition)` ou `NK_WHILEACH(container, qualifier, value, condition)` —
  parcourt tant que `condition(*it)` reste vrai.

Pratique pour raccourcir une boucle ; mais l'idiome direct (`for (auto& x : v)` rendu possible par les
`begin`/`end`) reste souvent plus lisible. (Note : un commentaire d'exemple du header évoque
`nkforeach` en minuscules, alors que la macro définie est `NK_FOREACH` en majuscules — incohérence du
header.)

### Pièges à retenir

- **SFINAE par catégorie** — sous RandomAccess, l'arithmétique (`+`, `-`, `[]`, `<`…) n'existe pas ;
  sous Bidirectional, `--` non plus. Choisir la `Category` selon les capacités réelles (liste chaînée
  → `NkForwardIteratorTag`).
- **Aucun bounds-check** sur `*`, `[]`, `Front`, `Back` — UB si vide ou hors-bornes. C'est délibéré
  (performance).
- **`NkInitializerList` non-propriétaire** — jamais retourner/stocker une liste vers un tableau local.
- **`NkMakeInitializerList<T>(args...)`** — buffer temporaire : consommer **dans l'appel**, ne pas
  stocker.
- **Pas d'allocateur** ici — ces deux fichiers ne touchent jamais NKMemory : ce sont des vues et des
  wrappers de pointeurs, **zéro allocation**.

---

### Exemple récapitulatif

```cpp
#include "NKContainers/Iterators/NkIterator.h"
#include "NKContainers/Iterators/NkInitializerList.h"
using namespace nkentseu;

int arr[5] = { 10, 20, 30, 40, 50 };

// Bornes déduites d'un tableau brut, parcours classique.
auto it  = GetBegin(arr);
auto end = GetEnd(arr);
int  sum = 0;
for (; it != end; ++it) sum += *it;            // 150

// Distance et navigation en O(1) (RandomAccess).
auto n   = NkDistance(GetBegin(arr), GetEnd(arr));   // 5
auto mid = NkNext(GetBegin(arr), 2);                 // pointe sur arr[2] == 30

// Vue const + conversion implicite, sans copier les données.
NkConstIterator<int> ci = GetBegin(arr);
int first = *ci;                                // 10 (lecture seule)

// Algorithme générique : appliquer une opération à chaque élément.
NkForeach(GetBegin(arr), GetEnd(arr), [&](int v) { total += v; });

// Liste littérale passée en argument (vue, zéro allocation) — consommée dans l'appel.
void SetClearColor(NkInitializerList<float> rgba);
SetClearColor(NkMakeInitializerList<float>(0.1f, 0.2f, 0.3f, 1.f));

// range-based for sur une NkInitializerList.
for (float c : NkMakeInitializerList<float>(0.f, 0.5f, 1.f))
    Submit(c);
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Les conteneurs séquentiels →](Sequential.md)
