# DX11 — écran figé après maximize (ResizeBuffers INVALID_CALL / command list non relâché)

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur (DX11 inutilisable en fenêtré redimensionnable)
- **Date** : 2026-07-04
- **Statut** : résolu (DX11)

## Symptômes

- Sur **DirectX 11** : cliquer le bouton **maximize** (ou redimensionner) **fige le rendu**
  (l'écran reste sur la même image), mais l'application **continue de tourner en arrière-plan**.
  **Minimiser débloque** le rendu. Reproductible uniquement en **interactif** (clic souris) —
  un `ShowWindow(SW_MAXIMIZE)` programmatique NE reproduisait PAS (timing de frame différent).

## Diagnostic (gdb sur le vif + log Present)

1. Attache gdb au process **gelé** → le thread principal **AVANCE** dans le rendu
   (FlushInstanced → CommandBuffer::Reset → WriteBuffer → driver NVIDIA), il n'est **PAS**
   bloqué. Le `WriteBuffer` (Map d'un buffer dynamique) attendait dans le driver → le GPU
   était **saturé** → back-buffers non recyclés → **problème de PRÉSENTATION**, pas un hang.
2. Log gated (`NK_DX11_PRESENT_DIAG`) dans `SubmitAndPresent`/`ResizeSwapchain` →
   au maximize : `ResizeSwapchain -> 1920x1009` puis **`ResizeBuffers (hr=0x887A0001)`**
   = **`DXGI_ERROR_INVALID_CALL`** = *il reste des références en cours sur les back-buffers*.
   → `ResizeBuffers` échoue → fallback `CreateSwapchain` (destroy+recreate) → le nouveau
   swapchain FLIP reste **occlus** sur le pilote NVIDIA jusqu'à un event fenêtre (minimize)
   → écran figé.

## Cause racine

Le **command list** DX11 de la frame précédente (`NkDirectX11CommandBuffer::mCmdList`,
produit par `FinishCommandList`) n'était **relâché qu'au `Reset()` de la frame suivante**.
Or il garde des références sur toutes les ressources qu'il référence — **dont le RTV du
back-buffer** (via `BeginRenderPass`/`OMSetRenderTargets`). Un `OnResize` (déclenché par le
maximize) survient **AVANT** ce `Reset`, pendant que `mCmdList` tient encore le back-buffer
→ `IDXGISwapChain::ResizeBuffers` échoue en `INVALID_CALL`.

## Fix (2 changements complémentaires)

1. **`NkDirectX11CommandBuffer::Execute`** : relâcher `mCmdList` **dès l'exécution**
   (`ExecuteCommandList` puis `mCmdList->Release(); mCmdList=nullptr;`) au lieu d'attendre
   le `Reset` suivant → plus de référence traînante au moment d'un resize.
2. **`NkDirectX11Device::ResizeSwapchain`** : réécrit pour utiliser **`ResizeBuffers`**
   (garde le MÊME swapchain, redimensionne ses buffers) au lieu de destroy+recreate ; avec
   `mContext->ClearState()` + release explicite du back-buffer (tex + rtv) AVANT.
   `ResizeBuffers` conserve le swapchain → **zéro état occlus**, présentation immédiate.
   Fallback `CreateSwapchain` conservé si `ResizeBuffers` échoue quand même (device removed…).

Fixes connexes du même chantier : `DestroySwapchain` relâche désormais explicitement le
back-buffer (`DestroyTexture` **skippe** les textures `isSwapchain`, `DestroyFramebuffer` ne
libère aucune texture) via les membres `mSwapchainColorTex`/`mSwapchainDepthTex` — sinon le
swapchain n'était jamais libéré (refcount>0) → conflit HWND / hang / fuite par resize.

## Règle générale

Sous DX11 flip-model, **avant tout `ResizeBuffers`**, relâcher TOUTES les références aux
back-buffers : contexte (`ClearState`), RTV/texture du back-buffer, ET les **command lists**
en attente (les tenir entre frames = référence traînante → `INVALID_CALL`). Toujours préférer
`ResizeBuffers` au destroy+recreate (le recreate d'un swapchain FLIP sur le même HWND peut
rester occlus jusqu'à un event fenêtre sur certains pilotes).

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX11/NkDirectX11CommandBuffer.cpp` (`Execute`).
- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX11/NkDirectX11Device.cpp` (`ResizeSwapchain`,
  `DestroySwapchain`, `SubmitAndPresent` diag `NK_DX11_PRESENT_DIAG`).
