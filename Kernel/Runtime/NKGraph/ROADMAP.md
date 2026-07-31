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
| P1 — Modèle de données (nodes/sockets typés/connexions/validation) | ✅ | `src/NKGraph/NkNodeGraph.h/.inl`, **en-tête pur** (testable sans lier de cible, comme `NkShortcutTable`), zero-STL. **Nommé `NkNodeGraph` et non `NkGraph` : `nkentseu::NkGraph<V,Alloc>` existe déjà dans NKContainers** (graphe pondéré générique, DFS/BFS, 877 l.). Conversions implicites **dirigées** et déclarées par le consommateur, jamais devinées. Une entrée n'accepte qu'une source (la 2ᵉ remplace, comme Blender/Unreal). Supprimer un nœud emporte ses liens. Identifiants **jamais recyclés**. |
| P2 — Évaluation (tri topologique, sous-graphes, plan aplati) | 🔶 | tri topologique + **refus du cycle à la connexion** (avec sa raison) livrés et testés. Restent : sous-graphes, plan aplati. |
| P3 — Sérialisation `.nkgraph` + undo/redo | ✅ | `NkNodeGraphIO.inl`. Format **texte**, une directive par ligne : un graphe se relit, se compare avec `git diff` et se répare à la main ; le binaire ferait gagner des octets sur des fichiers de quelques Ko. **Écart assumé** : annuler/refaire par **instantanés sérialisés**, pas par commandes inversibles — l'inverse de « supprimer un nœud » doit restaurer le nœud, tous ses liens **et** leurs identifiants, et c'est le genre d'inverse qu'on écrit presque juste, dont l'erreur ne se voit que trois manipulations plus tard. L'instantané est correct par construction. Coût : mémoire ∝ taille × profondeur ; négligeable à cette échelle, et l'API publique ne changera pas si un jour il faut basculer. |
| P4 — Widget canvas (NKEditorKit) | ❌ | pan/zoom, fils, recherche, groupes, preview |
| P5 — 1er consommateur : NKCode Phase 4 (Blueprint) OU matériaux T.2 | ❌ | le premier qui démarre construit AVEC le cœur |
| P6 — 2e consommateur (l'autre des deux, ou VFX) | ❌ | force la généralisation de l'API |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

**Preuve (31/07/2026)** — 12 cas dans `Applications/NKEditMeshHarness` (136 cas au
total, les 124 antérieurs inchangés). Ils sont choisis pour qu'une implantation
fausse **échoue**, pas pour confirmer ce qui marche :

| cas | pourquoi il discrimine | résultat |
|---|---|---|
| `topo-losange-insere-inverse` | les nœuds sont créés **D,C,B,A**, l'inverse de l'ordre attendu — un tri qui renverrait l'ordre d'insertion échouerait | `A>C>B>D` |
| `cycle-refuse` | vérifie aussi que le lien **n'a pas été posé** et que le graphe reste triable — un refus qui laisserait le lien donnerait le même code d'erreur | `cycle`, liens=2, triable |
| `conversion-dirigee` | testée dans les **deux sens** : une table symétrique par erreur passerait un test à sens unique | `réel>vect=ok`, `vect>réel=refusé` |
| `suppression-milieu` | porte sur le nœud **du milieu** : une extrémité ne montrerait pas l'oubli d'un sens | 2 liens → 0 |
| `entree-source-unique` | vérifie que la source restante est la **nouvelle** — garder l'ancienne donnerait le même compte | `y-la-nouvelle` |
| `sens-et-sockets` | « ce socket n'existe pas » ≠ « vous branchez une entrée sur une entrée » : l'interface doit pouvoir l'expliquer | 4 codes distincts |
| `identifiants-stables` | un id recyclé ferait pointer silencieusement une sauvegarde sur un autre nœud | pas de recyclage |
| `io-aller-retour` | compare les **textes caractère pour caractère**, pas des comptes : des libellés ou des conversions perdus laisseraient les comptes intacts | 246 o identiques |
| `io-identifiants-non-recycles` | **le piège du format** : sans la ligne `compteurs`, un aller-retour par ailleurs correct réattribuerait l'id du nœud supprimé | supprimé=2, nouveau=4 |
| `io-semantique-survit` | le graphe rechargé doit encore **accepter et refuser** comme avant — preuve que `conv` et les types ont été relus, pas seulement réécrits | `réel>vect=ok`, `réel>maillage=refusé` |
| `undo-restaure-les-liens` | **le piège de l'annulation** : la suppression du nœud du milieu tue 2 liens ; une annulation qui ne ressusciterait que le nœud laisserait un graphe coupé, d'apparence saine | nœud revenu, 2 liens, **mêmes ids** |
| `undo-branche-abandonnee` | remonter toute la pile doit redonner l'état initial **au texte près** — les comptes laisseraient passer une dérive de position ou de libellé | retour exact, refaisable 2→0 |

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
