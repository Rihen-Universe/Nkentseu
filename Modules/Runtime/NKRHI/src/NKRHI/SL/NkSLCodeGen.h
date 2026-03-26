#pragma once
// =============================================================================
// NkSLCodeGen.h  — v3.0
// CORRECTIONS:
//   - sAutoIdx/sSemanticIdx static éliminés → membres de classe (thread-safe)
//   - Ajout NkSLCodeGenMSLSpirvCross (chemin SPIRV-Cross)
//   - Ajout NkSLReflector pour la reflection automatique
// =============================================================================
#include "NkSLAST.h"

namespace nkentseu {

// Utilitaires libres partagés par tous les backends
const char* NkSLBaseTypeName(NkSLBaseType t);
NkString    NkSLTypeName    (NkSLBaseType t);

// =============================================================================
// Taille en bytes d'un type de base (pour la reflection UBO)
// =============================================================================
inline uint32 NkSLBaseTypeSize(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_BOOL:
        case NkSLBaseType::NK_INT:
        case NkSLBaseType::NK_UINT:
        case NkSLBaseType::NK_FLOAT:   return 4;
        case NkSLBaseType::NK_DOUBLE:  return 8;
        case NkSLBaseType::NK_IVEC2:
        case NkSLBaseType::NK_UVEC2:
        case NkSLBaseType::NK_VEC2:    return 8;
        case NkSLBaseType::NK_DVEC2:   return 16;
        case NkSLBaseType::NK_IVEC3:
        case NkSLBaseType::NK_UVEC3:
        case NkSLBaseType::NK_VEC3:    return 12;
        case NkSLBaseType::NK_DVEC3:   return 24;
        case NkSLBaseType::NK_IVEC4:
        case NkSLBaseType::NK_UVEC4:
        case NkSLBaseType::NK_VEC4:    return 16;
        case NkSLBaseType::NK_DVEC4:   return 32;
        case NkSLBaseType::NK_MAT2:    return 16;
        case NkSLBaseType::NK_MAT3:    return 48; // std140: mat3 = 3 * vec4
        case NkSLBaseType::NK_MAT4:    return 64;
        default:                    return 0;
    }
}

// Nombre de composantes vectorielles
inline uint32 NkSLBaseTypeComponents(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_VEC2: case NkSLBaseType::NK_IVEC2: case NkSLBaseType::NK_UVEC2: case NkSLBaseType::NK_DVEC2: return 2;
        case NkSLBaseType::NK_VEC3: case NkSLBaseType::NK_IVEC3: case NkSLBaseType::NK_UVEC3: case NkSLBaseType::NK_DVEC3: return 3;
        case NkSLBaseType::NK_VEC4: case NkSLBaseType::NK_IVEC4: case NkSLBaseType::NK_UVEC4: case NkSLBaseType::NK_DVEC4: return 4;
        case NkSLBaseType::NK_MAT2: return 4;
        case NkSLBaseType::NK_MAT3: return 9;
        case NkSLBaseType::NK_MAT4: return 16;
        default: return 1;
    }
}

// =============================================================================
class NkSLCodeGenBase {
public:
    explicit NkSLCodeGenBase(NkSLTarget target) : mTarget(target) {}
    virtual ~NkSLCodeGenBase() = default;
    virtual NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                        const NkSLCompileOptions& opts) = 0;
    NkSLTarget GetTarget() const { return mTarget; }
protected:
    void     IndentPush()           { mIndent++; }
    void     IndentPop()            { if (mIndent>0) mIndent--; }
    NkString Indent() const;
    void     Emit    (const NkString& s) { mOutput += s; }
    void     EmitLine(const NkString& s) { mOutput += Indent() + s + "\n"; }
    void     EmitNewLine()               { mOutput += "\n"; }
    NkString TypeToString    (NkSLTypeNode* t);
    NkString BaseTypeToString(NkSLBaseType t);
    NkString StorageToString (NkSLStorageQual s);
    void AddError  (uint32 l, const NkString& m) { mErrors.PushBack({l,0,"",m,true}); }
    void AddWarning(uint32 l, const NkString& m) { mWarnings.PushBack({l,0,"",m,false}); }
    NkSLTarget mTarget; NkString mOutput; uint32 mIndent = 0;
    NkVector<NkSLCompileError> mErrors, mWarnings;
};

