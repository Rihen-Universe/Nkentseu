// =============================================================================
// NkSLCodeGen_HLSL.cpp
// Génération HLSL SM5+ depuis l'AST NkSL.
//
// Différences majeures GLSL→HLSL :
//   - Les in/out deviennent des structs avec semantics
//   - Les uniform blocks deviennent des cbuffer
//   - Les samplers et textures sont séparés
//   - mat4 → float4x4 (row-major vs column-major !)
//   - mul() pour la multiplication matricielle
//   - saturate(), lerp() etc. au lieu de clamp(), mix()
//   - SV_Position, SV_Target, SV_Depth etc.
// =============================================================================
#include "NkSLCodeGen.h"
#include "NKContainers/String/NkFormat.h"
#include <cstdio>

namespace nkentseu {

// =============================================================================
NkString NkSLCodeGen_HLSL::BaseTypeToHLSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::VOID:   return "void";
        case NkSLBaseType::BOOL:   return "bool";
        case NkSLBaseType::INT:    return "int";    case NkSLBaseType::IVEC2: return "int2";
        case NkSLBaseType::IVEC3:  return "int3";   case NkSLBaseType::IVEC4: return "int4";
        case NkSLBaseType::UINT:   return "uint";   case NkSLBaseType::UVEC2: return "uint2";
        case NkSLBaseType::UVEC3:  return "uint3";  case NkSLBaseType::UVEC4: return "uint4";
        case NkSLBaseType::FLOAT:  return "float";  case NkSLBaseType::VEC2:  return "float2";
        case NkSLBaseType::VEC3:   return "float3"; case NkSLBaseType::VEC4:  return "float4";
        case NkSLBaseType::DOUBLE: return "double"; case NkSLBaseType::DVEC2: return "double2";
        case NkSLBaseType::DVEC3:  return "double3";case NkSLBaseType::DVEC4: return "double4";
        // HLSL : les matrices sont row-major par défaut mais pour rester compatible
        // avec le code C++ column-major on utilise column_major explicitement
        case NkSLBaseType::MAT2:   return "column_major float2x2";
        case NkSLBaseType::MAT3:   return "column_major float3x3";
        case NkSLBaseType::MAT4:   return "column_major float4x4";
        case NkSLBaseType::MAT2X3: return "column_major float2x3";
        case NkSLBaseType::MAT2X4: return "column_major float2x4";
        case NkSLBaseType::MAT3X2: return "column_major float3x2";
        case NkSLBaseType::MAT3X4: return "column_major float3x4";
        case NkSLBaseType::MAT4X2: return "column_major float4x2";
        case NkSLBaseType::MAT4X3: return "column_major float4x3";
        // Samplers HLSL (texture + sampler séparés ou combinés via SamplerState)
        case NkSLBaseType::SAMPLER2D:            return "Texture2D";
        case NkSLBaseType::SAMPLER2D_SHADOW:     return "Texture2D";   // avec SamplerComparisonState
        case NkSLBaseType::SAMPLER2D_ARRAY:      return "Texture2DArray";
        case NkSLBaseType::SAMPLER2D_ARRAY_SHADOW:return "Texture2DArray";
        case NkSLBaseType::SAMPLER_CUBE:         return "TextureCube";
        case NkSLBaseType::SAMPLER_CUBE_SHADOW:  return "TextureCube";
        case NkSLBaseType::SAMPLER3D:            return "Texture3D";
        case NkSLBaseType::ISAMPLER2D:           return "Texture2D<int4>";
        case NkSLBaseType::USAMPLER2D:           return "Texture2D<uint4>";
        case NkSLBaseType::IMAGE2D:              return "RWTexture2D<float4>";
        case NkSLBaseType::IIMAGE2D:             return "RWTexture2D<int4>";
        case NkSLBaseType::UIMAGE2D:             return "RWTexture2D<uint4>";
        default: return "float";
    }
}

NkString NkSLCodeGen_HLSL::TypeToHLSL(NkSLTypeNode* t) {
    if (!t) return "void";
    if (t->baseType == NkSLBaseType::STRUCT) return t->typeName;
    NkString s = BaseTypeToHLSL(t->baseType);
    if (t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        s += NkString(buf);
    }
    return s;
}

