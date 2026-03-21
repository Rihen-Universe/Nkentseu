// =============================================================================
// NkSLParser.cpp
// =============================================================================
#include "NkSLParser.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstring>

namespace nkentseu {

// =============================================================================
NkSLParser::NkSLParser(NkSLLexer& lexer) : mLexer(lexer) {}

// =============================================================================
// Utilitaires de consommation
// =============================================================================
NkSLToken NkSLParser::Peek() const {
    return const_cast<NkSLParser*>(this)->mLexer.Peek();
}

NkSLToken NkSLParser::PeekAt(uint32 offset) const {
    return const_cast<NkSLParser*>(this)->mLexer.PeekAt(offset);
}

NkSLToken NkSLParser::Consume() {
    return mLexer.Next();
}

NkSLToken NkSLParser::Consume(NkSLTokenKind expected, const char* errMsg) {
    if (Peek().kind == expected) return Consume();
    Error(NkString(errMsg) + " (got '" + Peek().text + "')", Peek().line);
    return Peek(); // Ne pas avancer sur erreur
}

bool NkSLParser::Check(NkSLTokenKind kind) const {
    return Peek().kind == kind;
}

bool NkSLParser::Match(NkSLTokenKind kind) {
    if (Check(kind)) { Consume(); return true; }
    return false;
}

bool NkSLParser::IsAtEnd() const {
    return Peek().kind == NkSLTokenKind::END_OF_FILE;
}

void NkSLParser::Error(const NkString& msg, uint32 line) {
    uint32 l = line > 0 ? line : Peek().line;
    mErrors.PushBack({l, 0, "", msg, true});
}

void NkSLParser::Warning(const NkString& msg, uint32 line) {
    uint32 l = line > 0 ? line : Peek().line;
    mWarnings.PushBack({l, 0, "", msg, false});
}

void NkSLParser::Synchronize() {
    // Mode panique : avancer jusqu'à un point de synchronisation
    while (!IsAtEnd()) {
        NkSLTokenKind k = Peek().kind;
        if (k == NkSLTokenKind::OP_SEMICOLON) { Consume(); return; }
        if (k == NkSLTokenKind::RBRACE)       return;
        if (k == NkSLTokenKind::KW_STRUCT  ||
            k == NkSLTokenKind::KW_UNIFORM ||
            k == NkSLTokenKind::KW_BUFFER  ||
            k == NkSLTokenKind::KW_IN      ||
            k == NkSLTokenKind::KW_OUT     ||
            k == NkSLTokenKind::AT_BINDING ||
            k == NkSLTokenKind::AT_STAGE   ||
            k == NkSLTokenKind::AT_ENTRY   ||
            IsTypeToken(k)) return;
        Consume();
    }
}

// =============================================================================
// Détection de tokens
// =============================================================================
bool NkSLParser::IsTypeToken(NkSLTokenKind k) const {
    return k >= NkSLTokenKind::KW_VOID && k <= NkSLTokenKind::KW_UIMAGE2D;
}

bool NkSLParser::IsQualifierToken(NkSLTokenKind k) const {
    return k == NkSLTokenKind::KW_IN       || k == NkSLTokenKind::KW_OUT     ||
           k == NkSLTokenKind::KW_INOUT    || k == NkSLTokenKind::KW_UNIFORM ||
           k == NkSLTokenKind::KW_BUFFER   || k == NkSLTokenKind::KW_SHARED  ||
           k == NkSLTokenKind::KW_CONST    || k == NkSLTokenKind::KW_SMOOTH  ||
           k == NkSLTokenKind::KW_FLAT     || k == NkSLTokenKind::KW_NOPERSPECTIVE ||
           k == NkSLTokenKind::KW_LOWP     || k == NkSLTokenKind::KW_MEDIUMP ||
           k == NkSLTokenKind::KW_HIGHP    || k == NkSLTokenKind::KW_INVARIANT;
}

// =============================================================================
NkSLBaseType NkSLParser::TokenToBaseType(NkSLTokenKind k) const {
    switch (k) {
        case NkSLTokenKind::KW_VOID:   return NkSLBaseType::VOID;
        case NkSLTokenKind::KW_BOOL:   return NkSLBaseType::BOOL;
        case NkSLTokenKind::KW_INT:    return NkSLBaseType::INT;
        case NkSLTokenKind::KW_UINT:   return NkSLBaseType::UINT;
        case NkSLTokenKind::KW_FLOAT:  return NkSLBaseType::FLOAT;
        case NkSLTokenKind::KW_DOUBLE: return NkSLBaseType::DOUBLE;
        case NkSLTokenKind::KW_IVEC2:  return NkSLBaseType::IVEC2;
        case NkSLTokenKind::KW_IVEC3:  return NkSLBaseType::IVEC3;
        case NkSLTokenKind::KW_IVEC4:  return NkSLBaseType::IVEC4;
        case NkSLTokenKind::KW_UVEC2:  return NkSLBaseType::UVEC2;
        case NkSLTokenKind::KW_UVEC3:  return NkSLBaseType::UVEC3;
        case NkSLTokenKind::KW_UVEC4:  return NkSLBaseType::UVEC4;
        case NkSLTokenKind::KW_VEC2:   return NkSLBaseType::VEC2;
        case NkSLTokenKind::KW_VEC3:   return NkSLBaseType::VEC3;
        case NkSLTokenKind::KW_VEC4:   return NkSLBaseType::VEC4;
        case NkSLTokenKind::KW_DVEC2:  return NkSLBaseType::DVEC2;
        case NkSLTokenKind::KW_DVEC3:  return NkSLBaseType::DVEC3;
        case NkSLTokenKind::KW_DVEC4:  return NkSLBaseType::DVEC4;
        case NkSLTokenKind::KW_MAT2:   return NkSLBaseType::MAT2;
        case NkSLTokenKind::KW_MAT3:   return NkSLBaseType::MAT3;
        case NkSLTokenKind::KW_MAT4:   return NkSLBaseType::MAT4;
        case NkSLTokenKind::KW_MAT2X3: return NkSLBaseType::MAT2X3;
        case NkSLTokenKind::KW_MAT2X4: return NkSLBaseType::MAT2X4;
        case NkSLTokenKind::KW_MAT3X2: return NkSLBaseType::MAT3X2;
        case NkSLTokenKind::KW_MAT3X4: return NkSLBaseType::MAT3X4;
        case NkSLTokenKind::KW_MAT4X2: return NkSLBaseType::MAT4X2;
        case NkSLTokenKind::KW_MAT4X3: return NkSLBaseType::MAT4X3;
        case NkSLTokenKind::KW_DMAT2:  return NkSLBaseType::DMAT2;
        case NkSLTokenKind::KW_DMAT3:  return NkSLBaseType::DMAT3;
        case NkSLTokenKind::KW_DMAT4:  return NkSLBaseType::DMAT4;
        case NkSLTokenKind::KW_SAMPLER2D:            return NkSLBaseType::SAMPLER2D;
        case NkSLTokenKind::KW_SAMPLER2D_SHADOW:     return NkSLBaseType::SAMPLER2D_SHADOW;
        case NkSLTokenKind::KW_SAMPLER2D_ARRAY:      return NkSLBaseType::SAMPLER2D_ARRAY;
        case NkSLTokenKind::KW_SAMPLER2D_ARRAY_SHADOW:return NkSLBaseType::SAMPLER2D_ARRAY_SHADOW;
        case NkSLTokenKind::KW_SAMPLERCUBE:          return NkSLBaseType::SAMPLER_CUBE;
        case NkSLTokenKind::KW_SAMPLERCUBE_SHADOW:   return NkSLBaseType::SAMPLER_CUBE_SHADOW;
        case NkSLTokenKind::KW_SAMPLER3D:            return NkSLBaseType::SAMPLER3D;
        case NkSLTokenKind::KW_ISAMPLER2D:           return NkSLBaseType::ISAMPLER2D;
        case NkSLTokenKind::KW_USAMPLER2D:           return NkSLBaseType::USAMPLER2D;
        case NkSLTokenKind::KW_IMAGE2D:              return NkSLBaseType::IMAGE2D;
        case NkSLTokenKind::KW_IIMAGE2D:             return NkSLBaseType::IIMAGE2D;
        case NkSLTokenKind::KW_UIMAGE2D:             return NkSLBaseType::UIMAGE2D;
        default: return NkSLBaseType::UNKNOWN;
    }
}

// =============================================================================
// Programme
// =============================================================================
NkSLProgramNode* NkSLParser::Parse() {
    auto* prog = new NkSLProgramNode();
    while (!IsAtEnd()) {
        try {
            NkSLNode* decl = ParseTopLevel();
            if (decl) prog->AddChild(decl);
        } catch (...) {
            Synchronize();
        }
    }
    return prog;
}

// =============================================================================
// Top-level : déterminer ce qu'on parse selon le contexte
// =============================================================================
NkSLNode* NkSLParser::ParseTopLevel() {
    NkSLTokenKind k = Peek().kind;

    // Annotation @stage, @entry, @binding, etc.
    if (k == NkSLTokenKind::AT_BINDING || k == NkSLTokenKind::AT_LOCATION ||
        k == NkSLTokenKind::AT_STAGE   || k == NkSLTokenKind::AT_ENTRY    ||
        k == NkSLTokenKind::AT_BUILTIN || k == NkSLTokenKind::AT_PUSH_CONSTANT) {
        return ParseAnnotation();
    }

    // struct
    if (k == NkSLTokenKind::KW_STRUCT) return ParseStructDecl();

    // uniform / buffer block
    if (k == NkSLTokenKind::KW_UNIFORM) {
        // uniform Block { ... } ou uniform type name;
        NkSLToken next1 = PeekAt(1);
        if (next1.kind == NkSLTokenKind::IDENT || IsTypeToken(next1.kind)) {
            // Vérifie si c'est un block (uniform IDENT { ...)
            NkSLToken next2 = PeekAt(2);
            if (next2.kind == NkSLTokenKind::LBRACE)
                return ParseBlockDecl(NkSLStorageQual::UNIFORM);
        }
        // uniform type name; → variable globale
        return ParseVarDecl(true);
    }

    if (k == NkSLTokenKind::KW_BUFFER) return ParseBlockDecl(NkSLStorageQual::BUFFER);

    // in / out → variable globale d'interface
    if (k == NkSLTokenKind::KW_IN || k == NkSLTokenKind::KW_OUT) return ParseVarDecl(true);

    // layout(push_constant) uniform Block { ... }
    if (k == NkSLTokenKind::KW_LAYOUT) {
        // Traitement simplifié : on reconnaît layout(push_constant)
        Consume(); // 'layout'
        Consume(NkSLTokenKind::LPAREN, "Expected '(' after 'layout'");
        bool isPushConst = false;
        while (!Check(NkSLTokenKind::RPAREN) && !IsAtEnd()) {
            NkSLToken qual = Consume();
            if (qual.text == "push_constant") isPushConst = true;
            else if (qual.kind == NkSLTokenKind::OP_COMMA) continue;
            // On ignore les autres qualificateurs (binding=N, location=N, etc.)
            else if (qual.kind == NkSLTokenKind::OP_ASSIGN || IsTypeToken(qual.kind)) continue;
            else if (qual.kind == NkSLTokenKind::LIT_INT) continue;
            else if (qual.kind == NkSLTokenKind::IDENT) continue;
        }
        Consume(NkSLTokenKind::RPAREN, "Expected ')' after layout qualifiers");
        if (isPushConst)
            return ParseBlockDecl(NkSLStorageQual::PUSH_CONSTANT);
        return ParseBlockDecl(NkSLStorageQual::UNIFORM);
    }

    // const
    if (k == NkSLTokenKind::KW_CONST) return ParseVarDecl(true);

    // Qualificateurs d'interpolation avant in/out
    if (k == NkSLTokenKind::KW_SMOOTH || k == NkSLTokenKind::KW_FLAT ||
        k == NkSLTokenKind::KW_NOPERSPECTIVE) return ParseVarDecl(true);

    // Précision
    if (k == NkSLTokenKind::KW_LOWP || k == NkSLTokenKind::KW_MEDIUMP ||
        k == NkSLTokenKind::KW_HIGHP) return ParseVarDecl(true);

    // Type → variable globale ou fonction
    if (IsTypeToken(k)) {
        // Lookahead pour distinguer fonction et variable :
        // type IDENT ( ... ) → fonction
        // type IDENT ; ou = ... ; → variable
        NkSLToken name = PeekAt(1);
        if (name.kind == NkSLTokenKind::IDENT) {
            NkSLToken after = PeekAt(2);
            if (after.kind == NkSLTokenKind::LPAREN) return ParseFunctionDecl();
        }
        return ParseVarDecl(true);
    }

    // IDENT : struct ou fonction de type utilisateur
    if (k == NkSLTokenKind::IDENT) {
        NkSLToken after = PeekAt(2);
        if (after.kind == NkSLTokenKind::LPAREN) return ParseFunctionDecl();
        return ParseVarDecl(true);
    }

    // Semicolons parasites
    if (k == NkSLTokenKind::OP_SEMICOLON) { Consume(); return nullptr; }

    Error("Unexpected token at top level: '" + Peek().text + "'");
    Consume();
    return nullptr;
}

// =============================================================================
// Annotation
// =============================================================================
NkSLAnnotationNode* NkSLParser::ParseAnnotation() {
    NkSLTokenKind ak = Peek().kind;
    auto* ann = new NkSLAnnotationNode(NkSLNodeKind::ANNOTATION_BINDING);
    ann->line = Peek().line;
    Consume(); // consume l'annotation

    if (ak == NkSLTokenKind::AT_BINDING || ak == NkSLTokenKind::AT_LOCATION) {
        ann->kind = (ak == NkSLTokenKind::AT_BINDING)
            ? NkSLNodeKind::ANNOTATION_BINDING
            : NkSLNodeKind::ANNOTATION_LOCATION;
        ann->binding = ParseBindingArgs();
    }
    else if (ak == NkSLTokenKind::AT_STAGE) {
        ann->kind = NkSLNodeKind::ANNOTATION_STAGE;
        if (Match(NkSLTokenKind::LPAREN)) {
            NkSLToken stageToken = Consume();
            if (stageToken.text == "vertex")       ann->stage = NkSLStage::VERTEX;
            else if (stageToken.text == "fragment") ann->stage = NkSLStage::FRAGMENT;
            else if (stageToken.text == "compute")  ann->stage = NkSLStage::COMPUTE;
            else if (stageToken.text == "geometry") ann->stage = NkSLStage::GEOMETRY;
            Consume(NkSLTokenKind::RPAREN, "Expected ')' after @stage argument");
        }
    }
    else if (ak == NkSLTokenKind::AT_ENTRY) {
        ann->kind = NkSLNodeKind::ANNOTATION_ENTRY;
        if (Match(NkSLTokenKind::LPAREN)) {
            ann->entryPoint = Consume().text;
            Consume(NkSLTokenKind::RPAREN, "Expected ')' after @entry argument");
        }
    }
    else if (ak == NkSLTokenKind::AT_BUILTIN) {
        ann->kind = NkSLNodeKind::ANNOTATION_BUILTIN;
        if (Match(NkSLTokenKind::LPAREN)) {
            ann->builtinName = Consume().text;
            Consume(NkSLTokenKind::RPAREN, "Expected ')' after @builtin argument");
        }
    }

    return ann;
}

NkSLBinding NkSLParser::ParseBindingArgs() {
    NkSLBinding b;
    if (!Match(NkSLTokenKind::LPAREN)) return b;
    while (!Check(NkSLTokenKind::RPAREN) && !IsAtEnd()) {
        NkSLToken name = Peek();
        if (name.kind == NkSLTokenKind::IDENT) {
            Consume(); // nom
            if (Match(NkSLTokenKind::OP_ASSIGN)) {
                NkSLToken val = Consume(NkSLTokenKind::LIT_INT, "Expected integer binding value");
                if (name.text == "set")      b.set     = (int32)val.intVal;
                if (name.text == "binding")  b.binding = (int32)val.intVal;
                if (name.text == "location") b.location= (int32)val.intVal;
                if (name.text == "offset")   b.offset  = (int32)val.intVal;
            }
        } else if (name.kind == NkSLTokenKind::LIT_INT) {
            // @location(0) syntax courte
            Consume();
            b.location = (int32)name.intVal;
        }
        Match(NkSLTokenKind::OP_COMMA);
    }
    Consume(NkSLTokenKind::RPAREN, "Expected ')' after binding args");
    return b;
}

// =============================================================================
// Type
// =============================================================================
NkSLTypeNode* NkSLParser::ParseType() {
    auto* t = new NkSLTypeNode();
    t->line = Peek().line;

    NkSLTokenKind k = Peek().kind;
    if (IsTypeToken(k)) {
        t->baseType = TokenToBaseType(k);
        t->typeName = Consume().text;
    } else if (k == NkSLTokenKind::IDENT) {
        // Type utilisateur (struct)
        t->baseType = NkSLBaseType::STRUCT;
        t->typeName = Consume().text;
    } else {
        Error("Expected type name");
        t->baseType = NkSLBaseType::FLOAT;
    }

    // Tableau ?
    if (Match(NkSLTokenKind::LBRACKET)) {
        if (Check(NkSLTokenKind::RBRACKET)) {
            t->isUnsized = true;
            t->arraySize = 0;
        } else if (Check(NkSLTokenKind::LIT_INT)) {
            t->arraySize = (uint32)Consume().intVal;
        }
        Consume(NkSLTokenKind::RBRACKET, "Expected ']'");
        t->kind = NkSLNodeKind::TYPE_ARRAY;
    }

    return t;
}

// =============================================================================
// Déclaration de variable
// =============================================================================
NkSLVarDeclNode* NkSLParser::ParseVarDecl(bool expectSemi) {
    auto* decl = new NkSLVarDeclNode();
    decl->line = Peek().line;

    // Qualificateurs
    for (;;) {
        NkSLTokenKind k = Peek().kind;
        if      (k == NkSLTokenKind::KW_CONST)         { decl->isConst = true; Consume(); }
        else if (k == NkSLTokenKind::KW_INVARIANT)     { decl->isInvariant = true; Consume(); }
        else if (k == NkSLTokenKind::KW_IN)            { decl->storage = NkSLStorageQual::IN; Consume(); decl->kind = NkSLNodeKind::DECL_INPUT; }
        else if (k == NkSLTokenKind::KW_OUT)           { decl->storage = NkSLStorageQual::OUT; Consume(); decl->kind = NkSLNodeKind::DECL_OUTPUT; }
        else if (k == NkSLTokenKind::KW_INOUT)         { decl->storage = NkSLStorageQual::INOUT; Consume(); }
        else if (k == NkSLTokenKind::KW_UNIFORM)       { decl->storage = NkSLStorageQual::UNIFORM; Consume(); }
        else if (k == NkSLTokenKind::KW_SHARED)        { decl->storage = NkSLStorageQual::SHARED; Consume(); }
        else if (k == NkSLTokenKind::KW_SMOOTH)        { decl->interp = NkSLInterpolation::SMOOTH; Consume(); }
        else if (k == NkSLTokenKind::KW_FLAT)          { decl->interp = NkSLInterpolation::FLAT; Consume(); }
        else if (k == NkSLTokenKind::KW_NOPERSPECTIVE) { decl->interp = NkSLInterpolation::NOPERSPECTIVE; Consume(); }
        else if (k == NkSLTokenKind::KW_LOWP)          { decl->precision = NkSLPrecision::LOWP; Consume(); }
        else if (k == NkSLTokenKind::KW_MEDIUMP)       { decl->precision = NkSLPrecision::MEDIUMP; Consume(); }
        else if (k == NkSLTokenKind::KW_HIGHP)         { decl->precision = NkSLPrecision::HIGHP; Consume(); }
        else break;
    }

    // Type
    decl->type = ParseType();

    // Nom
    if (Check(NkSLTokenKind::IDENT)) {
        decl->name = Consume().text;
    } else if (Check(NkSLTokenKind::OP_SEMICOLON)) {
        // Struct anonyme ou déclaration sans nom
    }

    // Initialisation
    if (Match(NkSLTokenKind::OP_ASSIGN)) {
        decl->initializer = ParseAssignment();
    }

    if (expectSemi)
        Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';' after variable declaration");

    return decl;
}

// =============================================================================
// Block (uniform / storage buffer / push_constant)
// =============================================================================
NkSLBlockDeclNode* NkSLParser::ParseBlockDecl(NkSLStorageQual storage) {
    auto* block = new NkSLBlockDeclNode();
    block->storage = storage;
    block->line    = Peek().line;

    // Consume 'uniform' / 'buffer' si présent
    if (Peek().kind == NkSLTokenKind::KW_UNIFORM ||
        Peek().kind == NkSLTokenKind::KW_BUFFER)
        Consume();

    // Nom du block
    if (Check(NkSLTokenKind::IDENT)) block->blockName = Consume().text;

    Consume(NkSLTokenKind::LBRACE, "Expected '{' for block declaration");

    while (!Check(NkSLTokenKind::RBRACE) && !IsAtEnd()) {
        NkSLVarDeclNode* member = ParseVarDecl(true);
        block->members.PushBack(member);
        block->AddChild(member);
    }

    Consume(NkSLTokenKind::RBRACE, "Expected '}'");

    // Nom d'instance optionnel
    if (Check(NkSLTokenKind::IDENT)) block->instanceName = Consume().text;

    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';' after block declaration");

    block->kind = storage == NkSLStorageQual::BUFFER
        ? NkSLNodeKind::DECL_STORAGE_BLOCK
        : (storage == NkSLStorageQual::PUSH_CONSTANT
           ? NkSLNodeKind::DECL_PUSH_CONSTANT
           : NkSLNodeKind::DECL_UNIFORM_BLOCK);

    return block;
}

// =============================================================================
// Struct
// =============================================================================
NkSLStructDeclNode* NkSLParser::ParseStructDecl() {
    auto* s = new NkSLStructDeclNode();
    s->line = Peek().line;
    Consume(); // 'struct'
    if (Check(NkSLTokenKind::IDENT)) s->name = Consume().text;
    Consume(NkSLTokenKind::LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::RBRACE) && !IsAtEnd()) {
        NkSLVarDeclNode* m = ParseVarDecl(true);
        s->members.PushBack(m);
        s->AddChild(m);
    }
    Consume(NkSLTokenKind::RBRACE, "Expected '}'");
    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';' after struct");
    return s;
}

