# Software (NkRHIDemoFullSL -bsw) — écran noir : shaders CPU stubbés

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur
- **Date** : 2026-06-07
- **Statut** : en cours (VM bytecode = la vraie solution)

## Symptôme

`NkRHIDemoFullSL.exe -bsw` : fenêtre noire, aucune géométrie. GL/VK/DX11/DX12 rendent
bien la même scène (cube + sphère + sol + ombre).

## Contexte

Backend Software (rasterizer CPU). Le device Software n'exécute pas de bytecode GPU :
il appelle des **lambdas C++** (`NkSWShader::vertFn` / `fragFn`, signatures dans
`NKRHI/Core/NkDescs.h`). Pour les backends GPU, NkSL est compilé en GLSL/HLSL/SPIR-V ;
pour le Software il faut un équivalent CPU.

## Cause racine

Dans `Applications/Sandbox/.../Base03/NkRHIDemoFullSL.cpp`, section
`targetApi == NK_GFX_API_SOFTWARE` (~ligne 1217), les lambdas CPU sont **stubbées /
commentées** : `vertFn` retourne un `NkVertexSoftware` vide (position non écrite),
`fragFn` a tout son corps en commentaire. Donc rien n'est projeté ni colorié → noir.

C'est un trou d'implémentation, pas un bug du rasterizer (le pipeline SW est prouvé :
d'autres démos SW rendent).

## Solution (en cours) — VM bytecode NkSL

Plutôt que de réécrire des lambdas manuelles, on branche la **VM NkSL** (module NKSL,
`VM/NkSLVM.{h,cpp}`, ISA `VM/NkSLByteCode.h`) :

1. Compiler les shaders NkSL vert+frag avec la cible `NkSLTarget::NK_BYTECODE`
   (`NkSLCompiler`), récupérer `res.bytecode`, `NkSLByteCodeDeserialize(...)` →
   `NkSLByteProgram`.
2. `vertFn` : `env.inputs = (const float*)(vdata + idx*stride)` (Vtx3D = 12 floats
   contigus, mêmes offsets que les `in` du générateur) ; `env.uniforms = (const uint8*)udata` ;
   `NkSLVM::Execute(vertProg, env)` ; écrire `out.position = {outputs[0..3]}` (gl_Position) et
   `out.attrs[k] = outputs[4+k]` (varyings).
3. `fragFn` : `env.inputs = interpolated.attrs` ; brancher `env.sampleTex/sampleShadow`
   sur des callbacks capturant les `NkSWTexture*` (shadow, albedo) ; `return {outputs[0..3]}`.

**Pré-requis générateur (à faire)** : réserver `gl_Position` en sortie VERTEX à
**offset 0 (count 4)**, varyings à partir de l'offset 4 — sinon l'assignation de
`gl_Position` lève « affectation symbole inconnu » (cf. `NkSLCodeGenBytecode::CollectSymbols`).
Garder les lambdas manuelles en fallback si la compilation bytecode échoue.

## Vérification

`NkRHIDemoFullSL -bsw` rend cube + sphère + sol + ombre, comparable à GL/VK.

## Liens

- `Kernel/Runtime/NKSL/src/NKSL/VM/` (NkSLByteCode.h, NkSLVM, NkSLByteCodeIO)
- `Kernel/Runtime/NKSL/src/NKSL/CodeGen/Bytecode/NkSLCodeGenBytecode.cpp` (gl_Position à réserver)
- `Kernel/Runtime/NKRHI/src/NKRHI/Core/NkDescs.h` (NkVertexSoftware, signatures vertFn/fragFn)
- `Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFullSL.cpp` (~l.1217)
