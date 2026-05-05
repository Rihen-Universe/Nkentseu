// =============================================================================
// FICHIER: NKECS/Reflect/NkReflect.h
// DESCRIPTION: Système de réflexion complet avec métadonnées extensibles.
// =============================================================================
#pragma once
#include "../NkECSDefines.h"
#include "../Core/NkTypeRegistry.h"
#include <cstdint>
#include <cstring>
#include <functional>

namespace nkentseu {
namespace ecs {
namespace reflect {

    // ============================================================================
    // 1. TYPES DE CHAMPS SUPPORTÉS
    // ============================================================================
    enum class NkFieldType : uint8 {
        // ── Primitifs ────────────────────────────────────────────────────────
        Unknown     = 0,
        Bool        = 1,
        Int8        = 2,
        Int16       = 3,
        Int32       = 4,
        Int64       = 5,
        UInt8       = 6,
        UInt16      = 7,
        UInt32      = 8,
        UInt64      = 9,
        Float32     = 10,
        Float64     = 11,
        String      = 12,
        
        // ── Mathématiques ─────────────────────────────────────────────────────
        Vec2        = 20,
        Vec3        = 21,
        Vec4        = 22,
        Quat        = 23,
        Mat4        = 24,
        
        // ── ECS ───────────────────────────────────────────────────────────────
        EntityId    = 30,
        ComponentId = 31,
        ArchetypeId = 32,
        
        // ── Complexes ─────────────────────────────────────────────────────────
        Array       = 40,   // Tableau d'éléments homogènes
        Object      = 41,   // Struct imbriquée avec ses propres FieldInfos
        Enum        = 42,   // Énumération nommée
        Flags       = 43,   // Bitfield d'flags
    };

    // ============================================================================
    // 2. MÉTADONNÉES EXTENSIBLES (FLAGS 64-BITS)
    // ============================================================================
    enum NkMetaFlag : uint64 {
        // ── Visibilité ───────────────────────────────────────────────────────
        NkMeta_Visible          = 1ULL << 0,
        NkMeta_HideInEditor     = 1ULL << 1,
        NkMeta_ReadOnly         = 1ULL << 2,
        NkMeta_Advanced         = 1ULL << 3,
        
        // ── Sérialisation ────────────────────────────────────────────────────
        NkMeta_Serialize        = 1ULL << 4,
        NkMeta_NoSerialize      = 1ULL << 5,
        NkMeta_SerializeDefault = 1ULL << 6,
        
        // ── Édition ──────────────────────────────────────────────────────────
        NkMeta_EditAnywhere     = 1ULL << 8,
        NkMeta_EditDefaultsOnly = 1ULL << 9,
        NkMeta_EditFixedSize    = 1ULL << 10,
        NkMeta_NoEdit           = 1ULL << 11,
        
        // ── Blueprint / Scripting ────────────────────────────────────────────
        NkMeta_BlueprintReadWrite = 1ULL << 16,
        NkMeta_BlueprintReadOnly  = 1ULL << 17,
        NkMeta_BlueprintCallable  = 1ULL << 18,
        NkMeta_BlueprintPure      = 1ULL << 19,
        
        // ── Réseau ───────────────────────────────────────────────────────────
        NkMeta_Replicated         = 1ULL << 24,
        NkMeta_RepNotify          = 1ULL << 25,
        NkMeta_RepSkipOwner       = 1ULL << 26,
        
        // ── UI / Affichage ───────────────────────────────────────────────────
        NkMeta_Range              = 1ULL << 32,
        NkMeta_Password           = 1ULL << 33,
        NkMeta_Multiline          = 1ULL << 34,
        NkMeta_ColorPicker        = 1ULL << 35,
        
        // ── Utilitaires ──────────────────────────────────────────────────────
        NkMeta_Instanced          = 1ULL << 40,
        NkMeta_Transient          = 1ULL << 41,
        NkMeta_Duplicate          = 1ULL << 42,
        NkMeta_NeverDuplicate     = 1ULL << 43,
        
