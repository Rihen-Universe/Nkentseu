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
    {"void",    NkSLTokenKind::KW_VOID},
    {"bool",    NkSLTokenKind::KW_BOOL},
    {"int",     NkSLTokenKind::KW_INT},
    {"uint",    NkSLTokenKind::KW_UINT},
    {"float",   NkSLTokenKind::KW_FLOAT},
    {"double",  NkSLTokenKind::KW_DOUBLE},
    // Vecteurs bool
    {"bvec2",   NkSLTokenKind::KW_BVEC2},
    {"bvec3",   NkSLTokenKind::KW_BVEC3},
    {"bvec4",   NkSLTokenKind::KW_BVEC4},
    // Vecteurs int
    {"ivec2",   NkSLTokenKind::KW_IVEC2},
    {"ivec3",   NkSLTokenKind::KW_IVEC3},
    {"ivec4",   NkSLTokenKind::KW_IVEC4},
    // Vecteurs uint
    {"uvec2",   NkSLTokenKind::KW_UVEC2},
    {"uvec3",   NkSLTokenKind::KW_UVEC3},
    {"uvec4",   NkSLTokenKind::KW_UVEC4},
    // Vecteurs float
    {"vec2",    NkSLTokenKind::KW_VEC2},
    {"vec3",    NkSLTokenKind::KW_VEC3},
    {"vec4",    NkSLTokenKind::KW_VEC4},
    // Vecteurs double
    {"dvec2",   NkSLTokenKind::KW_DVEC2},
    {"dvec3",   NkSLTokenKind::KW_DVEC3},
    {"dvec4",   NkSLTokenKind::KW_DVEC4},
    // Matrices
    {"mat2",    NkSLTokenKind::KW_MAT2},
    {"mat3",    NkSLTokenKind::KW_MAT3},
    {"mat4",    NkSLTokenKind::KW_MAT4},
    {"mat2x2",  NkSLTokenKind::KW_MAT2X2},
    {"mat2x3",  NkSLTokenKind::KW_MAT2X3},
    {"mat2x4",  NkSLTokenKind::KW_MAT2X4},
    {"mat3x2",  NkSLTokenKind::KW_MAT3X2},
    {"mat3x3",  NkSLTokenKind::KW_MAT3X3},
    {"mat3x4",  NkSLTokenKind::KW_MAT3X4},
    {"mat4x2",  NkSLTokenKind::KW_MAT4X2},
    {"mat4x3",  NkSLTokenKind::KW_MAT4X3},
    {"mat4x4",  NkSLTokenKind::KW_MAT4X4},
    {"dmat2",   NkSLTokenKind::KW_DMAT2},
    {"dmat3",   NkSLTokenKind::KW_DMAT3},
    {"dmat4",   NkSLTokenKind::KW_DMAT4},
    // Samplers
    {"sampler2D",            NkSLTokenKind::KW_SAMPLER2D},
    {"sampler2DShadow",      NkSLTokenKind::KW_SAMPLER2D_SHADOW},
    {"sampler2DArray",       NkSLTokenKind::KW_SAMPLER2D_ARRAY},
    {"sampler2DArrayShadow", NkSLTokenKind::KW_SAMPLER2D_ARRAY_SHADOW},
    {"samplerCube",          NkSLTokenKind::KW_SAMPLERCUBE},
    {"samplerCubeShadow",    NkSLTokenKind::KW_SAMPLERCUBE_SHADOW},
    {"sampler3D",            NkSLTokenKind::KW_SAMPLER3D},
    {"isampler2D",           NkSLTokenKind::KW_ISAMPLER2D},
    {"usampler2D",           NkSLTokenKind::KW_USAMPLER2D},
    // Images
    {"image2D",    NkSLTokenKind::KW_IMAGE2D},
    {"iimage2D",   NkSLTokenKind::KW_IIMAGE2D},
    {"uimage2D",   NkSLTokenKind::KW_UIMAGE2D},
    // Qualificateurs de stockage
    {"in",             NkSLTokenKind::KW_IN},
    {"out",            NkSLTokenKind::KW_OUT},
    {"inout",          NkSLTokenKind::KW_INOUT},
    {"uniform",        NkSLTokenKind::KW_UNIFORM},
    {"buffer",         NkSLTokenKind::KW_BUFFER},
    {"push_constant",  NkSLTokenKind::KW_PUSH_CONSTANT},
    {"shared",         NkSLTokenKind::KW_SHARED},
    {"workgroup",      NkSLTokenKind::KW_WORKGROUP},
    // Interpolation
    {"smooth",         NkSLTokenKind::KW_SMOOTH},
    {"flat",           NkSLTokenKind::KW_FLAT},
    {"noperspective",  NkSLTokenKind::KW_NOPERSPECTIVE},
    // Précision
    {"lowp",           NkSLTokenKind::KW_LOWP},
    {"mediump",        NkSLTokenKind::KW_MEDIUMP},
    {"highp",          NkSLTokenKind::KW_HIGHP},
    // Flux
    {"if",       NkSLTokenKind::KW_IF},
    {"else",     NkSLTokenKind::KW_ELSE},
    {"for",      NkSLTokenKind::KW_FOR},
    {"while",    NkSLTokenKind::KW_WHILE},
    {"do",       NkSLTokenKind::KW_DO},
    {"break",    NkSLTokenKind::KW_BREAK},
    {"continue", NkSLTokenKind::KW_CONTINUE},
    {"return",   NkSLTokenKind::KW_RETURN},
    {"discard",  NkSLTokenKind::KW_DISCARD},
    {"switch",   NkSLTokenKind::KW_SWITCH},
    {"case",     NkSLTokenKind::KW_CASE},
    {"default",  NkSLTokenKind::KW_DEFAULT},
    // Struct / layout
    {"struct",   NkSLTokenKind::KW_STRUCT},
    {"layout",   NkSLTokenKind::KW_LAYOUT},
    // Const
    {"const",    NkSLTokenKind::KW_CONST},
    {"invariant",NkSLTokenKind::KW_INVARIANT},
    // Builtins
    {"gl_Position",        NkSLTokenKind::KW_BUILTIN_POSITION},
    {"gl_FragCoord",       NkSLTokenKind::KW_BUILTIN_FRAGCOORD},
    {"gl_FragDepth",       NkSLTokenKind::KW_BUILTIN_FRAGDEPTH},
    {"gl_VertexID",        NkSLTokenKind::KW_BUILTIN_VERTEXID},
    {"gl_InstanceID",      NkSLTokenKind::KW_BUILTIN_INSTANCEID},
    {"gl_FrontFacing",     NkSLTokenKind::KW_BUILTIN_FRONTFACING},
    {"gl_LocalInvocationID",  NkSLTokenKind::KW_BUILTIN_LOCALINVID},
    {"gl_GlobalInvocationID", NkSLTokenKind::KW_BUILTIN_GLOBALINVID},
    {"gl_WorkGroupID",        NkSLTokenKind::KW_BUILTIN_WORKINVID},
    // Booléens
    {"true",  NkSLTokenKind::LIT_BOOL},
    {"false", NkSLTokenKind::LIT_BOOL},
    {nullptr, NkSLTokenKind::UNKNOWN},
};

