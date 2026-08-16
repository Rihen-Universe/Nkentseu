# NKContainers — Roadmap

État actuel (mai 2026) : large catalogue de conteneurs zero-STL livré et
utilisé en production (NkVector / NkHashMap / NkString partout dans NKRenderer
et services). Catégories couvertes : séquentiels, associatifs, adaptateurs,
fonctionnels, hétérogènes, vues, spécialisés (graph / octree / quadtree),
strings (avec encodings UTF-8/16/32/ASCII/Base64). Tests existants restreints
à un sous-ensemble : à étendre.

---

## 📊 Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Séquentiels (`NkVector`, `NkList`, `NkDoubleList`, `NkDeque`) | ✅ Livré | — | — |
| Cache-friendly (`NkArray`, `NkPool`, `NkRingBuffer`) | ✅ Livré | — | — |
| Associatifs ordonnés (`NkMap`, `NkSet`, `NkBinaryTree`, `NkBTree`, `NkPriorityQueue`, `NkTrie`) | ✅ Livré | — | — |
| Associatifs non-ordonnés (`NkHashMap`, `NkUnorderedMap`, `NkUnorderedSet`) | ✅ Livré | — | — |
| Adaptateurs (`NkStack`, `NkQueue`) | ✅ Livré | — | — |
| Hétérogènes (`NkPair`, `NkTuple`) | ✅ Livré | — | — |
| Fonctionnels (`NkFunction`, `NkBind`, `NkFunctional`) | ✅ Livré | — | — |
| Itérateurs (`NkIterator`, `NkInitializerList`) | ✅ Livré | — | — |
| Spécialisés (`NkGraph`, `NkOctree`, `NkQuadTree`) | ✅ Livré | — | — |
| Strings (`NkString`, `NkStringView`, `NkStringBuilder`, `NkFormat`) | ✅ Livré | — | — |
| Encodings (UTF-8, UTF-16, UTF-32, ASCII, Base64) | ✅ Livré | — | — |
| Utilities (`NkOptional`, `NkResult`, `NkVariant`) | 🔶 Partiel (forwarding) | S | Moyenne |
| Vues (`NkSpan`) | ✅ Livré | — | — |
| Tests étendus pour tous les conteneurs | 🔶 Partiel (9/40+) | L | Haute |
| Header umbrella complet | 🔶 Partiel (`NKContainers.h` minimal) | S | Moyenne |
| Wide string (`NkWString`) | 🔶 Partiel (header seulement) | M | Basse |
| Sémantique de déplacement (`Insert(T&&)` / `Emplace`) | 🔶 Partiel (séquentiels + `NkHashMap` ✅ ; associatifs restants ❌) | M | **Haute** |
| Paramètre template `Allocator` réellement générique | ❌ décoratif sur 18 conteneurs | M | Moyenne |
| Macros de capacité `NK_CPP11` / `NKENTSEU_CXX11_OR_LATER` | ❌ jamais définies (47 directives mortes) | M | Moyenne |

