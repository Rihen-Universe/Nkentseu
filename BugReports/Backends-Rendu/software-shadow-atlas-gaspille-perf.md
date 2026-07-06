# Software — rendu du shadow atlas 4096² gaspillé (perf) — ombres non échantillonnées

- **Catégorie** : Backends-Rendu (perf)
- **Sévérité** : majeur (perf)
- **Date** : 2026-07-05
- **Statut** : contourné (skip) ; à revoir quand les ombres software seront implémentées

## Symptôme

Backend Software, scène NKRenderer avec ombres (VSM, `cascadeCount=1`, atlas 4096²) : rasterisation ~133 ms/frame (~5-6 FPS à 1280×720) même après les autres optims. Le pass **Shadows** consommait une grosse part du temps.

## Cause racine

Le pass **Shadows** rend les casters (sphères, cubes…) dans le **shadow atlas VSM 4096×4096** (depth-only). Deux problèmes en software :
1. **Les ombres ne sont PAS échantillonnées** par le fragment géométrie software (échantillonnage d'atlas VSM pas encore implémenté) → tout ce rendu est **100 % gaspillé**.
2. Le rasterizer software **ignore le viewport** (il utilise la taille de la RT cible). Le pass shadow projette chaque caster dans un **petit slot** de l'atlas via un viewport, mais le software rasterise chaque caster sur **tout l'atlas 4096²** → des **dizaines de millions de pixels depth** écrits pour rien, par caster.

## Fix (contournement)

Dans `NkSoftwareDevice::CreateShader`, détecter les shaders `Shadow*` (`debugName` contient "Shadow") et installer un fixed-function qui **NE dessine pas** (sommet clippé `w<0`) :

```cpp
const bool isShadowPass = (desc.debugName && std::strstr(desc.debugName, "Shadow"));
if (isShadowPass && ...) {
    sh.vertFn = [](...){ NkVertexSoftware o{}; o.position={0,0,0,-1}; return o; };  // clippé
    sh.fragFn = [](...){ return math::NkVec4{0,0,0,0}; };
}
```

Zéro impact visuel (les ombres portées n'étaient de toute façon pas affichées en software).

## Vérification

`NK_SW_PERF=1` : rasterisation **~133 → ~100 ms** (~25 %). FPS **~5.7 → ~9.7** (HUD dt 102 ms). Rendu identique.

## Règle générale / à faire

Quand un pass rend dans une cible qui n'est **jamais lue** par le backend, c'est du gaspillage pur — le skipper. Ici, à terme, **implémenter l'échantillonnage de l'atlas VSM** (cf. `Resources/NKRenderer/Shaders/Include/NkShadowAtlas.glsli` : `slots[].shadowMatrix`, `tileUV`, compare depth) ET **respecter le viewport** dans le rasterizer software (rasteriser le caster uniquement dans le slot) — alors le pass shadow redeviendra utile ET raisonnable en coût.

Piste liée : le **clear** de l'atlas 4096² (BeginRenderPass) reste (~6 ms) ; skippable aussi tant que l'atlas n'est pas lu.

## Liens

- Correctif : `Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSoftwareDevice.cpp` (handler `Shadow*`).
- Autres optims perf software : `software-multithread-plus-lent-tuilage-scissor.md` (tuilage bbox), + nearest sur copies plein-écran, cache binding thread_local.
- Système d'ombre : `Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Shadow/NkVirtualShadowMaps.cpp`.
