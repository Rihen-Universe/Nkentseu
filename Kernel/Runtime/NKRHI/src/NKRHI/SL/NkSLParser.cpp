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
    return Peek().kind == NkSLTokenKind::NK_END_OF_FILE;
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
        if (k == NkSLTokenKind::NK_OP_SEMICOLON) { Consume(); return; }
        if (k == NkSLTokenKind::NK_RBRACE)       return;
        if (k == NkSLTokenKind::NK_KW_STRUCT  ||
            k == NkSLTokenKind::NK_KW_UNIFORM ||
            k == NkSLTokenKind::NK_KW_BUFFER  ||
            k == NkSLTokenKind::NK_KW_IN      ||
            k == NkSLTokenKind::NK_KW_OUT     ||
            k == NkSLTokenKind::NK_AT_BINDING ||
            k == NkSLTokenKind::NK_AT_STAGE   ||
            k == NkSLTokenKind::NK_AT_ENTRY   ||
            IsTypeToken(k)) return;
        Consume();
    }
}

// =============================================================================
// Détection de tokens
// =============================================================================
bool NkSLParser::IsTypeToken(NkSLTokenKind k) const {
    return k >= NkSLTokenKind::NK_KW_VOID && k <= NkSLTokenKind::NK_KW_UIMAGE2D;
}

bool NkSLParser::IsQualifierToken(NkSLTokenKind k) const {
    return k == NkSLTokenKind::NK_KW_IN       || k == NkSLTokenKind::NK_KW_OUT     ||
           k == NkSLTokenKind::NK_KW_INOUT    || k == NkSLTokenKind::NK_KW_UNIFORM ||
           k == NkSLTokenKind::NK_KW_BUFFER   || k == NkSLTokenKind::NK_KW_SHARED  ||
           k == NkSLTokenKind::NK_KW_CONST    || k == NkSLTokenKind::NK_KW_SMOOTH  ||
           k == NkSLTokenKind::NK_KW_FLAT     || k == NkSLTokenKind::NK_KW_NOPERSPECTIVE ||
           k == NkSLTokenKind::NK_KW_LOWP     || k == NkSLTokenKind::NK_KW_MEDIUMP ||
           k == NkSLTokenKind::NK_KW_HIGHP    || k == NkSLTokenKind::NK_KW_INVARIANT;
}