👉 **Les manques mesurés, avec leur provenance et leur méthode, sont détaillés
plus bas : [Inventaire des manques de Foundation](#-inventaire-des-manques-de-foundation-2026-08-16).**

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## ✅ Livré

### Séquentiels (`Sequential/`)
- [NkVector.h/.cpp](src/NKContainers/Sequential/NkVector.h) : tableau dynamique
  type std::vector, allocateur custom, itérateurs PascalCase + alias
  minuscules (range-based for compatible), gestion d'erreurs via
  `NkVectorError.h`. Tests : `test_vector.cpp` (PushBack, Insert, Erase,
  Resize, Front, Back, accès indexé).
- [NkList.h/.cpp](src/NKContainers/Sequential/NkList.h) : liste chaînée simple
  forward-only avec pointeur tail pour PushBack O(1) amorti, Reverse() in-place
- [NkDoubleList.h/.cpp](src/NKContainers/Sequential/NkDoubleList.h) : liste
  doublement chaînée
- [NkDeque.h/.cpp](src/NKContainers/Sequential/NkDeque.h) : double-ended queue
- `NkVectorError.h` : codes d'erreur dédiés Vector (out_of_range, etc.)

### Cache-friendly (`CacheFriendly/`)
- `NkArray` : tableau de taille fixe sur stack (équivalent `std::array`)
- `NkPool` : pool d'objets contigu pour itération cache-friendly
- `NkRingBuffer` : buffer circulaire FIFO

### Associatifs (`Associative/`)
- [NkMap.h/.cpp](src/NKContainers/Associative/NkMap.h) : map ordonnée
  (arbre rouge-noir probable). Tests : `test_map.cpp`
- [NkSet.h/.cpp](src/NKContainers/Associative/NkSet.h) : set ordonné
- [NkBinaryTree.h/.cpp](src/NKContainers/Associative/NkBinaryTree.h) : BST
  brut sans rééquilibrage
- [NkBTree.h/.cpp](src/NKContainers/Associative/NkBTree.h) : B-tree auto-équilibré
  optimisé pour stockage / cache. Tests : `test_btree.cpp`
- [NkHashMap.h/.cpp](src/NKContainers/Associative/NkHashMap.h) : table de
  hachage avec hasher par défaut FNV-1a, délègue à `NkHash<Key>` (spécialisations
  pour types POD + `NkString`). Bug majeur précédent (hash des octets bruts)
  résolu via délégation à `NkHash<T>`.
- [NkUnorderedMap.h/.cpp](src/NKContainers/Associative/NkUnorderedMap.h) +
  [NkUnorderedSet.h/.cpp](src/NKContainers/Associative/NkUnorderedSet.h) :
  variantes non-ordonnées
- [NkPriorityQueue.h/.cpp](src/NKContainers/Associative/NkPriorityQueue.h) :
  file de priorité (heap). Tests : `test_priority_queue.cpp`
- [NkTrie.h/.cpp](src/NKContainers/Associative/NkTrie.h) : arbre de préfixes
  pour recherche string

### Adaptateurs (`Adapters/`)
- `NkStack` : adaptateur LIFO header-only
- `NkQueue` : adaptateur FIFO header-only

### Hétérogènes (`Heterogeneous/`)
- [NkPair.h/.cpp](src/NKContainers/Heterogeneous/NkPair.h) : équivalent
  std::pair. Tests : `test_pair.cpp`
- [NkTuple.h/.cpp](src/NKContainers/Heterogeneous/NkTuple.h) : équivalent
  std::tuple variadique

### Fonctionnels (`Functional/`)
- [NkFunction.h/.cpp](src/NKContainers/Functional/NkFunction.h) : conteneur
  polymorphe pour callables (équivalent `std::function`), SBO supportée,
  méthodes membres const / non-const, lambdas, fonctions libres
- [NkBind.h/.cpp](src/NKContainers/Functional/NkBind.h) : équivalent `std::bind`
- [NkFunctional.h/.cpp](src/NKContainers/Functional/NkFunctional.h) :
  spécialisations `NkHash<T>` pour types POD + `NkString` (utilisé par
  NkHashMap)
- `NkFuntionV1.h` / `NkFuntionV2.h` : versions de transition (à nettoyer ?)

### Itérateurs (`Iterators/`)
- [NkIterator.h/.cpp](src/NKContainers/Iterators/NkIterator.h) : infrastructure
  commune (forward, bidirectional, random-access, const). Tests :
  `test_iterator.cpp`
- [NkInitializerList.h/.cpp](src/NKContainers/Iterators/NkInitializerList.h) :
  équivalent `std::initializer_list` zero-STL. Tests :
  `test_initializer_list.cpp`

### Spécialisés (`Specialized/`)
- [NkGraph.h/.cpp](src/NKContainers/Specialized/NkGraph.h) : graphe générique
  via liste d'adjacence, dirigé/non-dirigé, pondéré, DFS + BFS intégrés.
  Tests : `test_graph.cpp`
- [NkOctree.h](src/NKContainers/Specialized/NkOctree.h) : partitionnement 3D
  pour culling / collision / range queries (AABB et sphère)
- [NkQuadTree.h/.cpp](src/NKContainers/Specialized/NkQuadTree.h) : équivalent 2D

### Strings (`String/`)
- [NkString.h/.cpp](src/NKContainers/String/NkString.h) : string dynamique
  avec Small String Optimization (SSO) configurable via
  `NK_STRING_SSO_SIZE`, null-terminated pour interop C, conversion implicite
  vers `NkStringView`
- [NkStringView.h/.cpp](src/NKContainers/String/NkStringView.h) : vue
  non-owning équivalent `std::string_view`
- [NkBasicString.h/.cpp](src/NKContainers/String/NkBasicString.h) +
  [NkBasicStringView.h](src/NKContainers/String/NkBasicStringView.h) :
  templates de base
- [NkStringBuilder.h/.cpp](src/NKContainers/String/NkStringBuilder.h) :
  construction efficace de strings (équivalent std::stringstream)
- [NkStringUtils.h/.cpp](src/NKContainers/String/NkStringUtils.h) : helpers
  (split, trim, replace, to_int, etc.)
- [NkStringHash.h/.cpp](src/NKContainers/String/NkStringHash.h) : hash FNV-1a
  spécialisé pour NkString
- [NkFormat.h/.cpp](src/NKContainers/String/NkFormat.h) : moteur de formatage
  unifié, double syntaxe `{0:>10.2f}` accolades + `%-15s` printf, extension
  via ADL `NkToString(...)` ou spécialisation `NkFormatter<T>` ou macro
  `NK_FORMATTER`. Version 4.0.0 (2026).

### Encodings (`String/Encoding/`)
- `NkASCII.h/.cpp` : table ASCII et helpers
- `NkUTF8.h/.cpp` : encode/decode UTF-8 + validation
- `NkUTF16.h/.cpp` : encode/decode UTF-16 (BE + LE)
- `NkUTF32.h/.cpp` : encode/decode UTF-32
- `NkBase64.h` : encode/decode Base64
- `NkEncoding.h/.cpp` : façade unifiée des encodings

### Utilities (`Utilities/`)
- [NkOptional.h/.cpp](src/NKContainers/Utilities/NkOptional.h) : forwarding
  vers `NKCore/NkOptional.h` avec alias dans le namespace containers
- [NkResult.h/.cpp](src/NKContainers/Utilities/NkResult.h) : Result<T, E>
  Rust-style, forwarding vers implementation Core (mode dual : NKCore
  canonique + standalone fallback)
- [NkVariant.h/.cpp](src/NKContainers/Utilities/NkVariant.h) : forwarding
  variant

### Vues (`Views/`)
- [NkSpan.h/.cpp](src/NKContainers/Views/NkSpan.h) : vue non-owning sur
  tableau contigu. Tests : `test_span.cpp`

### Tests (9 fichiers)
`tests/` : test_btree, test_graph, test_initializer_list, test_iterator,
test_map, test_pair, test_priority_queue, test_span, test_vector

---

## 🔄 En cours / TODO immédiat

### Couverture des tests
9 fichiers de tests pour ~40 conteneurs distincts. Conteneurs **sans tests
dédiés** dans `tests/` :
- `NkList`, `NkDoubleList`, `NkDeque`, `NkArray`, `NkPool`, `NkRingBuffer`
- `NkStack`, `NkQueue` (adaptateurs)
- `NkSet`, `NkBinaryTree`, `NkTrie`
- `NkHashMap`, `NkUnorderedMap`, `NkUnorderedSet` (test_map couvre `NkMap`
  ordonné uniquement)
- `NkTuple`
- `NkFunction`, `NkBind` (critiques — utilisés massivement)
- `NkOctree`, `NkQuadTree`
- `NkString`, `NkStringView`, `NkStringBuilder`, `NkFormat`
- Tous les encodings UTF-8/16/32 + Base64
- `NkOptional`, `NkResult`, `NkVariant` (au moins un test de forwarding)

Effort estimé : L (un fichier de test minimal par conteneur).

### Header umbrella `NKContainers.h`
- Actuellement n'inclut que `NkPair`, `NkIterator`, `NkInitializerList`,
  `NkVectorError`, `NkVector`, `NkMap`, `NkBTree`. Devrait au minimum inclure
  : `NkList`, `NkHashMap`, `NkString`, `NkFormat`, `NkFunction`, `NkSpan`,
  `NkOptional`, `NkResult`, `NkVariant`, `NkTuple`. Effort : S.

### Nettoyage versions Function
- `NkFunction.h` + `NkFuntionV1.h` + `NkFuntionV2.h` coexistent dans
  `Functional/`. Identifier la version officielle et supprimer ou archiver
  les autres. Effort : S.

### `NkWString` (wide string)
- Header seul (`String/NkWString.h`) sans `.cpp`. À auditer : header-only
  intentionnel ou implémentation manquante ? Effort : M si à implémenter.

---

## ❌ À venir / À ajouter (futur proche)

### Conteneurs manquants par rapport à la STL / EASTL
- `NkFlatMap` / `NkFlatSet` (sorted vector backend) — plus cache-friendly
  que NkMap pour petites tailles, équivalent `boost::flat_map`
- `NkSlotMap` / `NkColonyMap` — référence stable + itération rapide (utilisé
  par les ECS modernes ; pertinent pour NKScene)
- `NkSmallVector<T, N>` — vector avec N éléments inline avant allocation
  heap (équivalent `llvm::SmallVector`). Économise alloc pour petites
  tailles.
- `NkInlineHashMap` — équivalent pour HashMap

### Spécialisés pour le moteur
- `NkKDTree` (k-dimensional tree) — culling fréquent en physique / animation
- `NkBVH` (Bounding Volume Hierarchy) — accélération raycast renderer + physics
- `NkConcurrentQueue` (MPMC lock-free) — job system NKThreading à venir
- `NkLRUCache<K, V>` — pattern documenté dans le Readme mais pas livré en
  tant que conteneur officiel

### Format / strings avancés
- `NkRegex` — moteur regex zero-STL (probablement gros effort, priorité basse)
- `NkPath` (chemin de fichier portable) — manipulé dans les exemples
  NKPlatform mais pas dispo en tant que conteneur. Doublon possible avec
  futur NKStream.
- Formatage `chrono` types (durations, dates) — à wirer une fois NKTime créé.

### `NkAny` ou `NkUntypedStorage`
- Équivalent `std::any`. Utile pour ECS / scripting bindings (NKScript).
  Effort : M.

### Persistance
- Pas d'API standard de sérialisation binaire / JSON sur les conteneurs.
  Aujourd'hui NkRenderer parse du JSON ad-hoc pour `.nkasset`. Une trait
  `NkSerializable<T>` côté containers + intégration NKStream serait nette.

---

## Bugs / quirks connus

- `NkHashMap` hasher par défaut a corrigé un bug majeur (hash des octets
  bruts → collisions silencieuses pour types non-POD avec pointeurs). Le
  fallback générique de `NkHash<T>` déclenche un `static_assert` clair, mais
  les utilisateurs doivent maintenant fournir leur spécialisation pour les
  types custom. Documenter clairement dans le Readme.
- Trois versions de `NkFunction` cohabitent (`NkFunction.h`,
  `NkFuntionV1.h`, `NkFuntionV2.h`) — risque de confusion.
- `NkFuntion` (sans "c") : faute de frappe persistante dans les noms de
  fichiers V1/V2 — à corriger ou aliaser.
- `NKContainers.h` umbrella header est minimal — les utilisateurs doivent
  inclure individuellement les headers qu'ils veulent, ce qui contredit le
  modèle "umbrella" annoncé pour NKCore et NKMath.
- `NkString` documenté avec emojis dans les commentaires (`🔹 Small String
  Optimization`), à harmoniser avec le reste du code.
- **`NkString::begin()` / `end()` non-const étaient déclarés sans corps**
  (`NkString.h:977` et `:991`) : `for (char &c : s)` compilait et échouait à
  l'édition de liens, sans erreur ni avertissement. **Corrigé le 2026-08-16** —
  voir §3 de l'inventaire pour la mesure et pour la raison qui l'a rendu
  invisible si longtemps.
- **Le paramètre template `Allocator` de 18 conteneurs est décoratif** : les
  constructeurs codent `&memory::NkGetDefaultAllocator()` en dur, donc tout autre
  allocateur est refusé à la compilation. Voir §4.

---

## 📊 Inventaire des manques de Foundation (2026-08-16)

> **Provenance** — worktree `Nkentseu-nkanim`, branche `feat/nkanimation`,
> commit `4e7b1615`, le 2026-08-16. Toolchain g++ 16.1.0, `-std=c++20`.
> Configurations : les conclusions de **syntaxe** ne dépendent d'aucun
> comportement d'exécution ; les conclusions d'**exécution** ont été prises dans
> **Debug et Release** et sont marquées comme telles.
>
> Cet inventaire vit ici, dans un document suivi par git, et **non dans le canal
> d'échanges** (`echanges/` est gitignoré : c'est une conversation, pas une
> archive). La conversation qui l'a produit peut disparaître ; ceci doit rester.

