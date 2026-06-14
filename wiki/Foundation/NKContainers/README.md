# NKContainers — documentation détaillée

Le module **NKContainers**, famille par famille. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKContainers.md](../NKContainers.md).

Chaque page suit la même structure : un **tutoriel** narratif (style SFML, la prose mène), un
**aperçu** tabulaire de toute l'API, puis une **référence-cours** où chaque élément est expliqué
avec sa complexité et ses cas d'usage concrets dans plusieurs domaines (rendu, ECS, physique,
animation, gameplay/IA, audio, UI/2D, IO, GPU).

Tous les conteneurs sont **zéro-STL**, templatés, conscients de l'allocateur (mémoire via
[NKMemory](../NKMemory.md), jamais `new`/`delete`), et exposent une **double casse** d'API : le
style maison `PascalCase` (`PushBack`, `Size`) **et** le style STL minuscule (`push_back`, `begin`)
pour le `range-based for` et les algorithmes.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Sequential.md](Sequential.md) | Ranger dans un ordre : tableau dynamique, listes chaînées, file double. | `NkVector.h`, `NkList.h`, `NkDoubleList.h`, `NkDeque.h` |
| [CacheFriendly.md](CacheFriendly.md) | Disposition mémoire optimale : tableau fixe/inline, pool d'objets, tampon circulaire. | `NkArray.h`, `NkPool.h`, `NkRingBuffer.h` |
| [Associative.md](Associative.md) | Associer clé→valeur et ensembles : tables de hachage, arbres ordonnés, trie, tas. | `NkHashMap.h`, `NkUnorderedMap.h`, `NkUnorderedSet.h`, `NkMap.h`, `NkSet.h`, `NkBTree.h`, `NkBinaryTree.h`, `NkTrie.h`, `NkPriorityQueue.h` |
| [Adapters.md](Adapters.md) | Restreindre une interface : file FIFO, pile LIFO par-dessus un conteneur. | `NkQueue.h`, `NkStack.h` |
| [Heterogeneous.md](Heterogeneous.md) | Regrouper des types différents : paire, tuple. | `NkPair.h`, `NkTuple.h` |
| [Strings.md](Strings.md) | Texte : chaîne possédante, vue, large, construction efficace, formatage, hash. | `NkString.h`, `NkBasicString.h`, `NkStringView.h`, `NkBasicStringView.h`, `NkWString.h`, `NkStringBuilder.h`, `NkStringHash.h`, `NkStringUtils.h`, `NkFormat.h` |
| [String-Encoding.md](String-Encoding.md) | Encodages de caractères : UTF-8/16/32, ASCII, Base64. | `NkUTF8.h`, `NkUTF16.h`, `NkUTF32.h`, `NkASCII.h`, `NkBase64.h`, `NkEncoding.h` |
| [Views.md](Views.md) | Vue contiguë non-possédante (équivalent `std::span`). | `NkSpan.h` |
| [Functional.md](Functional.md) | Encapsuler un appelable : `NkFunction`, liaison d'arguments, utilitaires. | `NkFunction.h`, `NkFunctional.h`, `NkBind.h` |
| [Iterators.md](Iterators.md) | Parcours générique : catégories d'itérateurs, liste d'initialisation. | `NkIterator.h`, `NkInitializerList.h` |
| [Utilities.md](Utilities.md) | Valeurs optionnelles, succès/erreur, somme de types : `NkOptional`, `NkResult`, `NkVariant`. | `NkOptional.h`, `NkResult.h`, `NkVariant.h` |
| [Specialized.md](Specialized.md) | Structures de domaine : graphe, partitionnement spatial (octree, quadtree). | `NkGraph.h`, `NkOctree.h`, `NkQuadTree.h` |

[← Récap NKContainers](../NKContainers.md) · [← Couche Foundation](../README.md)
