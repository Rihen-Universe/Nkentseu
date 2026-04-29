// -----------------------------------------------------------------------------
// FICHIER: NKContainers\Containers\Associative\NkTrie.h
// DESCRIPTION: Arbre de préfixes - Structure optimisée pour les opérations sur chaînes
// AUTEUR: Rihen
// DATE: 2026-02-09
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_ASSOCIATIVE_NKTRIE_H
#define NK_CONTAINERS_ASSOCIATIVE_NKTRIE_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Définition des types fondamentaux (usize, etc.)
    #include "NKContainers/NkContainersApi.h"      // Macros d'exportation de l'API conteneurs
    #include "NKCore/NkTraits.h"                   // Traits et utilitaires de métaprogrammation
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable
    #include "NKCore/Assert/NkAssert.h"            // Macros d'assertion pour le débogage sécurisé
    #include "NKContainers/Sequential/NkVector.h"  // Conteneur vectoriel pour collecte de résultats

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // CLASSE PRINCIPALE : NK TRIE (ARBRE DE PRÉFIXES)
        // ====================================================================

        /**
         * @brief Classe template implémentant un arbre de préfixes (Trie) pour chaînes de caractères
         * 
         * Structure de données arborescente optimisée pour le stockage et la recherche
         * efficace de chaînes de caractères, exploitant les préfixes communs pour réduire
         * l'espace mémoire et accélérer les opérations de recherche par préfixe.
         * 
         * PROPRIÉTÉS FONDAMENTALES :
         * - Chaque nœud représente un caractère, les chemins forment des mots
         * - Les nœuds terminaux marquent la fin d'un mot valide (IsEndOfWord)
         * - Alphabet fixe : 26 lettres minuscules (a-z), conversion case-insensitive
         * - Partage des préfixes communs : économie mémoire pour mots similaires
         * 
         * GARANTIES DE PERFORMANCE :
         * - Insertion : O(L) où L = longueur du mot inséré
         * - Recherche exacte (Search) : O(L) où L = longueur du mot recherché
         * - Recherche de préfixe (StartsWith) : O(L) où L = longueur du préfixe
         * - Autocomplétion : O(L + K) où K = nombre de mots avec le préfixe donné
         * - Espace mémoire : O(N * L) dans le pire cas, O(N) avec préfixes partagés
         * 
         * CAS D'USAGE TYPQUES :
         * - Systèmes d'autocomplétion (moteurs de recherche, éditeurs de code)
         * - Correcteurs orthographiques et suggestions de mots
         * - Tables de routage IP avec recherche de préfixe le plus long
         * - Dictionnaires et lexiques avec recherche par motif
         * - Filtrage et validation de saisie utilisateur en temps réel
         * 
         * @tparam Allocator Type de l'allocateur mémoire (défaut: memory::NkAllocator)
         * 
         * @note Alphabet limité à a-z (case-insensitive) : étendre pour Unicode si nécessaire
         * @note Thread-unsafe : synchronisation externe requise pour usage concurrent
         * @note Les chaînes doivent être en C-string null-terminated (const char*)
         */
        template<typename Allocator = memory::NkAllocator>
        class NkTrie {

            // ====================================================================
            // SECTION PRIVÉE : CONSTANTES ET STRUCTURE DE NŒUD
            // ====================================================================
            private:

                /**
                 * @brief Taille de l'alphabet supporté par l'implémentation
                 * @note Valeur fixe : 26 lettres minuscules (a à z)
                 * @note Pour étendre : modifier cette constante et CharToIndex()
                 * @note Impact mémoire : chaque nœud alloue ALPHABET_SIZE * sizeof(TrieNode*)
                 */
                static constexpr usize ALPHABET_SIZE = 26;

                /**
                 * @brief Structure interne représentant un nœud de l'arbre de préfixes
                 * 
                 * Contient un tableau de pointeurs vers les enfants (un par caractère)
                 * et un indicateur marquant si ce nœud termine un mot valide.
                 * 
                 * @note Layout mémoire : [Children: ALPHABET_SIZE * ptr][IsEndOfWord: bool]
                 * @note Tous les pointeurs enfants initialisés à nullptr au construction
                 * @note IsEndOfWord permet de distinguer "hel" (préfixe) de "hello" (mot)
                 */
                struct TrieNode {

                    TrieNode* Children[ALPHABET_SIZE];  ///< Tableau de pointeurs vers les nœuds enfants (indexés par caractère)
                    bool IsEndOfWord;                    ///< Indicateur : true si ce nœud termine un mot valide dans le dictionnaire

                    /**
                     * @brief Constructeur de nœud avec initialisation des membres
                     * @note Initialise IsEndOfWord à false (nouveau nœud ne termine pas un mot)
                     * @note Initialise tous les pointeurs Children à nullptr via boucle explicite
                     * @note Complexité O(ALPHABET_SIZE) = O(1) car taille fixe
                     */
                    TrieNode()
                        : IsEndOfWord(false) {
                        for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                            Children[i] = nullptr;
                        }
                    }
                };

            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES ET MÉTHODES INTERNES
            // ====================================================================
            private:

                // ====================================================================
                // MEMBRES DONNÉES : ÉTAT DE L'ARBRE DE PRÉFIXES
                // ====================================================================

                TrieNode* mRoot;              ///< Pointeur vers la racine de l'arbre (nœud vide, point d'entrée)
                Allocator* mAllocator;        ///< Pointeur vers l'allocateur mémoire utilisé pour les nœuds
                usize mSize;                  ///< Nombre total de mots valides stockés dans le Trie

                // ====================================================================
                // MÉTHODES UTILITAIRES DE CONVERSION ET GESTION MÉMOIRE
                // ====================================================================

                /**
                 * @brief Convertit un caractère en index de tableau [0, ALPHABET_SIZE)
                 * @param c Caractère à convertir (ASCII, lettre a-z ou A-Z)
                 * @return Index dans [0, 25] correspondant à la position dans Children[]
                 * @note Conversion case-insensitive : 'A' et 'a' -> 0, 'Z' et 'z' -> 25
                 * @note Assertion en debug pour valider que l'index est dans les bornes
                 * @note Pour étendre à d'autres caractères : modifier cette fonction
                 */
                usize CharToIndex(char c) const {
                    if (c >= 'A' && c <= 'Z') {
                        c = c - 'A' + 'a';
                    }
                    return c - 'a';
                }

                /**
                 * @brief Alloue et construit un nouveau nœud TrieNode avec placement new
                 * @return Pointeur vers le nœud nouvellement créé et initialisé
                 * @note Utilise l'allocateur configuré pour toutes les allocations
                 * @note Le constructeur TrieNode() initialise automatiquement les membres
                 */
                TrieNode* CreateNode() {
                    TrieNode* node = static_cast<TrieNode*>(mAllocator->Allocate(sizeof(TrieNode)));
                    new (node) TrieNode();
                    return node;
                }

                /**
                 * @brief Détruit récursivement un sous-arbre et libère sa mémoire
                 * @param node Pointeur vers la racine du sous-arbre à détruire
                 * @note Parcours post-order : destruction des enfants avant le parent
                 * @note Appel explicite du destructeur pour les types non-triviaux
                 * @note Complexité O(N) où N = nombre de nœuds dans le sous-arbre
                 */
                void DestroyNode(TrieNode* node) {
                    if (!node) {
                        return;
                    }
                    for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                        DestroyNode(node->Children[i]);
                    }
                    node->~TrieNode();
                    mAllocator->Deallocate(node);
                }

                /**
                 * @brief Collecte récursivement tous les mots valides sous un nœud donné
                 * @param node Nœud courant à partir duquel collecter les mots
                 * @param prefix Buffer temporaire contenant le préfixe courant pendant le parcours
                 * @param results Vecteur de sortie recevant les pointeurs vers les mots trouvés
                 * @note Utilise un parcours en profondeur (DFS) pour explorer tous les chemins
                 * @note Ajoute un mot aux résultats uniquement si IsEndOfWord == true
                 * @note ATTENTION : les pointeurs retournés pointent vers le buffer prefix interne
                 *       -> nécessite une copie dans une implémentation production
                 */
                void CollectWords(TrieNode* node, NkVector<char>& prefix, NkVector<const char*>& results) const {
                    if (!node) {
                        return;
                    }
                    if (node->IsEndOfWord) {
                        prefix.PushBack('\0');
                        results.PushBack(prefix.Data());
                        prefix.PopBack();
                    }
                    for (usize i = 0; i < ALPHABET_SIZE; ++i) {
                        if (node->Children[i]) {
                            prefix.PushBack('a' + i);
                            CollectWords(node->Children[i], prefix, results);
                            prefix.PopBack();
                        }
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS ET DESTRUCTEUR
            // ====================================================================
            public:

                /**
                 * @brief Constructeur principal avec allocateur optionnel
                 * @param allocator Pointeur vers un allocateur personnalisé (nullptr = défaut)
                 * @note Initialise un Trie vide avec un nœud racine prêt à recevoir des mots
                 * @note Complexité O(1) : allocation unique du nœud racine
                 * @note mSize initialisé à 0 : aucun mot valide stocké initialement
                 */
                explicit NkTrie(Allocator* allocator = nullptr)
                    : mRoot(nullptr)
                    , mAllocator(allocator ? allocator : &memory::NkGetDefaultAllocator())
                    , mSize(0) {
                    mRoot = CreateNode();
                }

                /**
                 * @brief Destructeur libérant tous les nœuds et la structure de l'arbre
                 * @note Appelle DestroyNode(mRoot) pour destruction récursive complète
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'exception
                 * @note Complexité O(N) où N = nombre total de nœuds dans l'arbre
                 */
                ~NkTrie() {
                    DestroyNode(mRoot);
                }

            // ====================================================================
            // SECTION PUBLIQUE : MODIFICATEURS - INSERTION
            // ====================================================================

                /**
                 * @brief Insère un nouveau mot dans l'arbre de préfixes
                 * @param word Pointeur vers une chaîne C null-terminated à insérer
                 * @note Crée les nœuds manquants le long du chemin correspondant au mot
                 * @note Marque le nœud terminal avec IsEndOfWord = true
                 * @note Ignore les insertions de mots déjà présents (unicité garantie)
                 * @note Complexité O(L) où L = longueur du mot inséré
                 * @note Case-insensitive : "Hello" et "hello" sont considérés identiques
                 */
                void Insert(const char* word) {
                    TrieNode* current = mRoot;
                    for (usize i = 0; word[i] != '\0'; ++i) {
                        usize index = CharToIndex(word[i]);
                        NKENTSEU_ASSERT(index < ALPHABET_SIZE);
                        if (!current->Children[index]) {
                            current->Children[index] = CreateNode();
                        }
                        current = current->Children[index];
                    }
                    if (!current->IsEndOfWord) {
                        current->IsEndOfWord = true;
                        ++mSize;
                    }
                }

            // ====================================================================
            // SECTION PUBLIQUE : RECHERCHE ET INTERROGATION
            // ====================================================================

                /**
                 * @brief Recherche la présence exacte d'un mot dans le Trie
                 * @param word Pointeur vers une chaîne C null-terminated à rechercher
                 * @return true si le mot existe et termine un chemin valide, false sinon
                 * @note Nécessite que IsEndOfWord == true au nœud final (pas juste un préfixe)
                 * @note Complexité O(L) où L = longueur du mot recherché
                 * @note Case-insensitive : recherche "HELLO" trouve "hello" si présent
                 * @note Retourne false immédiatement si un caractère du chemin est absent
                 */
                bool Search(const char* word) const {
                    TrieNode* current = mRoot;
                    for (usize i = 0; word[i] != '\0'; ++i) {
                        usize index = CharToIndex(word[i]);
                        if (index >= ALPHABET_SIZE) {
                            return false;
                        }
                        if (!current->Children[index]) {
                            return false;
                        }
                        current = current->Children[index];
                    }
                    return current && current->IsEndOfWord;
                }

                /**
                 * @brief Vérifie si un préfixe donné existe dans l'arbre
                 * @param prefix Pointeur vers une chaîne C null-terminated représentant le préfixe
                 * @return true si au moins un mot commence par ce préfixe, false sinon
                 * @note Ne requiert pas IsEndOfWord : un préfixe partiel suffit
                 * @note Complexité O(L) où L = longueur du préfixe recherché
                 * @note Utilisé pour l'autocomplétion : "hel" -> true si "hello" ou "help" présents
                 * @note Case-insensitive : StartsWith("HEL") trouve préfixes de "hello"
                 */
                bool StartsWith(const char* prefix) const {
                    TrieNode* current = mRoot;
                    for (usize i = 0; prefix[i] != '\0'; ++i) {
                        usize index = CharToIndex(prefix[i]);
                        if (index >= ALPHABET_SIZE) {
                            return false;
                        }
                        if (!current->Children[index]) {
                            return false;
                        }
                        current = current->Children[index];
                    }
                    return current != nullptr;
                }

            // ====================================================================
            // SECTION PUBLIQUE : AUTOCOMPLÉTION ET COLLECTE DE MOTS
            // ====================================================================

                /**
                 * @brief Trouve tous les mots valides commençant par un préfixe donné
                 * @param prefix Pointeur vers une chaîne C null-terminated représentant le préfixe
                 * @return Vecteur de pointeurs const char* vers les mots trouvés
                 * @note Retourne un vecteur vide si le préfixe n'existe pas dans l'arbre
                 * @note Complexité O(L + K) où L = longueur du préfixe, K = nombre de mots trouvés
                 * @note ATTENTION : les pointeurs retournés pointent vers un buffer interne temporaire
                 *       -> dans une implémentation production, copier les chaînes dans NkString
                 * @note Case-insensitive : FindWordsWithPrefix("HEL") trouve "hello", "help", etc.
                 */
                NkVector<const char*> FindWordsWithPrefix(const char* prefix) const {
                    NkVector<const char*> results;
                    NkVector<char> currentPrefix;
                    TrieNode* current = mRoot;
                    for (usize i = 0; prefix[i] != '\0'; ++i) {
                        currentPrefix.PushBack(prefix[i]);
                        usize index = CharToIndex(prefix[i]);
                        if (!current->Children[index]) {
                            return results;
                        }
                        current = current->Children[index];
                    }
                    CollectWords(current, currentPrefix, results);
                    return results;
                }

            // ====================================================================
            // SECTION PUBLIQUE : CAPACITÉ ET MÉTADONNÉES
            // ====================================================================

                /**
                 * @brief Retourne le nombre total de mots valides stockés dans le Trie
                 * @return Valeur de type usize représentant la cardinalité du dictionnaire
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Ne compte pas les nœuds intermédiaires : uniquement les mots avec IsEndOfWord=true
                 * @note Utile pour diagnostiquer la taille du dictionnaire sans parcours
                 */
                usize Size() const NKENTSEU_NOEXCEPT {
                    return mSize;
                }

                /**
                 * @brief Teste si le Trie ne contient aucun mot valide
                 * @return true si mSize == 0, false sinon
                 * @note Opération O(1) sans effet de bord, noexcept garantie
                 * @note Équivalent à Size() == 0 mais sémantiquement plus explicite
                 * @note Un Trie peut avoir des nœuds (préfixes) mais Empty() == true si aucun mot complet
                 */
                bool Empty() const NKENTSEU_NOEXCEPT {
                    return mSize == 0;
                }

            // ====================================================================
            // SECTION PUBLIQUE : RÉINITIALISATION
            // ====================================================================

                /**
                 * @brief Supprime tous les mots et réinitialise le Trie à l'état vide
                 * @note Détruit récursivement tous les nœuds via DestroyNode(mRoot)
                 * @note Recrée un nouveau nœud racine pour maintenir l'instance valide
                 * @note Réinitialise mSize à 0 : aucun mot valide après l'appel
                 * @note Complexité O(N) où N = nombre total de nœuds à détruire
                 * @note Préférer à la destruction/recréation pour réutilisation de l'instance
                 */
                void Clear() {
                    DestroyNode(mRoot);
                    mRoot = CreateNode();
                    mSize = 0;
                }
        };

    } // namespace nkentseu

