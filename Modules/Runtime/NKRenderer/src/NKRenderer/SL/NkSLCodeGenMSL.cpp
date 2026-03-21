// =============================================================================
// NkSLCodeGen_MSL.cpp
// Génération MSL 2.0+ depuis l'AST NkSL.
//
// Différences majeures GLSL→MSL :
//   - #include <metal_stdlib> + using namespace metal;
//   - Les in/out deviennent des structs avec [[attribute(N)]] / [[position]] etc.
//   - Les uniforms deviennent des constant buffers (constant T& name [[buffer(N)]])
//   - Les samplers et textures sont séparés (texture2d<float> + sampler)
//   - vec4 → float4, mat4 → float4x4 (même types SIMD qu'HLSL)
//   - dot(), cross(), normalize() → identiques
//   - discard_fragment() au lieu de discard
// =============================================================================
#include "NkSLCodeGen.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
NkString NkSLCodeGen_MSL::BaseTypeToMSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::VOID:   return "void";
        case NkSLBaseType::BOOL:   return "bool";
        case NkSLBaseType::INT:    return "int";    case NkSLBaseType::IVEC2: return "int2";
        case NkSLBaseType::IVEC3:  return "int3";   case NkSLBaseType::IVEC4: return "int4";
        case NkSLBaseType::UINT:   return "uint";   case NkSLBaseType::UVEC2: return "uint2";
        case NkSLBaseType::UVEC3:  return "uint3";  case NkSLBaseType::UVEC4: return "uint4";
        case NkSLBaseType::FLOAT:  return "float";  case NkSLBaseType::VEC2:  return "float2";
        case NkSLBaseType::VEC3:   return "float3"; case NkSLBaseType::VEC4:  return "float4";
        case NkSLBaseType::DOUBLE: return "float";  // Metal ne supporte pas double sur GPU
        case NkSLBaseType::DVEC2:  return "float2"; case NkSLBaseType::DVEC3: return "float3";
        case NkSLBaseType::DVEC4:  return "float4";
        case NkSLBaseType::MAT2:   return "float2x2";
        case NkSLBaseType::MAT3:   return "float3x3";
        case NkSLBaseType::MAT4:   return "float4x4";
        case NkSLBaseType::MAT2X3: return "float2x3"; case NkSLBaseType::MAT2X4: return "float2x4";
        case NkSLBaseType::MAT3X2: return "float3x2"; case NkSLBaseType::MAT3X4: return "float3x4";
        case NkSLBaseType::MAT4X2: return "float4x2"; case NkSLBaseType::MAT4X3: return "float4x3";
        // Samplers Metal (texture + sampler séparés)
        case NkSLBaseType::SAMPLER2D:            return "texture2d<float>";
        case NkSLBaseType::SAMPLER2D_SHADOW:     return "depth2d<float>";
        case NkSLBaseType::SAMPLER2D_ARRAY:      return "texture2d_array<float>";
        case NkSLBaseType::SAMPLER2D_ARRAY_SHADOW:return "depth2d_array<float>";
        case NkSLBaseType::SAMPLER_CUBE:         return "texturecube<float>";
        case NkSLBaseType::SAMPLER_CUBE_SHADOW:  return "depthcube<float>";
        case NkSLBaseType::SAMPLER3D:            return "texture3d<float>";
        case NkSLBaseType::ISAMPLER2D:           return "texture2d<int>";
        case NkSLBaseType::USAMPLER2D:           return "texture2d<uint>";
        case NkSLBaseType::IMAGE2D:              return "texture2d<float, access::read_write>";
        default: return "float";
    }
}

NkString NkSLCodeGen_MSL::TypeToMSL(NkSLTypeNode* t) {
    if (!t) return "void";
    if (t->baseType == NkSLBaseType::STRUCT) return t->typeName;
    NkString s = BaseTypeToMSL(t->baseType);
    if (t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        s = "array<" + s + ", " + NkString(buf + 1, t->arraySize) + ">";
    }
    return s;
}

