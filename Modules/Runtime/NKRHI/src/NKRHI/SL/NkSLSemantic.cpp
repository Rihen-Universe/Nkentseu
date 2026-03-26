// =============================================================================
// NkSLSemantic.cpp
// =============================================================================
#include "NkSLSemantic.h"

#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/String/NkFormat.h"

#include <cstring>

namespace nkentseu {

NkSLSemantic::NkSLSemantic(NkSLStage stage) : mStage(stage) {
    mResult.success = true;
}

void NkSLSemantic::Error(uint32 line, const NkString& msg) {
    mResult.AddError(line, msg);
}
void NkSLSemantic::Warning(uint32 line, const NkString& msg) {
    mResult.AddWarning(line, msg);
}

// =============================================================================
NkSLSemanticResult NkSLSemantic::Analyze(NkSLProgramNode* program) {
    if (!program) {
        mResult.AddError(0, "Null program node");
        return mResult;
    }
    AnalyzeProgram(program);
    CheckBindings();
    CheckEntryPoints();
    return mResult;
}

// =============================================================================
void NkSLSemantic::AnalyzeProgram(NkSLProgramNode* prog) {
    // Passe 1 : enregistrer toutes les déclarations de fonctions et structs
    // (pour permettre les références forward)
    for (auto* child : prog->children) {
        if (!child) continue;
        if (child->kind == NkSLNodeKind::NK_DECL_STRUCT) {
            auto* s = static_cast<NkSLStructDeclNode*>(child);
            NkSLSymbol sym;
            sym.name = s->name;
            sym.kind = NkSLSymbolKind::NK_STRUCT_TYPE;
            sym.structDecl = s;
            mSymbols.Define(sym);
        } else if (child->kind == NkSLNodeKind::NK_DECL_FUNCTION) {
            auto* fn = static_cast<NkSLFunctionDeclNode*>(child);
            NkSLSymbol* existing = mSymbols.Resolve(fn->name);
            if (!existing) {
                NkSLSymbol sym;
                sym.name = fn->name;
                sym.kind = NkSLSymbolKind::NK_FUNCTION;
                mSymbols.Define(sym);
                existing = mSymbols.Resolve(fn->name);
            }
            // Ajouter cette surcharge
            NkSLFunctionOverload ov;
            ov.decl       = fn;
            ov.returnType = NkSLResolvedType::FromNode(fn->returnType);
            for (auto* p : fn->params)
                ov.paramTypes.PushBack(NkSLResolvedType::FromNode(p->type));
            existing->overloads.PushBack(ov);
        }
    }

    // Passe 2 : analyser toutes les déclarations
    for (auto* child : prog->children)
        AnalyzeDecl(child);
}

void NkSLSemantic::AnalyzeDecl(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_DECL_STRUCT:
            AnalyzeStructDecl(static_cast<NkSLStructDeclNode*>(node));
            break;
        case NkSLNodeKind::NK_DECL_UNIFORM_BLOCK:
        case NkSLNodeKind::NK_DECL_STORAGE_BLOCK:
        case NkSLNodeKind::NK_DECL_PUSH_CONSTANT:
            AnalyzeBlockDecl(static_cast<NkSLBlockDeclNode*>(node));
            break;
        case NkSLNodeKind::NK_DECL_FUNCTION:
            AnalyzeFunction(static_cast<NkSLFunctionDeclNode*>(node));
            break;
        case NkSLNodeKind::NK_DECL_VAR:
        case NkSLNodeKind::NK_DECL_INPUT:
        case NkSLNodeKind::NK_DECL_OUTPUT:
            AnalyzeVarDecl(static_cast<NkSLVarDeclNode*>(node), true);
            break;
        case NkSLNodeKind::NK_ANNOTATION_BINDING:
        case NkSLNodeKind::NK_ANNOTATION_LOCATION:
        case NkSLNodeKind::NK_ANNOTATION_STAGE:
        case NkSLNodeKind::NK_ANNOTATION_ENTRY:
        case NkSLNodeKind::NK_ANNOTATION_BUILTIN:
            break; // Les annotations sont traitées par le parser, pas ici
        default:
            break;
    }
}

void NkSLSemantic::AnalyzeStructDecl(NkSLStructDeclNode* s) {
    if (s->name.Empty())
        Error(s->line, "Struct declaration missing name");

    // Vérifier que les membres ne se répètent pas
    NkUnorderedMap<NkString, bool> seen;
    for (auto* m : s->members) {
        if (seen.Find(m->name)) {
            Error(m->line, "Duplicate struct member '" + m->name + "' in struct '" + s->name + "'");
        }
        seen[m->name] = true;

        // Vérifier que le type existe
        if (m->type && m->type->baseType == NkSLBaseType::NK_STRUCT) {
            if (!mSymbols.Resolve(m->type->typeName))
                Error(m->line, "Unknown type '" + m->type->typeName + "' in struct member");
        }
    }
}

