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
