// =============================================================================
// NkSLCodeGen_Advanced.cpp  — v3.0
//
// CORRECTION BUG: SemanticFor() ne prend plus de static local.
// La signature est maintenant :
//   NkString SemanticFor(NkSLVarDeclNode* v, NkSLStage stage, bool isInput, int autoIndex)
// Le compteur autoIndex est passé depuis GenInputOutputStructs() comme variable locale.
// =============================================================================
#include "NkSLCodeGen.h"
#include "NkSLSemantic.h"
#include "NkSLCompiler.h"
#include "NkSLLexer.h"
#include "NkSLParser.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Texture calls HLSL
// =============================================================================
static NkString TranslateTextureCallHLSL(const NkString& funcName,
                                           const NkVector<NkString>& args) {
    if (args.Empty()) return funcName + "()";
    NkString tex  = args[0] + "_tex";
    NkString smp  = args[0] + "_smp";

    if (funcName == "texture" && args.Size() >= 2)
        return tex + ".Sample(" + smp + ", " + args[1] + ")";
    if (funcName == "textureLod" && args.Size() >= 3)
        return tex + ".SampleLevel(" + smp + ", " + args[1] + ", " + args[2] + ")";
    if (funcName == "textureLodOffset" && args.Size() >= 4)
        return tex + ".SampleLevel(" + smp + ", " + args[1] + ", " +
               args[2] + ", " + args[3] + ")";
    if (funcName == "textureOffset" && args.Size() >= 3)
        return tex + ".Sample(" + smp + ", " + args[1] + ", " + args[2] + ")";
    if (funcName == "texelFetch" && args.Size() >= 3)
        return tex + ".Load(int3(" + args[1] + ", " + args[2] + "))";
    if (funcName == "textureGather" && args.Size() >= 2)
        return tex + ".Gather(" + smp + ", " + args[1] + ")";
    if (funcName == "textureGrad" && args.Size() >= 4)
        return tex + ".SampleGrad(" + smp + ", " + args[1] + ", " +
               args[2] + ", " + args[3] + ")";
    if (funcName == "textureSize")
        return "((int2)(uint2){0,0}/* use GetDimensions */)";
    if (funcName == "imageLoad" && args.Size() >= 2)
        return args[0] + "[" + args[1] + "]";
    if (funcName == "imageStore" && args.Size() >= 3)
        return args[0] + "[" + args[1] + "] = " + args[2];
    if (funcName == "imageAtomicAdd"      && args.Size() >= 3)
        return "InterlockedAdd(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicMin"      && args.Size() >= 3)
        return "InterlockedMin(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicMax"      && args.Size() >= 3)
        return "InterlockedMax(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicAnd"      && args.Size() >= 3)
        return "InterlockedAnd(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicOr"       && args.Size() >= 3)
        return "InterlockedOr(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicXor"      && args.Size() >= 3)
        return "InterlockedXor(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicExchange" && args.Size() >= 3)
        return "InterlockedExchange(" + args[0] + "[" + args[1] + "], " + args[2] + ")";
    if (funcName == "imageAtomicCompSwap" && args.Size() >= 4)
        return "InterlockedCompareExchange(" + args[0] + "[" + args[1] + "], " +
               args[2] + ", " + args[3] + ")";
    if (funcName == "barrier" || funcName == "memoryBarrier")
        return "GroupMemoryBarrierWithGroupSync()";
    if (funcName == "groupMemoryBarrier")
        return "GroupMemoryBarrier()";
    if (funcName == "imageSize" && args.Size() >= 1)
        return "((int2)0 /* use GetDimensions */)";

    NkString result = NkSLCodeGenHLSL::IntrinsicToHLSL_Static(funcName) + "(";
    for (uint32 i = 0; i < (uint32)args.Size(); i++) {
        if (i > 0) result += ", ";
        result += args[i];
    }
    return result + ")";
}

// =============================================================================
// IntrinsicToHLSL_Static
// =============================================================================
NkString NkSLCodeGenHLSL::IntrinsicToHLSL_Static(const NkString& name) {
    if (name == "mix")         return "lerp";
    if (name == "fract")       return "frac";
    if (name == "mod")         return "fmod";
    if (name == "inversesqrt") return "rsqrt";
    if (name == "dFdx")        return "ddx";
    if (name == "dFdy")        return "ddy";
    if (name == "discard")     return "discard";
    return name;
}

