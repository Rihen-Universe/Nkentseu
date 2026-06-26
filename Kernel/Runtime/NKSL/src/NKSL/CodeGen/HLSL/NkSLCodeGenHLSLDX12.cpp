// =============================================================================
// NkSLCodeGenHLSL_DX12.cpp  — v4.0
//
// Backend HLSL SM6+ pour DirectX 12.
//
// Différences fondamentales avec NK_HLSL_DX11 (SM5) :
//
//   1. SHADER MODEL & PRAGMA
//      Tous les shaders émettent :
//        #pragma pack_matrix(column_major)
//        #define NKSL_SM  <60|65|66>
//      Le SM6 est sélectionné par opts.hlslShaderModelDX12 (60, 65, 66…).
//
//   2. REGISTER SPACES (space N)
//      DX11 : register(b0), register(t0)
//      DX12 : register(b0, space0), register(t0, space1)…
//      L'espace par défaut est opts.dx12DefaultSpace (généralement 0).
//
//   3. ROOT SIGNATURE INLINE (optionnel)
//      Si opts.dx12InlineRootSignature, émet :
//        #define RS_CBV  "CBV(b0, space0)"
//        [RootSignature(RS_CBV)]
//      avant chaque entry point. Compatible dxc -rootsig-define.
//
//   4. WAVE INTRINSICS (SM6.0+)
//      WaveGetLaneCount(), WaveGetLaneIndex(), WaveActiveSum(),
//      WaveActiveBitOr(), WaveAllTrue(), WaveActiveMax(), etc.
//      Traduit les équivalents GLSL/NkSL :
//        subgroupSize()       → WaveGetLaneCount()
//        subgroupElect()      → WaveIsFirstLane()
//        subgroupAdd(x)       → WaveActiveSum(x)
//        subgroupOr(x)        → WaveActiveBitOr(x)
//        subgroupAll(x)       → WaveActiveAllTrue(x)
//        subgroupAny(x)       → WaveActiveAnyTrue(x)
//        subgroupBroadcast(x,i)→ WaveReadLaneAt(x, i)
//
//   5. BINDLESS HEAP (SM6.6, optionnel)
//      Si opts.dx12BindlessHeap, émet l'en-tête :
//        // Bindless resources via SM6.6 descriptor heaps
//        #define NKSL_TEX2D(idx)    ResourceDescriptorHeap[idx]
//        #define NKSL_SAMPLER(idx)  SamplerDescriptorHeap[idx]
//      Les samplers et textures déclarés avec @binding obtiennent un
//      index numérique utilisable avec NKSL_TEX2D/NKSL_SAMPLER.
//
//   6. MESH/TASK SHADERS (SM6.5) — délégués à NkSLCodeGenAdvanced.
//
//   7. RAY TRACING (SM6.3) — délégués à NkSLCodeGenAdvanced.
// =============================================================================
#include "NKSL/CodeGen/NkSLCodeGen.h"
#include "NKSL/Frontend/NkSLSemantic.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {

// =============================================================================
// Table sémantique HLSL (partagée DX11/DX12)
// =============================================================================
struct HLSLSemanticRuleDX12 {
    const char* nameLower;
    bool        isPrefix;
    const char* inputSem;
    const char* outputSem;
    const char* fragOutSem;
};

static const HLSLSemanticRuleDX12 kSemRulesDX12[] = {
    {"position",   false, "POSITION",      "SV_Position",   "SV_Position"},
    {"pos",        false, "POSITION",      "SV_Position",   "SV_Position"},
    {"apos",       false, "POSITION",      "SV_Position",   "SV_Position"},
    {"vpos",       false, "SV_Position",   "SV_Position",   "SV_Position"},
    {"normal",     false, "NORMAL",        "NORMAL",        "NORMAL"},
    {"anormal",    false, "NORMAL",        "NORMAL",        "NORMAL"},
    {"vnormal",    false, "NORMAL",        "NORMAL",        "NORMAL"},
    {"tangent",    false, "TANGENT",       "TANGENT",       "TANGENT"},
    {"bitangent",  false, "BINORMAL",      "BINORMAL",      "BINORMAL"},
    {"color",      false, "COLOR",         "COLOR",         "SV_Target0"},
    {"colour",     false, "COLOR",         "COLOR",         "SV_Target0"},
    {"fragcolor",  true,  "COLOR",         "COLOR",         "SV_Target0"},
    {"outcolor",   true,  "COLOR",         "COLOR",         "SV_Target0"},
    {"uv",         true,  "TEXCOORD0",     "TEXCOORD0",     "TEXCOORD0"},
    {"texcoord",   true,  "TEXCOORD0",     "TEXCOORD0",     "TEXCOORD0"},
    {"fragdepth",  false, "DEPTH",         "DEPTH",         "SV_Depth"},
    {"instanceid", false, "SV_InstanceID", "SV_InstanceID", ""},
    {"vertexid",   false, "SV_VertexID",   "SV_VertexID",   ""},
    {nullptr, false, nullptr, nullptr, nullptr}
};

// =============================================================================
// Helpers internes
// =============================================================================
static bool ScanWritesDepthDX12(NkSLNode* node) {
    if (!node) return false;
    if (node->kind == NkSLNodeKind::NK_EXPR_ASSIGN) {
        auto* a = static_cast<NkSLAssignNode*>(node);
        if (a->lhs && a->lhs->kind == NkSLNodeKind::NK_EXPR_IDENT)
            if (static_cast<NkSLIdentNode*>(a->lhs)->name == "gl_FragDepth") return true;
    }
    for (auto* c : node->children) if (ScanWritesDepthDX12(c)) return true;
    return false;
}

