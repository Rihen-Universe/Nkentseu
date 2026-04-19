# Architecture Nkentseu — Moteur, Éditeur & Patient Virtuel 3D Emotif

> Document de référence — Nken / 2026  
> Framework : Nkentseu · Éditeur : Unkeny · Application cible : PV3DE

---

## Table des matières

1. [Vue d'ensemble](#1-vue-densemble)
2. [Couches du moteur Nkentseu](#2-couches-du-moteur-nkentseu)
3. [Framework Application — Nkentseu/Core](#3-framework-application--nkentseucore)
4. [Éditeur — Unkeny](#4-éditeur--unkeny)
5. [Patient Virtuel 3D Emotif (PV3DE)](#5-patient-virtuel-3d-emotif-pv3de)
6. [Système ECS — NKScene](#6-système-ecs--nkscene)
7. [Ordre d'implémentation](#7-ordre-dimplémentation)
8. [Conventions de code](#8-conventions-de-code)

---

## 1. Vue d'ensemble

Le projet se décompose en trois niveaux indépendants mais liés :

```
┌─────────────────────────────────────────────────────────────┐
│  Applications cibles                                        │
│  ┌──────────────────┐   ┌──────────────────────────────┐   │
│  │  Unkeny (éditeur)│   │  PV3DE (patient virtuel)     │   │
│  └──────────────────┘   └──────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Nkentseu — Application Framework                           │
│  Application · LayerStack · EventBus · NKScene ECS          │
├─────────────────────────────────────────────────────────────┤
│  Services moteur                                            │
│  NKFont · NKImage · NKAudio · NKPhysics · NKAnimation       │
│  NKScript · NKUI · NKRenderer                               │
├─────────────────────────────────────────────────────────────┤
│  NKRHI — Abstraction GPU                                    │
│  OpenGL · Vulkan · DX11/12 · Metal · Software · WebGL       │
├─────────────────────────────────────────────────────────────┤
│  Bibliothèques de base (zero-STL)                           │
│  NKCore · NKMath · NKTime · NKMemory · NKContainers         │
│  NKStream · NKThreading · NKLogger                          │
├─────────────────────────────────────────────────────────────┤
│  NKPlatform · NKWindow · NKEvent                            │
├─────────────────────────────────────────────────────────────┤
│  OS / Matériel                                              │
│  Windows · Linux · macOS · Android · iOS · Web · Xbox       │
└─────────────────────────────────────────────────────────────┘
```

**Principe fondamental** : chaque couche ne connaît que les couches situées en dessous d'elle. `PV3DE` ne dépend pas de `Unkeny`. `Unkeny` ne dépend pas de `PV3DE`. Les deux dépendent de `Nkentseu/Core`.

---

## 2. Couches du moteur Nkentseu

### 2.1 NKPlatform · NKWindow · NKEvent

| Module | Rôle |
|--------|------|
| `NKPlatform` | Détection compile-time de la plateforme, macros `NKENTSEU_PLATFORM_*` |
| `NKWindow` | Abstraction fenêtre native, `NkWindowConfig`, `NkMain.h` / `NkEntry.h` |
| `NKEvent` | Système d'événements typés : `NkWindowCloseEvent`, `NkKeyPressEvent`, `NkMouseMoveEvent`, etc. |

Point d'entrée : `int nkmain(const NkEntryState& state)` — abstraction cross-platform du `main()` natif.

### 2.2 Bibliothèques de base

Toutes **sans dépendance externe**, sans STL.

| Module | Rôle |
|--------|------|
| `NKCore` | Types de base (`nk_uint32`, `nk_float32`…), assert, macros export |
| `NKMath` | `NkVec2/3/4f`, `NkMat4f`, `NkQuat`, angles, interpolations |
| `NKTime` | `NkClock`, `NkChrono`, delta time, sleep |
| `NKMemory` | Allocateurs custom, arena, pool |
| `NKContainers` | `NkVector`, `NkHashMap`, `NkString`, `NkStringView` |
| `NKStream` | Lecture/écriture binaire et texte |
| `NKThreading` | Threads, mutexes, atomiques, job system |
| `NKLogger` | Logger typé, niveaux, sinks |

### 2.3 NKRHI — Abstraction GPU

Interface `NkIDevice` unique, backends swappables à l'exécution.

```
NkIDevice
  ├── NkOpenGLDevice
  ├── NkVulkanDevice
  ├── NkDX11Device
  ├── NkDX12Device
  ├── NkMetalDevice
  └── NkSoftwareDevice
```

Objets gérés via handles opaques : `NkTextureHandle`, `NkBufferHandle`, `NkPipelineHandle`, `NkShaderHandle`, `NkRenderPassHandle`, `NkFramebufferHandle`, `NkCommandBufferHandle`.

Flux standard par frame :
```
BeginFrame → BeginRenderPass → SetViewport → BindPipeline
→ BindDescriptors → Draw → EndRenderPass → SubmitAndPresent → EndFrame
```

### 2.4 Services moteur

| Module | Rôle principal |
|--------|----------------|
| `NKFont` | Parser TTF zero-copy, rasteriseur scanline, SDF/MSDF, atlas LRU |
| `NKImage` | Chargement PNG/JPG/BMP/TGA, conversion formats |
| `NKAudio` | PCM, spatialisé 3D, streaming, HRTF |
| `NKPhysics` | Rigidbody, collision AABB/OBB/sphere/capsule, raycast |
| `NKAnimation` | Skeletal animation, blend trees, IK |
| `NKScript` | Scripting Lua embarqué, binding C++ → script |
| `NKScene` | ECS (Entity Component System) — voir §6 |
| `NKUI` | Widgets, dock manager, layout stack, thème |

---

## 3. Framework Application — Nkentseu/Core

### 3.1 Classe de base `Application`

```
Application (abstraite)
  ├── virtual void OnInit()          — init des systèmes applicatifs
  ├── virtual void OnStart()         — après init, avant première frame
  ├── virtual void OnUpdate(float dt) — logique, fixedUpdate si nécessaire
  ├── virtual void OnRender()        — soumet les commandes GPU
  ├── virtual void OnEvent(NkEvent*) — reçoit les événements non consommés
  ├── virtual void OnStop()          — arrêt demandé (fenêtre fermée, ESC…)
  └── virtual void OnShutdown()      — libération des ressources
```

La boucle principale est **dans le framework**, pas dans l'application :

```
nkmain(state)
  └── config = BuildConfig(state)
      └── app = CreateApplication(config)  ← défini par l'utilisateur
          ├── app->OnInit()
          ├── app->OnStart()
          ├── while(running):
          │     PollEvents()
          │     accumulator += dt
          │     while(accumulator >= fixedStep): OnFixedUpdate(fixedStep)
          │     OnUpdate(dt)
          │     OnRender()
          │     SwapBuffers()
          ├── app->OnStop()
          └── app->OnShutdown()
```

### 3.2 `NkApplicationConfig`

```cpp
struct NkApplicationConfig {
    NkString        appName         = "NkApp";
    NkString        appVersion      = "1.0.0";
    NkLogLevel      logLevel        = NkLogLevel::NK_INFO;
    bool            debugMode       = false;
    float           fixedTimeStep   = 1.f / 60.f;  // 60 Hz physique
    NkEntryState    entryState;
    NkWindowConfig  windowConfig;
    NkDeviceInitInfo deviceInfo;
    NkString        appFileConfigPath;
};
```

### 3.3 `LayerStack`

Permet d'empiler des `Layer` (logique) et des `Overlay` (UI, debug) :

```
LayerStack
  ├── Layer "GameLayer"     → OnUpdate, OnRender, OnEvent (consomme ou propage)
  ├── Layer "PhysicsLayer"  → OnFixedUpdate
  ├── Overlay "UILayer"     → OnRender (rendu après scène)
  └── Overlay "DebugLayer"  → OnRender (stats, gizmos)
```

Les événements traversent la stack **de haut en bas** (Overlays d'abord).  
Les updates traversent la stack **de bas en haut** (Layers d'abord).

### 3.4 `EventBus`

Système publish/subscribe typé, sans allocation dynamique dans le chemin critique :

```cpp
// Souscription
EventBus::Subscribe<NkKeyPressEvent>(this, &MyLayer::OnKeyPress);

// Publication (depuis NKEvent)
EventBus::Dispatch(keyEvent);
```

---

## 4. Éditeur — Unkeny

### 4.1 Vue d'ensemble

`Unkeny` est une `Application` spécialisée. Elle ne réimplémente pas la boucle, elle ajoute des **couches éditeur** au-dessus du moteur.

```
UnkenyApp : Application
  └── LayerStack
        ├── EditorLayer       — orchestration des panels
        ├── ViewportLayer     — rendu scène dans FBO offscreen
        └── UILayer (NKUI)    — panels dockables
```

### 4.2 Panels

| Panel | Responsabilité |
|-------|----------------|
| `ViewportPanel` | Affiche le FBO de la scène comme texture NKUI. Gizmos de transform (translate, rotate, scale). Camera orbit/fly. |
| `SceneTreePanel` | Arbre hiérarchique des entités ECS. Drag & drop, renommage, activation/désactivation. |
| `InspectorPanel` | Affiche et édite les composants de l'entité sélectionnée. Réflexion des champs via macros `NK_REFLECT`. |
| `AssetBrowser` | Filesystem du projet. Thumbnails PNG/mesh. Double-clic = import dans la scène. |
| `ConsolePanel` | Sortie du `NKLogger`, filtres par niveau, recherche. |
| `DiagnosticPanel` | Spécifique PV3DE — affiche l'état émotionnel et le diagnostic courant. |

### 4.3 Systèmes transversaux éditeur

| Système | Rôle |
|---------|------|
| `CommandHistory` | Pattern Command, undo/redo illimité sur toutes les mutations de scène |
| `ProjectManager` | Gestion des fichiers `.nkproj`, sérialisation JSON de la scène |
| `AssetManager` | Cache des assets chargés, handles typés, hot-reload |
| `PluginSystem` | Chargement de `.dll`/`.so` d'extensions à chaud |
| `SelectionManager` | Entité sélectionnée courante, multi-sélection |
| `GizmoSystem` | Rendu et interaction des handles de transform 3D |

### 4.4 Pipeline de rendu éditeur

```
EditorLayer::OnRender()
  ├── [Viewport FBO] ViewportLayer::Render(scene, editorCamera)
  │     ├── RenderOpaques()
  │     ├── RenderTransparents()
  │     ├── RenderGizmos()
  │     └── Blit FBO → NkTexture (affiché dans ViewportPanel)
  └── [Swapchain] UILayer::Render()
        └── NKUI: DockManager → Panels → Submit GPU
```

---

## 5. Patient Virtuel 3D Emotif (PV3DE)

### 5.1 Objectif

Simuler un patient humain 3D capable d'**exprimer des émotions** et des **symptômes** de façon réaliste, pour assister un médecin dans le **diagnostic différentiel** en temps réel. Le système répond à des interrogations (voix ou texte), anime le visage et le corps en conséquence, et propose un rapport clinique.

### 5.2 Vue système

```
Entrées cliniques
  ├── Symptômes (JSON / API médicale)
  ├── Constantes vitales (FC, SpO2, température, PA)
  ├── Antécédents / allergies
  └── Requête voix médecin (NLP)
          │
          ▼
NKDiagnostic ──► NKEmotion ──► NKFace ──► NKPatientRenderer
                 NKSpeech ──►──────────►
NKBody ◄──── NKEmotion
          │
          ▼
Interface médecin
  ├── Viewport 3D patient
  ├── Panel diagnostic différentiel
  ├── Rapport clinique
  └── Export FHIR
```

### 5.3 Module NKDiagnostic — Cerveau clinique

Responsabilité : à partir des entrées cliniques, produire un **état clinique** qui pilote les modules visuels.

```
NKDiagnosticEngine
  ├── NkSymptomDatabase       — BDD symptômes (JSON, ~500 entrées initiales)
  ├── NkDiseaseDatabase       — BDD pathologies avec critères diagnostiques
  ├── NkDifferentialScorer    — score de vraisemblance par pathologie
  ├── NkClinicalStateOutput   — état de sortie consommé par NKEmotion/NKFace
  └── NkCaseLoader            — chargement de cas cliniques prédéfinis (.nkcase)
```

**État clinique de sortie** :

```cpp
struct NkClinicalState {
    float painLevel;          // 0–10 (échelle EVA)
    float nauseaLevel;        // 0–1
    float fatigueLevel;       // 0–1
    float anxietyLevel;       // 0–1
    float breathingDifficulty;// 0–1
    bool  isConscious;
    NkVector<NkSymptomId> activeSymptoms;
    NkVector<NkDiagnosisEntry> differentialRanking;
};
```

### 5.4 Module NKEmotion — FSM émotionnel

Machine à états finis pilotée par le `NkClinicalState`.

```
États émotionnels :
  Neutre ──► Inconfort ──► Douleur_Légère ──► Douleur_Sévère
  Neutre ──► Anxieux ──► Panique
  Neutre ──► Nauséeux
  Neutre ──► Épuisé
  Neutre ──► Confusion

Chaque état porte :
  - intensité : float [0,1]
  - Action Units FACS actifs (ex: Douleur_Sévère → AU4+6+7+9+10+25+43)
  - Posture corporelle cible
  - Paramètres vocaux (rythme, ton, souffle)
  - Durée de transition (blend time)
```

### 5.5 Module NKFace — Moteur facial FACS

Implémente le **Facial Action Coding System** de Ekman & Friesen.

```
NkFaceController
  ├── NkActionUnit[46]        — 46 AU standards (AU1 à AU46)
  ├── NkBlendshapeMap         — mapping AU → blendshapes du mesh 3D
  ├── NkMicroExpressionSystem — expressions involontaires (32–200ms)
  ├── NkBlinkController       — clignement spontané (3–7/min) + réflexe
  ├── NkGazeController        — direction regard, saccades, fixation
  └── NkFaceSolver            — combine AU → poids blendshapes finaux
```

**Action Units essentielles pour la douleur** :
- AU4 (froncement sourcils internes), AU6 (pommettes), AU7 (tension paupières), AU9 (plissement nez), AU10 (lèvre sup), AU25 (ouverture bouche), AU43/46 (fermeture yeux).

**Action Units pour l'anxiété** :
- AU1+2 (sourcils relevés), AU5 (yeux grands ouverts), AU20 (étirement lèvres), AU26 (mâchoire tendue).

### 5.6 Module NKBody — Animation corporelle

```
NkBodyController
  ├── NkSkeletonInstance      — instance de squelette 65 os (standard médical)
  ├── NkBreathController      — animation de respiration (amplitude, rythme)
  │     ├── Normal (16/min, faible amplitude)
  │     ├── Dyspnée (>20/min, grande amplitude)
  │     └── Apnée / Cheynes-Stokes
  ├── NkPostureBlender        — blend entre postures (assis, allongé, recroquevillé)
  ├── NkTremorController      — tremblements (Parkinson, fièvre, choc)
  └── NkIKSolver              — IK mains/pieds pour postures adaptatives
```

### 5.7 Module NKSpeech — Parole & synchronisation labiale

```
NkSpeechEngine
  ├── NkTTSInterface          — interface vers moteur TTS (embarqué ou API)
  ├── NkVisemeMapper          — phonèmes → visèmes (15 visèmes standard)
  ├── NkLipSyncController     — applique visèmes → AU25/26/27 + blendshapes
  ├── NkVoiceModulator        — modifie la voix selon l'état (souffle, tremblement)
  └── NkResponseGenerator     — génère les réponses aux questions du médecin
```

Exemples de réponses générées depuis la BDD :
- *Douleur thoracique*: "J'ai une douleur ici… qui irradie dans le bras gauche."
- *Dyspnée*: "J'ai… du mal à respirer surtout… quand je marche."

### 5.8 Module NKPatientRenderer — Rendu 3D réaliste

```
NkPatientRenderer
  ├── NkSkinShader            — PBR + subsurface scattering (SSS)
  ├── NkEyeShader             — réfraction cornée, iris procédural, larmes
  ├── NkHairSystem            — simulation cheveux (strand-based)
  ├── NkClothSimulator        — simulation vêtement patient
  └── NkBlendshapeMeshDriver  — applique les poids blendshapes au mesh GPU
```

### 5.9 Montage ECS du patient

L'entité "Patient" dans la scène porte :

```
Entity "Patient"
  ├── TransformComponent          — position, rotation, scale
  ├── MeshComponent               — mesh corps + mesh tête (blendshapes)
  ├── SkeletonComponent           — instance squelette animé
  ├── FaceComponent               → NkFaceController
  ├── EmotionComponent            → NKEmotion FSM
  ├── BodyComponent               → NkBodyController
  ├── DiagnosticComponent         → NKDiagnostic state
  ├── SpeechComponent             → NkSpeechEngine
  └── PatientRendererComponent    → NkPatientRenderer
```

### 5.10 Interface médecin

```
PatientVirtualApp (Application)
  └── LayerStack
        ├── PatientLayer          — update NKDiagnostic + NKEmotion + NKFace
        ├── ViewportLayer         — rendu 3D patient (perspective ou orthographique)
        └── MedicalUILayer        — NKUI : formulaire symptômes, panel différentiel
              ├── SymptomInputPanel   — saisie symptômes / constantes
              ├── DiagnosticPanel     — classement probabiliste des pathologies
              ├── PatientStatePanel   — état émotionnel courant en temps réel
              └── ReportPanel         — génération rapport FHIR / PDF
```

---

## 6. Système ECS — NKScene

### 6.1 Architecture

```
NkScene
  ├── NkEntityRegistry        — gestion des entités (IDs, génération)
  ├── NkComponentStorage<T>   — stockage contiguous (archetype ou sparse set)
  ├── NkSystemRegistry        — systèmes enregistrés avec leurs dépendances
  ├── NkSceneSerializer       — JSON / binaire NK
  └── NkSceneHierarchy        — parent/enfant, transform hérité
```

### 6.2 Pattern d'usage

```cpp
// Création d'entité
NkEntity e = scene.CreateEntity("Patient");

// Ajout de composants
scene.AddComponent<TransformComponent>(e, {pos, rot, scale});
scene.AddComponent<MeshComponent>(e, meshHandle);
scene.AddComponent<FaceComponent>(e);

// Itération dans un système
scene.ForEach<TransformComponent, MeshComponent>(
    [](NkEntity e, TransformComponent& t, MeshComponent& m) {
        // logique rendu
    });
```

### 6.3 Systèmes standards

| Système | Phase | Dépendances |
|---------|-------|-------------|
| `TransformSystem` | Update | — |
| `PhysicsSystem` | FixedUpdate | Transform |
| `AnimationSystem` | Update | Skeleton, Transform |
| `FaceSystem` | Update | Face, Emotion |
| `EmotionSystem` | Update | Diagnostic |
| `SpeechSystem` | Update | Speech, Face |
| `RenderSystem` | Render | Transform, Mesh, Material |

---

## 7. Ordre d'implémentation

### Phase 1 — Application Framework (maintenant)
- `Nkentseu/Core/Application.h` / `.cpp` — classe de base + boucle
- `Nkentseu/Core/Layer.h` — interface `Layer`
- `Nkentseu/Core/LayerStack.h` / `.cpp` — gestion de la pile
- `Nkentseu/Core/EventBus.h` — publish/subscribe typé
- `Nkentseu/Core/NkApplicationConfig.h` — mis à jour
- `Unkeny/src/Unkeny/UnkenyApp.h` / `.cpp` — application éditeur
- `Unkeny/src/Unkeny/EditorLayer.h` / `.cpp` — couche éditeur squelette

### Phase 2 — ECS minimal
- `NKScene/Entity.h` · `Component.h` · `System.h`
- `NKScene/NkScene.h` / `.cpp`
- `NKScene/Components/TransformComponent.h`
- `NKScene/Components/MeshComponent.h`

### Phase 3 — Viewport éditeur
- `ViewportLayer` avec FBO offscreen
- `ViewportPanel` affichant la texture dans NKUI
- Caméra éditeur (orbit + fly)
- Gizmo de sélection (raycasting)

### Phase 4 — Panels éditeur
- `SceneTreePanel`
- `InspectorPanel` avec réflexion basique
- `AssetBrowser`
- `CommandHistory`

### Phase 5 — PV3DE bootstrap
- `NKDiagnostic` (moteur de règles, BDD JSON)
- `NKEmotion` (FSM, 6 états)
- `NKFace` (46 AU, mapping blendshapes)
- `NKBody` (respiration + postures de base)
- `PatientVirtualApp` + `PatientLayer`

### Phase 6 — PV3DE complet
- `NKSpeech` (TTS + visèmes)
- `NKPatientRenderer` (PBR skin, eye shader)
- `MedicalUILayer` (interface médecin)
- Export rapport FHIR / PDF

---

## 8. Conventions de code

### Nommage
- Types/classes : `NkPascalCase` (ex: `NkFaceController`)
- Méthodes : `PascalCase` (ex: `OnUpdate`)
- Membres privés : `mCamelCase` (ex: `mLayerStack`)
- Membres publics : `camelCase` (ex: `painLevel`)
- Macros : `NKENTSEU_UPPER_SNAKE` (ex: `NKENTSEU_PLATFORM_WINDOWS`)
- Types primitifs : `nk_uint32`, `nk_float32`, `nk_bool`, etc.

### Includes
```cpp
// Ordre : plateforme → core → module courant → local
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NKScene/NkScene.h"
#include "MyLocalFile.h"
```

### Gestion mémoire
- Pas de `std::`, pas de `new`/`delete` raw — utiliser les allocateurs NK.
- Préférer valeurs et handles à des pointeurs nus.
- RAII via destructeurs des wrappers NK.

### Sérialisation
- Format projet : JSON (`.nkproj`, `.nkscene`, `.nkcase`)
- Format binaire rapide : `.nkb` (assets compilés)
- Cas cliniques PV3DE : `.nkcase` (JSON étendu)

---

*Dernière mise à jour : 2026 — Nken / Rihen*