// =============================================================================
// GenCall HLSL — override pour les fonctions de texture avancées
// =============================================================================
NkString NkSLCodeGenHLSL::GenCall(NkSLCallNode* call) {
    NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    NkVector<NkString> strArgs;
    for (auto* a : call->args) strArgs.PushBack(GenExpr(a));

    if (name == "texture"        || name == "textureLod"      ||
        name == "textureLodOffset"|| name == "textureOffset"  ||
        name == "texelFetch"     || name == "textureGather"   ||
        name == "textureGrad"    || name == "textureSize"     ||
        name == "imageLoad"      || name == "imageStore"      ||
        name.StartsWith("imageAtomic")) {
        return TranslateTextureCallHLSL(name, strArgs);
    }

    if (name == "atomicAdd" && strArgs.Size() >= 2)
        return "InterlockedAdd(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicMin" && strArgs.Size() >= 2)
        return "InterlockedMin(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicMax" && strArgs.Size() >= 2)
        return "InterlockedMax(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicAnd" && strArgs.Size() >= 2)
        return "InterlockedAnd(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicOr"  && strArgs.Size() >= 2)
        return "InterlockedOr(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicXor" && strArgs.Size() >= 2)
        return "InterlockedXor(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicExchange" && strArgs.Size() >= 2)
        return "InterlockedExchange(" + strArgs[0] + ", " + strArgs[1] + ")";
    if (name == "atomicCompSwap" && strArgs.Size() >= 3)
        return "InterlockedCompareExchange(" + strArgs[0] + ", " + strArgs[1] + ", " + strArgs[2] + ")";

    if (name == "vec2" || name == "vec3" || name == "vec4") {
        NkString hlslType = "float" + name.SubStr(3);
        NkString args;
        for (uint32 i = 0; i < (uint32)strArgs.Size(); i++) {
            if (i > 0) args += ", "; args += strArgs[i];
        }
        return hlslType + "(" + args + ")";
    }
    if (name == "ivec2" || name == "ivec3" || name == "ivec4") {
        NkString hlslType = "int" + name.SubStr(4);
        NkString args;
        for (uint32 i = 0; i < (uint32)strArgs.Size(); i++) {
            if (i > 0) args += ", "; args += strArgs[i];
        }
        return hlslType + "(" + args + ")";
    }
    if (name == "uvec2" || name == "uvec3" || name == "uvec4") {
        NkString hlslType = "uint" + name.SubStr(4);
        NkString args;
        for (uint32 i = 0; i < (uint32)strArgs.Size(); i++) {
            if (i > 0) args += ", "; args += strArgs[i];
        }
        return hlslType + "(" + args + ")";
    }
    if (name == "mat4") { name = "float4x4"; }
    if (name == "mat3") { name = "float3x3"; }
    if (name == "mat2") { name = "float2x2"; }

    name = IntrinsicToHLSL_Static(name);
    NkString args;
    for (uint32 i = 0; i < (uint32)strArgs.Size(); i++) {
        if (i > 0) args += ", "; args += strArgs[i];
    }
    return name + "(" + args + ")";
}

// =============================================================================
// Texture calls MSL
// =============================================================================
static NkString TranslateTextureCallMSL(const NkString& funcName,
                                          const NkVector<NkString>& args) {
    if (args.Empty()) return funcName + "()";
    NkString tex = args[0] + "_tex";
    NkString smp = args[0] + "_smp";

    if (funcName == "texture" && args.Size() >= 2)
        return tex + ".sample(" + smp + ", " + args[1] + ")";
    if (funcName == "textureLod" && args.Size() >= 3)
        return tex + ".sample(" + smp + ", " + args[1] + ", level(" + args[2] + "))";
    if (funcName == "textureLodOffset" && args.Size() >= 4)
        return tex + ".sample(" + smp + ", " + args[1] + ", level(" + args[2] + "))";
    if (funcName == "textureOffset" && args.Size() >= 3)
        return tex + ".sample(" + smp + ", " + args[1] + ")";
    if (funcName == "texelFetch" && args.Size() >= 3)
        return tex + ".read(uint2(" + args[1] + "), " + args[2] + ")";
    if (funcName == "textureGather" && args.Size() >= 2)
        return tex + ".gather(" + smp + ", " + args[1] + ")";
    if (funcName == "textureGrad" && args.Size() >= 4)
        return tex + ".sample(" + smp + ", " + args[1] + ", gradient2d(" +
               args[2] + ", " + args[3] + "))";
    if (funcName == "textureSize")
        return "int2(" + tex + ".get_width(), " + tex + ".get_height())";
    if (funcName == "imageLoad" && args.Size() >= 2)
        return args[0] + "_tex.read(uint2(" + args[1] + "))";
    if (funcName == "imageStore" && args.Size() >= 3)
        return args[0] + "_tex.write(" + args[2] + ", uint2(" + args[1] + "))";
    if (funcName == "imageAtomicAdd"      && args.Size() >= 3)
        return "atomic_fetch_add_explicit((volatile device atomic_int*)&" +
               args[0] + "[" + args[1] + "], " + args[2] + ", memory_order_relaxed)";
    if (funcName == "imageAtomicMin"      && args.Size() >= 3)
        return "atomic_fetch_min_explicit((volatile device atomic_int*)&" +
               args[0] + "[" + args[1] + "], " + args[2] + ", memory_order_relaxed)";
    if (funcName == "imageAtomicMax"      && args.Size() >= 3)
        return "atomic_fetch_max_explicit((volatile device atomic_int*)&" +
               args[0] + "[" + args[1] + "], " + args[2] + ", memory_order_relaxed)";
    if (funcName == "imageAtomicExchange" && args.Size() >= 3)
        return "atomic_exchange_explicit((volatile device atomic_int*)&" +
               args[0] + "[" + args[1] + "], " + args[2] + ", memory_order_relaxed)";
    if (funcName == "barrier" || funcName == "memoryBarrier" || funcName == "groupMemoryBarrier")
        return "threadgroup_barrier(mem_flags::mem_threadgroup)";
    if (funcName == "imageSize" && args.Size() >= 1)
        return "int2(" + args[0] + "_tex.get_width(), " + args[0] + "_tex.get_height())";

    NkString result = funcName + "(";
    for (uint32 i = 0; i < (uint32)args.Size(); i++) {
        if (i > 0) result += ", "; result += args[i];
    }
    return result + ")";
}

