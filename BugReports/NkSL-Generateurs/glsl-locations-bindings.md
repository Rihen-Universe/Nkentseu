# NkSL → GLSL : écran noir OpenGL (location/binding manquants)

- **Catégorie** : NkSL-Generateurs
- **Sévérité** : bloquant
- **Date** : 2026-06-01
- **Statut** : résolu

## Symptôme

Le shader NkSL compile et tourne sur OpenGL (terminaison propre) mais **écran
noir** : rien de visible. Sur Vulkan, `CompileToSPIRV` échoue → fallback texte →
le device refuse → crash.

## Cause racine

Le générateur `NkSLCodeGenGLSL` (et `GLSLVulkan`) n'émettait :
- **pas de `layout(location = N)`** sur les `in`/`out` quand la source NkSL ne les
  précise pas. En OpenGL, le linker assigne alors des locations d'attributs dans un
  ordre **non garanti** → `aPos` peut ne pas être à la location 0 → positions
  mélangées → `gl_Position` dégénéré → écran noir.
- **pas de `layout(binding = N)`** sur les blocs UBO sans binding explicite.
- Pour Vulkan, GLSL **exige** des locations explicites sur toute l'interface in/out
  (sinon glslang refuse → pas de SPIR-V).

Bonus Vulkan : `#extension GL_KHR_vulkan_glsl : require` émis à tort (ce n'est PAS
une extension déclarable, c'est le nom de la spec) → glslang rejette.

## Solution

`NkSLCodeGenGLSL.cpp` / `NkSLCodeGenGLSLVulkan.cpp` :
- Auto-assigner `layout(location=N)` sur TOUT in/out (compteurs `mAutoInLoc` /
  `mAutoOutLoc` ajoutés au header, dans l'ordre de déclaration → le vertex-out et le
  fragment-in matchent).
- Auto-assigner `layout(binding=N)` aux blocs UBO via le compteur **partagé**
  `mAutoBinding` (le device GL utilise le n° de binding descriptor à la fois comme
  point UBO et unité texture → numérotation unifiée UBO=0/shadow=1/albedo=2).
- Retirer `#extension GL_KHR_vulkan_glsl : require` du générateur Vulkan.

Bonus démo : `NkRHIDemoFullSL` doit appeler `SetClearColor`/`SetClearDepth` avant
`BeginRenderPass` (sinon, en GL/Software, le depth n'est jamais reset → tout est
rejeté par le depth test → noir).

## Vérification

- OpenGL et Vulkan rendent la scène 3D.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/SL/NkSLCodeGenGLSL.cpp`, `NkSLCodeGenGLSLVulkan.cpp`, `NkSLCodeGen.h`
- Mémoire : `project_session_20260601_nksl_pivot.md`