        // ── Réservé utilisateur ──────────────────────────────────────────────
        NkMeta_User0 = 1ULL << 56,
        NkMeta_User1 = 1ULL << 57,
        NkMeta_User2 = 1ULL << 58,
        NkMeta_User3 = 1ULL << 59,
        NkMeta_User4 = 1ULL << 60,
        NkMeta_User5 = 1ULL << 61,
        NkMeta_User6 = 1ULL << 62,
        NkMeta_User7 = 1ULL << 63,
    };

    // ============================================================================
    // 3. NkFieldInfo — Description d'un champ avec métadonnées
    // ============================================================================
    struct NkFieldInfo {
        const char*   name            = "<field>";
        NkFieldType   type            = NkFieldType::Unknown;
        uint32        offset          = 0;      // offsetof dans la struct parente
        uint32        size            = 0;      // sizeof du champ
        uint32        count           = 1;      // nombre d'éléments (array)
        uint64        metaFlags       = NkMeta_Visible | NkMeta_EditAnywhere | NkMeta_Serialize;
        
        // ── Métadonnées éditeur ──────────────────────────────────────────────
        const char*   category        = nullptr;    // Catégorie dans l'UI
        const char*   tooltip         = nullptr;    // Texte d'aide au survol
        const char*   displayName     = nullptr;    // Nom lisible dans l'UI
        float32       minValue        = 0.f;        // Pour sliders/ranges
        float32       maxValue        = 1.f;
        const char*   editCondition   = nullptr;    // Nom d'une propriété qui contrôle l'édition
        const char*   replicates      = nullptr;    // Flags réseau
        
        // ── Sous-champs si type == Object ────────────────────────────────────
        const NkFieldInfo* nested     = nullptr;
        uint32             nestedCount = 0;
        
        // ── Valeur par défaut (pour Reset) ───────────────────────────────────
        const void*       defaultValue = nullptr;
        
        // ── Helpers ──────────────────────────────────────────────────────────
        [[nodiscard]] bool HasFlag(NkMetaFlag flag) const noexcept {
            return (metaFlags & static_cast<uint64>(flag)) != 0;
        }
        
        [[nodiscard]] bool IsArray() const noexcept {
            return count > 1 || type == NkFieldType::Array;
        }
        
        [[nodiscard]] bool IsEditable() const noexcept {
            return !HasFlag(NkMeta_ReadOnly) && !HasFlag(NkMeta_NoEdit);
        }
        
        [[nodiscard]] bool IsSerializable() const noexcept {
            return HasFlag(NkMeta_Serialize) && !HasFlag(NkMeta_NoSerialize);
        }
    };

    // ============================================================================
    // 4. NkTypeInfo — Informations de réflexion complètes pour un type
    // ============================================================================
    struct NkTypeInfo {
        NkComponentId   componentId     = kInvalidComponentId;
        const char*     name            = "<type>";
        uint32          size            = 0;
        uint32          align           = 1;
        const NkFieldInfo* fields       = nullptr;
        uint32          fieldCount      = 0;
        
        // ── Fonctions de cycle de vie ────────────────────────────────────────
        using CtorFn = void(*)(void*);
        using DtorFn = void(*)(void*);
        using CopyFn = void(*)(void*, const void*);
        using MoveFn = void(*)(void*, void*);
        
        CtorFn defaultCtor = nullptr;
        DtorFn dtor        = nullptr;
        CopyFn copyCtor    = nullptr;
        MoveFn moveCtor    = nullptr;
        
        // ── Sérialisation JSON ───────────────────────────────────────────────
        using SerializeFn   = bool(*)(const void*, char*, uint32);
        using DeserializeFn = bool(*)(void*, const char*);
        
        SerializeFn   serialize   = nullptr;
        DeserializeFn deserialize = nullptr;
        
