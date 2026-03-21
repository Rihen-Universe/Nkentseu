// =============================================================================
// NkSLSymbolTable.cpp
// =============================================================================
#include "NkSLSymbolTable.h"

namespace nkentseu {

// =============================================================================
// NkSLResolvedType
// =============================================================================
uint32 NkSLResolvedType::ComponentCount() const {
    switch (base) {
        case NkSLBaseType::BOOL: case NkSLBaseType::INT: case NkSLBaseType::UINT:
        case NkSLBaseType::FLOAT: case NkSLBaseType::DOUBLE: return 1;
        case NkSLBaseType::IVEC2: case NkSLBaseType::UVEC2:
        case NkSLBaseType::VEC2:  case NkSLBaseType::DVEC2: return 2;
        case NkSLBaseType::IVEC3: case NkSLBaseType::UVEC3:
        case NkSLBaseType::VEC3:  case NkSLBaseType::DVEC3: return 3;
        case NkSLBaseType::IVEC4: case NkSLBaseType::UVEC4:
        case NkSLBaseType::VEC4:  case NkSLBaseType::DVEC4: return 4;
        case NkSLBaseType::MAT2:  return 4;
        case NkSLBaseType::MAT3:  return 9;
        case NkSLBaseType::MAT4:  return 16;
        default: return 0;
    }
}

NkSLBaseType NkSLResolvedType::ScalarBase() const {
    if (base >= NkSLBaseType::IVEC2 && base <= NkSLBaseType::IVEC4) return NkSLBaseType::INT;
    if (base >= NkSLBaseType::UVEC2 && base <= NkSLBaseType::UVEC4) return NkSLBaseType::UINT;
    if (base >= NkSLBaseType::VEC2  && base <= NkSLBaseType::VEC4)  return NkSLBaseType::FLOAT;
    if (base >= NkSLBaseType::DVEC2 && base <= NkSLBaseType::DVEC4) return NkSLBaseType::DOUBLE;
    if (base >= NkSLBaseType::MAT2  && base <= NkSLBaseType::MAT4)  return NkSLBaseType::FLOAT;
    if (base >= NkSLBaseType::DMAT2 && base <= NkSLBaseType::DMAT4) return NkSLBaseType::DOUBLE;
    return base;
}

NkSLResolvedType NkSLResolvedType::FromNode(NkSLTypeNode* t) {
    if (!t) return Void();
    NkSLResolvedType r;
    r.base      = t->baseType;
    r.typeName  = t->typeName;
    r.arraySize = t->arraySize;
    r.isUnsized = t->isUnsized;
    return r;
}

// =============================================================================
// NkSLScope
// =============================================================================
NkSLSymbol* NkSLScope::Find(const NkString& name) {
    auto* p = symbols.Find(name);
    if (p) return p;
    if (parent) return parent->Find(name);
    return nullptr;
}

bool NkSLScope::Define(const NkSLSymbol& sym) {
    if (symbols.Find(sym.name)) return false; // déjà défini dans ce scope
    symbols[sym.name] = sym;
    return true;
}

bool NkSLScope::Has(const NkString& name) const {
    return symbols.Find(name) != nullptr;
}

// =============================================================================
// NkSLSymbolTable
// =============================================================================
NkSLSymbolTable::NkSLSymbolTable() {
    // Scope global
    auto* global = new NkSLScope();
    mScopes.PushBack(global);
    mCurrent = global;
    RegisterBuiltins();
}

void NkSLSymbolTable::PushScope(bool isFunction, bool isLoop,
                                  NkSLResolvedType returnType) {
    auto* scope      = new NkSLScope();
    scope->parent    = mCurrent;
    scope->isFunction= isFunction;
    scope->isLoop    = isLoop;
    scope->returnType= returnType;
    mScopes.PushBack(scope);
    mCurrent = scope;
}

void NkSLSymbolTable::PopScope() {
    if (!mCurrent || !mCurrent->parent) return;
    mCurrent = mCurrent->parent;
}

bool NkSLSymbolTable::Define(const NkSLSymbol& sym) {
    return mCurrent->Define(sym);
}

NkSLSymbol* NkSLSymbolTable::Resolve(const NkString& name) {
    return mCurrent->Find(name);
}

const NkSLFunctionOverload* NkSLSymbolTable::ResolveFunction(
    const NkString& name,
    const NkVector<NkSLResolvedType>& argTypes)
{
    NkSLSymbol* sym = Resolve(name);
    if (!sym || sym->kind != NkSLSymbolKind::FUNCTION &&
                sym->kind != NkSLSymbolKind::BUILTIN_FUNC)
        return nullptr;

    // Chercher la meilleure surcharge
    const NkSLFunctionOverload* best = nullptr;
    uint32 bestScore = UINT32_MAX;

    for (auto& overload : sym->overloads) {
        if (overload.paramTypes.Size() != argTypes.Size()) continue;
        uint32 score = 0;
        bool valid = true;
        for (uint32 i = 0; i < (uint32)argTypes.Size(); i++) {
            if (argTypes[i] == overload.paramTypes[i]) {
                // Correspondance exacte
            } else if (IsImplicitlyConvertible(argTypes[i], overload.paramTypes[i])) {
                score += 10; // conversion implicite
            } else {
                valid = false; break;
            }
        }
        if (valid && score < bestScore) { bestScore = score; best = &overload; }
    }

    // Si aucune surcharge exacte, essayer les fonctions génériques (genType)
    // Pour les builtins comme abs(float), abs(vec2), abs(vec3)... on accepte
    // si le premier paramètre est numérique et que la signature a 1 param
    if (!best && sym->overloads.Size() > 0 && sym->overloads[0].isBuiltin) {
        auto& first = sym->overloads[0];
        if (argTypes.Size() == first.paramTypes.Size()) {
            bool compatible = true;
            for (uint32 i = 0; i < (uint32)argTypes.Size(); i++) {
                if (!argTypes[i].IsNumeric() && !argTypes[i].IsSampler()) {
                    compatible = false; break;
                }
            }
            if (compatible) best = &first;
        }
    }

    return best;
}

bool NkSLSymbolTable::IsInLoop() const {
    NkSLScope* s = mCurrent;
    while (s) {
        if (s->isLoop)     return true;
        if (s->isFunction) return false;
        s = s->parent;
    }
    return false;
}

NkSLResolvedType NkSLSymbolTable::CurrentReturnType() const {
    NkSLScope* s = mCurrent;
    while (s) {
        if (s->isFunction) return s->returnType;
        s = s->parent;
    }
    return NkSLResolvedType::Void();
}

bool NkSLSymbolTable::IsInFunction() const {
    NkSLScope* s = mCurrent;
    while (s) {
        if (s->isFunction) return true;
        s = s->parent;
    }
    return false;
}

// =============================================================================
// Compatibilité de types
// =============================================================================
bool NkSLSymbolTable::IsAssignable(const NkSLResolvedType& from,
                                    const NkSLResolvedType& to) {
    if (from == to) return true;
    return IsImplicitlyConvertible(from, to);
}

bool NkSLSymbolTable::IsImplicitlyConvertible(const NkSLResolvedType& from,
                                               const NkSLResolvedType& to) {
    // int → float, int → uint (avec warning), float → double
    if (from.base == NkSLBaseType::INT   && to.base == NkSLBaseType::FLOAT)  return true;
    if (from.base == NkSLBaseType::INT   && to.base == NkSLBaseType::UINT)   return true;
    if (from.base == NkSLBaseType::FLOAT && to.base == NkSLBaseType::DOUBLE) return true;
    if (from.base == NkSLBaseType::INT   && to.base == NkSLBaseType::DOUBLE) return true;
    // ivec → vec (même dimension)
    if (from.base == NkSLBaseType::IVEC2 && to.base == NkSLBaseType::VEC2)  return true;
    if (from.base == NkSLBaseType::IVEC3 && to.base == NkSLBaseType::VEC3)  return true;
    if (from.base == NkSLBaseType::IVEC4 && to.base == NkSLBaseType::VEC4)  return true;
    return false;
}

uint32 NkSLSymbolTable::VectorSize(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::IVEC2: case NkSLBaseType::UVEC2:
        case NkSLBaseType::VEC2:  case NkSLBaseType::DVEC2: return 2;
        case NkSLBaseType::IVEC3: case NkSLBaseType::UVEC3:
        case NkSLBaseType::VEC3:  case NkSLBaseType::DVEC3: return 3;
        case NkSLBaseType::IVEC4: case NkSLBaseType::UVEC4:
        case NkSLBaseType::VEC4:  case NkSLBaseType::DVEC4: return 4;
        default: return 1;
    }
}