void NkSLSemantic::AnalyzeBlockDecl(NkSLBlockDeclNode* b) {
    if (b->blockName.Empty())
        Warning(b->line, "Unnamed uniform/storage block");

    mBlocks.PushBack(b);

    // Enregistrer comme symbole si il a un nom d'instance
    if (!b->instanceName.Empty()) {
        NkSLSymbol sym;
        sym.name = b->instanceName;
        sym.kind = NkSLSymbolKind::NK_VARIABLE;
        sym.type.base     = NkSLBaseType::NK_STRUCT;
        sym.type.typeName = b->blockName;
        sym.line = b->line;
        if (!mSymbols.Define(sym))
            Error(b->line, "Redefinition of '" + b->instanceName + "'");
    }

    // Vérifier la cohérence du binding
    if (b->binding.HasBinding() && b->binding.HasSet()) {
        // OK pour Vulkan
    } else if (b->binding.HasBinding()) {
        // OK pour GLSL flat
    }
}

void NkSLSemantic::AnalyzeVarDecl(NkSLVarDeclNode* v, bool isGlobal) {
    if (v->name.Empty() && isGlobal) {
        Warning(v->line, "Anonymous global variable declaration");
        return;
    }

    // Vérifier que le type existe
    if (v->type && v->type->baseType == NkSLBaseType::NK_STRUCT) {
        if (!mSymbols.Resolve(v->type->typeName))
            Error(v->line, "Unknown type '" + v->type->typeName + "'");
    }

    // Vérifier que le type de tableau est valide
    if (v->type && v->type->arraySize > 4096)
        Warning(v->line, "Very large array size (" + NkFormat("{0}", v->type->arraySize) + ")");

    // Classement des variables globales
    if (isGlobal) {
        switch (v->storage) {
            case NkSLStorageQual::NK_IN:
                mInputVars.PushBack(v);
                break;
            case NkSLStorageQual::NK_OUT:
                mOutputVars.PushBack(v);
                break;
            case NkSLStorageQual::NK_UNIFORM:
                mUniforms.PushBack(v);
                // Vérifier que sampler/image uniformes ont un binding
                if (v->type && (NkSLTypeIsSampler(v->type->baseType) ||
                                NkSLTypeIsImage(v->type->baseType))) {
                    if (!v->binding.HasBinding())
                        Warning(v->line, "Sampler '" + v->name +
                                "' has no explicit @binding — auto-assigned");
                }
                break;
            default: break;
        }
    }

    // Vérifier que les variables non-const non-uniform globales ont un qualificateur
    if (isGlobal && !v->isConst &&
        v->storage == NkSLStorageQual::NK_NONE &&
        v->type && !NkSLTypeIsSampler(v->type->baseType) &&
        !NkSLTypeIsImage(v->type->baseType)) {
        // Constante globale sans qualificateur : OK si const
        if (!v->isConst)
            Warning(v->line, "Global variable '" + v->name +
                    "' has no storage qualifier (in/out/uniform) — treated as global const");
    }

    // Enregistrer dans la table des symboles
    if (!v->name.Empty()) {
        NkSLSymbol sym;
        sym.name    = v->name;
        sym.kind    = NkSLSymbolKind::NK_VARIABLE;
        sym.type    = NkSLResolvedType::FromNode(v->type);
        sym.varDecl = v;
        sym.line    = v->line;
        if (!mSymbols.Define(sym))
            Error(v->line, "Redefinition of '" + v->name + "'");
    }

    // Analyser l'initialiseur
    if (v->initializer) {
        NkSLResolvedType initType = TypeOf(v->initializer);
        NkSLResolvedType declType = NkSLResolvedType::FromNode(v->type);
        if (initType.base != NkSLBaseType::NK_UNKNOWN &&
            declType.base != NkSLBaseType::NK_UNKNOWN) {
            if (!NkSLSymbolTable::IsAssignable(initType, declType)) {
                Error(v->line, "Type mismatch in initialization of '" + v->name +
                      "': cannot assign '" + TypeName(initType) +
                      "' to '" + TypeName(declType) + "'");
            }
        }
    }
}

void NkSLSemantic::AnalyzeFunction(NkSLFunctionDeclNode* fn) {
    if (fn->name.Empty())
        Error(fn->line, "Function declaration missing name");

    NkSLResolvedType retType = NkSLResolvedType::FromNode(fn->returnType);

    // Pousher un scope pour la fonction
    mSymbols.PushScope(true, false, retType);

    // Enregistrer les paramètres
    for (auto* p : fn->params) {
        if (p->name.Empty()) continue;
        NkSLSymbol sym;
        sym.name    = p->name;
        sym.kind    = NkSLSymbolKind::NK_PARAMETER;
        sym.type    = NkSLResolvedType::FromNode(p->type);
        sym.line    = p->line;
        if (!mSymbols.Define(sym))
            Error(p->line, "Duplicate parameter name '" + p->name + "'");
    }

    // Analyser le corps
    if (fn->body) {
        AnalyzeBlock(static_cast<NkSLBlockNode*>(fn->body));
    }

    mSymbols.PopScope();

    // Entry point ?
    if (fn->isEntry) {
        mEntryPoints.PushBack(fn);

        // Vérifier que l'entry point a un type de retour void
        if (!retType.IsVoid()) {
            Warning(fn->line, "Entry point '" + fn->name +
                    "' should have void return type (return value ignored by GPU driver)");
        }
    }
}

