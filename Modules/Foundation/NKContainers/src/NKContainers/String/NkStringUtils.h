// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringUtils.h
// DESCRIPTION: String utilities (Split, Join, Trim, Format...)
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGUTILS_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGUTILS_H_INCLUDED

#include "NkString.h"
#include "NkStringView.h"

namespace nkentseu {
    namespace core {
        namespace string {
            
            // ========================================
            // BASIC OPERATIONS
            // ========================================
            
            /**
             * @brief Retourne la longueur de la chaîne (alias pour Length())
             */
            NKENTSEU_CORE_API usize NkLeng(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est vide
             */
            NKENTSEU_CORE_API bool NkIsEmpty(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne n'est pas vide
             */
            NKENTSEU_CORE_API bool NkIsNotEmpty(NkStringView str);
            
            // ========================================
            // CASE CONVERSION
            // ========================================
            
            /**
             * @brief Convertit en minuscules
             */
            NKENTSEU_CORE_API NkString NkToLower(NkStringView str);
            NKENTSEU_CORE_API void NkToLowerInPlace(NkString& str);
            NKENTSEU_CORE_API char NkToLower(char ch);
            
            /**
             * @brief Convertit en majuscules
             */
            NKENTSEU_CORE_API NkString NkToUpper(NkStringView str);
            NKENTSEU_CORE_API void NkToUpperInPlace(NkString& str);
            NKENTSEU_CORE_API char NkToUpper(char ch);
            
            /**
             * @brief Alterne la casse
             */
            NKENTSEU_CORE_API NkString NkToggleCase(NkStringView str);
            NKENTSEU_CORE_API void NkToggleCaseInPlace(NkString& str);
            
            /**
             * @brief Inverse la casse
             */
            NKENTSEU_CORE_API NkString NkSwapCase(NkStringView str);
            NKENTSEU_CORE_API void NkSwapCaseInPlace(NkString& str);
            
            // ========================================
            // TRIM (whitespace)
            // ========================================
            
            NKENTSEU_CORE_API NkStringView NkTrimLeft(NkStringView str);
            NKENTSEU_CORE_API NkStringView NkTrimRight(NkStringView str);
            NKENTSEU_CORE_API NkStringView NkTrim(NkStringView str);
            
            NKENTSEU_CORE_API NkString NkTrimLeftCopy(NkStringView str);
            NKENTSEU_CORE_API NkString NkTrimRightCopy(NkStringView str);
            NKENTSEU_CORE_API NkString NkTrimCopy(NkStringView str);
            
            NKENTSEU_CORE_API void NkTrimLeftInPlace(NkString& str);
            NKENTSEU_CORE_API void NkTrimRightInPlace(NkString& str);
            NKENTSEU_CORE_API void NkTrimInPlace(NkString& str);
            
            /**
             * @brief Trim avec caractères personnalisés
             */
            NKENTSEU_CORE_API NkStringView NkTrimLeftChars(NkStringView str, NkStringView chars);
            NKENTSEU_CORE_API NkStringView NkTrimRightChars(NkStringView str, NkStringView chars);
            NKENTSEU_CORE_API NkStringView NkTrimChars(NkStringView str, NkStringView chars);
            
            // ========================================
            // SPLIT
            // ========================================
            
            /**
             * @brief Sépare chaîne par délimiteur (retourne un tableau)
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
             * @brief Split avec délimiteur multiple
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
             * @brief Split en excluant les chaînes vides
             */
            template<typename Container>
            void NkSplitNoEmpty(NkStringView str, NkStringView delimiter, Container& result) {
                usize start = 0;
                while (start < str.Length()) {
                    auto pos = str.Find(delimiter, start);
                    if (pos == NkStringView::npos) {
                        auto part = str.SubStr(start);
                        if (!part.IsEmpty()) {
                            result.push_back(part);
                        }
                        break;
                    }
                    auto part = str.SubStr(start, pos - start);
                    if (!part.IsEmpty()) {
                        result.push_back(part);
                    }
                    start = pos + delimiter.Length();
                }
            }
            
            /**
             * @brief Split simplifié (callback)
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
             * @brief Split en limitant le nombre de parties
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
             * @brief Split par lignes
             */
            template<typename Container>
            void NkSplitLines(NkStringView str, Container& result, bool keepLineBreaks = false) {
                usize start = 0;
                for (usize i = 0; i < str.Length(); ++i) {
                    if (str[i] == '\n' || (str[i] == '\r' && i + 1 < str.Length() && str[i + 1] == '\n')) {
                        if (keepLineBreaks) {
                            result.push_back(str.SubStr(start, i - start + (str[i] == '\r' ? 2 : 1)));
                        } else {
                            result.push_back(str.SubStr(start, i - start));
                        }
                        if (str[i] == '\r') ++i; // Sauter le \n
                        start = i + 1;
                    }
                }
                if (start < str.Length()) {
                    result.push_back(str.SubStr(start));
                }
            }
            
            // ========================================
            // JOIN
            // ========================================
            
            /**
             * @brief Joint plusieurs chaînes
             */
            NKENTSEU_CORE_API NkString NkJoin(const NkStringView* strings, usize count, NkStringView separator);
            
            template<usize N>
            NkString NkJoin(const NkStringView (&strings)[N], NkStringView separator) {
                return NkJoin(strings, N, separator);
            }
            
            /**
             * @brief Joint un conteneur de chaînes
             */
            template<typename Container>
            NkString NkJoin(const Container& strings, NkStringView separator) {
                if (strings.empty()) return NkString();
                
                // Calcule taille totale
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
             * @brief Joint avec callback pour transformer chaque élément
             */
            template<typename Container, typename Transformer>
            NkString NkJoinTransform(const Container& items, NkStringView separator, Transformer transformer) {
                if (items.empty()) return NkString();
                
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
            
            // ========================================
            // REPLACE
            // ========================================
            
            NKENTSEU_CORE_API NkString NkReplace(NkStringView str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API NkString NkReplaceAll(NkStringView str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API void NkReplaceInPlace(NkString& str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API void NkReplaceAllInPlace(NkString& str, NkStringView from, NkStringView to);
            
            /**
             * @brief Remplace le premier/dernier uniquement
             */
            NKENTSEU_CORE_API NkString NkReplaceFirst(NkStringView str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API NkString NkReplaceLast(NkStringView str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API void NkReplaceFirstInPlace(NkString& str, NkStringView from, NkStringView to);
            NKENTSEU_CORE_API void NkReplaceLastInPlace(NkString& str, NkStringView from, NkStringView to);
            
            /**
             * @brief Remplace par callback
             */
            template<typename Callback>
            NkString NkReplaceCallback(NkStringView str, NkStringView pattern, Callback callback) {
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
            
            // ========================================
            // SEARCH & FIND
            // ========================================
            
            /**
             * @brief Vérifie si la chaîne commence par un préfixe
             */
            NKENTSEU_CORE_API bool NkStartsWith(NkStringView str, NkStringView prefix);
            NKENTSEU_CORE_API bool NkStartsWithIgnoreCase(NkStringView str, NkStringView prefix);
            
            /**
             * @brief Vérifie si la chaîne se termine par un suffixe
             */
            NKENTSEU_CORE_API bool NkEndsWith(NkStringView str, NkStringView suffix);
            NKENTSEU_CORE_API bool NkEndsWithIgnoreCase(NkStringView str, NkStringView suffix);
            
            /**
             * @brief Vérifie si la chaîne contient une sous-chaîne
             */
            NKENTSEU_CORE_API bool NkContains(NkStringView str, NkStringView substring);
            NKENTSEU_CORE_API bool NkContainsIgnoreCase(NkStringView str, NkStringView substring);
            NKENTSEU_CORE_API bool NkContainsAny(NkStringView str, NkStringView characters);
            NKENTSEU_CORE_API bool NkContainsNone(NkStringView str, NkStringView characters);
            NKENTSEU_CORE_API bool NkContainsOnly(NkStringView str, NkStringView characters);
            
            /**
             * @brief Trouve la première/dernière occurrence d'un caractère
             */
            NKENTSEU_CORE_API usize NkFindFirstOf(NkStringView str, NkStringView characters, usize start = 0);
            NKENTSEU_CORE_API usize NkFindLastOf(NkStringView str, NkStringView characters);
            NKENTSEU_CORE_API usize NkFindFirstNotOf(NkStringView str, NkStringView characters, usize start = 0);
            NKENTSEU_CORE_API usize NkFindLastNotOf(NkStringView str, NkStringView characters);
            
            /**
             * @brief Compte les occurrences
             */
            NKENTSEU_CORE_API usize NkCount(NkStringView str, NkStringView substring);
            NKENTSEU_CORE_API usize NkCount(NkStringView str, char character);
            
            /**
             * @brief Recherche insensible à la casse
             */
            NKENTSEU_CORE_API usize NkFindIgnoreCase(NkStringView str, NkStringView substring, usize start = 0);
            NKENTSEU_CORE_API usize NkFindLastIgnoreCase(NkStringView str, NkStringView substring);
            
            // ========================================
            // SUBSTRING & EXTRACTION
            // ========================================
            
            /**
             * @brief Extrait une sous-chaîne entre délimiteurs
             */
            NKENTSEU_CORE_API NkStringView NkSubstringBetween(NkStringView str, NkStringView startDelim, NkStringView endDelim);
            NKENTSEU_CORE_API NkString NkSubstringBetweenCopy(NkStringView str, NkStringView startDelim, NkStringView endDelim);
            
            /**
             * @brief Extrait avant/après un délimiteur
             */
            NKENTSEU_CORE_API NkStringView NkSubstringBefore(NkStringView str, NkStringView delimiter);
            NKENTSEU_CORE_API NkStringView NkSubstringAfter(NkStringView str, NkStringView delimiter);
            NKENTSEU_CORE_API NkStringView NkSubstringBeforeLast(NkStringView str, NkStringView delimiter);
            NKENTSEU_CORE_API NkStringView NkSubstringAfterLast(NkStringView str, NkStringView delimiter);
            
            /**
             * @brief Extrait le premier/dernier caractère
             */
            NKENTSEU_CORE_API char NkFirstChar(NkStringView str);
            NKENTSEU_CORE_API char NkLastChar(NkStringView str);
            
            /**
             * @brief Extrait les N premiers/derniers caractères
             */
            NKENTSEU_CORE_API NkStringView NkFirstChars(NkStringView str, usize count);
            NKENTSEU_CORE_API NkStringView NkLastChars(NkStringView str, usize count);
            
            /**
             * @brief Extrait du milieu
             */
            NKENTSEU_CORE_API NkStringView NkMid(NkStringView str, usize start, usize count = NkStringView::npos);
            
            // ========================================
            // TRANSFORMATION
            // ========================================
            
            /**
             * @brief Capitalise la première lettre
             */
            NKENTSEU_CORE_API NkString NkCapitalize(NkStringView str);
            NKENTSEU_CORE_API void NkCapitalizeInPlace(NkString& str);
            
            /**
             * @brief Capitalise chaque mot
             */
            NKENTSEU_CORE_API NkString NkTitleCase(NkStringView str);
            NKENTSEU_CORE_API void NkTitleCaseInPlace(NkString& str);
            
            /**
             * @brief Inverse la chaîne
             */
            NKENTSEU_CORE_API NkString NkReverse(NkStringView str);
            NKENTSEU_CORE_API void NkReverseInPlace(NkString& str);
            
            /**
             * @brief Supprime tous les caractères spécifiés
             */
            NKENTSEU_CORE_API NkString NkRemoveChars(NkStringView str, NkStringView charsToRemove);
            NKENTSEU_CORE_API void NkRemoveCharsInPlace(NkString& str, NkStringView charsToRemove);
            
            /**
             * @brief Supprime les doublons de caractères
             */
            NKENTSEU_CORE_API NkString NkRemoveDuplicates(NkStringView str, char duplicateChar);
            NKENTSEU_CORE_API void NkRemoveDuplicatesInPlace(NkString& str, char duplicateChar);
            
            /**
             * @brief Supprime les espaces multiples
             */
            NKENTSEU_CORE_API NkString NkRemoveExtraSpaces(NkStringView str);
            NKENTSEU_CORE_API void NkRemoveExtraSpacesInPlace(NkString& str);
            
            /**
             * @brief Insère une chaîne à une position donnée
             */
            NKENTSEU_CORE_API NkString NkInsert(NkStringView str, usize position, NkStringView insertStr);
            NKENTSEU_CORE_API void NkInsertInPlace(NkString& str, usize position, NkStringView insertStr);
            
            /**
             * @brief Supprime une sous-chaîne
             */
            NKENTSEU_CORE_API NkString NkErase(NkStringView str, usize position, usize count);
            NKENTSEU_CORE_API void NkEraseInPlace(NkString& str, usize position, usize count);
            
            // ========================================
            // NUMERIC CONVERSION
            // ========================================
            
            NKENTSEU_CORE_API bool NkParseInt(NkStringView str, int32& out);
            NKENTSEU_CORE_API bool NkParseInt64(NkStringView str, int64& out);
            NKENTSEU_CORE_API bool NkParseUInt(NkStringView str, uint32& out);
            NKENTSEU_CORE_API bool NkParseUInt64(NkStringView str, uint64& out);
            NKENTSEU_CORE_API bool NkParseFloat(NkStringView str, float32& out);
            NKENTSEU_CORE_API bool NkParseDouble(NkStringView str, float64& out);
            NKENTSEU_CORE_API bool NkParseBool(NkStringView str, bool& out);
            
            /**
             * @brief Conversion vers String
             */
            NKENTSEU_CORE_API NkString NkToString(int32 value);
            NKENTSEU_CORE_API NkString NkToString(int64 value);
            NKENTSEU_CORE_API NkString NkToString(uint32 value);
            NKENTSEU_CORE_API NkString NkToString(uint64 value);
            NKENTSEU_CORE_API NkString NkToString(float32 value, int precision = 6);
            NKENTSEU_CORE_API NkString NkToString(float64 value, int precision = 6);
            NKENTSEU_CORE_API NkString NkToString(bool value);
            
            /**
             * @brief Conversion hexadécimale
             */
            NKENTSEU_CORE_API NkString NkToHex(int32 value, bool prefix = false);
            NKENTSEU_CORE_API NkString NkToHex(int64 value, bool prefix = false);
            NKENTSEU_CORE_API NkString NkToHex(uint32 value, bool prefix = false);
            NKENTSEU_CORE_API NkString NkToHex(uint64 value, bool prefix = false);
            
            NKENTSEU_CORE_API bool NkParseHex(NkStringView str, uint32& out);
            NKENTSEU_CORE_API bool NkParseHex(NkStringView str, uint64& out);
            
            // ========================================
            // COMPARISON (case-insensitive)
            // ========================================
            
            NKENTSEU_CORE_API int NkCompareIgnoreCase(NkStringView lhs, NkStringView rhs);
            NKENTSEU_CORE_API bool NkEqualsIgnoreCase(NkStringView lhs, NkStringView rhs);
            
            /**
             * @brief Comparaison lexicographique naturelle (version)
             */
            NKENTSEU_CORE_API int NkCompareNatural(NkStringView lhs, NkStringView rhs);
            NKENTSEU_CORE_API int NkCompareNaturalIgnoreCase(NkStringView lhs, NkStringView rhs);
            
            // ========================================
            // PREDICATES
            // ========================================
            
            NKENTSEU_CORE_API bool NkIsWhitespace(char ch);
            NKENTSEU_CORE_API bool NkIsDigit(char ch);
            NKENTSEU_CORE_API bool NkIsAlpha(char ch);
            NKENTSEU_CORE_API bool NkIsAlphaNumeric(char ch);
            NKENTSEU_CORE_API bool NkIsLower(char ch);
            NKENTSEU_CORE_API bool NkIsUpper(char ch);
            NKENTSEU_CORE_API bool NkIsHexDigit(char ch);
            NKENTSEU_CORE_API bool NkIsPrintable(char ch);
            
            NKENTSEU_CORE_API bool NkIsAllWhitespace(NkStringView str);
            NKENTSEU_CORE_API bool NkIsAllDigits(NkStringView str);
            NKENTSEU_CORE_API bool NkIsAllAlpha(NkStringView str);
            NKENTSEU_CORE_API bool NkIsAllAlphaNumeric(NkStringView str);
            NKENTSEU_CORE_API bool NkIsAllHexDigits(NkStringView str);
            NKENTSEU_CORE_API bool NkIsAllPrintable(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est numérique (avec signe et décimales)
             */
            NKENTSEU_CORE_API bool NkIsNumeric(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est un entier valide
             */
            NKENTSEU_CORE_API bool NkIsInteger(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est un palindrome
             */
            NKENTSEU_CORE_API bool NkIsPalindrome(NkStringView str, bool ignoreCase = false);
            
            // ========================================
            // PADDING
            // ========================================
            
            NKENTSEU_CORE_API NkString NkPadLeft(NkStringView str, usize totalWidth, char paddingChar = ' ');
            NKENTSEU_CORE_API NkString NkPadRight(NkStringView str, usize totalWidth, char paddingChar = ' ');
            NKENTSEU_CORE_API NkString NkPadCenter(NkStringView str, usize totalWidth, char paddingChar = ' ');
            
            // ========================================
            // REPEAT
            // ========================================
            
            NKENTSEU_CORE_API NkString NkRepeat(NkStringView str, usize count);
            NKENTSEU_CORE_API NkString NkRepeat(char ch, usize count);
            
            // ========================================
            // FORMAT (sprintf-like simplifié)
            // ========================================
            
            NKENTSEU_CORE_API NkString NkFormat(const char* format, ...);
            
            /**
             * @brief Format avec arguments variadiques
             */
            NKENTSEU_CORE_API NkString NkVFormat(const char* format, va_list args);
            
            // ========================================
            // ESCAPING & UNESCAPING
            // ========================================
            
            /**
             * @brief Échappe les caractères spéciaux
             */
            NKENTSEU_CORE_API NkString NkEscape(NkStringView str, NkStringView charsToEscape, char escapeChar = '\\');
            
            /**
             * @brief Déséchappe les caractères
             */
            NKENTSEU_CORE_API NkString NkUnescape(NkStringView str, char escapeChar = '\\');
            
            /**
             * @brief Échappe pour les chaînes C (guillemets, retours à la ligne...)
             */
            NKENTSEU_CORE_API NkString NkCEscape(NkStringView str);
            NKENTSEU_CORE_API NkString NkCUnescape(NkStringView str);
            
            /**
             * @brief Échappe pour HTML
             */
            NKENTSEU_CORE_API NkString NkHTMLEscape(NkStringView str);
            NKENTSEU_CORE_API NkString NkHTMLUnescape(NkStringView str);
            
            /**
             * @brief Échappe pour URL
             */
            NKENTSEU_CORE_API NkString NkURLEncode(NkStringView str);
            NKENTSEU_CORE_API NkString NkURLDecode(NkStringView str);
            
            // ========================================
            // HASHING
            // ========================================
            
            /**
             * @brief Hash de chaîne (FNV-1a)
             */
            NKENTSEU_CORE_API uint64 NkHashFNV1a(NkStringView str);
            
            /**
             * @brief Hash insensible à la casse
             */
            NKENTSEU_CORE_API uint64 NkHashFNV1aIgnoreCase(NkStringView str);
            
            /**
             * @brief Hash DJB2
             */
            NKENTSEU_CORE_API uint64 NkHashDJB2(NkStringView str);
            
            /**
             * @brief Hash SDBM
             */
            NKENTSEU_CORE_API uint64 NkHashSDBM(NkStringView str);
            
            // ========================================
            // ENCODING HELPERS
            // ========================================
            
            /**
             * @brief Convertit UTF-8 en ASCII (perte de caractères non-ASCII)
             */
            NKENTSEU_CORE_API NkString NkToAscii(NkStringView str, char replacement = '?');
            
            /**
             * @brief Vérifie si la chaîne est valide ASCII
             */
            NKENTSEU_CORE_API bool NkIsValidAscii(NkStringView str);
            
            // ========================================
            // FILE PATH HELPERS
            // ========================================
            
            NKENTSEU_CORE_API NkStringView NkGetFileName(NkStringView path);
            NKENTSEU_CORE_API NkStringView NkGetFileNameWithoutExtension(NkStringView path);
            NKENTSEU_CORE_API NkStringView NkGetDirectory(NkStringView path);
            NKENTSEU_CORE_API NkStringView NkGetExtension(NkStringView path);
            NKENTSEU_CORE_API NkString NkChangeExtension(NkStringView path, NkStringView newExtension);
            NKENTSEU_CORE_API NkString NkCombinePaths(NkStringView path1, NkStringView path2);
            
            /**
             * @brief Normalise les séparateurs de chemin
             */
            NKENTSEU_CORE_API NkString NkNormalizePath(NkStringView path, char separator = '/');
            
            /**
             * @brief Vérifie si le chemin est absolu
             */
            NKENTSEU_CORE_API bool NkIsAbsolutePath(NkStringView path);
            
            // ========================================
            // TEMPLATE REPLACEMENT
            // ========================================
            
            /**
             * @brief Remplace les templates {{key}} par des valeurs
             */
            template<typename Map>
            NkString NkReplaceTemplates(NkStringView str, const Map& replacements) {
                NkString result(str);
                
                for (const auto& pair : replacements) {
                    NkString templateStr = NkFormat("{{%s}}", NkStringView(pair.first).Data());
                    result = NkReplaceAll(result.View(), templateStr.View(), NkStringView(pair.second));
                }
                
                return result;
            }
            
            // ========================================
            // LINE PROCESSING
            // ========================================
            
            /**
             * @brief Supprime les commentaires (à partir de // ou #)
             */
            NKENTSEU_CORE_API NkString NkRemoveComments(NkStringView str, NkStringView commentMarkers = "//#");
            
            /**
             * @brief Normalise les fins de ligne
             */
            NKENTSEU_CORE_API NkString NkNormalizeLineEndings(NkStringView str, NkStringView newline = "\n");
            
            /**
             * @brief Compte le nombre de lignes
             */
            NKENTSEU_CORE_API usize NkCountLines(NkStringView str);
            
            /**
             * @brief Obtient une ligne spécifique
             */
            NKENTSEU_CORE_API NkStringView NkGetLine(NkStringView str, usize lineNumber);
            
            // ========================================
            // STRING MANIPULATION
            // ========================================
            
            /**
             * @brief Remplit une chaîne avec un caractère
             */
            NKENTSEU_CORE_API void NkFill(NkString& str, char ch, usize count);
            
            /**
             * @brief Remplit une chaîne avec un caractère (nouvelle chaîne)
             */
            NKENTSEU_CORE_API NkString NkFillCopy(char ch, usize count);
            
            /**
             * @brief Nettoie la chaîne (trim + suppression espaces multiples)
             */
            NKENTSEU_CORE_API NkString NkClean(NkStringView str);
            NKENTSEU_CORE_API void NkCleanInPlace(NkString& str);
            
            /**
             * @brief Remplit les caractères manquants
             */
            NKENTSEU_CORE_API NkString NkFillMissing(NkStringView str, char fillChar, usize totalLength, bool left = true);
            
            // ========================================
            // CHARACTER MANIPULATION
            // ========================================
            
            /**
             * @brief Remplace un caractère
             */
            NKENTSEU_CORE_API NkString NkReplaceChar(NkStringView str, char from, char to);
            NKENTSEU_CORE_API void NkReplaceCharInPlace(NkString& str, char from, char to);
            
            /**
             * @brief Remplace tous les caractères
             */
            NKENTSEU_CORE_API NkString NkReplaceAllChars(NkStringView str, char from, char to);
            NKENTSEU_CORE_API void NkReplaceAllCharsInPlace(NkString& str, char from, char to);
            
            /**
             * @brief Supprime un caractère à une position
             */
            NKENTSEU_CORE_API NkString NkRemoveAt(NkStringView str, usize position);
            NKENTSEU_CORE_API void NkRemoveAtInPlace(NkString& str, usize position);
            
            /**
             * @brief Insère un caractère
             */
            NKENTSEU_CORE_API NkString NkInsertChar(NkStringView str, usize position, char ch);
            NKENTSEU_CORE_API void NkInsertCharInPlace(NkString& str, usize position, char ch);
            
            // ========================================
            // VALIDATION
            // ========================================
            
            /**
             * @brief Vérifie si la chaîne correspond à un motif
             */
            NKENTSEU_CORE_API bool NkMatchesPattern(NkStringView str, NkStringView pattern);
            
            /**
             * @brief Vérifie si la chaîne est un email valide
             */
            NKENTSEU_CORE_API bool NkIsEmail(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est une URL valide
             */
            NKENTSEU_CORE_API bool NkIsURL(NkStringView str);
            
            /**
             * @brief Vérifie si la chaîne est un identifiant valide
             */
            NKENTSEU_CORE_API bool NkIsIdentifier(NkStringView str);
            
            // ========================================
            // UTILITIES
            // ========================================
            
            /**
             * @brief Génère une chaîne aléatoire
             */
            NKENTSEU_CORE_API NkString NkRandomString(usize length, NkStringView charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
            
            /**
             * @brief Génère un UUID
             */
            NKENTSEU_CORE_API NkString NkGenerateUUID();
            
            /**
             * @brief Obfuscation simple de chaîne
             */
            NKENTSEU_CORE_API NkString NkObfuscate(NkStringView str, uint32 seed = 0);
            NKENTSEU_CORE_API NkString NkDeobfuscate(NkStringView str, uint32 seed = 0);
            
            /**
             * @brief Différence entre deux chaînes
             */
            NKENTSEU_CORE_API usize NkLevenshteinDistance(NkStringView str1, NkStringView str2);
            
            /**
             * @brief Similarité entre deux chaînes (0-1)
             */
            NKENTSEU_CORE_API float64 NkSimilarity(NkStringView str1, NkStringView str2);
            
            // ========================================
            // STRING VIEW MANIPULATION
            // ========================================
            
            /**
             * @brief Crée une vue sur une sous-chaîne
             */
            NKENTSEU_CORE_API NkStringView NkMakeView(const char* data, usize length);
            
            /**
             * @brief Crée une vue sur une chaîne C
             */
            NKENTSEU_CORE_API NkStringView NkMakeView(const char* cstr);
            
            // ========================================
            // MEMORY OPERATIONS
            // ========================================
            
            /**
             * @brief Copie sécurisée de chaîne
             */
            NKENTSEU_CORE_API usize NkSafeCopy(char* dest, usize destSize, NkStringView src);
            
            /**
             * @brief Concaténation sécurisée
             */
            NKENTSEU_CORE_API usize NkSafeConcat(char* dest, usize destSize, NkStringView src);
            
        } // namespace string
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_STRING_NKSTRINGUTILS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================