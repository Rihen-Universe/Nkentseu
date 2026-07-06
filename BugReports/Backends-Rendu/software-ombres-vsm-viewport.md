# Software — ombres portées VSM (viewport + passe shadow + échantillonnage atlas)

- **Catégorie** : Backends-Rendu
- **Sévérité** : évolution majeure (fidélité)
- **Date** : 2026-07-06
- **Statut** : implémenté (avec limites documentées)

## Contexte

Backend Software (via NKRenderer, `renderdemo --demo=2 -bsw`). Avant : la scène s'affichait mais **sans ombres portées** — les objets « flottaient ». La passe Shadow était volontairement **SKIP** (atlas VSM 4096² rendu mais jamais échantillonné, et le software ignorait le viewport → chaque caster couvrait tout l'atlas = gaspillage).

## Ce qui a été fait (3 parties)

### 1. Support du viewport dans le rasterizer
Le software ignorait `SetViewport` (no-op) et mappait toujours le NDC sur la cible entière. Ajout d'un viewport optionnel à `swraster::Project` (struct `NkSWViewport{x,y,w,h,flipY}`, `w==0` ⇒ plein RT) :

```
sx = x + (ndcx*0.5+0.5)*w
sy = flipY ? y + (1-ndcy)*0.5*h : y + (ndcy*0.5+0.5)*h
```

Propagé : `NkSoftwareCommandBuffer::SetViewport` → `dev->SwSetViewport` → `NkSWResolvedResources.vp*` (rempli dans `ResolveResources` **seulement** si viewport sous-rect : `width>0 && !pleinRT`) → `RasterizeList` → `ClipProject` → `Project`. Le mapping plein-cadre historique est **inchangé** (garde `vpW>0`) → **zéro régression** sur le rendu principal (vérifié avant/après). Le tuilage MT sur bbox restreint automatiquement le travail à la tuile.

### 2. Passe Shadow (rendu depth des casters)
FBO **depth-only** (colorBuf null) : le rasterizer écrit la depth sans appeler le fragFn (chemin `depthOnly`). Le shader `Shadow_DepthOnly` devient un vertFn fixed-function :
`clip = lightVP(push const @0) · model(ObjectUBO set1,b1 @0) · localPos`.
`depthRemap=1` en software (`DepthIsZeroToOne()=false` pour `NK_GFX_API_SOFTWARE`, renderMatrix en [-1,1]) → `Project` fait `sz=z*0.5+0.5`, cohérent avec l'échantillonnage.

### 3. Échantillonnage de l'atlas (fragment géométrie)
Le fragFn 3D récupère l'atlas depth `dev->SwFindTexInSet(0,11)` (SwFindTexInSet ne filtre PAS le format depth, contrairement au texBatch de ResolveResources) et le ShadowSlots UBO `dev->SwGetUBOBytes(0,3)`. Layout std140 (voir `NkVirtualShadowMaps.cpp` `ShadowSlotsUBOBlock`) :
- `slots[256]` × 112 o : `shadowMatrix`@0, `tileUV`@64 (minU,minV,maxU,maxV), `lightPosOrDir`@80, `packedIds`@96 ;
- `firstSlotPerLight`@28672, `slotCountPerLight`@28800 (4 int packés/vec4) ;
- `globalCfg`@28928 (.x=numSlots .z=depthRemap .w=shadowYFlip), `biasParams`@28944 (.x=shadowBias).

Par lumière : sélection de cascade par **couverture** (1re tuile dont `|ndc.xy|<=1`), `uv = tileUV.xy + (ndc.xy*0.5+0.5)*(tileUV.zw-tileUV.xy)` (pas de Y-flip, software≠DX), `fragD = ndcz*0.5+0.5`, **PCF 3×3** (`fragD - bias > stored ? ombre : lumière`). Le facteur atténue la contribution **directe** (diffus+spéc), pas l'ambiant.

**Cohérence intrinsèque** : `shadowMatrix == renderMatrix`, même remappage [-1,1]→[0,1] à l'écriture (Project) et à la lecture (shader). L'alignement pixel atlas ↔ UV lu est exact (`tileUV = tileRect/atlasSize`). Indépendant de la caméra principale.

## Perf
~8 FPS (sans ombres) → ~5 (PCF 3×3) → **~6** après l'optimisation `if (ndl>0) sampleShadow()` (pas de PCF sur l'hémisphère non éclairé, exact car la contribution directe y est nulle). 1280×720.

## 4. Casters instanciés (gl_InstanceID) — ombres des cubes

Ajout ultérieur (même jour). **Découverte** : la géométrie instanciée passe par défaut par **expansion object-UBO** (`FlushInstanced`, N draws avec instanceCount=1 ; `gpuInst=0` sauf `NK_INSTANCING_GPU_1DRAW=1`) → les cubes s'affichaient déjà correctement (chacun son ObjUBO). MAIS le pass **shadow** instancié utilise TOUJOURS le pipeline batché `DrawAll(mesh, n)` (1 draw, n instances, non gaté) → les ombres des cubes étaient skippées.