NkString NkSLCodeGen_HLSL::BuiltinToHLSL(const NkString& name, NkSLStage stage) {
    if (name == "gl_Position")    return "output.Position";
    if (name == "gl_FragCoord")   return "input.Position";
    if (name == "gl_FragDepth")   return "output.Depth";
    if (name == "gl_VertexID")    return "input.VertexID";
    if (name == "gl_InstanceID")  return "input.InstanceID";
    if (name == "gl_FrontFacing") return "input.IsFrontFace";
    if (name == "gl_LocalInvocationID")  return "GroupThreadID";
    if (name == "gl_GlobalInvocationID") return "DispatchThreadID";
    if (name == "gl_WorkGroupID")        return "GroupID";
    return name;
}

NkString NkSLCodeGen_HLSL::SemanticFor(NkSLVarDeclNode* v, NkSLStage stage, bool isInput) {
    // Si une location est spécifiée, utiliser TEXCOORD<N>
    if (v->binding.HasLocation()) {
        char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", v->binding.location);
        return NkString(buf);
    }
    // Heuristiques par nom
    if (v->name.ToLower().Contains("position") || v->name.ToLower().Contains("pos"))
        return isInput ? "POSITION" : "SV_Position";
    if (v->name.ToLower().Contains("normal") || v->name.ToLower().Contains("nrm"))
        return "NORMAL";
    if (v->name.ToLower().Contains("color") || v->name.ToLower().Contains("col"))
        return "COLOR";
    if (v->name.ToLower().Contains("uv") || v->name.ToLower().Contains("texcoord"))
        return "TEXCOORD0";
    // Défaut
    static int sSemanticIdx = 0;
    char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", sSemanticIdx++);
    return NkString(buf);
}

NkString NkSLCodeGen_HLSL::IntrinsicToHLSL(const NkString& name) {
    // Correspondances GLSL → HLSL
    if (name == "mix")       return "lerp";
    if (name == "fract")     return "frac";
    if (name == "mod")       return "fmod";
    if (name == "inversesqrt") return "rsqrt";
    if (name == "dFdx")      return "ddx";
    if (name == "dFdy")      return "ddy";
    if (name == "atan" && false) return "atan2"; // atan(y,x) → atan2(y,x)
    // Multiplication matricielle : en HLSL mul(m,v) au lieu de m*v
    // (géré dans GenCall)
    return name;
}

// =============================================================================
NkSLCompileResult NkSLCodeGen_HLSL::Generate(NkSLProgramNode* ast,
                                              NkSLStage stage,
                                              const NkSLCompileOptions& opts) {
    mOpts   = &opts;
    mStage  = stage;
    mOutput = "";
    mErrors.Clear();
    // Passe 1 : collecter in/out/uniforms/buffers
    CollectDecls(ast);

    // Passe 2 : générer
    GenProgram(ast);

    NkSLCompileResult res;
    res.success = mErrors.Empty();
    res.source  = mOutput;
    res.target  = NkSLTarget::HLSL;
    res.stage   = stage;
    res.errors  = mErrors;
    res.warnings= mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

void NkSLCodeGen_HLSL::GenProgram(NkSLProgramNode* prog) {
    EmitLine("// Generated by NkSL Shader Compiler");
    EmitLine(NkFormat("// Target: HLSL SM{0}", mOpts->hlslShaderModel));
    EmitNewLine();

    // Structs
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_STRUCT)
            GenStruct(static_cast<NkSLStructDeclNode*>(child));
    }

    // cbuffer
    for (auto* cb : mCBuffers) GenCBuffer(cb);

    // Storage buffers
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_STORAGE_BLOCK)
            GenSBuffer(static_cast<NkSLBlockDeclNode*>(child));
    }

    // Textures et samplers (pour les uniformes sampler2D etc.)
    int texReg = 0, sampReg = 0;
    for (auto* v : mUniforms) {
        if (!v->type) continue;
        if (NkSLTypeIsSampler(v->type->baseType)) {
            int b = v->binding.HasBinding() ? v->binding.binding : texReg++;
            char buf[128];
            // Texture
            snprintf(buf, sizeof(buf), "%s %s_tex : register(t%d);",
                     BaseTypeToHLSL(v->type->baseType).CStr(), v->name.CStr(), b);
            EmitLine(NkString(buf));
            // Sampler correspondant
            if (v->type->baseType == NkSLBaseType::SAMPLER2D_SHADOW ||
                v->type->baseType == NkSLBaseType::SAMPLER2D_ARRAY_SHADOW ||
                v->type->baseType == NkSLBaseType::SAMPLER_CUBE_SHADOW) {
                snprintf(buf, sizeof(buf), "SamplerComparisonState %s_smp : register(s%d);",
                         v->name.CStr(), b);
            } else {
                snprintf(buf, sizeof(buf), "SamplerState %s_smp : register(s%d);",
                         v->name.CStr(), b);
            }
            EmitLine(NkString(buf));
        }
    }
    if (!mUniforms.Empty()) EmitNewLine();

    // Génération déterministe des structs in/out (implémentée dans NkSLCodeGen_HLSL_Structs.cpp)
    GenInputOutputStructs();

    // Fonctions
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::DECL_FUNCTION)
            GenFunction(static_cast<NkSLFunctionDeclNode*>(child));
    }
}

