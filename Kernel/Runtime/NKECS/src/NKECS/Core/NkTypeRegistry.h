// =============================================================================
// Core/NkTypeRegistry.h
// Registre central de tous les types de components.
//
// Design :
//  - Chaque type T reçoit un NkComponentId unique via NkComponentTraits<T>::Id()
//    lors du premier appel (lazy init, thread-safe via atomic).
//  - NkTypeRegistry stocke les métadonnées : taille, alignement, ctor/dtor/move.
//  - Zéro macro obligatoire — l'enregistrement est automatique via template.
//  - NK_COMPONENT(T) est optionnel : il enrichit les métadonnées (nom, champs).
// =============================================================================

#pragma once

// -----------------------------------------------------------------------------
// En-têtes nécessaires pour le registre de types
// -----------------------------------------------------------------------------
#include "NKECS/NkECSDefines.h"     // Constantes (kMaxComponentTypes) et macros utilitaires
#include <atomic>                    // Pour les opérations thread‑safe sur les IDs
#include <cstring>                   // Pour std::memcpy, std::memcmp
#include <typeinfo>                  // Pour obtenir le nom du type à l'exécution
#include <new>                       // Pour le placement new (construction dans un buffer)

namespace nkentseu {
    namespace ecs {

        // =========================================================================
        // ComponentMeta
        // =========================================================================
        // Structure qui décrit tout ce que l'ECS a besoin de savoir sur un type
        // de composant sans avoir à connaître T à la compilation.
        // Elle contient :
        //   - un identifiant unique,
        //   - des pointeurs sur fonctions pour les opérations de cycle de vie,
        //   - la taille, l'alignement, et un hash du nom du type.
        // Cette indirection permet de manipuler les données de composants de
        // manière générique (par exemple dans les chunks de stockage).
        struct ComponentMeta {
            // Identifiant unique attribué par le registre.
            NkComponentId id        = kInvalidComponentId;

            // Nom lisible du type (par défaut le nom décoré retourné par typeid).
            // Peut être écrasé via SetName ou la macro NK_COMPONENT.
            const char*   name      = "<unknown>";

            // Hash 64 bits du nom du type (FNV‑1a) – utilisé pour des comparaisons
            // rapides et la compatibilité entre modules.
            uint64        typeHash  = 0;

            // Taille en octets du type (retournée par sizeof(T)).
            uint32        size      = 0;

            // Alignement requis en octets (retourné par alignof(T)).
            uint32        align     = 0;

            // Vrai si le type est une classe vide (std::is_empty_v<T>).
            // Les tag components ne consomment pas de mémoire dans les chunks.
            bool          isZeroSize = false;

            // ---------------------------------------------------------------------
            // Pointeurs sur fonctions de gestion du cycle de vie.
            // Ces pointeurs sont typés `void` pour rester indépendants de T.
            // Ils sont initialisés avec les fonctions du namespace `detail`
            // spécialisées pour chaque type.
            // ---------------------------------------------------------------------

            // Construit `count` instances de T par défaut à l'adresse `ptr`.
            void (*defaultConstruct)(void* ptr, uint32 count) = nullptr;

            // Détruit `count` instances de T situées à l'adresse `ptr`.
            void (*destruct)        (void* ptr, uint32 count) = nullptr;

            // Déplace `count` instances de T de `src` vers `dst`.
            // Utilisé lors du déplacement d'une entité vers un autre chunk.
            void (*moveConstruct)   (void* dst, void* src, uint32 count) = nullptr;

            // Copie `count` instances de T de `src` vers `dst`.
            // Utilisé pour la duplication d'entités ou de composants.
            void (*copyConstruct)   (void* dst, const void* src, uint32 count) = nullptr;

            // Échange deux instances de T (utile pour le tri, la défragmentation).
            void (*swapAt)          (void* a, void* b)        = nullptr;
        };

