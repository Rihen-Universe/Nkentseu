# NKCanvas Vulkan — Rendu : écran blanc, Y inversé, couleurs délavées

- **Catégorie** : NKCanvas (backend Vulkan / rendu)
- **Sévérité** : élevée (Vulkan inutilisable visuellement, une fois les pipelines OK)
- **Date** : 2026-06-05
- **Statut** : **résolu** (3 bugs)

> Ces 3 bugs n'apparaissaient qu'**après** le fix du crash pipeline
> (cf. [vulkan-spirv-2d-invalide.md](vulkan-spirv-2d-invalide.md)) : le rendu 2D
> Vulkan n'avait donc **jamais** été validé visuellement de bout en bout.

---

## Bug 1 — Écran tout blanc (cycle de frame jamais piloté)

### Symptôme
Vulkan s'initialise, toutes les scènes défilent dans les logs, shutdown propre,
**mais l'écran est entièrement blanc** (aucun rendu).

### Cause racine
`NkIGraphicsContext::BeginFrame()` / `EndFrame()` n'étaient **jamais appelés** dans
le flux NKCanvas :
- `NkRenderWindow::Clear()` appelle `mRenderer->Begin()` ; `Display()` appelle
  `mContext->Present()` — mais **personne** n'appelle `mContext->BeginFrame()`.
- La base `NkBatchRenderer2D::Begin()` appelle `BeginBackend()`, **vide** pour
  Vulkan (`NkVulkanRenderer2D::BeginBackend() {}`).

Conséquence Vulkan : le **render pass n'est jamais commencé** →
`GetVkCurrentCommandBuffer()` renvoie null → `SubmitBatches` sort tôt (aucun draw)
→ le clear (loadOp CLEAR) n'est jamais appliqué → l'image de swapchain présentée
est **non initialisée = blanche**. (OpenGL rend en immédiat sur le framebuffer par
défaut → fonctionne sans `BeginFrame`, d'où le bug masqué.)

### Solution
Piloter le cycle de frame dans les hooks backend Vulkan (isolé, n'affecte pas
GL/DX). L'ordre de la base est correct : `Begin()→BeginBackend()` (begin render
pass AVANT les draws) puis `End()→Flush()→EndBackend()` (draws records PUIS end
render pass) :
```cpp
void NkVulkanRenderer2D::BeginBackend() { if (mCtx) mCtx->BeginFrame(); }
void NkVulkanRenderer2D::EndBackend()   { if (mCtx) mCtx->EndFrame(); }
```
Contrat du contexte : `BeginFrame` = wait fence + acquire image + begin render
pass (clear) ; `EndFrame` = end render pass ; `Present` (déjà appelé par
`NkRenderWindow::Display`) = submit + present. Le flag `isAcquire` protège
`EndFrame`/`Present` si `BeginFrame` échoue (minimisé/resize).

Fichier : `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkVulkanRenderer2D.cpp`.

---

## Bug 2 — Rendu inversé sur l'axe Y

### Symptôme
Tout le rendu 2D est **à l'envers** (miroir vertical).

### Cause racine
Le **NDC Vulkan a +Y vers le bas**, à l'inverse d'OpenGL (+Y vers le haut). La
matrice de projection (`NkView2D::ToProjectionMatrix`) est de style OpenGL.

### Solution
**Viewport à hauteur négative** (façon idiomatique Vulkan de retourner Y sans
toucher la projection — VK_KHR_maintenance1 / Vulkan 1.1, universel) dans
`SubmitBatches` :
```cpp
VkViewport vp{
    (float)mViewport.left,
    (float)mViewport.top + (float)mViewport.height, // origine en bas
    (float)mViewport.width,
    -(float)mViewport.height,                       // hauteur NEGATIVE
    0.f, 1.f
};
```

---

## Bug 3 — Couleurs délavées / trop pâles (double encodage sRGB)

### Symptôme
Le rendu Vulkan est **trop clair / délavé** par rapport à OpenGL.

### Cause racine
Le format de swapchain choisi était **`VK_FORMAT_B8G8R8A8_SRGB`**. Les couleurs 2D
de Pong/NKCanvas sont **déjà encodées sRGB** (octets 0-255). Avec un format
swapchain `_SRGB`, le GPU applique une conversion **linéaire→sRGB** à l'écriture →
**double encodage sRGB** → image délavée.

### Solution
Préférer un format **UNORM** (`VK_FORMAT_B8G8R8A8_UNORM` / `R8G8B8A8_UNORM`) avec
colorspace `SRGB_NONLINEAR` : aucune conversion automatique, les octets sont
écrits tels quels (comportement identique à OpenGL ici). Sélection corrigée dans
`NkVulkanContext::CreateSwapchain` (+ fallback évitant un `_SRGB` explicite).

Fichier : `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkVulkanContext.cpp`.

---

## Notes / pièges (règles générales Vulkan vs OpenGL)

- **Cycle de frame explicite** : Vulkan exige `BeginFrame` (acquire + begin render
  pass) / `EndFrame` (end render pass) / `Present` (submit + present). Ne jamais
  supposer le rendu immédiat comme en OpenGL.
- **Y-flip** : toujours retourner Y (viewport hauteur négative recommandé).
- **sRGB** : si les couleurs sont déjà sRGB, swapchain **UNORM** ; ne réserver le
  format `_SRGB` qu'à un pipeline qui écrit du **linéaire**.

## Liens

- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkVulkanRenderer2D.cpp` (BeginBackend/EndBackend, SubmitBatches viewport)
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Vulkan/NkVulkanContext.cpp` (CreateSwapchain format, BeginFrame/EndFrame/Present)
- [vulkan-spirv-2d-invalide.md](vulkan-spirv-2d-invalide.md) (le crash pipeline qui masquait ces 3 bugs)
