# Rendu par backend : écran noir / crash / teinte (GL / VK / DX12 / SW)

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur
- **Date** : 2026-05-31 → 2026-06-04
- **Statut** : résolu (GL/VK/DX11/DX12) ; Software + finitions en cours

## Symptômes & solutions par backend

### OpenGL — écran noir
- **Cause** : locations/bindings manquants dans le GLSL généré + depth jamais
  cleared dans le démo. **Fix** : voir
  [../NkSL-Generateurs/glsl-locations-bindings.md](../NkSL-Generateurs/glsl-locations-bindings.md)
  + `SetClearColor/SetClearDepth` avant `BeginRenderPass`.
- Piège connexe (`NkOpenGLContext::GetInfo()` qui ne remplit pas
  `windowWidth/Height` → fallback 800×600 → moitié droite invisible) : remplir
  `i.windowWidth=mData.width; i.windowHeight=mData.height;`.

### Vulkan — crash / SPIR-V / teinte pâle
- **Crash validation** : `VK_LAYER_KHRONOS_validation` chargeait un `msvcp140.dll`
  Huawei → SIGSEGV. **Fix** : `validationLayers`/`debugMessenger` = `false` par
  défaut (opt-in).
- **SPIR-V** : `#extension GL_KHR_vulkan_glsl` parasite → glslang refuse. **Fix** :
  retiré du générateur.
- **Teinte pâle** : swapchain sRGB ré-encode la sortie linéaire. **Fix** : config
  `NkVulkanDesc::srgbSwapchain` (UNORM opt-in) ; démos mettent `false`.

### DirectX 12 — PSO E_INVALIDARG puis crash intermittent
- **PSO linkage** : ordre `SV_Position` VS-out/PS-in → voir
  [../DirectX12/pso-signature-linkage-E_INVALIDARG.md](../DirectX12/pso-signature-linkage-E_INVALIDARG.md).
- **Crash intermittent** : `NkUnorderedMap` → voir
  [../Memoire-Heap/nkunorderedmap-define-find-rehash.md](../Memoire-Heap/nkunorderedmap-define-find-rehash.md).
- **Finitions ouvertes (2026-06-04)** :
  - **Spam `ClearDepthStencilView ... DEPTH_READ invalide, attendu DEPTH_WRITE`** :
    tracking d'état de la depth désynchronisé dans `NkDirectX12CommandBuffer`
    (transition manquante DEPTH_READ→DEPTH_WRITE avant le clear, et barrier
    `Before state` qui ne matche pas). N'empêche pas le rendu mais à corriger.
  - **Textures (albédo) non visibles** : à investiguer (binding descriptor t1
    côté device, et la réflexion NkSL ne remonte que la ressource `ubo`).

### Software — écran noir
- Cible `NK_CPLUSPLUS` : le générateur `GenCPP` est incomplet (corps GLSL, `1f`
  invalide…). À porter comme DX12 (depuis le générateur de référence). En cours.

## Liens

- Mémoire : `project_session_20260531_nkrhi_backends.md`,
  `project_session_20260601_nksl_pivot.md`.
