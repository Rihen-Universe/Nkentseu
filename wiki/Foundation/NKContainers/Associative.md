# Les conteneurs associatifs

> Couche **Foundation** · NKContainers · Associer une **clé** à une **valeur**, ou ranger un
> **ensemble** d'éléments uniques : tables de hachage (`NkHashMap`, `NkUnorderedMap`,
> `NkUnorderedSet`), arbres ordonnés (`NkMap`, `NkSet`, `NkBinaryTree`, `NkBTree`), trie de
> préfixes (`NkTrie`) et file de priorité (`NkPriorityQueue`).

Les conteneurs séquentiels répondent à « range ces éléments dans un ordre ». Les conteneurs
**associatifs** répondent à une question différente : « **retrouve-moi** rapidement quelque chose à
partir d'une **clé** » — la valeur d'un identifiant d'entité, l'existence d'un mot dans un
dictionnaire, le prochain événement le plus urgent. Dès qu'on cherche par clé plutôt que par
position, parcourir un `NkVector` jusqu'à trouver (`O(n)`) devient le mauvais réflexe : on veut
`O(1)` ou `O(log n)`. Toute la page tient en un arbitrage : **le hachage est le plus rapide mais
n'ordonne rien ; l'arbre est un peu plus lent mais garde les clés triées**. Le reste — trie, tas,
arbres spécialisés — répond à des besoins plus pointus (préfixes, priorité, gros volumes).

Tous ces types sont **templatés** sur leurs clés/valeurs et conscients de l'**allocateur**
(`NkHashMap<Key, Value, Allocator = memory::NkAllocator, ...>`) : la mémoire des nœuds passe
**toujours** par NKMemory (`mAllocator->Allocate` + placement `new`, jamais `new`/`delete`), et
l'allocateur est passé **par pointeur** au constructeur sans être possédé (fallback sur
`memory::NkGetDefaultAllocator()`). Les tables et arbres exposent souvent une API en **deux casses**
(`Begin`/`begin`) pour le `range-based for`, mais — attention — la couverture varie d'un conteneur à
l'autre (voir plus bas).

- **Namespace** : `nkentseu`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## Les tables de hachage : `NkHashMap`, `NkUnorderedMap`, `NkUnorderedSet`

Une **table de hachage** est la structure « recherche `O(1)` » par excellence. L'idée : passer la
clé dans une fonction de **hachage** qui produit un entier, le ramener au nombre de *buckets*
(seaux), et ranger la paire dans le seau correspondant. Retrouver une clé revient à re-hacher et
aller directement au bon seau — sans parcourir le reste. Les trois conteneurs de cette famille
partagent la même mécanique interne : **chaînage séparé** (chaque bucket est une petite liste
chaînée de nœuds, pour absorber les collisions), **16 buckets** au départ, un **facteur de charge**
maximal de `0.75`, et un **doublement** du nombre de buckets dès qu'on dépasse ce seuil (un
*rehash*, qui redistribue tous les nœuds).

Ce n'est **pas** une structure ordonnée : l'ordre de parcours n'a aucun sens (il suit les buckets,
pas les clés). Si vous avez besoin que les clés ressortent triées, ce n'est pas le bon outil — c'est
`NkMap`/`NkSet`, plus bas.

`NkHashMap` est la map à hachage **complète et la plus sûre**. Son hasher par défaut,
`NkHashMapDefaultHasher`, **délègue à `NkHash<Key>`** (le hash maison de NKContainers, spécialisé
pour les entiers, flottants et `NkString`), ce qui fonctionne correctement même pour les types qui
contiennent des pointeurs internes. Elle offre l'API la plus riche : `At`, `TryGet`, `FindIterator`,
`InsertOrAssign`, `operator[]`, le réglage du facteur de charge (`SetMaxLoadFactor`), `Reserve`, et
des itérateurs `Iterator`/`ConstIterator` complets.

```cpp
NkHashMap<NkString, Texture*> textureCache;
textureCache.Insert("hero_diffuse", tex);
if (Texture** t = textureCache.Find("hero_diffuse")) {   // O(1) amorti
    Use(*t);
}
Texture*& slot = textureCache["hero_normal"];            // insère nullptr si absent
```

`NkUnorderedMap` est une variante « API standardisée » de la même map. **Différence cruciale** : son
hasher par défaut, `NkUnorderedMapDefaultHasher`, fait du **FNV-1a sur les octets bruts** de la clé
(`reinterpret_cast<const nk_uint8*>(&key)`, `sizeof(Key)` octets). C'est rapide et correct pour des
types « plats » (entiers, énums, petites structs sans pointeur), mais **faux pour tout type
contenant des pointeurs internes** (deux `NkString` égales peuvent avoir des octets différents) — il
faut alors fournir un `Hasher` custom. Côté API, elle remplace `FindIterator`/`At`/`TryGet` par un
`ForEach(fn)` pratique (`fn` reçoit `(const Key&, Value&)`), mais perd `MaxLoadFactor`/`Reserve`.