// =============================================================================
// Statements
// =============================================================================
void NkSLSemantic::AnalyzeStmt(NkSLNode* node) {
    if (!node) return;
    switch (node->kind) {
        case NkSLNodeKind::NK_STMT_BLOCK:
            mSymbols.PushScope();
            AnalyzeBlock(static_cast<NkSLBlockNode*>(node));
            mSymbols.PopScope();
            break;
        case NkSLNodeKind::NK_STMT_EXPR:
            if (!node->children.Empty()) TypeOf(node->children[0]);
            break;
        case NkSLNodeKind::NK_DECL_VAR:
            AnalyzeVarDecl(static_cast<NkSLVarDeclNode*>(node), false);
            break;
        case NkSLNodeKind::NK_STMT_IF:
            AnalyzeIf(static_cast<NkSLIfNode*>(node));
            break;
        case NkSLNodeKind::NK_STMT_FOR:
            AnalyzeFor(static_cast<NkSLForNode*>(node));
            break;
        case NkSLNodeKind::NK_STMT_WHILE:
            AnalyzeWhile(static_cast<NkSLWhileNode*>(node));
            break;
        case NkSLNodeKind::NK_STMT_DO_WHILE:
            mSymbols.PushScope(false, true);
            if (!node->children.Empty()) AnalyzeStmt(node->children[0]);
            if (node->children.Size() > 1) {
                NkSLResolvedType ct = TypeOf(node->children[1]);
                if (!ct.IsVoid() && ct.base != NkSLBaseType::NK_BOOL && ct.IsNumeric())
                    Warning(node->line, "do-while condition is not bool");
            }
            mSymbols.PopScope();
            break;
        case NkSLNodeKind::NK_STMT_RETURN:
            AnalyzeReturn(static_cast<NkSLReturnNode*>(node));
            break;
        case NkSLNodeKind::NK_STMT_BREAK:
            if (!mSymbols.IsInLoop())
                Error(node->line, "'break' outside of loop or switch");
            break;
        case NkSLNodeKind::NK_STMT_CONTINUE:
            if (!mSymbols.IsInLoop())
                Error(node->line, "'continue' outside of loop");
            break;
        case NkSLNodeKind::NK_STMT_DISCARD:
            if (mStage != NkSLStage::NK_FRAGMENT)
                Error(node->line, "'discard' only valid in fragment shaders");
            break;
        case NkSLNodeKind::NK_STMT_SWITCH:
            AnalyzeSwitch(node);
            break;
        default:
            break;
    }
}

void NkSLSemantic::AnalyzeBlock(NkSLBlockNode* block) {
    for (auto* child : block->children) AnalyzeStmt(child);
}

void NkSLSemantic::AnalyzeIf(NkSLIfNode* n) {
    if (n->condition) {
        NkSLResolvedType ct = TypeOf(n->condition);
        if (!ct.IsVoid() && ct.base != NkSLBaseType::NK_BOOL) {
            // GLSL accepte les numériques comme condition implicite
            Warning(n->line, "if condition is not bool (implicit conversion)");
        }
    }
    if (n->thenBranch) {
        mSymbols.PushScope();
        AnalyzeStmt(n->thenBranch);
        mSymbols.PopScope();
    }
    if (n->elseBranch) {
        mSymbols.PushScope();
        AnalyzeStmt(n->elseBranch);
        mSymbols.PopScope();
    }
}

void NkSLSemantic::AnalyzeFor(NkSLForNode* n) {
    mSymbols.PushScope(false, true);
    if (n->init)      AnalyzeStmt(n->init);
    if (n->condition) TypeOf(n->condition);
    if (n->increment) TypeOf(n->increment);
    if (n->body)      AnalyzeStmt(n->body);
    mSymbols.PopScope();
}

void NkSLSemantic::AnalyzeWhile(NkSLWhileNode* n) {
    mSymbols.PushScope(false, true);
    if (n->condition) {
        NkSLResolvedType ct = TypeOf(n->condition);
        if (!ct.IsVoid() && ct.base != NkSLBaseType::NK_BOOL)
            Warning(n->line, "while condition is not bool");
    }
    if (n->body) AnalyzeStmt(n->body);
    mSymbols.PopScope();
}

void NkSLSemantic::AnalyzeReturn(NkSLReturnNode* n) {
    NkSLResolvedType expected = mSymbols.CurrentReturnType();
    if (n->value) {
        NkSLResolvedType actual = TypeOf(n->value);
        if (!expected.IsVoid() && actual.base != NkSLBaseType::NK_UNKNOWN) {
            if (!NkSLSymbolTable::IsAssignable(actual, expected)) {
                Error(n->line, "Return type mismatch: expected '" +
                      TypeName(expected) + "', got '" + TypeName(actual) + "'");
            }
        }
    } else {
        if (!expected.IsVoid()) {
            Error(n->line, "Missing return value (expected '" +
                  TypeName(expected) + "')");
        }
    }
}