### 1. Sémantique de déplacement : état par conteneur

Mesuré par une unité de traduction jetable, un type strictement move-only, un
conteneur par fichier, en `-fsyntax-only`.

| Conteneur | Type de valeur move-only | Détail |
|---|---|---|
| `NkVector`, `NkList`, `NkDoubleList` | ✅ accepté | `PushBack(T&&)` + `EmplaceBack` présents hors garde |
| **`NkHashMap`** | ✅ **complété le 2026-08-16** | `Insert(Key, Value&&)` + `Emplace` — voir §2 |
| **`NkMap`, `NkUnorderedMap`** | ✅ **complétés le 2026-08-16** | idem, même forme |
| **`NkSet`, `NkUnorderedSet`** | ✅ **complétés le 2026-08-16** | `Insert(T&&)` + `Emplace(Args&&...)` |
| **`NkDeque`** | ✅ **complété le 2026-08-16** | `PushBack/PushFront(T&&)` + `EmplaceBack/EmplaceFront` |
| `NkStack` | ❌ refusé | le code **existe** mais est compilé hors du binaire par `#if defined(NK_CPP11)` (l. 256) — voir §5 |
| `NkQueue`, `NkRingBuffer`, `NkPriorityQueue` | ❌ refusé | même garde (l. 281 / 511 / 339) **plus** un second obstacle derrière |

