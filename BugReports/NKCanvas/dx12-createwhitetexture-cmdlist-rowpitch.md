# NKCanvas DX12 — Rien à l'écran : CreateWhiteTexture (cmdList fermée + RowPitch)

- **Catégorie** : NKCanvas (backend DirectX 12)
- **Sévérité** : élevée (backend DX12 n'affichait rien — fond OK, draws invisibles)
- **Date** : 2026-06-04
- **Statut** : **résolu**

## Symptôme

En DX12, Pong **n'affichait rien** (le contexte s'initialisait, « on évoluait »
dans les logs, mais aucun draw visible). Une fois la **debug layer** activée
(cf. [../DirectX12/debug-layer-graphics-tools.md](../DirectX12/debug-layer-graphics-tools.md)),
les messages de validation pointaient l'init de la **texture blanche 1×1** du
renderer 2D :

- `id=547` : enregistrement de commande sur une **command list déjà fermée**
  (Close → record → erreur).
- `id=921` : utilisation/référence d'une **ressource détruite** (l'upload buffer
  intermédiaire libéré trop tôt).

La texture blanche (utilisée comme texture par défaut pour les draws non
texturés) n'étant jamais correctement uploadée, tous les draws « blancs » étaient
invisibles.

## Cause racine

1. **Command list / fence mal séquencées** : `CreateWhiteTexture` réutilisait la
   command list principale (déjà `Close()`d / en vol) au lieu d'une liste d'upload
   dédiée correctement `Reset → record → Close → Execute → fence wait`.
2. **`RowPitch` non aligné** : le copy d'upload d'une texture utilise un
   `D3D12_PLACED_SUBRESOURCE_FOOTPRINT` dont le `RowPitch` doit être **aligné sur
   `D3D12_TEXTURE_DATA_PITCH_ALIGNMENT` (256)**. Pour une 1×1 RGBA (4 octets), un
   RowPitch de 4 non aligné → copie/upload invalide.
3. **Ordre d'init** : la texture blanche était créée avant que le registre/heap
   SRV ne soit prêt.

## Solution

- `CreateWhiteTexture` utilise désormais une **command list d'upload dédiée**
  (Reset/record/Close/Execute + attente fence) et garde l'upload buffer **vivant**
  jusqu'à la fin de la copie.
- **RowPitch aligné sur 256** (`Align(width*4, 256)`) pour le footprint d'upload,
  copie ligne par ligne depuis la source compacte.
- Création de la texture blanche **réordonnée après** l'init du registre/heap SRV.

Fichier : `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/DirectX/NkDX12Renderer2D.cpp`
(+ `NkDX12Context.cpp` pour le routage des messages debug vers NkLogger).

## Vérification

Avec la debug layer : plus de message `id=547` / `id=921`, `[NkDX12-2D]
Initialized`, et le rendu DX12 affiche (fond + draws). Confirmé au screenshot.

## Notes / pièges

- **clang-mingw ne supporte pas `__try/__except` SEH** : pour `EnableDebugLayer()`
  (qui peut faire un access violation si Graphics Tools absent), utiliser un check
  `LoadLibraryA("D3D12SDKLayers.dll")` préalable au lieu du SEH.
- `SetBreakOnSeverity(ERROR, TRUE)` **fait planter** l'app au 1er message d'erreur
  sans debugger attaché → pour *collecter* les messages, le mettre à **FALSE** et
  s'appuyer sur le callback `InfoQueue1` qui route vers NkLogger.

## Liens

- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/DirectX/NkDX12Renderer2D.cpp`
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/DirectX/NkDX12Context.cpp`
- [../DirectX12/debug-layer-graphics-tools.md](../DirectX12/debug-layer-graphics-tools.md)
