// -----------------------------------------------------------------------------
// FICHIER: NKContainers/Containers/Views/NkSpan.h
// DESCRIPTION: Vue non-possessive sur une séquence contiguë d'éléments
//              Équivalent à std::span (C++20) sans dépendance STL
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H
    #define NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H

    // -------------------------------------------------------------------------
    // INCLUSIONS DES DÉPENDANCES DU PROJET
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKContainers/NkContainersApi.h"
    #include "NKCore/NkTraits.h"
    #include "NKCore/Assert/NkAssert.h"

    // -------------------------------------------------------------------------
    // NAMESPACE PRINCIPAL DU PROJET
    // -------------------------------------------------------------------------
    namespace nkentseu
    {
        // =====================================================================
        // CLASSE TEMPLATE : NkSpan<T>
        // =====================================================================

        /**
         * @brief Vue légère non-possessive sur une séquence contiguë d'éléments
         * 
         * Cette classe fournit une vue en lecture/écriture sur un tableau contigu
         * sans en posséder la mémoire. Idéale pour passer des séquences en
         * paramètres sans copie, avec support natif des tableaux C, vecteurs,
         * et autres conteneurs à accès aléatoire.
         * 
         * @tparam T Type des éléments dans la séquence (peut être const)
         * 
         * @par Caractéristiques :
         * - Non-possessif : ne libère jamais la mémoire pointée
         * - Léger : taille fixe = sizeof(T*) + sizeof(usize)
         * - Zéro allocation : toutes les opérations sont O(1)
         * - Compatible range-based for : fournit begin()/end()
         * - Safe par défaut : assertions NKENTSEU_ASSERT sur les accès
         * 
         * @note Sécurité : la vue ne garantit pas la durée de vie des données.
         *       L'utilisateur doit s'assurer que la source reste valide.
         * 
         * @par Exemple d'utilisation :
         * @code
         *   void Process(NkSpan<int> data) {
         *       for (int& val : data) {
         *           val *= 2;  // Modification in-place
         *       }
         *   }
         *   
         *   NkVector<int> vec = {1, 2, 3};
         *   int arr[] = {4, 5, 6};
         *   
         *   Process(vec);  // OK : conversion implicite depuis conteneur
         *   Process(arr);  // OK : conversion implicite depuis tableau C
         * @endcode
         */
        template <typename T>
        class NkSpan
        {
        public:
            // -----------------------------------------------------------------
            // TYPES ET ALIASES PUBLIQUES
            // -----------------------------------------------------------------

            /**
             * @brief Alias du type d'élément géré par cette vue
             * @note Inclut les qualifiers const/volatile du template parameter
             */
            using ElementType = T;

            /**
             * @brief Alias du type de valeur (sans qualifiers)
             * @note Utile pour les opérations nécessitant le type "nu"
             */
            using ValueType = traits::NkRemoveCV<T>;

            /**
             * @brief Alias du type utilisé pour les tailles et indices
             */
            using SizeType = usize;

            /**
             * @brief Alias pour référence modifiable vers un élément
             */
            using Reference = T&;

            /**
             * @brief Alias pour référence constante vers un élément
             */
            using ConstReference = const T&;

            /**
             * @brief Alias pour pointeur modifiable vers un élément
             */
            using Pointer = T*;

            /**
             * @brief Alias pour pointeur constant vers un élément
             */
            using ConstPointer = const T*;

            /**
             * @brief Alias pour itérateur modifiable (pointeur brut)
             * @note Permet l'arithmétique de pointeurs et l'accès direct
             */
            using Iterator = T*;

            /**
             * @brief Alias pour itérateur constant (pointeur brut const)
             */
            using ConstIterator = const T*;

        private:
            // -----------------------------------------------------------------
            // MEMBRES PRIVÉS : données internes
            // -----------------------------------------------------------------

            /**
             * @brief Pointeur vers le premier élément de la séquence
             * @note Peut être nullptr si la vue est vide (Size() == 0)
             */
            T* mData;

            /**
             * @brief Nombre d'éléments dans la vue
             * @note Toujours >= 0, utilisé pour les vérifications de borne
             */
            SizeType mSize;

        public:
            // =================================================================
            // SECTION 1 : CONSTRUCTEURS ET ASSIGNATION
            // =================================================================

            // -----------------------------------------------------------------
            // Constructeur par défaut : vue vide
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue vide pointant vers nullptr
             * @note Opération noexcept, garantie constexpr
             * @par Exemple :
             * @code
             *   NkSpan<int> empty;  // empty.Size() == 0, empty.Data() == nullptr
             * @endcode
             */
            NKENTSEU_CONSTEXPR NkSpan() NKENTSEU_NOEXCEPT
                : mData(nullptr)
                , mSize(0)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur depuis pointeur + taille explicite
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue depuis un pointeur et un nombre d'éléments
             * @param data Pointeur vers le premier élément de la séquence
             * @param size Nombre d'éléments dans la vue
             * @note Aucune vérification de validité du pointeur n'est effectuée
             * @warning Comportement indéfini si data == nullptr et size > 0
             * @par Exemple :
             * @code
             *   int* buffer = GetBuffer();
             *   NkSpan<int> view(buffer, 100);  // Vue sur 100 éléments
             * @endcode
             */
            NKENTSEU_CONSTEXPR NkSpan(T* data, SizeType size) NKENTSEU_NOEXCEPT
                : mData(data)
                , mSize(size)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur depuis tableau C de taille connue (template)
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue depuis un tableau C statique
             * @tparam N Taille du tableau (déduite automatiquement)
             * @param arr Référence vers le tableau C source
             * @note Déduit automatiquement la taille via le template parameter N
             * @par Exemple :
             * @code
             *   int values[] = {1, 2, 3, 4, 5};
             *   NkSpan<int> span(values);  // size == 5 automatiquement
             * @endcode
             */
            template <SizeType N>
            NKENTSEU_CONSTEXPR NkSpan(T (&arr)[N]) NKENTSEU_NOEXCEPT
                : mData(arr)
                , mSize(N)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur depuis conteneur type "vector" (Data()/Size())
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue depuis un conteneur compatible
             * @tparam Container Type du conteneur source (déduit automatiquement)
             * @param container Référence vers le conteneur source
             * @note Requiert que Container ait les méthodes Data() et Size()
             * @par Exemple :
             * @code
             *   NkVector<float> vec = {1.0f, 2.0f, 3.0f};
             *   NkSpan<float> span(vec);  // Conversion implicite
             * @endcode
             */
            template <typename Container>
            NKENTSEU_CONSTEXPR NkSpan(Container& container) NKENTSEU_NOEXCEPT
                : mData(container.Data())
                , mSize(container.Size())
            {
            }

            // -----------------------------------------------------------------
            // Constructeur de copie
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue par copie d'une autre vue
             * @param other Vue source à copier
             * @note Copie superficielle : seuls le pointeur et la taille sont copiés
             * @par Exemple :
             * @code
             *   NkSpan<int> original = GetSpan();
             *   NkSpan<int> copy = original;  // Même données, vue indépendante
             * @endcode
             */
            NKENTSEU_CONSTEXPR NkSpan(const NkSpan& other) NKENTSEU_NOEXCEPT
                : mData(other.mData)
                , mSize(other.mSize)
            {
            }

            // -----------------------------------------------------------------
            // Opérateur d'assignation par copie
            // -----------------------------------------------------------------
            /**
             * @brief Assigne le contenu d'une autre vue
             * @param other Vue source à copier
             * @return Référence vers this pour chaînage
             * @note Opération noexcept, copie uniquement les métadonnées
             */
            NKENTSEU_CONSTEXPR NkSpan& operator=(const NkSpan& other) NKENTSEU_NOEXCEPT
            {
                mData = other.mData;
                mSize = other.mSize;
                return *this;
            }

            // =================================================================
            // SECTION 2 : ACCÈS AUX ÉLÉMENTS
            // =================================================================

            // -----------------------------------------------------------------
            // Opérateur d'indice avec vérification debug
            // -----------------------------------------------------------------
            /**
             * @brief Accède à l'élément à l'index spécifié (avec assertion)
             * @param index Position de l'élément (0 <= index < Size())
             * @return Référence modifiable vers l'élément demandé
             * @warning NKENTSEU_ASSERT si index >= Size() (mode debug uniquement)
             * @note Complexité : O(1), accès direct via arithmétique de pointeurs
             * @par Exemple :
             * @code
             *   NkSpan<int> span = GetSpan();
             *   span[0] = 42;  // Modifie le premier élément
             *   int val = span[span.Size() - 1];  // Accès au dernier
             * @endcode
             */
            Reference operator[](SizeType index) const NKENTSEU_NOEXCEPT
            {
                NKENTSEU_ASSERT(index < mSize);
                return mData[index];
            }

            // -----------------------------------------------------------------
            // Accès au premier élément
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence au premier élément de la vue
             * @return Référence modifiable vers mData[0]
             * @warning Assertion si la vue est vide (Size() == 0)
             * @note Plus sémantique que operator[](0) pour la lisibilité
             */
            Reference Front() const NKENTSEU_NOEXCEPT
            {
                NKENTSEU_ASSERT(mSize > 0);
                return mData[0];
            }

            // -----------------------------------------------------------------
            // Accès au dernier élément
            // -----------------------------------------------------------------
            /**
             * @brief Retourne une référence au dernier élément de la vue
             * @return Référence modifiable vers mData[Size()-1]
             * @warning Assertion si la vue est vide (Size() == 0)
             * @note Équivalent à operator[](Size() - 1) mais plus expressif
             */
            Reference Back() const NKENTSEU_NOEXCEPT
            {
                NKENTSEU_ASSERT(mSize > 0);
                return mData[mSize - 1];
            }

            // -----------------------------------------------------------------
            // Accès au pointeur de données brut
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le pointeur vers les données sous-jacentes
             * @return Pointeur vers le premier élément, ou nullptr si vide
             * @note noexcept, compatible avec les API C attendants T*
             * @par Exemple d'interopérabilité C :
             * @code
             *   void LegacyAPI(int* data, usize count);
             *   NkSpan<int> span = GetSpan();
             *   LegacyAPI(span.Data(), span.Size());
             * @endcode
             */
            NKENTSEU_CONSTEXPR Pointer Data() const NKENTSEU_NOEXCEPT
            {
                return mData;
            }

            // =================================================================
            // SECTION 3 : ITÉRATEURS (COMPATIBILITÉ RANGE-BASED FOR)
            // =================================================================

            // -----------------------------------------------------------------
            // Itérateur de début (pointeur vers premier élément)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un itérateur vers le premier élément
             * @return Pointeur vers mData[0]
             * @note Permet l'utilisation dans les boucles range-based for C++11
             * @par Exemple :
             * @code
             *   for (int& val : span) {
             *       Process(val);
             *   }
             * @endcode
             */
            NKENTSEU_CONSTEXPR Iterator begin() const NKENTSEU_NOEXCEPT
            {
                return mData;
            }

            // -----------------------------------------------------------------
            // Itérateur de fin (pointeur après le dernier élément)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un itérateur vers la position après le dernier élément
             * @return Pointeur vers mData[Size()]
             * @note Ne doit jamais être déréférencé, sert uniquement de borne de fin
             */
            NKENTSEU_CONSTEXPR Iterator end() const NKENTSEU_NOEXCEPT
            {
                return mData + mSize;
            }

            // =================================================================
            // SECTION 4 : CAPACITÉ ET INFORMATIONS SUR LA VUE
            // =================================================================

            // -----------------------------------------------------------------
            // Nombre d'éléments dans la vue
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le nombre d'éléments dans la vue
             * @return Valeur de type SizeType
             * @note Complexité : O(1), valeur pré-calculée au construction
             */
            NKENTSEU_CONSTEXPR SizeType Size() const NKENTSEU_NOEXCEPT
            {
                return mSize;
            }

            // -----------------------------------------------------------------
            // Taille en octets de la séquence
            // -----------------------------------------------------------------
            /**
             * @brief Retourne la taille totale en octets des données
             * @return Size() * sizeof(T)
             * @note Utile pour les opérations mémoire (memcpy, allocation, etc.)
             * @par Exemple :
             * @code
             *   NkSpan<char> bytes = GetByteSpan();
             *   memcpy(dest, bytes.Data(), bytes.SizeBytes());
             * @endcode
             */
            NKENTSEU_CONSTEXPR SizeType SizeBytes() const NKENTSEU_NOEXCEPT
            {
                return mSize * sizeof(T);
            }

            // -----------------------------------------------------------------
            // Vérification de vacuité
            // -----------------------------------------------------------------
            /**
             * @brief Indique si la vue ne contient aucun élément
             * @return true si Size() == 0, false sinon
             * @note Plus efficace et lisible que Size() == 0
             * @par Exemple :
             * @code
             *   if (span.Empty()) {
             *       return;  // Rien à traiter
             *   }
             * @endcode
             */
            NKENTSEU_CONSTEXPR bool Empty() const NKENTSEU_NOEXCEPT
            {
                return mSize == 0;
            }

            // =================================================================
            // SECTION 5 : SOUS-VUES (SUBVIEWS) — SANS COPIE DE DONNÉES
            // =================================================================

            // -----------------------------------------------------------------
            // Extraction du préfixe : premiers éléments
            // -----------------------------------------------------------------
            /**
             * @brief Crée une sous-vue contenant les 'count' premiers éléments
             * @param count Nombre d'éléments à inclure dans la sous-vue
             * @return Nouvelle instance NkSpan pointant au même début que this
             * @note Aucune copie de données : la sous-vue partage la mémoire source
             * @warning Assertion si count > Size()
             * @par Exemple :
             * @code
             *   NkSpan<int> data = GetSpan();  // {1,2,3,4,5}
             *   auto header = data.First(2);   // {1,2}
             * @endcode
             */
            NkSpan First(SizeType count) const
            {
                NKENTSEU_ASSERT(count <= mSize);
                return NkSpan(mData, count);
            }

            // -----------------------------------------------------------------
            // Extraction du suffixe : derniers éléments
            // -----------------------------------------------------------------
            /**
             * @brief Crée une sous-vue contenant les 'count' derniers éléments
             * @param count Nombre d'éléments à inclure dans la sous-vue
             * @return Nouvelle instance NkSpan pointant vers la fin de this
             * @note Aucune copie de données : la sous-vue partage la mémoire source
             * @warning Assertion si count > Size()
             * @par Exemple :
             * @code
             *   NkSpan<int> data = GetSpan();  // {1,2,3,4,5}
             *   auto footer = data.Last(2);    // {4,5}
             * @endcode
             */
            NkSpan Last(SizeType count) const
            {
                NKENTSEU_ASSERT(count <= mSize);
                return NkSpan(mData + (mSize - count), count);
            }

            // -----------------------------------------------------------------
            // Extraction d'une sous-vue arbitraire
            // -----------------------------------------------------------------
            /**
             * @brief Crée une sous-vue commençant à 'offset' avec 'count' éléments
             * @param offset Position de départ de la sous-vue dans la vue source
             * @param count Nombre d'éléments à inclure dans la sous-vue
             * @return Nouvelle instance NkSpan pointant dans les mêmes données
             * @note Aucune copie de données, opération O(1)
             * @warning Assertions si offset > Size() ou count > Size() - offset
             * @par Exemple :
             * @code
             *   NkSpan<int> data = GetSpan();  // {0,1,2,3,4,5,6,7,8,9}
             *   auto middle = data.Subspan(3, 4);  // {3,4,5,6}
             * @endcode
             */
            NkSpan Subspan(SizeType offset, SizeType count) const
            {
                NKENTSEU_ASSERT(offset <= mSize);
                NKENTSEU_ASSERT(count <= mSize - offset);
                return NkSpan(mData + offset, count);
            }
        };

        // =====================================================================
        // SECTION 6 : GUIDES DE DÉDUCTION DE TEMPLATE (C++17+)
        // =====================================================================

        // ---------------------------------------------------------------------
        // Guides de déduction pour construction depuis tableau C
        // ---------------------------------------------------------------------

        /**
         * @brief Guide de déduction : NkSpan depuis tableau C
         * @tparam T Type des éléments du tableau
         * @tparam N Taille du tableau (déduite automatiquement)
         * @param arr Référence vers le tableau C
         * @return NkSpan<T> avec Size() == N
         * @note Permet : NkSpan span(arr); sans spécifier le template parameter
         */
        template <typename T, usize N>
        NkSpan(T (&)[N]) -> NkSpan<T>;

        /**
         * @brief Guide de déduction : NkSpan depuis conteneur compatible
         * @tparam Container Type du conteneur source
         * @param cont Référence vers le conteneur
         * @return NkSpan<typename Container::ValueType>
         * @note Requiert que Container définisse ValueType, Data() et Size()
         */
        template <typename Container>
        NkSpan(Container&) -> NkSpan<typename Container::ValueType>;


        // =====================================================================
        // SECTION 7 : FONCTIONS HELPERS DE CRÉATION
        // =====================================================================

        // ---------------------------------------------------------------------
        // Helper : création depuis pointeur + taille
        // ---------------------------------------------------------------------
        /**
         * @brief Factory function pour créer un NkSpan depuis pointeur et taille
         * @tparam T Type des éléments (déduit automatiquement)
         * @param data Pointeur vers le premier élément
         * @param size Nombre d'éléments dans la vue
         * @return Instance NkSpan<T> initialisée
         * @note Utile pour éviter la syntaxe verbose du constructeur template
         * @par Exemple :
         * @code
         *   int* buffer = GetBuffer();
         *   auto span = NkMakeSpan(buffer, 100);  // Plus lisible que NkSpan<int>(...)
         * @endcode
         */
        template <typename T>
        NKENTSEU_CONSTEXPR NkSpan<T> NkMakeSpan(T* data, usize size) NKENTSEU_NOEXCEPT
        {
            return NkSpan<T>(data, size);
        }

        // ---------------------------------------------------------------------
        // Helper : création depuis tableau C de taille connue
        // ---------------------------------------------------------------------
        /**
         * @brief Factory function pour créer un NkSpan depuis un tableau C
         * @tparam T Type des éléments du tableau
         * @tparam N Taille du tableau (déduite automatiquement)
         * @param arr Référence vers le tableau C source
         * @return Instance NkSpan<T> avec Size() == N
         * @note Déduit automatiquement la taille, évite les erreurs manuelles
         * @par Exemple :
         * @code
         *   int values[] = {1, 2, 3};
         *   auto span = NkMakeSpan(values);  // Size() == 3 automatiquement
         * @endcode
         */
        template <typename T, usize N>
        NKENTSEU_CONSTEXPR NkSpan<T> NkMakeSpan(T (&arr)[N]) NKENTSEU_NOEXCEPT
        {
            return NkSpan<T>(arr);
        }

        // ---------------------------------------------------------------------
        // Helper : création depuis conteneur compatible
        // ---------------------------------------------------------------------
        /**
         * @brief Factory function pour créer un NkSpan depuis un conteneur
         * @tparam Container Type du conteneur source (déduit automatiquement)
         * @param cont Référence vers le conteneur source
         * @return Instance NkSpan<typename Container::ValueType>
         * @note Alternative explicite à la conversion implicite via constructeur
         * @par Exemple :
         * @code
         *   NkVector<float> vec = {1.0f, 2.0f};
         *   auto span = NkMakeSpan(vec);  // NkSpan<float> sur les données de vec
         * @endcode
         */
        template <typename Container>
        NKENTSEU_CONSTEXPR NkSpan<typename Container::ValueType> NkMakeSpan(Container& cont) NKENTSEU_NOEXCEPT
        {
            return NkSpan<typename Container::ValueType>(cont);
        }

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_CONTAINERS_VIEWS_NKSPAN_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // -------------------------------------------------------------------------
    // Exemple 1 : Passage de séquences à des fonctions génériques
    // -------------------------------------------------------------------------
    #include "NKContainers/Containers/Views/NkSpan.h"
    using namespace nkentseu;

    // Fonction acceptant n'importe quelle séquence contiguë
    void ProcessData(NkSpan<float> data)
    {
        for (float& val : data)
        {
            val = val * 2.0f + 1.0f;  // Transformation in-place
        }
    }

    // Usage avec différents types de sources
    void Example1()
    {
        // Depuis NkVector
        NkVector<float> vec = {1.0f, 2.0f, 3.0f};
        ProcessData(vec);  // Conversion implicite

        // Depuis tableau C
        float arr[] = {4.0f, 5.0f, 6.0f};
        ProcessData(arr);  // Conversion implicite

        // Depuis pointeur + taille manuelle
        float* buffer = GetFloatBuffer();
        ProcessData(NkSpan<float>(buffer, 100));
    }

    // -------------------------------------------------------------------------
    // Exemple 2 : Sous-vues et découpage de données
    // -------------------------------------------------------------------------
    void Example2()
    {
        NkSpan<int> data = GetDataSpan();  // {0,1,2,3,4,5,6,7,8,9}

        // Extraire l'en-tête (premiers 3 éléments)
        auto header = data.First(3);  // {0,1,2}

        // Extraire le pied de page (derniers 3 éléments)
        auto footer = data.Last(3);   // {7,8,9}

        // Extraire une section centrale
        auto middle = data.Subspan(3, 4);  // {3,4,5,6}

        // Les sous-vues partagent les données originales (zéro copie)
        header[0] = 999;  // Modifie aussi data[0]
    }

    // -------------------------------------------------------------------------
    // Exemple 3 : Itération et algorithmes
    // -------------------------------------------------------------------------
    void Example3()
    {
        NkSpan<int> values = GetValues();

        // Range-based for loop (C++11)
        for (int& val : values)
        {
            if (val < 0) val = 0;  // Clamp à zéro
        }

        // Avec itérateurs explicites
        auto it = std::find(values.begin(), values.end(), 42);
        if (it != values.end())
        {
            *it = -1;  // Remplacer la première occurrence de 42
        }

        // Algorithme STL sur span
        std::sort(values.begin(), values.end());
    }

    // -------------------------------------------------------------------------
    // Exemple 4 : Interopérabilité avec API C
    // -------------------------------------------------------------------------
    void Example4()
    {
        // API C attendants pointeur + taille
        void LegacyProcess(int* data, usize count);

        NkSpan<int> span = GetSpan();

        // Passage sécurisé aux API C
        if (!span.Empty())
        {
            LegacyProcess(span.Data(), span.Size());
        }

        // Copie mémoire avec SizeBytes()
        uint8_t* dest = GetDestinationBuffer();
        NkSpan<uint8_t> bytes = GetByteSpan();
        memcpy(dest, bytes.Data(), bytes.SizeBytes());
    }

    // -------------------------------------------------------------------------
    // Exemple 5 : Span const pour lecture seule
    // -------------------------------------------------------------------------
    void ReadOnlyProcess(NkSpan<const int> data)
    {
        // data[i] retourne const int&, modification interdite
        for (const int& val : data)
        {
            Sum += val;  // Lecture uniquement
        }
    }

    void Example5()
    {
        NkVector<int> mutableVec = {1, 2, 3};
        
        // Conversion implicite vers span const
        ReadOnlyProcess(mutableVec);  // OK : NkVector → NkSpan<const int>

        // Impossible de modifier via le span const :
        // ReadOnlyProcess(mutableVec)[0] = 999;  // Erreur de compilation
    }

    // -------------------------------------------------------------------------
    // Exemple 6 : Helpers de création NkMakeSpan
    // -------------------------------------------------------------------------
    void Example6()
    {
        // Depuis tableau C : déduction automatique du type et de la taille
        int staticArray[] = {10, 20, 30};
        auto span1 = NkMakeSpan(staticArray);  // NkSpan<int>, Size() == 3

        // Depuis pointeur + taille explicite
        int* dynamicBuffer = AllocateInts(50);
        auto span2 = NkMakeSpan(dynamicBuffer, 50);  // NkSpan<int>

        // Depuis conteneur : déduction du type d'élément
        NkVector<double> vec = {1.1, 2.2, 3.3};
        auto span3 = NkMakeSpan(vec);  // NkSpan<double>
    }

    // -------------------------------------------------------------------------
    // Exemple 7 : Bonnes pratiques et pièges à éviter
    // -------------------------------------------------------------------------

    // ✅ BON : La span est utilisée tant que la source est valide
    {
        NkVector<int> temp = {1, 2, 3};
        NkSpan<int> safeSpan = temp;
        UseSpan(safeSpan);  // OK : temp existe encore
    }

    // ❌ MAUVAIS : La span survit à la destruction de sa source
    {
        NkSpan<int> dangling;
        {
            NkVector<int> temp = {1, 2, 3};
            dangling = temp;  // dangling pointe vers les données de temp
        }
        // temp est détruit → dangling pointe vers mémoire libérée → UB !
        UseSpan(dangling);  // ⚠️ Comportement indéfini
    }

    // ✅ BON : Utiliser Empty() avant Front()/Back()
    void SafeAccess(NkSpan<int> span)
    {
        if (!span.Empty())
        {
            int first = span.Front();  // Sécurisé
            Process(first);
        }
    }

    // ✅ BON : Préférer les sous-vues pour éviter les copies
    void ProcessChunks(NkSpan<int> data)
    {
        // Traiter par blocs de 16 éléments sans allocation
        for (usize i = 0; i < data.Size(); i += 16)
        {
            usize remaining = data.Size() - i;
            usize chunkSize = (remaining < 16) ? remaining : 16;
            
            auto chunk = data.Subspan(i, chunkSize);  // Vue, pas copie
            ProcessChunk(chunk);  // Traitement du bloc
        }
    }

    // ⚠️ ATTENTION : NkSpan n'est pas thread-safe
    // Partager une instance entre threads nécessite synchronisation externe
    // Privilégier la copie de la vue (pas des données) pour la sécurité :
    //   NkSpan<T> localCopy = sharedSpan;  // Copie des métadonnées uniquement

    // -------------------------------------------------------------------------
    // Exemple 8 : Pattern "view" pour algorithmes sans allocation
    // -------------------------------------------------------------------------
    {
        // Algorithme de filtrage retournant une vue (pas un nouveau conteneur)
        NkSpan<int> FilterInPlace(NkSpan<int> data, std::function<bool(int)> pred)
        {
            usize writePos = 0;
            for (usize readPos = 0; readPos < data.Size(); ++readPos)
            {
                if (pred(data[readPos]))
                {
                    if (writePos != readPos)
                    {
                        data[writePos] = data[readPos];  // Déplacement in-place
                    }
                    ++writePos;
                }
            }
            return data.First(writePos);  // Vue sur les éléments conservés
        }

        // Usage : filtrage sans allocation mémoire supplémentaire
        NkVector<int> values = {1, -2, 3, -4, 5};
        auto positives = FilterInPlace(values, [](int v) { return v > 0; });
        // positives est une vue sur {1, 3, 5} dans le buffer original de values
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Utilisation et modification autorisées
//
// Généré par Rihen le 2026-02-09
// Date de création : 2026-02-09
// Dernière modification : 2026-04-26
// ============================================================