**La famille associative et le deque sont finis.** Restent les **adaptateurs**
(`NkStack`, `NkQueue`, `NkRingBuffer`, `NkPriorityQueue`), qui relèvent d'un autre
diagnostic : leur code de déplacement est écrit mais compilé hors du binaire par
`NK_CPP11`, et trois d'entre eux portent un second obstacle derrière. Ce n'est pas
la même réparation — voir §5.

#### Ce que les sets ont de particulier, et qui se documente

Dans une map, la clé est connue **avant** la valeur : `Emplace` peut chercher, puis
ne construire que si la clé est absente. **Dans un ensemble, la clé EST la valeur**
— il faut donc construire l'élément pour pouvoir le comparer (et le hacher). En cas
de doublon, le nœud est construit puis détruit. C'est le même compromis que
`std::set::emplace` / `std::unordered_set::emplace`, et il est écrit dans le
doxygen des deux méthodes **pour que personne ne le prenne pour un oubli**.

Corollaire mesuré : `Insert(T&&)` sur un doublon **ne consomme pas** la source —
rien n'est déplacé quand rien n'est inséré.

⚠️ **Ce n'est pas un manque théorique.** `NKAnimation/NkAnimation.h` déclare
**4 champs `NkHashMap`** (l. 277, 278, 684, 685) : tant que le conteneur refusait
les types non copiables, `NkAnimationTrack<T>` ne pouvait pas devenir un type
valeur à ressource possédée. Et `NKRenderer/Streaming/NkStreamingSystem` a
contourné le même manque par un **pool de slots** maison.

### 2. `NkHashMap` — complété le 2026-08-16 (ajouts purs)

Trois ajouts, **aucune signature existante modifiée** :

| Fichier | Ajout |
|---|---|
| `Heterogeneous/NkPair.h` | `NkPair(NkPiecewiseTag, const T1 &, Args &&...)` |
| `Associative/NkHashMap.h` | `Node(usize, const Key &, Node *, NkPiecewiseTag, Args &&...)` |
| `Associative/NkHashMap.h` | `Insert(const Key &, Value &&)` et `Emplace(const Key &, Args &&...)` |

Commit `54e57ea6`. Témoins : une TU move-only qui **échoue avant**
(`NkHashMap.h:1084`, *overload resolution selected deleted operator '='*) et
compile après, sur **g++ 16.1.0 et clang 22.1.4** ; puis **25 assertions
d'exécution en Debug et en Release** — zéro copie sur rvalue, source vidée,
`Emplace` sans copie ni move, et la contre-épreuve qu'une lvalue **copie
toujours** et laisse sa source intacte.

⭐ **Contre-épreuve qui compte plus que la TU** : le **pool de slots** de
`NKRenderer/Streaming/NkStreamingSystem` **pourrait être retiré**. Son propre
commentaire (`NkStreamEntry`, l. 102-109) nommait déjà sa cause :

> « `NkHashMap::Insert` prend son argument par `const Value &` et le **COPIE**
> dans le nœud ; or `NkImage` est non copiable depuis la migration valeur. Un
> `NkImage lowPayload;` ici rendrait `NkStreamEntry` non copiable et
> `RegisterTexture`/`RegisterMesh` ne compileraient plus. »

Une TU reproduisant l'entry avec la charge **par valeur** et **sans pool**
échoue avant et compile après. Le contournement n'est pas retiré ici — ce n'est
pas ce module — mais **il n'a plus de raison d'être**.