// =============================================================================
// Fonction
// =============================================================================
NkSLFunctionDeclNode* NkSLParser::ParseFunctionDecl(NkSLVarDeclNode*) {
    auto* fn = new NkSLFunctionDeclNode();
    fn->line = Peek().line;

    fn->returnType = ParseType();
    fn->name = Consume(NkSLTokenKind::IDENT, "Expected function name").text;

    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    while (!Check(NkSLTokenKind::RPAREN) && !IsAtEnd()) {
        auto* param = new NkSLParamNode();
        param->line = Peek().line;
        // Qualificateur inout/in/out
        if (Check(NkSLTokenKind::KW_IN))    { param->storage = NkSLStorageQual::IN;    Consume(); }
        if (Check(NkSLTokenKind::KW_OUT))   { param->storage = NkSLStorageQual::OUT;   Consume(); }
        if (Check(NkSLTokenKind::KW_INOUT)) { param->storage = NkSLStorageQual::INOUT; Consume(); }
        param->type = ParseType();
        if (Check(NkSLTokenKind::IDENT)) param->name = Consume().text;
        fn->params.PushBack(param);
        fn->AddChild(param);
        if (!Match(NkSLTokenKind::OP_COMMA)) break;
    }
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");

    // Prototype uniquement (pas de corps)
    if (Match(NkSLTokenKind::OP_SEMICOLON)) {
        fn->body = nullptr;
    } else {
        fn->body = ParseBlock();
        fn->AddChild(fn->body);
    }

    return fn;
}