`NkUnorderedSet` est l'**ensemble** correspondant : des éléments uniques, sans valeur associée, même
hachage FNV-1a brut (donc même précaution sur le hasher custom). Son `Insert` **renvoie `true` si
l'élément a été inséré, `false` s'il existait déjà** — pratique pour dédupliquer en une ligne. À
noter : il **n'expose aucun itérateur** (limitation connue), et son `Rehash` est privé.

> **En résumé.** Tables de hachage = recherche `O(1)` amorti, **aucun ordre**. `NkHashMap` = la plus
> complète et la **plus sûre** (hash via `NkHash`, marche partout). `NkUnorderedMap`/`NkUnorderedSet`
> = hash FNV-1a **sur octets bruts** → corrects pour types plats, mais fournissez un `Hasher` custom
> dès qu'une clé contient un pointeur (ex. `NkString`). `NkUnorderedSet::Insert` renvoie un booléen
> de nouveauté ; il n'a pas d'itérateurs.

---

## Les arbres ordonnés : `NkMap`, `NkSet`

Quand on veut que les clés restent **triées** — pour les parcourir dans l'ordre, trouver le plus
petit/grand, ou faire des requêtes de plage — la table de hachage ne convient plus. La réponse est
un **arbre binaire de recherche équilibré**, ici un **arbre Rouge-Noir** : chaque nœud garde ses
enfants gauche (plus petit) et droit (plus grand), et les rotations/recoloriages maintiennent
l'arbre équilibré, garantissant `Insert`/`Find`/`Contains` en **`O(log n)`**. Le parcours **in-order**
(via les itérateurs) ressort les clés en **ordre croissant**, gratuitement.

