// =============================================================================
// NkSLCodeGenGLSLVulkan.cpp  — v4.0
//
// Backend GLSL 4.50+ pour Vulkan.
//
// Différences fondamentales avec NK_GLSL (OpenGL) :
//
//   1. VERSION & EXTENSION
//      #version 450
//      #extension GL_KHR_vulkan_glsl : require
//      (+ GL_EXT_debug_printf si opts.vkDebugPrintf)
//
//   2. BINDINGS OBLIGATOIREMENT EXPLICITES
//      layout(set=N, binding=M) uniform ...
//      Le set est obligatoire. Pas d'aplatissement.
//
//   3. PUSH CONSTANTS
//      layout(push_constant) uniform PC { ... } pc;
//      Un seul block push_constant par stage.
//
//   4. INPUT ATTACHMENTS (subpass)
//      layout(input_attachment_index=N, set=S, binding=B) uniform subpassInput uInput;
//      Accès : subpassLoad(uInput)
//
//   5. VERTEX BUILTINS RENOMMÉS
//      gl_VertexID   → gl_VertexIndex    (Vulkan)
//      gl_InstanceID → gl_InstanceIndex  (Vulkan)
//
//   6. DRAW PARAMS (optionnel, si opts.vkDrawParams)
//      #extension GL_ARB_shader_draw_parameters : require
//      → gl_BaseVertex, gl_BaseInstance, gl_DrawID
//
//   7. STORAGE BUFFERS (SSBO)
//      layout(set=N, binding=M, std430) buffer ...
//      std430 (et non std140) pour les SSBO.
//
//   8. SPÉCIALISATION CONSTANTS (future)
//      layout(constant_id=N) const int VAL = default;
// =============================================================================
#include "NkSLCodeGen.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Builtins — remapping OpenGL → Vulkan
// =============================================================================
NkString NkSLCodeGenGLSLVulkan::BuiltinToVkGLSL(const NkString& name,
                                                   NkSLStage stage) {
    // Vulkan renomme gl_VertexID/InstanceID
    if (name == "gl_VertexID")    return "gl_VertexIndex";
    if (name == "gl_InstanceID")  return "gl_InstanceIndex";
    // Tous les autres builtins sont identiques à OpenGL
    if (name == "gl_Position")           return "gl_Position";
    if (name == "gl_FragCoord")          return "gl_FragCoord";
    if (name == "gl_FragDepth")          return "gl_FragDepth";
    if (name == "gl_FrontFacing")        return "gl_FrontFacing";
    if (name == "gl_LocalInvocationID")  return "gl_LocalInvocationID";
    if (name == "gl_GlobalInvocationID") return "gl_GlobalInvocationID";
    if (name == "gl_WorkGroupID")        return "gl_WorkGroupID";
    if (name == "gl_WorkGroupSize")      return "gl_WorkGroupSize";
    // Draw params (GL_ARB_shader_draw_parameters)
    if (name == "gl_BaseVertex")   return "gl_BaseVertex";
    if (name == "gl_BaseInstance") return "gl_BaseInstance";
    if (name == "gl_DrawID")       return "gl_DrawID";
    return name;
}

// =============================================================================
// layout(set=N, binding=M [...]) — construction du qualificateur de layout
// =============================================================================
NkString NkSLCodeGenGLSLVulkan::LayoutQualifier(const NkSLBinding& b,
                                                  bool isBlock,
                                                  bool isStorage,
                                                  bool isPushConstant,
                                                  const char* extra) {
    if (isPushConstant) {
        return "layout(push_constant) ";
    }

    char buf[128];
    int set     = b.HasSet()     ? b.set     : mAutoSet;
    int binding = b.HasBinding() ? b.binding : mAutoBinding++;

    if (b.HasInputAttachment()) {
        snprintf(buf, sizeof(buf),
                 "layout(input_attachment_index=%d, set=%d, binding=%d) ",
                 b.inputAttachment, set, binding);
        return NkString(buf);
    }

    if (extra && extra[0]) {
        snprintf(buf, sizeof(buf), "layout(set=%d, binding=%d, %s) ", set, binding, extra);
    } else {
        snprintf(buf, sizeof(buf), "layout(set=%d, binding=%d) ", set, binding);
    }
    return NkString(buf);
}

