#pragma once
// =============================================================================
// NkSLCodeGen.h  — v4.0
// NOUVEAUTÉS :
//   - NkSLCodeGenGLSLVulkan  : backend GLSL 4.50 pour Vulkan
//     (layout(set=N,binding=M), #extension GL_KHR_vulkan_glsl, subpassInput…)
//   - NkSLCodeGenHLSL_DX12   : backend HLSL SM6+ pour DirectX 12
//     (register spaceN, inline RootSignature, wave ops, bindless heap SM6.6)
//   - NkSLCodeGenHLSL_DX11   : renommé depuis NkSLCodeGenHLSL (alias maintenu)
// =============================================================================
#include "NkSLAST.h"

namespace nkentseu {

// Utilitaires libres partagés par tous les backends
const char* NkSLBaseTypeName(NkSLBaseType t);
NkString    NkSLTypeName    (NkSLBaseType t);

// =============================================================================
// Taille en bytes d'un type de base (std140 pour les UBO)
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
        case NkSLBaseType::NK_MAT3:    return 48; // std140 : mat3 = 3 × vec4
        case NkSLBaseType::NK_MAT4:    return 64;
        default:                       return 0;
    }
}

inline uint32 NkSLBaseTypeComponents(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_VEC2: case NkSLBaseType::NK_IVEC2:
        case NkSLBaseType::NK_UVEC2: case NkSLBaseType::NK_DVEC2: return 2;
        case NkSLBaseType::NK_VEC3: case NkSLBaseType::NK_IVEC3:
        case NkSLBaseType::NK_UVEC3: case NkSLBaseType::NK_DVEC3: return 3;
        case NkSLBaseType::NK_VEC4: case NkSLBaseType::NK_IVEC4:
        case NkSLBaseType::NK_UVEC4: case NkSLBaseType::NK_DVEC4: return 4;
        case NkSLBaseType::NK_MAT2: return 4;
        case NkSLBaseType::NK_MAT3: return 9;
        case NkSLBaseType::NK_MAT4: return 16;
        default: return 1;
    }
}

// =============================================================================
// Classe de base
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
    void     IndentPop()            { if (mIndent > 0) mIndent--; }
    NkString Indent() const;
    void     Emit    (const NkString& s) { mOutput += s; }
    void     EmitLine(const NkString& s) { mOutput += Indent() + s + "\n"; }
    void     EmitNewLine()               { mOutput += "\n"; }
    NkString TypeToString    (NkSLTypeNode* t);
    NkString BaseTypeToString(NkSLBaseType t);
    NkString StorageToString (NkSLStorageQual s);
    void AddError  (uint32 l, const NkString& m) { mErrors.PushBack({l,0,"",m,true}); }
    void AddWarning(uint32 l, const NkString& m) { mWarnings.PushBack({l,0,"",m,false}); }

    NkSLTarget mTarget;
    NkString   mOutput;
    uint32     mIndent = 0;
    NkVector<NkSLCompileError> mErrors, mWarnings;

    // Compute : taille de workgroup, renseignée par chaque GenProgram() depuis
    // NkSLProgramNode::localSize*. Sert à émettre layout(local_size_*) (GLSL/VK)
    // ou [numthreads(...)] (HLSL). Défaut 1,1,1 (stages non-compute).
    uint32 mLocalSizeX = 1, mLocalSizeY = 1, mLocalSizeZ = 1;
};

// Mappe un format d'image NkSL ("r32f"/"rgba8"/"rgba16f"...) vers le type
// d'élément HLSL d'un RWTexture2D (cs_5_0/cs_6_0). Défaut float4.
inline const char* NkSLImageFormatToHLSLElem(const NkString& fmt) {
    if (fmt == "r32f"  || fmt == "r16f")                       return "float";
    if (fmt == "rg32f" || fmt == "rg16f")                      return "float2";
    if (fmt == "r32i"  || fmt == "r16i"  || fmt == "r8i")      return "int";
    if (fmt == "r32ui" || fmt == "r16ui" || fmt == "r8ui")     return "uint";
    if (fmt == "rgba32i" || fmt == "rgba16i" || fmt == "rgba8i")  return "int4";
    if (fmt == "rgba32ui"|| fmt == "rgba16ui"|| fmt == "rgba8ui") return "uint4";
    // rgba8 / rgba16f / rgba32f / rgb10_a2 / ...
    return "float4";
}