// =============================================================================
// BaseTypeToHLSL (identique à DX11)
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::BaseTypeToHLSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_VOID:   return "void";
        case NkSLBaseType::NK_BOOL:   return "bool";
        case NkSLBaseType::NK_INT:    return "int";    case NkSLBaseType::NK_IVEC2: return "int2";
        case NkSLBaseType::NK_IVEC3:  return "int3";   case NkSLBaseType::NK_IVEC4: return "int4";
        case NkSLBaseType::NK_UINT:   return "uint";   case NkSLBaseType::NK_UVEC2: return "uint2";
        case NkSLBaseType::NK_UVEC3:  return "uint3";  case NkSLBaseType::NK_UVEC4: return "uint4";
        case NkSLBaseType::NK_FLOAT:  return "float";  case NkSLBaseType::NK_VEC2:  return "float2";
        case NkSLBaseType::NK_VEC3:   return "float3"; case NkSLBaseType::NK_VEC4:  return "float4";
        case NkSLBaseType::NK_DOUBLE: return "double"; case NkSLBaseType::NK_DVEC2: return "double2";
        case NkSLBaseType::NK_DVEC3:  return "double3";case NkSLBaseType::NK_DVEC4: return "double4";
        case NkSLBaseType::NK_MAT2:   return "column_major float2x2";
        case NkSLBaseType::NK_MAT3:   return "column_major float3x3";
        case NkSLBaseType::NK_MAT4:   return "column_major float4x4";
        case NkSLBaseType::NK_MAT2X3: return "column_major float2x3";
        case NkSLBaseType::NK_MAT2X4: return "column_major float2x4";
        case NkSLBaseType::NK_MAT3X2: return "column_major float3x2";
        case NkSLBaseType::NK_MAT3X4: return "column_major float3x4";
        case NkSLBaseType::NK_MAT4X2: return "column_major float4x2";
        case NkSLBaseType::NK_MAT4X3: return "column_major float4x3";
        case NkSLBaseType::NK_SAMPLER2D:             return "Texture2D";
        case NkSLBaseType::NK_SAMPLER2D_SHADOW:      return "Texture2D";
        case NkSLBaseType::NK_SAMPLER2D_ARRAY:       return "Texture2DArray";
        case NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW:return "Texture2DArray";
        case NkSLBaseType::NK_SAMPLER_CUBE:          return "TextureCube";
        case NkSLBaseType::NK_SAMPLER_CUBE_SHADOW:   return "TextureCube";
        case NkSLBaseType::NK_SAMPLER3D:             return "Texture3D";
        case NkSLBaseType::NK_ISAMPLER2D:            return "Texture2D<int4>";
        case NkSLBaseType::NK_USAMPLER2D:            return "Texture2D<uint4>";
        case NkSLBaseType::NK_IMAGE2D:               return "RWTexture2D<float4>";
        case NkSLBaseType::NK_IIMAGE2D:              return "RWTexture2D<int4>";
        case NkSLBaseType::NK_UIMAGE2D:              return "RWTexture2D<uint4>";
        // --- Samplers/images additionnels (HLSL SM6). Cube en UAV -> RWTexture2DArray.
        case NkSLBaseType::NK_SAMPLER1D:               return "Texture1D";
        case NkSLBaseType::NK_SAMPLER1D_ARRAY:         return "Texture1DArray";
        case NkSLBaseType::NK_SAMPLER_CUBE_ARRAY:      return "TextureCubeArray";
        case NkSLBaseType::NK_SAMPLER_CUBE_ARRAY_SHADOW:return "TextureCubeArray";
        case NkSLBaseType::NK_SAMPLER2DMS:             return "Texture2DMS<float4>";
        case NkSLBaseType::NK_ISAMPLER1D:              return "Texture1D<int4>";
        case NkSLBaseType::NK_USAMPLER1D:              return "Texture1D<uint4>";
        case NkSLBaseType::NK_ISAMPLER3D:              return "Texture3D<int4>";
        case NkSLBaseType::NK_USAMPLER3D:              return "Texture3D<uint4>";
        case NkSLBaseType::NK_ISAMPLER_CUBE:           return "TextureCube<int4>";
        case NkSLBaseType::NK_USAMPLER_CUBE:           return "TextureCube<uint4>";
        case NkSLBaseType::NK_ISAMPLER2D_ARRAY:        return "Texture2DArray<int4>";
        case NkSLBaseType::NK_USAMPLER2D_ARRAY:        return "Texture2DArray<uint4>";
        case NkSLBaseType::NK_ISAMPLER_CUBE_ARRAY:     return "TextureCubeArray<int4>";
        case NkSLBaseType::NK_USAMPLER_CUBE_ARRAY:     return "TextureCubeArray<uint4>";
        case NkSLBaseType::NK_IMAGE1D:                 return "RWTexture1D<float4>";
        case NkSLBaseType::NK_IIMAGE1D:                return "RWTexture1D<int4>";
        case NkSLBaseType::NK_UIMAGE1D:                return "RWTexture1D<uint4>";
        case NkSLBaseType::NK_IMAGE3D:                 return "RWTexture3D<float4>";
        case NkSLBaseType::NK_IIMAGE3D:                return "RWTexture3D<int4>";
        case NkSLBaseType::NK_UIMAGE3D:                return "RWTexture3D<uint4>";
        case NkSLBaseType::NK_IMAGE_CUBE:              return "RWTexture2DArray<float4>";
        case NkSLBaseType::NK_IIMAGE_CUBE:             return "RWTexture2DArray<int4>";
        case NkSLBaseType::NK_UIMAGE_CUBE:             return "RWTexture2DArray<uint4>";
        case NkSLBaseType::NK_IMAGE2D_ARRAY:           return "RWTexture2DArray<float4>";
        case NkSLBaseType::NK_IIMAGE2D_ARRAY:          return "RWTexture2DArray<int4>";
        case NkSLBaseType::NK_UIMAGE2D_ARRAY:          return "RWTexture2DArray<uint4>";
        default: return "float";
    }
}

NkString NkSLCodeGenHLSL_DX12::TypeToHLSL(NkSLTypeNode* t) {
    if (!t) return "void";
    // PAS de dimension d'array ici : HLSL veut l'array POSTFIXÉ après le nom
    // (`float4 a[32]`, comme C++). Suffixe ajouté via DX12_ArrSuffix() aux sites.
    if (t->baseType == NkSLBaseType::NK_STRUCT) return t->typeName;
    return BaseTypeToHLSL(t->baseType);
}

// Suffixe d'array postfixé `[N]` (HLSL/C++), ou "" si scalaire.
static NkString DX12_ArrSuffix(NkSLTypeNode* t) {
    if (t && t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        return NkString(buf);
    }
    return t && t->isUnsized ? NkString("[]") : NkString("");
}

// =============================================================================
// register(xN, spaceM) — spécifique DX12
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::RegisterDecl(const char* regType,
                                              int slot, int space) const {
    char buf[64];
    snprintf(buf, sizeof(buf), " : register(%s%d, space%d)", regType, slot, space);
    return NkString(buf);
}

// =============================================================================
// Wave Intrinsics — SM6.0+
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::WaveIntrinsicToDX12(const NkString& name) {
    if (name == "subgroupSize"        || name == "gl_SubgroupSize")  return "WaveGetLaneCount";
    if (name == "subgroupInvocation"  || name == "gl_SubgroupInvocationID") return "WaveGetLaneIndex";
    if (name == "subgroupElect")                                     return "WaveIsFirstLane";
    if (name == "subgroupAll")                                       return "WaveActiveAllTrue";
    if (name == "subgroupAny")                                       return "WaveActiveAnyTrue";
    if (name == "subgroupAdd")                                       return "WaveActiveSum";
    if (name == "subgroupMul")                                       return "WaveActiveProduct";
    if (name == "subgroupMin")                                       return "WaveActiveMin";
    if (name == "subgroupMax")                                       return "WaveActiveMax";
    if (name == "subgroupAnd" || name == "subgroupBitwiseAnd")      return "WaveActiveBitAnd";
    if (name == "subgroupOr"  || name == "subgroupBitwiseOr")       return "WaveActiveBitOr";
    if (name == "subgroupXor" || name == "subgroupBitwiseXor")      return "WaveActiveBitXor";
    if (name == "subgroupBroadcast")                                 return "WaveReadLaneAt";
    if (name == "subgroupBroadcastFirst")                            return "WaveReadLaneFirst";
    if (name == "subgroupBallot")                                    return "WaveActiveBallot";
    if (name == "subgroupExclusiveAdd")                              return "WavePrefixSum";
    if (name == "subgroupExclusiveMul")                              return "WavePrefixProduct";
    if (name == "subgroupQuadBroadcast")                             return "QuadReadLaneAt";
    if (name == "subgroupQuadSwapHorizontal")                        return "QuadReadAcrossX";
    if (name == "subgroupQuadSwapVertical")                          return "QuadReadAcrossY";
    if (name == "subgroupQuadSwapDiagonal")                          return "QuadReadAcrossDiagonal";
    return "";
}

// =============================================================================
// IntrinsicToHLSL (partagé DX11/DX12 + wave SM6)
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::IntrinsicToHLSL_Static(const NkString& name) {
    // Constructeurs de types GLSL → HLSL
    if (name == "vec2")  return "float2";
    if (name == "vec3")  return "float3";
    if (name == "vec4")  return "float4";
    if (name == "ivec2") return "int2";
    if (name == "ivec3") return "int3";
    if (name == "ivec4") return "int4";
    if (name == "uvec2") return "uint2";
    if (name == "uvec3") return "uint3";
    if (name == "uvec4") return "uint4";
    if (name == "mat2")  return "float2x2";
    if (name == "mat3")  return "float3x3";
    if (name == "mat4")  return "float4x4";
    // HLSL n'a pas de inverse() natif → helper nksl_inverse émis dans le préambule
    if (name == "inverse")   return "nksl_inverse";
    // Math
    if (name == "mix")       return "lerp";
    if (name == "fract")     return "frac";
    if (name == "mod")       return "fmod";
    if (name == "inversesqrt") return "rsqrt";
    if (name == "dFdx")     return "ddx";
    if (name == "dFdy")     return "ddy";
    if (name == "dFdxFine") return "ddx_fine";
    if (name == "dFdyFine") return "ddy_fine";
    if (name == "atan")     return "atan2";
    if (name == "EmitVertex") return "/* EmitVertex not supported in DX12 VS/PS */";
    if (name == "EndPrimitive") return "/* EndPrimitive not supported */";
    return name; // identique (abs, sin, cos, pow, sqrt, floor, ceil, round, clamp…)
}

NkString NkSLCodeGenHLSL_DX12::IntrinsicToHLSL(const NkString& name) {
    // Essayer d'abord les wave intrinsics (SM6 seulement)
    NkString wave = WaveIntrinsicToDX12(name);
    if (!wave.Empty()) return wave;
    return IntrinsicToHLSL_Static(name);
}

// =============================================================================
// BuiltinToHLSL
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::BuiltinToHLSL(const NkString& name, NkSLStage stage) {
    if (name == "gl_Position")          return "output._Position";
    if (name == "gl_FragCoord")         return "input._Position";
    if (name == "gl_FragDepth")         return "output._Depth";
    if (name == "gl_VertexID")          return "input._VertexID";
    if (name == "gl_InstanceID")        return "input._InstanceID";
    if (name == "gl_FrontFacing")       return "input.IsFrontFace";
    if (name == "gl_LocalInvocationID") return "GroupThreadID";
    if (name == "gl_GlobalInvocationID")return "DispatchThreadID";
    if (name == "gl_WorkGroupID")       return "GroupID";
    return name;
}

