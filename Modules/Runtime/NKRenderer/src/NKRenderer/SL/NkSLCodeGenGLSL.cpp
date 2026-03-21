// =============================================================================
// NkSLCodeGen_GLSL.cpp
// Génération GLSL 4.30+ depuis l'AST NkSL.
// =============================================================================
#include "NkSLCodeGen.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
// Utilitaires de base partagés
// =============================================================================
NkString NkSLCodeGenBase::Indent() const {
    NkString s;
    for (uint32 i = 0; i < mIndent; i++) s += "    ";
    return s;
}

NkString NkSLCodeGenBase::BaseTypeToString(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::VOID:   return "void";
        case NkSLBaseType::BOOL:   return "bool";
        case NkSLBaseType::INT:    return "int";    case NkSLBaseType::IVEC2: return "ivec2";
        case NkSLBaseType::IVEC3:  return "ivec3";  case NkSLBaseType::IVEC4: return "ivec4";
        case NkSLBaseType::UINT:   return "uint";   case NkSLBaseType::UVEC2: return "uvec2";
        case NkSLBaseType::UVEC3:  return "uvec3";  case NkSLBaseType::UVEC4: return "uvec4";
        case NkSLBaseType::FLOAT:  return "float";  case NkSLBaseType::VEC2:  return "vec2";
        case NkSLBaseType::VEC3:   return "vec3";   case NkSLBaseType::VEC4:  return "vec4";
        case NkSLBaseType::DOUBLE: return "double"; case NkSLBaseType::DVEC2: return "dvec2";
        case NkSLBaseType::DVEC3:  return "dvec3";  case NkSLBaseType::DVEC4: return "dvec4";
        case NkSLBaseType::MAT2:   return "mat2";   case NkSLBaseType::MAT3:  return "mat3";
        case NkSLBaseType::MAT4:   return "mat4";
        case NkSLBaseType::MAT2X3: return "mat2x3"; case NkSLBaseType::MAT2X4: return "mat2x4";
        case NkSLBaseType::MAT3X2: return "mat3x2"; case NkSLBaseType::MAT3X4: return "mat3x4";
        case NkSLBaseType::MAT4X2: return "mat4x2"; case NkSLBaseType::MAT4X3: return "mat4x3";
        case NkSLBaseType::DMAT2:  return "dmat2";  case NkSLBaseType::DMAT3:  return "dmat3";
        case NkSLBaseType::DMAT4:  return "dmat4";
        case NkSLBaseType::SAMPLER2D:            return "sampler2D";
        case NkSLBaseType::SAMPLER2D_SHADOW:     return "sampler2DShadow";
        case NkSLBaseType::SAMPLER2D_ARRAY:      return "sampler2DArray";
        case NkSLBaseType::SAMPLER2D_ARRAY_SHADOW:return "sampler2DArrayShadow";
        case NkSLBaseType::SAMPLER_CUBE:         return "samplerCube";
        case NkSLBaseType::SAMPLER_CUBE_SHADOW:  return "samplerCubeShadow";
        case NkSLBaseType::SAMPLER3D:            return "sampler3D";
        case NkSLBaseType::ISAMPLER2D:           return "isampler2D";
        case NkSLBaseType::USAMPLER2D:           return "usampler2D";
        case NkSLBaseType::IMAGE2D:              return "image2D";
        case NkSLBaseType::IIMAGE2D:             return "iimage2D";
        case NkSLBaseType::UIMAGE2D:             return "uimage2D";
        default: return "float";
    }
}

NkString NkSLCodeGenBase::TypeToString(NkSLTypeNode* t) {
    if (!t) return "void";
    if (t->baseType == NkSLBaseType::STRUCT) return t->typeName;
    NkString s = BaseTypeToString(t->baseType);
    if (t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        s += NkString(buf);
    } else if (t->isUnsized) {
        s += "[]";
    }
    return s;
}

// =============================================================================
// GLSL
// =============================================================================
NkString NkSLCodeGen_GLSL::BuiltinToGLSL(const NkString& name, NkSLStage stage) {
    if (name == "gl_Position")          return "gl_Position";
    if (name == "gl_FragCoord")         return "gl_FragCoord";
    if (name == "gl_FragDepth")         return "gl_FragDepth";
    if (name == "gl_VertexID")          return "gl_VertexID";
    if (name == "gl_InstanceID")        return "gl_InstanceID";
    if (name == "gl_FrontFacing")       return "gl_FrontFacing";
    if (name == "gl_LocalInvocationID") return "gl_LocalInvocationID";
    if (name == "gl_GlobalInvocationID")return "gl_GlobalInvocationID";
    if (name == "gl_WorkGroupID")       return "gl_WorkGroupID";
    return name;
}

