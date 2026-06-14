# Noge

> Couche **Engine** · Le framework applicatif au sommet du Runtime : boucle de jeu,
> LayerStack, EventBus, ECS gameplay (composants, entités, scènes, systèmes, scripting),
> et une suite création/éditeur (modélisation, animation, design 2D, IO, viewport, physique).

Noge est le **moteur de jeu** de Nkentseu, posé sur tout le Runtime (NKWindow, NKEvent,
NKCanvas, NKRHI, NKRenderer, NKECS…). Il fournit ce qui transforme une collection de modules
en application interactive : une classe `NkApplication` qui possède la fenêtre, le device GPU
et la boucle principale ; une pile de couches (`NkLayerStack`) pour structurer le gameplay et
l'UI ; un `NkEventBus` typé pour la communication découplée ; et un ECS gameplay haut-niveau
(handles `NkGameObject` style Unity, acteurs `NkActor`/`NkPawn`/`NkCharacter` style Unreal,
scènes, prefabs, scripts C++/C#/Python). Au-dessus, une **suite de création** (modélisation
polygonale, rigging/animation, illustration 2D, import/export FBX/glTF/OBJ/SVG, viewport
éditeur, simulation de tissu/cheveux/ragdoll) destinée à l'éditeur Nogee.

> ⚠️ **Statut — beaucoup de sous-systèmes sont des SPECS.** Le **Core** (boucle, layers,
> EventBus, profiler) et l'**ECS gameplay** (composants, entités, scènes, systèmes de base)
> sont implémentés et utilisés. En revanche les familles **Modeling, Animation, Design2D, IO,
> Viewport, Physics**, ainsi que **VisualScript** et une partie du **Scripting**, sont
> majoritairement des **headers de spécification sans `.cpp`** (algorithmes lourds déclarés,
> non implémentés), parfois avec des **incohérences de compilation** signalées par page.
> Considérez-les comme une API cible. Chaque page le précise élément par élément.

- **Namespace racine** : `nkentseu` (la plupart des sous-systèmes ECS vivent dans
  `nkentseu::ecs` ; renderer dans `nkentseu::renderer` ; mesh procédural dans `nkentseu::proc`).
