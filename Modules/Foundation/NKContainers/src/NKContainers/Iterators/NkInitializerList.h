/**
 * @File NkInitializerList.h
 * @Description Fournit une liste d'initialisation légère pour le framework Nkentseu, sans dépendances STL.
 * @Author TEUGUIA TADJUIDJE Rodolf
 * @Date 2025-06-10
 * @License Rihen
 */

#pragma once

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include "NkIterator.h" // Pour NkConstIterator et NkReverseIterator
#include <initializer_list>

/**
 * - nkentseu : Regroupe les composants de base du framework Nkentseu.
 *
 * @Description :
 * Ce namespace contient les classes et fonctions fondamentales pour la gestion des conteneurs,
 * des itérateurs, et des structures de données dans le framework Nkentseu. Il est conçu pour
 * être portable, performant, et indépendant de la STL.
 */
namespace nkentseu {

    /**
     * - NkInitializerList : Liste d'initialisation générique pour le framework Nkentseu.
     *
     * @Description :
     * Cette classe template fournit une liste d'initialisation légère et constante pour stocker
     * une séquence d'éléments contigus en lecture seule. Elle est conçue pour être utilisée avec
     * des tableaux statiques ou dynamiques, offrant un accès via des itérateurs et des méthodes
     * d'accès aux éléments. Optimisée pour minimiser l'empreinte mémoire et les coûts d'exécution,
     * elle s'intègre avec les itérateurs personnalisés du framework (`NkConstIterator`, `NkReverseIterator`).
     *
     * @Template :
     * - (typename T) : Type des éléments stockés dans la liste.
     *
     * @Members :
     * - (const T*) m_begin : Pointeur vers le premier élément de la liste.
     * - (const T*) m_end : Pointeur vers la position juste après le dernier élément.
     */
    template<typename T>
    class NkInitializerList {
    public:
        /**
         * @Alias ValueType
         * @Description Type des éléments stockés dans la liste.
         * @UnderlyingType T
         */
        using ValueType = T;

        /**
         * @Alias SizeType
         * @Description Type utilisé pour représenter la taille de la liste.
         * @UnderlyingType usize
         */
        using SizeType = usize;

        /**
         * @Alias Reference
         * @Description Type de référence constante vers les éléments.
         * @UnderlyingType const T&
         */
        using Reference = const T&;

        /**
         * @Alias Iterator
         * @Description Type d'itérateur constant pour parcourir la liste.
         * @UnderlyingType NkConstIterator<T>
         */
        using Iterator = NkConstIterator<T>;

        /**
         * @Alias ConstIterator
         * @Description Alias pour Iterator, pour la compatibilité avec les conventions.
         * @UnderlyingType NkConstIterator<T>
         */
        using ConstIterator = NkConstIterator<T>;

        /**
         * @Alias ReverseIterator
         * @Description Type d'itérateur inversé pour parcourir la liste à l'envers.
         * @UnderlyingType NkReverseIterator<ConstIterator>
         */
        using ReverseIterator = NkReverseIterator<ConstIterator>;

        /**
         * @Alias ConstReverseIterator
         * @Description Alias pour ReverseIterator, pour la compatibilité.
         * @UnderlyingType ReverseIterator
         */
        using ConstReverseIterator = ReverseIterator;

        /**
         * - NkInitializerList : Constructeur par défaut.
         *
         * @Description :
         * Initialise une liste vide avec des pointeurs nuls pour `m_begin` et `m_end`.
         * Utilisé lorsque aucun élément n'est fourni.
         *
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * NkInitializerList<int32_t> list; // Liste vide
         */
        constexpr NkInitializerList() noexcept : m_begin(nullptr), m_end(nullptr) {}

        /**
         * - NkInitializerList : Constructeur depuis std::initializer_list.
         *
         * @Description :
         * Permet d'utiliser les appels explicites avec std::initializer_list
         * et facilite l'interopérabilité STL -> Nkentseu.
         *
         * @param (std::initializer_list<T>) init : Liste standard source.
         */
        constexpr NkInitializerList(std::initializer_list<T> init) noexcept
            : m_begin(init.begin()), m_end(init.end()) {}