// =============================================================================
// GLSL
// =============================================================================
class NkSLCodeGenGLSL : public NkSLCodeGenBase {
public:
    NkSLCodeGenGLSL() : NkSLCodeGenBase(NkSLTarget::NK_GLSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     GenProgram (NkSLProgramNode* prog);
    void     GenDecl    (NkSLNode* node);
    void     GenVarDecl (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenBlock   (NkSLBlockDeclNode* b);
    void     GenStruct  (NkSLStructDeclNode* s);
    void     GenFunction(NkSLFunctionDeclNode* fn);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString BuiltinToGLSL (const NkString& name, NkSLStage stage);
    NkString TypeQualifier (NkSLVarDeclNode* v, int bindingBase);
    NkString LiteralToStr  (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage; int mAutoBinding=0;
};

// =============================================================================
// HLSL
// =============================================================================
class NkSLCodeGenHLSL : public NkSLCodeGenBase {
public:
    NkSLCodeGenHLSL() : NkSLCodeGenBase(NkSLTarget::NK_HLSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
    static NkString IntrinsicToHLSL_Static(const NkString& name);
private:
    void     CollectDecls         (NkSLProgramNode* prog);
    void     GenProgram           (NkSLProgramNode* prog);
    void     GenDecl              (NkSLNode* node);
    void     GenVarDecl           (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenCBuffer           (NkSLBlockDeclNode* b);
    void     GenSBuffer           (NkSLBlockDeclNode* b);
    void     GenStructuredBuffer  (NkSLBlockDeclNode* b);
    void     GenBindlessHeader    ();
    void     GenStruct            (NkSLStructDeclNode* s);
    void     GenFunction          (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs();
    void     GenGeometryShaderPrefix(NkSLStage stage);
    void     GenTessellationShaders (NkSLProgramNode* prog);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToHLSL    (NkSLTypeNode* t);
    NkString BaseTypeToHLSL(NkSLBaseType t);
    NkString BuiltinToHLSL (const NkString& name, NkSLStage stage);

    // CORRECTION BUG: SemanticFor prend maintenant un autoIndex en paramètre
    // au lieu d'utiliser un static local — thread-safe et déterministe
    NkString SemanticFor   (NkSLVarDeclNode* v, NkSLStage stage, bool isInput, int autoIndex);
    NkString IntrinsicToHLSL(const NkString& name);
    NkString LiteralToStr  (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage;
    NkString mInputStructName, mOutputStructName;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniforms;
    NkVector<NkSLBlockDeclNode*> mCBuffers, mSBuffers;
    NkVector<NkString>           mMatrixNames; // noms des membres de cbuffer de type matrice
    bool                         mWritesDepth = false; // true si gl_FragDepth est assigné dans le shader
};

// =============================================================================
// MSL (générateur natif)
// =============================================================================
class NkSLCodeGen_MSL : public NkSLCodeGenBase {
public:
    NkSLCodeGen_MSL() : NkSLCodeGenBase(NkSLTarget::NK_MSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     CollectDecls         (NkSLProgramNode* prog);
    void     GenProgram           (NkSLProgramNode* prog);
    void     GenDecl              (NkSLNode* node);
    void     GenVarDecl           (NkSLVarDeclNode* v, bool inStruct=false);
    void     GenStruct            (NkSLStructDeclNode* s);
    void     GenFunction          (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs();
    NkString BuildEntryPointSignature(NkSLFunctionDeclNode* fn);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToMSL      (NkSLTypeNode* t);
    NkString BaseTypeToMSL  (NkSLBaseType t);
    NkString BuiltinToMSL   (const NkString& name, NkSLStage stage);
    NkString IntrinsicToMSL (const NkString& name);
    NkString AddressSpaceFor(NkSLStorageQual s);
    NkString LiteralToStr   (NkSLLiteralNode* lit);
    const NkSLCompileOptions*   mOpts=nullptr; NkSLStage mStage;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniformVars;
    NkVector<NkSLBlockDeclNode*> mBufferDecls;
    bool                         mWritesDepth = false;
};

// =============================================================================
// MSL via SPIRV-Cross (chemin alternatif — plus robuste pour les cas edge)
// Nécessite NKSL_HAS_SPIRV_CROSS défini et spirv-cross embarqué
// =============================================================================
class NkSLCodeGenMSLSpirvCross : public NkSLCodeGenBase {
public:
    NkSLCodeGenMSLSpirvCross() : NkSLCodeGenBase(NkSLTarget::NK_MSL_SPIRV_CROSS) {}
    // Prend le SPIR-V binaire produit par NkSLCodeGenGLSL + glslang
    // et le convertit en MSL via SPIRV-Cross
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;

    // Surcharge directe depuis le SPIR-V binaire (sans passer par l'AST)
    NkSLCompileResult GenerateFromSPIRV(const NkVector<uint32>& spirvWords,
                                         NkSLStage stage,
                                         const NkSLCompileOptions& opts);
private:
    NkSLCompileResult SPIRVToMSL(const NkVector<uint32>& spirvWords,
                                  NkSLStage stage,
                                  const NkSLCompileOptions& opts);
};

// =============================================================================
// C++ Software Rasterizer
// =============================================================================
class NkSLCodeGen_CPP : public NkSLCodeGenBase {
public:
    NkSLCodeGen_CPP() : NkSLCodeGenBase(NkSLTarget::NK_CPLUSPLUS) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     GenProgram (NkSLProgramNode* prog);
    void     GenDecl    (NkSLNode* node);
    void     GenFunction(NkSLFunctionDeclNode* fn, bool asEntry=false);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToCPP      (NkSLTypeNode* t);
    NkString BaseTypeToCPP  (NkSLBaseType t);
    NkString IntrinsicToCPP (const NkString& name);
    NkString LiteralToStr   (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage;
};

// =============================================================================
// NkSLReflector — extraction automatique des bindings depuis l'AST
//
// Extrait depuis l'AST NkSL :
//   - Tous les uniform buffers (UBO) avec leur set/binding et taille
//   - Tous les storage buffers (SSBO)
//   - Tous les samplers/textures avec leur set/binding
//   - Toutes les storage images
//   - Tous les vertex inputs avec leur location
//   - Tous les fragment outputs
//   - Les push constants
//   - Les local_size pour compute shaders
//
// Usage :
//   NkSLReflector reflector;
//   NkSLReflection reflection = reflector.Reflect(ast, stage);
//   for (auto& r : reflection.resources) { ... }
// =============================================================================
class NkSLReflector {
public:
    NkSLReflection Reflect(NkSLProgramNode* ast, NkSLStage stage);

    // Génère un NkDescriptorSetLayoutDesc depuis la reflection
    // (helper pour la création automatique du pipeline layout)
    // Retourne une string JSON décrivant le layout (portable, sans dépendance RHI)
    NkString GenerateLayoutJSON(const NkSLReflection& reflection);

    // Génère le code C++ de création du DescriptorSetLayout pour NkIDevice
    NkString GenerateLayoutCPP(const NkSLReflection& reflection,
                                 const NkString& varName = "layout");

private:
    void ReflectDecl       (NkSLNode* node, NkSLStage stage, NkSLReflection& out);
    void ReflectVarDecl    (NkSLVarDeclNode* v, NkSLStage stage, NkSLReflection& out);
    void ReflectBlockDecl  (NkSLBlockDeclNode* b, NkSLStage stage, NkSLReflection& out);
    void ReflectFunction   (NkSLFunctionDeclNode* fn, NkSLStage stage, NkSLReflection& out);

    uint32 ComputeBlockSize(NkSLBlockDeclNode* b);
    uint32 ComputeMemberSize(NkSLVarDeclNode* m);

    int mAutoBinding = 0;
    int mAutoLocation = 0;
};

} // namespace nkentseu