NkString NkSLCodeGen_MSL::BuiltinToMSL(const NkString& name, NkSLStage stage) {
    if (name == "gl_Position")          return "out.position";
    if (name == "gl_FragCoord")         return "in.position";
    if (name == "gl_FragDepth")         return "out.depth";
    if (name == "gl_VertexID")          return "vertex_id";
    if (name == "gl_InstanceID")        return "instance_id";
    if (name == "gl_FrontFacing")       return "front_facing";
    if (name == "gl_LocalInvocationID") return "thread_position_in_threadgroup";
    if (name == "gl_GlobalInvocationID")return "thread_position_in_grid";
    if (name == "gl_WorkGroupID")       return "threadgroup_position_in_grid";
    return name;
}

NkString NkSLCodeGen_MSL::AddressSpaceFor(NkSLStorageQual s) {
    switch (s) {
        case NkSLStorageQual::UNIFORM:       return "constant";
        case NkSLStorageQual::BUFFER:        return "device";
        case NkSLStorageQual::PUSH_CONSTANT: return "constant";
        case NkSLStorageQual::SHARED:
        case NkSLStorageQual::WORKGROUP:     return "threadgroup";
        default: return "thread";
    }
}

NkString NkSLCodeGen_MSL::IntrinsicToMSL(const NkString& name) {
    if (name == "mix")       return "mix";     // MSL supporte mix() nativement
    if (name == "fract")     return "fract";   // MSL aussi
    if (name == "mod")       return "fmod";
    if (name == "inversesqrt") return "rsqrt";
    if (name == "dFdx")      return "dfdx";
    if (name == "dFdy")      return "dfdy";
    if (name == "discard")   return "discard_fragment()";
    return name;
}

// =============================================================================
NkSLCompileResult NkSLCodeGen_MSL::Generate(NkSLProgramNode* ast,
                                             NkSLStage stage,
                                             const NkSLCompileOptions& opts) {
    mOpts   = &opts;
    mStage  = stage;
    mOutput = "";
    mErrors.Clear();
    CollectDecls(ast);
    GenProgram(ast);

    NkSLCompileResult res;
    res.success = mErrors.Empty();
    res.source  = mOutput;
    res.target  = NkSLTarget::MSL;
    res.stage   = stage;
    res.errors  = mErrors;
    res.warnings= mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

void NkSLCodeGen_MSL::GenProgram(NkSLProgramNode* prog) {
    EmitLine("// Generated by NkSL Shader Compiler");
    EmitLine("// Target: MSL 2.0+");
    EmitLine("#include <metal_stdlib>");
    EmitLine("using namespace metal;");
    EmitNewLine();

    // Structs
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_STRUCT)
            GenStruct(static_cast<NkSLStructDeclNode*>(child));
    }

    // Structs in/out déterministes (implémentées dans NkSLCodeGen_HLSL_Structs.cpp)
    GenInputOutputStructs();

    // Fonctions helper (non-entry)
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_FUNCTION) {
            auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
            if (!fn->isEntry && fn->name != "main")
                GenFunction(fn);
        }
    }

    // Entry point
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_FUNCTION) {
            auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
            if (fn->isEntry || fn->name == "main")
                GenFunction(fn);
        }
    }
}