        // =========================================================================
        // Fonctions internes de gestion du cycle de vie (namespace detail)
        // =========================================================================
        // Ces templates génèrent les fonctions de construction / destruction /
        // copie / déplacement pour un type T donné. Elles utilisent `if constexpr`
        // pour optimiser les cas triviaux (types vides, trivialement copiables, etc.).
        namespace detail {

            // Construction par défaut de `count` objets T à l'adresse `ptr`.
            // Si T est vide, aucune opération n'est effectuée (gain de performance).
            template<typename T>
            void DefaultConstruct(void* ptr, uint32 count) noexcept {
                if constexpr (!std::is_empty_v<T>) {
                    T* p = static_cast<T*>(ptr);
                    for (uint32 i = 0; i < count; ++i) {
                        new (p + i) T();
                    }
                }
            }

            // Destruction de `count` objets T à l'adresse `ptr`.
            // Ignorée si T est vide ou trivialement destructible.
            template<typename T>
            void Destruct(void* ptr, uint32 count) noexcept {
                if constexpr (!std::is_empty_v<T> && !std::is_trivially_destructible_v<T>) {
                    T* p = static_cast<T*>(ptr);
                    for (uint32 i = 0; i < count; ++i) {
                        p[i].~T();
                    }
                }
            }

            // Construction par déplacement de `count` objets T de `src` vers `dst`.
            // Utilise std::memcpy si le type est trivialement déplaçable.
            template<typename T>
            void MoveConstruct(void* dst, void* src, uint32 count) noexcept {
                if constexpr (!std::is_empty_v<T>) {
                    T* d = static_cast<T*>(dst);
                    T* s = static_cast<T*>(src);
                    if constexpr (std::is_trivially_move_constructible_v<T>) {
                        std::memcpy(d, s, count * sizeof(T));
                    } else {
                        for (uint32 i = 0; i < count; ++i) {
                            new (d + i) T(std::move(s[i]));
                        }
                    }
                }
            }

            // Construction par copie de `count` objets T de `src` vers `dst`.
            // Utilise std::memcpy si le type est trivialement copiable.
            template<typename T>
            void CopyConstruct(void* dst, const void* src, uint32 count) noexcept {
                if constexpr (!std::is_empty_v<T>) {
                    T*       d = static_cast<T*>(dst);
                    const T* s = static_cast<const T*>(src);
                    if constexpr (std::is_trivially_copy_constructible_v<T>) {
                        std::memcpy(d, s, count * sizeof(T));
                    } else {
                        for (uint32 i = 0; i < count; ++i) {
                            new (d + i) T(s[i]);
                        }
                    }
                }
            }

            // Échange deux instances de T en utilisant ADL (Argument Dependent Lookup)
            // sur std::swap ou une surcharge personnalisée.
            template<typename T>
            void SwapAt(void* a, void* b) noexcept {
                if constexpr (!std::is_empty_v<T>) {
                    using std::swap;
                    swap(*static_cast<T*>(a), *static_cast<T*>(b));
                }
            }

            // ---------------------------------------------------------------------
            // Compteur global pour les IDs de composants.
            // Démarré à 1 car la valeur 0 est réservée pour "invalide".
            // ---------------------------------------------------------------------
            inline std::atomic<uint32> gNextComponentId{1u};

            // Stockage thread‑safe de l'ID pour un type T.
            // Chaque type a sa propre instance statique de cette structure.
            template<typename T>
            struct ComponentIdStorage {
                inline static std::atomic<NkComponentId> value{kInvalidComponentId};
            };

        } // namespace detail

        // =========================================================================
        // NkTypeRegistry – Singleton global gérant l'enregistrement des types
        // =========================================================================
        // Cette classe est responsable de l'attribution des identifiants uniques
        // (NkComponentId) pour chaque type de composant.
        // Elle utilise un mécanisme d'enregistrement paresseux (lazy) :
        // le premier appel à `IdOf<T>` ou `Register<T>` déclenche l'assignation.
        // Les métadonnées associées sont stockées dans un tableau indexé par l'ID.
        class NkTypeRegistry {
            public:
                // Accès au singleton (thread‑safe en C++11 et ultérieur).
                static NkTypeRegistry& Global() noexcept {
                    static NkTypeRegistry sInstance;
                    return sInstance;
                }

