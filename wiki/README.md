# Wiki Nkentseu

Documentation de référence du moteur **Nkentseu** (éditeur **Rihen**). Ce wiki vise
une description **exhaustive et exacte** de chaque module : chaque classe, fonction et
type publics y est décrit, sans saut.

## Organisation

```
wiki/
├── README.md            ← vous êtes ici (index général)
├── Architecture.md      ← couches du moteur + règles transverses
├── Conventions.md       ← nomenclature, zéro-STL, règle NKMemory
├── Getting-Started.md   ← premier programme
├── Foundation/          ← couche 1 : base (mémoire, conteneurs, maths, types, plateforme)
├── System/              ← couche 2 : services système (fenêtre, événements, fichiers, temps…)
├── Runtime/             ← couche 3 : sous-systèmes (NKCanvas, NKUI, NKAudio, NKFont, RHI…)
├── Engine/              ← couche 4 : moteur applicatif (Noge…)
└── Applications/        ← couche 5 : apps et démos d'exemple
```

## Les couches (dépendances strictes, acycliques)

> **Foundation → System → Runtime → Engine → Application**
> Une couche ne dépend QUE des couches inférieures. Voir [Architecture](Architecture.md).

| Couche | Rôle | Modules (extraits) |
|--------|------|--------------------|
| **Foundation** | Briques de base | NKMemory, NKContainers, NKMath, NKCore, NKPlatform |
| **System** | Services OS | NKLogger, NKThreading, NKTime, NKStream, NKFileSystem, NKNetwork, NKReflection, NKSerialization |
| **Runtime** | Sous-systèmes | NKWindow, NKEvent, NKCanvas, NKUI, NKAudio, NKFont, NKImage, NKRHI, NKSL, NKRenderer |
| **Engine** | Moteur applicatif | Noge (moteur), Nogee (éditeur) |
| **Application** | Apps / démos | Pong, Sandbox (5 démos NKCanvas/NKUI) |

## État de la documentation

| Module | Couche | État |
|--------|--------|------|
| **Toute la couche Foundation** (NKCore, NKMemory, NKContainers, NKMath, NKPlatform) | Foundation | ✅ complet |
| **Toute la couche System** (NKLogger, NKThreading, NKTime, NKStream, NKFileSystem, NKNetwork, NKReflection, NKSerialization) | System | ✅ complet |
| **Toute la couche Runtime** (NKWindow, NKEvent, NKImage, NKCamera, NKFont, NKCollision, NKECS, NKCanvas, NKRHI, NKSL, NKAudio, NKUI, NKRenderer) | Runtime | ✅ complet |
| **Engine** (Noge) | Engine | ✅ complet |

## Liens rapides

- [Architecture du moteur](Architecture.md)
- [Conventions de code](Conventions.md)
- [Premier programme](Getting-Started.md)
- [Couche Foundation](Foundation/README.md)
- [Couche System](System/README.md)