- **Header parapluie** : `#include "Noge/Nkentseu.h"` (inclut le Core + l'ECS gameplay ;
  n'inclut **pas** `NkEngineLayer`).

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Lancer une application : boucle, fenêtre, couches, événements, profilage | [Core](Noge/Core.md) |
| Attacher des données aux entités (transform, rendu, audio, physique, UI…) | [Composants ECS](Noge/Components.md) |
| Manipuler des objets de jeu : GameObject, acteurs, behaviours, factory | [Entités](Noge/Entities.md) |
| Organiser un niveau : graphe de scène, gestionnaire, prefabs, cycle de vie | [Scène](Noge/Scene.md) |
| Faire tourner la logique : rendu, transform, sérialisation, réflexion | [Systèmes ECS](Noge/Systems.md) |
| Écrire des scripts (C++ natif, DLL hot-reload, C#, Python) | [Scripting](Noge/Scripting.md) |
| Programmation visuelle (blueprint) et réplication réseau | [Visual scripting & réseau](Noge/VisualScript.md) |
| Modéliser en 3D : mesh éditable, modificateurs, booléens, sculpt, UV | [Modélisation](Noge/Modeling.md) |
| Animer : locomotion, anim 2D, IK, séquenceur, foule, facial | [Animation](Noge/Animation.md) |
| Dessiner en 2D : calques, raster, vectoriel, documents, couleur, texte | [Design 2D](Noge/Design2D.md) |
| Importer / exporter des assets 3D et 2D (FBX, glTF, OBJ, SVG) | [Import/Export](Noge/IO.md) |
| Outils d'éditeur : gizmo, picking GPU, caméra, sélection | [Viewport](Noge/Viewport.md) |
| Simuler tissu, cheveux, soft-body, ragdoll, mocap, jiggle bones | [Physique](Noge/Physics.md) |

---

## Aperçu des familles

- **Core** (`Core/…`, `Nkentseu.h`) — `NkApplication` (cycle de vie + boucle bloquante),
  `NkApplicationConfig`, point d'entrée `NkMainApp` (`nkmain`), `NkLayer`/`NkOverlay`/
  `NkLayerStack` (couches possédées par la stack), `NkEventBus` (pub/sub typé, zéro-alloc),
  `NkProfiler`. Deux `NkEngineLayer` coexistent (Core legacy vs `Layers/` v2 active).
- **Composants ECS** (`ECS/Components/…`) — composants POD gameplay : core (transform, tag,
  name, layer), animation (mesh/squelette/animator/IK), audio, physique 2D/3D, rendu (caméra,
  light, sprite, particles), scène (`NkSceneComponent` style Unreal), UI complète (canvas,
  bouton, slider, texte, HUD, UI 3D). Plus les handles `NkComponentHandle`/`NkOptional`/
  `NkRequired` et le registre de sérialisation.
- **Entités** (`ECS/Entities/…`, `ECS/Factory/…`) — `NkGameObject` (handle léger style Unity),
  hiérarchie `NkActor → NkPawn → NkCharacter` (style Unreal), `NkBehaviour` (script attaché) +
  ses 3 systèmes, `NkGameObjectFactory` (création + invariants).
- **Scène** (`ECS/Scene/…`, `ECS/Prefab/…`) — `NkSceneGraph`, `NkSceneManager` (chargement +
  transitions), systèmes de cycle de vie, `NkSceneScript`, `NkPrefab` + registre.
- **Systèmes ECS** (`ECS/Systems/…`) — `NkRenderSystem` (pont ECS→NKRenderer), `NkTransformSystem`
  (propagation hiérarchique DFS), `NkSceneSerializer` (JSON `.nkscene`), `NkReflectComponents`
  (réflexion pour l'InspectorPanel).
- **Scripting** (`ECS/Scripting/…`) — `NkScriptComponent` natif + 3 systèmes, pont DLL
  hot-reload (`NkScriptBridge`), backends C# (Mono/CoreCLR) et Python (CPython). Backends
  externes gardés par macros (no-op sans).
- **Visual scripting & réseau** (`ECS/VisualScript/…`, `ECS/Replication/…`) — `NkBlueprint`
  (graphe de nœuds), hot-reload, validation de graphe, `NkNetWorld` (réplication). **Spec.**
- **Modélisation** (`Modeling/…`, `Topology/…`, `Sculpt/…`, `UV/…`) — `NkEditableMesh`, stack
  de modificateurs, mesh procédural, undo stack, booléens BSP, half-edge, sculpt, éditeur UV.
- **Animation** (`Anim/…`, `Anim2D/…`, `Rigging/…`, `Sequencer/…`, `Crowd/…`, `Facial/…`) —
  locomotion/motion-matching, atlas 2D + tween, solveurs IK, séquenceur multi-piste, foule,
  rig facial FACS + matériaux peau.
- **Design 2D** (`Design/…`, `Doc/…`, `Color/…`, `Text/…`) — pile de calques, canvas raster
  tile-based, chemin vectoriel, documents (hybride/vectoriel), gestion colorimétrique, texte
  sur chemin.
- **Import/Export** (`IO/…`) — `NkFBXImporter`, `NkGLTFIO` (prioritaire), `NkOBJIO`, `NkSVGIO`.
- **Viewport** (`Viewport/…`, `Selection/…`) — gizmo de transformation, picking GPU par
  color-ID, caméra éditeur orbit/fly, masque de sélection raster.
- **Physique** (`Physics/…`, `Systems/NkPhysicsSystems.h`) — cloth (PBD/Verlet), cheveux,
  soft-body, ragdoll, mocap (BVH), jiggle bones, blend-shape controller + leurs systèmes.

---

## Index des familles de headers

| Dossier | Contenu | Documenté dans |
|---------|---------|----------------|
| `Noge/Nkentseu.h` | Parapluie moteur (Core + ECS gameplay). | [Core](Noge/Core.md) |
| `Core/NkApplication*.h`, `NkMainApp.h`, `NkLayer*.h`, `NkEventBus.h`, `NkProfiler.h`, `NkEngineLayer.h` | Framework applicatif : boucle, config, couches, bus d'événements, profiler. | [Core](Noge/Core.md) |
| `Layers/NkEngineLayer.h` | `NkEngineLayer` v2 (ECS + renderer). | [Core](Noge/Core.md) |
| `ECS/Components/…` | Handles + registre + composants core/anim/audio/physics/rendering/scene/UI. | [Composants ECS](Noge/Components.md) |
| `ECS/Entities/…`, `ECS/Factory/…` | `NkGameObject`, `NkActor`/`NkPawn`/`NkCharacter`, `NkBehaviour`, factory. | [Entités](Noge/Entities.md) |
| `ECS/Scene/…`, `ECS/Prefab/…` | Graphe de scène, manager, lifecycle, script, prefab. | [Scène](Noge/Scene.md) |
| `ECS/Systems/…` | Rendu, transform, sérialisation de scène, réflexion. | [Systèmes ECS](Noge/Systems.md) |
| `ECS/Scripting/…` | Script natif/DLL/C#/Python + systèmes. | [Scripting](Noge/Scripting.md) |
| `ECS/VisualScript/…`, `ECS/Replication/…` | Blueprint, hot-reload, validation, `NkNetWorld`. | [Visual scripting & réseau](Noge/VisualScript.md) |
| `Modeling/…`, `Topology/…`, `Sculpt/…`, `UV/…` | Mesh éditable, modificateurs, procédural, undo, booléens, half-edge, sculpt, UV. | [Modélisation](Noge/Modeling.md) |
| `Anim/…`, `Anim2D/…`, `Rigging/…`, `Sequencer/…`, `Crowd/…`, `Facial/…` | Locomotion, atlas/tween 2D, IK, séquenceur, foule, facial. | [Animation](Noge/Animation.md) |
| `Design/…`, `Doc/…`, `Color/…`, `Text/…` | Calques, raster, vectoriel, documents, couleur, texte. | [Design 2D](Noge/Design2D.md) |
| `IO/…` | FBX, glTF/GLB, OBJ/MTL, SVG. | [Import/Export](Noge/IO.md) |
| `Viewport/…`, `Selection/…` | Gizmo, selection buffer, caméra de viewport, sélection raster. | [Viewport](Noge/Viewport.md) |
| `Physics/…`, `Systems/NkPhysicsSystems.h` | Cloth/hair/softbody/ragdoll/mocap/jiggle + systèmes. | [Physique](Noge/Physics.md) |

---

[← Couche Engine](README.md) · [Index du wiki](../README.md)
