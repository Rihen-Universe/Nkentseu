#pragma once
// =============================================================================
// NkSLSemantic.h  — v2.1 complet
// =============================================================================
#include "NkSLAST.h"
#include "NkSLSymbolTable.h"

namespace nkentseu {

struct NkSLSemanticResult {
    bool success = false;
    NkVector<NkSLCompileError> errors, warnings;
    void AddError  (uint32 l, const NkString& m) { errors.PushBack({l,0,"",m,true}); success=false; }
    void AddWarning(uint32 l, const NkString& m) { warnings.PushBack({l,0,"",m,false}); }
};

class NkSLSemantic {
public:
    explicit NkSLSemantic(NkSLStage stage);

    NkSLSemanticResult Analyze(NkSLProgramNode* program);

    const NkVector<NkSLVarDeclNode*>&    GetInputVars()   const { return mInputVars; }
    const NkVector<NkSLVarDeclNode*>&    GetOutputVars()  const { return mOutputVars; }
    const NkVector<NkSLBlockDeclNode*>&  GetBlocks()      const { return mBlocks; }
    const NkVector<NkSLVarDeclNode*>&    GetUniforms()    const { return mUniforms; }
    const NkVector<NkSLFunctionDeclNode*>& GetEntryPoints() const { return mEntryPoints; }

    // Utilitaire statique : nom lisible d'un type résolu
    static NkString TypeName(const NkSLResolvedType& t);

    // Vérification inter-stages
    static bool CheckStageInterface(
        const NkSLSemantic& vertexSem,
        const NkSLSemantic& fragmentSem,
        NkVector<NkSLCompileError>& errors);

private:
    void AnalyzeProgram   (NkSLProgramNode*);
    void AnalyzeDecl      (NkSLNode*);
    void AnalyzeVarDecl   (NkSLVarDeclNode*, bool isGlobal=false);
    void AnalyzeBlockDecl (NkSLBlockDeclNode*);
    void AnalyzeStructDecl(NkSLStructDeclNode*);
    void AnalyzeFunction  (NkSLFunctionDeclNode*);
    void AnalyzeStmt      (NkSLNode*);
    void AnalyzeBlock     (NkSLBlockNode*);
    void AnalyzeIf        (NkSLIfNode*);
    void AnalyzeFor       (NkSLForNode*);
    void AnalyzeWhile     (NkSLWhileNode*);
    void AnalyzeReturn    (NkSLReturnNode*);
    void AnalyzeSwitch    (NkSLNode*);

    NkSLResolvedType TypeOf           (NkSLNode*);
    NkSLResolvedType TypeOfLiteral    (NkSLLiteralNode*);
    NkSLResolvedType TypeOfIdent      (NkSLIdentNode*);
    NkSLResolvedType TypeOfUnary      (NkSLUnaryNode*);
    NkSLResolvedType TypeOfBinary     (NkSLBinaryNode*);
    NkSLResolvedType TypeOfTernary    (NkSLNode*);
    NkSLResolvedType TypeOfCall       (NkSLCallNode*);
    NkSLResolvedType TypeOfMember     (NkSLMemberNode*);
    NkSLResolvedType TypeOfIndex      (NkSLIndexNode*);
    NkSLResolvedType TypeOfAssign     (NkSLAssignNode*);
    NkSLResolvedType TypeOfCast       (NkSLCastNode*);

    NkSLResolvedType ValidateSwizzle         (const NkSLResolvedType& base, const NkString& sw, uint32 line);
    NkSLResolvedType ValidateBuiltinCall     (const NkString& name, const NkVector<NkSLResolvedType>& argTypes, uint32 line);
    NkSLResolvedType ValidateConstructorCall (NkSLBaseType targetType, const NkVector<NkSLResolvedType>& argTypes, uint32 line);

    // Vérifie si lhs op rhs est valide malgré des types différents (mat*vec, scalar*vec, etc.)
    static bool AreCompatibleForOp(const NkString& op,
                                    const NkSLResolvedType& lhs,
                                    const NkSLResolvedType& rhs);

    void CheckBindings();
    void CheckEntryPoints();
    void Error  (uint32 line, const NkString& msg);
    void Warning(uint32 line, const NkString& msg);

    NkSLStage       mStage;
    NkSLSymbolTable mSymbols;
    NkSLSemanticResult mResult;

    NkVector<NkSLVarDeclNode*>      mInputVars, mOutputVars, mUniforms;
    NkVector<NkSLBlockDeclNode*>    mBlocks;
    NkVector<NkSLFunctionDeclNode*> mEntryPoints;
    NkUnorderedMap<uint32, NkString> mUsedBindings;
};

} // namespace nkentseu