// =============================================================================
// Statements
// =============================================================================
NkSLBlockNode* NkSLParser::ParseBlock() {
    auto* block = new NkSLBlockNode();
    block->line = Peek().line;
    Consume(NkSLTokenKind::LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::RBRACE) && !IsAtEnd()) {
        NkSLNode* stmt = ParseStatement();
        if (stmt) block->AddChild(stmt);
    }
    Consume(NkSLTokenKind::RBRACE, "Expected '}'");
    return block;
}

NkSLNode* NkSLParser::ParseStatement() {
    switch (Peek().kind) {
        case NkSLTokenKind::LBRACE:      return ParseBlock();
        case NkSLTokenKind::KW_IF:       return ParseIfStmt();
        case NkSLTokenKind::KW_FOR:      return ParseForStmt();
        case NkSLTokenKind::KW_WHILE:    return ParseWhileStmt();
        case NkSLTokenKind::KW_DO:       return ParseDoWhileStmt();
        case NkSLTokenKind::KW_RETURN:   return ParseReturnStmt();
        case NkSLTokenKind::KW_SWITCH:   return ParseSwitchStmt();
        case NkSLTokenKind::KW_BREAK: {
            auto* n = new NkSLNode(NkSLNodeKind::STMT_BREAK, Peek().line);
            Consume(); Consume(NkSLTokenKind::OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::KW_CONTINUE: {
            auto* n = new NkSLNode(NkSLNodeKind::STMT_CONTINUE, Peek().line);
            Consume(); Consume(NkSLTokenKind::OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::KW_DISCARD: {
            auto* n = new NkSLNode(NkSLNodeKind::STMT_DISCARD, Peek().line);
            Consume(); Consume(NkSLTokenKind::OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::OP_SEMICOLON:
            Consume(); return nullptr;
        default:
            // Déclaration locale ou expression
            if (IsTypeToken(Peek().kind) || IsQualifierToken(Peek().kind) ||
                Peek().kind == NkSLTokenKind::KW_CONST) {
                // Vérifie si c'est vraiment une déclaration (type IDENT)
                if ((IsTypeToken(Peek().kind) || Peek().kind == NkSLTokenKind::KW_CONST)
                    && PeekAt(1).kind == NkSLTokenKind::IDENT) {
                    return ParseVarDecl(true);
                }
            }
            return ParseExprStmt();
    }
}

NkSLNode* NkSLParser::ParseIfStmt() {
    auto* n = new NkSLIfNode(); n->line = Peek().line;
    Consume(); // 'if'
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    n->condition = ParseExpression();
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    n->thenBranch = ParseStatement();
    if (Match(NkSLTokenKind::KW_ELSE)) n->elseBranch = ParseStatement();
    n->AddChild(n->condition); n->AddChild(n->thenBranch);
    if (n->elseBranch) n->AddChild(n->elseBranch);
    return n;
}

NkSLNode* NkSLParser::ParseForStmt() {
    auto* n = new NkSLForNode(); n->line = Peek().line;
    Consume(); // 'for'
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    if (!Check(NkSLTokenKind::OP_SEMICOLON)) {
        if (IsTypeToken(Peek().kind) && PeekAt(1).kind == NkSLTokenKind::IDENT)
            n->init = ParseVarDecl(true);
        else
            n->init = ParseExprStmt();
    } else { Consume(); }
    if (!Check(NkSLTokenKind::OP_SEMICOLON)) n->condition = ParseExpression();
    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';'");
    if (!Check(NkSLTokenKind::RPAREN)) n->increment = ParseExpression();
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    n->body = ParseStatement();
    return n;
}

NkSLNode* NkSLParser::ParseWhileStmt() {
    auto* n = new NkSLWhileNode(); n->line = Peek().line;
    Consume(); // 'while'
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    n->condition = ParseExpression();
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    n->body = ParseStatement();
    n->AddChild(n->condition); n->AddChild(n->body);
    return n;
}

NkSLNode* NkSLParser::ParseDoWhileStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::STMT_DO_WHILE, Peek().line);
    Consume(); // 'do'
    n->AddChild(ParseStatement());
    Consume(NkSLTokenKind::KW_WHILE, "Expected 'while'");
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';'");
    return n;
}

NkSLNode* NkSLParser::ParseReturnStmt() {
    auto* n = new NkSLReturnNode(); n->line = Peek().line;
    Consume(); // 'return'
    if (!Check(NkSLTokenKind::OP_SEMICOLON)) {
        n->value = ParseExpression();
        n->AddChild(n->value);
    }
    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';'");
    return n;
}

NkSLNode* NkSLParser::ParseSwitchStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::STMT_SWITCH, Peek().line);
    Consume(); // 'switch'
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    Consume(NkSLTokenKind::LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::RBRACE) && !IsAtEnd()) {
        if (Match(NkSLTokenKind::KW_CASE)) {
            auto* cas = new NkSLNode(NkSLNodeKind::STMT_CASE, Peek().line);
            cas->AddChild(ParseExpression());
            Consume(NkSLTokenKind::OP_COLON, "Expected ':' after case");
            n->AddChild(cas);
        } else if (Match(NkSLTokenKind::KW_DEFAULT)) {
            Consume(NkSLTokenKind::OP_COLON, "Expected ':' after default");
            n->AddChild(new NkSLNode(NkSLNodeKind::STMT_CASE, Peek().line));
        } else {
            NkSLNode* stmt = ParseStatement();
            if (stmt) n->AddChild(stmt);
        }
    }
    Consume(NkSLTokenKind::RBRACE, "Expected '}'");
    return n;
}

NkSLNode* NkSLParser::ParseExprStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::STMT_EXPR, Peek().line);
    if (!Check(NkSLTokenKind::OP_SEMICOLON))
        n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::OP_SEMICOLON, "Expected ';'");
    return n;
}

