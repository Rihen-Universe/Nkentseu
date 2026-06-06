#pragma once
// =============================================================================
// NkSLSymbolTable.h
// Table des symboles pour l'analyse sémantique NkSL.
// Gère les scopes imbriqués, la résolution des noms, les surcharges.
// =============================================================================
#include "NkSLAST.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    // =============================================================================
    // Représentation d'un type résolu (après analyse sémantique)
    // =============================================================================
    struct NkSLResolvedType {
        NkSLBaseType  base      = NkSLBaseType::NK_UNKNOWN;
        NkString      typeName;        // nom du struct si base==STRUCT
        uint32        arraySize = 0;   // 0 = pas tableau
        bool          isUnsized = false;
        bool          isConst   = false;
        bool          isRef     = false; // out / inout param

        bool IsVoid()    const { return base == NkSLBaseType::NK_VOID; }
        bool IsScalar()  const { return base>=NkSLBaseType::NK_BOOL  && base<=NkSLBaseType::NK_DOUBLE; }
        bool IsVector()  const { return base>=NkSLBaseType::NK_IVEC2 && base<=NkSLBaseType::NK_DVEC4; }
        bool IsMatrix()  const { return base>=NkSLBaseType::NK_MAT2  && base<=NkSLBaseType::NK_DMAT4; }
        bool IsSampler() const { return NkSLTypeIsSampler(base); }
        bool IsImage()   const { return NkSLTypeIsImage(base); }
        bool IsNumeric() const { return IsScalar() || IsVector() || IsMatrix(); }
        bool IsFloat()   const {
            return base==NkSLBaseType::NK_FLOAT ||
                (base>=NkSLBaseType::NK_VEC2 && base<=NkSLBaseType::NK_VEC4) ||
                IsMatrix();
        }

        // Nombre de composantes scalaires (1 pour float, 4 pour vec4, 16 pour mat4)
        uint32 ComponentCount() const;

        // Type de base scalaire sous-jacent (vec3 → float, ivec2 → int)
        NkSLBaseType ScalarBase() const;

        bool operator==(const NkSLResolvedType& o) const {
            return base==o.base && typeName==o.typeName && arraySize==o.arraySize;
        }
        bool operator!=(const NkSLResolvedType& o) const { return !(*this==o); }

        static NkSLResolvedType Void()  { return {NkSLBaseType::NK_VOID}; }
        static NkSLResolvedType Bool()  { return {NkSLBaseType::NK_BOOL}; }
        static NkSLResolvedType Int()   { return {NkSLBaseType::NK_INT}; }
        static NkSLResolvedType UInt()  { return {NkSLBaseType::NK_UINT}; }
        static NkSLResolvedType Float() { return {NkSLBaseType::NK_FLOAT}; }
        static NkSLResolvedType Vec2()  { return {NkSLBaseType::NK_VEC2}; }
        static NkSLResolvedType Vec3()  { return {NkSLBaseType::NK_VEC3}; }
        static NkSLResolvedType Vec4()  { return {NkSLBaseType::NK_VEC4}; }
        static NkSLResolvedType Mat3()  { return {NkSLBaseType::NK_MAT3}; }
        static NkSLResolvedType Mat4()  { return {NkSLBaseType::NK_MAT4}; }
        static NkSLResolvedType FromNode(NkSLTypeNode* t);
    };

    // =============================================================================
    // Entrée dans la table des symboles
    // =============================================================================
    enum class NkSLSymbolKind : uint32 {
        NK_VARIABLE,
        NK_FUNCTION,
        NK_STRUCT_TYPE,
        NK_PARAMETER,
        NK_BUILTIN_VAR,
        NK_BUILTIN_FUNC,
    };

    struct NkSLFunctionOverload {
        NkVector<NkSLResolvedType> paramTypes;
        NkSLResolvedType           returnType;
        NkSLFunctionDeclNode*      decl = nullptr; // nullptr pour les builtins
        bool                       isBuiltin = false;
    };

    struct NkSLSymbol {
        NkString                          name;
        NkSLSymbolKind                    kind    = NkSLSymbolKind::NK_VARIABLE;
        NkSLResolvedType                  type;   // pour variables/params
        NkVector<NkSLFunctionOverload>    overloads; // pour fonctions
        NkSLStructDeclNode*               structDecl = nullptr;
        NkSLVarDeclNode*                  varDecl    = nullptr;
        uint32                            line       = 0;
    };

    // =============================================================================
    // Scope (portée)
    // =============================================================================
    struct NkSLScope {
        NkUnorderedMap<NkString, NkSLSymbol> symbols;
        NkSLScope*                            parent  = nullptr;
        NkSLResolvedType                      returnType; // type retour de la fonction courante
        bool                                  isFunction  = false;
        bool                                  isLoop      = false;

        NkSLSymbol* Find(const NkString& name);
        bool        Define(const NkSLSymbol& sym);
        bool        Has(const NkString& name) const;
    };

    // =============================================================================
    // Table des symboles complète
    // =============================================================================
    class NkSLSymbolTable {
        public:
            NkSLSymbolTable();

            void PushScope(bool isFunction=false, bool isLoop=false,
                        NkSLResolvedType returnType = NkSLResolvedType::Void());
            void PopScope();
            NkSLScope* CurrentScope() { return mCurrent; }

            // Définir un symbole dans le scope courant
            bool Define(const NkSLSymbol& sym);

            // Résoudre un nom (cherche du scope courant vers les scopes parents)
            NkSLSymbol* Resolve(const NkString& name);

            // Résoudre une fonction avec correspondance des arguments
            const NkSLFunctionOverload* ResolveFunction(
                const NkString& name,
                const NkVector<NkSLResolvedType>& argTypes);

            // Vérifier si on est dans une boucle (pour break/continue)
            bool IsInLoop() const;
            // Type de retour de la fonction courante
            NkSLResolvedType CurrentReturnType() const;
            // Vérifier si on est dans une fonction
            bool IsInFunction() const;

            // Compatibilité de types (pour les opérateurs, les affectations, les appels)
            static bool IsAssignable(const NkSLResolvedType& from, const NkSLResolvedType& to);
            static bool IsImplicitlyConvertible(const NkSLResolvedType& from, const NkSLResolvedType& to);
            // Type résultant d'une opération binaire (vec3 * float → vec3, etc.)
            static NkSLResolvedType BinaryResultType(const NkString& op,
                                                    const NkSLResolvedType& lhs,
                                                    const NkSLResolvedType& rhs);
            // Nombre de composantes pour un type vecteur
            static uint32 VectorSize(NkSLBaseType t);

        private:
            void RegisterBuiltins();
            void RegisterBuiltinFunctions();
            void RegisterBuiltinVars();

            NkSLScope*              mCurrent = nullptr;
            NkVector<NkSLScope*>    mScopes;  // ownership
    };

} // namespace nkentseu
