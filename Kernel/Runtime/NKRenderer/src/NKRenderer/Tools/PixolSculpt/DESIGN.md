# PixolSculpt — sculpt en espace-écran (pixol / 2.5D), compute-driven

> ⚠️ **Statut : squelette.** Rien ne tourne en production. Ce dossier pose
> l'architecture, la structure et les stubs ; l'implémentation et les tests
> viendront plus tard. Aucun `.cpp` n'est encore branché dans `NkRendererImpl`.

## 1. Idée directrice

Inspiré de l'approche **pixol / 2.5D de ZBrush**. Le chemin n'est *pas* « plus de
polygones » : on maintient un **G-buffer « pixol »** en espace-écran
(profondeur + normale + matériau + masque + couleur) et on le **mute via des
dispatchs compute bornés à la tuile sous la brosse**.

- Un **pixol** = un pixel qui porte Z + normale + matériau. C'est, à la lettre,
  un G-buffer — sauf qu'il est **écrit par l'utilisateur** (la brosse) au lieu
  d'être généré chaque frame par la rasterisation.
- Le **coût est borné par la résolution écran**, pas par le nombre de triangles.
  Un coup de brosse = un `Dispatch2D` sur le *dirty rect* (l'union des tuiles
  touchées), pas sur l'écran entier.
- Le **resolve** recopie le pixol dans le G-buffer deferred existant, pour que la
  passe de lighting (`NkDeferredPass`) l'éclaire **sans aucune autre modif**.

## 2. Pourquoi ça s'intègre proprement à NKRHI / NKRenderer (vérifié dans le code)

- **RHI** : compute déjà câblé de bout en bout — `CreateComputePipeline`,
  `BindComputePipeline`, `Dispatch`, `UAVBarrier`, storage images/buffers, sur
  tous les backends. Validé surtout **Vulkan + OpenGL** (les cibles éprouvées).
- **OpenGL** : `UAVBarrier`/`Barrier` émet un **vrai** `glMemoryBarrier(...)`
  (bits corrects : `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT`,
  `GL_SHADER_STORAGE_BARRIER_BIT`). Le device **exige GL 4.3+** au démarrage.
- **RenderGraph** : `AddComputePass(...)`, `NkPassType::NK_COMPUTE`,
  `Reads()/WritesStorage()`, et **insertion automatique des barriers** entre
  passes (`InsertBarriers` : storageWrites → `NK_UNORDERED_ACCESS`, reads →
  `NK_SHADER_READ`). Donc rien à synchroniser à la main entre Brush et Resolve.
- **NkSL est non fonctionnel** → les kernels sont en **GLSL** (`.comp.glsl`),
  compilés en SPIR-V via `NkShaderConverter::GlslToSpirv`. **Ne pas** passer par
  NkSL.

## 3. Structure du dossier

```
Tools/PixolSculpt/
├── NkSculptTypes.h         Enums (modes, falloff), config, formats, stats, dirty rect
├── NkSculptBrush.h         NkSculptBrush / NkSculptDab / NkSculptBrushGPU (push consts)
├── NkPixolBuffer.{h,cpp}   Le canvas pixol : storage images écran + import graph
├── NkSculptStroke.{h,cpp}  Trace : dabs interpolés + dirty rect (working set borné)
├── NkSculptPipelines.{h,cpp} Registre des pipelines compute (1/mode + resolve)
├── NkPixolSculptSystem.{h,cpp} Façade : possède tout, s'enregistre au graph
├── NkSculptTileStore.h     FUTUR — voie « vraie géométrie » multirésolution (HD ZBrush)
├── shaders/
│   ├── sculpt_brush.comp.glsl    Kernel de brosse (mute le pixol)
│   └── pixol_resolve.comp.glsl   Composite pixol → G-buffer
└── DESIGN.md               Ce fichier
```

Le glob du `NKRenderer.jenga` (`src/NKRenderer/**.cpp` / `**.h`) compile
automatiquement ce dossier — **aucune modif de build nécessaire**.

## 4. Flux par frame

