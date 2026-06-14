# Windows : dxcompiler.dll ne charge pas / debug layer absent (runtimes manquants)

- **Catégorie** : Environnement-Windows
- **Sévérité** : bloquant (environnement)
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

- `dxc.exe` : `dxcompiler.dll failed to load`, puis (PATH nettoyé) exit
  `0xC0000135` = **STATUS_DLL_NOT_FOUND**.
- Notre device qui charge `dxcompiler.dll` via `LoadLibrary` échoue ou produit du
  DXIL inutilisable.
- Le debug layer DX12 ne s'active jamais (pas de `D3D12SDKLayers.dll`).
- App graphique : `0xC0000142` = **STATUS_DLL_INIT_FAILED** au démarrage.

## Cause racine

Cette machine n'avait **pas le VC++ Redistributable x64** installé : `msvcp140.dll`
/ `vcruntime140.dll` **absents** de `C:\Windows\System32`. Pire, un `msvcp140.dll`
**incompatible** (Huawei DevEco) traînait sur le `PATH` → chargé en premier → crash
(`failed to load`, et historiquement SIGSEGV dans la validation Vulkan). Et le
debug layer D3D12 dépend de la feature Windows **Graphics Tools** (non installée).

## Solution

1. **Runtime VC++ x64** : installer *Microsoft Visual C++ Redistributable (x64)*
   (`vc_redist.x64.exe`) → fournit `msvcp140.dll`, `vcruntime140.dll`,
   `vcruntime140_1.dll` en System32. (Dépannage testé : pointer le PATH vers un
   dossier qui a déjà ces DLL — ex. `C:\Program Files\Blender Foundation\Blender 5.1\blender.crt`
   — a permis de faire tourner `dxc.exe` en CLI.)
2. **dxil.dll** (signature DXIL, pour que DX12 accepte le DXIL de dxc) : fourni par
   le Windows SDK / Graphics Tools, doit être chargeable à côté de `dxcompiler.dll`.
3. **Graphics Tools** (debug layer DX12) : voir
   [../DirectX12/debug-layer-graphics-tools.md](../DirectX12/debug-layer-graphics-tools.md).
4. **0xC0000142 au démarrage** : souvent une DLL injectée (ex. PIX
   `WinPixGpuCapturer.dll`) ou un process zombie qui verrouille l'exe. Tuer les
   instances restantes (`Get-Process NkRHIDemoFullSL | Stop-Process`), fermer PIX,
   relancer.

## Vérification

- `dxc.exe -T ps_6_0 ...` compile sans `failed to load`.
- `C:\Windows\System32\msvcp140.dll` et `D3D12SDKLayers.dll` existent.

## Liens

- `dxc.exe` : `C:\VulkanSDK\<ver>\Bin\dxc.exe` (ne contient pas `dxil.dll`).
- [../DirectX12/dxc-integration-clang-mingw.md](../DirectX12/dxc-integration-clang-mingw.md)
- Mémoire : `project_session_20260531_nkrhi_backends.md` (msvcp140 Huawei)
