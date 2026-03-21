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
    PROGRAM,
    TRANSLATION_UNIT,

    // Déclarations
    DECL_FUNCTION,
    DECL_VAR,
    DECL_STRUCT,
    DECL_UNIFORM_BLOCK,   // uniform Block { ... } name;
    DECL_STORAGE_BLOCK,   // buffer Block { ... } name;
    DECL_PUSH_CONSTANT,   // layout(push_constant) uniform Block { ... }
    DECL_INPUT,           // in type name;
    DECL_OUTPUT,          // out type name;
    DECL_PARAM,           // paramètre de fonction

    // Types
    TYPE_BASIC,
    TYPE_ARRAY,
    TYPE_STRUCT_REF,

    // Expressions
    EXPR_LITERAL,
    EXPR_IDENT,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_TERNARY,
    EXPR_CALL,            // foo(a, b)
    EXPR_INDEX,           // a[i]
    EXPR_MEMBER,          // a.b
    EXPR_CAST,            // type(expr)
    EXPR_ASSIGN,

    // Statements
    STMT_BLOCK,
    STMT_EXPR,
    STMT_IF,
    STMT_FOR,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_DISCARD,
    STMT_SWITCH,
    STMT_CASE,

    // Annotations
    ANNOTATION_BINDING,
    ANNOTATION_LOCATION,
    ANNOTATION_BUILTIN,
    ANNOTATION_STAGE,
    ANNOTATION_ENTRY,
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
    NkSLBaseType baseType = NkSLBaseType::FLOAT;
    NkString     typeName;   // nom du struct si STRUCT
    uint32       arraySize  = 0;  // 0 = pas de tableau, UINT32_MAX = non borné
    bool         isUnsized  = false;

    NkSLTypeNode() : NkSLNode(NkSLNodeKind::TYPE_BASIC) {}
};

// Annotation
struct NkSLAnnotationNode : NkSLNode {
    NkSLBinding binding;
    NkString    builtinName;
    NkSLStage   stage = NkSLStage::VERTEX;
    NkString    entryPoint;

    NkSLAnnotationNode(NkSLNodeKind k) : NkSLNode(k) {}
};

// Déclaration de variable
struct NkSLVarDeclNode : NkSLNode {
    NkString            name;
    NkSLTypeNode*       type        = nullptr;
    NkSLNode*           initializer = nullptr;
    NkSLStorageQual     storage     = NkSLStorageQual::NONE;
    NkSLInterpolation   interp      = NkSLInterpolation::SMOOTH;
    NkSLPrecision       precision   = NkSLPrecision::NONE;
    NkSLBinding         binding;
    bool                isConst     = false;
    bool                isInvariant = false;
    bool                isBuiltin   = false;
    NkString            builtinName;

    NkSLVarDeclNode() : NkSLNode(NkSLNodeKind::DECL_VAR) {}
};

// Block (uniform / storage buffer)
struct NkSLBlockDeclNode : NkSLNode {
    NkString             blockName;   // nom du block GLSL
    NkString             instanceName;// nom de l'instance (peut être vide)
    NkSLStorageQual      storage      = NkSLStorageQual::UNIFORM;
    NkSLBinding          binding;
    NkVector<NkSLVarDeclNode*> members;

    NkSLBlockDeclNode() : NkSLNode(NkSLNodeKind::DECL_UNIFORM_BLOCK) {}
};

// Struct
struct NkSLStructDeclNode : NkSLNode {
    NkString                   name;
    NkVector<NkSLVarDeclNode*> members;

    NkSLStructDeclNode() : NkSLNode(NkSLNodeKind::DECL_STRUCT) {}
};

// Paramètre de fonction
struct NkSLParamNode : NkSLNode {
    NkString          name;
    NkSLTypeNode*     type    = nullptr;
    NkSLStorageQual   storage = NkSLStorageQual::NONE;
    NkSLPrecision     precision = NkSLPrecision::NONE;

    NkSLParamNode() : NkSLNode(NkSLNodeKind::DECL_PARAM) {}
};

// Déclaration de fonction
struct NkSLFunctionDeclNode : NkSLNode {
    NkString                  name;
    NkSLTypeNode*             returnType  = nullptr;
    NkVector<NkSLParamNode*>  params;
    NkSLNode*                 body        = nullptr; // STMT_BLOCK ou nullptr si prototype
    bool                      isEntry     = false;   // @entry
    NkSLStage                 stage       = NkSLStage::VERTEX;