// =============================================================================
// Generate
// =============================================================================
NkSLCompileResult NkSLCodeGenGLSLVulkan::Generate(NkSLProgramNode* ast,
                                                    NkSLStage stage,
                                                    const NkSLCompileOptions& opts) {
    mOpts        = &opts;
    mStage       = stage;
    mOutput      = "";
    mAutoSet     = 0;
    mAutoBinding = 0;
    mErrors.Clear();
    mWarnings.Clear();

    GenProgram(ast);

    NkSLCompileResult res;
    res.success  = mErrors.Empty();
    res.source   = mOutput;
    res.target   = NkSLTarget::NK_GLSL_VULKAN;
    res.stage    = stage;
    res.errors   = mErrors;
    res.warnings = mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

// =============================================================================
// En-tête du fichier GLSL Vulkan
// =============================================================================
void NkSLCodeGenGLSLVulkan::GenProgram(NkSLProgramNode* prog) {
    uint32 ver = mOpts->glslVulkanVersion;
    if (ver < 450) ver = 450;

    char buf[256];
    snprintf(buf, sizeof(buf), "#version %u\n", ver);
    Emit(NkString(buf));

    // Extension obligatoire pour le GLSL Vulkan
    EmitLine("#extension GL_KHR_vulkan_glsl : require");

    // Extensions optionnelles
    if (mOpts->vkDrawParams) {
        EmitLine("#extension GL_ARB_shader_draw_parameters : require");
    }
    if (mOpts->vkDebugPrintf) {
        EmitLine("#extension GL_EXT_debug_printf : enable");
    }
    if (mStage == NkSLStage::NK_COMPUTE) {
        EmitLine("#extension GL_ARB_compute_shader : require");
    }
    EmitNewLine();

    // Déclarations globales
    for (auto* child : prog->children) {
        GenDecl(child);
    }
}

// =============================================================================
// Dispatch des déclarations globales
// =============================================================================
void NkSLCodeGenGLSLVulkan::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
        case NkSLNodeKind::NK_DECL_PUSH_CONSTANT:
            GenBlock(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_VAR:
        case NkSLNodeKind::NK_DECL_INPUT:
        case NkSLNodeKind::NK_DECL_OUTPUT:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), true); break;
        default: break;
    }
}

// =============================================================================
// Déclaration de variable globale
// =============================================================================
void NkSLCodeGenGLSLVulkan::GenVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
    if (!v || !v->type) return;
    NkString line;

    if (isGlobal) {
        bool isSampler = NkSLTypeIsSampler(v->type->baseType);
        bool isImage   = NkSLTypeIsImage(v->type->baseType);
        bool isSubpass = (v->type->baseType == NkSLBaseType::NK_SUBPASS_INPUT ||
                          v->type->baseType == NkSLBaseType::NK_SUBPASS_INPUT_MS);

        if (isSampler || isImage || isSubpass) {
            // layout(set=N, binding=M) uniform sampler2D ...
            // layout(input_attachment_index=K, set=N, binding=M) uniform subpassInput ...
            line += LayoutQualifier(v->binding, false, false, false);
            line += "uniform ";
        } else if (v->storage == NkSLStorageQual::NK_UNIFORM) {
            // Uniform scalaire isolé : layout(set=0, binding=N) uniform float x;
            // (rare en Vulkan, mais supporté)
            line += LayoutQualifier(v->binding, false, false, false);
            line += "uniform ";
        } else if (v->binding.HasLocation()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "layout(location = %d) ", v->binding.location);
            line += NkString(buf);
        }

        // Interpolation
        if (v->interp == NkSLInterpolation::NK_FLAT)          line += "flat ";
        if (v->interp == NkSLInterpolation::NK_NOPERSPECTIVE) line += "noperspective ";

        switch (v->storage) {
            case NkSLStorageQual::NK_IN:  line += "in ";  break;
            case NkSLStorageQual::NK_OUT: line += "out "; break;
            case NkSLStorageQual::NK_SHARED: line += "shared "; break;
            default: break;
        }
        if (v->isConst) line += "const ";
    } else {
        if (v->isConst) line += "const ";
    }

    // Précision
    if      (v->precision == NkSLPrecision::NK_LOWP)   line += "lowp ";
    else if (v->precision == NkSLPrecision::NK_MEDIUMP) line += "mediump ";
    else if (v->precision == NkSLPrecision::NK_HIGHP)   line += "highp ";

    line += TypeToString(v->type) + " " + v->name;

    if (v->initializer) {
        line += " = " + GenExpr(v->initializer);
    }
    EmitLine(line + ";");
}