void NkSLCodeGen_HLSL::GenCBuffer(NkSLBlockDeclNode* b) {
    int reg = b->binding.HasBinding() ? b->binding.binding : 0;
    char buf[64]; snprintf(buf, sizeof(buf), "cbuffer %s : register(b%d)",
                           b->blockName.CStr(), reg);
    EmitLine(NkString(buf));
    EmitLine("{");
    IndentPush();
    for (auto* m : b->members) {
        EmitLine(TypeToHLSL(m->type) + " " + m->name + ";");
    }
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGen_HLSL::GenSBuffer(NkSLBlockDeclNode* b) {
    int reg = b->binding.HasBinding() ? b->binding.binding : 0;
    char buf[64]; snprintf(buf, sizeof(buf), "RWStructuredBuffer<%s> %s : register(u%d);",
                           b->blockName.CStr(),
                           b->instanceName.Empty() ? b->blockName.CStr() : b->instanceName.CStr(),
                           reg);
    EmitLine(NkString(buf));
    EmitNewLine();
}

void NkSLCodeGen_HLSL::GenStruct(NkSLStructDeclNode* s) {
    EmitLine("struct " + s->name);
    EmitLine("{");
    IndentPush();
    for (auto* m : s->members) {
        EmitLine(TypeToHLSL(m->type) + " " + m->name + ";");
    }
    IndentPop();
    EmitLine("};");
    EmitNewLine();
}

void NkSLCodeGen_HLSL::GenFunction(NkSLFunctionDeclNode* fn) {
    // Entry point : signature spéciale
    if (fn->isEntry || fn->name == "main") {
        NkString retType = (mStage == NkSLStage::VERTEX || mStage == NkSLStage::FRAGMENT)
            ? mOutputStructName : "void";
        NkString sig = retType + " main(";
        if (!mInputVars.Empty()) sig += mInputStructName + " input";
        if (mStage == NkSLStage::COMPUTE) {
            if (!mInputVars.Empty()) sig += ", ";
            sig += "uint3 GroupThreadID : SV_GroupThreadID, ";
            sig += "uint3 DispatchThreadID : SV_DispatchThreadID, ";
            sig += "uint3 GroupID : SV_GroupID";
        }
        sig += ")";
        EmitLine(sig);
        EmitLine("{");
        IndentPush();
        // Déclarer la struct de sortie
        if (mStage == NkSLStage::VERTEX || mStage == NkSLStage::FRAGMENT)
            EmitLine(mOutputStructName + " output;");
        if (fn->body)
            for (auto* s : fn->body->children)
                GenStmt(s);
        if (mStage == NkSLStage::VERTEX || mStage == NkSLStage::FRAGMENT)
            EmitLine("return output;");
        IndentPop();
        EmitLine("}");
        EmitNewLine();
        return;
    }

    // Fonction régulière
    NkString sig = TypeToHLSL(fn->returnType) + " " + fn->name + "(";
    for (uint32 i = 0; i < (uint32)fn->params.Size(); i++) {
        auto* p = fn->params[i];
        if (i > 0) sig += ", ";
        switch (p->storage) {
            case NkSLStorageQual::IN:    sig += "in ";    break;
            case NkSLStorageQual::OUT:   sig += "out ";   break;
            case NkSLStorageQual::INOUT: sig += "inout "; break;
            default: break;
        }
        sig += TypeToHLSL(p->type);
        if (!p->name.Empty()) sig += " " + p->name;
    }
    sig += ")";

    if (!fn->body) { EmitLine(sig + ";"); EmitNewLine(); return; }
    EmitLine(sig);
    GenStmt(fn->body);
    EmitNewLine();
}

void NkSLCodeGen_HLSL::GenStmt(NkSLNode* node) {
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
            NkString line = (v->isConst ? "const " : "") + TypeToHLSL(v->type) + " " + v->name;
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
                    init = TypeToHLSL(vd->type) + " " + vd->name;
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
        case NkSLNodeKind::STMT_DO_WHILE: {
            EmitLine("do");
            GenStmt(node->children[0]);
            EmitLine("while (" + GenExpr(node->children[1]) + ");");
            break;
        }
        case NkSLNodeKind::STMT_RETURN: {
            auto* n = static_cast<NkSLReturnNode*>(node);
            if (n->value) EmitLine("return " + GenExpr(n->value) + ";");
            else          EmitLine("return;");
            break;
        }
        case NkSLNodeKind::STMT_BREAK:    EmitLine("break;");    break;
        case NkSLNodeKind::STMT_CONTINUE: EmitLine("continue;"); break;
        case NkSLNodeKind::STMT_DISCARD:  EmitLine("discard;");  break;
        default: break;
    }
}