                // ---------------------------------------------------------------------
                // Enregistre le type T si ce n'est pas déjà fait et retourne son ID.
                // Cette méthode est thread‑safe grâce à un double‑checked locking
                // et un spinlock interne.
                // ---------------------------------------------------------------------
                template<typename T>
                NkComponentId Register() noexcept {
                    // Supprime les qualificateurs const et référence.
                    using TRaw = std::remove_const_t<std::remove_reference_t<T>>;

                    // Lecture atomique sans lock – la plupart des appels trouveront l'ID déjà présent.
                    NkComponentId id = detail::ComponentIdStorage<TRaw>::value.load(std::memory_order_acquire);
                    if (id != kInvalidComponentId) {
                        return id;
                    }

                    // Double‑checked locking : on verrouille et on relit au cas où un autre
                    // thread aurait enregistré le type entre temps.
                    std::unique_lock lock(mMutex);
                    id = detail::ComponentIdStorage<TRaw>::value.load(std::memory_order_relaxed);
                    if (id != kInvalidComponentId) {
                        return id;
                    }

                    // Attribution d'un nouvel ID depuis le compteur global.
                    id = detail::gNextComponentId.fetch_add(1, std::memory_order_relaxed);
                    // Vérification que l'on ne dépasse pas la capacité maximale définie.
                    NKECS_ASSERT(id < kMaxComponentTypes);

                    // Construction des métadonnées pour ce type.
                    ComponentMeta meta{};
                    meta.id               = id;
                    meta.name             = typeid(TRaw).name();        // Nom démanglé à l'exécution
                    meta.typeHash         = detail::FNV1a(typeid(TRaw).name());
                    meta.size             = static_cast<uint32>(sizeof(TRaw));
                    meta.align            = static_cast<uint32>(alignof(TRaw));
                    meta.isZeroSize       = std::is_empty_v<TRaw>;
                    meta.defaultConstruct = detail::DefaultConstruct<TRaw>;
                    meta.destruct         = detail::Destruct<TRaw>;
                    meta.moveConstruct    = detail::MoveConstruct<TRaw>;
                    meta.copyConstruct    = detail::CopyConstruct<TRaw>;
                    meta.swapAt           = detail::SwapAt<TRaw>;

                    // Stockage dans le tableau (si l'ID est dans les limites).
                    if (id < kMaxComponentTypes) {
                        mMetas[id] = meta;
                    }

                    // Sauvegarde de l'ID dans le stockage atomique du type T.
                    detail::ComponentIdStorage<TRaw>::value.store(id, std::memory_order_release);

                    // Mise à jour du nombre de types enregistrés.
                    mNumRegistered = id + 1;
                    return id;
                }

                // ---------------------------------------------------------------------
                // Remplace le nom du composant par une chaîne plus lisible.
                // Typiquement appelé par la macro NK_COMPONENT.
                // ---------------------------------------------------------------------
                void SetName(NkComponentId id, const char* name) noexcept {
                    if (id < kMaxComponentTypes) {
                        mMetas[id].name = name;
                    }
                }

                // ---------------------------------------------------------------------
                // Récupère les métadonnées d'un composant à partir de son ID.
                // Retourne nullptr si l'ID est invalide ou non enregistré.
                // ---------------------------------------------------------------------
                [[nodiscard]] const ComponentMeta* Get(NkComponentId id) const noexcept {
                    if (id >= mNumRegistered) {
                        return nullptr;
                    }
                    return &mMetas[id];
                }

                // ---------------------------------------------------------------------
                // Retourne le nombre total de types enregistrés jusqu'à présent.
                // ---------------------------------------------------------------------
                [[nodiscard]] uint32 Count() const noexcept {
                    return mNumRegistered;
                }

