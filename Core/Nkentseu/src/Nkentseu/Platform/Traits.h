#pragma once

#include "Types.h"

namespace nkentseu
{
    
    // Trait pour le caractère nul de fin de chaîne
    template<typename CharT>
    struct NullTerminator {
        static constexpr CharT value = CharT(0);
    };

    // Spécialisations explicites pour plus de clarté
    template<> struct NullTerminator<charb>  { static constexpr charb  value = '\0'; };
    template<> struct NullTerminator<char8>  { static constexpr char8  value = u8'\0'; };
    template<> struct NullTerminator<char16> { static constexpr char16 value = u'\0'; };
    template<> struct NullTerminator<char32> { static constexpr char32 value = U'\0'; };
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
    // Spécialisation explicite uniquement pour wchar_t Windows
    template<> struct NullTerminator<wchar> { 
        static constexpr wchar value = L'\0'; // L'' pour wchar_t
    };
    #endif

    // Structures de base pour true/false
    template<bool Val>
    struct BoolConstant {
        static constexpr bool value = Val;
        using type = BoolConstant;
        using value_type = bool;
        constexpr operator bool() const noexcept { return value; }
    };

    using TrueType  = BoolConstant<true>;
    using FalseType = BoolConstant<false>;

    // Implémentation de base de IsSame
    template<typename T, typename U>
    struct IsSame : FalseType {}; // Cas général = types différents

    // Spécialisation quand les types sont identiques
    template<typename T>
    struct IsSame<T, T> : TrueType {}; // Cas spécialisé = même type

    // Version value pour une utilisation directe
    template<typename T, typename U>
    inline constexpr bool IsSame_v = IsSame<T, U>::value;

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        #define NK_LITERAL(CharT, str) \
            IsSame_v<CharT, charb>  ? str : \
            IsSame_v<CharT, char8>  ? u8##str : \
            IsSame_v<CharT, char16> ? u##str : \
            IsSame_v<CharT, char32> ? U##str : \
            IsSame_v<CharT, wchar>  ? L##str : \
            str
    #else
        #define NK_LITERAL(CharT, str) \
            IsSame_v<CharT, charb>  ? str : \
            IsSame_v<CharT, char8>  ? u8##str : \
            IsSame_v<CharT, char16> ? u##str : \
            IsSame_v<CharT, char32> ? U##str : \
            str
    #endif

    #define SLTS(CharT, str) NK_LITERAL(CharT, str)
    #define SLTT(CharT) NullTerminator<CharT>::value

    #if defined(NKENTSEU_PLATFORM_WINDOWS)
    #define NK_WIDE(str) L##str
    #else
    #define NK_WIDE(str) U##str
    #endif

    // Type alias de base
    template<bool B, typename T = void>
    struct EnableIf {};

    template<typename T>
    struct EnableIf<true, T> { using type = T; };

    template<bool B, typename T = void>
    using EnableIf_t = typename EnableIf<B, T>::type;

    // Conditional
    template<bool B, typename T, typename F>
    struct Conditional { using type = T; };

    template<typename T, typename F>
    struct Conditional<false, T, F> { using type = F; };

    template<bool B, typename T, typename F>
    using Conditional_t = typename Conditional<B, T, F>::type;

    // Intégral check
    template<typename T>
    struct IsIntegral { static constexpr bool value = false; };

    // Spécialisations pour tous les types entiers
    template<> struct IsIntegral<bool>       { static constexpr bool value = true; };
    template<> struct IsIntegral<charb>      { static constexpr bool value = true; };
    template<> struct IsIntegral<int8>       { static constexpr bool value = true; };
    template<> struct IsIntegral<uint8>      { static constexpr bool value = true; };
    template<> struct IsIntegral<int16>      { static constexpr bool value = true; };
    template<> struct IsIntegral<uint16>     { static constexpr bool value = true; };
    template<> struct IsIntegral<int32>      { static constexpr bool value = true; };
    template<> struct IsIntegral<uint32>     { static constexpr bool value = true; };
    template<> struct IsIntegral<int64>      { static constexpr bool value = true; };
    template<> struct IsIntegral<uint64>     { static constexpr bool value = true; };
    template<> struct IsIntegral<uintl32>    { static constexpr bool value = true; };