        /**
         * - NkInitializerList : Constructeur avec plage de pointeurs.
         *
         * @Description :
         * Initialise une liste à partir d'une plage définie par un pointeur de début et un pointeur de fin.
         * En mode débogage, vérifie que le pointeur de fin n'est pas avant le pointeur de début, et logue une erreur si c'est le cas.
         * Ne possède pas les données, uniquement des références aux éléments existants.
         *
         * @param (const T*) first : Pointeur vers le premier élément de la plage.
         * @param (const T*) last : Pointeur vers la position juste après le dernier élément.
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * int32_t arr[] = {1, 2, 3};
         * NkInitializerList<int32_t> list(arr, arr + 3); // Liste contenant {1, 2, 3}
         */
        constexpr NkInitializerList(const T* first, const T* last) noexcept 
            : m_begin(first), m_end(last) {
            #ifdef NKENTSEU_LOG_ENABLED
            if (m_end < m_begin) {
                NKENTSEU_LOG_ERROR("NkInitializerList: end pointer is before begin pointer.");
            }
            #endif
        }
        
        /*
        * - NkInitializerList : Constructeur avec pointeur et taille.
        *
        * @Description :
        * Initialise une liste à partir d'un pointeur vers les données et d'une taille spécifiée.
        * En mode débogage, vérifie que le pointeur de fin n'est pas avant le pointeur de début, et logue une erreur si c'est le cas.
        * Ne possède pas les données, uniquement des références aux éléments existants.
        * 
        * @param (const T*) data : Pointeur vers le premier élément des données.
        * @param (usize) size : Nombre d'éléments dans la liste.
        * @return (void) : Aucune valeur retournée.
        * 
        * Exemple :
        * int32_t arr[] = {1, 2, 3};
        * NkInitializerList<int32_t> list(arr, 3); // Liste contenant {1, 2, 3}
        */
        constexpr NkInitializerList(const T* data, usize size) noexcept 
            : m_begin(data), m_end(data + size) {
            #ifdef NKENTSEU_LOG_ENABLED
            if (m_end < m_begin) {
                NKENTSEU_LOG_ERROR("NkInitializerList: end pointer is before begin pointer.");
            }
            #endif
        }
        
        /**
         * - NkInitializerList : Constructeur avec tableau statique.
         *
         * @Description :
         * Initialise une liste à partir d'un tableau statique, en déterminant la taille automatiquement.
         * En mode débogage, vérifie que le pointeur de fin n'est pas avant le pointeur de début, et logue une erreur si c'est le cas.
         * Ne possède pas les données, uniquement des références aux éléments existants.
         *
         * @param (const T (&)[N]) arr : Tableau statique à copier dans la liste.
         * @return (void) : Aucune valeur retournée.
         *
         * Exemple :
         * int32_t arr[] = {1, 2, 3};
         * NkInitializerList<int32_t> list(arr); // Liste contenant {1, 2, 3}
         */
        template <usize N>
        constexpr NkInitializerList(const T (&arr)[N]) noexcept 
            : m_begin(arr), m_end(arr + N) {
            #ifdef NKENTSEU_LOG_ENABLED
            if (m_end < m_begin) {
                NKENTSEU_LOG_ERROR("NkInitializerList: end pointer is before begin pointer.");
            }
            #endif
        }

        /**
         * - Begin : Retourne un itérateur constant vers le début de la liste.
         *
         * @Description :
         * Fournit un itérateur constant pointant sur le premier élément de la liste.
         * Utilisé pour démarrer une itération en lecture seule.
         *
         * @return (Iterator) : Itérateur constant vers le premier élément.
         *
         * Exemple :
         * NkInitializerList<int32_t> list(arr, arr + 3);
         * auto it = list.Begin(); // Pointe sur arr[0]
         */
        [[nodiscard]] constexpr Iterator Begin() const noexcept { return Iterator(m_begin); }

        /**
         * - End : Retourne un itérateur constant vers la fin de la liste.
         *
         * @Description :
         * Fournit un itérateur constant pointant juste après le dernier élément de la liste.
         * Utilisé pour marquer la fin de l'itération.
         *
         * @return (Iterator) : Itérateur constant vers la fin.
         *
         * Exemple :
         * auto it = list.End(); // Pointe après arr[2]
         */
        [[nodiscard]] constexpr Iterator End() const noexcept { return Iterator(m_end); }

        /**
         * - CBegin : Retourne un itérateur constant explicite vers le début.
         *
         * @Description :
         * Identique à `Begin()`, mais fourni pour respecter les conventions de la STL
         * et garantir la const-correctness dans certains contextes.
         *
         * @return (ConstIterator) : Itérateur constant vers le premier élément.
         */
        [[nodiscard]] constexpr ConstIterator CBegin() const noexcept { return ConstIterator(m_begin); }

        /**
         * - CEnd : Retourne un itérateur constant explicite vers la fin.
         *
         * @Description :
         * Identique à `End()`, mais fourni pour la compatibilité et la const-correctness.
         *
         * @return (ConstIterator) : Itérateur constant vers la fin.
         */
        [[nodiscard]] constexpr ConstIterator CEnd() const noexcept { return ConstIterator(m_end); }

