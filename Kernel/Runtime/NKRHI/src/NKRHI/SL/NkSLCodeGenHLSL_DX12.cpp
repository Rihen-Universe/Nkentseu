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
#include "NkSLCodeGen.h"
#include "NkSLSemantic.h"
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
        default: return "float";
    }
}

NkString NkSLCodeGenHLSL_DX12::TypeToHLSL(NkSLTypeNode* t) {
    if (!t) return "void";
    if (t->baseType == NkSLBaseType::NK_STRUCT) return t->typeName;
    NkString s = BaseTypeToHLSL(t->baseType);
    if (t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        s += NkString(buf);
    }
    return s;
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
        if (isInput && r.inputSem && r.inputSem[0]) return r.inputSem;
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
    mRegB = mRegT = mRegS = mRegU = 0;

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
        snprintf(buf, sizeof(buf), "DescriptorTable(SRV(t%d, space%d))", mRegT, space);
        rs += NkString(buf);
        first = false;
        mRegT++;
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
    int autoIdx = 0;
    for (auto* v : mInputVars) {
        NkString type = TypeToHLSL(v->type);
        NkString sem  = SemanticFor(v, mStage, true, false, autoIdx++);
        if (!sem.Empty()) {
            EmitLine(type + " " + v->name + " : " + sem + ";");
        } else {
            EmitLine(type + " " + v->name + ";");
        }
    }
    if (mStage == NkSLStage::NK_VERTEX) {
        EmitLine("uint _VertexID   : SV_VertexID;");
        EmitLine("uint _InstanceID : SV_InstanceID;");
    }
    if (mStage == NkSLStage::NK_FRAGMENT) {
        EmitLine("float4 _Position  : SV_Position;");
        EmitLine("bool   IsFrontFace: SV_IsFrontFace;");
    }
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
        bool     isFOut  = (mStage == NkSLStage::NK_FRAGMENT);
        NkString sem     = SemanticFor(v, mStage, false, isFOut, autoIdx++);
        if (!sem.Empty()) {
            EmitLine(type + " " + v->name + " : " + sem + ";");
        } else {
            EmitLine(type + " " + v->name + ";");
        }
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
    int slot  = b->binding.HasBinding() ? b->binding.binding : mRegB++;
    int space = (int)mOpts->dx12DefaultSpace;

    if (b->storage == NkSLStorageQual::NK_PUSH_CONSTANT) {
        // DX12 : push constants → ConstantBuffer inline dans le root signature
        // On les mappe sur un cbuffer en b0 de l'espace dédié
        EmitLine("ConstantBuffer<" + b->blockName + "> " +
                 (b->instanceName.Empty() ? b->blockName : b->instanceName) +
                 RegisterDecl("b", slot, space) + ";");
        // Émet aussi le struct contenant les membres
        EmitLine("struct " + b->blockName);
        EmitLine("{");
        IndentPush();
        for (auto* m : b->members) {
            NkString line = TypeToHLSL(m->type) + " " + m->name;
            if (m->type && NkSLTypeIsMatrix(m->type->baseType)) {
                mMatrixNames.PushBack(m->name.ToLower());
            }
            EmitLine(line + ";");
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        return;
    }

    EmitLine("cbuffer " + b->blockName + RegisterDecl("b", slot, space));
    EmitLine("{");
    IndentPush();
    for (auto* m : b->members) {
        NkString line = TypeToHLSL(m->type) + " " + m->name;
        if (m->type && NkSLTypeIsMatrix(m->type->baseType)) {
            mMatrixNames.PushBack(m->name.ToLower());
        }
        EmitLine(line + ";");
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
    for (auto* m : b->members) EmitLine(TypeToHLSL(m->type) + " " + m->name + ";");
    IndentPop();
    EmitLine("};");

    // Lecture seule ou lecture-écriture ?
    // Heuristique : buffer sans 'readonly' → RW
    bool readOnly = true; // NkSL n'a pas de qualifier readonly pour l'instant
    NkString instName = b->instanceName.Empty() ? b->blockName : b->instanceName;

    if (readOnly) {
        int slot = b->binding.HasBinding() ? b->binding.binding : mRegT++;
        EmitLine("StructuredBuffer<" + b->blockName + "Elem> " +
                 instName + RegisterDecl("t", slot, space) + ";");
    } else {
        int slot = b->binding.HasBinding() ? b->binding.binding : mRegU++;
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
        NkString line = TypeToHLSL(m->type) + " " + m->name;
        EmitLine(line + ";");
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
        // Variable locale : identique à DX11
        NkString line;
        if (v->isConst) line += "static const ";
        line += TypeToHLSL(v->type) + " " + v->name;
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
        // Texture
        int tSlot = v->binding.HasBinding() ? v->binding.binding : mRegT++;
        NkString texType = BaseTypeToHLSL(v->type->baseType);
        EmitLine(texType + " " + v->name + "_tex" + RegisterDecl("t", tSlot, space) + ";");

        // Sampler associé
        int sSlot = v->binding.HasBinding() ? v->binding.binding : mRegS++;
        bool isShadow = (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW ||
                         v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                         v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
        NkString sampType = isShadow ? "SamplerComparisonState" : "SamplerState";
        EmitLine(sampType + " " + v->name + "_smp" + RegisterDecl("s", sSlot, space) + ";");
    } else if (isImage) {
        if (mOpts->dx12BindlessHeap) return;
        int uSlot = v->binding.HasBinding() ? v->binding.binding : mRegU++;
        EmitLine(BaseTypeToHLSL(v->type->baseType) + " " +
                 v->name + RegisterDecl("u", uSlot, space) + ";");
    } else if (v->storage == NkSLStorageQual::NK_UNIFORM) {
        // Uniform scalaire isolé — rare, on le met dans un cbuffer anonyme
        int bSlot = v->binding.HasBinding() ? v->binding.binding : mRegB++;
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

    // Fonctions non-entry
    for (auto* node : prog->children) {
        if (!node || node->kind != NkSLNodeKind::NK_DECL_FUNCTION) continue;
        auto* fn = static_cast<NkSLFunctionDeclNode*>(node);
        if (!fn->isEntry) GenFunction(fn);
    }

    // Entry points (avec RootSignature inline si demandé)
    for (auto* node : prog->children) {
        if (!node || node->kind != NkSLNodeKind::NK_DECL_FUNCTION) continue;
        auto* fn = static_cast<NkSLFunctionDeclNode*>(node);
        if (!fn->isEntry) continue;
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
        NkString sig = TypeToHLSL(fn->returnType) + " " + fn->name + "(";
        for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
            if (i > 0) sig += ", ";
            auto* p = fn->params[i];
            if (p->storage == NkSLStorageQual::NK_OUT)   sig += "out ";
            if (p->storage == NkSLStorageQual::NK_INOUT) sig += "inout ";
            sig += TypeToHLSL(p->type);
            if (!p->name.Empty()) sig += " " + p->name;
        }
        EmitLine(sig + ");");
        return;
    }

    if (fn->isEntry) {
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
        EmitLine("(void)output;");
        GenStmt(fn->body);
        EmitLine("return output;");
        IndentPop();
        EmitLine("}");
    } else {
        // Fonction helper ordinaire
        NkString sig = TypeToHLSL(fn->returnType) + " " + fn->name + "(";
        for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
            if (i > 0) sig += ", ";
            auto* p = fn->params[i];
            if (p->storage == NkSLStorageQual::NK_OUT)   sig += "out ";
            if (p->storage == NkSLStorageQual::NK_INOUT) sig += "inout ";
            sig += TypeToHLSL(p->type);
            if (!p->name.Empty()) sig += " " + p->name;
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
                    init = TypeToHLSL(vd->type) + " " + vd->name;
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
            EmitLine(n->value ? ("return " + GenExpr(n->value) + ";") : "return;");
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
            return BuiltinToHLSL(id->name, mStage);
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
            // mat * vec → mul() en HLSL
            if (b->op == "*") {
                NkString lname = lhs.ToLower();
                bool lhsMat = false;
                for (auto& mn : mMatrixNames)
                    if (lname == mn) { lhsMat = true; break; }
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
            return GenExpr(m->object) + "." + m->member;
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

    // Texture calls
    if ((callee == "texture" || callee == "textureLod" || callee == "texelFetch" ||
         callee == "textureGather" || callee == "textureGrad" || callee == "textureOffset" ||
         callee == "imageLoad" || callee == "imageStore") && !argStrs.Empty()) {
        NkString tex = argStrs[0] + "_tex";
        NkString smp = argStrs[0] + "_smp";
        if (callee == "texture" && argStrs.Size() >= 2)
            return tex + ".Sample(" + smp + ", " + argStrs[1] + ")";
        if (callee == "textureLod" && argStrs.Size() >= 3)
            return tex + ".SampleLevel(" + smp + ", " + argStrs[1] + ", " + argStrs[2] + ")";
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

    // Intrinsèques GLSL→HLSL génériques
    NkString mapped = IntrinsicToHLSL(callee);
    NkString result = mapped + "(";
    for (uint32 i = 0; i < (uint32)argStrs.Size(); i++) {
        if (i > 0) result += ", ";
        result += argStrs[i];
    }
    return result + ")";
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
            return s;
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
