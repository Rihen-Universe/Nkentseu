# DX12 — Shader utilise t0/s0 hors de la root signature

- **Catégorie** : DirectX12
- **Sévérité** : bloquant
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

`CreateGraphicsPipelineState` échoue (`E_INVALIDARG`) ; le debug layer indique une
incompatibilité root signature ↔ ressources du shader, OU un comportement de bind
incorrect, quand un shader référence `register(t0)` / `register(s0)`.

## Contexte

- Root signature par défaut `NkDirectX12Device::CreateDefaultRootSignature`.
- Shaders NkSL qui placent la 1re texture/sampler à `t0`/`s0`.

## Cause racine

La root signature avait ses ranges **SRV** et **Sampler** avec
`BaseShaderRegister = 1` (alignée historiquement sur les anciens shaders du démo
écrits à la main, qui mettaient `uShadowMap : register(t1)`). Les shaders NkSL
assignent naturellement les ressources à partir de **t0 / s0**
(`ushadowmap=t0/s0`, `ualbedomap=t1/s1`). Résultat : `t0`/`s0` ne sont **pas
couverts** par la table de descripteurs → PSO invalide.

## Solution

`NkDirectX12Device.cpp`, `CreateDefaultRootSignature` : passer
`BaseShaderRegister` de **1 → 0** pour les ranges SRV et Sampler :
```cpp
srvRange.BaseShaderRegister  = 0;   // couvre t0..t15
sampRange.BaseShaderRegister = 0;   // couvre s0..s15
```
Base 0 couvre `t0` (NkSL) ET `t1` (ancien démo), donc non régressif.

## Vérification

- Le démo NkSL (t0/s0) ET le démo non-SL (t1/s1) créent leur PSO et rendent.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp` (CreateDefaultRootSignature)