        // ── Helpers ──────────────────────────────────────────────────────────
        [[nodiscard]] const NkFieldInfo* FindField(const char* name) const noexcept {
            for (uint32 i = 0; i < fieldCount; ++i) {
                if (std::strcmp(fields[i].name, name) == 0) {
                    return &fields[i];
                }
            }
            return nullptr;
        }
        
        [[nodiscard]] bool HasField(const char* name) const noexcept {
            return FindField(name) != nullptr;
        }
    };

    // ============================================================================
    // 5. NkReflectRegistry — Registre global des types réfléchis
    // ============================================================================
    class NkReflectRegistry {
    public:
        [[nodiscard]] static NkReflectRegistry& Global() noexcept {
            static NkReflectRegistry instance;
            return instance;
        }
        
        // Enregistre un type avec ses métadonnées
        void Register(const NkTypeInfo& info) noexcept {
            if (info.componentId >= kMaxComponentTypes) {
                return;
            }
            mTypes[info.componentId] = info;
            if (info.componentId >= mCount) {
                mCount = info.componentId + 1;
            }
        }
        
        // Récupère les infos d'un type par ComponentId
        [[nodiscard]] const NkTypeInfo* Get(NkComponentId id) const noexcept {
            if (id >= mCount) {
                return nullptr;
            }
            return &mTypes[id];
        }
        
        // Récupère les infos d'un type par nom
        [[nodiscard]] const NkTypeInfo* GetByName(const char* name) const noexcept {
            for (uint32 i = 0; i < mCount; ++i) {
                if (mTypes[i].name && std::strcmp(mTypes[i].name, name) == 0) {
                    return &mTypes[i];
                }
            }
            return nullptr;
        }
        
        // Itère sur tous les types enregistrés
        template<typename Fn>
        void ForEach(Fn&& fn) const noexcept {
            for (uint32 i = 0; i < mCount; ++i) {
                if (mTypes[i].name) {
                    fn(mTypes[i]);
                }
            }
        }
        
        [[nodiscard]] uint32 Count() const noexcept {
            return mCount;
        }
        
    private:
        static constexpr uint32 kMaxTypes = 512u;
        NkTypeInfo mTypes[kMaxTypes] = {};
        uint32     mCount = 0;
    };

    // ============================================================================
    // 6. MACROS DE DÉCLARATION DE RÉFLEXION
    // ============================================================================
    