// =============================================================================
// Block uniforme / storage / push_constant
// =============================================================================
void NkSLCodeGenGLSLVulkan::GenBlock(NkSLBlockDeclNode* b) {
    bool isPush    = (b->storage == NkSLStorageQual::NK_PUSH_CONSTANT);
    bool isStorage = (b->storage == NkSLStorageQual::NK_BUFFER);

    // layout(...)
    if (isPush) {
        Emit("layout(push_constant) ");
    } else {
        NkString layoutStr = LayoutQualifier(b->binding, true, isStorage, false,
                                              isStorage ? "std430" : "std140");
        Emit(layoutStr);
    }

    // Qualificateur de stockage
    switch (b->storage) {
        case NkSLStorageQual::NK_UNIFORM:       Emit("uniform "); break;
        case NkSLStorageQual::NK_BUFFER:        Emit("buffer ");  break;
        case NkSLStorageQual::NK_PUSH_CONSTANT: Emit("uniform "); break;
        default: break;
    }

    EmitLine(b->blockName);
    EmitLine("{");
    IndentPush();
    for (auto* m : b->members) GenVarDecl(m, false);
    IndentPop();

    if (!b->instanceName.Empty())
        EmitLine("} " + b->instanceName + ";");
    else
        EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGenGLSLVulkan::GenStruct(NkSLStructDeclNode* s) {
    EmitLine("struct " + s->name);
    EmitLine("{");
    IndentPush();
    for (auto* m : s->members) GenVarDecl(m, false);
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGenGLSLVulkan::GenFunction(NkSLFunctionDeclNode* fn) {
    NkString sig = TypeToString(fn->returnType) + " " + fn->name + "(";
    for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
        auto* p = fn->params[i];
        if (i > 0) sig += ", ";
        switch (p->storage) {
            case NkSLStorageQual::NK_IN:    sig += "in ";    break;
            case NkSLStorageQual::NK_OUT:   sig += "out ";   break;
            case NkSLStorageQual::NK_INOUT: sig += "inout "; break;
            default: break;
        }
        sig += TypeToString(p->type);
        if (!p->name.Empty()) sig += " " + p->name;
    }
    sig += ")";

    if (!fn->body) { EmitLine(sig + ";"); EmitNewLine(); return; }
    EmitLine(sig);
    GenStmt(fn->body);
    EmitNewLine();
}

void NkSLCodeGenGLSLVulkan::GenStmt(NkSLNode* node) {
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
                    init = TypeToString(vd->type) + " " + vd->name;
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

NkString NkSLCodeGenGLSLVulkan::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::NK_EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::NK_EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            // Vulkan : remapper gl_VertexID → gl_VertexIndex, etc.
            return BuiltinToVkGLSL(id->name, mStage);
        }
        case NkSLNodeKind::NK_EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString op = GenExpr(u->operand);
            return u->prefix ? (u->op + op) : (op + u->op);
        }
        case NkSLNodeKind::NK_EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            return "(" + GenExpr(b->left) + " " + b->op + " " + GenExpr(b->right) + ")";
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
            return TypeToString(c->targetType) + "(" + GenExpr(c->expr) + ")";
        }
        case NkSLNodeKind::NK_STMT_EXPR:
            return node->children.Empty() ? "" : GenExpr(node->children[0]);
        default:
            return "/* unknown */";
    }
}

NkString NkSLCodeGenGLSLVulkan::GenCall(NkSLCallNode* call) {
    NkString callee = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;

    // subpassLoad(attachment) → identique, juste vérifier le nom
    // En GLSL Vulkan standard, subpassLoad est une fonction builtin

    NkString args;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
        if (i > 0) args += ", ";
        args += GenExpr(call->args[i]);
    }
    return callee + "(" + args + ")";
}

NkString NkSLCodeGenGLSLVulkan::LiteralToStr(NkSLLiteralNode* lit) {
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

} // namespace nkentseu
