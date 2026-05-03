// =============================================================================
// NkSLCodeGenHLSL_Structs.cpp  — v3.0
//
// CORRECTIONS BUG CRITIQUE:
//   - sAutoIdx static → paramètre autoIndex passé par valeur (thread-safe)
//   - sSemanticIdx static → même correction
//   - Le compteur est maintenant local à chaque appel GenInputOutputStructs()
//     ce qui garantit un résultat déterministe peu importe l'ordre de compilation
// =============================================================================
#include "NkSLCodeGen.h"
#include "NkSLSemantic.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Table de mapping sémantique HLSL
// =============================================================================
struct HLSLSemanticRule {
    const char* nameLower;
    bool        isPrefix;
    const char* inputSem;
    const char* outputSem;
    const char* fragOutSem;
};

static const HLSLSemanticRule kSemanticRules[] = {
    {"position",   false, "POSITION",      "SV_Position",  "SV_Position"},
    {"pos",        false, "POSITION",      "SV_Position",  "SV_Position"},
    {"apos",       false, "POSITION",      "SV_Position",  "SV_Position"},
    {"vpos",       false, "SV_Position",   "SV_Position",  "SV_Position"},
    {"normal",     false, "NORMAL",        "NORMAL",       "NORMAL"},
    {"anormal",    false, "NORMAL",        "NORMAL",       "NORMAL"},
    {"vnormal",    false, "NORMAL",        "NORMAL",       "NORMAL"},
    {"nrm",        false, "NORMAL",        "NORMAL",       "NORMAL"},
    {"tangent",    false, "TANGENT",       "TANGENT",      "TANGENT"},
    {"atangent",   false, "TANGENT",       "TANGENT",      "TANGENT"},
    {"bitangent",  false, "BINORMAL",      "BINORMAL",     "BINORMAL"},
    {"color",      false, "COLOR",         "COLOR",        "SV_Target0"},
    {"colour",     false, "COLOR",         "COLOR",        "SV_Target0"},
    {"col",        true,  "COLOR",         "COLOR",        "SV_Target0"},
    {"fragcolor",  true,  "COLOR",         "COLOR",        "SV_Target0"},
    {"outcolor",   true,  "COLOR",         "COLOR",        "SV_Target0"},
    {"uv",         true,  "TEXCOORD0",     "TEXCOORD0",    "TEXCOORD0"},
    {"texcoord",   true,  "TEXCOORD0",     "TEXCOORD0",    "TEXCOORD0"},
    {"tex",        true,  "TEXCOORD0",     "TEXCOORD0",    "TEXCOORD0"},
    {"fragdepth",  false, "DEPTH",         "DEPTH",        "SV_Depth"},
    {"depth",      true,  "DEPTH",         "DEPTH",        "SV_Depth"},
    {"instanceid", false, "SV_InstanceID", "SV_InstanceID",""},
    {"vertexid",   false, "SV_VertexID",   "SV_VertexID",  ""},
    {nullptr, false, nullptr, nullptr, nullptr}
};

// =============================================================================
// GetHLSLSemantic — CORRECTION: autoIndex en paramètre, plus de static local
// =============================================================================
NkString GetHLSLSemantic(NkSLVarDeclNode* v,
                          NkSLStage stage,
                          bool isVertexInput,
                          bool isFragmentOut,
                          int autoIndex)   // ← paramètre, PAS de static
{
    NkString name = v->name.ToLower();

    // Règle 1 : @location explicite
    if (v->binding.HasLocation()) {
        int loc = v->binding.location;
        char buf[64];

        for (int i = 0; kSemanticRules[i].nameLower; i++) {
            auto& r = kSemanticRules[i];
            bool match = r.isPrefix
                ? name.StartsWith(r.nameLower)
                : name == r.nameLower;
            if (match) {
                if (isFragmentOut && r.fragOutSem && r.fragOutSem[0]) {
                    if (name.StartsWith("fragcolor") || name.StartsWith("outcolor")) {
                        for (int j = 0; j < 4; j++) {
                            char numBuf[4]; snprintf(numBuf, sizeof(numBuf), "%d", j);
                            if (name.EndsWith(NkString(numBuf).View())) {
                                snprintf(buf, sizeof(buf), "SV_Target%d", j);
                                return NkString(buf);
                            }
                        }
                        snprintf(buf, sizeof(buf), "SV_Target%d", loc);
                        return NkString(buf);
                    }
                    return r.fragOutSem;
                }
                if (isVertexInput && r.inputSem && r.inputSem[0])
                    return r.inputSem;
                if (!isVertexInput && !isFragmentOut && r.outputSem && r.outputSem[0]) {
                    snprintf(buf, sizeof(buf), "TEXCOORD%d", loc);
                    return NkString(buf);
                }
            }
        }

        if (isFragmentOut) {
            snprintf(buf, sizeof(buf), "SV_Target%d", loc);
            return NkString(buf);
        }
        snprintf(buf, sizeof(buf), "TEXCOORD%d", loc);
        return NkString(buf);
    }

    // Règle 2 : heuristique par nom
    for (int i = 0; kSemanticRules[i].nameLower; i++) {
        auto& r = kSemanticRules[i];
        bool match = r.isPrefix
            ? name.StartsWith(r.nameLower)
            : name == r.nameLower;
        if (match) {
            if (isFragmentOut && r.fragOutSem && r.fragOutSem[0]) return r.fragOutSem;
            if (isVertexInput && r.inputSem   && r.inputSem[0])   return r.inputSem;
            if (r.outputSem && r.outputSem[0])                     return r.outputSem;
        }
    }

    // Règle 3 : auto-index — utilise le paramètre, PAS un static
    char buf[32];
    if (isFragmentOut) {
        snprintf(buf, sizeof(buf), "SV_Target%d", autoIndex);
    } else {
        snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
    }
    return NkString(buf);
}