**Pourquoi il fallait toucher `NkPair` et pas seulement `NkHashMap`** : le nœud
stocke une `NkPair<const Key, Value>`, et **tous** les constructeurs par
déplacement / forwarding de `NkPair` sont derrière `#if defined(NK_CPP11)` —
macro jamais définie (§5). La seule voie compilée copiait. Ajouter la surcharge
sur `Insert` sans donner à `NkPair` un moyen de construire sa valeur sur place
n'aurait rien débloqué.

**Pourquoi un tag plutôt qu'un constructeur variadique nu** : un variadique nu
serait glouton et capterait des appels qui résolvent aujourd'hui vers
`NkPair(const T1 &, const T2 &)` ou vers le constructeur de copie. Avec le tag,
**aucun appel écrit avant ce jour ne peut le sélectionner** : l'ajout est inerte
pour les 38 fichiers qui consomment `NkHashMap` et les 6 qui consomment `NkPair`.

**Sémantique retenue, et elle se documente** :
- `Insert(const Key &, Value &&)` **écrase** si la clé existe (déplacement-assignation), comme la version par copie ;
- `Emplace(const Key &, Args &&...)` **n'écrase jamais** et retourne `bool`. Ce choix évite de construire quoi que ce soit lorsque la clé est déjà là, et n'exige donc pas que `Value` soit assignable ;
- la clé reste prise par `const Key &`, comme partout dans ce dépôt. Élargir plus tard au forwarding de la clé restera rétro-compatible ; l'inverse ne le serait pas.

✅ **Fait le 2026-08-16, même forme** : `NkMap`, `NkUnorderedMap`, `NkSet`,
`NkUnorderedSet`, `NkDeque`.

⚠️ **Le tag a changé de nom ET de place, et la raison se garde.** Il s'appelait
`NkPairPiecewiseTag` et vivait dans `NkPair.h`. Il s'appelle désormais
**`NkPiecewiseTag`** et vit dans **`NkContainersApi.h`**.

> **Le prémisse « le tag est déjà en place, il leur servira » était FAUX pour les
> deux ensembles**, et il a coûté une compilation cassée avant d'être vu.
> `NkSet` et `NkUnorderedSet` stockent **`T` directement** — pas de `NkPair`,
> donc **ils n'incluent pas `NkPair.h`**, donc le tag n'existait pas dans leur
> portée. Ils n'ont pas non plus de garde `NK_CPP11` : un seul constructeur, qui
> copie. **Les ensembles ne sont pas des maps sans valeur** ; les traiter comme
> tels est l'erreur à ne pas refaire.

Le défaut était d'une espèce particulièrement discrète : `NkPiecewiseTag` est un
nom **non dépendant**, donc diagnostiqué à la **définition** du template —
**aucune instanciation n'était nécessaire**, un simple `#include` suffisait à
casser. Mesuré sur les 3 consommateurs réels (`Specialized/NkGraph.h`,
`NKEvent/NkDropSystem.cpp`, `NKRHI/Vulkan/NkVulkanDevice.cpp`), sur g++ **et**
clang. `NKContainers.h` n'incluant pas les ensembles, la portée s'arrêtait là.

⚠️ **Et il avait échappé au témoin de syntaxe** : celui-ci incluait les maps
**avant** les ensembles, et les maps tirent `NkPair.h` — **l'ordre d'inclusion
masquait le manque.** D'où la règle qui en sort : *un en-tête doit être compilé
**seul**, pas en compagnie de ceux qui satisfont ses dépendances à sa place.* Un
balayage « chaque en-tête compile seul » couvre désormais les 53 de
`NKContainers` (50/53 ; les 3 échecs — `NkBind.h`, `NkFuntionV2.h`,
`NkBasicStringView.h` — sont **antérieurs** et hors de ce chantier).

### 3. Famille « déclarée sans corps » — `NkString::begin()` / `end()`

`String/NkString.h:977` et `:991` déclarent les surcharges **non-const** de
`begin()` / `end()` ; `NkString.cpp` ne définit **que** les versions const.

Mesure qui ne dépend d'aucune interprétation — symboles de `NKContainers.lib` :

```
definis  :  nkentseu::NkString::begin() const
            nkentseu::NkString::end()   const
manquants:  nkentseu::NkString::begin()
            nkentseu::NkString::end()
```

**Conséquence** : `for (char &c : s)` **compile sans un mot** et échoue à
l'édition de liens. Ni erreur, ni avertissement — le compilateur voit une
déclaration et la croit.

⚠️ **Et ce n'était pas un piège théorique : il avait déjà une victime.**
`Applications/Sandbox/src/DemoNkentseu/Base01/main8.cpp:193` (`LowerAscii`)
écrit exactement cette boucle, et faisait **échouer la cible `Gamepad`** du
build complet. Mesure, configuration Debug, clang 22.1.4 :

| Build complet (203 cibles, `--keep-going`) | Construites | Échecs |
|---|---|---|
| **avant** (`4e7b1615`) | 201/203 | 2 — `Gamepad`, `NKTensorDemo` |
| **après** (`5c89c5fe`) | **202/203** | 1 — `NKTensorDemo` seul |

`NKTensorDemo` échoue sur `NkLog::Instance()`, défaut sans rapport et antérieur.
**Zéro régression sur 203 cibles.**

