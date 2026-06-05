# Noge — Roadmap (Engine Framework)

État actuel (mai 2026) : Noge est le **framework Application** au-dessus du
moteur Nkentseu (NKECS, NKRenderer, NKRHI…). Son ambition : fournir une API de
type Unity/Unreal (Application, LayerStack, EventBus, NkGameObject, NkActor,
NkPrefab, NkSceneGraph, NkComponentHandle) pour bâtir Noge (éditeur, codename
Nogee) et PV3DE (Patient Virtuel 3D Emotif).

À ce jour, le **squelette d'Application est en place** : `NkApplication.h/.cpp`,
`NkLayer`, `NkOverlay`, `NkLayerStack`, `NkApplicationConfig`, `NkEventBus`
(table fixe statique 64 types × 32 handlers), `NkProfiler`, `NkMainApp` (point
d'entrée `nkmain`) et `NkEngineLayer` (singleton qui orchestre `NkWorld`,
`NkScheduler`, `NkSceneManager`, `NkRenderer`, `NkRenderSystem`). La couche ECS
gameplay (NkGameObject, NkActor, NkPawn, NkCharacter, NkPrefab, NkSceneGraph,
NkSceneScript, NkSceneManager, NkBehaviour, NkScriptComponent) est largement
déclarée et en partie implémentée.

En revanche, **toute la galaxie design/anim/modeling déclarée dans
`src/Noge/`** (Anim2D, Color, Crowd, Design Raster/Vector, Doc hybride/vector,
Facial, IO FBX/GLTF/OBJ/SVG, Modeling, Rigging IK, Sculpt, Sequencer, Topology
half-edge/boolean, UV editor, Viewport gizmo) est à l'état **headers de
spécification** — riches en commentaires et en API, mais sans `.cpp`
d'implémentation. **Aucune application Noge ni PV3DE n'existe encore** : on
trouve une coquille `Applications/Nogee` (éditeur en gestation) et un dossier
`Applications/PV3DE` réduit à de la documentation.

---

## Synthèse par sous-système

| Sous-système | Statut | Effort restant | Priorité |
|--------------|--------|----------------|----------|
| Core — Application, LayerStack, EventBus, MainApp, Profiler | 🔶 | Stabilisation boucle, ménage `Run()` dupliqué, tests | Haute |
| Core — NkEngineLayer (orchestration ECS+Renderer) | 🔶 | Wiring complet Scheduler/SceneManager, Resize, Shutdown | Haute |
| ECS Gameplay (GameObject, Actor, Pawn, Character) | 🔶 | Implémentations `.cpp`, intégration Behaviour | Haute |
| ECS Composants Core/Rendering/Animation/Physics/Audio/UI | 🔶 | Headers complets, peu d'implémentation système | Haute |
| ECS Prefab + SceneGraph + SceneManager | 🔶 | Lifecycle, transitions, sérialisation | Haute |
| ECS Scripting C++ (NkBehaviour, NkScriptComponent) | 🔶 | NkBehaviourSystem, NkScriptSystem à câbler | Moyenne |
| ECS Scripting Lua / Python / C# (NKScript) | ❌ | Non démarré (placeholders headers) | Basse |
| ECS Visual Scripting (NkBlueprint) | ❌ | Headers seulement | Basse |
| ECS Réplication réseau (NkNetWorld) | ❌ | Header seulement | Basse |
| Sérialisation scène (NkSceneSerializer, ComponentRegistry) | 🔶 | Wiring JSON, tests roundtrip | Haute |
| Anim 3D (NkLocomotion, NkFootIK, NkMotionMatch) | ❌ | Header seulement | Moyenne |
| Anim 2D (NkAtlas2D, NkTween) | ❌ | Headers, pas d'implémentation | Basse |
| Audio components | 🔶 | Headers OK, manque NkAudioSystem | Moyenne |
| Physics components + NkPhysicsSystems | 🔶 | Headers OK, intégration NKPhysics | Moyenne |
| Crowd sim (NkCrowdGrid, NkCrowdManager) | ❌ | Spec seulement | Basse |
| Color (NkColor, NkPalette, NkHarmony) | ❌ | Header seulement | Basse |
| Design Raster (NkRasterCanvas, NkBrushEngine) | ❌ | Spec seulement | Basse |
| Design Vector (NkVectorPath, NkPaint) | ❌ | Spec seulement | Basse |
| Doc (HybridDocument, VectorDocument) | ❌ | Spec seulement | Basse |
| Modeling (EditableMesh, Modifier, ProceduralMesh, UndoStack) | ❌ | Spec seulement | Basse |
| Topology (HalfEdge, BooleanOp) | ❌ | Spec seulement | Basse |
| UV (NkUVEditor) | ❌ | Spec seulement | Basse |
| Sculpt (NkSculpting) | ❌ | Spec seulement | Basse |
| Rigging (NkIKSolver, NkFacialRig) | ❌ | Spec seulement | Moyenne |
| Sequencer (NkSequencer) | ❌ | Spec seulement | Basse |
| Selection (NkSelectionSystem) | ❌ | Spec seulement | Moyenne |
| Viewport (NkGizmo, NkSelectionBuffer, NkViewportCamera) | ❌ | Spec seulement | Haute |
| IO (FBX, GLTF, OBJ, SVG) | ❌ | Spec seulement | Moyenne |
| Text (NkTextPath) | ❌ | Spec seulement | Basse |
| Layer NkEngineLayer (Layers/) | 🔶 | OnAttach/OnUpdate écrits, intégration scheduler à finir | Haute |
| Noge / Nogee (éditeur) | 🔶 | Coquille NogeApp + EditorLayer/ViewportLayer/UILayer ; panels stubs | Haute |
| PV3DE | ❌ | Dossier vide (doc uniquement) | Plus tard |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## Livré

