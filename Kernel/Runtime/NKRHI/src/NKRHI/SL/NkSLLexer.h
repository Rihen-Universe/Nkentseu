#pragma once
// =============================================================================
// NkSLLexer.h
// Analyse lexicale du langage NkSL.
// NkSL utilise une syntaxe GLSL-like avec des annotations @ pour les bindings.
// =============================================================================
#include "NkSLTypes.h"

namespace nkentseu {

// =============================================================================
// Tokens
// =============================================================================
enum class NkSLTokenKind : uint32 {
    // Littéraux
    NK_LIT_INT, NK_LIT_UINT, NK_LIT_FLOAT, NK_LIT_DOUBLE, NK_LIT_BOOL, NK_LIT_STRING,
    // Identifiants
    NK_IDENT,
    // Mots-clés de types
    NK_KW_VOID,
    NK_KW_BOOL, NK_KW_INT, NK_KW_UINT, NK_KW_FLOAT, NK_KW_DOUBLE,
    NK_KW_BVEC2, NK_KW_BVEC3, NK_KW_BVEC4,
    NK_KW_IVEC2, NK_KW_IVEC3, NK_KW_IVEC4,
    NK_KW_UVEC2, NK_KW_UVEC3, NK_KW_UVEC4,
    NK_KW_VEC2, NK_KW_VEC3, NK_KW_VEC4,
    NK_KW_DVEC2, NK_KW_DVEC3, NK_KW_DVEC4,
    NK_KW_MAT2, NK_KW_MAT3, NK_KW_MAT4,
    NK_KW_MAT2X2, NK_KW_MAT2X3, NK_KW_MAT2X4,
    NK_KW_MAT3X2, NK_KW_MAT3X3, NK_KW_MAT3X4,
    NK_KW_MAT4X2, NK_KW_MAT4X3, NK_KW_MAT4X4,
    NK_KW_DMAT2, NK_KW_DMAT3, NK_KW_DMAT4,
    // Samplers
    NK_KW_SAMPLER2D, NK_KW_SAMPLER2D_SHADOW,
    NK_KW_SAMPLER2D_ARRAY, NK_KW_SAMPLER2D_ARRAY_SHADOW,
    NK_KW_SAMPLERCUBE, NK_KW_SAMPLERCUBE_SHADOW,
    NK_KW_SAMPLER3D,
    NK_KW_ISAMPLER2D, NK_KW_USAMPLER2D,
    // Images
    NK_KW_IMAGE2D, NK_KW_IIMAGE2D, NK_KW_UIMAGE2D,
    // Qualificateurs de stockage
    NK_KW_IN, NK_KW_OUT, NK_KW_INOUT,
    NK_KW_UNIFORM, NK_KW_BUFFER,
    NK_KW_PUSH_CONSTANT,
    NK_KW_SHARED, NK_KW_WORKGROUP,
    // Interpolation
    NK_KW_SMOOTH, NK_KW_FLAT, NK_KW_NOPERSPECTIVE,
    // Précision
    NK_KW_LOWP, NK_KW_MEDIUMP, NK_KW_HIGHP,
    // Contrôle de flux
    NK_KW_IF, NK_KW_ELSE, NK_KW_FOR, NK_KW_WHILE, NK_KW_DO,
    NK_KW_BREAK, NK_KW_CONTINUE, NK_KW_RETURN, NK_KW_DISCARD,
    NK_KW_SWITCH, NK_KW_CASE, NK_KW_DEFAULT,
    // Struct / block
    NK_KW_STRUCT, NK_KW_LAYOUT,
    // Qualificateurs compute
    NK_KW_LOCAL_SIZE_X, NK_KW_LOCAL_SIZE_Y, NK_KW_LOCAL_SIZE_Z,
    // Const
    NK_KW_CONST, NK_KW_INVARIANT,
    // Annotations NkSL (extensions au-delà de GLSL standard)
    NK_AT_BINDING,        // @binding(set=0, binding=1)
    NK_AT_LOCATION,       // @location(0)
    NK_AT_PUSH_CONSTANT,  // @push_constant
    NK_AT_STAGE,          // @stage(vertex)
    NK_AT_ENTRY,          // @entry
    NK_AT_BUILTIN,        // @builtin(position)
    // Builtins
    NK_KW_BUILTIN_POSITION,
    NK_KW_BUILTIN_FRAGCOORD,
    NK_KW_BUILTIN_FRAGDEPTH,
    NK_KW_BUILTIN_VERTEXID,
    NK_KW_BUILTIN_INSTANCEID,
    NK_KW_BUILTIN_FRONTFACING,
    NK_KW_BUILTIN_LOCALINVID,
    NK_KW_BUILTIN_GLOBALINVID,
    NK_KW_BUILTIN_WORKINVID,
    // Opérateurs
    NK_OP_PLUS, NK_OP_MINUS, NK_OP_STAR, NK_OP_SLASH, NK_OP_PERCENT,
    NK_OP_AND, NK_OP_OR, NK_OP_XOR, NK_OP_NOT, NK_OP_TILDE,
    NK_OP_LSHIFT, NK_OP_RSHIFT,
    NK_OP_EQ, NK_OP_NEQ, NK_OP_LT, NK_OP_GT, NK_OP_LE, NK_OP_GE,
    NK_OP_LAND, NK_OP_LOR, NK_OP_LNOT,
    NK_OP_ASSIGN,
    NK_OP_PLUS_ASSIGN, NK_OP_MINUS_ASSIGN, NK_OP_STAR_ASSIGN, NK_OP_SLASH_ASSIGN,
    NK_OP_AND_ASSIGN, NK_OP_OR_ASSIGN, NK_OP_XOR_ASSIGN,
    NK_OP_LSHIFT_ASSIGN, NK_OP_RSHIFT_ASSIGN,
    NK_OP_INC, NK_OP_DEC,
    NK_OP_DOT, NK_OP_COMMA, NK_OP_SEMICOLON, NK_OP_COLON, NK_OP_QUESTION,
    // Délimiteurs
    NK_LPAREN, NK_RPAREN,
    NK_LBRACKET, NK_RBRACKET,
    NK_LBRACE, NK_RBRACE,
    NK_AT,       // @
    NK_HASH,     // #
    // Spéciaux
    NK_END_OF_FILE,
    NK_UNKNOWN,
};

struct NkSLToken {
    NkSLTokenKind kind   = NkSLTokenKind::NK_UNKNOWN;
    NkString      text;       // texte brut du token
    uint32        line   = 0;
    uint32        column = 0;
    // Valeur pour les littéraux
    union {
        int64   intVal   = 0;
        uint64  uintVal;
        double  floatVal;
        bool    boolVal;
    };
};

// =============================================================================
// NkSLLexer
// =============================================================================
class NkSLLexer {
public:
    explicit NkSLLexer(const NkString& source, const NkString& filename = "shader");

