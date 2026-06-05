# DX12 — Spam validation : depth en DEPTH_READ au lieu de DEPTH_WRITE (clear/barrier)

- **Catégorie** : DirectX12
- **Sévérité** : majeur (spam continu ; le rendu fonctionne mais l'état GPU est incohérent)
- **Date** : 2026-06-04
- **Statut** : ouvert (analysé, fix device à faire)

## Symptôme

À chaque frame, le debug layer DX12 crache en boucle (souvent par paire) :
```
ID3D12CommandQueue::ExecuteCommandLists: Using ClearDepthStencilView ... :
Resource state (0x20: D3D12_RESOURCE_STATE_DEPTH_READ) of resource (...depth...)
is invalid for use as a depth buffer. Expected State Bits (all): 0x10:
D3D12_RESOURCE_STATE_DEPTH_WRITE, Actual State: 0x20: ...DEPTH_READ...

ID3D12CommandQueue::ExecuteCommandLists: Using ResourceBarrier ... : Before state
(0x10: ...DEPTH_WRITE) of resource (...depth...) specified by transition barrier
does not match with the state (0x20: ...DEPTH_READ) specified in preceding
ResourceBarrier or as InitialState
```
La scène rend quand même (le runtime tolère), mais le **tracking d'état** logiciel
et l'**état GPU réel** de la depth divergent.

## Contexte

- Backend DX12, démo `NkRHIDemoFullSL`. Nécessite le debug layer (Graphics Tools)
  pour voir ces messages — voir [debug-layer-graphics-tools.md](debug-layer-graphics-tools.md).

## Cause racine (analyse)

- La depth est **créée** en `DEPTH_WRITE` avec un tracking cohérent
  (`NkDirectX12Device::CreateTexture` : `initState = DEPTH_WRITE`, `t.state = initState`).
- `BeginRenderPass` re-transitionne en `DEPTH_WRITE` via `TransitionTextureState`
  (qui skippe le barrier si `it->state == to`), puis `ClearDepthStencilView`.
- `EndRenderPass` ne transitionne **que la couleur** (vers PRESENT), **pas la depth**.
- Le message « barrier Before=DEPTH_WRITE ne matche pas l'état réel DEPTH_READ »
  prouve qu'une transition vers `DEPTH_READ` a eu lieu sur le GPU **sans mettre à
  jour le tracking** (ou un barrier brut hors `TransitionTextureState`). Suspect
  principal : le **bind de la depth/shadow en SRV** (sampling de la shadow map) qui
  ne passe pas par le tracking — `BindDescriptorSet` côté DX12 **ne transitionne
  pas** les textures qu'il lie. D'où : tracking dit DEPTH_WRITE, GPU est en DEPTH_READ.

## Solution (à faire)

Côté device DX12 :
1. Faire passer **toute** transition de la depth par `TransitionTextureState`
   (jamais de barrier brut), pour que `it->state` reste synchrone avec le GPU.
2. Quand une texture depth est bindée en **SRV** (sampling), la transitionner
   explicitement en `DEPTH_READ`/`PIXEL_SHADER_RESOURCE` via `TransitionTextureState`
   (met à jour le tracking), puis la re-transitionner en `DEPTH_WRITE` au
   `BeginRenderPass` suivant (le barrier sera alors correct : DEPTH_READ→DEPTH_WRITE).
3. Alternative robuste : tracker l'état par (resource, frame) si la depth est
   partagée entre frames in-flight.

C'est la **même classe de bug** que les textures non transitionnées au bind
(cf. textures albédo) : `BindDescriptorSet` doit transitionner les ressources qu'il
lie ET mettre à jour le tracking.

## Vérification

- Plus aucun message `DEPTH_READ vs DEPTH_WRITE` ni `ResourceBarrier Before...
  does not match` dans le debug layer.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12CommandBuffer.cpp`
  (BeginRenderPass / EndRenderPass / BindDescriptorSet)
- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp`
  (CreateTexture, TransitionTextureState)
- [hlsl-dx-binding-shared-counter.md](../NkSL-Generateurs/hlsl-dx-binding-shared-counter.md)
