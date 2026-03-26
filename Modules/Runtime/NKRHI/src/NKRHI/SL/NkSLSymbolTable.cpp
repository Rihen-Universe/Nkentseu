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
        case NkSLBaseType::NK_BOOL: case NkSLBaseType::NK_INT: case NkSLBaseType::NK_UINT:
        case NkSLBaseType::NK_FLOAT: case NkSLBaseType::NK_DOUBLE: return 1;
        case NkSLBaseType::NK_IVEC2: case NkSLBaseType::NK_UVEC2:
        case NkSLBaseType::NK_VEC2:  case NkSLBaseType::NK_DVEC2: return 2;
        case NkSLBaseType::NK_IVEC3: case NkSLBaseType::NK_UVEC3:
        case NkSLBaseType::NK_VEC3:  case NkSLBaseType::NK_DVEC3: return 3;
        case NkSLBaseType::NK_IVEC4: case NkSLBaseType::NK_UVEC4:
        case NkSLBaseType::NK_VEC4:  case NkSLBaseType::NK_DVEC4: return 4;
        case NkSLBaseType::NK_MAT2:  return 4;
        case NkSLBaseType::NK_MAT3:  return 9;
        case NkSLBaseType::NK_MAT4:  return 16;
        default: return 0;
    }
}

NkSLBaseType NkSLResolvedType::ScalarBase() const {
    if (base >= NkSLBaseType::NK_IVEC2 && base <= NkSLBaseType::NK_IVEC4) return NkSLBaseType::NK_INT;
    if (base >= NkSLBaseType::NK_UVEC2 && base <= NkSLBaseType::NK_UVEC4) return NkSLBaseType::NK_UINT;
    if (base >= NkSLBaseType::NK_VEC2  && base <= NkSLBaseType::NK_VEC4)  return NkSLBaseType::NK_FLOAT;
    if (base >= NkSLBaseType::NK_DVEC2 && base <= NkSLBaseType::NK_DVEC4) return NkSLBaseType::NK_DOUBLE;
    if (base >= NkSLBaseType::NK_MAT2  && base <= NkSLBaseType::NK_MAT4)  return NkSLBaseType::NK_FLOAT;
    if (base >= NkSLBaseType::NK_DMAT2 && base <= NkSLBaseType::NK_DMAT4) return NkSLBaseType::NK_DOUBLE;
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
    if (!sym || sym->kind != NkSLSymbolKind::NK_FUNCTION &&
                sym->kind != NkSLSymbolKind::NK_BUILTIN_FUNC)
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
    if (from.base == NkSLBaseType::NK_INT   && to.base == NkSLBaseType::NK_FLOAT)  return true;
    if (from.base == NkSLBaseType::NK_INT   && to.base == NkSLBaseType::NK_UINT)   return true;
    if (from.base == NkSLBaseType::NK_FLOAT && to.base == NkSLBaseType::NK_DOUBLE) return true;
    if (from.base == NkSLBaseType::NK_INT   && to.base == NkSLBaseType::NK_DOUBLE) return true;
    // ivec → vec (même dimension)
    if (from.base == NkSLBaseType::NK_IVEC2 && to.base == NkSLBaseType::NK_VEC2)  return true;
    if (from.base == NkSLBaseType::NK_IVEC3 && to.base == NkSLBaseType::NK_VEC3)  return true;
    if (from.base == NkSLBaseType::NK_IVEC4 && to.base == NkSLBaseType::NK_VEC4)  return true;
    return false;
}

