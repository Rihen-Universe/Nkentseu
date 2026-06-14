# NKSL

> Couche **Runtime** · Le langage de shaders maison du moteur : AST et types du langage,
> frontend (lexer / parser / sémantique), compilateur multi-cibles (GLSL/HLSL/MSL/SPIR-V),
> conversion de shaders, annotations de matériaux et machine virtuelle bytecode.

NkSL est le **langage de shaders** de Nkentseu : une syntaxe GLSL-like enrichie d'annotations
`@` (`@binding`, `@stage`, `@entry`, `@param`…) que le moteur compile **une fois** vers toutes
les API graphiques cibles. Au lieu d'écrire un shader par backend, on écrit du NkSL et le
compilateur émet du GLSL (OpenGL et Vulkan), du HLSL (DX11 SM5 et DX12 SM6+), du MSL (Metal,
natif ou via SPIRV-Cross), du SPIR-V, du C++ logiciel ou un **bytecode** exécuté par une VM à
registres pour le rasterizer Software. C'est la pièce qui rend NKRHI et NKRenderer réellement
cross-platform au niveau des shaders.

Le pipeline complet est : **source NkSL → lexer → parser (AST) → analyse sémantique
(réflexion) → génération de code (backend par cible)**. À côté, deux services autonomes : la
**conversion** de shaders existants (GLSL Vulkan → SPIR-V → HLSL/MSL via glslang/SPIRV-Cross)
et la **VM bytecode** (compile NkSL → `NkSLByteProgram` → `.nkbc` → exécution sans toolchain).

- **Namespace** : `nkentseu` (pas de sous-namespace ; NKRHI réutilise même `NkSLStage` via
  `using NkShaderStage = NkSLStage;`)
- **Header parapluie** : `#include "NKSL/NKSL.h"`

> À noter : ce module NKSL est le **vrai compilateur** (AST/types/sémantique/codegen ci-dessous),
> à distinguer du transpileur NkSL **ad-hoc** de NKRenderer (transformation texte ad-hoc, autre
> dialecte). Les deux portent le nom « NkSL » mais ne partagent pas de code.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Comprendre l'arbre syntaxique, les types du langage, les capabilities par cible | [Types & AST](NKSL/Types-AST.md) |
| Lexer, parser, analyse sémantique, table des symboles (le frontend) | [Le frontend](NKSL/Frontend.md) |
| Compiler vers GLSL/HLSL/MSL/SPIR-V, convertir des shaders, gérer les annotations `@` | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| Émettre/charger du bytecode et l'exécuter (rasterizer Software) | [La machine virtuelle](NKSL/VM.md) |

Chaque page liste l'**API publique réelle** (enums, structs, classes, free functions) avec ses
pièges d'ownership et ses idiomes d'utilisation.

---

## Aperçu des familles

- **Core — Types & AST** (`Core/NkSLTypes.h`, `Core/NkSLAST.h`, `Core/NkSLFeatures.h`) — les
  enums fondamentaux (`NkSLTarget`, `NkSLStage` bitmask, `NkSLBaseType`, qualificateurs), les
  structs de binding/réflexion/options de compilation, l'arbre syntaxique polymorphe
  (`NkSLNode` et ses ~25 dérivés) et les capabilities par cible (`NkSLFeatureCaps`) + générateurs
  de stages avancés (mesh/tess/ray-tracing/bindless).
- **Frontend** (`Frontend/NkSLLexer.h`, `NkSLParser.h`, `NkSLSemantic.h`, `NkSLSymbolTable.h`) —
  `NkSLLexer` (tokens GLSL + annotations `@`), `NkSLParser` (descente récursive bornée
  anti-stack-overflow → AST), `NkSLSemantic` (typage, swizzle, surcharges, cohérence inter-stages),
  `NkSLSymbolTable` (scopes imbriqués, résolution de noms).
- **Compilateur & génération** (`Compiler/NkSLCompiler.h`, `Compiler/NkGLSLCompiler.h`,
  `CodeGen/NkSLCodeGen.h`, `ShaderConvert/NkShaderConvert.h`, `ShaderConvert/NkShaderAnnotations.h`) —
  la façade `NkSLCompiler` (compile/réflexion/multi-cibles + cache mémoire), les 8 backends
  `NkSLCodeGen*` (GLSL/GLSL-VK/HLSL-DX11/HLSL-DX12/MSL/MSL-SPIRVCross/C++), la conversion de
  fichiers (`NkShaderConverter`, cache disque `.nksc`) et les annotations de matériaux (`@param`,
  `@color`, `@range`…).
- **VM bytecode** (`VM/NkSLByteCode.h`, `VM/NkSLByteCodeIO.h`, `VM/NkSLVM.h`) — l'ISA à registres
  (`NkSLOp`), le programme `NkSLByteProgram` (un stage), la sérialisation `.nkbc` et l'interpréteur
  `NkSLVM::Execute` qui rend un vertex/fragment pour le rasterizer Software.

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKSL.h` | Parapluie (inclut tout). | — |
| `Core/NkSLTypes.h` | Enums (`NkSLTarget`/`NkSLStage`/`NkSLBaseType`…), `NkSLBinding`, `NkSLCompileResult/Options`, réflexion. | [Types & AST](NKSL/Types-AST.md) |
| `Core/NkSLAST.h` | `NkSLNode` + nœuds dérivés (AST polymorphe). | [Types & AST](NKSL/Types-AST.md) |
| `Core/NkSLFeatures.h` | `NkSLFeatureCaps`, layouts mesh/tess/RT/bindless, générateurs avancés. | [Types & AST](NKSL/Types-AST.md) |
| `Frontend/NkSLLexer.h` | `NkSLTokenKind`, `NkSLToken`, `NkSLLexer`. | [Le frontend](NKSL/Frontend.md) |
| `Frontend/NkSLParser.h` | `NkSLParser` (AST depuis tokens). | [Le frontend](NKSL/Frontend.md) |
| `Frontend/NkSLSemantic.h` | `NkSLSemantic`, `NkSLSemanticResult`. | [Le frontend](NKSL/Frontend.md) |
| `Frontend/NkSLSymbolTable.h` | `NkSLResolvedType`, `NkSLSymbol`, `NkSLScope`, `NkSLSymbolTable`. | [Le frontend](NKSL/Frontend.md) |
| `Compiler/NkSLCompiler.h` | `NkSLCompiler`, `NkSLCache`, `NkSLShaderLibrary`. | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| `Compiler/NkGLSLCompiler.h` | `NkGLSLToSPIRV` (glslang in-tree). | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| `CodeGen/NkSLCodeGen.h` | `NkSLCodeGenBase` + 8 backends, `NkSLReflector`. | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| `ShaderConvert/NkShaderConvert.h` | `NkShaderConverter`, `NkShaderFileResolver`, `NkShaderCache`. | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| `ShaderConvert/NkShaderAnnotations.h` | `NkShaderAnnotationParser`, métadonnées de matériaux. | [Compilateur & génération](NKSL/Compiler-CodeGen.md) |
| `VM/NkSLByteCode.h` | `NkSLOp`, `NkSLValue/Instr`, `NkSLByteProgram`, `NkSLVMEnv`. | [La machine virtuelle](NKSL/VM.md) |
| `VM/NkSLByteCodeIO.h` | Sérialisation `.nkbc`. | [La machine virtuelle](NKSL/VM.md) |
| `VM/NkSLVM.h` | `NkSLVM::Execute` (interpréteur). | [La machine virtuelle](NKSL/VM.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