// =============================================================================
NkSLBaseType NkSLParser::TokenToBaseType(NkSLTokenKind k) const {
    switch (k) {
        case NkSLTokenKind::NK_KW_VOID:   return NkSLBaseType::NK_VOID;
        case NkSLTokenKind::NK_KW_BOOL:   return NkSLBaseType::NK_BOOL;
        case NkSLTokenKind::NK_KW_INT:    return NkSLBaseType::NK_INT;
        case NkSLTokenKind::NK_KW_UINT:   return NkSLBaseType::NK_UINT;
        case NkSLTokenKind::NK_KW_FLOAT:  return NkSLBaseType::NK_FLOAT;
        case NkSLTokenKind::NK_KW_DOUBLE: return NkSLBaseType::NK_DOUBLE;
        case NkSLTokenKind::NK_KW_IVEC2:  return NkSLBaseType::NK_IVEC2;
        case NkSLTokenKind::NK_KW_IVEC3:  return NkSLBaseType::NK_IVEC3;
        case NkSLTokenKind::NK_KW_IVEC4:  return NkSLBaseType::NK_IVEC4;
        case NkSLTokenKind::NK_KW_UVEC2:  return NkSLBaseType::NK_UVEC2;
        case NkSLTokenKind::NK_KW_UVEC3:  return NkSLBaseType::NK_UVEC3;
        case NkSLTokenKind::NK_KW_UVEC4:  return NkSLBaseType::NK_UVEC4;
        case NkSLTokenKind::NK_KW_VEC2:   return NkSLBaseType::NK_VEC2;
        case NkSLTokenKind::NK_KW_VEC3:   return NkSLBaseType::NK_VEC3;
        case NkSLTokenKind::NK_KW_VEC4:   return NkSLBaseType::NK_VEC4;
        case NkSLTokenKind::NK_KW_DVEC2:  return NkSLBaseType::NK_DVEC2;
        case NkSLTokenKind::NK_KW_DVEC3:  return NkSLBaseType::NK_DVEC3;
        case NkSLTokenKind::NK_KW_DVEC4:  return NkSLBaseType::NK_DVEC4;
        case NkSLTokenKind::NK_KW_MAT2:   return NkSLBaseType::NK_MAT2;
        case NkSLTokenKind::NK_KW_MAT3:   return NkSLBaseType::NK_MAT3;
        case NkSLTokenKind::NK_KW_MAT4:   return NkSLBaseType::NK_MAT4;
        case NkSLTokenKind::NK_KW_MAT2X3: return NkSLBaseType::NK_MAT2X3;
        case NkSLTokenKind::NK_KW_MAT2X4: return NkSLBaseType::NK_MAT2X4;
        case NkSLTokenKind::NK_KW_MAT3X2: return NkSLBaseType::NK_MAT3X2;
        case NkSLTokenKind::NK_KW_MAT3X4: return NkSLBaseType::NK_MAT3X4;
        case NkSLTokenKind::NK_KW_MAT4X2: return NkSLBaseType::NK_MAT4X2;
        case NkSLTokenKind::NK_KW_MAT4X3: return NkSLBaseType::NK_MAT4X3;
        case NkSLTokenKind::NK_KW_DMAT2:  return NkSLBaseType::NK_DMAT2;
        case NkSLTokenKind::NK_KW_DMAT3:  return NkSLBaseType::NK_DMAT3;
        case NkSLTokenKind::NK_KW_DMAT4:  return NkSLBaseType::NK_DMAT4;
        case NkSLTokenKind::NK_KW_SAMPLER2D:            return NkSLBaseType::NK_SAMPLER2D;
        case NkSLTokenKind::NK_KW_SAMPLER2D_SHADOW:     return NkSLBaseType::NK_SAMPLER2D_SHADOW;
        case NkSLTokenKind::NK_KW_SAMPLER2D_ARRAY:      return NkSLBaseType::NK_SAMPLER2D_ARRAY;
        case NkSLTokenKind::NK_KW_SAMPLER2D_ARRAY_SHADOW:return NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW;
        case NkSLTokenKind::NK_KW_SAMPLERCUBE:          return NkSLBaseType::NK_SAMPLER_CUBE;
        case NkSLTokenKind::NK_KW_SAMPLERCUBE_SHADOW:   return NkSLBaseType::NK_SAMPLER_CUBE_SHADOW;
        case NkSLTokenKind::NK_KW_SAMPLER3D:            return NkSLBaseType::NK_SAMPLER3D;
        case NkSLTokenKind::NK_KW_ISAMPLER2D:           return NkSLBaseType::NK_ISAMPLER2D;
        case NkSLTokenKind::NK_KW_USAMPLER2D:           return NkSLBaseType::NK_USAMPLER2D;
        case NkSLTokenKind::NK_KW_IMAGE2D:              return NkSLBaseType::NK_IMAGE2D;
        case NkSLTokenKind::NK_KW_IIMAGE2D:             return NkSLBaseType::NK_IIMAGE2D;
        case NkSLTokenKind::NK_KW_UIMAGE2D:             return NkSLBaseType::NK_UIMAGE2D;
        default: return NkSLBaseType::NK_UNKNOWN;
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
    if (k == NkSLTokenKind::NK_AT_BINDING || k == NkSLTokenKind::NK_AT_LOCATION ||
        k == NkSLTokenKind::NK_AT_STAGE   || k == NkSLTokenKind::NK_AT_ENTRY    ||
        k == NkSLTokenKind::NK_AT_BUILTIN || k == NkSLTokenKind::NK_AT_PUSH_CONSTANT) {
        return ParseAnnotation();
    }

    // struct
    if (k == NkSLTokenKind::NK_KW_STRUCT) return ParseStructDecl();

    // uniform / buffer block
    if (k == NkSLTokenKind::NK_KW_UNIFORM) {
        // uniform Block { ... } ou uniform type name;
        NkSLToken next1 = PeekAt(1);
        if (next1.kind == NkSLTokenKind::NK_IDENT || IsTypeToken(next1.kind)) {
            // Vérifie si c'est un block (uniform IDENT { ...)
            NkSLToken next2 = PeekAt(2);
            if (next2.kind == NkSLTokenKind::NK_LBRACE)
                return ParseBlockDecl(NkSLStorageQual::NK_UNIFORM);
        }
        // uniform type name; → variable globale
        return ParseVarDecl(true);
    }

    if (k == NkSLTokenKind::NK_KW_BUFFER) return ParseBlockDecl(NkSLStorageQual::NK_BUFFER);

    // in / out → variable globale d'interface
    if (k == NkSLTokenKind::NK_KW_IN || k == NkSLTokenKind::NK_KW_OUT) return ParseVarDecl(true);

    // layout(push_constant) uniform Block { ... }
    if (k == NkSLTokenKind::NK_KW_LAYOUT) {
        // Traitement simplifié : on reconnaît layout(push_constant)
        Consume(); // 'layout'
        Consume(NkSLTokenKind::NK_LPAREN, "Expected '(' after 'layout'");
        bool isPushConst = false;
        while (!Check(NkSLTokenKind::NK_RPAREN) && !IsAtEnd()) {
            NkSLToken qual = Consume();
            if (qual.text == "push_constant") isPushConst = true;
            else if (qual.kind == NkSLTokenKind::NK_OP_COMMA) continue;
            // On ignore les autres qualificateurs (binding=N, location=N, etc.)
            else if (qual.kind == NkSLTokenKind::NK_OP_ASSIGN || IsTypeToken(qual.kind)) continue;
            else if (qual.kind == NkSLTokenKind::NK_LIT_INT) continue;
            else if (qual.kind == NkSLTokenKind::NK_IDENT) continue;
        }
        Consume(NkSLTokenKind::NK_RPAREN, "Expected ')' after layout qualifiers");
        if (isPushConst)
            return ParseBlockDecl(NkSLStorageQual::NK_PUSH_CONSTANT);
        return ParseBlockDecl(NkSLStorageQual::NK_UNIFORM);
    }

    // const
    if (k == NkSLTokenKind::NK_KW_CONST) return ParseVarDecl(true);

    // Qualificateurs d'interpolation avant in/out
    if (k == NkSLTokenKind::NK_KW_SMOOTH || k == NkSLTokenKind::NK_KW_FLAT ||
        k == NkSLTokenKind::NK_KW_NOPERSPECTIVE) return ParseVarDecl(true);

    // Précision
    if (k == NkSLTokenKind::NK_KW_LOWP || k == NkSLTokenKind::NK_KW_MEDIUMP ||
        k == NkSLTokenKind::NK_KW_HIGHP) return ParseVarDecl(true);

    // Type → variable globale ou fonction
    if (IsTypeToken(k)) {
        // Lookahead pour distinguer fonction et variable :
        // type IDENT ( ... ) → fonction
        // type IDENT ; ou = ... ; → variable
        NkSLToken name = PeekAt(1);
        if (name.kind == NkSLTokenKind::NK_IDENT) {
            NkSLToken after = PeekAt(2);
            if (after.kind == NkSLTokenKind::NK_LPAREN) return ParseFunctionDecl();
        }
        return ParseVarDecl(true);
    }

    // IDENT : struct ou fonction de type utilisateur
    if (k == NkSLTokenKind::NK_IDENT) {
        NkSLToken after = PeekAt(2);
        if (after.kind == NkSLTokenKind::NK_LPAREN) return ParseFunctionDecl();
        return ParseVarDecl(true);
    }

    // Semicolons parasites
    if (k == NkSLTokenKind::NK_OP_SEMICOLON) { Consume(); return nullptr; }

    Error("Unexpected token at top level: '" + Peek().text + "'");
    Consume();
    return nullptr;
}

// =============================================================================
// Annotation
// =============================================================================
NkSLAnnotationNode* NkSLParser::ParseAnnotation() {
    NkSLTokenKind ak = Peek().kind;
    auto* ann = new NkSLAnnotationNode(NkSLNodeKind::NK_ANNOTATION_BINDING);
    ann->line = Peek().line;
    Consume(); // consume l'annotation

    if (ak == NkSLTokenKind::NK_AT_BINDING || ak == NkSLTokenKind::NK_AT_LOCATION) {
        ann->kind = (ak == NkSLTokenKind::NK_AT_BINDING)
            ? NkSLNodeKind::NK_ANNOTATION_BINDING
            : NkSLNodeKind::NK_ANNOTATION_LOCATION;
        ann->binding = ParseBindingArgs();
    }
    else if (ak == NkSLTokenKind::NK_AT_STAGE) {
        ann->kind = NkSLNodeKind::NK_ANNOTATION_STAGE;
        if (Match(NkSLTokenKind::NK_LPAREN)) {
            NkSLToken stageToken = Consume();
            if (stageToken.text == "vertex")       ann->stage = NkSLStage::NK_VERTEX;
            else if (stageToken.text == "fragment") ann->stage = NkSLStage::NK_FRAGMENT;
            else if (stageToken.text == "compute")  ann->stage = NkSLStage::NK_COMPUTE;
            else if (stageToken.text == "geometry") ann->stage = NkSLStage::NK_GEOMETRY;
            Consume(NkSLTokenKind::NK_RPAREN, "Expected ')' after @stage argument");
        }
    }
    else if (ak == NkSLTokenKind::NK_AT_ENTRY) {
        ann->kind = NkSLNodeKind::NK_ANNOTATION_ENTRY;
        if (Match(NkSLTokenKind::NK_LPAREN)) {
            ann->entryPoint = Consume().text;
            Consume(NkSLTokenKind::NK_RPAREN, "Expected ')' after @entry argument");
        }
    }
    else if (ak == NkSLTokenKind::NK_AT_BUILTIN) {
        ann->kind = NkSLNodeKind::NK_ANNOTATION_BUILTIN;
        if (Match(NkSLTokenKind::NK_LPAREN)) {
            ann->builtinName = Consume().text;
            Consume(NkSLTokenKind::NK_RPAREN, "Expected ')' after @builtin argument");
        }
    }

    return ann;
}

NkSLBinding NkSLParser::ParseBindingArgs() {
    NkSLBinding b;
    if (!Match(NkSLTokenKind::NK_LPAREN)) return b;
    while (!Check(NkSLTokenKind::NK_RPAREN) && !IsAtEnd()) {
        NkSLToken name = Peek();
        if (name.kind == NkSLTokenKind::NK_IDENT) {
            Consume(); // nom
            if (Match(NkSLTokenKind::NK_OP_ASSIGN)) {
                NkSLToken val = Consume(NkSLTokenKind::NK_LIT_INT, "Expected integer binding value");
                if (name.text == "set")      b.set     = (int32)val.intVal;
                if (name.text == "binding")  b.binding = (int32)val.intVal;
                if (name.text == "location") b.location= (int32)val.intVal;
                if (name.text == "offset")   b.offset  = (int32)val.intVal;
            }
        } else if (name.kind == NkSLTokenKind::NK_LIT_INT) {
            // @location(0) syntax courte
            Consume();
            b.location = (int32)name.intVal;
        }
        Match(NkSLTokenKind::NK_OP_COMMA);
    }
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')' after binding args");
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
    } else if (k == NkSLTokenKind::NK_IDENT) {
        // Type utilisateur (struct)
        t->baseType = NkSLBaseType::NK_STRUCT;
        t->typeName = Consume().text;
    } else {
        Error("Expected type name");
        t->baseType = NkSLBaseType::NK_FLOAT;
    }

    // Tableau ?
    if (Match(NkSLTokenKind::NK_LBRACKET)) {
        if (Check(NkSLTokenKind::NK_RBRACKET)) {
            t->isUnsized = true;
            t->arraySize = 0;
        } else if (Check(NkSLTokenKind::NK_LIT_INT)) {
            t->arraySize = (uint32)Consume().intVal;
        }
        Consume(NkSLTokenKind::NK_RBRACKET, "Expected ']'");
        t->kind = NkSLNodeKind::NK_TYPE_ARRAY;
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
        if      (k == NkSLTokenKind::NK_KW_CONST)         { decl->isConst = true; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_INVARIANT)     { decl->isInvariant = true; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_IN)            { decl->storage = NkSLStorageQual::NK_IN; Consume(); decl->kind = NkSLNodeKind::NK_DECL_INPUT; }
        else if (k == NkSLTokenKind::NK_KW_OUT)           { decl->storage = NkSLStorageQual::NK_OUT; Consume(); decl->kind = NkSLNodeKind::NK_DECL_OUTPUT; }
        else if (k == NkSLTokenKind::NK_KW_INOUT)         { decl->storage = NkSLStorageQual::NK_INOUT; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_UNIFORM)       { decl->storage = NkSLStorageQual::NK_UNIFORM; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_SHARED)        { decl->storage = NkSLStorageQual::NK_SHARED; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_SMOOTH)        { decl->interp = NkSLInterpolation::NK_SMOOTH; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_FLAT)          { decl->interp = NkSLInterpolation::NK_FLAT; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_NOPERSPECTIVE) { decl->interp = NkSLInterpolation::NK_NOPERSPECTIVE; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_LOWP)          { decl->precision = NkSLPrecision::NK_LOWP; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_MEDIUMP)       { decl->precision = NkSLPrecision::NK_MEDIUMP; Consume(); }
        else if (k == NkSLTokenKind::NK_KW_HIGHP)         { decl->precision = NkSLPrecision::NK_HIGHP; Consume(); }
        else break;
    }

    // Type
    decl->type = ParseType();

    // Nom
    if (Check(NkSLTokenKind::NK_IDENT)) {
        decl->name = Consume().text;
    } else if (Check(NkSLTokenKind::NK_OP_SEMICOLON)) {
        // Struct anonyme ou déclaration sans nom
    }

    // Initialisation
    if (Match(NkSLTokenKind::NK_OP_ASSIGN)) {
        decl->initializer = ParseAssignment();
    }

    if (expectSemi)
        Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';' after variable declaration");

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
    if (Peek().kind == NkSLTokenKind::NK_KW_UNIFORM ||
        Peek().kind == NkSLTokenKind::NK_KW_BUFFER)
        Consume();

    // Nom du block
    if (Check(NkSLTokenKind::NK_IDENT)) block->blockName = Consume().text;

    Consume(NkSLTokenKind::NK_LBRACE, "Expected '{' for block declaration");

    while (!Check(NkSLTokenKind::NK_RBRACE) && !IsAtEnd()) {
        NkSLVarDeclNode* member = ParseVarDecl(true);
        block->members.PushBack(member);
        block->AddChild(member);
    }

    Consume(NkSLTokenKind::NK_RBRACE, "Expected '}'");

    // Nom d'instance optionnel
    if (Check(NkSLTokenKind::NK_IDENT)) block->instanceName = Consume().text;

    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';' after block declaration");

    block->kind = storage == NkSLStorageQual::NK_BUFFER
        ? NkSLNodeKind::NK_DECL_STORAGE_BLOCK
        : (storage == NkSLStorageQual::NK_PUSH_CONSTANT
           ? NkSLNodeKind::NK_DECL_PUSH_CONSTANT
           : NkSLNodeKind::NK_DECL_UNIFORM_BLOCK);

    return block;
}