⚠️ **Leçon de méthode, et elle borne l'inventaire lui-même** : *aucune recherche
par nom ne peut trouver ce défaut.* Un balayage qui compare les noms déclarés
dans le `.h` aux noms définis dans le `.cpp` voit `NkString::begin` défini et
passe son chemin — c'est **la surcharge** qui manque, pas le nom. Un balayage de
ce type a été écrit et lancé sur Foundation : il n'a pas trouvé ce défaut, alors
qu'il le cherchait. **Seule l'édition de liens décide.** Un inventaire complet de
cette famille demanderait une unité de traduction qui odr-utilise chaque
surcharge déclarée — chantier non ouvert, nommé ici.

### 4. Le paramètre template `Allocator` est décoratif — mais l'injection FONCTIONNE

> ⚠️ **CORRECTION du 2026-08-16, après mesure complémentaire.** La première
> rédaction disait « le paramètre `Allocator` est décoratif » **sans distinguer
> deux choses différentes**, et c'était trop large. Ce qui suit remplace cette
> formulation. *Un fait qui voyage n'est pas un fait vérifié* — y compris quand
> c'est moi qui l'ai écrit une heure plus tôt.

#### Ce qui MARCHE : l'injection d'allocateur par pointeur

`memory::NkAllocator` est une **base polymorphe** (`Allocate`/`Deallocate`
virtuels purs) avec 11+ sous-classes concrètes (`NkLinear`, `NkArena`, `NkStack`,
`NkPool`, `NkFreeList`, `NkBuddy`…). Passer un allocateur **dérivé** au
constructeur fonctionne, par dispatch virtuel. Mesuré avec un allocateur
compteur injecté dans un `NkHashMap` :

```
pendant           : allocs=54  frees=3   size=50
apres destruction : allocs=54  frees=54   -> L'INJECTION SERT REELLEMENT
```

**Rien n'est silencieusement ignoré.** Ce n'est donc **pas un défaut silencieux**,
et la dette ne change pas de priorité : elle reste une question de conception.

#### Ce qui NE MARCHE PAS : le paramètre TEMPLATE

**18 conteneurs** exposent un paramètre `typename Allocator` et codent malgré
tout `&memory::NkGetDefaultAllocator()` en dur dans leurs constructeurs :

```cpp
mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
```

Le type de cette expression est `memory::NkAllocator *`. Substituer un **autre
type** au paramètre template ne compile pas. Vérifié sur **5 des 18** —
`NkHashMap`, `NkVector`, `NkMap`, `NkSet`, `NkList` : **5 refus sur 5**. Les 13
autres portent le même motif sans avoir été instanciés ; le chiffre à retenir est
*18 exposent, 5 mesurés, 5 refusent*.

Le paramètre template est donc **redondant** avec un mécanisme qui, lui, marche.

#### ⚠️ Et la documentation prescrit précisément la forme cassée

Zéro appelant réel passe un allocateur non-défaut. **Mais 7 conteneurs portent un
bloc `@code` qui enseigne l'idiome qui ne compile pas** — `NkHashMap` (l. 1485),
`NkMap`, `NkSet`, `NkTrie`, `NkBTree`, `NkBinaryTree`, `NkUnorderedMap` :

```cpp
nkentseu::NkHashMap<usize, nkentseu::NkString, memory::NkPoolAllocator> cache(&pool);
```

Recopié mot pour mot, cet exemple donne **deux** erreurs :

```
error: no matching constructor for initialization of 'memory::NkPoolAllocator'
NkHashMap.h:638: error: cannot initialize a member subobject of type
                 'memory::NkPoolAllocator *' with an rvalue of type 'NkAllocator *'
```

**Même famille que les six commentaires `Free()` de `NkImage.h` et que le
`Insert(i, NkMove(values))` de ce fichier** : *un commentaire qui prescrit une
API qui n'existe pas.* C'est ce qui fera perdre une demi-journée au prochain qui
voudra un allocateur custom — pas le paramètre template en soi.

**Trois issues acceptables, l'ambiguïté ne l'est pas** : rendre le paramètre
réellement générique, le retirer, ou le garder et **réécrire les 7 blocs `@code`**
pour enseigner l'injection par pointeur, qui marche. Décision de niveau Rodolf :
ça touche 18 conteneurs partagés.

### 5. Macros de capacité jamais définies

Une macro **d'option** choisit entre deux comportements valides ; une macro de
**capacité** en désactive un qui devrait être actif. Seules les secondes sont des
défauts. Sur ~128 macros non définies balayées dans `Kernel/`, `Engine/`,
`Applications/`, `Integrations/`, ~120 sont des interrupteurs sains
(`*_BUILD_SHARED_LIB`, `*_HEADER_ONLY`, `NKSL_HAS_*`…). Restent :

| Macro | Directives `#if` / fichiers | État |
|---|---|---|
| `NK_CPP11` | 34 / 16 (NKContainers seul) | jamais définie |
| `NKENTSEU_CXX11_OR_LATER` | 13 / 5 | jamais définie — **c'est la cible de migration annoncée par `NkCompat.h:399`** |
| `NK_PLATFORM_WINDOWS` | 2 | traité par un autre chantier (`NkBasicString.cpp`) |

⚠️ **`NkCompat.h:399` porte une table de migration `NK_CPP11 → NKENTSEU_CXX11_OR_LATER`
dont la cible n'a jamais été écrite.** `NkPlatformDetect.h` ne contient aucune
occurrence de `__cplusplus` ni d'une macro de standard, alors que `NkCompat.h:28`
annonce en commentaire l'importer pour « `NKENTSEU_CXX_*` ».