    // Spécialisations pour les caractères étendus
    template<> struct IsIntegral<char16>     { static constexpr bool value = true; };
    template<> struct IsIntegral<char32>     { static constexpr bool value = true; };
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        template<> struct IsIntegral<wchar>      { static constexpr bool value = true; };
    #endif

    template<typename T>
    inline constexpr bool IsIntegral_v = IsIntegral<T>::value;

    // Version simplifiée pour les autres traits
    template<typename T>
    struct IsFloatingPoint { static constexpr bool value = false; };

    template<typename T>
    inline constexpr bool IsFloatingPoint_v = IsFloatingPoint<T>::value;

    template<> struct IsFloatingPoint<float32> { static constexpr bool value = true; };
    template<> struct IsFloatingPoint<float64> { static constexpr bool value = true; };
    template<> struct IsFloatingPoint<float80> { static constexpr bool value = true; };

    template<typename T>
    struct IsBooleanNatif { static constexpr bool value = false; };
    template<typename T>
    inline constexpr bool IsBooleanNatif_v = IsBooleanNatif<T>::value;
    template<> struct IsBooleanNatif<bool> { static constexpr bool value = true; };

    template<typename T>
    struct IsSigned {
        static constexpr bool value = T(-1) < T(0);
    };

    template<typename T>
    inline constexpr bool IsSigned_v = IsSigned<T>::value;

    template<typename T>
    struct IsUnsigned {
        static constexpr bool value = T(0) < T(-1);
    };

    template<typename T>
    inline constexpr bool IsUnsigned_v = IsUnsigned<T>::value;

    // Type de base vide
    // struct TrueType { static constexpr bool value = true; };
    // struct FrueType { static constexpr bool value = false; };

    // Exemple pour IsPointer
    template<typename T>
    struct IsPointer : FalseType{};

    template<typename T>
    struct IsPointer<T*> : TrueType {};

    template<typename T>
    inline constexpr bool IsPointer_v = IsPointer<T>::value;

    template<typename T> struct MakeUnsigned;

    template<> struct MakeUnsigned<int8> { using type = uint8; };
    template<> struct MakeUnsigned<int16> { using type = uint16; };
    template<> struct MakeUnsigned<int32> { using type = uint32; };
    template<> struct MakeUnsigned<int64> { using type = uint64; };
    template<> struct MakeUnsigned<uint8> { using type = uint8; };
    template<> struct MakeUnsigned<uint16> { using type = uint16; };
    template<> struct MakeUnsigned<uint32> { using type = uint32; };
    template<> struct MakeUnsigned<uint64> { using type = uint64; };

    template<typename T>
    struct DefaultPrecision {
        static constexpr usize value = 0;
    };

    template<>
    struct DefaultPrecision<float32> {
        static constexpr usize value = 6;
    };

    template<>
    struct DefaultPrecision<float64> {
        static constexpr usize value = 15;
    };
    
    template<typename CharT>
    struct NKENTSEU_TEMPLATE_API ConversionTraits {
        static const CharT TrueStr[];    // Tableau de taille inconnue
        static const CharT FalseStr[];
        static const CharT NullPtrStr[];
    };

    #define DEFINE_CONVERSION_TRAITS(CharType, PREFIX) \
    template<> \
    struct ConversionTraits<CharType> { \
        static constexpr CharType TrueStr[] = PREFIX ## "true"; \
        static constexpr CharType FalseStr[] = PREFIX ## "false"; \
        static constexpr CharType NullPtrStr[] = PREFIX ## "(null)"; \
    };

    // Génération des spécialisations pour chaque type de caractère
    DEFINE_CONVERSION_TRAITS(charb,)
    DEFINE_CONVERSION_TRAITS(char8, u8)
    DEFINE_CONVERSION_TRAITS(char16, u)
    DEFINE_CONVERSION_TRAITS(char32, U)
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        DEFINE_CONVERSION_TRAITS(wchar, L)
    #endif

