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
    LIT_INT, LIT_UINT, LIT_FLOAT, LIT_DOUBLE, LIT_BOOL, LIT_STRING,
    // Identifiants
    IDENT,
    // Mots-clés de types
    KW_VOID,
    KW_BOOL, KW_INT, KW_UINT, KW_FLOAT, KW_DOUBLE,
    KW_BVEC2, KW_BVEC3, KW_BVEC4,
    KW_IVEC2, KW_IVEC3, KW_IVEC4,
    KW_UVEC2, KW_UVEC3, KW_UVEC4,
    KW_VEC2, KW_VEC3, KW_VEC4,
    KW_DVEC2, KW_DVEC3, KW_DVEC4,
    KW_MAT2, KW_MAT3, KW_MAT4,
    KW_MAT2X2, KW_MAT2X3, KW_MAT2X4,
    KW_MAT3X2, KW_MAT3X3, KW_MAT3X4,
    KW_MAT4X2, KW_MAT4X3, KW_MAT4X4,
    KW_DMAT2, KW_DMAT3, KW_DMAT4,
    // Samplers
    KW_SAMPLER2D, KW_SAMPLER2D_SHADOW,
    KW_SAMPLER2D_ARRAY, KW_SAMPLER2D_ARRAY_SHADOW,
    KW_SAMPLERCUBE, KW_SAMPLERCUBE_SHADOW,
    KW_SAMPLER3D,
    KW_ISAMPLER2D, KW_USAMPLER2D,
    // Images
    KW_IMAGE2D, KW_IIMAGE2D, KW_UIMAGE2D,
    // Qualificateurs de stockage
    KW_IN, KW_OUT, KW_INOUT,
    KW_UNIFORM, KW_BUFFER,
    KW_PUSH_CONSTANT,
    KW_SHARED, KW_WORKGROUP,
    // Interpolation
    KW_SMOOTH, KW_FLAT, KW_NOPERSPECTIVE,
    // Précision
    KW_LOWP, KW_MEDIUMP, KW_HIGHP,
    // Contrôle de flux
    KW_IF, KW_ELSE, KW_FOR, KW_WHILE, KW_DO,
    KW_BREAK, KW_CONTINUE, KW_RETURN, KW_DISCARD,
    KW_SWITCH, KW_CASE, KW_DEFAULT,
    // Struct / block
    KW_STRUCT, KW_LAYOUT,
    // Qualificateurs compute
    KW_LOCAL_SIZE_X, KW_LOCAL_SIZE_Y, KW_LOCAL_SIZE_Z,
    // Const
    KW_CONST, KW_INVARIANT,
    // Annotations NkSL (extensions au-delà de GLSL standard)
    AT_BINDING,        // @binding(set=0, binding=1)
    AT_LOCATION,       // @location(0)
    AT_PUSH_CONSTANT,  // @push_constant
    AT_STAGE,          // @stage(vertex)
    AT_ENTRY,          // @entry
    AT_BUILTIN,        // @builtin(position)
    // Builtins
    KW_BUILTIN_POSITION,
    KW_BUILTIN_FRAGCOORD,
    KW_BUILTIN_FRAGDEPTH,
    KW_BUILTIN_VERTEXID,
    KW_BUILTIN_INSTANCEID,
    KW_BUILTIN_FRONTFACING,
    KW_BUILTIN_LOCALINVID,
    KW_BUILTIN_GLOBALINVID,
    KW_BUILTIN_WORKINVID,
    // Opérateurs
    OP_PLUS, OP_MINUS, OP_STAR, OP_SLASH, OP_PERCENT,
    OP_AND, OP_OR, OP_XOR, OP_NOT, OP_TILDE,
    OP_LSHIFT, OP_RSHIFT,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_LAND, OP_LOR, OP_LNOT,
    OP_ASSIGN,
    OP_PLUS_ASSIGN, OP_MINUS_ASSIGN, OP_STAR_ASSIGN, OP_SLASH_ASSIGN,
    OP_AND_ASSIGN, OP_OR_ASSIGN, OP_XOR_ASSIGN,
    OP_LSHIFT_ASSIGN, OP_RSHIFT_ASSIGN,
    OP_INC, OP_DEC,
    OP_DOT, OP_COMMA, OP_SEMICOLON, OP_COLON, OP_QUESTION,
    // Délimiteurs
    LPAREN, RPAREN,
    LBRACKET, RBRACKET,
    LBRACE, RBRACE,
    AT,       // @
    HASH,     // #
    // Spéciaux
    END_OF_FILE,
    UNKNOWN,
};

struct NkSLToken {
    NkSLTokenKind kind   = NkSLTokenKind::UNKNOWN;
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