                // ---------------------------------------------------------------------
                // Helper : retourne l'ID du type T (enregistre automatiquement si besoin).
                // ---------------------------------------------------------------------
                template<typename T>
                [[nodiscard]] NkComponentId IdOf() noexcept {
                    return Register<T>();
                }

                // ---------------------------------------------------------------------
                // Version const – ne modifie pas l'état du registre.
                // Si le type n'a jamais été enregistré, retourne kInvalidComponentId.
                // ---------------------------------------------------------------------
                template<typename T>
                [[nodiscard]] NkComponentId IdOfConst() const noexcept {
                    using TRaw = std::remove_const_t<std::remove_reference_t<T>>;
                    const NkComponentId id = detail::ComponentIdStorage<TRaw>::value.load(std::memory_order_acquire);
                    return id;
                }

            private:
                // Constructeur par défaut privé (singleton).
                NkTypeRegistry() noexcept = default;

                // Tableau contenant les métadonnées de tous les composants enregistrés.
                // Indexé par NkComponentId.
                ComponentMeta mMetas[kMaxComponentTypes] = {};

                // Nombre de types actuellement enregistrés (utilisé pour des bornes rapides).
                uint32        mNumRegistered             = 0;

                // ---------------------------------------------------------------------
                // SpinLock minimaliste
                // ---------------------------------------------------------------------
                // Évite la dépendance à <mutex> et son overhead.
                // L'enregistrement est rare (une fois par type au démarrage), donc
                // un spinlock est acceptable.
                struct SpinLock {
                    std::atomic_flag flag = ATOMIC_FLAG_INIT;

                    // Boucle jusqu'à l'acquisition du verrou.
                    void lock() noexcept {
                        while (flag.test_and_set(std::memory_order_acquire)) {
                            // attente active
                        }
                    }

                    // Libère le verrou.
                    void unlock() noexcept {
                        flag.clear(std::memory_order_release);
                    }
                };

                // RAII pour le SpinLock (équivalent de std::unique_lock).
                struct UniqueLock {
                    SpinLock& sl;

                    UniqueLock(SpinLock& s) noexcept : sl(s) {
                        sl.lock();
                    }

                    ~UniqueLock() noexcept {
                        sl.unlock();
                    }
                };

                SpinLock mMutex;
        };

        // =========================================================================
        // Helpers globaux – accès simplifié depuis n'importe où dans l'ECS
        // =========================================================================

        // Retourne l'identifiant de composant pour le type T.
        template<typename T>
        [[nodiscard]] NKECS_INLINE NkComponentId NkIdOf() noexcept {
            return NkTypeRegistry::Global().IdOf<T>();
        }

        // Retourne un pointeur vers les métadonnées associées au type T.
        template<typename T>
        [[nodiscard]] NKECS_INLINE const ComponentMeta* NkMetaOf() noexcept {
            const NkComponentId id = NkIdOf<T>();
            return NkTypeRegistry::Global().Get(id);
        }

        // =========================================================================
        // NkComponentMask – Représentation compacte d'un ensemble de ComponentId
        // =========================================================================
        // Utilisé pour définir les archétypes (signature d'une entité) et les
        // requêtes (Query). Chaque bit correspond à un ComponentId.
        // La taille est fixée à kMaxComponentTypes bits (256 bits → 4 mots de 64).
        struct NkComponentMask {
            // Nombre de mots de 64 bits nécessaires pour couvrir tous les composants.
            static constexpr uint32 kWords = kMaxComponentTypes / 64u;

            // Les bits sont stockés dans un tableau de uint64.
            uint64 words[kWords] = {};

            // Active le bit correspondant à l'ID `id`.
            void Set(NkComponentId id) noexcept {
                if (id < kMaxComponentTypes) {
                    words[id / 64u] |= (1ULL << (id % 64u));
                }
            }