// =============================================================================
// Expressions — escalade de précédence
// =============================================================================
NkSLNode* NkSLParser::ParseExpression()    { return ParseAssignment(); }
NkSLNode* NkSLParser::ParseAssignment() {
    NkSLNode* lhs = ParseTernary();
    // Opérateurs d'assignation
    NkSLTokenKind k = Peek().kind;
    if (k == NkSLTokenKind::OP_ASSIGN      ||
        k == NkSLTokenKind::OP_PLUS_ASSIGN  || k == NkSLTokenKind::OP_MINUS_ASSIGN ||
        k == NkSLTokenKind::OP_STAR_ASSIGN  || k == NkSLTokenKind::OP_SLASH_ASSIGN ||
        k == NkSLTokenKind::OP_AND_ASSIGN   || k == NkSLTokenKind::OP_OR_ASSIGN    ||
        k == NkSLTokenKind::OP_XOR_ASSIGN   ||
        k == NkSLTokenKind::OP_LSHIFT_ASSIGN|| k == NkSLTokenKind::OP_RSHIFT_ASSIGN) {
        auto* a = new NkSLAssignNode();
        a->op = Consume().text;
        a->lhs = lhs; a->rhs = ParseAssignment();
        a->AddChild(lhs); a->AddChild(a->rhs);
        return a;
    }
    return lhs;
}