void NkSLSemantic::AnalyzeSwitch(NkSLNode* n) {
    mSymbols.PushScope(false, true);
    if (!n->children.Empty()) {
        NkSLResolvedType ct = TypeOf(n->children[0]);
        if (ct.base != NkSLBaseType::NK_INT && ct.base != NkSLBaseType::NK_UINT)
            Error(n->line, "switch expression must be integer");
    }
    for (uint32 i = 1; i < (uint32)n->children.Size(); i++)
        AnalyzeStmt(n->children[i]);
    mSymbols.PopScope();
}

// =============================================================================
// Inférence de types — utilitaires
// =============================================================================
// Implémentation de la méthode statique déclarée dans NkSLSemantic.h
NkString NkSLSemantic::TypeName(const NkSLResolvedType& t) {
    if (t.base == NkSLBaseType::NK_STRUCT) return t.typeName;
    switch (t.base) {
        case NkSLBaseType::NK_VOID:   return "void";
        case NkSLBaseType::NK_BOOL:   return "bool";
        case NkSLBaseType::NK_INT:    return "int";    case NkSLBaseType::NK_IVEC2: return "ivec2";
        case NkSLBaseType::NK_IVEC3:  return "ivec3";  case NkSLBaseType::NK_IVEC4: return "ivec4";
        case NkSLBaseType::NK_UINT:   return "uint";   case NkSLBaseType::NK_UVEC2: return "uvec2";
        case NkSLBaseType::NK_UVEC3:  return "uvec3";  case NkSLBaseType::NK_UVEC4: return "uvec4";
        case NkSLBaseType::NK_FLOAT:  return "float";  case NkSLBaseType::NK_VEC2:  return "vec2";
        case NkSLBaseType::NK_VEC3:   return "vec3";   case NkSLBaseType::NK_VEC4:  return "vec4";
        case NkSLBaseType::NK_MAT2:   return "mat2";   case NkSLBaseType::NK_MAT3:  return "mat3";
        case NkSLBaseType::NK_MAT4:   return "mat4";
        case NkSLBaseType::NK_SAMPLER2D:        return "sampler2D";
        case NkSLBaseType::NK_SAMPLER2D_SHADOW: return "sampler2DShadow";
        case NkSLBaseType::NK_SAMPLER3D:        return "sampler3D";
        case NkSLBaseType::NK_SAMPLER_CUBE:     return "samplerCube";
        case NkSLBaseType::NK_IMAGE2D:          return "image2D";
        default: return "unknown";
    }
}

NkSLResolvedType NkSLSemantic::TypeOf(NkSLNode* node) {
    if (!node) return NkSLResolvedType::Void();
    switch (node->kind) {
        case NkSLNodeKind::NK_EXPR_LITERAL:  return TypeOfLiteral(static_cast<NkSLLiteralNode*>(node));
        case NkSLNodeKind::NK_EXPR_IDENT:    return TypeOfIdent  (static_cast<NkSLIdentNode*>(node));
        case NkSLNodeKind::NK_EXPR_UNARY:    return TypeOfUnary  (static_cast<NkSLUnaryNode*>(node));
        case NkSLNodeKind::NK_EXPR_BINARY:   return TypeOfBinary (static_cast<NkSLBinaryNode*>(node));
        case NkSLNodeKind::NK_EXPR_TERNARY:  return TypeOfTernary(node);
        case NkSLNodeKind::NK_EXPR_CALL:     return TypeOfCall   (static_cast<NkSLCallNode*>(node));
        case NkSLNodeKind::NK_EXPR_MEMBER:   return TypeOfMember (static_cast<NkSLMemberNode*>(node));
        case NkSLNodeKind::NK_EXPR_INDEX:    return TypeOfIndex  (static_cast<NkSLIndexNode*>(node));
        case NkSLNodeKind::NK_EXPR_ASSIGN:   return TypeOfAssign (static_cast<NkSLAssignNode*>(node));
        case NkSLNodeKind::NK_EXPR_CAST:     return TypeOfCast   (static_cast<NkSLCastNode*>(node));
        case NkSLNodeKind::NK_STMT_EXPR:
            return node->children.Empty() ? NkSLResolvedType::Void() : TypeOf(node->children[0]);
        default: return NkSLResolvedType{NkSLBaseType::NK_UNKNOWN};
    }
}

NkSLResolvedType NkSLSemantic::TypeOfLiteral(NkSLLiteralNode* lit) {
    NkSLResolvedType t;
    t.base = lit->baseType;
    return t;
}

