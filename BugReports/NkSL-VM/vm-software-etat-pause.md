# VM bytecode NkSL (Software) — état à la mise en pause (2026-06-07)

- **Catégorie** : NkSL-VM
- **Sévérité** : majeur (fonctionnel mais perf + résidus lighting)
- **Date** : 2026-06-07
- **Statut** : EN PAUSE (opt-in `NK_SW_VM=1`), à reprendre

## Ce qui MARCHE

NkSL → `NkSLTarget::NK_BYTECODE` (générateur `NkSLCodeGenBytecode`) → sérialisé →
`NkSLByteCodeDeserialize` → **`NkSLVM::Execute` par vertex/fragment** dans le device
Software (`NkRHIDemoFullSL -bsw`, opt-in `NK_SW_VM=1`). La scène **rend correctement
la géométrie** (cube + sphère + sol), le **sol texturé brique est net et éclairé**,
gl_Position / varyings / UBO std140 / samplers (albedo + shadow PCF) fonctionnent.

C'est la **preuve du concept** : une seule source NkSL exécutée sur CPU par
interprétation bytecode, **sans compilateur C++ embarqué** (≠ cible C++ source qui
exigerait une recompilation runtime).

## Bugs déjà CORRIGÉS cette session (générateur + VM)

1. **Table de symboles** : `NkUnorderedMap` → table linéaire `SymTable` (le rehash
   invalidait `Find` → tous les symboles « inconnus »). `NkSLCodeGenBytecode.h`.
2. **gl_Position** réservé en sortie VERTEX offset 0 (count 4), varyings dès offset 4.
3. **`++`/`--`** : tombaient dans un `OP_MOV` (pas d'incrément ni write-back) →
   **boucle for infinie** (PCF) → freeze. Émettent maintenant `OP_ADD/SUB` en place.
4. **Construction `vecN(...)` registres non contigus** : `base` capturé avant
   l'évaluation des args (qui allouent des registres) → `OP_CONSTRUCT` lisait les
   mauvais registres → `vec4(aPos,1.0).w = aPos.x` au lieu de 1.0 → **transform
   cassé**. Fix : évaluer tous les args, PUIS bloc contigu.
5. **`mat3(mat4)`** : `OP_CONSTRUCT` prenait les 9 premiers floats du mat4 au lieu de
   la **sous-matrice 3×3** (col 0..2 × lignes 0..2) → matrice des normales corrompue →
   **pas de lumière**. Fix dans `NkSLVM.cpp` OP_CONSTRUCT (+ cas mat4(mat3)).
6. **Garde-fou VM** : budget d'instructions borné (1M) → un bytecode mal formé ne
   gèle plus le process (bail + `return false`).

## Ce qui RESTE (à reprendre)

- **Lighting résiduel** : dessus du cube **magenta**, **liseré arc-en-ciel** sur la
  sphère. Suspects : terme spéculaire `pow(dot(N,H),32)`, normalisation des normales
  à angle rasant, ou registre non initialisé dans un chemin. À débugger avec un
  fragment ciblé (logger N, L, H, diff, spec).
- **Performance** : la VM **interprète** le bytecode **par pixel** → bien plus lente
  que les lambdas natives (surtout en Debug). Pistes : build Release, dispatch
  threadé du rasterizer SW, cache du programme, à terme un JIT ou compilation C++.
- **Texture sol** : légère déformation perçue (probablement mips absents / aliasing
  à angle rasant, cf. fiche damier DX11) — à confirmer une fois le lighting propre.

## Comment réactiver / tester

- Lancer `NkRHIDemoFullSL.exe -bsw` avec `NK_SW_VM=1` (env). Par défaut (sans la var)
  le Software utilise les **lambdas manuelles natives** (rapides, correctes).
- Checkpoints one-shot dans `vertFn`/`fragFn` (log `[NkSL-VM] vert#/frag# ok= pos/color`).

## Liens

- `Kernel/Runtime/NKSL/src/NKSL/VM/` (NkSLByteCode.h, NkSLVM.{h,cpp}, NkSLByteCodeIO.{h,cpp})
- `Kernel/Runtime/NKSL/src/NKSL/CodeGen/Bytecode/NkSLCodeGenBytecode.{h,cpp}`
- `Applications/Sandbox/src/DemoNkentseu/Base03/NkRHIDemoFullSL.cpp` (~l.1240, gate `NK_SW_VM`)
- [../Backends-Rendu/software-nksl-ecran-noir-lambdas-stub.md](../Backends-Rendu/software-nksl-ecran-noir-lambdas-stub.md)