// =============================================================================
// Struct
// =============================================================================
NkSLStructDeclNode* NkSLParser::ParseStructDecl() {
    auto* s = new NkSLStructDeclNode();
    s->line = Peek().line;
    Consume(); // 'struct'
    if (Check(NkSLTokenKind::NK_IDENT)) s->name = Consume().text;
    Consume(NkSLTokenKind::NK_LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::NK_RBRACE) && !IsAtEnd()) {
        NkSLVarDeclNode* m = ParseVarDecl(true);
        s->members.PushBack(m);
        s->AddChild(m);
    }
    Consume(NkSLTokenKind::NK_RBRACE, "Expected '}'");
    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';' after struct");
    return s;
}

// =============================================================================
// Fonction
// =============================================================================
NkSLFunctionDeclNode* NkSLParser::ParseFunctionDecl(NkSLVarDeclNode*) {
    auto* fn = new NkSLFunctionDeclNode();
    fn->line = Peek().line;

    fn->returnType = ParseType();
    fn->name = Consume(NkSLTokenKind::NK_IDENT, "Expected function name").text;

    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    while (!Check(NkSLTokenKind::NK_RPAREN) && !IsAtEnd()) {
        auto* param = new NkSLParamNode();
        param->line = Peek().line;
        // Qualificateur inout/in/out
        if (Check(NkSLTokenKind::NK_KW_IN))    { param->storage = NkSLStorageQual::NK_IN;    Consume(); }
        if (Check(NkSLTokenKind::NK_KW_OUT))   { param->storage = NkSLStorageQual::NK_OUT;   Consume(); }
        if (Check(NkSLTokenKind::NK_KW_INOUT)) { param->storage = NkSLStorageQual::NK_INOUT; Consume(); }
        param->type = ParseType();
        if (Check(NkSLTokenKind::NK_IDENT)) param->name = Consume().text;
        fn->params.PushBack(param);
        fn->AddChild(param);
        if (!Match(NkSLTokenKind::NK_OP_COMMA)) break;
    }
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");

    // Prototype uniquement (pas de corps)
    if (Match(NkSLTokenKind::NK_OP_SEMICOLON)) {
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
    Consume(NkSLTokenKind::NK_LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::NK_RBRACE) && !IsAtEnd()) {
        NkSLNode* stmt = ParseStatement();
        if (stmt) block->AddChild(stmt);
    }
    Consume(NkSLTokenKind::NK_RBRACE, "Expected '}'");
    return block;
}