NkSLResolvedType NkSLSymbolTable::BinaryResultType(const NkString& op,
                                                     const NkSLResolvedType& lhs,
                                                     const NkSLResolvedType& rhs) {
    // Opérateurs de comparaison → bool
    if (op=="==" || op=="!=" || op=="<" || op==">" || op=="<=" || op>="<=")
        return NkSLResolvedType::Bool();

    // Opérateurs logiques → bool
    if (op=="&&" || op=="||") return NkSLResolvedType::Bool();

    // Arithmétique
    if (lhs == rhs) return lhs;

    // scalar op vector → vector
    if (rhs.IsVector() && lhs.IsScalar()) return rhs;
    if (lhs.IsVector() && rhs.IsScalar()) return lhs;

    // scalar op matrix → matrix
    if (rhs.IsMatrix() && lhs.IsScalar()) return rhs;
    if (lhs.IsMatrix() && rhs.IsScalar()) return lhs;

    // matrix * vector (vec4 = mat4 * vec4)
    if (op == "*") {
        if (lhs.base == NkSLBaseType::MAT4 && rhs.base == NkSLBaseType::VEC4)
            return NkSLResolvedType::Vec4();
        if (lhs.base == NkSLBaseType::MAT3 && rhs.base == NkSLBaseType::VEC3)
            return NkSLResolvedType::Vec3();
        if (lhs.base == NkSLBaseType::MAT4 && rhs.base == NkSLBaseType::MAT4)
            return NkSLResolvedType::Mat4();
        if (lhs.base == NkSLBaseType::MAT3 && rhs.base == NkSLBaseType::MAT3)
            return NkSLResolvedType::Mat3();
    }

    // int op float → float
    if (IsImplicitlyConvertible(lhs, rhs)) return rhs;
    if (IsImplicitlyConvertible(rhs, lhs)) return lhs;

    return lhs; // fallback
}

