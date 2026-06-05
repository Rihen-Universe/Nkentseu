# DX12 — CreateGraphicsPipelineState échoue (E_INVALIDARG) : linkage VS↔PS

- **Catégorie** : DirectX12
- **Sévérité** : bloquant
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

```
CreateGraphicsPipelineState failed (hr=0x80070057)   // E_INVALIDARG
```
Avec le debug layer (voir [debug-layer-graphics-tools.md](debug-layer-graphics-tools.md)) :
```
Vertex Shader - Pixel Shader linkage error: Signatures between stages are
incompatible. Semantic 'TEXCOORD' is defined for mismatched hardware registers
between the output stage and input stage.
Semantic 'SV_Position' ... mismatched hardware registers ...
```

Variante (autre PSO, pipeline texte) :
```
CreateInputLayout: The provided input signature expects to read an element with
SemanticName/Index: 'TEXCOORD'/1, but the declaration doesn't provide a matching name.
```

## Contexte

- Backend DX12, shaders générés par **NkSL → HLSL DX12**.
- Le PSO se crée pour le démo `NkRHIDemoFullSL`.

## Cause racine

D3D12 assigne les **registres hardware** d'une signature inter-étages dans
l'**ordre de déclaration** des membres de struct (pas par nom de sémantique). Le
générateur HLSL DX12 émettait :
- **VS output** : `SV_Position` **en premier**, puis les varyings `TEXCOORD0..N`.
- **PS input** : les varyings d'abord, `SV_Position` **à la fin**.

→ `SV_Position` et les `TEXCOORD` tombent sur des registres **différents** entre
la sortie du VS et l'entrée du PS → linkage incompatible → `E_INVALIDARG`.

(Variante texte : le layout d'entrée DX du démo fournissait `TEXCOORD0` pour `aUV`
alors que NkSL génère `TEXCOORD1` — l'index sémantique = ordre de déclaration de
l'attribut de vertex.)

## Solution

1. **Ordre cohérent VS-out / PS-in** : dans `NkSLCodeGenHLSL_DX12.cpp`
   (`GenInputOutputStructs`), émettre `float4 _Position : SV_Position;` **AVANT**
   les varyings dans la struct d'**entrée du FRAGMENT**, exactement comme la struct
   de **sortie du VERTEX** (qui met SV_Position en premier). `IsFrontFace` reste
   après. DX11 (qui marchait) met aussi SV_Position en premier des deux côtés.

2. **Sémantique inter-étages identique** : un varying doit avoir la MÊME sémantique
   en sortie VS et en entrée PS. NkSL utilise `inputSem` (POSITION/NORMAL) UNIQUEMENT
   pour l'entrée du VERTEX (match du vertex layout) ; pour l'entrée du FRAGMENT
   (varyings), `TEXCOORD<loc>` comme la sortie VS (cf. `SemanticFor`).

3. **Layout DX = sémantiques du shader** : l'`AddAttribute(..., "TEXCOORD", idx)`
   du démo doit utiliser le même index que celui généré (index = ordre de
   déclaration de l'attribut). Ex. `aUV` en location 1 → `TEXCOORD1`.

## Vérification

- Plus de message `linkage error` ni `CreateInputLayout` dans le debug layer.
- Le PSO se crée, la scène 3D rend en DX12.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenHLSL_DX12.cpp` (GenInputOutputStructs, SemanticFor)
- `Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFullSL.cpp` (textVtxLayout)
- [hlsl-dx12-bugs-generateur.md](../NkSL-Generateurs/hlsl-dx12-bugs-generateur.md)
