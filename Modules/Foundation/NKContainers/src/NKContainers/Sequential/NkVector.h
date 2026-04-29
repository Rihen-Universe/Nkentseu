// =============================================================================
// NKContainers/Sequential/NkVector.h
// Tableau dynamique — conteneur séquentiel principal 100% indépendant de la STL.
//
// Design :
//  - Réutilisation DIRECTE des macros NKCore/NKPlatform (ZÉRO duplication)
//  - Gestion mémoire via NKMemory::NkAllocator pour flexibilité d'allocation
//  - Itérateurs PascalCase (Begin/End) + aliases minuscules pour compatibilité STL
//  - Gestion d'erreurs unifiée via NKENTSEU_CONTAINERS_THROW_* macros
//  - Indentation stricte : namespaces imbriqués, visibilité, blocs tous indentés
//  - Une instruction par ligne pour lisibilité et maintenance
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
//
// Historique des versions :
//   v1.1.0 (2026-02-09) :
//     - Ajout méthodes PascalCase Begin()/End()/CBegin()/CEnd() et variantes inverses
//     - Ajout IsEmpty() alias de Empty() pour cohérence de nommage
//     - Compatibilité Erase(ConstIterator) avec Begin()+i via conversion implicite
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED
#define NKENTSEU_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKVector dépend des modules suivants :
    //  - NKCore/NkTypes.h : Types fondamentaux (nk_size, nk_bool, etc.)
    //  - NKContainers/NkContainersApi.h : Macros d'export pour visibilité des symboles
    //  - NKCore/NkTraits.h : Utilitaires de métaprogrammation (NkMove, NkForward, etc.)
    //  - NKMemory/NkAllocator.h : Interface d'allocateur pour gestion mémoire flexible
    //  - NKMemory/NkFunction.h : Fonctions mémoire bas niveau (NkCopy, etc.)
    //  - NKCore/Assert/NkAssert.h : Système d'assertions pour validation debug
    //  - NKContainers/Iterators/NkIterator.h : Itérateurs génériques du framework
    //  - NKContainers/Iterators/NkInitializerList.h : Liste d'initialisation légère
    //  - NkVectorError.h : Codes d'erreur et gestion d'exceptions spécifiques Vector

    #include "NKCore/NkTypes.h"
    #include "NKContainers/NkContainersApi.h"
    #include "NKCore/NkTraits.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKMemory/NkFunction.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKContainers/Iterators/NkIterator.h"
    #include "NKContainers/Iterators/NkInitializerList.h"
    #include "NKContainers/Sequential/NkVectorError.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les composants NKContainers sont encapsulés dans ce namespace hiérarchique
    // pour éviter les collisions de noms et assurer une API cohérente et modulaire.
    // L'indentation reflète la hiérarchie : chaque niveau de namespace est indenté.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : CLASSE TEMPLATE NKVECTOR
        // ====================================================================
        /**
         * @brief Tableau dynamique — conteneur séquentiel principal du framework.
         * @tparam T Type des éléments stockés dans le vecteur
         * @tparam Allocator Type de l'allocateur utilisé pour la gestion mémoire
         * @ingroup SequentialContainers
         *
         * NkVector implémente un tableau dynamique à croissance automatique,
         * similaire à std::vector mais 100% indépendant de la STL. Il offre :
         *
         *  - Accès aléatoire en temps constant O(1) via operator[] et At()
         *  - Insertion/suppression en fin amortie O(1) via PushBack/PopBack
         *  - Insertion/suppression arbitraire O(n) via Insert/Erase
         *  - Gestion mémoire flexible via allocateur personnalisable
         *  - Itérateurs compatibles range-based for et algorithmes génériques
         *  - Gestion d'erreurs configurable (exceptions ou assertions)
         *
         * @note Nommage : Les méthodes PascalCase (Begin, End, Erase...) sont le
         *       standard interne du framework. Les aliases minuscules (begin, end)
         *       sont maintenus pour la compatibilité avec le range-based for C++
         *       et les algorithmes génériques de type STL.
         *
         * @note Complexité algorithmique :
         *       - Accès aléatoire [i] : O(1)
         *       - PushBack : O(1) amorti (réallocation occasionnelle)
         *       - Insert/Erase au milieu : O(n) (décalage des éléments)
         *       - Resize avec croissance : O(n) dans le pire cas
         *
         * @example Utilisation basique
         * @code
         * using namespace nkentseu::containers::sequential;
         *
         * // Construction avec liste d'initialisation
         * NkVector<int> values = {1, 2, 3, 4, 5};
         *
         * // Ajout d'éléments
         * values.PushBack(6);
         * values.EmplaceBack(7);  // Construction in-place
         *
         * // Accès aux éléments
         * int first = values.Front();      // 1
         * int last = values.Back();        // 7
         * int third = values[2];           // 3 (sans vérification bounds)
         * int fourth = values.At(3);       // 4 (avec vérification bounds)
         *
         * // Parcours avec itérateurs PascalCase
         * for (auto it = values.Begin(); it != values.End(); ++it) {
         *     printf("%d ", *it);
         * }
         *
         * // Parcours avec range-based for (compatibilité STL)
         * for (auto& val : values) {
         *     val *= 2;  // Modification in-place
         * }
         *
         * // Suppression d'éléments
         * values.Erase(values.Begin() + 1);  // Supprime l'index 1
         * values.PopBack();                   // Supprime le dernier
         * @endcode
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVector {
            public:

                // -----------------------------------------------------------------
                // Sous-section : Types membres publics
                // -----------------------------------------------------------------
                /**
                 * @brief Type des valeurs stockées dans le conteneur
                 * @ingroup VectorTypes
                 */
                using ValueType = T;

                /**
                 * @brief Type de l'allocateur utilisé pour la gestion mémoire
                 * @ingroup VectorTypes
                 */
                using AllocatorType = Allocator;

                /**
                 * @brief Type utilisé pour représenter la taille et les index
                 * @ingroup VectorTypes
                 * @note Alias vers nk_size pour cohérence avec le reste du framework
                 */
                using SizeType = nk_size;

                /**
                 * @brief Type utilisé pour les différences entre itérateurs
                 * @ingroup VectorTypes
                 * @note Alias vers nkentseu::ptrdiff pour compatibilité arithmétique
                 */
                using DifferenceType = nkentseu::ptrdiff;

                /**
                 * @brief Type de référence mutable aux éléments
                 * @ingroup VectorTypes
                 */
                using Reference = T&;

                /**
                 * @brief Type de référence constante aux éléments
                 * @ingroup VectorTypes
                 */
                using ConstReference = const T&;

                /**
                 * @brief Type de pointeur mutable vers les éléments
                 * @ingroup VectorTypes
                 */
                using Pointer = T*;

                /**
                 * @brief Type de pointeur constant vers les éléments
                 * @ingroup VectorTypes
                 */
                using ConstPointer = const T*;

                /**
                 * @brief Type d'itérateur mutable (pointeur brut pour performance)
                 * @ingroup VectorTypes
                 * @note Les itérateurs sont des pointeurs bruts pour accès direct O(1)
                 */
                using Iterator = T*;

                /**
                 * @brief Type d'itérateur constant (pointeur constant)
                 * @ingroup VectorTypes
                 */
                using ConstIterator = const T*;

                /**
                 * @brief Type d'itérateur inversé mutable
                 * @ingroup VectorTypes
                 * @note Adaptateur NkReverseIterator pour parcours en sens inverse
                 */
                using ReverseIterator = NkReverseIterator<Iterator>;

                /**
                 * @brief Type d'itérateur inversé constant
                 * @ingroup VectorTypes
                 */
                using ConstReverseIterator = NkReverseIterator<ConstIterator>;

            private:

                // -----------------------------------------------------------------
                // Sous-section : Membres de données privés
                // -----------------------------------------------------------------
                /**
                 * @brief Pointeur vers le début du buffer de données alloué
                 * @var mData
                 * @note Peut être nullptr si capacity == 0 (vecteur vide non alloué)
                 */
                T* mData;

                /**
                 * @brief Nombre d'éléments actuellement stockés dans le vecteur
                 * @var mSize
                 * @note Toujours inférieur ou égal à mCapacity
                 */
                SizeType mSize;

                /**
                 * @brief Capacité actuelle du buffer alloué (en nombre d'éléments)
                 * @var mCapacity
                 * @note Nombre maximal d'éléments pouvant être stockés sans réallocation
                 */
                SizeType mCapacity;

                /**
                 * @brief Pointeur vers l'allocateur utilisé pour la gestion mémoire
                 * @var mAllocator
                 * @note Par défaut pointe vers l'allocateur global du framework
                 */
                Allocator* mAllocator;

                // -----------------------------------------------------------------
                // Sous-section : Méthodes privées de gestion interne
                // -----------------------------------------------------------------
                /**
                 * @brief Calcule la nouvelle capacité cible pour une croissance donnée
                 * @param needed Capacité minimale requise pour l'opération en cours
                 * @return Nouvelle capacité recommandée (géométrique ou needed)
                 * @ingroup VectorInternals
                 *
                 * Stratégie de croissance : facteur ×2 pour réduire la fréquence
                 * des réallocations au prix d'un léger surcoût mémoire moyen.
                 * Pour les très grandes tailles, retourne MaxSize() pour éviter
                 * les dépassements arithmétiques.
                 *
                 * @note Complexité : O(1) — calcul purement arithmétique
                 */
                SizeType CalculateGrowth(SizeType needed) const {
                    SizeType cur = mCapacity;
                    SizeType max = MaxSize();
                    if (cur > max / 2) {
                        return max;
                    }
                    SizeType geometric = cur * 2;
                    return (geometric < needed) ? needed : geometric;
                }

                /**
                 * @brief Détruit explicitement une plage d'éléments
                 * @param first Pointeur vers le premier élément à détruire
                 * @param last Pointeur vers la position après le dernier élément
                 * @ingroup VectorInternals
                 *
                 * Appelle explicitement le destructeur de T pour chaque élément
                 * de la plage [first, last). Nécessaire pour les types non-triviaux
                 * gérant des ressources (RAII).
                 *
                 * @note Pour les types trivialement destructibles, cette fonction
                 *       est optimisée par le compilateur en no-op.
                 * @note Complexité : O(n) où n = last - first
                 */
                void DestroyRange(T* first, T* last) {
                    for (T* it = first; it != last; ++it) {
                        it->~T();
                    }
                }

                /**
                 * @brief Construit un objet de type T in-place via placement new
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param ptr Pointeur vers la mémoire où construire l'objet
                 * @param args Arguments forwardés au constructeur de T
                 * @ingroup VectorInternals
                 *
                 * Utilise le placement new pour construire directement dans
                 * le buffer raw alloué, évitant les copies/moves temporaires.
                 * Supporte le forwarding parfait si C++11 est disponible.
                 *
                 * @note En mode pré-C++11, fallback vers construction par copie
                 * @note La mémoire doit être correctement alignée pour le type T
                 */
                template<typename... Args>
                void ConstructAt(T* ptr, Args&&... args) {
                    #if defined(NKENTSEU_CXX11_OR_LATER)
                        new (ptr) T(traits::NkForward<Args>(args)...);
                    #else
                        new (ptr) T(args...);
                    #endif
                }

                /**
                 * @brief Déplace ou copie une plage d'éléments vers une destination
                 * @param dest Pointeur vers la destination du transfert
                 * @param src Pointeur vers la source des éléments
                 * @param count Nombre d'éléments à transférer
                 * @ingroup VectorInternals
                 *
                 * Optimisation pour les types trivialement copiables : utilise
                 * memory::NkCopy (memcpy) pour un transfert bloc ultra-rapide.
                 * Pour les types complexes, utilise move semantics élément par
                 * élément avec ConstructAt pour construction in-place.
                 *
                 * @note Détection compile-time via __is_trivial(T) ou traits équivalents
                 * @note Complexité : O(n) mais avec constante très faible pour types triviaux
                 */
                void MoveOrCopyRange(T* dest, T* src, SizeType count) {
                    if (NKENTSEU_IS_TRIVIALLY_COPYABLE(T)) {
                        memory::NkCopy(dest, src, count * sizeof(T));
                    }
                    else {
                        for (SizeType i = 0; i < count; ++i) {
                            ConstructAt(dest + i, traits::NkMove(src[i]));
                        }
                    }
                }

                /**
                 * @brief Réalloue le buffer interne avec une nouvelle capacité
                 * @param newCapacity Nouvelle capacité cible en nombre d'éléments
                 * @ingroup VectorInternals
                 *
                 * Étapes de la réallocation :
                 *  1. Si newCapacity == 0 : libère le buffer et réinitialise l'état
                 *  2. Alloue un nouveau buffer via l'allocateur avec alignement requis
                 *  3. Déplace/copie les éléments existants vers le nouveau buffer
                 *  4. Détruit les anciens éléments et libère l'ancien buffer
                 *  5. Met à jour les pointeurs et métadonnées internes
                 *
                 * @note Lève NKENTSEU_CONTAINERS_THROW_BAD_ALLOC si l'allocation échoue
                 * @note Complexité : O(n) pour le transfert des éléments existants
                 * @note Exception-safe : état valide même en cas d'échec d'allocation
                 */
                void Reallocate(SizeType newCapacity) {
                    if (newCapacity == 0) {
                        Clear();
                        if (mData) {
                            mAllocator->Deallocate(mData);
                            mData = nullptr;
                        }
                        mCapacity = 0;
                        return;
                    }

                    T* newData = static_cast<T*>(
                        mAllocator->Allocate(newCapacity * sizeof(T), alignof(T))
                    );

                    if (!newData) {
                        NKENTSEU_CONTAINERS_THROW_BAD_ALLOC(newCapacity);
                        return;
                    }

                    if (mData) {
                        MoveOrCopyRange(newData, mData, mSize);
                        DestroyRange(mData, mData + mSize);
                        mAllocator->Deallocate(mData);
                    }

                    mData = newData;
                    mCapacity = newCapacity;
                }

            public:

                // -----------------------------------------------------------------
                // Sous-section : Constructeurs
                // -----------------------------------------------------------------
                /**
                 * @brief Constructeur par défaut — vecteur vide sans allocation
                 * @ingroup VectorConstructors
                 *
                 * Initialise un vecteur avec :
                 *  - mData = nullptr (pas d'allocation initiale)
                 *  - mSize = 0, mCapacity = 0 (état vide)
                 *  - mAllocator = allocateur global par défaut
                 *
                 * @note Complexité : O(1) — aucune allocation mémoire
                 * @note Premier PushBack déclenchera l'allocation initiale
                 */
                NkVector()
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur avec allocateur personnalisé
                 * @param allocator Pointeur vers l'allocateur à utiliser (ou nullptr pour défaut)
                 * @ingroup VectorConstructors
                 *
                 * Permet d'utiliser un allocateur spécifique pour ce vecteur,
                 * utile pour :
                 *  - Pool d'allocations pour performance prédictible
                 *  - Mémoire alignée pour SIMD/GPU
                 *  - Tracking mémoire pour debugging/profiling
                 *
                 * @note Si allocator == nullptr, utilise l'allocateur global par défaut
                 * @note Complexité : O(1) — aucune allocation de données
                 */
                explicit NkVector(Allocator* allocator)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                }

                /**
                 * @brief Constructeur avec taille initiale — éléments value-initialisés
                 * @param count Nombre d'éléments à créer dans le vecteur
                 * @ingroup VectorConstructors
                 *
                 * Crée un vecteur contenant 'count' éléments, chacun initialisé
                 * via value-initialization (T() pour les types fondamentaux,
                 * constructeur par défaut pour les classes).
                 *
                 * @note Complexité : O(n) pour l'initialisation des éléments
                 * @note Réserve automatiquement la capacité nécessaire
                 *
                 * @example
                 * @code
                 * NkVector<int> zeros(10);        // {0,0,0,0,0,0,0,0,0,0}
                 * NkVector<std::string> strs(5);   // 5 strings vides
                 * @endcode
                 */
                explicit NkVector(SizeType count)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    Resize(count);
                }

                /**
                 * @brief Constructeur avec taille et valeur de remplissage
                 * @param count Nombre d'éléments à créer
                 * @param value Valeur à copier dans chaque élément
                 * @ingroup VectorConstructors
                 *
                 * Crée un vecteur contenant 'count' copies de 'value'.
                 * Utile pour initialiser avec une valeur spécifique différente
                 * de la value-initialization par défaut.
                 *
                 * @note Complexité : O(n) pour les copies successives
                 * @note Requiert que T soit CopyConstructible
                 *
                 * @example
                 * @code
                 * NkVector<int> ones(10, 1);           // {1,1,1,...}
                 * NkVector<std::string> defaults(5, "N/A");  // {"N/A",...}
                 * @endcode
                 */
                NkVector(SizeType count, const T& value)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    Resize(count, value);
                }

                /**
                 * @brief Constructeur depuis NkInitializerList — syntaxe {a, b, c}
                 * @param init Liste d'initialisation contenant les éléments
                 * @ingroup VectorConstructors
                 *
                 * Permet la construction avec liste d'initialisation :
                 *   NkVector<int> v = {1, 2, 3, 4};
                 *
                 * Réserve d'abord la capacité exacte pour éviter les réallocations,
                 * puis insère chaque élément via PushBack.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Compatible avec C++11 initializer list syntax
                 *
                 * @example
                 * @code
                 * auto coords = NkVector<float>{1.0f, 2.0f, 3.0f};
                 * auto names = NkVector<const char*>{"Alice", "Bob", "Charlie"};
                 * @endcode
                 */
                NkVector(NkInitializerList<T> init)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    Reserve(init.Size());
                    for (auto it = init.Begin(); it != init.End(); ++it) {
                        PushBack(*it);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list — interop STL
                 * @param init Liste d'initialisation STL standard
                 * @ingroup VectorConstructors
                 *
                 * Facilité d'interopérabilité avec du code utilisant std::initializer_list.
                 * Fonctionne comme le constructeur NkInitializerList mais accepte
                 * le type standard de la STL pour une migration progressive.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Nécessite <initializer_list> du compilateur (pas de dépendance STL runtime)
                 */
                NkVector(std::initializer_list<T> init)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    Reserve(init.size());
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Constructeur depuis une valeur unique — vecteur de taille 1
                 * @param value Valeur unique à stocker dans le vecteur
                 * @ingroup VectorConstructors
                 *
                 * Crée un vecteur contenant exactement un élément : la valeur fournie.
                 * Utile pour les fonctions retournant des collections optionnelles
                 * ou pour initialiser rapidement un vecteur mono-élément.
                 *
                 * @note Complexité : O(1) — allocation unique + construction
                 * @note Réserve exactement 1 élément de capacité initiale
                 *
                 * @example
                 * @code
                 * NkVector<int> singleton(42);  // Vecteur contenant {42}
                 * @endcode
                 */
                NkVector(const T& value)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    Reserve(1);
                    PushBack(value);
                }

                /**
                 * @brief Constructeur de copie — duplication profonde d'un vecteur
                 * @param other Vecteur source à copier
                 * @ingroup VectorConstructors
                 *
                 * Crée une copie indépendante du vecteur 'other' :
                 *  - Alloue un nouveau buffer de capacité suffisante
                 *  - Copie chaque élément via son constructeur de copie
                 *  - Utilise le même type d'allocateur que 'other'
                 *
                 * @note Complexité : O(n) pour la copie des éléments
                 * @note Requiert que T soit CopyConstructible
                 * @note Les modifications sur la copie n'affectent pas l'original
                 *
                 * @example
                 * @code
                 * NkVector<int> original = {1, 2, 3};
                 * NkVector<int> copy = original;  // Copie indépendante
                 * copy.PushBack(4);               // original reste {1,2,3}
                 * @endcode
                 */
                NkVector(const NkVector& other)
                    : mData(nullptr)
                    , mSize(0)
                    , mCapacity(0)
                    , mAllocator(other.mAllocator) {
                    Reserve(other.mSize);
                    for (SizeType i = 0; i < other.mSize; ++i) {
                        PushBack(other.mData[i]);
                    }
                }

                /**
                 * @brief Constructeur de déplacement — transfert de propriété efficace
                 * @param other Vecteur source dont transférer le contenu
                 * @ingroup VectorConstructors
                 *
                 * Transfère la propriété du buffer interne de 'other' vers ce vecteur :
                 *  - Copie les pointeurs et métadonnées (O(1))
                 *  - Réinitialise 'other' à l'état vide (nullptr, size=0, capacity=0)
                 *  - Évite toute allocation ou copie d'éléments
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après le move
                 *
                 * @example
                 * @code
                 * NkVector<int> source = MakeLargeVector();
                 * NkVector<int> dest = NKENTSEU_MOVE(source);  // Transfert O(1)
                 * // source est maintenant vide, dest contient les données
                 * @endcode
                 */
                NkVector(NkVector&& other) NKENTSEU_NOEXCEPT
                    : mData(other.mData)
                    , mSize(other.mSize)
                    , mCapacity(other.mCapacity)
                    , mAllocator(other.mAllocator) {
                    other.mData = nullptr;
                    other.mSize = 0;
                    other.mCapacity = 0;
                }

                /**
                 * @brief Destructeur — libération propre des ressources
                 * @ingroup VectorConstructors
                 *
                 * Libère toutes les ressources gérées par ce vecteur :
                 *  1. Détruit explicitement chaque élément stocké (appel destructeur)
                 *  2. Libère le buffer mémoire via l'allocateur associé
                 *  3. Réinitialise les pointeurs à nullptr pour sécurité
                 *
                 * @note noexcept : ne lève jamais d'exception (destructor safety)
                 * @note Complexité : O(n) pour la destruction des éléments + O(1) deallocation
                 * @note Garantit l'absence de fuites mémoire même en cas d'exception pendant construction
                 */
                ~NkVector() {
                    Clear();
                    if (mData) {
                        mAllocator->Deallocate(mData);
                    }
                }

                /**
                 * @brief Inverse l'ordre des éléments dans le vecteur in-place
                 * @ingroup VectorModifiers
                 *
                 * Échange symétriquement les éléments : premier avec dernier,
                 * deuxième avec avant-dernier, etc., jusqu'au milieu.
                 *
                 * @note Complexité : O(n/2) = O(n) échanges
                 * @note Utilise traits::NkSwap pour échange efficace (move semantics si disponible)
                 * @note Modifie le vecteur en place — pas d'allocation supplémentaire
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3, 4, 5};
                 * v.Reverse();  // v contient maintenant {5, 4, 3, 2, 1}
                 * @endcode
                 */
                void Reverse() {
                    if (mSize <= 1) {
                        return;
                    }
                    for (SizeType i = 0; i < mSize / 2; ++i) {
                        traits::NkSwap(mData[i], mData[mSize - 1 - i]);
                    }
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs d'affectation
                // -----------------------------------------------------------------
                /**
                 * @brief Remplace le contenu par 'count' copies de 'value'
                 * @param value Valeur à copier dans chaque nouvel élément
                 * @param count Nombre d'éléments à créer
                 * @ingroup VectorAssignment
                 *
                 * Équivalent à Clear() suivi de Resize(count, value), mais
                 * potentiellement plus efficace car peut réutiliser le buffer
                 * existant si la capacité est suffisante.
                 *
                 * @note Complexité : O(n) pour l'initialisation des nouveaux éléments
                 * @note Libère les éléments existants avant d'en créer de nouveaux
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v.Assign(5, 0);  // v contient maintenant {0,0,0,0,0}
                 * @endcode
                 */
                void Assign(const T& value, SizeType count) {
                    Clear();
                    Reserve(count);
                    for (SizeType i = 0; i < count; ++i) {
                        PushBack(value);
                    }
                }

                /**
                 * @brief Remplace le contenu par copie d'un tableau brut
                 * @param values Pointeur vers le tableau source des données
                 * @param count Nombre d'éléments à copier depuis le tableau
                 * @ingroup VectorAssignment
                 *
                 * Copie séquentiellement chaque élément du tableau [values, values+count)
                 * dans ce vecteur, après avoir vidé le contenu précédent.
                 *
                 * @note Complexité : O(n) pour les copies successives
                 * @note Requiert que T soit CopyConstructible
                 * @note Utile pour interop avec des APIs C ou buffers bruts
                 *
                 * @example
                 * @code
                 * int raw[] = {10, 20, 30};
                 * NkVector<int> v;
                 * v.Assign(raw, 3);  // v contient {10, 20, 30}
                 * @endcode
                 */
                void Assign(const T* values, SizeType count) {
                    Clear();
                    Reserve(count);
                    for (SizeType i = 0; i < count; ++i) {
                        PushBack(values[i]);
                    }
                }

                /**
                 * @brief Remplace le contenu depuis une NkInitializerList
                 * @param init Liste d'initialisation contenant les nouvelles valeurs
                 * @ingroup VectorAssignment
                 *
                 * Équivalent à Clear() suivi de construction depuis init,
                 * avec réservation préalable de capacité pour éviter réallocations.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Compatible avec syntaxe v = {a, b, c} via operator= surchargé
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v.Assign({4, 5, 6});  // v contient maintenant {4, 5, 6}
                 * @endcode
                 */
                void Assign(NkInitializerList<T> init) {
                    Clear();
                    Reserve(init.Size());
                    for (auto& val : init) {
                        PushBack(val);
                    }
                }

                /**
                 * @brief Remplace le contenu par copie d'un autre vecteur
                 * @param other Vecteur source dont copier le contenu
                 * @ingroup VectorAssignment
                 *
                 * Copie profonde du contenu de 'other' dans ce vecteur :
                 *  - Vide le contenu actuel via Clear()
                 *  - Réserve la capacité nécessaire
                 *  - Copie chaque élément via PushBack (constructeur de copie)
                 *
                 * @note Complexité : O(n) pour la copie des éléments
                 * @note Gère l'auto-affectation (this == &other) sans effet
                 *
                 * @example
                 * @code
                 * NkVector<int> a = {1, 2}, b = {3, 4};
                 * a.Assign(b);  // a contient maintenant {3, 4}
                 * @endcode
                 */
                void Assign(const NkVector& other) {
                    if (this != &other) {
                        Clear();
                        Reserve(other.mSize);
                        for (SizeType i = 0; i < other.mSize; ++i) {
                            PushBack(other.mData[i]);
                        }
                    }
                }

                /**
                 * @brief Opérateur d'affectation par copie — syntaxe v1 = v2
                 * @param other Vecteur source à copier
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup VectorAssignment
                 *
                 * Implémente la sémantique de copie via Assign(other) avec
                 * retour chaînable pour compatibilité avec les expressions C++.
                 *
                 * @note Complexité : O(n) pour la copie des éléments
                 * @note Gère l'auto-affectation sans effet secondaire
                 *
                 * @example
                 * @code
                 * NkVector<int> a = {1, 2}, b, c;
                 * b = a;        // b contient {1, 2}
                 * c = b = a;    // Chaînage : c et b contiennent {1, 2}
                 * @endcode
                 */
                NkVector& operator=(const NkVector& other) {
                    if (this != &other) {
                        Clear();
                        Reserve(other.mSize);
                        for (SizeType i = 0; i < other.mSize; ++i) {
                            PushBack(other.mData[i]);
                        }
                    }
                    return *this;
                }
                
                /**
                 * @brief Opérateur d'affectation par déplacement — transfert efficace
                 * @param other Vecteur source dont transférer le contenu
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup VectorAssignment
                 *
                 * Transfère la propriété du buffer de 'other' vers ce vecteur :
                 *  - Libère l'ancien buffer de ce vecteur si présent
                 *  - Adopte le buffer, size, capacity et allocateur de 'other'
                 *  - Réinitialise 'other' à l'état vide
                 *
                 * @note Complexité : O(1) — transfert de pointeurs uniquement
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note 'other' est laissé dans un état valide mais vide après l'opération
                 *
                 * @example
                 * @code
                 * NkVector<int> temp = ComputeExpensiveResult();
                 * result = NKENTSEU_MOVE(temp);  // Transfert O(1), pas de copie
                 * // temp est maintenant vide, result contient les données
                 * @endcode
                 */
                NkVector& operator=(NkVector&& other) NKENTSEU_NOEXCEPT {
                    if (this != &other) {
                        Clear();
                        if (mData) {
                            mAllocator->Deallocate(mData);
                        }
                        mData = other.mData;
                        mSize = other.mSize;
                        mCapacity = other.mCapacity;
                        mAllocator = other.mAllocator;
                        other.mData = nullptr;
                        other.mSize = 0;
                        other.mCapacity = 0;
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Liste d'initialisation contenant les nouvelles valeurs
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup VectorAssignment
                 *
                 * Permet la syntaxe v = {a, b, c} pour remplacer le contenu
                 * du vecteur par une nouvelle liste d'éléments.
                 *
                 * @note Complexité : O(n) pour l'insertion des nouveaux éléments
                 * @note Vide d'abord le contenu actuel via Clear()
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v = {4, 5, 6};  // v contient maintenant {4, 5, 6}
                 * @endcode
                 */
                NkVector& operator=(NkInitializerList<T> init) {
                    Clear();
                    Reserve(init.Size());
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list
                 * @param init Liste d'initialisation STL standard
                 * @return Référence vers *this pour chaînage d'opérations
                 * @ingroup VectorAssignment
                 *
                 * Interopérabilité avec code utilisant std::initializer_list.
                 * Fonctionne comme l'opérateur NkInitializerList mais accepte
                 * le type standard pour migration progressive depuis la STL.
                 *
                 * @note Complexité : O(n) pour l'insertion des éléments
                 * @note Nécessite <initializer_list> du compilateur uniquement
                 */
                NkVector& operator=(std::initializer_list<T> init) {
                    Clear();
                    Reserve(init.size());
                    for (auto& val : init) {
                        PushBack(val);
                    }
                    return *this;
                }

                // -----------------------------------------------------------------
                // Sous-section : Accès aux éléments
                // -----------------------------------------------------------------
                /**
                 * @brief Accès vérifié à un élément par index — version mutable
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence mutable vers l'élément à l'index spécifié
                 * @ingroup VectorAccess
                 *
                 * Vérifie que l'index est dans les bornes [0, Size()) avant
                 * d'accéder à l'élément. En cas d'index invalide :
                 *  - Avec NKENTSEU_USE_EXCEPTIONS : lève NkVectorOutOfRangeException
                 *  - Sans exceptions : NKENTSEU_ASSERT_MSG en debug, comportement indéfini en release
                 *
                 * @note Complexité : O(1) — accès direct via pointeur après vérification
                 * @note Préférer operator[] quand la validité de l'index est garantie
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {10, 20, 30};
                 * int x = v.At(1);      // x = 20 (OK)
                 * int y = v.At(5);      // Erreur : index hors bornes
                 * @endcode
                 */
                Reference At(SizeType index) {
                    if (index >= mSize) {
                        NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, mSize);
                    }
                    return mData[index];
                }

                /**
                 * @brief Accès vérifié à un élément par index — version constante
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence constante vers l'élément à l'index spécifié
                 * @ingroup VectorAccess
                 *
                 * Version const de At() pour les vecteurs en lecture seule.
                 * Même comportement de vérification des bornes que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct après vérification
                 * @note Permet d'accéder aux éléments d'un const NkVector&
                 *
                 * @example
                 * @code
                 * void PrintElement(const NkVector<int>& v, SizeType i) {
                 *     printf("%d\n", v.At(i));  // Accès const vérifié
                 * }
                 * @endcode
                 */
                ConstReference At(SizeType index) const {
                    if (index >= mSize) {
                        NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, mSize);
                    }
                    return mData[index];
                }

                /**
                 * @brief Accès non vérifié à un élément par index — version mutable
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence mutable vers l'élément à l'index spécifié
                 * @ingroup VectorAccess
                 *
                 * Accès direct sans vérification de bornes pour performance maximale.
                 * Utilise NKENTSEU_ASSERT en mode debug pour détecter les erreurs,
                 * mais aucun check en mode release (comportement indéfini si index invalide).
                 *
                 * @note Complexité : O(1) — accès mémoire direct sans branchement
                 * @note À utiliser uniquement quand la validité de l'index est garantie
                 * @note Préférer At() quand la sécurité prime sur la performance
                 *
                 * @example
                 * @code
                 * for (SizeType i = 0; i < v.Size(); ++i) {
                 *     v[i] *= 2;  // OK : i est toujours valide dans la boucle
                 * }
                 * @endcode
                 */
                Reference operator[](SizeType index) {
                    NKENTSEU_ASSERT(index < mSize);
                    return mData[index];
                }

                /**
                 * @brief Accès non vérifié à un élément par index — version constante
                 * @param index Index de l'élément à accéder (0-based)
                 * @return Référence constante vers l'élément à l'index spécifié
                 * @ingroup VectorAccess
                 *
                 * Version const de operator[] pour les vecteurs en lecture seule.
                 * Même sémantique de performance/sécurité que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct sans vérification runtime
                 * @note Compatible avec les algorithmes génériques attendant operator[]
                 *
                 * @example
                 * @code
                 * int Sum(const NkVector<int>& v) {
                 *     int total = 0;
                 *     for (SizeType i = 0; i < v.Size(); ++i) {
                 *         total += v[i];  // Accès const rapide
                 *     }
                 *     return total;
                 * }
                 * @endcode
                 */
                ConstReference operator[](SizeType index) const {
                    NKENTSEU_ASSERT(index < mSize);
                    return mData[index];
                }

                /**
                 * @brief Accès au premier élément — version mutable
                 * @return Référence mutable vers le premier élément (index 0)
                 * @ingroup VectorAccess
                 *
                 * Retourne une référence vers mData[0] avec assertion debug
                 * si le vecteur est vide. Équivalent à Front() = At(0) = [0]
                 * mais avec sémantique explicite "premier élément".
                 *
                 * @note Complexité : O(1) — accès direct au premier élément
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si vecteur vide
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v.Front() = 10;  // v contient maintenant {10, 2, 3}
                 * @endcode
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(mSize > 0);
                    return mData[0];
                }

                /**
                 * @brief Accès au premier élément — version constante
                 * @return Référence constante vers le premier élément (index 0)
                 * @ingroup VectorAccess
                 *
                 * Version const de Front() pour les vecteurs en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct
                 * @note Utile pour lire la première valeur sans modification
                 *
                 * @example
                 * @code
                 * int GetFirst(const NkVector<int>& v) {
                 *     return v.Front();  // Lecture du premier élément
                 * }
                 * @endcode
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(mSize > 0);
                    return mData[0];
                }

                /**
                 * @brief Accès au dernier élément — version mutable
                 * @return Référence mutable vers le dernier élément (index Size()-1)
                 * @ingroup VectorAccess
                 *
                 * Retourne une référence vers mData[mSize-1] avec assertion debug
                 * si le vecteur est vide. Équivalent à Back() = At(Size()-1)
                 * mais avec sémantique explicite "dernier élément".
                 *
                 * @note Complexité : O(1) — calcul d'index simple + accès direct
                 * @note Assertion en debug si Empty() == true
                 * @note Comportement indéfini en release si vecteur vide
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v.Back() = 4;  // v contient maintenant {1, 2, 4}
                 * @endcode
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(mSize > 0);
                    return mData[mSize - 1];
                }

                /**
                 * @brief Accès au dernier élément — version constante
                 * @return Référence constante vers le dernier élément (index Size()-1)
                 * @ingroup VectorAccess
                 *
                 * Version const de Back() pour les vecteurs en lecture seule.
                 * Même sémantique de vérification que la version mutable.
                 *
                 * @note Complexité : O(1) — accès direct au dernier élément
                 * @note Très utile pour les algorithmes de type "stack" (LIFO)
                 *
                 * @example
                 * @code
                 * int PeekTop(const NkVector<int>& stack) {
                 *     return stack.Back();  // Lire sans retirer
                 * }
                 * @endcode
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(mSize > 0);
                    return mData[mSize - 1];
                }

                /**
                 * @brief Accès au pointeur brut sous-jacent — version mutable
                 * @return Pointeur mutable vers le premier élément (ou nullptr si vide)
                 * @ingroup VectorAccess
                 *
                 * Retourne le pointeur interne mData pour interop avec des APIs
                 * attendant un tableau brut (T*). Permet un accès direct O(1)
                 * sans surcoût d'indirection.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Le pointeur reste valide jusqu'à la prochaine modification du vecteur
                 * @note Peut être nullptr si Capacity() == 0 (vecteur jamais alloué)
                 *
                 * @example
                 * @code
                 * NkVector<float> vertices = {1.0f, 2.0f, 3.0f};
                 * glBufferData(GL_ARRAY_BUFFER, vertices.Size()*sizeof(float),
                 *              vertices.Data(), GL_STATIC_DRAW);  // Interop OpenGL
                 * @endcode
                 */
                Pointer Data() NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Accès au pointeur brut sous-jacent — version constante
                 * @return Pointeur constant vers le premier élément (ou nullptr si vide)
                 * @ingroup VectorAccess
                 *
                 * Version const de Data() pour les vecteurs en lecture seule.
                 * Retourne const T* pour empêcher la modification via le pointeur.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Idéal pour les fonctions de lecture seule interopérant avec C APIs
                 *
                 * @example
                 * @code
                 * void ProcessReadonly(const NkVector<int>& data) {
                 *     const int* raw = data.Data();
                 *     for (SizeType i = 0; i < data.Size(); ++i) {
                 *         Compute(raw[i]);  // Lecture seule via pointeur brut
                 *     }
                 * }
                 * @endcode
                 */
                ConstPointer Data() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                // -----------------------------------------------------------------
                // Sous-section : Itérateurs — Style PascalCase (standard interne)
                // -----------------------------------------------------------------
                /**
                 * @brief Itérateur mutable vers le début du vecteur
                 * @return Iterator pointant sur le premier élément (ou End() si vide)
                 * @ingroup VectorIterators
                 *
                 * Méthode standard interne du framework (PascalCase). Retourne
                 * un pointeur brut T* pour un accès O(1) sans indirection.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Compatible avec les algorithmes génériques attendant des itérateurs
                 *
                 * @example
                 * @code
                 * for (auto it = v.Begin(); it != v.End(); ++it) {
                 *     *it *= 2;  // Modification via itérateur mutable
                 * }
                 * @endcode
                 */
                Iterator Begin() NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Itérateur constant vers le début du vecteur
                 * @return ConstIterator pointant sur le premier élément (ou End() si vide)
                 * @ingroup VectorIterators
                 *
                 * Version const de Begin() pour les vecteurs en lecture seule.
                 * Retourne const T* pour empêcher la modification via l'itérateur.
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Permet de parcourir un const NkVector& sans lever la const
                 *
                 * @example
                 * @code
                 * void PrintAll(const NkVector<int>& v) {
                 *     for (auto it = v.Begin(); it != v.End(); ++it) {
                 *         printf("%d ", *it);  // Lecture seule via itérateur const
                 *     }
                 * }
                 * @endcode
                 */
                ConstIterator Begin() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Itérateur constant explicite vers le début (compatibilité STL)
                 * @return ConstIterator pointant sur le premier élément
                 * @ingroup VectorIterators
                 *
                 * Alias vers Begin() const pour respecter la convention STL
                 * où cbegin() retourne toujours un const_iterator, même sur
                 * un conteneur non-const.
                 *
                 * @note Complexité : O(1) — délégation vers Begin() const
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour les templates génériques attendant cbegin()
                 *
                 * @example
                 * @code
                 * template<typename Container>
                 * void GenericPrint(const Container& c) {
                 *     for (auto it = c.cbegin(); it != c.cend(); ++it) {
                 *         std::cout << *it << " ";
                 *     }
                 * }
                 * @endcode
                 */
                ConstIterator CBegin() const NKENTSEU_NOEXCEPT {
                    return mData;
                }

                /**
                 * @brief Itérateur mutable vers la fin du vecteur (one-past-last)
                 * @return Iterator pointant après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Retourne un pointeur vers mData + mSize, représentant la
                 * position "juste après" le dernier élément valide. Utilisé
                 * comme borne supérieure exclusive dans les boucles d'itération.
                 *
                 * @note Complexité : O(1) — calcul d'adresse simple
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Ne jamais déréférencer : c'est une sentinelle, pas un élément valide
                 *
                 * @example
                 * @code
                 * // Boucle classique avec itérateurs
                 * for (auto it = v.Begin(); it != v.End(); ++it) {
                 *     Process(*it);
                 * }
                 * @endcode
                 */
                Iterator End() NKENTSEU_NOEXCEPT {
                    return mData + mSize;
                }

                /**
                 * @brief Itérateur constant vers la fin du vecteur (one-past-last)
                 * @return ConstIterator pointant après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Version const de End() pour les vecteurs en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 *
                 * @note Complexité : O(1) — calcul d'adresse simple
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Compatible avec les algorithmes STL-style sur const containers
                 *
                 * @example
                 * @code
                 * SizeType CountEven(const NkVector<int>& v) {
                 *     SizeType count = 0;
                 *     for (auto it = v.Begin(); it != v.End(); ++it) {
                 *         if (*it % 2 == 0) ++count;
                 *     }
                 *     return count;
                 * }
                 * @endcode
                 */
                ConstIterator End() const NKENTSEU_NOEXCEPT {
                    return mData + mSize;
                }

                /**
                 * @brief Itérateur constant explicite vers la fin (compatibilité STL)
                 * @return ConstIterator pointant après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Alias vers End() const pour respecter la convention STL
                 * où cend() retourne toujours un const_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers End() const
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour les templates génériques attendant cend()
                 */
                ConstIterator CEnd() const NKENTSEU_NOEXCEPT {
                    return mData + mSize;
                }

                /**
                 * @brief Itérateur inversé mutable vers le dernier élément
                 * @return ReverseIterator pointant sur le dernier élément (ou REnd() si vide)
                 * @ingroup VectorIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec End(),
                 * permettant de parcourir le vecteur en sens inverse. L'opération
                 * ++ sur un ReverseIterator avance vers le début du vecteur.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Nécessite que l'itérateur sous-jacent supporte l'opération --
                 *
                 * @example
                 * @code
                 * // Parcours à l'envers : dernier -> premier
                 * for (auto rit = v.RBegin(); rit != v.REnd(); ++rit) {
                 *     printf("%d ", *rit);
                 * }
                 * @endcode
                 */
                ReverseIterator RBegin() NKENTSEU_NOEXCEPT {
                    return ReverseIterator(End());
                }

                /**
                 * @brief Itérateur inversé constant vers le dernier élément
                 * @return ConstReverseIterator pointant sur le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Version const de RBegin() pour les vecteurs en lecture seule.
                 * Retourne un adaptateur const pour empêcher la modification
                 * des éléments via l'itérateur inversé.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Permet le parcours inversé d'un const NkVector&
                 *
                 * @example
                 * @code
                 * void PrintReverse(const NkVector<int>& v) {
                 *     for (auto rit = v.RBegin(); rit != v.REnd(); ++rit) {
                 *         printf("%d ", *rit);  // Lecture seule en ordre inverse
                 *     }
                 * }
                 * @endcode
                 */
                ConstReverseIterator RBegin() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(End());
                }

                /**
                 * @brief Itérateur inversé constant explicite (compatibilité STL)
                 * @return ConstReverseIterator pointant sur le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Alias vers RBegin() const pour respecter la convention STL
                 * où crbegin() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers RBegin() const
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour les templates génériques attendant crbegin()
                 */
                ConstReverseIterator CRBegin() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(End());
                }

                /**
                 * @brief Itérateur inversé mutable vers avant le premier élément
                 * @return ReverseIterator sentinelle marquant la fin du parcours inversé
                 * @ingroup VectorIterators
                 *
                 * Retourne un adaptateur NkReverseIterator initialisé avec Begin(),
                 * représentant la position "juste avant" le premier élément dans
                 * l'ordre inversé. Utilisé comme borne supérieure exclusive pour
                 * les boucles de parcours inversé.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Ne jamais déréférencer : c'est une sentinelle, pas un élément valide
                 *
                 * @example
                 * @code
                 * // Boucle inversée avec sentinelle REnd()
                 * for (auto rit = v.RBegin(); rit != v.REnd(); ++rit) {
                 *     Process(*rit);  // Traite du dernier au premier
                 * }
                 * @endcode
                 */
                ReverseIterator REnd() NKENTSEU_NOEXCEPT {
                    return ReverseIterator(Begin());
                }

                /**
                 * @brief Itérateur inversé constant vers avant le premier élément
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup VectorIterators
                 *
                 * Version const de REnd() pour les vecteurs en lecture seule.
                 * Même sémantique de sentinelle exclusive que la version mutable.
                 *
                 * @note Complexité : O(1) — construction d'adaptateur léger
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Compatible avec les algorithmes inversés sur const containers
                 */
                ConstReverseIterator REnd() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(Begin());
                }

                /**
                 * @brief Itérateur inversé constant explicite (compatibilité STL)
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup VectorIterators
                 *
                 * Alias vers REnd() const pour respecter la convention STL
                 * où crend() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation vers REnd() const
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour les templates génériques attendant crend()
                 */
                ConstReverseIterator CREnd() const NKENTSEU_NOEXCEPT {
                    return ConstReverseIterator(Begin());
                }

                // -----------------------------------------------------------------
                // Sous-section : Itérateurs — Style minuscule (compatibilité STL)
                // -----------------------------------------------------------------
                /**
                 * @brief Alias vers Begin() pour compatibilité range-based for
                 * @return Iterator pointant sur le premier élément
                 * @ingroup VectorIterators
                 *
                 * Simple délégation vers Begin() pour permettre l'usage du
                 * range-based for C++11 : for (auto& x : vector) { ... }
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Aucune logique dupliquée : appel direct à Begin()
                 *
                 * @example
                 * @code
                 * // Range-based for utilisant begin()/end() implicitement
                 * for (auto& value : myVector) {
                 *     value *= 2;  // Modification in-place
                 * }
                 * @endcode
                 */
                Iterator begin() NKENTSEU_NOEXCEPT {
                    return Begin();
                }

                /**
                 * @brief Alias vers Begin() const pour compatibilité range-based for const
                 * @return ConstIterator pointant sur le premier élément
                 * @ingroup VectorIterators
                 *
                 * Version const de begin() pour les vecteurs en lecture seule.
                 * Permet le range-based for sur const NkVector&.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Essentiel pour les fonctions prenant const NkVector&
                 */
                ConstIterator begin() const NKENTSEU_NOEXCEPT {
                    return Begin();
                }

                /**
                 * @brief Alias vers CBegin() pour compatibilité STL cbegin()
                 * @return ConstIterator pointant sur le premier élément
                 * @ingroup VectorIterators
                 *
                 * Délégation vers CBegin() pour respecter la convention STL
                 * où cbegin() retourne toujours un const_iterator.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour les templates génériques STL-compatible
                 */
                ConstIterator cbegin() const NKENTSEU_NOEXCEPT {
                    return CBegin();
                }

                /**
                 * @brief Alias vers End() pour compatibilité range-based for
                 * @return Iterator sentinelle après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Simple délégation vers End() pour permettre l'usage du
                 * range-based for C++11 avec la borne de fin.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Aucune logique dupliquée : appel direct à End()
                 */
                Iterator end() NKENTSEU_NOEXCEPT {
                    return End();
                }

                /**
                 * @brief Alias vers End() const pour compatibilité range-based for const
                 * @return ConstIterator sentinelle après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Version const de end() pour les vecteurs en lecture seule.
                 * Permet le range-based for sur const NkVector&.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstIterator end() const NKENTSEU_NOEXCEPT {
                    return End();
                }

                /**
                 * @brief Alias vers CEnd() pour compatibilité STL cend()
                 * @return ConstIterator sentinelle après le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Délégation vers CEnd() pour respecter la convention STL
                 * où cend() retourne toujours un const_iterator.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstIterator cend() const NKENTSEU_NOEXCEPT {
                    return CEnd();
                }

                /**
                 * @brief Alias vers RBegin() pour compatibilité STL rbegin()
                 * @return ReverseIterator vers le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Délégation vers RBegin() pour permettre l'usage des
                 * reverse iterators avec la nomenclature STL minuscule.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ReverseIterator rbegin() NKENTSEU_NOEXCEPT {
                    return RBegin();
                }

                /**
                 * @brief Alias vers RBegin() const pour compatibilité STL rbegin() const
                 * @return ConstReverseIterator vers le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Version const de rbegin() pour les vecteurs en lecture seule.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstReverseIterator rbegin() const NKENTSEU_NOEXCEPT {
                    return RBegin();
                }

                /**
                 * @brief Alias vers CRBegin() pour compatibilité STL crbegin()
                 * @return ConstReverseIterator vers le dernier élément
                 * @ingroup VectorIterators
                 *
                 * Délégation vers CRBegin() pour respecter la convention STL
                 * où crbegin() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstReverseIterator crbegin() const NKENTSEU_NOEXCEPT {
                    return CRBegin();
                }

                /**
                 * @brief Alias vers REnd() pour compatibilité STL rend()
                 * @return ReverseIterator sentinelle pour parcours inversé
                 * @ingroup VectorIterators
                 *
                 * Délégation vers REnd() pour permettre l'usage des
                 * reverse iterators avec la nomenclature STL minuscule.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ReverseIterator rend() NKENTSEU_NOEXCEPT {
                    return REnd();
                }

                /**
                 * @brief Alias vers REnd() const pour compatibilité STL rend() const
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup VectorIterators
                 *
                 * Version const de rend() pour les vecteurs en lecture seule.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstReverseIterator rend() const NKENTSEU_NOEXCEPT {
                    return REnd();
                }

                /**
                 * @brief Alias vers CREnd() pour compatibilité STL crend()
                 * @return ConstReverseIterator sentinelle pour parcours inversé const
                 * @ingroup VectorIterators
                 *
                 * Délégation vers CREnd() pour respecter la convention STL
                 * où crend() retourne toujours un const_reverse_iterator.
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                ConstReverseIterator crend() const NKENTSEU_NOEXCEPT {
                    return CREnd();
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes de capacité
                // -----------------------------------------------------------------
                /**
                 * @brief Vérifie si le vecteur est vide — style PascalCase
                 * @return true si Size() == 0, false sinon
                 * @ingroup VectorCapacity
                 *
                 * Méthode standard interne du framework (PascalCase) pour
                 * tester si le vecteur ne contient aucun élément.
                 *
                 * @note Complexité : O(1) — comparaison simple de membre
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette version dans le code applicatif NKContainers
                 *
                 * @example
                 * @code
                 * if (v.IsEmpty()) {
                 *     v.PushBack(defaultValue);  // Initialisation lazy
                 * }
                 * @endcode
                 */
                bool IsEmpty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Vérifie si le vecteur est vide — alias de IsEmpty()
                 * @return true si Size() == 0, false sinon
                 * @ingroup VectorCapacity
                 *
                 * Alias vers IsEmpty() pour compatibilité avec du code
                 * utilisant la nomenclature "Empty()" sans préfixe "Is".
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Aucune logique dupliquée : appel direct à IsEmpty()
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Vérifie si le vecteur est vide — compatibilité STL empty()
                 * @return true si Size() == 0, false sinon
                 * @ingroup VectorCapacity
                 *
                 * Alias vers Empty() pour compatibilité avec la STL et les
                 * algorithmes génériques attendant la méthode empty().
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Essentiel pour les templates génériques STL-compatible
                 *
                 * @example
                 * @code
                 * template<typename Container>
                 * void ProcessIfNotEmpty(const Container& c) {
                 *     if (!c.empty()) {  // Fonctionne avec NkVector grâce à cet alias
                 *         DoSomething(c);
                 *     }
                 * }
                 * @endcode
                 */
                bool empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement stockés
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup VectorCapacity
                 *
                 * Méthode standard interne (PascalCase) pour obtenir le nombre
                 * d'éléments valides dans le vecteur. Toujours inférieur ou
                 * égal à Capacity().
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Préférer cette version dans le code applicatif NKContainers
                 *
                 * @example
                 * @code
                 * for (SizeType i = 0; i < v.Size(); ++i) {
                 *     Process(v[i]);  // Boucle indexée classique
                 * }
                 * @endcode
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Retourne le nombre d'éléments — alias de Size() compatibilité STL
                 * @return Valeur de type SizeType représentant la taille courante
                 * @ingroup VectorCapacity
                 *
                 * Alias vers Size() pour compatibilité avec la STL et les
                 * algorithmes génériques attendant la méthode size().
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Essentiel pour les templates génériques STL-compatible
                 */
                SizeType size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Retourne la capacité actuelle du buffer alloué
                 * @return Valeur de type SizeType représentant la capacité en éléments
                 * @ingroup VectorCapacity
                 *
                 * Nombre maximal d'éléments pouvant être stockés sans
                 * réallocation mémoire. Toujours supérieur ou égal à Size().
                 *
                 * @note Complexité : O(1) — retour de membre direct
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour optimiser les insertions massives via Reserve()
                 *
                 * @example
                 * @code
                 * if (v.Capacity() < expectedSize) {
                 *     v.Reserve(expectedSize);  // Évite les réallocations multiples
                 * }
                 * @endcode
                 */
                SizeType Capacity() const NKENTSEU_NOEXCEPT {
                    return mCapacity;
                }

                /**
                 * @brief Retourne la capacité — alias de Capacity() compatibilité STL
                 * @return Valeur de type SizeType représentant la capacité en éléments
                 * @ingroup VectorCapacity
                 *
                 * Alias vers Capacity() pour compatibilité avec la STL et les
                 * algorithmes génériques attendant la méthode capacity().
                 *
                 * @note Complexité : O(1) — délégation inline
                 * @note noexcept : garantie de ne pas lever d'exception
                 */
                SizeType capacity() const NKENTSEU_NOEXCEPT {
                    return mCapacity;
                }

                /**
                 * @brief Retourne la taille maximale théorique représentable
                 * @return Valeur de type SizeType représentant la limite supérieure
                 * @ingroup VectorCapacity
                 *
                 * Calcule la taille maximale qu'un vecteur de type T pourrait
                 * théoriquement atteindre, basée sur la limite de SizeType et
                 * la taille de T. En pratique, la mémoire disponible est la
                 * vraie limite.
                 *
                 * @note Complexité : O(1) — calcul arithmétique simple
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Utile pour valider les requêtes de Resize/Reserve extrêmes
                 *
                 * @example
                 * @code
                 * if (requestedSize > v.MaxSize()) {
                 *     NKENTSEU_LOG_ERROR("Size request exceeds theoretical maximum");
                 *     return false;
                 * }
                 * @endcode
                 */
                SizeType MaxSize() const NKENTSEU_NOEXCEPT {
                    return static_cast<SizeType>(-1) / sizeof(T);
                }

                /**
                 * @brief Réserve de la capacité pour éviter les réallocations futures
                 * @param newCapacity Capacité minimale souhaitée en nombre d'éléments
                 * @ingroup VectorCapacity
                 *
                 * Si newCapacity > Capacity(), alloue un nouveau buffer de taille
                 * suffisante et transfère les éléments existants. Sinon, no-op.
                 *
                 * @note Complexité : O(n) si réallocation nécessaire, O(1) sinon
                 * @note Ne modifie pas Size() : les éléments existants sont préservés
                 * @note Utile avant une série d'insertions connues à l'avance
                 *
                 * @example
                 * @code
                 * NkVector<int> v;
                 * v.Reserve(1000);  // Alloue pour 1000 éléments d'un coup
                 * for (int i = 0; i < 1000; ++i) {
                 *     v.PushBack(i);  // Aucune réallocation pendant la boucle
                 * }
                 * @endcode
                 */
                void Reserve(SizeType newCapacity) {
                    if (newCapacity > mCapacity) {
                        Reallocate(newCapacity);
                    }
                }

                /**
                 * @brief Réduit la capacité pour correspondre exactement à la taille
                 * @ingroup VectorCapacity
                 *
                 * Si Size() < Capacity(), réalloue un buffer de taille exacte
                 * pour libérer la mémoire excédentaire. Sinon, no-op.
                 *
                 * @note Complexité : O(n) si réallocation nécessaire, O(1) sinon
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Utile après des suppressions massives pour réduire l'empreinte mémoire
                 *
                 * @example
                 * @code
                 * v.Erase(v.Begin(), v.Begin() + 500);  // Supprime 500 éléments
                 * v.ShrinkToFit();  // Libère la mémoire désormais inutile
                 * @endcode
                 */
                void ShrinkToFit() {
                    if (mSize < mCapacity) {
                        Reallocate(mSize);
                    }
                }

                // -----------------------------------------------------------------
                // Sous-section : Modificateurs de contenu
                // -----------------------------------------------------------------
                /**
                 * @brief Détruit tous les éléments et remet la taille à zéro
                 * @ingroup VectorModifiers
                 *
                 * Appelle explicitement le destructeur de T pour chaque élément
                 * stocké, puis remet mSize à 0. Ne libère pas le buffer mémoire :
                 * Capacity() reste inchangée pour réutilisation future.
                 *
                 * @note Complexité : O(n) pour la destruction des éléments
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note Les itérateurs, pointeurs et références deviennent invalides
                 *
                 * @example
                 * @code
                 * NkVector<std::string> v = {"a", "b", "c"};
                 * v.Clear();  // Détruit les 3 strings, v.Size() == 0
                 * v.PushBack("new");  // Réutilise le buffer existant
                 * @endcode
                 */
                void Clear() NKENTSEU_NOEXCEPT {
                    if (mData) {
                        DestroyRange(mData, mData + mSize);
                    }
                    mSize = 0;
                }

                /**
                 * @brief Ajoute une copie de l'élément à la fin du vecteur
                 * @param value Valeur à copier et ajouter
                 * @ingroup VectorModifiers
                 *
                 * Si la capacité est insuffisante, déclenche une réallocation
                 * avec croissance géométrique (facteur ×2 par défaut). Sinon,
                 * construit l'élément directement à la position mSize.
                 *
                 * @note Complexité : O(1) amorti (O(n) occasionnellement pour réallocation)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Requiert que T soit CopyConstructible
                 *
                 * @example
                 * @code
                 * NkVector<int> v;
                 * v.PushBack(1);  // v = {1}
                 * v.PushBack(2);  // v = {1, 2}
                 * @endcode
                 */
                void PushBack(const T& value) {
                    if (mSize >= mCapacity) {
                        Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                    }
                    ConstructAt(mData + mSize, value);
                    ++mSize;
                }

                /**
                 * @brief Ajoute un élément par déplacement à la fin du vecteur
                 * @param value Valeur à déplacer et ajouter
                 * @ingroup VectorModifiers
                 *
                 * Version move semantics de PushBack pour éviter les copies
                 * inutiles des types lourds. Utilise traits::NkMove pour
                 * transférer la propriété des ressources si possible.
                 *
                 * @note Complexité : O(1) amorti (comme PushBack const&)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Requiert que T soit MoveConstructible
                 * @note 'value' est laissé dans un état valide mais indéterminé après l'appel
                 *
                 * @example
                 * @code
                 * std::string heavy = MakeHeavyString();
                 * v.PushBack(NKENTSEU_MOVE(heavy));  // Déplace au lieu de copier
                 * // heavy est maintenant vide ou dans un état valide indéterminé
                 * @endcode
                 */
                void PushBack(T&& value) {
                    if (mSize >= mCapacity) {
                        Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                    }
                    ConstructAt(mData + mSize, traits::NkMove(value));
                    ++mSize;
                }

                /**
                 * @brief Construit un élément in-place à la fin du vecteur
                 * @tparam Args Types des arguments de construction (déduits)
                 * @param args Arguments forwardés au constructeur de T
                 * @ingroup VectorModifiers
                 *
                 * Évite toute copie ou déplacement temporaire en construisant
                 * directement l'objet dans le buffer du vecteur via placement new
                 * et forwarding parfait des arguments.
                 *
                 * @note Complexité : O(1) amorti (comme PushBack)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Idéal pour les types coûteux à copier/move ou non copyable
                 *
                 * @example
                 * @code
                 * // Sans EmplaceBack : copie temporaire
                 * v.PushBack(std::string(1000, 'x'));
                 *
                 * // Avec EmplaceBack : construction directe, zéro copie
                 * v.EmplaceBack(1000, 'x');  // Construit string(1000, 'x') in-place
                 * @endcode
                 */
                template<typename... Args>
                void EmplaceBack(Args&&... args) {
                    if (mSize >= mCapacity) {
                        Reserve(mCapacity == 0 ? 1 : CalculateGrowth(mSize + 1));
                    }
                    ConstructAt(mData + mSize, traits::NkForward<Args>(args)...);
                    ++mSize;
                }

                /**
                 * @brief Supprime le dernier élément du vecteur
                 * @ingroup VectorModifiers
                 *
                 * Décrémente mSize puis appelle explicitement le destructeur
                 * de T sur l'élément désormais hors du vecteur. Ne libère pas
                 * la mémoire : Capacity() reste inchangée.
                 *
                 * @note Complexité : O(1) — destruction d'un seul élément
                 * @note Assertion en debug si Empty() == true (underflow protection)
                 * @note Comportement indéfini en release si vecteur vide
                 * @note Peut invalider les itérateurs pointant vers l'élément supprimé
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3};
                 * v.PopBack();  // v = {1, 2}
                 * v.PopBack();  // v = {1}
                 * @endcode
                 */
                void PopBack() {
                    NKENTSEU_ASSERT(mSize > 0);
                    --mSize;
                    mData[mSize].~T();
                }

                /**
                 * @brief Redimensionne le vecteur avec value-initialization des nouveaux éléments
                 * @param newSize Nouvelle taille cible en nombre d'éléments
                 * @ingroup VectorModifiers
                 *
                 * Si newSize > Size() : réserve si nécessaire, puis construit
                 * les éléments manquants via value-initialization (T()).
                 * Si newSize < Size() : détruit les éléments excédentaires.
                 *
                 * @note Complexité : O(|newSize - Size|) pour construction/destruction
                 * @note Peut invalider tous les itérateurs si réallocation nécessaire
                 * @note Utile pour pré-allouer et initialiser un vecteur de taille connue
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2};
                 * v.Resize(5);        // v = {1, 2, 0, 0, 0} (value-initialized)
                 * v.Resize(3);        // v = {1, 2, 0} (supprime les 2 derniers)
                 * @endcode
                 */
                void Resize(SizeType newSize) {
                    if (newSize > mSize) {
                        Reserve(newSize);
                        for (SizeType i = mSize; i < newSize; ++i) {
                            ConstructAt(mData + i);
                        }
                    }
                    else if (newSize < mSize) {
                        DestroyRange(mData + newSize, mData + mSize);
                    }
                    mSize = newSize;
                }

                /**
                 * @brief Redimensionne le vecteur avec copie d'une valeur de remplissage
                 * @param newSize Nouvelle taille cible en nombre d'éléments
                 * @param value Valeur à copier dans les nouveaux éléments si croissance
                 * @ingroup VectorModifiers
                 *
                 * Comme Resize(newSize) mais utilise 'value' au lieu de
                 * value-initialization pour les éléments ajoutés lors d'une
                 * croissance. Plus explicite quand on veut une valeur spécifique.
                 *
                 * @note Complexité : O(|newSize - Size|) pour construction/destruction
                 * @note Peut invalider tous les itérateurs si réallocation nécessaire
                 * @note Requiert que T soit CopyConstructible
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2};
                 * v.Resize(5, -1);    // v = {1, 2, -1, -1, -1}
                 * v.Resize(3, 999);   // v = {1, 2, -1} (les 2 derniers supprimés)
                 * @endcode
                 */
                void Resize(SizeType newSize, const T& value) {
                    if (newSize > mSize) {
                        Reserve(newSize);
                        for (SizeType i = mSize; i < newSize; ++i) {
                            ConstructAt(mData + i, value);
                        }
                    }
                    else if (newSize < mSize) {
                        DestroyRange(mData + newSize, mData + mSize);
                    }
                    mSize = newSize;
                }

                /**
                 * @brief Échange le contenu de ce vecteur avec un autre en O(1)
                 * @param other Autre vecteur avec lequel échanger le contenu
                 * @ingroup VectorModifiers
                 *
                 * Échange les pointeurs et métadonnées internes sans toucher
                 * aux éléments stockés : opération constante indépendante de
                 * la taille des vecteurs.
                 *
                 * @note Complexité : O(1) — échange de quelques pointeurs/entiers
                 * @note noexcept : garantie de ne pas lever d'exception
                 * @note N'invalide aucun itérateur : ils suivent leur vecteur d'origine
                 * @note Utile pour l'idiome copy-and-swap et les algorithmes de tri
                 *
                 * @example
                 * @code
                 * NkVector<int> a = {1, 2}, b = {3, 4, 5};
                 * a.Swap(b);  // a = {3,4,5}, b = {1,2} en O(1)
                 *
                 * // Idiom copy-and-swap pour assignment exception-safe
                 * NkVector& operator=(NkVector other) {  // other est une copie
                 *     Swap(other);  // Échange O(1) avec la copie
                 *     return *this; // other détruit libère l'ancien contenu
                 * }
                 * @endcode
                 */
                void Swap(NkVector& other) NKENTSEU_NOEXCEPT {
                    traits::NkSwap(mData, other.mData);
                    traits::NkSwap(mSize, other.mSize);
                    traits::NkSwap(mCapacity, other.mCapacity);
                    traits::NkSwap(mAllocator, other.mAllocator);
                }

                // -----------------------------------------------------------------
                // Sous-section : Insertion d'éléments à position arbitraire
                // -----------------------------------------------------------------
                /**
                 * @brief Insère une copie d'un élément à une position donnée
                 * @param pos Itérateur constant vers la position d'insertion
                 * @param value Valeur à copier et insérer
                 * @return Iterator pointant vers le nouvel élément inséré
                 * @ingroup VectorModifiers
                 *
                 * Décale tous les éléments de [pos, End()) d'une position vers
                 * la droite, puis construit une copie de 'value' à la position
                 * libérée. Si la capacité est insuffisante, déclenche une
                 * réallocation avant le décalage.
                 *
                 * @note Complexité : O(n) dans le pire cas (décalage de tous les éléments)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note Requiert que T soit CopyConstructible et MoveConstructible
                 * @note 'pos' doit être dans [Begin(), End()] (End() = insertion en fin)
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 3, 4};
                 * auto it = v.Insert(v.Begin() + 1, 2);  // v = {1, 2, 3, 4}
                 * // 'it' pointe vers le nouvel élément 2 à l'index 1
                 * @endcode
                 */
                Iterator Insert(ConstIterator pos, const T& value) {
                    SizeType index = static_cast<SizeType>(pos - Begin());
                    NKENTSEU_ASSERT(index <= mSize);

                    if (mSize >= mCapacity) {
                        Reserve(CalculateGrowth(mSize + 1));
                    }

                    for (SizeType i = mSize; i > index; --i) {
                        ConstructAt(mData + i, traits::NkMove(mData[i - 1]));
                        mData[i - 1].~T();
                    }

                    ConstructAt(mData + index, value);
                    ++mSize;
                    return Begin() + index;
                }

                // -----------------------------------------------------------------
                // Sous-section : Suppression d'éléments à position arbitraire
                // -----------------------------------------------------------------
                /**
                 * @brief Supprime un élément à une position donnée
                 * @param pos Itérateur constant vers l'élément à supprimer
                 * @return Iterator pointant vers l'élément suivant la suppression
                 * @ingroup VectorModifiers
                 *
                 * Détruit l'élément à 'pos', puis décale tous les éléments
                 * de [pos+1, End()) d'une position vers la gauche pour combler
                 * le trou. Retourne un itérateur vers la nouvelle position
                 * occupée par l'élément qui suivait celui supprimé.
                 *
                 * @note Complexité : O(n) dans le pire cas (décalage de tous les éléments suivants)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note 'pos' doit être dans [Begin(), End()) (pas End() : élément doit exister)
                 * @note Compatible avec Begin()+i car Iterator est convertible en ConstIterator
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3, 4};
                 * auto next = v.Erase(v.Begin() + 1);  // Supprime l'index 1 (valeur 2)
                 * // v = {1, 3, 4}, 'next' pointe vers 3 (nouvel index 1)
                 * @endcode
                 */
                Iterator Erase(ConstIterator pos) {
                    SizeType index = static_cast<SizeType>(pos - Begin());
                    NKENTSEU_ASSERT(index < mSize);

                    mData[index].~T();

                    for (SizeType i = index; i < mSize - 1; ++i) {
                        ConstructAt(mData + i, traits::NkMove(mData[i + 1]));
                        mData[i + 1].~T();
                    }

                    --mSize;
                    return Begin() + index;
                }

                /**
                 * @brief Supprime une plage d'éléments [first, last)
                 * @param first Itérateur constant vers le début de la plage à supprimer
                 * @param last Itérateur constant vers la fin de la plage (exclusive)
                 * @return Iterator pointant vers le premier élément après la plage supprimée
                 * @ingroup VectorModifiers
                 *
                 * Détruit tous les éléments de [first, last), puis décale les
                 * éléments de [last, End()) vers la gauche pour combler l'espace
                 * libéré. Retourne un itérateur vers la nouvelle position du
                 * premier élément qui suivait la plage supprimée.
                 *
                 * @note Complexité : O(n) dans le pire cas (décalage de tous les éléments suivants)
                 * @note Peut invalider tous les itérateurs, pointeurs et références
                 * @note 'first' et 'last' doivent vérifier Begin() <= first <= last <= End()
                 * @note Si first == last : no-op, retourne first inchangé
                 *
                 * @example
                 * @code
                 * NkVector<int> v = {1, 2, 3, 4, 5};
                 * auto after = v.Erase(v.Begin() + 1, v.Begin() + 4);
                 * // Supprime les indices [1,4) = {2,3,4}, v = {1, 5}
                 * // 'after' pointe vers 5 (nouvel index 1)
                 * @endcode
                 */
                Iterator Erase(ConstIterator first, ConstIterator last) {
                    SizeType firstIndex = static_cast<SizeType>(first - Begin());
                    SizeType lastIndex = static_cast<SizeType>(last - Begin());
                    SizeType count = lastIndex - firstIndex;
                    NKENTSEU_ASSERT(firstIndex <= lastIndex && lastIndex <= mSize);

                    DestroyRange(mData + firstIndex, mData + lastIndex);

                    for (SizeType i = lastIndex; i < mSize; ++i) {
                        ConstructAt(
                            mData + firstIndex + (i - lastIndex),
                            traits::NkMove(mData[i])
                        );
                        mData[i].~T();
                    }

                    mSize -= count;
                    return Begin() + firstIndex;
                }

        }; // class NkVector

        // -------------------------------------------------------------------------
        // SECTION 4 : FONCTIONS NON-MEMBRES ET OPÉRATEURS
        // -------------------------------------------------------------------------
        // Ces fonctions libres fournissent une interface additionnelle pour
        // NkVector, compatible avec les algorithmes génériques et les idiomes C++.

        /**
         * @brief Échange le contenu de deux vecteurs via leur méthode membre Swap
         * @tparam T Type des éléments stockés
         * @tparam Allocator Type de l'allocateur utilisé
         * @param lhs Premier vecteur à échanger
         * @param rhs Second vecteur à échanger
         * @ingroup VectorAlgorithms
         *
         * Fonction libre permettant l'usage de std::swap-like avec NkVector.
         * Délègue simplement à la méthode membre Swap() pour une implémentation
         * O(1) efficace.
         *
         * @note Complexité : O(1) — délégation vers NkVector::Swap
         * @note noexcept : garantie de ne pas lever d'exception
         * @note Utile pour les algorithmes de tri et l'idiome copy-and-swap
         *
         * @example
         * @code
         * NkVector<int> a = {1, 2}, b = {3, 4};
         * NkSwap(a, b);  // a = {3,4}, b = {1,2} via délégation vers a.Swap(b)
         * @endcode
         */
        template<typename T, typename Allocator>
        void NkSwap(
            NkVector<T, Allocator>& lhs,
            NkVector<T, Allocator>& rhs
        ) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

        /**
         * @brief Compare l'égalité élément par élément de deux vecteurs
         * @tparam T Type des éléments stockés (doit supporter operator==)
         * @tparam Allocator Type de l'allocateur utilisé
         * @param lhs Premier vecteur à comparer
         * @param rhs Second vecteur à comparer
         * @return true si les deux vecteurs ont même taille et éléments égaux, false sinon
         * @ingroup VectorAlgorithms
         *
         * Effectue une comparaison séquentielle :
         *  1. Vérifie d'abord l'égalité des tailles (short-circuit si différent)
         *  2. Compare chaque paire d'éléments via operator==
         *  3. Retourne false dès qu'une différence est trouvée
         *
         * @note Complexité : O(n) dans le pire cas, O(1) si tailles différentes
         * @note Requiert que T supporte l'opérateur d'égalité ==
         * @note Les allocateurs ne sont pas comparés : deux vecteurs avec mêmes
         *       éléments mais allocateurs différents sont considérés égaux
         *
         * @example
         * @code
         * NkVector<int> a = {1, 2, 3}, b = {1, 2, 3}, c = {1, 2, 4};
         * bool eq1 = (a == b);  // true : mêmes éléments
         * bool eq2 = (a == c);  // false : dernier élément différent
         * @endcode
         */
        template<typename T, typename Allocator>
        bool operator==(
            const NkVector<T, Allocator>& lhs,
            const NkVector<T, Allocator>& rhs
        ) {
            if (lhs.Size() != rhs.Size()) {
                return false;
            }
            for (typename NkVector<T, Allocator>::SizeType i = 0; i < lhs.Size(); ++i) {
                if (!(lhs[i] == rhs[i])) {
                    return false;
                }
            }
            return true;
        }

        /**
         * @brief Compare la non-égalité de deux vecteurs (négation de operator==)
         * @tparam T Type des éléments stockés
         * @tparam Allocator Type de l'allocateur utilisé
         * @param lhs Premier vecteur à comparer
         * @param rhs Second vecteur à comparer
         * @return true si les vecteurs diffèrent, false s'ils sont égaux
         * @ingroup VectorAlgorithms
         *
         * Implémentation triviale via négation de operator== pour cohérence
         * avec les conventions C++ et éviter la duplication de logique.
         *
         * @note Complexité : Identique à operator== (délégation)
         * @note Requiert que T supporte l'opérateur d'égalité ==
         *
         * @example
         * @code
         * NkVector<int> a = {1, 2}, b = {1, 3};
         * if (a != b) {
         *     printf("Vectors differ\n");  // Affiché car 2 != 3
         * }
         * @endcode
         */
        template<typename T, typename Allocator>
        bool operator!=(
            const NkVector<T, Allocator>& lhs,
            const NkVector<T, Allocator>& rhs
        ) {
            return !(lhs == rhs);
        }

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 5 : VALIDATION ET MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Ces directives aident à vérifier la configuration de compilation lors
    // du build, particulièrement utile pour déboguer les problèmes liés aux
    // options de compilation (C++11, exceptions, assertions, etc.).

    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #if defined(NKENTSEU_CXX11_OR_LATER)
            #pragma message("NkVector: C++11 move semantics ENABLED")
        #else
            #pragma message("NkVector: C++11 move semantics DISABLED (using copy fallback)")
        #endif
        #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS
            #pragma message("NkVector: Exception handling ENABLED")
        #else
            #pragma message("NkVector: Exception handling DISABLED (using assertions)")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_SEQUENTIAL_NKVECTOR_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DÉTAILLÉS