NkString NkSLCodeGen_GLSL::TypeQualifier(NkSLVarDeclNode* v, int bindingBase) {
    if (!v->type) return "";
    bool isSamplerOrImage = NkSLTypeIsSampler(v->type->baseType) ||
                            NkSLTypeIsImage(v->type->baseType);
    if (isSamplerOrImage) {
        int b = v->binding.HasBinding() ? v->binding.binding : mAutoBinding++;
        char buf[64]; snprintf(buf, sizeof(buf), "layout(binding = %d) ", b + bindingBase);
        return NkString(buf);
    }
    return "";
}

// =============================================================================
NkSLCompileResult NkSLCodeGen_GLSL::Generate(NkSLProgramNode* ast,
                                              NkSLStage stage,
                                              const NkSLCompileOptions& opts) {
    mOpts   = &opts;
    mStage  = stage;
    mOutput = "";
    mErrors.Clear();

    GenProgram(ast);

    NkSLCompileResult res;
    res.success = mErrors.Empty();
    res.source  = mOutput;
    res.target  = NkSLTarget::GLSL;
    res.stage   = stage;
    res.errors  = mErrors;
    res.warnings= mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

void NkSLCodeGen_GLSL::GenProgram(NkSLProgramNode* prog) {
    // Header
    uint32 ver = mOpts->glslVersion;
    if (ver < 430) ver = 430;
    char buf[128];
    snprintf(buf, sizeof(buf), "#version %u core\n", ver);
    EmitLine(NkString(buf));
    EmitNewLine();

    // Extensions
    if (mStage == NkSLStage::COMPUTE) {
        EmitLine("#extension GL_ARB_compute_shader : require");
        EmitNewLine();
    }

    // Précision par défaut (pour ES)
    if (mOpts->glslEs) {
        EmitLine("precision highp float;");
        EmitLine("precision highp int;");
        EmitNewLine();
    }

    // Déclarations
    for (auto* child : prog->children) {
        GenDecl(child);
    }
}

void NkSLCodeGen_GLSL::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node));
            break;
        case NkSLNodeKind::DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::DECL_STORAGE_BLOCK:
        case NkSLNodeKind::DECL_PUSH_CONSTANT:
            GenBlock(static_cast<NkSLBlockDeclNode*>(node));
            break;
        case NkSLNodeKind::DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node));
            break;
        case NkSLNodeKind::DECL_VAR:
        case NkSLNodeKind::DECL_INPUT:
        case NkSLNodeKind::DECL_OUTPUT:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), true);
            break;
        case NkSLNodeKind::ANNOTATION_BINDING:
        case NkSLNodeKind::ANNOTATION_LOCATION:
        case NkSLNodeKind::ANNOTATION_STAGE:
        case NkSLNodeKind::ANNOTATION_ENTRY:
        case NkSLNodeKind::ANNOTATION_BUILTIN:
            // Annotations GLSL : intégrées dans les déclarations suivantes
            // On les ignore ici car elles ont déjà été absorbées par le sémantique
            break;
        default:
            break;
    }
}