NkSLNode* NkSLParser::ParseTernary() {
    NkSLNode* cond = ParseLogicalOr();
    if (Match(NkSLTokenKind::OP_QUESTION)) {
        auto* t = new NkSLNode(NkSLNodeKind::EXPR_TERNARY, Peek().line);
        t->AddChild(cond);
        t->AddChild(ParseExpression());
        Consume(NkSLTokenKind::OP_COLON, "Expected ':' in ternary");
        t->AddChild(ParseTernary());
        return t;
    }
    return cond;
}

#define PARSE_BINARY(Name, Lower, ...) \
NkSLNode* NkSLParser::Name() { \
    NkSLNode* left = Lower(); \
    while (true) { \
        NkSLTokenKind k = Peek().kind; \
        bool match = false; \
        NkSLTokenKind ops[] = {__VA_ARGS__}; \
        for (auto op : ops) { if (k == op) { match = true; break; } } \
        if (!match) break; \
        auto* b = new NkSLBinaryNode(); \
        b->op = Peek().text; Consume(); \
        b->left = left; b->right = Lower(); \
        b->AddChild(b->left); b->AddChild(b->right); \
        left = b; \
    } \
    return left; \
}

PARSE_BINARY(ParseLogicalOr,       ParseLogicalAnd,      NkSLTokenKind::OP_LOR)
PARSE_BINARY(ParseLogicalAnd,      ParseBitwiseOr,       NkSLTokenKind::OP_LAND)
PARSE_BINARY(ParseBitwiseOr,       ParseBitwiseXor,      NkSLTokenKind::OP_OR)
PARSE_BINARY(ParseBitwiseXor,      ParseBitwiseAnd,      NkSLTokenKind::OP_XOR)
PARSE_BINARY(ParseBitwiseAnd,      ParseEquality,        NkSLTokenKind::OP_AND)
PARSE_BINARY(ParseEquality,        ParseRelational,      NkSLTokenKind::OP_EQ, NkSLTokenKind::OP_NEQ)
PARSE_BINARY(ParseRelational,      ParseShift,           NkSLTokenKind::OP_LT, NkSLTokenKind::OP_GT,
                                                         NkSLTokenKind::OP_LE, NkSLTokenKind::OP_GE)