NkString NkSLCodeGen_HLSL::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            return BuiltinToHLSL(id->name, mStage);
        }
        case NkSLNodeKind::EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString op = GenExpr(u->operand);
            return u->prefix ? (u->op + op) : (op + u->op);
        }
        case NkSLNodeKind::EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            // La multiplication de matrices en HLSL nécessite mul()
            // On détecte les cas courants via les types (simplifié ici)
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

NkString NkSLCodeGen_HLSL::GenCall(NkSLCallNode* call) {
    NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    name = IntrinsicToHLSL(name);

    NkString args;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
        if (i > 0) args += ", ";
        args += GenExpr(call->args[i]);
    }

    // texture() → tex.Sample(smp, uv)
    // texture(sampler, uv) → sampler_tex.Sample(sampler_smp, uv)
    if (name == "texture" && call->args.Size() >= 2) {
        NkString texName = GenExpr(call->args[0]);
        NkString coord   = GenExpr(call->args[1]);
        return texName + "_tex.Sample(" + texName + "_smp, " + coord + ")";
    }
    if (name == "textureSize" && call->args.Size() >= 1) {
        NkString texName = GenExpr(call->args[0]);
        return "((int2)" + texName + "_tex.GetDimensions(0))";
    }
    // mix → lerp
    if (name == "mix" && call->args.Size() == 3) {
        NkString a = GenExpr(call->args[0]);
        NkString b = GenExpr(call->args[1]);
        NkString t = GenExpr(call->args[2]);
        return "lerp(" + a + ", " + b + ", " + t + ")";
    }

    return name + "(" + args + ")";
}


// =============================================================================
// LiteralToStr HLSL
// =============================================================================
NkString NkSLCodeGen_HLSL::LiteralToStr(NkSLLiteralNode* lit) {
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
// CollectDecls — passe 1 : collecter in/out/uniforms/buffers
// Appelée par Generate() au lieu de la boucle inline
// =============================================================================
void NkSLCodeGen_HLSL::CollectDecls(NkSLProgramNode* prog) {
    mInputVars.Clear(); mOutputVars.Clear();
    mCBuffers.Clear(); mSBuffers.Clear(); mUniforms.Clear();
    for (auto* child : prog->children) {
        if (!child) continue;
        switch (child->kind) {
            case NkSLNodeKind::DECL_INPUT:
                mInputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_OUTPUT:
                mOutputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_UNIFORM_BLOCK:
            case NkSLNodeKind::DECL_PUSH_CONSTANT:
                mCBuffers.PushBack(static_cast<NkSLBlockDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_STORAGE_BLOCK:
                mSBuffers.PushBack(static_cast<NkSLBlockDeclNode*>(child)); break;
            case NkSLNodeKind::DECL_VAR: {
                auto* v = static_cast<NkSLVarDeclNode*>(child);
                if (v->storage == NkSLStorageQual::UNIFORM) mUniforms.PushBack(v);
                break;
            }
            default: break;
        }
    }
}

// =============================================================================
// GenDecl + GenVarDecl HLSL
// =============================================================================
void NkSLCodeGen_HLSL::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::DECL_PUSH_CONSTANT:
            GenCBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_STORAGE_BLOCK:
            GenSBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node)); break;
        case NkSLNodeKind::DECL_VAR:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), true); break;
        default: break;
    }
}

void NkSLCodeGen_HLSL::GenVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
    if (!v || !v->type) return;
    // Les globales in/out/uniform sont gérées via les structs — on émet seulement les const
    if (isGlobal) {
        if (!v->isConst) return; // les uniformes sont dans les cbuffers
    }
    NkString line = (v->isConst ? "static const " : "") + TypeToHLSL(v->type) + " " + v->name;
    if (v->initializer) line += " = " + GenExpr(v->initializer);
    EmitLine(line + ";");
}

} // namespace nkentseu