void NkSLCodeGen_GLSL::GenVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
    if (!v || !v->type) return;
    NkString line;

    if (isGlobal) {
        // Qualificateur de layout pour les bindings
        bool isSamplerOrImage = NkSLTypeIsSampler(v->type->baseType) ||
                                NkSLTypeIsImage(v->type->baseType);
        if (isSamplerOrImage && mOpts->flattenGLSLBindings) {
            int b = v->binding.HasBinding() ? v->binding.binding : mAutoBinding++;
            char buf[64]; snprintf(buf, sizeof(buf), "layout(binding = %d) ", b);
            line += NkString(buf);
        } else if (v->binding.HasLocation()) {
            char buf[64]; snprintf(buf, sizeof(buf), "layout(location = %d) ", v->binding.location);
            line += NkString(buf);
        }

        // Qualificateur d'interpolation
        if (v->interp == NkSLInterpolation::FLAT)          line += "flat ";
        if (v->interp == NkSLInterpolation::NOPERSPECTIVE)  line += "noperspective ";

        // Qualificateur de stockage
        switch (v->storage) {
            case NkSLStorageQual::IN:      line += "in ";      break;
            case NkSLStorageQual::OUT:     line += "out ";     break;
            case NkSLStorageQual::UNIFORM:
                if (!NkSLTypeIsSampler(v->type->baseType) && !NkSLTypeIsImage(v->type->baseType))
                    line += "uniform ";
                else
                    line += "uniform ";
                break;
            case NkSLStorageQual::SHARED:  line += "shared ";  break;
            default: break;
        }
        if (v->isConst) line += "const ";
    } else {
        if (v->isConst) line += "const ";
    }

    if (v->precision != NkSLPrecision::NONE) {
        if      (v->precision == NkSLPrecision::LOWP)   line += "lowp ";
        else if (v->precision == NkSLPrecision::MEDIUMP) line += "mediump ";
        else if (v->precision == NkSLPrecision::HIGHP)   line += "highp ";
    }

    line += TypeToString(v->type) + " " + v->name;

    // Tableau (si défini dans le nom et non dans le type)
    if (v->type && v->type->arraySize > 0 && v->type->kind != NkSLNodeKind::TYPE_ARRAY) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", v->type->arraySize);
        line += NkString(buf);
    }

    if (v->initializer) {
        line += " = " + GenExpr(v->initializer);
    }
    EmitLine(line + ";");
}

void NkSLCodeGen_GLSL::GenBlock(NkSLBlockDeclNode* b) {
    NkString line;
    // binding layout
    if (b->binding.HasBinding()) {
        char buf[128];
        if (b->binding.HasSet() && !mOpts->flattenGLSLBindings) {
            snprintf(buf, sizeof(buf), "layout(set = %d, binding = %d, std140) ",
                     b->binding.set, b->binding.binding);
        } else {
            int flat = b->binding.HasBinding() ? b->binding.binding : mAutoBinding++;
            snprintf(buf, sizeof(buf), "layout(binding = %d, std140) ", flat);
        }
        Emit(NkString(buf));
    } else {
        Emit("layout(std140) ");
    }

    // Qualificateur du block
    switch (b->storage) {
        case NkSLStorageQual::UNIFORM:       Emit("uniform "); break;
        case NkSLStorageQual::BUFFER:        Emit("buffer ");  break;
        case NkSLStorageQual::PUSH_CONSTANT: Emit("uniform "); break;
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

void NkSLCodeGen_GLSL::GenStruct(NkSLStructDeclNode* s) {
    EmitLine("struct " + s->name);
    EmitLine("{");
    IndentPush();
    for (auto* m : s->members) GenVarDecl(m, false);
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGen_GLSL::GenFunction(NkSLFunctionDeclNode* fn) {
    // Signature
    NkString sig = TypeToString(fn->returnType) + " " + fn->name + "(";
    for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
        auto* p = fn->params[i];
        if (i > 0) sig += ", ";
        switch (p->storage) {
            case NkSLStorageQual::IN:    sig += "in ";    break;
            case NkSLStorageQual::OUT:   sig += "out ";   break;
            case NkSLStorageQual::INOUT: sig += "inout "; break;
            default: break;
        }
        sig += TypeToString(p->type);
        if (!p->name.Empty()) sig += " " + p->name;
    }
    sig += ")";

    if (!fn->body) {
        EmitLine(sig + ";");
        EmitNewLine();
        return;
    }

    EmitLine(sig);
    GenStmt(fn->body);
    EmitNewLine();
}

void NkSLCodeGen_GLSL::GenStmt(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::STMT_BLOCK: {
            EmitLine("{");
            IndentPush();
            for (auto* c : node->children) GenStmt(c);
            IndentPop();
            EmitLine("}");
            break;
        }
        case NkSLNodeKind::STMT_EXPR: {
            if (!node->children.Empty())
                EmitLine(GenExpr(node->children[0]) + ";");
            break;
        }
        case NkSLNodeKind::DECL_VAR: {
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), false);
            break;
        }
        case NkSLNodeKind::STMT_IF: {
            auto* n = static_cast<NkSLIfNode*>(node);
            EmitLine("if (" + GenExpr(n->condition) + ")");
            GenStmt(n->thenBranch);
            if (n->elseBranch) {
                EmitLine("else");
                GenStmt(n->elseBranch);
            }
            break;
        }
        case NkSLNodeKind::STMT_FOR: {
            auto* n = static_cast<NkSLForNode*>(node);
            NkString init, cond, inc;
            if (n->init) {
                // init est parfois une déclaration
                if (n->init->kind == NkSLNodeKind::DECL_VAR) {
                    auto* vd = static_cast<NkSLVarDeclNode*>(n->init);
                    init = TypeToString(vd->type) + " " + vd->name;
                    if (vd->initializer) init += " = " + GenExpr(vd->initializer);
                } else {
                    init = GenExpr(n->init);
                }
            }
            if (n->condition) cond = GenExpr(n->condition);
            if (n->increment) inc  = GenExpr(n->increment);
            EmitLine("for (" + init + "; " + cond + "; " + inc + ")");
            GenStmt(n->body);
            break;
        }
        case NkSLNodeKind::STMT_WHILE: {
            auto* n = static_cast<NkSLWhileNode*>(node);
            EmitLine("while (" + GenExpr(n->condition) + ")");
            GenStmt(n->body);
            break;
        }
        case NkSLNodeKind::STMT_DO_WHILE: {
            EmitLine("do");
            GenStmt(node->children[0]);
            EmitLine("while (" + GenExpr(node->children[1]) + ");");
            break;
        }
        case NkSLNodeKind::STMT_RETURN: {
            auto* n = static_cast<NkSLReturnNode*>(node);
            if (n->value)
                EmitLine("return " + GenExpr(n->value) + ";");
            else
                EmitLine("return;");
            break;
        }
        case NkSLNodeKind::STMT_BREAK:    EmitLine("break;");    break;
        case NkSLNodeKind::STMT_CONTINUE: EmitLine("continue;"); break;
        case NkSLNodeKind::STMT_DISCARD:  EmitLine("discard;");  break;
        case NkSLNodeKind::STMT_SWITCH: {
            EmitLine("switch (" + GenExpr(node->children[0]) + ")");
            EmitLine("{");
            for (uint32 i = 1; i < (uint32)node->children.Size(); i++)
                GenStmt(node->children[i]);
            EmitLine("}");
            break;
        }
        case NkSLNodeKind::STMT_CASE: {
            if (!node->children.Empty())
                EmitLine("case " + GenExpr(node->children[0]) + ":");
            else
                EmitLine("default:");
            break;
        }
        default:
            // Déclaration imbriquée ou expression
            if (node->kind == NkSLNodeKind::STMT_EXPR && !node->children.Empty())
                EmitLine(GenExpr(node->children[0]) + ";");
            break;
    }
}

