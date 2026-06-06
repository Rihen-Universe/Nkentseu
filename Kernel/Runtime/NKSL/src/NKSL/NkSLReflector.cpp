// =============================================================================
// NkSLReflector.cpp  — v3.0
//
// Reflection automatique depuis l'AST NkSL.
// Extrait tous les bindings, vertex inputs, outputs, et tailles de buffers
// sans aucune analyse du bytecode — directement depuis l'AST parsé.
//
// Usage :
//   NkSLReflector reflector;
//   NkSLReflection r = reflector.Reflect(ast, NkSLStage::NK_VERTEX);
//   for (auto& res : r.resources)
//       printf("binding %d: %s\n", res.binding, res.name.CStr());
//
//   // Générer le layout JSON
//   NkString json = reflector.GenerateLayoutJSON(r);
//
//   // Générer le code C++ de création du layout
//   NkString cpp = reflector.GenerateLayoutCPP(r, "myLayout");
// =============================================================================
#include "NkSLCodeGen.h"
#include "NKContainers/String/NkFormat.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Reflect — point d'entrée principal
// =============================================================================
NkSLReflection NkSLReflector::Reflect(NkSLProgramNode* ast, NkSLStage stage) {
    NkSLReflection out;
    mAutoBinding = 0;
    mAutoLocation = 0;

    if (!ast) return out;

    // Compute : taille de workgroup collectée par le parser depuis
    // layout(local_size_x/y/z) in;  (défaut 1,1,1 pour les autres stages).
    out.localSizeX = ast->localSizeX;
    out.localSizeY = ast->localSizeY;
    out.localSizeZ = ast->localSizeZ;

    for (auto* child : ast->children) {
        ReflectDecl(child, stage, out);
    }

    return out;
}

// =============================================================================
// ReflectDecl — dispatch selon le type de nœud
// =============================================================================
void NkSLReflector::ReflectDecl(NkSLNode* node, NkSLStage stage, NkSLReflection& out) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_DECL_VAR:
        case NkSLNodeKind::NK_DECL_INPUT:
        case NkSLNodeKind::NK_DECL_OUTPUT:
            ReflectVarDecl(static_cast<NkSLVarDeclNode*>(node), stage, out);
            break;
        case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
        case NkSLNodeKind::NK_DECL_PUSH_CONSTANT:
            ReflectBlockDecl(static_cast<NkSLBlockDeclNode*>(node), stage, out);
            break;
        case NkSLNodeKind::NK_DECL_FUNCTION:
            ReflectFunction(static_cast<NkSLFunctionDeclNode*>(node), stage, out);
            break;
        default:
            break;
    }
}

// =============================================================================
// ReflectVarDecl — variables globales (in/out/uniform)
// =============================================================================
void NkSLReflector::ReflectVarDecl(NkSLVarDeclNode* v, NkSLStage stage, NkSLReflection& out) {
    if (!v || !v->type) return;

    // Vertex inputs
    if (v->storage == NkSLStorageQual::NK_IN && stage == NkSLStage::NK_VERTEX) {
        NkSLVertexInput vi;
        vi.name       = v->name;
        vi.location   = v->binding.HasLocation() ? (uint32)v->binding.location : (uint32)mAutoLocation++;
        vi.baseType   = v->type->baseType;
        vi.components = NkSLBaseTypeComponents(v->type->baseType);
        out.vertexInputs.PushBack(vi);
        return;
    }

    // Fragment outputs
    if (v->storage == NkSLStorageQual::NK_OUT && stage == NkSLStage::NK_FRAGMENT) {
        NkSLStageOutput so;
        so.name     = v->name;
        so.location = v->binding.HasLocation() ? (uint32)v->binding.location : (uint32)mAutoLocation++;
        so.baseType = v->type->baseType;
        out.stageOutputs.PushBack(so);
        return;
    }

    // Uniforms (samplers, images, etc.)
    if (v->storage == NkSLStorageQual::NK_UNIFORM) {
        NkSLResourceBinding rb;
        rb.name     = v->name;
        rb.stages   = stage;
        rb.baseType = v->type->baseType;
        rb.binding  = v->binding.HasBinding()  ? (uint32)v->binding.binding  : (uint32)mAutoBinding++;
        rb.set      = v->binding.HasSet()       ? (uint32)v->binding.set      : 0;

        if (NkSLTypeIsSampler(v->type->baseType)) {
            rb.kind = NkSLResourceKind::NK_SAMPLED_TEXTURE;
        } else if (NkSLTypeIsImage(v->type->baseType)) {
            rb.kind = NkSLResourceKind::NK_STORAGE_IMAGE;
        } else {
            rb.kind      = NkSLResourceKind::NK_UNIFORM_BUFFER;
            rb.sizeBytes = NkSLBaseTypeSize(v->type->baseType);
        }

        if (v->type->arraySize > 0) rb.arraySize = v->type->arraySize;

        out.resources.PushBack(rb);
    }
}

