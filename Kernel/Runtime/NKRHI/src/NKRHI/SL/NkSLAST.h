#pragma once
// =============================================================================
// NkSLAST.h
// Nœuds de l'AST (Abstract Syntax Tree) du langage NkSL.
// =============================================================================
#include "NkSLTypes.h"

namespace nkentseu {

// =============================================================================
// Types de nœuds AST
// =============================================================================
enum class NkSLNodeKind : uint32 {
    // Programme
    NK_PROGRAM,
    NK_TRANSLATION_UNIT,

    // Déclarations
    NK_DECL_FUNCTION,
    NK_DECL_VAR,
    NK_DECL_STRUCT,
    NK_DECL_UNIFORM_BLOCK,   // uniform Block { ... } name;
    NK_DECL_STORAGE_BLOCK,   // buffer Block { ... } name;
    NK_DECL_PUSH_CONSTANT,   // layout(push_constant) uniform Block { ... }
    NK_DECL_INPUT,           // in type name;
    NK_DECL_OUTPUT,          // out type name;
    NK_DECL_PARAM,           // paramètre de fonction

    // Types
    NK_TYPE_BASIC,
    NK_TYPE_ARRAY,
    NK_TYPE_STRUCT_REF,

    // Expressions
    NK_EXPR_LITERAL,
    NK_EXPR_IDENT,
    NK_EXPR_UNARY,
    NK_EXPR_BINARY,
    NK_EXPR_TERNARY,
    NK_EXPR_CALL,            // foo(a, b)
    NK_EXPR_INDEX,           // a[i]
    NK_EXPR_MEMBER,          // a.b
    NK_EXPR_CAST,            // type(expr)
    NK_EXPR_ASSIGN,

    // Statements
    NK_STMT_BLOCK,
    NK_STMT_EXPR,
    NK_STMT_IF,
    NK_STMT_FOR,
    NK_STMT_WHILE,
    NK_STMT_DO_WHILE,
    NK_STMT_RETURN,
    NK_STMT_BREAK,
    NK_STMT_CONTINUE,
    NK_STMT_DISCARD,
    NK_STMT_SWITCH,
    NK_STMT_CASE,

    // Annotations
    NK_ANNOTATION_BINDING,
    NK_ANNOTATION_LOCATION,
    NK_ANNOTATION_BUILTIN,
    NK_ANNOTATION_STAGE,
    NK_ANNOTATION_ENTRY,
};

// =============================================================================
// Nœud AST de base
// =============================================================================
struct NkSLNode {
    NkSLNodeKind             kind;
    uint32                   line    = 0;
    uint32                   column  = 0;
    NkVector<NkSLNode*>      children;
    NkSLNode*                parent  = nullptr;

    explicit NkSLNode(NkSLNodeKind k, uint32 ln=0, uint32 col=0)
        : kind(k), line(ln), column(col) {}
    virtual ~NkSLNode() {
        for (auto* c : children) delete c;
    }

    void AddChild(NkSLNode* n) {
        if (n) { n->parent = this; children.PushBack(n); }
    }
};

// =============================================================================
// Nœuds spécialisés
// =============================================================================

// Type (leaf)
struct NkSLTypeNode : NkSLNode {
    NkSLBaseType baseType = NkSLBaseType::NK_FLOAT;
    NkString     typeName;   // nom du struct si STRUCT
    uint32       arraySize  = 0;  // 0 = pas de tableau, UINT32_MAX = non borné
    bool         isUnsized  = false;

    NkSLTypeNode() : NkSLNode(NkSLNodeKind::NK_TYPE_BASIC) {}
};

// Annotation
struct NkSLAnnotationNode : NkSLNode {
    NkSLBinding binding;
    NkString    builtinName;
    NkSLStage   stage = NkSLStage::NK_VERTEX;
    NkString    entryPoint;

    NkSLAnnotationNode(NkSLNodeKind k) : NkSLNode(k) {}
};

// Déclaration de variable
struct NkSLVarDeclNode : NkSLNode {
    NkString            name;
    NkSLTypeNode*       type        = nullptr;
    NkSLNode*           initializer = nullptr;
    NkSLStorageQual     storage     = NkSLStorageQual::NK_NONE;
    NkSLInterpolation   interp      = NkSLInterpolation::NK_SMOOTH;
    NkSLPrecision       precision   = NkSLPrecision::NK_NONE;
    NkSLBinding         binding;
    bool                isConst     = false;
    bool                isInvariant = false;
    bool                isBuiltin   = false;
    NkString            builtinName;

    NkSLVarDeclNode() : NkSLNode(NkSLNodeKind::NK_DECL_VAR) {}
};

// Block (uniform / storage buffer)
struct NkSLBlockDeclNode : NkSLNode {
    NkString             blockName;   // nom du block GLSL
    NkString             instanceName;// nom de l'instance (peut être vide)
    NkSLStorageQual      storage      = NkSLStorageQual::NK_UNIFORM;
    NkSLBinding          binding;
    NkVector<NkSLVarDeclNode*> members;

    NkSLBlockDeclNode() : NkSLNode(NkSLNodeKind::NK_DECL_UNIFORM_BLOCK) {}
};

// Struct
struct NkSLStructDeclNode : NkSLNode {
    NkString                   name;
    NkVector<NkSLVarDeclNode*> members;

