// =============================================================================
// Storage/NkComponentPool.h
// =============================================================================
// Pool de composants pour un type spécifique au sein d'un archétype.
//
// Cette classe gère un tableau contigu d'instances d'un même type de composant.
// Elle est conçue pour être utilisée en interne par `NkArchetype` et n'est
// normalement pas manipulée directement par l'utilisateur final.
//
// Caractéristiques :
//   - Layout mémoire SoA (Structure of Arrays) : toutes les instances du même
//     type sont stockées de manière contiguë pour une excellente localité
//     spatiale et la possibilité d'optimisations SIMD.
//   - Gestion automatique du cycle de vie : construction, destruction,
//     déplacement, copie via les pointeurs de fonction fournis par
//     `ComponentMeta`.
//   - Pas d'allocation individuelle par élément : un seul bloc mémoire aligné.
//   - Aucune fonction virtuelle (ABI stable et performances maximales).
//   - Croissance dynamique avec stratégie 1.5x.
// =============================================================================

#pragma once

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include <cstdlib>   // pour std::free, std::aligned_alloc, _aligned_free, _aligned_malloc
#include <cstring>   // pour std::memcpy

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkComponentPool
        // =====================================================================
        // Classe responsable du stockage et de la gestion de la mémoire pour
        // un type de composant unique. Elle est instanciée par `NkArchetype`.
        class NkComponentPool {
          public:
            // -----------------------------------------------------------------
            // Constructeurs / Destructeur / Sémantique de copie et déplacement
            // -----------------------------------------------------------------

            // Constructeur par défaut (pool vide sans métadonnées).
            NkComponentPool() noexcept = default;

            // Construit une pool associée aux métadonnées `meta`.
            // Les métadonnées définissent la taille, l'alignement et les
            // fonctions de cycle de vie du composant.
            explicit NkComponentPool(const ComponentMeta* meta) noexcept
                : mMeta(meta) {
                NKECS_ASSERT(meta != nullptr);
            }

            // Destructeur : détruit tous les éléments et libère le bloc mémoire.
            ~NkComponentPool() noexcept {
                Clear();
                if (mData != nullptr) {
#if defined(_MSC_VER)
                    _aligned_free(mData);
#else
                    std::free(mData);
#endif
                    mData = nullptr;
                }
                mCapacity = 0;
            }

            // Copie interdite (chaque pool est unique au sein de son archétype).
            NkComponentPool(const NkComponentPool&) = delete;
            NkComponentPool& operator=(const NkComponentPool&) = delete;

            // Déplacement autorisé (pour stockage dans des conteneurs).
            NkComponentPool(NkComponentPool&& o) noexcept
                : mMeta(o.mMeta)
                , mData(o.mData)
                , mSize(o.mSize)
                , mCapacity(o.mCapacity) {
                // L'objet source devient vide (il ne possède plus les données).
                o.mData     = nullptr;
                o.mSize     = 0;
                o.mCapacity = 0;
            }

            // Opérateur d'affectation par déplacement (idiome copy-and-swap simplifié).
            NkComponentPool& operator=(NkComponentPool&& o) noexcept {
                if (this != &o) {
                    // Destruction explicite de l'état actuel.
                    this->~NkComponentPool();
                    // Reconstruction par déplacement.
                    new (this) NkComponentPool(std::move(o));
                }
                return *this;
            }

            // -----------------------------------------------------------------
            // Accesseurs (lecture seule)
            // -----------------------------------------------------------------
            [[nodiscard]] uint32 Size() const noexcept {
                return mSize;
            }

            [[nodiscard]] uint32 Capacity() const noexcept {
                return mCapacity;
            }

            [[nodiscard]] bool Empty() const noexcept {
                return mSize == 0;
            }

            [[nodiscard]] const ComponentMeta* Meta() const noexcept {
                return mMeta;
            }

            // Retourne un pointeur générique (void*) vers l'élément à l'index `index`.
            // Si le composant est de taille nulle (tag), retourne nullptr.
            [[nodiscard]] void* At(uint32 index) const noexcept {
                NKECS_ASSERT(index < mSize);
                if (mMeta->isZeroSize) {
                    return nullptr;
                }
                // Calcul de l'adresse : base + index * taille_de_l'élément.
                return static_cast<uint8*>(mData) + static_cast<usize>(index) * mMeta->size;
            }

            // Version typée de `At()`.
            // Précondition : T doit correspondre au type de la pool.
            template<typename T>
            [[nodiscard]] T* AtTyped(uint32 index) const noexcept {
                return static_cast<T*>(At(index));
            }

            // Retourne un pointeur vers le tableau brut des éléments.
            // Utilisé pour des itérations ultra-rapides ou des traitements SIMD.
            template<typename T>
            [[nodiscard]] T* Data() const noexcept {
                return static_cast<T*>(mData);
            }

            // -----------------------------------------------------------------
            // Mutations (ajout, suppression, migration)
            // -----------------------------------------------------------------

            // Ajoute un nouvel élément construit par défaut à la fin de la pool.
            // Retourne l'index du nouvel élément (identique à l'ancienne taille).
            uint32 PushDefault() noexcept {
                // Garantit la capacité avant d'écrire.
                Reserve(mSize + 1);
                if (!mMeta->isZeroSize && mMeta->defaultConstruct != nullptr) {
                    mMeta->defaultConstruct(At(mSize), 1);
                }
                // Incrémente la taille logique.
                return mSize++;
            }

            // Ajoute une copie de l'élément pointé par `src` à la fin de la pool.
            uint32 PushCopy(const void* src) noexcept {
                Reserve(mSize + 1);
                if (!mMeta->isZeroSize && src != nullptr && mMeta->copyConstruct != nullptr) {
                    mMeta->copyConstruct(At(mSize), src, 1);
                } else if (!mMeta->isZeroSize && mMeta->defaultConstruct != nullptr) {
                    // Fallback si src est null ou copyConstruct absent.
                    mMeta->defaultConstruct(At(mSize), 1);
                }
                return mSize++;
            }

            // Ajoute un élément déplacé depuis `src` à la fin de la pool.
            uint32 PushMove(void* src) noexcept {
                Reserve(mSize + 1);
                if (!mMeta->isZeroSize && src != nullptr && mMeta->moveConstruct != nullptr) {
                    mMeta->moveConstruct(At(mSize), src, 1);
                } else if (!mMeta->isZeroSize && mMeta->defaultConstruct != nullptr) {
                    mMeta->defaultConstruct(At(mSize), 1);
                }
                return mSize++;
            }

            // Constante spéciale retournée par `SwapRemove` lorsque l'élément
            // supprimé était déjà le dernier (aucun swap n'a eu lieu).
            static constexpr uint32 kSwapRemoveNoSwap = 0xFFFFFFFFu;

            // Supprime l'élément à l'index `index` en utilisant la technique du
            // swap-remove (échange avec le dernier élément puis destruction du dernier).
            // Cette opération est en O(1) mais change l'ordre des éléments.
            // Retourne l'index de l'élément qui a été déplacé vers `index`,
            // ou `kSwapRemoveNoSwap` si aucun swap n'a eu lieu.
            uint32 SwapRemove(uint32 index) noexcept {
                NKECS_ASSERT(index < mSize);
                const uint32 last = mSize - 1;

                if (index != last) {
                    // Échange l'élément à supprimer avec le dernier.
                    if (!mMeta->isZeroSize && mMeta->swapAt != nullptr) {
                        mMeta->swapAt(At(index), At(last));
                    }
                }

                // Détruit le dernier élément (qui contient maintenant l'ancien élément
                // à supprimer, ou était déjà celui à supprimer).
                if (!mMeta->isZeroSize && mMeta->destruct != nullptr) {
                    mMeta->destruct(At(last), 1);
                }

                // Décrémente la taille logique.
                --mSize;

                // Retourne l'index de l'élément déplacé, ou la constante spéciale.
                return (index != last) ? last : kSwapRemoveNoSwap;
            }

            // Déplace l'élément situé à l'index `srcIdx` dans la pool `srcPool`
            // vers la fin de cette pool. Utilisé lors de la migration d'une entité
            // entre archétypes.
            // Précondition : les deux pools doivent gérer le même type de composant.
            void MoveFrom(NkComponentPool& srcPool, uint32 srcIdx) noexcept {
                NKECS_ASSERT(srcPool.mMeta == mMeta);
                if (mMeta->isZeroSize) {
                    // Les tags ne consomment pas de mémoire.
                    ++mSize;
                    return;
                }
                Reserve(mSize + 1);
                if (mMeta->moveConstruct != nullptr) {
                    mMeta->moveConstruct(At(mSize), srcPool.At(srcIdx), 1);
                }
                ++mSize;
            }

            // Détruit tous les éléments de la pool et remet la taille logique à zéro.
            // La capacité mémoire reste inchangée (le buffer est conservé).
            void Clear() noexcept {
                if (!mMeta->isZeroSize && mSize > 0 && mMeta->destruct != nullptr && mData != nullptr) {
                    mMeta->destruct(mData, mSize);
                }
                mSize = 0;
            }

            // Garantit que la pool peut contenir au moins `minCapacity` éléments.
            // Si nécessaire, un nouveau bloc mémoire aligné est alloué et les
            // éléments existants y sont déplacés (move) ou copiés (memcpy).
            void Reserve(uint32 minCapacity) noexcept {
                // Les tags ne consomment pas de mémoire, inutile de réserver.
                if (mMeta->isZeroSize || minCapacity <= mCapacity) {
                    return;
                }

                const uint32 newCap = GrowCapacity(mCapacity, minCapacity);
                const usize  bytes  = static_cast<usize>(newCap) * mMeta->size;

                // Allocation alignée selon les besoins du type.
#if defined(_MSC_VER)
                void* newData = _aligned_malloc(bytes, mMeta->align);
#else
                void* newData = std::aligned_alloc(mMeta->align, bytes);
#endif
                NKECS_ASSERT(newData != nullptr);

                if (mData != nullptr && mSize > 0) {
                    // Déplacement ou copie des éléments existants vers le nouveau buffer.
                    if (mMeta->moveConstruct != nullptr) {
                        mMeta->moveConstruct(newData, mData, mSize);
                    } else {
                        // Fallback si moveConstruct absent : copie binaire.
                        std::memcpy(newData, mData, static_cast<usize>(mSize) * mMeta->size);
                    }

                    // Destruction des anciens éléments (avant libération du buffer).
                    // Note : pour les types trivialement destructibles, cette opération
                    // peut être omise, mais `mMeta->destruct` la gère en interne.
                    if (mMeta->destruct != nullptr) {
                        mMeta->destruct(mData, mSize);
                    }
                }

                // Libération de l'ancien buffer.
#if defined(_MSC_VER)
                if (mData != nullptr) {
                    _aligned_free(mData);
                }
#else
                if (mData != nullptr) {
                    std::free(mData);
                }
#endif

                mData     = newData;
                mCapacity = newCap;
            }

          private:
            // Calcule la nouvelle capacité en appliquant une stratégie de croissance
            // géométrique (1.5x) jusqu'à atteindre `required`.
            static uint32 GrowCapacity(uint32 current, uint32 required) noexcept {
                uint32 cap = (current < 8u) ? 8u : current;
                while (cap < required) {
                    cap += cap / 2u;
                }
                return cap;
            }

            // -----------------------------------------------------------------
            // Membres privés
            // -----------------------------------------------------------------
            const ComponentMeta* mMeta     = nullptr;   // Métadonnées du type stocké
            void*                mData     = nullptr;   // Bloc mémoire aligné
            uint32               mSize     = 0;         // Nombre d'éléments actuellement présents
            uint32               mCapacity = 0;         // Nombre maximal d'éléments avant réallocation
        };

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCOMPONENTPOOL.H
// =============================================================================
//
// `NkComponentPool` est une classe interne destinée à être utilisée par
// `NkArchetype` et `NkWorld`. Cependant, comprendre son fonctionnement est
// utile pour étendre le moteur ou déboguer des problèmes de performance.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création manuelle d'une pool et manipulation d'éléments
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkComponentPool.h"
    #include "NkECS/Core/NkTypeRegistry.h"

    // Définition d'un composant simple.
    struct Health {
        float current;
        float max;
    };
    NK_COMPONENT(Health);

    void ExampleBasicPool() {
        using namespace nkentseu::ecs;

        // Récupération des métadonnées du composant Health.
        const ComponentMeta* meta = NkMetaOf<Health>();

        // Création d'une pool vide.
        NkComponentPool pool(meta);

        // Ajout d'éléments par défaut.
        uint32 idx1 = pool.PushDefault();
        uint32 idx2 = pool.PushDefault();
        uint32 idx3 = pool.PushDefault();

        // Accès aux éléments via AtTyped<T>().
        Health* h1 = pool.AtTyped<Health>(idx1);
        if (h1) {
            h1->current = 100.0f;
            h1->max     = 100.0f;
        }

        // Ajout d'une copie d'un élément existant.
        uint32 idx4 = pool.PushCopy(h1);

        // Suppression par swap‑remove.
        uint32 swappedIdx = pool.SwapRemove(idx1);
        // Après cela, l'élément qui était à `swappedIdx` a été déplacé à la place
        // de l'élément supprimé. La taille de la pool est maintenant 3.

        printf("Pool size: %u, capacity: %u\n", pool.Size(), pool.Capacity());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déplacement d'éléments entre deux pools (simulation de migration)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkComponentPool.h"

    void ExampleMoveBetweenPools() {
        using namespace nkentseu::ecs;

        const ComponentMeta* meta = NkMetaOf<Health>();

        // Pool source avec quelques éléments.
        NkComponentPool srcPool(meta);
        uint32 srcIdx = srcPool.PushDefault();
        Health* h = srcPool.AtTyped<Health>(srcIdx);
        h->current = 50.0f;
        h->max     = 100.0f;

        // Pool destination vide.
        NkComponentPool dstPool(meta);

        // Déplacement de l'élément de srcPool vers dstPool.
        dstPool.MoveFrom(srcPool, srcIdx);
        // Après MoveFrom, l'élément est présent dans dstPool.
        // Il faut ensuite supprimer manuellement l'élément de srcPool.
        srcPool.SwapRemove(srcIdx);

        // Vérification.
        Health* moved = dstPool.AtTyped<Health>(0);
        printf("Moved health: current=%.1f, max=%.1f\n", moved->current, moved->max);
        // srcPool est maintenant vide.
        printf("Src pool size: %u, Dst pool size: %u\n", srcPool.Size(), dstPool.Size());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Itération rapide sur le tableau brut (Data<T>)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkComponentPool.h"

    void ExampleFastIteration() {
        using namespace nkentseu::ecs;

        const ComponentMeta* meta = NkMetaOf<Health>();
        NkComponentPool pool(meta);

        // Ajout de nombreux éléments.
        for (int i = 0; i < 1000; ++i) {
            uint32 idx = pool.PushDefault();
            Health* h = pool.AtTyped<Health>(idx);
            h->current = static_cast<float>(i);
            h->max     = 100.0f;
        }

        // Récupération du pointeur brut et itération directe (sans appels de fonction).
        Health* data = pool.Data<Health>();
        uint32 count = pool.Size();

        // Boucle très efficace (le compilateur peut vectoriser).
        for (uint32 i = 0; i < count; ++i) {
            data[i].current += 10.0f;
        }

        printf("Updated %u health components\n", count);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion de composants de taille nulle (Tags)
// -----------------------------------------------------------------------------
/*
    #include "NkECS/Storage/NkComponentPool.h"

    // Définition d'un tag (struct vide).
    struct IsPlayer {};
    NK_COMPONENT(IsPlayer);

    void ExampleTagPool() {
        using namespace nkentseu::ecs;

        const ComponentMeta* meta = NkMetaOf<IsPlayer>();
        NkComponentPool pool(meta);

        // Ajout de "tags" : ils n'occupent aucune mémoire.
        uint32 idx1 = pool.PushDefault();
        uint32 idx2 = pool.PushDefault();

        // La taille logique augmente, mais la capacité reste à 0 (car isZeroSize).
        printf("Size: %u, Capacity: %u\n", pool.Size(), pool.Capacity());

        // At() retourne nullptr pour les tags.
        void* ptr = pool.At(idx1);
        assert(ptr == nullptr);

        // Suppression par swap‑remove (ne manipule aucune mémoire).
        pool.SwapRemove(idx1);
        printf("After remove: Size: %u\n", pool.Size());
    }
*/

// =============================================================================