NkSLResolvedType NkSLSemantic::TypeOfIdent(NkSLIdentNode* id) {
    NkSLSymbol* sym = mSymbols.Resolve(id->name);
    if (!sym) {
        Error(id->line, "Undeclared identifier '" + id->name + "'");
        return {NkSLBaseType::NK_UNKNOWN};
    }
    if (sym->kind == NkSLSymbolKind::NK_FUNCTION || sym->kind == NkSLSymbolKind::NK_BUILTIN_FUNC)
        return NkSLResolvedType{NkSLBaseType::NK_UNKNOWN}; // fonction, pas un type
    return sym->type;
}

NkSLResolvedType NkSLSemantic::TypeOfUnary(NkSLUnaryNode* u) {
    NkSLResolvedType t = TypeOf(u->operand);
    if (u->op == "!" || u->op == "not") {
        if (t.base != NkSLBaseType::NK_BOOL)
            Warning(u->line, "Logical NOT applied to non-bool type");
        return NkSLResolvedType::Bool();
    }
    if (u->op == "~") {
        if (t.base != NkSLBaseType::NK_INT && t.base != NkSLBaseType::NK_UINT)
            Error(u->line, "Bitwise NOT requires integer type");
    }
    return t;
}

NkSLResolvedType NkSLSemantic::TypeOfBinary(NkSLBinaryNode* b) {
    NkSLResolvedType lhs = TypeOf(b->left);
    NkSLResolvedType rhs = TypeOf(b->right);

    if (lhs.base == NkSLBaseType::NK_UNKNOWN || rhs.base == NkSLBaseType::NK_UNKNOWN)
        return {NkSLBaseType::NK_UNKNOWN};

    // Vérifications spécifiques
    if (b->op == "/" && rhs.base == NkSLBaseType::NK_INT)
        Warning(b->line, "Integer division may truncate");

    if ((b->op == "%" || b->op == "&" || b->op == "|" || b->op == "^" ||
         b->op == "<<" || b->op == ">>") &&
        (lhs.base == NkSLBaseType::NK_FLOAT || lhs.base == NkSLBaseType::NK_VEC2 ||
         lhs.base == NkSLBaseType::NK_VEC3  || lhs.base == NkSLBaseType::NK_VEC4)) {
        Error(b->line, "Bitwise/modulo operator requires integer operands");
        return {NkSLBaseType::NK_UNKNOWN};
    }

    // Vérifier compatibilité
    if (!NkSLSymbolTable::IsAssignable(rhs, lhs) &&
        !NkSLSymbolTable::IsAssignable(lhs, rhs) &&
        !AreCompatibleForOp(b->op, lhs, rhs)) {
        Error(b->line, "Type mismatch in binary expression: '" +
              TypeName(lhs) + "' " + b->op + " '" + TypeName(rhs) + "'");
        return {NkSLBaseType::NK_UNKNOWN};
    }

    return NkSLSymbolTable::BinaryResultType(b->op, lhs, rhs);
}

bool NkSLSemantic::AreCompatibleForOp(const NkString& op,
                                        const NkSLResolvedType& lhs,
                                        const NkSLResolvedType& rhs) {
    if (op == "*") {
        // matrix * vector
        if (lhs.base == NkSLBaseType::NK_MAT4 && rhs.base == NkSLBaseType::NK_VEC4) return true;
        if (lhs.base == NkSLBaseType::NK_MAT3 && rhs.base == NkSLBaseType::NK_VEC3) return true;
        if (lhs.base == NkSLBaseType::NK_MAT2 && rhs.base == NkSLBaseType::NK_VEC2) return true;
        // scalar * vector/matrix
        if (lhs.IsScalar() && (rhs.IsVector() || rhs.IsMatrix())) return true;
        if (rhs.IsScalar() && (lhs.IsVector() || lhs.IsMatrix())) return true;
    }
    if (op == "+" || op == "-") {
        if (lhs.IsScalar() && rhs.IsVector()) return true;
        if (lhs.IsVector() && rhs.IsScalar()) return true;
    }
    return false;
}

NkSLResolvedType NkSLSemantic::TypeOfTernary(NkSLNode* n) {
    if (n->children.Size() < 3) return {NkSLBaseType::NK_UNKNOWN};
    TypeOf(n->children[0]); // condition
    NkSLResolvedType t1 = TypeOf(n->children[1]);
    NkSLResolvedType t2 = TypeOf(n->children[2]);
    if (t1 != t2 && !NkSLSymbolTable::IsAssignable(t2, t1))
        Warning(n->line, "Ternary branches have different types");
    return t1;
}