NkString NkSLCodeGen_GLSL::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            return BuiltinToGLSL(id->name, mStage);
        }
        case NkSLNodeKind::EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString operand = GenExpr(u->operand);
            return u->prefix ? (u->op + operand) : (operand + u->op);
        }
        case NkSLNodeKind::EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            return "(" + GenExpr(b->left) + " " + b->op + " " + GenExpr(b->right) + ")";
        }
        case NkSLNodeKind::EXPR_TERNARY: {
            return "(" + GenExpr(node->children[0]) + " ? " +
                         GenExpr(node->children[1]) + " : " +
                         GenExpr(node->children[2]) + ")";
        }
        case NkSLNodeKind::EXPR_ASSIGN: {
            auto* a = static_cast<NkSLAssignNode*>(node);
            return GenExpr(a->lhs) + " " + a->op + " " + GenExpr(a->rhs);
        }
        case NkSLNodeKind::EXPR_CALL:
            return GenCall(static_cast<NkSLCallNode*>(node));
        case NkSLNodeKind::EXPR_MEMBER: {
            auto* m = static_cast<NkSLMemberNode*>(node);
            return GenExpr(m->object) + "." + m->member;
        }
        case NkSLNodeKind::EXPR_INDEX: {
            auto* idx = static_cast<NkSLIndexNode*>(node);
            return GenExpr(idx->array) + "[" + GenExpr(idx->index) + "]";
        }
        case NkSLNodeKind::EXPR_CAST: {
            auto* c = static_cast<NkSLCastNode*>(node);
            return TypeToString(c->targetType) + "(" + GenExpr(c->expr) + ")";
        }
        case NkSLNodeKind::STMT_EXPR:
            return node->children.Empty() ? "" : GenExpr(node->children[0]);
        default:
            return "/* unknown expr */";
    }
}

NkString NkSLCodeGen_GLSL::GenCall(NkSLCallNode* call) {
    // GLSL : appels identiques au NkSL sauf quelques intrinsèques
    NkString callee = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    NkString args;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
        if (i > 0) args += ", ";
        args += GenExpr(call->args[i]);
    }
    return callee + "(" + args + ")";
}