PARSE_BINARY(ParseShift,           ParseAdditive,        NkSLTokenKind::OP_LSHIFT, NkSLTokenKind::OP_RSHIFT)
PARSE_BINARY(ParseAdditive,        ParseMultiplicative,  NkSLTokenKind::OP_PLUS, NkSLTokenKind::OP_MINUS)
PARSE_BINARY(ParseMultiplicative,  ParseUnary,           NkSLTokenKind::OP_STAR, NkSLTokenKind::OP_SLASH,
                                                         NkSLTokenKind::OP_PERCENT)
#undef PARSE_BINARY

NkSLNode* NkSLParser::ParseUnary() {
    NkSLTokenKind k = Peek().kind;
    if (k == NkSLTokenKind::OP_MINUS || k == NkSLTokenKind::OP_LNOT ||
        k == NkSLTokenKind::OP_TILDE || k == NkSLTokenKind::OP_PLUS  ||
        k == NkSLTokenKind::OP_INC   || k == NkSLTokenKind::OP_DEC) {
        auto* u = new NkSLUnaryNode();
        u->op = Consume().text; u->prefix = true;
        u->operand = ParseUnary();
        u->AddChild(u->operand);
        return u;
    }
    return ParsePostfix();
}

NkSLNode* NkSLParser::ParsePostfix() {
    NkSLNode* base = ParsePrimary();
    for (;;) {
        NkSLTokenKind k = Peek().kind;
        if (k == NkSLTokenKind::OP_DOT) {
            Consume();
            auto* m = new NkSLMemberNode();
            m->object = base;
            m->member = Consume(NkSLTokenKind::IDENT, "Expected member name").text;
            m->AddChild(base);
            base = m;
        }
        else if (k == NkSLTokenKind::LBRACKET) {
            Consume();
            auto* idx = new NkSLIndexNode();
            idx->array = base; idx->index = ParseExpression();
            Consume(NkSLTokenKind::RBRACKET, "Expected ']'");
            idx->AddChild(base); idx->AddChild(idx->index);
            base = idx;
        }
        else if (k == NkSLTokenKind::LPAREN) {
            // Appel de fonction via expr (méthode)
            Consume();
            auto* call = new NkSLCallNode();
            call->calleeExpr = base;
            while (!Check(NkSLTokenKind::RPAREN) && !IsAtEnd()) {
                call->args.PushBack(ParseAssignment());
                call->AddChild(call->args.Back());
                if (!Match(NkSLTokenKind::OP_COMMA)) break;
            }
            Consume(NkSLTokenKind::RPAREN, "Expected ')'");
            base = call;
        }
        else if (k == NkSLTokenKind::OP_INC || k == NkSLTokenKind::OP_DEC) {
            auto* u = new NkSLUnaryNode();
            u->op = Consume().text; u->prefix = false;
            u->operand = base; u->AddChild(base);
            base = u;
        }
        else break;
    }
    return base;
}

