# NKECS — documentation détaillée

Le module **NKECS**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKECS.md](../NKECS.md).

Chaque page documente l'**API publique réelle** (signatures, complexité, idiomes,
pièges d'ownership et d'allocation). Les symboles **externes** (déclarés dans un autre
header du module) et l'API **fictive** des commentaires sont signalés explicitement.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [World.md](World.md) | Le monde ECS : `NkEntityId` (index+génération), création (`CreateEntity`, builders), destruction immédiate/différée, `Add`/`Get`/`Has`/`Set`/`Remove`, requêtes, événements, `FlushDeferred`/`DrainEvents`. Constantes, types et `NkSpan`. | `World/NkWorld.h`, `NkECSDefines.h`, `NKECS.h` |
| [Storage.md](Storage.md) | Stockage par archétypes : `NkComponentMask`, registre de types & `ComponentMeta`, `NkComponentPool` (SoA, swap-remove), `NkArchetype`/`NkEntityIndex`, graphe d'archétypes & transitions add/remove. | `Storage/NkArchetype.h`, `Storage/NkArchetypeGraph.h`, `Storage/NkComponentPool.h`, `Core/NkTypeRegistry.h` |
| [Query-Systems.md](Query-Systems.md) | Requêtes typées `NkQueryResult<Ts...>` (`With`/`Without`, `ForEach`/`ForEachBatch`), systèmes (`NkSystem`, `NkSystemDesc`, groupes, `NkLambdaSystem`), ordonnancement par DAG et vagues parallèles (`NkScheduler`, `NkJobPool`). | `Query/NkQuery.h`, `System/NkScheduler.h`, `System/NkSystem.h` |
| [Events-Reflect.md](Events-Reflect.md) | Bus d'événements gameplay (canaux par type, Emit vs Queue/Drain), réflexion de composants (`NkReflectRegistry`, `NkTypeInfo`, macros `NK_REFLECT_*`), sérialisation JSON (⚠️ STL-full, dépend de Prefab/Blueprint chez Noge). | `Events/NkGameplayEventBus.h`, `Reflect/NkReflect.h`, `Serialization/NkJsonSerialization.h` |

[← Récap NKECS](../NKECS.md) · [← Couche Runtime](../README.md)
