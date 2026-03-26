// =============================================================================
// NkSLCodeGenHLSL.cpp
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

// Scan récursif pour détecter si gl_FragDepth est écrit dans le corps d'un shader
static bool ScanWritesDepth(NkSLNode* node) {
    if (!node) return false;
    if (node->kind == NkSLNodeKind::NK_EXPR_ASSIGN) {
        auto* a = static_cast<NkSLAssignNode*>(node);
        if (a->lhs && a->lhs->kind == NkSLNodeKind::NK_EXPR_IDENT) {
            auto* id = static_cast<NkSLIdentNode*>(a->lhs);
            if (id->name == "gl_FragDepth") return true;
        }
    }
    for (auto* c : node->children)
        if (ScanWritesDepth(c)) return true;
    return false;
}

// Scan récursif des déclarations locales de type matrice dans le corps d'une fonction
static void ScanLocalMatrices(NkSLNode* node, NkVector<NkString>& out) {
    if (!node) return;
    if (node->kind == NkSLNodeKind::NK_DECL_VAR) {
        auto* v = static_cast<NkSLVarDeclNode*>(node);
        // Variable locale (pas uniforme/in/out) de type matrice
        if (v->type && NkSLTypeIsMatrix(v->type->baseType) &&
            v->storage == NkSLStorageQual::NK_NONE &&
            !v->name.Empty())
            out.PushBack(v->name.ToLower());
    }
    for (auto* c : node->children)
        ScanLocalMatrices(c, out);
}

// static bool NkSLTypeIsMatrix(NkSLBaseType t) {
//     return t == NkSLBaseType::NK_MAT2   || t == NkSLBaseType::NK_MAT3   ||
//            t == NkSLBaseType::NK_MAT4   || t == NkSLBaseType::NK_MAT2X3 ||
//            t == NkSLBaseType::NK_MAT2X4 || t == NkSLBaseType::NK_MAT3X2 ||
//            t == NkSLBaseType::NK_MAT3X4 || t == NkSLBaseType::NK_MAT4X2 ||
//            t == NkSLBaseType::NK_MAT4X3;
// }

// =============================================================================
NkString NkSLCodeGenHLSL::BaseTypeToHLSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_VOID:   return "void";
        case NkSLBaseType::NK_BOOL:   return "bool";
        case NkSLBaseType::NK_INT:    return "int";    case NkSLBaseType::NK_IVEC2: return "int2";
        case NkSLBaseType::NK_IVEC3:  return "int3";   case NkSLBaseType::NK_IVEC4: return "int4";
        case NkSLBaseType::NK_UINT:   return "uint";   case NkSLBaseType::NK_UVEC2: return "uint2";
        case NkSLBaseType::NK_UVEC3:  return "uint3";  case NkSLBaseType::NK_UVEC4: return "uint4";
        case NkSLBaseType::NK_FLOAT:  return "float";  case NkSLBaseType::NK_VEC2:  return "float2";
        case NkSLBaseType::NK_VEC3:   return "float3"; case NkSLBaseType::NK_VEC4:  return "float4";
        case NkSLBaseType::NK_DOUBLE: return "double"; case NkSLBaseType::NK_DVEC2: return "double2";
        case NkSLBaseType::NK_DVEC3:  return "double3";case NkSLBaseType::NK_DVEC4: return "double4";
        // HLSL : les matrices sont row-major par défaut mais pour rester compatible
        // avec le code C++ column-major on utilise column_major explicitement
        case NkSLBaseType::NK_MAT2:   return "column_major float2x2";
        case NkSLBaseType::NK_MAT3:   return "column_major float3x3";
        case NkSLBaseType::NK_MAT4:   return "column_major float4x4";
        case NkSLBaseType::NK_MAT2X3: return "column_major float2x3";
        case NkSLBaseType::NK_MAT2X4: return "column_major float2x4";
        case NkSLBaseType::NK_MAT3X2: return "column_major float3x2";
        case NkSLBaseType::NK_MAT3X4: return "column_major float3x4";
        case NkSLBaseType::NK_MAT4X2: return "column_major float4x2";
        case NkSLBaseType::NK_MAT4X3: return "column_major float4x3";
        // Samplers HLSL (texture + sampler séparés ou combinés via SamplerState)
        case NkSLBaseType::NK_SAMPLER2D:            return "Texture2D";
        case NkSLBaseType::NK_SAMPLER2D_SHADOW:     return "Texture2D";   // avec SamplerComparisonState
        case NkSLBaseType::NK_SAMPLER2D_ARRAY:      return "Texture2DArray";
        case NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW:return "Texture2DArray";
        case NkSLBaseType::NK_SAMPLER_CUBE:         return "TextureCube";
        case NkSLBaseType::NK_SAMPLER_CUBE_SHADOW:  return "TextureCube";
        case NkSLBaseType::NK_SAMPLER3D:            return "Texture3D";
        case NkSLBaseType::NK_ISAMPLER2D:           return "Texture2D<int4>";
        case NkSLBaseType::NK_USAMPLER2D:           return "Texture2D<uint4>";
        case NkSLBaseType::NK_IMAGE2D:              return "RWTexture2D<float4>";
        case NkSLBaseType::NK_IIMAGE2D:             return "RWTexture2D<int4>";
        case NkSLBaseType::NK_UIMAGE2D:             return "RWTexture2D<uint4>";
        default: return "float";
    }
}