NkSLResolvedType NkSLSemantic::TypeOfCall(NkSLCallNode* call) {
    // Collecter les types des arguments
    NkVector<NkSLResolvedType> argTypes;
    for (auto* arg : call->args)
        argTypes.PushBack(TypeOf(arg));

    // Cas spécial : constructeur (vec4, mat4, etc.)
    NkSLSymbol* sym = mSymbols.Resolve(call->callee);
    if (sym && sym->kind == NkSLSymbolKind::NK_STRUCT_TYPE) {
        // Constructeur de struct — vérifier que le nombre d'args correspond
        auto* s = sym->structDecl;
        if (s && (uint32)call->args.Size() != (uint32)s->members.Size())
            Warning(call->line, "Struct constructor argument count mismatch for '" +
                    call->callee + "'");
        NkSLResolvedType rt; rt.base = NkSLBaseType::NK_STRUCT; rt.typeName = call->callee;
        return rt;
    }

    // Constructeur de type primitif
    static const struct { const char* name; NkSLBaseType t; } kConstructors[] = {
        {"float",NkSLBaseType::NK_FLOAT},{"int",NkSLBaseType::NK_INT},{"uint",NkSLBaseType::NK_UINT},
        {"bool",NkSLBaseType::NK_BOOL},
        {"vec2",NkSLBaseType::NK_VEC2},{"vec3",NkSLBaseType::NK_VEC3},{"vec4",NkSLBaseType::NK_VEC4},
        {"ivec2",NkSLBaseType::NK_IVEC2},{"ivec3",NkSLBaseType::NK_IVEC3},{"ivec4",NkSLBaseType::NK_IVEC4},
        {"uvec2",NkSLBaseType::NK_UVEC2},{"uvec3",NkSLBaseType::NK_UVEC3},{"uvec4",NkSLBaseType::NK_UVEC4},
        {"mat2",NkSLBaseType::NK_MAT2},{"mat3",NkSLBaseType::NK_MAT3},{"mat4",NkSLBaseType::NK_MAT4},
        {"mat2x2",NkSLBaseType::NK_MAT2},{"mat2x3",NkSLBaseType::NK_MAT2X3},
        {"mat2x4",NkSLBaseType::NK_MAT2X4},{"mat3x2",NkSLBaseType::NK_MAT3X2},
        {"mat3x4",NkSLBaseType::NK_MAT3X4},{"mat4x2",NkSLBaseType::NK_MAT4X2},
        {"mat4x3",NkSLBaseType::NK_MAT4X3},
        {nullptr, NkSLBaseType::NK_UNKNOWN}
    };
    for (int i = 0; kConstructors[i].name; i++) {
        if (call->callee == kConstructors[i].name)
            return ValidateConstructorCall(kConstructors[i].t, argTypes, call->line);
    }

    // Résolution de fonction normale
    const NkSLFunctionOverload* ov = mSymbols.ResolveFunction(call->callee, argTypes);
    if (!ov) {
        // Message d'erreur détaillé avec les types des arguments
        NkString argStr;
        for (uint32 i = 0; i < (uint32)argTypes.Size(); i++) {
            if (i > 0) argStr += ", ";
            argStr += TypeName(argTypes[i]);
        }
        Error(call->line, "No matching function for '" + call->callee +
              "(" + argStr + ")'");
        return {NkSLBaseType::NK_UNKNOWN};
    }

    return ov->returnType;
}

NkSLResolvedType NkSLSemantic::TypeOfMember(NkSLMemberNode* m) {
    NkSLResolvedType base = TypeOf(m->object);
    if (base.base == NkSLBaseType::NK_UNKNOWN) return base;

    // Swizzle sur vecteur
    if (base.IsVector() || base.base == NkSLBaseType::NK_FLOAT) {
        return ValidateSwizzle(base, m->member, m->line);
    }

    // Accès membre sur struct
    if (base.base == NkSLBaseType::NK_STRUCT) {
        NkSLSymbol* sym = mSymbols.Resolve(base.typeName);
        if (sym && sym->structDecl) {
            for (auto* mem : sym->structDecl->members) {
                if (mem->name == m->member)
                    return NkSLResolvedType::FromNode(mem->type);
            }
        }
        Error(m->line, "No member '" + m->member + "' in struct '" + base.typeName + "'");
        return {NkSLBaseType::NK_UNKNOWN};
    }

    Error(m->line, "Member access on non-struct non-vector type '" + TypeName(base) + "'");
    return {NkSLBaseType::NK_UNKNOWN};
}