            // Désactive le bit correspondant à l'ID `id`.
            void Clear(NkComponentId id) noexcept {
                if (id < kMaxComponentTypes) {
                    words[id / 64u] &= ~(1ULL << (id % 64u));
                }
            }

            // Teste si le bit correspondant à `id` est actif.
            [[nodiscard]] bool Has(NkComponentId id) const noexcept {
                if (id >= kMaxComponentTypes) {
                    return false;
                }
                return (words[id / 64u] >> (id % 64u)) & 1ULL;
            }

            // Vérifie si ce masque contient **tous** les bits de `other`.
            // (i.e. `this` est un sur‑ensemble de `other`).
            [[nodiscard]] bool ContainsAll(const NkComponentMask& other) const noexcept {
                for (uint32 i = 0; i < kWords; ++i) {
                    if ((words[i] & other.words[i]) != other.words[i]) {
                        return false;
                    }
                }
                return true;
            }

            // Vérifie si ce masque et `other` partagent au moins un bit actif.
            [[nodiscard]] bool HasAny(const NkComponentMask& other) const noexcept {
                for (uint32 i = 0; i < kWords; ++i) {
                    if (words[i] & other.words[i]) {
                        return true;
                    }
                }
                return false;
            }

            // Vrai si aucun bit n'est actif.
            [[nodiscard]] bool IsEmpty() const noexcept {
                for (uint32 i = 0; i < kWords; ++i) {
                    if (words[i]) {
                        return false;
                    }
                }
                return true;
            }

            // Opérateurs de comparaison (utilisés pour l'égalité des archétypes).
            bool operator==(const NkComponentMask& o) const noexcept {
                return std::memcmp(words, o.words, sizeof(words)) == 0;
            }

            bool operator!=(const NkComponentMask& o) const noexcept {
                return !(*this == o);
            }

            // Calcule un hash 64 bits du masque (via FNV‑1a).
            // Utilisé pour stocker les archétypes dans une table de hachage.
            [[nodiscard]] uint64 Hash() const noexcept {
                return detail::FNV1aBytes(
                    reinterpret_cast<const uint8*>(words),
                    sizeof(words));
            }

            // Compte le nombre de bits à 1 (nombre de composants dans la signature).
            [[nodiscard]] uint32 Count() const noexcept {
                uint32 n = 0;
                for (uint32 i = 0; i < kWords; ++i) {
                    n += static_cast<uint32>(__builtin_popcountll(words[i]));
                }
                return n;
            }

            // Parcourt tous les bits actifs et appelle la fonction `fn` avec l'ID.
            // Utile pour itérer sur les composants d'un archétype.
            template<typename Fn>
            void ForEach(Fn&& fn) const noexcept {
                for (uint32 w = 0; w < kWords; ++w) {
                    uint64 word = words[w];
                    while (word) {
                        // Position du bit le plus bas (count trailing zeros).
                        const uint32 bit = static_cast<uint32>(__builtin_ctzll(word));
                        // Conversion en ID global.
                        fn(static_cast<NkComponentId>(w * 64u + bit));
                        // Efface le bit le plus bas pour passer au suivant.
                        word &= word - 1ULL;
                    }
                }
            }
        };

        // =========================================================================
        // Macro NK_COMPONENT (optionnelle) – Enregistre le type et lui donne un nom
        // =========================================================================
        // Utilisation : juste après la définition de la struct, écrire NK_COMPONENT(MaStruct).
        // Elle crée un objet statique dont le constructeur appelle Register<Type>()
        // et SetName() avec le nom littéral de la macro.
        // Cela permet d'avoir des noms lisibles dans l'éditeur sans avoir à écrire
        // du code manuellement.
        #define NK_COMPONENT(Type)                                                    \
            namespace {                                                               \
                /* Objet dont la construction force l'enregistrement du type. */      \
                struct _NkAutoReg_##Type {                                            \
                    _NkAutoReg_##Type() noexcept {                                    \
                        ::nkentseu::ecs::NkTypeRegistry::Global().Register<Type>();   \
                        ::nkentseu::ecs::NkTypeRegistry::Global().SetName(            \
                            ::nkentseu::ecs::NkIdOf<Type>(), #Type);                  \
                    }                                                                 \
                };                                                                    \
                /* Instance statique unique par unité de compilation. */              \
                _NkAutoReg_##Type _sNkAutoReg_##Type;                                 \
            }

    } // namespace ecs
} // namespace nkentseu