// Mappe un format d'image NkSL vers le type d'élément MSL (texture2d<T,...>).
inline const char* NkSLImageFormatToMSLElem(const NkString& fmt) {
    if (fmt == "r32i" || fmt == "r16i" || fmt == "r8i" ||
        fmt == "rgba32i" || fmt == "rgba16i" || fmt == "rgba8i")  return "int";
    if (fmt == "r32ui" || fmt == "r16ui" || fmt == "r8ui" ||
        fmt == "rgba32ui" || fmt == "rgba16ui" || fmt == "rgba8ui") return "uint";
    return "float"; // formats float et normalisés (r32f, rgba8, rgba16f...)
}

// =============================================================================
// GLSL — OpenGL 4.30+ (bindings aplatis, pas de set=)
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

    const NkSLCompileOptions* mOpts  = nullptr;
    NkSLStage                 mStage = NkSLStage::NK_VERTEX;
    int                       mAutoBinding = 0;
    // Auto-assignation des locations d'interface (in/out) et bindings UBO quand
    // la source NkSL ne les precise pas. Indispensable en OpenGL : sans
    // layout(location=) explicite, le linker GL assigne les locations d'attributs
    // dans un ordre NON garanti -> aPos peut ne pas etre a la location 0 -> les
    // positions sont melangees -> ecran noir. Compteurs separes in/out, dans
    // l'ordre de declaration (le vertex out et le fragment in matchent alors).
    int                       mAutoInLoc   = 0;
    int                       mAutoOutLoc  = 0;
};

// =============================================================================
// GLSL — Vulkan 4.50+ (layout(set=N, binding=M), extensions VK)
//
// Différences majeures par rapport à NK_GLSL :
//   - #version 450 + #extension GL_KHR_vulkan_glsl : require
//   - layout(set=N, binding=M) sur tous les uniforms/samplers (obligatoire)
//   - push_constant : layout(push_constant) uniform Block { ... }
//   - subpassInput / subpassLoad() pour les input attachments
//   - gl_BaseVertex, gl_BaseInstance (si opts.vkDrawParams)
//   - Pas de layout(binding=) aplatit — on conserve les set/binding d'origine
//   - gl_VertexIndex, gl_InstanceIndex au lieu de gl_VertexID, gl_InstanceID
// =============================================================================
class NkSLCodeGenGLSLVulkan : public NkSLCodeGenBase {
public:
    NkSLCodeGenGLSLVulkan() : NkSLCodeGenBase(NkSLTarget::NK_GLSL_VULKAN) {}
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
    NkString BuiltinToVkGLSL(const NkString& name, NkSLStage stage);
    NkString LiteralToStr   (NkSLLiteralNode* lit);

    // Génère layout(set=N, binding=M) pour un uniform/sampler Vulkan
    NkString LayoutQualifier(const NkSLBinding& b, bool isBlock, bool isStorage,
                              bool isPushConstant, const char* extra = nullptr);

    const NkSLCompileOptions* mOpts  = nullptr;
    NkSLStage                 mStage = NkSLStage::NK_VERTEX;
    int                       mAutoSet     = 0;
    int                       mAutoBinding = 0;
    // Vulkan GLSL EXIGE des layout(location=) explicites sur toute l'interface
    // in/out (sinon glslang refuse -> SPIR-V echoue). Auto-assignation dans
    // l'ordre de declaration, compteurs separes in/out.
    int                       mAutoInLoc   = 0;
    int                       mAutoOutLoc  = 0;
};