// =============================================================================
// LiteralToStr — implémentation partageable (utilisée par GenExpr)
// =============================================================================
NkString NkSLCodeGen_GLSL::LiteralToStr(NkSLLiteralNode* lit) {
    char buf[64];
    switch (lit->baseType) {
        case NkSLBaseType::INT:
            snprintf(buf, sizeof(buf), "%lld", (long long)lit->intVal);
            return buf;
        case NkSLBaseType::UINT:
            snprintf(buf, sizeof(buf), "%lluu", (unsigned long long)lit->uintVal);
            return buf;
        case NkSLBaseType::FLOAT: {
            snprintf(buf, sizeof(buf), "%.8g", lit->floatVal);
            bool hasDot = false;
            for (int i = 0; buf[i]; i++) if (buf[i]=='.'||buf[i]=='e'||buf[i]=='E') hasDot=true;
            NkString s(buf); if (!hasDot) s += ".0";
            return s;
        }
        case NkSLBaseType::DOUBLE:
            snprintf(buf, sizeof(buf), "%.16glf", lit->floatVal);
            return buf;
        case NkSLBaseType::BOOL:
            return lit->boolVal ? "true" : "false";
        default:
            return "0";
    }
}

// NkSLBaseTypeName / NkSLTypeName — libres globaux déclarés dans NkSLCodeGen.h
const char* NkSLBaseTypeName(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::VOID:   return "void";
        case NkSLBaseType::BOOL:   return "bool";
        case NkSLBaseType::INT:    return "int";    case NkSLBaseType::IVEC2: return "ivec2";
        case NkSLBaseType::IVEC3:  return "ivec3";  case NkSLBaseType::IVEC4: return "ivec4";
        case NkSLBaseType::UINT:   return "uint";   case NkSLBaseType::UVEC2: return "uvec2";
        case NkSLBaseType::UVEC3:  return "uvec3";  case NkSLBaseType::UVEC4: return "uvec4";
        case NkSLBaseType::FLOAT:  return "float";  case NkSLBaseType::VEC2:  return "vec2";
        case NkSLBaseType::VEC3:   return "vec3";   case NkSLBaseType::VEC4:  return "vec4";
        case NkSLBaseType::DOUBLE: return "double"; case NkSLBaseType::DVEC2: return "dvec2";
        case NkSLBaseType::DVEC3:  return "dvec3";  case NkSLBaseType::DVEC4: return "dvec4";
        case NkSLBaseType::MAT2:   return "mat2";   case NkSLBaseType::MAT3:  return "mat3";
        case NkSLBaseType::MAT4:   return "mat4";
        case NkSLBaseType::MAT2X3: return "mat2x3"; case NkSLBaseType::MAT2X4: return "mat2x4";
        case NkSLBaseType::MAT3X2: return "mat3x2"; case NkSLBaseType::MAT3X4: return "mat3x4";
        case NkSLBaseType::MAT4X2: return "mat4x2"; case NkSLBaseType::MAT4X3: return "mat4x3";
        case NkSLBaseType::DMAT2:  return "dmat2";  case NkSLBaseType::DMAT3:  return "dmat3";
        case NkSLBaseType::DMAT4:  return "dmat4";
        case NkSLBaseType::SAMPLER2D:             return "sampler2D";
        case NkSLBaseType::SAMPLER2D_SHADOW:      return "sampler2DShadow";
        case NkSLBaseType::SAMPLER2D_ARRAY:       return "sampler2DArray";
        case NkSLBaseType::SAMPLER2D_ARRAY_SHADOW:return "sampler2DArrayShadow";
        case NkSLBaseType::SAMPLER_CUBE:          return "samplerCube";
        case NkSLBaseType::SAMPLER_CUBE_SHADOW:   return "samplerCubeShadow";
        case NkSLBaseType::SAMPLER3D:             return "sampler3D";
        case NkSLBaseType::ISAMPLER2D:            return "isampler2D";
        case NkSLBaseType::USAMPLER2D:            return "usampler2D";
        case NkSLBaseType::IMAGE2D:               return "image2D";
        case NkSLBaseType::IIMAGE2D:              return "iimage2D";
        case NkSLBaseType::UIMAGE2D:              return "uimage2D";
        default: return "float";
    }
}

NkString NkSLTypeName(NkSLBaseType t) { return NkString(NkSLBaseTypeName(t)); }

} // namespace nkentseu