// =============================================================================
// GenCall MSL — override pour les fonctions avancées
// =============================================================================
NkString NkSLCodeGen_MSL::GenCall(NkSLCallNode* call) {
    NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    NkVector<NkString> strArgs;
    for (auto* a : call->args) strArgs.PushBack(GenExpr(a));

    if (name == "texture"        || name == "textureLod"     ||
        name == "textureLodOffset"|| name == "textureOffset" ||
        name == "texelFetch"     || name == "textureGather"  ||
        name == "textureGrad"    || name == "textureSize"    ||
        name == "imageLoad"      || name == "imageStore"     ||
        name.StartsWith("imageAtomic")) {
        return TranslateTextureCallMSL(name, strArgs);
    }

    if (name == "atomicAdd" && strArgs.Size() >= 2)
        return "atomic_fetch_add_explicit(&" + strArgs[0] + ", " + strArgs[1] + ", memory_order_relaxed)";

    name = IntrinsicToMSL(name);
    NkString args;
    for (uint32 i = 0; i < (uint32)strArgs.Size(); i++) {
        if (i > 0) args += ", "; args += strArgs[i];
    }
    return name + "(" + args + ")";
}

// =============================================================================
// Geometry shader HLSL
// =============================================================================
void NkSLCodeGenHLSL::GenGeometryShaderPrefix(NkSLStage stage) {
    if (stage != NkSLStage::NK_GEOMETRY) return;
    EmitLine("[maxvertexcount(32)]");
}

void NkSLCodeGenHLSL::GenTessellationShaders(NkSLProgramNode* prog) {
    if (mStage == NkSLStage::NK_TESS_CONTROL) {
        EmitLine("// Hull shader (tessellation control)");
        EmitLine("struct HS_CONSTANT_DATA { float edges[4]:SV_TessFactor; float inside[2]:SV_InsideTessFactor; };");
        EmitLine("[domain(\"quad\")]");
        EmitLine("[partitioning(\"fractional_even\")]");
        EmitLine("[outputtopology(\"triangle_cw\")]");
        EmitLine("[outputcontrolpoints(4)]");
        EmitLine("[patchconstantfunc(\"PatchConstantFunc\")]");
        EmitNewLine();
        EmitLine("HS_CONSTANT_DATA PatchConstantFunc(InputPatch<VS_OUT, 4> patch, uint patchID : SV_PrimitiveID) {");
        IndentPush();
        EmitLine("HS_CONSTANT_DATA output;");
        EmitLine("output.edges[0] = output.edges[1] = output.edges[2] = output.edges[3] = 1.0;");
        EmitLine("output.inside[0] = output.inside[1] = 1.0;");
        EmitLine("return output;");
        IndentPop();
        EmitLine("}");
        EmitNewLine();
    }
    if (mStage == NkSLStage::NK_TESS_EVAL) {
        EmitLine("// Domain shader (tessellation evaluation)");
        EmitLine("[domain(\"quad\")]");
    }
}

