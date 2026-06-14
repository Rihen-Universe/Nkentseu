# NKRHI — documentation détaillée

Le module **NKRHI** (Render Hardware Interface), partie par partie. Pour une vue d'ensemble et
un guide « par où commencer », voir le récap : [../NKRHI.md](../NKRHI.md).

Chaque page liste l'**API réelle** des headers concernés — structs, enums, signatures de
méthodes et fonctions libres — avec les **pièges** documentés (impl virtuelles par défaut
no-op, ownership des handles via le device, double-PushBack dans certains builders, champs sans
valeur par défaut…). Tout est dans `namespace nkentseu`, sauf indication contraire.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Device.md](Device.md) | Créer un device (`NkIDevice`), le fabriquer/détruire (`NkDeviceFactory`), le configurer (`NkContextDesc` + options par backend), choisir le GPU (`NkGpuPolicy`) et gérer le swapchain (`NkISwapchain`). Plus tous les types fondamentaux : handles opaques, `NkGPUFormat`, enums d'état du pipeline, viewport/clear, résultats. | `Core/NkIDevice.h`, `Core/NkDeviceFactory.h`, `Core/NkDeviceInitInfo.h`, `Core/NkContextDesc.h`, `Core/NkContextInfo.h`, `Core/NkGraphicsApi.h`, `Core/NkGpuPolicy.h`, `Core/NkTypes.h`, `Core/NkISwapchain.h` |
| [Resources.md](Resources.md) | Décrire immuablement chaque ressource GPU et la lier à un pipeline : buffers, textures, samplers, layouts de vertex, états rasterizer/depth/blend, shaders, render passes, framebuffers, pipelines graphique/compute, descriptor sets, barrières, copies, indirect args. | `Core/NkDescs.h` |
| [Commands-Compute.md](Commands-Compute.md) | Enregistrer le travail GPU avec `NkICommandBuffer` (bind, draw, dispatch, copy, barrières), recycler les command buffers (`NkCommandPool`), faire du calcul GPU générique (`NkComputeContext` + builder fluide) et du deep learning from-scratch (`NkMLContext` : tenseurs, MatMul, convolution, activations, attention, optimiseurs). | `Commands/NkICommandBuffer.h`, `Commands/NkCommandPool.h`, `Core/NkCommandPool.h`, `Core/NkComputeContext.h`, `Core/NkML.h` |
| [Shaders-Tools.md](Shaders-Tools.md) | Compiler des shaders NkSL vers le bon backend et obtenir la reflection (`NkSLIntegration`), transformer un shader NkSL en lambdas C++ pour le rasterizer logiciel (`NkSWShaderBridge`), et afficher des outils 3D : types de gizmo de manipulation et grille de référence infinie (`NkGrid3D`). | `SL/NkSLIntegration.h`, `SL/NkSWShaderBridge.h`, `Tools/NkGizmo/NkGizmo3DTypes.h`, `Tools/Grid3D/NkGrid3D.h`, `Tools/Grid3D/NkGrid3DShaders.h` |

[← Récap NKRHI](../NKRHI.md) · [← Couche Runtime](../README.md)