NkSLNode* NkSLParser::ParseStatement() {
    switch (Peek().kind) {
        case NkSLTokenKind::NK_LBRACE:      return ParseBlock();
        case NkSLTokenKind::NK_KW_IF:       return ParseIfStmt();
        case NkSLTokenKind::NK_KW_FOR:      return ParseForStmt();
        case NkSLTokenKind::NK_KW_WHILE:    return ParseWhileStmt();
        case NkSLTokenKind::NK_KW_DO:       return ParseDoWhileStmt();
        case NkSLTokenKind::NK_KW_RETURN:   return ParseReturnStmt();
        case NkSLTokenKind::NK_KW_SWITCH:   return ParseSwitchStmt();
        case NkSLTokenKind::NK_KW_BREAK: {
            auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_BREAK, Peek().line);
            Consume(); Consume(NkSLTokenKind::NK_OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::NK_KW_CONTINUE: {
            auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_CONTINUE, Peek().line);
            Consume(); Consume(NkSLTokenKind::NK_OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::NK_KW_DISCARD: {
            auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_DISCARD, Peek().line);
            Consume(); Consume(NkSLTokenKind::NK_OP_SEMICOLON,"Expected ';'"); return n;
        }
        case NkSLTokenKind::NK_OP_SEMICOLON:
            Consume(); return nullptr;
        default:
            // Déclaration locale ou expression
            if (IsTypeToken(Peek().kind) || IsQualifierToken(Peek().kind) ||
                Peek().kind == NkSLTokenKind::NK_KW_CONST) {
                // Vérifie si c'est vraiment une déclaration (type IDENT)
                if ((IsTypeToken(Peek().kind) || Peek().kind == NkSLTokenKind::NK_KW_CONST)
                    && PeekAt(1).kind == NkSLTokenKind::NK_IDENT) {
                    return ParseVarDecl(true);
                }
            }
            return ParseExprStmt();
    }
}

NkSLNode* NkSLParser::ParseIfStmt() {
    auto* n = new NkSLIfNode(); n->line = Peek().line;
    Consume(); // 'if'
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    n->condition = ParseExpression();
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    n->thenBranch = ParseStatement();
    if (Match(NkSLTokenKind::NK_KW_ELSE)) n->elseBranch = ParseStatement();
    n->AddChild(n->condition); n->AddChild(n->thenBranch);
    if (n->elseBranch) n->AddChild(n->elseBranch);
    return n;
}

NkSLNode* NkSLParser::ParseForStmt() {
    auto* n = new NkSLForNode(); n->line = Peek().line;
    Consume(); // 'for'
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    if (!Check(NkSLTokenKind::NK_OP_SEMICOLON)) {
        if (IsTypeToken(Peek().kind) && PeekAt(1).kind == NkSLTokenKind::NK_IDENT)
            n->init = ParseVarDecl(true);
        else
            n->init = ParseExprStmt();
    } else { Consume(); }
    if (!Check(NkSLTokenKind::NK_OP_SEMICOLON)) n->condition = ParseExpression();
    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';'");
    if (!Check(NkSLTokenKind::NK_RPAREN)) n->increment = ParseExpression();
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    n->body = ParseStatement();
    return n;
}

NkSLNode* NkSLParser::ParseWhileStmt() {
    auto* n = new NkSLWhileNode(); n->line = Peek().line;
    Consume(); // 'while'
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    n->condition = ParseExpression();
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    n->body = ParseStatement();
    n->AddChild(n->condition); n->AddChild(n->body);
    return n;
}

NkSLNode* NkSLParser::ParseDoWhileStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_DO_WHILE, Peek().line);
    Consume(); // 'do'
    n->AddChild(ParseStatement());
    Consume(NkSLTokenKind::NK_KW_WHILE, "Expected 'while'");
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';'");
    return n;
}