    NkSLFunctionDeclNode() : NkSLNode(NkSLNodeKind::DECL_FUNCTION) {}
};

// Littéral
struct NkSLLiteralNode : NkSLNode {
    NkSLBaseType baseType = NkSLBaseType::FLOAT;
    union {
        int64  intVal   = 0;
        uint64 uintVal;
        double floatVal;
        bool   boolVal;
    };

    NkSLLiteralNode() : NkSLNode(NkSLNodeKind::EXPR_LITERAL) {}
};

// Identifiant
struct NkSLIdentNode : NkSLNode {
    NkString name;
    NkSLIdentNode() : NkSLNode(NkSLNodeKind::EXPR_IDENT) {}
};

// Unaire
struct NkSLUnaryNode : NkSLNode {
    NkString  op;
    bool      prefix = true;
    NkSLNode* operand = nullptr;

    NkSLUnaryNode() : NkSLNode(NkSLNodeKind::EXPR_UNARY) {}
};

// Binaire
struct NkSLBinaryNode : NkSLNode {
    NkString  op;
    NkSLNode* left  = nullptr;
    NkSLNode* right = nullptr;

    NkSLBinaryNode() : NkSLNode(NkSLNodeKind::EXPR_BINARY) {}
};

// Appel de fonction / constructeur
struct NkSLCallNode : NkSLNode {
    NkString             callee; // nom de la fonction ou du constructeur
    NkSLNode*            calleeExpr = nullptr; // si méthode
    NkVector<NkSLNode*>  args;

    NkSLCallNode() : NkSLNode(NkSLNodeKind::EXPR_CALL) {}
};

// Accès membre
struct NkSLMemberNode : NkSLNode {
    NkSLNode* object = nullptr;
    NkString  member;

    NkSLMemberNode() : NkSLNode(NkSLNodeKind::EXPR_MEMBER) {}
};

// Index
struct NkSLIndexNode : NkSLNode {
    NkSLNode* array = nullptr;
    NkSLNode* index = nullptr;

    NkSLIndexNode() : NkSLNode(NkSLNodeKind::EXPR_INDEX) {}
};

// Cast
struct NkSLCastNode : NkSLNode {
    NkSLTypeNode* targetType = nullptr;
    NkSLNode*     expr       = nullptr;

    NkSLCastNode() : NkSLNode(NkSLNodeKind::EXPR_CAST) {}
};

// Assignation
struct NkSLAssignNode : NkSLNode {
    NkString  op; // "=", "+=", etc.
    NkSLNode* lhs = nullptr;
    NkSLNode* rhs = nullptr;

    NkSLAssignNode() : NkSLNode(NkSLNodeKind::EXPR_ASSIGN) {}
};

// Bloc de statements
struct NkSLBlockNode : NkSLNode {
    // children = liste de statements

    NkSLBlockNode() : NkSLNode(NkSLNodeKind::STMT_BLOCK) {}
};

// If
struct NkSLIfNode : NkSLNode {
    NkSLNode* condition  = nullptr;
    NkSLNode* thenBranch = nullptr;
    NkSLNode* elseBranch = nullptr;

    NkSLIfNode() : NkSLNode(NkSLNodeKind::STMT_IF) {}
};

// For
struct NkSLForNode : NkSLNode {
    NkSLNode* init      = nullptr;
    NkSLNode* condition = nullptr;
    NkSLNode* increment = nullptr;
    NkSLNode* body      = nullptr;

    NkSLForNode() : NkSLNode(NkSLNodeKind::STMT_FOR) {}
};

// While
struct NkSLWhileNode : NkSLNode {
    NkSLNode* condition = nullptr;
    NkSLNode* body      = nullptr;

    NkSLWhileNode() : NkSLNode(NkSLNodeKind::STMT_WHILE) {}
};

// Return
struct NkSLReturnNode : NkSLNode {
    NkSLNode* value = nullptr;

    NkSLReturnNode() : NkSLNode(NkSLNodeKind::STMT_RETURN) {}
};

// Programme entier
struct NkSLProgramNode : NkSLNode {
    NkString  filename;
    NkSLStage stage = NkSLStage::VERTEX;
    // children = déclarations globales dans l'ordre

    NkSLProgramNode() : NkSLNode(NkSLNodeKind::PROGRAM) {}
};

} // namespace nkentseu