// =============================================================================
// ReflectBlockDecl — uniform/storage blocks
// =============================================================================
void NkSLReflector::ReflectBlockDecl(NkSLBlockDeclNode* b, NkSLStage stage, NkSLReflection& out) {
    if (!b) return;

    NkSLResourceBinding rb;
    rb.name      = b->instanceName.Empty() ? b->blockName : b->instanceName;
    rb.typeName  = b->blockName;
    rb.stages    = stage;
    rb.binding   = b->binding.HasBinding() ? (uint32)b->binding.binding : (uint32)mAutoBinding++;
    rb.set       = b->binding.HasSet()     ? (uint32)b->binding.set     : 0;
    rb.sizeBytes = ComputeBlockSize(b);

    switch (b->storage) {
        case NkSLStorageQual::NK_UNIFORM:       rb.kind = NkSLResourceKind::NK_UNIFORM_BUFFER;  break;
        case NkSLStorageQual::NK_BUFFER:        rb.kind = NkSLResourceKind::NK_STORAGE_BUFFER;  break;
        case NkSLStorageQual::NK_PUSH_CONSTANT: rb.kind = NkSLResourceKind::NK_PUSH_CONSTANT;   break;
        default:                             rb.kind = NkSLResourceKind::NK_UNIFORM_BUFFER;  break;
    }

    out.resources.PushBack(rb);
}

// =============================================================================
// ReflectFunction — extrait les local_size pour compute
// =============================================================================
void NkSLReflector::ReflectFunction(NkSLFunctionDeclNode* fn, NkSLStage stage, NkSLReflection& out) {
    // Pour les compute shaders, on pourrait extraire local_size depuis des annotations
    // Pour l'instant on met les valeurs par défaut — à enrichir si @local_size est ajouté
    if (stage == NkSLStage::NK_COMPUTE && (fn->isEntry || fn->name == "main")) {
        // Valeurs par défaut — seront overridées si une annotation @local_size est présente
        if (out.localSizeX == 1 && out.localSizeY == 1 && out.localSizeZ == 1) {
            out.localSizeX = 1;
            out.localSizeY = 1;
            out.localSizeZ = 1;
        }
    }
}

// =============================================================================
// Layout std140 / std430 — alignements et tailles.
//
// Différence clé (corrige le désalignement des SSBO) :
//   - std140 (UBO)                : l'alignement d'un tableau ET son stride sont
//     arrondis au multiple de 16 (vec4). Ex. float[] -> stride 16.
//   - std430 (SSBO/buffer + push_constant Vulkan) : pas d'arrondi à 16 ; le
//     stride d'un tableau vaut l'alignement naturel de l'élément. Ex. float[] ->
//     stride 4. Utiliser des règles std140 sur un SSBO std430 décale TOUT.
// =============================================================================
namespace {

inline uint32 NkAlignUp(uint32 v, uint32 a) { return a ? ((v + a - 1) & ~(a - 1)) : v; }

// Alignement de base (octets) d'un scalaire / vecteur / matrice.
uint32 NkSLBaseAlign(NkSLBaseType t, bool std430) {
    switch (t) {
        case NkSLBaseType::NK_BOOL: case NkSLBaseType::NK_INT:
        case NkSLBaseType::NK_UINT: case NkSLBaseType::NK_FLOAT: return 4;
        case NkSLBaseType::NK_DOUBLE:                            return 8;
        case NkSLBaseType::NK_IVEC2: case NkSLBaseType::NK_UVEC2:
        case NkSLBaseType::NK_VEC2:                              return 8;
        case NkSLBaseType::NK_DVEC2:                             return 16;
        case NkSLBaseType::NK_IVEC3: case NkSLBaseType::NK_UVEC3:
        case NkSLBaseType::NK_VEC3:
        case NkSLBaseType::NK_IVEC4: case NkSLBaseType::NK_UVEC4:
        case NkSLBaseType::NK_VEC4:                              return 16;
        case NkSLBaseType::NK_MAT2: return std430 ? 8u : 16u;    // colonnes vec2
        case NkSLBaseType::NK_MAT3:                              return 16; // colonnes vec3
        case NkSLBaseType::NK_MAT4:                              return 16; // colonnes vec4
        default:                                                return 4;
    }
}

// Taille "naturelle" (hors stride de tableau).
uint32 NkSLNaturalSize(NkSLBaseType t, bool std430) {
    switch (t) {
        case NkSLBaseType::NK_MAT2: return (std430 ? 8u : 16u) * 2u;
        case NkSLBaseType::NK_MAT3: return 16u * 3u; // 48
        case NkSLBaseType::NK_MAT4: return 16u * 4u; // 64
        default:                    return NkSLBaseTypeSize(t);
    }
}

// Stride d'un élément de tableau.
uint32 NkSLArrayStride(NkSLBaseType t, bool std430) {
    uint32 align  = NkSLBaseAlign(t, std430);
    uint32 stride = NkAlignUp(NkSLNaturalSize(t, std430), align);
    if (!std430) stride = NkAlignUp(stride, 16); // std140 : multiple de 16
    return stride;
}

} // namespace