// =============================================================================
// SemanticFor (identique à DX11, autoIndex passé en paramètre)
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::SemanticFor(NkSLVarDeclNode* v, NkSLStage stage,
                                             bool isInput, bool isFragOut,
                                             int autoIndex) {
    NkString name = v->name.ToLower();
    for (int i = 0; kSemRulesDX12[i].nameLower; i++) {
        auto& r = kSemRulesDX12[i];
        bool match = r.isPrefix ? name.StartsWith(r.nameLower) : name == r.nameLower;
        if (!match) continue;
        if (isFragOut && r.fragOutSem && r.fragOutSem[0]) {
            if (name.StartsWith("fragcolor") || name.StartsWith("outcolor")) {
                for (int j = 0; j < 4; j++) {
                    char nb[4]; snprintf(nb, sizeof(nb), "%d", j);
                    if (name.EndsWith(NkString(nb).View())) {
                        char buf[32]; snprintf(buf, sizeof(buf), "SV_Target%d", j);
                        return NkString(buf);
                    }
                }
                int loc = v->binding.HasLocation() ? v->binding.location : 0;
                char buf[32]; snprintf(buf, sizeof(buf), "SV_Target%d", loc);
                return NkString(buf);
            }
            return r.fragOutSem;
        }
        // inputSem (POSITION/NORMAL/COLOR…) UNIQUEMENT pour les attributs d'entree
        // du VERTEX (ils matchent le vertex layout cote device). Pour l'entree du
        // FRAGMENT (varyings issus du VS), la semantique DOIT etre identique a la
        // sortie du vertex -> TEXCOORD<loc>. Sinon le linkage VS->PS echoue : ex.
        // vnormal sort en TEXCOORD1 mais entrerait en NORMAL -> les signatures ne
        // matchent pas -> CreateGraphicsPipelineState E_INVALIDARG.
        if (isInput && stage == NkSLStage::NK_VERTEX && r.inputSem && r.inputSem[0])
            return r.inputSem;
        if (isInput && stage != NkSLStage::NK_VERTEX) {
            int loc = v->binding.HasLocation() ? v->binding.location : autoIndex;
            char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", loc);
            return NkString(buf);
        }
        if (!isInput && !isFragOut && r.outputSem && r.outputSem[0]) {
            int loc = v->binding.HasLocation() ? v->binding.location : autoIndex;
            char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", loc);
            return NkString(buf);
        }
    }
    // Fallback
    if (isFragOut) {
        int loc = v->binding.HasLocation() ? v->binding.location : 0;
        char buf[32]; snprintf(buf, sizeof(buf), "SV_Target%d", loc);
        return NkString(buf);
    }
    if (!isInput) {
        char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
        return NkString(buf);
    }
    char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
    return NkString(buf);
}

// =============================================================================
// CollectDecls
// =============================================================================
void NkSLCodeGenHLSL_DX12::CollectDecls(NkSLProgramNode* prog) {
    mInputVars.Clear(); mOutputVars.Clear();
    mUniforms.Clear(); mCBuffers.Clear(); mSBuffers.Clear();
    mMatrixNames.Clear(); mWritesDepth = false;
    mReg = 0;

    for (auto* node : prog->children) {
        if (!node) continue;
        if (node->kind == NkSLNodeKind::NK_DECL_VAR ||
            node->kind == NkSLNodeKind::NK_DECL_INPUT ||
            node->kind == NkSLNodeKind::NK_DECL_OUTPUT) {
            auto* v = static_cast<NkSLVarDeclNode*>(node);
            if (v->storage == NkSLStorageQual::NK_IN)
                mInputVars.PushBack(v);
            else if (v->storage == NkSLStorageQual::NK_OUT)
                mOutputVars.PushBack(v);
            else if (v->storage == NkSLStorageQual::NK_UNIFORM)
                mUniforms.PushBack(v);
        } else if (node->kind == NkSLNodeKind::NK_DECL_UNIFORM_BLOCK ||
                   node->kind == NkSLNodeKind::NK_DECL_PUSH_CONSTANT) {
            mCBuffers.PushBack(static_cast<NkSLBlockDeclNode*>(node));
        } else if (node->kind == NkSLNodeKind::NK_DECL_STORAGE_BLOCK) {
            mSBuffers.PushBack(static_cast<NkSLBlockDeclNode*>(node));
        } else if (node->kind == NkSLNodeKind::NK_DECL_FUNCTION) {
            auto* fn = static_cast<NkSLFunctionDeclNode*>(node);
            if (fn->body && fn->isEntry) {
                mWritesDepth = ScanWritesDepthDX12(fn->body);
            }
        }
    }
}

// =============================================================================
// GenRootSignature inline (optionnel, DX12 seulement)
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenRootSignature() {
    // Construit une RootSignature minimale à partir des CBuffers et SRVs détectés
    NkString rs = "\"";
    bool first = true;
    int slot = 0;
    int space = (int)mOpts->dx12DefaultSpace;

    for (auto* b : mCBuffers) {
        if (!first) rs += ", ";
        char buf[64];
        snprintf(buf, sizeof(buf), "CBV(b%d, space%d)", slot++, space);
        rs += NkString(buf);
        first = false;
    }
    for (auto* v : mUniforms) {
        if (!v->type) continue;
        bool isSampler = NkSLTypeIsSampler(v->type->baseType);
        if (!isSampler) continue;
        if (!first) rs += ", ";
        char buf[64];
        snprintf(buf, sizeof(buf), "DescriptorTable(SRV(t%d, space%d))", mReg, space);
        rs += NkString(buf);
        first = false;
        mReg++;
    }
    rs += "\"";

    EmitLine("#define NKSL_ROOT_SIG " + rs);
    EmitLine("[RootSignature(NKSL_ROOT_SIG)]");
}

// =============================================================================
// GenBindlessHeader SM6.6
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenBindlessHeader() {
    EmitLine("// Bindless resources — SM6.6 descriptor heaps");
    EmitLine("#define NKSL_TEX2D(idx)       ((Texture2D)         ResourceDescriptorHeap[idx])");
    EmitLine("#define NKSL_RWTEX2D(idx)     ((RWTexture2D<float4>)ResourceDescriptorHeap[idx])");
    EmitLine("#define NKSL_BUF(T, idx)      ((StructuredBuffer<T>)ResourceDescriptorHeap[idx])");
    EmitLine("#define NKSL_RWBUF(T, idx)    ((RWStructuredBuffer<T>)ResourceDescriptorHeap[idx])");
    EmitLine("#define NKSL_SAMPLER(idx)     ((SamplerState)SamplerDescriptorHeap[idx])");
    EmitLine("#define NKSL_SAMPLER_CMP(idx) ((SamplerComparisonState)SamplerDescriptorHeap[idx])");
    EmitNewLine();
}