NkString NkSLCodeGenHLSL::TypeToHLSL(NkSLTypeNode* t) {
    if (!t) return "void";
    if (t->baseType == NkSLBaseType::NK_STRUCT) return t->typeName;
    NkString s = BaseTypeToHLSL(t->baseType);
    if (t->arraySize > 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "[%u]", t->arraySize);
        s += NkString(buf);
    }
    return s;
}

NkString NkSLCodeGenHLSL::BuiltinToHLSL(const NkString& name, NkSLStage stage) {
    if (name == "gl_Position")    return "output._Position";  // struct field = _Position
    if (name == "gl_FragCoord")   return "input._Position";
    if (name == "gl_FragDepth")   return "output._Depth";
    if (name == "gl_VertexID")    return "input._VertexID";
    if (name == "gl_InstanceID")  return "input._InstanceID";
    if (name == "gl_FrontFacing") return "input.IsFrontFace";
    if (name == "gl_LocalInvocationID")  return "GroupThreadID";
    if (name == "gl_GlobalInvocationID") return "DispatchThreadID";
    if (name == "gl_WorkGroupID")        return "GroupID";
    return name;
}
// NkSLVarDeclNode* v, NkSLStage stage, bool isInput, int autoIndex
// NkString NkSLCodeGenHLSL::SemanticFor(NkSLVarDeclNode* v, NkSLStage stage, bool isInput, int autoIndex) {
//     // Si une location est spécifiée, utiliser TEXCOORD<N>
//     if (v->binding.HasLocation()) {
//         char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", v->binding.location);
//         return NkString(buf);
//     }
//     // Heuristiques par nom
//     if (v->name.ToLower().Contains("position") || v->name.ToLower().Contains("pos"))
//         return isInput ? "POSITION" : "SV_Position";
//     if (v->name.ToLower().Contains("normal") || v->name.ToLower().Contains("nrm"))
//         return "NORMAL";
//     if (v->name.ToLower().Contains("color") || v->name.ToLower().Contains("col"))
//         return "COLOR";
//     if (v->name.ToLower().Contains("uv") || v->name.ToLower().Contains("texcoord"))
//         return "TEXCOORD0";
//     // Défaut
//     // static int sSemanticIdx = 0;
//     // char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", sSemanticIdx++);
//     char buf[32]; snprintf(buf, sizeof(buf), "TEXCOORD%d", autoIndex);
    