// =============================================================================
// HLSL — DirectX 11 (SM5, fxc, register classiques)
//
// register(bN)  cbuffer
// register(tN)  Texture2D / StructuredBuffer
// register(sN)  SamplerState
// register(uN)  RWTexture2D / RWStructuredBuffer
// =============================================================================
class NkSLCodeGenHLSL_DX11 : public NkSLCodeGenBase {
public:
    NkSLCodeGenHLSL_DX11() : NkSLCodeGenBase(NkSLTarget::NK_HLSL_DX11) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
    static NkString IntrinsicToHLSL_Static(const NkString& name);
private:
    void     CollectDecls          (NkSLProgramNode* prog);
    void     GenProgram            (NkSLProgramNode* prog);
    void     GenDecl               (NkSLNode* node);
    void     GenVarDecl            (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenCBuffer            (NkSLBlockDeclNode* b);
    void     GenSBuffer            (NkSLBlockDeclNode* b);
    void     GenStruct             (NkSLStructDeclNode* s);
    void     GenFunction           (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs ();
    void     GenGeometryShaderPrefix(NkSLStage stage);
    void     GenTessellationShaders (NkSLProgramNode* prog);
    void     GenStructuredBuffer    (NkSLBlockDeclNode* b);
    void     GenBindlessHeader      ();
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToHLSL    (NkSLTypeNode* t);
    NkString BaseTypeToHLSL(NkSLBaseType t);
    NkString BuiltinToHLSL (const NkString& name, NkSLStage stage);
    NkString SemanticFor   (NkSLVarDeclNode* v, NkSLStage stage,
                             bool isInput, bool isFragOut, int autoIndex);
    NkString IntrinsicToHLSL(const NkString& name);
    NkString LiteralToStr   (NkSLLiteralNode* lit);

    const NkSLCompileOptions*    mOpts = nullptr;
    NkSLStage                    mStage;
    NkString                     mInputStructName, mOutputStructName;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniforms;
    NkVector<NkSLBlockDeclNode*> mCBuffers, mSBuffers;
    NkVector<NkString>           mMatrixNames;
    bool                         mWritesDepth = false;
    // Compteur de binding PARTAGE (b/t/s/u) : le device DX mappe binding->slot.
    // Doit matcher le compteur partage du cote demo (ubo=0, shadow=1, albedo=2).
    int                          mReg = 0;
};

// Alias de rétrocompatibilité — l'ancien NkSLCodeGenHLSL pointe sur DX11
using NkSLCodeGenHLSL = NkSLCodeGenHLSL_DX11;

// =============================================================================
// HLSL — DirectX 12 (SM6+, dxc, space N, wave ops)
//
// Différences majeures par rapport à NK_HLSL_DX11 :
//   - #pragma pack_matrix(column_major)  (identique DX11)
//   - register(bN, spaceM) / register(tN, spaceM) / etc.
//   - RootSignature inline si opts.dx12InlineRootSignature
//   - Wave Intrinsics SM6.0 : WaveGetLaneCount, WaveActiveSum, etc.
//   - Mesh/Task shaders SM6.5 (via NkSLCodeGenAdvanced)
//   - Bindless SM6.6 : ResourceDescriptorHeap[idx] / SamplerDescriptorHeap[idx]
//     si opts.dx12BindlessHeap
//   - SM6 → #define NKSL_SM  60  (SM6.0), 65 (SM6.5), 66 (SM6.6) etc.
// =============================================================================
class NkSLCodeGenHLSL_DX12 : public NkSLCodeGenBase {
public:
    NkSLCodeGenHLSL_DX12() : NkSLCodeGenBase(NkSLTarget::NK_HLSL_DX12) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
    static NkString IntrinsicToHLSL_Static(const NkString& name);
private:
    void     CollectDecls          (NkSLProgramNode* prog);
    void     GenProgram            (NkSLProgramNode* prog);
    void     GenDecl               (NkSLNode* node);
    void     GenVarDecl            (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenCBuffer            (NkSLBlockDeclNode* b);
    void     GenSBuffer            (NkSLBlockDeclNode* b);
    void     GenStruct             (NkSLStructDeclNode* s);
    void     GenFunction           (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs ();
    // DX12-specific
    void     GenRootSignature      ();
    void     GenBindlessHeader     ();
    NkString RegisterDecl          (const char* regType, int slot, int space) const;
    NkString WaveIntrinsicToDX12   (const NkString& name);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToHLSL    (NkSLTypeNode* t);
    NkString BaseTypeToHLSL(NkSLBaseType t);
    NkString BuiltinToHLSL (const NkString& name, NkSLStage stage);
    NkString SemanticFor   (NkSLVarDeclNode* v, NkSLStage stage,
                             bool isInput, bool isFragOut, int autoIndex);
    NkString IntrinsicToHLSL(const NkString& name);
    NkString LiteralToStr   (NkSLLiteralNode* lit);

    const NkSLCompileOptions*    mOpts = nullptr;
    NkSLStage                    mStage;
    NkString                     mInputStructName, mOutputStructName;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniforms;
    NkVector<NkSLBlockDeclNode*> mCBuffers, mSBuffers;
    NkVector<NkString>           mMatrixNames;
    bool                         mWritesDepth = false;
    // Register counters par slot type (DX12 utilise space séparé)
    // Compteur de binding PARTAGE (b/t/s/u). Le device DX mappe le numero de
    // binding du descripteur directement sur le slot du registre, et le demo binde
    // avec un compteur partage (ubo=0, shadow=1, albedo=2) comme GLSL. Donc le
    // registre HLSL doit = ce numero partage (ubo->b0, shadow->t1/s1, albedo->t2/s2)
    // et NON des compteurs separes par namespace (qui donnaient shadow->t0, albedo->t1).
    int  mReg = 0;
};

// =============================================================================
// MSL (générateur natif depuis AST)
// =============================================================================
class NkSLCodeGen_MSL : public NkSLCodeGenBase {
public:
    NkSLCodeGen_MSL() : NkSLCodeGenBase(NkSLTarget::NK_MSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     CollectDecls          (NkSLProgramNode* prog);
    void     GenProgram            (NkSLProgramNode* prog);
    void     GenDecl               (NkSLNode* node);
    void     GenVarDecl            (NkSLVarDeclNode* v, bool inStruct=false);
    void     GenStruct             (NkSLStructDeclNode* s);
    void     GenFunction           (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs ();
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

    const NkSLCompileOptions*    mOpts = nullptr;
    NkSLStage                    mStage;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniformVars;
    NkVector<NkSLBlockDeclNode*> mBufferDecls;
    bool                         mWritesDepth = false;
};

// =============================================================================
// MSL via SPIRV-Cross (chemin alternatif plus robuste pour les cas edge)
// =============================================================================
class NkSLCodeGenMSLSpirvCross : public NkSLCodeGenBase {
public:
    NkSLCodeGenMSLSpirvCross() : NkSLCodeGenBase(NkSLTarget::NK_MSL_SPIRV_CROSS) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
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
class NkSLCodeGenCPP : public NkSLCodeGenBase {
public:
    NkSLCodeGenCPP() : NkSLCodeGenBase(NkSLTarget::NK_CPLUSPLUS) {}
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

    const NkSLCompileOptions* mOpts  = nullptr;
    NkSLStage                 mStage = NkSLStage::NK_VERTEX;
};

// =============================================================================
// NkSLReflector — extraction automatique des bindings depuis l'AST
// =============================================================================
class NkSLReflector {
public:
    NkSLReflection Reflect(NkSLProgramNode* ast, NkSLStage stage);

    NkString GenerateLayoutJSON(const NkSLReflection& reflection);
    NkString GenerateLayoutCPP (const NkSLReflection& reflection,
                                 const NkString& varName = "layout");

private:
    void ReflectDecl      (NkSLNode* node, NkSLStage stage, NkSLReflection& out);
    void ReflectVarDecl   (NkSLVarDeclNode* v, NkSLStage stage, NkSLReflection& out);
    void ReflectBlockDecl (NkSLBlockDeclNode* b, NkSLStage stage, NkSLReflection& out);
    void ReflectFunction  (NkSLFunctionDeclNode* fn, NkSLStage stage, NkSLReflection& out);

    uint32 ComputeBlockSize (NkSLBlockDeclNode* b);
    uint32 ComputeMemberSize(NkSLVarDeclNode* m);

    int mAutoBinding = 0;
    int mAutoLocation = 0;
};

} // namespace nkentseu