// =============================================================================
// GenInputOutputStructs
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenInputOutputStructs() {
    mInputStructName  = "NkInput";
    mOutputStructName = "NkOutput";

    // Input struct
    EmitLine("struct " + mInputStructName);
    EmitLine("{");
    IndentPush();
    // FRAGMENT : SV_Position declare AVANT les varyings, dans le MEME ordre que la
    // sortie du VERTEX (qui met SV_Position en premier). D3D12 assigne les registres
    // hardware dans l'ordre de declaration ; si SV_Position est en 1er cote VS-out
    // mais en dernier cote PS-in, les TEXCOORD/SV_Position tombent sur des registres
    // differents -> « Signatures between stages are incompatible » au PSO. (DX11 met
    // aussi SV_Position en premier des deux cotes.)
    if (mStage == NkSLStage::NK_FRAGMENT)
        EmitLine("float4 _Position  : SV_Position;");
    int autoIdx = 0;
    int vtxTexIdx = 0; // index TEXCOORD courant pour les attributs VS (uv, uv2…)
    for (auto* v : mInputVars) {
        NkString type = TypeToHLSL(v->type);
        NkString suf  = DX12_ArrSuffix(v->type);
        NkString sem;
        if (mStage == NkSLStage::NK_VERTEX) {
            // Sémantiques d'ENTRÉE VS = input layout du renderer (NkRender3D) :
            // POSITION0·NORMAL0·TANGENT0·TEXCOORD0(uv)·TEXCOORD1(uv2)·COLOR0. Les noms
            // sont préfixés "a" → classer par sous-chaîne, UV par index courant (PAS la
            // @location, sinon aUV→TEXCOORD3 ≠ layout → vColor/vUV garbage → matériaux noirs).
            NkString n = v->name.ToLower();
            if      (n.Contains("pos"))      sem = "POSITION";
            else if (n.Contains("normal") || n.Contains("nrm")) sem = "NORMAL";
            else if (n.Contains("tangent") || n.Contains("bitangent")) sem = "TANGENT";
            else if (n.Contains("color") || n.Contains("col")) sem = "COLOR";
            else { char b[24]; snprintf(b,sizeof(b),"TEXCOORD%d",vtxTexIdx++); sem = b; }
        } else {
            sem = SemanticFor(v, mStage, true, false, autoIdx++);
        }
        if (!sem.Empty()) EmitLine(type + " " + v->name + suf + " : " + sem + ";");
        else              EmitLine(type + " " + v->name + suf + ";");
    }
    if (mStage == NkSLStage::NK_VERTEX) {
        EmitLine("uint _VertexID   : SV_VertexID;");
        EmitLine("uint _InstanceID : SV_InstanceID;");
    }
    if (mStage == NkSLStage::NK_FRAGMENT)
        EmitLine("bool   IsFrontFace: SV_IsFrontFace;");
    IndentPop();
    EmitLine("};");
    EmitNewLine();

    // Output struct
    EmitLine("struct " + mOutputStructName);
    EmitLine("{");
    IndentPush();
    if (mStage == NkSLStage::NK_VERTEX) {
        EmitLine("float4 _Position : SV_Position;");
    }
    autoIdx = 0;
    for (auto* v : mOutputVars) {
        NkString type    = TypeToHLSL(v->type);
        NkString suf     = DX12_ArrSuffix(v->type);
        bool     isFOut  = (mStage == NkSLStage::NK_FRAGMENT);
        NkString sem     = SemanticFor(v, mStage, false, isFOut, autoIdx++);
        if (!sem.Empty()) EmitLine(type + " " + v->name + suf + " : " + sem + ";");
        else              EmitLine(type + " " + v->name + suf + ";");
    }
    if (mWritesDepth) {
        EmitLine("float _Depth : SV_Depth;");
    }
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

// =============================================================================
// GenCBuffer — DX12 : register(bN, spaceM)
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenCBuffer(NkSLBlockDeclNode* b) {
    // Compteur partage : avancer mReg meme si le binding est explicite (le parser
    // capture @binding pour les blocs mais PAS pour les samplers -> sans cet
    // avancement, les samplers repartiraient de t0 au lieu de suivre le ubo).
    int slot  = b->binding.HasBinding() ? b->binding.binding : mReg;
    if (slot >= mReg) mReg = slot + 1;
    int space = (int)mOpts->dx12DefaultSpace;

    if (b->storage == NkSLStorageQual::NK_PUSH_CONSTANT) {
        // DX12 : push constants → ConstantBuffer<struct> inline dans le root signature.
        // Le struct DOIT être déclaré AVANT le ConstantBuffer<> (sinon `unexpected token`).
        EmitLine("struct " + b->blockName);
        EmitLine("{");
        IndentPush();
        for (auto* m : b->members) {
            EmitLine(TypeToHLSL(m->type) + " " + m->name.ToLower() + DX12_ArrSuffix(m->type) + ";");
            if (m->type && NkSLTypeIsMatrix(m->type->baseType))
                mMatrixNames.PushBack(m->name.ToLower());
        }
        IndentPop();
        EmitLine("};");
        // Push constants → root constants côté device : registre FIXE b0, space1 (dédié)
        // pour matcher NkDX12RootLayout::ROOT_CONSTANTS (sinon mismatch root-sig / push
        // data jamais reçue). space1 évite toute collision avec les cbuffers (space0).
        EmitLine("ConstantBuffer<" + b->blockName + "> " +
                 (b->instanceName.Empty() ? b->blockName : b->instanceName) +
                 RegisterDecl("b", 0, 1) + ";");
        EmitNewLine();
        return;
    }

    EmitLine("cbuffer " + b->blockName + RegisterDecl("b", slot, space));
    EmitLine("{");
    IndentPush();
    // Les membres d'un cbuffer HLSL sont au SCOPE GLOBAL (pas un struct) → une variable
    // locale du même nom les masque (ex. `mat2 isoMat = mat2(uSh2D.isoMat.x…)` → collision).
    // On préfixe par le nom de bloc (décl ET accès, cf. GenExpr MEMBER) pour les isoler.
    NkString pfx = b->blockName; pfx.ToLower(); // ToLower MUTE → copier
    for (auto* m : b->members) {
        NkString mn = pfx + "_" + m->name.ToLower();
        EmitLine(TypeToHLSL(m->type) + " " + mn + DX12_ArrSuffix(m->type) + ";");
        if (m->type && NkSLTypeIsMatrix(m->type->baseType))
            mMatrixNames.PushBack(mn);
    }
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

// =============================================================================
// GenSBuffer — DX12 : register(tN / uN, spaceM)
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenSBuffer(NkSLBlockDeclNode* b) {
    int space = (int)mOpts->dx12DefaultSpace;

    // Construit le struct
    EmitLine("struct " + b->blockName + "Elem");
    EmitLine("{");
    IndentPush();
    for (auto* m : b->members) EmitLine(TypeToHLSL(m->type) + " " + m->name.ToLower() + DX12_ArrSuffix(m->type) + ";");
    IndentPop();
    EmitLine("};");

    // Lecture seule ou lecture-écriture ?
    // Heuristique : buffer sans 'readonly' → RW
    bool readOnly = true; // NkSL n'a pas de qualifier readonly pour l'instant
    NkString instName = b->instanceName.Empty() ? b->blockName : b->instanceName;

    if (readOnly) {
        int slot = b->binding.HasBinding() ? b->binding.binding : mReg++;
        EmitLine("StructuredBuffer<" + b->blockName + "Elem> " +
                 instName + RegisterDecl("t", slot, space) + ";");
    } else {
        int slot = b->binding.HasBinding() ? b->binding.binding : mReg++;
        EmitLine("RWStructuredBuffer<" + b->blockName + "Elem> " +
                 instName + RegisterDecl("u", slot, space) + ";");
    }
    EmitNewLine();
}

// =============================================================================
// GenStruct
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenStruct(NkSLStructDeclNode* s) {
    EmitLine("struct " + s->name);
    EmitLine("{");
    IndentPush();
    for (auto* m : s->members) {
        EmitLine(TypeToHLSL(m->type) + " " + m->name.ToLower() + DX12_ArrSuffix(m->type) + ";");
        if (m->type && NkSLTypeIsMatrix(m->type->baseType))
            mMatrixNames.PushBack(m->name.ToLower()); // membres de struct matriciels → mul()
    }
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

// =============================================================================
// GenVarDecl (uniforms/textures/samplers — DX12 : avec space)
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
    if (!v || !v->type) return;
    if (!isGlobal) {
        // Variable locale : identique à DX11. Nom en lowercase pour matcher les
        // references du corps (l'AST garde la casse d'origine sur la decl mais
        // lowercase les idents references -> sans ToLower, decl 'worldPos' != ref
        // 'worldpos' -> X3004 undeclared en HLSL sensible a la casse).
        // Traquer les variables LOCALES de type matrice : GenExpr BINARY doit
        // emettre mul() (et non '*' composante-a-composante) quand elles
        // multiplient un vecteur/matrice (ex. normalmat * aNormal).
        if (NkSLTypeIsMatrix(v->type->baseType))
            mMatrixNames.PushBack(v->name.ToLower());
        NkString line;
        if (v->isConst) line += "static const ";
        line += TypeToHLSL(v->type) + " " + v->name.ToLower() + DX12_ArrSuffix(v->type);
        if (v->initializer) line += " = " + GenExpr(v->initializer);
        EmitLine(line + ";");
        return;
    }

    // Constante globale (PI, F0, kCascadeFadeStart, tableaux Poisson…) : émise en
    // static const lowercase (sinon non déclarée → X3004 dans le corps).
    if (v->isConst && v->storage != NkSLStorageQual::NK_UNIFORM) {
        NkString line = "static const " + TypeToHLSL(v->type) + " " + v->name.ToLower() + DX12_ArrSuffix(v->type);
        if (v->initializer) line += " = " + GenExpr(v->initializer);
        EmitLine(line + ";");
        return;
    }

    bool isSampler = NkSLTypeIsSampler(v->type->baseType);
    bool isImage   = NkSLTypeIsImage(v->type->baseType);
    int  space     = (int)mOpts->dx12DefaultSpace;

    if (isSampler) {
        if (mOpts->dx12BindlessHeap) {
            // En mode bindless, on n'émet pas les déclarations individuelles
            // — les textures sont accédées via NKSL_TEX2D(idx)
            return;
        }
        // Texture. Nom en lowercase pour matcher les references du corps (GenCall
        // construit "<tex>_tex" a partir du nom lowercase de l'ident) -> sinon
        // decl 'uShadowMap_tex' != ref 'ushadowmap_tex' (HLSL sensible a la casse).
        NkString vn = v->name.ToLower();
        // Un sampler combine = UNE ressource -> texture ET sampler partagent le MEME
        // index de binding (t<n>/s<n>), pris sur le compteur PARTAGE (mReg).
        int slot = v->binding.HasBinding() ? v->binding.binding : mReg++;
        int tSlot = slot;
        NkString texType = BaseTypeToHLSL(v->type->baseType);
        EmitLine(texType + " " + vn + "_tex" + RegisterDecl("t", tSlot, space) + ";");

        // Sampler associé (meme index que la texture)
        int sSlot = slot;
        bool isShadow = (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW ||
                         v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                         v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
        NkString sampType = isShadow ? "SamplerComparisonState" : "SamplerState";
        EmitLine(sampType + " " + vn + "_smp" + RegisterDecl("s", sSlot, space) + ";");
    } else if (isImage) {
        if (mOpts->dx12BindlessHeap) return;
        int uSlot = v->binding.HasBinding() ? v->binding.binding : mReg++;
        // RWTexture2D typé selon le format si fourni (sinon float4/int4/uint4).
        NkString imgType = v->binding.HasImageFormat()
            ? (NkString("RWTexture2D<") + NkSLImageFormatToHLSLElem(v->binding.imageFormat) + ">")
            : BaseTypeToHLSL(v->type->baseType);
        EmitLine(imgType + " " + v->name.ToLower() + RegisterDecl("u", uSlot, space) + ";");
    } else if (v->storage == NkSLStorageQual::NK_UNIFORM) {
        // Uniform scalaire isolé — rare, on le met dans un cbuffer anonyme
        int bSlot = v->binding.HasBinding() ? v->binding.binding : mReg++;
        EmitLine("cbuffer _NkAutoUB" + v->name + RegisterDecl("b", bSlot, space));
        EmitLine("{");
        IndentPush();
        EmitLine(TypeToHLSL(v->type) + " " + v->name + ";");
        IndentPop();
        EmitLine("};");
    }
    // in/out → gérés par GenInputOutputStructs
}

// =============================================================================
// GenDecl
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::NK_DECL_PUSH_CONSTANT:
            GenCBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
            GenSBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_VAR:
        case NkSLNodeKind::NK_DECL_INPUT:
        case NkSLNodeKind::NK_DECL_OUTPUT:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), true); break;
        case NkSLNodeKind::NK_DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node)); break;
        default: break;
    }
}