void NkSLCodeGen_MSL::GenStruct(NkSLStructDeclNode* s) {
    EmitLine("struct " + s->name);
    EmitLine("{");
    IndentPush();
    for (auto* m : s->members)
        EmitLine(TypeToMSL(m->type) + " " + m->name + ";");
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGen_MSL::GenFunction(NkSLFunctionDeclNode* fn) {
    // Entry point Metal
    if (fn->isEntry || fn->name == "main") {
        // Construction de la signature d'entry point (implémentée dans NkSLCodeGen_HLSL_Structs.cpp)
        NkString outType = (mStage == NkSLStage::VERTEX)  ? "VertexOut" :
                           (mStage == NkSLStage::FRAGMENT) ? "FragOut"   : "void";
        NkString sig = BuildEntryPointSignature(fn);
        EmitLine(sig);
        EmitLine("{");
        IndentPush();
        EmitLine(outType + " out;");
        if (fn->body) {
            for (auto* s : fn->body->children)
                GenStmt(s);
        }
        if (mStage != NkSLStage::COMPUTE) EmitLine("return out;");
        IndentPop();
        EmitLine("}");
        EmitNewLine();
        return;
    }

    // Fonction régulière
    NkString sig = TypeToMSL(fn->returnType) + " " + fn->name + "(";
    for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
        auto* p = fn->params[i];
        if (i > 0) sig += ", ";
        if (p->storage == NkSLStorageQual::INOUT) sig += "thread ";
        sig += TypeToMSL(p->type);
        if (p->storage == NkSLStorageQual::OUT || p->storage == NkSLStorageQual::INOUT) sig += "&";
        if (!p->name.Empty()) sig += " " + p->name;
    }
    sig += ")";
    if (!fn->body) { EmitLine(sig + ";"); return; }
    EmitLine(sig);
    GenStmt(fn->body);
    EmitNewLine();
}

void NkSLCodeGen_MSL::GenStmt(NkSLNode* node) {
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
        case NkSLNodeKind::STMT_EXPR:
            if (!node->children.Empty()) EmitLine(GenExpr(node->children[0]) + ";");
            break;
        case NkSLNodeKind::DECL_VAR: {
            auto* v = static_cast<NkSLVarDeclNode*>(node);
            NkString line = (v->isConst ? "const " : "") + TypeToMSL(v->type) + " " + v->name;
            if (v->initializer) line += " = " + GenExpr(v->initializer);
            EmitLine(line + ";");
            break;
        }
        case NkSLNodeKind::STMT_IF: {
            auto* n = static_cast<NkSLIfNode*>(node);
            EmitLine("if (" + GenExpr(n->condition) + ")");
            GenStmt(n->thenBranch);
            if (n->elseBranch) { EmitLine("else"); GenStmt(n->elseBranch); }
            break;
        }
        case NkSLNodeKind::STMT_FOR: {
            auto* n = static_cast<NkSLForNode*>(node);
            NkString init, cond, inc;
            if (n->init) {
                if (n->init->kind == NkSLNodeKind::DECL_VAR) {
                    auto* vd = static_cast<NkSLVarDeclNode*>(n->init);
                    init = TypeToMSL(vd->type) + " " + vd->name;
                    if (vd->initializer) init += " = " + GenExpr(vd->initializer);
                } else init = GenExpr(n->init);
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
        case NkSLNodeKind::STMT_RETURN: {
            auto* n = static_cast<NkSLReturnNode*>(node);
            if (n->value) EmitLine("return " + GenExpr(n->value) + ";");
            else          EmitLine("return;");
            break;
        }
        case NkSLNodeKind::STMT_BREAK:    EmitLine("break;");                break;
        case NkSLNodeKind::STMT_CONTINUE: EmitLine("continue;");             break;
        case NkSLNodeKind::STMT_DISCARD:  EmitLine("discard_fragment();"); break;
        default: break;
    }
}

NkString NkSLCodeGen_MSL::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            return BuiltinToMSL(id->name, mStage);
        }
        case NkSLNodeKind::EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString op = GenExpr(u->operand);
            return u->prefix ? (u->op + op) : (op + u->op);
        }
        case NkSLNodeKind::EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            return "(" + GenExpr(b->left) + " " + b->op + " " + GenExpr(b->right) + ")";
        }
        case NkSLNodeKind::EXPR_TERNARY:
            return "(" + GenExpr(node->children[0]) + " ? " +
                         GenExpr(node->children[1]) + " : " +
                         GenExpr(node->children[2]) + ")";
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
        case NkSLNodeKind::STMT_EXPR:
            return node->children.Empty() ? "" : GenExpr(node->children[0]);
        default: return "/* unknown */";
    }
}