### Core — Application framework

- `NkApplication` (header + `.cpp`) : `Init()`, `Run()`, `Quit()`,
  `PushLayer()`, `PushOverlay()`, singleton `Get()`.
- Boucle dans `Run()` : `PollEvents` → `OnFixedUpdate` (accumulateur sur
  `fixedTimeStep`) → `OnUpdate(dt)` → `BeginFrame` → `OnRender` →
  `OnUIRender` → `SubmitAndPresent` → `EndFrame`. Cap `maxDeltaTime`.
- `NkApplicationConfig` : appName/version, logLevel/debug, `fixedTimeStep`
  (1/60), `maxDeltaTime` (0.25), `targetFPS`, `NkEntryState`,
  `NkWindowConfig`, `NkDeviceInitInfo`, `appFileConfigPath`.
- `NkLayer` / `NkOverlay` : `OnAttach`, `OnDetach`, `OnUpdate`,
  `OnFixedUpdate`, `OnRender`, `OnEvent` (consommation par retour `true`),
  `OnUIRender`.
- `NkLayerStack` : insertion layers à gauche de `mLayerInsertIndex`, overlays
  à droite ; itération update gauche→droite, événements droite→gauche ;
  `delete` automatique des layers à la destruction.
- `NkEventBus` : publish/subscribe **statique zéro-alloc**, table fixe 64 types
  × 32 handlers, stockage inline 64 bytes, deux variantes `Subscribe`
  (functor capturé inline / méthode membre via `memcpy` du pmf), `Dispatch`
  par `StaticTypeId`, `UnsubscribeAll(void*)`, `DispatchRaw`, `Clear`.
- Pont natif `NkEventSystem` → `NkEventBus` câblé dans `InitPlatform()`
  pour close/resize, clavier (press/release/text), souris (move/buttons/
  double-click/wheel V+H).
- `NkProfiler` : singleton `Global()`, markers (Frame/System/Query/Script/
  Custom), stats min/max/avg, `NextFrame()`, export JSON ; macros
  `NK_PROFILE_SCOPE` / `NK_PROFILE_SAMPLE`.
- `NkMainApp.h` : `nkmain(NkEntryState)` qui appelle l'usine utilisateur
  `NkMainApplication(config)`, `Init` → `Run` → `delete`.

### ECS gameplay — surface publique en place

- `Nkentseu.h` agrège tous les headers publics (Application, composants Core/
  Rendering/Animation/Physics/Audio/UI, NkGameObject, NkActor/Pawn/Character,
  NkBehaviour, NkPrefab, NkSceneGraph, NkSceneManager, NkSceneScript,
  NkSceneLifecycleSystem, NkTransformSystem, NkReflectComponents, NkScript*,
  NkBlueprint, NkComponentHandle, NkSerialization/Asset).