// =============================================================================
// GenProgram — orchestration DX12
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenProgram(NkSLProgramNode* prog) {
    uint32 sm = mOpts->hlslShaderModelDX12;
    if (sm < 60) sm = 60;

    // En-tête DX12
    char smBuf[16]; snprintf(smBuf, sizeof(smBuf), "%u", sm);
    EmitLine("// HLSL DX12 — Shader Model " + NkString(smBuf) +
             " (generated by NkSL v4.0)");
    EmitLine("#pragma pack_matrix(column_major)");
    EmitLine("#define NKSL_SM " + NkString(smBuf));
    EmitNewLine();

    // ── Helpers NKSL (identiques DX11) ───────────────────────────────────────
    // nksl_texsize2d : textureSize(sampler2D) — GetDimensions() est void en HLSL
    EmitLine("float2 nksl_texsize2d(Texture2D t) { uint w,h; t.GetDimensions(w,h); return float2(w,h); }");
    // nksl_inverse : HLSL n'a pas de inverse() natif pour les matrices
    EmitLine("column_major float3x3 nksl_inverse(column_major float3x3 m) {");
    IndentPush();
    EmitLine("float a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],k=m[2][2];");
    EmitLine("float det = a*(e*k-f*h) - b*(d*k-f*g) + c*(d*h-e*g);");
    EmitLine("float inv = 1.0f/det;");
    EmitLine("column_major float3x3 r;");
    EmitLine("r[0]=float3( (e*k-f*h)*inv, -(b*k-c*h)*inv,  (b*f-c*e)*inv );");
    EmitLine("r[1]=float3(-(d*k-f*g)*inv,  (a*k-c*g)*inv, -(a*f-c*d)*inv );");
    EmitLine("r[2]=float3( (d*h-e*g)*inv, -(a*h-b*g)*inv,  (a*e-b*d)*inv );");
    EmitLine("return r;");
    IndentPop();
    EmitLine("}");
    EmitNewLine();

    // Taille de workgroup compute (depuis l'AST), pour [numthreads(...)].
    mLocalSizeX = prog->localSizeX;
    mLocalSizeY = prog->localSizeY;
    mLocalSizeZ = prog->localSizeZ;

    // SM6.6 bindless header
    if (mOpts->dx12BindlessHeap && sm >= 66) {
        GenBindlessHeader();
    }

    // Structs globaux
    for (auto* node : prog->children) {
        if (node && node->kind == NkSLNodeKind::NK_DECL_STRUCT)
            GenDecl(node);
    }

    // CBuffers et SBuffers
    for (auto* node : prog->children) {
        if (!node) continue;
        if (node->kind == NkSLNodeKind::NK_DECL_UNIFORM_BLOCK ||
            node->kind == NkSLNodeKind::NK_DECL_PUSH_CONSTANT ||
            node->kind == NkSLNodeKind::NK_DECL_STORAGE_BLOCK)
            GenDecl(node);
    }

    // Variables globales (textures, samplers)
    for (auto* node : prog->children) {
        if (!node) continue;
        if (node->kind == NkSLNodeKind::NK_DECL_VAR ||
            node->kind == NkSLNodeKind::NK_DECL_INPUT ||
            node->kind == NkSLNodeKind::NK_DECL_OUTPUT)
            GenDecl(node);
    }

    // I/O structs
    GenInputOutputStructs();

    // Fonctions non-entry (le point d'entree est isEntry OU nomme "main",
    // car la source NkSL ne flague pas toujours isEntry — aligne sur DX11).
    for (auto* node : prog->children) {
        if (!node || node->kind != NkSLNodeKind::NK_DECL_FUNCTION) continue;
        auto* fn = static_cast<NkSLFunctionDeclNode*>(node);
        if (!fn->isEntry && fn->name != "main") GenFunction(fn);
    }

    // Entry points (avec RootSignature inline si demandé)
    for (auto* node : prog->children) {
        if (!node || node->kind != NkSLNodeKind::NK_DECL_FUNCTION) continue;
        auto* fn = static_cast<NkSLFunctionDeclNode*>(node);
        if (!fn->isEntry && fn->name != "main") continue;
        if (mOpts->dx12InlineRootSignature) GenRootSignature();
        GenFunction(fn);
    }
}