        /**
         * - RBegin : Retourne un itérateur inversé vers le dernier élément.
         *
         * @Description :
         * Fournit un itérateur inversé pointant sur le dernier élément de la liste,
         * permettant une itération à l'envers.
         *
         * @return (ReverseIterator) : Itérateur inversé vers le dernier élément.
         *
         * Exemple :
         * auto rit = list.RBegin(); // Pointe sur arr[2]
         */
        [[nodiscard]] constexpr ReverseIterator RBegin() const noexcept { return ReverseIterator(End()); }

        /**
         * - REnd : Retourne un itérateur inversé vers avant le premier élément.
         *
         * @Description :
         * Fournit un itérateur inversé pointant avant le premier élément, marquant la fin
         * de l'itération inversée.
         *
         * @return (ReverseIterator) : Itérateur inversé vers avant le premier élément.
         */
        [[nodiscard]] constexpr ReverseIterator REnd() const noexcept { return ReverseIterator(Begin()); }

        /**
         * - CRBegin : Retourne un itérateur inversé constant explicite vers le dernier élément.
         *
         * @Description :
         * Identique à `RBegin()`, mais fourni pour la compatibilité et la const-correctness.
         *
         * @return (ConstReverseIterator) : Itérateur inversé constant vers le dernier élément.
         */
        [[nodiscard]] constexpr ConstReverseIterator CRBegin() const noexcept { return RBegin(); }

        /**
         * - CREnd : Retourne un itérateur inversé constant explicite vers avant le premier élément.
         *
         * @Description :
         * Identique à `REnd()`, mais fourni pour la compatibilité et la const-correctness.
         *
         * @return (ConstReverseIterator) : Itérateur inversé constant vers avant le premier élément.
         */
        [[nodiscard]] constexpr ConstReverseIterator CREnd() const noexcept { return REnd(); }

        /**
         * - Data : Retourne un pointeur vers les données brutes.
         *
         * @Description :
         * Fournit un accès direct au tableau sous-jacent de la liste.
         * Ne vérifie pas la validité pour des raisons de performance.
         *
         * @return (const T*) : Pointeur vers le premier élément.
         *
         * Exemple :
         * const int32_t* ptr = list.Data(); // Pointeur sur arr[0]
         */
        [[nodiscard]] constexpr const T* Data() const noexcept { return m_begin; }

        /**
         * - Size : Retourne le nombre d'éléments dans la liste.
         *
         * @Description :
         * Calcule la taille de la liste en soustrayant les pointeurs de fin et de début.
         * Retourne toujours une valeur non négative.
         *
         * @return (SizeType) : Nombre d'éléments dans la liste.
         *
         * Exemple :
         * usize size = list.Size(); // size = 3
         */
        [[nodiscard]] constexpr SizeType Size() const noexcept { return static_cast<SizeType>(m_end - m_begin); }

        /**
         * - Empty : Vérifie si la liste est vide.
         *
         * @Description :
         * Retourne `true` si la liste ne contient aucun élément, `false` sinon.
         * Comparaison simple des pointeurs pour une performance optimale.
         *
         * @return (bool) : `true` si la liste est vide, `false` sinon.
         *
         * Exemple :
         * bool Empty = list.Empty(); // false pour une liste non vide
         */
        [[nodiscard]] constexpr bool Empty() const noexcept { return m_begin == m_end; }

        /**
         * - Front : Accède au premier élément de la liste.
         *
         * @Description :
         * Retourne une référence constante au premier élément.
         * Ne vérifie pas si la liste est vide pour des raisons de performance.
         * L'utilisateur doit s'assurer que la liste n'est pas vide.
         *
         * @return (Reference) : Référence constante au premier élément.
         *
         * Exemple :
         * int32_t first = list.Front(); // first = 1
         */
        [[nodiscard]] constexpr Reference Front() const noexcept { return *m_begin; }

        /**
         * - Back : Accède au dernier élément de la liste.
         *
         * @Description :
         * Retourne une référence constante au dernier élément.
         * Ne vérifie pas si la liste est vide; l'utilisateur doit vérifier.
         *
         * @return (Reference) : Référence constante au dernier élément.
         *
         * Exemple :
         * int32_t last = list.Back(); // last = 3
         */
        [[nodiscard]] constexpr Reference Back() const noexcept { return *(m_end - 1); }