- `NkComponentHandle<T>` (24 bytes, world ptr + entity id), résolution
  paresseuse via `NkWorld::Get<T>`, opérateurs `->`/`*`/`bool`.
- `NkOptionalComponent<T>` (never-crash via dummy interne) et
  `NkRequiredComponent<T>` (assertion) pour invariants type `NkTransform`.
- `NkComponentRegistry` (singleton Meyer's) : enregistrement par `NkTypeId`,
  fallback hex-binary, intégration `NkISerializable`, macros
  `NK_REGISTER_COMPONENT` / `_CUSTOM`.
- Headers détaillés et autosuffisants (taille fixe par composant, pas
  d'allocations) pour : `NkStaticMesh`, `NkSkeleton` (256 os), `NkSkeletalMesh`
  (64 blendshapes, 8 matériaux), `NkAnimationClip` (128 props, 512 curves),
  `NkAnimator` (state machine + params + triggers), `NkMeshEditor`, `NkIKChain`
  (FABRIK 16 os), `NkAudioSource`/`NkAudioListener`/`NkAudioMixer`,
  `NkRigidbody2D/3D`, `NkCollider2D/3D`, `NkJoint`, `NkTrigger`, `NkTag`,
  `NkTransform`, `NkCoreComponents`.

### NkEngineLayer — orchestration moteur

- Header en place : tient `NkWorld`, `NkScheduler`, `NkSceneManager`,
  `NkRenderer`, `NkRenderSystem`. Singleton `Get()`.
- `OnAttach()` : `InitRenderer()` → `RegisterCoreSystems()` →
  `mScheduler.Init(mWorld)`.
- API scène : `RegisterScene(name, factory)`, `LoadScene(name, transition)`
  qui enregistre le lifecycle dans le scheduler et appelle `BeginPlay()`.
- `Resize(w, h)` propagé au renderer.

### Éditeur Nogee (coquille Noge)

- `Applications/Nogee` : `NogeApp` hérite de `Application`, expose les
  callbacks standard, possède `EditorLayer`, `ViewportLayer`, `UILayer` et
  un `NkEditorCamera` partagé.
- Panels présents en stubs : `AssetBrowser`, `ConsolePanel`, `InspectorPanel`,
  `SceneTreePanel`.

---

## En cours / TODO immédiat

Cette section suit l'ordre officiel d'`ARCHITECTURE.md §7`.

### Phase 1 — Application Framework (réf. ARCHITECTURE §7.1)

État : **🔶 Quasi livré, à stabiliser.**

- ✅ `Core/NkApplication.h/.cpp`, `NkLayer.h`, `NkLayerStack.h/.cpp`,
  `NkEventBus.h`, `NkApplicationConfig.h`, `NkMainApp.h`.
- ❌ `NkApplication.cpp` contient **deux définitions de `Run()`** (la
  seconde est mort-née, sans boucle) — à fusionner avant tout autre travail.
- ❌ Boucle de detach explicite des layers manquante (la fin de `Run()`
  contient un `for` vide ; le cleanup repose sur le destructeur du
  `NkLayerStack`).
- ❌ Typo `NkkLambdaStorage` (devrait être `kLambdaStorage`) dans
  `NkEventBus::Subscribe` (static_asserts cassés à la compile dès qu'on
  appelle `Subscribe`).
- ❌ `NkApplication::OnEvent` (callback virtuel) déclaré dans ARCHITECTURE.md
  §3.1 mais absent du `.h` actuel — décider : drop ou réintroduire.
- ❌ Tests unitaires LayerStack / EventBus.
- ❌ Surcharge `appFileConfigPath` (chargement JSON/YAML d'override) non
  implémentée.

### Phase 2 — ECS minimal (réf. §7.2)

État : **🔶 Surface complète, implémentations partielles.**

- ✅ Wrappers gameplay `NkGameObject`, `NkActor`, `NkPawn`, `NkCharacter` :
  headers + `.cpp` présents.
- ✅ `NkPrefab`, `NkPrefabRegistry`, `NkPrefabInstance` (header + .cpp).
- ✅ `NkSceneGraph`, `NkSceneManager`, `NkSceneScript`,
  `NkSceneLifecycleSystem` (header + .cpp).
- ✅ `NkTransformSystem`, `NkRenderSystem` (header + .cpp).
- ✅ `NkGameObjectFactory` (header + .cpp).
- 🔶 `NkBehaviour` / `NkBehaviourSystem` : headers seulement, pas de `.cpp`.
- 🔶 `NkScriptComponent` / `NkScriptSystem` : headers seulement.
- 🔶 `NkSceneSerializer` : header présent, `.cpp` à compléter pour le
  format `.nkscene` JSON décrit.
- 🔶 `NkComponentRegistry` : implémentation header-only OK, manque les
  appels `NK_REGISTER_COMPONENT` pour tous les composants du moteur.
- ❌ `NkReflectComponents` : header présent mais boucle de réflexion à
  câbler pour `NkSceneSerializer` (fallback automatique).

### Phase 3 — Viewport éditeur (réf. §7.3)

État : **🔶 Coquille Nogee en place, viewport pas câblé.**

- 🔶 `NogeApp` + `EditorLayer` + `ViewportLayer` + `UILayer` (squelettes
  `Applications/Nogee/`).
- ❌ FBO offscreen pour scène 3D dans `ViewportLayer` — non implémenté.
- ❌ `NkEditorCamera` orbit/fly — header seul, comportement à écrire.
- ❌ `NkGizmo` (translate/rotate/scale 3D) — header `Viewport/NkGizmo.h`
  seulement.
- ❌ `NkSelectionBuffer` (raycast picking via ID-buffer) — header seul.
- ❌ `NkViewportCamera` — header seul.
- ❌ Blit FBO → `NkTexture` affichée dans `ViewportPanel` NKUI — TODO.

### Phase 4 — Panels éditeur (réf. §7.4)

État : **🔶 Stubs présents, pas d'interaction.**

- 🔶 `SceneTreePanel`, `InspectorPanel`, `AssetBrowser`, `ConsolePanel` :
  fichiers présents dans `Applications/Nogee/src/Nogee/Panels/`.
- ❌ Drag & drop scène ; renommage entités ; activation/désactivation.
- ❌ Réflexion `NK_REFLECT` côté Inspector — à brancher sur
  `NkReflectComponents`.
- ❌ `CommandHistory` (undo/redo pattern Command) — pas commencé.
- ❌ `AssetManager` avec cache + hot-reload — pas commencé (les imports
  GLTF/FBX/OBJ/SVG n'ont aussi que des headers).
- ❌ `ProjectManager` (`.nkproj`, sérialisation scène) — non démarré.
- ❌ `PluginSystem` chargement `.dll`/`.so` à chaud — non démarré.
- ❌ `SelectionManager` global — `Selection/NkSelectionSystem.h` est un
  header isolé.
- ❌ `DiagnosticPanel` (spécifique PV3DE) — non démarré.

### Phase 5 — PV3DE bootstrap (réf. §7.5)

État : **❌ Non démarré.**

- ❌ `NKDiagnostic` (NkSymptomDatabase, NkDiseaseDatabase,
  NkDifferentialScorer, NkClinicalState) — absent.
- ❌ `NKEmotion` FSM (Neutre/Inconfort/Douleur/Anxiété/Nausée/Épuisé/
  Confusion) — absent.
- ❌ `NKFace` (46 Action Units FACS, BlendshapeMap, MicroExpression, Blink,
  Gaze, FaceSolver) — absent ; seul `Facial/NkFacialRig.h` et
  `Facial/NkSkinMaterial.h` esquissent la couche bas niveau (sans `.cpp`).
- ❌ `NKBody` (SkeletonInstance 65 os, Breath, Posture, Tremor, IKSolver) —
  absent côté Noge ; `Anim/NkLocomotion.h` esquisse `NkCrowdAgent`/`NkFootIK`.
- ❌ `PatientVirtualApp` + `PatientLayer` — `Applications/PV3DE/src/`
  contient des docs, pas de code C++.

### Phase 6 — PV3DE complet (réf. §7.6)

État : **❌ Non démarré.**

- ❌ `NKSpeech` (TTS, Viseme, LipSync, VoiceModulator, ResponseGenerator).
- ❌ `NKPatientRenderer` (SkinShader SSS, EyeShader, HairSystem,
  ClothSimulator, BlendshapeMeshDriver).
- ❌ `MedicalUILayer` (SymptomInput, DiagnosticPanel, PatientStatePanel,
  ReportPanel).
- ❌ Export FHIR / PDF.

---

## À venir / À ajouter (futur proche)

### Sous-systèmes Noge — headers de spécification à transformer en code

Tous présents en `*.h` riches (commentaires, structs, API publique) mais
**sans `.cpp`** d'implémentation au 2026-05-26 :

- **Anim2D** — `NkAtlas2D` (parsing TexturePacker JSON + LibGDX XML, frames
  nommées, NkAnimation2D), `NkTween` (NkEase 32 fonctions, manager,
  séquences).
- **Anim 3D / Locomotion** — `NkFootIK` (raycast + IK cheville),
  `NkLocomotion`, `NkMotionMatch` (DB de frames + nearest neighbor),
  `NkCrowdAgent`, `NkCrowdGrid`, `NkCrowdManager`.
- **Color** — `NkColor` (sRGB/Linear/HSL/HSV/LAB/LCH/CMYK/XYZ/OKLab/OKLch
  avec espace pivot LinearRGB), `NkPalette`, `NkSwatch`, `NkHarmony`.
- **Design Raster** — `NkRasterCanvas` tile-based 64×64 (Depth8/16/32),
  `NkBrushEngine`, `NkBrushDab`, FlushDirtyTiles GPU.
- **Design Vector** — `NkVectorPath` (commandes SVG, NonZero/EvenOdd),
  `NkPaint` (Solid/Linear/Radial/Conic/Pattern/ImageFill), tessellation vers
  `NkRender2D::FillPolygon`.
- **Document** — `NkVectorDocument` (artboards + layers + objects, JSON +
  SVG), `NkHybridDocument` (NkSymmetryTool radial/mandala, NkPerspectiveGuide
  1/2/3 PdF, NkStabilizer, NkAnimationAssist onion skin, NkTimelapseRecorder).
- **Modeling** — `NkEditableMesh`, `NkMeshModifier`, `NkProceduralMesh`,
  `NkUndoStack`.
- **Topology** — `NkHalfEdge`, `NkBooleanOp` (union/diff/intersect mesh).
- **UV** — `NkUVEditor`.
- **Sculpt** — `NkSculpting` (brosses, multires).
- **Rigging** — `NkIKSolver`, `NkFacialRig`, `NkSkinMaterial`.
- **Sequencer** — `NkSequencer` (tracks d'animation/caméra, NLA, markers,
  camera shots, NkRenderOutput).
- **Selection** — `NkSelectionSystem` (multi-sélection, history).
- **Viewport** — `NkGizmo`, `NkSelectionBuffer`, `NkViewportCamera`.
- **IO** — `NkFBXImporter`, `NkGLTFIO`, `NkOBJIO`, `NkSVGIO`.
- **Text** — `NkTextPath` (texte sur chemin vectoriel).
- **Physics** — `NkPhysicsMesh`, `Systems/NkPhysicsSystems.h`
  (intégration NKPhysics).

### ECS — pièces manquantes

- **NkBehaviourSystem.cpp** + **NkScriptSystem.cpp** (lifecycle MonoBehaviour-
  style : OnAwake/OnStart/OnEnable/OnUpdate/OnLateUpdate/OnFixedUpdate/
  OnDestroy via Scheduler).
- **NkBlueprint** (visual scripting) + `NkBlueprintHotReload` +
  `NkValidGraph` : headers stubs.
- **NkScriptCSharp** (mono/CoreCLR binding) — header stub.
- **NkScriptPython** — header stub.
- **NKScript Lua** (annoncé ARCHITECTURE §2.4) — **non démarré, planifié**.
- **NkNetWorld** (réplication) — header stub.
- **NkSceneSerializer** : finir `Save` / `Load` `.nkscene` JSON + tests
  roundtrip + intégration `NkReflectComponents`.
- **Asset hot-reload + cache** : promu vers
  `NKSerialization/Asset/NkAssetImporter` + `NkAssetMetadata` mais
  AssetManager runtime à finaliser.

### Noge (éditeur — codename Nogee)

- **Statut** : 🔶 coquille (`NogeApp` + EditorLayer/ViewportLayer/UILayer +
  4 panels stubs) ; aucun panel n'est encore fonctionnel.
- À écrire : pipeline FBO scène, NkGizmo 3D, raycast picking, dock manager
  NKUI réel, CommandHistory, ProjectManager `.nkproj`, hot-reload assets.

### PV3DE

- **Statut** : ❌ aucun code C++. `Applications/PV3DE/` contient seulement
  `ARCHITECTURE.md` et `ARCHITECTURE (2).md` + `file.md`.
- À démarrer après Phase 4 (panels éditeur stables) — Phases 5 et 6 de
  l'architecture.

---

## Bugs / quirks connus

- **`NkApplication.cpp` contient deux définitions de `Run()`** : la
  première est la boucle réelle, la seconde recommence un init et ne
  contient pas de boucle. Le linker pickrait la première mais le fichier ne
  compile probablement pas tel quel — à nettoyer en priorité.
- **`NkEventBus::Subscribe` référence `internal::NkkLambdaStorage`** alors
  que la constante interne s'appelle `kLambdaStorage` : tout appel
  `Subscribe` casse à la compile (static_assert). Corriger.
- **`__builtin_memcpy`** utilisé pour copier les pointeurs de méthode dans
  `NkEventBus` : OK sur GCC/Clang/MSVC récents, mais pas portable strict —
  basculer sur `nk::memcpy` du module NKMemory.
- **`NkApplication::Run` boucle de detach des layers vide** (commentée),
  cleanup reposant uniquement sur `~NkLayerStack`.
- **`NkLayerStack::PopLayer`** décrémente `mLayerInsertIndex` même si le
  layer trouvé est en réalité un overlay (situé après l'index) — bug
  potentiel quand on supprime des overlays via `PopLayer`.
- **`NkEngineLayer::sInstance`** : déclaration `static` dans le .h mais la
  définition globale dans le .cpp est commentée — à confirmer.
- **`NkComponentHandle` cache de pointeur** : prévu dans la documentation
  ("invalidé après FlushDeferred"), mais l'implémentation actuelle ne
  cache rien (résolution à chaque appel via `mWorld->Get<T>`).

---

## Dépendances

### Couches en dessous (Noge utilise)

- **Foundation** : `NKCore`, `NKMath`, `NKMemory`, `NKContainers`
  (`NkVector`, `NkString`, `NkUnorderedMap`), `NKLogger`, `NKTime`
  (`NkClock`).
- **System** : `NKPlatform`, `NKWindow` (`NkWindow`, `NkEntry`, `NkEvent`,
  `NkEventSystem`, `NkMain`), `NKEvent` (événements typés), `NKRHI`
  (`NkIDevice`, `NkDeviceFactory`, `NkICommandBuffer`, `NkDeviceInitInfo`),
  `NKSerialization` (`NkArchive`, `NkISerializable`, `NkSchemaVersioning`,
  `NkSerializer`, `NkJSONWriter/Reader`, `Asset/NkAssetImporter`,
  `Asset/NkAssetMetadata`).
- **Runtime** : `NKECS` (`NkWorld`, `NkScheduler`, `NkSystem`,
  `NkECSDefines`, `NkTypeRegistry`), `NKRenderer` (`NkRenderer`, `NkRender2D`
  pour Anim2D/Design, `NkRendererTypes`), à venir : `NKAudio`, `NKPhysics`,
  `NKAnimation`, `NKUI`, `NKScript` Lua, `NKFont`, `NKImage`.

### Applications qui dépendent de Noge

- **Applications/Nogee** (codename éditeur, futur Noge) — `NogeApp :
  public Application`, utilise tous les Layers et Panels Noge.
- **Applications/PV3DE** — placeholder (docs seulement) ; à venir
  `PatientVirtualApp : public Application`.
- **Applications/Sandbox**, **Applications/Pong**, **Applications/NkAudioDemo**,
  **Applications/NkImageDemo**, **Applications/Songoo**, **Applications/Model**,
  **Applications/NKPA** — démos s'appuyant directement sur NKECS/NKRenderer/
  NKRHI sans passer par `NkApplication` (la plupart définissent leur propre
  `nkmain` ou utilisent les démos `DemoNkentseu`).

---

*Dernière mise à jour : 2026-05-26 — audit Noge complet (96 fichiers,
26 sous-systèmes).*