> ⭐ **CORRECTION DU 2026-08-16 — la détection de standard EXISTE, et le §5
> concluait à son absence pour avoir cherché dans le mauvais fichier.**
>
> Ce paragraphe disait vrai de `NkPlatformDetect.h` (**0** occurrence de
> `__cplusplus`) et en tirait que la capacité n'existait pas. **Elle existe, dans
> le fichier voisin** — `NKPlatform/NkCompilerDetect.h`, **15** occurrences de
> `__cplusplus`, chaîne complète l. 386-415 :
>
> | macro | définie sur ce dépôt (C++17) ? | sémantique réelle |
> |---|---|---|
> | `NK_CPP11` | ❌ jamais, nulle part | **n'existe dans aucun en-tête de détection** |
> | `NKENTSEU_CXX11_OR_LATER` | ❌ jamais | cible de migration annoncée, jamais écrite |
> | `NKENTSEU_CPP11` | ❌ **non** | branche `#elif` — vaut « **exactement** C++11 » |
> | **`NKENTSEU_HAS_CPP11`** | ✅ **OUI** | `NkCompilerDetect.h:439`, sous `CPP11‖14‖17‖20‖23` — c'est « **C++11 ou plus** » |
>
> **Mesure directe** (TU compilée `-std=c++17`, `__cplusplus = 201703`) :
> `NKENTSEU_CPP11` non définie · `NKENTSEU_CPP17` **définie** ·
> `NKENTSEU_HAS_CPP11` **définie** · `NK_CPP11` non définie.
>
> ⚠️ **Le piège à ne pas répéter** : substituer `NKENTSEU_CPP11` à `NK_CPP11`
> **n'ouvrirait rien** — la chaîne étant en `#elif`, elle est fausse dès C++14.
> La seule macro correcte est **`NKENTSEU_HAS_CPP11`**.
>
> **Et la cause première du silence est un include :** `NkCompat.h:28` importe
> `NkPlatformDetect.h` « pour `NKENTSEU_CXX_*` » — le fichier qui n'en contient
> aucune. C'est `NkCompilerDetect.h` qu'il fallait. Aucune macro de standard
> n'arrive donc jamais, et toutes les gardes restent fermées **en silence**.

⚠️ **Et définir `NK_CPP11` ne se fait PAS d'une ligne** — mesuré :

```
-DNK_CPP11 -> NkStringView.h:1857: error: inline namespace must be specified at initial definition
              (rouvre en `inline namespace literals` ce que NkTypeUtils.h:147 declare en `namespace literals`)
```

Un défaut réel dans une branche que **personne n'a jamais compilée**. Activer la
macro fait compiler 47 directives de code jamais lu par un compilateur. C'est un
**chantier à part**, à mener avec un build complet mesuré avant / après — pas un
correctif. Il n'était d'ailleurs pas la solution du manque §1 : sur `NkHashMap`,
`-DNK_CPP11` donnait **exactement la même erreur, à la ligne près**, parce que le
constructeur à forwarding parfait était instancié avec des lvalues const.

#### 📐 Recensement chiffré du 2026-08-16 — combien dort, et ce que coûte l'ouverture

*Périmètre : `Kernel/`, `Applications/`, `Engine/`, hors `Externals/`.
Configuration : **C++17, Debug, sans optimisation**, g++ 16.1.0.
Comptage sur **lignes préprocesseur réelles** (`^\s*#\s*(if|ifdef|elif)`), pas
par `grep` nu — le `grep` naïf rend **83** occurrences dans `Foundation` là où il
n'y a que **34** gardes : facteur **2,4** de sur-comptage, dû aux blocs `@code`
qui enseignent l'idiome.*

| mesure | valeur |
|---|---|
| gardes `NK_CPP11` réelles | **34** dans `Foundation`, **37** tous périmètres, sur **16 fichiers** |
| autres macros de la famille (`NK_CPP14/17/20`, `NK_CPP11_SUPPORT`) | **0** — `NK_CPP11` est seule |
| **lignes de code dormantes** derrière ces gardes | **914** |
| dont constructeurs de déplacement / assignation | 15 blocs |
| dont forwarding parfait / variadiques | 13 blocs |
| dont `noexcept` | 3 blocs |
| dont divers | 6 blocs |
| `initializer_list` | **0** |

**Ce que coûte l'ouverture, mesuré** — balayage « chaque en-tête compile **seul** »
sur les 53 de `NKContainers` :

```
sans NK_CPP11 : 50/53      (3 echecs ANTERIEURS, hors sujet :
                            NkBind.h, NkFuntionV2.h, NkBasicStringView.h)
avec -DNK_CPP11 : 35/53    -> 15 echecs NOUVEAUX
```

⚠️ **Les 15 nouveaux échecs ont UNE seule cause, pas quinze.** Tous portent
`inline namespace must be specified at initial definition` :

```
NKCore/NkTypeUtils.h:147     namespace literals {          <- definition INITIALE, non gardee
NkStringView.h:1857          inline namespace literals {   <- rouvre INLINE, sous NK_CPP11
NkStringHash.h:571           inline namespace literals {   <- idem
```

Isolé sur une TU de deux `#include` : **compile sans la macro, échoue avec**, et
**dans les deux ordres d'inclusion** — donc conflit réel, pas artefact d'ordre.

🎯 **Ce que ces nombres disent, et c'est l'inverse de ce qu'on croyait** : le
verrou n'est pas « 47 directives risquées à auditer ». C'est **une incohérence de
namespace en deux lignes** devant **914 lignes de code déjà écrites**. Les trois
constructeurs *piecewise* de §1/§2 (`NkPair`, et les notes « volontairement HORS
de `#if defined(NK_CPP11)` » dans `NkHashMap`/`NkMap`/`NkUnorderedMap`) sont des
contournements de ce verrou, et **10 blocs `@code`** enseignent aux appelants un
idiome mort.