// =============================================================================
// Cette section fournit des exemples concrets et complets d'utilisation de
// NkVector, couvrant les cas d'usage courants et les bonnes pratiques.
// Ces exemples sont en commentaires pour ne pas affecter la compilation.
// =============================================================================

/*
    // -----------------------------------------------------------------------------
    // Exemple 1 : Construction et initialisation variées
    // -----------------------------------------------------------------------------
    #include <NKContainers/Sequential/NkVector.h>
    using namespace nkentseu::containers::sequential;

    void ConstructionExamples() {
        // Constructeur par défaut : vecteur vide
        NkVector<int> empty;

        // Constructeur avec taille : éléments value-initialized
        NkVector<int> zeros(5);  // {0, 0, 0, 0, 0}

        // Constructeur avec taille et valeur de remplissage
        NkVector<int> ones(5, 1);  // {1, 1, 1, 1, 1}

        // Constructeur depuis liste d'initialisation
        NkVector<int> init = {10, 20, 30};  // {10, 20, 30}

        // Constructeur de copie
        NkVector<int> copy = init;  // Copie profonde : {10, 20, 30}

        #if defined(NKENTSEU_CXX11_OR_LATER)
        // Constructeur de déplacement (C++11)
        NkVector<int> temp = MakeLargeVector();
        NkVector<int> moved = NKENTSEU_MOVE(temp);  // Transfert O(1), temp vide
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 2 : Accès aux éléments — méthodes et bonnes pratiques
    // -----------------------------------------------------------------------------
    void AccessExamples() {
        NkVector<int> v = {10, 20, 30, 40, 50};

        // Accès non vérifié (rapide) — à utiliser quand l'index est garanti valide
        int a = v[0];        // a = 10
        int b = v[4];        // b = 50
        // int c = v[10];    // ❌ Comportement indéfini en release !

        // Accès vérifié (sûr) — lève exception ou assertion si index invalide
        int d = v.At(2);     // d = 30
        // int e = v.At(10); // ✅ Erreur détectée : NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE

        // Accès aux extrémités
        int first = v.Front();  // first = 10
        int last = v.Back();    // last = 50

        // Accès via pointeur brut pour interop C/API bas niveau
        const int* raw = v.Data();
        for (nk_size i = 0; i < v.Size(); ++i) {
            printf("%d ", raw[i]);  // Affiche: 10 20 30 40 50
        }
    }

    // -----------------------------------------------------------------------------
    // Exemple 3 : Parcours avec itérateurs — styles PascalCase et STL
    // -----------------------------------------------------------------------------
    void IterationExamples() {
        NkVector<int> v = {1, 2, 3, 4, 5};

        // Style PascalCase (standard interne NKContainers)
        for (auto it = v.Begin(); it != v.End(); ++it) {
            *it *= 2;  // Modification via itérateur mutable
        }
        // v contient maintenant {2, 4, 6, 8, 10}

        // Style minuscule (compatibilité STL / range-based for)
        for (auto& val : v) {
            val += 1;  // Modification directe via référence
        }
        // v contient maintenant {3, 5, 7, 9, 11}

        // Parcours en lecture seule (const)
        void PrintVector(const NkVector<int>& vec) {
            for (auto it = vec.CBegin(); it != vec.CEnd(); ++it) {
                printf("%d ", *it);  // Lecture seule via itérateur const
            }
        }

        // Parcours inversé
        for (auto rit = v.RBegin(); rit != v.REnd(); ++rit) {
            printf("%d ", *rit);  // Affiche: 11 9 7 5 3 (ordre inverse)
        }
    }

    // -----------------------------------------------------------------------------
    // Exemple 4 : Modification — PushBack, PopBack, Insert, Erase
    // -----------------------------------------------------------------------------
    void ModificationExamples() {
        NkVector<int> v;

        // Ajout en fin : PushBack et EmplaceBack
        v.PushBack(1);              // v = {1}
        v.PushBack(2);              // v = {1, 2}
        #if defined(NKENTSEU_CXX11_OR_LATER)
        v.EmplaceBack(3);           // v = {1, 2, 3} (construction in-place)
        #endif

        // Suppression en fin : PopBack
        v.PopBack();                // v = {1, 2}

        // Insertion à position arbitraire
        auto it = v.Insert(v.Begin() + 1, 99);  // Insère 99 à l'index 1
        // v = {1, 99, 2}, 'it' pointe vers le nouvel élément 99

        // Suppression à position arbitraire
        v.Erase(v.Begin());         // Supprime l'index 0 (valeur 1)
        // v = {99, 2}

        // Suppression de plage
        NkVector<int> w = {1, 2, 3, 4, 5};
        w.Erase(w.Begin() + 1, w.Begin() + 4);  // Supprime indices [1,4) = {2,3,4}
        // w = {1, 5}
    }

    // -----------------------------------------------------------------------------
    // Exemple 5 : Gestion de capacité — Reserve, ShrinkToFit
    // -----------------------------------------------------------------------------
    void CapacityExamples() {
        NkVector<int> v;

        // Réserver à l'avance pour éviter les réallocations multiples
        v.Reserve(1000);            // Alloue pour 1000 éléments d'un coup
        for (int i = 0; i < 1000; ++i) {
            v.PushBack(i);          // Aucune réallocation pendant la boucle
        }
        // v.Size() == 1000, v.Capacity() >= 1000

        // Libérer la mémoire excédentaire après suppressions
        v.Erase(v.Begin(), v.Begin() + 500);  // Supprime 500 éléments
        v.ShrinkToFit();            // Réalloue pour exactement 500 éléments
        // v.Capacity() == v.Size() == 500, mémoire excédentaire libérée

        // Vérifier l'état
        if (v.IsEmpty()) {
            // Gérer le cas vide
        }
        nk_size currentSize = v.Size();
        nk_size currentCapacity = v.Capacity();
    }

    // -----------------------------------------------------------------------------
    // Exemple 6 : Gestion d'erreurs — mode avec/sans exceptions
    // -----------------------------------------------------------------------------
    void ErrorHandlingExamples() {
        NkVector<int> v = {1, 2, 3};

        // Accès hors bornes avec At() — comportement dépend de NKENTSEU_USE_EXCEPTIONS
        try {
            int val = v.At(10);  // Index 10 >= Size()=3
        }
        #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS
        catch (const nkentseu::containers::errors::NkVectorOutOfRangeException& e) {
            NKENTSEU_LOG_ERROR("Out of range: index=%zu, size=%zu",
                               e.GetIndex(), v.Size());
            // Gestion de l'erreur : fallback, retour d'erreur, etc.
        }
        #endif
        // En mode sans exceptions : NKENTSEU_ASSERT_MSG en debug, no-op en release

        // Allocation échouée — même principe
        try {
            v.Reserve(nkentseu::nk_size(-1));  // Demande irréaliste
        }
        #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS
        catch (const nkentseu::containers::errors::NkVectorBadAllocException& e) {
            NKENTSEU_LOG_ERROR("Allocation failed: requested %zu bytes", e.GetIndex());
            // Stratégie de fallback : réduire la demande, utiliser un pool, etc.
        }
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 7 : Intégration avec algorithmes génériques
    // -----------------------------------------------------------------------------
    template<typename Iterator, typename Predicate>
    Iterator NkFindIf(Iterator first, Iterator last, Predicate pred) {
        for (; first != last; ++first) {
            if (pred(*first)) {
                return first;
            }
        }
        return last;
    }

    void AlgorithmIntegration() {
        NkVector<int> v = {1, 2, 3, 4, 5, 6};

        // Utiliser NkVector avec algorithmes génériques grâce aux itérateurs
        auto it = NkFindIf(v.Begin(), v.End(), [](int x) { return x % 2 == 0; });
        if (it != v.End()) {
            printf("First even: %d\n", *it);  // Affiche: First even: 2
        }

        // Range-based for grâce aux aliases begin()/end()
        int sum = 0;
        for (int val : v) {
            sum += val;
        }
        // sum = 21
    }

    // -----------------------------------------------------------------------------
    // Exemple 8 : Optimisations avancées — EmplaceBack, Swap, move semantics
    // -----------------------------------------------------------------------------
    #if defined(NKENTSEU_CXX11_OR_LATER)
    struct Expensive {
        nkentseu::nk_uint8* data;
        nk_size size;

        Expensive(nk_size s) : size(s) {
            data = static_cast<nkentseu::nk_uint8*>(malloc(s));
        }

        // Constructeur de déplacement
        Expensive(Expensive&& other) NKENTSEU_NOEXCEPT
            : data(other.data), size(other.size) {
            other.data = nullptr;
            other.size = 0;
        }

        ~Expensive() { free(data); }

        // Désactiver copie pour forcer l'usage de move
        Expensive(const Expensive&) = delete;
        Expensive& operator=(const Expensive&) = delete;
    };

    void AdvancedOptimizations() {
        NkVector<Expensive> v;

        // EmplaceBack : construit directement dans le buffer, zéro copie/move temporaire
        v.EmplaceBack(1024);  // Construit Expensive(1024) in-place dans v

        // PushBack avec move : évite la copie grâce à move semantics
        Expensive temp(2048);
        v.PushBack(NKENTSEU_MOVE(temp));  // Déplace temp dans v, temp devient vide

        // Swap O(1) pour réorganiser sans copier les données lourdes
        NkVector<Expensive> other;
        other.EmplaceBack(512);
        v.Swap(other);  // Échange les buffers en O(1), pas de copie des données Expensive
    }
    #endif
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis std::vector vers NkVector :

    std::vector<T> API          | NkVector<T> équivalent
    ----------------------------|----------------------------------
    std::vector<T> v;          | NkVector<T> v;
    v.push_back(x);            | v.PushBack(x);  // PascalCase standard interne
    v.emplace_back(args...);   | v.EmplaceBack(args...);
    v[i]                       | v[i] ou v.At(i) pour vérification bounds
    v.at(i)                    | v.At(i)  // Même nom, même sémantique
    v.begin(), v.end()         | v.begin()/end() OU v.Begin()/End() (les deux)
    v.size(), v.capacity()     | v.size()/capacity() OU v.Size()/Capacity()
    v.empty()                  | v.empty() OU v.Empty() OU v.IsEmpty()
    v.resize(n, val)           | v.Resize(n, val)
    v.reserve(n)               | v.Reserve(n)
    v.shrink_to_fit()          | v.ShrinkToFit()
    v.clear()                  | v.Clear()
    v.insert(pos, val)         | v.Insert(pos, val)
    v.erase(pos)               | v.Erase(pos)
    std::swap(a, b)            | NkSwap(a, b) ou a.Swap(b)

    Avantages de NkVector vs std::vector :
    - Zéro dépendance STL : portable sur plateformes embarquées, kernels, etc.
    - Allocateur personnalisable via template (pas de std::allocator hardcoded)
    - Gestion d'erreurs configurable : exceptions ou assertions selon le build
    - Nommage PascalCase cohérent avec le reste du framework NK*
    - Macros NKENTSEU_* pour export/import DLL, inline hints, etc.
    - Intégration avec le système de logging et d'assertions NKPlatform

    Configuration CMake recommandée :
    # Mode développement avec exceptions pour débogage facile
    target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=1)

    # Mode production sans exceptions pour déterminisme et performance
    target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=0)

    # Activer C++11 pour move semantics et EmplaceBack
    target_compile_features(monapp PRIVATE cxx_std_11)
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================