// Annotations (commencent par @)
struct AtEntry { const char* text; NkSLTokenKind kind; };
static const AtEntry kAnnotations[] = {
    {"binding",       NkSLTokenKind::AT_BINDING},
    {"location",      NkSLTokenKind::AT_LOCATION},
    {"push_constant", NkSLTokenKind::AT_PUSH_CONSTANT},
    {"stage",         NkSLTokenKind::AT_STAGE},
    {"entry",         NkSLTokenKind::AT_ENTRY},
    {"builtin",       NkSLTokenKind::AT_BUILTIN},
    {nullptr,         NkSLTokenKind::UNKNOWN},
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
    return NkSLTokenKind::IDENT;
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
    if (tok.kind == NkSLTokenKind::LIT_BOOL)
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
    if (isDouble)     { tok.kind = NkSLTokenKind::LIT_DOUBLE; tok.floatVal = atof(buf.CStr()); }
    else if (isFloat) { tok.kind = NkSLTokenKind::LIT_FLOAT;  tok.floatVal = atof(buf.CStr()); }
    else if (isUint)  { tok.kind = NkSLTokenKind::LIT_UINT;   tok.uintVal  = (uint64)strtoull(buf.CStr(), nullptr, isHex?16:10); }
    else              { tok.kind = NkSLTokenKind::LIT_INT;     tok.intVal   = (int64) strtoll (buf.CStr(), nullptr, isHex?16:10); }
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
    tok.kind = NkSLTokenKind::AT;
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
        case '+': tok.kind = twoChar('+', NkSLTokenKind::OP_INC, twoChar('=', NkSLTokenKind::OP_PLUS_ASSIGN, NkSLTokenKind::OP_PLUS)); break;
        case '-': tok.kind = twoChar('-', NkSLTokenKind::OP_DEC, twoChar('=', NkSLTokenKind::OP_MINUS_ASSIGN, NkSLTokenKind::OP_MINUS)); break;
        case '*': tok.kind = twoChar('=', NkSLTokenKind::OP_STAR_ASSIGN, NkSLTokenKind::OP_STAR); break;
        case '/': tok.kind = twoChar('=', NkSLTokenKind::OP_SLASH_ASSIGN, NkSLTokenKind::OP_SLASH); break;
        case '%': tok.kind = twoChar('=', NkSLTokenKind::OP_PERCENT, NkSLTokenKind::OP_PERCENT); break; // simplified
        case '=': tok.kind = twoChar('=', NkSLTokenKind::OP_EQ, NkSLTokenKind::OP_ASSIGN); break;
        case '!': tok.kind = twoChar('=', NkSLTokenKind::OP_NEQ, NkSLTokenKind::OP_LNOT); break;
        case '<':
            if (Match('<')) { tok.text += '<'; tok.kind = twoChar('=', NkSLTokenKind::OP_LSHIFT_ASSIGN, NkSLTokenKind::OP_LSHIFT); }
            else tok.kind = twoChar('=', NkSLTokenKind::OP_LE, NkSLTokenKind::OP_LT);
            break;
        case '>':
            if (Match('>')) { tok.text += '>'; tok.kind = twoChar('=', NkSLTokenKind::OP_RSHIFT_ASSIGN, NkSLTokenKind::OP_RSHIFT); }
            else tok.kind = twoChar('=', NkSLTokenKind::OP_GE, NkSLTokenKind::OP_GT);
            break;
        case '&': tok.kind = twoChar('&', NkSLTokenKind::OP_LAND, twoChar('=', NkSLTokenKind::OP_AND_ASSIGN, NkSLTokenKind::OP_AND)); break;
        case '|': tok.kind = twoChar('|', NkSLTokenKind::OP_LOR, twoChar('=', NkSLTokenKind::OP_OR_ASSIGN, NkSLTokenKind::OP_OR)); break;
        case '^': tok.kind = twoChar('=', NkSLTokenKind::OP_XOR_ASSIGN, NkSLTokenKind::OP_XOR); break;
        case '~': tok.kind = NkSLTokenKind::OP_TILDE; break;
        case '.': tok.kind = NkSLTokenKind::OP_DOT; break;
        case ',': tok.kind = NkSLTokenKind::OP_COMMA; break;
        case ';': tok.kind = NkSLTokenKind::OP_SEMICOLON; break;
        case ':': tok.kind = NkSLTokenKind::OP_COLON; break;
        case '?': tok.kind = NkSLTokenKind::OP_QUESTION; break;
        case '(': tok.kind = NkSLTokenKind::LPAREN; break;
        case ')': tok.kind = NkSLTokenKind::RPAREN; break;
        case '[': tok.kind = NkSLTokenKind::LBRACKET; break;
        case ']': tok.kind = NkSLTokenKind::RBRACKET; break;
        case '{': tok.kind = NkSLTokenKind::LBRACE; break;
        case '}': tok.kind = NkSLTokenKind::RBRACE; break;
        case '#': tok.kind = NkSLTokenKind::HASH; break;
        default:  tok.kind = NkSLTokenKind::UNKNOWN; break;
    }
    return tok;
}

// =============================================================================
NkSLToken NkSLLexer::ReadToken() {
    SkipWhitespaceAndComments();
    if (IsAtEnd()) {
        NkSLToken tok; tok.kind = NkSLTokenKind::END_OF_FILE; tok.line = mLine; return tok;
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
    tok.kind = NkSLTokenKind::LIT_STRING;
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
