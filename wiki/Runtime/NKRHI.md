# NKRHI

> Couche **Runtime** · L'interface matérielle de rendu (Render Hardware Interface) :
> une abstraction unique au-dessus de six backends graphiques — Vulkan, OpenGL, DirectX 11,
> DirectX 12, Metal et un rasterizer logiciel — pour créer des ressources GPU, enregistrer des
> commandes de rendu, faire du compute, du machine learning et afficher des outils 3D.

NKRHI est la **frontière entre votre code et le GPU**. Au-dessus de lui, les couches de plus
haut niveau (NKCanvas, NKRenderer, Noge) décrivent *ce qu'elles veulent dessiner* sans jamais
appeler directement Vulkan ou Direct3D ; en dessous, six implémentations traduisent ces
intentions dans l'API native de la plateforme. On écrit son moteur **une seule fois** et il
tourne partout : Windows (VK/DX12/DX11/GL/SW), macOS/iOS (Metal/GL/SW), Linux/Android (VK/GL/SW),
Web (WebGL/WebGPU).

Tout passe par trois objets : un **device** (`NkIDevice`) qui crée et détruit les ressources et
soumet le travail au GPU ; des **descripteurs** (`NkXxxDesc`) qui décrivent immuablement ce que
l'on veut (textures, buffers, pipelines, render passes…) et renvoient un **handle** opaque
64 bits ; et des **command buffers** (`NkICommandBuffer`) qui enregistrent la séquence de
commandes (bind, draw, dispatch, copy) avant soumission. Le device se fabrique via
`NkDeviceFactory` — point d'entrée unique de toute la couche, avec détection runtime du meilleur
backend.

- **Namespace** : `nkentseu` (les shaders ML : `nkentseu::NkMLShaders` ; l'intégration NkSL :
  `nkentseu::nksl` ; le pont software : `nkentseu::swbridge`)
- **Header parapluie** : `#include "NKRHI/NkRHI.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Page |
|--------|------|
| Créer un device, choisir/détecter le backend GPU, gérer le swapchain | [Le device](NKRHI/Device.md) |
| Connaître les types fondamentaux : handles, formats, enums d'état | [Le device](NKRHI/Device.md) |
| Décrire des textures, buffers, samplers, pipelines, render passes, bindings | [Les ressources](NKRHI/Resources.md) |
| Enregistrer des commandes de rendu (bind, draw, dispatch, copy, barrières) | [Commandes & compute](NKRHI/Commands-Compute.md) |
| Faire du calcul GPU générique ou du machine learning (tenseurs, MatMul, Adam…) | [Commandes & compute](NKRHI/Commands-Compute.md) |
| Compiler des shaders NkSL, exécuter un shader logiciel, afficher gizmo/grille 3D | [Shaders & outils](NKRHI/Shaders-Tools.md) |

Chaque page liste l'**API réelle** des headers concernés, avec les structs, enums, signatures
et les **pièges** connus (impl par défaut no-op, double-PushBack, ownership des handles…).

---

## Aperçu des familles

- **Device & fabrique** (`Core/NkIDevice.h`, `Core/NkDeviceFactory.h`) — `NkIDevice` est
  l'interface abstraite qui crée/détruit toutes les ressources, soumet les command buffers et
  gère les frames. `NkDeviceFactory` la fabrique (`Create`, `CreateAutoDetect`,
  `CreateWithFallback`) et la détruit (`Destroy`). On ne fait jamais `new`/`delete` dessus.
- **Configuration** (`Core/NkContextDesc.h`, `Core/NkDeviceInitInfo.h`, `Core/NkGpuPolicy.h`,
  `Core/NkContextInfo.h`) — le descripteur de création (API choisie, options par backend,
  sélection GPU, format de swapchain cross-API), le bloc d'init (surface, taille, callbacks) et
  la remontée d'infos runtime.
- **Types fondamentaux** (`Core/NkTypes.h`, `Core/NkGraphicsApi.h`) — handles opaques typés
  (`NkBufferHandle`, `NkTextureHandle`…), formats GPU (`NkGPUFormat`), tous les enums d'état du
  pipeline fixe (topology, cull, blend, depth, sampler…), viewport/scissor/clear, résultats.
- **Swapchain** (`Core/NkISwapchain.h`) — interface multi-fenêtre optionnelle (`AcquireNextImage`
  → render → `Present`), créée et détruite par le device.
- **Descripteurs de ressources** (`Core/NkDescs.h`) — tous les `NkXxxDesc` immuables : buffers,
  textures, samplers, layouts de vertex, états rasterizer/depth/blend, shaders, render passes,
  framebuffers, pipelines graphique/compute, descriptor sets, barrières, copies, indirect args.
- **Commandes** (`Commands/NkICommandBuffer.h`, `Commands/NkCommandPool.h`) — `NkICommandBuffer`
  enregistre tout le travail GPU ; `NkCommandPool` recycle les command buffers d'une frame à
  l'autre (thread-safe).
- **Compute & ML** (`Core/NkComputeContext.h`, `Core/NkML.h`) — `NkComputeContext` (et son builder
  fluide) pour le calcul GPU générique ; `NkMLContext` pour le deep learning from-scratch
  (tenseurs, MatMul, convolution, activations, attention, optimiseurs Adam/SGD) — 100 % GLSL
  compute, sans CUDA.
- **Shaders** (`SL/NkSLIntegration.h`, `SL/NkSWShaderBridge.h`) — pont vers le **vrai**
  compilateur NkSL (module NKSL) : compilation source/fichier vers le bon backend, reflection ;
  et le bridge qui transforme un shader NkSL en lambdas C++ exécutables par le rasterizer
  logiciel.
- **Outils 3D** (`Tools/NkGizmo/NkGizmo3DTypes.h`, `Tools/Grid3D/NkGrid3D.h`) — types de gizmo de
  manipulation (translate/rotate/scale) et grille de référence infinie (`NkGrid3D`, Create/Destroy).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKRHI/NkRHI.h` | Parapluie (inclut tout). | — |
