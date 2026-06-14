#pragma once
// =============================================================================
// NKSL.h — En-tête parapluie (umbrella) du module NKSL (Nkentseu Shader Language)
// =============================================================================
// Inclure CE SEUL fichier donne accès à toute l'API publique de NKSL :
//   - Types & enums            (NkSLStage, NkSLTarget, NkSLCompileOptions, …)
//   - Features de langage       (extensions / capabilities NkSL)
//   - Compilateur & cache       (NkSLCompiler, NkSLShaderLibrary)
//   - glslang GLSL→SPIR-V       (NkGLSLToSPIRV — stub si glslang absent)
//   - ShaderConvert             (SPIR-V→GLSL/HLSL/MSL via SPIRV-Cross + annotations)
//
// Usage :   #include "NKSL/NKSL.h"
//
// Les en-têtes INTERNES (Frontend : Lexer/Parser/Semantic/SymbolTable ; Core/AST ;
// CodeGen/*) restent inclus-ables individuellement pour qui veut étendre le
// compilateur, mais ne sont pas nécessaires aux simples consommateurs.
//
// Convention d'architecture : NKSL ne dépend QUE de Foundation/System
// (NKCore, NKContainers, NKMemory, NKMath, NKLogger, NKThreading, NKFileSystem)
// + glslang/SPIRV-Cross optionnels. Il ne connaît PAS NKRHI : c'est NKRHI
// (NkSLIntegration) qui convertit une sortie NkSL en NkShaderDesc.
// =============================================================================

// ── Core : types, enums, features ────────────────────────────────────────────
#include "NKSL/Core/NkSLTypes.h"
#include "NKSL/Core/NkSLFeatures.h"

// ── Compilateur + bibliothèque + cache ───────────────────────────────────────
#include "NKSL/Compiler/NkSLCompiler.h"
#include "NKSL/Compiler/NkGLSLCompiler.h"

// ── ShaderConvert (glslang + SPIRV-Cross) ────────────────────────────────────
#include "NKSL/ShaderConvert/NkShaderConvert.h"
#include "NKSL/ShaderConvert/NkShaderAnnotations.h"