    // Macro principale pour déclarer un type réflexif
    #define NK_REFLECT_BEGIN(Type)                                              \
    namespace {                                                                 \
    struct _NkReflectReg_##Type {                                              \
        _NkReflectReg_##Type() noexcept {                                      \
            using _T = Type;                                                   \
            static constexpr ::nkentseu::ecs::reflect::NkFieldInfo _fields[] = {
    
    // Déclare un champ avec métadonnées par défaut
    #define NK_FIELD(FieldName)                                                \
        { #FieldName,                                                          \
          ::nkentseu::ecs::reflect::NkFieldType::Unknown,                     \
          static_cast<uint32>(offsetof(_T, FieldName)),                       \
          static_cast<uint32>(sizeof(std::declval<_T>().FieldName)),          \
          1, NkMeta_Visible | NkMeta_EditAnywhere | NkMeta_Serialize,         \
          nullptr, nullptr, nullptr, 0.f, 1.f, nullptr, nullptr, nullptr, 0, nullptr },
    
    // Déclare un champ avec type explicite
    #define NK_FIELD_EX(FieldName, FieldType)                                  \
        { #FieldName, FieldType,                                               \
          static_cast<uint32>(offsetof(_T, FieldName)),                       \
          static_cast<uint32>(sizeof(std::declval<_T>().FieldName)),          \
          1, NkMeta_Visible | NkMeta_EditAnywhere | NkMeta_Serialize,         \
          nullptr, nullptr, nullptr, 0.f, 1.f, nullptr, nullptr, nullptr, 0, nullptr },
    
    // Déclare un champ avec métadonnées personnalisées
    #define NK_FIELD_META(FieldName, MetaFlags, Category, Tooltip, Min, Max)  \
        { #FieldName,                                                          \
          ::nkentseu::ecs::reflect::NkFieldType::Unknown,                     \
          static_cast<uint32>(offsetof(_T, FieldName)),                       \
          static_cast<uint32>(sizeof(std::declval<_T>().FieldName)),          \
          1, MetaFlags, Category, Tooltip, nullptr, Min, Max, nullptr, nullptr, nullptr, 0, nullptr },
    
    // Déclare un tableau
    #define NK_ARRAY(FieldName, Count)                                        \
        { #FieldName,                                                          \
          ::nkentseu::ecs::reflect::NkFieldType::Unknown,                     \
          static_cast<uint32>(offsetof(_T, FieldName)),                       \
          static_cast<uint32>(sizeof(std::declval<_T>().FieldName[0])),       \
          Count, NkMeta_Visible | NkMeta_EditFixedSize | NkMeta_Serialize,    \
          nullptr, nullptr, nullptr, 0.f, 1.f, nullptr, nullptr, nullptr, 0, nullptr },
    
    // Fin de la déclaration et enregistrement
    #define NK_REFLECT_END()                                                   \
            };                                                                 \
            ::nkentseu::ecs::reflect::NkTypeInfo info{};                       \
            info.componentId = ::nkentseu::ecs::NkIdOf<_T>();                 \
            info.name        = #Type;                                          \
            info.size        = static_cast<uint32>(sizeof(_T));               \
            info.align       = alignof(_T);                                    \
            info.fields      = _fields;                                        \
            info.fieldCount  = static_cast<uint32>(                            \
                sizeof(_fields) / sizeof(_fields[0]));                        \
            ::nkentseu::ecs::reflect::NkReflectRegistry::Global().Register(info); \
        }                                                                      \
    } _sNkReflectReg_##Type; }
    
    // Macro pour les propriétés style Unreal/Unity
    #define NK_PROPERTY(FieldName, ...)                                       \
        NK_FIELD_META(FieldName, NK_META_PACK_FLAGS(__VA_ARGS__),             \
                     NK_META_EXTRACT_CATEGORY(__VA_ARGS__),                   \
                     NK_META_EXTRACT_TOOLTIP(__VA_ARGS__),                    \
                     NK_META_EXTRACT_MIN(__VA_ARGS__),                        \
                     NK_META_EXTRACT_MAX(__VA_ARGS__))
    
    // Helpers pour extraire les arguments nommés (simplifiés)
    #define NK_META_EXTRACT_CATEGORY(...) NK_META_FIND_ARG(Category, __VA_ARGS__, nullptr)
    #define NK_META_EXTRACT_TOOLTIP(...)  NK_META_FIND_ARG(Tooltip, __VA_ARGS__, nullptr)
    #define NK_META_EXTRACT_MIN(...)      NK_META_FIND_ARG(Min, __VA_ARGS__, 0.f)
    #define NK_META_EXTRACT_MAX(...)      NK_META_FIND_ARG(Max, __VA_ARGS__, 1.f)
    
    // Pack des flags depuis les arguments
    #define NK_META_PACK_FLAGS(...)                                           \
        (NK_META_FLAG_IF(EditAnywhere, __VA_ARGS__) |                        \
         NK_META_FLAG_IF(BlueprintReadWrite, __VA_ARGS__) |                  \
         NK_META_FLAG_IF(Replicated, __VA_ARGS__) |                          \
         NK_META_FLAG_IF(Range, __VA_ARGS__) | 0)
    