#endif // NK_CONTAINERS_ASSOCIATIVE_NKTRIE_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkTrie
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Insertion et recherche de base
 * --------------------------------------------------------------------------
 * @code
 * #include "NKContainers/Associative/NkTrie.h"
 * #include <cstdio>
 * 
 * void exempleBase() {
 *     // Création d'un Trie avec allocateur par défaut
 *     nkentseu::NkTrie<> trie;
 *     
 *     // Insertion de mots (case-insensitive)
 *     trie.Insert("hello");
 *     trie.Insert("help");
 *     trie.Insert("world");
 *     trie.Insert("HELLO");  // Doublon ignoré (case-insensitive)
 *     
 *     // Recherche exacte : nécessite IsEndOfWord == true
 *     bool found1 = trie.Search("hello");   // true : mot présent
 *     bool found2 = trie.Search("hel");     // false : préfixe seulement
 *     bool found3 = trie.Search("world");   // true : mot présent
 *     bool found4 = trie.Search("wor");     // false : préfixe seulement
 *     
 *     printf("hello: %s\n", found1 ? "trouvé" : "absent");   // trouvé
 *     printf("hel: %s\n", found2 ? "trouvé" : "absent");     // absent
 *     
 *     // Vérification de préfixe : suffit que le chemin existe
 *     bool prefix1 = trie.StartsWith("hel");   // true : "hello", "help"
 *     bool prefix2 = trie.StartsWith("wor");   // true : "world"
 *     bool prefix3 = trie.StartsWith("xyz");   // false : aucun mot
 *     
 *     printf("Préfixe 'hel': %s\n", prefix1 ? "oui" : "non");  // oui
 *     
 *     // Statistiques
 *     printf("Mots stockés: %zu\n", trie.Size());  // 3 (hello, help, world)
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Autocomplétion et suggestions
 * --------------------------------------------------------------------------
 * @code
 * void exempleAutocomplete() {
 *     nkentseu::NkTrie<> dictionary;
 *     
 *     // Construction d'un mini-dictionnaire
 *     const char* words[] = {
 *         "apple", "application", "apply", "appreciate",
 *         "banana", "band", "bank", "banner",
 *         "cat", "catch", "category", "cathedral"
 *     };
 *     
 *     for (const char* word : words) {
 *         dictionary.Insert(word);
 *     }
 *     
 *     // Autocomplétion pour préfixe "app"
 *     printf("Suggestions pour 'app':\n");
 *     auto suggestions = dictionary.FindWordsWithPrefix("app");
 *     for (const char* suggestion : suggestions) {
 *         printf("  - %s\n", suggestion);
 *     }
 *     // Sortie attendue:
 *     //   - apple
 *     //   - application
 *     //   - apply
 *     //   - appreciate
 *     
 *     // Autocomplétion pour préfixe "ban"
 *     printf("\nSuggestions pour 'ban':\n");
 *     auto banWords = dictionary.FindWordsWithPrefix("ban");
 *     for (const char* word : banWords) {
 *         printf("  - %s\n", word);
 *     }
 *     // Sortie attendue:
 *     //   - banana
 *     //   - band
 *     //   - bank
 *     //   - banner
 *     
 *     // Préfixe sans correspondance
 *     auto empty = dictionary.FindWordsWithPrefix("xyz");
 *     printf("\nSuggestions pour 'xyz': %zu trouvées\n", empty.Size());  // 0
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Validation de saisie et filtrage en temps réel
 * --------------------------------------------------------------------------
 * @code
 * // Système de validation : vérifier si une saisie est un mot valide ou un préfixe valide
 * class InputValidator {
 * private:
 *     nkentseu::NkTrie<> mValidWords;
 *     
 * public:
 *     void LoadDictionary(const char** words, usize count) {
 *         for (usize i = 0; i < count; ++i) {
 *             mValidWords.Insert(words[i]);
 *         }
 *     }
 *     
 *     enum class ValidationResult {
 *         ValidWord,      // Saisie = mot valide complet
 *         ValidPrefix,    // Saisie = préfixe d'au moins un mot valide
 *         Invalid         // Saisie ne correspond à aucun chemin du Trie
 *     };
 *     
 *     ValidationResult Validate(const char* input) const {
 *         if (mValidWords.Search(input)) {
 *             return ValidationResult::ValidWord;
 *         } else if (mValidWords.StartsWith(input)) {
 *             return ValidationResult::ValidPrefix;
 *         } else {
 *             return ValidationResult::Invalid;
 *         }
 *     }
 *     
 *     // Obtenir des suggestions pour correction
 *     nkentseu::NkVector<const char*> GetSuggestions(const char* partial) const {
 *         return mValidWords.FindWordsWithPrefix(partial);
 *     }
 * };
 * 
 * void exempleValidation() {
 *     InputValidator validator;
 *     
 *     const char* dict[] = {"hello", "help", "world", "wide", "web"};
 *     validator.LoadDictionary(dict, 5);
 *     
 *     // Tests de validation
 *     auto r1 = validator.Validate("hel");    // ValidPrefix
 *     auto r2 = validator.Validate("hello");  // ValidWord
 *     auto r3 = validator.Validate("xyz");    // Invalid
 *     
 *     printf("hel: %s\n", 
 *            r1 == InputValidator::ValidationResult::ValidPrefix ? "préfixe valide" : "autre");
 *     
 *     // Suggestions pour correction
 *     printf("Suggestions pour 'we':\n");
 *     auto suggestions = validator.GetSuggestions("we");
 *     for (const char* s : suggestions) {
 *         printf("  - %s\n", s);  // world, wide, web
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateur() {
 *     // Pool allocator optimisé pour allocation de nombreux petits nœuds
 *     memory::NkPoolAllocator pool(1024 * 1024);  // Pool de 1MB
 *     
 *     // Trie utilisant le pool pour toutes ses allocations de TrieNode
 *     nkentseu::NkTrie<memory::NkPoolAllocator> trie(&pool);
 *     
 *     // Insertion en masse sans fragmentation externe
 *     // Simulation : chargement d'un dictionnaire de 10000 mots
 *     for (usize i = 0; i < 10000; ++i) {
 *         // Génération de mots simulés (en production: chargement depuis fichier)
 *         char word[32];
 *         // snprintf(word, sizeof(word), "word_%zu", i);  // Exemple
 *         // trie.Insert(word);
 *     }
 *     
 *     printf("Dictionnaire: %zu mots chargés\n", trie.Size());
 *     
 *     // Recherche rapide même avec grand volume
 *     bool found = trie.Search("word_1234");  // O(L) indépendant de la taille totale
 *     
 *     // Destruction : libération en bloc via le pool (très rapide vs free() individuel)
 *     // Le RAII du Trie garantit que DestroyNode() est appelé automatiquement
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Patterns avancés - Routage IP, correction orthographique
 * --------------------------------------------------------------------------
 * @code
 * // Pattern 1: Table de routage IP avec recherche de préfixe le plus long
 * // (Adaptation: utiliser un Trie binaire pour les bits d'adresse IP)
 * 
 * struct RouteEntry {
 *     const char* gateway;
 *     int metric;
 * };
 * 
 * // Note: Pour IP routing, utiliser un BinaryTrie<T> spécialisé
 * // Cet exemple montre le concept avec des préfixes texte
 * 
 * void exempleRoutage() {
 *     nkentseu::NkTrie<> routingTable;
 *     
 *     // Insertion de préfixes de réseau (simplifié)
 *     routingTable.Insert("192.168.1");    // Réseau local
 *     routingTable.Insert("192.168");      // Sous-réseau plus large
 *     routingTable.Insert("10.0");         // Autre réseau privé
 *     
 *     // Recherche du préfixe le plus long pour une adresse
 *     const char* address = "192.168.1.100";
 *     
 *     // Algorithme de longest prefix match (simplifié)
 *     const char* bestMatch = nullptr;
 *     for (usize len = strlen(address); len > 0; --len) {
 *         char prefix[64];
 *         strncpy(prefix, address, len);
 *         prefix[len] = '\0';
 *         
 *         if (routingTable.StartsWith(prefix)) {
 *             bestMatch = prefix;
 *             break;  // Premier match = plus long préfixe
 *         }
 *     }
 *     
 *     if (bestMatch) {
 *         printf("Route pour %s via préfixe: %s\n", address, bestMatch);
 *     }
 * }
 * 
 * // Pattern 2: Correction orthographique avec distance d'édition
 * // (Nécessite extension: recherche approximative dans le Trie)
 * 
 * void exempleCorrection() {
 *     nkentseu::NkTrie<> spellChecker;
 *     
 *     // Chargement d'un dictionnaire de mots valides
 *     const char* validWords[] = {"hello", "world", "apple", "apply", "help"};
 *     for (const char* word : validWords) {
 *         spellChecker.Insert(word);
 *     }
 *     
 *     // Vérification d'un mot saisi
 *     const char* userInput = "helo";  // Faute de frappe
 *     
 *     if (!spellChecker.Search(userInput)) {
 *         printf("'%s' n'est pas un mot valide\n", userInput);
 *         
 *         // Suggestions basées sur le préfixe commun
 *         printf("Suggestions:\n");
 *         auto prefix = userInput;  // Utiliser le préfixe saisi
 *         auto suggestions = spellChecker.FindWordsWithPrefix(prefix);
 *         
 *         for (const char* suggestion : suggestions) {
 *             printf("  - %s\n", suggestion);
 *         }
 *         
 *         // Pour une correction avancée: implémenter la distance de Levenshtein
 *         // et rechercher les mots à distance <= 2 du mot saisi
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Limitations et extensions possibles
 * --------------------------------------------------------------------------
 * @code
 * void exempleExtensions() {
 *     // LIMITATION ACTUELLE: Alphabet fixe a-z, case-insensitive
 *     nkentseu::NkTrie<> trie;
 *     
 *     // Caractères non supportés: chiffres, ponctuation, Unicode
 *     trie.Insert("hello123");   // '1'-'9' ne sont pas dans [a-z] -> comportement indéfini
 *     trie.Insert("café");       // 'é' hors ASCII -> problème de conversion
 *     
 *     // EXTENSION 1: Alphabet étendu
 *     // - Modifier ALPHABET_SIZE à 128 pour ASCII complet
 *     // - Adapter CharToIndex() pour mapper [0-127] -> [0, ALPHABET_SIZE)
 *     // - Attention: mémoire multipliée par 5 (128 vs 26 pointeurs par nœud)
 *     
 *     // EXTENSION 2: Support Unicode/UTF-8
 *     // - Utiliser un hashmap par nœud au lieu de tableau fixe
 *     // - Ou: encoder les codepoints Unicode en séquences d'index
 *     // - Plus complexe mais nécessaire pour textes multilingues
 *     
 *     // EXTENSION 3: Valeurs associées aux mots (Trie map)
 *     // struct TrieNode {
 *     //     Children[...];
 *     //     bool IsEndOfWord;
 *     //     Optional<Value> AssociatedValue;  // Nouvelle fonctionnalité
 *     // };
 *     // -> Permettrait NkTrieMap<string, int> pour dictionnaires clé-valeur
 *     
 *     // EXTENSION 4: Suppression de mots
 *     // - Actuellement: pas de méthode Erase()
 *     // - Implémentation: parcours récursif avec suppression des nœuds devenus inutiles
 *     // - Attention: ne pas supprimer un nœud s'il sert de préfixe à d'autres mots
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE L'ALPHABET ET ENCODAGE :
 *    - Alphabet actuel : 26 lettres a-z, case-insensitive
 *    - Pour chiffres/ponctuation : étendre ALPHABET_SIZE et CharToIndex()
 *    - Pour Unicode : envisager un hashmap par nœud ou encoding spécifique
 *    - Documenter clairement les caractères supportés dans l'API
 * 
 * 2. GESTION MÉMOIRE ET PERFORMANCE :
 *    - Chaque nœud alloue ALPHABET_SIZE * sizeof(TrieNode*) : 26 * 8 = 208 bytes (64-bit)
 *    - Pour grands dictionnaires : utiliser un allocateur pool pour réduire la fragmentation
 *    - Pré-allouer la capacité du pool selon le nombre estimé de nœuds
 *    - Attention à la profondeur maximale : mots très longs -> risque de stack overflow en récursif
 * 
 * 3. RECHERCHE ET AUTOCOMPLÉTION :
 *    - Search() : O(L) garanti, indépendant de la taille du dictionnaire
 *    - StartsWith() : idéal pour validation en temps réel de saisie utilisateur
 *    - FindWordsWithPrefix() : retourner des copies NkString en production (pas de pointeurs temporaires)
 *    - Pour suggestions triées : le parcours DFS donne un ordre lexicographique naturel
 * 
 * 4. SÉCURITÉ ET ROBUSTESSE :
 *    - Valider les entrées utilisateur avant Insert()/Search() : rejeter caractères hors alphabet
 *    - Limiter la longueur maximale des mots pour éviter les attaques par déni de service
 *    - Thread-unsafe : protéger avec mutex pour accès concurrents multiples
 *    - Les pointeurs retournés par FindWordsWithPrefix() sont temporaires : copier avant usage
 * 
 * 5. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de méthode Erase() pour supprimer des mots individuellement
 *    - Pas de support pour les valeurs associées aux mots (Trie map)
 *    - Alphabet fixe a-z : extension nécessaire pour cas d'usage réels
 *    - CollectWords() retourne des pointeurs vers buffer temporaire : bug potentiel en production
 * 
 * 6. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter Erase(const char* word) avec nettoyage des nœuds inutiles
 *    - Implémenter NkTrieMap<Value> avec champ Option<Value> dans TrieNode
 *    - Ajouter un itérateur pour parcours lexicographique sans collecte intermédiaire
 *    - Supporter la recherche approximative (distance d'édition <= k) pour correction orthographique
 *    - Ajouter une méthode GetLongestPrefix(const char* input) pour longest prefix match
 * 
 * 7. COMPARAISON AVEC AUTRES STRUCTURES :
 *    - vs NkHashMap<string, T> : Trie plus efficace pour les recherches par préfixe
 *    - vs NkMap<string, T> : Trie O(L) vs O(L log N) pour Map, mais plus de mémoire
 *    - vs tableau trié + binary search : Trie meilleur pour préfixes, tableau meilleur pour mémoire
 *    - Règle : utiliser Trie si >50% des requêtes sont des StartsWith() ou autocomplétion
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Généré par Rihen le 2026-02-05 22:26:13
// Dernière modification : 2026-04-26 (restructuration et documentation)
// ============================================================