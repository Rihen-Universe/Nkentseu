# Couche System

La couche **System** fournit les **services bas niveau** du moteur, bâtis sur
[Foundation](../Foundation/README.md) : journalisation, threading, temps, flux, système de
fichiers, réseau, réflexion et sérialisation. Elle abstrait l'OS et offre une API portable et
**zéro-STL** à toutes les couches supérieures (Runtime, Engine, Applications).

## Modules

| Module | Rôle | Doc |
|--------|------|-----|
| **NKLogger** | Journalisation : logger central, niveaux, formatage, sinks (console, fichier, rotation, async…) | [NKLogger.md](NKLogger.md) |
| **NKThreading** | Threads, pool, mutex/spinlock, primitives de synchronisation, futures/promesses | [NKThreading.md](NKThreading.md) |
| **NKTime** | Horloges, chrono, durées, dates, fuseaux horaires | [NKTime.md](NKTime.md) |
| **NKStream** | Flux binaire/fichier/console + interface ressource `NKIResource` | [NKStream.md](NKStream.md) |
| **NKFileSystem** | Chemins, fichiers, dossiers, surveillance (watcher) | [NKFileSystem.md](NKFileSystem.md) |
| **NKNetwork** | Sockets, UDP fiable, bitstream, connexion, RPC, lobby, HTTP | [NKNetwork.md](NKNetwork.md) |
| **NKReflection** | Réflexion minimaliste : types, classes, propriétés, méthodes | [NKReflection.md](NKReflection.md) |
| **NKSerialization** | Sérialisation JSON/XML/YAML/binaire/natif + pipeline d'assets | [NKSerialization.md](NKSerialization.md) |

## Dépendances

```
Foundation (NKCore, NKMemory, NKContainers, NKMath, NKPlatform)
   ▲
System
   ├── NKLogger        (messages via NKContainers, temps via NKTime)
   ├── NKThreading     (atomics NKCore, allocation NKMemory)
   ├── NKTime
   ├── NKStream        (NKIResource ; base des I/O fichier)
   ├── NKFileSystem    (chemins, I/O ; utilise NKStream)
   ├── NKNetwork       (transport/RPC/lobby ; ⚠️ parapluie NKNetwork.h cassé, voir module)
   ├── NKReflection    (descripteurs de types runtime)
   └── NKSerialization (formats ; pont avec NKReflection)
```

> **Règle dure (rappel Foundation)** : toute allocation passe par NKMemory
> (`New`/`Delete`), jamais `new`/`delete` directs ; toute classe avec `Create` expose `Destroy`.
> Voir [Conventions](../Conventions.md).

[← Index du wiki](../README.md) · [← Couche Foundation](../Foundation/README.md)
