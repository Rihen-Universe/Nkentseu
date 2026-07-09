# NKGraph — Roadmap (substrat de graphe de nodes UNIQUE)

> Décision d'architecture validée par Rihen le 2026-07-09. **Un seul système de
> graphe de nodes** pour tout l'écosystème : matériaux (NKRenderer Phase T.2),
> VFX (Noge), Blueprint/Scratch (NKCode), modélisation procédurale (Kernel/AI),
> anim graphs / state machines (NkAnima M2), futur rig graph. Aucun code encore —
> ce document est la spec de référence AVANT la première implémentation.

## Architecture en 3 couches (la séparation est la règle n°1)

```
Couche 3 — SÉMANTIQUE MÉTIER (chez chaque consommateur)
   bibliothèques de nodes + backends d'exécution :
   compilateur NkSL (matériaux) · plan d'exécution particules (VFX) ·
   VM/codegen script (Blueprint) · commandes NkMeshEditCommand (procédural)
        ▲
Couche 2 — ÉDITION (Engine/NKEditorKit)
   widget canvas node-based : pan/zoom, fils, sélection, recherche de nodes,
   groupes/commentaires, preview — UX apprise UNE fois, partagée partout
        ▲
Couche 1 — CŒUR (ce module : Kernel/Runtime/NKGraph)
   modèle de données pur : NkGraph, NkGraphNode, sockets TYPÉS, connexions,
   validation de types, sous-graphes, tri topologique / ordre d'évaluation,
   sérialisation .nkgraph, undo/redo (commandes inversibles)
```

**On unifie l'AUTORAT (modèle + sérialisation + UI), jamais l'EXÉCUTION.** Les
besoins d'exécution sont incompatibles : les matériaux se **compilent** vers NkSL
à l'édition (au runtime il n'y a plus de graphe), le VFX **évalue par frame** un
plan aplati (boucle chaude, jamais d'interprétation naïve), le Blueprint
**interprète** (VM) ou génère du C++. Le cœur fournit « l'ordre d'évaluation et
les types » ; chaque domaine fournit « ses nodes et ce qu'il en fait ».

## Garde-fous (là où ce genre de décision échoue)

1. **Cœur 100% agnostique** : aucun type métier (« texture », « bone », « son »)
   dans NKGraph. Les types de sockets sont enregistrés par les consommateurs via
   des type-ids (NKReflection). Un `if (nodeType == ...)` métier dans le cœur =
   architecture morte.
2. **Règle des deux consommateurs** : le cœur se construit AVEC son premier
   client réel, et c'est le **deuxième** client qui force la généralisation de
   l'API. Pas d'abstraction dans le vide. Premier client = **celui qui démarre
   en premier** : la Phase 4 « Graph » de NKCode est marquée « PROCHAIN » chez
   l'agent NKCode (note de coordination posée dans son ROADMAP le 2026-07-09) —
   sinon le graphe de matériaux (périmètre fermé, backend NkSL existant).
3. **Précédents internes** : même mouvement que NkGizmo3D (extrait de Demo3D →
   NKRenderer/Core) et NKEditorKit (extrait pour NKCode → partagé). Précédents
   externes : Unreal (EdGraph unique pour Blueprint/matériaux/anim), Blender
   (un système de nodes : shader/geometry/compositor) ; contre-exemple Godot
   (implémentations séparées, VisualScript abandonné).

## Synthèse

| Brique | Statut | Notes |
|--------|--------|-------|
| P1 — Modèle de données (nodes/sockets typés/connexions/validation) | ❌ | zero-STL, NKContainers/NKMemory |
| P2 — Évaluation (tri topologique, sous-graphes, plan aplati) | ❌ | ordre d'évaluation exposé, pas d'exécution métier |
| P3 — Sérialisation `.nkgraph` + undo/redo | ❌ | NKSerialization ; commandes inversibles (modèle NkAnimationEditor) |
| P4 — Widget canvas (NKEditorKit) | ❌ | pan/zoom, fils, recherche, groupes, preview |
| P5 — 1er consommateur : NKCode Phase 4 (Blueprint) OU matériaux T.2 | ❌ | le premier qui démarre construit AVEC le cœur |
| P6 — 2e consommateur (l'autre des deux, ou VFX) | ❌ | force la généralisation de l'API |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

## Consommateurs prévus (état de leur côté)

- **NKRenderer Phase T.2** — graphe de matériaux (extension des templates
  existants, compat ascendante `.nkasset`). Cf. `Kernel/Runtime/NKRenderer/ROADMAP.md`.
- **Noge VFX** — graphe d'effets (émetteurs/forces/rendu). Cf. `Engine/Noge/ROADMAP.md`.
  ⚠️ `Engine/Noge/src/Noge/ECS/VisualScript/NkBlueprint.h` (header de spec 696
  lignes, aucun .cpp) devra être **réaligné sur NKGraph** au moment de son
  implémentation — ne pas l'implémenter en silo.
- **NKCode** — Blueprint + Scratch (`src/NKCode/{Graph,Blueprint,Blocks}/` =
  README seulement à ce jour) : NKCode devient un **client** de NKGraph comme les
  autres. ⚠️ NKCode est tenu par un autre agent — coordination nécessaire avant
  d'y implémenter quoi que ce soit.
- **Kernel/AI** — graphe de modélisation procédurale (pilotable par prompt).
  Cf. `Kernel/AI/ROADMAP.md`.
- **NkAnima M2** — anim graph / state machine (HFSM) éditables. Cf.
  `Applications/NkAnima/ROADMAP.md`.

## Dépendances

- Cœur : NKCore, NKMemory, NKContainers, NKSerialization, NKReflection (type-ids
  de sockets). **Rien d'autre** — pas de NKRHI, pas de NKRenderer, pas d'UI.
- Widget (couche 2) : NKEditorKit (qui a ses propres deps UI).
- Pas de `.jenga` tant que P1 n'est pas démarré (module doc-only pour l'instant).