// =============================================================================
// Enregistrement des builtins GLSL
// =============================================================================
void NkSLSymbolTable::RegisterBuiltins() {
    RegisterBuiltinVars();
    RegisterBuiltinFunctions();
}

void NkSLSymbolTable::RegisterBuiltinVars() {
    auto defVar = [&](const NkString& name, NkSLBaseType type) {
        NkSLSymbol sym;
        sym.name = name;
        sym.kind = NkSLSymbolKind::BUILTIN_VAR;
        sym.type.base = type;
        mCurrent->Define(sym);
    };
    defVar("gl_Position",          NkSLBaseType::VEC4);
    defVar("gl_FragCoord",         NkSLBaseType::VEC4);
    defVar("gl_FragDepth",         NkSLBaseType::FLOAT);
    defVar("gl_VertexID",          NkSLBaseType::INT);
    defVar("gl_InstanceID",        NkSLBaseType::INT);
    defVar("gl_FrontFacing",       NkSLBaseType::BOOL);
    defVar("gl_LocalInvocationID", NkSLBaseType::UVEC3);
    defVar("gl_GlobalInvocationID",NkSLBaseType::UVEC3);
    defVar("gl_WorkGroupID",       NkSLBaseType::UVEC3);
    defVar("gl_WorkGroupSize",     NkSLBaseType::UVEC3);
    defVar("gl_NumWorkGroups",     NkSLBaseType::UVEC3);
    defVar("gl_PointSize",         NkSLBaseType::FLOAT);
    defVar("gl_ClipDistance",      NkSLBaseType::FLOAT); // array
    defVar("gl_Layer",             NkSLBaseType::INT);
    defVar("gl_ViewportIndex",     NkSLBaseType::INT);
}

