# DX12 — dxc échoue « missing entry point definition » (-E main codé en dur)

- **Catégorie** : DirectX12
- **Sévérité** : majeur (DX12 retombe sur fxc/DXBC SM5 au lieu du vrai dxc/DXIL SM6)
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

Log DX12 à chaque shader généré par NkSL :
```
[NkRHI_DX12][ERR] dxc stage 1 indispo/echec -> fallback fxc: error: missing entry point definition
[NkRHI_DX12][ERR] dxc stage 2 indispo/echec -> fallback fxc: error: missing entry point definition
```
Le rendu marche (fxc prend le relais) mais DX12 n'utilise PAS son vrai compilateur
dxc (DXIL/SM6), contrairement à l'objectif.

## Cause racine

`NkDxcCompileHLSL` (dans `NkDirectX12Device.cpp`) appelait dxc avec l'entrée
**codée en dur** `-E main` :
```cpp
const wchar_t* args[] = { L"-T", profile, L"-E", L"main" };
```
Or le générateur HLSL DX12 de NkSL nomme l'entrée d'après la fonction
(`VSMain`, `PSMain`…), pas `main`. dxc ne trouve donc aucune fonction `main`
→ « missing entry point definition ». Le fallback fxc, lui, utilisait
`s.entryPoint` (le vrai nom) et réussissait — d'où la confusion (« ça rend »).

Note : les shaders **écrits à la main** dans la démo (passe shadow) ont une
entrée `main` et compilaient donc avec dxc — seul le HLSL **généré** échouait.

## Solution

Passer le vrai `entryPoint` à dxc (le même que fxc) :
```cpp
static bool NkDxcCompileHLSL(const char* src, const wchar_t* profile,
                             const char* entryUtf8, ...) {
    wchar_t entryW[128];
    const char* e = (entryUtf8 && *entryUtf8) ? entryUtf8 : "main";
    usize n=0; for (; e[n] && n<127; ++n) entryW[n]=(wchar_t)(unsigned char)e[n];
    entryW[n]=L'\0';
    const wchar_t* args[] = { L"-T", profile, L"-E", entryW };
    ...
}
// appel : NkDxcCompileHLSL(s.hlslSource, dxcProfile, entry, *target, dxcErr);
```

## Vérification

- Plus de message « fallback fxc / missing entry » sur `NkRHIDemoFullSL -bdx12`.
- dxc compile l'entrée `VSMain`/`PSMain` en DXIL SM6.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp` (NkDxcCompileHLSL, CreateShader)
- [dxc-integration-clang-mingw.md](dxc-integration-clang-mingw.md)