Fix — support générique de l'instance-index en software :
- `NkSoftwareDevice` : `mCurInstance` + `SwSetCurInstance`/`SwCurInstance` (gl_InstanceID).
- `NkSWShader::usesInstancing` : le vertFn dépend de l'instance.
- `ExecuteDrawFast`/`ExecuteDrawIndexedFast` : quand `usesInstancing && instanceCount>1`, **re-génèrent les sommets par instance** (`SwSetCurInstance(inst)` avant chaque `RasterizeList`). Sinon (défaut), sommets calculés **une seule fois** → chemin non-instancié **strictement inchangé**.
- `ShadowInstanced_DepthOnly` dé-skippé : vertFn lit `SwGetUBOBytes(1,4)` (InstanceUBO) @ `SwCurInstance()*64` (mat4 col-major), `clip = lightVP(pushConst) · instModel · localPos` (l'ObjUBO(1,1) est l'identité côté GPU).

Résultat : grille d'ombres au sol sous l'arc de cubes instanciés (une ombre par cube). Perf ~4.9 FPS (re-gen des sommets par instance pour le pass shadow).

## 5. Perf — breakdown mesuré (NK_SW_PERF, MIN du replay par frame, 1280×720)

Toggles de diagnostic ajoutés : `NK_SW_NOSHADOW=1` (coupe l'échantillonnage dans le fragment géométrie), `NK_SW_NOSHADOWPASS=1` (coupe le rendu des casters dans l'atlas), `NK_SW_PCF=2` (2×2 au lieu de 3×3).

Mesures (MIN, bruit de fond ±40 ms car process lancé en arrière-plan) :
- **Tout activé** : ~240-285 ms (le « défaut » est le plus bruité).
- **Sans échantillonnage d'ombre** (`NK_SW_NOSHADOW`) : ~234 ms → **sampling ≈ 50 ms**.
- **Sans passe shadow** (`NK_SW_NOSHADOWPASS`) : **~187 ms (stable)** → **rendu de la passe ≈ 50-99 ms**.
- ⇒ **Base pipeline (géométrie + tonemap/FXAA, sans ombres) ≈ 135-187 ms.**

**Constats :**
1. Les ombres ~**doublent** le coût de la frame (pass + sampling). Ce n'est PAS gratuit (un run antérieur anormalement bruité l'avait faussement suggéré).
2. Le **gros poste = la passe shadow** (~50-99 ms). Cause : elle **re-rend les 14 slots de l'atlas chaque frame** — le clear global efface même les slots « cached », donc NkRenderer les re-rend (`NkVirtualShadowMaps.cpp` : *V2 todo : ClearRect + skip cached rerender*). **Levier en code partagé** (NKRenderer), hors backend software.
3. Le **choix du noyau PCF n'est PAS un levier** : `3×3 ≈283 ms` vs `2×2 ≈309 ms` = bruit. Gardé **3×3** (meilleure qualité).
4. **Cache de bindings** ajouté au fragment 3D (résolution UBO/texture 1×/draw au lieu de par pixel, via `SwBindGen`) : sain et output-identique, mais gain sous le bruit (le lookup avait déjà un cache interne).

**Piège de mesure CRITIQUE** : `NK_SW_PERF=1` logge 1 ligne/frame dans un app.log de plusieurs Mo → I/O synchrone par frame qui **fausse la HUD FPS** (lectures 5-8 artificiellement basses). Sans ce log, la HUD est à **60 FPS** (vsync-capé) ; mesurer le coût raster via le replay NK_SW_PERF (MIN, pas avg).

## Limites assumées
- Une seule lumière directionnelle validée pour l'instant (le mécanisme couvre spots/points par la même sélection de tuile, à valider).
- Biais de profondeur fixe (pas de slope/normal bias).
- **Optimisation perf restante = la passe shadow** (re-render des slots cached, code NKRenderer partagé) et/ou SIMD du fragment par pixel. Le PCF et les lookups ne sont pas des leviers.

## Vérification
Captures : sphères + colonnes + cube central projettent des ombres douces au sol, scène ancrée. Rendu principal identique avant/après le support du viewport (non-régression). Aucune modification des backends GPU ni de `NKRenderer/Tools/Shadow/`.

## Fichiers
`Kernel/Runtime/NKRHI/src/NKRHI/Software/` : `Raster/NkSWRasterCore.h` (Project+viewport, ClipProject), `NkSWFastPath.h` (propagation vp), `NkSoftwareCommandBuffer.cpp` (SetViewport), `NkSoftwareDevice.h` (mCurViewport), `NkSoftwareDevice.cpp` (shader Shadow depth-only + sampling VSM dans le fragFn 3D).

## Valorisation
Publication + article : `D:/Rihen/Rodolf/Publications/09_2026-07-06_software-shadows/`.
