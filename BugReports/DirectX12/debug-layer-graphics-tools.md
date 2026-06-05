# DX12 — Obtenir les messages de validation (debug layer / Graphics Tools)

- **Catégorie** : DirectX12 (outil de diagnostic)
- **Sévérité** : outil (débloque le diagnostic d'autres bugs)
- **Date** : 2026-06-04
- **Statut** : résolu

## Symptôme

Un appel DX12 échoue avec un `HRESULT` **opaque**, typiquement
`CreateGraphicsPipelineState failed (hr=0x80070057)` (= `E_INVALIDARG`), **sans
aucune explication**. Impossible de savoir QUEL champ est invalide. On tourne en
rond à deviner (input layout ? root signature ? format ? linkage VS/PS ?).

Symptôme secondaire : on active `dxCfg.debugDevice` mais **aucun message** de
validation n'apparaît dans les logs, et on ne voit pas la ligne
`InfoQueue1 callback enregistre`.

## Contexte

- Backend DirectX 12 (`NkDirectX12Device`).
- Windows 10/11. Toolchain clang-mingw.
- Arrive dès qu'un PSO / une ressource / un barrier est mal configuré.

## Cause racine

Le **debug layer D3D12** (`D3D12SDKLayers.dll`) n'est **pas installé par défaut**
sur Windows. C'est une *fonctionnalité facultative* du système appelée
**« Graphics Tools »** (Feature-on-Demand). Sans elle :

- `D3D12GetDebugInterface(...)` échoue silencieusement (`SUCCEEDED` est faux),
  `EnableDebugLayer()` n'a aucun effet.
- Le device n'expose **pas** `ID3D12InfoQueue` / `ID3D12InfoQueue1` → aucun
  message de validation n'est routé vers le log.
- On ne récupère que le `HRESULT` brut, inexploitable.

Pour vérifier l'absence : le fichier `C:\Windows\System32\D3D12SDKLayers.dll`
**n'existe pas**.

## Solution

### 1. Installer « Graphics Tools »

**Méthode GUI** : touche Windows → *« Fonctionnalités facultatives »* → bouton
*« Afficher les fonctionnalités »* / *« Ajouter une fonctionnalité »* → chercher
**« Graphics Tools »** (ou *« Outils graphiques »*) → Installer (~1 min).

**Méthode PowerShell admin** (clic droit Démarrer → *Terminal (administrateur)*) :
```powershell
Add-WindowsCapability -Online -Name "Tools.Graphics.DirectX~~~~0.0.1.0"
```

**Vérifier** :
```powershell
Get-WindowsCapability -Online -Name "Tools.Graphics.DirectX*"
# -> State : Installed
```
(le fichier `C:\Windows\System32\D3D12SDKLayers.dll` doit alors exister.)

### 2. Activer le debug layer dans le moteur

Déjà câblé dans `NkDirectX12Device.cpp` :
- **Build Debug** : le debug layer est **actif par défaut** (`#ifdef _DEBUG wantDebug=true`).
- **Build Release** : poser la variable d'environnement `NK_DX12_DEBUG=1`.
- L'`InfoQueue1` route les messages vers NkLog via `DX12MessageCallback`
  (ligne `[NkRHI_DX12] InfoQueue1 callback enregistre` confirme l'activation).
- **Important** : les messages stockés sont normalement *drainés en EndFrame*. Si
  l'app **abandonne avant la 1re frame** (échec PSO au setup), on draine
  l'InfoQueue **immédiatement après l'échec** dans `CreateGraphicsPipeline`
  (`DrainDX12InfoQueue(mInfoQueue.Get())`), sinon on n'a que le `E_INVALIDARG`.

### 3. Relancer et lire le message

```
.\NkRHIDemoFullSL.exe -bdx12
```
On obtient alors le vrai message, ex. :
```
ID3D12Device::CreateGraphicsPipelineState: Vertex Shader - Pixel Shader linkage
error: Signatures between stages are incompatible. Semantic 'TEXCOORD' is defined
for mismatched hardware registers between the output stage and input stage.
```
→ exploitable directement (voir [pso-signature-linkage-E_INVALIDARG.md](pso-signature-linkage-E_INVALIDARG.md)).

## Vérification

- `Get-WindowsCapability` → `State : Installed`.
- Au lancement, le log contient `InfoQueue1 callback enregistre`.
- Sur une erreur DX12, des lignes `[NkRHI_DX12][.../...] ID3D12...` détaillées
  apparaissent.

## Notes / pièges

- **PIX ne suffit pas** : PIX (`WinPixGpuCapturer.dll`) profile le GPU mais
  n'installe **pas** le debug layer de validation. Inutile pour ce diagnostic.
- Pour ajouter un dump ad-hoc du PSO desc sans debug layer : on logge déjà
  `numInputElems / RTVFormats / DSVFormat / DepthEnable / input elements` après un
  échec PSO (utile pour écarter input layout / formats / depth).
- Switches diagnostic disponibles : `NK_DX12_DEBUG=1`, `NK_DX12_DUMPHLSL=1`
  (logge le HLSL compilé par le device), `NK_DISABLE_DXC=1` (force fxc au lieu de dxc).

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX12/NkDirectX12Device.cpp` (Initialize, CreateGraphicsPipeline, LogDX12Message/DrainDX12InfoQueue)
- [pso-signature-linkage-E_INVALIDARG.md](pso-signature-linkage-E_INVALIDARG.md)
- Mémoire : `project_session_20260601_nksl_pivot.md`
