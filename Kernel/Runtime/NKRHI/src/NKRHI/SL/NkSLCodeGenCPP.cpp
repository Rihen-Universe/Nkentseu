// =============================================================================
// NkSLCodeGenCPP.cpp
// Génération C++ (software rasterizer) depuis l'AST NkSL.
//
// Produit un fichier .h / .cpp qui expose :
//   NkSWVertexShader_<name>  : struct avec operator()
//   NkSWFragmentShader_<name>: struct avec operator()
//
// Les types GLSL sont mappés vers des types NkMath :
//   vec4 → NkVec4f, mat4 → NkMat4f, etc.
// Les intrinsèques GLSL sont mappées vers des fonctions NkMath.
// =============================================================================
#include "NkSLCodeGen.h"
#include <cstdio>

namespace nkentseu {

    // =============================================================================
    NkString NkSLCodeGenCPP::BaseTypeToCPP(NkSLBaseType t) {
        switch (t) {
            case NkSLBaseType::NK_VOID:   return "void";
            case NkSLBaseType::NK_BOOL:   return "bool";
            case NkSLBaseType::NK_INT:    return "int32";
            case NkSLBaseType::NK_IVEC2:  return "NkVec2i";
            case NkSLBaseType::NK_IVEC3:  return "NkVec3i";
            case NkSLBaseType::NK_IVEC4:  return "NkVec4i";
            case NkSLBaseType::NK_UINT:   return "uint32";
            case NkSLBaseType::NK_UVEC2:  return "NkVec2u";
            case NkSLBaseType::NK_UVEC3:  return "NkVec3u";
            case NkSLBaseType::NK_UVEC4:  return "NkVec4u";
            case NkSLBaseType::NK_FLOAT:  return "float32";
            case NkSLBaseType::NK_VEC2:   return "NkVec2f";
            case NkSLBaseType::NK_VEC3:   return "NkVec3f";
            case NkSLBaseType::NK_VEC4:   return "NkVec4f";
            case NkSLBaseType::NK_DOUBLE: return "float64";
            case NkSLBaseType::NK_DVEC2:  return "NkVec2d";
            case NkSLBaseType::NK_DVEC3:  return "NkVec3d";
            case NkSLBaseType::NK_DVEC4:  return "NkVec4d";
            case NkSLBaseType::NK_MAT2:   return "NkMat2f";
            case NkSLBaseType::NK_MAT3:   return "NkMat3f";
            case NkSLBaseType::NK_MAT4:   return "NkMat4f";
            // Pour les samplers en CPU: pointeur vers une texture software
            case NkSLBaseType::NK_SAMPLER2D:
            case NkSLBaseType::NK_SAMPLER2D_SHADOW:
            case NkSLBaseType::NK_SAMPLER2D_ARRAY:
                return "const NkSWTexture2D*";
            case NkSLBaseType::NK_SAMPLER_CUBE:
                return "const NkSWTextureCube*";
            default: return "float32";
        }
    }

    NkString NkSLCodeGenCPP::TypeToCPP(NkSLTypeNode* t) {
        if (!t) return "void";
        if (t->baseType == NkSLBaseType::NK_STRUCT) return t->typeName;
        NkString s = BaseTypeToCPP(t->baseType);
        if (t->arraySize > 0) {
            char buf[64]; snprintf(buf, sizeof(buf), "std::array<%s, %u>", s.CStr(), t->arraySize);
            return NkString(buf);
        }
        return s;
    }

    NkString NkSLCodeGenCPP::IntrinsicToCPP(const NkString& name) {
        // Mappages GLSL → NkMath / std
        if (name == "dot")         return "NkDot";
        if (name == "cross")       return "NkCross";
        if (name == "normalize")   return "NkNormalize";
        if (name == "length")      return "NkLength";
        if (name == "distance")    return "NkDistance";
        if (name == "reflect")     return "NkReflect";
        if (name == "refract")     return "NkRefract";
        if (name == "mix")         return "NkMix";
        if (name == "clamp")       return "NkClamp";
        if (name == "saturate")    return "NkSaturate";
        if (name == "smoothstep")  return "NkSmoothstep";
        if (name == "step")        return "NkStep";
        if (name == "pow")         return "NkPow";
        if (name == "sqrt")        return "NkSqrt";
        if (name == "inversesqrt") return "NkInverseSqrt";
        if (name == "abs")         return "NkAbs";
        if (name == "sign")        return "NkSign";
        if (name == "floor")       return "NkFloor";
        if (name == "ceil")        return "NkCeil";
        if (name == "round")       return "NkRound";
        if (name == "fract")       return "NkFract";
        if (name == "mod")         return "NkMod";
        if (name == "min")         return "NkMin";
        if (name == "max")         return "NkMax";
        if (name == "sin")         return "NkSin";
        if (name == "cos")         return "NkCos";
        if (name == "tan")         return "NkTan";
        if (name == "asin")        return "NkAsin";
        if (name == "acos")        return "NkAcos";
        if (name == "atan")        return "NkAtan";
        if (name == "atan2")       return "NkAtan2";
        if (name == "exp")         return "NkExp";
        if (name == "exp2")        return "NkExp2";
        if (name == "log")         return "NkLog";
        if (name == "log2")        return "NkLog2";
        if (name == "radians")     return "NkToRadians";
        if (name == "degrees")     return "NkToDegrees";
        if (name == "transpose")   return "NkTranspose";
        if (name == "inverse")     return "NkInverse";
        if (name == "determinant") return "NkDeterminant";
        // Texture sampling (implémenté via NkSWTexture2D)
        if (name == "texture")     return "NkSWTexture_Sample";
        if (name == "textureLod")  return "NkSWTexture_SampleLod";
        // Constructeurs vec
        if (name == "vec2")  return "NkVec2f";
        if (name == "vec3")  return "NkVec3f";
        if (name == "vec4")  return "NkVec4f";
        if (name == "ivec2") return "NkVec2i";
        if (name == "ivec3") return "NkVec3i";
        if (name == "ivec4") return "NkVec4i";
        if (name == "mat3")  return "NkMat3f";
        if (name == "mat4")  return "NkMat4f";
        return name;
    }