// =============================================================================
// GenFunction
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenFunction(NkSLFunctionDeclNode* fn) {
    if (!fn->body) {
        // Prototype
        NkString sig = TypeToHLSL(fn->returnType) + " " + fn->name.ToLower() + "("; // appels lowercase
        for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
            if (i > 0) sig += ", ";
            auto* p = fn->params[i];
            if (p->storage == NkSLStorageQual::NK_OUT)   sig += "out ";
            if (p->storage == NkSLStorageQual::NK_INOUT) sig += "inout ";
            // Sampler param → 2 params HLSL (DX12 sépare texture & sampler : le corps
            // référence <name>_tex / <name>_smp via le bloc texture). Le site d'appel
            // passe aussi les 2 (expansion dans GenCall).
            if (p->type && NkSLTypeIsSampler(p->type->baseType)) {
                NkString pn = p->name; pn.ToLower(); // ToLower MUTE → copier d'abord
                const bool sh = (p->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW       ||
                                 p->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                                 p->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
                sig += TypeToHLSL(p->type) + " " + pn + "_tex, " +
                       (sh ? NkString("SamplerComparisonState") : NkString("SamplerState")) + " " + pn + "_smp";
            } else {
                sig += TypeToHLSL(p->type);
                if (!p->name.Empty()) sig += " " + p->name.ToLower();
            }
        }
        EmitLine(sig + ");");
        return;
    }

    if (fn->isEntry || fn->name == "main") {
        // Entrée compute : [numthreads(X,Y,Z)] + sémantiques SV_* (les builtins
        // gl_GlobalInvocationID/gl_WorkGroupID/gl_LocalInvocationID sont déjà
        // mappés vers DispatchThreadID/GroupID/GroupThreadID par BuiltinToHLSL).
        if (mStage == NkSLStage::NK_COMPUTE) {
            char nt[96];
            snprintf(nt, sizeof(nt), "[numthreads(%u, %u, %u)]",
                     mLocalSizeX, mLocalSizeY, mLocalSizeZ);
            EmitLine(NkString(nt));
            EmitLine("void " + fn->name +
                     "(uint3 DispatchThreadID : SV_DispatchThreadID,"
                     " uint3 GroupID : SV_GroupID,"
                     " uint3 GroupThreadID : SV_GroupThreadID,"
                     " uint GroupIndex : SV_GroupIndex)");
            GenStmt(fn->body);
            EmitNewLine();
            return;
        }
        // Entry point — signature HLSL DX12
        NkString stageSuffix;
        switch (mStage) {
            case NkSLStage::NK_VERTEX:   stageSuffix = "vs_6_0"; break;
            case NkSLStage::NK_FRAGMENT: stageSuffix = "ps_6_0"; break;
            case NkSLStage::NK_COMPUTE:  stageSuffix = "cs_6_0"; break;
            default:                     stageSuffix = "vs_6_0"; break;
        }
        (void)stageSuffix; // utilisé en CMake / dxc invocation, pas ici

        NkString retType = mOutputStructName;
        NkString sig = retType + " " + fn->name +
                       "(" + mInputStructName + " input)";
        EmitLine(sig);
        EmitLine("{");
        IndentPush();
        EmitLine(mOutputStructName + " output;");
        // NB : pas de "(void)output;" — invalide en HLSL (X3017 : on ne caste pas
        // un struct en void). 'output' est de toute façon utilisé/retourné.
        mInEntryReturnsStruct = true; // un `return;` nu → `return output;`
        GenStmt(fn->body);
        mInEntryReturnsStruct = false;
        // Y-flip 3D : DX a NDC Y-bas + origine top-left (comme VK) mais SANS viewport
        // négatif → géométrie 3D inversée. Réplique le flip GL. Skybox fullscreen (sans
        // inputs / pure push-constant / sans varyings) reste droit.
        if (mStage == NkSLStage::NK_VERTEX) {
            bool hasInputs = !mInputVars.Empty();
            bool hasVaryingOut = !mOutputVars.Empty();
            bool hasUBO=false, hasPC=false;
            for (auto* cb : mCBuffers) {
                if (cb->storage == NkSLStorageQual::NK_PUSH_CONSTANT) hasPC=true; else hasUBO=true;
            }
            const bool purePC = hasPC && !hasUBO;
            if (hasInputs && !purePC && hasVaryingOut)
                EmitLine("output._Position.y = -output._Position.y;");
        }
        EmitLine("return output;");
        IndentPop();
        EmitLine("}");
    } else {
        // Fonction helper ordinaire
        NkString sig = TypeToHLSL(fn->returnType) + " " + fn->name.ToLower() + "("; // appels lowercase
        for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
            if (i > 0) sig += ", ";
            auto* p = fn->params[i];
            if (p->storage == NkSLStorageQual::NK_OUT)   sig += "out ";
            if (p->storage == NkSLStorageQual::NK_INOUT) sig += "inout ";
            // Sampler param → 2 params HLSL (DX12 sépare texture & sampler : le corps
            // référence <name>_tex / <name>_smp via le bloc texture). Le site d'appel
            // passe aussi les 2 (expansion dans GenCall).
            if (p->type && NkSLTypeIsSampler(p->type->baseType)) {
                NkString pn = p->name; pn.ToLower(); // ToLower MUTE → copier d'abord
                const bool sh = (p->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW       ||
                                 p->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                                 p->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
                sig += TypeToHLSL(p->type) + " " + pn + "_tex, " +
                       (sh ? NkString("SamplerComparisonState") : NkString("SamplerState")) + " " + pn + "_smp";
            } else {
                sig += TypeToHLSL(p->type);
                if (!p->name.Empty()) sig += " " + p->name.ToLower();
            }
        }
        sig += ")";
        EmitLine(sig);
        GenStmt(fn->body);
    }
    EmitNewLine();
}

// =============================================================================
// GenStmt
// =============================================================================
void NkSLCodeGenHLSL_DX12::GenStmt(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_STMT_BLOCK: {
            EmitLine("{");
            IndentPush();
            for (auto* c : node->children) GenStmt(c);
            IndentPop();
            EmitLine("}");
            break;
        }
        case NkSLNodeKind::NK_STMT_EXPR:
            if (!node->children.Empty())
                EmitLine(GenExpr(node->children[0]) + ";");
            break;
        case NkSLNodeKind::NK_DECL_VAR:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), false);
            break;
        case NkSLNodeKind::NK_STMT_IF: {
            auto* n = static_cast<NkSLIfNode*>(node);
            EmitLine("if (" + GenExpr(n->condition) + ")");
            GenStmt(n->thenBranch);
            if (n->elseBranch) { EmitLine("else"); GenStmt(n->elseBranch); }
            break;
        }
        case NkSLNodeKind::NK_STMT_FOR: {
            auto* n = static_cast<NkSLForNode*>(node);
            NkString init, cond, inc;
            if (n->init) {
                if (n->init->kind == NkSLNodeKind::NK_DECL_VAR) {
                    auto* vd = static_cast<NkSLVarDeclNode*>(n->init);
                    init = TypeToHLSL(vd->type) + " " + vd->name.ToLower();
                    if (vd->initializer) init += " = " + GenExpr(vd->initializer);
                } else { init = GenExpr(n->init); }
            }
            if (n->condition) cond = GenExpr(n->condition);
            if (n->increment) inc  = GenExpr(n->increment);
            EmitLine("for (" + init + "; " + cond + "; " + inc + ")");
            GenStmt(n->body);
            break;
        }
        case NkSLNodeKind::NK_STMT_WHILE: {
            auto* n = static_cast<NkSLWhileNode*>(node);
            EmitLine("while (" + GenExpr(n->condition) + ")");
            GenStmt(n->body);
            break;
        }
        case NkSLNodeKind::NK_STMT_DO_WHILE:
            EmitLine("do");
            GenStmt(node->children[0]);
            EmitLine("while (" + GenExpr(node->children[1]) + ");");
            break;
        case NkSLNodeKind::NK_STMT_RETURN: {
            auto* n = static_cast<NkSLReturnNode*>(node);
            // Dans l'entry (main retourne une struct), un `return;` nu → `return output;`.
            EmitLine(n->value ? ("return " + GenExpr(n->value) + ";")
                              : (mInEntryReturnsStruct ? "return output;" : "return;"));
            break;
        }
        case NkSLNodeKind::NK_STMT_BREAK:    EmitLine("break;");    break;
        case NkSLNodeKind::NK_STMT_CONTINUE: EmitLine("continue;"); break;
        case NkSLNodeKind::NK_STMT_DISCARD:  EmitLine("discard;");  break;
        case NkSLNodeKind::NK_STMT_SWITCH:
            EmitLine("switch (" + GenExpr(node->children[0]) + ")");
            EmitLine("{");
            for (uint32 i = 1; i < (uint32)node->children.Size(); i++)
                GenStmt(node->children[i]);
            EmitLine("}");
            break;
        case NkSLNodeKind::NK_STMT_CASE:
            if (!node->children.Empty())
                EmitLine("case " + GenExpr(node->children[0]) + ":");
            else
                EmitLine("default:");
            break;
        default: break;
    }
}

