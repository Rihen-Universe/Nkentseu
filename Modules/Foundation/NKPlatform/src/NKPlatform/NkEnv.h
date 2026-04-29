// =============================================================================
// NKPlatform/NkEnv.h
// Gestion portable des variables d'environnement.
//
// Design :
//  - Abstraction multiplateforme : Windows (SetEnvironmentVariable, GetEnvironmentVariable)
//    et POSIX (setenv, getenv, unsetenv).
//  - Gestion des erreurs via codes de retour (bool / chaîne vide).
//  - Support des chemins : séparateur système, ajout de répertoire au PATH.
//  - Aucune dépendance STL, types génériques pour paires et conteneurs.
//  - Export/import de symboles via NKENTSEU_PLATFORM_API pour DLL/statique.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKENV_H
#define NKENTSEU_PLATFORM_NKENV_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des en-têtes C standards pour les types de base et fonctions mémoire.
    // NkPlatformExport.h fournit NKENTSEU_PLATFORM_API pour l'export de symboles.
    // NkPlatformInline.h fournit NKENTSEU_API_INLINE pour les fonctions inline exportées.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    #include <cstddef>
    #include <cstring>
    #include <cstdlib>
    #include <new>

    // =========================================================================
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu::env.
    // Les symboles publics sont exportés via NKENTSEU_PLATFORM_API.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Le contenu de 'env' (dans 'nkentseu') est indenté de deux niveaux

    namespace nkentseu {

        namespace env {

            // =================================================================
            // SECTION 3 : TYPES DE BASE ET UTILITAIRES
            // =================================================================
            // Définitions de types génériques pour l'API d'environnement.

            /**
             * @brief Type entier non signé pour les tailles et indices
             * @typedef NkSize
             * @ingroup EnvTypes
             *
             * Alias portable pour unsigned int, utilisé uniformément dans l'API
             * pour les tailles de chaînes, indices de tableaux, etc.
             */
            using NkSize = unsigned int;

            // -----------------------------------------------------------------
            // Sous-section : Utilitaire de déplacement générique (move semantics)
            // -----------------------------------------------------------------

            /**
             * @brief Déplacement générique d'une valeur (équivalent à std::move)
             * @tparam T Type de la valeur à déplacer
             * @param t Référence vers la valeur à déplacer
             * @return Rvalue reference vers la valeur déplacée
             * @ingroup EnvUtilities
             *
             * Permet d'implémenter la sémantique de déplacement sans dépendre de <utility>.
             * Utilisé pour optimiser les transferts de ressources dans les conteneurs.
             *
             * @example
             * @code
             * NkEnvString str1("hello");
             * NkEnvString str2(NkMove(str1));  // str1 est vidé, str2 prend possession
             * @endcode
             */
            template <typename T>
            NKENTSEU_FORCE_INLINE constexpr T&& NkMove(T& t) noexcept {
                return static_cast<T&&>(t);
            }

            // =================================================================
            // SECTION 4 : CLASSE NKENVSTRING (CHAÎNE DYNAMIQUE SANS STL)
            // =================================================================
            // Implémentation d'une chaîne de caractères allouée dynamiquement,
            // sans dépendance à la STL, pour une portabilité maximale.

            /**
             * @brief Chaîne de caractères allouée dynamiquement (sans STL)
             * @class NkEnvString
             * @ingroup EnvTypes
             *
             * Fournit une gestion safe des chaînes C avec :
             *  - Allocation/désallocation automatique via malloc/free
             *  - Sémantique de copie et de déplacement (copy/move semantics)
             *  - Comparaisons et concaténation
             *  - Accès en lecture seule via CStr()
             *
             * @note Cette classe n'est pas thread-safe : la synchronisation externe
             * est requise pour un accès concurrent.
             *
             * @example
             * @code
             * NkEnvString name("PATH");
             * NkEnvString value("/usr/bin:/bin");
             *
             * if (name == NkEnvString("PATH")) {
             *     // Faire quelque chose avec value.CStr()
             * }
             * @endcode
             */
            class NKENTSEU_CLASS_EXPORT NkEnvString {
                public:
                    // -----------------------------------------------------------------
                    // Constructeurs et destructeur
                    // -----------------------------------------------------------------

                    /**
                     * @brief Constructeur par défaut (chaîne vide)
                     */
                    NkEnvString() noexcept;

                    /**
                     * @brief Constructeur depuis une chaîne C
                     * @param str Chaîne C null-terminée à copier
                     */
                    explicit NkEnvString(const char* str);

                    /**
                     * @brief Constructeur depuis un caractère unique
                     * @param ch Caractère à convertir en chaîne d'un caractère
                     */
                    explicit NkEnvString(char ch);

                    /**
                     * @brief Constructeur de copie
                     * @param other Chaîne source à copier
                     */
                    NkEnvString(const NkEnvString& other);

                    /**
                     * @brief Constructeur de déplacement
                     * @param other Chaîne source dont prendre possession
                     */
                    NkEnvString(NkEnvString&& other) noexcept;

                    /**
                     * @brief Destructeur (libère la mémoire allouée)
                     */
                    ~NkEnvString();

                    // -----------------------------------------------------------------
                    // Opérateurs d'affectation
                    // -----------------------------------------------------------------

                    /**
                     * @brief Opérateur d'affectation par copie
                     * @param other Chaîne source à copier
                     * @return Référence vers this pour chaînage
                     */
                    NkEnvString& operator=(const NkEnvString& other);

                    /**
                     * @brief Opérateur d'affectation par déplacement
                     * @param other Chaîne source dont prendre possession
                     * @return Référence vers this pour chaînage
                     */
                    NkEnvString& operator=(NkEnvString&& other) noexcept;

                    // -----------------------------------------------------------------
                    // Accesseurs (PascalCase pour cohérence API)
                    // -----------------------------------------------------------------

                    /**
                     * @brief Obtenir la chaîne C sous-jacente (null-terminée)
                     * @return Pointeur vers la chaîne C (jamais nullptr, retourne "" si vide)
                     */
                    const char* CStr() const noexcept;

                    /**
                     * @brief Obtenir la longueur de la chaîne (sans le null terminator)
                     * @return Nombre de caractères dans la chaîne
                     */
                    NkSize Size() const noexcept;

                    /**
                     * @brief Vérifier si la chaîne est vide
                     * @return true si la chaîne ne contient aucun caractère
                     */
                    bool Empty() const noexcept;

                    /**
                     * @brief Vider la chaîne et libérer la mémoire
                     */
                    void Clear() noexcept;

                    // -----------------------------------------------------------------
                    // Opérateurs de comparaison
                    // -----------------------------------------------------------------

                    /**
                     * @brief Comparaison d'égalité
                     * @param other Chaîne à comparer
                     * @return true si les deux chaînes ont le même contenu
                     */
                    bool operator==(const NkEnvString& other) const noexcept;

                    /**
                     * @brief Comparaison d'inégalité
                     * @param other Chaîne à comparer
                     * @return true si les deux chaînes ont un contenu différent
                     */
                    bool operator!=(const NkEnvString& other) const noexcept;

                    /**
                     * @brief Comparaison d'ordre lexicographique
                     * @param other Chaîne à comparer
                     * @return true si this < other selon l'ordre lexicographique
                     */
                    bool operator<(const NkEnvString& other) const noexcept;

                private:
                    /** @brief Pointeur vers le buffer de données (nullptr si vide) */
                    char* mData;

                    /** @brief Longueur de la chaîne (sans compter le null terminator) */
                    NkSize mLength;
            };

            /**
             * @brief Concaténation de deux NkEnvString
             * @param lhs Chaîne de gauche
             * @param rhs Chaîne de droite
             * @return Nouvelle chaîne contenant lhs + rhs
             * @ingroup EnvOperators
             */
            NKENTSEU_API_INLINE NkEnvString operator+(
                const NkEnvString& lhs,
                const NkEnvString& rhs
            );

            // =================================================================
            // SECTION 5 : STRUCTURE NKENVPAIR (PAIRE CLÉ/VALEUR GÉNÉRIQUE)
            // =================================================================

            /**
             * @brief Paire générique clé/valeur pour les conteneurs associatifs
             * @struct NkEnvPair
             * @tparam K Type de la clé
             * @tparam V Type de la valeur
             * @ingroup EnvTypes
             *
             * Structure simple pour stocker une association clé-valeur.
             * Supporte la construction par copie et par déplacement.
             *
             * @example
             * @code
             * NkEnvPair<NkEnvString, NkEnvString> pair(
             *     NkEnvString("KEY"),
             *     NkEnvString("VALUE")
             * );
             * @endcode
             */
            template <typename K, typename V>
            struct NKENTSEU_CLASS_EXPORT NkEnvPair {
                /** @brief Clé de la paire */
                K key;

                /** @brief Valeur associée à la clé */
                V value;

                /**
                 * @brief Constructeur par défaut
                 */
                NkEnvPair() = default;

                /**
                 * @brief Constructeur avec initialisation par copie
                 * @param k Clé à copier
                 * @param v Valeur à copier
                 */
                NkEnvPair(const K& k, const V& v) : key(k), value(v) {}

                /**
                 * @brief Constructeur avec initialisation par déplacement
                 * @param k Clé à déplacer
                 * @param v Valeur à déplacer
                 */
                NkEnvPair(K&& k, V&& v) : key(NkMove(k)), value(NkMove(v)) {}
            };

            // =================================================================
            // SECTION 6 : CLASSE NKENVVECTOR (CONTENEUR VECTORIEL GÉNÉRIQUE)
            // =================================================================

            /**
             * @brief Conteneur vectoriel dynamique générique (sans STL)
             * @class NkEnvVector
             * @tparam T Type des éléments stockés
             * @ingroup EnvContainers
             *
             * Implémentation minimaliste d'un vecteur dynamique avec :
             *  - Allocation/growth automatique (doublement de capacité)
             *  - Sémantique de copie et de déplacement
             *  - Accès par indice avec At() (sans bounds checking pour performance)
             *  - Itérateurs style C via Begin()/End()
             *
             * @note Ce conteneur n'est pas thread-safe.
             *
             * @example
             * @code
             * NkEnvVector<int> values;
             * values.PushBack(1);
             * values.PushBack(2);
             * for (NkSize i = 0; i < values.Size(); ++i) {
             *     printf("%d\n", values[i]);
             * }
             * @endcode
             */
            template <typename T>
            class NKENTSEU_CLASS_EXPORT NkEnvVector {
                public:
                    // -----------------------------------------------------------------
                    // Constructeurs et destructeur
                    // -----------------------------------------------------------------

                    /**
                     * @brief Constructeur par défaut (vecteur vide)
                     */
                    NkEnvVector() noexcept;

                    /**
                     * @brief Constructeur de copie
                     * @param other Vecteur source à copier
                     */
                    NkEnvVector(const NkEnvVector& other);

                    /**
                     * @brief Constructeur de déplacement
                     * @param other Vecteur source dont prendre possession
                     */
                    NkEnvVector(NkEnvVector&& other) noexcept;

                    /**
                     * @brief Destructeur (libère la mémoire et détruit les éléments)
                     */
                    ~NkEnvVector();

                    // -----------------------------------------------------------------
                    // Opérateurs d'affectation
                    // -----------------------------------------------------------------

                    /**
                     * @brief Opérateur d'affectation par copie
                     * @param other Vecteur source à copier
                     * @return Référence vers this pour chaînage
                     */
                    NkEnvVector& operator=(const NkEnvVector& other);

                    /**
                     * @brief Opérateur d'affectation par déplacement
                     * @param other Vecteur source dont prendre possession
                     * @return Référence vers this pour chaînage
                     */
                    NkEnvVector& operator=(NkEnvVector&& other) noexcept;

                    // -----------------------------------------------------------------
                    // Modification du conteneur
                    // -----------------------------------------------------------------

                    /**
                     * @brief Ajouter un élément par copie à la fin
                     * @param value Élément à ajouter
                     */
                    void PushBack(const T& value);

                    /**
                     * @brief Ajouter un élément par déplacement à la fin
                     * @param value Élément à déplacer
                     */
                    void PushBack(T&& value);

                    /**
                     * @brief Retirer le dernier élément (détruit l'élément)
                     */
                    void PopBack() noexcept;

                    /**
                     * @brief Vider le conteneur (détruit tous les éléments)
                     */
                    void Clear() noexcept;

                    /**
                     * @brief Réserver de la capacité pour éviter les réallocations
                     * @param capacity Nouvelle capacité minimale souhaitée
                     */
                    void Reserve(NkSize capacity);

                    // -----------------------------------------------------------------
                    // Accès aux éléments
                    // -----------------------------------------------------------------

                    /**
                     * @brief Accéder à un élément par indice (version mutable)
                     * @param index Indice de l'élément (0 <= index < Size())
                     * @return Référence vers l'élément
                     * @note Aucune vérification de bounds : comportement indéfini si hors limites
                     */
                    T& At(NkSize index);

                    /**
                     * @brief Accéder à un élément par indice (version const)
                     * @param index Indice de l'élément (0 <= index < Size())
                     * @return Référence const vers l'élément
                     */
                    const T& At(NkSize index) const;

                    /**
                     * @brief Opérateur d'accès par indice (version mutable)
                     * @param index Indice de l'élément
                     * @return Référence vers l'élément
                     */
                    T& operator[](NkSize index);

                    /**
                     * @brief Opérateur d'accès par indice (version const)
                     * @param index Indice de l'élément
                     * @return Référence const vers l'élément
                     */
                    const T& operator[](NkSize index) const;

                    // -----------------------------------------------------------------
                    // Informations de capacité
                    // -----------------------------------------------------------------

                    /**
                     * @brief Obtenir le nombre d'éléments stockés
                     * @return Nombre d'éléments utilisés
                     */
                    NkSize Size() const noexcept;

                    /**
                     * @brief Obtenir la capacité allouée
                     * @return Nombre maximal d'éléments pouvant être stockés sans réallocation
                     */
                    NkSize Capacity() const noexcept;

                    /**
                     * @brief Vérifier si le conteneur est vide
                     * @return true si Size() == 0
                     */
                    bool Empty() const noexcept;

                    // -----------------------------------------------------------------
                    // Itérateurs simplifiés (style C)
                    // -----------------------------------------------------------------

                    /**
                     * @brief Obtenir un pointeur vers le premier élément (mutable)
                     * @return Pointeur vers mData ou nullptr si vide
                     */
                    T* Begin() noexcept;

                    /**
                     * @brief Obtenir un pointeur vers le premier élément (const)
                     * @return Pointeur const vers mData ou nullptr si vide
                     */
                    const T* Begin() const noexcept;

                    /**
                     * @brief Obtenir un pointeur vers après le dernier élément (mutable)
                     * @return Pointeur vers mData + mSize
                     */
                    T* End() noexcept;

                    /**
                     * @brief Obtenir un pointeur vers après le dernier élément (const)
                     * @return Pointeur const vers mData + mSize
                     */
                    const T* End() const noexcept;

                private:
                    /** @brief Pointeur vers le tableau dynamique d'éléments */
                    T* mData;

                    /** @brief Nombre d'éléments actuellement stockés */
                    NkSize mSize;

                    /** @brief Capacité allouée du tableau */
                    NkSize mCapacity;

                    /**
                     * @brief Doubler la capacité (ou initialiser à 4 si vide)
                     * @note Appelée automatiquement par PushBack si nécessaire
                     */
                    void Grow();
            };

            // =================================================================
            // SECTION 7 : CLASSE NKENVUMAP (CONTENEUR ASSOCIATIF NON ORDONNÉ)
            // =================================================================

            /**
             * @brief Conteneur associatif non ordonné (recherche linéaire)
             * @class NkEnvUMap
             * @tparam K Type des clés (doit supporter operator==)
             * @tparam V Type des valeurs
             * @ingroup EnvContainers
             *
             * Implémentation simple d'une map non ordonnée avec :
             *  - Stockage interne via NkEnvVector<NkEnvPair<K, V>>
             *  - Recherche linéaire O(n) - adapté aux petits ensembles
             *  - Insertion, mise à jour, suppression de paires clé/valeur
             *
             * @note Pour de grands ensembles, envisager une implémentation avec hachage.
             *
             * @example
             * @code
             * NkEnvUMap<NkEnvString, NkEnvString> envVars;
             * envVars.Set(NkEnvString("PATH"), NkEnvString("/usr/bin"));
             *
             * const NkEnvString* value = envVars.Find(NkEnvString("PATH"));
             * if (value) {
             *     printf("PATH = %s\n", value->CStr());
             * }
             * @endcode
             */
            template <typename K, typename V>
            class NKENTSEU_CLASS_EXPORT NkEnvUMap {
                public:
                    // -----------------------------------------------------------------
                    // Constructeurs et destructeur (défaut)
                    // -----------------------------------------------------------------

                    /** @brief Constructeur par défaut */
                    NkEnvUMap() noexcept = default;

                    /** @brief Constructeur de copie */
                    NkEnvUMap(const NkEnvUMap& other) = default;

                    /** @brief Constructeur de déplacement */
                    NkEnvUMap(NkEnvUMap&& other) noexcept = default;

                    /** @brief Destructeur */
                    ~NkEnvUMap() = default;

                    // -----------------------------------------------------------------
                    // Opérateurs d'affectation (défaut)
                    // -----------------------------------------------------------------

                    /** @brief Opérateur d'affectation par copie */
                    NkEnvUMap& operator=(const NkEnvUMap& other) = default;

                    /** @brief Opérateur d'affectation par déplacement */
                    NkEnvUMap& operator=(NkEnvUMap&& other) noexcept = default;

                    // -----------------------------------------------------------------
                    // Interface publique
                    // -----------------------------------------------------------------

                    /**
                     * @brief Définir ou mettre à jour une paire clé/valeur (copie)
                     * @param key Clé à insérer/mettre à jour
                     * @param value Valeur associée
                     *
                     * Si la clé existe déjà, sa valeur est mise à jour.
                     * Sinon, une nouvelle paire est ajoutée.
                     */
                    void Set(const K& key, const V& value);

                    /**
                     * @brief Définir ou mettre à jour une paire clé/valeur (déplacement)
                     * @param key Clé à insérer/mettre à jour
                     * @param value Valeur associée
                     */
                    void Set(K&& key, V&& value);

                    /**
                     * @brief Rechercher une valeur par clé
                     * @param key Clé à rechercher
                     * @return Pointeur vers la valeur si trouvée, nullptr sinon
                     * @note Le pointeur retourné est valide tant que la map n'est pas modifiée
                     */
                    const V* Find(const K& key) const noexcept;

                    /**
                     * @brief Supprimer une paire par clé
                     * @param key Clé à supprimer
                     * @return true si la clé a été trouvée et supprimée, false sinon
                     */
                    bool Remove(const K& key) noexcept;

                    /**
                     * @brief Vider complètement la map
                     */
                    void Clear() noexcept;

                    /**
                     * @brief Obtenir le nombre de paires stockées
                     * @return Nombre d'éléments dans la map
                     */
                    NkSize Size() const noexcept;

                    /**
                     * @brief Obtenir un accès direct aux données internes (lecture seule)
                     * @return Pointeur vers le tableau interne de paires, ou nullptr si vide
                     * @note À utiliser avec précaution : pas de bounds checking
                     */
                    const NkEnvPair<K, V>* Data() const noexcept;

                private:
                    /** @brief Conteneur interne stockant les paires clé/valeur */
                    NkEnvVector<NkEnvPair<K, V>> mPairs;

                    /**
                     * @brief Rechercher l'indice d'une clé dans le vecteur interne
                     * @param key Clé à rechercher
                     * @return Indice si trouvé, -1 sinon
                     */
                    int FindIndex(const K& key) const noexcept;
            };

            // =================================================================
            // SECTION 8 : API PUBLIQUE DE GESTION DES VARIABLES D'ENVIRONNEMENT
            // =================================================================
            // Fonctions de haut niveau pour interagir avec l'environnement système.

            /**
             * @brief Récupérer la valeur d'une variable d'environnement
             * @param name Nom de la variable à lire
             * @param result [out] Référence vers la chaîne qui recevra la valeur
             * @return Référence vers result (pour chaînage) ou chaîne vide si non trouvée
             * @ingroup EnvAPI
             *
             * Si la variable n'existe pas, result est vidé et une chaîne vide est retournée.
             *
             * @example
             * @code
             * NkEnvString name("PATH");
             * NkEnvString value;
             * NkGet(name, value);
             * if (!value.Empty()) {
             *     printf("PATH = %s\n", value.CStr());
             * }
             * @endcode
             */
            NKENTSEU_API_INLINE NkEnvString NkGet(
                const NkEnvString& name,
                NkEnvString& result
            ) noexcept;

            /**
             * @brief Définir une variable d'environnement
             * @param name Nom de la variable
             * @param value Nouvelle valeur à assigner
             * @param overwrite Si false et que la variable existe, ne pas modifier
             * @return true si l'opération a réussi, false en cas d'erreur
             * @ingroup EnvAPI
             *
             * @note Sur Windows, les modifications ne persistent pas après la fin du processus.
             * Pour une persistance, utiliser les APIs système appropriées (Registry, etc.).
             */
            NKENTSEU_API_INLINE bool NkSet(
                const NkEnvString& name,
                const NkEnvString& value,
                bool overwrite = true
            ) noexcept;

            /**
             * @brief Supprimer une variable d'environnement
             * @param name Nom de la variable à supprimer
             * @return true si l'opération a réussi, false si la variable n'existait pas
             * @ingroup EnvAPI
             */
            NKENTSEU_API_INLINE bool NkUnset(const NkEnvString& name) noexcept;

            /**
             * @brief Vérifier l'existence d'une variable d'environnement
             * @param name Nom de la variable à tester
             * @return true si la variable existe, false sinon
             * @ingroup EnvAPI
             */
            NKENTSEU_API_INLINE bool NkExists(const NkEnvString& name) noexcept;

            /**
             * @brief Obtenir une copie de toutes les variables d'environnement
             * @return NkEnvUMap contenant toutes les paires clé/valeur de l'environnement
             * @ingroup EnvAPI
             *
             * @note Cette fonction peut être coûteuse : éviter de l'appeler fréquemment.
             * Les modifications ultérieures de l'environnement ne sont pas reflétées
             * dans la map retournée.
             */
            NKENTSEU_API_INLINE NkEnvUMap<NkEnvString, NkEnvString> NkGetAll() noexcept;

            /**
             * @brief Obtenir le séparateur de chemin spécifique à la plateforme
             * @return ';' sur Windows, ':' sur POSIX
             * @ingroup EnvUtilities
             *
             * Utilisé pour parser ou construire des variables de type PATH.
             *
             * @example
             * @code
             * char sep = NkGetPathSeparator();
             * // Windows: sep == ';', Linux/macOS: sep == ':'
             * @endcode
             */
            NKENTSEU_API_INLINE char NkGetPathSeparator() noexcept;

            /**
             * @brief Ajouter un répertoire au début d'une variable de chemin
             * @param directory Chemin du répertoire à ajouter
             * @param varName Nom de la variable (défaut: "PATH")
             * @return true si l'opération a réussi, false en cas d'erreur
             * @ingroup EnvPathUtils
             *
             * Le répertoire est préfixé avec le séparateur approprié.
             * Si la variable n'existe pas, elle est créée avec le répertoire seul.
             *
             * @example
             * @code
             * NkPrependToPath(NkEnvString("/opt/myapp/bin"));
             * // PATH devient "/opt/myapp/bin:<ancienne valeur>"
             * @endcode
             */
            NKENTSEU_API_INLINE bool NkPrependToPath(
                const NkEnvString& directory,
                const NkEnvString& varName = NkEnvString("PATH")
            );

            /**
             * @brief Ajouter un répertoire à la fin d'une variable de chemin
             * @param directory Chemin du répertoire à ajouter
             * @param varName Nom de la variable (défaut: "PATH")
             * @return true si l'opération a réussi, false en cas d'erreur
             * @ingroup EnvPathUtils
             *
             * Le répertoire est suffixé avec le séparateur approprié.
             * Si la variable n'existe pas, elle est créée avec le répertoire seul.
             */
            NKENTSEU_API_INLINE bool NkAppendToPath(
                const NkEnvString& directory,
                const NkEnvString& varName = NkEnvString("PATH")
            );

        } // namespace env

    } // namespace nkentseu

    // =========================================================================
    // SECTION 9 : IMPLÉMENTATIONS DES TEMPLATES (HEADER-ONLY)
    // =========================================================================
    // Les implémentations des templates doivent être dans le header
    // pour permettre l'instanciation dans les unités de traduction clientes.

    namespace nkentseu {

        namespace env {

            // -----------------------------------------------------------------
            // Implémentation de NkEnvVector<T>
            // -----------------------------------------------------------------

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>::NkEnvVector() noexcept
                : mData(nullptr), mSize(0), mCapacity(0) {
            }

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>::NkEnvVector(const NkEnvVector& other)
                : mData(nullptr), mSize(0), mCapacity(0) {
                Reserve(other.mSize);
                for (NkSize i = 0; i < other.mSize; ++i) {
                    PushBack(other.mData[i]);
                }
            }

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>::NkEnvVector(NkEnvVector&& other) noexcept
                : mData(other.mData), mSize(other.mSize), mCapacity(other.mCapacity) {
                other.mData = nullptr;
                other.mSize = 0;
                other.mCapacity = 0;
            }

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>::~NkEnvVector() {
                Clear();
                if (mData) {
                    std::free(mData);
                    mData = nullptr;
                }
            }

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>& NkEnvVector<T>::operator=(
                const NkEnvVector& other
            ) {
                if (this != &other) {
                    NkEnvVector temp(other);
                    *this = NkMove(temp);
                }
                return *this;
            }

            template <typename T>
            NKENTSEU_API_INLINE NkEnvVector<T>& NkEnvVector<T>::operator=(
                NkEnvVector&& other
            ) noexcept {
                if (this != &other) {
                    Clear();
                    if (mData) {
                        std::free(mData);
                    }
                    mData = other.mData;
                    mSize = other.mSize;
                    mCapacity = other.mCapacity;
                    other.mData = nullptr;
                    other.mSize = 0;
                    other.mCapacity = 0;
                }
                return *this;
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::PushBack(const T& value) {
                if (mSize >= mCapacity) {
                    Grow();
                }
                new (mData + mSize) T(value);
                ++mSize;
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::PushBack(T&& value) {
                if (mSize >= mCapacity) {
                    Grow();
                }
                new (mData + mSize) T(NkMove(value));
                ++mSize;
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::PopBack() noexcept {
                if (mSize > 0) {
                    mData[mSize - 1].~T();
                    --mSize;
                }
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::Clear() noexcept {
                for (NkSize i = 0; i < mSize; ++i) {
                    mData[i].~T();
                }
                mSize = 0;
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::Reserve(NkSize capacity) {
                if (capacity <= mCapacity) {
                    return;
                }
                T* newData = static_cast<T*>(
                    std::malloc(capacity * sizeof(T))
                );
                if (!newData) {
                    return;
                }
                for (NkSize i = 0; i < mSize; ++i) {
                    new (newData + i) T(NkMove(mData[i]));
                    mData[i].~T();
                }
                if (mData) {
                    std::free(mData);
                }
                mData = newData;
                mCapacity = capacity;
            }

            template <typename T>
            NKENTSEU_API_INLINE T& NkEnvVector<T>::At(NkSize index) {
                return mData[index];
            }

            template <typename T>
            NKENTSEU_API_INLINE const T& NkEnvVector<T>::At(NkSize index) const {
                return mData[index];
            }

            template <typename T>
            NKENTSEU_API_INLINE T& NkEnvVector<T>::operator[](NkSize index) {
                return At(index);
            }

            template <typename T>
            NKENTSEU_API_INLINE const T& NkEnvVector<T>::operator[](NkSize index) const {
                return At(index);
            }

            template <typename T>
            NKENTSEU_API_INLINE NkSize NkEnvVector<T>::Size() const noexcept {
                return mSize;
            }

            template <typename T>
            NKENTSEU_API_INLINE NkSize NkEnvVector<T>::Capacity() const noexcept {
                return mCapacity;
            }

            template <typename T>
            NKENTSEU_API_INLINE bool NkEnvVector<T>::Empty() const noexcept {
                return mSize == 0;
            }

            template <typename T>
            NKENTSEU_API_INLINE T* NkEnvVector<T>::Begin() noexcept {
                return mData;
            }

            template <typename T>
            NKENTSEU_API_INLINE const T* NkEnvVector<T>::Begin() const noexcept {
                return mData;
            }

            template <typename T>
            NKENTSEU_API_INLINE T* NkEnvVector<T>::End() noexcept {
                return mData + mSize;
            }

            template <typename T>
            NKENTSEU_API_INLINE const T* NkEnvVector<T>::End() const noexcept {
                return mData + mSize;
            }

            template <typename T>
            NKENTSEU_API_INLINE void NkEnvVector<T>::Grow() {
                NkSize newCap = (mCapacity == 0) ? 4 : mCapacity * 2;
                Reserve(newCap);
            }

            // -----------------------------------------------------------------
            // Implémentation de NkEnvUMap<K, V>
            // -----------------------------------------------------------------

            template <typename K, typename V>
            NKENTSEU_API_INLINE int NkEnvUMap<K, V>::FindIndex(
                const K& key
            ) const noexcept {
                for (NkSize i = 0; i < mPairs.Size(); ++i) {
                    if (mPairs[i].key == key) {
                        return static_cast<int>(i);
                    }
                }
                return -1;
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE void NkEnvUMap<K, V>::Set(
                const K& key,
                const V& value
            ) {
                int idx = FindIndex(key);
                if (idx >= 0) {
                    mPairs[static_cast<NkSize>(idx)].value = value;
                    return;
                }
                mPairs.PushBack(NkEnvPair<K, V>(key, value));
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE void NkEnvUMap<K, V>::Set(K&& key, V&& value) {
                int idx = FindIndex(key);
                if (idx >= 0) {
                    mPairs[static_cast<NkSize>(idx)].value = NkMove(value);
                    return;
                }
                mPairs.PushBack(NkEnvPair<K, V>(NkMove(key), NkMove(value)));
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE const V* NkEnvUMap<K, V>::Find(
                const K& key
            ) const noexcept {
                int idx = FindIndex(key);
                if (idx >= 0) {
                    return &(mPairs[static_cast<NkSize>(idx)].value);
                }
                return nullptr;
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE bool NkEnvUMap<K, V>::Remove(
                const K& key
            ) noexcept {
                int idx = FindIndex(key);
                if (idx < 0) {
                    return false;
                }
                NkSize uidx = static_cast<NkSize>(idx);
                mPairs[uidx].key.~K();
                mPairs[uidx].value.~V();
                for (NkSize i = uidx; i < mPairs.Size() - 1; ++i) {
                    mPairs[i] = NkMove(mPairs[i + 1]);
                }
                mPairs.PopBack();
                return true;
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE void NkEnvUMap<K, V>::Clear() noexcept {
                mPairs.Clear();
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE NkSize NkEnvUMap<K, V>::Size() const noexcept {
                return mPairs.Size();
            }

            template <typename K, typename V>
            NKENTSEU_API_INLINE const NkEnvPair<K, V>* NkEnvUMap<K, V>::Data() const noexcept {
                return mPairs.Begin();
            }

        } // namespace env

    } // namespace nkentseu

#endif // NKENTSEU_PLATFORM_NKENV_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKENV.H
// =============================================================================
// Ce fichier fournit une API portable pour manipuler les variables d'environnement
// sans dépendance à la STL, adaptée aux environnements embarqués et systèmes critiques.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture et écriture basique d'une variable d'environnement
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"
    #include <cstdio>

    int main() {
        using namespace nkentseu::env;

        // Définir une variable temporaire
        NkEnvString varName("MY_APP_VAR");
        NkEnvString varValue("Hello from NkEnv");
        NkSet(varName, varValue);

        // Lire la variable
        NkEnvString result;
        NkGet(varName, result);
        std::printf("MY_APP_VAR = %s\n", result.CStr());

        // Vérifier l'existence
        if (NkExists(varName)) {
            std::printf("Variable exists!\n");
        }

        // Supprimer la variable
        NkUnset(varName);
        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Modification du PATH pour ajouter des répertoires d'exécution
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"

    void SetupApplicationPath() {
        using namespace nkentseu::env;

        // Ajouter un répertoire personnalisé au début du PATH (priorité haute)
        NkPrependToPath(NkEnvString("/opt/myapp/bin"));

        // Ajouter un autre répertoire à la fin (priorité basse)
        NkAppendToPath(NkEnvString("/usr/local/myapp/tools"));

        // Vérifier le résultat
        NkEnvString pathValue;
        NkGet(NkEnvString("PATH"), pathValue);
        // pathValue contient maintenant le PATH mis à jour
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Lister toutes les variables d'environnement (debug)
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"
    #include <cstdio>

    void DumpEnvironment() {
        using namespace nkentseu::env;

        NkEnvUMap<NkEnvString, NkEnvString> allVars = NkGetAll();

        std::printf("=== Environment Variables (%u total) ===\n", allVars.Size());
        for (NkSize i = 0; i < allVars.Size(); ++i) {
            const auto& pair = allVars.Data()[i];
            std::printf("%s = %s\n", pair.key.CStr(), pair.value.CStr());
        }
        std::printf("=======================================\n");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion conditionnelle basée sur une variable d'environnement
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"

    bool InitializeWithConfig() {
        using namespace nkentseu::env;

        NkEnvString configPath;
        NkGet(NkEnvString("MYAPP_CONFIG"), configPath);

        if (configPath.Empty()) {
            // Fallback vers un chemin par défaut
            configPath = NkEnvString("/etc/myapp/config.ini");
        }

        // Charger la configuration depuis configPath.CStr()
        return LoadConfigFile(configPath.CStr());
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation des conteneurs génériques sans STL
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"

    void ProcessEnvVariables() {
        using namespace nkentseu::env;

        // Créer un vecteur de chaînes
        NkEnvVector<NkEnvString> searchPaths;
        searchPaths.PushBack(NkEnvString("/usr/bin"));
        searchPaths.PushBack(NkEnvString("/usr/local/bin"));
        searchPaths.PushBack(NkEnvString("~/bin"));

        // Itérer avec les itérateurs style C
        for (const NkEnvString* it = searchPaths.Begin(); it != searchPaths.End(); ++it) {
            std::printf("Searching in: %s\n", it->CStr());
        }

        // Créer une map pour un cache de valeurs
        NkEnvUMap<NkEnvString, NkEnvString> cache;
        cache.Set(NkEnvString("last_result"), NkEnvString("success"));

        const NkEnvString* cached = cache.Find(NkEnvString("last_result"));
        if (cached) {
            std::printf("Cached: %s\n", cached->CStr());
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Concaténation et manipulation de chaînes NkEnvString
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkEnv.h"

    NkEnvString BuildFullPath(const char* base, const char* subpath) {
        using namespace nkentseu::env;

        NkEnvString result(base);
        if (result.CStr()[result.Size() - 1] != '/' &&
            result.CStr()[result.Size() - 1] != '\\') {
            result = result + NkEnvString('/');
        }
        result = result + NkEnvString(subpath);
        return result;
    }

    // Utilisation :
    // NkEnvString fullPath = BuildFullPath("/home/user", "documents/file.txt");
    // => "/home/user/documents/file.txt"
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
    #include "NKPlatform/NkEnv.h"              // Gestion d'environnement

    void PlatformAwareInit() {
        using namespace nkentseu::env;

        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Sur Windows, utiliser des chemins avec backslashes
            NkPrependToPath(NkEnvString("C:\\MyApp\\bin"));
        #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
            // Sur Unix-like, utiliser des chemins POSIX
            NkPrependToPath(NkEnvString("/opt/myapp/bin"));
        #endif

        // Vérifier une variable de debug multiplateforme
        NkEnvString debugFlag;
        NkGet(NkEnvString("MYAPP_DEBUG"), debugFlag);
        if (debugFlag == NkEnvString("1") || debugFlag == NkEnvString("true")) {
            EnableDebugMode();
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================