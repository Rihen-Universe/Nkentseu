# NKSL — documentation détaillée

Le module **NKSL**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKSL.md](../NKSL.md).

Chaque page liste l'**API publique réelle** (enums, structs, classes, free functions) du
module, avec ses pièges d'ownership (l'AST en `new`/`delete` bruts, les pointeurs internes
invalidables) et ses idiomes d'utilisation (pipeline frontend, caches, capacités
conditionnelles glslang/SPIRV-Cross).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Types-AST.md](Types-AST.md) | L'arbre syntaxique polymorphe, les types/enums du langage (`NkSLTarget`/`NkSLStage`/`NkSLBaseType`), bindings, réflexion, options de compilation et capabilities par cible. | `Core/NkSLTypes.h`, `Core/NkSLAST.h`, `Core/NkSLFeatures.h` |
| [Frontend.md](Frontend.md) | Le frontend de compilation : lexer (tokens GLSL + annotations `@`), parser récursif vers l'AST, analyse sémantique (typage/swizzle/surcharges/inter-stages), table des symboles à scopes imbriqués. | `Frontend/NkSLLexer.h`, `Frontend/NkSLParser.h`, `Frontend/NkSLSemantic.h`, `Frontend/NkSLSymbolTable.h` |
| [Compiler-CodeGen.md](Compiler-CodeGen.md) | La façade `NkSLCompiler` (+ caches), les 8 backends de génération (GLSL/HLSL/MSL/SPIR-V/C++), la conversion de fichiers shaders et les annotations de matériaux (`@binding`/`@stage`/`@entry`/`@param`…). | `Compiler/NkSLCompiler.h`, `Compiler/NkGLSLCompiler.h`, `CodeGen/NkSLCodeGen.h`, `ShaderConvert/NkShaderConvert.h`, `ShaderConvert/NkShaderAnnotations.h` |
| [VM.md](VM.md) | La machine virtuelle bytecode : ISA à registres (`NkSLOp`), programme/valeurs runtime, sérialisation `.nkbc`, interpréteur `NkSLVM` pour le rasterizer Software. | `VM/NkSLByteCode.h`, `VM/NkSLByteCodeIO.h`, `VM/NkSLVM.h` |

[← Récap NKSL](../NKSL.md) · [← Couche Runtime](../README.md)