```
App/Éditeur :  SetBrush() → BeginStroke() → AddStrokeSample()* → EndStroke()
                          │ (accumulés dans NkSculptStroke : dabs + dirty rect)
                          ▼
NkRenderGraph::Execute (déclenché par NkRendererImpl::Present) :
   [PixolSculpt_Brush]   compute  → dispatch des dabs sur le DIRTY RECT
                                    (écrit depth/normal/color, UAVBarrier)
        │  (barrier auto : UNORDERED_ACCESS → SHADER_READ)
        ▼
   [PixolSculpt_Resolve] compute  → pixol → G-buffer (albedo/normal)
        │
        ▼
   [Deferred_Lighting]   compute  → éclaire le G-buffer (passe EXISTANTE)
```

Le `NkSculptStroke` garantit que seules les **tuiles touchées** sont dispatchées,
avec une **borne dure** `maxDabsPerFrame` (anti-explosion).

## 5. Diff d'intégration dans `NkRendererImpl` (à appliquer plus tard)

> Calqué exactement sur les autres sous-systèmes (`NkVFXSystem`,
> `NkSimulationRenderer`…). **Non appliqué** ici : c'est le moteur qui tourne.

### `NkRendererImpl.h`

```cpp
// 1) accesseur (façade) — à déclarer aussi dans NkRenderer.h si exposé publiquement
NkPixolSculptSystem* GetPixolSculpt() { return mPixolSculpt.Get(); }

// 2) membre (ordre = ordre d'init ; après mRender3D/mTextures/mShaders)
memory::NkUniquePtr<class NkPixolSculptSystem> mPixolSculpt;

// 3) helper d'init
bool InitPixolSculpt();
```

### `NkRendererImpl.cpp`

```cpp
// Dans Initialize(), après Render3D (il a besoin de texLib + shaderLib) :
if (mCfg.Has(NK_SS_PIXOL_SCULPT)) {        // nouveau flag, cf. NkSubsystemFlags
    if (!InitPixolSculpt()) return false;
}

bool NkRendererImpl::InitPixolSculpt() {
    if (mPixolSculpt) return true;
    mPixolSculpt.Reset(AllocOwned<NkPixolSculptSystem>());
    if (!mPixolSculpt->Init(mDevice, mRenderGraph.Get(), mTextures.Get(),
                            mShaders.Get(), mCfg.width, mCfg.height)) {
        mPixolSculpt.Reset();
        NkRSetLastError(NkRResult::NK_ERR_UNKNOWN, "NkPixolSculptSystem::Init failed");
        return false;
    }
    return true;
}

// Dans BuildDefaultRenderGraph(), AVANT la passe de lighting deferred :
if (mPixolSculpt) mPixolSculpt->RegisterToRenderGraph();
```

### Flag de sous-système

Ajouter `NK_SS_PIXOL_SCULPT` à `NkSubsystemFlags` (et le câbler dans
`EnableSubsystem`/`DisableSubsystem` + `RebuildRenderGraph`).

## 6. Ce qui reste à implémenter (ordre conseillé)

1. `NkPixolBuffer::CreateTargets` — créer les 5 storage images (Vulkan d'abord).
2. `NkSculptPipelines::LoadCompute` — GLSL → SPIR-V → compute pipeline.
3. `sculpt_brush.comp.glsl` mode `RAISE`/`LOWER` + reconstruction de normale.
4. `NkSculptStroke::AddSample`/`ExpandDirty` — interpolation + dirty rect.
5. `RecordBrushPass` — bind + push constants + `Dispatch` borné + `UAVBarrier`.
6. `pixol_resolve.comp.glsl` + `RecordResolvePass` — composite vers G-buffer.
7. Brancher le diff §5, tester sur **Vulkan puis OpenGL**.
8. (Plus tard) `NkSculptTileStore` pour la vraie géométrie multirésolution.

## 7. Pièges connus

- **Format des storage images en OpenGL** : `glBindImageTexture` est appelé avec
  un format **codé en dur `GL_RGBA32F`** dans le backend GL actuel
  (`NkOpenglDevice.cpp`). À vérifier/étendre si on utilise `r32f`/`rgba16f`/`rgba8`
  comme prévu ici, sinon les `imageStore` typés ne colleront pas.
- **Depth du G-buffer** : souvent une depth-stencil non-storage → le resolve du Z
  peut nécessiter une petite passe graphique plutôt qu'un `imageStore` direct.
- **Async compute** : ne pas l'activer tant que le chemin synchrone n'est pas
  validé (la synchro cross-queue est le plus dur à déboguer).
```
