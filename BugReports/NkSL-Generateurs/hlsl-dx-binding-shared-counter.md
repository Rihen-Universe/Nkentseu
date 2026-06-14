# NkSL → HLSL : textures invisibles sur DX (compteur de binding séparé vs partagé)

- **Catégorie** : NkSL-Generateurs
- **Sévérité** : majeur
- **Date** : 2026-06-04
- **Statut** : **résolu DX11 + DX12**. Sur DX12 il a fallu EN PLUS corriger la
  root signature (voir [../DirectX12/root-signature-table-par-registre.md](../DirectX12/root-signature-table-par-registre.md)) :
  le compteur partagé donne le bon registre dans le HLSL, mais le device DX12
  plaçait mal la table de descripteurs.

## Symptôme

Les textures (albédo, texte) **ne s'affichent pas** sur DX11 et DX12, alors qu'elles
sont visibles sur OpenGL et Vulkan. Aucune erreur de compilation ni de PSO.

## Cause racine

Le device DX (DX11 et DX12) mappe le **numéro de binding du descripteur → slot de
registre** (SRV `t`, sampler `s`, cbuffer `b`). Le démo binde avec un compteur
**partagé** : `ubo=0, shadow=1, albedo=2` (comme GLSL). Mais le générateur HLSL
assignait des registres avec des compteurs **séparés par namespace** repartant de 0 :
- `ubo → b0`, `shadow → t0/s0`, `albedo → t1/s1`.

Donc le device met l'albédo (binding 2) sur le slot `t2`, mais le shader le lit sur
`t1` → **albédo jamais échantillonné**. GLSL marche car son compteur partagé donne
`shadow=binding1, albedo=binding2`, qui matche le démo.

Subtilité : le parser NkSL capture `@binding` pour les **blocs** (cbuffer) mais PAS
pour les **samplers** (`HasBinding()` false sur les samplers). Donc le cbuffer
utilisait son binding explicite (0) **sans incrémenter** le compteur, et les
samplers repartaient de 0.

## Solution

Générateur HLSL (`NkSLCodeGenHLSL_DX12.cpp`, idem à faire pour `NkSLCodeGenHLSL.cpp`
DX11) : utiliser **UN compteur de binding partagé** `mReg` (au lieu de mRegB/T/S/U
séparés), et l'**avancer même quand le binding est explicite** :
```cpp
// cbuffer / texture / image :
int slot = bind.HasBinding() ? bind.binding : mReg;
if (slot >= mReg) mReg = slot + 1;   // avance le compteur partage
// Un sampler combine = UNE ressource -> texture ET sampler partagent le MEME slot.
```
Résultat pour le démo : `ubo→b0, shadow→t1/s1, albedo→t2/s2`, qui matche le mapping
binding→slot du device.

## Vérification

- HLSL généré : `ushadowmap_tex : register(t1)`, `ualbedomap_tex : register(t2)`.
- DX12 : la texture albédo s'affiche.
- **DX11 reste à corriger** (`NkSLCodeGenHLSL.cpp` : `texReg/sampReg/uavReg` locaux +
  `GenCBuffer` qui force `reg=0`) — ajouter un membre `mReg` partagé, reset dans
  `GenProgram`, et l'utiliser dans `GenCBuffer` (avec avancement) + la boucle
  textures.

## Note connexe — clamp sur les bords (GL/VK)

Sur GL/VK la texture est visible mais **clampée aux bords** : c'est le
`clamp(input.vuv, 0.0, 1.0)` explicite dans le shader + le mode d'adressage du
sampler (CLAMP_TO_EDGE). Si on veut du tiling, retirer le `clamp()` et mettre
`addressMode = REPEAT`.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenHLSL_DX12.cpp` (mReg, GenCBuffer, GenVarDecl)
- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenHLSL.cpp` (DX11 — à faire)
- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX11/NkDirectX11CommandBuffer.cpp` (mapping slot=binding)
- [../DirectX12/depth-state-tracking-clear-barrier.md](../DirectX12/depth-state-tracking-clear-barrier.md)
