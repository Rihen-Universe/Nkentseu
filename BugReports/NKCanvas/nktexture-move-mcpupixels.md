# NKCanvas — NkTexture (move) ne transfère pas `mCPUPixels`

- **Catégorie** : NKCanvas (cycle de vie ressource)
- **Sévérité** : moyenne (perte de données silencieuse ; pas un crash)
- **Date** : 2026-06-04
- **Statut** : **résolu**

## Symptôme

Après un déplacement (`NkTexture b = std::move(a);` ou move-assign), la copie CPU
des pixels (`mCPUPixels`) était **perdue** : `b` récupérait le handle/GPU id mais
pas le buffer pixels, et `a` gardait un buffer orphelin. Conséquences possibles :
`GetPixels()`/ré-upload incorrects, et asymétrie de propriété du buffer.

Découvert en marge de l'enquête `c0000374` (cf.
[heap-c0000374-renderer-alloc-mismatch.md](heap-c0000374-renderer-alloc-mismatch.md)) ;
**ce n'était PAS le corrupteur**, mais un vrai bug de correction.

## Cause racine

Le **move-ctor** et le **move-assign** de `NkTexture` recopiaient les membres
triviaux (`mWidth/mHeight/mHandle/mGPUId/mFilter/mWrap/mRenderer`) et invalidaient
la source, **mais oubliaient le membre `NkVector<uint8> mCPUPixels`**. Il restait
donc dans la source (et était écrasé/vidé côté destination).

## Solution

Transférer explicitement `mCPUPixels` dans les deux opérations de move
(`NkTexture.cpp`) :

```cpp
// move-ctor : dans la liste d'init
, mCPUPixels(static_cast<NkVector<uint8>&&>(other.mCPUPixels))

// move-assign : après Destroy() et la recopie des autres membres
mCPUPixels = static_cast<NkVector<uint8>&&>(other.mCPUPixels);
```

`static_cast<...&&>` invoque le move de `NkVector` (zero-STL, pas de `std::move`).

## Vérification

Build vert ; plus de perte de pixels après move. (Aucun impact sur le `c0000374`,
qui avait une autre cause.)

## Liens

- `Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp` (move-ctor, move-assign)
