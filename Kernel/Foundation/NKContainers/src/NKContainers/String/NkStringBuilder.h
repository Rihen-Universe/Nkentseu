// -----------------------------------------------------------------------------
// FICHIER: Core\NKContainers\src\NKContainers\String\NkStringBuilder.h
// DESCRIPTION: Builder de chaînes pour concaténations efficaces (buffer croissant)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_STRING_NKSTRINGBUILDER_H
#define NK_CONTAINERS_STRING_NKSTRINGBUILDER_H

    // -------------------------------------------------------------------------
    // Inclusions standard
    // -------------------------------------------------------------------------
    #include <cstdarg>

    // -------------------------------------------------------------------------
    // Inclusions des dépendances du projet
    // -------------------------------------------------------------------------
    #include "NkString.h"
    #include "NkStringView.h"
    #include "NKMemory/NkAllocator.h"
    #include "NKMemory/NkFunction.h"

    // -------------------------------------------------------------------------
    // Namespace principal du projet
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // =====================================================================
        // CLASSE : NkStringBuilder
        // =====================================================================

        /**
         * @brief Builder de chaînes pour concaténations efficaces
         * 
         * Cette classe fournit un mécanisme optimisé pour construire des chaînes
         * via de multiples opérations d'ajout, sans les pénalités d'allocation
         * répétées d'une approche naïve avec NkString.
         * 
         * Caractéristiques principales :
         * - Buffer interne à croissance automatique (stratégie géométrique)
         * - Évite les réallocations multiples lors de concaténations en boucle
         * - Interface fluide avec opérateur << et méthodes Append() chaînables
         * - Support des types numériques, booléens, chaînes, itérateurs
         * - Compatible avec allocateurs personnalisés
         * - Conversion finale vers NkString ou NkStringView
         * 
         * @par Exemple d'utilisation basique :
         * @code
         *   nkentseu::NkStringBuilder sb;
         *   sb.Append("Hello")
         *     .Append(' ')
         *     .Append("World")
         *     .AppendLine("!");
         *   nkentseu::NkString result = sb.ToString();  // "Hello World!\n"
         * @endcode
         * 
         * @note Performance : Pré-allouer avec Reserve() si la taille finale est connue.
         *       Ex: sb.Reserve(1024) avant une boucle d'ajouts massifs.
         * 
         * @warning Thread-safety : Cette classe n'est PAS thread-safe par défaut.
         *          La synchronisation externe est requise pour un accès concurrent.
         */
        class NKENTSEU_CONTAINERS_API NkStringBuilder {

            // =================================================================
            // TYPES PUBLICS ET CONSTANTES
            // =================================================================
            public:

                /// Type utilisé pour les tailles, indices et capacités
                using SizeType = usize;

                /// Type des éléments stockés (caractères)
                using ValueType = char;

                /// Itérateur modifiable sur le buffer interne
                using Iterator = char*;

                /// Itérateur constant sur le buffer interne
                using ConstIterator = const char*;

                /// Itérateur inverse modifiable
                using ReverseIterator = char*;

                /// Itérateur inverse constant
                using ConstReverseIterator = const char*;

                /// Capacité initiale par défaut pour les nouveaux builders
                static constexpr SizeType DEFAULT_CAPACITY = 256;

                /// Capacité maximale théorique (évite overflow lors des calculs)
                static constexpr SizeType MAX_CAPACITY = (SizeType)-1 - 1;

            // =================================================================
            // CONSTRUCTEURS ET DESTRUCTEUR
            // =================================================================
            public:

                /**
                 * @brief Constructeur par défaut
                 * 
                 * Initialise un builder vide avec capacité DEFAULT_CAPACITY.
                 * Utilise l'allocateur par défaut du système.
                 * 
                 * @note Complexité : O(1), allocation unique du buffer initial.
                 */
                NkStringBuilder();

                /**
                 * @brief Constructeur avec capacité initiale personnalisée
                 * 
                 * @param initialCapacity Taille initiale du buffer interne
                 * 
                 * @note Utile si la taille finale approximative est connue à l'avance.
                 *       Évite les réallocations précoces lors de gros builds.
                 */
                explicit NkStringBuilder(SizeType initialCapacity);

                /**
                 * @brief Constructeur avec allocateur personnalisé
                 * 
                 * @param allocator Référence vers l'allocateur à utiliser
                 * 
                 * @note Toutes les allocations futures utiliseront cet allocateur.
                 *       L'allocateur doit rester valide pendant toute la durée
                 *       de vie de l'instance NkStringBuilder.
                 */
                explicit NkStringBuilder(memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur complet : capacité + allocateur
                 * 
                 * @param initialCapacity Taille initiale du buffer interne
                 * @param allocator Référence vers l'allocateur à utiliser
                 * 
                 * @note Combinaison des deux options précédentes pour contrôle maximal.
                 */
                NkStringBuilder(SizeType initialCapacity, memory::NkIAllocator& allocator);

                /**
                 * @brief Constructeur depuis NkStringView
                 * 
                 * @param view Vue de chaîne à copier dans le builder
                 * 
                 * @note Initialise le builder avec le contenu de view.
                 *       La vue source peut être détruite après construction.
                 */
                explicit NkStringBuilder(NkStringView view);

                /**
                 * @brief Constructeur depuis chaîne C terminée par null
                 * 
                 * @param str Pointeur vers une chaîne C-style (const char*)
                 * 
                 * @note Copie les données de str dans le builder.
                 *       Si str est nullptr : initialise un builder vide.
                 */
                explicit NkStringBuilder(const char* str);

                /**
                 * @brief Constructeur depuis NkString
                 * 
                 * @param str Instance NkString à copier dans le builder
                 * 
                 * @note Copie les données de str dans le builder.
                 *       Plus efficace que conversion str -> view -> builder.
                 */
                explicit NkStringBuilder(const NkString& str);

                /**
                 * @brief Constructeur de copie
                 * 
                 * @param other Instance NkStringBuilder à copier
                 * 
                 * @note Effectue une copie profonde du buffer interne.
                 *       Préserve la capacité et l'allocateur de other.
                 */
                NkStringBuilder(const NkStringBuilder& other);

                /**
                 * @brief Constructeur de déplacement
                 * 
                 * @param other Instance NkStringBuilder à déplacer
                 * 
                 * @note Transfère la propriété du buffer de other vers this.
                 *       other devient vide et réinitialisé après l'opération.
                 *       Complexité : O(1), aucune copie de données.
                 */
                NkStringBuilder(NkStringBuilder&& other) noexcept;

                /**
                 * @brief Destructeur
                 * 
                 * @note Libère le buffer interne si alloué sur le heap.
                 *       Garantit aucune fuite mémoire.
                 */
                ~NkStringBuilder();

            // =================================================================
            // OPÉRATEURS D'AFFECTATION
            // =================================================================
            public:

                /**
                 * @brief Affectation par copie
                 * 
                 * @param other Instance source à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Gère l'auto-affectation.
                 *       Réalloue si capacité insuffisante.
                 *       Préserve l'allocateur courant de this.
                 */
                NkStringBuilder& operator=(const NkStringBuilder& other);

                /**
                 * @brief Affectation par déplacement
                 * 
                 * @param other Instance source à déplacer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Transfère les ressources de other vers this.
                 *       other devient vide après l'opération.
                 *       Noexcept : garantit pas d'exception pour sécurité STL.
                 */
                NkStringBuilder& operator=(NkStringBuilder&& other) noexcept;

                /**
                 * @brief Affectation depuis NkStringView
                 * 
                 * @param view Vue de chaîne à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Remplace le contenu actuel par une copie de view.
                 *       Plus efficace que conversion view -> NkString -> assign.
                 */
                NkStringBuilder& operator=(NkStringView view);

                /**
                 * @brief Affectation depuis chaîne C-style
                 * 
                 * @param str Chaîne C terminée par null à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Remplace le contenu actuel par une copie de str.
                 *       Gère le cas str == nullptr (devient builder vide).
                 */
                NkStringBuilder& operator=(const char* str);

                /**
                 * @brief Affectation depuis NkString
                 * 
                 * @param str Instance NkString à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Remplace le contenu actuel par une copie de str.
                 *       Optimisé : évite conversion intermédiaire.
                 */
                NkStringBuilder& operator=(const NkString& str);

            // =================================================================
            // OPÉRATIONS D'AJOUT (APPEND) - CHAÎNES ET CARACTÈRES
            // =================================================================
            public:

                /**
                 * @brief Ajoute une chaîne C à la fin
                 * 
                 * @param str Chaîne C terminée par null à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Calcule automatiquement la longueur via strlen-like.
                 *       Gère str == nullptr comme chaîne vide (no-op).
                 *       Garantit null-termination après ajout.
                 */
                NkStringBuilder& Append(const char* str);

                /**
                 * @brief Ajoute N caractères depuis un buffer à la fin
                 * 
                 * @param str Pointeur vers les données à ajouter
                 * @param length Nombre de caractères à copier depuis str
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'ajouter des sous-parties ou données non-terminées.
                 *       Plus efficace que Append(str) si length < strlen(str).
                 */
                NkStringBuilder& Append(const char* str, SizeType length);

                /**
                 * @brief Ajoute le contenu d'une autre NkString à la fin
                 * 
                 * @param str Instance NkString à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Optimisé : évite conversion intermédiaire.
                 *       Gère l'auto-concaténation correctement.
                 */
                NkStringBuilder& Append(const NkString& str);

                /**
                 * @brief Ajoute le contenu d'une NkStringView à la fin
                 * 
                 * @param view Vue de chaîne à concaténer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Efficace : copie directe depuis la vue sans allocation temporaire.
                 */
                NkStringBuilder& Append(NkStringView view);

                /**
                 * @brief Ajoute un caractère unique à la fin
                 * 
                 * @param ch Caractère à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Méthode la plus courante pour construction caractère par caractère.
                 *       Très efficace : O(1) amorti grâce au buffer croissant.
                 */
                NkStringBuilder& Append(char ch);

                /**
                 * @brief Ajoute N répétitions d'un caractère à la fin
                 * 
                 * @param count Nombre de fois répéter le caractère
                 * @param ch Caractère à répéter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.Append(3, '-') ajoute "---" à la fin.
                 *       Utilise NkMemSet pour efficacité sur remplissage.
                 */
                NkStringBuilder& Append(SizeType count, char ch);

                /**
                 * @brief Ajoute une plage d'éléments via itérateurs
                 * 
                 * @tparam InputIterator Type d'itérateur d'entrée (ForwardIterator minimum)
                 * @param first Itérateur vers le premier élément à ajouter
                 * @param last Itérateur vers après le dernier élément à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'ajouter depuis n'importe quel conteneur itérable.
                 *       Les éléments doivent être convertibles en char.
                 * 
                 * @par Exemple :
                 * @code
                 *   std::vector<char> chars = {'H', 'e', 'l', 'l', 'o'};
                 *   sb.Append(chars.begin(), chars.end());  // Ajoute "Hello"
                 * @endcode
                 */
                template<typename InputIterator>
                NkStringBuilder& Append(InputIterator first, InputIterator last);

                /**
                 * @brief Ajoute tous les éléments d'un conteneur
                 * 
                 * @tparam Container Type de conteneur itérable (vector, list, array, etc.)
                 * @param container Référence constante vers le conteneur source
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Délègue à Append(begin(), end()) pour réutilisation du code.
                 *       Les éléments du conteneur doivent être convertibles en char.
                 */
                template<typename Container>
                NkStringBuilder& Append(const Container& container);

            // =================================================================
            // OPÉRATIONS D'AJOUT - TYPES NUMÉRIQUES ET BOOLÉENS
            // =================================================================
            public:

                /**
                 * @brief Ajoute un entier signé 8 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.Append(static_cast<int8>(-42)) ajoute "-42"
                 */
                NkStringBuilder& Append(int8 value);

                /**
                 * @brief Ajoute un entier signé 16 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(int16 value);

                /**
                 * @brief Ajoute un entier signé 32 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(int32 value);

                /**
                 * @brief Ajoute un entier signé 64 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(int64 value);

                /**
                 * @brief Ajoute un entier non-signé 8 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(uint8 value);

                /**
                 * @brief Ajoute un entier non-signé 16 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(uint16 value);

                /**
                 * @brief Ajoute un entier non-signé 32 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(uint32 value);

                /**
                 * @brief Ajoute un entier non-signé 64 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Append(uint64 value);

                /**
                 * @brief Ajoute un flottant 32 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Précision par défaut : 6 décimales.
                 *       Accepte notation scientifique pour valeurs extrêmes.
                 */
                NkStringBuilder& Append(float32 value);

                /**
                 * @brief Ajoute un flottant 64 bits en représentation décimale
                 * 
                 * @param value Valeur à convertir et ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Précision par défaut : 6 décimales.
                 */
                NkStringBuilder& Append(float64 value);

                /**
                 * @brief Ajoute une valeur booléenne en texte
                 * 
                 * @param value Valeur booléenne à convertir
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ajoute "true" ou "false" (minuscules, style JSON).
                 */
                NkStringBuilder& Append(bool value);

            // =================================================================
            // OPÉRATIONS D'AJOUT - REPRÉSENTATIONS ALTERNATIVES
            // =================================================================
            public:

                /**
                 * @brief Ajoute un entier non-signé 32 bits en hexadécimal
                 * 
                 * @param value Valeur à convertir
                 * @param prefix true pour ajouter le préfixe "0x", false sinon
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Chiffres en minuscules : "ff", "a1b2", etc.
                 *       Ex: sb.AppendHex(255, true) ajoute "0xff"
                 */
                NkStringBuilder& AppendHex(uint32 value, bool prefix = false);

                /**
                 * @brief Ajoute un entier non-signé 64 bits en hexadécimal
                 * 
                 * @param value Valeur à convertir
                 * @param prefix true pour ajouter le préfixe "0x"
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& AppendHex(uint64 value, bool prefix = false);

                /**
                 * @brief Ajoute un entier non-signé 32 bits en binaire
                 * 
                 * @param value Valeur à convertir
                 * @param bits Nombre de bits à afficher (1-32, défaut: 32)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.AppendBinary(5, 8) ajoute "00000101"
                 *       Padding avec zéros à gauche si bits > largeur réelle.
                 */
                NkStringBuilder& AppendBinary(uint32 value, SizeType bits = 32);

                /**
                 * @brief Ajoute un entier non-signé 64 bits en binaire
                 * 
                 * @param value Valeur à convertir
                 * @param bits Nombre de bits à afficher (1-64, défaut: 64)
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& AppendBinary(uint64 value, SizeType bits = 64);

                /**
                 * @brief Ajoute un entier non-signé 32 bits en octal
                 * 
                 * @param value Valeur à convertir
                 * @param prefix true pour ajouter le préfixe "0", false sinon
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.AppendOctal(64, true) ajoute "0100"
                 */
                NkStringBuilder& AppendOctal(uint32 value, bool prefix = false);

                /**
                 * @brief Ajoute un entier non-signé 64 bits en octal
                 * 
                 * @param value Valeur à convertir
                 * @param prefix true pour ajouter le préfixe "0"
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& AppendOctal(uint64 value, bool prefix = false);

            // =================================================================
            // OPÉRATEUR DE FLUX STYLE (<<)
            // =================================================================
            public:

                /**
                 * @brief Opérateur << pour chaîne C
                 * 
                 * @param str Chaîne C à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(str). Permet syntaxe style flux :
                 *       sb << "Hello" << " " << "World";
                 */
                NkStringBuilder& operator<<(const char* str);

                /**
                 * @brief Opérateur << pour NkString
                 * 
                 * @param str Instance NkString à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(str).
                 */
                NkStringBuilder& operator<<(const NkString& str);

                /**
                 * @brief Opérateur << pour NkStringView
                 * 
                 * @param view Vue de chaîne à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(view).
                 */
                NkStringBuilder& operator<<(NkStringView view);

                /**
                 * @brief Opérateur << pour caractère unique
                 * 
                 * @param ch Caractère à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(ch).
                 */
                NkStringBuilder& operator<<(char ch);

                /**
                 * @brief Opérateur << pour entier signé 8 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(value).
                 */
                NkStringBuilder& operator<<(int8 value);

                /**
                 * @brief Opérateur << pour entier signé 16 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(int16 value);

                /**
                 * @brief Opérateur << pour entier signé 32 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(int32 value);

                /**
                 * @brief Opérateur << pour entier signé 64 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(int64 value);

                /**
                 * @brief Opérateur << pour entier non-signé 8 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(uint8 value);

                /**
                 * @brief Opérateur << pour entier non-signé 16 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(uint16 value);

                /**
                 * @brief Opérateur << pour entier non-signé 32 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(uint32 value);

                /**
                 * @brief Opérateur << pour entier non-signé 64 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(uint64 value);

                /**
                 * @brief Opérateur << pour flottant 32 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(float32 value);

                /**
                 * @brief Opérateur << pour flottant 64 bits
                 * 
                 * @param value Valeur à ajouter
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& operator<<(float64 value);

                /**
                 * @brief Opérateur << pour booléen
                 * 
                 * @param value Valeur booléenne à ajouter
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ajoute "true" ou "false".
                 */
                NkStringBuilder& operator<<(bool value);

            // =================================================================
            // OPÉRATIONS D'AJOUT DE LIGNES ET FORMATAGE
            // =================================================================
            public:

                /**
                 * @brief Ajoute un saut de ligne (newline)
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ajoute '\n' (Unix-style).
                 *       Pour Windows-style, utiliser Append("\r\n").
                 */
                NkStringBuilder& AppendLine();

                /**
                 * @brief Ajoute une chaîne suivie d'un saut de ligne
                 * 
                 * @param str Vue de chaîne à ajouter avant le newline
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Append(str).AppendLine().
                 */
                NkStringBuilder& AppendLine(NkStringView str);

                /**
                 * @brief Ajoute une chaîne C suivie d'un saut de ligne
                 * 
                 * @param str Chaîne C à ajouter avant le newline
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Append(str).AppendLine().
                 */
                NkStringBuilder& AppendLine(const char* str);

                /**
                 * @brief Ajoute une NkString suivie d'un saut de ligne
                 * 
                 * @param str Instance NkString à ajouter avant le newline
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Append(str).AppendLine().
                 */
                NkStringBuilder& AppendLine(const NkString& str);

                /**
                 * @brief Ajoute une chaîne formatée style printf
                 * 
                 * @param format Chaîne de format avec spécificateurs (%s, %d, %.2f, etc.)
                 * @param ... Arguments variables correspondant aux spécificateurs
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Syntaxe compatible vsnprintf.
                 *       @warning Ne pas passer de NkString directement : utiliser CStr().
                 * 
                 * @par Exemple :
                 * @code
                 *   sb.AppendFormat("User %s has %d points", "Alice", 42);
                 *   // Ajoute : "User Alice has 42 points"
                 * @endcode
                 */
                NkStringBuilder& AppendFormat(const char* format, ...);

                /**
                 * @brief Ajoute une chaîne formatée avec va_list
                 * 
                 * @param format Chaîne de format avec spécificateurs
                 * @param args Liste d'arguments de type va_list
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Version pour wrappers ou fonctions forwardant les arguments.
                 *       Ne modifie pas args : peut être réutilisé après appel.
                 */
                NkStringBuilder& AppendVFormat(const char* format, va_list args);

            // =================================================================
            // OPÉRATIONS D'INSERTION À POSITION
            // =================================================================
            public:

                /**
                 * @brief Insère un caractère à une position donnée
                 * 
                 * @param pos Index où insérer le nouveau caractère (0 = début)
                 * @param ch Caractère à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Décale les caractères existants vers la droite.
                 *       Assertion si pos > Length().
                 */
                NkStringBuilder& Insert(SizeType pos, char ch);

                /**
                 * @brief Insère une chaîne C à une position donnée
                 * 
                 * @param pos Index d'insertion
                 * @param str Chaîne C à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Délègue à Insert(pos, str, strlen(str)).
                 */
                NkStringBuilder& Insert(SizeType pos, const char* str);

                /**
                 * @brief Insère N caractères depuis un buffer à une position
                 * 
                 * @param pos Index d'insertion
                 * @param str Pointeur vers les données à insérer
                 * @param length Nombre de caractères à copier depuis str
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'insérer des sous-parties ou données non-terminées.
                 */
                NkStringBuilder& Insert(SizeType pos, const char* str, SizeType length);

                /**
                 * @brief Insère une NkStringView à une position donnée
                 * 
                 * @param pos Index d'insertion
                 * @param view Vue de chaîne à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Efficace : copie directe depuis la vue.
                 */
                NkStringBuilder& Insert(SizeType pos, NkStringView view);

                /**
                 * @brief Insère une NkString à une position donnée
                 * 
                 * @param pos Index d'insertion
                 * @param str Instance NkString à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Délègue à Insert(pos, str.View()).
                 */
                NkStringBuilder& Insert(SizeType pos, const NkString& str);

                /**
                 * @brief Insère N répétitions d'un caractère à une position
                 * 
                 * @param pos Index d'insertion
                 * @param count Nombre de répétitions du caractère
                 * @param ch Caractère à insérer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.Insert(2, 3, '*') sur "abcd" donne "ab***cd"
                 */
                NkStringBuilder& Insert(SizeType pos, SizeType count, char ch);

                /**
                 * @brief Insère une plage d'éléments via itérateurs à une position
                 * 
                 * @tparam InputIterator Type d'itérateur d'entrée
                 * @param pos Index d'insertion
                 * @param first Itérateur vers le premier élément à insérer
                 * @param last Itérateur vers après le dernier élément
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'insérer depuis n'importe quel conteneur itérable.
                 */
                template<typename InputIterator>
                NkStringBuilder& Insert(SizeType pos, InputIterator first, InputIterator last);

            // =================================================================
            // OPÉRATIONS DE REMPLACEMENT
            // =================================================================
            public:

                /**
                 * @brief Remplace une plage par une chaîne C
                 * 
                 * @param pos Index de début de la plage à remplacer
                 * @param count Nombre de caractères à remplacer
                 * @param str Chaîne C de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Si count == strlen(str) : copie directe in-place.
                 *       Sinon : Erase + Insert pour gérer différence de taille.
                 */
                NkStringBuilder& Replace(SizeType pos, SizeType count, const char* str);

                /**
                 * @brief Remplace une plage par une NkStringView
                 * 
                 * @param pos Index de début de la plage
                 * @param count Nombre de caractères à remplacer
                 * @param view Vue de chaîne de remplacement
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& Replace(SizeType pos, SizeType count, NkStringView view);

                /**
                 * @brief Remplace toutes les occurrences d'un caractère par un autre
                 * 
                 * @param oldChar Caractère à rechercher
                 * @param newChar Caractère de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Parcours linéaire : O(n).
                 *       Ex: sb.Replace('a', 'A') sur "banana" donne "bAnAnA"
                 */
                NkStringBuilder& Replace(char oldChar, char newChar);

                /**
                 * @brief Remplace la première occurrence d'une sous-chaîne
                 * 
                 * @param oldStr Sous-chaîne à rechercher
                 * @param newStr Chaîne de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Implémentation naïve O(n*m) : suffisante pour la plupart des cas.
                 */
                NkStringBuilder& Replace(NkStringView oldStr, NkStringView newStr);

                /**
                 * @brief Remplace toutes les occurrences d'un caractère par un autre
                 * 
                 * @param oldChar Caractère à rechercher
                 * @param newChar Caractère de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Replace(oldChar, newChar). Nom plus explicite.
                 */
                NkStringBuilder& ReplaceAll(char oldChar, char newChar);

                /**
                 * @brief Remplace toutes les occurrences d'une sous-chaîne
                 * 
                 * @param oldStr Sous-chaîne à rechercher
                 * @param newStr Chaîne de remplacement
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Parcours séquentiel : les remplacements ne se chevauchent pas.
                 *       Ex: sb.ReplaceAll("aa", "b") sur "aaa" donne "ba" (pas "bbb")
                 */
                NkStringBuilder& ReplaceAll(NkStringView oldStr, NkStringView newStr);

            // =================================================================
            // OPÉRATIONS DE SUPPRESSION ET NETTOYAGE
            // =================================================================
            public:

                /**
                 * @brief Supprime une plage de caractères
                 * 
                 * @param pos Index de début de la plage à supprimer
                 * @param count Nombre de caractères à supprimer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Décale les caractères suivants vers la gauche.
                 *       Ne réduit pas automatiquement la capacité.
                 */
                NkStringBuilder& Remove(SizeType pos, SizeType count);

                /**
                 * @brief Supprime le caractère à une position donnée
                 * 
                 * @param pos Index du caractère à supprimer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Remove(pos, 1).
                 */
                NkStringBuilder& RemoveAt(SizeType pos);

                /**
                 * @brief Supprime N caractères au début de la chaîne
                 * 
                 * @param count Nombre de caractères à retirer du préfixe
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Remove(0, count).
                 */
                NkStringBuilder& RemovePrefix(SizeType count);

                /**
                 * @brief Supprime N caractères à la fin de la chaîne
                 * 
                 * @param count Nombre de caractères à retirer du suffixe
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Équivalent à Remove(Length() - count, count).
                 */
                NkStringBuilder& RemoveSuffix(SizeType count);

                /**
                 * @brief Supprime toutes les occurrences d'un caractère
                 * 
                 * @param ch Caractère à supprimer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Compactage in-place : O(n).
                 *       Ex: sb.RemoveAll('a') sur "banana" donne "bnn"
                 */
                NkStringBuilder& RemoveAll(char ch);

                /**
                 * @brief Supprime toutes les occurrences d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à supprimer
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Parcours séquentiel avec compactage.
                 */
                NkStringBuilder& RemoveAll(NkStringView str);

                /**
                 * @brief Supprime tous les caractères d'espacement (whitespace)
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Supprime : ' ', '\t', '\n', '\r', '\v', '\f'
                 *       Ex: "a b\tc\nd" -> "abcd"
                 */
                NkStringBuilder& RemoveWhitespace();

                /**
                 * @brief Supprime les espaces blancs aux deux extrémités
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Combine TrimLeft() + TrimRight().
                 */
                NkStringBuilder& Trim();

                /**
                 * @brief Supprime les espaces blancs à gauche uniquement
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& TrimLeft();

                /**
                 * @brief Supprime les espaces blancs à droite uniquement
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& TrimRight();

                /**
                 * @brief Vide le builder sans libérer la capacité
                 * 
                 * @note Met Length() à 0, préserve Capacity() pour réutilisation.
                 *       Aucune libération mémoire : efficace pour réutilisation.
                 *       Le null-terminator est maintenu en position 0.
                 */
                void Clear() noexcept;

                /**
                 * @brief Réinitialise le builder (alias pour Clear)
                 * 
                 * @note Identique fonctionnellement à Clear().
                 *       Nom alternatif pour sémantique de "reset".
                 */
                void Reset() noexcept;

                /**
                 * @brief Libère toute la mémoire allouée
                 * 
                 * @note Réinitialise à l'état par défaut : capacité DEFAULT_CAPACITY.
                 *       Utile pour libérer de la mémoire après un gros build.
                 */
                void Release();

            // =================================================================
            // CAPACITÉ, TAILLE ET ÉTAT
            // =================================================================
            public:

                /**
                 * @brief Retourne le nombre de caractères dans le builder
                 * 
                 * @return Longueur actuelle en caractères (exclut null-terminator)
                 * 
                 * @note Complexité O(1) : valeur pré-calculée et maintenue.
                 */
                SizeType Length() const noexcept;

                /**
                 * @brief Alias pour Length() (convention STL)
                 * 
                 * @return Nombre d'éléments dans le builder
                 */
                SizeType Size() const noexcept;

                /**
                 * @brief Alias pour Length() avec cast explicite usize
                 * 
                 * @return Longueur castée en type usize standard
                 */
                usize Count() const noexcept;

                /**
                 * @brief Retourne la capacité totale allouée
                 * 
                 * @return Nombre maximal de caractères stockables sans réallocation
                 * 
                 * @note Capacity() >= Length() toujours.
                 *       La différence représente l'espace de croissance réservé.
                 */
                SizeType Capacity() const noexcept;

                /**
                 * @brief Retourne la taille maximale théorique
                 * 
                 * @return MAX_CAPACITY : valeur limite avant overflow
                 */
                SizeType MaxSize() const noexcept;

                /**
                 * @brief Teste si le builder est vide
                 * 
                 * @return true si Length() == 0, false sinon
                 */
                bool Empty() const noexcept;

                /**
                 * @brief Teste si le buffer interne est nullptr
                 * 
                 * @return true si mBuffer == nullptr, false sinon
                 * 
                 * @note Un builder peut être non-null mais vide (buffer valide, longueur 0).
                 */
                bool IsNull() const noexcept;

                /**
                 * @brief Teste si le builder est nullptr OU vide
                 * 
                 * @return true si mBuffer == nullptr OU mLength == 0
                 * 
                 * @note Méthode pratique pour les vérifications de validité rapides.
                 */
                bool IsNullOrEmpty() const noexcept;

                /**
                 * @brief Teste si le buffer est plein (Length() == Capacity())
                 * 
                 * @return true si aucune croissance possible sans réallocation
                 * 
                 * @note Utile pour optimiser les appels à Reserve() avant boucles.
                 */
                bool IsFull() const noexcept;

                /**
                 * @brief Réserve de la capacité pour croissance future
                 * 
                 * @param newCapacity Nouvelle capacité minimale souhaitée
                 * 
                 * @note Si newCapacity <= Capacity(), aucune action.
                 *       Sinon : alloue un nouveau buffer et copie les données.
                 *       Utile pour éviter les réallocations multiples en boucle.
                 */
                void Reserve(SizeType newCapacity);

                /**
                 * @brief Redimensionne le builder à une longueur cible
                 * 
                 * @param newSize Nouvelle longueur souhaitée
                 * @param fillChar Caractère à utiliser pour l'extension (défaut: '\\0')
                 * 
                 * @note Si newSize < Length() : tronque à newSize caractères.
                 *       Si newSize > Length() : étend avec fillChar.
                 *       Peut déclencher réallocation si newSize > Capacity().
                 */
                void Resize(SizeType newSize, char fillChar = '\0');

                /**
                 * @brief Réduit la capacité au strict nécessaire
                 * 
                 * @note Si Length() < Capacity() : réalloue à taille exacte.
                 *       Peut déclencher une réallocation et copie.
                 *       Utile après construction finale pour minimiser l'empreinte mémoire.
                 */
                void ShrinkToFit();

            // =================================================================
            // ACCÈS AUX ÉLÉMENTS
            // =================================================================
            public:

                /**
                 * @brief Accès indexé sans vérification de bornes (version modifiable)
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence modifiable vers le caractère
                 * 
                 * @warning Comportement indéfini si index >= Length().
                 *          Utiliser At() en mode développement pour sécurité.
                 */
                char& operator[](SizeType index);

                /**
                 * @brief Accès indexé sans vérification de bornes (version constante)
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 */
                const char& operator[](SizeType index) const;

                /**
                 * @brief Accès indexé avec vérification de bornes (version modifiable)
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence modifiable vers le caractère
                 * 
                 * @note Lève une assertion avec message en mode debug si index hors bornes.
                 *       Préférer cette méthode pour code robuste en développement.
                 */
                char& At(SizeType index);

                /**
                 * @brief Accès indexé avec vérification de bornes (version constante)
                 * 
                 * @param index Position du caractère à accéder
                 * @return Référence constante vers le caractère
                 */
                const char& At(SizeType index) const;

                /**
                 * @brief Accès au premier caractère (version modifiable)
                 * 
                 * @return Référence modifiable vers le caractère en position 0
                 * 
                 * @warning Assertion si Empty() : vérifier avant appel.
                 */
                char& Front();

                /**
                 * @brief Accès au premier caractère (version constante)
                 * 
                 * @return Référence constante vers le caractère en position 0
                 */
                const char& Front() const;

                /**
                 * @brief Accès au dernier caractère (version modifiable)
                 * 
                 * @return Référence modifiable vers le dernier caractère
                 * 
                 * @warning Assertion si Empty() : vérifier avant appel.
                 */
                char& Back();

                /**
                 * @brief Accès au dernier caractère (version constante)
                 * 
                 * @return Référence constante vers le dernier caractère
                 */
                const char& Back() const;

                /**
                 * @brief Accès aux données brutes (version modifiable)
                 * 
                 * @return Pointeur modifiable vers le premier caractère
                 * 
                 * @warning Modifier les données via ce pointeur peut invalider
                 *          les métadonnées internes (longueur, null-terminator).
                 *          Utiliser avec précaution, préférer les méthodes API.
                 */
                char* Data() noexcept;

                /**
                 * @brief Accès aux données brutes (version constante)
                 * 
                 * @return Pointeur constant vers le premier caractère
                 * 
                 * @note Retourne toujours un pointeur valide, même si Empty().
                 *       Les données sont garanties null-terminated.
                 */
                const char* Data() const noexcept;

                /**
                 * @brief Accès compatible C-string (null-terminated garanti)
                 * 
                 * @return Pointeur constant vers chaîne C-style
                 * 
                 * @note Méthode privilégiée pour interop avec APIs C.
                 *       Le pointeur reste valide jusqu'à la prochaine
                 *       modification non-const de cette instance.
                 */
                const char* CStr() const noexcept;

            // =================================================================
            // EXTRACTION DE SOUS-CHAÎNES (RETOURNE DES VUES)
            // =================================================================
            public:

                /**
                 * @brief Extrait une sous-vue à partir d'une position
                 * 
                 * @param pos Position de départ dans le builder courant
                 * @param count Nombre maximal de caractères à inclure (npos = jusqu'à la fin)
                 * @return Nouvelle NkStringView sur la sous-partie spécifiée
                 * 
                 * @note Aucune copie : la vue référence les données internes du builder.
                 *       @warning La vue devient invalide si le builder est modifié.
                 *       Assertion si pos > Length().
                 */
                NkStringView SubStr(SizeType pos = 0, SizeType count = NkStringView::npos) const;

                /**
                 * @brief Extrait une sous-vue par bornes [start, end)
                 * 
                 * @param start Index inclus du début de la sous-vue
                 * @param end Index exclu de la fin de la sous-vue
                 * @return Nouvelle NkStringView sur l'intervalle spécifié
                 * 
                 * @note Interface style "slice" Python : plus intuitive pour certains cas.
                 *       Aucune copie : attention à la durée de vie du builder.
                 */
                NkStringView Slice(SizeType start, SizeType end) const;

                /**
                 * @brief Retourne une vue sur les N premiers caractères
                 * 
                 * @param count Nombre de caractères à extraire du début
                 * @return Vue sur le préfixe de longueur min(count, Length())
                 */
                NkStringView Left(SizeType count) const;

                /**
                 * @brief Retourne une vue sur les N derniers caractères
                 * 
                 * @param count Nombre de caractères à extraire de la fin
                 * @return Vue sur le suffixe de longueur min(count, Length())
                 */
                NkStringView Right(SizeType count) const;

                /**
                 * @brief Alias pour SubStr() (convention Qt-style)
                 * 
                 * @param pos Position de départ
                 * @param count Longueur optionnelle (npos = jusqu'à la fin)
                 * @return Sous-vue spécifiée
                 */
                NkStringView Mid(SizeType pos, SizeType count = NkStringView::npos) const;

                /**
                 * @brief Crée une copie possédante du contenu actuel
                 * 
                 * @return Nouvelle instance NkString avec copie des données
                 * 
                 * @note Allocation mémoire nécessaire : utiliser avec précaution
                 *       dans les boucles ou code performance-critical.
                 */
                NkString ToString() const;

                /**
                 * @brief Crée une copie possédante d'une sous-partie
                 * 
                 * @param pos Position de départ dans le builder
                 * @param count Nombre de caractères à copier (npos = jusqu'à la fin)
                 * @return Nouvelle instance NkString contenant la sous-partie
                 * 
                 * @note Équivalent à SubStr(pos, count).ToString().
                 */
                NkString ToString(SizeType pos, SizeType count) const;

            // =================================================================
            // RECHERCHE ET TESTS DE CONTENU
            // =================================================================
            public:

                /**
                 * @brief Recherche la première occurrence d'un caractère
                 * 
                 * @param ch Caractère à rechercher
                 * @param pos Position de départ pour la recherche (défaut: 0)
                 * @return Index de la première occurrence, ou npos si non trouvé
                 * 
                 * @note Parcours linéaire optimisé : O(n) worst-case.
                 */
                SizeType Find(char ch, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche la première occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position de départ pour la recherche (défaut: 0)
                 * @return Index de la première occurrence, ou npos si non trouvé
                 * 
                 * @note Implémentation naïve O(n*m) : suffisante pour la plupart des cas.
                 */
                SizeType Find(NkStringView str, SizeType pos = 0) const noexcept;

                /**
                 * @brief Recherche la dernière occurrence d'un caractère
                 * 
                 * @param ch Caractère à rechercher
                 * @param pos Position limite pour recherche arrière (défaut: npos = fin)
                 * @return Index de la dernière occurrence, ou npos si non trouvé
                 * 
                 * @note Recherche depuis min(pos, Length()-1) vers le début.
                 */
                SizeType FindLast(char ch, SizeType pos = NkStringView::npos) const noexcept;

                /**
                 * @brief Recherche la dernière occurrence d'une sous-chaîne
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @param pos Position limite pour recherche arrière (défaut: npos)
                 * @return Index de la dernière occurrence, ou npos si non trouvé
                 */
                SizeType FindLast(NkStringView str, SizeType pos = NkStringView::npos) const noexcept;

                /**
                 * @brief Vérifie si le builder contient un caractère donné
                 * 
                 * @param ch Caractère à rechercher
                 * @return true si le caractère est présent au moins une fois
                 */
                bool Contains(char ch) const noexcept;

                /**
                 * @brief Vérifie si le builder contient une sous-chaîne donnée
                 * 
                 * @param str Sous-chaîne à rechercher
                 * @return true si str est trouvé dans le builder
                 */
                bool Contains(NkStringView str) const noexcept;

                /**
                 * @brief Vérifie si le builder commence par un caractère donné
                 * 
                 * @param ch Caractère à tester en première position
                 * @return true si Length() > 0 && Data()[0] == ch
                 */
                bool StartsWith(char ch) const noexcept;

                /**
                 * @brief Vérifie si le builder commence par un préfixe donné
                 * 
                 * @param prefix Vue représentant le préfixe attendu
                 * @return true si les premiers caractères correspondent exactement
                 * 
                 * @note Retourne false si prefix.Length() > Length().
                 */
                bool StartsWith(NkStringView prefix) const noexcept;

                /**
                 * @brief Vérifie si le builder se termine par un caractère donné
                 * 
                 * @param ch Caractère à tester en dernière position
                 * @return true si Length() > 0 && Data()[Length()-1] == ch
                 */
                bool EndsWith(char ch) const noexcept;

                /**
                 * @brief Vérifie si le builder se termine par un suffixe donné
                 * 
                 * @param suffix Vue représentant le suffixe attendu
                 * @return true si les derniers caractères correspondent exactement
                 * 
                 * @note Retourne false si suffix.Length() > Length().
                 */
                bool EndsWith(NkStringView suffix) const noexcept;

            // =================================================================
            // COMPARAISONS
            // =================================================================
            public:

                /**
                 * @brief Compare lexicographiquement avec une NkStringView
                 * 
                 * @param other Vue de chaîne à comparer
                 * @return <0 si this < other, 0 si égal, >0 si this > other
                 * 
                 * @note Comparaison binaire octet par octet (case-sensitive).
                 *       Utilise NkMemCompare pour efficacité sur grands buffers.
                 */
                int Compare(NkStringView other) const noexcept;

                /**
                 * @brief Compare lexicographiquement avec un autre NkStringBuilder
                 * 
                 * @param other Autre builder à comparer
                 * @return <0 si this < other, 0 si égal, >0 si this > other
                 * 
                 * @note Délègue à Compare(other.View()) pour réutilisation.
                 */
                int Compare(const NkStringBuilder& other) const noexcept;

                /**
                 * @brief Teste l'égalité binaire avec une NkStringView
                 * 
                 * @param other Vue de chaîne à comparer
                 * @return true si mêmes données et même longueur
                 */
                bool Equals(NkStringView other) const noexcept;

                /**
                 * @brief Teste l'égalité binaire avec un autre NkStringBuilder
                 * 
                 * @param other Autre builder à comparer
                 * @return true si contenus identiques
                 */
                bool Equals(const NkStringBuilder& other) const noexcept;

            // =================================================================
            // TRANSFORMATIONS IN-PLACE
            // =================================================================
            public:

                /**
                 * @brief Convertit le contenu en minuscules (ASCII) in-place
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Modifie directement le buffer interne : pas d'allocation supplémentaire.
                 *       Conversion ASCII uniquement : pour Unicode, utiliser ICU.
                 */
                NkStringBuilder& ToLower();

                /**
                 * @brief Convertit le contenu en majuscules (ASCII) in-place
                 * 
                 * @return Référence vers this pour chaînage
                 */
                NkStringBuilder& ToUpper();

                /**
                 * @brief Met la première lettre en majuscule, le reste en minuscules
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "hELLO" -> "Hello"
                 *       Conversion ASCII uniquement.
                 */
                NkStringBuilder& Capitalize();

                /**
                 * @brief Met chaque mot en Title Case (majuscule après espace)
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "hello world" -> "Hello World"
                 *       Délimiteurs de mots : espaces, tabulations.
                 */
                NkStringBuilder& TitleCase();

                /**
                 * @brief Inverse l'ordre des caractères in-place
                 * 
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: "abc" -> "cba"
                 *       Complexité O(n/2) : échange symétrique.
                 */
                NkStringBuilder& Reverse();

                /**
                 * @brief Remplit à gauche avec un caractère pour atteindre une largeur cible
                 * 
                 * @param totalWidth Largeur finale souhaitée
                 * @param paddingChar Caractère de remplissage (défaut: espace)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Si Length() >= totalWidth : no-op.
                 *       Ex: sb.PadLeft(5, '0') sur "42" donne "00042"
                 */
                NkStringBuilder& PadLeft(SizeType totalWidth, char paddingChar = ' ');

                /**
                 * @brief Remplit à droite avec un caractère pour atteindre une largeur cible
                 * 
                 * @param totalWidth Largeur finale souhaitée
                 * @param paddingChar Caractère de remplissage (défaut: espace)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Ex: sb.PadRight(5, '.') sur "Hi" donne "Hi..."
                 */
                NkStringBuilder& PadRight(SizeType totalWidth, char paddingChar = ' ');

                /**
                 * @brief Centre le contenu avec remplissage symétrique
                 * 
                 * @param totalWidth Largeur finale souhaitée
                 * @param paddingChar Caractère de remplissage (défaut: espace)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Si padding impair : le caractère excédentaire est ajouté à droite.
                 *       Ex: sb.PadCenter(5, '-') sur "X" donne "--X--"
                 */
                NkStringBuilder& PadCenter(SizeType totalWidth, char paddingChar = ' ');

            // =================================================================
            // ITÉRATEURS (COMPATIBILITÉ STL)
            // =================================================================
            public:

                /**
                 * @brief Itérateur modifiable vers le premier élément
                 * 
                 * @return Pointeur modifiable vers Data()
                 */
                Iterator begin() noexcept;

                /**
                 * @brief Itérateur constant vers le premier élément
                 * 
                 * @return Pointeur constant vers Data()
                 */
                ConstIterator begin() const noexcept;

                /**
                 * @brief Alias const pour begin() (cohérence API STL)
                 * 
                 * @return Pointeur constant vers Data()
                 */
                ConstIterator cbegin() const noexcept;

                /**
                 * @brief Itérateur modifiable vers après le dernier élément
                 * 
                 * @return Pointeur modifiable vers Data() + Length()
                 */
                Iterator end() noexcept;

                /**
                 * @brief Itérateur constant vers après le dernier élément
                 * 
                 * @return Pointeur constant vers Data() + Length()
                 */
                ConstIterator end() const noexcept;

                /**
                 * @brief Alias const pour end() (cohérence API STL)
                 * 
                 * @return Pointeur constant vers Data() + Length()
                 */
                ConstIterator cend() const noexcept;

                /**
                 * @brief Itérateur inverse modifiable vers le dernier élément
                 * 
                 * @return Pointeur modifiable vers Data() + Length() - 1
                 * 
                 * @warning Comportement indéfini si Empty() : vérifier avant usage
                 */
                ReverseIterator rbegin() noexcept;

                /**
                 * @brief Itérateur inverse constant vers le dernier élément
                 * 
                 * @return Pointeur constant vers Data() + Length() - 1
                 */
                ConstReverseIterator rbegin() const noexcept;

                /**
                 * @brief Alias const pour rbegin()
                 * 
                 * @return Pointeur constant vers Data() + Length() - 1
                 */
                ConstReverseIterator crbegin() const noexcept;

                /**
                 * @brief Itérateur inverse modifiable vers avant le premier élément
                 * 
                 * @return Pointeur modifiable vers Data() - 1
                 * 
                 * @note Position sentinelle : ne pas déréférencer
                 */
                ReverseIterator rend() noexcept;

                /**
                 * @brief Itérateur inverse constant vers avant le premier élément
                 * 
                 * @return Pointeur constant vers Data() - 1
                 */
                ConstReverseIterator rend() const noexcept;

                /**
                 * @brief Alias const pour rend()
                 * 
                 * @return Pointeur constant vers Data() - 1
                 */
                ConstReverseIterator crend() const noexcept;

            // =================================================================
            // OPÉRATIONS STYLE FLUX (STREAM-LIKE)
            // =================================================================
            public:

                /**
                 * @brief Écrit des données brutes dans le builder
                 * 
                 * @param data Pointeur vers les données à écrire
                 * @param size Nombre de bytes à copier
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Permet d'ajouter des données binaires ou non-textuelles.
                 *       Aucune interprétation de encoding : copie byte-à-byte.
                 */
                NkStringBuilder& Write(const void* data, SizeType size);

                /**
                 * @brief Écrit un caractère unique
                 * 
                 * @param ch Caractère à écrire
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour Append(ch). Nom alternatif pour style flux.
                 */
                NkStringBuilder& WriteChar(char ch);

                /**
                 * @brief Écrit une chaîne suivie d'un saut de ligne
                 * 
                 * @param str Vue de chaîne à écrire avant le newline (optionnel)
                 * @return Référence vers this pour chaînage
                 * 
                 * @note Alias pour AppendLine(str).
                 *       Si str est vide : ajoute uniquement le newline.
                 */
                NkStringBuilder& WriteLine(NkStringView str = NkStringView());

            // =================================================================
            // OPÉRATEURS DE CONVERSION
            // =================================================================
            public:

                /**
                 * @brief Conversion explicite vers NkString
                 * 
                 * @return Nouvelle instance NkString avec copie des données
                 * 
                 * @note Allocation mémoire nécessaire.
                 *       Syntaxe : NkString s = static_cast<NkString>(sb);
                 */
                explicit operator NkString() const;

                /**
                 * @brief Conversion explicite vers NkStringView
                 * 
                 * @return Vue en lecture seule sur les données internes
                 * 
                 * @note Aucune copie : la vue référence le buffer interne.
                 *       @warning La vue devient invalide si le builder est modifié.
                 */
                explicit operator NkStringView() const noexcept;

                /**
                 * @brief Conversion implicite vers C-string
                 * 
                 * @return Pointeur constant vers les données null-terminated
                 * 
                 * @note Permet d'utiliser le builder directement dans des APIs C.
                 *       Ex: printf("%s", static_cast<const char*>(sb));
                 *       @warning Le pointeur devient invalide après modification du builder.
                 */
                operator const char*() const noexcept;

                /**
                 * @brief Retourne une vue sur le contenu actuel
                 * 
                 * @return NkStringView en lecture seule sur les données internes
                 * 
                 * @note Méthode explicite préférée à la conversion implicite.
                 *       Plus claire pour l'intention du code.
                 */
                NkStringView View() const noexcept {
                    return NkStringView(mBuffer, mLength);
                }

            // =================================================================
            // UTILITAIRES DIVERS
            // =================================================================
            public:

                /**
                 * @brief Échange le contenu avec un autre builder en O(1)
                 * 
                 * @param other Référence vers l'autre NkStringBuilder à échanger
                 * 
                 * @note Échange uniquement les pointeurs et métadonnées.
                 *       Aucune copie de données : très efficace.
                 *       Noexcept : garantie pour conteneurs STL-like.
                 */
                void Swap(NkStringBuilder& other) noexcept;

                /**
                 * @brief Calcule un hash rapide du contenu actuel
                 * 
                 * @return Valeur de hash SizeType
                 * 
                 * @note Algorithme FNV-1a : bon compromis distribution/performance.
                 *       Non-cryptographique : pour tables de hachage uniquement.
                 */
                SizeType Hash() const noexcept;

            // =================================================================
            // MÉTHODES STATIQUES : JOIN DE COLLECTIONS
            // =================================================================
            public:

                /**
                 * @brief Joint un tableau de NkStringView avec un séparateur
                 * 
                 * @param separator Vue du séparateur à insérer entre les éléments
                 * @param strings Pointeur vers le premier élément du tableau de vues
                 * @param count Nombre d'éléments dans le tableau
                 * @return Nouveau NkStringBuilder contenant la concaténation
                 * 
                 * @note Pré-alloue la taille exacte pour éviter les réallocations multiples.
                 *       Gère correctement le cas count == 0 (retourne builder vide).
                 */
                static NkStringBuilder Join(
                    NkStringView separator, 
                    const NkStringView* strings, 
                    SizeType count);

                /**
                 * @brief Joint un tableau de NkStringBuilder avec un séparateur
                 * 
                 * @param separator Vue du séparateur à insérer entre les éléments
                 * @param builders Pointeur vers le premier élément du tableau de builders
                 * @param count Nombre d'éléments dans le tableau
                 * @return Nouveau NkStringBuilder contenant la concaténation
                 * 
                 * @note Utilise View() de chaque builder pour éviter copies intermédiaires.
                 */
                static NkStringBuilder Join(
                    NkStringView separator, 
                    const NkStringBuilder* builders, 
                    SizeType count);

                /**
                 * @brief Joint un conteneur quelconque avec un séparateur
                 * 
                 * @tparam Container Type de conteneur itérable (vector, list, array, etc.)
                 * @param separator Vue du séparateur à insérer entre les éléments
                 * @param items Référence constante vers le conteneur source
                 * @return Nouveau NkStringBuilder contenant la concaténation
                 * 
                 * @note Les éléments du conteneur doivent être convertibles en NkStringView.
                 *       Pré-calcule la taille totale pour une allocation unique optimale.
                 * 
                 * @par Exemple :
                 * @code
                 *   std::vector<NkString> words = {"Hello", "World"};
                 *   auto result = NkStringBuilder::Join(" ", words).ToString();
                 *   // "Hello World"
                 * @endcode
                 */
                template<typename Container>
                static NkStringBuilder Join(NkStringView separator, const Container& items);

            // =================================================================
            // MEMBRES PRIVÉS : GESTION INTERNE DU BUFFER
            // =================================================================
            private:

                /// Pointeur vers l'allocateur utilisé (nullptr = allocateur par défaut)
                memory::NkIAllocator* mAllocator;

                /// Pointeur vers le buffer de caractères (heap ou stack via SSO-like)
                char* mBuffer;

                /// Nombre actuel de caractères dans le buffer (exclut null-terminator)
                SizeType mLength;

                /// Capacité totale allouée du buffer (en caractères, exclut null-terminator)
                SizeType mCapacity;

                /**
                 * @brief Garantit que la capacité peut accueillir la taille demandée
                 * 
                 * @param needed Taille minimale requise (en caractères)
                 * 
                 * @note Si needed <= Capacity() : no-op.
                 *       Sinon : calcule nouvelle capacité avec stratégie de croissance
                 *       et appelle Reallocate() si nécessaire.
                 */
                void EnsureCapacity(SizeType needed);

                /**
                 * @brief Garantit que la capacité peut accueillir des données supplémentaires
                 * 
                 * @param additionalSize Nombre de caractères supplémentaires prévus
                 * 
                 * @note Délègue à EnsureCapacity(mLength + additionalSize).
                 *       Interface plus intuitive pour les appels d'ajout.
                 */
                void GrowIfNeeded(SizeType additionalSize);

                /**
                 * @brief Réalloue le buffer à une nouvelle capacité
                 * 
                 * @param newCapacity Nouvelle capacité souhaitée (en caractères)
                 * 
                 * @note Alloue un nouveau buffer via mAllocator, copie les données existantes,
                 *       libère l'ancien buffer, et met à jour mBuffer/mCapacity.
                 *       Préserve mLength et garantit null-termination.
                 */
                void Reallocate(SizeType newCapacity);

                /**
                 * @brief Helper : convertit un entier signé 64 bits en chaîne décimale
                 * 
                 * @param value Valeur entière à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 * 
                 * @note Écrit la représentation décimale de value dans buffer.
                 *       Gère le signe négatif automatiquement.
                 *       Retourne 0 si bufferSize insuffisant.
                 */
                static SizeType IntToString(int64 value, char* buffer, SizeType bufferSize);

                /**
                 * @brief Helper : convertit un entier non-signé 64 bits en chaîne décimale
                 * 
                 * @param value Valeur entière à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 */
                static SizeType UIntToString(uint64 value, char* buffer, SizeType bufferSize);

                /**
                 * @brief Helper : convertit un flottant 64 bits en chaîne décimale
                 * 
                 * @param value Valeur flottante à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @param precision Nombre de décimales à afficher (défaut: 6)
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 * 
                 * @note Accepte notation scientifique pour valeurs extrêmes.
                 *       Retourne 0 si bufferSize insuffisant.
                 */
                static SizeType FloatToString(
                    float64 value, 
                    char* buffer, 
                    SizeType bufferSize, 
                    int precision = 6);

                /**
                 * @brief Helper : convertit un entier non-signé 64 bits en hexadécimal
                 * 
                 * @param value Valeur entière à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @param prefix true pour ajouter le préfixe "0x"
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 * 
                 * @note Chiffres en minuscules : "ff", "a1b2", etc.
                 */
                static SizeType HexToString(
                    uint64 value, 
                    char* buffer, 
                    SizeType bufferSize, 
                    bool prefix);

                /**
                 * @brief Helper : convertit un entier non-signé 64 bits en binaire
                 * 
                 * @param value Valeur entière à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @param bits Nombre de bits à afficher (1-64)
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 * 
                 * @note Padding avec zéros à gauche si bits > largeur réelle.
                 */
                static SizeType BinaryToString(
                    uint64 value, 
                    char* buffer, 
                    SizeType bufferSize, 
                    SizeType bits);

                /**
                 * @brief Helper : convertit un entier non-signé 64 bits en octal
                 * 
                 * @param value Valeur entière à convertir
                 * @param buffer Pointeur vers le buffer de destination
                 * @param bufferSize Taille disponible du buffer destination
                 * @param prefix true pour ajouter le préfixe "0"
                 * @return Nombre de caractères écrits (exclut null-terminator)
                 */
                static SizeType OctalToString(
                    uint64 value, 
                    char* buffer, 
                    SizeType bufferSize, 
                    bool prefix);

                /**
                 * @brief Libère le buffer interne si alloué sur le heap
                 * 
                 * @note No-op si buffer est nullptr ou en mode "stack-like".
                 *       Met mBuffer à nullptr après libération.
                 */
                void FreeBuffer();

                /**
                 * @brief Alloue un nouveau buffer de capacité donnée
                 * 
                 * @param capacity Nouvelle capacité souhaitée (en caractères)
                 * 
                 * @note Utilise mAllocator si défini, sinon allocateur par défaut.
                 *       Ne copie pas les données existantes : appeler après
                 *       avoir sauvegardé le contenu si nécessaire.
                 */
                void AllocateBuffer(SizeType capacity);

                /**
                 * @brief Copie le contenu d'un autre builder dans this
                 * 
                 * @param other Référence constante vers le builder source
                 * 
                 * @note Alloue un nouveau buffer si nécessaire et copie les données.
                 *       Utilisé par le constructeur de copie et operator=.
                 */
                void CopyFrom(const NkStringBuilder& other);

                /**
                 * @brief Déplace le contenu d'un autre builder dans this
                 * 
                 * @param other Référence rvalue vers le builder source à déplacer
                 * 
                 * @note Transfère la propriété du buffer de other vers this.
                 *       other devient vide et réinitialisé après l'opération.
                 *       Utilisé par le constructeur de déplacement et operator=.
                 */
                void MoveFrom(NkStringBuilder&& other) noexcept;
        };

        // =====================================================================
        // OPÉRATEURS NON-MEMBRES : COMPARAISON
        // =====================================================================

        /**
         * @brief Teste l'égalité binaire entre deux NkStringBuilder
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si contenus identiques (même longueur, mêmes bytes)
         */
        inline bool operator==(const NkStringBuilder& lhs, const NkStringBuilder& rhs) noexcept {
            return lhs.Equals(rhs);
        }

        /**
         * @brief Teste la différence entre deux NkStringBuilder
         * 
         * @param lhs Opérande gauche
         * @param rhs Opérande droite
         * @return true si contenus différents
         */
        inline bool operator!=(const NkStringBuilder& lhs, const NkStringBuilder& rhs) noexcept {
            return !lhs.Equals(rhs);
        }

        /**
         * @brief Teste l'égalité NkStringBuilder vs NkStringView
         * 
         * @param lhs Builder à comparer
         * @param rhs Vue de chaîne à comparer
         * @return true si égalité binaire
         */
        inline bool operator==(const NkStringBuilder& lhs, NkStringView rhs) noexcept {
            return lhs.Equals(rhs);
        }

        /**
         * @brief Teste la différence NkStringBuilder vs NkStringView
         * 
         * @param lhs Builder à comparer
         * @param rhs Vue de chaîne à comparer
         * @return true si inégalité
         */
        inline bool operator!=(const NkStringBuilder& lhs, NkStringView rhs) noexcept {
            return !lhs.Equals(rhs);
        }

        /**
         * @brief Teste l'égalité NkStringView vs NkStringBuilder (symétrique)
         * 
         * @param lhs Vue de chaîne à comparer
         * @param rhs Builder à comparer
         * @return true si égalité binaire
         */
        inline bool operator==(NkStringView lhs, const NkStringBuilder& rhs) noexcept {
            return rhs.Equals(lhs);
        }

        /**
         * @brief Teste la différence NkStringView vs NkStringBuilder (symétrique)
         * 
         * @param lhs Vue de chaîne à comparer
         * @param rhs Builder à comparer
         * @return true si inégalité
         */
        inline bool operator!=(NkStringView lhs, const NkStringBuilder& rhs) noexcept {
            return !rhs.Equals(lhs);
        }

        // =====================================================================
        // OPÉRATEURS NON-MEMBRES : CONCATÉNATION (+)
        // =====================================================================

        /**
         * @brief Concatène un NkStringBuilder avec une NkStringView
         * 
         * @param lhs Builder de gauche
         * @param rhs Vue de chaîne à droite
         * @return Nouveau NkStringBuilder contenant lhs + rhs
         * 
         * @note Crée un nouveau builder : n'affecte pas lhs ni rhs.
         *       Optimisé : pré-calcule la taille totale pour une seule allocation.
         */
        inline NkStringBuilder operator+(const NkStringBuilder& lhs, NkStringView rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }

        /**
         * @brief Concatène une NkStringView avec un NkStringBuilder
         * 
         * @param lhs Vue de chaîne à gauche
         * @param rhs Builder de droite
         * @return Nouveau NkStringBuilder contenant lhs + rhs
         */
        inline NkStringBuilder operator+(NkStringView lhs, const NkStringBuilder& rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }

        /**
         * @brief Concatène deux NkStringBuilder
         * 
         * @param lhs Builder de gauche
         * @param rhs Builder de droite
         * @return Nouveau NkStringBuilder contenant lhs + rhs
         * 
         * @note Crée un nouveau builder : n'affecte pas lhs ni rhs.
         *       Optimisé : pré-calcule la taille totale pour une seule allocation.
         */
        inline NkStringBuilder operator+(const NkStringBuilder& lhs, const NkStringBuilder& rhs) {
            NkStringBuilder result(lhs);
            result.Append(rhs);
            return result;
        }

    } // namespace nkentseu

// =====================================================================
// IMPLÉMENTATIONS DE TEMPLATES (DOIVENT ÊTRE DANS LE HEADER)
// =====================================================================

namespace nkentseu {

    // -------------------------------------------------------------------------
    // Implémentation : Append avec itérateurs
    // -------------------------------------------------------------------------
    template<typename InputIterator>
    NkStringBuilder& NkStringBuilder::Append(InputIterator first, InputIterator last) {
        SizeType count = 0;
        for (auto it = first; it != last; ++it) {
            ++count;
        }
        GrowIfNeeded(count);
        for (auto it = first; it != last; ++it) {
            mBuffer[mLength++] = *it;
        }
        mBuffer[mLength] = '\0';
        return *this;
    }

    // -------------------------------------------------------------------------
    // Implémentation : Append depuis conteneur
    // -------------------------------------------------------------------------
    template<typename Container>
    NkStringBuilder& NkStringBuilder::Append(const Container& container) {
        return Append(container.begin(), container.end());
    }

    // -------------------------------------------------------------------------
    // Implémentation : Insert avec itérateurs
    // -------------------------------------------------------------------------
    template<typename InputIterator>
    NkStringBuilder& NkStringBuilder::Insert(SizeType pos, InputIterator first, InputIterator last) {
        NKENTSEU_ASSERT(pos <= mLength);
        SizeType count = 0;
        for (auto it = first; it != last; ++it) {
            ++count;
        }
        if (count == 0) {
            return *this;
        }
        GrowIfNeeded(count);
        memory::NkMemMove(mBuffer + pos + count, mBuffer + pos, mLength - pos);
        SizeType i = 0;
        for (auto it = first; it != last; ++it, ++i) {
            mBuffer[pos + i] = *it;
        }
        mLength += count;
        mBuffer[mLength] = '\0';
        return *this;
    }

    // -------------------------------------------------------------------------
    // Implémentation : Join statique depuis conteneur
    // -------------------------------------------------------------------------
    template<typename Container>
    NkStringBuilder NkStringBuilder::Join(NkStringView separator, const Container& items) {
        NkStringBuilder result;
        if (items.empty()) {
            return result;
        }
        auto it = items.begin();
        result.Append(*it);
        ++it;
        for (; it != items.end(); ++it) {
            result.Append(separator);
            result.Append(*it);
        }
        return result;
    }

} // namespace nkentseu

#endif // NK_CONTAINERS_STRING_NKSTRINGBUILDER_H

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Construction fluide avec opérateur <<
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb;
        sb << "User: " << "Alice"
           << ", Age: " << 30
           << ", Active: " << true
           << ", Score: " << 98.5f
           << "\n";
        
        nkentseu::NkString result = sb.ToString();
        // "User: Alice, Age: 30, Active: true, Score: 98.500000\n"
    }

    // =====================================================================
    // Exemple 2 : Concaténation efficace en boucle (évite N allocations)
    // =====================================================================
    {
        // ❌ À ÉVITER : NkString en boucle -> N réallocations
        // nkentseu::NkString bad;
        // for (int i = 0; i < 1000; ++i) {
        //     bad += "chunk";  // Réallocation à chaque itération après SSO
        // }
        
        // ✅ PRÉFÉRER : NkStringBuilder avec buffer croissant
        nkentseu::NkStringBuilder sb;
        sb.Reserve(5000);  // Pré-allocation optionnelle si taille estimée connue
        for (int i = 0; i < 1000; ++i) {
            sb.Append("chunk");
        }
        nkentseu::NkString result = sb.ToString();  // Une seule allocation finale
    }

    // =====================================================================
    // Exemple 3 : Formatage de logs structurés
    // =====================================================================
    {
        nkentseu::NkStringBuilder BuildLogEntry(
            const char* level,
            uint64_t timestamp,
            nkentseu::NkStringView message) {
            
            return nkentseu::NkStringBuilder()
                .Append('[')
                .Append(level)
                .Append("] @")
                .AppendHex(timestamp, true)  // "0x..." prefix
                .Append(" | ")
                .Append(message.Trimmed())
                .AppendLine()
                .ToString();
        }
        
        // Usage :
        // BuildLogEntry("ERROR", 0x1234567890ABCDEF, "  Connection failed  ")
        // -> "[ERROR] @0x1234567890abcdef | Connection failed\n"
    }

    // =====================================================================
    // Exemple 4 : Génération de CSV avec Join statique
    // =====================================================================
    {
        std::vector<nkentseu::NkStringView> fields = {"Name", "Age", "City"};
        auto csvLine = nkentseu::NkStringBuilder::Join(",", fields).ToString();
        // "Name,Age,City"
        
        // Avec données :
        std::vector<nkentseu::NkString> row = {"Alice", "30", "Paris"};
        auto csvRow = nkentseu::NkStringBuilder::Join(",", row).ToString();
        // "Alice,30,Paris"
    }

    // =====================================================================
    // Exemple 5 : Transformation in-place avec chaînage
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("  hello   world  ");
        sb.Trim()                    // "hello   world"
          .ReplaceAll(' ', '_')      // "hello___world"
          .ToUpper()                 // "HELLO___WORLD"
          .PadCenter(20, '-');       // "--HELLO___WORLD---"
        
        nkentseu::NkString result = sb.ToString();
    }

    // =====================================================================
    // Exemple 6 : Insertion et remplacement précis
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("HelloWorld");
        
        // Insérer un espace au milieu
        sb.Insert(5, ' ');  // "Hello World"
        
        // Remplacer "World" par "nkentseu"
        usize pos = sb.Find("World");
        if (pos != nkentseu::NkStringView::npos) {
            sb.Replace(pos, 5, "nkentseu");  // "Hello nkentseu"
        }
        
        // Supprimer les 6 premiers caractères
        sb.RemovePrefix(6);  // "nkentseu"
    }

    // =====================================================================
    // Exemple 7 : Conversion numérique avec différentes bases
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb;
        
        uint32 value = 255;
        
        sb.Append("Decimal: ")
          .Append(value)           // "255"
          .Append(", Hex: ")
          .AppendHex(value, true)  // "0xff"
          .Append(", Binary: ")
          .AppendBinary(value, 8)  // "11111111"
          .Append(", Octal: ")
          .AppendOctal(value, true);  // "0377"
        
        // Résultat : "Decimal: 255, Hex: 0xff, Binary: 11111111, Octal: 0377"
    }

    // =====================================================================
    // Exemple 8 : Gestion d'allocateur personnalisé pour pooling
    // =====================================================================
    {
        // Supposons un allocateur pool pour builders temporaires
        class TempAllocator : public nkentseu::memory::NkIAllocator {
            // ... implémentation spécifique ...
        };
        
        TempAllocator tempAlloc;
        
        // Création de builders avec allocateur pool
        nkentseu::NkStringBuilder sb1(256, tempAlloc);
        nkentseu::NkStringBuilder sb2(256, tempAlloc);
        
        sb1 << "Temp data 1";
        sb2 << "Temp data 2";
        
        // Utilisation...
        ProcessData(sb1.CStr(), sb2.CStr());
        
        // Libération groupée possible si l'allocateur supporte le reset
        // tempAlloc.Reset();  // Libère tout le pool en O(1)
    }

    // =====================================================================
    // Exemple 9 : Vue temporaire avec précaution de durée de vie
    // =====================================================================
    {
        nkentseu::NkStringBuilder sb("Hello World");
        
        // ✅ BON : Utiliser la vue immédiatement, avant modification du builder
        nkentseu::NkStringView view = sb.View();
        ProcessView(view);  // Traitement immédiat : vue valide
        
        // ❌ DANGEREUX : Stocker la vue puis modifier le builder
        // auto dangerous = sb.View();
        // sb.Append(" more");  // ⚠️ La vue 'dangerous' peut être invalide maintenant !
        
        // ✅ BON : Si besoin de conserver après modification, convertir en NkString
        nkentseu::NkString owned = sb.ToString();  // Copie possédante
        sb.Append(" more");  // Safe : owned reste valide
    }

    // =====================================================================
    // Exemple 10 : Bonnes pratiques de performance
    // =====================================================================
    {
        // ✅ PRÉFÉRER : Pré-allouer si taille finale estimable
        nkentseu::NkStringBuilder sb;
        sb.Reserve(1024);  // Évite ~3-4 réallocations pour 1KB de données
        
        // ✅ PRÉFÉRER : Utiliser Append(str, length) si longueur connue
        const char* data = GetLargeBuffer();
        usize len = GetLargeBufferSize();
        sb.Append(data, len);  // Évite le calcul de strlen
        
        // ✅ PRÉFÉRER : Chaîner les appels pour éviter les temporaires
        sb.Append("Prefix: ")
          .Append(value)
          .Append(" suffix");  // Un seul builder, pas de temporaires intermédiaires
        
        // ✅ PRÉFÉRER : Utiliser ToString() une seule fois à la fin
        nkentseu::NkString final = sb.ToString();  // Allocation unique finale
        // Éviter : sb.ToString() dans une boucle -> N allocations inutiles
    }
*/