NkSLNode* NkSLParser::ParseReturnStmt() {
    auto* n = new NkSLReturnNode(); n->line = Peek().line;
    Consume(); // 'return'
    if (!Check(NkSLTokenKind::NK_OP_SEMICOLON)) {
        n->value = ParseExpression();
        n->AddChild(n->value);
    }
    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';'");
    return n;
}

NkSLNode* NkSLParser::ParseSwitchStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_SWITCH, Peek().line);
    Consume(); // 'switch'
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    Consume(NkSLTokenKind::NK_LBRACE, "Expected '{'");
    while (!Check(NkSLTokenKind::NK_RBRACE) && !IsAtEnd()) {
        if (Match(NkSLTokenKind::NK_KW_CASE)) {
            auto* cas = new NkSLNode(NkSLNodeKind::NK_STMT_CASE, Peek().line);
            cas->AddChild(ParseExpression());
            Consume(NkSLTokenKind::NK_OP_COLON, "Expected ':' after case");
            n->AddChild(cas);
        } else if (Match(NkSLTokenKind::NK_KW_DEFAULT)) {
            Consume(NkSLTokenKind::NK_OP_COLON, "Expected ':' after default");
            n->AddChild(new NkSLNode(NkSLNodeKind::NK_STMT_CASE, Peek().line));
        } else {
            NkSLNode* stmt = ParseStatement();
            if (stmt) n->AddChild(stmt);
        }
    }
    Consume(NkSLTokenKind::NK_RBRACE, "Expected '}'");
    return n;
}

