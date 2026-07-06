# Software — le multi-thread est PLUS LENT que le mono-thread (tuilage sur le scissor)

- **Catégorie** : Backends-Rendu (perf)
- **Sévérité** : majeur (perf)
- **Date** : 2026-07-05
- **Statut** : résolu

## Symptôme

Backend **Software** (rasterizer CPU tuilé multi-thread), scène NKRenderer (`renderdemo --demo=2 -bsw`) :
- Rendu **très lent** (~1 FPS à 1280×720). Rasterisation ≈ **690 ms/frame** (mesure `NK_SW_PERF=1`).
- **Contre-intuitif** : forcer le **mono-thread** `NK_SW_NOMT=1` était **plus RAPIDE** (~420 ms) que le multi-thread 16 cœurs (~690 ms).
- L'utilisateur avait remarqué : « plus la fenêtre est petite, plus le FPS explose ».

## Cause racine

Dans `RasterizeList` (`NkSWFastPath.h`), la décision de paralléliser et le **tuilage** se faisaient sur le **rectangle de scissor** (`cx0,cy0,cx1,cy1`), qui vaut le plus souvent **le plein écran** (1280×720). Donc **chaque draw** — même une petite sphère qui ne couvre que 1-2 tuiles — dispatchait **toutes les tuiles de l'écran** (`(1280/64)×(720/64) = 240 tuiles`) au thread pool, dont ~238 **vides** (le triangle ne les recouvre pas → early-out par tuile).

La scène fait **~1500 draws/frame** (sphères, cubes instanciés dessinés un par un, colonnes…). → `1500 × 240` dispatches de tuiles + `Join()` par draw. L'**overhead de dispatch/synchro** du thread pool écrasait tout gain de parallélisme. En mono-thread, pas de dispatch → plus rapide.

## Fix

Restreindre le tuilage/parallélisme à la **bounding box écran des triangles projetés du draw** (∩ scissor), pas au scissor plein écran :

```cpp
// NkSWFastPath.h — RasterizeList, avant le dispatch
float bbx0=1e30f,bby0=1e30f,bbx1=-1e30f,bby1=-1e30f;
for (uint32 t=0;t<triCount;++t) for(int k=0;k<3;++k){
    float x=tris[t].s[k].sx,y=tris[t].s[k].sy;
    if(x<bbx0)bbx0=x; if(x>bbx1)bbx1=x; if(y<bby0)bby0=y; if(y>bby1)bby1=y; }
int32 bx0=cx0,by0=cy0,bx1=cx1,by1=cy1;   // puis intersecter avec [floor(bb0), ceil(bb1)+1]
// doParallel & tuilage sur (bW=bx1-bx0, bH=by1-by0), seuil ~160×160 sinon inline mono-thread
```

Petit draw → petite bbox → **inline mono-thread** (pas de dispatch). Gros draw (plein-écran, sol, grille) → bbox large → parallélisé.

## Vérification

`NK_SW_PERF=1` : rasterisation **~690 ms → ~135 ms** (**~5×**), **rendu identique** (aucune régression visuelle). Frame ~1 FPS → plusieurs FPS.

## Règle générale

Pour un rasterizer tuilé, **toujours** borner le tuilage à la bounding box réelle de la primitive/du draw, jamais au scissor. Le coût de dispatch d'une tâche parallèle n'est amorti que si la tâche a assez de travail (seuil d'aire). Beaucoup de petits draws = préférer l'inline.

## Liens

- Correctif : `Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSWFastPath.h` (`RasterizeList`).
- Mesure : `NkSoftwareCommandBuffer::Execute` (opt-in `NK_SW_PERF=1`).
- Cache résolution binding (gain annexe ~6 %) : `NkSoftwareDevice::SwGetUBOBytes/SwGetTexAt` (cache thread_local, clé `SwBindGen()`).
