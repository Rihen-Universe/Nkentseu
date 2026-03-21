#pragma once
// =============================================================================
// NkSLParser.h
// Parser récursif descendant du langage NkSL.
// Produit un NkSLProgramNode (AST complet).
// =============================================================================
#include "NkSLLexer.h"
#include "NkSLAST.h"

namespace nkentseu {

class NkSLParser {
public:
    explicit NkSLParser(NkSLLexer& lexer);

    // Parse le programme entier
    NkSLProgramNode* Parse();

    const NkVector<NkSLCompileError>& GetErrors()   const { return mErrors; }
    const NkVector<NkSLCompileError>& GetWarnings()  const { return mWarnings; }
    bool HasErrors() const { return !mErrors.Empty(); }

private:
    // ── Déclarations globales ─────────────────────────────────────────────────
    NkSLNode*             ParseTopLevel();
    NkSLFunctionDeclNode* ParseFunctionDecl(NkSLVarDeclNode* protoHead=nullptr);
    NkSLVarDeclNode*      ParseVarDecl(bool expectSemi=true);
    NkSLStructDeclNode*   ParseStructDecl();
    NkSLBlockDeclNode*    ParseBlockDecl(NkSLStorageQual storage);

    // ── Annotations ───────────────────────────────────────────────────────────
    NkSLAnnotationNode*   ParseAnnotation();
    NkSLBinding           ParseBindingArgs();

    // ── Types ─────────────────────────────────────────────────────────────────
    NkSLTypeNode*         ParseType();
    NkSLBaseType          TokenToBaseType(NkSLTokenKind kind) const;
    bool                  IsTypeToken(NkSLTokenKind kind) const;
    bool                  IsQualifierToken(NkSLTokenKind kind) const;

    // ── Statements ────────────────────────────────────────────────────────────
    NkSLNode*             ParseStatement();
    NkSLBlockNode*        ParseBlock();
    NkSLNode*             ParseIfStmt();
    NkSLNode*             ParseForStmt();
    NkSLNode*             ParseWhileStmt();
    NkSLNode*             ParseDoWhileStmt();
    NkSLNode*             ParseReturnStmt();
    NkSLNode*             ParseSwitchStmt();
    NkSLNode*             ParseExprStmt();

    // ── Expressions (précédence d'opérateurs par montée) ─────────────────────
    NkSLNode*             ParseExpression();
    NkSLNode*             ParseAssignment();
    NkSLNode*             ParseTernary();
    NkSLNode*             ParseLogicalOr();
    NkSLNode*             ParseLogicalAnd();
    NkSLNode*             ParseBitwiseOr();
    NkSLNode*             ParseBitwiseXor();
    NkSLNode*             ParseBitwiseAnd();
    NkSLNode*             ParseEquality();
    NkSLNode*             ParseRelational();
    NkSLNode*             ParseShift();
    NkSLNode*             ParseAdditive();
    NkSLNode*             ParseMultiplicative();
    NkSLNode*             ParseUnary();
    NkSLNode*             ParsePostfix();
    NkSLNode*             ParsePrimary();
    NkSLNode*             ParseCallOrCast(const NkString& name);

    // ── Utilitaires ───────────────────────────────────────────────────────────
    NkSLToken             Consume();
    NkSLToken             Consume(NkSLTokenKind expected, const char* errMsg);
    bool                  Check(NkSLTokenKind kind) const;
    bool                  Match(NkSLTokenKind kind);
    bool                  IsAtEnd() const;
    NkSLToken             Peek() const;
    NkSLToken             PeekAt(uint32 offset) const;

    void Error(const NkString& msg, uint32 line=0);
    void Warning(const NkString& msg, uint32 line=0);

    // Synchronisation après erreur (mode panique)
    void Synchronize();

    NkSLLexer&                 mLexer;
    NkSLToken                  mCurrent;
    bool                       mHasCurrent = false;
    NkVector<NkSLCompileError> mErrors;
    NkVector<NkSLCompileError> mWarnings;
};

} // namespace nkentseu