// =============================================================================
// ComputeBlockSize — std140 pour UBO, std430 pour SSBO/push_constant ; offsets
// alignés par membre (et non simple somme des tailles).
// =============================================================================
uint32 NkSLReflector::ComputeBlockSize(NkSLBlockDeclNode* b) {
    if (!b) return 0;
    const bool std430 = (b->storage == NkSLStorageQual::NK_BUFFER ||
                         b->storage == NkSLStorageQual::NK_PUSH_CONSTANT);
    uint32 offset   = 0;
    uint32 maxAlign = std430 ? 1u : 16u; // std140 : taille de bloc multiple de 16
    for (auto* m : b->members) {
        if (!m || !m->type) continue;
        const bool isArray = (m->type->arraySize > 0);
        uint32 align = NkSLBaseAlign(m->type->baseType, std430);
        if (isArray && !std430) align = NkAlignUp(align, 16); // std140 : tableau aligné 16
        uint32 size  = isArray
            ? NkSLArrayStride(m->type->baseType, std430) * m->type->arraySize
            : NkSLNaturalSize(m->type->baseType, std430);
        if (align > maxAlign) maxAlign = align;
        offset = NkAlignUp(offset, align);
        offset += size;
    }
    return NkAlignUp(offset, maxAlign ? maxAlign : 16u);
}

uint32 NkSLReflector::ComputeMemberSize(NkSLVarDeclNode* m) {
    // Conservé pour compat : taille d'un membre isolé (règles std140).
    if (!m || !m->type) return 0;
    if (m->type->arraySize > 0)
        return NkSLArrayStride(m->type->baseType, false) * m->type->arraySize;
    return NkSLNaturalSize(m->type->baseType, false);
}

// =============================================================================
// GenerateLayoutJSON — génère une description JSON du layout
// =============================================================================
NkString NkSLReflector::GenerateLayoutJSON(const NkSLReflection& reflection) {
    NkString json = "{\n";
    json += "  \"resources\": [\n";

    for (uint32 i = 0; i < (uint32)reflection.resources.Size(); i++) {
        const auto& r = reflection.resources[i];
        char buf[512];
        const char* kindStr = "unknown";
        switch (r.kind) {
            case NkSLResourceKind::NK_UNIFORM_BUFFER:  kindStr = "uniform_buffer"; break;
            case NkSLResourceKind::NK_STORAGE_BUFFER:  kindStr = "storage_buffer"; break;
            case NkSLResourceKind::NK_PUSH_CONSTANT:   kindStr = "push_constant";  break;
            case NkSLResourceKind::NK_SAMPLED_TEXTURE: kindStr = "sampled_texture";break;
            case NkSLResourceKind::NK_STORAGE_IMAGE:   kindStr = "storage_image";  break;
            case NkSLResourceKind::NK_SAMPLER:         kindStr = "sampler";        break;
            default: break;
        }
        snprintf(buf, sizeof(buf),
            "    {\n"
            "      \"name\": \"%s\",\n"
            "      \"kind\": \"%s\",\n"
            "      \"set\": %u,\n"
            "      \"binding\": %u,\n"
            "      \"size_bytes\": %u,\n"
            "      \"array_size\": %u\n"
            "    }%s\n",
            r.name.CStr(), kindStr,
            r.set, r.binding, r.sizeBytes, r.arraySize,
            (i + 1 < (uint32)reflection.resources.Size()) ? "," : "");
        json += NkString(buf);
    }

    json += "  ],\n";
    json += "  \"vertex_inputs\": [\n";

    for (uint32 i = 0; i < (uint32)reflection.vertexInputs.Size(); i++) {
        const auto& vi = reflection.vertexInputs[i];
        char buf[256];
        snprintf(buf, sizeof(buf),
            "    { \"name\": \"%s\", \"location\": %u, \"components\": %u }%s\n",
            vi.name.CStr(), vi.location, vi.components,
            (i + 1 < (uint32)reflection.vertexInputs.Size()) ? "," : "");
        json += NkString(buf);
    }

    json += "  ],\n";
    json += "  \"stage_outputs\": [\n";

    for (uint32 i = 0; i < (uint32)reflection.stageOutputs.Size(); i++) {
        const auto& so = reflection.stageOutputs[i];
        char buf[256];
        snprintf(buf, sizeof(buf),
            "    { \"name\": \"%s\", \"location\": %u }%s\n",
            so.name.CStr(), so.location,
            (i + 1 < (uint32)reflection.stageOutputs.Size()) ? "," : "");
        json += NkString(buf);
    }

    json += "  ]\n";
    json += "}\n";
    return json;
}