        /**
         * - operator[] : Accède à un élément par index.
         *
         * @Description :
         * Retourne une référence constante à l'élément à l'index spécifié.
         * Ne vérifie pas les bornes pour la performance; l'utilisateur doit s'assurer
         * que l'index est valide.
         *
         * @param (SizeType) index : Index de l'élément à accéder.
         * @return (Reference) : Référence constante à l'élément.
         *
         * Exemple :
         * int32_t value = list[1]; // value = 2
         */
        [[nodiscard]] constexpr Reference operator[](SizeType index) const noexcept { return m_begin[index]; }

        /**
         * - operator== : Compare l'égalité avec une autre liste.
         *
         * @Description :
         * Vérifie si deux listes ont la même taille et les mêmes éléments.
         * Compare chaque élément séquentiellement; retourne `false` dès qu'une différence est trouvée.
         *
         * @param (const NkInitializerList&) other : Autre liste à comparer.
         * @return (bool) : `true` si les listes sont égales, `false` sinon.
         *
         * Exemple :
         * NkInitializerList<int32_t> list2(arr, arr + 3);
         * bool equal = (list == list2); // true
         */
        [[nodiscard]] constexpr bool operator==(const NkInitializerList& other) const noexcept {
            if (Size() != other.Size()) return false;
            for (SizeType i = 0; i < Size(); ++i) {
                if (m_begin[i] != other.m_begin[i]) return false;
            }
            return true;
        }

        /**
         * - operator!= : Compare la non-égalité avec une autre liste.
         *
         * @Description :
         * Retourne l'opposé du résultat de `operator==`.
         * Fourni pour la commodité et la cohérence avec les conventions.
         *
         * @param (const NkInitializerList&) other : Autre liste à comparer.
         * @return (bool) : `true` si les listes sont différentes, `false` sinon.
         *
         * Exemple :
         * int32_t arr2[] = {1, 2, 4};
         * NkInitializerList<int32_t> list2(arr2, arr2 + 3);
         * bool notEqual = (list != list2); // true
         */
        [[nodiscard]] constexpr bool operator!=(const NkInitializerList& other) const noexcept {
            return !(*this == other);
        }

        [[nodiscard]] constexpr NkInitializerList<T> operator=(const NkInitializerList<T>& other) noexcept {
            m_begin = other.m_begin;
            m_end = other.m_end;
            return *this;
        }

        [[nodiscard]] constexpr NkInitializerList<T> operator=(std::initializer_list<T> init) noexcept {
            m_begin = init.begin();
            m_end = init.end();
            return *this;
        }

        [[nodiscard]] constexpr NkInitializerList<T> operator=(const std::initializer_list<T>& init) noexcept {
            m_begin = init.begin();
            m_end = init.end();
            return *this;
        }

    private:
        const T* m_begin;
        const T* m_end;
    };

    /**
     * - NkInitializerListHelper : Aide à la création de NkInitializerList à partir de tableaux.
     *
     * @Description :
     * Cette structure template est utilisée pour faciliter la création de `NkInitializerList`
     * à partir de tableaux statiques. Elle permet de convertir un tableau en une liste d'initialisation
     * sans nécessiter de données supplémentaires, en copiant les éléments dans un tableau interne.
     *
     * @Template :
     * - (typename T) : Type des éléments de la liste.
     * - (usize N) : Nombre d'éléments dans le tableau.
     */
    template<typename T, usize N>
    struct NkInitializerListHelper {
        T array[N];
        constexpr NkInitializerListHelper(const T (&arr)[N]) noexcept {
            for (usize i = 0; i < N; ++i) {
                array[i] = arr[i];
            }
        }
        constexpr operator NkInitializerList<T>() const noexcept {
            return NkInitializerList<T>(array, array + N);
        }
    };

    /**
     * - NkInitializerListHelper : Spécialisation pour les listes vides.
     *
     * @Description :
     * Cette spécialisation de `NkInitializerListHelper` est utilisée pour gérer le cas
     * où la liste d'initialisation est vide (0 éléments). Elle permet de créer une
     * `NkInitializerList` vide sans nécessiter de données supplémentaires.
     *
     * @Template :
     * - (typename T) : Type des éléments de la liste.
     */
    template<typename T>
    struct NkInitializerListHelper<T, 0> {
        constexpr NkInitializerListHelper() noexcept {}
        template<usize N>
        constexpr NkInitializerListHelper(const T (&arr)[N]) noexcept {(void)arr;}
        constexpr operator NkInitializerList<T>() const noexcept {
            return NkInitializerList<T>();
        }
    };

