// =============================================================================
// NkSLCodeGen_HLSL_Structs.cpp
// Génération déterministe des structs d'entrée/sortie pour HLSL et MSL.
//
// Règle fondamentale :
//   La sémantique d'une variable in/out est TOUJOURS dérivée de sa @location().
//   Deux variables avec la même @location et le même nom sont GARANTIES cohérentes
//   entre vertex et fragment — le backend ne devine jamais.
//
// Pour HLSL :
//   @location(0) in vec3 aPos   → float3 aPos : POSITION  (vertex input)
//                                 float3 aPos : TEXCOORD0  (inter-stage)
//   @location(0) out vec4 col   → float4 col  : SV_Target0 (fragment output)
//
// Pour MSL :
//   @location(0) in vec3 aPos   → float3 aPos [[attribute(0)]]  (vertex input)
//   @location(0) out vec3 vPos  → float3 vPos                   (inter-stage, pas d'attr)
//   [[position]] toujours sur le vertex position output
// =============================================================================
#include "NkSLCodeGen.h"
#include "NkSLSemantic.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// HLSL : génération des structs d'entrée/sortie avec sémantiques déterministes
// =============================================================================

// Table de mapping location → sémantique pour les vertex inputs standards
// Priorité absolue à @location explicite → TEXCOORD<N>
// L'heuristique par nom n'est utilisée QUE si aucune @location n'est spécifiée
// ET que le nom correspond à un pattern connu
struct HLSLSemanticRule {
    const char* nameLower;   // pattern de nom (lowercase, correspondance exacte ou préfixe)
    bool        isPrefix;    // true = correspondance de préfixe
    const char* inputSem;    // sémantique en entrée (vertex shader)
    const char* outputSem;   // sémantique en sortie (entre stages)
    const char* fragOutSem;  // sémantique de sortie fragment
};

static const HLSLSemanticRule kSemanticRules[] = {
    // Positions
    {"position",   false, "POSITION",   "SV_Position", "SV_Position"},
    {"pos",        false, "POSITION",   "SV_Position", "SV_Position"},
    {"apos",       false, "POSITION",   "SV_Position", "SV_Position"},
    {"vpos",       false, "SV_Position","SV_Position", "SV_Position"},
    // Normales
    {"normal",     false, "NORMAL",     "NORMAL",      "NORMAL"},
    {"anormal",    false, "NORMAL",     "NORMAL",      "NORMAL"},
    {"vnormal",    false, "NORMAL",     "NORMAL",      "NORMAL"},
    {"nrm",        false, "NORMAL",     "NORMAL",      "NORMAL"},
    // Tangentes
    {"tangent",    false, "TANGENT",    "TANGENT",     "TANGENT"},
    {"atangent",   false, "TANGENT",    "TANGENT",     "TANGENT"},
    {"bitangent",  false, "BINORMAL",   "BINORMAL",    "BINORMAL"},
    // Couleurs
    {"color",      false, "COLOR",      "COLOR",       "SV_Target0"},
    {"colour",     false, "COLOR",      "COLOR",       "SV_Target0"},
    {"col",        true,  "COLOR",      "COLOR",       "SV_Target0"},
    {"fragcolor",  true,  "COLOR",      "COLOR",       "SV_Target0"},
    {"outcolor",   true,  "COLOR",      "COLOR",       "SV_Target0"},
    // UV / texcoords
    {"uv",         true,  "TEXCOORD0",  "TEXCOORD0",   "TEXCOORD0"},
    {"texcoord",   true,  "TEXCOORD0",  "TEXCOORD0",   "TEXCOORD0"},
    {"tex",        true,  "TEXCOORD0",  "TEXCOORD0",   "TEXCOORD0"},
    // Profondeur
    {"fragdepth",  false, "DEPTH",      "DEPTH",       "SV_Depth"},
    {"depth",      true,  "DEPTH",      "DEPTH",       "SV_Depth"},
    // Instance ID
    {"instanceid", false, "SV_InstanceID", "SV_InstanceID", ""},
    {"vertexid",   false, "SV_VertexID", "SV_VertexID", ""},
    {nullptr, false, nullptr, nullptr, nullptr}
};