//     return NkString(buf);
// }

NkString NkSLCodeGenHLSL::IntrinsicToHLSL(const NkString& name) {
    // Constructeurs de types GLSL → HLSL
    if (name == "vec2")  return "float2";
    if (name == "vec3")  return "float3";
    if (name == "vec4")  return "float4";
    if (name == "ivec2") return "int2";
    if (name == "ivec3") return "int3";
    if (name == "ivec4") return "int4";
    if (name == "uvec2") return "uint2";
    if (name == "uvec3") return "uint3";
    if (name == "uvec4") return "uint4";
    if (name == "mat2")  return "float2x2";
    if (name == "mat3")  return "float3x3";
    if (name == "mat4")  return "float4x4";
    // Intrinsèques GLSL → HLSL
    if (name == "mix")         return "lerp";
    if (name == "fract")       return "frac";
    if (name == "mod")         return "fmod";
    if (name == "inversesqrt") return "rsqrt";
    if (name == "dFdx")        return "ddx";
    if (name == "dFdy")        return "ddy";
    if (name == "atan" && false) return "atan2"; // atan(y,x) → atan2(y,x)
    // HLSL n'a pas de inverse() natif — on émet un helper nksl_inverse (surcharge mat3/mat4)
    if (name == "inverse")     return "nksl_inverse";
    return name;
}

// =============================================================================
NkSLCompileResult NkSLCodeGenHLSL::Generate(NkSLProgramNode* ast,
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
    res.target  = NkSLTarget::NK_HLSL;
    res.stage   = stage;
    res.errors  = mErrors;
    res.warnings= mWarnings;
    for (uint32 i = 0; i < (uint32)res.source.Size(); i++)
        res.bytecode.PushBack((uint8)res.source[i]);
    return res;
}

void NkSLCodeGenHLSL::GenProgram(NkSLProgramNode* prog) {
    EmitLine("// Generated by NkSL Shader Compiler");
    EmitLine(NkFormat("// Target: HLSL SM{0}", mOpts->hlslShaderModel));
    EmitNewLine();

    // ── Helpers NKSL ─────────────────────────────────────────────────────────
    // nksl_texsize2d : équivalent de textureSize(sampler2D, lod) — GetDimensions() est void
    EmitLine("float2 nksl_texsize2d(Texture2D t) { uint w,h; t.GetDimensions(w,h); return float2(w,h); }");
    // nksl_inverse : HLSL n'a pas de inverse() natif pour les matrices
    EmitLine("column_major float3x3 nksl_inverse(column_major float3x3 m) {");
    IndentPush();
    EmitLine("float a=m[0][0],b=m[0][1],c=m[0][2],d=m[1][0],e=m[1][1],f=m[1][2],g=m[2][0],h=m[2][1],k=m[2][2];");
    EmitLine("float det = a*(e*k-f*h) - b*(d*k-f*g) + c*(d*h-e*g);");
    EmitLine("float inv = 1.0f/det;");
    EmitLine("column_major float3x3 r;");
    EmitLine("r[0]=float3( (e*k-f*h)*inv, -(b*k-c*h)*inv,  (b*f-c*e)*inv );");
    EmitLine("r[1]=float3(-(d*k-f*g)*inv,  (a*k-c*g)*inv, -(a*f-c*d)*inv );");
    EmitLine("r[2]=float3( (d*h-e*g)*inv, -(a*h-b*g)*inv,  (a*e-b*d)*inv );");
    EmitLine("return r;");
    IndentPop();
    EmitLine("}");
    EmitNewLine();

    // Structs
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::NK_DECL_STRUCT)
            GenStruct(static_cast<NkSLStructDeclNode*>(child));
    }

    // cbuffer
    for (auto* cb : mCBuffers) GenCBuffer(cb);

    // Storage buffers
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::NK_DECL_STORAGE_BLOCK)
            GenSBuffer(static_cast<NkSLBlockDeclNode*>(child));
    }

    // Textures et samplers (pour les uniformes sampler2D etc.)
    int texReg = 0, sampReg = 0;
    for (auto* v : mUniforms) {
        if (!v->type) continue;
        if (NkSLTypeIsSampler(v->type->baseType)) {
            int b = v->binding.HasBinding() ? v->binding.binding : texReg++;
            char buf[128];
            // Lowercase : les références dans le corps sont en lowercase (id->name via AST)
            NkString texName = v->name.ToLower();
            // Texture
            snprintf(buf, sizeof(buf), "%s %s_tex : register(t%d);",
                     BaseTypeToHLSL(v->type->baseType).CStr(), texName.CStr(), b);
            EmitLine(NkString(buf));
            // Sampler correspondant
            if (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW ||
                v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW ||
                v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW) {
                snprintf(buf, sizeof(buf), "SamplerComparisonState %s_smp : register(s%d);",
                         texName.CStr(), b);
            } else {
                snprintf(buf, sizeof(buf), "SamplerState %s_smp : register(s%d);",
                         texName.CStr(), b);
            }
            EmitLine(NkString(buf));
        }
    }
    if (!mUniforms.Empty()) EmitNewLine();

    // Génération déterministe des structs in/out (implémentée dans NkSLCodeGenHLSL_Structs.cpp)
    GenInputOutputStructs();

    // Fonctions
    for (auto* child : prog->children) {
        if (child && child->kind == NkSLNodeKind::NK_DECL_FUNCTION)
            GenFunction(static_cast<NkSLFunctionDeclNode*>(child));
    }
}

