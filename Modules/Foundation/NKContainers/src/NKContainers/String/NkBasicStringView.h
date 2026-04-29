// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/NkBasicStringView.h
// DESCRIPTION: Vue générique sur chaîne de caractères (non-possessive, lecture seule)
//              Support multi-encodage via template CharT (char, char16_t, char32_t, wchar_t)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_STRING_NKBASICSTRINGVIEW_H
    #define NK_CONTAINERS_STRING_NKBASICSTRINGVIEW_H

    // -------------------------------------------------------------------------
    // INCLUSIONS DES DÉPENDANCES DU PROJET
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKMemory/NkFunction.h"

    // -------------------------------------------------------------------------
    // NAMESPACE PRINCIPAL DU PROJET
    // -------------------------------------------------------------------------
    namespace nkentseu
    {
        // =====================================================================
        // CLASSE TEMPLATE : BasicStringView<CharT>
        // =====================================================================

        /**
         * @brief Vue générique non-possessive sur une séquence de caractères
         * 
         * Cette classe fournit une vue en lecture seule sur une séquence de caractères
         * sans en posséder la mémoire. Idéale pour passer des chaînes en paramètres
         * sans copie ni allocation, avec support natif des encodages multiples.
         * 
         * @tparam CharT Type de caractère : char (UTF-8), char16_t (UTF-16), 
         *               char32_t (UTF-32), ou wchar_t (plateforme-dépendant)
         * 
         * @note Sécurité : la vue ne garantit pas la durée de vie des données pointées.
         *       L'utilisateur doit s'assurer que la source reste valide pendant l'usage.
         * 
         * @par Exemple d'utilisation :
         * @code
         *   const char* source = "Hello World";
         *   BasicStringView<char> view(source);
         *   // view.Length() == 11, view[0] == 'H'
         * @endcode
         */
        template <typename CharT>
        class BasicStringView
        {
        public:
            // -----------------------------------------------------------------
            // TYPES ET CONSTANTES PUBLIQUES
            // -----------------------------------------------------------------

            /**
             * @brief Alias du type de caractère géré par cette vue
             */
            using CharType = CharT;

            /**
             * @brief Alias du type utilisé pour les tailles et indices
             */
            using SizeType = usize;

            /**
             * @brief Valeur spéciale indiquant "position non trouvée" ou "fin de chaîne"
             * @note Équivalent à std::string::npos, valeur maximale de SizeType
             */
            static constexpr SizeType npos = static_cast<SizeType>(-1);

            // =================================================================
            // SECTION 1 : CONSTRUCTEURS ET ASSIGNATION
            // =================================================================

            // -----------------------------------------------------------------
            // Constructeur par défaut : vue vide (nullptr, longueur 0)
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue vide pointant vers nullptr
             * @note Opération noexcept, garantie constexpr en C++20+
             */
            NKENTSEU_CONSTEXPR BasicStringView() NKENTSEU_NOEXCEPT
                : mData(nullptr)
                , mLength(0)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur depuis C-string terminée par nul
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue depuis une chaîne C terminée par nul
             * @param str Pointeur vers la chaîne source (peut être nullptr)
             * @note Calcule automatiquement la longueur via CalculateLength()
             *       Si str == nullptr, la vue résultante est vide
             */
            NKENTSEU_CONSTEXPR BasicStringView(const CharT* str) NKENTSEU_NOEXCEPT
                : mData(str)
                , mLength(str ? CalculateLength(str) : 0)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur depuis pointeur + longueur explicite
            // -----------------------------------------------------------------
            /**
             * @brief Construit une vue depuis un pointeur et une longueur explicite
             * @param str Pointeur vers les données caractères
             * @param length Nombre de caractères dans la vue
             * @note Permet de créer des vues sur des sous-chaînes ou données binaires
             *       Aucune vérification de borne n'est effectuée (responsabilité utilisateur)
             */
            NKENTSEU_CONSTEXPR BasicStringView(const CharT* str, SizeType length) NKENTSEU_NOEXCEPT
                : mData(str)
                , mLength(length)
            {
            }

            // -----------------------------------------------------------------
            // Constructeur de copie et opérateur d'assignation (par défaut)
            // -----------------------------------------------------------------
            /**
             * @brief Constructeur de copie par défaut
             * @note Copie superficielle : seuls le pointeur et la longueur sont copiés
             */
            NKENTSEU_CONSTEXPR BasicStringView(const BasicStringView&) NKENTSEU_NOEXCEPT = default;

            /**
             * @brief Opérateur d'assignation par copie par défaut
             * @return Référence vers this pour chaînage
             */
            BasicStringView& operator=(const BasicStringView&) NKENTSEU_NOEXCEPT = default;

            // =================================================================
            // SECTION 2 : ACCÈS AUX ÉLÉMENTS
            // =================================================================

            // -----------------------------------------------------------------
            // Opérateur d'indice : accès sans vérification de borne
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère à l'index spécifié (sans vérification)
             * @param index Position du caractère (0 <= index < Length())
             * @return Référence constante vers le caractère demandé
             * @warning Comportement indéfini si index >= Length()
             * @note Préférer At() en mode debug pour la sécurité
             */
            NKENTSEU_CONSTEXPR CharT operator[](SizeType index) const NKENTSEU_NOEXCEPT
            {
                return mData[index];
            }

            // -----------------------------------------------------------------
            // Méthode At : accès avec vérification de borne (debug)
            // -----------------------------------------------------------------
            /**
             * @brief Accède au caractère à l'index spécifié (avec assertion)
             * @param index Position du caractère (0 <= index < Length())
             * @return Référence constante vers le caractère demandé
             * @note Déclenche NKENTSEU_ASSERT si index hors bornes (mode debug uniquement)
             * @par Exemple :
             * @code
             *   view.At(0);  // OK si view non vide
             *   view.At(100);  // Assertion failure si Length() <= 100
             * @endcode
             */
            NKENTSEU_CONSTEXPR CharT At(SizeType index) const
            {
                NKENTSEU_ASSERT(index < mLength);
                return mData[index];
            }

            // -----------------------------------------------------------------
            // Accès au premier caractère
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le premier caractère de la vue
             * @return Référence constante vers le caractère en position 0
             * @warning Comportement indéfini si la vue est vide
             * @note Utiliser Empty() pour vérifier avant appel en contexte incertain
             */
            NKENTSEU_CONSTEXPR CharT Front() const NKENTSEU_NOEXCEPT
            {
                return mData[0];
            }

            // -----------------------------------------------------------------
            // Accès au dernier caractère
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le dernier caractère de la vue
             * @return Référence constante vers le caractère en position Length()-1
             * @warning Comportement indéfini si la vue est vide
             * @note Équivalent à Back() dans std::string_view
             */
            NKENTSEU_CONSTEXPR CharT Back() const NKENTSEU_NOEXCEPT
            {
                return mData[mLength - 1];
            }

            // -----------------------------------------------------------------
            // Accès au pointeur de données brut
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le pointeur vers les données caractères sous-jacentes
             * @return Pointeur constant vers le premier caractère, ou nullptr si vide
             * @note La mémoire pointée n'est pas possédée par cette vue
             * @par Exemple d'interopérabilité C :
             * @code
             *   printf("%.*s", static_cast<int>(view.Length()), view.Data());
             * @endcode
             */
            NKENTSEU_CONSTEXPR const CharT* Data() const NKENTSEU_NOEXCEPT
            {
                return mData;
            }

            // =================================================================
            // SECTION 3 : CAPACITÉ ET ÉTAT
            // =================================================================

            // -----------------------------------------------------------------
            // Longueur de la vue (alias sémantique)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne le nombre de caractères dans la vue
             * @return Valeur de type SizeType représentant la longueur
             * @note Complexité : O(1), valeur pré-calculée au construction
             */
            NKENTSEU_CONSTEXPR SizeType Length() const NKENTSEU_NOEXCEPT
            {
                return mLength;
            }

            // -----------------------------------------------------------------
            // Taille de la vue (alias pour compatibilité STL)
            // -----------------------------------------------------------------
            /**
             * @brief Alias de Length() pour compatibilité avec les conteneurs STL
             * @return Nombre de caractères dans la vue
             * @note Permet d'utiliser BasicStringView dans des templates génériques
             *       attendants une méthode Size()
             */
            NKENTSEU_CONSTEXPR SizeType Size() const NKENTSEU_NOEXCEPT
            {
                return mLength;
            }

            // -----------------------------------------------------------------
            // Vérification de vacuité
            // -----------------------------------------------------------------
            /**
             * @brief Indique si la vue ne contient aucun caractère
             * @return true si Length() == 0, false sinon
             * @note Plus efficace que Length() == 0 pour la lisibilité du code
             * @par Exemple :
             * @code
             *   if (view.Empty()) { /* gérer le cas vide */ }
             * @endcode
             */
            NKENTSEU_CONSTEXPR bool Empty() const NKENTSEU_NOEXCEPT
            {
                return mLength == 0;
            }

            // =================================================================
            // SECTION 4 : MODIFICATEURS DE VUE (non-destructifs)
            // =================================================================

            // -----------------------------------------------------------------
            // Suppression de préfixe : avance le pointeur de départ
            // -----------------------------------------------------------------
            /**
             * @brief Réduit la vue en supprimant n caractères depuis le début
             * @param n Nombre de caractères à retirer du préfixe
             * @note Modifie mData et mLength, sans toucher aux données originales
             * @warning Aucun contrôle de borne : comportement indéfini si n > Length()
             * @par Exemple :
             * @code
             *   BasicStringView<char> v("Hello World");
             *   v.RemovePrefix(6);  // v contient désormais "World"
             * @endcode
             */
            NKENTSEU_CONSTEXPR void RemovePrefix(SizeType n) NKENTSEU_NOEXCEPT
            {
                mData += n;
                mLength -= n;
            }

            // -----------------------------------------------------------------
            // Suppression de suffixe : réduit la longueur
            // -----------------------------------------------------------------
            /**
             * @brief Réduit la vue en supprimant n caractères depuis la fin
             * @param n Nombre de caractères à retirer du suffixe
             * @note Modifie uniquement mLength, mData reste inchangé
             * @warning Aucun contrôle de borne : comportement indéfini si n > Length()
             * @par Exemple :
             * @code
             *   BasicStringView<char> v("Hello World");
             *   v.RemoveSuffix(6);  // v contient désormais "Hello"
             * @endcode
             */
            NKENTSEU_CONSTEXPR void RemoveSuffix(SizeType n) NKENTSEU_NOEXCEPT
            {
                mLength -= n;
            }

            // -----------------------------------------------------------------
            // Extraction de sous-vue : SubStr
            // -----------------------------------------------------------------
            /**
             * @brief Crée une nouvelle vue sur une sous-partie de la chaîne
             * @param pos Position de départ de la sous-vue (défaut : 0)
             * @param count Nombre maximal de caractères à inclure (défaut : npos = jusqu'à la fin)
             * @return Nouvelle instance BasicStringView pointant dans les mêmes données
             * @note Aucune copie de données : la nouvelle vue partage la mémoire source
             * @par Exemples :
             * @code
             *   BasicStringView<char> v("0123456789");
             *   auto sub1 = v.SubStr(2, 3);    // "234"
             *   auto sub2 = v.SubStr(5);       // "56789"
             *   auto sub3 = v.SubStr(0, npos); // copie complète (vue, pas données)
             * @endcode
             */
            NKENTSEU_CONSTEXPR BasicStringView SubStr(
                SizeType pos = 0,
                SizeType count = npos) const
            {
                SizeType rcount = (count == npos || pos + count > mLength)
                    ? mLength - pos
                    : count;
                return BasicStringView(mData + pos, rcount);
            }

            // -----------------------------------------------------------------
            // Échange de contenu entre deux vues
            // -----------------------------------------------------------------
            /**
             * @brief Échange le contenu de cette vue avec une autre
             * @param other Référence vers la vue avec laquelle échanger
             * @note Opération noexcept, utile pour les algorithmes de tri/permutation
             * @par Exemple :
             * @code
             *   BasicStringView<char> a("foo"), b("bar");
             *   a.Swap(b);  // a contient "bar", b contient "foo"
             * @endcode
             */
            void Swap(BasicStringView& other) NKENTSEU_NOEXCEPT
            {
                const CharT* tmpData = mData;
                mData = other.mData;
                other.mData = tmpData;

                SizeType tmpLen = mLength;
                mLength = other.mLength;
                other.mLength = tmpLen;
            }

            // =================================================================
            // SECTION 5 : OPÉRATIONS DE COMPARAISON ET RECHERCHE
            // =================================================================

            // -----------------------------------------------------------------
            // Comparaison lexicographique avec une autre vue
            // -----------------------------------------------------------------
            /**
             * @brief Compare cette vue avec une autre selon l'ordre lexicographique
             * @param other Vue de référence pour la comparaison
             * @return -1 si *this < other, 0 si égales, +1 si *this > other
             * @note Comparaison octet-par-octet, sensible à la casse et à l'encodage
             * @par Exemples :
             * @code
             *   BasicStringView<char> a("apple"), b("banana");
             *   a.Compare(b);  // retourne -1 (a < b)
             *   a.Compare(a);  // retourne 0 (a == a)
             * @endcode
             */
            int Compare(BasicStringView other) const NKENTSEU_NOEXCEPT
            {
                SizeType minLen = mLength < other.mLength ? mLength : other.mLength;

                for (SizeType i = 0; i < minLen; ++i)
                {
                    if (mData[i] < other.mData[i])
                    {
                        return -1;
                    }
                    if (mData[i] > other.mData[i])
                    {
                        return 1;
                    }
                }

                if (mLength < other.mLength)
                {
                    return -1;
                }
                if (mLength > other.mLength)
                {
                    return 1;
                }
                return 0;
            }

            // -----------------------------------------------------------------
            // Vérification de préfixe (vue)
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue commence par le préfixe spécifié
             * @param prefix Vue représentant le préfixe recherché
             * @return true si les premiers caractères correspondent exactement à prefix
             * @note Complexité : O(prefix.Length()), comparaison sensible à la casse
             * @par Exemples :
             * @code
             *   BasicStringView<char> url("https://example.com");
             *   url.StartsWith("https://");  // true
             *   url.StartsWith("http://");   // false
             * @endcode
             */
            bool StartsWith(BasicStringView prefix) const NKENTSEU_NOEXCEPT
            {
                if (prefix.mLength > mLength)
                {
                    return false;
                }
                return SubStr(0, prefix.mLength).Compare(prefix) == 0;
            }

            // -----------------------------------------------------------------
            // Vérification de préfixe (caractère unique)
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue commence par le caractère spécifié
             * @param ch Caractère à tester en première position
             * @return true si la vue n'est pas vide et mData[0] == ch
             * @note Plus efficace que StartsWith(BasicStringView) pour un seul caractère
             */
            bool StartsWith(CharT ch) const NKENTSEU_NOEXCEPT
            {
                return mLength > 0 && mData[0] == ch;
            }

            // -----------------------------------------------------------------
            // Vérification de suffixe (vue)
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue se termine par le suffixe spécifié
             * @param suffix Vue représentant le suffixe recherché
             * @return true si les derniers caractères correspondent exactement à suffix
             * @note Complexité : O(suffix.Length()), comparaison sensible à la casse
             * @par Exemples :
             * @code
             *   BasicStringView<char> file("document.pdf");
             *   file.EndsWith(".pdf");   // true
             *   file.EndsWith(".txt");   // false
             * @endcode
             */
            bool EndsWith(BasicStringView suffix) const NKENTSEU_NOEXCEPT
            {
                if (suffix.mLength > mLength)
                {
                    return false;
                }
                return SubStr(mLength - suffix.mLength).Compare(suffix) == 0;
            }

            // -----------------------------------------------------------------
            // Vérification de suffixe (caractère unique)
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue se termine par le caractère spécifié
             * @param ch Caractère à tester en dernière position
             * @return true si la vue n'est pas vide et mData[Length()-1] == ch
             * @note Plus efficace que EndsWith(BasicStringView) pour un seul caractère
             */
            bool EndsWith(CharT ch) const NKENTSEU_NOEXCEPT
            {
                return mLength > 0 && mData[mLength - 1] == ch;
            }

            // -----------------------------------------------------------------
            // Recherche de sous-chaîne (vue)
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue contient la sous-chaîne spécifiée
             * @param str Vue représentant la sous-chaîne recherchée
             * @return true si Find(str) != npos, false sinon
             * @note Alias sémantique pour une meilleure lisibilité du code
             */
            bool Contains(BasicStringView str) const NKENTSEU_NOEXCEPT
            {
                return Find(str) != npos;
            }

            // -----------------------------------------------------------------
            // Recherche de caractère unique
            // -----------------------------------------------------------------
            /**
             * @brief Vérifie si la vue contient le caractère spécifié
             * @param ch Caractère à rechercher
             * @return true si Find(ch) != npos, false sinon
             * @note Plus efficace que Contains(BasicStringView) pour un seul caractère
             */
            bool Contains(CharT ch) const NKENTSEU_NOEXCEPT
            {
                return Find(ch) != npos;
            }

            // -----------------------------------------------------------------
            // Recherche de sous-chaîne avec position de départ
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la première occurrence d'une sous-chaîne
             * @param str Vue représentant la sous-chaîne à trouver
             * @param pos Position de départ de la recherche (défaut : 0)
             * @return Index de la première occurrence, ou npos si non trouvé
             * @note Algorithme naïf O(n*m) : suffisant pour les petites chaînes
             *       Pour les recherches intensives, envisager un index ou algorithme KMP
             * @par Exemples :
             * @code
             *   BasicStringView<char> text("hello world");
             *   text.Find("world");        // retourne 6
             *   text.Find("o");            // retourne 4 (premier 'o')
             *   text.Find("o", 5);         // retourne 7 (deuxième 'o')
             *   text.Find("xyz");          // retourne npos
             * @endcode
             */
            SizeType Find(BasicStringView str, SizeType pos = 0) const NKENTSEU_NOEXCEPT
            {
                if (str.mLength == 0)
                {
                    return pos;
                }
                if (pos > mLength)
                {
                    return npos;
                }
                if (str.mLength > mLength - pos)
                {
                    return npos;
                }

                for (SizeType i = pos; i <= mLength - str.mLength; ++i)
                {
                    bool match = true;
                    for (SizeType j = 0; j < str.mLength; ++j)
                    {
                        if (mData[i + j] != str.mData[j])
                        {
                            match = false;
                            break;
                        }
                    }
                    if (match)
                    {
                        return i;
                    }
                }
                return npos;
            }

            // -----------------------------------------------------------------
            // Recherche de caractère avec position de départ
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la première occurrence d'un caractère
             * @param ch Caractère à rechercher
             * @param pos Position de départ de la recherche (défaut : 0)
             * @return Index de la première occurrence, ou npos si non trouvé
             * @note Complexité : O(n) dans le pire cas, optimisé pour accès séquentiel
             * @par Exemples :
             * @code
             *   BasicStringView<char> csv("a,b,c,d");
             *   csv.Find(',');           // retourne 1
             *   csv.Find(',', 2);        // retourne 3
             *   csv.Find(';');           // retourne npos
             * @endcode
             */
            SizeType Find(CharT ch, SizeType pos = 0) const NKENTSEU_NOEXCEPT
            {
                for (SizeType i = pos; i < mLength; ++i)
                {
                    if (mData[i] == ch)
                    {
                        return i;
                    }
                }
                return npos;
            }

            // -----------------------------------------------------------------
            // Recherche inverse de caractère
            // -----------------------------------------------------------------
            /**
             * @brief Recherche la dernière occurrence d'un caractère
             * @param ch Caractère à rechercher
             * @param pos Position de départ de la recherche inverse (défaut : npos = fin)
             * @return Index de la dernière occurrence <= pos, ou npos si non trouvé
             * @note Recherche effectuée de droite à gauche depuis la position spécifiée
             * @par Exemples :
             * @code
             *   BasicStringView<char> path("/usr/local/bin");
             *   path.RFind('/');              // retourne 10 (dernier '/')
             *   path.RFind('/', 5);           // retourne 4 (deuxième '/')
             *   path.RFind('x');              // retourne npos
             * @endcode
             */
            SizeType RFind(CharT ch, SizeType pos = npos) const NKENTSEU_NOEXCEPT
            {
                if (mLength == 0)
                {
                    return npos;
                }
                SizeType searchPos = (pos >= mLength) ? mLength - 1 : pos;

                for (SizeType i = searchPos + 1; i > 0; --i)
                {
                    if (mData[i - 1] == ch)
                    {
                        return i - 1;
                    }
                }
                return npos;
            }

            // =================================================================
            // SECTION 6 : SUPPORT DES ITÉRATEURS (COMPATIBILITÉ RANGE-BASED FOR)
            // =================================================================

            // -----------------------------------------------------------------
            // Itérateur de début (lecture seule)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un itérateur constant vers le premier caractère
             * @return Pointeur constant vers mData[0]
             * @note Permet l'utilisation dans les boucles range-based for C++11
             * @par Exemple :
             * @code
             *   for (char c : view) { process(c); }
             * @endcode
             */
            const CharT* begin() const NKENTSEU_NOEXCEPT
            {
                return mData;
            }

            // -----------------------------------------------------------------
            // Itérateur de fin (lecture seule)
            // -----------------------------------------------------------------
            /**
             * @brief Retourne un itérateur constant vers la position après le dernier caractère
             * @return Pointeur constant vers mData[Length()]
             * @note Ne doit jamais être déréférencé, sert uniquement de borne de fin
             */
            const CharT* end() const NKENTSEU_NOEXCEPT
            {
                return mData + mLength;
            }

        private:
            // -----------------------------------------------------------------
            // MEMBRES PRIVÉS : données internes
            // -----------------------------------------------------------------

            /**
             * @brief Pointeur vers les données caractères sous-jacentes
             * @note Non-possessif : ne pas libérer, durée de vie gérée par l'appelant
             */
            const CharT* mData;

            /**
             * @brief Nombre de caractères dans la vue
             * @note Pré-calculé pour un accès O(1) à Length()
             */
            SizeType mLength;

            // -----------------------------------------------------------------
            // Helper privé : calcul de longueur pour C-string terminée par nul
            // -----------------------------------------------------------------
            /**
             * @brief Calcule la longueur d'une C-string terminée par nul
             * @param str Pointeur vers la chaîne à mesurer (peut être nullptr)
             * @return Nombre de caractères avant le nul terminal, ou 0 si nullptr
             * @note Complexité : O(n) où n = longueur réelle de la chaîne
             * @warning constexpr en C++20+ uniquement si la boucle est constexpr-compatible
             */
            static NKENTSEU_CONSTEXPR SizeType CalculateLength(const CharT* str) NKENTSEU_NOEXCEPT
            {
                SizeType len = 0;
                while (str && str[len] != CharT(0))
                {
                    ++len;
                }
                return len;
            }
        };

        // =====================================================================
        // SECTION 7 : ALIASES DE TYPES PRÉ-DÉFINIS PAR ENCODAGE
        // =====================================================================

        // -----------------------------------------------------------------
        // Alias pour vue sur chaîne UTF-8 (char)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour BasicStringView<char> — encodage UTF-8 recommandé
         * @note Type par défaut pour la majorité des usages dans le projet
         */
        using StringView8 = BasicStringView<char>;

        // -----------------------------------------------------------------
        // Alias pour vue sur chaîne UTF-16 (char16_t)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour BasicStringView<char16_t> — encodage UTF-16
         * @note Utile pour l'interopérabilité avec les API Windows ou Java
         */
        using StringView16 = BasicStringView<char16_t>;

        // -----------------------------------------------------------------
        // Alias pour vue sur chaîne UTF-32 (char32_t)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour BasicStringView<char32_t> — encodage UTF-32
         * @note Chaque codepoint Unicode tient sur un seul CharT, mais mémoire intensive
         */
        using StringView32 = BasicStringView<char32_t>;

        // -----------------------------------------------------------------
        // Alias pour vue sur chaîne wide (wchar_t, plateforme-dépendant)
        // -----------------------------------------------------------------
        /**
         * @brief Alias pour BasicStringView<wchar_t> — encodage wide natif
         * @note wchar_t = 2 octets sur Windows (UTF-16), 4 octets sur Linux (UTF-32)
         * @warning À éviter pour du code portable, préférer StringView8/16/32 explicites
         */
        using WStringView = BasicStringView<wchar_t>;

        // -----------------------------------------------------------------
        // Alias par défaut du projet (décommenter pour changer le standard)
        // -----------------------------------------------------------------
        // using StringView = StringView8;  // UTF-8 par défaut

        // =====================================================================
        // SECTION 8 : OPÉRATEURS DE COMPARAISON LIBRES
        // =====================================================================

        // -----------------------------------------------------------------
        // Égalité : comparaison lexicographique
        // -----------------------------------------------------------------
        /**
         * @brief Opérateur d'égalité pour BasicStringView
         * @tparam CharT Type de caractère de la vue
         * @param lhs Vue de gauche pour la comparaison
         * @param rhs Vue de droite pour la comparaison
         * @return true si lhs.Compare(rhs) == 0, false sinon
         * @note Comparaison sensible à la casse et à l'encodage
         */
        template <typename CharT>
        bool operator==(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NKENTSEU_NOEXCEPT
        {
            return lhs.Compare(rhs) == 0;
        }

        // -----------------------------------------------------------------
        // Inégalité : négation de l'égalité
        // -----------------------------------------------------------------
        /**
         * @brief Opérateur d'inégalité pour BasicStringView
         * @tparam CharT Type de caractère de la vue
         * @param lhs Vue de gauche pour la comparaison
         * @param rhs Vue de droite pour la comparaison
         * @return true si lhs.Compare(rhs) != 0, false sinon
         */
        template <typename CharT>
        bool operator!=(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NKENTSEU_NOEXCEPT
        {
            return lhs.Compare(rhs) != 0;
        }

        // -----------------------------------------------------------------
        // Ordre strict : comparaison lexicographique inférieure
        // -----------------------------------------------------------------
        /**
         * @brief Opérateur de comparaison inférieure pour BasicStringView
         * @tparam CharT Type de caractère de la vue
         * @param lhs Vue de gauche pour la comparaison
         * @param rhs Vue de droite pour la comparaison
         * @return true si lhs.Compare(rhs) < 0, false sinon
         * @note Permet l'utilisation dans std::set, std::map, std::sort, etc.
         */
        template <typename CharT>
        bool operator<(BasicStringView<CharT> lhs, BasicStringView<CharT> rhs) NKENTSEU_NOEXCEPT
        {
            return lhs.Compare(rhs) < 0;
        }

    } // namespace nkentseu

#endif // NK_CONTAINERS_STRING_NKBASICSTRINGVIEW_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // -------------------------------------------------------------------------
    // Exemple 1 : Création et accès basique
    // -------------------------------------------------------------------------
    #include "NKContainers/String/NkBasicStringView.h"
    using namespace nkentseu;

    const char* source = "Hello, World!";
    BasicStringView<char> view(source);

    // Accès aux éléments
    char first = view.Front();        // 'H'
    char last = view.Back();          // '!'
    char third = view[2];             // 'l'
    char safe = view.At(2);           // 'l' (avec assertion debug)

    // Informations de capacité
    bool isEmpty = view.Empty();      // false
    usize len = view.Length();        // 13
    const char* raw = view.Data();    // pointeur vers "Hello, World!"

    // -------------------------------------------------------------------------
    // Exemple 2 : Manipulation de sous-vues (sans copie de données)
    // -------------------------------------------------------------------------
    BasicStringView<char> path("/usr/local/bin/app");

    // Extraire le nom du fichier
    auto filename = path.SubStr(path.RFind('/') + 1);  // "app"

    // Vérifier l'extension
    if (filename.EndsWith(".exe"))
    {
        // Traiter comme exécutable Windows
    }

    // Supprimer le préfixe de chemin
    path.RemovePrefix(5);  // path contient maintenant "local/bin/app"

    // -------------------------------------------------------------------------
    // Exemple 3 : Recherche et comparaison
    // -------------------------------------------------------------------------
    BasicStringView<char> text("The quick brown fox jumps over the lazy dog");

    // Recherche de mot
    usize pos = text.Find("fox");  // retourne 16

    // Vérification de présence
    if (text.Contains("quick") && text.Contains("lazy"))
    {
        // Le texte contient les deux mots-clés
    }

    // Comparaison lexicographique
    BasicStringView<char> a("apple"), b("banana");
    if (a < b)
    {
        // "apple" vient avant "banana" dans l'ordre alphabétique
    }

    // -------------------------------------------------------------------------
    // Exemple 4 : Itération et interopérabilité
    // -------------------------------------------------------------------------
    BasicStringView<char> csv("red,green,blue");

    // Boucle range-based (C++11)
    for (char c : csv)
    {
        ProcessChar(c);
    }

    // Interop avec API C : printf avec précision
    printf("CSV: %.*s\n", static_cast<int>(csv.Length()), csv.Data());

    // -------------------------------------------------------------------------
    // Exemple 5 : Support multi-encodage
    // -------------------------------------------------------------------------
    // UTF-16 pour interop Windows
    const char16_t* wsource = u"Bonjour le monde";
    BasicStringView<char16_t> wview(wsource);
    if (wview.StartsWith(u"Bon"))
    {
        // Préfixe détecté en UTF-16
    }

    // UTF-32 pour traitement de codepoints Unicode
    const char32_t* usource = U"🎉🎊🎈";
    BasicStringView<char32_t> uview(usource);
    usize emojiCount = uview.Length();  // 3 codepoints, pas 12 octets

    // -------------------------------------------------------------------------
    // Exemple 6 : Utilisation dans des conteneurs et algorithmes STL
    // -------------------------------------------------------------------------
    #include <set>
    #include <algorithm>

    // BasicStringView est comparable → compatible avec std::set
    std::set<BasicStringView<char>> keywords;
    keywords.insert("error");
    keywords.insert("warning");
    keywords.insert("info");

    if (keywords.count("warning") > 0)
    {
        // Mot-clé trouvé dans l'ensemble
    }

    // Tri de vues (sans copier les données sous-jacentes)
    std::vector<BasicStringView<char>> items = { "zebra", "apple", "mango" };
    std::sort(items.begin(), items.end());
    // items est maintenant : ["apple", "mango", "zebra"]

    // -------------------------------------------------------------------------
    // Exemple 7 : Bonnes pratiques et pièges à éviter
    // -------------------------------------------------------------------------

    // ✅ BON : La vue est utilisée tant que la source est valide
    {
        std::string temp = "temporary";
        BasicStringView<char> safeView(temp.c_str());
        UseView(safeView);  // OK : temp existe encore
    }

    // ❌ MAUVAIS : La vue survit à la destruction de sa source
    {
        BasicStringView<char> dangling;
        {
            std::string temp = "temporary";
            dangling = BasicStringView<char>(temp.c_str());
        }
        // dangling pointe maintenant vers une mémoire libérée → UB !
        UseView(dangling);  // ⚠️ Comportement indéfini
    }

    // ✅ BON : Utiliser Empty() avant Front()/Back()
    void SafeAccess(BasicStringView<char> view)
    {
        if (!view.Empty())
        {
            char first = view.Front();  // Sécurisé
            Process(first);
        }
    }

    // ✅ BON : Préférer At() en mode debug pour détecter les erreurs d'index
    #ifdef NK_DEBUG
        char c = view.At(index);  // Assertion si index hors bornes
    #else
        char c = view[index];     // Accès direct, plus rapide en release
    #endif
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Utilisation et modification autorisées
//
// Généré par Rihen le 2026-02-05 22:26:13
// Date de création : 2026-02-05 22:26:13
// Dernière modification : 2026-04-26
// ============================================================