// =============================================================================
// GenerateLayoutCPP — génère le code C++ NkIDevice pour créer le layout
// =============================================================================
NkString NkSLReflector::GenerateLayoutCPP(const NkSLReflection& reflection,
                                            const NkString& varName) {
    NkString cpp;
    cpp += "// Auto-generated by NkSLReflector\n";
    cpp += "// Do not edit manually\n\n";

    cpp += "NkDescriptorSetLayoutDesc " + varName + ";\n";

    for (auto& r : reflection.resources) {
        char buf[512];
        const char* bindingType = "NK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";
        switch (r.kind) {
            case NkSLResourceKind::NK_UNIFORM_BUFFER:  bindingType = "NK_DESCRIPTOR_TYPE_UNIFORM_BUFFER";  break;
            case NkSLResourceKind::NK_STORAGE_BUFFER:  bindingType = "NK_DESCRIPTOR_TYPE_STORAGE_BUFFER";  break;
            case NkSLResourceKind::NK_PUSH_CONSTANT:   bindingType = "NK_DESCRIPTOR_TYPE_PUSH_CONSTANT";   break;
            case NkSLResourceKind::NK_SAMPLED_TEXTURE: bindingType = "NK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"; break;
            case NkSLResourceKind::NK_STORAGE_IMAGE:   bindingType = "NK_DESCRIPTOR_TYPE_STORAGE_IMAGE";   break;
            case NkSLResourceKind::NK_SAMPLER:         bindingType = "NK_DESCRIPTOR_TYPE_SAMPLER";         break;
            default: break;
        }

        if (r.kind == NkSLResourceKind::NK_PUSH_CONSTANT) {
            snprintf(buf, sizeof(buf),
                "// Push constant: %s (%u bytes)\n"
                "%s.AddPushConstant(\"%s\", NK_SHADER_STAGE_ALL, 0, %u);\n",
                r.name.CStr(), r.sizeBytes,
                varName.CStr(), r.name.CStr(), r.sizeBytes);
        } else {
            snprintf(buf, sizeof(buf),
                "// %s: set=%u binding=%u%s\n"
                "%s.AddBinding(%u, %u, %s, 1, NK_SHADER_STAGE_ALL); // %s\n",
                r.name.CStr(), r.set, r.binding,
                r.sizeBytes > 0 ? (" size=" + ::nkentseu::NkFormat("{0}", r.sizeBytes) + " bytes").CStr() : "",
                varName.CStr(), r.set, r.binding, bindingType, r.name.CStr());
        }
        cpp += NkString(buf);
    }

    cpp += "\n// Vertex input layout\n";
    cpp += "NkVertexInputDesc " + varName + "_vertex;\n";

    uint32 stride = 0;
    for (auto& vi : reflection.vertexInputs) {
        uint32 size = NkSLBaseTypeSize(vi.baseType);
        char buf[256];
        const char* fmtStr = "NK_FORMAT_R32G32B32A32_FLOAT";
        switch (vi.components) {
            case 1: fmtStr = "NK_FORMAT_R32_FLOAT";          break;
            case 2: fmtStr = "NK_FORMAT_R32G32_FLOAT";       break;
            case 3: fmtStr = "NK_FORMAT_R32G32B32_FLOAT";    break;
            case 4: fmtStr = "NK_FORMAT_R32G32B32A32_FLOAT"; break;
        }
        snprintf(buf, sizeof(buf),
            "%s_vertex.AddAttribute(%u, 0, %s, %u); // %s\n",
            varName.CStr(), vi.location, fmtStr, stride, vi.name.CStr());
        cpp += NkString(buf);
        stride += size;
    }

    char strideBuf[64];
    snprintf(strideBuf, sizeof(strideBuf),
        "%s_vertex.SetStride(0, %u);\n", varName.CStr(), stride);
    cpp += NkString(strideBuf);

    return cpp;
}

} // namespace nkentseu
