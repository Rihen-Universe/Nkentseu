#pragma once
// =============================================================================
// NkSLCodeGen.h  — v2.1 complet
// =============================================================================
#include "NkSLAST.h"

namespace nkentseu {

// Utilitaires libres partagés par tous les backends
const char* NkSLBaseTypeName(NkSLBaseType t);
NkString    NkSLTypeName    (NkSLBaseType t);

// =============================================================================
class NkSLCodeGenBase {
public:
    explicit NkSLCodeGenBase(NkSLTarget target) : mTarget(target) {}
    virtual ~NkSLCodeGenBase() = default;
    virtual NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                        const NkSLCompileOptions& opts) = 0;
    NkSLTarget GetTarget() const { return mTarget; }
protected:
    void     IndentPush()           { mIndent++; }
    void     IndentPop()            { if (mIndent>0) mIndent--; }
    NkString Indent() const;
    void     Emit    (const NkString& s) { mOutput += s; }
    void     EmitLine(const NkString& s) { mOutput += Indent() + s + "\n"; }
    void     EmitNewLine()               { mOutput += "\n"; }
    NkString TypeToString    (NkSLTypeNode* t);
    NkString BaseTypeToString(NkSLBaseType t);
    NkString StorageToString (NkSLStorageQual s);
    void AddError  (uint32 l, const NkString& m) { mErrors.PushBack({l,0,"",m,true}); }
    void AddWarning(uint32 l, const NkString& m) { mWarnings.PushBack({l,0,"",m,false}); }
    NkSLTarget mTarget; NkString mOutput; uint32 mIndent = 0;
    NkVector<NkSLCompileError> mErrors, mWarnings;
};

// =============================================================================
// GLSL
// =============================================================================
class NkSLCodeGen_GLSL : public NkSLCodeGenBase {
public:
    NkSLCodeGen_GLSL() : NkSLCodeGenBase(NkSLTarget::GLSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     GenProgram (NkSLProgramNode* prog);
    void     GenDecl    (NkSLNode* node);
    void     GenVarDecl (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenBlock   (NkSLBlockDeclNode* b);
    void     GenStruct  (NkSLStructDeclNode* s);
    void     GenFunction(NkSLFunctionDeclNode* fn);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString BuiltinToGLSL (const NkString& name, NkSLStage stage);
    NkString TypeQualifier (NkSLVarDeclNode* v, int bindingBase);
    NkString LiteralToStr  (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage; int mAutoBinding=0;
};

// =============================================================================
// HLSL
// =============================================================================
class NkSLCodeGen_HLSL : public NkSLCodeGenBase {
public:
    NkSLCodeGen_HLSL() : NkSLCodeGenBase(NkSLTarget::HLSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
    // Accessible depuis NkSLCodeGen_Advanced.cpp
    static NkString IntrinsicToHLSL_Static(const NkString& name);
private:
    void     CollectDecls         (NkSLProgramNode* prog);
    void     GenProgram           (NkSLProgramNode* prog);
    void     GenDecl              (NkSLNode* node);
    void     GenVarDecl           (NkSLVarDeclNode* v, bool isGlobal=false);
    void     GenCBuffer           (NkSLBlockDeclNode* b);
    void     GenSBuffer           (NkSLBlockDeclNode* b);
    void     GenStructuredBuffer  (NkSLBlockDeclNode* b);
    void     GenBindlessHeader    ();
    void     GenStruct            (NkSLStructDeclNode* s);
    void     GenFunction          (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs();
    void     GenGeometryShaderPrefix(NkSLStage stage);
    void     GenTessellationShaders (NkSLProgramNode* prog);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToHLSL    (NkSLTypeNode* t);
    NkString BaseTypeToHLSL(NkSLBaseType t);
    NkString BuiltinToHLSL (const NkString& name, NkSLStage stage);
    NkString SemanticFor   (NkSLVarDeclNode* v, NkSLStage stage, bool isInput);
    NkString IntrinsicToHLSL(const NkString& name);
    NkString LiteralToStr  (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage;
    NkString mInputStructName, mOutputStructName;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniforms;
    NkVector<NkSLBlockDeclNode*> mCBuffers, mSBuffers;
};

// =============================================================================
// MSL
// =============================================================================
class NkSLCodeGen_MSL : public NkSLCodeGenBase {
public:
    NkSLCodeGen_MSL() : NkSLCodeGenBase(NkSLTarget::MSL) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     CollectDecls         (NkSLProgramNode* prog);
    void     GenProgram           (NkSLProgramNode* prog);
    void     GenDecl              (NkSLNode* node);
    void     GenVarDecl           (NkSLVarDeclNode* v, bool inStruct=false);
    void     GenStruct            (NkSLStructDeclNode* s);
    void     GenFunction          (NkSLFunctionDeclNode* fn);
    void     GenInputOutputStructs();
    NkString BuildEntryPointSignature(NkSLFunctionDeclNode* fn);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToMSL      (NkSLTypeNode* t);
    NkString BaseTypeToMSL  (NkSLBaseType t);
    NkString BuiltinToMSL   (const NkString& name, NkSLStage stage);
    NkString IntrinsicToMSL (const NkString& name);
    NkString AddressSpaceFor(NkSLStorageQual s);
    NkString LiteralToStr   (NkSLLiteralNode* lit);
    const NkSLCompileOptions*   mOpts=nullptr; NkSLStage mStage;
    NkVector<NkSLVarDeclNode*>   mInputVars, mOutputVars, mUniformVars;
    NkVector<NkSLBlockDeclNode*> mBufferDecls;
};

// =============================================================================
// C++ Software
// =============================================================================
class NkSLCodeGen_CPP : public NkSLCodeGenBase {
public:
    NkSLCodeGen_CPP() : NkSLCodeGenBase(NkSLTarget::CPLUSPLUS) {}
    NkSLCompileResult Generate(NkSLProgramNode* ast, NkSLStage stage,
                                const NkSLCompileOptions& opts) override;
private:
    void     GenProgram (NkSLProgramNode* prog);
    void     GenDecl    (NkSLNode* node);
    void     GenFunction(NkSLFunctionDeclNode* fn, bool asEntry=false);
    void     GenStmt    (NkSLNode* node);
    NkString GenExpr    (NkSLNode* node);
    NkString GenCall    (NkSLCallNode* call);
    NkString TypeToCPP      (NkSLTypeNode* t);
    NkString BaseTypeToCPP  (NkSLBaseType t);
    NkString IntrinsicToCPP (const NkString& name);
    NkString LiteralToStr   (NkSLLiteralNode* lit);
    const NkSLCompileOptions* mOpts=nullptr; NkSLStage mStage;
};

} // namespace nkentseu