// =============================================================================
// GenExpr
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::NK_EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::NK_EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            // Builtin GLSL (gl_Position…) d'abord
            NkString builtin = BuiltinToHLSL(id->name, mStage);
            if (builtin != id->name) return builtin;
            // Référence à une entrée/sortie de stage → input.x / output.x.
            // L'AST normalise v->name en lowercase à la déclaration mais garde la
            // casse d'origine dans le corps → comparaison insensible à la casse,
            // et on renvoie v->name pour matcher le champ de struct généré.
            NkString idLow = id->name.ToLower();
            for (auto* v : mInputVars)
                if (v->name.ToLower() == idLow) return "input." + v->name;
            for (auto* v : mOutputVars)
                if (v->name.ToLower() == idLow) return "output." + v->name;
            return id->name;
        }
        case NkSLNodeKind::NK_EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString op = GenExpr(u->operand);
            return u->prefix ? (u->op + op) : (op + u->op);
        }
        case NkSLNodeKind::NK_EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            NkString lhs = GenExpr(b->left);
            NkString rhs = GenExpr(b->right);
            // mat * vec / mat * mat → mul() en HLSL. Un résultat de mul() est
            // lui-même une matrice → détecter aussi StartsWith("mul(") pour les
            // chaînes (proj*view)*world.
            if (b->op == "*") {
                bool lhsMat = lhs.StartsWith("mul(");
                // Casts/constructeurs de matrice (mat3(m)→(float3x3)(…), float3x3(…),
                // nksl_inverse(…)) produisent une matrice → mul() requis.
                if (!lhsMat) {
                    static const char* kMatPfx[] = {
                        "(float2x2)","(float3x3)","(float4x4)","(float2x3)","(float3x2)",
                        "(float2x4)","(float4x2)","(float3x4)","(float4x3)",
                        "float2x2(","float3x3(","float4x4(","nksl_inverse(","transpose(", nullptr };
                    for (int k=0; kMatPfx[k]; k++)
                        if (lhs.StartsWith(kMatPfx[k])) { lhsMat = true; break; }
                }
                if (!lhsMat) {
                    // ATTENTION : NkString::ToLower() MUTE EN PLACE et retourne *this.
                    // Appeler lhs.ToLower() lowercaserait lhs lui-même → la sortie du
                    // `return` ci-dessous deviendrait minuscule (bug `.sample` des
                    // textures multipliées). On copie d'abord.
                    NkString lname = lhs; lname.ToLower();
                    for (auto& mn : mMatrixNames)
                        // exact (var/cbuffer-membre) OU accès membre struct (slots[i].shadowmatrix).
                        if (lname == mn || lname.EndsWith(NkString("." + mn).View())) { lhsMat = true; break; }
                }
                if (lhsMat) return "mul(" + lhs + ", " + rhs + ")";
            }
            return "(" + lhs + " " + b->op + " " + rhs + ")";
        }
        case NkSLNodeKind::NK_EXPR_TERNARY:
            return "(" + GenExpr(node->children[0]) + " ? " +
                         GenExpr(node->children[1]) + " : " +
                         GenExpr(node->children[2]) + ")";
        case NkSLNodeKind::NK_EXPR_ASSIGN: {
            auto* a = static_cast<NkSLAssignNode*>(node);
            return GenExpr(a->lhs) + " " + a->op + " " + GenExpr(a->rhs);
        }
        case NkSLNodeKind::NK_EXPR_CALL:
            return GenCall(static_cast<NkSLCallNode*>(node));
        case NkSLNodeKind::NK_EXPR_MEMBER: {
            auto* m = static_cast<NkSLMemberNode*>(node);
            // Supprimer le préfixe d'instance cbuffer (ubo.model → model) : en
            // HLSL les membres d'un cbuffer sont des globales, pas un struct.
            if (m->object && m->object->kind == NkSLNodeKind::NK_EXPR_IDENT) {
                auto* id = static_cast<NkSLIdentNode*>(m->object);
                NkString idLow = id->name.ToLower();
                for (auto* cb : mCBuffers)
                    if (!cb->instanceName.Empty() &&
                        cb->instanceName.ToLower() == idLow) {
                        // Push constant = `ConstantBuffer<PC> pc` → accès QUALIFIÉ pc.membre
                        // (PAS global). cbuffer classique = membres globaux → on strippe l'instance.
                        // Lowercase toujours (membres déclarés lowercase).
                        if (cb->storage == NkSLStorageQual::NK_PUSH_CONSTANT)
                            return idLow + "." + m->member.ToLower();
                        // cbuffer classique : membres préfixés par nom de bloc (cf. GenCBuffer)
                        // pour éviter la collision avec des locales (scope global HLSL).
                        { NkString pfx = cb->blockName; pfx.ToLower();
                          return pfx + "_" + m->member.ToLower(); }
                    }
            }
            // Membre de struct/array (slots[i].tileUV) : les membres sont DÉCLARÉS en
            // minuscules (GenStruct ToLower) → lowercase l'accès pour matcher. Les
            // swizzles (.xyz/.rgb) sont déjà minuscules → inoffensif.
            return GenExpr(m->object) + "." + m->member.ToLower();
        }
        case NkSLNodeKind::NK_EXPR_INDEX: {
            auto* idx = static_cast<NkSLIndexNode*>(node);
            return GenExpr(idx->array) + "[" + GenExpr(idx->index) + "]";
        }
        case NkSLNodeKind::NK_EXPR_CAST: {
            auto* c = static_cast<NkSLCastNode*>(node);
            return TypeToHLSL(c->targetType) + "(" + GenExpr(c->expr) + ")";
        }
        case NkSLNodeKind::NK_STMT_EXPR:
            return node->children.Empty() ? "" : GenExpr(node->children[0]);
        default:
            return "/* unknown */";
    }
}