    // Helper pour conditionnellement ajouter un flag
    #define NK_META_FLAG_IF(Flag, ...)                                       \
        (NK_META_HAS_ARG(Flag, __VA_ARGS__) ? static_cast<uint64>(NkMeta_##Flag) : 0ULL)
    
    // Vérifie si un argument nommé est présent (simplifié)
    #define NK_META_HAS_ARG(Arg, ...)                                        \
        NK_META_CHECK_HELPER(NK_META_CAT(NK_META_ARG_, Arg), __VA_ARGS__, 0, 0)
    
    #define NK_META_CHECK_HELPER(x, y, z, ...) z
    #define NK_META_CAT(a, b) NK_META_CAT_IMPL(a, b)
    #define NK_META_CAT_IMPL(a, b) a##_##b
    #define NK_META_ARG_EditAnywhere 1
    #define NK_META_ARG_BlueprintReadWrite 1
    #define NK_META_ARG_Replicated 1
    #define NK_META_ARG_Range 1
    #define NK_META_ARG_Category 1
    #define NK_META_ARG_Tooltip 1
    #define NK_META_ARG_Min 1
    #define NK_META_ARG_Max 1
    
    // Trouve la valeur d'un argument nommé (simplifié - retourne la valeur par défaut)
    #define NK_META_FIND_ARG(Arg, Default, ...) Default

} // namespace reflect
} // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKREFLECT.H
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration simple d'un composant réflexif
// -----------------------------------------------------------------------------
struct PlayerStats {
    float32 health = 100.f;
    float32 mana = 50.f;
    int32   level = 1;
};

NK_COMPONENT(PlayerStats)

NK_REFLECT_BEGIN(PlayerStats)
    NK_FIELD_META(health, 
                  NkMeta_Range | NkMeta_EditAnywhere | NkMeta_Serialize,
                  "Stats", "Points de vie du joueur", 0.f, 1000.f)
    NK_FIELD_META(mana,
                  NkMeta_Range | NkMeta_EditAnywhere | NkMeta_Serialize,
                  "Stats", "Points de mana du joueur", 0.f, 500.f)
    NK_FIELD_META(level,
                  NkMeta_EditDefaultsOnly | NkMeta_ReadOnly,
                  "Stats", "Niveau du personnage (lecture seule)", 1, 100)
NK_REFLECT_END()

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation du registre pour introspection
// -----------------------------------------------------------------------------
void Exemple_Introspection() {
    using namespace nkentseu::ecs::reflect;
    
    const NkTypeInfo* info = NkReflectRegistry::Global().Get(NkIdOf<PlayerStats>());
    if (info) {
        printf("Type: %s, Size: %u bytes, Fields: %u\n",
               info->name, info->size, info->fieldCount);
        
        for (uint32 i = 0; i < info->fieldCount; ++i) {
            const NkFieldInfo& field = info->fields[i];
            printf("  - %s: offset=%u, size=%u, flags=0x%llx\n",
                   field.name, field.offset, field.size, field.metaFlags);
            
            if (field.HasFlag(NkMeta_Range)) {
                printf("    Range: [%.1f, %.1f]\n", field.minValue, field.maxValue);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Accès aux métadonnées pour générer une UI d'éditeur
// -----------------------------------------------------------------------------
void Exemple_EditeurUI() {
    using namespace nkentseu::ecs::reflect;
    
    const NkTypeInfo* info = NkReflectRegistry::Global().GetByName("PlayerStats");
    if (!info) return;
    
    for (uint32 i = 0; i < info->fieldCount; ++i) {
        const NkFieldInfo& field = info->fields[i];
        
        // Ignorer les champs cachés
        if (field.HasFlag(NkMeta_HideInEditor)) {
            continue;
        }
        
        // Afficher la catégorie
        if (field.category && std::strcmp(field.category, "Stats") == 0) {
            printf("=== %s ===\n", field.category);
        }
        
        // Afficher le champ avec tooltip
        printf("%s", field.displayName ? field.displayName : field.name);
        if (field.tooltip) {
            printf(" [? %s]", field.tooltip);
        }
        printf("\n");
        
        // Afficher un slider si c'est un range
        if (field.HasFlag(NkMeta_Range)) {
            printf("  Slider: %.1f -> %.1f\n", field.minValue, field.maxValue);
        }
        
        // Afficher comme readonly si nécessaire
        if (field.HasFlag(NkMeta_ReadOnly)) {
            printf("  [Lecture seule]\n");
        }
    }
}
*/