**Reste à mesurer avant toute décision** : l'effet sur le **build complet** (203
cibles), la macro forcée, Debug **et** Release. Le balayage d'en-têtes ne couvre
que `NKContainers` ; il ne dit rien des `.cpp` ni des autres modules.

### 6. Dette zero-STL réelle de Foundation : six fichiers

⚠️ **Un `grep std::` sur Foundation surcompte d'un facteur 4 à 30** — ces fichiers
portent des centaines de lignes d'exemples en commentaire.

**Méthode qui ne ment pas** : `gcc -fpreprocessed -dD -E <fichier>` retire les
commentaires **sans** résoudre les inclusions, puis on compte les directives
survivantes. *Un commentaire peut écrire `#include <vector>` ; il ne peut pas
l'inclure.*

| Module | `grep std::` | inclusions STL **réelles** |
|---|---|---|
| NKContainers | 345 | `<new>` `<utility>` `<type_traits>` `<initializer_list>` `<stdexcept>` |
| NKCore | 290 | `<string>` `<sstream>` `<stdexcept>` `<type_traits>` |
| NKMath | 98 | `<ostream>`×3 `<iostream>` `<string>` `<algorithm>` `<initializer_list>` |
| NKMemory | 191 | `<new>`×3 `<memory>` `<type_traits>` |
| **NKPlatform** | 98 | `<new>` **et rien d'autre — ce module est propre** |

Les six fichiers qui portent la dette :

| Fichier | STL réelle | Consommateurs | Nature |
|---|---|---|---|
| `NKContainers/String/NkFormat.h` | `<stdexcept>` | 57 | aucun type d'erreur maison employé ici — or `Utilities/NkResult.h` existe |
| `NKMath/NkAngle.h` + `.cpp`, `NkColor.cpp` | `<ostream>` | API publique | `friend std::ostream &operator<<` **dans la surface publique** : tout consommateur de `NkAngle` hérite de `<ostream>` par transitivité. **La plus structurelle des six.** |
| `NKMath/NkLegacySystem.h` | `<string>` `<iostream>` `<algorithm>` | 8 | min/max/clamp et sortie de débogage |
| `NKCore/NkEnumeration.h` | `<string>` `<sstream>` `<stdexcept>` | **0** | **code mort** — personne ne l'inclut. À supprimer ou réparer, pas à arbitrer. |
| `NKMemory/NkStlAdapter.h` | `<memory>` | 1 | **par conception** : c'est l'adaptateur STL. Non-défaut, cité pour que personne ne le « corrige ». |

⚠️ **Nuance qui change l'arbitrage sur NKCore** : NKCore est **sous**
NKContainers dans l'ordre de dépendance (`NkCompat.h:29` inclut
`NKCore/Assert/NkAssert.h`). NKCore ne **peut pas** employer `NkString` sans
inverser la dépendance. Son `<string>` n'est pas un contournement silencieux,
c'est une contrainte de couche — le seul vrai remède serait une chaîne minimale
dans NKCore lui-même. **Nommé, non tranché.**

### 7. Ce que cet inventaire dit de la méthode

Trois instruments ont été pris en défaut en une session, chacun d'une façon
différente, et c'est le résultat le plus réutilisable :

1. **`grep` compte les commentaires** — facteur 4 à 30 sur la dette STL. Remède : le préprocesseur.
2. **Une recherche par nom ne voit pas une surcharge manquante** — `begin()` non-const est invisible à tout balayage textuel qui trouve `begin() const`. Remède : l'éditeur de liens.
3. **Une macro mal orthographiée dans un `#if` ne se voit nulle part** — le code compile, il compile juste l'autre branche. Remède : mesurer que la branche attendue est bien celle qui est prise.
4. **Un témoin qui inclut plusieurs en-têtes ensemble ne teste aucun en-tête.**
   Ajouté le 2026-08-16, et payé le jour même : le témoin des cinq conteneurs
   incluait les maps **avant** les sets. Les maps tirent `NkPair.h` ; le tag y
   était défini ; les sets ne l'incluent pas. **Le témoin passait, et NKContainers
   ne compilait pas.** Remède, une ligne par en-tête :

   ```
   echo '#include "<en-tete>"' > solo.cpp && clang++ -fsyntax-only solo.cpp
   ```

   Un en-tête doit compiler **seul**. Un témoin multi-en-têtes mesure l'union des
   inclusions, pas leur autonomie — et l'ordre d'inclusion masque le manque.

Le point commun des quatre : **ça répond, donc on croit avoir demandé.**

---

## Dépendances

- **Couches en dessous (utilisées)** : NKPlatform (export, inline, foundation
  log), NKCore (types fondamentaux, traits, asserts), NKMemory (NkAllocator
  pour gestion mémoire flexible, `NkFunction` mémoire bas niveau, hash table
  interne)
- **Modules au-dessus qui en dépendent** : NKMath (`NkFormat` pour
  `NkMathFormat.h`), NKRHI (handles + NkVector dans command buffers), NKRenderer
  (NkVector / NkHashMap / NkString partout), services moteur (NKFont
  glyph cache, NKImage formats, NKAudio sources, NKScene ECS storage), Nkentseu
  application framework (EventBus, LayerStack), Noge éditeur, PV3DE