| `Core/NkTypes.h` | Handles opaques, `NkGPUFormat`, enums d'état pipeline, viewport/clear, résultats. | [Device](NKRHI/Device.md) |
| `Core/NkIDevice.h` | `NkIDevice` (création ressources, soumission), `NkDeviceCaps`, frame. | [Device](NKRHI/Device.md) |
| `Core/NkDeviceFactory.h` | `NkDeviceFactory` (Create/AutoDetect/Fallback/Destroy). | [Device](NKRHI/Device.md) |
| `Core/NkDeviceInitInfo.h` | `NkDeviceInitInfo`, callbacks present/resize, helpers init. | [Device](NKRHI/Device.md) |
| `Core/NkContextDesc.h` | `NkContextDesc` + descripteurs par backend + format swapchain. | [Device](NKRHI/Device.md) |
| `Core/NkContextInfo.h` | `NkContextInfo` (infos runtime du device). | [Device](NKRHI/Device.md) |
| `Core/NkGraphicsApi.h` | `NkGraphicsApi` (enum réel via NKEvent), `NkGraphicsApiName`. | [Device](NKRHI/Device.md) |
| `Core/NkGpuPolicy.h` | `NkGpuPolicy` (hints process-level, match vendor PCI). | [Device](NKRHI/Device.md) |
| `Core/NkISwapchain.h` | `NkISwapchain` (multi-fenêtre), `NkSwapchainDesc`. | [Device](NKRHI/Device.md) |
| `Core/NkDescs.h` | Tous les `NkXxxDesc` ressources/pipeline, descriptors, barrières, copies. | [Ressources](NKRHI/Resources.md) |
| `Commands/NkICommandBuffer.h` | `NkICommandBuffer`, `NkCommandBufferType`. | [Commandes & compute](NKRHI/Commands-Compute.md) |
| `Commands/NkCommandPool.h` | `NkCommandPool` (recyclage thread-safe). | [Commandes & compute](NKRHI/Commands-Compute.md) |
| `Core/NkCommandPool.h` | Copie identique de `NkCommandPool` (n'inclure qu'un des deux). | [Commandes & compute](NKRHI/Commands-Compute.md) |
| `Core/NkComputeContext.h` | `NkComputeContext`, `NkComputeBuilder`, helpers compute. | [Commandes & compute](NKRHI/Commands-Compute.md) |
| `Core/NkML.h` | `NkMLContext`, `NkTensor`, `NkTensorShape`, `NkMLShaders`. | [Commandes & compute](NKRHI/Commands-Compute.md) |
| `SL/NkSLIntegration.h` | Pont vers le compilateur NkSL (création shaders, reflection). | [Shaders & outils](NKRHI/Shaders-Tools.md) |
| `SL/NkSWShaderBridge.h` | Pont shader software (NkSL → lambdas C++). | [Shaders & outils](NKRHI/Shaders-Tools.md) |
| `Tools/NkGizmo/NkGizmo3DTypes.h` | Types/enums de gizmo 3D (données seules). | [Shaders & outils](NKRHI/Shaders-Tools.md) |
| `Tools/NkGizmo/NkGizmo3D.h` | **Vide** — aucune API de manipulation livrée. | [Shaders & outils](NKRHI/Shaders-Tools.md) |
| `Tools/Grid3D/NkGrid3D.h` | `NkGrid3D` (grille de référence, Init/Shutdown). | [Shaders & outils](NKRHI/Shaders-Tools.md) |
| `Tools/Grid3D/NkGrid3DShaders.h` | Mal nommé : contient les shaders **gizmo** (`gizmoshaders`). | [Shaders & outils](NKRHI/Shaders-Tools.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
