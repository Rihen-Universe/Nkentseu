# OpenGL — ombres manquantes / dépendantes de la hauteur (glClipControl vs clipZ01 d'ombre)

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur
- **Date** : 2026-07-03
- **Statut** : résolu (OpenGL) ; VK / DX11 / DX12 étaient déjà corrects

## Symptômes

- Sur **OpenGL uniquement** : seule une PARTIE des objets projette une ombre au sol ;
  les autres n'en ont aucune. VK / DX11 / DX12 rendent toutes les ombres correctement.
- Indice décisif : l'ombre d'un caster qui **monte/descend en Y** (le cube central
  rotatif de Demo3D, `y = 0.5 + sin(t)*0.2`) **disparaît quand il est haut** et
  **réapparaît progressivement quand il est bas**.
- Le scintillement/glissement apparent en bougeant la caméra (les casters entrent/sortent
  de la moitié rendue) — masquait le vrai problème et ressemblait à du swimming/clipping
  de cascade.

## Cause racine

Le backend OpenGL force, à l'init du device, la convention de profondeur de Vulkan :

```cpp
// Kernel/Runtime/NKRHI/src/NKRHI/OpenGL/NkOpenglDevice.cpp:453
if (glClipControl) glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
```

→ le **NDC z d'OpenGL est [0,1]** (comme DX/VK), PAS le `[-1,1]` historique d'OpenGL.

Mais le système d'ombre (`NkVirtualShadowMaps`) traitait GL comme du `[-1,1]` :
`DepthIsZeroToOne()` renvoyait `false` pour GL, donc :
1. `ApplyDepthClipCorrection()` NE bakait PAS `clipZ01` dans les matrices d'ombre
   (directionnelle / spot / point) → la matrice ortho produisait un clip z ∈ [-w, w].
2. `globalCfg.z = depthRemap = 1` → le shader `NkShadowAtlas.glsli` refaisait `z*0.5+0.5`
   au sampling.

Avec `glClipControl(GL_ZERO_TO_ONE)` actif, un clip z < 0 est **clippé** : la **moitié
« near » du frustum de la lumière disparaît**. Les casters situés dans cette moitié ne
sont jamais rendus dans l'atlas d'ombre → **ombres manquantes**. Le cube qui bobbe en Y
traverse la frontière z = 0 dans l'espace lumière → ombre qui apparaît/disparaît selon la
hauteur. Le **rendu principal (perspectif)** était peu affecté (la projection perspective
concentre la géométrie visible en z > 0), mais l'**ortho d'ombre linéaire** coupait pile
la moitié → bug **GL only**.

## Fix

`DepthIsZeroToOne()` inclut désormais OpenGL (puisque le device force `glClipControl(
GL_ZERO_TO_ONE)`), exactement comme VK/DX :

```cpp
// Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Shadow/NkVirtualShadowMaps.cpp:109
return api == NK_GFX_API_VULKAN
    || api == NK_GFX_API_DX11
    || api == NK_GFX_API_DX12
    || api == NK_GFX_API_OPENGL;   // <- glClipControl(ZERO_TO_ONE) -> GL est en [0,1]
```

Effet : `clipZ01` est baké dans les matrices d'ombre (dir/spot/point) → clip z ∈ [0, w]
(plus de clipping de la moitié near) ET `depthRemap = 0` (le shader ne refait pas le
remap, la profondeur atlas est déjà [0,1]). Une ligne, backend GL aligné sur les 3 autres.

## Règle générale

Dès qu'un backend force une convention de profondeur (ici `glClipControl` côté RHI GL),
**tous** les chemins qui construisent des matrices de projection (rendu principal ET
shadow map) doivent être cohérents avec cette convention. Le piège : le perspectif tolère
l'incohérence (peu de géométrie en z < 0), l'ortho non (coupe franchement la moitié).

## Liens

- Correctif : `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Shadow/NkVirtualShadowMaps.cpp`
  (`DepthIsZeroToOne`, `ApplyDepthClipCorrection`, `globalCfg.z`).
- Source de la convention GL : `Kernel/Runtime/NKRHI/src/NKRHI/OpenGL/NkOpenglDevice.cpp:453`.
- Sampling d'ombre : `Resources/NKRenderer/Shaders/Include/NkShadowAtlas.glsli` (`globalCfg.z`).