    #define NK_UNUSED (void)

    ///////////////////////////////////////////////////////////////////////////////
    // 1. Différencier classes/structs
    ///////////////////////////////////////////////////////////////////////////////

    template<typename T>
    struct IsClass {
    private:
        template<typename U> 
        static char test(int U::*); // Teste si on peut déclarer un pointeur sur membre
        
        template<typename> 
        static int test(...); // Fallback

    public:
        static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(char);
    };

    template<typename T>
    inline constexpr bool IsClass_v = IsClass<T>::value;

    // Version pour les structs (même implémentation que les classes)
    template<typename T>
    inline constexpr bool IsStruct_v = IsClass_v<T>;

    // Implémentation de underlying_type
    template<typename T>
    struct UnderlyingType {
        using type = __underlying_type(T); // Extension de compilateur
    };

    template<typename T>
    using UnderlyingType_t = typename UnderlyingType<T>::type;

    ///////////////////////////////////////////////////////////////////////////////
    // 2. Détection des enums
    ///////////////////////////////////////////////////////////////////////////////

    template<typename T>
    struct IsEnum {
    private:
        template<typename U>
        static char test(typename UnderlyingType<U>::type*);
        
        template<typename>
        static int test(...);

    public:
        static constexpr bool value = sizeof(test<T>(nullptr)) == sizeof(char);
    };

    template<typename T>
    inline constexpr bool IsEnum_v = IsEnum<T>::value;

    ///////////////////////////////////////////////////////////////////////////////
    // 3. Détection des unions
    ///////////////////////////////////////////////////////////////////////////////

    template<typename T>
    struct IsUnion {
        static constexpr bool value = __is_union(T); // Extension compiler-dépendante
    };

    template<typename T>
    inline constexpr bool IsUnion_v = IsUnion<T>::value;

    ///////////////////////////////////////////////////////////////////////////////
    // 4. Détection des fonctions
    ///////////////////////////////////////////////////////////////////////////////

    template<typename T>
    struct IsFunction : FalseType {};

    // Spécialisations pour tous les types de fonctions
    template<typename Ret, typename... Args>
    struct IsFunction<Ret(Args...)> : TrueType {};

    template<typename Ret, typename... Args>
    struct IsFunction<Ret(Args..., ...)> : TrueType {}; // Fonctions variadiques

    template<typename T>
    inline constexpr bool IsFunction_v = IsFunction<T>::value;

    ///////////////////////////////////////////////////////////////////////////////
    // 5. Détection des tableaux
    ///////////////////////////////////////////////////////////////////////////////

    template<typename T>
    struct IsArray : FalseType {};

    template<typename T, size_t N>
    struct IsArray<T[N]> : TrueType {};

    template<typename T>
    struct IsArray<T[]> : TrueType {};

    template<typename T>
    inline constexpr bool IsArray_v = IsArray<T>::value;

    //-----------------------------------------------------------------------------
    // Type traits avancés
    //-----------------------------------------------------------------------------

    // Détection d'héritage
    template<typename Base, typename Derived>
    struct IsDerivedFrom {
    private:
        template<typename T>
        static TrueType test(Base*, T);
        static FalseType test(void*, ...);

    public:
        static constexpr bool value = decltype(test(static_cast<Derived*>(nullptr), nullptr))::value;
    };

    template<typename Base, typename Derived>
    inline constexpr bool IsDerivedFrom_v = IsDerivedFrom<Base, Derived>::value;

    //-----------------------------------------------------------------------------
    // Système de réflexion de base
    //-----------------------------------------------------------------------------

    //-----------------------------------------------------------------------------
    // remove_const_t
    //-----------------------------------------------------------------------------

    template<typename T>
    struct RemoveConst { using type = T; };

    template<typename T>
    struct RemoveConst<const T> { using type = T; };