NkSLNode* NkSLParser::ParsePrimary() {
    NkSLToken tok = Peek();

    // Littéral
    if (tok.kind == NkSLTokenKind::LIT_INT    ||
        tok.kind == NkSLTokenKind::LIT_UINT   ||
        tok.kind == NkSLTokenKind::LIT_FLOAT  ||
        tok.kind == NkSLTokenKind::LIT_DOUBLE ||
        tok.kind == NkSLTokenKind::LIT_BOOL) {
        Consume();
        auto* lit = new NkSLLiteralNode();
        lit->line = tok.line;
        switch (tok.kind) {
            case NkSLTokenKind::LIT_INT:    lit->baseType=NkSLBaseType::INT;    lit->intVal=tok.intVal;     break;
            case NkSLTokenKind::LIT_UINT:   lit->baseType=NkSLBaseType::UINT;   lit->uintVal=tok.uintVal;   break;
            case NkSLTokenKind::LIT_FLOAT:  lit->baseType=NkSLBaseType::FLOAT;  lit->floatVal=tok.floatVal; break;
            case NkSLTokenKind::LIT_DOUBLE: lit->baseType=NkSLBaseType::DOUBLE; lit->floatVal=tok.floatVal; break;
            case NkSLTokenKind::LIT_BOOL:   lit->baseType=NkSLBaseType::BOOL;   lit->boolVal=tok.boolVal;   break;
            default: break;
        }
        return lit;
    }

    // Builtin
    if (tok.kind >= NkSLTokenKind::KW_BUILTIN_POSITION &&
        tok.kind <= NkSLTokenKind::KW_BUILTIN_WORKINVID) {
        Consume();
        auto* id = new NkSLIdentNode();
        id->name = tok.text; id->line = tok.line;
        return id;
    }

    // Type constructeur ou appel de fonction
    if (IsTypeToken(tok.kind)) {
        NkString typeName = Consume().text;
        if (Check(NkSLTokenKind::LPAREN))
            return ParseCallOrCast(typeName);
        // Type seul (improbable ici, mais safe)
        auto* id = new NkSLIdentNode(); id->name = typeName; id->line = tok.line; return id;
    }

    // Identifiant
    if (tok.kind == NkSLTokenKind::IDENT) {
        NkString name = Consume().text;
        if (Check(NkSLTokenKind::LPAREN))
            return ParseCallOrCast(name);
        auto* id = new NkSLIdentNode(); id->name = name; id->line = tok.line; return id;
    }

    // Parenthèse
    if (tok.kind == NkSLTokenKind::LPAREN) {
        Consume();
        NkSLNode* expr = ParseExpression();
        Consume(NkSLTokenKind::RPAREN, "Expected ')'");
        return expr;
    }

    Error("Unexpected token in expression: '" + tok.text + "'", tok.line);
    Consume();
    return new NkSLLiteralNode(); // node vide de récupération
}

NkSLNode* NkSLParser::ParseCallOrCast(const NkString& name) {
    auto* call = new NkSLCallNode();
    call->callee = name;
    call->line   = Peek().line;
    Consume(NkSLTokenKind::LPAREN, "Expected '('");
    while (!Check(NkSLTokenKind::RPAREN) && !IsAtEnd()) {
        call->args.PushBack(ParseAssignment());
        call->AddChild(call->args.Back());
        if (!Match(NkSLTokenKind::OP_COMMA)) break;
    }
    Consume(NkSLTokenKind::RPAREN, "Expected ')'");
    return call;
}

} // namespace nkentseu
