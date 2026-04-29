// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\CacheFriendly\NkRingBuffer.h
// DESCRIPTION: Tampon circulaire - File FIFO à capacité fixe
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H
#define NK_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits pour move/forward et utilitaires méta
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKMemory/NkFunction.h"               // Wrapper pour les fonctions et callables
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Iterators/NkIterator.h" // Infrastructure commune pour les itérateurs
    #include "NKContainers/Iterators/NkInitializerList.h" // Support des listes d'initialisation

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK RING BUFFER (TAMPON CIRCULAIRE)
        // ====================================================================

        /**
         * @brief Classe template implémentant un tampon circulaire FIFO à capacité fixe
         * 
         * Structure de données en anneau offrant une file First-In-First-Out avec
         * comportement d'écrasement automatique quand la capacité est atteinte.
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Allocation unique d'un bloc contigu de capacity * sizeof(T)
         * - Deux index circulaires : mHead (écriture), mTail (lecture)
         * - Arithmétique modulo pour le wrapping : (index + 1) % capacity
         * - Quand plein : Push() écrase l'élément le plus ancien (mTail avance)
         * - Mémoire pré-allouée : aucune réallocation après construction
         * 
         * GARANTIES DE PERFORMANCE :
         * - Push() : O(1) constant, écriture directe + avance de mHead
         * - Pop() : O(1) constant, lecture directe + avance de mTail
         * - Accès par index : O(1) constant via calcul (mTail + index) % capacity
         * - Mémoire : sizeof(T) * capacity exactement, aucun overhead dynamique
         * - Localité excellente : données contiguës, prefetching CPU optimal
         * 
         * CAS D'USAGE TYPQUES :
         * - Buffers audio/vidéo pour streaming en temps réel
         * - Logging circulaire avec conservation des N dernières entrées
         * - Patterns producer-consumer avec producteur plus rapide que consommateur
         * - Files d'attente lock-free (avec primitives atomiques externes)
         * - Échantillonnage de données avec fenêtre glissante fixe
         * - Cache LRU simplifié avec éviction automatique des anciens éléments
         * 
         * @tparam T Type des éléments stockés (doit être copiable/déplaçable et destructible)
         * @tparam Allocator Type de l'allocateur pour le bloc principal (défaut: memory::NkAllocator)
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour usage concurrent
         * @note Comportement d'écrasement : Push() sur buffer plein remplace l'élément le plus ancien
         * @note Les éléments sont construits/détruits via placement new pour gestion manuelle
         * @note Non redimensionnable : capacity fixe déterminée à la construction
         */
        template<typename T, typename Allocator = memory::NkAllocator>
        class NkRingBuffer {

            // ====================================================================
            // SECTION PUBLIQUE : ALIASES DE TYPES
            // ====================================================================
            public:

                // ====================================================================
                // ALIASES DE TYPES POUR LA LISIBILITÉ ET LA GÉNÉRICITÉ
                // ====================================================================

                using ValueType = T;              ///< Alias du type des éléments stockés
                using SizeType = usize;           ///< Alias du type pour les tailles/comptages
                using Reference = T&;             ///< Alias de référence non-const vers un élément
                using ConstReference = const T&;  ///< Alias de référence const vers un élément
                using Pointer = T*;               ///< Alias de pointeur non-const vers un élément
                using ConstPointer = const T*;    ///< Alias de pointeur const vers un élément

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET MÉTHODES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DU TAMPON CIRCULAIRE
                // ====================================================================

                T* mData;                   ///< Pointeur vers le bloc mémoire contigu alloué
                SizeType mCapacity;         ///< Capacité maximale fixe du tampon (nombre de slots)
                SizeType mHead;             ///< Index d'écriture : prochain slot à remplir
                SizeType mTail;             ///< Index de lecture : prochain slot à lire
                SizeType mSize;             ///< Nombre d'éléments actuellement stockés (0 <= mSize <= mCapacity)
                Allocator* mAllocator;      ///< Pointeur vers l'allocateur utilisé pour le bloc principal

                // ====================================================================
                // MÉTHODES UTILITAIRES INTERNES
                // ====================================================================

                /**
                 * @brief Calcule l'index suivant dans l'anneau avec wrapping modulo
                 * @param index Index courant dans [0, mCapacity)
                 * @return Index suivant : (index + 1) % mCapacity
                 * @note Complexité O(1) : opération arithmétique simple
                 * @note Méthode const : ne modifie pas l'état du tampon
                 * @note Utilisée pour avancer mHead et mTail de façon circulaire
                 */
                SizeType NextIndex(SizeType index) const {
                    return (index + 1) % mCapacity;
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal avec capacité et allocateur par défaut
                 * @param capacity Nombre maximal d'éléments que le tampon peut contenir
                 * @note Alloue un bloc contigu de capacity * sizeof(T) via l'allocateur par défaut
                 * @note Initialise les index mHead/mTail à 0 et mSize à 0 (tampon vide)
                 * @note Assertion en debug pour garantir capacity > 0
                 * @note Complexité : O(1) pour l'allocation, pas d'initialisation des éléments
                 */
                explicit NkRingBuffer(SizeType capacity)
                    : mData(nullptr)
                    , mCapacity(capacity)
                    , mHead(0)
                    , mTail(0)
                    , mSize(0)
                    , mAllocator(&memory::NkGetDefaultAllocator()) {
                    NKENTSEU_ASSERT(capacity > 0);
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                }

                /**
                 * @brief Constructeur depuis NkInitializerList avec allocateur optionnel
                 * @param init Liste d'éléments pour initialiser le tampon
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Alloue un bloc de taille max(init.Size(), 1) pour éviter capacity=0
                 * @note Insère les éléments via Push() : peut écraser si init.Size() > capacity
                 * @note Les éléments sont copiés via placement new dans le bloc alloué
                 */
                NkRingBuffer(NkInitializerList<T> init, Allocator* allocator = nullptr)
                    : mData(nullptr)
                    , mCapacity(init.Size() > 0 ? init.Size() : 1)
                    , mHead(0)
                    , mTail(0)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Constructeur depuis std::initializer_list avec allocateur optionnel
                 * @param init Liste standard d'éléments pour initialiser le tampon
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Compatibilité avec la syntaxe C++11 : NkRingBuffer<int> rb = {1, 2, 3};
                 * @note Insère les éléments via Push() : comportement FIFO avec écrasement si plein
                 */
                NkRingBuffer(std::initializer_list<T> init, Allocator* allocator = nullptr)
                    : mData(nullptr)
                    , mCapacity(init.size() > 0 ? init.size() : 1)
                    , mHead(0)
                    , mTail(0)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                    for (auto& val : init) {
                        Push(val);
                    }
                }

                /**
                 * @brief Constructeur avec capacité et allocateur personnalisé explicite
                 * @param capacity Nombre maximal d'éléments que le tampon peut contenir
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Alloue un bloc contigu via l'allocateur spécifié
                 * @note Assertion en debug pour garantir capacity > 0
                 * @note Utile pour utiliser un allocateur pool ou personnalisé pour le bloc principal
                 */
                explicit NkRingBuffer(SizeType capacity, Allocator* allocator)
                    : mData(nullptr)
                    , mCapacity(capacity)
                    , mHead(0)
                    , mTail(0)
                    , mSize(0)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator()) {
                    NKENTSEU_ASSERT(capacity > 0);
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                }

                /**
                 * @brief Constructeur de copie pour duplication profonde du tampon
                 * @param other Tampon source à dupliquer élément par élément
                 * @note Alloue un nouveau bloc de même capacité que other
                 * @note Copie les éléments dans l'ordre FIFO en préservant mSize
                 * @note Utilise placement new pour construire les copies dans le nouveau bloc
                 * @note Complexité O(N) où N = other.mSize : copie de chaque élément actif
                 */
                NkRingBuffer(const NkRingBuffer& other)
                    : mData(nullptr)
                    , mCapacity(other.mCapacity)
                    , mHead(other.mHead)
                    , mTail(other.mTail)
                    , mSize(other.mSize)
                    , mAllocator(other.mAllocator) {
                    mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                    for (SizeType i = 0; i < mSize; ++i) {
                        SizeType idx = (mTail + i) % mCapacity;
                        new (&mData[idx]) T(other.mData[idx]);
                    }
                }

                // ====================================================================
                // SUPPORT C++11 : CONSTRUCTEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Constructeur de déplacement pour transfert efficace des ressources
                     * @param other Tampon source dont transférer le contenu
                     * @note noexcept : transfert par échange de pointeurs, aucune allocation
                     * @note other est laissé dans un état valide mais vide après le move
                     * @note Complexité O(1) : simple réaffectation de pointeurs et métadonnées
                     */
                    NkRingBuffer(NkRingBuffer&& other) NKENTSEU_NOEXCEPT
                        : mData(other.mData)
                        , mCapacity(other.mCapacity)
                        , mHead(other.mHead)
                        , mTail(other.mTail)
                        , mSize(other.mSize)
                        , mAllocator(other.mAllocator) {
                        other.mData = nullptr;
                        other.mCapacity = 0;
                        other.mHead = 0;
                        other.mTail = 0;
                        other.mSize = 0;
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Destructeur libérant le bloc mémoire et détruisant les éléments actifs
                 * @note Appelle Clear() pour détruire chaque élément actif via son destructeur
                 * @note Libère le bloc principal via mAllocator->Deallocate(mData)
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 * @note Complexité O(N) pour la destruction des éléments + O(1) pour la libération
                 */
                ~NkRingBuffer() {
                    Clear();
                    if (mData) {
                        mAllocator->Deallocate(mData);
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================
            public:

                /**
                 * @brief Opérateur d'affectation depuis NkInitializerList
                 * @param init Nouvelle liste d'éléments pour remplacer le contenu actuel
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Vide le tampon via Clear() avant d'insérer les nouveaux éléments
                 * @note Complexité O(n + m) où n = taille actuelle, m = taille de la liste
                 */
                NkRingBuffer& operator=(NkInitializerList<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis std::initializer_list (C++11)
                 * @param init Nouvelle liste standard d'éléments pour remplacer le contenu
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Syntaxe C++11 : buffer = {4, 5, 6};
                 */
                NkRingBuffer& operator=(std::initializer_list<T> init) {
                    Clear();
                    for (auto& val : init) {
                        Push(val);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par copie avec gestion d'auto-affectation
                 * @param other Tampon source à copier
                 * @return Référence vers l'instance courante pour le chaînage d'opérations
                 * @note Libère les ressources actuelles avant de copier le contenu de other
                 * @note Préserve l'allocateur courant : ne copie pas mAllocator de other
                 * @note Complexité O(N) où N = other.mSize : copie de chaque élément actif
                 */
                NkRingBuffer& operator=(const NkRingBuffer& other) {
                    if (this != &other) {
                        Clear();
                        if (mData) {
                            mAllocator->Deallocate(mData);
                        }
                        mCapacity = other.mCapacity;
                        mHead = other.mHead;
                        mTail = other.mTail;
                        mSize = other.mSize;
                        mAllocator = other.mAllocator;
                        mData = static_cast<T*>(mAllocator->Allocate(mCapacity * sizeof(T)));
                        for (SizeType i = 0; i < mSize; ++i) {
                            SizeType idx = (mTail + i) % mCapacity;
                            new (&mData[idx]) T(other.mData[idx]);
                        }
                    }
                    return *this;
                }

                // ====================================================================
                // SUPPORT C++11 : OPÉRATEUR DE DÉPLACEMENT
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Opérateur d'affectation par déplacement avec transfert de ressources
                     * @param other Tampon source dont transférer le contenu
                     * @return Référence vers l'instance courante pour le chaînage d'opérations
                     * @note noexcept : échange de pointeurs sans allocation ni copie
                     * @note other est laissé dans un état valide mais vide après l'opération
                     * @note Complexité O(1) : réaffectation directe des membres internes
                     */
                    NkRingBuffer& operator=(NkRingBuffer&& other) NKENTSEU_NOEXCEPT {
                        if (this != &other) {
                            Clear();
                            if (mData) {
                                mAllocator->Deallocate(mData);
                            }
                            mData = other.mData;
                            mCapacity = other.mCapacity;
                            mHead = other.mHead;
                            mTail = other.mTail;
                            mSize = other.mSize;
                            mAllocator = other.mAllocator;
                            other.mData = nullptr;
                            other.mCapacity = 0;
                            other.mHead = 0;
                            other.mTail = 0;
                            other.mSize = 0;
                        }
                        return *this;
                    }

                #endif // defined(NK_CPP11)

            // ====================================================================
            // SECTION PUBLIQUE : ACCÈS AUX ÉLÉMENTS (FRONT, BACK, INDEX)
            // ====================================================================
            public:

                /**
                 * @brief Accède à l'élément à l'avant du tampon (le plus ancien, version mutable)
                 * @return Référence non-const vers l'élément à mTail (prochain à être poppé)
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Complexité O(1) : accès direct via mData[mTail]
                 * @note Permet la modification : buffer.Front() = newValue;
                 */
                Reference Front() {
                    NKENTSEU_ASSERT(!Empty());
                    return mData[mTail];
                }

                /**
                 * @brief Accède à l'élément à l'avant du tampon (version const)
                 * @return Référence const vers l'élément à mTail
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkRingBuffer const ou accès en lecture seule
                 * @note Complexité O(1) : accès direct via mData[mTail]
                 */
                ConstReference Front() const {
                    NKENTSEU_ASSERT(!Empty());
                    return mData[mTail];
                }

                /**
                 * @brief Accède à l'élément à l'arrière du tampon (le plus récent, version mutable)
                 * @return Référence non-const vers l'élément avant mHead (dernier Push())
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Calcul de l'index : (mHead + mCapacity - 1) % mCapacity pour gérer le wrapping
                 * @note Complexe O(1) : calcul arithmétique simple + accès direct
                 */
                Reference Back() {
                    NKENTSEU_ASSERT(!Empty());
                    SizeType idx = (mHead + mCapacity - 1) % mCapacity;
                    return mData[idx];
                }

                /**
                 * @brief Accède à l'élément à l'arrière du tampon (version const)
                 * @return Référence const vers l'élément avant mHead
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkRingBuffer const ou accès en lecture seule
                 * @note Calcul de l'index : (mHead + mCapacity - 1) % mCapacity pour gérer le wrapping
                 */
                ConstReference Back() const {
                    NKENTSEU_ASSERT(!Empty());
                    SizeType idx = (mHead + mCapacity - 1) % mCapacity;
                    return mData[idx];
                }

                /**
                 * @brief Accès par index avec vérification des bornes en debug
                 * @param index Position dans [0, mSize) relative au début logique (mTail)
                 * @return Référence non-const vers l'élément à la position logique index
                 * @pre index < mSize (assertion NKENTSEU_ASSERT en debug)
                 * @note Calcul de l'index physique : (mTail + index) % mCapacity pour le wrapping
                 * @note Permet un accès de type tableau : buffer[2] retourne le 3ème élément FIFO
                 */
                Reference operator[](SizeType index) {
                    NKENTSEU_ASSERT(index < mSize);
                    SizeType idx = (mTail + index) % mCapacity;
                    return mData[idx];
                }

                /**
                 * @brief Accès par index avec vérification des bornes en debug (version const)
                 * @param index Position dans [0, mSize) relative au début logique (mTail)
                 * @return Référence const vers l'élément à la position logique index
                 * @pre index < mSize (assertion NKENTSEU_ASSERT en debug)
                 * @note Version const pour usage sur NkRingBuffer const ou accès en lecture seule
                 * @note Calcul de l'index physique : (mTail + index) % mCapacity pour le wrapping
                 */
                ConstReference operator[](SizeType index) const {
                    NKENTSEU_ASSERT(index < mSize);
                    SizeType idx = (mTail + index) % mCapacity;
                    return mData[idx];
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================
            public:

                /**
                 * @brief Teste si le tampon ne contient aucun élément
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

                /**
                 * @brief Teste si le tampon a atteint sa capacité maximale
                 * @return true si mSize == mCapacity, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Utile pour anticiper le comportement d'écrasement de Push()
                 */
                bool IsFull() const NKENTSEU_NOEXCEPT {
                    return mSize == mCapacity;
                }

                /**
                 * @brief Retourne le nombre d'éléments actuellement stockés
                 * @return Valeur de type SizeType dans [0, mCapacity]
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Reflète exactement le nombre d'éléments actifs (pas la capacité allouée)
                 */
                SizeType Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Retourne la capacité maximale fixe du tampon
                 * @return Valeur de type SizeType égale à la capacité configurée à la construction
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Valeur constante : ne change jamais après la construction
                 */
                SizeType Capacity() const NKENTSEU_NOEXCEPT {
                    return mCapacity;
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS - CLEAR, PUSH, POP
            // ====================================================================
            public:

                /**
                 * @brief Supprime tous les éléments en appelant leurs destructeurs
                 * @note Complexité O(N) où N = mSize : appel de Pop() en boucle
                 * @note Détruit chaque élément via son destructeur avant de libérer le slot
                 * @note Après Clear(), Empty() == true et Size() == 0, mais Capacity() inchangé
                 * @note Ne libère pas le bloc mémoire principal : conserve la capacité allouée
                 */
                void Clear() {
                    while (!Empty()) {
                        Pop();
                    }
                }

                /**
                 * @brief Ajoute un nouvel élément à l'arrière du tampon (Push FIFO)
                 * @param value Référence const vers l'élément à ajouter
                 * @note Si le tampon est plein : écrase l'élément le plus ancien (mTail avance)
                 * @note Si le tampon n'est pas plein : incrémente mSize après insertion
                 * @note Utilise placement new pour construire l'élément dans le slot mHead
                 * @note Avance mHead via NextIndex() pour le wrapping circulaire
                 * @note Complexité O(1) constant : pas de réallocation, opérations arithmétiques simples
                 */
                void Push(const T& value) {
                    if (IsFull()) {
                        mData[mHead].~T();
                        new (&mData[mHead]) T(value);
                        mHead = NextIndex(mHead);
                        mTail = NextIndex(mTail);
                    } else {
                        new (&mData[mHead]) T(value);
                        mHead = NextIndex(mHead);
                        ++mSize;
                    }
                }

                // ====================================================================
                // SUPPORT C++11 : MOVE SEMANTICS ET EMPLACE
                // ====================================================================
                #if defined(NK_CPP11)

                    /**
                     * @brief Ajoute un élément par déplacement à l'arrière du tampon
                     * @param value Rvalue reference vers l'élément à déplacer dans le tampon
                     * @note Optimise les transferts pour les types lourds (NkString, NkVector, etc.)
                     * @note Comportement identique à Push(const T&) mais avec move semantics
                     * @note Nécessite que T soit MoveConstructible
                     */
                    void Push(T&& value) {
                        if (IsFull()) {
                            mData[mHead].~T();
                            new (&mData[mHead]) T(traits::NkMove(value));
                            mHead = NextIndex(mHead);
                            mTail = NextIndex(mTail);
                        } else {
                            new (&mData[mHead]) T(traits::NkMove(value));
                            mHead = NextIndex(mHead);
                            ++mSize;
                        }
                    }

                    /**
                     * @brief Construit et ajoute un élément directement à l'arrière du tampon
                     * @tparam Args Types des arguments de construction de T
                     * @param args Arguments forwarding vers le constructeur de T
                     * @note Évite toute copie/move temporaire : construction in-place dans le slot mHead
                     * @note Particulièrement efficace pour les types avec constructeurs complexes
                     * @note Comportement d'écrasement identique : si plein, remplace l'élément le plus ancien
                     */
                    template<typename... Args>
                    void Emplace(Args&&... args) {
                        if (IsFull()) {
                            mData[mHead].~T();
                            new (&mData[mHead]) T(traits::NkForward<Args>(args)...);
                            mHead = NextIndex(mHead);
                            mTail = NextIndex(mTail);
                        } else {
                            new (&mData[mHead]) T(traits::NkForward<Args>(args)...);
                            mHead = NextIndex(mHead);
                            ++mSize;
                        }
                    }

                #endif // defined(NK_CPP11)

                /**
                 * @brief Supprime et retourne l'élément à l'avant du tampon (Pop FIFO)
                 * @return Copie de l'élément qui était à mTail (le plus ancien)
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Détruit l'élément via son destructeur après copie pour le retour
                 * @note Avance mTail via NextIndex() pour le wrapping circulaire
                 * @note Décrémente mSize pour refléter la suppression
                 * @note Complexité O(1) constant : copie + destruction + arithmétique simple
                 * @note Pour éviter la copie si inutile : utiliser PopDiscard()
                 */
                T Pop() {
                    NKENTSEU_ASSERT(!Empty());
                    T value = mData[mTail];
                    mData[mTail].~T();
                    mTail = NextIndex(mTail);
                    --mSize;
                    return value;
                }

                /**
                 * @brief Supprime l'élément à l'avant sans retourner sa valeur (optimisation)
                 * @pre Le tampon ne doit pas être vide (assertion NKENTSEU_ASSERT en debug)
                 * @note Détruit l'élément via son destructeur sans copie pour retour
                 * @note Avance mTail via NextIndex() pour le wrapping circulaire
                 * @note Décrémente mSize pour refléter la suppression
                 * @note Plus rapide que Pop() quand la valeur n'est pas nécessaire
                 * @note Complexité O(1) constant : destruction + arithmétique simple
                 */
                void PopDiscard() {
                    NKENTSEU_ASSERT(!Empty());
                    mData[mTail].~T();
                    mTail = NextIndex(mTail);
                    --mSize;
                }

                /**
                 * @brief Échange le contenu de ce tampon avec un autre en temps constant
                 * @param other Autre instance de NkRingBuffer à échanger
                 * @note noexcept : échange de pointeurs et métadonnées via traits::NkSwap()
                 * @note Complexité O(1) : aucune allocation, copie ou destruction d'éléments
                 * @note Préférer à l'assignation pour les permutations efficaces de grands tampons
                 */
                void Swap(NkRingBuffer& other) NKENTSEU_NOEXCEPT {
                    traits::NkSwap(mData, other.mData);
                    traits::NkSwap(mCapacity, other.mCapacity);
                    traits::NkSwap(mHead, other.mHead);
                    traits::NkSwap(mTail, other.mTail);
                    traits::NkSwap(mSize, other.mSize);
                    traits::NkSwap(mAllocator, other.mAllocator);
                }
        };

        // ====================================================================
        // FONCTIONS NON-MEMBRES : SWAP GLOBAL
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour ADL (Argument-Dependent Lookup)
         * @tparam T Type des éléments du tampon
         * @tparam Allocator Type de l'allocateur pour le bloc principal
         * @param lhs Premier tampon à échanger
         * @param rhs Second tampon à échanger
         * @note noexcept : délègue à la méthode membre Swap()
         * @note Permet l'usage idiomatique : using nkentseu::NkSwap; NkSwap(a, b);
         */
        template<typename T, typename Allocator>
        void NkSwap(NkRingBuffer<T, Allocator>& lhs, NkRingBuffer<T, Allocator>& rhs) NKENTSEU_NOEXCEPT {
            lhs.Swap(rhs);
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_CACHEFRIENDLY_NKRINGBUFFER_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkRingBuffer
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Opérations de base FIFO avec écrasement
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/CacheFriendly/NkRingBuffer.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un tampon circulaire de capacité 5
 *     nkentseu::NkRingBuffer<int> buffer(5);
 *     
 *     // Push d'éléments : FIFO, ordre préservé
 *     buffer.Push(1);  // [1]
 *     buffer.Push(2);  // [1, 2]
 *     buffer.Push(3);  // [1, 2, 3]
 *     
 *     // Accès aux extrémités
 *     printf("Front: %d, Back: %d\n", buffer.Front(), buffer.Back());  // 1, 3
 *     
 *     // Pop : retire l'élément le plus ancien
 *     int val = buffer.Pop();  // 1, buffer: [2, 3]
 *     printf("Popped: %d, New front: %d\n", val, buffer.Front());  // 1, 2
 *     
 *     // Remplissage jusqu'à capacité
 *     buffer.Push(4);
 *     buffer.Push(5);
 *     buffer.Push(6);  // buffer plein: [2, 3, 4, 5, 6]
 *     
 *     // Push sur buffer plein : écrase l'élément le plus ancien (2)
 *     buffer.Push(7);  // [3, 4, 5, 6, 7] - 2 a été écrasé
 *     printf("Après écrasement - Front: %d, Size: %zu\n", 
 *            buffer.Front(), buffer.Size());  // 3, 5
 *     
 *     // Accès par index (ordre FIFO)
 *     printf("Éléments: ");
 *     for (usize i = 0; i < buffer.Size(); ++i) {
 *         printf("%d ", buffer[i]);
 *     }
 *     // Sortie: 3 4 5 6 7
 *     printf("\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Logging circulaire avec conservation des N dernières entrées
 * --------------------------------------------------------------------------
 * @code
 * #include "NKCore/NkString.h"
 * 
 * class CircularLogger {
 * private:
 *     nkentseu::NkRingBuffer<nkentseu::NkString> mBuffer;
 *     
 * public:
 *     CircularLogger(usize maxEntries) : mBuffer(maxEntries) {}
 *     
 *     void Log(const char* message) {
 *         // Emplace pour construction directe sans copie temporaire
 *         #if defined(NK_CPP11)
 *         mBuffer.Emplace("%zu: %s", /* timestamp * /, message);
 *         #else
 *         nkentseu::NkString entry(message);
 *         mBuffer.Push(entry);
 *         #endif
 *     }
 *     
 *     void PrintAll() const {
 *         printf("=== Dernières %zu entrées ===\n", mBuffer.Size());
 *         for (usize i = 0; i < mBuffer.Size(); ++i) {
 *             printf("  %s\n", mBuffer[i].CStr());
 *         }
 *     }
 *     
 *     bool IsFull() const { return mBuffer.IsFull(); }
 * };
 * 
 * void exempleLogging() {
 *     CircularLogger logger(10);  // Garde les 10 derniers logs
 *     
 *     // Simulation d'événements
 *     for (usize i = 0; i < 15; ++i) {
 *         char msg[64];
 *         // snprintf(msg, sizeof(msg), "Événement #%zu", i);
 *         logger.Log(msg);
 *     }
 *     
 *     // Affiche uniquement les 10 derniers (les 5 premiers ont été écrasés)
 *     logger.PrintAll();
 *     printf("Buffer plein: %s\n", logger.IsFull() ? "oui" : "non");  // oui
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Buffer audio/vidéo pour streaming en temps réel
 * --------------------------------------------------------------------------
 * @code
 * struct AudioSample {
 *     float left;   // Canal gauche
 *     float right;  // Canal droit
 *     
 *     AudioSample(float l = 0.0f, float r = 0.0f) : left(l), right(r) {}
 * };
 * 
 * class AudioStreamBuffer {
 * private:
 *     nkentseu::NkRingBuffer<AudioSample> mBuffer;
 *     static constexpr usize BUFFER_SIZE = 4096;  // ~93ms à 44.1kHz stéréo
 *     
 * public:
 *     AudioStreamBuffer() : mBuffer(BUFFER_SIZE) {}
 *     
 *     // Producteur : ajoute des échantillons au buffer
 *     void Write(const AudioSample* samples, usize count) {
 *         for (usize i = 0; i < count; ++i) {
 *             mBuffer.Push(samples[i]);  // Écrase les anciens si plein
 *         }
 *     }
 *     
 *     // Consommateur : lit des échantillons depuis le buffer
 *     usize Read(AudioSample* outBuffer, usize maxCount) {
 *         usize count = 0;
 *         while (count < maxCount && !mBuffer.Empty()) {
 *             outBuffer[count++] = mBuffer.Pop();  // Copie + suppression
 *         }
 *         return count;
 *     }
 *     
 *     // Vérifie si le buffer a assez de données pour un traitement
 *     bool HasEnoughSamples(usize required) const {
 *         return mBuffer.Size() >= required;
 *     }
 *     
 *     // Clear pour reset du stream
 *     void Reset() { mBuffer.Clear(); }
 * };
 * 
 * void exempleAudioStream() {
 *     AudioStreamBuffer stream;
 *     
 *     // Simulation : producteur écrit des échantillons
 *     AudioSample chunk[256];
 *     for (usize i = 0; i < 256; ++i) {
 *         chunk[i] = AudioSample(0.5f, -0.5f);  // Signal test
 *     }
 *     stream.Write(chunk, 256);
 *     
 *     // Consommateur lit pour traitement
 *     AudioSample output[128];
 *     usize readCount = stream.Read(output, 128);
 *     printf("Lu %zu échantillons\n", readCount);
 *     
 *     // Le buffer gère automatiquement le dépassement :
 *     // si le producteur est plus rapide, les anciens échantillons sont écrasés
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Pattern producer-consumer avec optimisation PopDiscard
 * --------------------------------------------------------------------------
 * @code
 * // Simulation simplifiée de tâche en arrière-plan
 * void exempleProducerConsumer() {
 *     nkentseu::NkRingBuffer<int> taskQueue(100);
 *     
 *     // Producteur : génère des tâches
 *     auto producer = [&]() {
 *         for (int i = 0; i < 150; ++i) {
 *             if (!taskQueue.IsFull()) {
 *                 taskQueue.Push(i);  // Ajoute la tâche
 *             }
 *             // Si plein : les tâches les plus anciennes sont perdues (acceptable pour ce cas)
 *         }
 *     };
 *     
 *     // Consommateur : traite les tâches
 *     auto consumer = [&]() {
 *         while (!taskQueue.Empty()) {
 *             int task = taskQueue.Pop();  // Récupère et supprime
 *             // Traitement de la tâche...
 *             printf("Traitement tâche #%d\n", task);
 *         }
 *     };
 *     
 *     // Exécution simulée
 *     producer();
 *     printf("Tâches en attente: %zu\n", taskQueue.Size());
 *     consumer();
 *     printf("File vide: %s\n", taskQueue.Empty() ? "oui" : "non");
 * }
 * 
 * // Optimisation : PopDiscard() quand la valeur n'est pas nécessaire
 * void exemplePopDiscard() {
 *     nkentseu::NkRingBuffer<int> buffer(50);
 *     
 *     // Remplissage
 *     for (int i = 0; i < 100; ++i) {
 *         buffer.Push(i);  // Les 50 premiers sont écrasés
 *     }
 *     
 *     // Vidage sans besoin des valeurs : plus rapide que Pop()
 *     while (!buffer.Empty()) {
 *         buffer.PopDiscard();  // Détruit + avance mTail, pas de copie de retour
 *     }
 *     printf("Buffer vidé, taille: %zu\n", buffer.Size());  // 0
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurCustom() {
 *     // Allocateur pool pour le bloc principal du ring buffer
 *     memory::NkPoolAllocator poolAlloc(64 * 1024);  // 64KB pool
 *     
 *     // Ring buffer utilisant l'allocateur personnalisé
 *     nkentseu::NkRingBuffer<float, memory::NkPoolAllocator> 
 *         signalBuffer(4096, &poolAlloc);
 *     
 *     // Avantages :
 *     // - Allocation du bloc dans le pool -> pas de fragmentation externe
 *     // - Libération en bloc à la destruction -> très rapide
 *     // - Idéal pour les systèmes embarqués ou temps réel
 *     
 *     // Utilisation normale
 *     for (usize i = 0; i < 1000; ++i) {
 *         signalBuffer.Push(static_cast<float>(i) * 0.001f);
 *     }
 *     
 *     printf("Signal buffer: %zu/%zu échantillons\n",
 *            signalBuffer.Size(), signalBuffer.Capacity());
 *     
 *     // Destruction : libération en bloc via le pool
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Comparaison avec NkQueue - quand choisir quoi ?
 * --------------------------------------------------------------------------
 * @code
 * void exempleChoixConteneur() {
 *     // CAS 1: Besoin de capacité fixe avec écrasement automatique
 *     // -> NkRingBuffer (comportement circulaire)
 *     nkentseu::NkRingBuffer<int> ringBuf(100);
 *     for (int i = 0; i < 150; ++i) {
 *         ringBuf.Push(i);  // Les 50 premiers sont automatiquement écrasés
 *     }
 *     // ringBuf contient [50, 51, ..., 149] - toujours 100 éléments max
 *     
 *     // CAS 2: Besoin de file FIFO sans perte de données
 *     // -> NkQueue (croissance dynamique via conteneur sous-jacent)
 *     nkentseu::NkQueue<int> queue;  // Utilise NkDeque par défaut
 *     for (int i = 0; i < 150; ++i) {
 *         queue.Push(i);  // Tous les éléments sont conservés
 *     }
 *     // queue contient [0, 1, ..., 149] - 150 éléments, mémoire dynamique
 *     
 *     // COMPARAISON DE PERFORMANCE:
 *     // - NkRingBuffer: O(1) garanti, mémoire fixe, pas d'allocation après construction
 *     // - NkQueue: O(1) amorti, mémoire dynamique, allocations occasionnelles
 *     
 *     // RÈGLE PRATIQUE:
 *     // - Utiliser NkRingBuffer si: capacité fixe acceptable, écrasement OK, performance critique
 *     // - Utiliser NkQueue si: pas de perte de données tolérée, taille variable, simplicité
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE LA CAPACITÉ :
 *    - Dimensionner selon le pire cas de décalage producer/consumer
 *    - Mémoire allouée = capacity * sizeof(T) : prévoir l'impact pour les types larges
 *    - Pour les types lourds : envisager NkRingBuffer<T*> avec gestion manuelle des pointeurs
 *    - Utiliser Capacity() et Size() pour monitorer l'utilisation en production
 * 
 * 2. GESTION DU COMPORTEMENT D'ÉCRASEMENT :
 *    - Push() sur buffer plein écrase l'élément le plus ancien : documenter ce comportement
 *    - Pour éviter la perte de données : vérifier !IsFull() avant Push() si critique
 *    - Pour les logs/monitoring : l'écrasement est souvent souhaitable (garder les plus récents)
 *    - Pour les données critiques : utiliser NkQueue ou vérifier la capacité avant insertion
 * 
 * 3. OPTIMISATIONS DE PERFORMANCE :
 *    - Localité mémoire excellente : parcours séquentiel des éléments pour prefetching CPU
 *    - Pas de réallocation : idéal pour les boucles temps réel sans allocation dynamique
 *    - PopDiscard() plus rapide que Pop() quand la valeur n'est pas nécessaire
 *    - Emplace() évite les constructions temporaires pour les types complexes
 * 
 * 4. ACCÈS ET MODIFICATION :
 *    - Front()/Back() : accès O(1) aux extrémités sans suppression
 *    - operator[] : accès aléatoire en O(1) dans l'ordre FIFO, avec vérification de bornes en debug
 *    - Les références retournées permettent la modification : buffer.Front() = newValue;
 *    - Attention : modifier un élément ne change pas l'ordre FIFO du tampon
 * 
 * 5. SÉCURITÉ ET ROBUSTESSE :
 *    - Assertions en debug pour valider !Empty() avant Front()/Back()/Pop()
 *    - Vérifier le retour de Push() indirectement via IsFull() si perte de données inacceptable
 *    - Thread-unsafe par défaut : protéger avec mutex si accès concurrents producer/consumer
 *    - Pour les types avec ressources : le destructeur est appelé automatiquement sur Pop/Clear
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas d'itérateurs : à ajouter pour compatibilité range-based for et algorithmes génériques
 *    - Pas de méthode Peek() alias de Front() pour sémantique plus claire
 *    - Pas de support pour les opérations en bloc (PushMultiple, PopMultiple)
 *    - Capacité fixe : pas d'expansion dynamique si besoin de plus d'espace
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter des itérateurs Forward pour parcours en ordre FIFO (défi : gestion du wrapping)
 *    - Implémenter PushMultiple(const T* data, usize count) pour insertion en bloc optimisée
 *    - Ajouter une méthode Span() retournant une vue contiguë si possible (quand pas de wrapping)
 *    - Supporter un mode "no-overwrite" : Push() retourne bool et échoue si plein
 *    - Ajouter des statistiques : peakSize(), overwriteCount() pour monitoring
 * 
 * 8. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs NkQueue : RingBuffer = capacité fixe + écrasement, Queue = dynamique + pas de perte
 *    - vs NkVector : RingBuffer = FIFO sémantique + O(1) Push/Pop, Vector = accès aléatoire + réallocation
 *    - vs std::deque : RingBuffer = plus simple, mémoire contiguë, pas d'itérateurs
 *    - Règle : utiliser NkRingBuffer quand capacité fixe acceptable et performance O(1) critique
 * 
 * 9. PATTERNS AVANCÉS :
 *    - Double buffer : deux NkRingBuffer pour swap producer/consumer sans locking
 *    - Watermarking : seuils high/low pour déclencher actions quand buffer presque plein/vide
 *    - Priority overwrite : écraser sélectivement les éléments de faible priorité (nécessite métadonnées)
 *    - Lock-free : avec atomics sur mHead/mTail pour usage multi-thread sans mutex (avancé)
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================