    template<typename T>
    using RemoveConst_t = typename RemoveConst<T>::type;

    //-----------------------------------------------------------------------------
    // is_base_of_v
    //-----------------------------------------------------------------------------

    namespace detail {
        template<typename B, typename D>
        auto test_base_of(B*) -> TrueType;
        
        template<typename>
        auto test_base_of(...) -> FalseType;
    }

    template<typename Base, typename Derived>
    struct IsBaseOf {
        using BaseNaked = RemoveConst_t<Base>;
        using DerivedNaked = RemoveConst_t<Derived>;
        
        static constexpr bool value = decltype(
            detail::test_base_of<BaseNaked>(
                static_cast<DerivedNaked*>(nullptr)
            )
        )::value;
    };

    template<typename B, typename D>
    inline constexpr bool IsBaseOf_v = IsBaseOf<B, D>::value;

    //-----------------------------------------------------------------------------
    // is_polymorphic_v
    //-----------------------------------------------------------------------------

    namespace detail {
        template<typename T>
        struct IsPolymorphicHelper {
            struct Helper : T {
                virtual ~Helper() {} // Force la polymorphie si T est polymorphique
            };
            
            static constexpr bool value = !__is_trivially_constructible(Helper);
        };
    }

    template<typename T>
    struct IsPolymorphic : BoolConstant<
        std::is_class_v<T> && 
        detail::IsPolymorphicHelper<T>::value
    > {};

    template<typename T>
    inline constexpr bool IsPolymorphic_v = IsPolymorphic<T>::value;

    //-----------------------------------------------------------------------------
    // is_convertible_v
    //-----------------------------------------------------------------------------

    namespace detail {
        template<typename From, typename To>
        auto test_convertible(int) -> decltype(
            void(static_cast<To(*)()>(nullptr)(static_cast<From(*)()>(nullptr))), // Correction des parenthèses
            TrueType{}
        );
        
        template<typename, typename>
        auto test_convertible(...) -> FalseType;
    }

    template<typename From, typename To>
    struct IsConvertible : decltype(
        detail::test_convertible<From, To>(0)
    ) {};

    template<typename From, typename To>
    inline constexpr bool IsConvertible_v = IsConvertible<From, To>::value;
    struct MethodInfo {
        std::string name;
        std::string signature;
    };

    class Object;

    struct TypeInfo {
        std::string name;
        std::vector<const TypeInfo*> bases;
        std::unordered_map<std::string, MethodInfo> methods;

        template<typename T>
        static const TypeInfo* Get() {
            static_assert(IsBaseOf_v<Object, T>, "Types must inherit from Object");
            return &T::StaticTypeInfo();
        }
    };

    class Object {
    public:
        virtual ~Object() = default;
        
        virtual const TypeInfo* GetTypeInfo() const = 0;
        
        template<typename T>
        bool IsInstanceOf() const {
            return IsDerivedFrom_v<T, RemoveConst_t<decltype(*this)>>;
        }

        static const TypeInfo* ClassTypeInfo() {
            static TypeInfo info{
                "Object",
                {},
                {}
            };
            return &info;
        }
    };

    //-----------------------------------------------------------------------------
    // Macros pour la déclaration de types
    //-----------------------------------------------------------------------------

    #define DECLARE_CLASS(ClassName, ...) \
        public: \
            using BaseClasses = std::tuple<__VA_ARGS__>; \
            static const TypeInfo& StaticTypeInfo() { \
                static TypeInfo info{ \
                    #ClassName, \
                    {__VA_ARGS__::ClassTypeInfo()}, \
                    {} \
                }; \
                return info; \
            } \
            const TypeInfo* GetTypeInfo() const override { \
                return &StaticTypeInfo(); \
            }