NkSLNode* NkSLParser::ParseExprStmt() {
    auto* n = new NkSLNode(NkSLNodeKind::NK_STMT_EXPR, Peek().line);
    if (!Check(NkSLTokenKind::NK_OP_SEMICOLON))
        n->AddChild(ParseExpression());
    Consume(NkSLTokenKind::NK_OP_SEMICOLON, "Expected ';'");
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
    if (k == NkSLTokenKind::NK_OP_ASSIGN      ||
        k == NkSLTokenKind::NK_OP_PLUS_ASSIGN  || k == NkSLTokenKind::NK_OP_MINUS_ASSIGN ||
        k == NkSLTokenKind::NK_OP_STAR_ASSIGN  || k == NkSLTokenKind::NK_OP_SLASH_ASSIGN ||
        k == NkSLTokenKind::NK_OP_AND_ASSIGN   || k == NkSLTokenKind::NK_OP_OR_ASSIGN    ||
        k == NkSLTokenKind::NK_OP_XOR_ASSIGN   ||
        k == NkSLTokenKind::NK_OP_LSHIFT_ASSIGN|| k == NkSLTokenKind::NK_OP_RSHIFT_ASSIGN) {
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
    if (Match(NkSLTokenKind::NK_OP_QUESTION)) {
        auto* t = new NkSLNode(NkSLNodeKind::NK_EXPR_TERNARY, Peek().line);
        t->AddChild(cond);
        t->AddChild(ParseExpression());
        Consume(NkSLTokenKind::NK_OP_COLON, "Expected ':' in ternary");
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

PARSE_BINARY(ParseLogicalOr,       ParseLogicalAnd,      NkSLTokenKind::NK_OP_LOR)
PARSE_BINARY(ParseLogicalAnd,      ParseBitwiseOr,       NkSLTokenKind::NK_OP_LAND)
PARSE_BINARY(ParseBitwiseOr,       ParseBitwiseXor,      NkSLTokenKind::NK_OP_OR)
PARSE_BINARY(ParseBitwiseXor,      ParseBitwiseAnd,      NkSLTokenKind::NK_OP_XOR)
PARSE_BINARY(ParseBitwiseAnd,      ParseEquality,        NkSLTokenKind::NK_OP_AND)
PARSE_BINARY(ParseEquality,        ParseRelational,      NkSLTokenKind::NK_OP_EQ, NkSLTokenKind::NK_OP_NEQ)
PARSE_BINARY(ParseRelational,      ParseShift,           NkSLTokenKind::NK_OP_LT, NkSLTokenKind::NK_OP_GT,
                                                         NkSLTokenKind::NK_OP_LE, NkSLTokenKind::NK_OP_GE)
PARSE_BINARY(ParseShift,           ParseAdditive,        NkSLTokenKind::NK_OP_LSHIFT, NkSLTokenKind::NK_OP_RSHIFT)
PARSE_BINARY(ParseAdditive,        ParseMultiplicative,  NkSLTokenKind::NK_OP_PLUS, NkSLTokenKind::NK_OP_MINUS)
PARSE_BINARY(ParseMultiplicative,  ParseUnary,           NkSLTokenKind::NK_OP_STAR, NkSLTokenKind::NK_OP_SLASH,
                                                         NkSLTokenKind::NK_OP_PERCENT)
#undef PARSE_BINARY

NkSLNode* NkSLParser::ParseUnary() {
    NkSLTokenKind k = Peek().kind;
    if (k == NkSLTokenKind::NK_OP_MINUS || k == NkSLTokenKind::NK_OP_LNOT ||
        k == NkSLTokenKind::NK_OP_TILDE || k == NkSLTokenKind::NK_OP_PLUS  ||
        k == NkSLTokenKind::NK_OP_INC   || k == NkSLTokenKind::NK_OP_DEC) {
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
        if (k == NkSLTokenKind::NK_OP_DOT) {
            Consume();
            auto* m = new NkSLMemberNode();
            m->object = base;
            m->member = Consume(NkSLTokenKind::NK_IDENT, "Expected member name").text;
            m->AddChild(base);
            base = m;
        }
        else if (k == NkSLTokenKind::NK_LBRACKET) {
            Consume();
            auto* idx = new NkSLIndexNode();
            idx->array = base; idx->index = ParseExpression();
            Consume(NkSLTokenKind::NK_RBRACKET, "Expected ']'");
            idx->AddChild(base); idx->AddChild(idx->index);
            base = idx;
        }
        else if (k == NkSLTokenKind::NK_LPAREN) {
            // Appel de fonction via expr (méthode)
            Consume();
            auto* call = new NkSLCallNode();
            call->calleeExpr = base;
            while (!Check(NkSLTokenKind::NK_RPAREN) && !IsAtEnd()) {
                call->args.PushBack(ParseAssignment());
                call->AddChild(call->args.Back());
                if (!Match(NkSLTokenKind::NK_OP_COMMA)) break;
            }
            Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
            base = call;
        }
        else if (k == NkSLTokenKind::NK_OP_INC || k == NkSLTokenKind::NK_OP_DEC) {
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
    if (tok.kind == NkSLTokenKind::NK_LIT_INT    ||
        tok.kind == NkSLTokenKind::NK_LIT_UINT   ||
        tok.kind == NkSLTokenKind::NK_LIT_FLOAT  ||
        tok.kind == NkSLTokenKind::NK_LIT_DOUBLE ||
        tok.kind == NkSLTokenKind::NK_LIT_BOOL) {
        Consume();
        auto* lit = new NkSLLiteralNode();
        lit->line = tok.line;
        switch (tok.kind) {
            case NkSLTokenKind::NK_LIT_INT:    lit->baseType=NkSLBaseType::NK_INT;    lit->intVal=tok.intVal;     break;
            case NkSLTokenKind::NK_LIT_UINT:   lit->baseType=NkSLBaseType::NK_UINT;   lit->uintVal=tok.uintVal;   break;
            case NkSLTokenKind::NK_LIT_FLOAT:  lit->baseType=NkSLBaseType::NK_FLOAT;  lit->floatVal=tok.floatVal; break;
            case NkSLTokenKind::NK_LIT_DOUBLE: lit->baseType=NkSLBaseType::NK_DOUBLE; lit->floatVal=tok.floatVal; break;
            case NkSLTokenKind::NK_LIT_BOOL:   lit->baseType=NkSLBaseType::NK_BOOL;   lit->boolVal=tok.boolVal;   break;
            default: break;
        }
        return lit;
    }

    // Builtin
    if (tok.kind >= NkSLTokenKind::NK_KW_BUILTIN_POSITION &&
        tok.kind <= NkSLTokenKind::NK_KW_BUILTIN_WORKINVID) {
        Consume();
        auto* id = new NkSLIdentNode();
        id->name = tok.text; id->line = tok.line;
        return id;
    }

    // Type constructeur ou appel de fonction
    if (IsTypeToken(tok.kind)) {
        NkString typeName = Consume().text;
        if (Check(NkSLTokenKind::NK_LPAREN))
            return ParseCallOrCast(typeName);
        // Type seul (improbable ici, mais safe)
        auto* id = new NkSLIdentNode(); id->name = typeName; id->line = tok.line; return id;
    }

    // Identifiant
    if (tok.kind == NkSLTokenKind::NK_IDENT) {
        NkString name = Consume().text;
        if (Check(NkSLTokenKind::NK_LPAREN))
            return ParseCallOrCast(name);
        auto* id = new NkSLIdentNode(); id->name = name; id->line = tok.line; return id;
    }

    // Parenthèse
    if (tok.kind == NkSLTokenKind::NK_LPAREN) {
        Consume();
        NkSLNode* expr = ParseExpression();
        Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
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
    Consume(NkSLTokenKind::NK_LPAREN, "Expected '('");
    while (!Check(NkSLTokenKind::NK_RPAREN) && !IsAtEnd()) {
        call->args.PushBack(ParseAssignment());
        call->AddChild(call->args.Back());
        if (!Match(NkSLTokenKind::NK_OP_COMMA)) break;
    }
    Consume(NkSLTokenKind::NK_RPAREN, "Expected ')'");
    return call;
}

} // namespace nkentseu