    NkSLStructDeclNode() : NkSLNode(NkSLNodeKind::NK_DECL_STRUCT) {}
};

// Paramètre de fonction
struct NkSLParamNode : NkSLNode {
    NkString          name;
    NkSLTypeNode*     type    = nullptr;
    NkSLStorageQual   storage = NkSLStorageQual::NK_NONE;
    NkSLPrecision     precision = NkSLPrecision::NK_NONE;

    NkSLParamNode() : NkSLNode(NkSLNodeKind::NK_DECL_PARAM) {}
};

// Déclaration de fonction
struct NkSLFunctionDeclNode : NkSLNode {
    NkString                  name;
    NkSLTypeNode*             returnType  = nullptr;
    NkVector<NkSLParamNode*>  params;
    NkSLNode*                 body        = nullptr; // STMT_BLOCK ou nullptr si prototype
    bool                      isEntry     = false;   // @entry
    NkSLStage                 stage       = NkSLStage::NK_VERTEX;

    NkSLFunctionDeclNode() : NkSLNode(NkSLNodeKind::NK_DECL_FUNCTION) {}
};

// Littéral
struct NkSLLiteralNode : NkSLNode {
    NkSLBaseType baseType = NkSLBaseType::NK_FLOAT;
    union {
        int64  intVal   = 0;
        uint64 uintVal;
        double floatVal;
        bool   boolVal;
    };

    NkSLLiteralNode() : NkSLNode(NkSLNodeKind::NK_EXPR_LITERAL) {}
};

// Identifiant
struct NkSLIdentNode : NkSLNode {
    NkString name;
    NkSLIdentNode() : NkSLNode(NkSLNodeKind::NK_EXPR_IDENT) {}
};

// Unaire
struct NkSLUnaryNode : NkSLNode {
    NkString  op;
    bool      prefix = true;
    NkSLNode* operand = nullptr;

    NkSLUnaryNode() : NkSLNode(NkSLNodeKind::NK_EXPR_UNARY) {}
};

// Binaire
struct NkSLBinaryNode : NkSLNode {
    NkString  op;
    NkSLNode* left  = nullptr;
    NkSLNode* right = nullptr;

    NkSLBinaryNode() : NkSLNode(NkSLNodeKind::NK_EXPR_BINARY) {}
};

// Appel de fonction / constructeur
struct NkSLCallNode : NkSLNode {
    NkString             callee; // nom de la fonction ou du constructeur
    NkSLNode*            calleeExpr = nullptr; // si méthode
    NkVector<NkSLNode*>  args;

    NkSLCallNode() : NkSLNode(NkSLNodeKind::NK_EXPR_CALL) {}
};

// Accès membre
struct NkSLMemberNode : NkSLNode {
    NkSLNode* object = nullptr;
    NkString  member;

    NkSLMemberNode() : NkSLNode(NkSLNodeKind::NK_EXPR_MEMBER) {}
};

// Index
struct NkSLIndexNode : NkSLNode {
    NkSLNode* array = nullptr;
    NkSLNode* index = nullptr;

    NkSLIndexNode() : NkSLNode(NkSLNodeKind::NK_EXPR_INDEX) {}
};

// Cast
struct NkSLCastNode : NkSLNode {
    NkSLTypeNode* targetType = nullptr;
    NkSLNode*     expr       = nullptr;

    NkSLCastNode() : NkSLNode(NkSLNodeKind::NK_EXPR_CAST) {}
};

// Assignation
struct NkSLAssignNode : NkSLNode {
    NkString  op; // "=", "+=", etc.
    NkSLNode* lhs = nullptr;
    NkSLNode* rhs = nullptr;

    NkSLAssignNode() : NkSLNode(NkSLNodeKind::NK_EXPR_ASSIGN) {}
};

// Bloc de statements
struct NkSLBlockNode : NkSLNode {
    // children = liste de statements

    NkSLBlockNode() : NkSLNode(NkSLNodeKind::NK_STMT_BLOCK) {}
};

// If
struct NkSLIfNode : NkSLNode {
    NkSLNode* condition  = nullptr;
    NkSLNode* thenBranch = nullptr;
    NkSLNode* elseBranch = nullptr;

    NkSLIfNode() : NkSLNode(NkSLNodeKind::NK_STMT_IF) {}
};

// For
struct NkSLForNode : NkSLNode {
    NkSLNode* init      = nullptr;
    NkSLNode* condition = nullptr;
    NkSLNode* increment = nullptr;
    NkSLNode* body      = nullptr;

    NkSLForNode() : NkSLNode(NkSLNodeKind::NK_STMT_FOR) {}
};

// While
struct NkSLWhileNode : NkSLNode {
    NkSLNode* condition = nullptr;
    NkSLNode* body      = nullptr;

    NkSLWhileNode() : NkSLNode(NkSLNodeKind::NK_STMT_WHILE) {}
};

// Return
struct NkSLReturnNode : NkSLNode {
    NkSLNode* value = nullptr;

    NkSLReturnNode() : NkSLNode(NkSLNodeKind::NK_STMT_RETURN) {}
};

// Programme entier
struct NkSLProgramNode : NkSLNode {
    NkString  filename;
    NkSLStage stage = NkSLStage::NK_VERTEX;
    // children = déclarations globales dans l'ordre

    NkSLProgramNode() : NkSLNode(NkSLNodeKind::NK_PROGRAM) {}
};

} // namespace nkentseu
