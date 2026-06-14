# DX12 — Intégrer dxc (DXIL/SM6) sous clang-mingw

- **Catégorie** : DirectX12
- **Sévérité** : majeur
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

On veut que DX12 utilise son **vrai compilateur** (`dxc` → DXIL, Shader Model 6)
au lieu de `D3DCompile` (`fxc` → DXBC, SM5.x). Obstacles rencontrés :
- `#include <dxc/dxcapi.h>` → flot de warnings `unknown attribute 'uuid' ignored`.
- `__uuidof` / `IID_PPV_ARGS` renvoient de **mauvais IID** → `QueryInterface`
  échoue à l'exécution.
- `dxc.exe` / `dxcompiler.dll` : `failed to load` puis `0xC0000135` (DLL not found).

## Cause racine

1. **clang-mingw ignore `__declspec(uuid(...))`** : la macro `CROSS_PLATFORM_UUIDOF`
   de `dxcapi.h` (chemin `_WIN32`) repose dessus → `__uuidof` ne récupère pas le
   GUID → `IID_PPV_ARGS` produit un IID nul/faux.
2. **`dxcompiler.dll` a besoin du runtime VC++ x64** (`msvcp140.dll`,
   `vcruntime140.dll`) — voir
   [../Environnement-Windows/runtime-vc-dxcompiler-graphics-tools.md](../Environnement-Windows/runtime-vc-dxcompiler-graphics-tools.md).

## Solution

Dans `NkDirectX12Device.cpp` :

1. **Include guardé + warnings silencés** :
   ```cpp
   #if defined(__has_include)
   #  if __has_include(<dxc/dxcapi.h>)
   #    pragma clang diagnostic push
   #    pragma clang diagnostic ignored "-Wignored-attributes"
   #    pragma clang diagnostic ignored "-Wunknown-attributes"
   #    include <dxc/dxcapi.h>
   #    pragma clang diagnostic pop
   #    define NK_HAS_DXC 1
   #  endif
   #endif
   ```

2. **CLSID + IID hardcodés** (valeurs canoniques de `dxcapi.h`), passés
   explicitement — PAS de `__uuidof`/`IID_PPV_ARGS` :
   ```cpp
   static const CLSID kCLSID_DxcCompiler = {0x73e22d93,...};
   static const IID   kIID_IDxcCompiler3 = {0x228B4687,...};
   static const IID   kIID_IDxcResult    = {0x58346CDA,...};
   static const IID   kIID_IDxcBlob      = {0x8BA5FB08,...};
   static const IID   kIID_IDxcBlobUtf8  = {0x3DA636C9,...};
   template <typename T> void** NkPpv(ComPtr<T>& p){ return (void**)p.GetAddressOf(); }
   ```

3. **Chargement dynamique** : `LoadLibraryA("dxcompiler.dll")` +
   `GetProcAddress("DxcCreateInstance")` (pas de lien `dxcompiler.lib`).
   `IDxcCompiler3::Compile` avec args `-T vs_6_0/ps_6_0 -E main` → blob DXIL via
   `GetOutput(DXC_OUT_OBJECT, kIID_IDxcBlob, ...)`.

4. **Fallback fxc** : si dxc indisponible (`dxcompiler.dll` absente / runtime VC
   manquant), retomber sur `D3DCompile` (DXBC SM5.1). Switch `NK_DISABLE_DXC=1`
   pour forcer fxc (utile au diagnostic : compare DXIL vs DXBC).

## Vérification

- Build clang-mingw sans warning sur dxcapi.h.
- Au runtime, aucun message « fallback fxc » → dxc compile bien le HLSL NkSL en DXIL.
- ⚠️ Le DXIL doit être **signé** (dxc charge `dxil.dll`) pour que
  `CreateGraphicsPipelineState` l'accepte ; sinon `E_INVALIDARG`. Le Windows SDK /
  Graphics Tools fournit `dxil.dll`.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp` (NkDxcCompileHLSL, CreateShader)
- Mémoire : `project_session_20260601_nksl_pivot.md`