// =============================================================================
// StructuredBuffer HLSL
// =============================================================================
void NkSLCodeGenHLSL::GenStructuredBuffer(NkSLBlockDeclNode* b) {
    int reg = b->binding.HasBinding() ? b->binding.binding : 0;
    bool isReadWrite = (b->storage == NkSLStorageQual::NK_BUFFER);
    char buf[256];
    if (isReadWrite) {
        snprintf(buf, sizeof(buf), "RWStructuredBuffer<%s> %s : register(u%d);",
                 b->blockName.CStr(),
                 b->instanceName.Empty() ? b->blockName.ToLower().CStr() : b->instanceName.CStr(),
                 reg);
    } else {
        snprintf(buf, sizeof(buf), "StructuredBuffer<%s> %s : register(t%d);",
                 b->blockName.CStr(),
                 b->instanceName.Empty() ? b->blockName.ToLower().CStr() : b->instanceName.CStr(),
                 reg);
    }
    EmitLine(NkString(buf));
}

// =============================================================================
// Bindless HLSL
// =============================================================================
void NkSLCodeGenHLSL::GenBindlessHeader() {
    EmitLine("// Bindless resource access (SM 6.6+)");
    EmitLine("// Usage: Texture2D tex = ResourceDescriptorHeap[texIndex];");
    EmitLine("// Usage: SamplerState smp = SamplerDescriptorHeap[smpIndex];");
    EmitNewLine();
}

// =============================================================================
// CompileWithSemantic (dans NkSLCompiler)
// =============================================================================
NkSLCompileResult NkSLCompiler::CompileWithSemantic(
    const NkString&          source,
    NkSLStage                stage,
    NkSLTarget               target,
    const NkSLCompileOptions& opts,
    const NkString&          filename)
{
    NkVector<NkSLCompileError> ppErrors;
    NkString processed = Preprocess(source, "", &ppErrors);
    if (!ppErrors.Empty()) {
        NkSLCompileResult res; res.success=false; res.errors=ppErrors; return res;
    }

    NkSLLexer  lexer(processed, filename);
    NkSLParser parser(lexer);
    NkSLProgramNode* ast = parser.Parse();

    if (parser.HasErrors()) {
        NkSLCompileResult res; res.success=false; res.errors=parser.GetErrors();
        delete ast; return res;
    }

    NkSLSemantic semantic(stage);
    NkSLSemanticResult semResult = semantic.Analyze(ast);

    if (!semResult.success) {
        NkSLCompileResult res;
        res.success  = false;
        res.errors   = semResult.errors;
        res.warnings = semResult.warnings;
        delete ast;
        return res;
    }

    for (auto& w : semResult.warnings)
        logger.Infof("%s:%u [warning]: %s\n", filename.CStr(), w.line, w.message.CStr());

    NkSLCompileResult res;
    switch (target) {
        case NkSLTarget::NK_GLSL: {
            NkSLCodeGenGLSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        case NkSLTarget::NK_SPIRV: {
            NkSLCodeGenGLSL glslGen;
            NkSLCompileOptions glslOpts = opts; glslOpts.glslVersion = 450;
            NkSLCompileResult glslRes = glslGen.Generate(ast, stage, glslOpts);
            if (!glslRes.success) { res = glslRes; break; }
            res = CompileToSPIRV(glslRes.source, stage, opts);
            if (!res.success) { res = glslRes; res.target = NkSLTarget::NK_GLSL; }
            break;
        }
        case NkSLTarget::NK_HLSL: {
            NkSLCodeGenHLSL gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        case NkSLTarget::NK_MSL:
        case NkSLTarget::NK_MSL_SPIRV_CROSS: {
            if (opts.preferSpirvCrossForMSL) {
                NkSLCodeGenMSLSpirvCross gen;
                res = gen.Generate(ast, stage, opts);
                if (!res.success) { NkSLCodeGen_MSL ng; res = ng.Generate(ast, stage, opts); }
            } else {
                NkSLCodeGen_MSL gen; res = gen.Generate(ast, stage, opts);
            }
            break;
        }
        case NkSLTarget::NK_CPLUSPLUS: {
            NkSLCodeGenCPP gen;
            res = gen.Generate(ast, stage, opts);
            break;
        }
        default:
            res.success = false;
            res.AddError(0, "Unsupported target");
            break;
    }

    delete ast;
    for (auto& w : semResult.warnings) res.warnings.PushBack(w);

    if (res.success && mCacheEnabled) {
        uint64 h = HashSource(source, stage, target, opts);
        mCache.Put(h, target, stage, res);
    }

    return res;
}

} // namespace nkentseu
