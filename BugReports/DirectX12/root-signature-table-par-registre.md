# DX12 — Textures multiples invisibles : table de descripteurs offsetée (sol noir)

- **Catégorie** : DirectX12
- **Sévérité** : bloquant (textures non échantillonnées dès qu'il y a ≥2 SRV)
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

Démo `NkRHIDemoFullSL` sur DX12 : cube et sphère rendus, mais le **sol est noir**
(la texture albédo n'est pas échantillonnée). GL/VK/DX11 affichent le sol texturé.
Le sol fait `basecolor *= albedo.Sample(...)` → albédo noir = sol noir.
Le HLSL généré est pourtant correct (`ualbedomap : register(t2)`).

## Cause racine

La root signature avait **une seule grande table** SRV couvrant `t0..t15`
(`params[2]`, range 16 descripteurs). `BindDescriptorSet` faisait
`SetGraphicsRootDescriptorTable(2, GPUFrom(idxTexture))` **pour chaque** texture,
en pointant la base de la table sur le slot heap **individuel** de la texture.

Or une table de N descripteurs est lue par offset depuis sa base : le shader lit
`t1 = base+1`, `t2 = base+2`. Comme :
1. shadow (heap idx X) et albédo (heap idx Y) ne sont **pas contigus** dans le heap,
2. la **dernière** texture liée écrase la base (root param 2 unique),

ni `t1` ni `t2` ne tombait sur le bon SRV → albédo lit du noir/indéfini.

DX11 ne souffrait pas du problème car il lie par slot direct
(`PSSetShaderResources(slot, 1, &srv)`), sans offset de table.

## Solution

Passer à **UNE table de descripteurs d'1 seul descripteur PAR registre**
(`t0`, `t1`, … chacun sa table). Une table d'1 descripteur peut pointer
n'importe quel slot du heap, **même non contigu** → plus d'offset par registre.

Layout root signature (`NkDX12RootLayout` dans `NkDirectX12Device.h`) :
- param 0 : 32-bit constants (b15, 32 valeurs)
- param 1 : root CBV (b0)
- params 2..9   : tables SRV t0..t7 (1 desc chacune)
- params 10..17 : tables sampler s0..s7
- params 18..21 : tables UAV u0..u3
- Budget DWORD : 32 + 2 + 8 + 8 + 4 = **54 ≤ 64**.

`CreateDefaultRootSignature` génère ces tables en boucle (helper `fillTable`,
`NumDescriptors=1`, `BaseShaderRegister=k`, `OffsetInDescriptorsFromTableStart=0`).

`BindDescriptorSet` mappe **slot de binding → root param** :
`SetGraphicsRootDescriptorTable(SRV_BASE + b.slot, GPUFrom(srvIdx))`,
idem `SAMP_BASE + slot` et `UAV_BASE + slot` (helpers `bindSrv/bindSamp/bindUav`,
garde `slot < NUM_*`). Le CBV b0 reste un root CBV (`ROOT_CBV`).

## Vérification

- Capture DX12 : le sol affiche la texture verte clampée + l'ombre du cube,
  identique à GL/VK/DX11.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.h` (NkDX12RootLayout)
- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp` (CreateDefaultRootSignature)
- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12CommandBuffer.cpp` (BindDescriptorSet)
- [../NkSL-Generateurs/hlsl-dx-binding-shared-counter.md](../NkSL-Generateurs/hlsl-dx-binding-shared-counter.md)
- [depth-state-tracking-clear-barrier.md](depth-state-tracking-clear-barrier.md) (spam restant)