    /**
     * - MakeInitializerList : Crée une NkInitializerList à partir de données brutes.
     *
     * @Description :
     * Cette fonction template utilitaire construit une `NkInitializerList` à partir
     * d'une liste d'arguments. Elle permet de créer facilement des listes d'initialisation
     * sans avoir à spécifier explicitement les types ou la taille.
     *
     * @Template :
     * - (typename T) : Type des éléments de la liste.
     * - (typename... Args) : Types des arguments à inclure dans la liste.
     *
     * @param (Args&&...) args : Arguments à inclure dans la liste.
     * @return (NkInitializerList<T, sizeof...(Args)>) : Liste d'initialisation contenant les éléments.
     *
     * Exemple :
     * auto list = MakeInitializerList<int32_t>(1, 2, 3); // Crée une liste {1, 2, 3}
     */
    template<typename T, typename... Args>
    constexpr NkInitializerListHelper<T, sizeof...(Args)> NkMakeInitializerList(Args&&... args) noexcept {
        return NkInitializerListHelper<T, sizeof...(Args)>({static_cast<T>(args)...});
    }

    /**
     * - MakeInitializerList : Crée une NkInitializerList à partir de données brutes.
     *
     * @Description :
     * Cette fonction template utilitaire construit une `NkInitializerList` à partir
     * d'un pointeur vers des données et d'une taille. Elle calcule le pointeur de fin
     * en ajoutant la taille au pointeur de début, simplifiant la création de listes.
     *
     * @Template :
     * - (typename T) : Type des éléments de la liste.
     *
     * @param (const T*) data : Pointeur vers le début des données.
     * @param (usize) size : Nombre d'éléments dans les données.
     * @return (NkInitializerList<T>) : Liste d'initialisation contenant les éléments.
     *
     * Exemple :
     * int32_t arr[] = {1, 2, 3};
     * auto list = MakeInitializerList(arr, 3); // Crée une liste {1, 2, 3}
     * NkForeach(list, [](int32_t value) { printf("%d\n", value); }); // Affiche : 1, 2, 3
     */
    template <typename T, usize N>
    constexpr NkInitializerList<T> NkMakeInitializerList(const T (&arr)[N]) noexcept {
        return NkInitializerList<T>(arr);
    }

    /*
        * - MakeInitializerList : Crée une NkInitializerList à partir d'un pointeur et d'une taille.
        *
        * @Description :
        * Cette fonction template utilitaire construit une `NkInitializerList` à partir
        * d'un pointeur vers des données et d'une taille. Elle est utile pour créer des listes
        * à partir de tableaux dynamiques ou de buffers.
        *
        * @Template :
        * - (typename T) : Type des éléments de la liste.
        *
        * @param (const T*) data : Pointeur vers le début des données.
        * @param (usize) size : Nombre d'éléments dans les données.
        * @return (NkInitializerList<T>) : Liste d'initialisation contenant les éléments.
        *
        * Exemple :
        * int32_t arr[] = {1, 2, 3};
        * auto list = MakeInitializerList(arr, 3); // Crée une liste {1, 2, 3}
    */
    template <typename T>
    constexpr NkInitializerList<T> NkMakeInitializerList(const T* data, usize size) noexcept {
        return NkInitializerList<T>(data, size);
    }

    /**
     * - NkMakeInitializerList : Crée une NkInitializerList vide.
     *
     * @Description :
     * Cette fonction template utilitaire crée une `NkInitializerList` vide, sans éléments.
     * Utile pour initialiser des listes sans données, permettant une utilisation uniforme
     * avec les autres fonctions de création de listes.
     *
     * @Template :
     * - (typename T) : Type des éléments de la liste.
     *
     * @return (NkInitializerList<T>) : Liste d'initialisation vide.
     *
     * Exemple :
     * auto emptyList = NkMakeInitializerList<int32_t>(); // Crée une liste vide
     */
    template <typename T>
    constexpr NkInitializerList<T> NkMakeInitializerList() noexcept {
        return NkInitializerList<T>();
    }

    template <typename T>
    constexpr NkInitializerList<T> NkMakeInitializerList(std::initializer_list<T> init) noexcept {
        return NkInitializerList<T>(init);
    }

    NKENTSEU_CONTAINERS_API template <class _Elem>
    NKENTSEU_NODISCARD constexpr const _Elem* begin(NkInitializerList<_Elem> _Ilist) noexcept {
        return _Ilist.Begin();
    }

    NKENTSEU_CONTAINERS_API template <class _Elem>
    NKENTSEU_NODISCARD constexpr const _Elem* end(NkInitializerList<_Elem> _Ilist) noexcept {
        return _Ilist.End();
    }
} // namespace nkentseu
