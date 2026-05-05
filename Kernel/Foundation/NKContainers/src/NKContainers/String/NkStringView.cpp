// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringView.cpp
// DESCRIPTION: Implémentation de la classe NkStringView (vue de chaîne non-possessive)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusion de l'en-tête correspondant
// -------------------------------------------------------------------------
#include "NkStringView.h"

// -------------------------------------------------------------------------
// Inclusions des dépendances internes
// -------------------------------------------------------------------------
#include "NkString.h"
#include "NkStringUtils.h"
#include "Encoding/NkASCII.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/Assert/NkAssert.h"

// -------------------------------------------------------------------------
// Inclusions standard
// -------------------------------------------------------------------------
#include <cstdlib>
#include <cstring>

// -------------------------------------------------------------------------
// Namespace principal du projet
// -------------------------------------------------------------------------
namespace nkentseu {

    // =====================================================================
    // CONSTRUCTEURS
    // =====================================================================

    /**
     * @brief Constructeur depuis une instance NkString
     * 
     * @param str Référence constante vers un NkString source
     * 
     * @note Crée une vue sur les données internes de str sans copie.
     *       La vue devient invalide si le NkString source est modifié ou détruit.
     *       Complexité : O(1), aucun allocation mémoire.
     */
    NkStringView::NkStringView(const NkString& str) noexcept
        : mData(str.Data())
        , mLength(str.Length()) {
    }

    // =====================================================================
    // OPÉRATIONS DE COMPARAISON DE CHAÎNES
    // =====================================================================

    /**
     * @brief Compare cette vue avec une autre en ignorant la casse ASCII
     * 
     * @param other Vue de chaîne à comparer
     * @return <0 si this < other, 0 si égal, >0 si this > other (ordre lexicographique)
     * 
     * @note Délègue à string::NkCompareIgnoreCase() pour cohérence API.
     *       Conversion ASCII uniquement : pour Unicode complet, utiliser ICU.
     */
    int NkStringView::CompareIgnoreCase(NkStringView other) const noexcept {
        return string::NkCompareIgnoreCase(*this, other);
    }

    /**
     * @brief Compare avec tri "naturel" (nombres comparés numériquement)
     * 
     * @param other Vue de chaîne à comparer
     * @return <0 si this < other, 0 si égal, >0 si this > other (ordre naturel)
     * 
     * @note Ex: "file2.txt" < "file10.txt" (contre "file10" < "file2" en binaire)
     *       Utile pour le tri de noms de fichiers, versions, etc.
     */
    int NkStringView::CompareNatural(NkStringView other) const noexcept {
        return string::NkCompareNatural(*this, other);
    }

    // =====================================================================
    // PRÉDICATS : PREFIXE / SUFFIXE / CONTENU (CASE-INSENSITIVE)
    // =====================================================================

    /**
     * @brief Vérifie si la vue commence par un préfixe donné (case-insensitive)
     * 
     * @param prefix Vue représentant le préfixe attendu
     * @return true si les premiers caractères correspondent (ASCII case-insensitive)
     */
    bool NkStringView::StartsWithIgnoreCase(NkStringView prefix) const noexcept {
        return string::NkStartsWithIgnoreCase(*this, prefix);
    }

    /**
     * @brief Vérifie si la vue se termine par un suffixe donné (case-insensitive)
     * 
     * @param suffix Vue représentant le suffixe attendu
     * @return true si les derniers caractères correspondent (ASCII case-insensitive)
     */
    bool NkStringView::EndsWithIgnoreCase(NkStringView suffix) const noexcept {
        return string::NkEndsWithIgnoreCase(*this, suffix);
    }

    /**
     * @brief Vérifie si la vue contient une sous-chaîne (case-insensitive)
     * 
     * @param str Sous-chaîne à rechercher
     * @return true si trouvée en ignorant la casse ASCII
     */
    bool NkStringView::ContainsIgnoreCase(NkStringView str) const noexcept {
        return string::NkContainsIgnoreCase(*this, str);
    }

    // =====================================================================
    // PRÉDICATS : CONTENU AVEC ENSEMBLE DE CARACTÈRES
    // =====================================================================