// Retourne la sémantique HLSL pour une variable selon son contexte
NkString GetHLSLSemantic(NkSLVarDeclNode* v,
                          NkSLStage stage,
                          bool isVertexInput,  // true = entrée du vertex shader
                          bool isFragmentOut,  // true = sortie du fragment shader
                          int autoIndex)       // index auto si pas de @location
{
    NkString name = v->name.ToLower();

    // Règle 1 : @location explicite → TEXCOORD<N> (sauf position et fragment out)
    if (v->binding.HasLocation()) {
        int loc = v->binding.location;
        char buf[64];

        // Exception : si le nom matche "position" avec @location, on garde SV_Position
        for (int i = 0; kSemanticRules[i].nameLower; i++) {
            auto& r = kSemanticRules[i];
            bool match = r.isPrefix
                ? name.StartsWith(r.nameLower)
                : name == r.nameLower;
            if (match) {
                if (isFragmentOut && r.fragOutSem && r.fragOutSem[0]) {
                    // Fragment output avec index si multiple
                    if (name.StartsWith("fragcolor") || name.StartsWith("outcolor")) {
                        // fragcolor0..3
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
                if (!isVertexInput && !isFragmentOut)
                    return r.outputSem && r.outputSem[0] ? r.outputSem
                           : (snprintf(buf,sizeof(buf),"TEXCOORD%d",loc), NkString(buf));
            }
        }

        // Cas général
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

    // Règle 3 : auto-index
    char buf[32];
    if (isFragmentOut) {
        snprintf(buf, sizeof(buf), "SV_Target%d", autoIndex);
    } else {
        snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
    }
    return NkString(buf);
}

// =============================================================================
// Génération des structs HLSL avec sémantiques déterministes
// Remplace la méthode GenProgram partielle dans NkSLCodeGen_HLSL
// =============================================================================
void NkSLCodeGen_HLSL::GenInputOutputStructs() {
    // ── Struct d'entrée du vertex shader ──────────────────────────────────────
    if (mStage == NkSLStage::VERTEX && !mInputVars.Empty()) {
        EmitLine("struct VS_Input");
        EmitLine("{");
        IndentPush();
        int autoIdx = 0;
        for (auto* v : mInputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, true, false, autoIdx++);
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        EmitLine("uint _VertexID : SV_VertexID;");
        EmitLine("uint _InstanceID : SV_InstanceID;");
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mInputStructName = "VS_Input";
    }

    // ── Struct de sortie du vertex (= entrée du fragment) ─────────────────────
    if (mStage == NkSLStage::VERTEX) {
        EmitLine("struct VS_Output");
        EmitLine("{");
        IndentPush();
        // Position en premier, toujours
        bool hasPosition = false;
        for (auto* v : mOutputVars) {
            NkString n = v->name.ToLower();
            if (n == "position" || n == "pos" || n == "vpos" ||
                n == "fragcoord" || n == "worldpos") {
                // Position builtin : vérifier si c'est gl_Position ou un varying
            }
        }
        EmitLine("float4 _Position : SV_Position;");
        int autoIdx = 0;
        for (auto* v : mOutputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, false, false, autoIdx++);
            // Ne pas dupliquer SV_Position
            if (sem == "SV_Position") continue;
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mOutputStructName = "VS_Output";
    }

    // ── Struct d'entrée du fragment (= sortie du vertex) ──────────────────────
    if (mStage == NkSLStage::FRAGMENT && !mInputVars.Empty()) {
        EmitLine("struct PS_Input");
        EmitLine("{");
        IndentPush();
        EmitLine("float4 _Position : SV_Position;");
        int autoIdx = 0;
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
    if (mStage == NkSLStage::FRAGMENT) {
        EmitLine("struct PS_Output");
        EmitLine("{");
        IndentPush();
        int autoIdx = 0;
        for (auto* v : mOutputVars) {
            NkString sem = GetHLSLSemantic(v, mStage, false, true, autoIdx++);
            EmitLine(TypeToHLSL(v->type) + " " + v->name + " : " + sem + ";");
        }
        // Si pas de sortie explicite, ajouter SV_Target0 par défaut
        if (mOutputVars.Empty()) {
            EmitLine("float4 _Color : SV_Target0;");
        }
        // Profondeur si nécessaire
        EmitLine("float _Depth : SV_Depth;");
        IndentPop();
        EmitLine("};");
        EmitNewLine();
        mOutputStructName = "PS_Output";
    }

    // ── Compute shader : pas de struct, juste les thread IDs ─────────────────
    if (mStage == NkSLStage::COMPUTE) {
        mInputStructName  = "";
        mOutputStructName = "";
    }
}

// =============================================================================
// MSL : génération des structs avec attributs déterministes
// =============================================================================
void NkSLCodeGen_MSL::GenInputOutputStructs() {
    NkString inStructName  = (mStage==NkSLStage::VERTEX) ? "VertexIn"  : "FragIn";
    NkString outStructName = (mStage==NkSLStage::VERTEX) ? "VertexOut" : "FragOut";

    // ── Struct d'entrée ───────────────────────────────────────────────────────
    if (!mInputVars.Empty()) {
        EmitLine("struct " + inStructName);
        EmitLine("{");
        IndentPush();
        // Vertex inputs → [[attribute(N)]]
        if (mStage == NkSLStage::VERTEX) {
            int autoLoc = 0;
            for (auto* v : mInputVars) {
                int loc = v->binding.HasLocation() ? v->binding.location : autoLoc;
                autoLoc = loc + 1;
                char buf[64]; snprintf(buf, sizeof(buf), " [[attribute(%d)]]", loc);
                EmitLine(TypeToMSL(v->type) + " " + v->name + NkString(buf) + ";");
            }
            EmitLine("uint _vertex_id   [[vertex_id]];");
            EmitLine("uint _instance_id [[instance_id]];");
        }
        // Fragment inputs (inter-stage) → pas d'attribut spécial
        else if (mStage == NkSLStage::FRAGMENT) {
            EmitLine("float4 _position [[position]];");
            for (auto* v : mInputVars) {
                // Les varyings inter-stages n'ont pas d'attribut en MSL —
                // ils sont transmis via la struct de sortie du vertex
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
        if (mStage == NkSLStage::VERTEX) {
            // Position builtin obligatoire en premier
            EmitLine("float4 _position [[position]];");
            // Varyings de sortie (inter-stage)
            for (auto* v : mOutputVars) {
                EmitLine(TypeToMSL(v->type) + " " + v->name + ";");
            }
        }
        else if (mStage == NkSLStage::FRAGMENT) {
            // Render targets [[color(N)]]
            int colorIdx = 0;
            for (auto* v : mOutputVars) {
                char buf[64]; snprintf(buf, sizeof(buf), " [[color(%d)]]", colorIdx++);
                EmitLine(TypeToMSL(v->type) + " " + v->name + NkString(buf) + ";");
            }
            if (mOutputVars.Empty()) {
                EmitLine("float4 _color [[color(0)]];");
            }
            EmitLine("float _depth [[depth(any)]];");
        }
        IndentPop();
        EmitLine("};");
        EmitNewLine();
    }
}

// =============================================================================
// Signature de l'entry point MSL complète avec tous les paramètres
// =============================================================================
NkString NkSLCodeGen_MSL::BuildEntryPointSignature(NkSLFunctionDeclNode* fn) {
    NkString inStructName  = (mStage==NkSLStage::VERTEX) ? "VertexIn"  :
                             (mStage==NkSLStage::FRAGMENT) ? "FragIn" : "";
    NkString outStructName = (mStage==NkSLStage::VERTEX) ? "VertexOut" :
                             (mStage==NkSLStage::FRAGMENT) ? "FragOut" : "void";

    NkString qualifier;
    if      (mStage == NkSLStage::VERTEX)   qualifier = "vertex ";
    else if (mStage == NkSLStage::FRAGMENT)  qualifier = "fragment ";
    else if (mStage == NkSLStage::COMPUTE)   qualifier = "kernel ";

    NkString retType = (mStage == NkSLStage::COMPUTE) ? "void" : outStructName;
    NkString sig     = qualifier + retType + " " + fn->name + "_entry(";

    bool first = true;

    // Stage in (struct d'entrée)
    if (!inStructName.Empty()) {
        sig += inStructName + " _in [[stage_in]]";
        first = false;
    }

    // Buffers uniformes (constant T& name [[buffer(N)]])
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

    // Textures et samplers
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

            // Sampler correspondant
            sig += ", ";
            bool isShadow = (v->type->baseType == NkSLBaseType::SAMPLER2D_SHADOW ||
                             v->type->baseType == NkSLBaseType::SAMPLER2D_ARRAY_SHADOW ||
                             v->type->baseType == NkSLBaseType::SAMPLER_CUBE_SHADOW);
            snprintf(buf, sizeof(buf), "sampler %s_smp [[sampler(%d)]]",
                     v->name.CStr(), sampIdx);
            sig += NkString(buf);
            texIdx++; sampIdx++;
        }
        else if (NkSLTypeIsImage(v->type->baseType)) {
            if (!first) sig += ", ";
            int bidx = v->binding.HasBinding() ? v->binding.binding : texIdx++;
            char buf[256];
            snprintf(buf, sizeof(buf), "%s %s_tex [[texture(%d)]]",
                     BaseTypeToMSL(v->type->baseType).CStr(), v->name.CStr(), bidx);
            sig += NkString(buf);
            first = false;
        }
    }

    // Compute : thread IDs
    if (mStage == NkSLStage::COMPUTE) {
        if (!first) sig += ", ";
        sig += "uint3 _thread_pos_in_grid     [[thread_position_in_grid]], ";
        sig += "uint3 _thread_pos_in_group    [[thread_position_in_threadgroup]], ";
        sig += "uint3 _threadgroup_pos_in_grid[[threadgroup_position_in_grid]]";
    }

    sig += ")";
    return sig;
}

} // namespace nkentseu
