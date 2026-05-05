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
// ComputeBlockSize — taille std140 d'un uniform block
// =============================================================================
uint32 NkSLReflector::ComputeBlockSize(NkSLBlockDeclNode* b) {
    if (!b) return 0;
    uint32 total = 0;
    for (auto* m : b->members) {
        total += ComputeMemberSize(m);
    }
    // Alignement std140 : multiple de 16 bytes
    return (total + 15) & ~15u;
}

uint32 NkSLReflector::ComputeMemberSize(NkSLVarDeclNode* m) {
    if (!m || !m->type) return 0;
    uint32 baseSize = NkSLBaseTypeSize(m->type->baseType);
    if (m->type->arraySize > 0) {
        // std140 : chaque élément de tableau est aligné sur 16 bytes
        uint32 elemSize = (baseSize + 15) & ~15u;
        return elemSize * m->type->arraySize;
    }
    return baseSize;
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