    // =============================================================================
    NkSLCompileResult NkSLCodeGenCPP::Generate(NkSLProgramNode* ast,
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
        res.target  = NkSLTarget::NK_CPLUSPLUS;
        res.stage   = stage;
        res.errors  = mErrors;
        res.warnings= mWarnings;
        for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
            res.bytecode.PushBack((uint8)res.source[i]);
        return res;
    }

    void NkSLCodeGenCPP::GenProgram(NkSLProgramNode* prog) {
        EmitLine("// Generated by NkSL Shader Compiler");
        EmitLine("// Target: C++ Software Rasterizer");
        EmitLine("#pragma once");
        EmitLine("#include \"NKMath/NKMath.h\"");
        EmitLine("#include \"NKRHI/Software/NkSoftwareDevice.h\"");
        EmitNewLine();
        EmitLine("namespace nkentseu {");
        EmitNewLine();

        // Structs utilisateur
        for (auto* child : prog->children) {
            if (child && child->kind == NkSLNodeKind::NK_DECL_STRUCT) {
                EmitLine("struct " + static_cast<NkSLStructDeclNode*>(child)->name);
                EmitLine("{");
                IndentPush();
                for (auto* m : static_cast<NkSLStructDeclNode*>(child)->members)
                    EmitLine(TypeToCPP(m->type) + " " + m->name + ";");
                IndentPop();
                EmitLine("};");
                EmitNewLine();
            }
        }

        // Variables globales uniformes → membres d'une struct UBO
        bool hasUniforms = false;
        for (auto* child : prog->children) {
            if (!child) continue;
            if (child->kind == NkSLNodeKind::NK_DECL_UNIFORM_BLOCK ||
                child->kind == NkSLNodeKind::NK_DECL_PUSH_CONSTANT) {
                if (!hasUniforms) {
                    EmitLine("// Uniform data (matches CPU-side struct layout)");
                    hasUniforms = true;
                }
                auto* b = static_cast<NkSLBlockDeclNode*>(child);
                EmitLine("struct " + b->blockName + "_CPU {");
                IndentPush();
                for (auto* m : b->members)
                    EmitLine(TypeToCPP(m->type) + " " + m->name + ";");
                IndentPop();
                EmitLine("};");
                EmitNewLine();
            }
        }

        // Fonctions helper
        for (auto* child : prog->children) {
            if (child && child->kind == NkSLNodeKind::NK_DECL_FUNCTION) {
                auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
                if (!fn->isEntry && fn->name != "main")
                    GenFunction(fn, false);
            }
        }

        // Entry points comme lambdas / fonctions statiques
        for (auto* child : prog->children) {
            if (child && child->kind == NkSLNodeKind::NK_DECL_FUNCTION) {
                auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
                if (fn->isEntry || fn->name == "main")
                    GenFunction(fn, true);
            }
        }

        EmitNewLine();
        EmitLine("} // namespace nkentseu");
    }