NkString NkSLCodeGen_MSL::GenCall(NkSLCallNode* call) {
    NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    name = IntrinsicToMSL(name);

    // texture(sampler, uv) → sampler_tex.sample(sampler_smp, uv)
    if (name == "texture" && call->args.Size() >= 2) {
        NkString texName = GenExpr(call->args[0]);
        NkString coord   = GenExpr(call->args[1]);
        return texName + "_tex.sample(" + texName + "_smp, " + coord + ")";
    }

    NkString args;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
        if (i > 0) args += ", ";
        args += GenExpr(call->args[i]);
    }

    // Constructeurs : vec4(x,y,z,w) → float4(x,y,z,w)
    NkString mapped = name;
    if (name == "vec2" || name == "vec3" || name == "vec4") mapped = "float" + name.SubStr(3);
    if (name == "ivec2"|| name == "ivec3"|| name == "ivec4") mapped = "int" + name.SubStr(4);
    if (name == "uvec2"|| name == "uvec3"|| name == "uvec4") mapped = "uint" + name.SubStr(4);
    if (name == "mat4") mapped = "float4x4";
    if (name == "mat3") mapped = "float3x3";
    if (name == "mat2") mapped = "float2x2";

    return mapped + "(" + args + ")";
}


// =============================================================================
// LiteralToStr MSL
// =============================================================================
NkString NkSLCodeGen_MSL::LiteralToStr(NkSLLiteralNode* lit) {
    char buf[64];
    switch (lit->baseType) {
        case NkSLBaseType::INT:  snprintf(buf,sizeof(buf),"%lld",(long long)lit->intVal); return buf;
        case NkSLBaseType::UINT: snprintf(buf,sizeof(buf),"%lluu",(unsigned long long)lit->uintVal); return buf;
        case NkSLBaseType::FLOAT: {
            snprintf(buf,sizeof(buf),"%.8g",lit->floatVal);
            NkString s(buf);
            bool hasDot=false; for(int i=0;buf[i];i++) if(buf[i]=='.'||buf[i]=='e') hasDot=true;
            if (!hasDot) s+=".0";
            return s+"f";
        }
        case NkSLBaseType::BOOL: return lit->boolVal ? "true" : "false";
        default: return "0";
    }
}

// =============================================================================
// CollectDecls MSL
// =============================================================================
void NkSLCodeGen_MSL::CollectDecls(NkSLProgramNode* prog) {
    mInputVars.Clear(); mOutputVars.Clear();
    mUniformVars.Clear(); mBufferDecls.Clear();
    for (auto* child : prog->children) {
        if (!child) continue;
        switch (child->kind) {
            case NkSLNodeKind::DECL_INPUT:
                mInputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_OUTPUT:
                mOutputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_UNIFORM_BLOCK:
            case NkSLNodeKind::DECL_PUSH_CONSTANT:
            case NkSLNodeKind::DECL_STORAGE_BLOCK:
                mBufferDecls.PushBack(static_cast<NkSLBlockDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_VAR: {
                auto* v = static_cast<NkSLVarDeclNode*>(child);
                if (v->storage == NkSLStorageQual::UNIFORM) mUniformVars.PushBack(v);
                break;
            }
            default: break;
        }
    }
}

// =============================================================================
// GenDecl + GenVarDecl MSL
// =============================================================================
void NkSLCodeGen_MSL::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_VAR:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), false); break;
        default: break;
    }
}

void NkSLCodeGen_MSL::GenVarDecl(NkSLVarDeclNode* v, bool inStruct) {
    if (!v || !v->type) return;
    if (!inStruct && !v->isConst) return; // les buffers/uniformes gérés via entry point sig
    NkString line = (v->isConst ? "constant " : "") + TypeToMSL(v->type) + " " + v->name;
    if (v->initializer) line += " = " + GenExpr(v->initializer);
    EmitLine(line + ";");
}

} // namespace nkentseu