NkSLResolvedType NkSLSemantic::ValidateSwizzle(const NkSLResolvedType& base,
                                                 const NkString& swizzle,
                                                 uint32 line) {
    uint32 vecSize = NkSLSymbolTable::VectorSize(base.base);
    if (base.IsScalar()) vecSize = 1;

    // Vérifier que le set est cohérent (pas de mélange xyzw et rgba)
    bool hasXYZW = false, hasRGBA = false, hasSTPQ = false;
    for (uint32 i = 0; i < (uint32)swizzle.Size(); i++) {
        char c = swizzle[i];
        if (c=='x'||c=='y'||c=='z'||c=='w') hasXYZW = true;
        if (c=='r'||c=='g'||c=='b'||c=='a') hasRGBA = true;
        if (c=='s'||c=='t'||c=='p'||c=='q') hasSTPQ = true;
    }
    int sets = (hasXYZW?1:0) + (hasRGBA?1:0) + (hasSTPQ?1:0);
    if (sets > 1)
        Error(line, "Mixed swizzle sets in '" + swizzle + "'");

    // Vérifier l'indice max
    for (uint32 i = 0; i < (uint32)swizzle.Size(); i++) {
        char c = swizzle[i];
        uint32 idx = 0;
        if (c=='x'||c=='r'||c=='s') idx=0;
        else if (c=='y'||c=='g'||c=='t') idx=1;
        else if (c=='z'||c=='b'||c=='p') idx=2;
        else if (c=='w'||c=='a'||c=='q') idx=3;
        else { Error(line, "Invalid swizzle component '" + NkString(1,c) + "'"); break; }

        if (idx >= vecSize)
            Error(line, "Swizzle '" + NkString(1,c) + "' out of range for " +
                  TypeName(base));
    }

    // Type résultant
    NkSLResolvedType result;
    result.base = base.ScalarBase();
    uint32 n = (uint32)swizzle.Size();
    if (n == 1) { /* scalar */ }
    else if (n == 2) {
        if (base.ScalarBase()==NkSLBaseType::NK_FLOAT)  result.base = NkSLBaseType::NK_VEC2;
        if (base.ScalarBase()==NkSLBaseType::NK_INT)    result.base = NkSLBaseType::NK_IVEC2;
        if (base.ScalarBase()==NkSLBaseType::NK_UINT)   result.base = NkSLBaseType::NK_UVEC2;
    } else if (n == 3) {
        if (base.ScalarBase()==NkSLBaseType::NK_FLOAT)  result.base = NkSLBaseType::NK_VEC3;
        if (base.ScalarBase()==NkSLBaseType::NK_INT)    result.base = NkSLBaseType::NK_IVEC3;
        if (base.ScalarBase()==NkSLBaseType::NK_UINT)   result.base = NkSLBaseType::NK_UVEC3;
    } else if (n == 4) {
        if (base.ScalarBase()==NkSLBaseType::NK_FLOAT)  result.base = NkSLBaseType::NK_VEC4;
        if (base.ScalarBase()==NkSLBaseType::NK_INT)    result.base = NkSLBaseType::NK_IVEC4;
        if (base.ScalarBase()==NkSLBaseType::NK_UINT)   result.base = NkSLBaseType::NK_UVEC4;
    } else {
        Error(line, "Swizzle with more than 4 components");
    }
    return result;
}

NkSLResolvedType NkSLSemantic::TypeOfIndex(NkSLIndexNode* idx) {
    NkSLResolvedType arrType = TypeOf(idx->array);
    NkSLResolvedType idxType = TypeOf(idx->index);

    if (idxType.base != NkSLBaseType::NK_INT && idxType.base != NkSLBaseType::NK_UINT)
        Error(idx->line, "Array index must be integer");

    if (arrType.arraySize > 0 || arrType.isUnsized) {
        // tableau → type élément
        NkSLResolvedType elem = arrType;
        elem.arraySize = 0; elem.isUnsized = false;
        return elem;
    }
    // vecteur[i] → scalaire
    if (arrType.IsVector()) return NkSLResolvedType{arrType.ScalarBase()};
    // matrice[i] → vecteur colonne
    if (arrType.IsMatrix()) {
        switch (arrType.base) {
            case NkSLBaseType::NK_MAT2: return NkSLResolvedType::Vec2();
            case NkSLBaseType::NK_MAT3: return NkSLResolvedType::Vec3();
            case NkSLBaseType::NK_MAT4: return NkSLResolvedType::Vec4();
            default: return NkSLResolvedType::Vec4();
        }
    }
    Error(idx->line, "Subscript on non-array non-vector type '" + TypeName(arrType) + "'");
    return {NkSLBaseType::NK_UNKNOWN};
}

NkSLResolvedType NkSLSemantic::TypeOfAssign(NkSLAssignNode* a) {
    NkSLResolvedType lhs = TypeOf(a->lhs);
    NkSLResolvedType rhs = TypeOf(a->rhs);

    // Vérifier que le LHS est assignable (pas une const, pas un littéral)
    if (a->lhs->kind == NkSLNodeKind::NK_EXPR_LITERAL)
        Error(a->line, "Cannot assign to a literal");

    if (lhs.isConst)
        Error(a->line, "Cannot assign to const");

    if (!NkSLSymbolTable::IsAssignable(rhs, lhs) &&
        lhs.base != NkSLBaseType::NK_UNKNOWN && rhs.base != NkSLBaseType::NK_UNKNOWN)
        Error(a->line, "Type mismatch in assignment: cannot assign '" +
              TypeName(rhs) + "' to '" + TypeName(lhs) + "'");

    return lhs;
}

NkSLResolvedType NkSLSemantic::TypeOfCast(NkSLCastNode* c) {
    TypeOf(c->expr); // analyser l'expression mais ignorer le type
    return NkSLResolvedType::FromNode(c->targetType);
}

