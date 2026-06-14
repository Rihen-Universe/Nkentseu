# NKECS

> Couche **Runtime** · Un ECS (Entity-Component-System) bas niveau à architecture
> par archétypes : monde, stockage dense, requêtes typées, systèmes ordonnancés,
> bus d'événements gameplay, réflexion de composants et sérialisation JSON.

NKECS est le cœur de données du gameplay : des **entités** (de simples identifiants),
des **composants** (de la donnée pure attachée aux entités) et des **systèmes** (la
logique qui itère sur les composants). Le stockage est organisé par **archétypes** —
toutes les entités de même signature partagent un bloc mémoire dense, ce qui rend
l'itération cache-friendly et vectorisable. C'est la fondation sur laquelle la couche
moteur (Noge) bâtit `NkGameObject`, les prefabs et les blueprints : NKECS lui-même
reste **générique et autonome**, il ne connaît rien de ces concepts plus haut.

On crée un monde, on y peuple des entités via un builder fluide, on leur ajoute des
composants (ce qui peut déclencher une **migration d'archétype**), puis on itère
avec des **requêtes** ou on laisse un **scheduler** orchestrer des systèmes en
parallèle. Un **bus d'événements** gameplay (distinct du bus moteur) relie le tout.

- **Namespace** : `nkentseu::ecs` (cœur). Des alias de commodité sont réexportés
  dans `nkentseu` sauf si `NK_ECS_NO_ALIASES` est défini.
- **Header parapluie** : `#include "NKECS/NKECS.h"`
- **Thread-safety** : `NkWorld` n'est **pas** thread-safe (mutex externe requis si
  accès concurrent). Le registre de types et le bus d'événements sont lockés.

> **Avertissement de portée.** Cette doc ne décrit que l'API réellement déclarée
> dans les headers. Deux pièges majeurs sont signalés tels quels : (1) les exemples
> en commentaire de `NKECS.h` utilisent une API **aspirationnelle/fictive**
> (`AddComponent`, `ForEach` sur le World) — la vraie méthode d'ajout est
> `NkWorld::Add<T>` ; (2) `NkJsonSerialization.h` est **STL-full** et dépend de
> `NkPrefab`/`NkBlueprint` qui vivent chez Noge — il **ne compile pas en standalone**.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Créer un monde, peupler / détruire des entités, lire-écrire des composants | [Le monde](NKECS/World.md) |
| Comprendre le stockage dense : archétypes, graphe de transitions, pools, registre de types | [Le stockage](NKECS/Storage.md) |
| Itérer efficacement sur les composants ; écrire et ordonnancer des systèmes | [Requêtes & systèmes](NKECS/Query-Systems.md) |
| Émettre/écouter des événements gameplay, réfléchir les composants, sérialiser en JSON | [Événements & réflexion](NKECS/Events-Reflect.md) |

Chaque page documente l'**API publique réelle** (signatures, complexité, idiomes,
pièges d'ownership et d'allocation), en signalant les symboles **externes** (déclarés
dans d'autres headers du module) et l'API **fictive** des commentaires.

---

## Aperçu des familles

- **Monde** (`World/NkWorld.h`, `NkECSDefines.h`) — `NkWorld` est la façade :
  création (`CreateEntity`, builders `Create()`/`CreateBatch()`), destruction
  immédiate ou différée, `Add`/`Get`/`Has`/`Set`/`Remove` de composants, requêtes,
  bus d'événements et opérations différées (`FlushDeferred`). `NkEntityId` est un
  identifiant fort (index + génération) anti-dangling.
- **Stockage** (`Storage/NkArchetype.h`, `Storage/NkArchetypeGraph.h`,
  `Storage/NkComponentPool.h`, `Core/NkTypeRegistry.h`) — un `NkArchetype` groupe
  les entités de même signature en SoA, un `NkComponentPool` par type ; le
  `NkArchetypeGraph` cache et crée les archétypes et leurs transitions (add/remove
  composant) ; le `NkTypeRegistry` attribue les `NkComponentId` et leurs métadonnées
  de cycle de vie ; `NkComponentMask` est le bitset de signature (256 bits).
- **Requêtes & systèmes** (`Query/NkQuery.h`, `System/NkSystem.h`,
  `System/NkScheduler.h`) — `NkQueryResult<Ts...>` itère (entité par entité ou par
  lot SIMD) avec filtres `With`/`Without` ; `NkSystem` + `NkSystemDesc` décrivent la
  logique et ses dépendances de lecture/écriture ; `NkScheduler` construit un DAG et
  exécute en vagues parallèles via un `NkJobPool`.
- **Événements & réflexion** (`Events/NkGameplayEventBus.h`, `Reflect/NkReflect.h`,
  `Serialization/NkJsonSerialization.h`) — `NkGameplayEventBus` (canaux par type,
  Emit synchrone ou Queue/Drain), `NkReflectRegistry` + macros `NK_REFLECT_*` pour la
  réflexion des champs, et les helpers JSON (STL-full, non autonomes).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKECS.h` | Parapluie (inclut tout) + alias `nkentseu::` + `static_assert` de taille. | — |
| `NkECSDefines.h` | Types fondamentaux (`NkEntityId`, `NkComponentId`…), constantes, macros, `NkSpan`, `detail::FNV1a`. | [Monde](NKECS/World.md) |
| `World/NkWorld.h` | `NkWorld`, `NkEntityBuilder`, `NkBatchBuilder`. | [Monde](NKECS/World.md) |
| `Core/NkTypeRegistry.h` | `NkTypeRegistry`, `ComponentMeta`, `NkComponentMask`, `NkIdOf`/`NkMetaOf`, `NK_COMPONENT`. | [Stockage](NKECS/Storage.md) |
| `Storage/NkComponentPool.h` | `NkComponentPool` (stockage SoA d'un type). | [Stockage](NKECS/Storage.md) |
| `Storage/NkArchetype.h` | `NkArchetype`, `NkEntityRecord`, `NkEntityIndex`. | [Stockage](NKECS/Storage.md) |
| `Storage/NkArchetypeGraph.h` | `NkArchetypeGraph` (caches Mask→Id et transitions). | [Stockage](NKECS/Storage.md) |
| `Query/NkQuery.h` | `NkQueryResult<Ts...>`, `detail::MakeMask`. | [Requêtes & systèmes](NKECS/Query-Systems.md) |
| `System/NkSystem.h` | `NkSystem`, `NkSystemDesc`, `NkSystemGroup`, `NkLambdaSystem`. | [Requêtes & systèmes](NKECS/Query-Systems.md) |
| `System/NkScheduler.h` | `NkScheduler`, `NkJobPool`. | [Requêtes & systèmes](NKECS/Query-Systems.md) |
| `Events/NkGameplayEventBus.h` | `NkGameplayEventBus`, `NkEventChannel<T>`, `IEventQueue`. | [Événements & réflexion](NKECS/Events-Reflect.md) |
| `Reflect/NkReflect.h` | `NkReflectRegistry`, `NkTypeInfo`, `NkFieldInfo`, macros `NK_REFLECT_*`. | [Événements & réflexion](NKECS/Events-Reflect.md) |
| `Serialization/NkJsonSerialization.h` | Helpers JSON prefab/blueprint (⚠️ STL-full, non autonome). | [Événements & réflexion](NKECS/Events-Reflect.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
