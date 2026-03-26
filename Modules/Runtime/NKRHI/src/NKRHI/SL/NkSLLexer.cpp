// =============================================================================
// NkSLLexer.cpp
// =============================================================================
#include "NkSLLexer.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstring>
#include <cctype>

namespace nkentseu {

// =============================================================================
// Table des mots-clés
// =============================================================================
struct KwEntry { const char* text; NkSLTokenKind kind; };

static const KwEntry kKeywords[] = {
    // Types scalaires
    {"void",    NkSLTokenKind::NK_KW_VOID},
    {"bool",    NkSLTokenKind::NK_KW_BOOL},
    {"int",     NkSLTokenKind::NK_KW_INT},
    {"uint",    NkSLTokenKind::NK_KW_UINT},
    {"float",   NkSLTokenKind::NK_KW_FLOAT},
    {"double",  NkSLTokenKind::NK_KW_DOUBLE},
    // Vecteurs bool
    {"bvec2",   NkSLTokenKind::NK_KW_BVEC2},
    {"bvec3",   NkSLTokenKind::NK_KW_BVEC3},
    {"bvec4",   NkSLTokenKind::NK_KW_BVEC4},
    // Vecteurs int
    {"ivec2",   NkSLTokenKind::NK_KW_IVEC2},
    {"ivec3",   NkSLTokenKind::NK_KW_IVEC3},
    {"ivec4",   NkSLTokenKind::NK_KW_IVEC4},
    // Vecteurs uint
    {"uvec2",   NkSLTokenKind::NK_KW_UVEC2},
    {"uvec3",   NkSLTokenKind::NK_KW_UVEC3},
    {"uvec4",   NkSLTokenKind::NK_KW_UVEC4},
    // Vecteurs float
    {"vec2",    NkSLTokenKind::NK_KW_VEC2},
    {"vec3",    NkSLTokenKind::NK_KW_VEC3},
    {"vec4",    NkSLTokenKind::NK_KW_VEC4},
    // Vecteurs double
    {"dvec2",   NkSLTokenKind::NK_KW_DVEC2},
    {"dvec3",   NkSLTokenKind::NK_KW_DVEC3},
    {"dvec4",   NkSLTokenKind::NK_KW_DVEC4},
    // Matrices
    {"mat2",    NkSLTokenKind::NK_KW_MAT2},
    {"mat3",    NkSLTokenKind::NK_KW_MAT3},
    {"mat4",    NkSLTokenKind::NK_KW_MAT4},
    {"mat2x2",  NkSLTokenKind::NK_KW_MAT2X2},
    {"mat2x3",  NkSLTokenKind::NK_KW_MAT2X3},
    {"mat2x4",  NkSLTokenKind::NK_KW_MAT2X4},
    {"mat3x2",  NkSLTokenKind::NK_KW_MAT3X2},
    {"mat3x3",  NkSLTokenKind::NK_KW_MAT3X3},
    {"mat3x4",  NkSLTokenKind::NK_KW_MAT3X4},
    {"mat4x2",  NkSLTokenKind::NK_KW_MAT4X2},
    {"mat4x3",  NkSLTokenKind::NK_KW_MAT4X3},
    {"mat4x4",  NkSLTokenKind::NK_KW_MAT4X4},
    {"dmat2",   NkSLTokenKind::NK_KW_DMAT2},
    {"dmat3",   NkSLTokenKind::NK_KW_DMAT3},
    {"dmat4",   NkSLTokenKind::NK_KW_DMAT4},
    // Samplers
    {"sampler2D",            NkSLTokenKind::NK_KW_SAMPLER2D},
    {"sampler2DShadow",      NkSLTokenKind::NK_KW_SAMPLER2D_SHADOW},
    {"sampler2DArray",       NkSLTokenKind::NK_KW_SAMPLER2D_ARRAY},
    {"sampler2DArrayShadow", NkSLTokenKind::NK_KW_SAMPLER2D_ARRAY_SHADOW},
    {"samplerCube",          NkSLTokenKind::NK_KW_SAMPLERCUBE},
    {"samplerCubeShadow",    NkSLTokenKind::NK_KW_SAMPLERCUBE_SHADOW},
    {"sampler3D",            NkSLTokenKind::NK_KW_SAMPLER3D},
    {"isampler2D",           NkSLTokenKind::NK_KW_ISAMPLER2D},
    {"usampler2D",           NkSLTokenKind::NK_KW_USAMPLER2D},
    // Images
    {"image2D",    NkSLTokenKind::NK_KW_IMAGE2D},
    {"iimage2D",   NkSLTokenKind::NK_KW_IIMAGE2D},
    {"uimage2D",   NkSLTokenKind::NK_KW_UIMAGE2D},
    // Qualificateurs de stockage
    {"in",             NkSLTokenKind::NK_KW_IN},
    {"out",            NkSLTokenKind::NK_KW_OUT},
    {"inout",          NkSLTokenKind::NK_KW_INOUT},
    {"uniform",        NkSLTokenKind::NK_KW_UNIFORM},
    {"buffer",         NkSLTokenKind::NK_KW_BUFFER},
    {"push_constant",  NkSLTokenKind::NK_KW_PUSH_CONSTANT},
    {"shared",         NkSLTokenKind::NK_KW_SHARED},
    {"workgroup",      NkSLTokenKind::NK_KW_WORKGROUP},
    // Interpolation
    {"smooth",         NkSLTokenKind::NK_KW_SMOOTH},
    {"flat",           NkSLTokenKind::NK_KW_FLAT},
    {"noperspective",  NkSLTokenKind::NK_KW_NOPERSPECTIVE},
    // Précision
    {"lowp",           NkSLTokenKind::NK_KW_LOWP},
    {"mediump",        NkSLTokenKind::NK_KW_MEDIUMP},
    {"highp",          NkSLTokenKind::NK_KW_HIGHP},
    // Flux
    {"if",       NkSLTokenKind::NK_KW_IF},
    {"else",     NkSLTokenKind::NK_KW_ELSE},
    {"for",      NkSLTokenKind::NK_KW_FOR},
    {"while",    NkSLTokenKind::NK_KW_WHILE},
    {"do",       NkSLTokenKind::NK_KW_DO},
    {"break",    NkSLTokenKind::NK_KW_BREAK},
    {"continue", NkSLTokenKind::NK_KW_CONTINUE},
    {"return",   NkSLTokenKind::NK_KW_RETURN},
    {"discard",  NkSLTokenKind::NK_KW_DISCARD},
    {"switch",   NkSLTokenKind::NK_KW_SWITCH},
    {"case",     NkSLTokenKind::NK_KW_CASE},
    {"default",  NkSLTokenKind::NK_KW_DEFAULT},
    // Struct / layout
    {"struct",   NkSLTokenKind::NK_KW_STRUCT},
    {"layout",   NkSLTokenKind::NK_KW_LAYOUT},
    // Const
    {"const",    NkSLTokenKind::NK_KW_CONST},
    {"invariant",NkSLTokenKind::NK_KW_INVARIANT},
    // Builtins
    {"gl_Position",        NkSLTokenKind::NK_KW_BUILTIN_POSITION},
    {"gl_FragCoord",       NkSLTokenKind::NK_KW_BUILTIN_FRAGCOORD},
    {"gl_FragDepth",       NkSLTokenKind::NK_KW_BUILTIN_FRAGDEPTH},
    {"gl_VertexID",        NkSLTokenKind::NK_KW_BUILTIN_VERTEXID},
    {"gl_InstanceID",      NkSLTokenKind::NK_KW_BUILTIN_INSTANCEID},
    {"gl_FrontFacing",     NkSLTokenKind::NK_KW_BUILTIN_FRONTFACING},
    {"gl_LocalInvocationID",  NkSLTokenKind::NK_KW_BUILTIN_LOCALINVID},
    {"gl_GlobalInvocationID", NkSLTokenKind::NK_KW_BUILTIN_GLOBALINVID},
    {"gl_WorkGroupID",        NkSLTokenKind::NK_KW_BUILTIN_WORKINVID},
    // Booléens
    {"true",  NkSLTokenKind::NK_LIT_BOOL},
    {"false", NkSLTokenKind::NK_LIT_BOOL},
    {nullptr, NkSLTokenKind::NK_UNKNOWN},
};

// Annotations (commencent par @)
struct AtEntry { const char* text; NkSLTokenKind kind; };
static const AtEntry kAnnotations[] = {
    {"binding",       NkSLTokenKind::NK_AT_BINDING},
    {"location",      NkSLTokenKind::NK_AT_LOCATION},
    {"push_constant", NkSLTokenKind::NK_AT_PUSH_CONSTANT},
    {"stage",         NkSLTokenKind::NK_AT_STAGE},
    {"entry",         NkSLTokenKind::NK_AT_ENTRY},
    {"builtin",       NkSLTokenKind::NK_AT_BUILTIN},
    {nullptr,         NkSLTokenKind::NK_UNKNOWN},
};

// =============================================================================
NkSLLexer::NkSLLexer(const NkString& source, const NkString& filename)
    : mSource(source), mFilename(filename) {}

// =============================================================================
char NkSLLexer::Current() const {
    if (mPos >= (uint32)mSource.Size()) return '\0';
    return mSource[mPos];
}

char NkSLLexer::LookAhead(uint32 offset) const {
    uint32 p = mPos + offset;
    if (p >= (uint32)mSource.Size()) return '\0';
    return mSource[p];
}

char NkSLLexer::Advance() {
    char c = Current();
    mPos++;
    if (c == '\n') { mLine++; mColumn = 1; }
    else           { mColumn++; }
    return c;
}

bool NkSLLexer::Match(char expected) {
    if (Current() == expected) { Advance(); return true; }
    return false;
}

bool NkSLLexer::IsAtEnd() const {
    return mPos >= (uint32)mSource.Size();
}

// =============================================================================
void NkSLLexer::SkipWhitespaceAndComments() {
    for (;;) {
        while (!IsAtEnd() && (Current() == ' ' || Current() == '\t'
               || Current() == '\r' || Current() == '\n'))
            Advance();
        // Commentaire ligne
        if (Current() == '/' && LookAhead() == '/') {
            while (!IsAtEnd() && Current() != '\n') Advance();
            continue;
        }
        // Commentaire bloc
        if (Current() == '/' && LookAhead() == '*') {
            Advance(); Advance();
            while (!IsAtEnd()) {
                if (Current() == '*' && LookAhead() == '/') {
                    Advance(); Advance(); break;
                }
                Advance();
            }
            continue;
        }
        break;
    }
}

// =============================================================================
bool NkSLLexer::IsAlpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
bool NkSLLexer::IsDigit(char c) { return c>='0'&&c<='9'; }
bool NkSLLexer::IsAlNum(char c) { return IsAlpha(c)||IsDigit(c); }
bool NkSLLexer::IsHexDigit(char c) {
    return IsDigit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F');
}

void NkSLLexer::Error(const NkString& msg) {
    mErrors.PushBack({mLine, mColumn, mFilename, msg, true});
}

// =============================================================================
NkSLTokenKind NkSLLexer::KeywordOrIdent(const NkString& s) {
    for (int i = 0; kKeywords[i].text; i++) {
        if (s == kKeywords[i].text) return kKeywords[i].kind;
    }
    return NkSLTokenKind::NK_IDENT;
}

// =============================================================================
NkSLToken NkSLLexer::ReadIdentOrKeyword() {
    NkSLToken tok;
    tok.line = mLine; tok.column = mColumn;
    NkString buf;
    while (!IsAtEnd() && IsAlNum(Current()))
        buf += Advance();
    tok.kind = KeywordOrIdent(buf);
    tok.text = buf;
    if (tok.kind == NkSLTokenKind::NK_LIT_BOOL)
        tok.boolVal = (buf == "true");
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::ReadNumber() {
    NkSLToken tok;
    tok.line = mLine; tok.column = mColumn;
    NkString buf;
    bool isFloat = false, isDouble = false, isHex = false, isUint = false;

    if (Current() == '0' && (LookAhead() == 'x' || LookAhead() == 'X')) {
        // Hexadécimal
        buf += Advance(); buf += Advance();
        isHex = true;
        while (!IsAtEnd() && IsHexDigit(Current())) buf += Advance();
    } else {
        while (!IsAtEnd() && IsDigit(Current())) buf += Advance();
        if (Current() == '.' && IsDigit(LookAhead())) {
            isFloat = true;
            buf += Advance();
            while (!IsAtEnd() && IsDigit(Current())) buf += Advance();
        }
        if (Current() == 'e' || Current() == 'E') {
            isFloat = true;
            buf += Advance();
            if (Current() == '+' || Current() == '-') buf += Advance();
            while (!IsAtEnd() && IsDigit(Current())) buf += Advance();
        }
    }

    // Suffixe
    if (Current() == 'f' || Current() == 'F') { isFloat = true; buf += Advance(); }
    else if (Current() == 'd' || Current() == 'D') { isDouble = true; buf += Advance(); }
    else if (Current() == 'u' || Current() == 'U') { isUint = true; buf += Advance(); }

    tok.text = buf;
    if (isDouble)     { tok.kind = NkSLTokenKind::NK_LIT_DOUBLE; tok.floatVal = atof(buf.CStr()); }
    else if (isFloat) { tok.kind = NkSLTokenKind::NK_LIT_FLOAT;  tok.floatVal = atof(buf.CStr()); }
    else if (isUint)  { tok.kind = NkSLTokenKind::NK_LIT_UINT;   tok.uintVal  = (uint64)strtoull(buf.CStr(), nullptr, isHex?16:10); }
    else              { tok.kind = NkSLTokenKind::NK_LIT_INT;     tok.intVal   = (int64) strtoll (buf.CStr(), nullptr, isHex?16:10); }
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::ReadAnnotation() {
    // Consomme '@'
    Advance();
    NkSLToken tok;
    tok.line = mLine; tok.column = mColumn;
    NkString buf;
    while (!IsAtEnd() && IsAlNum(Current())) buf += Advance();
    tok.text = "@" + buf;
    tok.kind = NkSLTokenKind::NK_AT;
    for (int i = 0; kAnnotations[i].text; i++) {
        if (buf == kAnnotations[i].text) { tok.kind = kAnnotations[i].kind; break; }
    }
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::ReadOperator() {
    NkSLToken tok;
    tok.line = mLine; tok.column = mColumn;
    char c = Advance();
    tok.text = NkString(1, c);

    auto twoChar = [&](char next, NkSLTokenKind two, NkSLTokenKind one) -> NkSLTokenKind {
        if (Match(next)) { tok.text += next; return two; }
        return one;
    };
    auto threeChar = [&](char second, char third, NkSLTokenKind three,
                          char second2, NkSLTokenKind two2,
                          NkSLTokenKind one) -> NkSLTokenKind {
        if (Current() == second && LookAhead() == third) {
            tok.text += Advance(); tok.text += Advance(); return three;
        }
        if (Match(second2)) { tok.text += second2; return two2; }
        return one;
    };
    (void)threeChar;

    switch (c) {
        case '+': tok.kind = twoChar('+', NkSLTokenKind::NK_OP_INC, twoChar('=', NkSLTokenKind::NK_OP_PLUS_ASSIGN, NkSLTokenKind::NK_OP_PLUS)); break;
        case '-': tok.kind = twoChar('-', NkSLTokenKind::NK_OP_DEC, twoChar('=', NkSLTokenKind::NK_OP_MINUS_ASSIGN, NkSLTokenKind::NK_OP_MINUS)); break;
        case '*': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_STAR_ASSIGN, NkSLTokenKind::NK_OP_STAR); break;
        case '/': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_SLASH_ASSIGN, NkSLTokenKind::NK_OP_SLASH); break;
        case '%': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_PERCENT, NkSLTokenKind::NK_OP_PERCENT); break; // simplified
        case '=': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_EQ, NkSLTokenKind::NK_OP_ASSIGN); break;
        case '!': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_NEQ, NkSLTokenKind::NK_OP_LNOT); break;
        case '<':
            if (Match('<')) { tok.text += '<'; tok.kind = twoChar('=', NkSLTokenKind::NK_OP_LSHIFT_ASSIGN, NkSLTokenKind::NK_OP_LSHIFT); }
            else tok.kind = twoChar('=', NkSLTokenKind::NK_OP_LE, NkSLTokenKind::NK_OP_LT);
            break;
        case '>':
            if (Match('>')) { tok.text += '>'; tok.kind = twoChar('=', NkSLTokenKind::NK_OP_RSHIFT_ASSIGN, NkSLTokenKind::NK_OP_RSHIFT); }
            else tok.kind = twoChar('=', NkSLTokenKind::NK_OP_GE, NkSLTokenKind::NK_OP_GT);
            break;
        case '&': tok.kind = twoChar('&', NkSLTokenKind::NK_OP_LAND, twoChar('=', NkSLTokenKind::NK_OP_AND_ASSIGN, NkSLTokenKind::NK_OP_AND)); break;
        case '|': tok.kind = twoChar('|', NkSLTokenKind::NK_OP_LOR, twoChar('=', NkSLTokenKind::NK_OP_OR_ASSIGN, NkSLTokenKind::NK_OP_OR)); break;
        case '^': tok.kind = twoChar('=', NkSLTokenKind::NK_OP_XOR_ASSIGN, NkSLTokenKind::NK_OP_XOR); break;
        case '~': tok.kind = NkSLTokenKind::NK_OP_TILDE; break;
        case '.': tok.kind = NkSLTokenKind::NK_OP_DOT; break;
        case ',': tok.kind = NkSLTokenKind::NK_OP_COMMA; break;
        case ';': tok.kind = NkSLTokenKind::NK_OP_SEMICOLON; break;
        case ':': tok.kind = NkSLTokenKind::NK_OP_COLON; break;
        case '?': tok.kind = NkSLTokenKind::NK_OP_QUESTION; break;
        case '(': tok.kind = NkSLTokenKind::NK_LPAREN; break;
        case ')': tok.kind = NkSLTokenKind::NK_RPAREN; break;
        case '[': tok.kind = NkSLTokenKind::NK_LBRACKET; break;
        case ']': tok.kind = NkSLTokenKind::NK_RBRACKET; break;
        case '{': tok.kind = NkSLTokenKind::NK_LBRACE; break;
        case '}': tok.kind = NkSLTokenKind::NK_RBRACE; break;
        case '#': tok.kind = NkSLTokenKind::NK_HASH; break;
        default:  tok.kind = NkSLTokenKind::NK_UNKNOWN; break;
    }
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::ReadToken() {
    SkipWhitespaceAndComments();
    if (IsAtEnd()) {
        NkSLToken tok; tok.kind = NkSLTokenKind::NK_END_OF_FILE; tok.line = mLine; return tok;
    }
    char c = Current();
    if (c == '@')  return ReadAnnotation();
    if (IsAlpha(c)) return ReadIdentOrKeyword();
    if (IsDigit(c) || (c == '.' && IsDigit(LookAhead()))) return ReadNumber();
    if (c == '"' || c == '\'') return ReadStringLiteral();
    return ReadOperator();
}

NkSLToken NkSLLexer::ReadStringLiteral() {
    NkSLToken tok; tok.line = mLine; tok.column = mColumn;
    char quote = Advance();
    NkString buf;
    while (!IsAtEnd() && Current() != quote) {
        if (Current() == '\\') { Advance(); buf += Advance(); }
        else buf += Advance();
    }
    if (!IsAtEnd()) Advance(); // consume closing quote
    tok.kind = NkSLTokenKind::NK_LIT_STRING;
    tok.text = buf;
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::Peek() {
    if (mLookahead.Empty()) {
        mLookahead.PushBack(ReadToken());
    }
    return mLookahead[0];
}

NkSLToken NkSLLexer::PeekAt(uint32 offset) {
    while ((uint32)mLookahead.Size() <= offset)
        mLookahead.PushBack(ReadToken());
    return mLookahead[offset];
}

NkSLToken NkSLLexer::Next() {
    if (!mLookahead.Empty()) {
        NkSLToken tok = mLookahead[0];
        mLookahead.Erase(mLookahead.Begin());
        return tok;
    }
    return ReadToken();
}

} // namespace nkentseu