    #define DECLARE_METHOD(ClassName, ReturnType, MethodName, ...) \
        class MethodName##_tag {}; \
        template<> \
        struct MethodTraits<ClassName::MethodName##_tag> { \
            static inline MethodInfo info{ \
                #MethodName, \
                #ReturnType " " #MethodName "(" #__VA_ARGS__ ")" \
            }; \
        }; \
        ReturnType MethodName(__VA_ARGS__)

    //-----------------------------------------------------------------------------
    // Infrastructure de réflexion
    //-----------------------------------------------------------------------------

    template<typename T>
    struct MethodTraits;

    template<typename Ret, typename... Args>
    struct MethodTraits<Ret(*)(Args...)> {
        using ReturnType = Ret;
        using Parameters = std::tuple<Args...>;
        static constexpr std::string_view name = "";
    };

    #define RegisterMethod \
        }; /* Fermeture de la portée précédente */ \
        template<> \
        struct MethodTraits<__COUNTER__> { \
            static constexpr auto _name = __FUNCTION__; \
            using Signature = decltype([](auto&&... args)

    // Pour supporter les méthodes const
    #define RegisterCMethod \
        }; \
        template<> \
        struct MethodTraits<__COUNTER__> { \
            static constexpr auto _name = __FUNCTION__; \
            using Signature = decltype([](const auto& self, auto&&... args)

    //-----------------------------------------------------------------------------
    // Fonctionnalités avancées
    //-----------------------------------------------------------------------------

    template<typename T>
    constexpr bool IsObjectType = IsDerivedFrom_v<Object, T>;

    template<class...> struct MakeVoid { using type = void; };
    template<class... T> using Void_t = typename MakeVoid<T...>::type;

    ///////////////////////////////////////////////////////////////////////////
    // Déterminer si un type est "référençable"
    ///////////////////////////////////////////////////////////////////////////
    
    template<typename T>
    struct IsReferenceable : TrueType {}; // Par défaut vrai

    template<>
    struct IsReferenceable<void> : FalseType {}; // Void n'est pas référençable

    template<typename T>
    struct IsReferenceable<const T> : IsReferenceable<T> {}; // Propagation cv

    template<typename T>
    struct IsReferenceable<volatile T> : IsReferenceable<T> {};

    template<typename T>
    struct IsReferenceable<const volatile T> : IsReferenceable<T> {};

    ///////////////////////////////////////////////////////////////////////////
    // Implémentation de AddRValueReference
    ///////////////////////////////////////////////////////////////////////////
    
    template<typename T, typename = void>
    struct AddRValueReference_impl {
        using type = T&&; // Cas général pour types référençables
    };

    template<typename T>
    struct AddRValueReference_impl<T, EnableIf_t<!IsReferenceable<T>::value>> {
        using type = T; // Cas spécial pour types non référençables
    };

    template<typename T>
    struct AddRValueReference {
        using type = typename AddRValueReference_impl<T>::type;
    };

    template<typename T>
    using AddRValueReference_t = typename AddRValueReference<T>::type;

    ///////////////////////////////////////////////////////////////////////////
    // Implémentation de declval
    ///////////////////////////////////////////////////////////////////////////
    
    template<typename T>
    AddRValueReference_t<T> DeclVal() noexcept {
        static_assert(IsReferenceable<T>::value, "T doit être référençable");
        // Ne sera jamais compilé - utilisé uniquement en contexte non-évalué
    };

    template<typename T>
    typename AddRValueReference<T>::type DeclVal() noexcept;
    
    // Version alternative avec syntaxe moderne
    template<typename T>
    T&& DeclVal() noexcept;

    // Helper to Get argument by index
    template<usize Index, typename... Args>
    struct GetArgHelper;

    template<typename T, typename... Rest>
    struct GetArgHelper<0, T, Rest...> {
        static T Get(T&& t, Rest&&...) { return t; }
    };

    template<usize Index, typename T, typename... Rest>
    struct GetArgHelper<Index, T, Rest...> {
        static auto Get(T&&, Rest&&... rest) {
            return GetArgHelper<Index - 1, Rest...>::Get(rest...);
        }
    };

    template<usize Index, typename... Args>
    auto GetArg(Args&&... args) {
        return GetArgHelper<Index, Args...>::Get(args...);
    }
} // namespace nkentseu
