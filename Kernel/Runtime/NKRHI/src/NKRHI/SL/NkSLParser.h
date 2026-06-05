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

    // Compute : taille de workgroup collectée depuis layout(local_size_*) in;
    // puis recopiée dans le NkSLProgramNode à la fin de Parse().
    uint32 mLocalSizeX = 1;
    uint32 mLocalSizeY = 1;
    uint32 mLocalSizeZ = 1;

    // Garde anti stack-overflow : profondeur de récursion expression/statement.
    // Au-delà, on émet une erreur et on arrête de récurser (pas de crash).
    static constexpr uint32 kMaxParseDepth = 512;
    uint32 mDepth = 0;

    // Déclarateurs supplémentaires d'une déclaration multiple (ex. "vec3 a, b, c;").
    // ParseVarDecl renvoie le 1er déclarateur et empile les suivants ici ; les
    // sites qui construisent des listes (top-level, blocs, membres struct/block)
    // les drainent juste après l'appel (puis Clear()). Évite un nœud "groupe"
    // que tous les backends devraient connaître.
    NkVector<NkSLVarDeclNode*> mPendingDecls;
};

} // namespace nkentseu
