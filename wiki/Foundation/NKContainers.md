# NKContainers

> Couche **Foundation** · Conteneurs **zéro-STL** : séquentiels, associatifs, chaînes,
> vues, fonctionnels, spécialisés. L'équivalent maison de `<vector>`, `<string>`,
> `<unordered_map>`, `<span>`, `<functional>`…

NKContainers fournit toutes les structures de données du moteur, **sans la STL** et en
allouant via [NKMemory](NKMemory.md). 53 headers organisés par familles. Types templatés
avec une API proche de la STL mais en `NkPascalCase` (`PushBack`, `Size`, `Find`…).

- **Namespace** : `nkentseu` / `nkentseu::containers`
- **Header parapluie** : `#include "NKContainers/NKContainers.h"`

---

## 1. Documentation détaillée

| Fichier | Famille | Conteneurs |
|---------|---------|-----------|
| [Sequential.md](NKContainers/Sequential.md) | Séquentiels | `NkVector`, `NkList`, `NkDoubleList`, `NkDeque` |
| [CacheFriendly.md](NKContainers/CacheFriendly.md) | Cache-friendly | `NkArray`, `NkPool`, `NkRingBuffer` |
| [Associative.md](NKContainers/Associative.md) | Associatifs | `NkHashMap`, `NkUnorderedMap/Set`, `NkMap`, `NkSet`, `NkBTree`, `NkBinaryTree`, `NkTrie`, `NkPriorityQueue` |
| [Adapters.md](NKContainers/Adapters.md) | Adaptateurs | `NkQueue`, `NkStack` |
| [Heterogeneous.md](NKContainers/Heterogeneous.md) | Hétérogènes | `NkPair`, `NkTuple` |
| [Strings.md](NKContainers/Strings.md) | Chaînes | `NkString`, `NkStringView`, `NkWString`, `NkStringBuilder`, `NkFormat`, `NkStringUtils`, `NkStringHash` |
| [String-Encoding.md](NKContainers/String-Encoding.md) | Encodages | `NkUTF8/16/32`, `NkASCII`, `NkBase64`, `NkEncoding` |
| [Views.md](NKContainers/Views.md) | Vues | `NkSpan` |
| [Functional.md](NKContainers/Functional.md) | Fonctionnels | `NkFunction`, `NkFunctional`, `NkBind` |
| [Iterators.md](NKContainers/Iterators.md) | Itérateurs | `NkIterator`, `NkInitializerList` |
| [Utilities.md](NKContainers/Utilities.md) | Utilitaires | `NkOptional`, `NkResult`, `NkVariant` |
| [Specialized.md](NKContainers/Specialized.md) | Spécialisés | `NkGraph`, `NkOctree`, `NkQuadTree` |

---

## 2. Aperçu par famille

- **Séquentiels** : `NkVector` (tableau dynamique contigu, le plus utilisé), `NkList` /
  `NkDoubleList` (listes chaînées), `NkDeque` (file double).
- **Cache-friendly** : `NkArray` (tableau de taille fixe / inline), `NkPool` (pool
  d'objets), `NkRingBuffer` (tampon circulaire).
- **Associatifs** : `NkHashMap` / `NkUnorderedMap` (tables de hachage), `NkMap` / `NkSet`
  (arbres ordonnés), `NkBTree` / `NkBinaryTree` / `NkTrie`, `NkPriorityQueue` (tas).
- **Adaptateurs** : `NkQueue`, `NkStack` (par-dessus un conteneur sous-jacent).
- **Hétérogènes** : `NkPair`, `NkTuple`.
- **Chaînes** : `NkString` (équivalent `std::string`), `NkStringView` (vue non-possédante),
  `NkWString` (large), `NkStringBuilder` (construction efficace), `NkFormat` (formatage),
  `NkStringUtils`, `NkStringHash`. Sous-module **encodage** (UTF-8/16/32, ASCII, Base64).
- **Vues** : `NkSpan` (vue contiguë non-possédante, équivalent `std::span`).
- **Fonctionnels** : `NkFunction` (callable), `NkBind`, `NkFunctional`.
- **Itérateurs** : `NkIterator`, `NkInitializerList`.
- **Utilitaires** : `NkOptional`, `NkResult` (succès/erreur), `NkVariant`.
- **Spécialisés** : `NkGraph`, `NkOctree`, `NkQuadTree` (partitionnement spatial).

> ⚠️ Coquille connue : `NkFuntionV1.h` / `NkFuntionV2.h` (« Funtion » au lieu de
> « Function ») — anciennes itérations conservées ; préférez `NkFunction.h`.

---

## 3. Index des 53 headers

| Sous-dossier | Headers |
|--------------|---------|
| `Sequential/` | `NkVector.h`, `NkList.h`, `NkDoubleList.h`, `NkDeque.h`, `NkVectorError.h` |
| `CacheFriendly/` | `NkArray.h`, `NkPool.h`, `NkRingBuffer.h` |
| `Associative/` | `NkHashMap.h`, `NkUnorderedMap.h`, `NkUnorderedSet.h`, `NkMap.h`, `NkSet.h`, `NkBTree.h`, `NkBinaryTree.h`, `NkTrie.h`, `NkPriorityQueue.h` |
| `Adapters/` | `NkQueue.h`, `NkStack.h` |
| `Heterogeneous/` | `NkPair.h`, `NkTuple.h` |
| `String/` | `NkString.h`, `NkBasicString.h`, `NkStringView.h`, `NkBasicStringView.h`, `NkWString.h`, `NkStringBuilder.h`, `NkStringHash.h`, `NkStringUtils.h`, `NkFormat.h` |
| `String/Encoding/` | `NkUTF8.h`, `NkUTF16.h`, `NkUTF32.h`, `NkASCII.h`, `NkBase64.h`, `NkEncoding.h` |
| `Views/` | `NkSpan.h` |
| `Functional/` | `NkFunction.h`, `NkFunctional.h`, `NkBind.h`, `NkFuntionV1.h`, `NkFuntionV2.h` |
| `Iterators/` | `NkIterator.h`, `NkInitializerList.h` |
| `Utilities/` | `NkOptional.h`, `NkResult.h`, `NkVariant.h` |
| `Specialized/` | `NkGraph.h`, `NkOctree.h`, `NkQuadTree.h` |
| racine | `NKContainers.h`, `NkCompat.h`, `NkContainersApi.h` |

---

[← Couche Foundation](README.md) · [Index du wiki](../README.md)