uint32 NkSLSymbolTable::VectorSize(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_IVEC2: case NkSLBaseType::NK_UVEC2:
        case NkSLBaseType::NK_VEC2:  case NkSLBaseType::NK_DVEC2: return 2;
        case NkSLBaseType::NK_IVEC3: case NkSLBaseType::NK_UVEC3:
        case NkSLBaseType::NK_VEC3:  case NkSLBaseType::NK_DVEC3: return 3;
        case NkSLBaseType::NK_IVEC4: case NkSLBaseType::NK_UVEC4:
        case NkSLBaseType::NK_VEC4:  case NkSLBaseType::NK_DVEC4: return 4;
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
        if (lhs.base == NkSLBaseType::NK_MAT4 && rhs.base == NkSLBaseType::NK_VEC4)
            return NkSLResolvedType::Vec4();
        if (lhs.base == NkSLBaseType::NK_MAT3 && rhs.base == NkSLBaseType::NK_VEC3)
            return NkSLResolvedType::Vec3();
        if (lhs.base == NkSLBaseType::NK_MAT4 && rhs.base == NkSLBaseType::NK_MAT4)
            return NkSLResolvedType::Mat4();
        if (lhs.base == NkSLBaseType::NK_MAT3 && rhs.base == NkSLBaseType::NK_MAT3)
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
        sym.kind = NkSLSymbolKind::NK_BUILTIN_VAR;
        sym.type.base = type;
        mCurrent->Define(sym);
    };
    defVar("gl_Position",          NkSLBaseType::NK_VEC4);
    defVar("gl_FragCoord",         NkSLBaseType::NK_VEC4);
    defVar("gl_FragDepth",         NkSLBaseType::NK_FLOAT);
    defVar("gl_VertexID",          NkSLBaseType::NK_INT);
    defVar("gl_InstanceID",        NkSLBaseType::NK_INT);
    defVar("gl_FrontFacing",       NkSLBaseType::NK_BOOL);
    defVar("gl_LocalInvocationID", NkSLBaseType::NK_UVEC3);
    defVar("gl_GlobalInvocationID",NkSLBaseType::NK_UVEC3);
    defVar("gl_WorkGroupID",       NkSLBaseType::NK_UVEC3);
    defVar("gl_WorkGroupSize",     NkSLBaseType::NK_UVEC3);
    defVar("gl_NumWorkGroups",     NkSLBaseType::NK_UVEC3);
    defVar("gl_PointSize",         NkSLBaseType::NK_FLOAT);
    defVar("gl_ClipDistance",      NkSLBaseType::NK_FLOAT); // array
    defVar("gl_Layer",             NkSLBaseType::NK_INT);
    defVar("gl_ViewportIndex",     NkSLBaseType::NK_INT);
}