    /**
     * @brief Vérifie si la vue contient au moins un des caractères donnés
     * 
     * @param chars Chaîne C des caractères à rechercher
     * @return true si au moins un caractère de chars est présent
     * 
     * @note Complexité : O(n*m) où n=this->Length(), m=strlen(chars)
     *       Parcours naïf : suffisant pour la plupart des cas d'usage.
     */
    bool NkStringView::ContainsAny(const char* chars) const noexcept {
        for (SizeType i = 0; i < mLength; ++i) {
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[i] == chars[j]) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Vérifie si la vue contient TOUS les caractères donnés
     * 
     * @param chars Chaîne C des caractères requis
     * @return true si chaque caractère de chars apparaît au moins une fois
     * 
     * @note Ex: "abc".ContainsAll("ab") -> true
     *       "abc".ContainsAll("abcd") -> false (d manquant)
     */
    bool NkStringView::ContainsAll(const char* chars) const noexcept {
        for (SizeType j = 0; chars[j] != '\0'; ++j) {
            bool found = false;
            for (SizeType i = 0; i < mLength; ++i) {
                if (mData[i] == chars[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Vérifie si la vue ne contient AUCUN des caractères donnés
     * 
     * @param chars Chaîne C des caractères interdits
     * @return true si aucun caractère de chars n'est présent
     * 
     * @note Équivalent à !ContainsAny(chars) : implémentation par délégation.
     */
    bool NkStringView::ContainsNone(const char* chars) const noexcept {
        return !ContainsAny(chars);
    }

    // =====================================================================
    // RECHERCHE DE POSITIONS : FIND (RECHERCHE AVANT)
    // =====================================================================

    /**
     * @brief Recherche la première occurrence d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position de départ pour la recherche (défaut: 0)
     * @return Index de la première occurrence, ou npos si non trouvé
     * 
     * @note Implémentation naïve O(n*m) : suffisante pour la plupart des cas.
     *       Pour performances critiques sur grands textes, envisager
     *       des algorithmes avancés (Boyer-Moore, Knuth-Morris-Pratt).
     */
    NkStringView::SizeType NkStringView::Find(
        NkStringView str, 
        SizeType pos) const noexcept {

        if (str.mLength == 0) {
            return pos;
        }
        if (pos > mLength) {
            return npos;
        }
        if (str.mLength > mLength - pos) {
            return npos;
        }
        for (SizeType i = pos; i <= mLength - str.mLength; ++i) {
            if (memory::NkMemCompare(mData + i, str.mData, str.mLength) == 0) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Recherche la première occurrence d'un caractère
     * 
     * @param c Caractère à rechercher
     * @param pos Position de départ (défaut: 0)
     * @return Index de la première occurrence, ou npos si absent
     * 
     * @note Parcours linéaire optimisé : O(n) worst-case, très efficace.
     */
    NkStringView::SizeType NkStringView::Find(char c, SizeType pos) const noexcept {
        if (pos >= mLength) {
            return npos;
        }
        for (SizeType i = pos; i < mLength; ++i) {
            if (mData[i] == c) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Recherche case-insensitive d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position de départ
     * @return Index de la première occurrence (ASCII case-insensitive), ou npos
     */
    NkStringView::SizeType NkStringView::FindIgnoreCase(
        NkStringView str, 
        SizeType pos) const noexcept {
        return string::NkFindIgnoreCase(*this, str, pos);
    }

    // =====================================================================
    // RECHERCHE DE POSITIONS : RFIND (RECHERCHE ARRIÈRE)
    // =====================================================================

    /**
     * @brief Recherche la dernière occurrence d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position limite pour recherche arrière (défaut: npos = fin)
     * @return Index de la dernière occurrence, ou npos si non trouvé
     * 
     * @note Recherche depuis min(pos, Length()-str.Length()) vers le début.
     */
    NkStringView::SizeType NkStringView::RFind(
        NkStringView str, 
        SizeType pos) const noexcept {

        if (str.mLength == 0) {
            return (pos < mLength) ? pos : mLength;
        }
        if (str.mLength > mLength) {
            return npos;
        }
        SizeType searchPos = (pos == npos || pos >= mLength - str.mLength) 
            ? mLength - str.mLength 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            if (memory::NkMemCompare(mData + idx, str.mData, str.mLength) == 0) {
                return idx;
            }
        }
        return npos;
    }

    /**
     * @brief Recherche la dernière occurrence d'un caractère
     * 
     * @param c Caractère à rechercher
     * @param pos Position limite pour recherche arrière (défaut: npos)
     * @return Index de la dernière occurrence, ou npos si absent
     */
    NkStringView::SizeType NkStringView::RFind(char c, SizeType pos) const noexcept {
        if (mLength == 0) {
            return npos;
        }
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            if (mData[i - 1] == c) {
                return i - 1;
            }
        }
        return npos;
    }

    /**
     * @brief Recherche arrière case-insensitive d'une sous-chaîne
     * 
     * @param str Sous-chaîne à rechercher
     * @param pos Position limite pour recherche arrière
     * @return Index de la dernière occurrence (ASCII case-insensitive), ou npos
     */
    NkStringView::SizeType NkStringView::RFindIgnoreCase(
        NkStringView str, 
        SizeType pos) const noexcept {
        return string::NkFindLastIgnoreCase(*this, str);
    }

    // =====================================================================
    // RECHERCHE PAR ENSEMBLE : FINDFIRSTOF / FINDLASTOF
    // =====================================================================

    /**
     * @brief Trouve le premier caractère appartenant à un ensemble (NkStringView)
     * 
     * @param chars Vue des caractères à rechercher
     * @param pos Position de départ
     * @return Index du premier match, ou npos si aucun
     */
    NkStringView::SizeType NkStringView::FindFirstOf(
        NkStringView chars, 
        SizeType pos) const noexcept {
        for (SizeType i = pos; i < mLength; ++i) {
            for (SizeType j = 0; j < chars.mLength; ++j) {
                if (mData[i] == chars.mData[j]) {
                    return i;
                }
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le premier caractère appartenant à un ensemble (C-string)
     * 
     * @param chars Chaîne C des caractères à rechercher
     * @param pos Position de départ
     * @return Index du premier match, ou npos si aucun
     */
    NkStringView::SizeType NkStringView::FindFirstOf(
        const char* chars, 
        SizeType pos) const noexcept {
        for (SizeType i = pos; i < mLength; ++i) {
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[i] == chars[j]) {
                    return i;
                }
            }
        }
        return npos;
    }

    /**
     * @brief Alias pour Find(char, pos) via interface FindFirstOf
     * 
     * @param c Caractère unique à rechercher
     * @param pos Position de départ
     * @return Index de la première occurrence
     */
    NkStringView::SizeType NkStringView::FindFirstOf(char c, SizeType pos) const noexcept {
        return Find(c, pos);
    }

    /**
     * @brief Trouve le dernier caractère appartenant à un ensemble (NkStringView)
     * 
     * @param chars Vue des caractères à rechercher
     * @param pos Position limite pour recherche arrière
     * @return Index du dernier match, ou npos si aucun
     */
    NkStringView::SizeType NkStringView::FindLastOf(
        NkStringView chars, 
        SizeType pos) const noexcept {
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            for (SizeType j = 0; j < chars.mLength; ++j) {
                if (mData[idx] == chars.mData[j]) {
                    return idx;
                }
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le dernier caractère appartenant à un ensemble (C-string)
     * 
     * @param chars Chaîne C des caractères à rechercher
     * @param pos Position limite pour recherche arrière
     * @return Index du dernier match, ou npos si aucun
     */
    NkStringView::SizeType NkStringView::FindLastOf(
        const char* chars, 
        SizeType pos) const noexcept {
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[idx] == chars[j]) {
                    return idx;
                }
            }
        }
        return npos;
    }

    /**
     * @brief Alias pour RFind(char, pos) via interface FindLastOf
     * 
     * @param c Caractère unique à rechercher
     * @param pos Position limite pour recherche arrière
     * @return Index de la dernière occurrence
     */
    NkStringView::SizeType NkStringView::FindLastOf(char c, SizeType pos) const noexcept {
        return RFind(c, pos);
    }

    // =====================================================================
    // RECHERCHE PAR EXCLUSION : FINDFIRSTNOTOF / FINDLASTNOTOF
    // =====================================================================

    /**
     * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble (NkStringView)
     * 
     * @param chars Vue des caractères à exclure
     * @param pos Position de départ
     * @return Index du premier caractère "extérieur", ou npos
     */
    NkStringView::SizeType NkStringView::FindFirstNotOf(
        NkStringView chars, 
        SizeType pos) const noexcept {
        for (SizeType i = pos; i < mLength; ++i) {
            bool found = false;
            for (SizeType j = 0; j < chars.mLength; ++j) {
                if (mData[i] == chars.mData[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble (C-string)
     * 
     * @param chars Chaîne C des caractères à exclure
     * @param pos Position de départ
     * @return Index du premier caractère "extérieur", ou npos
     */
    NkStringView::SizeType NkStringView::FindFirstNotOf(
        const char* chars, 
        SizeType pos) const noexcept {
        for (SizeType i = pos; i < mLength; ++i) {
            bool found = false;
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[i] == chars[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le premier caractère différent d'un caractère unique
     * 
     * @param c Caractère à exclure (recherche tout sauf c)
     * @param pos Position de départ
     * @return Index du premier caractère différent de c, ou npos
     */
    NkStringView::SizeType NkStringView::FindFirstNotOf(char c, SizeType pos) const noexcept {
        for (SizeType i = pos; i < mLength; ++i) {
            if (mData[i] != c) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble (NkStringView)
     * 
     * @param chars Vue des caractères à exclure
     * @param pos Position limite pour recherche arrière
     * @return Index du dernier caractère "extérieur", ou npos
     */
    NkStringView::SizeType NkStringView::FindLastNotOf(
        NkStringView chars, 
        SizeType pos) const noexcept {
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            bool found = false;
            for (SizeType j = 0; j < chars.mLength; ++j) {
                if (mData[idx] == chars.mData[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return idx;
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble (C-string)
     * 
     * @param chars Chaîne C des caractères à exclure
     * @param pos Position limite pour recherche arrière
     * @return Index du dernier caractère "extérieur", ou npos
     */
    NkStringView::SizeType NkStringView::FindLastNotOf(
        const char* chars, 
        SizeType pos) const noexcept {
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            SizeType idx = i - 1;
            bool found = false;
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[idx] == chars[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return idx;
            }
        }
        return npos;
    }

    /**
     * @brief Trouve le dernier caractère différent d'un caractère unique
     * 
     * @param c Caractère à exclure
     * @param pos Position limite pour recherche arrière
     * @return Index du dernier caractère différent de c, ou npos
     */
    NkStringView::SizeType NkStringView::FindLastNotOf(char c, SizeType pos) const noexcept {
        SizeType searchPos = (pos == npos || pos >= mLength) 
            ? mLength - 1 
            : pos;
        for (SizeType i = searchPos + 1; i > 0; --i) {
            if (mData[i - 1] != c) {
                return i - 1;
            }
        }
        return npos;
    }

    // =====================================================================
    // COMPTAGE D'OCCURRENCES
    // =====================================================================

    /**
     * @brief Compte le nombre d'occurrences d'une sous-chaîne
     * 
     * @param str Sous-chaîne à compter
     * @return Nombre de fois où str apparaît (non-overlapping)
     * 
     * @note Délègue à string::NkCount() pour cohérence API.
     */
    NkStringView::SizeType NkStringView::Count(NkStringView str) const noexcept {
        return string::NkCount(*this, str);
    }

    /**
     * @brief Compte le nombre d'occurrences d'un caractère
     * 
     * @param c Caractère à compter
     * @return Fréquence du caractère dans cette vue
     * 
     * @note Délègue à string::NkCount() : parcours linéaire optimisé.
     */
    NkStringView::SizeType NkStringView::Count(char c) const noexcept {
        return string::NkCount(*this, c);
    }

    /**
     * @brief Compte le total d'occurrences de caractères d'un ensemble
     * 
     * @param chars Chaîne C des caractères à comptabiliser
     * @return Somme des fréquences de chaque caractère de chars
     * 
     * @note Ex: "aabbcc".CountAny("ab") -> 4 (2 'a' + 2 'b')
     */
    NkStringView::SizeType NkStringView::CountAny(const char* chars) const noexcept {
        SizeType count = 0;
        for (SizeType i = 0; i < mLength; ++i) {
            for (SizeType j = 0; chars[j] != '\0'; ++j) {
                if (mData[i] == chars[j]) {
                    ++count;
                    break;
                }
            }
        }
        return count;
    }

    // =====================================================================
    // CONVERSIONS NUMÉRIQUES - PARSING
    // =====================================================================

    /**
     * @brief Tente de parser cette vue comme entier signé 32 bits
     * 
     * @param out Référence pour stocker le résultat
     * @return true si parsing réussi, false si format invalide
     * 
     * @note Délègue à string::NkParseInt() : accepte espaces, signe, décimaux.
     */
    bool NkStringView::ToInt(int& out) const noexcept {
        return string::NkParseInt(*this, out);
    }

    /**
     * @brief Alias pour ToInt() avec type explicite int32
     * 
     * @param out Référence pour le résultat int32
     * @return true si succès, false sinon
     */
    bool NkStringView::ToInt32(int32& out) const noexcept {
        return string::NkParseInt(*this, out);
    }

    /**
     * @brief Tente de parser comme entier signé 64 bits
     * 
     * @param out Référence pour le résultat int64
     * @return true si succès, false sinon
     */
    bool NkStringView::ToInt64(int64& out) const noexcept {
        return string::NkParseInt64(*this, out);
    }

    /**
     * @brief Tente de parser comme entier non-signé 32 bits
     * 
     * @param out Référence pour le résultat uint32
     * @return true si succès, false si négatif ou format invalide
     */
    bool NkStringView::ToUInt(uint32& out) const noexcept {
        return string::NkParseUInt(*this, out);
    }

    /**
     * @brief Tente de parser comme entier non-signé 64 bits
     * 
     * @param out Référence pour le résultat uint64
     * @return true si succès, false sinon
     */
    bool NkStringView::ToUInt64(uint64& out) const noexcept {
        return string::NkParseUInt64(*this, out);
    }

    /**
     * @brief Tente de parser comme flottant 32 bits
     * 
     * @param out Référence pour le résultat float32
     * @return true si succès, false si format invalide
     */
    bool NkStringView::ToFloat(float32& out) const noexcept {
        return string::NkParseFloat(*this, out);
    }

    /**
     * @brief Tente de parser comme flottant 64 bits
     * 
     * @param out Référence pour le résultat float64
     * @return true si succès, false sinon
     */
    bool NkStringView::ToDouble(float64& out) const noexcept {
        return string::NkParseDouble(*this, out);
    }

    /**
     * @brief Tente de parser comme valeur booléenne texte
     * 
     * @param out Référence pour le résultat bool
     * @return true si parsing réussi
     * 
     * @note Accepte : "true"/"false", "1"/"0", "yes"/"no" (case-insensitive)
     */
    bool NkStringView::ToBool(bool& out) const noexcept {
        return string::NkParseBool(*this, out);
    }

    // =====================================================================
    // CONVERSIONS AVEC VALEURS PAR DÉFAUT (FALLBACK)
    // =====================================================================

    /**
     * @brief Parse en int ou retourne la valeur par défaut
     * 
     * @param defaultValue Valeur à retourner en cas d'échec
     * @return Résultat du parsing ou defaultValue
     * 
     * @note Évite la gestion explicite de bool de retour.
     *       Utile pour les configurations, paramètres optionnels.
     */
    int NkStringView::ToIntOrDefault(int defaultValue) const noexcept {
        int result;
        return ToInt(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en int32 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    int32 NkStringView::ToInt32OrDefault(int32 defaultValue) const noexcept {
        int32 result;
        return ToInt32(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en int64 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    int64 NkStringView::ToInt64OrDefault(int64 defaultValue) const noexcept {
        int64 result;
        return ToInt64(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en uint32 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    uint32 NkStringView::ToUIntOrDefault(uint32 defaultValue) const noexcept {
        uint32 result;
        return ToUInt(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en uint64 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    uint64 NkStringView::ToUInt64OrDefault(uint64 defaultValue) const noexcept {
        uint64 result;
        return ToUInt64(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en float32 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    float32 NkStringView::ToFloatOrDefault(float32 defaultValue) const noexcept {
        float32 result;
        return ToFloat(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en float64 avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    float64 NkStringView::ToDoubleOrDefault(float64 defaultValue) const noexcept {
        float64 result;
        return ToDouble(result) ? result : defaultValue;
    }

    /**
     * @brief Parse en bool avec fallback
     * 
     * @param defaultValue Valeur par défaut en cas d'échec
     * @return Résultat ou defaultValue
     */
    bool NkStringView::ToBoolOrDefault(bool defaultValue) const noexcept {
        bool result;
        return ToBool(result) ? result : defaultValue;
    }

    // =====================================================================
    // CONVERSIONS VERS TYPES POSSÉDANTS (NkString)
    // =====================================================================

    /**
     * @brief Crée une copie possédante de cette vue
     * 
     * @return Nouvelle instance NkString avec copie des données
     * 
     * @note Allocation mémoire nécessaire : utiliser avec précaution
     *       dans les boucles ou code performance-critical.
     */
    NkString NkStringView::ToString() const noexcept {
        return NkString(mData, mLength);
    }

    /**
     * @brief Retourne une version en minuscules (ASCII)
     * 
     * @return Nouveau NkString avec caractères convertis
     * 
     * @note Ne modifie pas la vue source : retourne une copie transformée.
     */
    NkString NkStringView::ToLower() const noexcept {
        return string::NkToLower(*this);
    }

    /**
     * @brief Retourne une version en majuscules (ASCII)
     * 
     * @return Nouveau NkString avec caractères convertis
     */
    NkString NkStringView::ToUpper() const noexcept {
        return string::NkToUpper(*this);
    }

    /**
     * @brief Retourne une version avec première lettre en majuscule
     * 
     * @return Nouveau NkString capitalisé
     * 
     * @note Ex: "hello world" -> "Hello world"
     */
    NkString NkStringView::ToCapitalized() const noexcept {
        return string::NkCapitalize(*this);
    }

    /**
     * @brief Retourne une version en Title Case (majuscule après espace)
     * 
     * @return Nouveau NkString en title case
     * 
     * @note Ex: "hello world" -> "Hello World"
     */
    NkString NkStringView::ToTitleCase() const noexcept {
        return string::NkTitleCase(*this);
    }

    // =====================================================================
    // PRÉDICATS DE CONTENU (TESTS SÉMANTIQUES)
    // =====================================================================

    /**
     * @brief Vérifie si la vue ne contient que des espaces blancs
     * 
     * @return true si tous les caractères sont whitespace
     */
    bool NkStringView::IsWhitespace() const noexcept {
        return string::NkIsAllWhitespace(*this);
    }

    /**
     * @brief Vérifie si la vue ne contient que des chiffres décimaux
     * 
     * @return true si tous les caractères sont '0'-'9'
     */
    bool NkStringView::IsDigits() const noexcept {
        return string::NkIsAllDigits(*this);
    }

    /**
     * @brief Vérifie si la vue ne contient que des lettres ASCII
     * 
     * @return true si tous les caractères sont A-Z ou a-z
     */
    bool NkStringView::IsAlpha() const noexcept {
        return string::NkIsAllAlpha(*this);
    }

    /**
     * @brief Vérifie si la vue est alphanumérique ASCII
     * 
     * @return true si tous les caractères sont lettres ou chiffres
     */
    bool NkStringView::IsAlphaNumeric() const noexcept {
        return string::NkIsAllAlphaNumeric(*this);
    }

    /**
     * @brief Vérifie si la vue contient uniquement des chiffres hexadécimaux
     * 
     * @return true si tous les caractères sont 0-9, A-F ou a-f
     */
    bool NkStringView::IsHexDigits() const noexcept {
        return string::NkIsAllHexDigits(*this);
    }

    /**
     * @brief Vérifie si tous les caractères alphabétiques sont en minuscules
     * 
     * @return true si aucune majuscule présente (ignore non-lettres)
     */
    bool NkStringView::IsLower() const noexcept {
        for (SizeType i = 0; i < mLength; ++i) {
            if (encoding::ascii::NkIsUpper(mData[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Vérifie si tous les caractères alphabétiques sont en majuscules
     * 
     * @return true si aucune minuscule présente (ignore non-lettres)
     */
    bool NkStringView::IsUpper() const noexcept {
        for (SizeType i = 0; i < mLength; ++i) {
            if (encoding::ascii::NkIsLower(mData[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Vérifie si tous les caractères sont imprimables ASCII
     * 
     * @return true si tous dans la plage 32-126
     */
    bool NkStringView::IsPrintable() const noexcept {
        return string::NkIsAllPrintable(*this);
    }

    /**
     * @brief Vérifie si la vue représente un nombre valide
     * 
     * @return true si format entier ou flottant reconnu
     */
    bool NkStringView::IsNumeric() const noexcept {
        return string::NkIsNumeric(*this);
    }

    /**
     * @brief Vérifie si la vue représente un entier valide
     * 
     * @return true si format entier signé/non-signé reconnu
     */
    bool NkStringView::IsInteger() const noexcept {
        return string::NkIsInteger(*this);
    }

    /**
     * @brief Vérifie si la vue représente un flottant valide
     * 
     * @return true si format décimal reconnu (contient un point)
     */
    bool NkStringView::IsFloat() const noexcept {
        return string::NkIsNumeric(*this) && Contains('.');
    }

    /**
     * @brief Vérifie si la vue représente une valeur booléenne texte
     * 
     * @return true si "true"/"false"/"1"/"0" (case-insensitive)
     */
    bool NkStringView::IsBoolean() const noexcept {
        NkStringView trimmed = Trimmed();
        return trimmed == "true" || 
               trimmed == "false" ||
               trimmed == "TRUE" || 
               trimmed == "FALSE" ||
               trimmed == "True" || 
               trimmed == "False" ||
               trimmed == "1" || 
               trimmed == "0";
    }

    /**
     * @brief Vérifie si la vue est un palindrome
     * 
     * @return true si symétrie caractère par caractère
     * 
     * @note Délègue à string::NkIsPalindrome() : comparaison case-sensitive.
     */
    bool NkStringView::IsPalindrome() const noexcept {
        return string::NkIsPalindrome(*this);
    }

    // =====================================================================
    // UTILITAIRES : HASHING ET SIMILARITÉ
    // =====================================================================

    /**
     * @brief Calcule un hash rapide de cette vue
     * 
     * @return Valeur de hash SizeType
     * 
     * @note Algorithme FNV-1a : bon compromis distribution/performance.
     *       Non-cryptographique : pour tables de hachage uniquement.
     */
    NkStringView::SizeType NkStringView::Hash() const noexcept {
        return string::NkHashFNV1a(*this);
    }

    /**
     * @brief Calcule un hash case-insensitive
     * 
     * @return Valeur de hash SizeType (ASCII lowercase avant hash)
     */
    NkStringView::SizeType NkStringView::HashIgnoreCase() const noexcept {
        return string::NkHashFNV1aIgnoreCase(*this);
    }

    /**
     * @brief Alias pour Hash() avec nom explicite FNV-1a
     * 
     * @return Valeur de hash SizeType selon spécification FNV-1a
     */
    NkStringView::SizeType NkStringView::HashFNV1a() const noexcept {
        return string::NkHashFNV1a(*this);
    }

    /**
     * @brief Alias pour HashIgnoreCase() avec nom explicite FNV-1a
     * 
     * @return Valeur de hash SizeType (ASCII lowercase avant FNV-1a)
     */
    NkStringView::SizeType NkStringView::HashFNV1aIgnoreCase() const noexcept {
        return string::NkHashFNV1aIgnoreCase(*this);
    }

    /**
     * @brief Calcule la distance de Levenshtein (nombre d'éditions)
     * 
     * @param other Vue à comparer
     * @return Nombre minimal d'insertions/suppressions/substitutions
     * 
     * @note Complexité O(n*m) : utiliser avec précaution sur longues chaînes.
     *       Utile pour : fuzzy matching, correction orthographique.
     */
    NkStringView::SizeType NkStringView::LevenshteinDistance(
        NkStringView other) const noexcept {
        return string::NkLevenshteinDistance(*this, other);
    }

    /**
     * @brief Calcule un score de similarité normalisé [0.0 - 1.0]
     * 
     * @param other Vue à comparer
     * @return 1.0 si identique, 0.0 si totalement différent
     * 
     * @note Basé sur la distance de Levenshtein normalisée par la longueur max.
     */
    float64 NkStringView::Similarity(NkStringView other) const noexcept {
        return string::NkSimilarity(*this, other);
    }

    // =====================================================================
    // UTILITAIRES : PATTERN MATCHING ET REGEX
    // =====================================================================

    /**
     * @brief Vérifie si la vue correspond à un pattern avec wildcards
     * 
     * @param pattern Pattern avec '*' (multi) et '?' (single)
     * @return true si la vue match le pattern
     * 
     * @note Ex: "file*.txt" match "file123.txt" mais pas "file.doc"
     *       Implémentation backtracking : éviter patterns trop complexes.
     */
    bool NkStringView::MatchesPattern(NkStringView pattern) const noexcept {
        return string::NkMatchesPattern(*this, pattern);
    }

    /**
     * @brief Vérifie si la vue correspond à une expression régulière
     * 
     * @param regex Expression régulière (implémentation simplifiée)
     * @return true si match réussi
     * 
     * @warning Implémentation basique : recherche littérale uniquement.
     *          Pour un vrai moteur regex, intégrer une bibliothèque dédiée.
     */
    bool NkStringView::MatchesRegex(NkStringView regex) const noexcept {
        return Find(regex) != npos;
    }

    // =====================================================================
    // FONCTIONS DE TRIM (SUPPRESSION D'ESPACES)
    // =====================================================================

    /**
     * @brief Retourne une vue avec espaces blancs supprimés aux deux extrémités
     * 
     * @return Nouvelle vue tronquée des espaces en début et fin
     * 
     * @note Aucune copie : la vue résultante référence les données originales.
     *       Espaces blancs : ' ', '\t', '\n', '\r', '\v', '\f'
     */
    NkStringView NkStringView::Trimmed() const noexcept {
        return string::NkTrim(*this);
    }

    /**
     * @brief Retourne une vue avec espaces blancs supprimés à gauche uniquement
     * 
     * @return Nouvelle vue tronquée des espaces en début
     */
    NkStringView NkStringView::TrimmedLeft() const noexcept {
        return string::NkTrimLeft(*this);
    }

    /**
     * @brief Retourne une vue avec espaces blancs supprimés à droite uniquement
     * 
     * @return Nouvelle vue tronquée des espaces en fin
     */
    NkStringView NkStringView::TrimmedRight() const noexcept {
        return string::NkTrimRight(*this);
    }

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Parsing sécurisé de paramètres de configuration
    // =====================================================================
    {
        bool ParseConfigValue(nkentseu::NkStringView key, nkentseu::NkStringView value, Config& cfg) {
            if (key.EqualsIgnoreCase("timeout")) {
                int32 timeout = 0;
                if (value.ToInt32(timeout) && timeout > 0) {
                    cfg.timeout = timeout;
                    return true;
                }
                return false;
            }
            if (key.EqualsIgnoreCase("enable_feature")) {
                bool enabled = false;
                if (value.ToBool(enabled)) {
                    cfg.featureEnabled = enabled;
                    return true;
                }
                return false;
            }
            if (key.EqualsIgnoreCase("max_retries")) {
                uint32 retries = value.ToUIntOrDefault(3);  // Fallback à 3 si invalide
                cfg.maxRetries = (retries <= 10) ? static_cast<uint8>(retries) : 10;
                return true;
            }
            return false;  // Clé inconnue
        }
    }

    // =====================================================================
    // Exemple 2 : Validation d'entrée utilisateur avec prédicats chaînés
    // =====================================================================
    {
        bool IsValidUsername(nkentseu::NkStringView username) {
            return username.Length() >= 3 && 
                   username.Length() <= 20 && 
                   username.IsAlphaNumeric() && 
                   !username.StartsWith("_") && 
                   !username.EndsWith("_");
        }
        
        bool IsValidPort(nkentseu::NkStringView portStr) {
            uint32 port = 0;
            return portStr.IsDigits() && 
                   portStr.ToUInt32(port) && 
                   port >= 1 && port <= 65535;
        }
    }

    // =====================================================================
    // Exemple 3 : Recherche et extraction avec Find/FindFirstOf
    // =====================================================================
    {
        nkentseu::NkString ExtractDomain(nkentseu::NkStringView url) {
            // Trouver le début du domaine après "://"
            auto protoEnd = url.Find("://");
            if (protoEnd == nkentseu::NkStringView::npos) {
                return {};  // URL invalide
            }
            
            auto domainStart = url.SubStr(protoEnd + 3);
            auto domainEnd = domainStart.FindFirstOf("/?#");
            
            return (domainEnd == nkentseu::NkStringView::npos) 
                ? domainStart 
                : domainStart.SubStr(0, domainEnd);
        }
        
        // Usage : ExtractDomain("https://example.com/path") -> "example.com"
    }

    // =====================================================================
    // Exemple 4 : Hash pour tables de hachage case-insensitive
    // =====================================================================
    {
        struct CaseInsensitiveHash {
            size_t operator()(const nkentseu::NkStringView& str) const noexcept {
                return static_cast<size_t>(str.HashIgnoreCase());
            }
        };
        
        struct CaseInsensitiveEqual {
            bool operator()(
                const nkentseu::NkStringView& a, 
                const nkentseu::NkStringView& b) const noexcept {
                return a.EqualsIgnoreCase(b);
            }
        };
        
        // Utilisation dans une HashMap custom (pseudo-code) :
        // HashMap<NkStringView, ValueType, CaseInsensitiveHash, CaseInsensitiveEqual> cache;
        // 
        // Insertion et lookup sans allocation de clés temporaires :
        // cache.Insert("database_url"_sv, "postgres://...");
        // auto* value = cache.Find("DATABASE_URL"_sv);  // Trouve malgré la différence de casse
    }

    // =====================================================================
    // Exemple 5 : Similarité pour suggestion de commandes (fuzzy matching)
    // =====================================================================
    {
        nkentseu::NkStringView FindBestMatch(
            nkentseu::NkStringView userInput,
            const std::vector<nkentseu::NkStringView>& candidates,
            float64 minSimilarity = 0.7) {
            
            nkentseu::NkStringView bestMatch;
            float64 bestScore = 0.0;
            
            for (const auto& candidate : candidates) {
                float64 score = userInput.Similarity(candidate);
                if (score > bestScore && score >= minSimilarity) {
                    bestScore = score;
                    bestMatch = candidate;
                }
            }
            
            return bestMatch;  // Vue vide si aucun match suffisant
        }
        
        // Usage :
        // auto cmds = {"start", "stop", "restart", "status"};
        // auto suggestion = FindBestMatch("strt", cmds);  // -> "start"
    }

    // =====================================================================
    // Exemple 6 : Nettoyage et normalisation de texte utilisateur
    // =====================================================================
    {
        nkentseu::NkString NormalizeInput(nkentseu::NkStringView raw) {
            return raw.Trimmed()           // Supprimer espaces bordure
                      .ToLower()           // Minuscules pour comparaison
                      .ToString();         // Copie possédante pour retour
        }
        
        // Usage :
        // NormalizeInput("  Hello   WORLD  ") -> "hello world"
    }

    // =====================================================================
    // Exemple 7 : Attention aux durées de vie (piège classique)
    // =====================================================================
    {
        // ❌ DANGEREUX : Retourner une vue sur donnée locale
        // nkentseu::NkStringView BadReturn() {
        //     nkentseu::NkString temp = "temporary";
        //     return temp.View();  // ⚠️ Vue invalide après retour de fonction !
        // }
        
        // ✅ BON : Retourner un type possédant si la durée de vie doit dépasser le scope
        nkentseu::NkString SafeReturn() {
            nkentseu::NkString temp = "temporary";
            return temp;  // Copie : durée de vie indépendante
        }
        
        // ✅ BON : Utiliser NkStringView uniquement pour paramètres ou variables locales
        void SafeUsage(nkentseu::NkStringView input) {
            // Traitement immédiat : la vue est valide pendant l'appel
            ProcessImmediately(input);
            
            // Si besoin de conserver : convertir en type possédant
            if (NeedToStore(input)) {
                storedData = input.ToString();  // Copie explicite
            }
        }
    }

    // =====================================================================
    // Exemple 8 : Pattern matching pour filtrage de logs
    // =====================================================================
    {
        bool ShouldLog(nkentseu::NkStringView message, nkentseu::NkStringView filter) {
            if (filter.Empty()) {
                return true;  // Pas de filtre : tout logger
            }
            
            // Support des wildcards * et ?
            return message.MatchesPattern(filter);
        }
        
        // Usage :
        // ShouldLog("ERROR: Connection failed", "ERROR*") -> true
        // ShouldLog("INFO: Started", "ERROR*") -> false
        // ShouldLog("WARN: Disk 90%", "DISK??%") -> true
    }

    // =====================================================================
    // Exemple 9 : Conversion numérique robuste avec fallback
    // =====================================================================
    {
        struct ConfigValue {
            int32 timeout = 30;              // Défaut : 30 secondes
            float32 threshold = 0.75f;       // Défaut : 75%
            bool verbose = false;            // Défaut : silencieux
        };
        
        ConfigValue ParseConfig(const std::map<nkentseu::NkStringView, nkentseu::NkStringView>& raw) {
            ConfigValue cfg;
            
            auto it = raw.find("timeout");
            if (it != raw.end()) {
                cfg.timeout = it->second.ToInt32OrDefault(30);
            }
            
            it = raw.find("threshold");
            if (it != raw.end()) {
                cfg.threshold = it->second.ToFloatOrDefault(0.75f);
            }
            
            it = raw.find("verbose");
            if (it != raw.end()) {
                cfg.verbose = it->second.ToBoolOrDefault(false);
            }
            
            return cfg;
        }
    }

    // =====================================================================
    // Exemple 10 : Bonnes pratiques et pièges à éviter
    // =====================================================================
    {
        // ✅ BON : Utiliser NkStringView pour paramètres, NkString pour retours possédants
        nkentseu::NkString Transform(nkentseu::NkStringView input) {
            if (input.StartsWith("prefix_")) {
                return input.SubStr(7).Trimmed().ToUpper().ToString();
            }
            return input.ToString();  // Copie explicite
        }
        
        // ❌ À ÉVITER : Concaténation naïve en boucle sans pré-allocation
        // nkentseu::NkString bad;
        // for (int i = 0; i < 1000; ++i) {
        //     bad += "chunk";  // Peut déclencher ~10 réallocations
        // }
        
        // ✅ PRÉFÉRER : Réutilisation de buffer avec Reserve
        nkentseu::NkString BuildCsv(const std::vector<const char*>& fields) {
            nkentseu::NkString result;
            
            // Estimation : somme des longueurs + séparateurs
            size_t estimated = 0;
            for (const char* f : fields) {
                estimated += f ? strlen(f) + 1 : 1;
            }
            result.Reserve(estimated);  // Évite N réallocations
            
            for (size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) result += ',';
                if (fields[i]) result += fields[i];
            }
            return result;
        }
    }
*/