    void NkSLCodeGenCPP::GenFunction(NkSLFunctionDeclNode* fn, bool asEntry) {
        if (asEntry) {
            // Entry point : génère une fonction statique
            NkString stageName = (mStage == NkSLStage::NK_VERTEX)  ? "vertex"   :
                                (mStage == NkSLStage::NK_FRAGMENT) ? "fragment" : "compute";
            EmitLine("// Entry point: " + stageName);
            EmitLine("static inline void NkSW_" + fn->name + "_entry(");
            IndentPush();
            EmitLine("const void* vertexData,");
            EmitLine("uint32_t vertexIndex,");
            EmitLine("const void* uniformData,");
            EmitLine("NkSWVertex& output");
            IndentPop();
            EmitLine(") {");
            IndentPush();
            EmitLine("// Cast des données d'entrée");
            EmitLine("(void)vertexData; (void)vertexIndex; (void)uniformData;");
            if (fn->body) {
                for (auto* s : fn->body->children) GenStmt(s);
            }
            IndentPop();
            EmitLine("}");
            EmitNewLine();
            return;
        }

        // Fonction helper régulière
        NkString sig = TypeToCPP(fn->returnType) + " " + fn->name + "(";
        for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
            auto* p = fn->params[i];
            if (i > 0) sig += ", ";
            if (p->storage == NkSLStorageQual::NK_OUT || p->storage == NkSLStorageQual::NK_INOUT) {
                sig += TypeToCPP(p->type) + "& " + p->name;
            } else if (NkSLTypeIsMatrix(p->type->baseType) || p->type->baseType == NkSLBaseType::NK_STRUCT) {
                sig += "const " + TypeToCPP(p->type) + "& " + p->name;
            } else {
                sig += TypeToCPP(p->type) + " " + p->name;
            }
        }
        sig += ")";

        if (!fn->body) { EmitLine("inline " + sig + ";"); return; }
        EmitLine("inline " + sig);
        GenStmt(fn->body);
        EmitNewLine();
    }

    void NkSLCodeGenCPP::GenStmt(NkSLNode* node) {
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
                if (!node->children.Empty()) EmitLine(GenExpr(node->children[0]) + ";");
                break;
            case NkSLNodeKind::NK_DECL_VAR: {
                auto* v = static_cast<NkSLVarDeclNode*>(node);
                NkString line = (v->isConst ? "const " : "") + TypeToCPP(v->type) + " " + v->name;
                if (v->initializer) line += " = " + GenExpr(v->initializer);
                EmitLine(line + ";");
                break;
            }
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
                        init = TypeToCPP(vd->type) + " " + vd->name;
                        if (vd->initializer) init += " = " + GenExpr(vd->initializer);
                    } else init = GenExpr(n->init);
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
            case NkSLNodeKind::NK_STMT_RETURN: {
                auto* n = static_cast<NkSLReturnNode*>(node);
                if (n->value) EmitLine("return " + GenExpr(n->value) + ";");
                else          EmitLine("return;");
                break;
            }
            case NkSLNodeKind::NK_STMT_BREAK:    EmitLine("break;");    break;
            case NkSLNodeKind::NK_STMT_CONTINUE: EmitLine("continue;"); break;
            case NkSLNodeKind::NK_STMT_DISCARD:
                EmitLine("return; // discard");
                break;
            default: break;
        }
    }

    NkString NkSLCodeGenCPP::GenExpr(NkSLNode* node) {
        if (!node) return "";
        switch (node->kind) {
            case NkSLNodeKind::NK_EXPR_LITERAL:
                return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
            case NkSLNodeKind::NK_EXPR_IDENT: {
                auto* id = static_cast<NkSLIdentNode*>(node);
                // Builtins GL → équivalents software
                if (id->name == "gl_Position")    return "output.position";
                if (id->name == "gl_FragCoord")   return "input.fragCoord";
                if (id->name == "gl_FragDepth")   return "output.depth";
                if (id->name == "gl_VertexID")    return "vertexIndex";
                return id->name;
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
            case NkSLNodeKind::NK_STMT_EXPR:
                return node->children.Empty() ? "" : GenExpr(node->children[0]);
            default: return "/* unknown */";
        }
    }

    NkString NkSLCodeGenCPP::GenCall(NkSLCallNode* call) {
        NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
        name = IntrinsicToCPP(name);

        NkString args;
        for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
            if (i > 0) args += ", ";
            args += GenExpr(call->args[i]);
        }

        // texture(sampler, uv) → NkSWTexture_Sample(sampler, uv)
        if (name == "NkSWTexture_Sample" && call->args.Size() >= 2) {
            return "NkSWTexture_Sample(" + GenExpr(call->args[0]) + ", " +
                GenExpr(call->args[1]) + ")";
        }

        return name + "(" + args + ")";
    }


    // =============================================================================
    // LiteralToStr C++
    // =============================================================================
    NkString NkSLCodeGenCPP::LiteralToStr(NkSLLiteralNode* lit) {
        char buf[64];
        switch (lit->baseType) {
            case NkSLBaseType::NK_INT:    snprintf(buf,sizeof(buf),"%lld",(long long)lit->intVal); return buf;
            case NkSLBaseType::NK_UINT:   snprintf(buf,sizeof(buf),"%lluu",(unsigned long long)lit->uintVal); return buf;
            case NkSLBaseType::NK_FLOAT:  snprintf(buf,sizeof(buf),"%.8gf",lit->floatVal); return buf;
            case NkSLBaseType::NK_DOUBLE: snprintf(buf,sizeof(buf),"%.16g",lit->floatVal); return buf;
            case NkSLBaseType::NK_BOOL:   return lit->boolVal ? "true" : "false";
            default: return "0";
        }
    }

} // namespace nkentseu