// =============================================================================
// GenCall — wave intrinsics + texture calls DX12
// =============================================================================
NkString NkSLCodeGenHLSL_DX12::GenCall(NkSLCallNode* call) {
    NkString callee = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;

    // Construire les args
    NkVector<NkString> argStrs;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++)
        argStrs.PushBack(GenExpr(call->args[i]));

    // Constructeur d'array GLSL `Type[](e0, e1, …)` → initialiseur HLSL `{ … }`.
    // Le callee encode la dim (ex. "vec2[]") → présence de '[' = array ctor.
    {
        bool isArrayCtor = false;
        for (uint32 i = 0; i < callee.Size(); i++)
            if (callee[i] == '[') { isArrayCtor = true; break; }
        if (isArrayCtor) {
            NkString a;
            for (uint32 i = 0; i < (uint32)argStrs.Size(); i++) { if (i>0) a += ", "; a += argStrs[i]; }
            return "{ " + a + " }";
        }
    }

    // Wave intrinsics SM6
    NkString wave = WaveIntrinsicToDX12(callee);
    if (!wave.Empty()) {
        NkString result = wave + "(";
        for (uint32 i = 0; i < (uint32)argStrs.Size(); i++) {
            if (i > 0) result += ", ";
            result += argStrs[i];
        }
        return result + ")";
    }

    // textureSize → helper (GetDimensions() est void, non inlinable)
    if (callee == "textureSize" && !argStrs.Empty())
        return "nksl_texsize2d(" + argStrs[0] + "_tex)";

    // Texture calls
    if ((callee == "texture" || callee == "textureLod" || callee == "texelFetch" ||
         callee == "textureGather" || callee == "textureGrad" || callee == "textureOffset" ||
         callee == "imageLoad" || callee == "imageStore") && !argStrs.Empty()) {
        NkString tex = argStrs[0] + "_tex";
        NkString smp = argStrs[0] + "_smp";
        if (callee == "texture" && argStrs.Size() >= 2) {
            // sampler2DShadow → SampleCmpLevelZero (comparaison hardware).
            bool isShadow = false;
            NkString texLow = argStrs[0].ToLower();
            for (auto* v : mUniforms) {
                if (!v->type) continue;
                if (v->name.ToLower() == texLow) {
                    isShadow = (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW       ||
                                v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                                v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
                    break;
                }
            }
            if (isShadow) {
                // texture(sampler2DShadow, vec3(u,v,z)) → tex.SampleCmpLevelZero(smp, float2(u,v), z)
                auto* coordNode = call->args[1];
                if (coordNode && coordNode->kind == NkSLNodeKind::NK_EXPR_CALL) {
                    auto* vc = static_cast<NkSLCallNode*>(coordNode);
                    if ((vc->callee == "vec3" || vc->callee == "float3") && vc->args.Size() == 3) {
                        NkString su = GenExpr(vc->args[0]);
                        NkString sv = GenExpr(vc->args[1]);
                        NkString sz = GenExpr(vc->args[2]);
                        return tex + ".SampleCmpLevelZero(" + smp + ", float2(" +
                               su + ", " + sv + "), " + sz + ")";
                    }
                }
                return tex + ".SampleCmpLevelZero(" + smp + ", (" + argStrs[1] +
                       ").xy, (" + argStrs[1] + ").z)";
            }
            return tex + ".Sample(" + smp + ", " + argStrs[1] + ")";
        }
        if (callee == "textureLod" && argStrs.Size() >= 3) {
            // Sampler shadow : textureLod(s, vec3(uv,z), lod) = comparaison d'ombre à LOD
            // explicite → SampleCmpLevelZero (sinon SampleLevel + SamplerComparisonState = invalide).
            bool isShadowLod = false;
            NkString texLow2 = argStrs[0].ToLower();
            for (auto* v : mUniforms)
                if (v->type && v->name.ToLower() == texLow2) {
                    isShadowLod = (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW       ||
                                   v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                                   v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
                    break;
                }
            if (isShadowLod)
                return tex + ".SampleCmpLevelZero(" + smp + ", (" + argStrs[1] + ").xy, (" + argStrs[1] + ").z)";
            return tex + ".SampleLevel(" + smp + ", " + argStrs[1] + ", " + argStrs[2] + ")";
        }
        if (callee == "textureOffset" && argStrs.Size() >= 3)
            return tex + ".Sample(" + smp + ", " + argStrs[1] + ", " + argStrs[2] + ")";
        if (callee == "texelFetch" && argStrs.Size() >= 3)
            return tex + ".Load(int3(" + argStrs[1] + ", " + argStrs[2] + "))";
        if (callee == "textureGather" && argStrs.Size() >= 2)
            return tex + ".Gather(" + smp + ", " + argStrs[1] + ")";
        if (callee == "textureGrad" && argStrs.Size() >= 4)
            return tex + ".SampleGrad(" + smp + ", " + argStrs[1] + ", " +
                   argStrs[2] + ", " + argStrs[3] + ")";
        if (callee == "imageLoad" && argStrs.Size() >= 2)
            return argStrs[0] + "[" + argStrs[1] + "]";
        if (callee == "imageStore" && argStrs.Size() >= 3)
            return argStrs[0] + "[" + argStrs[1] + "] = " + argStrs[2];
    }

    // Barrières compute
    if (callee == "barrier" || callee == "memoryBarrier")
        return "GroupMemoryBarrierWithGroupSync()";
    if (callee == "groupMemoryBarrier")
        return "GroupMemoryBarrier()";

    // atan(y,x) 2 args → atan2 ; comparaisons vectorielles GLSL → opérateurs HLSL.
    if (argStrs.Size() == 2) {
        if (callee == "atan") return "atan2(" + argStrs[0] + ", " + argStrs[1] + ")";
        const char* relOp = nullptr;
        if      (callee == "lessThan")         relOp = "<";
        else if (callee == "lessThanEqual")    relOp = "<=";
        else if (callee == "greaterThan")      relOp = ">";
        else if (callee == "greaterThanEqual") relOp = ">=";
        else if (callee == "equal")            relOp = "==";
        else if (callee == "notEqual")         relOp = "!=";
        if (relOp) return "(" + argStrs[0] + " " + relOp + " " + argStrs[1] + ")";
    }

    // Intrinsèques GLSL→HLSL génériques (vec4→float4, mat3→float3x3,
    // inverse→nksl_inverse, mix→lerp… via IntrinsicToHLSL)
    NkString mapped = IntrinsicToHLSL(callee);
    // FXC/dxc n'acceptent pas floatN(scalaire) ni floatNxN(matN) en forme
    // fonction → forme cast (float3x3)(m) / (float3)(s) (extraction / broadcast).
    if (argStrs.Size() == 1 &&
        (mapped == "float2"   || mapped == "float3"   || mapped == "float4"   ||
         mapped == "int2"     || mapped == "int3"     || mapped == "int4"     ||
         mapped == "uint2"    || mapped == "uint3"    || mapped == "uint4"    ||
         mapped == "float2x2" || mapped == "float3x3" || mapped == "float4x4")) {
        return "(" + mapped + ")(" + argStrs[0] + ")";
    }
    // Lowercase le nom d'appel : les fonctions utilisateur sont DÉCLARÉES en minuscules
    // (GenFunction) mais l'AST garde la casse d'origine aux appels (NkShadowSlopeBiasMul,
    // G_Schlick, D_GGX…). Les intrinsèques/constructeurs HLSL (lerp, float3, mul…) sont
    // déjà minuscules → lowercase inoffensif. Les méthodes texture sont traitées plus haut.
    // CONVENTION MATRICE constructeur : GLSL `matN(c0,c1,...)` prend les COLONNES,
    // alors que HLSL `floatNxN(r0,r1,...)` prend les LIGNES. Sans correction, un
    // `mat3(T,B,N) * v` (GLSL) émis tel quel donne `mul(float3x3(T,B,N), v)` qui
    // est la TRANSPOSÉE → N de surface faux sur DX12 → éclairage direct ≈ 0 (scène
    // sombre). On enveloppe donc tout constructeur de matrice multi-arguments dans
    // transpose() pour rétablir la sémantique colonne de GLSL. (Le cas 1-arg =
    // cast/extraction matrice→matrice est traité plus haut, sans transpose.)
    const bool isMatCtor = (mapped == "float2x2" || mapped == "float3x3" ||
                            mapped == "float4x4" || mapped == "float2x3" ||
                            mapped == "float3x2" || mapped == "float2x4" ||
                            mapped == "float4x2" || mapped == "float3x4" ||
                            mapped == "float4x3");
    const bool wrapTranspose = isMatCtor && argStrs.Size() > 1;

    NkString result = mapped.ToLower() + "(";
    for (uint32 i = 0; i < (uint32)argStrs.Size(); i++) {
        if (i > 0) result += ", ";
        // Expansion sampler : un sampler uniforme passé en argument d'une fonction
        // utilisateur doit fournir texture + sampler (DX12 les sépare ; le param de la
        // fonction a été éclaté en <name>_tex / <name>_smp dans GenFunction).
        bool expanded = false;
        if (i < (uint32)call->args.Size() && call->args[i] &&
            call->args[i]->kind == NkSLNodeKind::NK_EXPR_IDENT) {
            NkString anLow = static_cast<NkSLIdentNode*>(call->args[i])->name; anLow.ToLower();
            for (auto* v : mUniforms)
                if (v->type && NkSLTypeIsSampler(v->type->baseType)) {
                    NkString vn = v->name; vn.ToLower();
                    if (vn == anLow) { result += vn + "_tex, " + vn + "_smp"; expanded = true; break; }
                }
        }
        if (!expanded) result += argStrs[i];
    }
    result += ")";
    if (wrapTranspose) return "transpose(" + result + ")";
    return result;
}

NkString NkSLCodeGenHLSL_DX12::LiteralToStr(NkSLLiteralNode* lit) {
    char buf[64];
    switch (lit->baseType) {
        case NkSLBaseType::NK_INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)lit->intVal); return buf;
        case NkSLBaseType::NK_UINT:
            snprintf(buf, sizeof(buf), "%lluu", (unsigned long long)lit->uintVal); return buf;
        case NkSLBaseType::NK_FLOAT: {
            snprintf(buf, sizeof(buf), "%.8g", lit->floatVal);
            bool hasDot = false;
            for (int i = 0; buf[i]; i++) if (buf[i]=='.'||buf[i]=='e'||buf[i]=='E') hasDot=true;
            NkString s(buf); if (!hasDot) s += ".0";
            // Suffixe 'f' explicite (comme DX11) : sans lui, dxc/fxc traitent le
            // littéral comme un double → promotions/mismatch de type dans les ops
            // vectorielles (X3020). Force le type float.
            return s + "f";
        }
        case NkSLBaseType::NK_DOUBLE:
            snprintf(buf, sizeof(buf), "%.16glf", lit->floatVal); return buf;
        case NkSLBaseType::NK_BOOL:
            return lit->boolVal ? "true" : "false";
        default: return "0";
    }
}

// =============================================================================
// Generate — point d'entrée public
// =============================================================================
NkSLCompileResult NkSLCodeGenHLSL_DX12::Generate(NkSLProgramNode* ast,
                                                    NkSLStage stage,
                                                    const NkSLCompileOptions& opts) {
    mOpts   = &opts;
    mStage  = stage;
    mOutput = "";
    mErrors.Clear();
    mWarnings.Clear();

    CollectDecls(ast);
    GenProgram(ast);

    NkSLCompileResult res;
    res.success  = mErrors.Empty();
    res.source   = mOutput;
    res.target   = NkSLTarget::NK_HLSL_DX12;
    res.stage    = stage;
    res.errors   = mErrors;
    res.warnings = mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

} // namespace nkentseu