// =============================================================================
// GenInputOutputStructs HLSL
// CORRECTION: les compteurs autoIdx sont maintenant des variables locales
// réinitialisées à chaque appel — plus de static, thread-safe et déterministe
// =============================================================================
void NkSLCodeGenHLSL::GenInputOutputStructs() {
    // ── Struct d'entrée du vertex ─────────────────────────────────────────────
    if (mStage == NkSLStage::NK_VERTEX && !mInputVars.Empty()) {
        EmitLine("struct VS_Input");
        EmitLine("{");
        IndentPush();
        int autoIdx = 0;  // ← variable locale, PAS static
        for (auto* v : mInputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, true, false, autoIdx++);
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        EmitLine("uint _VertexID   : SV_VertexID;");
        EmitLine("uint _InstanceID : SV_InstanceID;");
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mInputStructName = "VS_Input";
    }

    // ── Struct de sortie du vertex ────────────────────────────────────────────
    if (mStage == NkSLStage::NK_VERTEX) {
        EmitLine("struct VS_Output");
        EmitLine("{");
        IndentPush();
        EmitLine("float4 _Position : SV_Position;");
        int autoIdx = 0;  // ← variable locale, PAS static
        for (auto* v : mOutputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, false, false, autoIdx++);
            if (sem == "SV_Position") continue;
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mOutputStructName = "VS_Output";
    }

    // ── Struct d'entrée du fragment ───────────────────────────────────────────
    if (mStage == NkSLStage::NK_FRAGMENT && !mInputVars.Empty()) {
        EmitLine("struct PS_Input");
        EmitLine("{");
        IndentPush();
        EmitLine("float4 _Position : SV_Position;");
        int autoIdx = 0;  // ← variable locale, PAS static
        for (auto* v : mInputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, false, false, autoIdx++);
            if (sem == "SV_Position") continue;
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mInputStructName = "PS_Input";
    }

    // ── Struct de sortie du fragment ──────────────────────────────────────────
    if (mStage == NkSLStage::NK_FRAGMENT) {
        // Déterminer si le shader a des sorties couleur réelles
        bool hasColorOutputs = false;
        for (auto* v : mOutputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, false, true, 0);
            if (sem != "SV_Depth" && sem != "DEPTH")
                hasColorOutputs = true;
        }

        // Passes depth-only (aucune sortie couleur, pas d'écriture explicite de depth)
        // → pas de struct PS_Output, retour void
        if (!hasColorOutputs && !mWritesDepth) {
            mOutputStructName = "";
        } else {
            EmitLine("struct PS_Output");
            EmitLine("{");
            IndentPush();
            int autoIdx = 0;  // ← variable locale, PAS static
            for (auto* v : mOutputVars) {
                NkString sem = GetHLSLSemantic(v, mStage, false, true, autoIdx++);
                EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
            }
            // Fallback couleur uniquement si aucune sortie déclarée et pas depth-only
            if (mOutputVars.Empty() && !mWritesDepth) {
                EmitLine("float4 _Color : SV_Target0;");
            }
            // SV_Depth uniquement si gl_FragDepth est explicitement écrit dans le shader
            if (mWritesDepth) {
                EmitLine("float _Depth : SV_Depth;");
            }
            IndentPop();
            EmitLine("};");
            EmitNewLine();
            mOutputStructName = "PS_Output";
        }
    }

    // ── Compute : pas de struct ───────────────────────────────────────────────
    if (mStage == NkSLStage::NK_COMPUTE) {
        mInputStructName  = "";
        mOutputStructName = "";
    }
}