NkSLResolvedType NkSLSemantic::ValidateConstructorCall(NkSLBaseType targetType,
                                                         const NkVector<NkSLResolvedType>& argTypes,
                                                         uint32 line) {
    NkSLResolvedType result; result.base = targetType;

    // Vérifications de base
    uint32 targetComponents = NkSLResolvedType{targetType}.ComponentCount();
    if (targetComponents == 0) return result; // type inconnu, on accepte

    uint32 providedComponents = 0;
    for (auto& t : argTypes) providedComponents += t.ComponentCount();

    if (argTypes.Size() == 1 && argTypes[0].IsScalar()) {
        // Broadcase scalaire → vec (vec4(1.0) est valide)
        return result;
    }
    if (providedComponents < targetComponents && !(argTypes.Size()==1)) {
        Warning(line, "Constructor '" + TypeName(result) +
                "' has too few components (" +
                NkFormat("{0}", providedComponents) + " < " +
                NkFormat("{0}", targetComponents) + ")");
    }
    return result;
}

// =============================================================================
// Vérifications globales
// =============================================================================
void NkSLSemantic::CheckBindings() {
    // Vérifier les conflits de binding
    NkUnorderedMap<uint64, NkString> usedBindings; // (set<<32|binding) → nom
    auto check = [&](int set, int binding, const NkString& name, uint32 line) {
        if (binding < 0) return;
        uint64 key = ((uint64)(uint32)set << 32) | (uint32)binding;
        auto* existing = usedBindings.Find(key);
        if (existing) {
            Error(line, "Binding conflict: '" + name + "' and '" + *existing +
                  "' share binding " + NkFormat("{0}", binding) +
                  " on set " + NkFormat("{0}", set));
        } else {
            usedBindings[key] = name;
        }
    };

    for (auto* b : mBlocks) {
        check(b->binding.set, b->binding.binding, b->blockName, b->line);
    }
    for (auto* v : mUniforms) {
        check(v->binding.set, v->binding.binding, v->name, v->line);
    }
}

void NkSLSemantic::CheckEntryPoints() {
    // Vérifier qu'il y a au moins un entry point
    if (mEntryPoints.Empty()) {
        Warning(0, "No @entry function found — did you forget @entry annotation?");
        return;
    }

    // Vérifier que les entry points ont le bon stage
    for (auto* ep : mEntryPoints) {
        if (ep->stage != mStage && ep->stage != NkSLStage::NK_VERTEX) {
            // Le stage est défini par @stage(), pas par le nom de la fonction
            // On vérifie juste que c'est cohérent
        }
    }

    // Pour le fragment shader, vérifier qu'il y a au moins un out
    if (mStage == NkSLStage::NK_FRAGMENT && mOutputVars.Empty()) {
        Warning(0, "Fragment shader has no output variables (discard-only shader?)");
    }
}

// =============================================================================
// Vérification inter-stages (vertex out ↔ fragment in)
// =============================================================================
bool NkSLSemantic::CheckStageInterface(
    const NkSLSemantic& vertSem,
    const NkSLSemantic& fragSem,
    NkVector<NkSLCompileError>& errors)
{
    bool ok = true;
    auto& vertOuts = vertSem.mOutputVars;
    auto& fragIns  = fragSem.mInputVars;

    // Chaque in du fragment doit avoir un out correspondant dans le vertex
    for (auto* fragIn : fragIns) {
        bool found = false;
        for (auto* vertOut : vertOuts) {
            if (vertOut->name == fragIn->name) {
                // Vérifier la compatibilité de types
                auto t1 = NkSLResolvedType::FromNode(vertOut->type);
                auto t2 = NkSLResolvedType::FromNode(fragIn->type);
                if (t1 != t2) {
                    errors.PushBack({fragIn->line, 0, "",
                        "Stage interface mismatch for '" + fragIn->name +
                        "': vertex outputs '" + TypeName(t1) +
                        "' but fragment expects '" + TypeName(t2) + "'", true});
                    ok = false;
                }
                // Vérifier les locations
                if (vertOut->binding.HasLocation() && fragIn->binding.HasLocation()) {
                    if (vertOut->binding.location != fragIn->binding.location) {
                        errors.PushBack({fragIn->line, 0, "",
                            "Location mismatch for '" + fragIn->name +
                            "': vertex @location(" +
                            NkFormat("{0}", vertOut->binding.location) +
                            ") but fragment @location(" +
                            NkFormat("{0}", fragIn->binding.location) + ")", true});
                        ok = false;
                    }
                }
                found = true; break;
            }
        }
        if (!found) {
            errors.PushBack({fragIn->line, 0, "",
                "Fragment input '" + fragIn->name +
                "' has no matching vertex output", false}); // warning, pas erreur
        }
    }

    // Avertir pour les vertex outs sans fragment in correspondant
    for (auto* vertOut : vertOuts) {
        bool found = false;
        for (auto* fragIn : fragIns)
            if (fragIn->name == vertOut->name) { found = true; break; }
        if (!found) {
            errors.PushBack({vertOut->line, 0, "",
                "Vertex output '" + vertOut->name +
                "' is not consumed by fragment shader", false});
        }
    }

    return ok;
}

} // namespace nkentseu
