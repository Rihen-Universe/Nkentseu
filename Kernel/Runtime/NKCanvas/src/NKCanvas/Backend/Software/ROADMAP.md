# NKCanvas — Backend Software (rasterizer 2D CPU) — Roadmap

> Roadmap dédiée au **rendu logiciel 2D** de NKCanvas (dossier
> `src/NKCanvas/Backend/Software/`). Objectif : un rendu 2D CPU **fiable et net**
> (anti-aliasé, filtrage correct, vrai alpha), sans carte graphique.
> Architecture **autonome de NKRHI** (aucune dépendance croisée).
> Périmètre strict : **on ne touche qu'au backend Software** de NKCanvas.
> Dernière mise à jour : 2026-07-04.

État actuel : rasterizer **scanline 2D mono-thread** présenté sur surface native
double-bufferisée. `NkSoftwareContext` ([NkSoftwareContext.cpp](NkSoftwareContext.cpp))
gère la présentation multi-OS ; `NkSoftwareRenderer2D`
([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp)) rasterise les triangles issus
du batcher commun (`NkBatchRenderer2D`). Le rendu marche mais reste **aliasé**,
en **nearest-neighbor** (`SetFilter` no-op), sans **vrai canal alpha** en sortie.
Un rasterizer barycentrique + depth **dormant** existe dans le framebuffer
(`NkSoftwareFramebuffer::FillTriangleInterpolated`) mais n'est pas utilisé par le 2D.

---

## Synthèse

| Domaine / Tâche                                             | Statut  | Phase | Effort | Priorité |
|------------------------------------------------------------|---------|-------|--------|----------|
| Rasterizer scanline 2D (`RasterizeTriangle`)               | ✅      | —     | —      | —        |
| Batching par (texture, blend) via `NkBatchRenderer2D`      | ✅      | —     | —      | —        |
| Blend `NK_ALPHA` / `NK_ADD` / `NK_MULTIPLY`                | ✅      | —     | —      | —        |
| SIMD spans (SSE2/NEON, `NkSWPixel.h`)                      | ✅      | —     | —      | —        |
| Blit sprite nearest Q16.16 fixed-point                     | ✅      | —     | —      | —        |
| Scissor/clip (framebuffer + stack renderer)                | ✅      | —     | —      | —        |
| Présentation native multi-OS + fast memcpy Windows         | ✅      | —     | —      | —        |
| **Anti-aliasing (couverture / supersampling)**             | ❌      | 2     | L      | P1       |
| **Sampling bilinéaire (`SetFilter` no-op)**                | ❌      | 1     | S      | P1       |
| **Vrai alpha en sortie (dst.a forcé 255)**                 | ❌      | 1     | M      | P1       |
| Cohérence arrondi SIMD vs scalaire (`>>8` vs `/255`)       | ❌      | 1     | S      | P1       |
| Bug swizzle présentation XCB (couleurs fausses)            | ❌      | 1     | S      | P1       |
| Heuristique `ColorTo255` [0..1] vs [0..255] fragile        | ❌      | 1     | S      | P2       |
| Unifier sur rasterizer barycentrique (dormant)             | 🔶      | 1     | M      | P2       |
| Multi-thread tuilé (scanlines / tuiles)                    | ⏳      | 3     | L      | P2       |
| `BlendSpanAdd` réellement SIMD (scalaire aujourd'hui)      | ⏳      | 3     | S      | P2       |
| Primitives courbes : bézier / paths / gradients            | ⏳      | 3     | L      | P2       |
| Cercles/ellipses analytiques (approx. polygonale)          | ⏳      | 3     | M      | P3       |
| Interpréteur compute CPU (`Dispatch` stub)                 | ⏳      | 4     | XL     | P3       |
| Shaders programmables 2D (backend « Unsupported »)         | ⏳      | 4     | XL     | P3       |

Légende : ✅ livré · 🔶 partiel/dormant · ⏳ à venir · ❌ bug/manquant · 🚫 hors périmètre.

---

## ✅ Livré (état réel du code)

- **Rasterizer scanline** `NkSoftwareRenderer2D::RasterizeTriangle`
  ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):325) : tri Y, split long-edge,
  interpolation X + RGBA + UV par `EdgeStep`, remplissage de spans. Fast lanes couleur
  uniforme → `FillSpanOpaque` / `BlendSpanAlpha` / `BlendSpanAdd` (:425-446).
- **Batcher commun** `NkBatchRenderer2D` : tout (rects, cercles, lignes, texte)
  tessellé en triangles, groupé par (texture, blendMode), `kMaxVertices=65536`.
- **Blit sprite** `BlitTexture` ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):199) :
  nearest + pas UV **Q16.16 fixed-point**, tint + alpha ; fast path sprites non tournés.
- **Blend modes** : `NK_ALPHA` (source-over), `NK_ADD`, `NK_MULTIPLY`
  ([NkSWPixel.h](NkSWPixel.h) + inline :470-501).
- **SIMD** ([NkSWPixel.h](NkSWPixel.h)) : `FillSpanOpaque` (4 px/it), `BlendSpanAlpha`
  (2 px/it SSE2), format **octet-natif** (BGRA Windows / RGBA ailleurs) pour éviter
  toute conversion.
- **Présentation** : Windows = `memcpy` direct + `BitBlt` ; X11/Wayland/XCB/Android/
  macOS/Emscripten variantes. Double-buffer `Swap` sur `EndFrame`.
- **Clip** : rect framebuffer + scissor stack renderer, appliqués aux spans.

---

## 🔶 Dormant (présent mais non branché par le 2D)