// Nom GLSL d'un type de base (pour les constructeurs)
static const char* BaseTypeNameGLSL(NkSLBaseType t) {
    switch (t) {
        case NkSLBaseType::NK_VEC2:  return "vec2";  case NkSLBaseType::NK_VEC3:  return "vec3";
        case NkSLBaseType::NK_VEC4:  return "vec4";
        case NkSLBaseType::NK_IVEC2: return "ivec2"; case NkSLBaseType::NK_IVEC3: return "ivec3";
        case NkSLBaseType::NK_IVEC4: return "ivec4";
        case NkSLBaseType::NK_UVEC2: return "uvec2"; case NkSLBaseType::NK_UVEC3: return "uvec3";
        case NkSLBaseType::NK_UVEC4: return "uvec4";
        case NkSLBaseType::NK_MAT2:  return "mat2";  case NkSLBaseType::NK_MAT3:  return "mat3";
        case NkSLBaseType::NK_MAT4:  return "mat4";
        case NkSLBaseType::NK_FLOAT: return "float"; case NkSLBaseType::NK_INT:   return "int";
        case NkSLBaseType::NK_UINT:  return "uint";  case NkSLBaseType::NK_BOOL:  return "bool";
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
            NkSLSymbol sym; sym.name=name; sym.kind=NkSLSymbolKind::NK_BUILTIN_FUNC;
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
    for (auto t : {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4}) {
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
    for (auto t : {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4}) {
        defFunc("pow",         {t, t}, t);
        defFunc("exp",         {t}, t);
        defFunc("log",         {t}, t);
        defFunc("exp2",        {t}, t);
        defFunc("log2",        {t}, t);
        defFunc("sqrt",        {t}, t);
        defFunc("inversesqrt", {t}, t);
    }

    // ── Fonctions courantes ────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4,
                   NkSLBaseType::NK_INT,   NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_IVEC3, NkSLBaseType::NK_IVEC4}) {
        defFunc("abs",   {t}, t);
        defFunc("sign",  {t}, t);
        defFunc("min",   {t, t}, t);
        defFunc("max",   {t, t}, t);
    }
    for (auto t : {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4}) {
        defFunc("floor",      {t}, t);
        defFunc("trunc",      {t}, t);
        defFunc("round",      {t}, t);
        defFunc("roundEven",  {t}, t);
        defFunc("ceil",       {t}, t);
        defFunc("fract",      {t}, t);
        defFunc("mod",        {t, t}, t);
        defFunc("mod",        {t, NkSLBaseType::NK_FLOAT}, t);
        defFunc("modf",       {t, t}, t); // out param simplifié
        defFunc("min",        {t, NkSLBaseType::NK_FLOAT}, t);
        defFunc("max",        {t, NkSLBaseType::NK_FLOAT}, t);
        defFunc("clamp",      {t, t, t}, t);
        defFunc("clamp",      {t, NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_FLOAT}, t);
        defFunc("mix",        {t, t, t}, t);
        defFunc("mix",        {t, t, NkSLBaseType::NK_FLOAT}, t);
        defFunc("step",       {t, t}, t);
        defFunc("step",       {NkSLBaseType::NK_FLOAT, t}, t);
        defFunc("smoothstep", {t, t, t}, t);
        defFunc("smoothstep", {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_FLOAT, t}, t);
        defFunc("isnan",      {t}, NkSLBaseType::NK_BOOL);
        defFunc("isinf",      {t}, NkSLBaseType::NK_BOOL);
    }

    // ── Géométrie ──────────────────────────────────────────────────────────────
    for (auto t : {NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4}) {
        defFunc("length",    {t}, NkSLBaseType::NK_FLOAT);
        defFunc("distance",  {t, t}, NkSLBaseType::NK_FLOAT);
        defFunc("dot",       {t, t}, NkSLBaseType::NK_FLOAT);
        defFunc("normalize", {t}, t);
        defFunc("reflect",   {t, t}, t);
        defFunc("refract",   {t, t, NkSLBaseType::NK_FLOAT}, t);
        defFunc("faceforward",{t, t, t}, t);
    }
    defFunc("dot",   {NkSLBaseType::NK_FLOAT, NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_FLOAT);
    defFunc("cross", {NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_VEC3);

    // ── Matrices ───────────────────────────────────────────────────────────────
    defFunc("matrixCompMult", {NkSLBaseType::NK_MAT2, NkSLBaseType::NK_MAT2}, NkSLBaseType::NK_MAT2);
    defFunc("matrixCompMult", {NkSLBaseType::NK_MAT3, NkSLBaseType::NK_MAT3}, NkSLBaseType::NK_MAT3);
    defFunc("matrixCompMult", {NkSLBaseType::NK_MAT4, NkSLBaseType::NK_MAT4}, NkSLBaseType::NK_MAT4);
    defFunc("transpose",     {NkSLBaseType::NK_MAT4}, NkSLBaseType::NK_MAT4);
    defFunc("transpose",     {NkSLBaseType::NK_MAT3}, NkSLBaseType::NK_MAT3);
    defFunc("transpose",     {NkSLBaseType::NK_MAT2}, NkSLBaseType::NK_MAT2);
    defFunc("inverse",       {NkSLBaseType::NK_MAT4}, NkSLBaseType::NK_MAT4);
    defFunc("inverse",       {NkSLBaseType::NK_MAT3}, NkSLBaseType::NK_MAT3);
    defFunc("inverse",       {NkSLBaseType::NK_MAT2}, NkSLBaseType::NK_MAT2);
    defFunc("determinant",   {NkSLBaseType::NK_MAT4}, NkSLBaseType::NK_FLOAT);
    defFunc("determinant",   {NkSLBaseType::NK_MAT3}, NkSLBaseType::NK_FLOAT);
    defFunc("outerProduct",  {NkSLBaseType::NK_VEC4, NkSLBaseType::NK_VEC4}, NkSLBaseType::NK_MAT4);
    defFunc("outerProduct",  {NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_MAT3);

    // ── Textures ───────────────────────────────────────────────────────────────
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2},  NkSLBaseType::NK_VEC4);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER2D_SHADOW, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_FLOAT);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER2D_ARRAY, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_VEC4);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER2D_ARRAY_SHADOW, NkSLBaseType::NK_VEC4}, NkSLBaseType::NK_FLOAT);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER_CUBE, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_VEC4);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER_CUBE_SHADOW, NkSLBaseType::NK_VEC4}, NkSLBaseType::NK_FLOAT);
    defFunc("texture",    {NkSLBaseType::NK_SAMPLER3D, NkSLBaseType::NK_VEC3}, NkSLBaseType::NK_VEC4);
    defFunc("textureLod", {NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_VEC4);
    defFunc("textureLod", {NkSLBaseType::NK_SAMPLER3D, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_VEC4);
    defFunc("textureOffset",{NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_IVEC2}, NkSLBaseType::NK_VEC4);
    defFunc("textureGrad",{NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC2}, NkSLBaseType::NK_VEC4);
    defFunc("textureGather",{NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2}, NkSLBaseType::NK_VEC4);
    defFunc("textureGather",{NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_VEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_VEC4);
    defFunc("textureSize",{NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_INT}, NkSLBaseType::NK_IVEC2);
    defFunc("textureSize",{NkSLBaseType::NK_SAMPLER3D, NkSLBaseType::NK_INT}, NkSLBaseType::NK_IVEC3);
    defFunc("textureSize",{NkSLBaseType::NK_SAMPLER_CUBE, NkSLBaseType::NK_INT}, NkSLBaseType::NK_IVEC2);
    defFunc("texelFetch", {NkSLBaseType::NK_SAMPLER2D, NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_VEC4);

    // ── Image load/store ───────────────────────────────────────────────────────
    defFunc("imageLoad",       {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2}, NkSLBaseType::NK_VEC4);
    defFunc("imageStore",      {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_VEC4}, NkSLBaseType::NK_VOID);
    defFunc("imageAtomicAdd",  {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicMin",  {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicMax",  {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicAnd",  {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicOr",   {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicXor",  {NkSLBaseType::NK_IMAGE2D,  NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicExchange",{NkSLBaseType::NK_IMAGE2D, NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageAtomicCompSwap",{NkSLBaseType::NK_IMAGE2D, NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_INT, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("imageSize",       {NkSLBaseType::NK_IMAGE2D},  NkSLBaseType::NK_IVEC2);

    // ── Constructeurs vecteurs / matrices ──────────────────────────────────────
    for (auto t : {NkSLBaseType::NK_VEC2, NkSLBaseType::NK_VEC3, NkSLBaseType::NK_VEC4,
                   NkSLBaseType::NK_IVEC2, NkSLBaseType::NK_IVEC3, NkSLBaseType::NK_IVEC4,
                   NkSLBaseType::NK_UVEC2, NkSLBaseType::NK_UVEC3, NkSLBaseType::NK_UVEC4,
                   NkSLBaseType::NK_MAT2, NkSLBaseType::NK_MAT3, NkSLBaseType::NK_MAT4}) {
        NkSLSymbol* sym = mCurrent->symbols.Find(BaseTypeNameGLSL(t));
        if (!sym) {
            NkSLSymbol s; s.name = BaseTypeNameGLSL(t);
            s.kind = NkSLSymbolKind::NK_BUILTIN_FUNC;
            mCurrent->Define(s);
            sym = mCurrent->symbols.Find(s.name);
        }
        // Surcharge générique (le vrai nombre de composantes est vérifié dans le sémantique)
        NkSLFunctionOverload ov;
        ov.isBuiltin     = true;
        ov.returnType.base = t;
        // Un seul param générique (la vérification du nombre est faite dans le sémantique)
        NkSLResolvedType gf; gf.base = NkSLBaseType::NK_FLOAT;
        ov.paramTypes.PushBack(gf);
        sym->overloads.PushBack(ov);
    }

    // ── Divers ─────────────────────────────────────────────────────────────────
    defFunc("discard",        {}, NkSLBaseType::NK_VOID);
    defFunc("dFdx",           {NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_FLOAT);
    defFunc("dFdx",           {NkSLBaseType::NK_VEC2},  NkSLBaseType::NK_VEC2);
    defFunc("dFdx",           {NkSLBaseType::NK_VEC3},  NkSLBaseType::NK_VEC3);
    defFunc("dFdx",           {NkSLBaseType::NK_VEC4},  NkSLBaseType::NK_VEC4);
    defFunc("dFdy",           {NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_FLOAT);
    defFunc("dFdy",           {NkSLBaseType::NK_VEC2},  NkSLBaseType::NK_VEC2);
    defFunc("dFdy",           {NkSLBaseType::NK_VEC3},  NkSLBaseType::NK_VEC3);
    defFunc("dFdy",           {NkSLBaseType::NK_VEC4},  NkSLBaseType::NK_VEC4);
    defFunc("fwidth",         {NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_FLOAT);
    defFunc("barrier",        {}, NkSLBaseType::NK_VOID);
    defFunc("memoryBarrier",  {}, NkSLBaseType::NK_VOID);
    defFunc("groupMemoryBarrier", {}, NkSLBaseType::NK_VOID);
    defFunc("allInvocationsEqual", {NkSLBaseType::NK_BOOL}, NkSLBaseType::NK_BOOL);
    defFunc("packUnorm2x16",  {NkSLBaseType::NK_VEC2},  NkSLBaseType::NK_UINT);
    defFunc("unpackUnorm2x16",{NkSLBaseType::NK_UINT},  NkSLBaseType::NK_VEC2);
    defFunc("packHalf2x16",   {NkSLBaseType::NK_VEC2},  NkSLBaseType::NK_UINT);
    defFunc("unpackHalf2x16", {NkSLBaseType::NK_UINT},  NkSLBaseType::NK_VEC2);
    defFunc("bitfieldExtract",{NkSLBaseType::NK_INT, NkSLBaseType::NK_INT, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("bitfieldInsert", {NkSLBaseType::NK_INT, NkSLBaseType::NK_INT, NkSLBaseType::NK_INT, NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("bitCount",       {NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("findLSB",        {NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("findMSB",        {NkSLBaseType::NK_INT}, NkSLBaseType::NK_INT);
    defFunc("floatBitsToInt", {NkSLBaseType::NK_FLOAT}, NkSLBaseType::NK_INT);
    defFunc("intBitsToFloat", {NkSLBaseType::NK_INT}, NkSLBaseType::NK_FLOAT);
    defFunc("uintBitsToFloat",{NkSLBaseType::NK_UINT}, NkSLBaseType::NK_FLOAT);
    defFunc("emit",           {}, NkSLBaseType::NK_VOID); // geometry shader
    defFunc("EndPrimitive",   {}, NkSLBaseType::NK_VOID);
    defFunc("EmitVertex",     {}, NkSLBaseType::NK_VOID);
}



} // namespace nkentseu