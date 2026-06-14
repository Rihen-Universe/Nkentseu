# NkSL → HLSL DX12 : bugs du générateur (HLSL invalide)

- **Catégorie** : NkSL-Generateurs
- **Sévérité** : bloquant
- **Date** : 2026-06-01 → 2026-06-04
- **Statut** : résolu

## Symptôme

Le HLSL généré par `NkSLCodeGenHLSL_DX12` ne compile pas (fxc/dxc) ou produit un
PSO invalide. Erreurs vues : `X3004 undeclared identifier`, `X3020 type mismatch`,
`cannot convert float3 to float3x3`, `error: use of undeclared identifier`,
linkage VS/PS incompatible.

## Contexte

- Cible `NK_HLSL_DX12` (et en partie `NK_HLSL_DX11`), toolchain clang-mingw.
- Référence qui marche : `NkSLCodeGenHLSL_DX11` (DX11), complet et correct.

## Cause racine + Solution (liste des fixes appliqués)

Le générateur DX12 émettait du scaffolding HLSL correct (cbuffer/struct/register)
mais un **corps de fonction GLSL** + plusieurs incohérences. Fixes dans
`NkSLCodeGenHLSL_DX12.cpp` :

| Bug | Symptôme | Fix |
|-----|----------|-----|
| Corps GLSL (`ubo.model*vec4(aPos)`, pas de `mul`) | `X3004 'ubo'`, `void main()` | Porter `GenExpr`/`GenCall`/`IntrinsicToHLSL` de DX11 : mapping `input.`/`output.`, strip `ubo.`, `mul()` matriciel, helpers `nksl_inverse`/`nksl_texsize2d`, `vec→float`, `inverse→nksl_inverse`, shadow `SampleCmpLevelZero`, casts FXC `(float3x3)(m)` |
| Entry non détectée | `void main()` (branche helper) | `if (fn->isEntry || fn->name=="main")` aux 3 sites (DX11 le fait) |
| `(void)output;` | `X3017 cannot convert NkOutput to void` | Retirer (invalide en HLSL) |
| Variables locales casse | `X3004 undeclared 'worldpos'` | `v->name.ToLower()` sur les décls locales (`GenVarDecl` + for-init) — l'AST garde la casse origine sur la décl mais lowercase les refs |
| Littéraux float sans `f` | `X3020 type mismatch` | `LiteralToStr` → suffixe `f` (sinon traité en double) |
| Casse textures/samplers | `undeclared 'ushadowmap_tex'` | `v->name.ToLower()` sur `_tex`/`_smp`/image |
| Matrices LOCALES non traquées | `cannot convert float3 to float3x3` (`normalmat * aNormal` sans `mul`) | Pousser les locales matricielles dans `mMatrixNames` (`GenVarDecl`) |
| Membres cbuffer casse incohérente | `undeclared 'lightDirW'`/`'lightvp'` | Membres cbuffer/struct en lowercase + `GenExpr` MEMBER strip retourne `m->member.ToLower()` |
| Sémantique inter-étages | linkage `TEXCOORD`/`SV_Position` incompatible | `inputSem` (POSITION/NORMAL) seulement pour entrée VERTEX ; entrée FRAGMENT → `TEXCOORD<loc>` ; et `SV_Position` en 1er dans la struct PS (voir [../DirectX12/pso-signature-linkage-E_INVALIDARG.md](../DirectX12/pso-signature-linkage-E_INVALIDARG.md)) |

## Méthode de diagnostic

Compiler le HLSL généré **directement avec `dxc.exe`** (en CLI, avec le runtime
VC++ dispo) donne des erreurs claires avec ligne/colonne — bien plus rapide que de
relancer l'app. Puis activer le **debug layer DX12** pour les erreurs de PSO
(linkage, input layout) que le compilateur ne voit pas.

## Vérification

- `dxc -T vs_6_0/ps_6_0` compile le HLSL sans erreur.
- DX12 crée le PSO et rend la scène 3D.
- **Important** : penser à `rm -rf ./nksl_cache` après tout changement de
  générateur (le cache NkSL est clé sur la source d'entrée, pas la sortie).

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenHLSL_DX12.cpp`
- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenHLSL.cpp` (DX11, référence)
- Mémoire : `project_session_20260601_nksl_pivot.md`