    // Consomme le prochain token
    NkSLToken Next();
    // Regarde le prochain token sans le consommer
    NkSLToken Peek();
    // Regarde N tokens en avance
    NkSLToken PeekAt(uint32 offset);

    bool      IsAtEnd() const;
    uint32    GetLine()  const { return mLine; }
    uint32    GetColumn()const { return mColumn; }

    const NkVector<NkSLCompileError>& GetErrors() const { return mErrors; }

private:
    void         SkipWhitespaceAndComments();
    NkSLToken    ReadToken();
    NkSLToken    ReadNumber();
    NkSLToken    ReadIdentOrKeyword();
    NkSLToken    ReadAnnotation();       // commence par @
    NkSLToken    ReadPreprocessor();     // commence par #
    NkSLToken    ReadStringLiteral();
    NkSLToken    ReadOperator();

    char         Current() const;
    char         LookAhead(uint32 offset=1) const;
    char         Advance();
    bool         Match(char expected);
    bool         MatchStr(const char* str);

    void         Error(const NkString& msg);

    static NkSLTokenKind KeywordOrIdent(const NkString& s);
    static bool          IsAlpha(char c);
    static bool          IsDigit(char c);
    static bool          IsAlNum(char c);
    static bool          IsHexDigit(char c);

    NkString                  mSource;
    NkString                  mFilename;
    uint32                    mPos    = 0;
    uint32                    mLine   = 1;
    uint32                    mColumn = 1;
    NkVector<NkSLToken>       mLookahead; // buffer de lookahead
    NkVector<NkSLCompileError>mErrors;
};

} // namespace nkentseu