- **`NkSoftwareFramebuffer::FillTriangleInterpolated`** ([NkSoftwareContext.h](NkSoftwareContext.h):224) :
  rasterizer **edge-function/barycentrique** avec winding CCW/CW + depth test, plus
  `DrawLine` Bresenham et un **depth buffer** complet — machinerie 3D minimale mais
  **inutilisée** par `NkSoftwareRenderer2D`. Candidat d'unification (Phase 1) pour une
  base de rasterisation unique et correcte.

---

## ⏳ À venir — plan par phases (base d'implémentation)

### Phase 1 — Fiabilité (corrections, cohérence)

1. **Vrai canal alpha en sortie** : aujourd'hui `BlendPixel` force `dst.a=255`
   ([NkSWPixel.h](NkSWPixel.h):102,107) → pas de compositing premultiplié correct
   (bloque render-to-texture 2D fiable). Passer en alpha premultiplié optionnel.
2. **Sampling bilinéaire** : `NkSW_SetTextureFilter`/`SetWrap` sont des **no-ops**
   ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):76-81) ; le sampler est nearest
   malgré le commentaire « bilinéaire » périmé (:116). Implémenter bilinéaire + wrap.
   Réf. Scratchapixel : `interpolation/bilinear-filtering`,
   `3d-basic-rendering/introduction-to-texturing/introduction-to-texturing-texture-filtering`.
3. **Cohérence arrondi** SIMD (`>>8`) vs résidu scalaire (`/255`) dans `BlendSpanAlpha`
   ([NkSWPixel.h](NkSWPixel.h):219-275) → écart ≤1 par pixel, à uniformiser.
4. **Bug présentation XCB** : depth 24 câblé + RGBA poussé sans swizzle
   ([NkSoftwareContext.cpp](NkSoftwareContext.cpp):494) → couleurs fausses sur certains
   visuels. Aligner sur le swizzle X11/Wayland.
5. **`ColorTo255`** heuristique [0..1] vs [0..255] ([NkSoftwareContext.h](NkSoftwareContext.h):353)
   → remplacer par une convention explicite.
6. **Unifier (option)** le 2D sur `FillTriangleInterpolated` barycentrique dormant pour
   une seule source de vérité (règle top-left, sous-pixel).
   Réf. : `rasterization-practical-implementation/rasterization-stage`.

### Phase 2 — Qualité (rendu 2D net)

7. **Anti-aliasing** : couverture analytique par arête (ou supersampling 4×) — aucune AA
   aujourd'hui, bords `ceilf/floorf` durs ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):340).
   Réf. échantillonnage : `monte-carlo-methods-in-practice/monte-carlo-methods`.

### Phase 3 — Puissance

8. **Multi-thread tuilé** : paralléliser scanlines/tuiles via NKThreading (aucun MT
   aujourd'hui). 9. **`BlendSpanAdd` vraiment SIMD** (branche SSE2 actuellement scalaire,
   [NkSWPixel.h](NkSWPixel.h):169-186). 10. **Primitives courbes** : bézier, paths,
   gradients (absents de `NkIRenderer2D.h`), cercles/ellipses analytiques.
   Réf. courbes : `geometry/bezier-curve-rendering-utah-teapot/bezier-curve`.

### Phase 4 — Extensions (optionnel)

11. **Compute CPU réel** : `NkSoftwareComputeContext::Dispatch` est un **no-op** volontaire
    ([NkSoftwareComputeContext.cpp](NkSoftwareComputeContext.cpp):200) (shaders stockés en
    source brute, jamais compilés). 12. **Shaders 2D programmables** : backend actuellement
    « Unsupported » ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):119).

---

## ❌ Bugs / lacunes connues

1. **Aucun AA** (bords aliasés partout).
2. **Sampling nearest only** ; `SetFilter`/`SetWrap` no-ops ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):76).
3. **Sortie alpha forcée 255** ([NkSWPixel.h](NkSWPixel.h):102) → pas de vrai compositing.
4. **Backend texture factice** : create renvoie un ID bidon, update/delete no-ops
   ([NkSoftwareRenderer2D.cpp](NkSoftwareRenderer2D.cpp):63-82) ; sampling lit `GetCPUPixels()` direct.
5. **Écart arrondi SIMD/scalaire** dans `BlendSpanAlpha`.
6. **XCB** : swizzle/depth présentation potentiellement faux.
7. **VSync** : `SetVSync`/`GetVSync` = flag stocké seulement, ne cadence pas la présentation
   ([NkSoftwareContext.cpp](NkSoftwareContext.cpp):154).
8. **Compute** : `Dispatch`/`WaitIdle`/`MemoryBarrier` no-ops mais `SupportsCompute()` renvoie true.
9. **Dead code** : `ConvertRGBAtoBGRA_ForGDI` ([NkSoftwareContext.cpp](NkSoftwareContext.cpp):48) non utilisé.

---

## Dépendances

- **NKThreading** (Phase 3, tuilage) — déjà dans le moteur.
- **NKMemory** : `NkAlloc`/`NkFree` uniquement (jamais heap CRT).
- **Autonome de NKRHI** : aucune dépendance ni symbole NKRHI (à préserver).
  Aucune modif hors `src/NKCanvas/Backend/Software/`.

---

## Références Scratchapixel (archive locale)

Chemin : `D:\Scratchapixel\markdown\lessons\`. Pivots 2D : `interpolation/bilinear-filtering`,
`introduction-to-texturing/*` (filtrage, color-space), `rasterization-practical-implementation/*`
(règle top-left, sous-pixel), `geometry/bezier-*` (courbes). Usage **personnel hors ligne** ;
contenu © Scratchapixel (CC BY-NC-ND 4.0) — ne pas redistribuer.