// =============================================================================
// SemanticFor — CORRECTION: autoIndex en paramètre (plus de static local)
// =============================================================================
NkString NkSLCodeGenHLSL::SemanticFor(NkSLVarDeclNode* v,
                                         NkSLStage stage,
                                         bool isInput,
                                         bool isFragOut,
                                         int autoIndex)
{
    (void)isFragOut;
    if (v->binding.HasLocation()) {
        char buf[32];
        NkString name = v->name.ToLower();
        if (name == "position" || name == "pos" || name.StartsWith("apos") ||
            name.StartsWith("vpos")) {
            if (!isInput) return "SV_Position";
            return "POSITION";
        }
        snprintf(buf, sizeof(buf), "TEXCOORD%d", v->binding.location);
        return NkString(buf);
    }

    NkString n = v->name.ToLower();
    if (n == "position" || n == "pos" || n == "apos" || n == "vpos" || n == "fragcoord")
        return isInput ? "POSITION" : "SV_Position";
    if (n == "normal"   || n == "anormal" || n == "vnormal") return "NORMAL";
    if (n == "tangent"  || n == "atangent") return "TANGENT";
    if (n == "bitangent"|| n == "abitangent") return "BINORMAL";
    if (n == "color"    || n == "acolor" || n == "vcolor" || n.StartsWith("col"))
        return "COLOR";
    if (n == "uv"       || n == "texcoord" || n == "vuv" || n.StartsWith("uv"))
        return "TEXCOORD0";
    if (n == "uv1"      || n == "texcoord1") return "TEXCOORD1";
    if (n == "uv2"      || n == "texcoord2") return "TEXCOORD2";
    if (n == "fragcolor"|| n == "outcolor" || n == "fragcolor0")
        return stage == NkSLStage::NK_FRAGMENT ? "SV_Target0" : "COLOR";
    if (n == "fragcolor1") return "SV_Target1";
    if (n == "fragcolor2") return "SV_Target2";
    if (n == "fragcolor3") return "SV_Target3";
    if (n == "fragdepth" || n == "depth")
        return stage == NkSLStage::NK_FRAGMENT ? "SV_Depth" : "DEPTH";
    if (n == "instanceid") return "SV_InstanceID";
    if (n == "vertexid")   return "SV_VertexID";

    // Auto-index depuis le paramètre, PAS un static
    char buf[32];
    snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
    return NkString(buf);
}