`NkMap` est la map ordonnée clé→valeur. Son comparateur par défaut est `NkMapLess` (`operator<`) ;
pour un tri **décroissant**, on passe un foncteur inversé en paramètre template. Elle offre `Insert`,
`Find` (renvoie un `Value*`), `FindIterator`, `At`, `TryGet`, `InsertOrAssign`, `operator[]`, et un
itérateur bidirectionnel **par conception** (l'incrément est implémenté ; `ConstIterator` est un
simple alias d'`Iterator`).

`NkSet` est l'ensemble ordonné équivalent (mêmes arbres, comparateur `NkSetLess`). Sa recherche
diffère de celle de `NkMap` : **`NkSet::Find` renvoie un *itérateur*** (`end()` si absent), là où
`NkMap::Find` renvoie un pointeur. Son `Insert` renvoie un booléen de nouveauté comme l'unordered
set.

```cpp
NkSet<int> sortedIds;
sortedIds.Insert(42);
sortedIds.Insert(7);
for (int id : sortedIds) { /* 7 puis 42 : ordre croissant garanti */ }
```

**Piège de performance commun** aux deux : leur `Erase` est actuellement en **`O(n log n)`** (une
recopie naïve de tout l'arbre — le vrai RB-Delete est un TODO). Tant qu'on insère et qu'on parcourt,
ils sont au top ; si votre charge est faite de **suppressions massives**, mesurez.

> **En résumé.** Arbres Rouge-Noir = clés **triées**, `Insert`/`Find`/`Contains` en `O(log n)`,
> parcours in-order croissant gratuit. `NkMap` (clé→valeur, `Find` → `Value*`) ; `NkSet` (éléments
> uniques, `Find` → **itérateur**, `Insert` → booléen). Tri inversé via comparateur custom. Réserve :
> `Erase` en `O(n log n)` pour l'instant.

---

## Les arbres spécialisés : `NkBinaryTree`, `NkBTree`, `NkTrie`

Au-delà des maps/sets génériques, trois arbres répondent à des besoins ciblés.

`NkBinaryTree` est un **arbre binaire de recherche non équilibré**, volontairement simple et
transparent : sa struct `Node{ T Value; Node* Left; Node* Right; }` est **publique**, on accède à la
racine (`GetRoot()`), et surtout il expose **quatre parcours par visiteur** — `InOrder` (trié
croissant), `PreOrder`, `PostOrder` et `LevelOrder` (BFS). C'est l'outil pédagogique et utilitaire :
analyser une hiérarchie, parcourir un BSP/quadtree de démonstration, calculer une `Height()`, vérifier
`IsBalanced()`, trouver `Min`/`Max`. Comme il n'est **pas** auto-équilibré, une insertion triée le
dégénère en liste (`O(n)`) ; pour de la perf garantie, prenez `NkMap`/`NkSet`.

`NkBTree` est un **arbre B** multi-voies (`order` clampé à 3 minimum, jusqu'à `2*order-1` clés par
nœud) auto-équilibré, pensé pour les **gros volumes** où l'on minimise le nombre de nœuds visités —
le modèle des index de bases de données et des systèmes de fichiers. Son `Insert` et son `Search`
sont en `O(log_m n)` (logarithme en base de l'ordre `m`). Il accepte les **doublons**. Attention : la
**suppression n'est pas implémentée**, et il n'a pas d'itérateurs.

`NkTrie` est un **arbre de préfixes** pour chaînes C, à l'alphabet **fixe `a-z` (26 lettres),
insensible à la casse**. Chaque nœud a 26 enfants ; un mot est un chemin de la racine à un nœud
marqué fin-de-mot. C'est *la* structure de l'**autocomplétion** et du test de préfixe : `Insert`,
`Search` (mot complet), `StartsWith` (préfixe partiel), et `FindWordsWithPrefix` (tous les mots
commençant par un préfixe) sont en `O(L)` sur la longueur de la chaîne — indépendant du nombre de
mots stockés.

```cpp
NkTrie<> dico;
dico.Insert("sword");
dico.Insert("shield");
bool has = dico.Search("sword");          // true
bool pre = dico.StartsWith("sh");         // true (préfixe de "shield")
NkVector<const char*> sugg = dico.FindWordsWithPrefix("s");  // autocomplétion
```

> **En résumé.** `NkBinaryTree` = BST simple **non équilibré**, struct `Node` publique, parcours par
> visiteur (`InOrder`/`PreOrder`/`PostOrder`/`LevelOrder`) — utilitaire et lisible, mais sans garantie
> de perf. `NkBTree` = arbre B multi-voies pour **gros volumes** (`O(log_m n)`, doublons OK,
> **pas de suppression**). `NkTrie` = trie de **préfixes** `a-z` (autocomplétion `O(L)`), mais
> `FindWordsWithPrefix` renvoie des pointeurs **temporaires** (à copier — voir Référence).

---

## La file de priorité : `NkPriorityQueue`

Dernier conteneur, à part : `NkPriorityQueue` ne range pas par clé mais par **priorité**. On y pousse
des éléments dans n'importe quel ordre, et `Top()` rend **toujours** le plus prioritaire ; `Pop()` le
retire. En interne, c'est un **tas binaire** (*binary heap*) stocké à plat dans un `NkVector`
(`parent=(i-1)/2`, `left=2i+1`, `right=2i+2`), ce qui donne `Push` et `Pop` en **`O(log n)`** et
`Top` en `O(1)`. Par défaut c'est un **max-heap** (l'élément le plus *grand* au sommet, via
`NkPriorityLess` qui utilise `operator<`) ; pour un **min-heap**, on fournit un comparateur inversé.

C'est l'ossature de tout ce qui se traite « par ordre d'importance » : la frontière d'un **A\***
(le nœud de plus petit coût d'abord — min-heap), une file de **tâches** ordonnancées, un **scheduler**
d'événements temporels, une sélection des `k` plus proches/plus visibles. Le tas n'a **pas
d'itérateurs** (les parcourir n'aurait pas de sens : seul le sommet est ordonné).

> **En résumé.** `NkPriorityQueue` = tas binaire, `Top()` `O(1)`, `Push`/`Pop` `O(log n)`,
> **max-heap par défaut** (comparateur inversé pour min-heap). L'outil du A\*, des schedulers et de
> toute file « par priorité ». Pas d'itérateurs.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par conteneur. Complexités entre crochets quand utile.
L'allocateur (`Allocator* = nullptr`) est omis des signatures ci-dessous pour la lisibilité.

### `NkHashMap<Key, Value, Hasher, KeyEqual>` — table de hachage (chaînage séparé)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkHashMap()`, `(NkInitializerList)`, `(std::initializer_list)`, copie, déplacement | Vide / liste / liste STL / copie profonde `[O(n)]` / vol de pointeurs `[O(1)]` |
| Affectation | `operator=` (copie, déplacement, `NkInitializerList`, `std::initializer_list`) | Réaffectation (la copie préserve l'allocateur courant) |
| Capacité | `Empty`, `Size`, `BucketCount`, `LoadFactor`, `MaxLoadFactor`, `SetMaxLoadFactor` | Vide ? / compte / nb seaux / charge / charge max / régler la charge max |
| Modification | `Clear` `[O(n)]`, `Rehash` `[O(n)]`, `Reserve`, `Swap` `[O(1)]` | Vider / redistribuer / pré-réserver / échanger |
| Insertion | `Insert` `[O(1)*]`, `InsertOrAssign`, `operator[]` | Insérer/mettre à jour ; insérer-ou-affecter (true si nouveau) ; accès-création |
| Suppression | `Erase` `[O(1)*]`, `Remove` (alias d'`Erase`) | Retirer par clé |
| Recherche | `Find`, `Contains`, `FindIterator`, `TryGet`, `At` | Pointeur / présence / itérateur / copie-si-présent / accès vérifié (assert) |
| Itération | `Begin`/`End`, `CBegin`/`CEnd`, `begin`/`end`/`cbegin`/`cend` | Itérateurs `Iterator`/`ConstIterator` (forward) ; **invalidés par Insert/Erase/Rehash** |
| Foncteurs | `NkHashMapDefaultHasher` (→ `NkHash<Key>`), `NkHashMapDefaultEqual` (`==`) | Hachage **sûr** par défaut |

### `NkUnorderedMap<Key, Value, Hasher, KeyEqual>` — table de hachage « standardisée »

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction / Affectation | `NkUnorderedMap()`, `(NkInitializerList)`, `(std::initializer_list)`, copie, déplacement ; `operator=` (×4) | Mêmes formes que `NkHashMap` |
| Capacité | `Empty`/`empty`, `Size`/`size`, `BucketCount`, `LoadFactor` | (Pas de `MaxLoadFactor`/`Reserve`) |
| Modification | `Clear`, `Rehash`, `Insert` `[O(1)*]`, `Erase` `[O(1)*]` | Vider / redistribuer / insérer / retirer |
| Accès | `Find`, `Contains`, `operator[]`, `ForEach(fn)` | Pointeur / présence / accès-création ; `fn(const Key&, Value&)` |
| Itération | `Begin`/`End`, `CBegin`/`CEnd`, `begin`/`end`/`cbegin`/`cend` | Itérateurs internes (pas de conversion Iterator→ConstIterator) |
| Foncteurs | `NkUnorderedMapDefaultHasher` (**FNV-1a octets bruts**), `NkUnorderedMapDefaultEqual` (`==`) | Hachage **plat seulement** |

### `NkUnorderedSet<T, Hasher, Equal>` — ensemble à hachage

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction / Affectation | `NkUnorderedSet()`, `(NkInitializerList)`, `(std::initializer_list)`, copie, déplacement ; `operator=` (×4) | — |
| Capacité | `Empty`, `Size` | Vide ? / compte |
| Modification | `Clear`, `Insert` `[O(1)*]`, `Erase` `[O(1)*]` | `Insert` **renvoie `true` si inséré, `false` si doublon** |
| Recherche | `Contains` | Présence |
| (Interne) | `Rehash` **privé**, **pas d'itérateurs** | Limitations connues |
| Foncteurs | `NkUnorderedSetDefaultHasher` (**FNV-1a octets bruts**), `NkUnorderedSetDefaultEqual` (`==`) | Hachage **plat seulement** |

### `NkMap<Key, Value, Compare>` — map ordonnée (arbre Rouge-Noir)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction / Affectation | `NkMap()`, `(NkInitializerList)`, `(std::initializer_list)`, copie `[O(n log n)]`, déplacement ; `operator=` (×4) | — |
| Capacité | `Empty`, `Size` | Vide ? / compte |
| Modification | `Clear` `[O(n)]`, `Insert` `[O(log n)]`, `InsertOrAssign`, `operator[]` | Vider / insérer-maj / insérer-ou-affecter / accès-création |
| Suppression | `Erase(const Key&)` `[O(n log n)]`, `Erase(Iterator)` | Par clé / par itérateur (renvoie `end()`) |
| Recherche | `Find` (→ `Value*`), `Contains`, `FindIterator`, `At`, `TryGet` | Pointeur / présence / itérateur / accès vérifié / copie-si-présent |
| Itération | `begin`/`end`, `Begin`/`End` (in-order **croissant**) | `ConstIterator = Iterator` ; bidirectionnel (incrément seul implémenté) |
| Comparateur | `NkMapLess` (`<`) | Tri inversé via foncteur custom |

### `NkSet<T, Compare>` — ensemble ordonné (arbre Rouge-Noir)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction / Affectation | `NkSet()`, `(NkInitializerList)`, `(std::initializer_list)`, copie, déplacement ; `operator=` (×4) | — |
| Capacité | `Empty`, `Size` | Vide ? / compte |
| Modification | `Clear`, `Insert` `[O(log n)]`, `Erase` `[O(n log n)]` | `Insert` **renvoie true si inséré, false si doublon** |
| Recherche | `Find` (→ **`Iterator`**), `Contains` `[O(log n)]` | `Find` renvoie un **itérateur** (`end()` si absent) |
| Itération | `begin`/`end` **uniquement** (in-order croissant) | `ConstIterator = Iterator`, lecture seule |
| Comparateur | `NkSetLess` (`<`) | Tri inversé via foncteur custom |

### `NkBinaryTree<T>` — BST non équilibré

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBinaryTree()`, copie ; `operator=` (copie) | Pas de move |
| Capacité | `Empty`, `Size`, `Height` `[O(n)]` | Vide ? / compte / hauteur |
| Modification | `Clear` `[O(n)]`, `Insert` `[O(h)]` | Vider ; insérer (doublons **ignorés**) |
| Recherche | `Contains`, `Find` (→ `const T*`), `GetRoot` (→ `Node*`) | Présence / pointeur / racine (struct `Node` **publique**) |
| Parcours | `InOrder`, `PreOrder`, `PostOrder`, `LevelOrder` (visiteur) | In-order trié ; pré/post ; BFS niveau par niveau |
| Analyse | `Min`, `Max`, `IsBalanced` | Extrêmes (assert si vide) ; critère AVL `[O(n)]` |

### `NkBTree<T>` — arbre B multi-voies

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBTree(order = 3)` | `order` clampé à ≥ 3 ; pas de copie/move |
| Insertion | `Insert` `[O(log_m n)]` | Doublons acceptés ; gère le split de racine. **Pas de suppression** |
| Recherche | `Search` `[O(log_m n)]` | Présence |
| Capacité | `Size`, `Empty` | Compte / vide ? |

### `NkTrie<>` — trie de préfixes `a-z`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkTrie()`, `Clear` | Crée/recrée la racine ; pas de copie/move |
| Insertion | `Insert` `[O(L)]` | Insensible à la casse, unicité. **Pas de suppression de mot** |
| Recherche | `Search` `[O(L)]`, `StartsWith` `[O(L)]` | Mot complet (requiert fin-de-mot) / préfixe partiel |
| Autocomplétion | `FindWordsWithPrefix` `[O(L+K)]` | Tous les mots d'un préfixe — **pointeurs temporaires** (à copier) |
| Capacité | `Size`, `Empty` | Nb de mots complets / vide ? |

### `NkPriorityQueue<T, Compare>` — file de priorité (tas binaire)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction / Affectation | `NkPriorityQueue()`, `(Allocator*)`, `(NkInitializerList)`, `(std::initializer_list)` ; `operator=` (listes) | Pas de copie/move déclarés |
| Accès | `Top` `[O(1)]`, `Empty`, `Size`, `GetAllocator` | Sommet (assert si vide) / vide ? / compte / allocateur |
| Modification | `Push(const T&)`/`Push(T&&)` `[O(log n)]`, `Emplace(args…)`, `Pop` `[O(log n)]`, `Clear` `[O(n)]`, `Swap` `[O(1)]` | Insérer (copie/move/in-place) ; retirer le sommet ; vider ; échanger |
| Comparateur | `NkPriorityLess` (`<` → **max-heap**) | Min-heap via foncteur inversé |
| Libre | `NkSwap(a, b)` `[O(1)]` | Échange de deux files |

---

## Référence complète

Chaque élément repris en détail. Le trivial est bref ; ce qui décide d'un choix d'architecture est
développé à fond, avec ses usages par domaine.

### Choisir : le tableau de décision

Le critère premier est **ce que vous demandez à la structure** :

- **Retrouver une valeur par clé, le plus vite possible, l'ordre m'est égal** → `NkHashMap`
  (ou `NkUnorderedMap` pour des clés plates).
- **Tester l'appartenance / dédupliquer, sans ordre** → `NkUnorderedSet`.
- **Les clés doivent rester triées (parcours ordonné, min/max, plages)** → `NkMap` / `NkSet`.
- **Compléter un mot, tester un préfixe** → `NkTrie`.
- **Traiter toujours « le plus prioritaire d'abord »** → `NkPriorityQueue`.
- **Index sur gros volume / doublons multi-voies** → `NkBTree`. **BST lisible à parcourir** → `NkBinaryTree`.

| Besoin | Find/Contains | Insert | Erase | Ordre | Itérateurs |
|--------|---------------|--------|-------|-------|------------|
| `NkHashMap` | **O(1)\*** | O(1)\* | O(1)\* | aucun | oui |
| `NkUnorderedMap` | **O(1)\*** | O(1)\* | O(1)\* | aucun | oui |
| `NkUnorderedSet` | **O(1)\*** | O(1)\* | O(1)\* | aucun | **non** |
| `NkMap` | O(log n) | O(log n) | **O(n log n)** | trié | oui (in-order) |
| `NkSet` | O(log n) | O(log n) | **O(n log n)** | trié | oui (in-order) |
| `NkBinaryTree` | O(h) | O(h) | — | trié (in-order) | non (visiteurs) |
| `NkBTree` | O(log_m n) | O(log_m n) | **non impl.** | trié | non |
| `NkTrie` | O(L) | O(L) | **non impl.** | préfixe | non |
| `NkPriorityQueue` | `Top` O(1) | O(log n) | `Pop` O(log n) | priorité | non |

(\*) amorti : `O(1)` en moyenne, `O(n)` lors d'un rehash.

### `NkHashMap` à fond

C'est la map **de référence** du moteur. Sa sécurité vient de son hasher par défaut, qui délègue à
`NkHash<Key>` (spécialisé pour `int32/uint32/int64/uint64/float32/float64/NkString`) : pour un type
custom, il suffit de fournir `template<> struct NkHash<MyType>`. Le facteur de charge `0.75` et le
doublement gardent les chaînes de collision courtes ; `Reserve(n)` pré-dimensionne pour éviter les
rehash dans une boucle de chargement, et `SetMaxLoadFactor` ajuste le compromis mémoire/vitesse.

Côté accès, l'éventail est large : `Find` (pointeur, `nullptr` si absent), `Contains` (présence),
`FindIterator` (itérateur, `End()` si absent), `TryGet` (copie dans `outValue`), `At` (référence
vérifiée, **assert** si absent), `InsertOrAssign` (renvoie `true` si c'était une nouvelle clé), et
`operator[]` (crée `Value()` si absent — `Value` doit être *default-constructible*, et l'insertion
**invalide les itérateurs**). `Remove` est un **alias exact** d'`Erase`.

Cas d'usage, par domaine :
- **Rendu / GPU** — cache de ressources `nom → Texture*/Shader*/Pipeline*`, table de *bind groups*,
  mémoïsation d'état de pipeline (hacher une clé d'état pour réutiliser un PSO).
- **ECS** — annuaire `EntityId → index` ou `nom de composant → type`, mapping `archétype → colonnes`.
- **Audio** — banque de sons `nom → Sample*`, voix actives indexées par `id de source`.
- **UI / 2D** — atlas `glyphe → quad`, table `nom de widget → état`, cache de mise en page.
- **IO** — table de symboles d'un parseur, déduplication de chaînes (*interning*), index de fichiers.
- **Gameplay / IA** — table de *blackboard* `clé → valeur`, mémoïsation de calculs coûteux.

### `NkUnorderedMap` et `NkUnorderedSet` à fond

Même moteur de hachage, mais le **hasher par défaut FNV-1a sur octets bruts** change tout : il
hache `sizeof(Key)` octets de la clé telle quelle. Pour un `int`, un `enum`, une petite struct
*POD*, c'est rapide et juste. Pour tout type contenant un **pointeur interne** (à commencer par
`NkString`), deux clés logiquement égales auront des octets différents → collisions ratées et
recherches fausses : **fournissez alors un `Hasher` custom** (ou utilisez `NkHashMap`). `NkUnorderedMap`
remplace l'API riche par `ForEach(fn)` (`fn(const Key&, Value&)`), idéal pour appliquer une fonction
à toutes les paires sans manipuler d'itérateurs. `NkUnorderedSet::Insert` renvoie le **booléen de
nouveauté**, ce qui en fait l'outil idéal pour dédupliquer un flux (« est-ce la première fois que je
vois cet élément ? »). Souvenez-vous qu'il **n'a pas d'itérateurs** : pour énumérer un ensemble, il
faut un `NkSet`.

- **Rendu** — set de matériaux/textures uniques à charger (déduplication d'un graphe de scène).
- **ECS / physique** — set de paires de collision déjà traitées cette frame, set d'entités « vues ».
- **Gameplay / IA** — `ForEach` sur une table `entité → score` pour appliquer un effet global ;
  set des cases déjà visitées d'un pathfinding (clés plates `int` → hash brut parfait).
- **IO** — set de chemins déjà scannés, de hash de fichiers déjà importés.

### `NkMap` et `NkSet` à fond

Le choix de l'arbre Rouge-Noir se justifie par **une seule chose que le hachage ne sait pas faire :
l'ordre**. Le parcours in-order rend les clés triées, ce qui ouvre des usages impossibles avec une
table : itérer dans l'ordre, prendre le plus petit/grand, faire des requêtes de plage. `NkMap::Find`
renvoie un `Value*` ; `NkSet::Find` renvoie un **itérateur** (asymétrie à mémoriser). `NkMap` offre
en plus `At`/`TryGet`/`InsertOrAssign`/`operator[]` ; `NkSet::Insert` renvoie le booléen de nouveauté.
Le **comparateur** (`NkMapLess`/`NkSetLess`, `operator<`) se remplace par un foncteur inversé pour un
tri décroissant. **Réserve majeure** : `Erase` est en `O(n log n)` (recopie naïve, RB-Delete à venir)
— évitez les patterns à suppressions massives en boucle chaude.

- **Rendu** — *render queue* triée par profondeur/matériau (clé = clé de tri composite), table
  d'événements GPU ordonnés par timestamp.
- **Animation** — table de *keyframes* `temps → pose` parcourue dans l'ordre pour interpoler ;
  `FindIterator` pour localiser l'intervalle encadrant le temps courant.
- **Audio** — séquenceur `tick → note/événement`, parcouru chronologiquement.
- **Physique** — *sweep and prune* : ensemble d'intervalles triés sur un axe.
- **Gameplay / IA** — classement (*leaderboard*) `score → joueur`, ensemble ordonné d'objectifs.
- **UI / IO** — répertoire trié `nom → entrée`, table de configuration parcourue alphabétiquement.

### `NkBinaryTree` à fond

Le seul des associatifs dont la struct `Node` est **publique** et qui expose la racine : il est fait
pour être **inspecté et parcouru**, pas pour la performance garantie (il n'est pas auto-équilibré, et
une insertion de clés déjà triées le dégénère en liste `O(n)`). Ses quatre parcours par **visiteur**
en font sa valeur : `InOrder` (tri croissant), `PreOrder` (racine d'abord — sérialiser/cloner un
arbre), `PostOrder` (enfants d'abord — libérer/évaluer un arbre d'expression), `LevelOrder` (BFS,
niveau par niveau, via une file `NkVector<Node*>`). Avec `Height`, `IsBalanced`, `Min`, `Max`, c'est
un bon support de visualisation et de traitement d'arbre.

- **Rendu / spatial** — support de démonstration d'un BSP/quadtree, parcours niveau-par-niveau d'une
  hiérarchie de bornes (`LevelOrder` pour un *frustum culling* pédagogique).
- **Gameplay** — arbre de décision/expression évalué en `PostOrder` ; arbre de dialogue parcouru en
  `PreOrder`.
- **IO** — analyse d'un AST simple (post-ordre = évaluation, pré-ordre = émission).

### `NkBTree` à fond

L'arbre B se distingue en **regroupant beaucoup de clés par nœud** (jusqu'à `2*order-1`), ce qui
réduit la hauteur de l'arbre et donc le nombre de nœuds visités par recherche — exactement ce qu'on
veut quand chaque nœud coûte cher à atteindre (page disque, bloc de cache froid). `Insert` et
`Search` sont en `O(log_m n)`, il accepte les **doublons**, et le type `T` doit fournir `operator<`,
`operator==` et copie/move. **Limite forte** : la **suppression n'est pas implémentée**, et il n'y a
pas d'itérateurs.

- **IO / streaming** — index sur disque d'un fichier d'assets, table de localisation d'un pack
  binaire, B-Tree de *resource handles* triés.
- **Gros volumes** — index `clé → offset` d'une base de données embarquée, dictionnaire massif où la
  faible hauteur prime sur le surcoût par nœud.

### `NkTrie` à fond

Le trie ne hache ni ne compare des clés entières : il descend **lettre par lettre**. Son atout est
que `Insert`/`Search`/`StartsWith`/`FindWordsWithPrefix` coûtent `O(L)` (longueur de la chaîne),
**indépendamment du nombre de mots** stockés — un dictionnaire d'un million d'entrées teste un mot
aussi vite qu'un dictionnaire de dix. L'alphabet est **fixe `a-z` (26), insensible à la casse** ;
`Search` exige un nœud marqué fin-de-mot, là où `StartsWith` se contente d'un préfixe partiel.

**Piège à connaître absolument** : `FindWordsWithPrefix` renvoie un `NkVector<const char*>` dont les
pointeurs visent un **buffer temporaire interne** — ils deviennent **invalides après le retour** de
la fonction. En production, recopiez immédiatement chaque résultat dans un `NkString`.

- **UI / texte** — **autocomplétion** d'un champ de recherche, d'une console de commandes (« tape
  `te` → propose `texture`, `terrain`… »), filtrage en direct d'une liste.
- **Gameplay** — reconnaissance de **combos**/commandes saisies au clavier, validation d'un mot dans
  un jeu de lettres, filtre de noms interdits.
- **IO** — table de commandes d'un interpréteur, routage par préfixe d'identifiants.

### `NkPriorityQueue` à fond

Le tas binaire ne maintient qu'**un seul invariant** — la racine est l'extrême — ce qui suffit pour
servir « le plus prioritaire d'abord » sans jamais trier l'ensemble. `Top` est `O(1)`, `Push`/`Pop`
remontent/redescendent un seul chemin de la hauteur de l'arbre (`O(log n)`). `Emplace` construit
l'élément en place. Par défaut (`NkPriorityLess`, `operator<`) c'est un **max-heap** ; pour un
**min-heap** (le cas le plus fréquent en pathfinding), on fournit un comparateur inversé. La free
function `NkSwap` échange deux files en `O(1)`. Pas d'itérateurs : seul le sommet est ordonné.

- **IA / pathfinding** — la **frontière (open set) d'un A\*** ou d'un Dijkstra (min-heap sur le coût
  `f = g + h`) : l'opération dominante de tout calcul de chemin.
- **Gameplay** — **scheduler** d'événements temporels (min-heap sur l'instant de déclenchement),
  file de tâches par priorité.
- **Rendu** — sélection des `k` objets les plus proches/les plus visibles, ordre de chargement de
  *streaming* (mip-maps les plus utiles d'abord), tri partiel d'une file de transparence.
- **Audio** — *voice stealing* : quand toutes les voix sont prises, libérer la moins prioritaire
  (tas sur la priorité de mixage).

### Le socle commun

- **Allocateur conscient.** Tous prennent un `Allocator*` (par défaut `memory::NkGetDefaultAllocator()`,
  type `memory::NkAllocator`), passé **par pointeur** et **non possédé** : nœuds et tas viennent de
  NKMemory, jamais de `new`. Voir [NKMemory](../NKMemory.md).
- **Double casse d'API — couverture inégale.** `NkHashMap`/`NkUnorderedMap` exposent `Begin/begin/CBegin`
  ; `NkMap` a `Begin`/`begin` (sans `cbegin`) ; `NkSet` a **`begin`/`end` seulement** ; `NkUnorderedSet`,
  `NkBTree`, `NkTrie`, `NkPriorityQueue` **n'ont pas d'itérateurs**. Vérifiez la table d'API avant un
  `range-based for`.
- **Sémantique de déplacement.** La plupart gardent move/copie via `#if defined(NK_CPP11)` (sauf
  `NkHashMap` qui les déclare sans garde, et `NkBTree`/`NkTrie`/`NkPriorityQueue` qui n'ont pas de
  copie/move déclarés).
- **Foncteurs personnalisables.** Hasher/comparateur se passent en paramètres template :
  `NkHash<MyType>` pour `NkHashMap`, un `Hasher` custom pour les conteneurs FNV-1a, `NkMapLess`/
  `NkSetLess`/`NkPriorityLess` inversés pour un ordre/un tas décroissant.
- **`operator[]` insère.** Sur `NkHashMap`/`NkUnorderedMap`/`NkMap`, indexer une clé absente crée
  `Value()` (donc `Value` *default-constructible*) **et invalide les itérateurs** — utilisez `Find`
  si vous voulez seulement lire.

---

### Exemple

```cpp
#include "NKContainers/NKContainers.h"
using namespace nkentseu;

// HashMap : cache de ressources nom -> pointeur, recherche O(1).
NkHashMap<NkString, Texture*> cache;
cache.Insert("hero", heroTex);
if (Texture** t = cache.Find("hero")) Use(*t);   // O(1) amorti

// Set ordonné : identifiants triés, parcours croissant garanti.
NkSet<int> ids;
ids.Insert(42); ids.Insert(7);
for (int id : ids) { /* 7, puis 42 */ }

// Trie : autocomplétion d'une console de commandes.
NkTrie<> commands;
commands.Insert("texture");
commands.Insert("terrain");
bool isPrefix = commands.StartsWith("te");        // true

// PriorityQueue : frontière d'un A* (min-heap via comparateur inversé).
NkPriorityQueue<Node, memory::NkAllocator, CostGreater> openSet;
openSet.Push(start);
while (!openSet.Empty()) {
    Node best = openSet.Top();   // le plus petit coût d'abord
    openSet.Pop();
    // ... expansion des voisins ...
}
```

---

[← Index NKContainers](README.md) · [Récap NKContainers](../NKContainers.md) · [Adapters →](Adapters.md)
