// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringUtils.h
// DESCRIPTION: Bibliothèque d'utilitaires de manipulation de chaînes (Split, Join, Trim, Format...)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGUTILS_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGUTILS_H_INCLUDED

    // -------------------------------------------------------------------------
    // Inclusions standard
    // -------------------------------------------------------------------------
    #include <cstdarg>

    // -------------------------------------------------------------------------
    // Inclusions des dépendances du projet
    // -------------------------------------------------------------------------
    #include "NkString.h"
    #include "NkStringView.h"
    #include "NKCore/NkTraits.h"
    #include "NkStringFormat.h"

    // -------------------------------------------------------------------------
    // Namespace principal du projet
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -------------------------------------------------------------------------
        // Namespace dédié aux utilitaires de chaînes
        // -------------------------------------------------------------------------
        namespace string {

            // =====================================================================
            // SECTION : OPÉRATIONS DE BASE
            // =====================================================================

            /**
             * @brief Retourne la longueur d'une vue de chaîne
             * 
             * @param str Vue de chaîne à mesurer
             * @return Nombre de caractères dans la vue
             * 
             * @note Alias pour NkStringView::Length(). Fourni pour cohérence API
             *       avec les autres fonctions utilitaires du namespace.
             */
            NKENTSEU_CORE_API usize NkLength(NkStringView str);

            /**
             * @brief Vérifie si une vue de chaîne est vide
             * 
             * @param str Vue de chaîne à tester
             * @return true si str.Length() == 0, false sinon
             * 
             * @note Alias pour NkStringView::Empty(). N'inclut pas le test nullptr.
             */
            NKENTSEU_CORE_API bool NkEmpty(NkStringView str);

            /**
             * @brief Vérifie si une vue de chaîne n'est pas vide
             * 
             * @param str Vue de chaîne à tester
             * @return true si str.Length() > 0, false sinon
             * 
             * @note Inverse logique de NkEmpty(). Utile pour les conditions positives.
             */
            NKENTSEU_CORE_API bool NkIsNotEmpty(NkStringView str);

            // =====================================================================
            // SECTION : CONVERSION DE CASSE (ASCII)
            // =====================================================================

            /**
             * @brief Convertit une chaîne en minuscules (ASCII)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec caractères convertis
             * 
             * @note Conversion ASCII uniquement via ::tolower.
             *       Pour Unicode complet, utiliser une bibliothèque comme ICU.
             */
            NKENTSEU_CORE_API NkString NkToLower(NkStringView str);

            /**
             * @brief Convertit une chaîne en minuscules in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Modifie directement le contenu : pas d'allocation supplémentaire.
             *       Conversion ASCII uniquement.
             */
            NKENTSEU_CORE_API void NkToLowerInPlace(NkString& str);

            /**
             * @brief Convertit un caractère unique en minuscule (ASCII)
             * 
             * @param ch Caractère à convertir
             * @return Version minuscule si ch est majuscule, sinon ch inchangé
             */
            NKENTSEU_CORE_API char NkToLower(char ch);

            /**
             * @brief Convertit une chaîne en majuscules (ASCII)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec caractères convertis
             */
            NKENTSEU_CORE_API NkString NkToUpper(NkStringView str);

            /**
             * @brief Convertit une chaîne en majuscules in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Modifie directement le contenu : pas d'allocation supplémentaire.
             */
            NKENTSEU_CORE_API void NkToUpperInPlace(NkString& str);

            /**
             * @brief Convertit un caractère unique en majuscule (ASCII)
             * 
             * @param ch Caractère à convertir
             * @return Version majuscule si ch est minuscule, sinon ch inchangé
             */
            NKENTSEU_CORE_API char NkToUpper(char ch);

            /**
             * @brief Alterne la casse des caractères (minuscule->majuscule et inversement)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec casse alternée
             * 
             * @note Ex: "Hello" -> "hELLO"
             *       Conversion ASCII uniquement.
             */
            NKENTSEU_CORE_API NkString NkToggleCase(NkStringView str);

            /**
             * @brief Alterne la casse in-place
             * 
             * @param str Référence vers la chaîne à modifier
             */
            NKENTSEU_CORE_API void NkToggleCaseInPlace(NkString& str);

            /**
             * @brief Inverse la casse (alias pour ToggleCase)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec casse inversée
             * 
             * @note Alias sémantique pour NkToggleCase(). Même comportement.
             */
            NKENTSEU_CORE_API NkString NkSwapCase(NkStringView str);

            /**
             * @brief Inverse la casse in-place (alias pour ToggleCaseInPlace)
             * 
             * @param str Référence vers la chaîne à modifier
             */
            NKENTSEU_CORE_API void NkSwapCaseInPlace(NkString& str);

            // =====================================================================
            // SECTION : TRIM (SUPPRESSION D'ESPACES)
            // =====================================================================

            /**
             * @brief Supprime les espaces blancs à gauche (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle vue tronquée des espaces en début
             * 
             * @note Aucune copie : la vue résultante référence les données originales.
             *       Espaces blancs : ' ', '\t', '\n', '\r', '\v', '\f'
             */
            NKENTSEU_CORE_API NkStringView NkTrimLeft(NkStringView str);

            /**
             * @brief Supprime les espaces blancs à droite (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle vue tronquée des espaces en fin
             */
            NKENTSEU_CORE_API NkStringView NkTrimRight(NkStringView str);

            /**
             * @brief Supprime les espaces blancs aux deux extrémités (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle vue tronquée des espaces en début et fin
             */
            NKENTSEU_CORE_API NkStringView NkTrim(NkStringView str);

            /**
             * @brief Supprime les espaces blancs à gauche (retourne une copie possédante)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec espaces gauche supprimés
             * 
             * @note Alloue une nouvelle chaîne : utiliser NkTrimLeft() si une vue suffit.
             */
            NKENTSEU_CORE_API NkString NkTrimLeftCopy(NkStringView str);

            /**
             * @brief Supprime les espaces blancs à droite (copie possédante)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec espaces droite supprimés
             */
            NKENTSEU_CORE_API NkString NkTrimRightCopy(NkStringView str);

            /**
             * @brief Supprime les espaces blancs aux deux extrémités (copie possédante)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString trimmée
             */
            NKENTSEU_CORE_API NkString NkTrimCopy(NkStringView str);

            /**
             * @brief Supprime les espaces blancs à gauche in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Décale les données vers la gauche : opération O(n).
             *       Peut être coûteuse sur de grandes chaînes.
             */
            NKENTSEU_CORE_API void NkTrimLeftInPlace(NkString& str);

            /**
             * @brief Supprime les espaces blancs à droite in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Ajuste simplement la longueur : opération O(1).
             */
            NKENTSEU_CORE_API void NkTrimRightInPlace(NkString& str);

            /**
             * @brief Supprime les espaces blancs aux deux extrémités in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Combine TrimLeftInPlace + TrimRightInPlace.
             */
            NKENTSEU_CORE_API void NkTrimInPlace(NkString& str);

            /**
             * @brief Trim avec ensemble de caractères personnalisés (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @param chars Vue des caractères à supprimer en bordure
             * @return Nouvelle vue tronquée des caractères spécifiés
             * 
             * @note Ex: NkTrimChars("###hello###", "#") -> "hello"
             *       Recherche linéaire : O(n*m) où n=str.length, m=chars.length
             */
            NKENTSEU_CORE_API NkStringView NkTrimLeftChars(NkStringView str, NkStringView chars);

            /**
             * @brief Trim droit avec caractères personnalisés (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @param chars Vue des caractères à supprimer en bordure
             * @return Nouvelle vue tronquée à droite
             */
            NKENTSEU_CORE_API NkStringView NkTrimRightChars(NkStringView str, NkStringView chars);

            /**
             * @brief Trim bilatéral avec caractères personnalisés (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @param chars Vue des caractères à supprimer en bordure
             * @return Nouvelle vue tronquée des deux côtés
             */
            NKENTSEU_CORE_API NkStringView NkTrimChars(NkStringView str, NkStringView chars);

            // =====================================================================
            // SECTION : SPLIT (DÉCOUPAGE DE CHAÎNES)
            // =====================================================================

            /**
             * @brief Découpe une chaîne par délimiteur dans un conteneur
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param delimiter Vue du délimiteur de séparation
             * @param result Référence vers le conteneur de réception
             * 
             * @note Les éléments ajoutés sont des NkStringView : ils référencent
             *       les données originales de str. Garantir que str reste valide
             *       pendant l'utilisation des résultats.
             * 
             * @par Exemple :
             * @code
             *   std::vector<NkStringView> parts;
             *   NkSplit("a,b,c", ",", parts);
             *   // parts contient : "a", "b", "c" (vues sur la chaîne originale)
             * @endcode
             */
            template<typename Container>
            void NkSplit(NkStringView str, NkStringView delimiter, Container& result) {
                usize start = 0;
                while (start < str.Length()) {
                    auto pos = str.Find(delimiter, start);
                    if (pos == NkStringView::npos) {
                        result.push_back(str.SubStr(start));
                        break;
                    }
                    result.push_back(str.SubStr(start, pos - start));
                    start = pos + delimiter.Length();
                }
            }

            /**
             * @brief Découpe une chaîne par caractère délimiteur dans un conteneur
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param delimiter Caractère délimiteur unique
             * @param result Référence vers le conteneur de réception
             * 
             * @note Version optimisée pour délimiteur single-char : évite la recherche
             *       de sous-chaîne, utilise comparaison directe de caractères.
             */
            template<typename Container>
            void NkSplit(NkStringView str, char delimiter, Container& result) {
                usize start = 0;
                for (usize i = 0; i < str.Length(); ++i) {
                    if (str[i] == delimiter) {
                        result.push_back(str.SubStr(start, i - start));
                        start = i + 1;
                    }
                }
                if (start < str.Length()) {
                    result.push_back(str.SubStr(start));
                }
            }

            /**
             * @brief Découpe une chaîne par n'importe lequel des délimiteurs donnés
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param delimiters Vue des caractères considérés comme délimiteurs
             * @param result Référence vers le conteneur de réception
             * 
             * @note Ex: NkSplitAny("a,b;c|d", ",;|", result) -> ["a","b","c","d"]
             *       Complexité : O(n*m) où n=str.length, m=delimiters.length
             */
            template<typename Container>
            void NkSplitAny(NkStringView str, NkStringView delimiters, Container& result) {
                usize start = 0;
                usize i = 0;
                while (i < str.Length()) {
                    bool isDelimiter = false;
                    for (usize d = 0; d < delimiters.Length(); ++d) {
                        if (str[i] == delimiters[d]) {
                            isDelimiter = true;
                            break;
                        }
                    }
                    if (isDelimiter) {
                        if (i > start) {
                            result.push_back(str.SubStr(start, i - start));
                        }
                        start = i + 1;
                    }
                    ++i;
                }
                if (start < str.Length()) {
                    result.push_back(str.SubStr(start));
                }
            }

            /**
             * @brief Découpe une chaîne en excluant les parties vides du résultat
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param delimiter Vue du délimiteur de séparation
             * @param result Référence vers le conteneur de réception
             * 
             * @note Utile pour : parsing de CSV avec champs vides, tokens multiples consécutifs.
             *       Ex: NkSplitNoEmpty("a,,b,,,c", ",", result) -> ["a","b","c"]
             */
            template<typename Container>
            void NkSplitNoEmpty(NkStringView str, NkStringView delimiter, Container& result) {
                usize start = 0;
                while (start < str.Length()) {
                    auto pos = str.Find(delimiter, start);
                    if (pos == NkStringView::npos) {
                        auto part = str.SubStr(start);
                        if (!part.Empty()) {
                            result.push_back(part);
                        }
                        break;
                    }
                    auto part = str.SubStr(start, pos - start);
                    if (!part.Empty()) {
                        result.push_back(part);
                    }
                    start = pos + delimiter.Length();
                }
            }

            /**
             * @brief Découpe une chaîne avec callback pour traitement immédiat
             * 
             * @tparam Callback Type du foncteur acceptant NkStringView en paramètre
             * @param str Vue de chaîne à découper
             * @param delimiter Caractère délimiteur unique
             * @param callback Foncteur appelé pour chaque partie découpée
             * 
             * @note Évite l'allocation d'un conteneur intermédiaire : idéal pour
             *       le streaming ou le traitement à la volée.
             * 
             * @par Exemple :
             * @code
             *   NkSplitEach("a,b,c", ',', [](NkStringView part) {
             *       ProcessToken(part);  // Traitement immédiat sans stockage
             *   });
             * @endcode
             */
            template<typename Callback>
            void NkSplitEach(NkStringView str, char delimiter, Callback callback) {
                usize start = 0;
                for (usize i = 0; i < str.Length(); ++i) {
                    if (str[i] == delimiter) {
                        if (i > start) {
                            callback(str.SubStr(start, i - start));
                        }
                        start = i + 1;
                    }
                }
                if (start < str.Length()) {
                    callback(str.SubStr(start));
                }
            }

            /**
             * @brief Découpe une chaîne en limitant le nombre de parties produites
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param delimiter Vue du délimiteur de séparation
             * @param maxSplit Nombre maximal de divisions à effectuer
             * @param result Référence vers le conteneur de réception
             * 
             * @note La dernière partie contient tout le reste de la chaîne non-découpé.
             *       Ex: NkSplitN("a.b.c.d", ".", 2, result) -> ["a", "b.c.d"]
             */
            template<typename Container>
            void NkSplitN(NkStringView str, NkStringView delimiter, usize maxSplit, Container& result) {
                usize start = 0;
                usize count = 0;
                while (start < str.Length() && count < maxSplit - 1) {
                    auto pos = str.Find(delimiter, start);
                    if (pos == NkStringView::npos) {
                        break;
                    }
                    result.push_back(str.SubStr(start, pos - start));
                    start = pos + delimiter.Length();
                    ++count;
                }
                if (start < str.Length()) {
                    result.push_back(str.SubStr(start));
                }
            }

            /**
             * @brief Découpe une chaîne par lignes (gestion \n et \r\n)
             * 
             * @tparam Container Type de conteneur supportant push_back(NkStringView)
             * @param str Vue de chaîne à découper
             * @param result Référence vers le conteneur de réception
             * @param keepLineBreaks true pour inclure les sauts de ligne dans les parties
             * 
             * @note Gère automatiquement les fins de ligne Unix (\n) et Windows (\r\n).
             *       Les lignes vides sont conservées sauf si str est vide.
             */
            template<typename Container>
            void NkSplitLines(NkStringView str, Container& result, bool keepLineBreaks = false) {
                usize start = 0;
                for (usize i = 0; i < str.Length(); ++i) {
                    if (str[i] == '\n' || 
                        (str[i] == '\r' && i + 1 < str.Length() && str[i + 1] == '\n')) {
                        if (keepLineBreaks) {
                            result.push_back(
                                str.SubStr(start, i - start + (str[i] == '\r' ? 2 : 1)));
                        } else {
                            result.push_back(str.SubStr(start, i - start));
                        }
                        if (str[i] == '\r') {
                            ++i;
                        }
                        start = i + 1;
                    }
                }
                if (start < str.Length()) {
                    result.push_back(str.SubStr(start));
                }
            }

            // =====================================================================
            // SECTION : JOIN (CONCATÉNATION DE COLLECTIONS)
            // =====================================================================

            /**
             * @brief Concatène un tableau de vues de chaînes avec séparateur
             * 
             * @param strings Pointeur vers le premier élément du tableau de vues
             * @param count Nombre d'éléments dans le tableau
             * @param separator Vue du séparateur à insérer entre les éléments
             * @return Nouvelle instance NkString contenant la concaténation
             * 
             * @note Pré-alloue la taille exacte pour éviter les réallocations multiples.
             *       Gère correctement le cas count == 0 (retourne chaîne vide).
             */
            NKENTSEU_CORE_API NkString NkJoin(
                const NkStringView* strings, 
                usize count, 
                NkStringView separator);

            /**
             * @brief Concatène un tableau statique de vues de chaînes avec séparateur
             * 
             * @tparam N Taille compile-time du tableau
             * @param strings Référence vers le tableau statique de vues
             * @param separator Vue du séparateur à insérer entre les éléments
             * @return Nouvelle instance NkString contenant la concaténation
             * 
             * @note Version template pour tableaux statiques : déduit N automatiquement.
             *       Plus type-safe que la version avec pointeur + count manuel.
             */
            template<usize N>
            NkString NkJoin(const NkStringView (&strings)[N], NkStringView separator) {
                return NkJoin(strings, N, separator);
            }

            /**
             * @brief Concatène un conteneur quelconque de chaînes avec séparateur
             * 
             * @tparam Container Type de conteneur itérable (vector, list, array, etc.)
             * @param strings Référence constante vers le conteneur source
             * @param separator Vue du séparateur à insérer entre les éléments
             * @return Nouvelle instance NkString contenant la concaténation
             * 
             * @note Les éléments du conteneur doivent être convertibles en NkStringView.
             *       Pré-calcule la taille totale pour une allocation unique optimale.
             * 
             * @par Exemple :
             * @code
             *   std::vector<NkString> words = {"Hello", "World"};
             *   auto result = NkJoin(words, " ");  // "Hello World"
             * @endcode
             */
            template<typename Container>
            NkString NkJoin(const Container& strings, NkStringView separator) {
                if (strings.empty()) {
                    return NkString();
                }
                usize totalSize = 0;
                for (const auto& str : strings) {
                    totalSize += NkStringView(str).Length();
                }
                totalSize += separator.Length() * (strings.size() - 1);
                NkString result;
                result.Reserve(totalSize);
                auto it = strings.begin();
                result.Append(*it);
                ++it;
                for (; it != strings.end(); ++it) {
                    result.Append(separator);
                    result.Append(*it);
                }
                return result;
            }

            /**
             * @brief Concatène un conteneur avec transformation de chaque élément
             * 
             * @tparam Container Type de conteneur itérable
             * @tparam Transformer Type du foncteur de transformation (NkStringView -> NkString)
             * @param items Référence constante vers le conteneur source
             * @param separator Vue du séparateur à insérer entre les éléments
             * @param transformer Foncteur appliqué à chaque élément avant concaténation
             * @return Nouvelle instance NkString contenant la concaténation transformée
             * 
             * @note Utile pour : formater des nombres, échapper des chaînes, appliquer un style.
             *       Le transformer est appelé une fois par élément, dans l'ordre d'itération.
             * 
             * @par Exemple :
             * @code
             *   std::vector<int> numbers = {1, 2, 3};
             *   auto result = NkJoinTransform(numbers, ", ", 
             *       [](int n) { return NkFormat("[{0}]", n); });
             *   // Résultat : "[1], [2], [3]"
             * @endcode
             */
            template<typename Container, typename Transformer>
            NkString NkJoinTransform(
                const Container& items, 
                NkStringView separator, 
                Transformer transformer) {
                if (items.empty()) {
                    return NkString();
                }
                NkString result;
                auto it = items.begin();
                result.Append(transformer(*it));
                ++it;
                for (; it != items.end(); ++it) {
                    result.Append(separator);
                    result.Append(transformer(*it));
                }
                return result;
            }

            // =====================================================================
            // SECTION : REMPLACEMENT DE SOUS-CHAÎNES
            // =====================================================================

            /**
             * @brief Remplace la première occurrence d'une sous-chaîne
             * 
             * @param str Vue de chaîne source
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * @return Nouvelle instance NkString avec remplacement effectué
             * 
             * @note Retourne une copie : str n'est pas modifiée.
             *       Si from n'est pas trouvé, retourne une copie identique de str.
             */
            NKENTSEU_CORE_API NkString NkReplace(
                NkStringView str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace toutes les occurrences d'une sous-chaîne
             * 
             * @param str Vue de chaîne source
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * @return Nouvelle instance NkString avec tous les remplacements effectués
             * 
             * @note Parcours séquentiel : les remplacements ne se chevauchent pas.
             *       Ex: NkReplaceAll("aaa", "aa", "b") -> "ba" (pas "bbb")
             */
            NKENTSEU_CORE_API NkString NkReplaceAll(
                NkStringView str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace la première occurrence in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * 
             * @note Peut déclencher réallocation si la nouvelle chaîne est plus longue.
             *       Plus efficace que Replace + assign si str est déjà mutable.
             */
            NKENTSEU_CORE_API void NkReplaceInPlace(
                NkString& str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace toutes les occurrences in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * 
             * @note Attention aux performances : chaque remplacement peut réallocation.
             *       Pour de nombreux remplacements, préférer construire une nouvelle chaîne.
             */
            NKENTSEU_CORE_API void NkReplaceAllInPlace(
                NkString& str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace uniquement la première occurrence (alias pour NkReplace)
             * 
             * @param str Vue de chaîne source
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * @return Nouvelle instance NkString avec remplacement
             */
            NKENTSEU_CORE_API NkString NkReplaceFirst(
                NkStringView str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace uniquement la dernière occurrence
             * 
             * @param str Vue de chaîne source
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             * @return Nouvelle instance NkString avec remplacement de la dernière occurrence
             * 
             * @note Nécessite une recherche depuis la fin : légèrement plus coûteux que ReplaceFirst.
             */
            NKENTSEU_CORE_API NkString NkReplaceLast(
                NkStringView str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace la première occurrence in-place (alias)
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             */
            NKENTSEU_CORE_API void NkReplaceFirstInPlace(
                NkString& str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace la dernière occurrence in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Sous-chaîne à rechercher
             * @param to Chaîne de remplacement
             */
            NKENTSEU_CORE_API void NkReplaceLastInPlace(
                NkString& str, 
                NkStringView from, 
                NkStringView to);

            /**
             * @brief Remplace les occurrences via callback de transformation
             * 
             * @tparam Callback Type du foncteur (NkStringView match -> NkString replacement)
             * @param str Vue de chaîne source
             * @param pattern Sous-chaîne à rechercher
             * @param callback Foncteur appelé pour chaque occurrence trouvée
             * @return Nouvelle instance NkString avec remplacements appliqués
             * 
             * @note Le callback reçoit la sous-chaîne matchée exacte (incluant pattern).
             *       Utile pour : remplacement conditionnel, transformation contextuelle.
             * 
             * @par Exemple :
             * @code
             *   // Remplacer les nombres par leur carré entre crochets
             *   auto result = NkReplaceCallback("a1b2c3", NkStringView("1234567890"),
             *       [](NkStringView match) {
             *           int n = match[0] - '0';
             *           return NkFormat("[{0}]", n * n);
             *       });
             *   // Résultat : "a[1]b[4]c[9]"
             * @endcode
             */
            template<typename Callback>
            NkString NkReplaceCallback(
                NkStringView str, 
                NkStringView pattern, 
                Callback callback) {
                NkString result;
                usize lastPos = 0;
                usize pos = 0;
                while ((pos = str.Find(pattern, lastPos)) != NkStringView::npos) {
                    result.Append(str.SubStr(lastPos, pos - lastPos));
                    result.Append(callback(str.SubStr(pos, pattern.Length())));
                    lastPos = pos + pattern.Length();
                }
                result.Append(str.SubStr(lastPos));
                return result;
            }

            // =====================================================================
            // SECTION : RECHERCHE ET TESTS DE CONTENU
            // =====================================================================

            /**
             * @brief Vérifie si une chaîne commence par un préfixe donné
             * 
             * @param str Vue de chaîne à tester
             * @param prefix Vue du préfixe attendu
             * @return true si str commence exactement par prefix
             * 
             * @note Comparaison binaire : case-sensitive.
             *       Retourne false si prefix.Length() > str.Length().
             */
            NKENTSEU_CORE_API bool NkStartsWith(NkStringView str, NkStringView prefix);

            /**
             * @brief Vérifie le préfixe en ignorant la casse ASCII
             * 
             * @param str Vue de chaîne à tester
             * @param prefix Vue du préfixe attendu
             * @return true si str commence par prefix (case-insensitive ASCII)
             */
            NKENTSEU_CORE_API bool NkStartsWithIgnoreCase(NkStringView str, NkStringView prefix);

            /**
             * @brief Vérifie si une chaîne se termine par un suffixe donné
             * 
             * @param str Vue de chaîne à tester
             * @param suffix Vue du suffixe attendu
             * @return true si str se termine exactement par suffix
             */
            NKENTSEU_CORE_API bool NkEndsWith(NkStringView str, NkStringView suffix);

            /**
             * @brief Vérifie le suffixe en ignorant la casse ASCII
             * 
             * @param str Vue de chaîne à tester
             * @param suffix Vue du suffixe attendu
             * @return true si str se termine par suffix (case-insensitive ASCII)
             */
            NKENTSEU_CORE_API bool NkEndsWithIgnoreCase(NkStringView str, NkStringView suffix);

            /**
             * @brief Vérifie si une chaîne contient une sous-chaîne donnée
             * 
             * @param str Vue de chaîne à tester
             * @param substring Sous-chaîne à rechercher
             * @return true si substring est trouvé dans str
             */
            NKENTSEU_CORE_API bool NkContains(NkStringView str, NkStringView substring);

            /**
             * @brief Vérifie la présence d'une sous-chaîne (case-insensitive)
             * 
             * @param str Vue de chaîne à tester
             * @param substring Sous-chaîne à rechercher
             * @return true si substring est trouvé (ASCII case-insensitive)
             */
            NKENTSEU_CORE_API bool NkContainsIgnoreCase(NkStringView str, NkStringView substring);

            /**
             * @brief Vérifie si str contient au moins un des caractères donnés
             * 
             * @param str Vue de chaîne à tester
             * @param characters Vue des caractères à rechercher
             * @return true si au moins un caractère de characters est présent dans str
             */
            NKENTSEU_CORE_API bool NkContainsAny(NkStringView str, NkStringView characters);

            /**
             * @brief Vérifie si str ne contient AUCUN des caractères donnés
             * 
             * @param str Vue de chaîne à tester
             * @param characters Vue des caractères interdits
             * @return true si aucun caractère de characters n'apparaît dans str
             */
            NKENTSEU_CORE_API bool NkContainsNone(NkStringView str, NkStringView characters);

            /**
             * @brief Vérifie si str contient UNIQUEMENT les caractères donnés
             * 
             * @param str Vue de chaîne à tester
             * @param characters Vue des caractères autorisés
             * @return true si tous les caractères de str sont dans characters
             * 
             * @note Ex: NkContainsOnly("abc", "abcdef") -> true
             *       NkContainsOnly("abc123", "abcdef") -> false
             */
            NKENTSEU_CORE_API bool NkContainsOnly(NkStringView str, NkStringView characters);

            /**
             * @brief Trouve la première occurrence d'un caractère parmi un ensemble
             * 
             * @param str Vue de chaîne à parcourir
             * @param characters Vue des caractères à rechercher
             * @param start Position de départ pour la recherche (défaut: 0)
             * @return Index de la première occurrence, ou npos si aucun match
             */
            NKENTSEU_CORE_API usize NkFindFirstOf(
                NkStringView str, 
                NkStringView characters, 
                usize start = 0);

            /**
             * @brief Trouve la dernière occurrence d'un caractère parmi un ensemble
             * 
             * @param str Vue de chaîne à parcourir
             * @param characters Vue des caractères à rechercher
             * @return Index de la dernière occurrence, ou npos si aucun match
             */
            NKENTSEU_CORE_API usize NkFindLastOf(NkStringView str, NkStringView characters);

            /**
             * @brief Trouve le premier caractère N'APPARTENANT PAS à un ensemble
             * 
             * @param str Vue de chaîne à parcourir
             * @param characters Vue des caractères à exclure
             * @param start Position de départ pour la recherche (défaut: 0)
             * @return Index du premier caractère "extérieur", ou npos si tous exclus
             */
            NKENTSEU_CORE_API usize NkFindFirstNotOf(
                NkStringView str, 
                NkStringView characters, 
                usize start = 0);

            /**
             * @brief Trouve le dernier caractère N'APPARTENANT PAS à un ensemble
             * 
             * @param str Vue de chaîne à parcourir
             * @param characters Vue des caractères à exclure
             * @return Index du dernier caractère "extérieur", ou npos si tous exclus
             */
            NKENTSEU_CORE_API usize NkFindLastNotOf(NkStringView str, NkStringView characters);

            /**
             * @brief Compte le nombre d'occurrences d'une sous-chaîne
             * 
             * @param str Vue de chaîne à analyser
             * @param substring Sous-chaîne à compter
             * @return Nombre d'occurrences non-chevauchantes de substring dans str
             * 
             * @note Parcours séquentiel : complexité O(n*m) dans le pire cas.
             *       Les occurrences ne se chevauchent pas : "aaa".Count("aa") = 1
             */
            NKENTSEU_CORE_API usize NkCount(NkStringView str, NkStringView substring);

            /**
             * @brief Compte le nombre d'occurrences d'un caractère
             * 
             * @param str Vue de chaîne à analyser
             * @param character Caractère à compter
             * @return Fréquence du caractère dans str
             * 
             * @note Parcours linéaire optimisé : O(n), très efficace.
             */
            NKENTSEU_CORE_API usize NkCount(NkStringView str, char character);

            /**
             * @brief Recherche insensible à la casse d'une sous-chaîne
             * 
             * @param str Vue de chaîne à parcourir
             * @param substring Sous-chaîne à rechercher
             * @param start Position de départ pour la recherche (défaut: 0)
             * @return Index de la première occurrence (ASCII case-insensitive), ou npos
             */
            NKENTSEU_CORE_API usize NkFindIgnoreCase(
                NkStringView str, 
                NkStringView substring, 
                usize start = 0);

            /**
             * @brief Recherche arrière insensible à la casse d'une sous-chaîne
             * 
             * @param str Vue de chaîne à parcourir
             * @param substring Sous-chaîne à rechercher
             * @return Index de la dernière occurrence (ASCII case-insensitive), ou npos
             */
            NKENTSEU_CORE_API usize NkFindLastIgnoreCase(NkStringView str, NkStringView substring);

            // =====================================================================
            // SECTION : EXTRACTION DE SOUS-CHAÎNES
            // =====================================================================

            /**
             * @brief Extrait la sous-chaîne située entre deux délimiteurs
             * 
             * @param str Vue de chaîne source
             * @param startDelim Délimiteur de début de l'extraction
             * @param endDelim Délimiteur de fin de l'extraction
             * @return Vue sur la sous-chaîne entre les délimiteurs, ou vue vide si non trouvé
             * 
             * @note Recherche la première occurrence de startDelim, puis la première
             *       occurrence de endDelim après celle-ci. Les délimiteurs ne sont
             *       pas inclus dans le résultat.
             * 
             * @par Exemple :
             * @code
             *   NkSubstringBetween("<tag>content</tag>", "<tag>", "</tag>") 
             *   -> "content"
             * @endcode
             */
            NKENTSEU_CORE_API NkStringView NkSubstringBetween(
                NkStringView str, 
                NkStringView startDelim, 
                NkStringView endDelim);

            /**
             * @brief Extrait la sous-chaîne entre délimiteurs (copie possédante)
             * 
             * @param str Vue de chaîne source
             * @param startDelim Délimiteur de début
             * @param endDelim Délimiteur de fin
             * @return Nouvelle instance NkString contenant l'extraction
             * 
             * @note Alloue une nouvelle chaîne : utiliser NkSubstringBetween() si
             *       une vue suffit et que la source reste valide.
             */
            NKENTSEU_CORE_API NkString NkSubstringBetweenCopy(
                NkStringView str, 
                NkStringView startDelim, 
                NkStringView endDelim);

            /**
             * @brief Extrait tout ce qui précède un délimiteur
             * 
             * @param str Vue de chaîne source
             * @param delimiter Délimiteur de séparation
             * @return Vue sur la partie avant le premier delimiter, ou str entier si non trouvé
             */
            NKENTSEU_CORE_API NkStringView NkSubstringBefore(
                NkStringView str, 
                NkStringView delimiter);

            /**
             * @brief Extrait tout ce qui suit un délimiteur
             * 
             * @param str Vue de chaîne source
             * @param delimiter Délimiteur de séparation
             * @return Vue sur la partie après le premier delimiter, ou vue vide si non trouvé
             */
            NKENTSEU_CORE_API NkStringView NkSubstringAfter(
                NkStringView str, 
                NkStringView delimiter);

            /**
             * @brief Extrait tout ce qui précède le DERNIER délimiteur
             * 
             * @param str Vue de chaîne source
             * @param delimiter Délimiteur de séparation
             * @return Vue sur la partie avant le dernier delimiter, ou str entier si non trouvé
             * 
             * @note Utile pour : extraire le chemin sans le nom de fichier, etc.
             */
            NKENTSEU_CORE_API NkStringView NkSubstringBeforeLast(
                NkStringView str, 
                NkStringView delimiter);

            /**
             * @brief Extrait tout ce qui suit le DERNIER délimiteur
             * 
             * @param str Vue de chaîne source
             * @param delimiter Délimiteur de séparation
             * @return Vue sur la partie après le dernier delimiter, ou vue vide si non trouvé
             * 
             * @note Utile pour : extraire l'extension de fichier, le nom de fichier, etc.
             */
            NKENTSEU_CORE_API NkStringView NkSubstringAfterLast(
                NkStringView str, 
                NkStringView delimiter);

            /**
             * @brief Retourne le premier caractère d'une chaîne
             * 
             * @param str Vue de chaîne source
             * @return Premier caractère, ou '\\0' si str est vide
             * 
             * @warning Aucun check de bornes : vérifier !str.Empty() avant appel si nécessaire.
             */
            NKENTSEU_CORE_API char NkFirstChar(NkStringView str);

            /**
             * @brief Retourne le dernier caractère d'une chaîne
             * 
             * @param str Vue de chaîne source
             * @return Dernier caractère, ou '\\0' si str est vide
             */
            NKENTSEU_CORE_API char NkLastChar(NkStringView str);

            /**
             * @brief Extrait les N premiers caractères (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @param count Nombre de caractères à extraire du début
             * @return Vue sur le préfixe de longueur min(count, str.Length())
             */
            NKENTSEU_CORE_API NkStringView NkFirstChars(NkStringView str, usize count);

            /**
             * @brief Extrait les N derniers caractères (retourne une vue)
             * 
             * @param str Vue de chaîne source
             * @param count Nombre de caractères à extraire de la fin
             * @return Vue sur le suffixe de longueur min(count, str.Length())
             */
            NKENTSEU_CORE_API NkStringView NkLastChars(NkStringView str, usize count);

            /**
             * @brief Extrait une sous-chaîne à partir d'une position (alias pour SubStr)
             * 
             * @param str Vue de chaîne source
             * @param start Position de début de l'extraction
             * @param count Nombre maximal de caractères à extraire (npos = jusqu'à la fin)
             * @return Vue sur la sous-chaîne extraite
             */
            NKENTSEU_CORE_API NkStringView NkMid(
                NkStringView str, 
                usize start, 
                usize count = NkStringView::npos);

            // =====================================================================
            // SECTION : TRANSFORMATIONS DE CHAÎNES
            // =====================================================================

            /**
             * @brief Met la première lettre en majuscule, le reste inchangé
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString capitalisée
             * 
             * @note Ex: "hello world" -> "Hello world"
             *       Conversion ASCII uniquement.
             */
            NKENTSEU_CORE_API NkString NkCapitalize(NkStringView str);

            /**
             * @brief Capitalise la première lettre in-place
             * 
             * @param str Référence vers la chaîne à modifier
             */
            NKENTSEU_CORE_API void NkCapitalizeInPlace(NkString& str);

            /**
             * @brief Met la première lettre de chaque mot en majuscule
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString en title case
             * 
             * @note Délimiteurs de mots : espaces, tabulations.
             *       Ex: "hello world" -> "Hello World"
             */
            NKENTSEU_CORE_API NkString NkTitleCase(NkStringView str);

            /**
             * @brief Applique le title case in-place
             * 
             * @param str Référence vers la chaîne à modifier
             */
            NKENTSEU_CORE_API void NkTitleCaseInPlace(NkString& str);

            /**
             * @brief Inverse l'ordre des caractères
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString inversée
             * 
             * @note Ex: "abc" -> "cba"
             *       Complexité O(n) : copie + inversion.
             */
            NKENTSEU_CORE_API NkString NkReverse(NkStringView str);

            /**
             * @brief Inverse la chaîne in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * 
             * @note Échange symétrique : complexité O(n/2), aucune allocation.
             */
            NKENTSEU_CORE_API void NkReverseInPlace(NkString& str);

            /**
             * @brief Supprime toutes les occurrences des caractères spécifiés
             * 
             * @param str Vue de chaîne source
             * @param charsToRemove Vue des caractères à éliminer
             * @return Nouvelle instance NkString avec caractères supprimés
             * 
             * @note Ex: NkRemoveChars("a1b2c3", "123") -> "abc"
             *       Parcours + compactage : O(n*m) où m=charsToRemove.Length()
             */
            NKENTSEU_CORE_API NkString NkRemoveChars(
                NkStringView str, 
                NkStringView charsToRemove);

            /**
             * @brief Supprime les caractères spécifiés in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param charsToRemove Vue des caractères à éliminer
             * 
             * @note Compactage in-place : peut être coûteux sur grandes chaînes.
             */
            NKENTSEU_CORE_API void NkRemoveCharsInPlace(
                NkString& str, 
                NkStringView charsToRemove);

            /**
             * @brief Supprime les occurrences consécutives d'un caractère
             * 
             * @param str Vue de chaîne source
             * @param duplicateChar Caractère dont il faut supprimer les doublons consécutifs
             * @return Nouvelle instance NkString avec doublons réduits à un seul
             * 
             * @note Ex: NkRemoveDuplicates("aabbcc", 'b') -> "abcc"
             *       Ne supprime que les répétitions adjacentes.
             */
            NKENTSEU_CORE_API NkString NkRemoveDuplicates(
                NkStringView str, 
                char duplicateChar);

            /**
             * @brief Supprime les doublons consécutifs in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param duplicateChar Caractère à dé-dupliquer
             */
            NKENTSEU_CORE_API void NkRemoveDuplicatesInPlace(
                NkString& str, 
                char duplicateChar);

            /**
             * @brief Remplace les séquences d'espaces multiples par un seul espace
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec espaces normalisés
             * 
             * @note Ex: "a   b\t\tc" -> "a b c"
             *       Considère tous les whitespace ASCII comme équivalents.
             */
            NKENTSEU_CORE_API NkString NkRemoveExtraSpaces(NkStringView str);

            /**
             * @brief Normalise les espaces in-place
             * 
             * @param str Référence vers la chaîne à modifier
             */
            NKENTSEU_CORE_API void NkRemoveExtraSpacesInPlace(NkString& str);

            /**
             * @brief Insère une sous-chaîne à une position donnée
             * 
             * @param str Vue de chaîne source
             * @param position Index où insérer les nouvelles données (0 = début)
             * @param insertStr Vue de la chaîne à insérer
             * @return Nouvelle instance NkString avec insertion effectuée
             * 
             * @note Équivalent à : str.SubStr(0,pos) + insertStr + str.SubStr(pos)
             *       Assertion si position > str.Length().
             */
            NKENTSEU_CORE_API NkString NkInsert(
                NkStringView str, 
                usize position, 
                NkStringView insertStr);

            /**
             * @brief Insère une sous-chaîne in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param position Index d'insertion
             * @param insertStr Vue de la chaîne à insérer
             * 
             * @note Peut déclencher réallocation si capacité insuffisante.
             */
            NKENTSEU_CORE_API void NkInsertInPlace(
                NkString& str, 
                usize position, 
                NkStringView insertStr);

            /**
             * @brief Supprime une plage de caractères
             * 
             * @param str Vue de chaîne source
             * @param position Index de début de la plage à supprimer
             * @param count Nombre de caractères à supprimer
             * @return Nouvelle instance NkString avec plage retirée
             * 
             * @note Équivalent à : str.SubStr(0,pos) + str.SubStr(pos+count)
             */
            NKENTSEU_CORE_API NkString NkErase(
                NkStringView str, 
                usize position, 
                usize count);

            /**
             * @brief Supprime une plage de caractères in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param position Index de début de la plage
             * @param count Nombre de caractères à supprimer
             * 
             * @note Décale les données suivantes vers la gauche : O(n) worst-case.
             */
            NKENTSEU_CORE_API void NkEraseInPlace(
                NkString& str, 
                usize position, 
                usize count);

            // =====================================================================
            // SECTION : CONVERSIONS NUMÉRIQUES (PARSING)
            // =====================================================================

            /**
             * @brief Tente de parser une chaîne comme entier 32 bits signé
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour stocker le résultat
             * @return true si parsing réussi, false si format invalide ou overflow
             * 
             * @note Accepte : espaces initiaux, signe +/-, chiffres décimaux.
             *       Rejette tout caractère non-numérique après le nombre.
             */
            NKENTSEU_CORE_API bool NkParseInt(NkStringView str, int32& out);

            /**
             * @brief Tente de parser comme entier 64 bits signé
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat int64
             * @return true si succès, false sinon
             */
            NKENTSEU_CORE_API bool NkParseInt64(NkStringView str, int64& out);

            /**
             * @brief Tente de parser comme entier non-signé 32 bits
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat uint32
             * @return true si succès, false si négatif ou format invalide
             */
            NKENTSEU_CORE_API bool NkParseUInt(NkStringView str, uint32& out);

            /**
             * @brief Tente de parser comme entier non-signé 64 bits
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat uint64
             * @return true si succès, false sinon
             */
            NKENTSEU_CORE_API bool NkParseUInt64(NkStringView str, uint64& out);

            /**
             * @brief Tente de parser comme flottant 32 bits
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat float32
             * @return true si succès, false si format invalide
             * 
             * @note Accepte notation décimale et scientifique (1.23e-4)
             */
            NKENTSEU_CORE_API bool NkParseFloat(NkStringView str, float32& out);

            /**
             * @brief Tente de parser comme flottant 64 bits
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat float64
             * @return true si succès, false sinon
             */
            NKENTSEU_CORE_API bool NkParseDouble(NkStringView str, float64& out);

            /**
             * @brief Tente de parser comme valeur booléenne texte
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat bool
             * @return true si parsing réussi
             * 
             * @note Accepte : "true"/"false", "1"/"0", "yes"/"no" (case-insensitive ASCII)
             */
            NKENTSEU_CORE_API bool NkParseBool(NkStringView str, bool& out);

            // =====================================================================
            // SECTION : FORMATAGE (DÉLÉGUÉ À NkStringFormat.h)
            // =====================================================================

            /**
             * @brief Formatage style printf avec arguments variables
             * 
             * @param format Chaîne de format avec spécificateurs (%s, %d, %.2f, etc.)
             * @param ... Arguments variables correspondant aux spécificateurs
             * @return Nouvelle instance NkString formatée
             * 
             * @note Syntaxe compatible vsnprintf : %s, %d, %f, %x, %.2f, etc.
             *       @warning Ne pas passer de NkString directement : utiliser CStr().
             * 
             * @par Exemple :
             * @code
             *   auto msg = NkFormatf("User %s has %d points", "Alice", 42);
             *   // "User Alice has 42 points"
             * @endcode
             */
            NKENTSEU_CORE_API NkString NkFormatf(const char* format, ...);

            /**
             * @brief Formatage style printf avec va_list
             * 
             * @param format Chaîne de format avec spécificateurs
             * @param args Liste d'arguments de type va_list
             * @return Nouvelle instance NkString formatée
             * 
             * @note Version pour wrappers ou fonctions forwardant les arguments.
             *       Ne modifie pas args : peut être réutilisé après appel.
             */
            NKENTSEU_CORE_API NkString NkVFormatf(const char* format, va_list args);

            // =====================================================================
            // SECTION : CONVERSION VERS CHAÎNE (TOSTRING)
            // =====================================================================

            /**
             * @brief Convertit une valeur quelconque en NkString
             * 
             * @tparam T Type de la valeur (entier, flottant, booléen)
             * @param value Valeur à convertir
             * @param precision Précision pour les flottants (ignorée pour les autres types)
             * @return Nouvelle instance NkString contenant la représentation textuelle
             * 
             * @note Utilise if constexpr pour dispatch compile-time selon le type :
             *       - bool : "true" / "false"
             *       - flottants : formatage avec précision via snprintf
             *       - entiers : %lld ou %llu selon le signe
             *       - autres : static_assert avec message d'erreur
             * 
             * @par Exemple :
             * @code
             *   auto s1 = NkToString(42);           // "42"
             *   auto s2 = NkToString(3.14159, 2);   // "3.14"
             *   auto s3 = NkToString(true);         // "true"
             * @endcode
             */
            template<typename T>
            NKENTSEU_CORE_API NkString NkToString(T value, int precision = 6) {
                if constexpr (traits::NkIsSame_v<T, bool>) {
                    return value ? NkString("true") : NkString("false");
                } else if constexpr (traits::NkIsFloatingPoint_v<T>) {
                    return NkFormatf("%.*f", precision, static_cast<double>(value));
                } else if constexpr (traits::NkIsIntegral_v<T>) {
                    if constexpr (traits::NkIsSigned_v<T>) {
                        return NkFormatf("%lld", static_cast<long long>(value));
                    } else {
                        return NkFormatf("%llu", static_cast<unsigned long long>(value));
                    }
                } else {
                    static_assert(sizeof(T) == 0, "Type non supporté par NkToString");
                    return NkString();
                }
            }

            /**
             * @brief Surcharge explicite pour int32
             * 
             * @param value Valeur entière à convertir
             * @param precision Paramètre ignoré (présent pour uniformité API)
             * @return Représentation décimale de value
             */
            NKENTSEU_CORE_API NkString NkToString(int32 value, int = 6);

            /**
             * @brief Surcharge explicite pour int64
             * 
             * @param value Valeur entière à convertir
             * @param precision Paramètre ignoré
             * @return Représentation décimale de value
             */
            NKENTSEU_CORE_API NkString NkToString(int64 value, int = 6);

            /**
             * @brief Surcharge explicite pour uint32
             * 
             * @param value Valeur entière non-signée à convertir
             * @param precision Paramètre ignoré
             * @return Représentation décimale de value
             */
            NKENTSEU_CORE_API NkString NkToString(uint32 value, int = 6);

            /**
             * @brief Surcharge explicite pour uint64
             * 
             * @param value Valeur entière non-signée à convertir
             * @param precision Paramètre ignoré
             * @return Représentation décimale de value
             */
            NKENTSEU_CORE_API NkString NkToString(uint64 value, int = 6);

            /**
             * @brief Surcharge explicite pour float32
             * 
             * @param value Valeur flottante à convertir
             * @param precision Nombre de décimales (défaut: 6)
             * @return Représentation décimale de value avec précision spécifiée
             */
            NKENTSEU_CORE_API NkString NkToString(float32 value, int precision = 6);

            /**
             * @brief Surcharge explicite pour float64
             * 
             * @param value Valeur flottante à convertir
             * @param precision Nombre de décimales (défaut: 6)
             * @return Représentation décimale de value avec précision spécifiée
             */
            NKENTSEU_CORE_API NkString NkToString(float64 value, int precision = 6);

            /**
             * @brief Surcharge explicite pour bool
             * 
             * @param value Valeur booléenne à convertir
             * @param precision Paramètre ignoré
             * @return "true" ou "false" selon value
             */
            NKENTSEU_CORE_API NkString NkToString(bool value, int = 6);

            /**
             * @brief Conversion vers chaîne via format printf personnalisé
             * 
             * @param format Chaîne de format avec spécificateurs
             * @param ... Arguments variables
             * @return Nouvelle instance NkString formatée
             * 
             * @note Alias pratique pour NkFormatf() : même fonctionnalité, nom alternatif.
             */
            NKENTSEU_CORE_API NkString NkToStringf(const char* format, ...);

            // =====================================================================
            // SECTION : CONVERSIONS HEXADÉCIMALES
            // =====================================================================

            /**
             * @brief Convertit un entier signé 32 bits en hexadécimal
             * 
             * @param value Valeur à convertir
             * @param prefix true pour ajouter le préfixe "0x", false sinon
             * @return Représentation hexadécimale minuscules de value
             * 
             * @note La valeur est traitée comme non-signée pour l'affichage hex.
             *       Ex: NkToHex(-1, true) -> "0xffffffff"
             */
            NKENTSEU_CORE_API NkString NkToHex(int32 value, bool prefix = false);

            /**
             * @brief Convertit un entier signé 64 bits en hexadécimal
             * 
             * @param value Valeur à convertir
             * @param prefix true pour ajouter le préfixe "0x"
             * @return Représentation hexadécimale minuscules de value
             */
            NKENTSEU_CORE_API NkString NkToHex(int64 value, bool prefix = false);

            /**
             * @brief Convertit un entier non-signé 32 bits en hexadécimal
             * 
             * @param value Valeur à convertir
             * @param prefix true pour ajouter le préfixe "0x"
             * @return Représentation hexadécimale minuscules de value
             */
            NKENTSEU_CORE_API NkString NkToHex(uint32 value, bool prefix = false);

            /**
             * @brief Convertit un entier non-signé 64 bits en hexadécimal
             * 
             * @param value Valeur à convertir
             * @param prefix true pour ajouter le préfixe "0x"
             * @return Représentation hexadécimale minuscules de value
             */
            NKENTSEU_CORE_API NkString NkToHex(uint64 value, bool prefix = false);

            /**
             * @brief Tente de parser une chaîne hexadécimale en uint32
             * 
             * @param str Vue de chaîne à parser (avec ou sans préfixe "0x")
             * @param out Référence pour le résultat uint32
             * @return true si parsing réussi, false si format invalide ou overflow
             * 
             * @note Accepte chiffres 0-9, lettres a-f/A-F. Ignorer les espaces initiaux.
             */
            NKENTSEU_CORE_API bool NkParseHex(NkStringView str, uint32& out);

            /**
             * @brief Tente de parser une chaîne hexadécimale en uint64
             * 
             * @param str Vue de chaîne à parser
             * @param out Référence pour le résultat uint64
             * @return true si succès, false sinon
             */
            NKENTSEU_CORE_API bool NkParseHex(NkStringView str, uint64& out);

            // =====================================================================
            // SECTION : COMPARAISONS (CASE-INSENSITIVE & NATURELLES)
            // =====================================================================

            /**
             * @brief Compare deux chaînes en ignorant la casse ASCII
             * 
             * @param lhs Première chaîne à comparer
             * @param rhs Deuxième chaîne à comparer
             * @return <0 si lhs<rhs, 0 si égal, >0 si lhs>rhs (ordre lexicographique)
             */
            NKENTSEU_CORE_API int NkCompareIgnoreCase(NkStringView lhs, NkStringView rhs);

            /**
             * @brief Teste l'égalité de deux chaînes (case-insensitive ASCII)
             * 
             * @param lhs Première chaîne
             * @param rhs Deuxième chaîne
             * @return true si les chaînes sont égales en ignorant la casse
             */
            NKENTSEU_CORE_API bool NkEqualsIgnoreCase(NkStringView lhs, NkStringView rhs);

            /**
             * @brief Compare deux chaînes avec tri "naturel" (nombres comparés numériquement)
             * 
             * @param lhs Première chaîne à comparer
             * @param rhs Deuxième chaîne à comparer
             * @return <0 si lhs<rhs, 0 si égal, >0 si lhs>rhs (ordre naturel)
             * 
             * @note Ex: "file2.txt" < "file10.txt" (contre "file10" < "file2" en binaire)
             *       Utile pour le tri de noms de fichiers, versions, etc.
             */
            NKENTSEU_CORE_API int NkCompareNatural(NkStringView lhs, NkStringView rhs);

            /**
             * @brief Compare avec tri naturel en ignorant la casse
             * 
             * @param lhs Première chaîne à comparer
             * @param rhs Deuxième chaîne à comparer
             * @return Résultat de comparaison naturelle case-insensitive
             */
            NKENTSEU_CORE_API int NkCompareNaturalIgnoreCase(NkStringView lhs, NkStringView rhs);

            // =====================================================================
            // SECTION : PRÉDICATS DE CARACTÈRES ET DE CHAÎNES
            // =====================================================================

            /**
             * @brief Vérifie si un caractère est un espace blanc ASCII
             * 
             * @param ch Caractère à tester
             * @return true pour ' ', '\t', '\n', '\r', '\v', '\f'
             */
            NKENTSEU_CORE_API bool NkIsWhitespace(char ch);

            /**
             * @brief Vérifie si un caractère est un chiffre décimal (0-9)
             * 
             * @param ch Caractère à tester
             * @return true si ch est entre '0' et '9' inclus
             */
            NKENTSEU_CORE_API bool NkIsDigit(char ch);

            /**
             * @brief Vérifie si un caractère est une lettre alphabétique ASCII
             * 
             * @param ch Caractère à tester
             * @return true pour A-Z ou a-z
             */
            NKENTSEU_CORE_API bool NkIsAlpha(char ch);

            /**
             * @brief Vérifie si un caractère est alphanumérique ASCII
             * 
             * @param ch Caractère à tester
             * @return true si lettre ou chiffre
             */
            NKENTSEU_CORE_API bool NkIsAlphaNumeric(char ch);

            /**
             * @brief Vérifie si un caractère est une minuscule ASCII
             * 
             * @param ch Caractère à tester
             * @return true pour a-z
             */
            NKENTSEU_CORE_API bool NkIsLower(char ch);

            /**
             * @brief Vérifie si un caractère est une majuscule ASCII
             * 
             * @param ch Caractère à tester
             * @return true pour A-Z
             */
            NKENTSEU_CORE_API bool NkIsUpper(char ch);

            /**
             * @brief Vérifie si un caractère est un chiffre hexadécimal
             * 
             * @param ch Caractère à tester
             * @return true pour 0-9, A-F ou a-f
             */
            NKENTSEU_CORE_API bool NkIsHexDigit(char ch);

            /**
             * @brief Vérifie si un caractère est imprimable ASCII
             * 
             * @param ch Caractère à tester
             * @return true pour la plage 32-126 (espace à tilde)
             */
            NKENTSEU_CORE_API bool NkIsPrintable(char ch);

            /**
             * @brief Vérifie si une chaîne ne contient que des espaces blancs
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont whitespace
             */
            NKENTSEU_CORE_API bool NkIsAllWhitespace(NkStringView str);

            /**
             * @brief Vérifie si une chaîne ne contient que des chiffres décimaux
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont '0'-'9'
             */
            NKENTSEU_CORE_API bool NkIsAllDigits(NkStringView str);

            /**
             * @brief Vérifie si une chaîne ne contient que des lettres ASCII
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont A-Z ou a-z
             */
            NKENTSEU_CORE_API bool NkIsAllAlpha(NkStringView str);

            /**
             * @brief Vérifie si une chaîne est alphanumérique ASCII
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont lettres ou chiffres
             */
            NKENTSEU_CORE_API bool NkIsAllAlphaNumeric(NkStringView str);

            /**
             * @brief Vérifie si une chaîne ne contient que des chiffres hexadécimaux
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont 0-9, A-F ou a-f
             */
            NKENTSEU_CORE_API bool NkIsAllHexDigits(NkStringView str);

            /**
             * @brief Vérifie si une chaîne ne contient que des caractères imprimables ASCII
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les caractères sont dans la plage 32-126
             */
            NKENTSEU_CORE_API bool NkIsAllPrintable(NkStringView str);

            /**
             * @brief Vérifie si une chaîne représente un nombre valide (entier ou flottant)
             * 
             * @param str Vue de chaîne à tester
             * @return true si format numérique reconnu
             * 
             * @note Accepte : signe optionnel, décimales, exposant scientifique (e/E)
             */
            NKENTSEU_CORE_API bool NkIsNumeric(NkStringView str);

            /**
             * @brief Vérifie si une chaîne représente un entier valide
             * 
             * @param str Vue de chaîne à tester
             * @return true si format entier signé/non-signé reconnu
             * 
             * @note Accepte : espaces initiaux, signe +/-, chiffres décimaux uniquement.
             */
            NKENTSEU_CORE_API bool NkIsInteger(NkStringView str);

            /**
             * @brief Vérifie si une chaîne est un palindrome
             * 
             * @param str Vue de chaîne à tester
             * @param ignoreCase true pour comparaison case-insensitive (ASCII)
             * @return true si la chaîne se lit identiquement dans les deux sens
             * 
             * @note Comparaison caractère par caractère symétrique.
             *       ignoreCase affecte uniquement la comparaison, pas la transformation.
             */
            NKENTSEU_CORE_API bool NkIsPalindrome(NkStringView str, bool ignoreCase = false);

            // =====================================================================
            // SECTION : PADDING (REMPLISSAGE)
            // =====================================================================

            /**
             * @brief Remplit à gauche avec un caractère pour atteindre une largeur cible
             * 
             * @param str Vue de chaîne source
             * @param totalWidth Largeur finale souhaitée
             * @param paddingChar Caractère de remplissage (défaut: espace)
             * @return Nouvelle instance NkString paddée à gauche
             * 
             * @note Si str.Length() >= totalWidth : retourne copie de str inchangée.
             *       Ex: NkPadLeft("42", 5, '0') -> "00042"
             */
            NKENTSEU_CORE_API NkString NkPadLeft(
                NkStringView str, 
                usize totalWidth, 
                char paddingChar = ' ');

            /**
             * @brief Remplit à droite avec un caractère pour atteindre une largeur cible
             * 
             * @param str Vue de chaîne source
             * @param totalWidth Largeur finale souhaitée
             * @param paddingChar Caractère de remplissage (défaut: espace)
             * @return Nouvelle instance NkString paddée à droite
             * 
             * @note Ex: NkPadRight("Hi", 5, '.') -> "Hi..."
             */
            NKENTSEU_CORE_API NkString NkPadRight(
                NkStringView str, 
                usize totalWidth, 
                char paddingChar = ' ');

            /**
             * @brief Centre la chaîne avec remplissage symétrique
             * 
             * @param str Vue de chaîne source
             * @param totalWidth Largeur finale souhaitée
             * @param paddingChar Caractère de remplissage (défaut: espace)
             * @return Nouvelle instance NkString centrée
             * 
             * @note Si padding impair : le caractère excédentaire est ajouté à droite.
             *       Ex: NkPadCenter("X", 5, '-') -> "--X--"
             */
            NKENTSEU_CORE_API NkString NkPadCenter(
                NkStringView str, 
                usize totalWidth, 
                char paddingChar = ' ');

            // =====================================================================
            // SECTION : RÉPÉTITION DE CHAÎNES
            // =====================================================================

            /**
             * @brief Répète une sous-chaîne N fois
             * 
             * @param str Vue de chaîne à répéter
             * @param count Nombre de répétitions
             * @return Nouvelle instance NkString contenant str répétée count fois
             * 
             * @note Pré-alloue la taille exacte : une seule allocation.
             *       Ex: NkRepeat("ab", 3) -> "ababab"
             */
            NKENTSEU_CORE_API NkString NkRepeat(NkStringView str, usize count);

            /**
             * @brief Répète un caractère unique N fois
             * 
             * @param ch Caractère à répéter
             * @param count Nombre de répétitions
             * @return Nouvelle instance NkString contenant ch répété count fois
             * 
             * @note Version optimisée pour single-char : utilise memset-like si possible.
             *       Ex: NkRepeat('*', 5) -> "*****"
             */
            NKENTSEU_CORE_API NkString NkRepeat(char ch, usize count);

            // =====================================================================
            // SECTION : ÉCHAPPEMENT / DÉSÉCHAPPEMENT
            // =====================================================================

            /**
             * @brief Échappe les caractères spéciaux avec un caractère d'échappement
             * 
             * @param str Vue de chaîne source
             * @param charsToEscape Vue des caractères à échapper
             * @param escapeChar Caractère d'échappement à insérer (défaut: '\\')
             * @return Nouvelle instance NkString avec caractères échappés
             * 
             * @note Ex: NkEscape("a\"b", "\"", '\\') -> "a\\\"b"
             *       Chaque caractère de charsToEscape est préfixé par escapeChar.
             */
            NKENTSEU_CORE_API NkString NkEscape(
                NkStringView str, 
                NkStringView charsToEscape, 
                char escapeChar = '\\');

            /**
             * @brief Déséchappe une chaîne précédemment échappée
             * 
             * @param str Vue de chaîne échappée à parser
             * @param escapeChar Caractère d'échappement utilisé (défaut: '\\')
             * @return Nouvelle instance NkString avec séquences d'échappement résolues
             * 
             * @note Ex: NkUnescape("a\\\"b") -> "a\"b"
             *       Gère les séquences : \\, \", \', \n, \t, \r, etc.
             */
            NKENTSEU_CORE_API NkString NkUnescape(NkStringView str, char escapeChar = '\\');

            /**
             * @brief Échappe une chaîne pour usage dans du code C/C++
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec échappements C-style
             * 
             * @note Échappe : ", ', \, \n, \r, \t, \0, etc.
             *       Résultat prêt à être inséré dans une chaîne littérale C.
             */
            NKENTSEU_CORE_API NkString NkCEscape(NkStringView str);

            /**
             * @brief Déséchappe une chaîne C-style
             * 
             * @param str Vue de chaîne échappée C-style à parser
             * @return Nouvelle instance NkString avec séquences résolues
             * 
             * @note Interprète : \\, \", \', \n, \r, \t, \xHH, \ooo, etc.
             */
            NKENTSEU_CORE_API NkString NkCUnescape(NkStringView str);

            /**
             * @brief Échappe une chaîne pour insertion sûre dans du HTML
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString avec entités HTML
             * 
             * @note Remplace : & -> &amp;, < -> &lt;, > -> &gt;, " -> &quot;, ' -> &#39;
             *       Prévention XSS basique pour affichage web.
             */
            NKENTSEU_CORE_API NkString NkHTMLEscape(NkStringView str);

            /**
             * @brief Déséchappe les entités HTML
             * 
             * @param str Vue de chaîne avec entités HTML à décoder
             * @return Nouvelle instance NkString avec entités résolues
             * 
             * @note Reconnaît les entités nommées (&amp;, &lt;, etc.) et numériques (&#39;).
             */
            NKENTSEU_CORE_API NkString NkHTMLUnescape(NkStringView str);

            /**
             * @brief Encode une chaîne pour inclusion dans une URL (percent-encoding)
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString encodée pour URL
             * 
             * @note Remplace les caractères non-URL-safe par %HH (hexadécimal).
             *       Conserve : A-Z, a-z, 0-9, -, _, ., ~
             *       Ex: "a b" -> "a%20b"
             */
            NKENTSEU_CORE_API NkString NkURLEncode(NkStringView str);

            /**
             * @brief Décode une chaîne URL-encodée (percent-decoding)
             * 
             * @param str Vue de chaîne encodée à décoder
             * @return Nouvelle instance NkString décodée
             * 
             * @note Interprète les séquences %HH et les remplace par le caractère correspondant.
             *       Gère également les '+' comme espaces (convention application/x-www-form-urlencoded).
             */
            NKENTSEU_CORE_API NkString NkURLDecode(NkStringView str);

            // =====================================================================
            // SECTION : HASHING DE CHAÎNES
            // =====================================================================

            /**
             * @brief Calcule un hash FNV-1a 64 bits d'une chaîne
             * 
             * @param str Vue de chaîne à hasher
             * @return Valeur de hash uint64 selon l'algorithme FNV-1a
             * 
             * @note Bon compromis distribution/performance pour tables de hachage.
             *       Non-cryptographique : ne pas utiliser pour la sécurité.
             */
            NKENTSEU_CORE_API uint64 NkHashFNV1a(NkStringView str);

            /**
             * @brief Calcule un hash FNV-1a case-insensitive
             * 
             * @param str Vue de chaîne à hasher
             * @return Valeur de hash uint64 (ASCII lowercase avant hash)
             * 
             * @note Utile pour les dictionnaires case-insensitive.
             */
            NKENTSEU_CORE_API uint64 NkHashFNV1aIgnoreCase(NkStringView str);

            /**
             * @brief Calcule un hash DJB2 64 bits d'une chaîne
             * 
             * @param str Vue de chaîne à hasher
             * @return Valeur de hash uint64 selon l'algorithme DJB2
             * 
             * @note Algorithme simple et rapide : hash = hash * 33 + c
             *       Bonne distribution pour les petites chaînes.
             */
            NKENTSEU_CORE_API uint64 NkHashDJB2(NkStringView str);

            /**
             * @brief Calcule un hash SDBM 64 bits d'une chaîne
             * 
             * @param str Vue de chaîne à hasher
             * @return Valeur de hash uint64 selon l'algorithme SDBM
             * 
             * @note Algorithme : hash = c + (hash << 6) + (hash << 16) - hash
             *       Alternative à FNV-1a avec caractéristiques différentes.
             */
            NKENTSEU_CORE_API uint64 NkHashSDBM(NkStringView str);

            // =====================================================================
            // SECTION : UTILITAIRES D'ENCODAGE
            // =====================================================================

            /**
             * @brief Convertit UTF-8 en ASCII avec remplacement des caractères non-ASCII
             * 
             * @param str Vue de chaîne UTF-8 source
             * @param replacement Caractère à utiliser pour remplacer les caractères non-ASCII (défaut: '?')
             * @return Nouvelle instance NkString contenant uniquement des caractères ASCII
             * 
             * @note Perte d'information : les caractères multi-bytes UTF-8 sont remplacés.
             *       Utile pour : fallback d'affichage, normalisation basique.
             */
            NKENTSEU_CORE_API NkString NkToAscii(NkStringView str, char replacement = '?');

            /**
             * @brief Vérifie si une chaîne contient uniquement des caractères ASCII valides
             * 
             * @param str Vue de chaîne à tester
             * @return true si tous les bytes sont dans la plage 0-127
             */
            NKENTSEU_CORE_API bool NkIsValidAscii(NkStringView str);

            // =====================================================================
            // SECTION : UTILITAIRES DE CHEMINS DE FICHIERS
            // =====================================================================

            /**
             * @brief Extrait le nom de fichier depuis un chemin
             * 
             * @param path Vue de chemin à analyser
             * @return Vue sur la composante nom de fichier (après le dernier '/' ou '\')
             * 
             * @note Gère les séparateurs Unix (/) et Windows (\).
             *       Ex: NkGetFileName("/usr/bin/app.exe") -> "app.exe"
             */
            NKENTSEU_CORE_API NkStringView NkGetFileName(NkStringView path);

            /**
             * @brief Extrait le nom de fichier sans extension
             * 
             * @param path Vue de chemin à analyser
             * @return Vue sur le nom de fichier sans l'extension après le dernier '.'
             * 
             * @note Ex: NkGetFileNameWithoutExtension("file.tar.gz") -> "file.tar"
             *       Ne retire que la dernière extension.
             */
            NKENTSEU_CORE_API NkStringView NkGetFileNameWithoutExtension(NkStringView path);

            /**
             * @brief Extrait le répertoire parent depuis un chemin
             * 
             * @param path Vue de chemin à analyser
             * @return Vue sur la partie chemin excluant le nom de fichier final
             * 
             * @note Ex: NkGetDirectory("/usr/bin/app.exe") -> "/usr/bin"
             *       Retourne vue vide si path ne contient pas de séparateur.
             */
            NKENTSEU_CORE_API NkStringView NkGetDirectory(NkStringView path);

            /**
             * @brief Extrait l'extension de fichier (avec le point)
             * 
             * @param path Vue de chemin à analyser
             * @return Vue sur l'extension (incluant le '.'), ou vue vide si aucune
             * 
             * @note Ex: NkGetExtension("file.txt") -> ".txt"
             *       Recherche le dernier '.' après le dernier séparateur de chemin.
             */
            NKENTSEU_CORE_API NkStringView NkGetExtension(NkStringView path);

            /**
             * @brief Remplace l'extension d'un chemin par une nouvelle
             * 
             * @param path Vue de chemin source
             * @param newExtension Nouvelle extension à appliquer (avec ou sans '.')
             * @return Nouvelle instance NkString avec extension modifiée
             * 
             * @note Si newExtension ne commence pas par '.', il est ajouté automatiquement.
             *       Ex: NkChangeExtension("file.txt", "bak") -> "file.bak"
             */
            NKENTSEU_CORE_API NkString NkChangeExtension(
                NkStringView path, 
                NkStringView newExtension);

            /**
             * @brief Combine deux composantes de chemin avec le séparateur approprié
             * 
             * @param path1 Premier élément de chemin
             * @param path2 Deuxième élément de chemin à ajouter
             * @return Nouvelle instance NkString avec chemin combiné
             * 
             * @note Ajoute automatiquement un séparateur si nécessaire.
             *       Gère les cas : path1 vide, path2 absolu, séparateurs multiples.
             */
            NKENTSEU_CORE_API NkString NkCombinePaths(NkStringView path1, NkStringView path2);

            /**
             * @brief Normalise les séparateurs de chemin vers un caractère cible
             * 
             * @param path Vue de chemin à normaliser
             * @param separator Caractère séparateur cible (défaut: '/')
             * @return Nouvelle instance NkString avec séparateurs uniformisés
             * 
             * @note Remplace tous les '\' par separator.
             *       Utile pour : portabilité Unix/Windows, génération de chemins cohérents.
             */
            NKENTSEU_CORE_API NkString NkNormalizePath(NkStringView path, char separator = '/');

            /**
             * @brief Vérifie si un chemin est absolu
             * 
             * @param path Vue de chemin à tester
             * @return true si le chemin commence par un indicateur d'absolu
             * 
             * @note Reconnaît :
             *       - Unix : commence par '/'
             *       - Windows : commence par "X:/" ou "X:\\" ou "\\\\" (UNC)
             */
            NKENTSEU_CORE_API bool NkIsAbsolutePath(NkStringView path);

            // =====================================================================
            // SECTION : REMPLACEMENT DE TEMPLATES {{key}}
            // =====================================================================

            /**
             * @brief Remplace les placeholders {{key}} par des valeurs depuis une map
             * 
             * @tparam Map Type de conteneur associatif supportant l'itération (key->value)
             * @param str Vue de chaîne template contenant les placeholders
             * @param replacements Map des remplacements (clé -> valeur)
             * @return Nouvelle instance NkString avec tous les placeholders résolus
             * 
             * @note Syntaxe : {{nom_clé}} est remplacé par la valeur correspondante.
             *       Les clés non-trouvées restent inchangées dans le résultat.
             *       Les remplacements sont appliqués séquentiellement : ordre non-garanti.
             * 
             * @par Exemple :
             * @code
             *   std::unordered_map<NkString, NkString> vars = {
             *       {"name", "Alice"}, {"age", "30"}
             *   };
             *   auto result = NkReplaceTemplates("Hello {{name}}, age {{age}}", vars);
             *   // "Hello Alice, age 30"
             * @endcode
             */
            template<typename Map>
            NkString NkReplaceTemplates(NkStringView str, const Map& replacements) {
                NkString result(str);
                for (const auto& pair : replacements) {
                    NkString templateStr = NkFormatf("{{%s}}", NkStringView(pair.first).Data());
                    result = NkReplaceAll(
                        result.View(), 
                        templateStr.View(), 
                        NkStringView(pair.second));
                }
                return result;
            }

            // =====================================================================
            // SECTION : TRAITEMENT DE LIGNES ET COMMENTAIRES
            // =====================================================================

            /**
             * @brief Supprime les commentaires d'une chaîne source
             * 
             * @param str Vue de chaîne source potentiellement commentée
             * @param commentMarkers Vue des marqueurs de début de commentaire (défaut: "//#")
             * @return Nouvelle instance NkString avec commentaires retirés
             * 
             * @note Supprime tout ce qui suit un marqueur de commentaire jusqu'à la fin de ligne.
             *       Gère les marqueurs multiples : si l'un est trouvé, la ligne est tronquée.
             *       Ex: NkRemoveComments("code // comment", "//#") -> "code "
             */
            NKENTSEU_CORE_API NkString NkRemoveComments(
                NkStringView str, 
                NkStringView commentMarkers = "//#");

            /**
             * @brief Normalise les fins de ligne vers un format cible
             * 
             * @param str Vue de chaîne source avec fins de ligne potentiellement mixtes
             * @param newline Séquence de fin de ligne cible (défaut: "\n")
             * @return Nouvelle instance NkString avec fins de ligne uniformisées
             * 
             * @note Reconnaît et convertit : \r\n (Windows), \n (Unix), \r (Mac classique).
             *       Utile pour : portabilité entre plateformes, génération de fichiers cohérents.
             */
            NKENTSEU_CORE_API NkString NkNormalizeLineEndings(
                NkStringView str, 
                NkStringView newline = "\n");

            /**
             * @brief Compte le nombre de lignes dans une chaîne
             * 
             * @param str Vue de chaîne à analyser
             * @return Nombre de lignes (défini par le nombre de sauts de ligne + 1 si non vide)
             * 
             * @note Une chaîne vide contient 0 ligne.
             *       "a\nb\nc" -> 3 lignes, "a" -> 1 ligne, "" -> 0 ligne.
             */
            NKENTSEU_CORE_API usize NkCountLines(NkStringView str);

            /**
             * @brief Extrait une ligne spécifique par numéro (0-indexé)
             * 
             * @param str Vue de chaîne multi-lignes à parcourir
             * @param lineNumber Index de la ligne à extraire (0 = première ligne)
             * @return Vue sur la ligne demandée, ou vue vide si lineNumber hors limites
             * 
             * @note Les sauts de ligne ne sont pas inclus dans le résultat.
             *       Parcours séquentiel : complexité O(n) pour accéder à la ligne N.
             */
            NKENTSEU_CORE_API NkStringView NkGetLine(NkStringView str, usize lineNumber);

            // =====================================================================
            // SECTION : MANIPULATIONS DIVERSES DE CHAÎNES
            // =====================================================================

            /**
             * @brief Remplit une chaîne mutable avec un caractère répété
             * 
             * @param str Référence vers la chaîne à remplir
             * @param ch Caractère à répéter
             * @param count Nombre de répétitions
             * 
             * @note Redimensionne str à count caractères puis remplit avec ch.
             *       Équivalent à : str.Resize(count); std::fill(str.begin(), str.end(), ch);
             */
            NKENTSEU_CORE_API void NkFill(NkString& str, char ch, usize count);

            /**
             * @brief Crée une nouvelle chaîne remplie d'un caractère répété
             * 
             * @param ch Caractère à répéter
             * @param count Nombre de répétitions
             * @return Nouvelle instance NkString contenant ch répété count fois
             * 
             * @note Alias pratique pour NkRepeat(ch, count) : même fonctionnalité.
             */
            NKENTSEU_CORE_API NkString NkFillCopy(char ch, usize count);

            /**
             * @brief Nettoie une chaîne : trim + suppression espaces multiples
             * 
             * @param str Vue de chaîne source
             * @return Nouvelle instance NkString nettoyée
             * 
             * @note Combine : NkTrim() + NkRemoveExtraSpaces()
             *       Ex: "  a   b  " -> "a b"
             */
            NKENTSEU_CORE_API NkString NkClean(NkStringView str);

            /**
             * @brief Nettoie une chaîne in-place
             * 
             * @param str Référence vers la chaîne à nettoyer
             * 
             * @note Applique TrimInPlace + RemoveExtraSpacesInPlace séquentiellement.
             */
            NKENTSEU_CORE_API void NkCleanInPlace(NkString& str);

            /**
             * @brief Remplit une chaîne avec un caractère pour atteindre une longueur cible
             * 
             * @param str Vue de chaîne source
             * @param fillChar Caractère de remplissage
             * @param totalLength Longueur finale souhaitée
             * @param left true pour padding à gauche, false pour padding à droite
             * @return Nouvelle instance NkString avec remplissage appliqué
             * 
             * @note Alias sémantique pour NkPadLeft/NkPadRight selon le paramètre left.
             */
            NKENTSEU_CORE_API NkString NkFillMissing(
                NkStringView str, 
                char fillChar, 
                usize totalLength, 
                bool left = true);

            // =====================================================================
            // SECTION : MANIPULATIONS DE CARACTÈRES UNIQUES
            // =====================================================================

            /**
             * @brief Remplace toutes les occurrences d'un caractère par un autre
             * 
             * @param str Vue de chaîne source
             * @param from Caractère à remplacer
             * @param to Caractère de remplacement
             * @return Nouvelle instance NkString avec remplacements effectués
             * 
             * @note Parcours simple : O(n). Plus efficace que NkReplace pour single-char.
             */
            NKENTSEU_CORE_API NkString NkReplaceChar(
                NkStringView str, 
                char from, 
                char to);

            /**
             * @brief Remplace un caractère par un autre in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Caractère à remplacer
             * @param to Caractère de remplacement
             * 
             * @note Modification directe : aucune allocation supplémentaire.
             */
            NKENTSEU_CORE_API void NkReplaceCharInPlace(NkString& str, char from, char to);

            /**
             * @brief Alias pour NkReplaceChar (nom alternatif)
             * 
             * @param str Vue de chaîne source
             * @param from Caractère à remplacer
             * @param to Caractère de remplacement
             * @return Nouvelle instance NkString avec remplacements
             */
            NKENTSEU_CORE_API NkString NkReplaceAllChars(
                NkStringView str, 
                char from, 
                char to);

            /**
             * @brief Alias pour NkReplaceCharInPlace (nom alternatif)
             * 
             * @param str Référence vers la chaîne à modifier
             * @param from Caractère à remplacer
             * @param to Caractère de remplacement
             */
            NKENTSEU_CORE_API void NkReplaceAllCharsInPlace(NkString& str, char from, char to);

            /**
             * @brief Supprime le caractère à une position donnée
             * 
             * @param str Vue de chaîne source
             * @param position Index du caractère à supprimer
             * @return Nouvelle instance NkString sans le caractère à position
             * 
             * @note Équivalent à NkErase(str, position, 1)
             */
            NKENTSEU_CORE_API NkString NkRemoveAt(NkStringView str, usize position);

            /**
             * @brief Supprime le caractère à une position in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param position Index du caractère à supprimer
             * 
             * @note Décale les caractères suivants vers la gauche : O(n) worst-case.
             */
            NKENTSEU_CORE_API void NkRemoveAtInPlace(NkString& str, usize position);

            /**
             * @brief Insère un caractère unique à une position donnée
             * 
             * @param str Vue de chaîne source
             * @param position Index où insérer le nouveau caractère
             * @param ch Caractère à insérer
             * @return Nouvelle instance NkString avec insertion effectuée
             */
            NKENTSEU_CORE_API NkString NkInsertChar(
                NkStringView str, 
                usize position, 
                char ch);

            /**
             * @brief Insère un caractère unique in-place
             * 
             * @param str Référence vers la chaîne à modifier
             * @param position Index d'insertion
             * @param ch Caractère à insérer
             * 
             * @note Peut déclencher réallocation si capacité insuffisante.
             */
            NKENTSEU_CORE_API void NkInsertCharInPlace(
                NkString& str, 
                usize position, 
                char ch);

            // =====================================================================
            // SECTION : VALIDATION DE FORMATS SPÉCIFIQUES
            // =====================================================================

            /**
             * @brief Vérifie si une chaîne correspond à un pattern avec wildcards
             * 
             * @param str Vue de chaîne à tester
             * @param pattern Pattern avec '*' (multi) et '?' (single)
             * @return true si str match le pattern
             * 
             * @note '*' match zéro ou plusieurs caractères quelconques.
             *       '?' match exactement un caractère quelconque.
             *       Ex: NkMatchesPattern("file123.txt", "file*.txt") -> true
             * 
             * @warning Implémentation backtracking : éviter patterns trop complexes
             *          ou chaînes très longues pour des raisons de performance.
             */
            NKENTSEU_CORE_API bool NkMatchesPattern(NkStringView str, NkStringView pattern);

            /**
             * @brief Vérifie si une chaîne a un format d'email valide (basique)
             * 
             * @param str Vue de chaîne à tester
             * @return true si str ressemble à un email selon règles simplifiées
             * 
             * @note Vérifications basiques :
             *       - Contient exactement un '@'
             *       - Partie locale et domaine non-vides
             *       - Domaine contient au moins un '.'
             * 
             * @warning Validation simplifiée : ne remplace pas une vérification RFC 5322 complète.
             *          Suffisant pour filtrage basique, pas pour validation stricte.
             */
            NKENTSEU_CORE_API bool NkIsEmail(NkStringView str);

            /**
             * @brief Vérifie si une chaîne a un format d'URL valide (basique)
             * 
             * @param str Vue de chaîne à tester
             * @return true si str ressemble à une URL selon règles simplifiées
             * 
             * @note Vérifications basiques :
             *       - Commence par un schéma reconnu (http://, https://, ftp://, etc.)
             *       - Contient un hôte non-vide après le schéma
             * 
             * @warning Validation simplifiée : ne parse pas tous les cas RFC 3986.
             *          Utile pour filtrage, pas pour parsing complet d'URL.
             */
            NKENTSEU_CORE_API bool NkIsURL(NkStringView str);

            /**
             * @brief Vérifie si une chaîne est un identifiant valide (style C/C++)
             * 
             * @param str Vue de chaîne à tester
             * @return true si str est un identifiant syntaxiquement valide
             * 
             * @note Règles :
             *       - Premier caractère : lettre ou underscore
             *       - Caractères suivants : lettres, chiffres, underscores
             *       - Non vide
             * 
             * @note Ex: NkIsIdentifier("_var1") -> true
             *       NkIsIdentifier("123abc") -> false (commence par chiffre)
             */
            NKENTSEU_CORE_API bool NkIsIdentifier(NkStringView str);

            // =====================================================================
            // SECTION : UTILITAIRES AVANCÉS
            // =====================================================================

            /**
             * @brief Génère une chaîne aléatoire de longueur donnée
             * 
             * @param length Nombre de caractères à générer
             * @param charset Vue des caractères autorisés dans le résultat (défaut: alphanumérique)
             * @return Nouvelle instance NkString contenant des caractères aléatoires
             * 
             * @note Utilise le générateur aléatoire par défaut du projet.
             *       Pour un usage cryptographique, utiliser une source d'entropie dédiée.
             * 
             * @par Exemple :
             * @code
             *   auto token = NkRandomString(16);  // "aB3xK9mL2pQ7rT5w"
             *   auto hex = NkRandomString(8, "0123456789abcdef");  // "3f9a2c1e"
             * @endcode
             */
            NKENTSEU_CORE_API NkString NkRandomString(
                usize length, 
                NkStringView charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

            /**
             * @brief Génère un UUID version 4 (aléatoire) au format standard
             * 
             * @return Nouvelle instance NkString contenant un UUID formaté
             * 
             * @note Format : 8-4-4-4-12 caractères hexadécimaux séparés par des tirets.
             *       Ex: "550e8400-e29b-41d4-a716-446655440000"
             * 
             * @warning Utilise le générateur aléatoire par défaut : pas cryptographiquement sûr.
             *          Pour des UUID sécurisés, utiliser une source d'entropie dédiée.
             */
            NKENTSEU_CORE_API NkString NkGenerateUUID();

            /**
             * @brief Applique une obfuscation simple réversible à une chaîne
             * 
             * @param str Vue de chaîne à obfusquer
             * @param seed Graine optionnelle pour la transformation (défaut: 0)
             * @return Nouvelle instance NkString obfusquée
             * 
             * @note Algorithme simple XOR + rotation : suffisant pour masquage basique,
             *       pas pour chiffrement sécurisé. Réversible via NkDeobfuscate avec même seed.
             * 
             * @warning NE PAS UTILISER pour protéger des données sensibles.
             *          Pour du chiffrement réel, utiliser une bibliothèque cryptographique dédiée.
             */
            NKENTSEU_CORE_API NkString NkObfuscate(NkStringView str, uint32 seed = 0);

            /**
             * @brief Désobfusque une chaîne précédemment obfusquée
             * 
             * @param str Vue de chaîne obfusquée à restaurer
             * @param seed Graine utilisée lors de l'obfuscation (doit correspondre)
             * @return Nouvelle instance NkString avec contenu original restauré
             * 
             * @note Opération inverse de NkObfuscate : même seed requis pour récupération correcte.
             */
            NKENTSEU_CORE_API NkString NkDeobfuscate(NkStringView str, uint32 seed = 0);

            /**
             * @brief Calcule la distance de Levenshtein entre deux chaînes
             * 
             * @param str1 Première chaîne à comparer
             * @param str2 Deuxième chaîne à comparer
             * @return Nombre minimal d'éditions (insertion/suppression/substitution) pour transformer str1 en str2
             * 
             * @note Complexité O(n*m) : utiliser avec précaution sur longues chaînes.
             *       Utile pour : fuzzy matching, correction orthographique, similarité textuelle.
             * 
             * @par Exemple :
             * @code
             *   NkLevenshteinDistance("kitten", "sitting") -> 3
             *   // kitten -> sitten (sub k->s)
             *   // sitten -> sittin (sub e->i)
             *   // sittin -> sitting (insert g)
             * @endcode
             */
            NKENTSEU_CORE_API usize NkLevenshteinDistance(NkStringView str1, NkStringView str2);

            /**
             * @brief Calcule un score de similarité normalisé [0.0 - 1.0] entre deux chaînes
             * 
             * @param str1 Première chaîne à comparer
             * @param str2 Deuxième chaîne à comparer
             * @return Score de similarité : 1.0 = identique, 0.0 = totalement différent
             * 
             * @note Basé sur la distance de Levenshtein normalisée par max(len1, len2).
             *       Formule : 1.0 - (distance / max_length)
             */
            NKENTSEU_CORE_API float64 NkSimilarity(NkStringView str1, NkStringView str2);

            // =====================================================================
            // SECTION : CRÉATION ET MANIPULATION DE STRINGVIEW
            // =====================================================================

            /**
             * @brief Crée une vue de chaîne à partir de données brutes + longueur
             * 
             * @param data Pointeur vers les données caractères
             * @param length Nombre de caractères dans la vue
             * @return Nouvelle instance NkStringView sur les données spécifiées
             * 
             * @note Aucune copie : la vue référence directement data.
             *       Garantir que data reste valide pendant l'utilisation de la vue.
             */
            NKENTSEU_CORE_API NkStringView NkMakeView(const char* data, usize length);

            /**
             * @brief Crée une vue de chaîne à partir d'une C-string terminée par null
             * 
             * @param cstr Pointeur vers chaîne C-style
             * @return Nouvelle instance NkStringView sur cstr
             * 
             * @note Calcule automatiquement la longueur via parcours jusqu'au null-terminator.
             *       Si cstr == nullptr : retourne vue vide (nullptr, 0).
             */
            NKENTSEU_CORE_API NkStringView NkMakeView(const char* cstr);

            // =====================================================================
            // SECTION : OPÉRATIONS MÉMOIRE SÉCURISÉES
            // =====================================================================

            /**
             * @brief Copie une vue de chaîne dans un buffer avec vérification de bornes
             * 
             * @param dest Pointeur vers le buffer de destination
             * @param destSize Taille disponible du buffer destination (en bytes)
             * @param src Vue de chaîne source à copier
             * @return Nombre de bytes effectivement copiés (exclut le null-terminator ajouté)
             * 
             * @note Garantit :
             *       - Aucun dépassement de buffer : copie au plus destSize-1 bytes
             *       - Null-termination systématique : dest[copied] = '\\0' si destSize > 0
             *       - Retourne 0 si destSize == 0 ou src.IsNull()
             * 
             * @warning dest DOIT pointer vers un buffer d'au moins destSize bytes.
             *          La fonction ne vérifie pas la validité de dest, seulement les bornes.
             */
            NKENTSEU_CORE_API usize NkSafeCopy(char* dest, usize destSize, NkStringView src);

            /**
             * @brief Concatène une vue de chaîne à un buffer existant avec vérification de bornes
             * 
             * @param dest Pointeur vers le buffer de destination (doit être null-terminated)
             * @param destSize Taille totale disponible du buffer destination (en bytes)
             * @param src Vue de chaîne source à ajouter à la fin
             * @return Nouvelle longueur de la chaîne dans dest (exclut le null-terminator)
             * 
             * @note Garantit :
             *       - Aucun dépassement de buffer : concatène au plus l'espace restant
             *       - Null-termination préservée après l'opération
             *       - Retourne la longueur précédente si aucun espace disponible
             * 
             * @warning dest DOIT être un buffer valide, null-terminated, d'au moins destSize bytes.
             *          Comportement indéfini si dest n'est pas correctement initialisé.
             * 
             * @par Exemple :
             * @code
             *   char buffer[64] = "Hello";
             *   NkSafeConcat(buffer, sizeof(buffer), " World");
             *   // buffer contient maintenant "Hello World"
             * @endcode
             */
            NKENTSEU_CORE_API usize NkSafeConcat(char* dest, usize destSize, NkStringView src);

        } // namespace string

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGUTILS_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Parsing de configuration avec split et trim
    // =====================================================================
    {
        bool ParseConfigLine(nkentseu::NkStringView line, ConfigEntry& out) {
            // Ignorer commentaires et lignes vides
            auto cleaned = nkentseu::string::NkTrim(line);
            if (cleaned.Empty() || cleaned.StartsWith('#')) {
                return false;
            }
            
            // Parser clé=valeur
            auto eqPos = cleaned.Find('=');
            if (eqPos == nkentseu::NkStringView::npos) {
                return false;
            }
            
            out.key = nkentseu::string::NkTrimCopy(cleaned.Left(eqPos));
            out.value = nkentseu::string::NkTrimCopy(cleaned.SubStr(eqPos + 1));
            
            return !out.key.Empty() && !out.value.Empty();
        }
    }

    // =====================================================================
    // Exemple 2 : Génération de logs structurés avec formatage
    // =====================================================================
    {
        nkentseu::NkString FormatLogEntry(
            const char* level,
            uint64_t timestamp,
            nkentseu::NkStringView message) {
            
            return nkentseu::string::NkFormat(
                "[{0}] @{1:hex16} | {2}",
                nkentseu::string::NkToUpper(level),
                timestamp,
                nkentseu::string::NkTrimCopy(message)
            );
        }
        
        // Usage :
        // FormatLogEntry("error", 0x1234567890ABCDEF, "  Connection failed  ")
        // -> "[ERROR] @00001234567890abcdef | Connection failed"
    }

    // =====================================================================
    // Exemple 3 : Validation d'entrée utilisateur avec prédicats
    // =====================================================================
    {
        bool IsValidUsername(nkentseu::NkStringView username) {
            // Règles : 3-20 chars, alphanumérique + underscore uniquement
            return username.Length() >= 3 && 
                   username.Length() <= 20 && 
                   nkentseu::string::NkIsAllAlphaNumeric(username) || 
                   nkentseu::string::NkContainsOnly(username, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
        }
        
        bool IsValidPort(nkentseu::NkStringView portStr) {
            uint32 port = 0;
            return nkentseu::string::NkParseUInt(portStr, port) && 
                   port >= 1 && port <= 65535;
        }
    }

    // =====================================================================
    // Exemple 4 : Manipulation de chemins de fichiers
    // =====================================================================
    {
        nkentseu::NkString BuildOutputPath(
            nkentseu::NkStringView sourceFile,
            nkentseu::NkStringView outputDir,
            nkentseu::NkStringView newExtension) {
            
            // Extraire le nom de base sans extension
            auto baseName = nkentseu::string::NkGetFileNameWithoutExtension(sourceFile);
            
            // Combiner avec le répertoire de sortie et la nouvelle extension
            auto path = nkentseu::string::NkCombinePaths(outputDir, baseName);
            return nkentseu::string::NkChangeExtension(path, newExtension);
        }
        
        // Usage :
        // BuildOutputPath("src/main.cpp", "build", ".o")
        // -> "build/main.o"
    }

    // =====================================================================
    // Exemple 5 : Traitement de texte avec transformations chaînées
    // =====================================================================
    {
        nkentseu::NkString NormalizeUserInput(nkentseu::NkStringView raw) {
            return nkentseu::string::NkClean(raw)     // trim + espaces multiples
                              .View()
                              .ToLower()               // minuscules
                              .ToString();             // copie possédante pour retour
        }
        
        // Usage :
        // NormalizeUserInput("  Hello   WORLD  ") -> "hello world"
    }

    // =====================================================================
    // Exemple 6 : Recherche et remplacement avancé avec callback
    // =====================================================================
    {
        // Mettre en évidence les mots-clés dans un texte
        nkentseu::NkString HighlightKeywords(
            nkentseu::NkStringView text,
            const std::vector<nkentseu::NkStringView>& keywords) {
            
            nkentseu::NkString result(text);
            
            for (const auto& kw : keywords) {
                result = nkentseu::string::NkReplaceAll(
                    result.View(),
                    kw,
                    nkentseu::string::NkFormat("**{0}**", kw)  // Markdown-style highlight
                );
            }
            
            return result;
        }
        
        // Usage :
        // HighlightKeywords("Hello world", {"world"}) -> "Hello **world**"
    }

    // =====================================================================
    // Exemple 7 : Hash pour tables de hachage case-insensitive
    // =====================================================================
    {
        struct CaseInsensitiveHash {
            size_t operator()(const nkentseu::NkString& str) const noexcept {
                return static_cast<size_t>(
                    nkentseu::string::NkHashFNV1aIgnoreCase(str.View()));
            }
        };
        
        struct CaseInsensitiveEqual {
            bool operator()(
                const nkentseu::NkString& a, 
                const nkentseu::NkString& b) const noexcept {
                return nkentseu::string::NkEqualsIgnoreCase(a.View(), b.View());
            }
        };
        
        // Utilisation dans une HashMap custom :
        // HashMap<NkString, Value, CaseInsensitiveHash, CaseInsensitiveEqual> dict;
        // dict.Insert("Key", value);
        // dict.Find("key");  // Trouve "Key" malgré la différence de casse
    }

    // =====================================================================
    // Exemple 8 : Bonnes pratiques et pièges à éviter
    // =====================================================================
    {
        // ✅ BON : Utiliser NkStringView pour paramètres, NkString pour retours possédants
        nkentseu::NkString ProcessText(nkentseu::NkStringView input) {
            if (input.StartsWith("CMD:")) {
                return input.SubStr(4).Trim().ToUpper().ToString();
            }
            return input.ToString();
        }
        
        // ❌ À ÉVITER : Retourner une vue sur donnée locale
        // nkentseu::NkStringView BadReturn() {
        //     nkentseu::NkString temp = "local";
        //     return temp.View();  // ⚠️ Vue invalide après retour !
        // }
        
        // ✅ BON : Pré-allouer avant boucle de concaténations
        nkentseu::NkString BuildCsv(const std::vector<const char*>& fields) {
            nkentseu::NkString result;
            
            // Estimation de taille pour éviter réallocations multiples
            usize estimated = 0;
            for (const char* f : fields) {
                estimated += f ? strlen(f) + 1 : 1;
            }
            result.Reserve(estimated);
            
            for (usize i = 0; i < fields.size(); ++i) {
                if (i > 0) result += ',';
                if (fields[i]) result += fields[i];
            }
            return result;
        }
    }
*/