// ============================================================
// Copyright © 2025-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTYPREGISTRY.H
// =============================================================================
//
// Le registre de types est au cœur du système de composants. Il permet
// d'enregistrer dynamiquement des types et d'obtenir leurs métadonnées.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Enregistrement manuel d'un composant (sans macro)
// -----------------------------------------------------------------------------
/*
    #include "NKECS/Core/NkTypeRegistry.h"

    struct Position {
        float x, y, z;
    };

    struct Velocity {
        float vx, vy, vz;
    };

    void ManualRegistration() {
        using namespace nkentseu::ecs;

        // Accès au singleton.
        NkTypeRegistry& reg = NkTypeRegistry::Global();

        // Enregistrement explicite (appelé une seule fois).
        NkComponentId posId = reg.Register<Position>();
        NkComponentId velId = reg.Register<Velocity>();

        // Attribution d'un nom lisible.
        reg.SetName(posId, "Position");
        reg.SetName(velId, "Velocity");

        // Récupération des métadonnées.
        const ComponentMeta* metaPos = reg.Get(posId);
        if (metaPos) {
            printf("Component '%s' : size=%u, align=%u\n", metaPos->name, metaPos->size, metaPos->align);
        }

        // Utilisation des helpers globaux.
        NkComponentId id1 = NkIdOf<Position>();  // enregistre si nécessaire
        NkComponentId id2 = NkIdOf<Velocity>();

        const ComponentMeta* metaVel = NkMetaOf<Velocity>();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation de la macro NK_COMPONENT
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Core/NkTypeRegistry.h"

    // Définition du composant.
    struct Health {
        float current;
        float max;
    };
    NK_COMPONENT(Health);  // Enregistrement automatique avec nom "Health"

    struct Armor {
        float value;
    };
    NK_COMPONENT(Armor);

    void MacroUsage() {
        using namespace nkentseu::ecs;

        // Les composants sont déjà enregistrés grâce aux objets statiques.
        NkComponentId healthId = NkIdOf<Health>();   // retourne l'ID sans appel à Register()
        NkComponentId armorId  = NkIdOf<Armor>();

        // Vérification des métadonnées.
        const ComponentMeta* meta = NkMetaOf<Health>();
        assert(meta != nullptr);
        assert(std::string(meta->name) == "Health");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Manipulation de NkComponentMask
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Core/NkTypeRegistry.h"

    void ExampleComponentMask() {
        using namespace nkentseu::ecs;

        // Supposons que Position et Velocity sont enregistrés.
        NkComponentId posId = NkIdOf<Position>();
        NkComponentId velId = NkIdOf<Velocity>();

        NkComponentMask mask;
        mask.Set(posId);
        mask.Set(velId);

        // Test d'appartenance.
        assert(mask.Has(posId));
        assert(mask.Has(velId));

        // Itération sur les bits actifs.
        mask.ForEach([](NkComponentId cid) {
            const ComponentMeta* meta = NkTypeRegistry::Global().Get(cid);
            printf("Mask contains: %s\n", meta->name);
        });

        // Comparaison de masques.
        NkComponentMask mask2;
        mask2.Set(posId);
        assert(mask.ContainsAll(mask2));  // mask est un sur-ensemble
        assert(mask.HasAny(mask2));       // ils partagent au moins un bit

        // Hachage pour utilisation dans une unordered_map.
        uint64 hash = mask.Hash();
    }
*/

// =============================================================================