void NkSLCodeGenHLSL::GenCBuffer(NkSLBlockDeclNode* b) {
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

void NkSLCodeGenHLSL::GenSBuffer(NkSLBlockDeclNode* b) {
    int reg = b->binding.HasBinding() ? b->binding.binding : 0;
    char buf[64]; snprintf(buf, sizeof(buf), "RWStructuredBuffer<%s> %s : register(u%d);",
                           b->blockName.CStr(),
                           b->instanceName.Empty() ? b->blockName.CStr() : b->instanceName.CStr(),
                           reg);
    EmitLine(NkString(buf));
    EmitNewLine();
}

void NkSLCodeGenHLSL::GenStruct(NkSLStructDeclNode* s) {
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

void NkSLCodeGenHLSL::GenFunction(NkSLFunctionDeclNode* fn) {
    // Entry point : signature spéciale
    if (fn->isEntry || fn->name == "main") {
        bool hasOutput = (mStage == NkSLStage::NK_VERTEX && !mOutputStructName.Empty()) ||
                         (mStage == NkSLStage::NK_FRAGMENT && !mOutputStructName.Empty());
        NkString retType = hasOutput ? mOutputStructName : "void";
        NkString sig = retType + " main(";
        if (!mInputVars.Empty()) sig += mInputStructName + " input";
        if (mStage == NkSLStage::NK_COMPUTE) {
            if (!mInputVars.Empty()) sig += ", ";
            sig += "uint3 GroupThreadID : SV_GroupThreadID, ";
            sig += "uint3 DispatchThreadID : SV_DispatchThreadID, ";
            sig += "uint3 GroupID : SV_GroupID";
        }
        sig += ")";
        EmitLine(sig);
        EmitLine("{");
        IndentPush();
        // Déclarer la struct de sortie seulement si elle existe
        if (hasOutput)
            EmitLine(mOutputStructName + " output;");
        // SV_Depth : initialiser à 0 seulement si le shader écrit explicitement gl_FragDepth
        // (évite d'écraser la profondeur rasterisée dans les passes depth-only)
        if (mStage == NkSLStage::NK_FRAGMENT && mWritesDepth)
            EmitLine("output._Depth = 0.0f;");
        if (fn->body)
            for (auto* s : fn->body->children)
                GenStmt(s);
        if (hasOutput)
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
            case NkSLStorageQual::NK_IN:    sig += "in ";    break;
            case NkSLStorageQual::NK_OUT:   sig += "out ";   break;
            case NkSLStorageQual::NK_INOUT: sig += "inout "; break;
            default: break;
        }
        sig += TypeToHLSL(p->type);
        if (!p->name.Empty()) sig += " " + p->name.ToLower();
    }
    sig += ")";

    if (!fn->body) { EmitLine(sig + ";"); EmitNewLine(); return; }
    EmitLine(sig);
    GenStmt(fn->body);
    EmitNewLine();
}

void NkSLCodeGenHLSL::GenStmt(NkSLNode* node) {
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
            // Normaliser en lowercase : les références dans le corps de fonction
            // (NK_EXPR_IDENT) sont stockées en lowercase par l'AST NkSL,
            // donc les déclarations locales doivent l'être aussi.
            NkString line = (v->isConst ? "const " : "") + TypeToHLSL(v->type) + " " + v->name.ToLower();
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
                    init = TypeToHLSL(vd->type) + " " + vd->name.ToLower();
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
        case NkSLNodeKind::NK_STMT_DO_WHILE: {
            EmitLine("do");
            GenStmt(node->children[0]);
            EmitLine("while (" + GenExpr(node->children[1]) + ");");
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
        case NkSLNodeKind::NK_STMT_DISCARD:  EmitLine("discard;");  break;
        default: break;
    }
}

NkString NkSLCodeGenHLSL::GenExpr(NkSLNode* node) {
    if (!node) return "";
    switch (node->kind) {
        case NkSLNodeKind::NK_EXPR_LITERAL:
            return LiteralToStr(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::NK_EXPR_IDENT: {
            auto* id = static_cast<NkSLIdentNode*>(node);
            // Vérifier d'abord les builtins GLSL
            NkString builtin = BuiltinToHLSL(id->name, mStage);
            if (builtin != id->name) return builtin;
            // Comparaison insensible à la casse : l'AST normalise en lowercase pour les
            // déclarations (v->name) mais les références dans le corps gardent la casse
            // d'origine (id->name). On utilise v->name pour l'output afin de matcher les
            // champs de struct générés par GenInputOutputStructs.
            NkString idLow = id->name.ToLower();
            for (auto* v : mInputVars)
                if (v->name.ToLower() == idLow) return "input." + v->name;
            for (auto* v : mOutputVars)
                if (v->name.ToLower() == idLow) return "output." + v->name;
            return id->name;
        }
        case NkSLNodeKind::NK_EXPR_UNARY: {
            auto* u = static_cast<NkSLUnaryNode*>(node);
            NkString op = GenExpr(u->operand);
            return u->prefix ? (u->op + op) : (op + u->op);
        }
        case NkSLNodeKind::NK_EXPR_BINARY: {
            auto* b = static_cast<NkSLBinaryNode*>(node);
            if (b->op == "*") {
                // Détecter la multiplication matrice*vecteur/matrice → mul() en HLSL.
                // Un résultat de mul() est aussi une matrice → vérifier StartsWith("mul(").
                NkString lhs = GenExpr(b->left);
                NkString rhs = GenExpr(b->right);
                bool lhsIsMat = lhs.StartsWith("mul(");
                if (!lhsIsMat) {
                    for (const auto& mname : mMatrixNames)
                        if (lhs == mname) { lhsIsMat = true; break; }
                }
                if (lhsIsMat) return "mul(" + lhs + ", " + rhs + ")";
                return "(" + lhs + " * " + rhs + ")";
            }
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
            // Supprimer le préfixe d'instance cbuffer (ubo.mvp → mvp en HLSL)
            if (m->object && m->object->kind == NkSLNodeKind::NK_EXPR_IDENT) {
                auto* id = static_cast<NkSLIdentNode*>(m->object);
                NkString idLow = id->name.ToLower();
                for (auto* cb : mCBuffers)
                    if (!cb->instanceName.Empty() &&
                        cb->instanceName.ToLower() == idLow)
                        return m->member;
            }
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

NkString NkSLCodeGenHLSL::GenCall(NkSLCallNode* call) {
    NkString name = call->calleeExpr ? GenExpr(call->calleeExpr) : call->callee;
    name = IntrinsicToHLSL(name);

    NkString args;
    for (uint32 i = 0; i < (uint32)call->args.Size(); i++) {
        if (i > 0) args += ", ";
        args += GenExpr(call->args[i]);
    }

    // texture() / textureSize() — conversion GLSL→HLSL
    if (name == "texture" && call->args.Size() >= 2) {
        NkString texName = GenExpr(call->args[0]);
        NkString coord   = GenExpr(call->args[1]);
        // Détecter si c'est un sampler shadow (sampler2DShadow etc.)
        bool isShadow = false;
        for (auto* v : mUniforms) {
            if (!v->type) continue;
            if (v->name.ToLower() == texName) {
                isShadow = (v->type->baseType == NkSLBaseType::NK_SAMPLER2D_SHADOW        ||
                            v->type->baseType == NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW  ||
                            v->type->baseType == NkSLBaseType::NK_SAMPLER_CUBE_SHADOW);
                break;
            }
        }
        if (isShadow) {
            // GLSL: texture(sampler2DShadow, vec3(u,v,compareZ)) → float
            // HLSL: tex.SampleCmpLevelZero(SamplerComparisonState, float2(u,v), compareZ)
            // Si l'argument est vec3(u,v,z) on extrait directement les composantes
            auto* coordNode = call->args[1];
            if (coordNode && coordNode->kind == NkSLNodeKind::NK_EXPR_CALL) {
                auto* vc = static_cast<NkSLCallNode*>(coordNode);
                if ((vc->callee == "vec3" || vc->callee == "float3") && vc->args.Size() == 3) {
                    NkString su = GenExpr(vc->args[0]);
                    NkString sv = GenExpr(vc->args[1]);
                    NkString sz = GenExpr(vc->args[2]);
                    return texName + "_tex.SampleCmpLevelZero(" +
                           texName + "_smp, float2(" + su + ", " + sv + "), " + sz + ")";
                }
            }
            // Fallback : swizzle .xy / .z sur le float3 généré (valide en HLSL SM5+)
            return texName + "_tex.SampleCmpLevelZero(" +
                   texName + "_smp, (" + coord + ").xy, (" + coord + ").z)";
        }
        return texName + "_tex.Sample(" + texName + "_smp, " + coord + ")";
    }
    if (name == "textureSize" && call->args.Size() >= 1) {
        // HLSL : GetDimensions() est void, ne peut pas être inline
        // On utilise le helper nksl_texsize2d() émis dans le préambule
        NkString texName = GenExpr(call->args[0]);
        return "nksl_texsize2d(" + texName + "_tex)";
    }
    // mat3(mat4) → (float3x3)(mat4_expr) en HLSL (extrait la sous-matrice 3x3 supérieure-gauche)
    if (name == "float3x3" && call->args.Size() == 1) {
        NkString arg = GenExpr(call->args[0]);
        return "(float3x3)(" + arg + ")";
    }
    // Constructeur vecteur avec 1 seul argument :
    // FXC (DX11/DX12) n'accepte pas float3(scalar) pour broadcaster un scalaire.
    // La forme cast (float3)(scalar) est valide et broadcaste correctement.
    // (fonctionne aussi si arg est déjà un vec3 : cast no-op)
    if ((name == "float2" || name == "float3" || name == "float4" ||
         name == "int2"   || name == "int3"   || name == "int4"   ||
         name == "uint2"  || name == "uint3"  || name == "uint4"  ||
         name == "double2"|| name == "double3"|| name == "double4") &&
        call->args.Size() == 1) {
        NkString arg = GenExpr(call->args[0]);
        return "(" + name + ")(" + arg + ")";
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
NkString NkSLCodeGenHLSL::LiteralToStr(NkSLLiteralNode* lit) {
    char buf[64];
    switch (lit->baseType) {
        case NkSLBaseType::NK_INT:  snprintf(buf,sizeof(buf),"%lld",(long long)lit->intVal); return buf;
        case NkSLBaseType::NK_UINT: snprintf(buf,sizeof(buf),"%lluu",(unsigned long long)lit->uintVal); return buf;
        case NkSLBaseType::NK_FLOAT: {
            snprintf(buf,sizeof(buf),"%.8g",lit->floatVal);
            NkString s(buf);
            bool hasDot=false; for(int i=0;buf[i];i++) if(buf[i]=='.'||buf[i]=='e') hasDot=true;
            if (!hasDot) s+=".0";
            return s+"f";
        }
        case NkSLBaseType::NK_BOOL: return lit->boolVal ? "true" : "false";
        default: return "0";
    }
}

// =============================================================================
// CollectDecls — passe 1 : collecter in/out/uniforms/buffers
// Appelée par Generate() au lieu de la boucle inline
// =============================================================================
void NkSLCodeGenHLSL::CollectDecls(NkSLProgramNode* prog) {
    mInputVars.Clear(); mOutputVars.Clear();
    mCBuffers.Clear(); mSBuffers.Clear(); mUniforms.Clear();
    mMatrixNames.Clear();
    mWritesDepth = false;
    for (auto* child : prog->children) {
        if (!child) continue;
        switch (child->kind) {
            case NkSLNodeKind::NK_DECL_INPUT:
                mInputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::NK_DECL_OUTPUT:
                mOutputVars.PushBack(static_cast<NkSLVarDeclNode*>(child)); break;
            case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
            case NkSLNodeKind::NK_DECL_PUSH_CONSTANT: {
                auto* cb = static_cast<NkSLBlockDeclNode*>(child);
                mCBuffers.PushBack(cb);
                // Collecter les membres de type matrice pour détecter mul() dans GenExpr
                for (auto* m : cb->members)
                    if (m->type && NkSLTypeIsMatrix(m->type->baseType))
                        mMatrixNames.PushBack(m->name);
                break;
            }
            case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
                mSBuffers.PushBack(static_cast<NkSLBlockDeclNode*>(child)); break;
            case NkSLNodeKind::NK_DECL_VAR: {
                auto* v = static_cast<NkSLVarDeclNode*>(child);
                if (v->storage == NkSLStorageQual::NK_UNIFORM) mUniforms.PushBack(v);
                break;
            }
            case NkSLNodeKind::NK_DECL_FUNCTION:
                // Scanner les variables locales matricielles dans le corps de la fonction
                ScanLocalMatrices(child, mMatrixNames);
                // Détecter si gl_FragDepth est écrit (pour conditionnellement émettre SV_Depth)
                if (mStage == NkSLStage::NK_FRAGMENT && ScanWritesDepth(child))
                    mWritesDepth = true;
                break;
            default: break;
        }
    }
}

// =============================================================================
// GenDecl + GenVarDecl HLSL
// =============================================================================
void NkSLCodeGenHLSL::GenDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_DECL_STRUCT:
            GenStruct(static_cast<NkSLStructDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::NK_DECL_PUSH_CONSTANT:
            GenCBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
            GenSBuffer(static_cast<NkSLBlockDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_FUNCTION:
            GenFunction(static_cast<NkSLFunctionDeclNode*>(node)); break;
        case NkSLNodeKind::NK_DECL_VAR:
            GenVarDecl(static_cast<NkSLVarDeclNode*>(node), true); break;
        default: break;
    }
}

void NkSLCodeGenHLSL::GenVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
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