// =============================================================================
// MSL GenInputOutputStructs
// CORRECTION: autoLoc est une variable locale, PAS un static
// =============================================================================
void NkSLCodeGen_MSL::GenInputOutputStructs() {
    NkString inStructName  = (mStage==NkSLStage::NK_VERTEX) ? "VertexIn"  : "FragIn";
    NkString outStructName = (mStage==NkSLStage::NK_VERTEX) ? "VertexOut" : "FragOut";

    // ── Struct d'entrée ───────────────────────────────────────────────────────
    if (!mInputVars.Empty()) {
        EmitLine("struct " + inStructName);
        EmitLine("{");
        IndentPush();
        if (mStage == NkSLStage::NK_VERTEX) {
            int autoLoc = 0;  // ← variable locale, PAS static
            for (auto* v : mInputVars) {
                int loc = v->binding.HasLocation() ? v->binding.location : autoLoc;
                autoLoc = loc + 1;
                char buf[64]; snprintf(buf, sizeof(buf), " [[attribute(%d)]]", loc);
                EmitLine(TypeToMSL(v->type) + " " + v->name + NkString(buf) + ";");
            }
            EmitLine("uint _vertex_id   [[vertex_id]];");
            EmitLine("uint _instance_id [[instance_id]];");
        } else if (mStage == NkSLStage::NK_FRAGMENT) {
            EmitLine("float4 _position [[position]];");
            for (auto* v : mInputVars) {
                EmitLine(TypeToMSL(v->type) + " " + v->name + ";");
            }
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
    }

    // ── Struct de sortie ─────────────────────────────────────────────────────
    {
        EmitLine("struct " + outStructName);
        EmitLine("{");
        IndentPush();
        if (mStage == NkSLStage::NK_VERTEX) {
            EmitLine("float4 _position [[position]];");
            for (auto* v : mOutputVars) {
                EmitLine(TypeToMSL(v->type) + " " + v->name + ";");
            }
        } else if (mStage == NkSLStage::NK_FRAGMENT) {
            int colorIdx = 0;  // ← variable locale, PAS static
            for (auto* v : mOutputVars) {
                char buf[64]; snprintf(buf, sizeof(buf), " [[color(%d)]]", colorIdx++);
                EmitLine(TypeToMSL(v->type) + " " + v->name + NkString(buf) + ";");
            }
            if (mOutputVars.Empty() && !mWritesDepth) {
                // Passes depth-only : pas de sortie couleur ni depth
                // (la struct reste vide — à terme on pourrait omettre le struct entier)
            } else if (mOutputVars.Empty()) {
                EmitLine("float4 _color [[color(0)]];");
            }
            // Depth uniquement si gl_FragDepth est explicitement écrit
            if (mWritesDepth) {
                EmitLine("float _depth [[depth(any)]];");
            }
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
    }
}

// =============================================================================
// BuildEntryPointSignature MSL — inchangé fonctionnellement
// =============================================================================
NkString NkSLCodeGen_MSL::BuildEntryPointSignature(NkSLFunctionDeclNode* fn) {
    // Passe depth-only : fragment shader sans sortie couleur ni gl_FragDepth → void
    bool isDepthOnly = (mStage == NkSLStage::NK_FRAGMENT) && mOutputVars.Empty() && !mWritesDepth;
    NkString inStructName  = (mStage==NkSLStage::NK_VERTEX)   ? "VertexIn"  :
                             (mStage==NkSLStage::NK_FRAGMENT)  ? "FragIn"    : "";
    NkString outStructName = (mStage==NkSLStage::NK_VERTEX)   ? "VertexOut" :
                             (mStage==NkSLStage::NK_FRAGMENT && !isDepthOnly) ? "FragOut" : "void";

    NkString qualifier;
    if      (mStage == NkSLStage::NK_VERTEX)   qualifier = "vertex ";
    else if (mStage == NkSLStage::NK_FRAGMENT)  qualifier = "fragment ";
    else if (mStage == NkSLStage::NK_COMPUTE)   qualifier = "kernel ";

    NkString retType = (mStage == NkSLStage::NK_COMPUTE) ? "void" : outStructName;
    NkString sig     = qualifier + retType + " " + fn->name + "_entry(";

    bool first = true;

    if (!inStructName.Empty()) {
        sig += inStructName + " _in [[stage_in]]";
        first = false;
    }

    int bufIdx = 0;
    for (auto* b : mBufferDecls) {
        if (!first) sig += ", ";
        NkString addrSpace = AddressSpaceFor(b->storage);
        int bidx = b->binding.HasBinding() ? b->binding.binding : bufIdx++;
        char buf[256];
        NkString instName = b->instanceName.Empty()
            ? b->blockName.ToLower()
            : b->instanceName;
        snprintf(buf, sizeof(buf), "%s %s& %s [[buffer(%d)]]",
                 addrSpace.CStr(), b->blockName.CStr(), instName.CStr(), bidx);
        sig += NkString(buf);
        first = false;
    }

    int texIdx = 0, sampIdx = 0;
    for (auto* v : mUniformVars) {
        if (!v->type) continue;
        if (NkSLTypeIsSampler(v->type->baseType)) {
            if (!first) sig += ", ";
            int bidx = v->binding.HasBinding() ? v->binding.binding : texIdx;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s_tex [[texture(%d)]]",
                     BaseTypeToMSL(v->type->baseType).CStr(), v->name.CStr(), bidx);
            sig += NkString(buf);
            first = false;
            sig += ", ";
            snprintf(buf, sizeof(buf), "sampler %s_smp [[sampler(%d)]]",
                     v->name.CStr(), sampIdx);
            sig += NkString(buf);
            texIdx++; sampIdx++;
        } else if (NkSLTypeIsImage(v->type->baseType)) {
            if (!first) sig += ", ";
            int bidx = v->binding.HasBinding() ? v->binding.binding : texIdx++;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s_tex [[texture(%d)]]",
                     BaseTypeToMSL(v->type->baseType).CStr(), v->name.CStr(), bidx);
            sig += NkString(buf);
            first = false;
        }
    }

    if (mStage == NkSLStage::NK_COMPUTE) {
        if (!first) sig += ", ";
        sig += "uint3 _thread_pos_in_grid     [[thread_position_in_grid]], ";
        sig += "uint3 _thread_pos_in_group    [[thread_position_in_threadgroup]], ";
        sig += "uint3 _threadgroup_pos_in_grid[[threadgroup_position_in_grid]]";
    }

    sig += ")";
    return sig;
}

} // namespace nkentseu
