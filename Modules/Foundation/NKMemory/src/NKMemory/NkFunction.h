// =============================================================================
// NKMemory/NkFunction.h
// Fonctions utilitaires mémoire : copie, comparaison, recherche, transformation.
//
// Design :
//  - Séparation claire déclarations (.h) / implémentations (.cpp)
//  - Réutilisation maximale de NKMemory/NkUtils.h (ZÉRO duplication)
//  - API cohérente avec surcharges pour slices et offsets
//  - Templates type-safe pour swap/construct/destroy
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKFUNCTION_H
#define NKENTSEU_MEMORY_NKFUNCTION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DÉPENDANCES
    // -------------------------------------------------------------------------
    #include "NKMemory/NkMemoryApi.h"      // NKENTSEU_MEMORY_API
    #include "NKMemory/NkAllocator.h"      // NkAllocator, NK_MEMORY_DEFAULT_ALIGNMENT
    #include "NKCore/NkTypes.h"            // nk_size, nk_uint8, nk_int32, etc.
    #include "NKCore/NkTraits.h"           // traits::NkForward, traits::NkMove

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION ET MACROS UTILITAIRES
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryFunctions Fonctions Utilitaires Mémoire
     * @brief API unifiée pour opérations mémoire bas niveau
     * 
     * Caractéristiques :
     *  - Wrappers autour de NkMemCopy/NkMemMove/etc. de NkUtils.h
     *  - Surcharges avec bounds checking pour slices/offsets
     *  - Templates pour opérations type-safe (swap, construct, destroy)
     *  - Fonctions avancées : recherche de pattern, checksum, alignement
     *  - Fallback portable avec optimisations SIMD optionnelles (NkFunctionSIMD.h)
     * 
     * @note Toutes les fonctions gèrent nullptr et size==0 de façon sûre
     * @note Les opérations avec offsets effectuent un bounds checking explicite
     * @note En release, les assertions de bounds peuvent être désactivées via config
     */

    namespace nkentseu {
        namespace memory {

            // -------------------------------------------------------------------------
            // SECTION 3 : ALIASES DE TYPES POUR LISIBILITÉ
            // -------------------------------------------------------------------------
            /** @brief Alias pour nk_size - usage local dans ce module */
            using usize = nk_size;
            /** @brief Alias pour nk_uint8 */
            using uint8 = nk_uint8;
            /** @brief Alias pour nk_int32 */
            using int32 = nk_int32;
            /** @brief Alias pour nk_uint32 */
            using uint32 = nk_uint32;
            /** @brief Alias pour decltype(nullptr) */
            using nknullptr = decltype(nullptr);

            // -------------------------------------------------------------------------
            // SECTION 4 : OPÉRATIONS DE BASE (COPY, MOVE, SET, COMPARE)
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryBasicOps Opérations Mémoire de Base
             * @brief Wrappers sécurisés autour des primitives NkUtils
             */

            /**
             * @brief Copie un bloc mémoire (source et destination non chevauchantes)
             * @param dst Pointeur de destination
             * @param src Pointeur de source
             * @param size Nombre d'octets à copier
             * @return dst
             * @ingroup MemoryBasicOps
             * @note Délègue à NkMemCopy de NkUtils.h (optimisations AVX2 si disponibles)
             */
            NKENTSEU_MEMORY_API 
            void* NkCopy(void* dst, const void* src, usize size) noexcept;

            /**
             * @brief Copie un slice avec bounds checking
             * @param dst Buffer de destination
             * @param dstSize Taille totale du buffer destination
             * @param dstOffset Offset dans le buffer destination
             * @param src Buffer source
             * @param srcSize Taille totale du buffer source
             * @param srcOffset Offset dans le buffer source
             * @param size Nombre d'octets demandés
             * @return dst, ou dst inchangé si bounds invalides
             * @ingroup MemoryBasicOps
             * @note Copie min(dst_available, src_available, size) octets
             */
            NKENTSEU_MEMORY_API 
            void* NkCopy(void* dst, usize dstSize, usize dstOffset,
                        const void* src, usize srcSize, usize srcOffset, usize size) noexcept;

            /**
             * @brief Déplace un bloc mémoire (chevauchement autorisé)
             * @param dst Pointeur de destination
             * @param src Pointeur de source
             * @param size Nombre d'octets à déplacer
             * @return dst
             * @ingroup MemoryBasicOps
             * @note Délègue à NkMemMove de NkUtils.h
             */
            NKENTSEU_MEMORY_API 
            void* NkMove(void* dst, const void* src, usize size) noexcept;

            /**
             * @brief Déplace un slice avec bounds checking
             * @note Même signature que NkCopy avec offsets, délègue à NkMemMove
             */
            NKENTSEU_MEMORY_API 
            void* NkMove(void* dst, usize dstSize, usize dstOffset,
                        const void* src, usize srcSize, usize srcOffset, usize size) noexcept;

            /**
             * @brief Remplit un bloc mémoire avec une valeur octet
             * @param dst Pointeur vers la destination
             * @param value Valeur à écrire (truncée à 8 bits)
             * @param size Nombre d'octets à remplir
             * @return dst
             * @ingroup MemoryBasicOps
             * @note Délègue à NkMemSet de NkUtils.h
             */
            NKENTSEU_MEMORY_API 
            void* NkSet(void* dst, int32 value, usize size) noexcept;

            /**
             * @brief Remplit un slice avec bounds checking
             * @note Même principe que NkCopy avec offsets
             */
            NKENTSEU_MEMORY_API 
            void* NkSet(void* dst, usize dstSize, usize offset, int32 value, usize size) noexcept;

            /**
             * @brief Compare deux blocs mémoire octet par octet
             * @param a Premier bloc
             * @param b Deuxième bloc
             * @param size Nombre d'octets à comparer
             * @return <0 si a<b, 0 si égal, >0 si a>b
             * @ingroup MemoryBasicOps
             * @note Délègue à NkMemCompare de NkUtils.h
             */
            NKENTSEU_MEMORY_API 
            int32 NkCompare(const void* a, const void* b, usize size) noexcept;

            /**
             * @brief Compare deux blocs avec tailles potentiellement différentes
             * @note Compare min(size, aSize, bSize) octets
             */
            NKENTSEU_MEMORY_API 
            int32 NkCompare(const void* a, usize aSize, const void* b, usize bSize, usize size) noexcept;

            /**
             * @brief Compare deux slices avec offsets et bounds checking
             * @note Compare min(available_in_a, available_in_b, size) octets
             */
            NKENTSEU_MEMORY_API 
            int32 NkCompare(const void* a, usize aSize, usize offsetA,
                           const void* b, usize bSize, usize offsetB, usize size) noexcept;

            // -------------------------------------------------------------------------
            // SECTION 5 : RECHERCHE ET ANALYSE
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemorySearch Recherche et Analyse Mémoire
             * @brief Fonctions de recherche de valeurs, patterns, différences
             */

            /**
             * @brief Trouve la première occurrence d'un octet dans un buffer
             * @param ptr Buffer à parcourir
             * @param value Octet à rechercher
             * @param size Taille du buffer
             * @return Pointeur vers l'occurrence, ou nullptr si non trouvé
             * @ingroup MemorySearch
             */
            NKENTSEU_MEMORY_API 
            void* NkFind(const void* ptr, uint8 value, usize size) noexcept;

            /**
             * @brief Trouve la dernière occurrence d'un octet dans un buffer
             * @note Parcours de fin vers début
             */
            NKENTSEU_MEMORY_API 
            void* NkFindLast(const void* buffer, usize size, uint8 value) noexcept;

            /**
             * @brief Trouve le premier index où deux buffers diffèrent
             * @param ptr1 Premier buffer
             * @param ptr2 Deuxième buffer
             * @param count Nombre d'octets maximum à comparer
             * @return Index de première différence, ou count si identiques
             * @ingroup MemorySearch
             */
            NKENTSEU_MEMORY_API 
            usize NkFindDifference(const void* ptr1, const void* ptr2, usize count) noexcept;

            /**
             * @brief Teste si un buffer est entièrement à zéro
             * @param ptr Buffer à tester
             * @param count Nombre d'octets à vérifier
             * @return true si tous les octets sont 0
             * @ingroup MemorySearch
             * @note Plus rapide que NkCompare avec un buffer de zéros pour petits buffers
             */
            NKENTSEU_MEMORY_API 
            bool NkIsZero(const void* ptr, usize count) noexcept;

            /**
             * @brief Teste si deux régions mémoire se chevauchent
             * @param ptr1 Début de la première région
             * @param ptr2 Début de la deuxième région
             * @param count Taille des régions (supposées égales)
             * @return true si chevauchement détecté
             * @ingroup MemorySearch
             */
            NKENTSEU_MEMORY_API 
            bool NkOverlaps(const void* ptr1, const void* ptr2, usize count) noexcept;

            /**
             * @brief Teste si un buffer commence par un pattern donné
             * @param buffer Buffer à tester
             * @param size Taille du buffer
             * @param pattern Pattern à rechercher
             * @param patternSize Taille du pattern
             * @return true si buffer commence par pattern
             * @ingroup MemorySearch
             */
            NKENTSEU_MEMORY_API 
            bool NkEqualsPattern(const void* buffer, usize size, 
                                const void* pattern, usize patternSize) noexcept;

            /**
             * @brief Valide l'accessibilité d'un buffer (défensif)
             * @param buffer Buffer à valider
             * @param size Taille supposée
             * @return true si buffer semble accessible (test minimal)
             * @note Ne garantit pas la sécurité : utilise volatile pour éviter l'optimisation
             * @ingroup MemorySearch
             */
            NKENTSEU_MEMORY_API 
            bool NkValidateMemory(const void* buffer, usize size) noexcept;

            /**
             * @brief Recherche un pattern dans un buffer (première occurrence)
             * @param buffer Buffer à parcourir (haystack)
             * @param size Taille du buffer
             * @param pattern Pattern à rechercher (needle)
             * @param patternSize Taille du pattern
             * @return Pointeur vers première occurrence, ou nullptr
             * @ingroup MemorySearch
             * @note Implémentation naïve O(n*m) - voir NkFunctionSIMD.h pour version optimisée
             */
            NKENTSEU_MEMORY_API 
            void* NkSearchPattern(const void* buffer, usize size,
                                 const void* pattern, usize patternSize) noexcept;

            /**
             * @brief Recherche un pattern dans un buffer (dernière occurrence)
             * @note Parcours de fin vers début
             */
            NKENTSEU_MEMORY_API 
            void* NkFindLastPattern(const void* buffer, usize size,
                                   const void* pattern, usize patternSize) noexcept;

            // -------------------------------------------------------------------------
            // SECTION 6 : TRANSFORMATIONS ET MANIPULATIONS
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryTransform Transformations Mémoire
             * @brief Fonctions de modification in-place ou avec transformation
             */

            /**
             * @brief Inverse l'ordre des octets dans un buffer
             * @param buffer Buffer à inverser
             * @param size Taille du buffer
             * @note In-place, O(n/2) swaps
             * @ingroup MemoryTransform
             */
            NKENTSEU_MEMORY_API 
            void NkReverse(void* buffer, usize size) noexcept;

            /**
             * @brief Rotation circulaire d'un buffer
             * @param buffer Buffer à faire pivoter
             * @param size Taille du buffer
             * @param offset Décalage (positif = droite, négatif = gauche)
             * @note Algorithme triple-reverse : O(n) temps, O(1) espace
             * @ingroup MemoryTransform
             */
            NKENTSEU_MEMORY_API 
            void NkRotate(void* buffer, usize size, int32 offset) noexcept;

            /**
             * @brief Alias vers NkSet pour clarté sémantique
             * @ingroup MemoryTransform
             */
            NKENTSEU_MEMORY_API 
            void NkFill(void* dst, usize size, int32 value) noexcept;

            /**
             * @brief Applique une fonction de transformation octet par octet
             * @param dst Buffer de destination
             * @param src Buffer source
             * @param size Nombre d'octets à transformer
             * @param func Fonction de transformation (uint8 → uint8)
             * @note dst et src peuvent être identiques pour transformation in-place
             * @ingroup MemoryTransform
             */
            NKENTSEU_MEMORY_API 
            void NkTransform(void* dst, const void* src, usize size, 
                            int32 (*func)(int32)) noexcept;

            /**
             * @brief Copie conditionnelle basée sur un masque
             * @param dst Buffer de destination (modifié in-place)
             * @param conditionMask Masque d'octets (non-zero = copier)
             * @param values Buffer source des valeurs à copier
             * @param size Nombre d'octets à traiter
             * @note Pour chaque i : if (mask[i]) dst[i] = values[i]
             * @ingroup MemoryTransform
             */
            NKENTSEU_MEMORY_API 
            void NkConditionalSet(void* dst, const void* conditionMask, 
                                 const void* values, usize size) noexcept;

            // -------------------------------------------------------------------------
            // SECTION 7 : UTILITAIRES D'ALLOCATION ET ALIGNEMENT
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryAllocUtils Utilitaires d'Allocation
             * @brief Helpers pour duplication, alignement, mapping mémoire
             */

            /**
             * @brief Duplique un buffer avec allocation automatique
             * @param src Buffer source à dupliquer
             * @param size Taille à copier
             * @return Nouveau buffer alloué, ou nullptr en cas d'échec
             * @note Utilise NkAlloc de l'allocateur par défaut
             * @note L'appelant est responsable de NkFree() le résultat
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void* NkDuplicate(const void* src, usize size) noexcept;

            /**
             * @brief Calcule un checksum simple (FNV-1a like)
             * @param data Données à hasher
             * @param size Nombre d'octets
             * @return Checksum 32-bit
             * @note Non cryptographique : pour détection d'erreurs uniquement
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            uint32 NkChecksum(const void* data, usize size) noexcept;

            /**
             * @brief Inverse l'ordre des octets dans chaque élément (endian swap)
             * @param buffer Buffer contenant les éléments
             * @param elementSize Taille de chaque élément (2, 4, ou 8 typiquement)
             * @param count Nombre d'éléments
             * @note Utile pour conversion big-endian ↔ little-endian
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void NkSwapEndian(void* buffer, usize elementSize, usize count) noexcept;

            /**
             * @brief Zero-initialisation sécurisée (anti-optimization)
             * @param ptr Buffer à zéro
             * @param size Taille en octets
             * @note Utilise volatile pour empêcher l'optimisation du compiler
             * @note Utile pour effacer des données sensibles (clés, mots de passe)
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void NkSecureZero(void* ptr, usize size) noexcept;

            /**
             * @brief Alias vers NkMemZero pour cohérence d'API
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void NkZero(void* ptr, usize size) noexcept;

            /**
             * @brief Alloue de la mémoire alignée
             * @param alignment Alignement requis (puissance de 2)
             * @param size Taille à allouer
             * @return Pointeur aligné, ou nullptr en cas d'échec
             * @note Délègue à NkAlloc avec paramètre d'alignement
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void* NkAllocAligned(usize alignment, usize size) noexcept;

            /**
             * @brief Libère une mémoire allouée via NkAllocAligned
             * @param ptr Pointeur à libérer
             * @param size Taille (informationnelle, peut être ignorée)
             * @note Délègue à NkFree
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void NkFreeAligned(void* ptr, usize size) noexcept;

            /**
             * @brief Wrapper compatible POSIX pour allocation alignée
             * @param ptr Output parameter pour recevoir le pointeur alloué
             * @param alignment Alignement requis
             * @param size Taille à allouer
             * @return 0 si succès, -1 si échec
             * @note Signature compatible avec posix_memalign
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            int32 NkPosixAligned(void** ptr, usize alignment, usize size) noexcept;

            /**
             * @brief Ajuste un pointeur et un espace disponible pour alignement
             * @param align Alignement requis (puissance de 2)
             * @param size Taille nécessaire après alignement
             * @param outPtr Pointeur à ajuster (modifié in-place si succès)
             * @param space Espace disponible (réduit de l'ajustement si succès)
             * @return true si alignement possible dans l'espace disponible
             * @note Utile pour allocation dans un buffer pré-alloué (arena)
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            bool NkAlignPointer(usize align, usize size, void*& outPtr, usize& space) noexcept;

            /**
             * @brief Calcule le pointeur aligné sans modifier l'original
             * @param ptr Pointeur de référence
             * @param align Alignement requis
             * @param size Taille nécessaire (pour validation)
             * @return Pointeur aligné, ou nullptr si impossible
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void* NkAlignPointer(void* ptr, usize align, usize size) noexcept;

            /**
             * @brief Alloue de la mémoire virtuelle (mmap/VirtualAlloc)
             * @param size Taille à allouer
             * @return Pointeur vers région mappée, ou nullptr
             * @note Délègue à NkGetVirtualAllocator().Allocate()
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void* NkMap(usize size) noexcept;

            /**
             * @brief Libère une région allouée via NkMap
             * @param ptr Pointeur à unmapper
             * @param size Taille (informationnelle)
             * @note Délègue à NkGetVirtualAllocator().Deallocate()
             * @ingroup MemoryAllocUtils
             */
            NKENTSEU_MEMORY_API 
            void NkUnmap(void* ptr, usize size) noexcept;

            // -------------------------------------------------------------------------
            // SECTION 8 : TEMPLATES TYPE-SAFE
            // -------------------------------------------------------------------------
            /**
             * @defgroup MemoryTemplates Templates Type-Safe
             * @brief Fonctions génériques pour opérations sur types C++
             */

            /**
             * @brief Échange deux valeurs de type T (move-semantics)
             * @tparam T Type des valeurs (doit être movable)
             * @param a Première valeur
             * @param b Deuxième valeur
             * @note Utilise traits::NkMove pour perfect forwarding
             * @ingroup MemoryTemplates
             */
            template <typename T>
            void NkSwap(T& a, T& b) noexcept;

            /**
             * @brief Échange deux pointeurs vers T
             * @tparam T Type pointé
             * @param a Pointeur vers première valeur
             * @param b Pointeur vers deuxième valeur
             * @note Safe : vérifie nullptr et égalité de pointeurs avant swap
             * @ingroup MemoryTemplates
             */
            template <typename T>
            void NkSwapPtr(T* a, T* b) noexcept;

            /**
             * @brief Échange deux valeurs trivialement copiables (byte-wise)
             * @tparam T Type trivialement copiable (static_assert)
             * @param a Première valeur
             * @param b Deuxième valeur
             * @note Plus rapide que NkSwap pour types POD : copie octet par octet
             * @ingroup MemoryTemplates
             */
            template <typename T>
            void NkSwapTrivial(T& a, T& b) noexcept;

            /**
             * @brief Spécialisation de NkSwap pour pointeurs de pointeurs
             * @note Échange les pointeurs eux-mêmes, pas les valeurs pointées
             */
            template <typename T>
            void NkSwap(T*& a, T*& b) noexcept;

            /**
             * @brief Construct placement-new avec perfect forwarding
             * @tparam T Type à construire
             * @tparam Args Types des arguments du constructeur
             * @param ptr Pointeur vers mémoire pré-allouée (doit être alignée pour T)
             * @param args Arguments forwardés au constructeur de T
             * @return ptr casté vers T*, ou nullptr si ptr était nullptr
             * @note N'alloue PAS de mémoire : suppose que ptr pointe vers mémoire valide
             * @ingroup MemoryTemplates
             */
            template <typename T, typename... Args>
            T* NkConstruct(T* ptr, Args&&... args);

            /**
             * @brief Détruit explicitement un objet (appelle le destructeur)
             * @tparam T Type de l'objet à détruire
             * @param ptr Pointeur vers l'objet (nullptr accepté, no-op)
             * @note N'appelle PAS delete : seulement le destructeur ~T()
             * @note L'appelant reste responsable de deallocation si nécessaire
             * @ingroup MemoryTemplates
             */
            template <typename T>
            void NkDestroy(T* ptr) noexcept;

        } // namespace memory

        // -------------------------------------------------------------------------
        // SECTION 9 : NAMESPACE ALIAS POUR ACCÈS COURT
        // -------------------------------------------------------------------------
        /** @brief Alias de namespace pour accès court : nkentseu::mem::NkCopy */
        namespace mem = memory;

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 10 : IMPLÉMENTATIONS TEMPLATES INLINE
    // -------------------------------------------------------------------------
    // Les templates doivent être dans le header pour l'instantiation.

    namespace nkentseu {
        namespace memory {

            // ====================================================================
            // Implémentations Inline - Templates Type-Safe
            // ====================================================================

            template <typename T>
            void NkSwap(T& a, T& b) noexcept {
                traits::NkSwap(a, b);
            }

            template <typename T>
            void NkSwapPtr(T* a, T* b) noexcept {
                if (!a || !b || a == b) {
                    return;  // Safe : no-op sur nullptr ou même adresse
                }
                T temp = traits::NkMove(*a);
                *a = traits::NkMove(*b);
                *b = traits::NkMove(temp);
            }

            template <typename T>
            void NkSwapTrivial(T& a, T& b) noexcept {
                static_assert(std::is_trivially_copyable_v<T>, 
                             "NkSwapTrivial requires trivially copyable types.");
                if (&a == &b) {
                    return;  // No-op si même adresse
                }
                // Swap byte-wise pour types POD
                uint8* pa = reinterpret_cast<uint8*>(&a);
                uint8* pb = reinterpret_cast<uint8*>(&b);
                for (usize i = 0; i < sizeof(T); ++i) {
                    const uint8 tmp = pa[i];
                    pa[i] = pb[i];
                    pb[i] = tmp;
                }
            }

            template <typename T>
            void NkSwap(T*& a, T*& b) noexcept {
                T* temp = a;
                a = b;
                b = temp;
            }

            template <typename T, typename... Args>
            T* NkConstruct(T* ptr, Args&&... args) {
                if (!ptr) {
                    return nullptr;
                }
                return new (ptr) T(traits::NkForward<Args>(args)...);
            }

            template <typename T>
            void NkDestroy(T* ptr) noexcept {
                if (!ptr) {
                    return;
                }
                ptr->~T();
            }

        } // namespace memory
    } // namespace nkentseu

#endif // NKENTSEU_MEMORY_NKFUNCTION_H

// =============================================================================
// NOTES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Copie avec bounds checking
    uint8_t src[100] = { /\* ... *\/;
    uint8_t dst[50];
    
    // Copie 30 bytes de src[10..39] vers dst[5..34]
    nkentseu::memory::NkCopy(dst, sizeof(dst), 5, src, sizeof(src), 10, 30);
    
    // Exemple 2 : Recherche de pattern
    const char* text = "Hello, World!";
    const char* pattern = "World";
    
    if (void* pos = nkentseu::memory::NkSearchPattern(text, strlen(text), 
                                                      pattern, strlen(pattern))) {
        printf("Found at offset %ld\n", static_cast<const char*>(pos) - text);
    }
    
    // Exemple 3 : Rotation circulaire
    uint8_t buffer[] = {1, 2, 3, 4, 5};
    nkentseu::memory::NkRotate(buffer, sizeof(buffer), 2);  // → {4, 5, 1, 2, 3}
    
    // Exemple 4 : Construction placement-new
    alignas(MyClass) uint8_t storage[sizeof(MyClass)];
    MyClass* obj = nkentseu::memory::NkConstruct<MyClass>(
        reinterpret_cast<MyClass*>(storage), arg1, arg2);
    // ... utilisation ...
    nkentseu::memory::NkDestroy(obj);  // Appelle ~MyClass()
    
    // Exemple 5 : Alignement dans un buffer arena
    void* arena = malloc(4096);
    void* ptr = arena;
    usize space = 4096;
    
    if (nkentseu::memory::NkAlignPointer(64, sizeof(MyStruct), ptr, space)) {
        // ptr est maintenant aligné 64 bytes, space réduit de l'ajustement
        MyStruct* obj = new (ptr) MyStruct();  // Placement new
    }
*/

// =============================================================================
// COMPARAISON AVEC FONCTIONS STANDARD C
// =============================================================================
/*
    | Fonction C      | Équivalent NKMemory      | Avantages NKMemory |
    |----------------|-------------------------|-------------------|
    | memcpy         | NkCopy                  | Bounds checking, AVX2 optim |
    | memmove        | NkMove                  | Idem + détection chevauchement |
    | memset         | NkSet / NkFill          | Idem + slicing |
    | memcmp         | NkCompare               | Idem + tailles différentes |
    | memchr         | NkFind                  | Idem + NkFindLast |
    | memcmp + loop  | NkSearchPattern         | API unifiée, extensible SIMD |
    | malloc + memcpy| NkDuplicate             | Allocation + copie en un appel |
    | explicit_bzero | NkSecureZero            | Portable, garanti anti-optim |

    Quand utiliser NKMemory :
    ✓ Besoin de bounds checking explicite pour sécurité
    ✓ Intégration avec le système d'allocateurs NKMemory
    ✓ Optimisations plateforme-aware (AVX2, etc.)
    ✓ Cohérence avec le reste du codebase NK*

    Quand utiliser les fonctions C standard :
    ✓ Code legacy ou interop avec bibliothèques externes
    ✓ Performance critique avec compiler bien optimisé (les builtins sont excellents)
    ✓ Simplicité maximale pour opérations triviales
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================