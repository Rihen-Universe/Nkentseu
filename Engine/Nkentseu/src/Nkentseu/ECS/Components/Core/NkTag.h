#pragma once
// -----------------------------------------------------------------------------
// FICHIER: NkECS/Components/Core/NkTag.h
// DESCRIPTION: Composants d'identité, de filtrage, et de métadonnées d'entité.
//   NkTag      — étiquette textuelle (Player, Enemy, Ground, Trigger, etc.)
//   NkName     — nom lisible de l'entité (affiché dans l'éditeur)
//   NkLayer    — couche de rendu / collision (bitfield)
//   NkActive   — entité active/inactive (remplace Destroy pour masquer)
//   NkStatic   — tag : entité statique (ne bouge jamais → optimisation)
//   NkPersist  — tag : survit au changement de scène
// -----------------------------------------------------------------------------

#include "../../NkECSDefines.h"
#include "../../Core/NkTypeRegistry.h"
#include <cstring>

namespace nkentseu { namespace ecs {

// =============================================================================
// NkName — nom lisible de l'entité (affiché dans l'éditeur)
// =============================================================================

struct NkName {
    static constexpr uint32 kMaxLen = 128u;
    char value[kMaxLen] = "Entity";

    NkName() noexcept = default;
    explicit NkName(const char* name) noexcept {
        std::strncpy(value, name, kMaxLen - 1);
    }

    [[nodiscard]] const char* Get() const noexcept { return value; }
    void Set(const char* name) noexcept { std::strncpy(value, name, kMaxLen - 1); }
    bool operator==(const NkName& o) const noexcept {
        return std::strcmp(value, o.value) == 0;
    }
};
NK_COMPONENT(NkName)

// =============================================================================
// NkTag — étiquette(s) multi-tags via bitfield 64 bits
// Chaque bit correspond à un tag prédéfini ou utilisateur.
// =============================================================================

enum class NkTagBit : uint64 {
    None        = 0,
    Player      = 1ULL << 0,
    Enemy       = 1ULL << 1,
    Ally        = 1ULL << 2,
    Neutral     = 1ULL << 3,
    Ground      = 1ULL << 4,
    Wall        = 1ULL << 5,
    Ceiling     = 1ULL << 6,
    Trigger     = 1ULL << 7,
    Sensor      = 1ULL << 8,
    Projectile  = 1ULL << 9,
    Pickup      = 1ULL << 10,
    Interactable= 1ULL << 11,
    Destructible= 1ULL << 12,
    Environment = 1ULL << 13,
    Terrain     = 1ULL << 14,
    Water       = 1ULL << 15,
    UI          = 1ULL << 16,
    Camera      = 1ULL << 17,
    Light       = 1ULL << 18,
    Particle    = 1ULL << 19,
    Audio       = 1ULL << 20,
    // Bits 21-47 : libres pour l'utilisateur
    // Bits 48-63 : système interne
    EditorOnly  = 1ULL << 48,
    Prefab      = 1ULL << 49,
    Internal    = 1ULL << 63,
};

inline NkTagBit operator|(NkTagBit a, NkTagBit b) noexcept {
    return static_cast<NkTagBit>(static_cast<uint64>(a) | static_cast<uint64>(b));
}
inline bool operator&(NkTagBit a, NkTagBit b) noexcept {
    return (static_cast<uint64>(a) & static_cast<uint64>(b)) != 0;
}

struct NkTag {
    uint64 bits = static_cast<uint64>(NkTagBit::None);

    NkTag() noexcept = default;
    explicit NkTag(NkTagBit tag) noexcept : bits(static_cast<uint64>(tag)) {}

    void  Add(NkTagBit tag) noexcept    { bits |= static_cast<uint64>(tag); }
    void  Remove(NkTagBit tag) noexcept { bits &= ~static_cast<uint64>(tag); }
    bool  Has(NkTagBit tag) const noexcept {
        return (bits & static_cast<uint64>(tag)) != 0;
    }
    bool  HasAll(NkTagBit tags) const noexcept {
        const uint64 t = static_cast<uint64>(tags);
        return (bits & t) == t;
    }
    bool  HasAny(NkTagBit tags) const noexcept {
        return (bits & static_cast<uint64>(tags)) != 0;
    }
    bool  HasNone(NkTagBit tags) const noexcept { return !HasAny(tags); }
    void  Clear() noexcept { bits = 0; }
};
NK_COMPONENT(NkTag)

// =============================================================================
// NkLayer — couche de rendu / collision (32 couches)
// Compatible avec un système de masque de collision (layerA vs layerB).
// =============================================================================

struct NkLayer {
    uint32 layer = 0;           // couche principale (0-31)
    uint32 mask  = 0xFFFFFFFFu; // masque de collision (quelles couches cet objet détecte)

    NkLayer() noexcept = default;
    explicit NkLayer(uint32 layer, uint32 mask = 0xFFFFFFFFu) noexcept
        : layer(layer), mask(mask) {}

    [[nodiscard]] bool CanCollideWith(const NkLayer& other) const noexcept {
        return (mask & (1u << other.layer)) != 0
            && (other.mask & (1u << layer)) != 0;
    }

    [[nodiscard]] uint32 LayerBit() const noexcept { return 1u << layer; }

    // Couches prédéfinies standard
    static constexpr uint32 kDefault     = 0;
    static constexpr uint32 kUI          = 5;
    static constexpr uint32 kIgnoreRaycast = 2;
    static constexpr uint32 kWater       = 4;
    static constexpr uint32 kPostProcessing = 8;
};
NK_COMPONENT(NkLayer)

// =============================================================================
// NkActive — contrôle l'activité de l'entité sans la détruire
// Une entité inactive est ignorée par les systèmes (via query .Without<NkInactive>)
// =============================================================================

struct NkInactive {}; // tag component — présent = inactif, absent = actif
NK_COMPONENT(NkInactive)

// =============================================================================
// NkStatic — entité statique (ne bouge jamais)
// Le TransformSystem la skip, les systèmes physiques peuvent l'optimiser.
// =============================================================================

struct NkStatic {}; // tag component
NK_COMPONENT(NkStatic)

// =============================================================================
// NkPersist — survive aux changements de scène (ex: GameManager, AudioManager)
// =============================================================================

struct NkPersist {}; // tag component
NK_COMPONENT(NkPersist)

// =============================================================================
// NkHideInEditor — masque l'entité dans le panneau de scène de l'éditeur
// =============================================================================

struct NkHideInEditor {}; // tag component
NK_COMPONENT(NkHideInEditor)

// =============================================================================
// NkEntityMeta — métadonnées complètes d'une entité (pour l'éditeur)
// =============================================================================

struct NkEntityMeta {
    uint32 uid = 0;              // ID persistant (survit aux rechargements)
    char   prefabPath[256] = {}; // chemin du prefab source si instancié
    bool   isPrefabInstance = false;
    bool   isPrefabRoot     = false;
    uint32 createdFrame     = 0;  // frame de création (debug)

    NkEntityMeta() noexcept = default;
};
NK_COMPONENT(NkEntityMeta)

}} // namespace nkentseu::ecs
