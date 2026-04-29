// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringView.h
// DESCRIPTION: Vue de chaîne non-possessive en lecture seule (style std::string_view)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED

    // -------------------------------------------------------------------------
    // Inclusions des dépendances du projet
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKContainers/NkContainersApi.h"
    #include "NKPlatform/NkCompilerDetect.h"
    #include "NKPlatform/NkPlatformInline.h"
    #include "NKMemory/NkFunction.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKCore/NkTraits.h"

    // -------------------------------------------------------------------------
    // Inclusions standard
    // -------------------------------------------------------------------------
    #include <stddef.h>

    // -------------------------------------------------------------------------
    // Namespace principal du projet
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -------------------------------------------------------------------------
        // Déclaration anticipée de NkString (dépendance circulaire)
        // -------------------------------------------------------------------------
        class NkString;

        // =====================================================================
        // CLASSE : NkStringView
        // =====================================================================

        /**
         * @brief Vue de chaîne non-possessive en lecture seule
         * 
         * Cette classe fournit une référence légère et non-possessive vers 
         * une séquence de caractères, sans copie des données sous-jacentes.
         * 
         * Caractéristiques principales :
         * - Aucune allocation mémoire interne
         * - Copie et déplacement à coût constant O(1)
         * - Compatible avec les chaînes C et NkString
         * - Interface similaire à std::string_view (C++17)
         * 
         * @warning Sécurité : Cette classe ne possède PAS les données pointées.
         *          Il est de la responsabilité de l'appelant de garantir que
         *          la source originale reste valide pendant toute la durée
         *          d'utilisation de l'instance NkStringView.
         * 
         * @note Idéal pour : paramètres de fonctions, parsing temporaire,
         *       sous-chaînes sans allocation, interfaces API légères.
         */
        class NKENTSEU_CORE_API NkStringView {

            // =================================================================
            // TYPES PUBLICS ET ALIASES
            // =================================================================
            public:

                /// Type des éléments stockés (caractères)
                using ValueType = char;

                /// Type utilisé pour les tailles et indices
                using SizeType = usize;

                /// Itérateur en lecture seule sur les caractères
                using Iterator = const char*;

                /// Alias pour itérateur constant (cohérence API)
                using ConstIterator = const char*;

                /// Itérateur inverse en lecture seule
                using ReverseIterator = const char*;

                /// Alias pour itérateur inverse constant
                using ConstReverseIterator = const char*;

                /// Valeur spéciale représentant "non trouvé" ou "infini"
                static constexpr SizeType npos = static_cast<SizeType>(-1);

            // =================================================================
            // CONSTRUCTEURS ET DESTRUCTEUR
            // =================================================================
            public:

                /**
                 * @brief Constructeur par défaut : vue vide et nulle
                 * 
                 * Initialise une vue pointant vers nullptr avec longueur 0.
                 * Utile pour les variables membres initialisées plus tard.
                 */
                NKENTSEU_CONSTEXPR NkStringView() noexcept 
                    : mData(nullptr), 
                      mLength(0) {
                }

                /**
                 * @brief Constructeur depuis une chaîne C terminée par null
                 * 
                 * @param str Pointeur vers une chaîne C-style (const char*)
                 * 
                 * @note Calcule automatiquement la longueur via strlen-like.
                 *       Si str est nullptr, la vue reste vide (longueur 0).
                 */
                NKENTSEU_CONSTEXPR NkStringView(const char* str) noexcept 
                    : mData(str), 
                      mLength(str ? CalculateLength(str) : 0) {
                }

                /**
                 * @brief Constructeur depuis pointeur + longueur explicite
                 * 
                 * @param str Pointeur vers les données caractères
                 * @param length Nombre de caractères dans la vue
                 * 
                 * @note Permet de référencer des sous-parties de buffer,
                 *       des données non-terminées par null, ou binaires.
                 */
                NKENTSEU_CONSTEXPR NkStringView(const char* str, SizeType length) noexcept 
                    : mData(str), 
                      mLength(length) {
                }

                /**
                 * @brief Constructeur depuis une instance NkString
                 * 
                 * @param str Référence constante vers un NkString
                 * 
                 * @note Crée une vue sur les données internes de NkString.
                 *       La vue devient invalide si le NkString est modifié
                 *       ou détruit.
                 */
                NkStringView(const NkString& str) noexcept;

                /**
                 * @brief Constructeur depuis un tableau de caractères statique
                 * 
                 * @tparam N Taille compile-time du tableau source
                 * @param str Référence vers le tableau de caractères
                 * 
                 * @note Exclut automatiquement le caractère null terminal.
                 *       Déduit la longueur à la compilation pour efficacité.
                 */
                template<SizeType N>
                NKENTSEU_CONSTEXPR NkStringView(const char (&str)[N]) noexcept 
                    : mData(str), 
                      mLength(N - 1) {
                }

                /**
                 * @brief Constructeur de copie par défaut
                 * 
                 * @note Copie uniquement les pointeurs : opération O(1), très rapide.
                 */
                NKENTSEU_CONSTEXPR NkStringView(const NkStringView&) noexcept = default;

                /**
                 * @brief Constructeur de déplacement par défaut
                 * 
                 * @note Transfère la propriété logique des pointeurs : O(1).
                 */
                NKENTSEU_CONSTEXPR NkStringView(NkStringView&&) noexcept = default;

                /**
                 * @brief Destructeur par défaut
                 * 
                 * @note Aucune libération de mémoire : la vue ne possède pas les données.
                 */
                ~NkStringView() = default;

            // =================================================================
            // OPÉRATEURS D'AFFECTATION
            // =================================================================
            public:

                /**
                 * @brief Opérateur d'affectation par copie
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkStringView& operator=(const NkStringView&) noexcept = default;

                /**
                 * @brief Opérateur d'affectation par déplacement
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkStringView& operator=(NkStringView&&) noexcept = default;

                /**
                 * @brief Affectation depuis une chaîne C-style
                 * 
                 * @param str Pointeur vers chaîne C terminée par null
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Recalcule la longueur si str n'est pas nullptr.
                 */
                NkStringView& operator=(const char* str) noexcept {
                    mData = str;
                    mLength = str ? CalculateLength(str) : 0;
                    return *this;
                }

            // =================================================================
            // ACCÈS AUX ÉLÉMENTS
            // =================================================================
            public:

                /**
                 * @brief Accès indexé sans vérification de bornes (release)
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 * 
                 * @warning Comportement indéfini si index >= Length().
                 *          Utiliser At() pour une version avec assertion.
                 */
                char operator[](SizeType index) const noexcept {
                    NKENTSEU_ASSERT(index < mLength);
                    return mData[index];
                }

                /**
                 * @brief Accès indexé avec vérification de bornes
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 * 
                 * @note Lève une assertion en mode debug si index hors bornes.
                 *       Préférer cette méthode pour la sécurité en développement.
                 */
                char At(SizeType index) const {
                    NKENTSEU_ASSERT_MSG(index < mLength, "StringView index out of bounds");
                    return mData[index];
                }

                /**
                 * @brief Accès au premier caractère de la vue
                 * 
                 * @return Référence constante vers le premier caractère
                 * 
                 * @warning Assertion si la vue est vide. Vérifier Empty() avant.
                 */
                char Front() const noexcept {
                    NKENTSEU_ASSERT(mLength > 0);
                    return mData[0];
                }

                /**
                 * @brief Accès au dernier caractère de la vue
                 * 
                 * @return Référence constante vers le dernier caractère
                 * 
                 * @warning Assertion si la vue est vide. Vérifier Empty() avant.
                 */
                char Back() const noexcept {
                    NKENTSEU_ASSERT(mLength > 0);
                    return mData[mLength - 1];
                }

                /**
                 * @brief Accès direct au pointeur de données brutes
                 * 
                 * @return Pointeur constant vers le premier caractère
                 * 
                 * @note Peut être nullptr si la vue est vide ou nulle.
                 *       Aucun caractère null terminal garanti.
                 */
                const char* Data() const noexcept { 
                    return mData; 
                }

                /**
                 * @brief Accès compatible C-string (avec avertissement)
                 * 
                 * @return Pointeur constant vers les données
                 * 
                 * @warning NkStringView ne garantit PAS de null-terminator.
                 *          Utiliser uniquement avec des APIs tolérantes
                 *          ou après vérification manuelle de la terminaison.
                 */
                NKENTSEU_CONSTEXPR const char* CStr() const noexcept {
                    return mData;
                }

                /**
                 * @brief Accès mutable aux données (contourne la const)
                 * 
                 * @return Pointeur non-constant vers les données
                 * 
                 * @warning DANGEREUX : La vue est conçue comme read-only.
                 *          Modifier les données peut corrompre la source originale.
                 *          À utiliser avec extrême précaution uniquement si
                 *          vous contrôlez la durée de vie et l'unicité de la source.
                 */
                NKENTSEU_CONSTEXPR char* DataMutable() noexcept {
                    return const_cast<char*>(mData);
                }

            // =================================================================
            // CAPACITÉ ET ÉTAT
            // =================================================================
            public:

                /**
                 * @brief Retourne le nombre de caractères dans la vue
                 * 
                 * @return Longueur de la séquence en caractères
                 * 
                 * @note Complexité O(1), valeur pré-calculée au construction.
                 */
                NKENTSEU_CONSTEXPR SizeType Length() const noexcept { 
                    return mLength; 
                }

                /**
                 * @brief Alias pour Length() (convention STL)
                 * 
                 * @return Nombre d'éléments dans la vue
                 */
                NKENTSEU_CONSTEXPR SizeType Size() const noexcept { 
                    return mLength; 
                }

                /**
                 * @brief Alias pour Length() avec cast explicite usize
                 * 
                 * @return Longueur castée en type usize standard
                 */
                NKENTSEU_CONSTEXPR usize Count() const noexcept { 
                    return static_cast<usize>(mLength); 
                }

                /**
                 * @brief Teste si la vue ne contient aucun caractère
                 * 
                 * @return true si Length() == 0, false sinon
                 */
                NKENTSEU_CONSTEXPR bool Empty() const noexcept { 
                    return mLength == 0; 
                }

                /**
                 * @brief Teste si le pointeur interne est nullptr
                 * 
                 * @return true si mData == nullptr, false sinon
                 * 
                 * @note Une vue peut être non-null mais vide (pointeur valide, longueur 0).
                 */
                NKENTSEU_CONSTEXPR bool IsNull() const noexcept { 
                    return mData == nullptr; 
                }

                /**
                 * @brief Teste si la vue est nullptr OU vide
                 * 
                 * @return true si mData == nullptr OU mLength == 0
                 * 
                 * @note Méthode pratique pour les vérifications de validité rapides.
                 */
                NKENTSEU_CONSTEXPR bool IsNullOrEmpty() const noexcept { 
                    return mData == nullptr || mLength == 0; 
                }

                /**
                 * @brief Retourne la capacité théorique (identique à Length pour View)
                 * 
                 * @return Valeur égale à Length() : pas de buffer sur-alloué
                 * 
                 * @note Existant pour compatibilité API avec les conteneurs possédants.
                 */
                NKENTSEU_CONSTEXPR SizeType Capacity() const noexcept { 
                    return mLength; 
                }

                /**
                 * @brief Retourne la taille maximale théorique
                 * 
                 * @return npos - 1 : valeur limite avant overflow
                 */
                NKENTSEU_CONSTEXPR SizeType MaxSize() const noexcept { 
                    return npos - 1; 
                }

            // =================================================================
            // GESTION DE MÉMOIRE (NO-OP pour View)
            // =================================================================
            public:

                /**
                 * @brief Réinitialise la vue à l'état vide/null
                 * 
                 * @note Ne libère aucune mémoire : la vue ne possède pas les données.
                 *       Équivalent à assigner une vue par défaut.
                 */
                void Clear() noexcept {
                    mData = nullptr;
                    mLength = 0;
                }

                /**
                 * @brief Alias pour Clear() (sémantique de reset)
                 * 
                 * @note Identique fonctionnellement à Clear().
                 */
                void Reset() noexcept {
                    mData = nullptr;
                    mLength = 0;
                }

            // =================================================================
            // MODIFICATEURS DE VUE (ajustement des bornes)
            // =================================================================
            public:

                /**
                 * @brief Réduit la vue en supprimant N caractères au début
                 * 
                 * @param n Nombre de caractères à retirer du préfixe
                 * 
                 * @note Déplace le pointeur interne et ajuste la longueur.
                 *       Assertion si n > Length().
                 */
                void RemovePrefix(SizeType n) noexcept {
                    NKENTSEU_ASSERT(n <= mLength);
                    mData += n;
                    mLength -= n;
                }

                /**
                 * @brief Réduit la vue en supprimant N caractères à la fin
                 * 
                 * @param n Nombre de caractères à retirer du suffixe
                 * 
                 * @note Ajuste uniquement la longueur, le pointeur reste inchangé.
                 *       Assertion si n > Length().
                 */
                void RemoveSuffix(SizeType n) noexcept {
                    NKENTSEU_ASSERT(n <= mLength);
                    mLength -= n;
                }

                /**
                 * @brief No-op pour compatibilité API conteneurs
                 * 
                 * @note NkStringView n'alloue pas : pas de mémoire à "shrink".
                 *       Méthode présente uniquement pour uniformité d'interface.
                 */
                NKENTSEU_CONSTEXPR void ShrinkToFit() noexcept {
                    // Aucune action nécessaire pour une vue non-possessive
                }

            // =================================================================
            // OPÉRATIONS DE SOUS-CHAÎNES ET TRONCATURE
            // =================================================================
            public:

                /**
                 * @brief Extrait une sous-vue à partir d'une position
                 * 
                 * @param pos Position de départ dans la vue courante
                 * @param count Nombre maximal de caractères à inclure
                 * @return Nouvelle NkStringView sur la sous-partie spécifiée
                 * 
                 * @note Si count == npos ou dépasse la fin, retourne jusqu'à la fin.
                 *       Assertion si pos > Length().
                 */
                NkStringView SubStr(SizeType pos = 0, SizeType count = npos) const {
                    NKENTSEU_ASSERT(pos <= mLength);
                    SizeType rcount = (count == npos || pos + count > mLength) 
                        ? mLength - pos 
                        : count;
                    return NkStringView(mData + pos, rcount);
                }

                /**
                 * @brief Extrait une sous-vue par bornes [start, end)
                 * 
                 * @param start Index inclus du début de la sous-vue
                 * @param end Index exclu de la fin de la sous-vue
                 * @return Nouvelle NkStringView sur l'intervalle spécifié
                 * 
                 * @note Interface style "slice" Python : plus intuitive pour certains cas.
                 *       Assertion si start > end ou end > Length().
                 */
                NkStringView Slice(SizeType start, SizeType end) const {
                    NKENTSEU_ASSERT(start <= end && end <= mLength);
                    return NkStringView(mData + start, end - start);
                }

                /**
                 * @brief Retourne les N premiers caractères
                 * 
                 * @param count Nombre de caractères à extraire du début
                 * @return Vue sur le préfixe de longueur min(count, Length())
                 */
                NkStringView Left(SizeType count) const noexcept {
                    return SubStr(0, count);
                }

                /**
                 * @brief Retourne les N derniers caractères
                 * 
                 * @param count Nombre de caractères à extraire de la fin
                 * @return Vue sur le suffixe de longueur min(count, Length())
                 */
                NkStringView Right(SizeType count) const noexcept {
                    if (count >= mLength) {
                        return *this;
                    }
                    return SubStr(mLength - count, count);
                }

                /**
                 * @brief Alias pour SubStr() (convention Qt-style)
                 * 
                 * @param pos Position de départ
                 * @param count Longueur optionnelle (npos = jusqu'à la fin)
                 * @return Sous-vue spécifiée
                 */
                NkStringView Mid(SizeType pos, SizeType count = npos) const {
                    return SubStr(pos, count);
                }

                /**
                 * @brief Retourne une vue avec espaces blancs supprimés aux deux extrémités
                 * 
                 * @return Nouvelle vue tronquée des espaces en début et fin
                 * 
                 * @note Définition d'espace blanc : ' ', '\t', '\n', '\r', '\v', '\f'
                 *       Implémentation déléguée à TrimmedLeft/Right pour réutilisation.
                 */
                NkStringView Trimmed() const noexcept;

                /**
                 * @brief Retourne une vue avec espaces blancs supprimés à gauche uniquement
                 * 
                 * @return Nouvelle vue tronquée des espaces en début
                 */
                NkStringView TrimmedLeft() const noexcept;

                /**
                 * @brief Retourne une vue avec espaces blancs supprimés à droite uniquement
                 * 
                 * @return Nouvelle vue tronquée des espaces en fin
                 */
                NkStringView TrimmedRight() const noexcept;

                /**
                 * @brief Trime les caractères spécifiés aux deux extrémités
                 * 
                 * @param chars Chaîne C contenant les caractères à retirer
                 * @return Nouvelle vue sans les caractères de chars en bordure
                 * 
                 * @note Parcours linéaire : O(n + m) où n=length, m=chars.length
                 */
                NkStringView TrimmedChars(const char* chars) const noexcept {
                    SizeType start = 0;
                    SizeType end = mLength;
                    
                    // Troncature gauche : avancer tant que caractère dans chars
                    while (start < end) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[start] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            break;
                        }
                        ++start;
                    }
                    
                    // Troncature droite : reculer tant que caractère dans chars
                    while (end > start) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[end - 1] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            break;
                        }
                        --end;
                    }
                    
                    return SubStr(start, end - start);
                }

                /**
                 * @brief Trime les caractères spécifiés à gauche uniquement
                 * 
                 * @param chars Chaîne C contenant les caractères à retirer
                 * @return Nouvelle vue sans les caractères de chars en début
                 */
                NkStringView TrimmedLeftChars(const char* chars) const noexcept {
                    SizeType start = 0;
                    while (start < mLength) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[start] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            break;
                        }
                        ++start;
                    }
                    return SubStr(start);
                }

                /**
                 * @brief Trime les caractères spécifiés à droite uniquement
                 * 
                 * @param chars Chaîne C contenant les caractères à retirer
                 * @return Nouvelle vue sans les caractères de chars en fin
                 */
                NkStringView TrimmedRightChars(const char* chars) const noexcept {
                    SizeType end = mLength;
                    while (end > 0) {
                        bool found = false;
                        for (SizeType i = 0; chars[i] != '\0'; ++i) {
                            if (mData[end - 1] == chars[i]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            break;
                        }
                        --end;
                    }
                    return SubStr(0, end);
                }

                /**
                 * @brief Échange le contenu de deux vues en O(1)
                 * 
                 * @param other Référence vers l'autre vue à échanger
                 * 
                 * @note Échange uniquement les pointeurs et longueurs : très efficace.
                 *       Ne modifie pas les données sous-jacentes.
                 */
                void Swap(NkStringView& other) noexcept {
                    const char* tmpData = mData;
                    mData = other.mData;
                    other.mData = tmpData;
                    
                    SizeType tmpLen = mLength;
                    mLength = other.mLength;
                    other.mLength = tmpLen;
                }

            // =================================================================
            // OPÉRATIONS DE COMPARAISON ET RECHERCHE
            // =================================================================
            public:

                /**
                 * @brief Compare lexicographiquement avec une autre vue
                 * 
                 * @param other Vue à comparer avec this
                 * @return <0 si this < other, 0 si égal, >0 si this > other
                 * 
                 * @note Comparaison binaire octet par octet (case-sensitive).
                 *       Utilise NkMemCompare pour efficacité sur grands buffers.
                 */
                int Compare(NkStringView other) const noexcept {
                    SizeType minLen = mLength < other.mLength ? mLength : other.mLength;
                    
                    if (minLen > 0) {
                        int result = memory::NkMemCompare(mData, other.mData, minLen);
                        if (result != 0) {
                            return result;
                        }
                    }
                    
                    if (mLength < other.mLength) {
                        return -1;
                    }
                    if (mLength > other.mLength) {
                        return 1;
                    }
                    return 0;
                }

                /**
                 * @brief Compare en ignorant la casse ASCII
                 * 
                 * @param other Vue à comparer
                 * @return Résultat de comparaison case-insensitive
                 * 
                 * @note Délègue à encoding::ascii::NkCompareIgnoreCase.
                 *       Ne gère que l'ASCII : pour Unicode complet, utiliser ICU.
                 */
                int CompareIgnoreCase(NkStringView other) const noexcept;

                /**
                 * @brief Compare avec tri "naturel" (nombres comparés numériquement)
                 * 
                 * @param other Vue à comparer
                 * @return Résultat de comparaison naturelle
                 * 
                 * @note Ex: "file2.txt" < "file10.txt" (contre "file10" < "file2" en binaire)
                 *       Utile pour le tri de noms de fichiers, versions, etc.
                 */
                int CompareNatural(NkStringView other) const noexcept;

                /**
                 * @brief Teste l'égalité binaire avec une autre vue
                 * 
                 * @param other Vue à comparer
                 * @return true si mêmes données et même longueur
                 */
                bool Equals(NkStringView other) const noexcept {
                    return Compare(other) == 0;
                }

                /**
                 * @brief Teste l'égalité en ignorant la casse ASCII
                 * 
                 * @param other Vue à comparer
                 * @return true si égalité case-insensitive
                 */
                bool EqualsIgnoreCase(NkStringView other) const noexcept {
                    return CompareIgnoreCase(other) == 0;
                }

                /**
                 * @brief Teste l'égalité avec comparaison naturelle
                 * 
                 * @param other Vue à comparer
                 * @return true si égalité naturelle
                 */
                bool EqualsNatural(NkStringView other) const noexcept {
                    return CompareNatural(other) == 0;
                }

                /**
                 * @brief Vérifie si la vue commence par un préfixe donné
                 * 
                 * @param prefix Vue représentant le préfixe attendu
                 * @return true si les premiers caractères correspondent exactement
                 * 
                 * @note Comparaison binaire rapide via NkMemCompare.
                 *       Retourne false si prefix est plus long que this.
                 */
                bool StartsWith(NkStringView prefix) const noexcept {
                    if (prefix.mLength > mLength) {
                        return false;
                    }
                    return memory::NkMemCompare(mData, prefix.mData, prefix.mLength) == 0;
                }

                /**
                 * @brief Vérifie si la vue commence par un caractère donné
                 * 
                 * @param c Caractère à tester en première position
                 * @return true si Length() > 0 && mData[0] == c
                 */
                bool StartsWith(char c) const noexcept {
                    return mLength > 0 && mData[0] == c;
                }

                /**
                 * @brief Vérifie le préfixe en ignorant la casse
                 * 
                 * @param prefix Préfixe attendu
                 * @return true si préfixe match case-insensitive
                 */
                bool StartsWithIgnoreCase(NkStringView prefix) const noexcept;

                /**
                 * @brief Vérifie si la vue se termine par un suffixe donné
                 * 
                 * @param suffix Vue représentant le suffixe attendu
                 * @return true si les derniers caractères correspondent exactement
                 * 
                 * @note Compare à partir de (mLength - suffix.mLength).
                 *       Retourne false si suffix est plus long que this.
                 */
                bool EndsWith(NkStringView suffix) const noexcept {
                    if (suffix.mLength > mLength) {
                        return false;
                    }
                    return memory::NkMemCompare(
                        mData + mLength - suffix.mLength, 
                        suffix.mData, 
                        suffix.mLength) == 0;
                }

                /**
                 * @brief Vérifie si la vue se termine par un caractère donné
                 * 
                 * @param c Caractère à tester en dernière position
                 * @return true si Length() > 0 && mData[Length()-1] == c
                 */
                bool EndsWith(char c) const noexcept {
                    return mLength > 0 && mData[mLength - 1] == c;
                }

                /**
                 * @brief Vérifie le suffixe en ignorant la casse
                 * 
                 * @param suffix Suffixe attendu
                 * @return true si suffixe match case-insensitive
                 */
                bool EndsWithIgnoreCase(NkStringView suffix) const noexcept;

                /**
                 * @brief Vérifie si la vue contient une sous-chaîne donnée
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @return true si Find(str) != npos
                 */
                bool Contains(NkStringView str) const noexcept {
                    return Find(str) != npos;
                }

                /**
                 * @brief Vérifie si la vue contient un caractère donné
                 * 
                 * @param c Caractère à rechercher
                 * @return true si le caractère est présent au moins une fois
                 */
                bool Contains(char c) const noexcept {
                    return Find(c) != npos;
                }

                /**
                 * @brief Vérifie la présence d'une sous-chaîne (case-insensitive)
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @return true si trouvée en ignorant la casse ASCII
                 */
                bool ContainsIgnoreCase(NkStringView str) const noexcept;

                /**
                 * @brief Vérifie si la vue contient au moins un des caractères donnés
                 * 
                 * @param chars Chaîne C des caractères à tester
                 * @return true si au moins un caractère de chars est présent
                 */
                bool ContainsAny(const char* chars) const noexcept;

                /**
                 * @brief Vérifie si la vue contient TOUS les caractères donnés
                 * 
                 * @param chars Chaîne C des caractères requis
                 * @return true si chaque caractère de chars apparaît au moins une fois
                 */
                bool ContainsAll(const char* chars) const noexcept;

                /**
                 * @brief Vérifie si la vue ne contient AUCUN des caractères donnés
                 * 
                 * @param chars Chaîne C des caractères interdits
                 * @return true si aucun caractère de chars n'est présent
                 */
                bool ContainsNone(const char* chars) const noexcept;

            // =================================================================
            // RECHERCHE DE POSITIONS (Find / RFind)
            // =================================================================
            public:

                /**
                 * @brief Recherche la première occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position de départ pour la recherche (défaut: 0)
                 * @return Index de la première occurrence, ou npos si non trouvé
                 * 
                 * @note Implémentation naïve O(n*m) : optimisable avec algorithmes
                 *       avancés (Boyer-Moore, etc.) si nécessaire pour performances.
                 */
                SizeType Find(NkStringView str, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche la première occurrence d'un caractère
                 * 
                 * @param c Caractère à rechercher
                 * @param pos Position de départ (défaut: 0)
                 * @return Index de la première occurrence, ou npos si absent
                 * 
                 * @note Parcours linéaire optimisé : O(n) worst-case, souvent plus rapide.
                 */
                SizeType Find(char c, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche case-insensitive d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position de départ
                 * @return Index de la première occurrence (ASCII case-insensitive)
                 */
                SizeType FindIgnoreCase(NkStringView str, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche la dernière occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position de départ pour recherche arrière (défaut: npos = fin)
                 * @return Index de la dernière occurrence, ou npos si non trouvé
                 * 
                 * @note Recherche depuis la fin vers le début jusqu'à pos.
                 */
                SizeType RFind(NkStringView str, SizeType pos = npos) const noexcept;

                /**
                 * @brief Recherche la dernière occurrence d'un caractère
                 * 
                 * @param c Caractère à rechercher
                 * @param pos Position limite haute pour la recherche (défaut: npos)
                 * @return Index de la dernière occurrence, ou npos si absent
                 */
                SizeType RFind(char c, SizeType pos = npos) const noexcept;

                /**
                 * @brief Recherche arrière case-insensitive d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence (ASCII case-insensitive)
                 */
                SizeType RFindIgnoreCase(NkStringView str, SizeType pos = npos) const noexcept;

            // =================================================================
            // RECHERCHE PAR ENSEMBLE DE CARACTÈRES (FindFirstOf / FindLastOf)
            // =================================================================
            public:

                /**
                 * @brief Trouve le premier caractère appartenant à un ensemble
                 * 
                 * @param chars Vue des caractères à rechercher
                 * @param pos Position de départ
                 * @return Index du premier match, ou npos si aucun
                 * 
                 * @note Utilisé pour : tokenization, parsing de délimiteurs multiples.
                 */
                SizeType FindFirstOf(NkStringView chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le premier caractère appartenant à un ensemble (C-string)
                 * 
                 * @param chars Chaîne C des caractères à rechercher
                 * @param pos Position de départ
                 * @return Index du premier match, ou npos si aucun
                 */
                SizeType FindFirstOf(const char* chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Alias pour Find(char, pos) via interface FindFirstOf
                 * 
                 * @param c Caractère unique à rechercher
                 * @param pos Position de départ
                 * @return Index de la première occurrence
                 */
                SizeType FindFirstOf(char c, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le dernier caractère appartenant à un ensemble
                 * 
                 * @param chars Vue des caractères à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier match, ou npos si aucun
                 */
                SizeType FindLastOf(NkStringView chars, SizeType pos = npos) const noexcept;

                /**
                 * @brief Trouve le dernier caractère appartenant à un ensemble (C-string)
                 * 
                 * @param chars Chaîne C des caractères à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier match, ou npos si aucun
                 */
                SizeType FindLastOf(const char* chars, SizeType pos = npos) const noexcept;

                /**
                 * @brief Alias pour RFind(char, pos) via interface FindLastOf
                 * 
                 * @param c Caractère unique à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence
                 */
                SizeType FindLastOf(char c, SizeType pos = npos) const noexcept;

            // =================================================================
            // RECHERCHE PAR EXCLUSION (FindFirstNotOf / FindLastNotOf)
            // =================================================================
            public:

                /**
                 * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble
                 * 
                 * @param chars Vue des caractères à exclure
                 * @param pos Position de départ
                 * @return Index du premier caractère "extérieur", ou npos
                 * 
                 * @note Utile pour : sauter des espaces, ignorer des préfixes, etc.
                 */
                SizeType FindFirstNotOf(NkStringView chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble (C-string)
                 * 
                 * @param chars Chaîne C des caractères à exclure
                 * @param pos Position de départ
                 * @return Index du premier caractère "extérieur", ou npos
                 */
                SizeType FindFirstNotOf(const char* chars, SizeType pos = 0) const noexcept;

                /**
                 * @brief Alias pour recherche d'un caractère spécifique via exclusion
                 * 
                 * @param c Caractère à exclure (recherche tout sauf c)
                 * @param pos Position de départ
                 * @return Index du premier caractère différent de c
                 */
                SizeType FindFirstNotOf(char c, SizeType pos = 0) const noexcept;

                /**
                 * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble
                 * 
                 * @param chars Vue des caractères à exclure
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier caractère "extérieur", ou npos
                 */
                SizeType FindLastNotOf(NkStringView chars, SizeType pos = npos) const noexcept;

                /**
                 * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble (C-string)
                 * 
                 * @param chars Chaîne C des caractères à exclure
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier caractère "extérieur", ou npos
                 */
                SizeType FindLastNotOf(const char* chars, SizeType pos = npos) const noexcept;

                /**
                 * @brief Alias pour recherche arrière d'un caractère spécifique via exclusion
                 * 
                 * @param c Caractère à exclure
                 * @param pos Position limite pour recherche arrière
                 * @return Index du dernier caractère différent de c
                 */
                SizeType FindLastNotOf(char c, SizeType pos = npos) const noexcept;

            // =================================================================
            // COMPTAGE D'OCCURRENCES
            // =================================================================
            public:

                /**
                 * @brief Compte le nombre d'occurrences d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à compter
                 * @return Nombre de fois où str apparaît (non-overlapping par défaut)
                 * 
                 * @note Parcours séquentiel : complexité O(n*m) dans le pire cas.
                 */
                SizeType Count(NkStringView str) const noexcept;

                /**
                 * @brief Compte le nombre d'occurrences d'un caractère
                 * 
                 * @param c Caractère à compter
                 * @return Fréquence du caractère dans la vue
                 * 
                 * @note Parcours linéaire optimisé : O(n), très efficace.
                 */
                SizeType Count(char c) const noexcept;

                /**
                 * @brief Compte le total d'occurrences de caractères d'un ensemble
                 * 
                 * @param chars Chaîne C des caractères à comptabiliser
                 * @return Somme des fréquences de chaque caractère de chars
                 */
                SizeType CountAny(const char* chars) const noexcept;

            // =================================================================
            // CONVERSIONS NUMÉRIQUES ET TYPES PRIMITIFS
            // =================================================================
            public:

                /**
                 * @brief Tente de parser la vue comme entier signé
                 * 
                 * @param out Référence pour stocker le résultat
                 * @return true si parsing réussi, false si format invalide
                 * 
                 * @note Accepte : espaces initiaux, signe +/-, chiffres décimaux.
                 *       Rejette tout caractère non-numérique après le nombre.
                 */
                bool ToInt(int& out) const noexcept;

                /**
                 * @brief Tente de parser comme int32 explicite
                 * 
                 * @param out Référence pour le résultat int32
                 * @return true si succès, false sinon
                 */
                bool ToInt32(int32& out) const noexcept;

                /**
                 * @brief Tente de parser comme int64 pour grandes valeurs
                 * 
                 * @param out Référence pour le résultat int64
                 * @return true si succès, false sinon
                 */
                bool ToInt64(int64& out) const noexcept;

                /**
                 * @brief Tente de parser comme entier non-signé 32 bits
                 * 
                 * @param out Référence pour le résultat uint32
                 * @return true si succès, false si négatif ou format invalide
                 */
                bool ToUInt(uint32& out) const noexcept;

                /**
                 * @brief Tente de parser comme entier non-signé 64 bits
                 * 
                 * @param out Référence pour le résultat uint64
                 * @return true si succès, false sinon
                 */
                bool ToUInt64(uint64& out) const noexcept;

                /**
                 * @brief Tente de parser comme nombre flottant simple précision
                 * 
                 * @param out Référence pour le résultat float32
                 * @return true si succès, false si format invalide
                 * 
                 * @note Accepte notation décimale et scientifique (1.23e-4)
                 */
                bool ToFloat(float32& out) const noexcept;

                /**
                 * @brief Tente de parser comme nombre flottant double précision
                 * 
                 * @param out Référence pour le résultat float64
                 * @return true si succès, false sinon
                 */
                bool ToDouble(float64& out) const noexcept;

                /**
                 * @brief Tente de parser comme valeur booléenne
                 * 
                 * @param out Référence pour le résultat bool
                 * @return true si parsing réussi
                 * 
                 * @note Accepte : "true"/"false", "1"/"0", "yes"/"no" (case-insensitive)
                 */
                bool ToBool(bool& out) const noexcept;

            // =================================================================
            // CONVERSIONS AVEC VALEURS PAR DÉFAUT (Fallback)
            // =================================================================
            public:

                /**
                 * @brief Parse en int ou retourne la valeur par défaut
                 * 
                 * @param defaultValue Valeur à retourner en cas d'échec
                 * @return Résultat du parsing ou defaultValue
                 * 
                 * @note Évite la gestion explicite de bool de retour.
                 *       Utile pour les configurations, paramètres optionnels.
                 */
                int ToIntOrDefault(int defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en int32 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                int32 ToInt32OrDefault(int32 defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en int64 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                int64 ToInt64OrDefault(int64 defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en uint32 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                uint32 ToUIntOrDefault(uint32 defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en uint64 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                uint64 ToUInt64OrDefault(uint64 defaultValue = 0) const noexcept;

                /**
                 * @brief Parse en float32 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                float32 ToFloatOrDefault(float32 defaultValue = 0.0f) const noexcept;

                /**
                 * @brief Parse en float64 avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                float64 ToDoubleOrDefault(float64 defaultValue = 0.0) const noexcept;

                /**
                 * @brief Parse en bool avec fallback
                 * 
                 * @param defaultValue Valeur par défaut en cas d'échec
                 * @return Résultat ou defaultValue
                 */
                bool ToBoolOrDefault(bool defaultValue = false) const noexcept;

            // =================================================================
            // CONVERSIONS VERS TYPES POSSESSIFS (NkString)
            // =================================================================
            public:

                /**
                 * @brief Crée une copie possédante de la vue
                 * 
                 * @return Nouvelle instance NkString avec copie des données
                 * 
                 * @note Allocation mémoire nécessaire : utiliser avec précaution
                 *       dans les boucles ou code performance-critical.
                 */
                NkString ToString() const noexcept;

                /**
                 * @brief Retourne une version en minuscules (ASCII)
                 * 
                 * @return Nouveau NkString avec caractères convertis
                 * 
                 * @note Ne modifie pas la vue source : retourne une copie transformée.
                 *       Conversion ASCII uniquement : pour Unicode, utiliser ICU.
                 */
                NkString ToLower() const noexcept;

                /**
                 * @brief Retourne une version en majuscules (ASCII)
                 * 
                 * @return Nouveau NkString avec caractères convertis
                 */
                NkString ToUpper() const noexcept;

                /**
                 * @brief Retourne une version avec première lettre en majuscule
                 * 
                 * @return Nouveau NkString capitalisé
                 * 
                 * @note Ex: "hello world" -> "Hello world"
                 */
                NkString ToCapitalized() const noexcept;

                /**
                 * @brief Retourne une version en Title Case (majuscule après espace)
                 * 
                 * @return Nouveau NkString en title case
                 * 
                 * @note Ex: "hello world" -> "Hello World"
                 */
                NkString ToTitleCase() const noexcept;

            // =================================================================
            // PRÉDICATS DE CONTENU (Tests sémantiques)
            // =================================================================
            public:

                /**
                 * @brief Vérifie si la vue ne contient que des espaces blancs
                 * 
                 * @return true si tous les caractères sont whitespace
                 */
                bool IsWhitespace() const noexcept;

                /**
                 * @brief Vérifie si la vue ne contient que des chiffres décimaux
                 * 
                 * @return true si tous les caractères sont '0'-'9'
                 */
                bool IsDigits() const noexcept;

                /**
                 * @brief Vérifie si la vue ne contient que des lettres ASCII
                 * 
                 * @return true si tous les caractères sont A-Z ou a-z
                 */
                bool IsAlpha() const noexcept;

                /**
                 * @brief Vérifie si la vue est alphanumérique ASCII
                 * 
                 * @return true si tous les caractères sont lettres ou chiffres
                 */
                bool IsAlphaNumeric() const noexcept;

                /**
                 * @brief Vérifie si la vue contient uniquement des chiffres hexadécimaux
                 * 
                 * @return true si tous les caractères sont 0-9, A-F ou a-f
                 */
                bool IsHexDigits() const noexcept;

                /**
                 * @brief Vérifie si tous les caractères alphabétiques sont en minuscules
                 * 
                 * @return true si aucune majuscule présente (ignore non-lettres)
                 */
                bool IsLower() const noexcept;

                /**
                 * @brief Vérifie si tous les caractères alphabétiques sont en majuscules
                 * 
                 * @return true si aucune minuscule présente (ignore non-lettres)
                 */
                bool IsUpper() const noexcept;

                /**
                 * @brief Vérifie si tous les caractères sont imprimables ASCII
                 * 
                 * @return true si tous dans la plage 32-126
                 */
                bool IsPrintable() const noexcept;

                /**
                 * @brief Vérifie si la vue représente un nombre valide (entier ou flottant)
                 * 
                 * @return true si format numérique reconnu
                 * 
                 * @note Accepte : signe, décimales, exposant scientifique
                 */
                bool IsNumeric() const noexcept;

                /**
                 * @brief Vérifie si la vue représente un entier valide
                 * 
                 * @return true si format entier signé/non-signé reconnu
                 */
                bool IsInteger() const noexcept;

                /**
                 * @brief Vérifie si la vue représente un flottant valide
                 * 
                 * @return true si format décimal ou scientifique reconnu
                 */
                bool IsFloat() const noexcept;

                /**
                 * @brief Vérifie si la vue représente une valeur booléenne texte
                 * 
                 * @return true si "true"/"false"/"1"/"0"/"yes"/"no" (case-insensitive)
                 */
                bool IsBoolean() const noexcept;

                /**
                 * @brief Vérifie si la vue est un palindrome (lecture identique dans les deux sens)
                 * 
                 * @return true si symétrie caractères par caractères
                 * 
                 * @note Comparaison binaire : case-sensitive, accents significatifs
                 */
                bool IsPalindrome() const noexcept;

            // =================================================================
            // ITÉRATEURS (Compatibilité STL)
            // =================================================================
            public:

                /**
                 * @brief Itérateur vers le premier élément
                 * 
                 * @return Pointeur constant vers mData
                 */
                ConstIterator begin() const noexcept { 
                    return mData; 
                }

                /**
                 * @brief Itérateur vers la position après le dernier élément
                 * 
                 * @return Pointeur constant vers mData + mLength
                 */
                ConstIterator end() const noexcept { 
                    return mData + mLength; 
                }

                /**
                 * @brief Alias const pour begin() (cohérence API STL)
                 * 
                 * @return Pointeur constant vers mData
                 */
                ConstIterator cbegin() const noexcept { 
                    return mData; 
                }

                /**
                 * @brief Alias const pour end() (cohérence API STL)
                 * 
                 * @return Pointeur constant vers mData + mLength
                 */
                ConstIterator cend() const noexcept { 
                    return mData + mLength; 
                }

                /**
                 * @brief Itérateur inverse vers le dernier élément
                 * 
                 * @return Pointeur constant vers mData + mLength - 1
                 * 
                 * @warning Comportement indéfini si Empty() : vérifier avant usage
                 */
                ReverseIterator rbegin() const noexcept { 
                    return mData + mLength - 1; 
                }

                /**
                 * @brief Itérateur inverse vers avant le premier élément
                 * 
                 * @return Pointeur constant vers mData - 1
                 * 
                 * @note Position sentinelle : ne pas déréférencer
                 */
                ReverseIterator rend() const noexcept { 
                    return mData - 1; 
                }

                /**
                 * @brief Alias const pour rbegin()
                 * 
                 * @return Pointeur constant vers mData + mLength - 1
                 */
                ConstReverseIterator crbegin() const noexcept { 
                    return mData + mLength - 1; 
                }

                /**
                 * @brief Alias const pour rend()
                 * 
                 * @return Pointeur constant vers mData - 1
                 */
                ConstReverseIterator crend() const noexcept { 
                    return mData - 1; 
                }

            // =================================================================
            // UTILITAIRES AVANCÉS (Hash, Similarité, Pattern)
            // =================================================================
            public:

                /**
                 * @brief Calcule un hash rapide de la vue
                 * 
                 * @return Valeur de hash SizeType
                 * 
                 * @note Algorithme interne optimisé pour performance.
                 *       Non-cryptographique : pour tables de hachage uniquement.
                 */
                SizeType Hash() const noexcept;

                /**
                 * @brief Calcule un hash case-insensitive
                 * 
                 * @return Valeur de hash SizeType (ASCII lowercase avant hash)
                 */
                SizeType HashIgnoreCase() const noexcept;

                /**
                 * @brief Calcule un hash via l'algorithme FNV-1a
                 * 
                 * @return Valeur de hash SizeType selon spécification FNV-1a
                 * 
                 * @note Bon compromis distribution/performance pour hash tables.
                 */
                SizeType HashFNV1a() const noexcept;

                /**
                 * @brief Calcule un hash FNV-1a case-insensitive
                 * 
                 * @return Valeur de hash SizeType (ASCII lowercase avant FNV-1a)
                 */
                SizeType HashFNV1aIgnoreCase() const noexcept;

                /**
                 * @brief Calcule la distance de Levenshtein (nombre d'éditions)
                 * 
                 * @param other Vue à comparer
                 * @return Nombre minimal d'insertions/suppressions/substitutions
                 * 
                 * @note Complexité O(n*m) : utiliser avec précaution sur longues chaînes.
                 *       Utile pour : fuzzy matching, correction orthographique.
                 */
                SizeType LevenshteinDistance(NkStringView other) const noexcept;

                /**
                 * @brief Calcule un score de similarité normalisé [0.0 - 1.0]
                 * 
                 * @param other Vue à comparer
                 * @return 1.0 si identique, 0.0 si totalement différent
                 * 
                 * @note Basé sur la distance de Levenshtein normalisée par la longueur max.
                 */
                float64 Similarity(NkStringView other) const noexcept;

                /**
                 * @brief Retourne une vue sans les N premiers caractères
                 * 
                 * @param count Nombre de caractères à retirer du début
                 * @return Vue sur le suffixe restant
                 * 
                 * @note Équivalent à Right(Length() - count)
                 */
                NkStringView WithoutPrefix(SizeType count) const noexcept {
                    return Right(mLength - count);
                }

                /**
                 * @brief Retourne une vue sans les N derniers caractères
                 * 
                 * @param count Nombre de caractères à retirer de la fin
                 * @return Vue sur le préfixe restant
                 * 
                 * @note Équivalent à Left(Length() - count)
                 */
                NkStringView WithoutSuffix(SizeType count) const noexcept {
                    return Left(mLength - count);
                }

                /**
                 * @brief Alias pour Trimmed() (sémantique de suppression de padding)
                 * 
                 * @return Vue tronquée des espaces en bordure
                 */
                NkStringView WithoutPadding() const noexcept {
                    return Trimmed();
                }

                /**
                 * @brief Vérifie si la vue correspond à un pattern avec wildcards
                 * 
                 * @param pattern Pattern avec '*' (multi) et '?' (single)
                 * @return true si la vue match le pattern
                 * 
                 * @note Ex: "file*.txt" match "file123.txt" mais pas "file.doc"
                 *       Implémentation backtracking : éviter patterns trop complexes.
                 */
                bool MatchesPattern(NkStringView pattern) const noexcept;

                /**
                 * @brief Vérifie si la vue correspond à une expression régulière
                 * 
                 * @param regex Expression régulière au format supporté
                 * @return true si match réussi
                 * 
                 * @warning Implémentation dépendante du moteur regex du projet.
                 *          Vérifier la documentation pour la syntaxe supportée.
                 */
                bool MatchesRegex(NkStringView regex) const noexcept;

                /**
                 * @brief Alias pour RFind(str, pos) (nom alternatif)
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence
                 */
                SizeType FindLast(NkStringView str, SizeType pos = npos) const noexcept {
                    return RFind(str, pos);
                }

                /**
                 * @brief Alias pour RFind(c, pos) (nom alternatif)
                 * 
                 * @param c Caractère à rechercher
                 * @param pos Position limite pour recherche arrière
                 * @return Index de la dernière occurrence
                 */
                SizeType FindLast(char c, SizeType pos = npos) const noexcept {
                    return RFind(c, pos);
                }

            // =================================================================
            // OPÉRATEURS ET SURCHARGES
            // =================================================================
            public:

                /**
                 * @brief Accès au premier caractère via déréférencement d'itérateur
                 * 
                 * @return Caractère en position 0
                 * 
                 * @warning Assertion si Empty() : équivalent à Front()
                 */
                char operator*() const noexcept {
                    return Front();
                }

                /**
                 * @brief Opérateur d'ajout : no-op pour StringView
                 * 
                 * @param other Vue à "ajouter" (ignorée)
                 * @return Référence vers this inchangé
                 * 
                 * @note StringView ne peut pas modifier sa taille.
                 *       Cet opérateur existe uniquement pour compatibilité syntaxique
                 *       dans les templates génériques. Ne modifie rien.
                 */
                NkStringView& operator+=(NkStringView other) noexcept {
                    // Aucune modification possible : vue non-possessive
                    return *this;
                }

                /**
                 * @brief Opérateur d'égalité générique via conversion implicite
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si égalité binaire après conversion
                 */
                template<typename T>
                bool operator==(const T& other) const noexcept {
                    return Compare(NkStringView(other)) == 0;
                }

                /**
                 * @brief Opérateur de différence générique
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si inégalité
                 */
                template<typename T>
                bool operator!=(const T& other) const noexcept {
                    return Compare(NkStringView(other)) != 0;
                }

                /**
                 * @brief Opérateur de comparaison inférieure générique
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si this < other lexicographiquement
                 */
                template<typename T>
                bool operator<(const T& other) const noexcept {
                    return Compare(NkStringView(other)) < 0;
                }

                /**
                 * @brief Opérateur de comparaison inférieure ou égale générique
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si this <= other
                 */
                template<typename T>
                bool operator<=(const T& other) const noexcept {
                    return Compare(NkStringView(other)) <= 0;
                }

                /**
                 * @brief Opérateur de comparaison supérieure générique
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si this > other
                 */
                template<typename T>
                bool operator>(const T& other) const noexcept {
                    return Compare(NkStringView(other)) > 0;
                }

                /**
                 * @brief Opérateur de comparaison supérieure ou égale générique
                 * 
                 * @tparam T Type convertible en NkStringView
                 * @param other Objet à comparer
                 * @return true si this >= other
                 */
                template<typename T>
                bool operator>=(const T& other) const noexcept {
                    return Compare(NkStringView(other)) >= 0;
                }

            // =================================================================
            // MEMBRES PRIVÉS ET UTILITAIRES INTERNES
            // =================================================================
            private:

                /// Pointeur vers les données caractères (non-possessif)
                const char* mData;

                /// Nombre de caractères dans la vue
                SizeType mLength;

                /**
                 * @brief Calcule la longueur d'une chaîne C terminée par null
                 * 
                 * @param str Pointeur vers la chaîne à mesurer
                 * @return Nombre de caractères avant le null terminator
                 * 
                 * @note Implémentation constexpr pour évaluation compile-time
                 *       quand possible. Boucle simple type strlen.
                 */
                static NKENTSEU_CONSTEXPR SizeType CalculateLength(const char* str) noexcept {
                    SizeType len = 0;
                    if (str) {
                        while (str[len] != '\0') {
                            ++len;
                        }
                    }
                    return len;
                }
        };

        // =====================================================================
        // OPÉRATEURS DE COMPARAISON LIBRES (NkStringView vs NkStringView / char*)
        // =====================================================================

        /**
         * @brief Comparaison d'égalité entre deux NkStringView
         * 
         * @param lhs Vue de gauche
         * @param rhs Vue de droite
         * @return true si contenus identiques (même longueur, mêmes bytes)
         */
        NK_INLINE bool operator==(const NkStringView& lhs, const NkStringView& rhs) noexcept {
            return lhs.Compare(rhs) == 0;
        }

        /**
         * @brief Comparaison d'égalité NkStringView vs C-string
         * 
         * @param lhs Vue à comparer
         * @param rhs Chaîne C terminée par null
         * @return true si égalité binaire
         * 
         * @note Crée temporairement une NkStringView depuis rhs pour la comparaison.
         */
        NK_INLINE bool operator==(const NkStringView& lhs, const char* rhs) noexcept {
            return lhs.Compare(NkStringView(rhs)) == 0;
        }

        /**
         * @brief Comparaison d'égalité C-string vs NkStringView (symétrique)
         * 
         * @param lhs Chaîne C terminée par null
         * @param rhs Vue à comparer
         * @return true si égalité binaire
         */
        NK_INLINE bool operator==(const char* lhs, const NkStringView& rhs) noexcept {
            return NkStringView(lhs).Compare(rhs) == 0;
        }

        /**
         * @brief Comparaison de différence entre deux NkStringView
         * 
         * @param lhs Vue de gauche
         * @param rhs Vue de droite
         * @return true si contenus différents
         */
        NK_INLINE bool operator!=(const NkStringView& lhs, const NkStringView& rhs) noexcept {
            return lhs.Compare(rhs) != 0;
        }

        /**
         * @brief Comparaison de différence NkStringView vs C-string
         * 
         * @param lhs Vue à comparer
         * @param rhs Chaîne C
         * @return true si inégalité
         */
        NK_INLINE bool operator!=(const NkStringView& lhs, const char* rhs) noexcept {
            return lhs.Compare(NkStringView(rhs)) != 0;
        }

        /**
         * @brief Comparaison de différence C-string vs NkStringView (symétrique)
         * 
         * @param lhs Chaîne C
         * @param rhs Vue à comparer
         * @return true si inégalité
         */
        NK_INLINE bool operator!=(const char* lhs, const NkStringView& rhs) noexcept {
            return NkStringView(lhs).Compare(rhs) != 0;
        }

        // =====================================================================
        // OPÉRATEURS DE COMPARAISON GÉNÉRIQUES (Templates)
        // =====================================================================

        /**
         * @brief Comparaison inférieure générique via NkStringView
         * 
         * @tparam T1 Type convertible en NkStringView
         * @tparam T2 Type convertible en NkStringView
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs < rhs lexicographiquement
         */
        template<typename T1, typename T2>
        NK_INLINE bool operator<(const T1& lhs, const T2& rhs) noexcept {
            return NkStringView(lhs).Compare(NkStringView(rhs)) < 0;
        }

        /**
         * @brief Comparaison inférieure ou égale générique
         * 
         * @tparam T1 Type convertible en NkStringView
         * @tparam T2 Type convertible en NkStringView
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs <= rhs
         */
        template<typename T1, typename T2>
        NK_INLINE bool operator<=(const T1& lhs, const T2& rhs) noexcept {
            return NkStringView(lhs).Compare(NkStringView(rhs)) <= 0;
        }

        /**
         * @brief Comparaison supérieure générique
         * 
         * @tparam T1 Type convertible en NkStringView
         * @tparam T2 Type convertible en NkStringView
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs > rhs
         */
        template<typename T1, typename T2>
        NK_INLINE bool operator>(const T1& lhs, const T2& rhs) noexcept {
            return NkStringView(lhs).Compare(NkStringView(rhs)) > 0;
        }

        /**
         * @brief Comparaison supérieure ou égale générique
         * 
         * @tparam T1 Type convertible en NkStringView
         * @tparam T2 Type convertible en NkStringView
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si lhs >= rhs
         */
        template<typename T1, typename T2>
        NK_INLINE bool operator>=(const T1& lhs, const T2& rhs) noexcept {
            return NkStringView(lhs).Compare(NkStringView(rhs)) >= 0;
        }

        // =====================================================================
        // LITTÉRAUX DÉFINIS PAR L'UTILISATEUR (C++11+)
        // =====================================================================
        #if defined(NK_CPP11)

            inline namespace literals {

                /**
                 * @brief Littéral "_sv" pour création concise de NkStringView
                 * 
                 * @param str Pointeur vers les données du littéral
                 * @param len Longueur compile-time du littéral
                 * @return NkStringView sur les données du littéral
                 * 
                 * @note Ex: auto view = "hello"_sv;  // Crée une vue sur "hello"
                 *       Aucune allocation, longueur connue à la compilation.
                 */
                inline NkStringView operator""_sv(const char* str, size_t len) noexcept {
                    return NkStringView(str, static_cast<usize>(len));
                }

                /**
                 * @brief Littéral alternatif "_nv" (nkentseu view)
                 * 
                 * @param str Pointeur vers les données du littéral
                 * @param len Longueur compile-time du littéral
                 * @return NkStringView sur les données du littéral
                 * 
                 * @note Alias pour "_sv" : préfixe spécifique au namespace projet
                 *       pour éviter les conflits avec std::string_view littéral.
                 */
                inline NkStringView operator""_nv(const char* str, size_t len) noexcept {
                    return NkStringView(str, static_cast<usize>(len));
                }

            } // inline namespace literals

        #endif // defined(NK_CPP11)

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGVIEW_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Paramètre de fonction léger sans copie
    // =====================================================================
    {
        // Fonction acceptant n'importe quel type de chaîne sans allocation
        void ProcessInput(nkentseu::NkStringView input) {
            if (input.StartsWith("CMD:")) {
                auto command = input.SubStr(4);  // Vue sur "..." après "CMD:"
                
                if (command.EqualsIgnoreCase("START")) {
                    StartSystem();
                } else if (command.EqualsIgnoreCase("STOP")) {
                    StopSystem();
                }
            }
        }
        
        // Appels possibles sans conversions :
        ProcessInput("CMD:START");              // C-string littérale
        ProcessInput(stdString);                // std::string (si conversion définie)
        ProcessInput(nkString);                 // NkString
        ProcessInput(otherView);                // NkStringView existant
        ProcessInput(buffer.data(), buffer.size()); // Buffer + longueur
    }

    // =====================================================================
    // Exemple 2 : Parsing de configuration avec sous-vues
    // =====================================================================
    {
        bool ParseConfigLine(nkentseu::NkStringView line, ConfigEntry& out) {
            // Ignorer les lignes vides ou commentaires
            auto trimmed = line.Trimmed();
            if (trimmed.Empty() || trimmed.StartsWith('#')) {
                return false;
            }
            
            // Trouver le séparateur clé=valeur
            auto eqPos = trimmed.Find('=');
            if (eqPos == nkentseu::NkStringView::npos) {
                return false;  // Format invalide
            }
            
            // Extraire et nettoyer clé et valeur
            out.key = trimmed.Left(eqPos).Trimmed().ToString();
            out.value = trimmed.SubStr(eqPos + 1).Trimmed().ToString();
            
            // Validation optionnelle
            if (out.key.IsAlphaNumeric() && !out.value.IsNullOrEmpty()) {
                return true;
            }
            
            return false;
        }
    }

    // =====================================================================
    // Exemple 3 : Recherche et remplacement sans allocation intermédiaire
    // =====================================================================
    {
        // Compter les occurrences d'un motif dans un flux
        usize CountOccurrences(nkentseu::NkStringView text, nkentseu::NkStringView pattern) {
            if (pattern.Empty()) {
                return 0;  // Éviter boucle infinie sur pattern vide
            }
            
            usize count = 0;
            usize pos = 0;
            
            while ((pos = text.Find(pattern, pos)) != nkentseu::NkStringView::npos) {
                ++count;
                pos += pattern.Length();  // Avancer après l'occurrence trouvée
            }
            
            return count;
        }
        
        // Usage : CountOccurrences("a,b,c,d", ",") retourne 3
    }

    // =====================================================================
    // Exemple 4 : Validation d'entrée utilisateur avec prédicats
    // =====================================================================
    {
        enum class ValidationRule {
            USERNAME,   // Alphanumérique + underscore, 3-20 chars
            EMAIL,      // Contient @ et ., format basique
            PORT_NUMBER // Entier 1-65535
        };
        
        bool ValidateInput(nkentseu::NkStringView input, ValidationRule rule) {
            switch (rule) {
                case ValidationRule::USERNAME:
                    return input.Length() >= 3 && 
                           input.Length() <= 20 && 
                           input.IsAlphaNumeric() || input.Contains('_');
                           
                case ValidationRule::EMAIL:
                    return input.Contains('@') && 
                           input.Find('.', input.Find('@')) != nkentseu::NkStringView::npos;
                           
                case ValidationRule::PORT_NUMBER:
                    uint32 port = 0;
                    return input.ToUInt32(port) && port >= 1 && port <= 65535;
                    
                default:
                    return false;
            }
        }
    }

    // =====================================================================
    // Exemple 5 : Utilisation des littéraux pour code concis
    // =====================================================================
    {
        using namespace nkentseu::literals;  // Activer les littéraux "_sv" / "_nv"
        
        // Création de vues sans spécifier manuellement la longueur
        auto greeting = "Hello, World!"_sv;
        auto keyword = "ERROR"_nv;
        
        // Comparaisons directes avec littéraux
        if (logLevel == "DEBUG"_sv) {
            EnableVerboseLogging();
        }
        
        // Sous-vues via littéraux
        auto protocol = "https://example.com/path"_sv;
        if (protocol.StartsWith("https://"_sv)) {
            auto host = protocol.SubStr(8);  // Vue sur "example.com/path"
            auto domain = host.Left(host.Find('/'));  // Vue sur "example.com"
        }
    }

    // =====================================================================
    // Exemple 6 : Hash pour tables de hachage (unordered_map-like)
    // =====================================================================
    {
        // Structure de hash pour utiliser NkStringView comme clé
        struct NkStringViewHash {
            using is_transparent = void;  // Permet lookup hétérogène
            
            size_t operator()(nkentseu::NkStringView view) const noexcept {
                return static_cast<size_t>(view.HashFNV1a());
            }
        };
        
        // Structure d'égalité pour la table de hachage
        struct NkStringViewEqual {
            using is_transparent = void;
            
            bool operator()(nkentseu::NkStringView a, nkentseu::NkStringView b) const noexcept {
                return a.Equals(b);
            }
        };
        
        // Utilisation dans un conteneur custom (pseudo-code)
        // HashMap<NkStringView, ValueType, NkStringViewHash, NkStringViewEqual> cache;
        
        // Insertion et lookup sans allocation de clés temporaires
        // cache.Insert("user_session"_sv, sessionData);
        // auto* found = cache.Find(inputView);  // inputView peut être C-string, NkString, etc.
    }

    // =====================================================================
    // Exemple 7 : Attention aux durées de vie (piège classique)
    // =====================================================================
    {
        nkentseu::NkStringView DangerousExample() {
            std::string temp = "temporary";
            
            // ⚠️ DANGEREUX : Retourne une vue sur une variable locale
            // La vue devient invalide dès la sortie de la fonction !
            // return nkentseu::NkStringView(temp);  // NE FAITES PAS ÇA
        }
        
        // ✅ BONNE PRATIQUE : Retourner un type possédant si la durée de vie doit dépasser le scope
        nkentseu::NkString SafeExample() {
            std::string temp = "temporary";
            return nkentseu::NkString(temp);  // Copie : durée de vie indépendante
        }
        
        // ✅ BONNE PRATIQUE : Utiliser NkStringView uniquement pour paramètres ou variables locales
        void SafeUsage(nkentseu::NkStringView input) {
            // Traitement immédiat : la vue est valide pendant l'appel
            ProcessImmediately(input);
            
            // Si besoin de conserver : convertir en type possédant
            if (NeedToStore(input)) {
                storedData = input.ToString();  // Copie explicite
            }
        }
    }
*/