// Nom GLSL d'un type de base (pour les constructeurs)
static const char* BaseTypeNameGLSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::VEC2:  return "vec2";  case NkSLBaseType::VEC3:  return "vec3";
        case NkSLBaseType::VEC4:  return "vec4";
        case NkSLBaseType::IVEC2: return "ivec2"; case NkSLBaseType::IVEC3: return "ivec3";
        case NkSLBaseType::IVEC4: return "ivec4";
        case NkSLBaseType::UVEC2: return "uvec2"; case NkSLBaseType::UVEC3: return "uvec3";
        case NkSLBaseType::UVEC4: return "uvec4";
        case NkSLBaseType::MAT2:  return "mat2";  case NkSLBaseType::MAT3:  return "mat3";
        case NkSLBaseType::MAT4:  return "mat4";
        case NkSLBaseType::FLOAT: return "float"; case NkSLBaseType::INT:   return "int";
        case NkSLBaseType::UINT:  return "uint";  case NkSLBaseType::BOOL:  return "bool";
        default: return "unknown";
    }
}

void NkSLSymbolTable::RegisterBuiltinFunctions() {
    // Helper pour enregistrer une surcharge builtin
    auto defFunc = [&](const NkString& name,
                        std::initializer_list<NkSLBaseType> params,
                        NkSLBaseType ret) {
        NkSLSymbol* existing = mCurrent->symbols.Find(name);
        if (!existing) {
            NkSLSymbol sym; sym.name=name; sym.kind=NkSLSymbolKind::BUILTIN_FUNC;
            mCurrent->Define(sym);
            existing = mCurrent->symbols.Find(name);
        }
        NkSLFunctionOverload ov;
        ov.isBuiltin  = true;
        ov.returnType.base = ret;
        for (auto p : params) {
            NkSLResolvedType pt; pt.base = p;
            ov.paramTypes.PushBack(pt);
        }
        existing->overloads.PushBack(ov);
    };

    // Utiliser un type générique "genType" représenté par FLOAT
    // Les surcharges pour vec2/vec3/vec4 sont enregistrées séparément
    // (dans la résolution on accepte tout type numérique pour les builtins)

    // ── Trigonométrie ─────────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::FLOAT, NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4}) {
        defFunc("radians",    {t}, t);
        defFunc("degrees",    {t}, t);
        defFunc("sin",        {t}, t);
        defFunc("cos",        {t}, t);
        defFunc("tan",        {t}, t);
        defFunc("asin",       {t}, t);
        defFunc("acos",       {t}, t);
        defFunc("atan",       {t}, t);
        defFunc("atan",       {t, t}, t); // atan(y, x)
        defFunc("sinh",       {t}, t);
        defFunc("cosh",       {t}, t);
        defFunc("tanh",       {t}, t);
        defFunc("asinh",      {t}, t);
        defFunc("acosh",      {t}, t);
        defFunc("atanh",      {t}, t);
    }

    // ── Exponentielle ─────────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::FLOAT, NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4}) {
        defFunc("pow",         {t, t}, t);
        defFunc("exp",         {t}, t);
        defFunc("log",         {t}, t);
        defFunc("exp2",        {t}, t);
        defFunc("log2",        {t}, t);
        defFunc("sqrt",        {t}, t);
        defFunc("inversesqrt", {t}, t);
    }

    // ── Fonctions courantes ────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::FLOAT, NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4,
                   NkSLBaseType::INT,   NkSLBaseType::IVEC2, NkSLBaseType::IVEC3, NkSLBaseType::IVEC4}) {
        defFunc("abs",   {t}, t);
        defFunc("sign",  {t}, t);
        defFunc("min",   {t, t}, t);
        defFunc("max",   {t, t}, t);
    }
    for (auto t : {NkSLBaseType::FLOAT, NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4}) {
        defFunc("floor",      {t}, t);
        defFunc("trunc",      {t}, t);
        defFunc("round",      {t}, t);
        defFunc("roundEven",  {t}, t);
        defFunc("ceil",       {t}, t);
        defFunc("fract",      {t}, t);
        defFunc("mod",        {t, t}, t);
        defFunc("mod",        {t, NkSLBaseType::FLOAT}, t);
        defFunc("modf",       {t, t}, t); // out param simplifié
        defFunc("min",        {t, NkSLBaseType::FLOAT}, t);
        defFunc("max",        {t, NkSLBaseType::FLOAT}, t);
        defFunc("clamp",      {t, t, t}, t);
        defFunc("clamp",      {t, NkSLBaseType::FLOAT, NkSLBaseType::FLOAT}, t);
        defFunc("mix",        {t, t, t}, t);
        defFunc("mix",        {t, t, NkSLBaseType::FLOAT}, t);
        defFunc("step",       {t, t}, t);
        defFunc("step",       {NkSLBaseType::FLOAT, t}, t);
        defFunc("smoothstep", {t, t, t}, t);
        defFunc("smoothstep", {NkSLBaseType::FLOAT, NkSLBaseType::FLOAT, t}, t);
        defFunc("isnan",      {t}, NkSLBaseType::BOOL);
        defFunc("isinf",      {t}, NkSLBaseType::BOOL);
    }

    // ── Géométrie ──────────────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4}) {
        defFunc("length",    {t}, NkSLBaseType::FLOAT);
        defFunc("distance",  {t, t}, NkSLBaseType::FLOAT);
        defFunc("dot",       {t, t}, NkSLBaseType::FLOAT);
        defFunc("normalize", {t}, t);
        defFunc("reflect",   {t, t}, t);
        defFunc("refract",   {t, t, NkSLBaseType::FLOAT}, t);
        defFunc("faceforward",{t, t, t}, t);
    }
    defFunc("dot",   {NkSLBaseType::FLOAT, NkSLBaseType::FLOAT}, NkSLBaseType::FLOAT);
    defFunc("cross", {NkSLBaseType::VEC3, NkSLBaseType::VEC3}, NkSLBaseType::VEC3);

    // ── Matrices ───────────────────────────────────────────────────────────────
    defFunc("matrixCompMult", {NkSLBaseType::MAT2, NkSLBaseType::MAT2}, NkSLBaseType::MAT2);
    defFunc("matrixCompMult", {NkSLBaseType::MAT3, NkSLBaseType::MAT3}, NkSLBaseType::MAT3);
    defFunc("matrixCompMult", {NkSLBaseType::MAT4, NkSLBaseType::MAT4}, NkSLBaseType::MAT4);
    defFunc("transpose",     {NkSLBaseType::MAT4}, NkSLBaseType::MAT4);
    defFunc("transpose",     {NkSLBaseType::MAT3}, NkSLBaseType::MAT3);
    defFunc("transpose",     {NkSLBaseType::MAT2}, NkSLBaseType::MAT2);
    defFunc("inverse",       {NkSLBaseType::MAT4}, NkSLBaseType::MAT4);
    defFunc("inverse",       {NkSLBaseType::MAT3}, NkSLBaseType::MAT3);
    defFunc("inverse",       {NkSLBaseType::MAT2}, NkSLBaseType::MAT2);
    defFunc("determinant",   {NkSLBaseType::MAT4}, NkSLBaseType::FLOAT);
    defFunc("determinant",   {NkSLBaseType::MAT3}, NkSLBaseType::FLOAT);
    defFunc("outerProduct",  {NkSLBaseType::VEC4, NkSLBaseType::VEC4}, NkSLBaseType::MAT4);
    defFunc("outerProduct",  {NkSLBaseType::VEC3, NkSLBaseType::VEC3}, NkSLBaseType::MAT3);

    // ── Textures ───────────────────────────────────────────────────────────────
    defFunc("texture",    {NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2},  NkSLBaseType::VEC4);
    defFunc("texture",    {NkSLBaseType::SAMPLER2D_SHADOW, NkSLBaseType::VEC3}, NkSLBaseType::FLOAT);
    defFunc("texture",    {NkSLBaseType::SAMPLER2D_ARRAY, NkSLBaseType::VEC3}, NkSLBaseType::VEC4);
    defFunc("texture",    {NkSLBaseType::SAMPLER2D_ARRAY_SHADOW, NkSLBaseType::VEC4}, NkSLBaseType::FLOAT);
    defFunc("texture",    {NkSLBaseType::SAMPLER_CUBE, NkSLBaseType::VEC3}, NkSLBaseType::VEC4);
    defFunc("texture",    {NkSLBaseType::SAMPLER_CUBE_SHADOW, NkSLBaseType::VEC4}, NkSLBaseType::FLOAT);
    defFunc("texture",    {NkSLBaseType::SAMPLER3D, NkSLBaseType::VEC3}, NkSLBaseType::VEC4);
    defFunc("textureLod", {NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2, NkSLBaseType::FLOAT}, NkSLBaseType::VEC4);
    defFunc("textureLod", {NkSLBaseType::SAMPLER3D, NkSLBaseType::VEC3, NkSLBaseType::FLOAT}, NkSLBaseType::VEC4);
    defFunc("textureOffset",{NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2, NkSLBaseType::IVEC2}, NkSLBaseType::VEC4);
    defFunc("textureGrad",{NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2, NkSLBaseType::VEC2, NkSLBaseType::VEC2}, NkSLBaseType::VEC4);
    defFunc("textureGather",{NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2}, NkSLBaseType::VEC4);
    defFunc("textureGather",{NkSLBaseType::SAMPLER2D, NkSLBaseType::VEC2, NkSLBaseType::INT}, NkSLBaseType::VEC4);
    defFunc("textureSize",{NkSLBaseType::SAMPLER2D, NkSLBaseType::INT}, NkSLBaseType::IVEC2);
    defFunc("textureSize",{NkSLBaseType::SAMPLER3D, NkSLBaseType::INT}, NkSLBaseType::IVEC3);
    defFunc("textureSize",{NkSLBaseType::SAMPLER_CUBE, NkSLBaseType::INT}, NkSLBaseType::IVEC2);
    defFunc("texelFetch", {NkSLBaseType::SAMPLER2D, NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::VEC4);

    // ── Image load/store ───────────────────────────────────────────────────────
    defFunc("imageLoad",       {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2}, NkSLBaseType::VEC4);
    defFunc("imageStore",      {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::VEC4}, NkSLBaseType::VOID);
    defFunc("imageAtomicAdd",  {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicMin",  {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicMax",  {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicAnd",  {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicOr",   {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicXor",  {NkSLBaseType::IMAGE2D,  NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicExchange",{NkSLBaseType::IMAGE2D, NkSLBaseType::IVEC2, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageAtomicCompSwap",{NkSLBaseType::IMAGE2D, NkSLBaseType::IVEC2, NkSLBaseType::INT, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("imageSize",       {NkSLBaseType::IMAGE2D},  NkSLBaseType::IVEC2);

    // ── Constructeurs vecteurs / matrices ──────────────────────────────────────
    for (auto t : {NkSLBaseType::VEC2, NkSLBaseType::VEC3, NkSLBaseType::VEC4,
                   NkSLBaseType::IVEC2, NkSLBaseType::IVEC3, NkSLBaseType::IVEC4,
                   NkSLBaseType::UVEC2, NkSLBaseType::UVEC3, NkSLBaseType::UVEC4,
                   NkSLBaseType::MAT2, NkSLBaseType::MAT3, NkSLBaseType::MAT4}) {
        NkSLSymbol* sym = mCurrent->symbols.Find(BaseTypeNameGLSL(t));
        if (!sym) {
            NkSLSymbol s; s.name = BaseTypeNameGLSL(t);
            s.kind = NkSLSymbolKind::BUILTIN_FUNC;
            mCurrent->Define(s);
            sym = mCurrent->symbols.Find(s.name);
        }
        // Surcharge générique (le vrai nombre de composantes est vérifié dans le sémantique)
        NkSLFunctionOverload ov;
        ov.isBuiltin     = true;
        ov.returnType.base = t;
        // Un seul param générique (la vérification du nombre est faite dans le sémantique)
        NkSLResolvedType gf; gf.base = NkSLBaseType::FLOAT;
        ov.paramTypes.PushBack(gf);
        sym->overloads.PushBack(ov);
    }

    // ── Divers ─────────────────────────────────────────────────────────────────
    defFunc("discard",        {}, NkSLBaseType::VOID);
    defFunc("dFdx",           {NkSLBaseType::FLOAT}, NkSLBaseType::FLOAT);
    defFunc("dFdx",           {NkSLBaseType::VEC2},  NkSLBaseType::VEC2);
    defFunc("dFdx",           {NkSLBaseType::VEC3},  NkSLBaseType::VEC3);
    defFunc("dFdx",           {NkSLBaseType::VEC4},  NkSLBaseType::VEC4);
    defFunc("dFdy",           {NkSLBaseType::FLOAT}, NkSLBaseType::FLOAT);
    defFunc("dFdy",           {NkSLBaseType::VEC2},  NkSLBaseType::VEC2);
    defFunc("dFdy",           {NkSLBaseType::VEC3},  NkSLBaseType::VEC3);
    defFunc("dFdy",           {NkSLBaseType::VEC4},  NkSLBaseType::VEC4);
    defFunc("fwidth",         {NkSLBaseType::FLOAT}, NkSLBaseType::FLOAT);
    defFunc("barrier",        {}, NkSLBaseType::VOID);
    defFunc("memoryBarrier",  {}, NkSLBaseType::VOID);
    defFunc("groupMemoryBarrier", {}, NkSLBaseType::VOID);
    defFunc("allInvocationsEqual", {NkSLBaseType::BOOL}, NkSLBaseType::BOOL);
    defFunc("packUnorm2x16",  {NkSLBaseType::VEC2},  NkSLBaseType::UINT);
    defFunc("unpackUnorm2x16",{NkSLBaseType::UINT},  NkSLBaseType::VEC2);
    defFunc("packHalf2x16",   {NkSLBaseType::VEC2},  NkSLBaseType::UINT);
    defFunc("unpackHalf2x16", {NkSLBaseType::UINT},  NkSLBaseType::VEC2);
    defFunc("bitfieldExtract",{NkSLBaseType::INT, NkSLBaseType::INT, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("bitfieldInsert", {NkSLBaseType::INT, NkSLBaseType::INT, NkSLBaseType::INT, NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("bitCount",       {NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("findLSB",        {NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("findMSB",        {NkSLBaseType::INT}, NkSLBaseType::INT);
    defFunc("floatBitsToInt", {NkSLBaseType::FLOAT}, NkSLBaseType::INT);
    defFunc("intBitsToFloat", {NkSLBaseType::INT}, NkSLBaseType::FLOAT);
    defFunc("uintBitsToFloat",{NkSLBaseType::UINT}, NkSLBaseType::FLOAT);
    defFunc("emit",           {}, NkSLBaseType::VOID); // geometry shader
    defFunc("EndPrimitive",   {}, NkSLBaseType::VOID);
    defFunc("EmitVertex",     {}, NkSLBaseType::VOID);
}



} // namespace nkentseu