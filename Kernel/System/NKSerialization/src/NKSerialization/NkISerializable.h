#pragma once
// =============================================================================
// NKSerialization/NkISerializable.h — v3 (interface unifiée, STL-free)
// =============================================================================
/**
 * @file NkISerializable.h
 * @brief Interface de base et registry pour la sérialisation/désérialisation.
 * 
 * Ce fichier définit l'infrastructure pour :
 * - Déclarer des types sérialisables via l'interface NkISerializable
 * - Enregistrer des factories de création via NkSerializableRegistry
 * - Créer dynamiquement des instances par nom de type (polymorphisme)
 * - Sérialiser/désérialiser via NkArchive (binaire, JSON, texte, etc.)
 * 
 * 🔹 PHILOSOPHIE STL-FREE :
 *    • Aucun usage de std::unique_ptr, std::function, ou <memory>
 *    • Gestion mémoire explicite via raw pointers + documentation d'ownership
 *    • Compatibilité avec NkOwned<T> ou autres wrappers maison si nécessaire
 * 
 * 🔹 GESTION MÉMOIRE — CONTRAT CLAIR :
 *    • NkSerializableRegistry::Create() retourne un raw pointer via new
 *    • L'APPELANT EST RESPONSABLE du delete — documenté explicitement
 *    • Alternative : utiliser NkOwned<NkISerializable> pour RAII automatique
 * 
 * 🔹 THREAD-SAFETY :
 *    • Le registry global est thread-safe en lecture après initialisation
 *    • L'enregistrement (Register) doit être fait avant tout accès concurrent
 *    • Typiquement : enregistrer tous les types au démarrage, dans un seul thread
 * 
 * @author nkentseu
 * @version 3.0
 * @date 2026
 */

#pragma once
// =============================================================================
// NKSerialization/NkISerializable.h — v3 (interface unifiée, STL-free)
// =============================================================================
/**
 * @file NkISerializable.h
 * @brief Interface de base et registry pour la sérialisation/désérialisation.
 * 
 * Ce fichier définit l'infrastructure pour :
 * - Déclarer des types sérialisables via l'interface NkISerializable
 * - Enregistrer des factories de création via NkSerializableRegistry
 * - Créer dynamiquement des instances par nom de type (polymorphisme)
 * - Sérialiser/désérialiser via NkArchive (binaire, JSON, texte, etc.)
 * 
 * 🔹 PHILOSOPHIE STL-FREE :
 *    • Aucun usage de std::unique_ptr, std::function, ou <memory>
 *    • Gestion mémoire explicite via raw pointers + documentation d'ownership
 *    • Compatibilité avec NkOwned<T> ou autres wrappers maison si nécessaire
 * 
 * 🔹 GESTION MÉMOIRE — CONTRAT CLAIR :
 *    • NkSerializableRegistry::Create() retourne un raw pointer via new
 *    • L'APPELANT EST RESPONSABLE du delete — documenté explicitement
 *    • Alternative : utiliser NkOwned<NkISerializable> pour RAII automatique
 * 
 * 🔹 THREAD-SAFETY :
 *    • Le registry global est thread-safe en lecture après initialisation
 *    • L'enregistrement (Register) doit être fait avant tout accès concurrent
 *    • Typiquement : enregistrer tous les types au démarrage, dans un seul thread
 * 
 * @author nkentseu
 * @version 3.0
 * @date 2026
 */

#ifndef NKENTSEU_SERIALIZATION_NKISERIALIZABLE_H
    #define NKENTSEU_SERIALIZATION_NKISERIALIZABLE_H

    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include <cstring>

    namespace nkentseu {

        // =====================================================================
        // 🎭 NkISerializable — Interface de base pour types sérialisables
        // =====================================================================
        /**
         * @class NkISerializable
         * @brief Interface polymorphique pour la sérialisation/désérialisation.
         * 
         * 🔹 Rôle :
         *    • Définir un contrat commun pour tous les types sérialisables
         *    • Permettre la sérialisation polymorphique via pointeur de base
         *    • Fournir le nom du type pour l'enregistrement dans le registry
         * 
         * 🔹 Méthodes virtuelles pures :
         *    • Serialize() : écrit l'état de l'objet dans un NkArchive
         *    • Deserialize() : lit l'état depuis un NkArchive vers l'objet
         *    • GetTypeName() : retourne le nom du type pour lookup dynamique
         * 
         * 🔹 Contrat d'implémentation :
         *    • Serialize/Deserialize doivent être symétriques (même ordre de champs)
         *    • GetTypeName() doit retourner une string statique valide à vie
         *    • Toutes les méthodes doivent être noexcept si possible
         * 
         * @warning Le destructeur est virtuel et noexcept — essentiel pour
         *          la suppression polymorphique correcte via pointeur de base.
         * 
         * @example
         * @code
         * class PlayerData : public NkISerializable {
         * public:
         *     nk_uint32 level = 1;
         *     nk_float32 health = 100.f;
         *     char name[64] = {};
         * 
         *     nk_bool Serialize(NkArchive& archive) const override {
         *         return archive.Write(level)
         *             && archive.Write(health)
         *             && archive.WriteString(name);
         *     }
         * 
         *     nk_bool Deserialize(const NkArchive& archive) override {
         *         return archive.Read(level)
         *             && archive.Read(health)
         *             && archive.ReadString(name, sizeof(name));
         *     }
         * 
         *     const char* GetTypeName() const noexcept override {
         *         return "PlayerData";
         *     }
         * };
         * 
         * // Enregistrement automatique via macro (dans un .cpp) :
         * // NK_REGISTER_SERIALIZABLE(PlayerData);
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkISerializable {
            public:
                /// Destructeur virtuel noexcept pour suppression polymorphique sûre
                virtual ~NkISerializable() noexcept;

                /**
                 * @brief Sérialise l'état de l'objet dans un archive.
                 * @param archive Archive de destination (binaire, JSON, texte, etc.).
                 * @return true si la sérialisation a réussi, false en cas d'erreur.
                 * 
                 * 🔹 Contrat :
                 *    • Doit écrire les champs dans un ordre déterministe
                 *    • Doit gérer les erreurs d'écriture (retourner false)
                 *    • Ne doit pas modifier l'état de l'objet (const method)
                 * 
                 * @note L'implémentation typique chaîne les appels Write() :
                 *       return archive.Write(field1) && archive.Write(field2);
                 */
                [[nodiscard]] virtual nk_bool Serialize(NkArchive& archive) const = 0;

                /**
                 * @brief Désérialise l'état de l'objet depuis un archive.
                 * @param archive Archive source contenant les données.
                 * @return true si la désérialisation a réussi, false en cas d'erreur.
                 * 
                 * 🔹 Contrat :
                 *    • Doit lire les champs dans le MÊME ordre que Serialize()
                 *    • Doit initialiser l'objet à un état valide même en cas d'erreur partielle
                 *    • Peut modifier l'état de l'objet (non-const method)
                 * 
                 * @warning L'ordre de lecture DOIT correspondre exactement à l'ordre
                 *          d'écriture dans Serialize() — sinon corruption de données.
                 */
                [[nodiscard]] virtual nk_bool Deserialize(const NkArchive& archive) = 0;

                /**
                 * @brief Retourne le nom du type pour identification dans le registry.
                 * @return String C statique, valide à vie, unique par type.
                 * 
                 * 🔹 Exigences :
                 *    • Retourner toujours la même valeur pour une instance donnée
                 *    • La string doit être statique ou avoir une durée de vie 'static'
                 *    • Doit correspondre exactement au nom utilisé dans Register<T>()
                 * 
                 * @note Typiquement implémenté comme : return "NomDuType";
                 *       La macro NK_REGISTER_SERIALIZABLE utilise #Type automatiquement.
                 */
                [[nodiscard]] virtual const char* GetTypeName() const noexcept = 0;
        };

        // =====================================================================
        // 🗄️ NkSerializableRegistry — Registry global pour factories de types
        // =====================================================================
        /**
         * @class NkSerializableRegistry
         * @brief Registry singleton pour l'enregistrement et la création dynamique de types.
         * 
         * 🔹 Responsabilités :
         *    • Enregistrer des types sérialisables par nom via Register<T>()
         *    • Créer des instances dynamiques par nom via Create(typeName)
         *    • Vérifier l'enregistrement d'un type via IsRegistered(typeName)
         *    • Désenregistrer un type via Unregister(typeName) (rare)
         * 
         * 🔹 Pattern Singleton thread-safe :
         *    • Global() retourne une référence à une variable static locale
         *    • Garantie d'initialisation thread-safe en C++11+ (Meyer's singleton)
         *    • Accès en lecture concurrente safe après initialisation complète
         * 
         * 🔹 Gestion mémoire — CONTRAT EXPLICITE :
         *    • Create() alloue via new et retourne un raw pointer
         *    • L'APPELANT EST RESPONSABLE du delete — documentation claire
         *    • Alternative recommandée : NkOwned<NkISerializable> pour RAII
         * 
         * @warning Ce registry n'est PAS thread-safe pour l'enregistrement (Register).
         *          Tous les Register<T>() doivent être appelés avant tout accès
         *          concurrent, typiquement au démarrage dans un seul thread.
         * 
         * @example
         * @code
         * // Enregistrement au démarrage (dans un .cpp ou module init) :
         * NkSerializableRegistry::Register<PlayerData>("PlayerData");
         * // Ou via macro : NK_REGISTER_SERIALIZABLE(PlayerData);
         * 
         * // Création dynamique plus tard :
         * auto* obj = NkSerializableRegistry::Global().Create("PlayerData");
         * if (obj) {
         *     // obj est un PlayerData* mais typé NkISerializable*
         *     // L'utiliser, puis :
         *     delete obj;  // ✅ Responsibility of caller
         * }
         * 
         * // Alternative RAII avec NkOwned (si disponible) :
         * // auto owned = NkOwned<NkISerializable>(
         * //     NkSerializableRegistry::Global().Create("PlayerData")
         * // );  // delete automatique à la destruction
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkSerializableRegistry {
            public:
                /**
                 * @typedef FactoryFn
                 * @brief Signature des fonctions factory pour création d'instances.
                 * 
                 * 🔹 Signature : NkISerializable* (*)()
                 *    • Retourne un raw pointer via new
                 *    • Aucun paramètre — construction par défaut requise
                 *    • L'appelant prend ownership et doit delete
                 * 
                 * @note Utilisé en interne par Register<T>() et RegisterFactory().
                 *       Les utilisateurs normaux n'ont pas besoin de manipuler
                 *       ce type directement.
                 */
                using FactoryFn = NkISerializable*(*)();

                /**
                 * @struct Entry
                 * @brief Entrée de registre : nom du type + factory associée.
                 * 
                 * 🔹 Champs :
                 *    • name[128] : buffer fixe pour le nom du type (null-terminated)
                 *                 — limite à 127 chars + null terminator
                 *    • factory : pointeur vers la fonction de création
                 * 
                 * @note Le buffer fixe évite les allocations dynamiques dans le registry,
                 *       cohérent avec la philosophie STL-free et performance-critical.
                 */
                struct Entry {
                    /// Nom du type (buffer fixe, null-terminated, max 127 chars)
                    char name[128] = {};

                    /// Factory pour créer une instance du type
                    FactoryFn factory = nullptr;
                };

                /**
                 * @brief Accès au singleton global du registry.
                 * @return Référence à l'instance unique de NkSerializableRegistry.
                 * 
                 * 🔹 Thread-safety :
                 *    • Initialisation thread-safe garantie par C++11 (Meyer's singleton)
                 *    • Accès en lecture concurrente safe après première utilisation
                 *    • L'enregistrement (Register) doit être fait avant accès concurrent
                 * 
                 * @note Pattern classique de singleton sans overhead de mutex en lecture.
                 */
                [[nodiscard]] static NkSerializableRegistry& Global() noexcept;

                /**
                 * @brief Enregistre un type T dans le registry sous un nom donné.
                 * @tparam T Type à enregistrer (doit dériver de NkISerializable).
                 * @param typeName Nom unique pour lookup via Create()/IsRegistered().
                 * 
                 * 🔹 Comportement :
                 *    • Si typeName existe déjà : met à jour la factory (override)
                 *    • Sinon : ajoute une nouvelle entrée dans le registre
                 *    • La factory créée appelle new T() — T doit avoir un ctor par défaut
                 * 
                 * @warning ⚠️ Thread-safety : Register() n'est PAS thread-safe.
                 *          Tous les enregistrements doivent être faits avant
                 *          tout accès concurrent, typiquement au démarrage.
                 * 
                 * @note Préférer la macro NK_REGISTER_SERIALIZABLE(T) pour éviter
                 *       les erreurs de nommage (#Type vs "Type").
                 * 
                 * @example
                 * @code
                 * // Enregistrement explicite (dans un .cpp) :
                 * NkSerializableRegistry::Register<PlayerData>("PlayerData");
                 * 
                 * // Ou via macro (recommandé) :
                 * // NK_REGISTER_SERIALIZABLE(PlayerData);  // Dans un .cpp
                 * @endcode
                 */
                template<typename T>
                static void Register(const char* typeName) noexcept;

                /**
                 * @brief Enregistre une factory personnalisée pour un type.
                 * @param typeName Nom unique pour lookup via Create()/IsRegistered().
                 * @param factory Fonction factory retournant NkISerializable* via new.
                 * 
                 * 🔹 Quand utiliser RegisterFactory() au lieu de Register<T>() ?
                 *    • Quand le type n'a pas de constructeur par défaut
                 *    • Quand la création nécessite des paramètres ou logique custom
                 *    • Quand on veut injecter des dépendances à la création
                 * 
                 * @warning La factory doit :
                 *    • Retourner un pointeur valide via new (pas nullptr sauf erreur)
                 *    • Ne pas lever d'exceptions (ou les catcher en interne)
                 *    • Retourner un objet dont le type dynamique correspond au nom
                 * 
                 * @example
                 * @code
                 * // Factory custom pour un type avec paramètres :
                 * auto factory = []() -> NkISerializable* {
                 *     return new ComplexType(/ *param=* /42, / *config=* /"default");
                 * };
                 * NkSerializableRegistry::RegisterFactory("ComplexType", factory);
                 * @endcode
                 */
                static void RegisterFactory(const char* typeName, FactoryFn factory) noexcept;

                /**
                 * @brief Désenregistre un type du registry par son nom.
                 * @param typeName Nom du type à retirer.
                 * 
                 * 🔹 Comportement :
                 *    • Recherche linéaire par nom dans mEntries
                 *    • Suppression de l'entrée via EraseAt() si trouvée
                 *    • No-op silencieux si le nom n'est pas trouvé
                 * 
                 * @warning ⚠️ Thread-safety : Unregister() n'est PAS thread-safe.
                 *          À utiliser uniquement pendant l'initialisation ou
                 *          dans un contexte mono-thread contrôlé.
                 * 
                 * @note Rarement nécessaire — préférer garder les types enregistrés
                 *       pour toute la durée de vie de l'application.
                 */
                static void Unregister(const char* typeName) noexcept;

                /**
                 * @brief Crée une instance dynamique d'un type enregistré.
                 * @param typeName Nom du type à instancier (doit être enregistré).
                 * @return Pointeur vers NkISerializable nouvellement alloué, ou nullptr.
                 * 
                 * 🔹 Contrat de gestion mémoire — LIRE ATTENTIVEMENT :
                 *    • La méthode alloue via new dans la factory enregistrée
                 *    • L'APPELANT EST RESPONSABLE du delete sur le pointeur retourné
                 *    • En cas d'erreur (type non trouvé, factory nullptr) : retourne nullptr
                 * 
                 * 🔹 Alternative RAII recommandée :
                 *    • Utiliser NkOwned<NkISerializable> si disponible dans votre codebase
                 *    • Ou wrapper manuel avec RAII local si la durée de vie est courte
                 * 
                 * @warning Ne pas stocker le pointeur brut au-delà de la scope courante
                 *          sans documentation claire de l'ownership transfer.
                 * 
                 * @example
                 * @code
                 * // Usage basique avec delete manuel :
                 * auto* obj = NkSerializableRegistry::Global().Create("PlayerData");
                 * if (obj) {
                 *     // Utiliser obj...
                 *     delete obj;  // ✅ Responsibility of caller
                 * }
                 * 
                 * // Usage RAII avec NkOwned (si disponible) :
                 * auto owned = NkOwned<NkISerializable>(
                 *     NkSerializableRegistry::Global().Create("PlayerData")
                 * );  // delete automatique à la destruction de 'owned'
                 * 
                 * // Usage avec cast typé après vérification :
                 * auto* base = NkSerializableRegistry::Global().Create("PlayerData");
                 * if (auto* player = dynamic_cast<PlayerData*>(base)) {
                 *     player->level = 5;  // Accès aux méthodes spécifiques
                 *     delete base;  // Toujours delete via le type de base
                 * }
                 * @endcode
                 */
                [[nodiscard]] NkISerializable* Create(const char* typeName) const noexcept;

                /**
                 * @brief Vérifie si un type est enregistré dans le registry.
                 * @param typeName Nom du type à tester.
                 * @return true si le type est enregistré avec une factory valide.
                 * 
                 * @note Complexité O(n) où n = nombre de types enregistrés.
                 *       Typiquement n < 100 → impact négligeable.
                 *       Pour des milliers de types, envisager un hash map.
                 * 
                 * @example
                 * @code
                 * if (NkSerializableRegistry::Global().IsRegistered("PlayerData")) {
                 *     auto* obj = NkSerializableRegistry::Global().Create("PlayerData");
                 *     // ... utiliser obj ...
                 *     delete obj;
                 * } else {
                 *     logger.Error("Type 'PlayerData' non enregistré\n");
                 * }
                 * @endcode
                 */
                [[nodiscard]] bool IsRegistered(const char* typeName) const noexcept;

            private:
                /// Registre interne des types : vecteur d'entrées nom+factory
                NkVector<Entry> mEntries;
        };

    } // namespace nkentseu

    // =====================================================================
    // 🎯 Macro d'enregistrement automatique de types sérialisables
    // =====================================================================
    /**
     * @def NK_REGISTER_SERIALIZABLE(Type)
     * @brief Macro pour l'enregistrement automatique d'un type au chargement du module.
     * 
     * 🔹 Usage :
     *    • Placer dans un fichier .cpp (jamais dans un header)
     *    • Après la définition complète du type Type
     *    • Type doit dériver de NkISerializable et avoir un ctor par défaut
     * 
     * 🔹 Mécanisme :
     *    • Crée un objet static avec un constructeur qui appelle Register<Type>()
     *    • L'objet est construit avant main() (initialisation statique)
     *    • Le nom utilisé est #Type (stringification du token Type)
     * 
     * 🔹 Avantages :
     *    • Évite les erreurs de nommage ("Type" vs #Type)
     *    • Garantit l'enregistrement même si le type n'est pas utilisé ailleurs
     *    • Centralise l'enregistrement près de la définition du type
     * 
     * @warning ⚠️ À placer UNIQUEMENT dans un .cpp, jamais dans un header !
     *          Sinon : définitions multiples, ODR violation, comportement indéfini.
     * 
     * @example
     * @code
     * // Dans PlayerData.cpp (après la définition de la classe) :
     * #include "MyGame/Data/PlayerData.h"
     * #include "NKSerialization/NkISerializable.h"
     * 
     * // ... implémentation des méthodes de PlayerData ...
     * 
     * // Enregistrement automatique — une seule fois, dans ce .cpp :
     * NK_REGISTER_SERIALIZABLE(PlayerData);
     * @endcode
     */
    #define NK_REGISTER_SERIALIZABLE(Type) \
        static struct _NkSerializableAutoReg_##Type { \
            _NkSerializableAutoReg_##Type() noexcept { \
                nkentseu::NkSerializableRegistry::Register<Type>(#Type); \
            } \
        } _nk_serializable_autoreg_##Type

    // =====================================================================
    // 📚 EXEMPLES D'UTILISATION COMPLETS ET DÉTAILLÉS
    // =====================================================================
    /**
     * @addtogroup NkISerializable_Examples
     * @{
     */

    /**
     * @example 01_basic_serialization.cpp
     * @brief Définition et usage basique d'un type sérialisable.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * #include "NKSerialization/NkArchive.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Définition d'un type sérialisable simple ──────────────────────
     * class PlayerStats : public NkISerializable {
     * public:
     *     nk_uint32 level = 1;
     *     nk_float32 health = 100.f;
     *     nk_float32 mana = 50.f;
     *     char playerName[64] = {};
     * 
     *     // Constructeur par défaut requis pour l'enregistrement
     *     PlayerStats() noexcept = default;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // Chaînage des Write() — retour false si une écriture échoue
     *         return archive.Write(level)
     *             && archive.Write(health)
     *             && archive.Write(mana)
     *             && archive.WriteString(playerName, sizeof(playerName));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // Lecture dans le MÊME ordre que Serialize() — crucial !
     *         return archive.Read(level)
     *             && archive.Read(health)
     *             && archive.Read(mana)
     *             && archive.ReadString(playerName, sizeof(playerName));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "PlayerStats";  // Doit correspondre au nom d'enregistrement
     *     }
     * };
     * 
     * // ── Enregistrement automatique (dans un .cpp, après la définition) ─
     * // NK_REGISTER_SERIALIZABLE(PlayerStats);
     * 
     * // ── Usage : sérialisation dans un fichier binaire ─────────────────
     * void SavePlayerStats(const PlayerStats& stats, const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForWrite(filename)) {
     *         logger.Error("Impossible d'ouvrir %s en écriture\n", filename);
     *         return;
     *     }
     * 
     *     // Écriture du type pour désérialisation polymorphique future
     *     archive.WriteString(stats.GetTypeName());
     * 
     *     // Sérialisation du contenu via l'interface de base
     *     if (!stats.Serialize(archive)) {
     *         logger.Error("Échec de la sérialisation de PlayerStats\n");
     *         return;
     *     }
     * 
     *     archive.Close();
     *     logger.Info("Stats joueur sauvegardées dans %s\n", filename);
     * }
     * 
     * // ── Usage : désérialisation polymorphique depuis un fichier ──────
     * NkISerializable* LoadSerializedObject(const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForRead(filename)) {
     *         logger.Error("Impossible d'ouvrir %s en lecture\n", filename);
     *         return nullptr;
     *     }
     * 
     *     // Lecture du nom du type pour création dynamique
     *     char typeName[128] = {};
     *     if (!archive.ReadString(typeName, sizeof(typeName))) {
     *         logger.Error("Échec de lecture du nom de type\n");
     *         return nullptr;
     *     }
     * 
     *     // Création via le registry — caller owns le résultat
     *     auto* obj = NkSerializableRegistry::Global().Create(typeName);
     *     if (!obj) {
     *         logger.Error("Type '%s' non enregistré ou non créable\n", typeName);
     *         return nullptr;
     *     }
     * 
     *     // Désérialisation du contenu
     *     if (!obj->Deserialize(archive)) {
     *         logger.Error("Échec de la désérialisation de %s\n", typeName);
     *         delete obj;  // ✅ Cleanup en cas d'erreur
     *         return nullptr;
     *     }
     * 
     *     archive.Close();
     *     return obj;  // ✅ Ownership transfer to caller — MUST delete later
     * }
     * 
     * // ── Usage complet avec gestion RAII via NkOwned (si disponible) ─
     * void Example_WithNkOwned() {
     *     // Sauvegarde
     *     PlayerStats stats;
     *     stats.level = 10;
     *     stats.health = 75.f;
     *     std::strncpy(stats.playerName, "Hero", 63);
     *     SavePlayerStats(stats, "save.dat");
     * 
     *     // Chargement avec RAII via NkOwned
     *     if (auto* raw = LoadSerializedObject("save.dat")) {
     *         // Wrapper RAII pour delete automatique
     *         auto owned = NkOwned<NkISerializable>(raw);
     * 
     *         // Cast typé pour accès aux méthodes spécifiques
     *         if (auto* playerStats = dynamic_cast<PlayerStats*>(owned.Get())) {
     *             logger.Info("Joueur chargé : %s, niveau %u, PV %.1f\n",
     *                         playerStats->playerName,
     *                         playerStats->level,
     *                         playerStats->health);
     *         }
     *         // delete automatique à la fin du scope grâce à NkOwned
     *     }
     * }
     * @endcode
     */

    /**
     * @example 02_polymorphic_serialization.cpp
     * @brief Sérialisation polymorphique d'une hiérarchie de types.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Hiérarchie de classes sérialisables ──────────────────────────
     * class GameEntity : public NkISerializable {
     * public:
     *     nk_uint32 entityId = 0;
     *     char name[64] = {};
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return archive.Write(entityId)
     *             && archive.WriteString(name, sizeof(name));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return archive.Read(entityId)
     *             && archive.ReadString(name, sizeof(name));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "GameEntity";
     *     }
     * };
     * 
     * class Enemy : public GameEntity {
     * public:
     *     nk_float32 damage = 10.f;
     *     nk_float32 armor = 5.f;
     *     nk_uint32 patrolRadius = 50;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // D'abord sérialiser la base, puis les champs dérivés
     *         return GameEntity::Serialize(archive)
     *             && archive.Write(damage)
     *             && archive.Write(armor)
     *             && archive.Write(patrolRadius);
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // D'abord désérialiser la base, puis les champs dérivés
     *         return GameEntity::Deserialize(archive)
     *             && archive.Read(damage)
     *             && archive.Read(armor)
     *             && archive.Read(patrolRadius);
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "Enemy";  // Nom unique pour le registry
     *     }
     * };
     * 
     * class NPC : public GameEntity {
     * public:
     *     nk_int32 dialogueTreeId = -1;
     *     nk_bool isMerchant = false;
     *     nk_float32 goldAmount = 0.f;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return GameEntity::Serialize(archive)
     *             && archive.Write(dialogueTreeId)
     *             && archive.Write(isMerchant)
     *             && archive.Write(goldAmount);
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return GameEntity::Deserialize(archive)
     *             && archive.Read(dialogueTreeId)
     *             && archive.Read(isMerchant)
     *             && archive.Read(goldAmount);
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "NPC";
     *     }
     * };
     * 
     * // ── Enregistrement des types dérivés (dans des .cpp respectifs) ──
     * // Enemy.cpp : NK_REGISTER_SERIALIZABLE(Enemy);
     * // NPC.cpp   : NK_REGISTER_SERIALIZABLE(NPC);
     * // Note : GameEntity n'a pas besoin d'être enregistré si on ne crée
     * //        jamais d'instances polymorphiques de la base seule.
     * 
     * // ── Sérialisation d'un conteneur polymorphique ───────────────────
     * bool SaveEntityList(const NkVector<GameEntity*>& entities,
     *                     const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForWrite(filename)) {
     *         return false;
     *     }
     * 
     *     // Écriture du nombre d'entités
     *     nk_uint32 count = static_cast<nk_uint32>(entities.Size());
     *     if (!archive.Write(count)) {
     *         return false;
     *     }
     * 
     *     // Sérialisation de chaque entité avec son type
     *     for (const auto* entity : entities) {
     *         if (!entity) {
     *             continue;  // Skip les pointeurs nuls
     *         }
     * 
     *         // Écriture du nom du type pour désérialisation polymorphique
     *         if (!archive.WriteString(entity->GetTypeName())) {
     *             return false;
     *         }
     * 
     *         // Sérialisation du contenu via l'interface de base
     *         if (!entity->Serialize(archive)) {
     *             return false;
     *         }
     *     }
     * 
     *     return archive.Close();
     * }
     * 
     * // ── Désérialisation polymorphique avec cleanup RAII ──────────────
     * NkVector<NkOwned<GameEntity>> LoadEntityList(const char* filename) {
     *     NkVector<NkOwned<GameEntity>> result;
     * 
     *     NkArchive archive;
     *     if (!archive.OpenForRead(filename)) {
     *         return result;  // Vecteur vide en cas d'erreur
     *     }
     * 
     *     // Lecture du nombre d'entités
     *     nk_uint32 count = 0;
     *     if (!archive.Read(count)) {
     *         return result;
     *     }
     * 
     *     // Désérialisation de chaque entité
     *     for (nk_uint32 i = 0; i < count; ++i) {
     *         // Lecture du nom du type
     *         char typeName[128] = {};
     *         if (!archive.ReadString(typeName, sizeof(typeName))) {
     *             break;  // Erreur de lecture — arrêt propre
     *         }
     * 
     *         // Création dynamique via le registry
     *         auto* raw = NkSerializableRegistry::Global().Create(typeName);
     *         if (!raw) {
     *             logger.Warning("Type '%s' non enregistré — skip entité %u\n",
     *                            typeName, i);
     *             continue;
     *         }
     * 
     *         // Désérialisation du contenu
     *         if (!raw->Deserialize(archive)) {
     *             logger.Warning("Échec désérialisation entité %u (%s)\n", i, typeName);
     *             delete raw;  // ✅ Cleanup manuel car pas encore dans NkOwned
     *             continue;
     *         }
     * 
     *         // Cast vers GameEntity* (sûr car tous les types dérivent de GameEntity)
     *         auto* entity = static_cast<GameEntity*>(raw);
     * 
     *         // Transfert dans NkOwned pour gestion RAII automatique
     *         result.PushBack(NkOwned<GameEntity>(entity));
     *     }
     * 
     *     archive.Close();
     *     return result;  // NkOwned gère le delete automatique à la destruction
     * }
     * 
     * // ── Usage : chargement et utilisation typée ──────────────────────
     * void Example_PolymorphicUsage() {
     *     auto entities = LoadEntityList("entities.dat");
     * 
     *     for (const auto& owned : entities) {
     *         GameEntity* base = owned.Get();
     *         if (!base) {
     *             continue;
     *         }
     * 
     *         logger.Info("Entité: %s (ID: %u)\n", base->name, base->entityId);
     * 
     *         // Dispatch typé via dynamic_cast pour logique spécifique
     *         if (auto* enemy = dynamic_cast<Enemy*>(base)) {
     *             logger.Info("  → Ennemi: dégâts=%.1f, armure=%.1f\n",
     *                         enemy->damage, enemy->armor);
     *         }
     *         else if (auto* npc = dynamic_cast<NPC*>(base)) {
     *             logger.Info("  → PNJ: marchand=%s, or=%.1f\n",
     *                         npc->isMerchant ? "oui" : "non",
     *                         npc->goldAmount);
     *         }
     *     }
     *     // ✅ Cleanup automatique via NkOwned à la fin du scope
     * }
     * @endcode
     */

    /**
     * @example 03_custom_factory_registration.cpp
     * @brief Enregistrement de factories personnalisées pour types complexes.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Type avec constructeur paramétré (pas de ctor par défaut) ───
     * class ConfigData : public NkISerializable {
     * public:
     *     nk_uint32 version;
     *     nk_float32 difficultyMultiplier;
     *     char configName[128];
     * 
     *     // Constructeur requis — pas de ctor par défaut
     *     ConfigData(nk_uint32 ver, nk_float32 mult, const char* name) noexcept
     *         : version(ver)
     *         , difficultyMultiplier(mult) {
     *         std::strncpy(configName, name, 127);
     *         configName[127] = '\0';
     *     }
     * 
     *     // Constructeur par défaut factice pour compatibilité registry
     *     // (ne sera JAMAIS appelé via le registry grâce à la factory custom)
     *     ConfigData() noexcept : version(0), difficultyMultiplier(1.f) {
     *         configName[0] = '\0';
     *     }
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return archive.Write(version)
     *             && archive.Write(difficultyMultiplier)
     *             && archive.WriteString(configName, sizeof(configName));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return archive.Read(version)
     *             && archive.Read(difficultyMultiplier)
     *             && archive.ReadString(configName, sizeof(configName));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "ConfigData";
     *     }
     * };
     * 
     * // ── Enregistrement via factory custom (pas de macro possible) ───
     * void RegisterConfigData() {
     *     // Factory qui fournit les paramètres requis à la construction
     *     auto factory = []() -> NkISerializable* {
     *         // Paramètres par défaut pour la désérialisation
     *         // Les vraies valeurs seront écrasées par Deserialize()
     *         return new ConfigData(
     *             / *version=* /0,
     *             / *mult=* /1.f,
     *             / *name=* /""
     *         );
     *     };
     * 
     *     NkSerializableRegistry::RegisterFactory("ConfigData", factory);
     * }
     * 
     * // ── Factory avec injection de dépendances (cas avancé) ───────────
     * class NetworkConfig : public NkISerializable {
     * public:
     *     char serverAddress[256];
     *     nk_uint16 port;
     *     nk_bool useEncryption;
     * 
     *     // Dépendance externe : un service de validation d'adresse
     *     using AddressValidator = bool(*)(const char*);
     *     AddressValidator validator = nullptr;
     * 
     *     NetworkConfig(AddressValidator v = nullptr) noexcept
     *         : port(8080), useEncryption(true), validator(v) {
     *         serverAddress[0] = '\0';
     *     }
     * 
     *     // ... Serialize/Deserialize/GetTypeName ...
     * };
     * 
     * void RegisterNetworkConfig(AddressValidator globalValidator) {
     *     // Capture de la dépendance dans la lambda de factory
     *     auto factory = [globalValidator]() -> NkISerializable* {
     *         return new NetworkConfig(globalValidator);
     *     };
     * 
     *     NkSerializableRegistry::RegisterFactory("NetworkConfig", factory);
     * }
     * 
     * // ── Usage : création via registry avec factory custom ────────────
     * void Example_CustomFactoryUsage() {
     *     // Enregistrement au démarrage
     *     RegisterConfigData();
     *     RegisterNetworkConfig(&ValidateIpAddress);  // Injection de dépendance
     * 
     *     // Création dynamique plus tard
     *     auto* raw = NkSerializableRegistry::Global().Create("ConfigData");
     *     if (raw) {
     *         // La factory a utilisé les paramètres par défaut
     *         // Deserialize() écrasera avec les vraies valeurs du fichier
     *         NkArchive archive;
     *         if (archive.OpenForRead("config.dat") && raw->Deserialize(archive)) {
     *             if (auto* config = static_cast<ConfigData*>(raw)) {
     *                 logger.Info("Config chargée: %s v%u\n",
     *                             config->configName, config->version);
     *             }
     *         }
     *         delete raw;  // ✅ Responsibility of caller
     *     }
     * }
     * @endcode
     */

    /**
     * @example 04_best_practices_pitfalls.cpp
     * @brief Bonnes pratiques et pièges courants avec NkISerializable.
     * 
     * @code
     * // ✅ BONNES PRATIQUES
     * 
     * // 1. Toujours vérifier le résultat de Create() avant utilisation
     * void SafeCreateExample() {
     *     auto* obj = NkSerializableRegistry::Global().Create("MyType");
     *     if (!obj) {  // ✅ Guard explicite
     *         logger.Error("Type 'MyType' non enregistré ou non créable\n");
     *         return;
     *     }
     *     // Utiliser obj...
     *     delete obj;  // ✅ Cleanup obligatoire
     * }
     * 
     * // 2. Utiliser NkOwned pour RAII quand la durée de vie est locale
     * void Example_WithNkOwned() {
     *     auto owned = NkOwned<NkISerializable>(
     *         NkSerializableRegistry::Global().Create("MyType")
     *     );
     *     if (owned) {
     *         // Utiliser owned.Get()...
     *     }
     *     // delete automatique à la fin du scope — pas de fuite possible
     * }
     * 
     * // 3. Maintenir l'ordre Serialize/Deserialize strictement identique
     * class GoodSerializable : public NkISerializable {
     *     nk_uint32 a; nk_float32 b; char c[32];
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // Ordre : a → b → c
     *         return archive.Write(a) && archive.Write(b) && archive.WriteString(c, 32);
     *     }
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // MÊME ordre : a → b → c ✅
     *         return archive.Read(a) && archive.Read(b) && archive.ReadString(c, 32);
     *     }
     *     const char* GetTypeName() const noexcept override { return "GoodSerializable"; }
     * };
     * 
     * // 4. Enregistrer tous les types au démarrage, dans un seul thread
     * void RegisterAllTypesAtStartup() {
     *     // Dans main() ou module init, avant tout accès concurrent :
     *     NK_REGISTER_SERIALIZABLE(PlayerStats);
     *     NK_REGISTER_SERIALIZABLE(Enemy);
     *     NK_REGISTER_SERIALIZABLE(NPC);
     *     // ... etc.
     *     // Ensuite : accès concurrents en lecture sont safe
     * }
     * 
     * // 5. Documenter clairement l'ownership transfer dans les APIs
     * /// @return NkISerializable* nouvellement alloué — caller responsable du delete
     * NkISerializable* CreateSerializedObject(const char* typeName);
     * 
     * 
     * // ❌ PIÈGES À ÉVITER
     * 
     * // 1. Oublier de delete le résultat de Create() — fuite mémoire garantie
     * // ❌ auto* obj = registry.Create("MyType");
     * //    // ... utilisation ...
     * //    // OUBLI du delete → fuite mémoire
     * // ✅ Toujours : delete obj; ou utiliser NkOwned pour RAII
     * 
     * // 2. Changer l'ordre des champs entre Serialize et Deserialize
     * // ❌ class BadSerializable {
     * //        nk_bool Serialize(...) { return archive.Write(a) && archive.Write(b); }
     * //        nk_bool Deserialize(...) { return archive.Read(b) && archive.Read(a); }  // ❌ Ordre inversé !
     * //    };
     * // ✅ Toujours maintenir le MÊME ordre dans les deux méthodes
     * 
     * // 3. Enregistrer un type sans constructeur par défaut via Register<T>()
     * // ❌ class NoDefaultCtor { NoDefaultCtor(int x); };  // Pas de ctor()
     * //    NK_REGISTER_SERIALIZABLE(NoDefaultCtor);  // ❌ Compilation OK, runtime crash (new T() échoue)
     * // ✅ Utiliser RegisterFactory() avec une factory qui fournit les paramètres requis
     * 
     * // 4. Appeler Register() depuis plusieurs threads sans synchronisation
     * // ❌ Thread A: Register<TypeA>("A");  // Race condition avec Thread B
     * //    Thread B: Register<TypeB>("B");
     * // ✅ Tous les Register() dans un seul thread au démarrage, avant accès concurrents
     * 
     * // 5. Utiliser un nom de type non-unique ou mal orthographié
     * // ❌ NK_REGISTER_SERIALIZABLE(PlayerStats);  // Enregistré comme "PlayerStats"
     * //    registry.Create("PlayerStat");  // ❌ Orthographe différente → nullptr
     * // ✅ Toujours utiliser #Type via la macro pour éviter les erreurs de frappe :
     * //    NK_REGISTER_SERIALIZABLE(PlayerStats);  // #Type → "PlayerStats" automatiquement
     * 
     * // 6. Conserver un pointeur brut au-delà de sa validité sans documentation
     * // ❌ NkISerializable* cached = registry.Create("MyType");
     * //    // ... plus tard, après un changement de scène ou unload de module ...
     * //    cached->Serialize(archive);  // ❌ Pointeur potentiellement dangling
     * // ✅ Toujours re-créer via Create() quand nécessaire, ou documenter clairement
     * //    la durée de vie attendue et les conditions d'invalidation
     * @endcode
     */

    /** @} */ // end of NkISerializable_Examples group

    // =============================================================================
    // 📚 EXEMPLES D'UTILISATION — NkISerializable + NkSerializableRegistry
    // =============================================================================
    /**
     * @addtogroup NkISerializable_Examples
     * @{
     */

    /**
     * @example 01_basic_serialization.cpp
     * @brief Définition et usage basique d'un type sérialisable.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * #include "NKSerialization/NkArchive.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Définition d'un type sérialisable simple ──────────────────────
     * class PlayerStats : public NkISerializable {
     * public:
     *     nk_uint32 level = 1;
     *     nk_float32 health = 100.f;
     *     nk_float32 mana = 50.f;
     *     char playerName[64] = {};
     * 
     *     // Constructeur par défaut requis pour l'enregistrement
     *     PlayerStats() noexcept = default;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // Chaînage des Write() — retour false si une écriture échoue
     *         return archive.Write(level)
     *             && archive.Write(health)
     *             && archive.Write(mana)
     *             && archive.WriteString(playerName, sizeof(playerName));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // Lecture dans le MÊME ordre que Serialize() — crucial !
     *         return archive.Read(level)
     *             && archive.Read(health)
     *             && archive.Read(mana)
     *             && archive.ReadString(playerName, sizeof(playerName));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "PlayerStats";  // Doit correspondre au nom d'enregistrement
     *     }
     * };
     * 
     * // ── Enregistrement automatique (dans un .cpp, après la définition) ─
     * // NK_REGISTER_SERIALIZABLE(PlayerStats);
     * 
     * // ── Usage : sérialisation dans un fichier binaire ─────────────────
     * void SavePlayerStats(const PlayerStats& stats, const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForWrite(filename)) {
     *         logger.Error("Impossible d'ouvrir %s en écriture\n", filename);
     *         return;
     *     }
     * 
     *     // Écriture du type pour désérialisation polymorphique future
     *     archive.WriteString(stats.GetTypeName());
     * 
     *     // Sérialisation du contenu via l'interface de base
     *     if (!stats.Serialize(archive)) {
     *         logger.Error("Échec de la sérialisation de PlayerStats\n");
     *         return;
     *     }
     * 
     *     archive.Close();
     *     logger.Info("Stats joueur sauvegardées dans %s\n", filename);
     * }
     * 
     * // ── Usage : désérialisation polymorphique depuis un fichier ──────
     * NkISerializable* LoadSerializedObject(const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForRead(filename)) {
     *         logger.Error("Impossible d'ouvrir %s en lecture\n", filename);
     *         return nullptr;
     *     }
     * 
     *     // Lecture du nom du type pour création dynamique
     *     char typeName[128] = {};
     *     if (!archive.ReadString(typeName, sizeof(typeName))) {
     *         logger.Error("Échec de lecture du nom de type\n");
     *         return nullptr;
     *     }
     * 
     *     // Création via le registry — caller owns le résultat
     *     auto* obj = NkSerializableRegistry::Global().Create(typeName);
     *     if (!obj) {
     *         logger.Error("Type '%s' non enregistré ou non créable\n", typeName);
     *         return nullptr;
     *     }
     * 
     *     // Désérialisation du contenu
     *     if (!obj->Deserialize(archive)) {
     *         logger.Error("Échec de la désérialisation de %s\n", typeName);
     *         delete obj;  // ✅ Cleanup en cas d'erreur
     *         return nullptr;
     *     }
     * 
     *     archive.Close();
     *     return obj;  // ✅ Ownership transfer to caller — MUST delete later
     * }
     * 
     * // ── Usage complet avec gestion RAII via NkOwned (si disponible) ─
     * void Example_WithNkOwned() {
     *     // Sauvegarde
     *     PlayerStats stats;
     *     stats.level = 10;
     *     stats.health = 75.f;
     *     std::strncpy(stats.playerName, "Hero", 63);
     *     SavePlayerStats(stats, "save.dat");
     * 
     *     // Chargement avec RAII via NkOwned
     *     if (auto* raw = LoadSerializedObject("save.dat")) {
     *         // Wrapper RAII pour delete automatique
     *         auto owned = NkOwned<NkISerializable>(raw);
     * 
     *         // Cast typé pour accès aux méthodes spécifiques
     *         if (auto* playerStats = dynamic_cast<PlayerStats*>(owned.Get())) {
     *             logger.Info("Joueur chargé : %s, niveau %u, PV %.1f\n",
     *                         playerStats->playerName,
     *                         playerStats->level,
     *                         playerStats->health);
     *         }
     *         // delete automatique à la fin du scope grâce à NkOwned
     *     }
     * }
     * @endcode
     */

    /**
     * @example 02_polymorphic_serialization.cpp
     * @brief Sérialisation polymorphique d'une hiérarchie de types.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Hiérarchie de classes sérialisables ──────────────────────────
     * class GameEntity : public NkISerializable {
     * public:
     *     nk_uint32 entityId = 0;
     *     char name[64] = {};
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return archive.Write(entityId)
     *             && archive.WriteString(name, sizeof(name));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return archive.Read(entityId)
     *             && archive.ReadString(name, sizeof(name));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "GameEntity";
     *     }
     * };
     * 
     * class Enemy : public GameEntity {
     * public:
     *     nk_float32 damage = 10.f;
     *     nk_float32 armor = 5.f;
     *     nk_uint32 patrolRadius = 50;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // D'abord sérialiser la base, puis les champs dérivés
     *         return GameEntity::Serialize(archive)
     *             && archive.Write(damage)
     *             && archive.Write(armor)
     *             && archive.Write(patrolRadius);
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // D'abord désérialiser la base, puis les champs dérivés
     *         return GameEntity::Deserialize(archive)
     *             && archive.Read(damage)
     *             && archive.Read(armor)
     *             && archive.Read(patrolRadius);
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "Enemy";  // Nom unique pour le registry
     *     }
     * };
     * 
     * class NPC : public GameEntity {
     * public:
     *     nk_int32 dialogueTreeId = -1;
     *     nk_bool isMerchant = false;
     *     nk_float32 goldAmount = 0.f;
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return GameEntity::Serialize(archive)
     *             && archive.Write(dialogueTreeId)
     *             && archive.Write(isMerchant)
     *             && archive.Write(goldAmount);
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return GameEntity::Deserialize(archive)
     *             && archive.Read(dialogueTreeId)
     *             && archive.Read(isMerchant)
     *             && archive.Read(goldAmount);
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "NPC";
     *     }
     * };
     * 
     * // ── Enregistrement des types dérivés (dans des .cpp respectifs) ──
     * // Enemy.cpp : NK_REGISTER_SERIALIZABLE(Enemy);
     * // NPC.cpp   : NK_REGISTER_SERIALIZABLE(NPC);
     * // Note : GameEntity n'a pas besoin d'être enregistré si on ne crée
     * //        jamais d'instances polymorphiques de la base seule.
     * 
     * // ── Sérialisation d'un conteneur polymorphique ───────────────────
     * bool SaveEntityList(const NkVector<GameEntity*>& entities,
     *                     const char* filename) {
     *     NkArchive archive;
     *     if (!archive.OpenForWrite(filename)) {
     *         return false;
     *     }
     * 
     *     // Écriture du nombre d'entités
     *     nk_uint32 count = static_cast<nk_uint32>(entities.Size());
     *     if (!archive.Write(count)) {
     *         return false;
     *     }
     * 
     *     // Sérialisation de chaque entité avec son type
     *     for (const auto* entity : entities) {
     *         if (!entity) {
     *             continue;  // Skip les pointeurs nuls
     *         }
     * 
     *         // Écriture du nom du type pour désérialisation polymorphique
     *         if (!archive.WriteString(entity->GetTypeName())) {
     *             return false;
     *         }
     * 
     *         // Sérialisation du contenu via l'interface de base
     *         if (!entity->Serialize(archive)) {
     *             return false;
     *         }
     *     }
     * 
     *     return archive.Close();
     * }
     * 
     * // ── Désérialisation polymorphique avec cleanup RAII ──────────────
     * NkVector<NkOwned<GameEntity>> LoadEntityList(const char* filename) {
     *     NkVector<NkOwned<GameEntity>> result;
     * 
     *     NkArchive archive;
     *     if (!archive.OpenForRead(filename)) {
     *         return result;  // Vecteur vide en cas d'erreur
     *     }
     * 
     *     // Lecture du nombre d'entités
     *     nk_uint32 count = 0;
     *     if (!archive.Read(count)) {
     *         return result;
     *     }
     * 
     *     // Désérialisation de chaque entité
     *     for (nk_uint32 i = 0; i < count; ++i) {
     *         // Lecture du nom du type
     *         char typeName[128] = {};
     *         if (!archive.ReadString(typeName, sizeof(typeName))) {
     *             break;  // Erreur de lecture — arrêt propre
     *         }
     * 
     *         // Création dynamique via le registry
     *         auto* raw = NkSerializableRegistry::Global().Create(typeName);
     *         if (!raw) {
     *             logger.Warning("Type '%s' non enregistré — skip entité %u\n",
     *                            typeName, i);
     *             continue;
     *         }
     * 
     *         // Désérialisation du contenu
     *         if (!raw->Deserialize(archive)) {
     *             logger.Warning("Échec désérialisation entité %u (%s)\n", i, typeName);
     *             delete raw;  // ✅ Cleanup manuel car pas encore dans NkOwned
     *             continue;
     *         }
     * 
     *         // Cast vers GameEntity* (sûr car tous les types dérivent de GameEntity)
     *         auto* entity = static_cast<GameEntity*>(raw);
     * 
     *         // Transfert dans NkOwned pour gestion RAII automatique
     *         result.PushBack(NkOwned<GameEntity>(entity));
     *     }
     * 
     *     archive.Close();
     *     return result;  // NkOwned gère le delete automatique à la destruction
     * }
     * 
     * // ── Usage : chargement et utilisation typée ──────────────────────
     * void Example_PolymorphicUsage() {
     *     auto entities = LoadEntityList("entities.dat");
     * 
     *     for (const auto& owned : entities) {
     *         GameEntity* base = owned.Get();
     *         if (!base) {
     *             continue;
     *         }
     * 
     *         logger.Info("Entité: %s (ID: %u)\n", base->name, base->entityId);
     * 
     *         // Dispatch typé via dynamic_cast pour logique spécifique
     *         if (auto* enemy = dynamic_cast<Enemy*>(base)) {
     *             logger.Info("  → Ennemi: dégâts=%.1f, armure=%.1f\n",
     *                         enemy->damage, enemy->armor);
     *         }
     *         else if (auto* npc = dynamic_cast<NPC*>(base)) {
     *             logger.Info("  → PNJ: marchand=%s, or=%.1f\n",
     *                         npc->isMerchant ? "oui" : "non",
     *                         npc->goldAmount);
     *         }
     *     }
     *     // ✅ Cleanup automatique via NkOwned à la fin du scope
     * }
     * @endcode
     */

    /**
     * @example 03_custom_factory_registration.cpp
     * @brief Enregistrement de factories personnalisées pour types complexes.
     * 
     * @code
     * #include "NKSerialization/NkISerializable.h"
     * 
     * using namespace nkentseu;
     * 
     * // ── Type avec constructeur paramétré (pas de ctor par défaut) ───
     * class ConfigData : public NkISerializable {
     * public:
     *     nk_uint32 version;
     *     nk_float32 difficultyMultiplier;
     *     char configName[128];
     * 
     *     // Constructeur requis — pas de ctor par défaut
     *     ConfigData(nk_uint32 ver, nk_float32 mult, const char* name) noexcept
     *         : version(ver)
     *         , difficultyMultiplier(mult) {
     *         std::strncpy(configName, name, 127);
     *         configName[127] = '\0';
     *     }
     * 
     *     // Constructeur par défaut factice pour compatibilité registry
     *     // (ne sera JAMAIS appelé via le registry grâce à la factory custom)
     *     ConfigData() noexcept : version(0), difficultyMultiplier(1.f) {
     *         configName[0] = '\0';
     *     }
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         return archive.Write(version)
     *             && archive.Write(difficultyMultiplier)
     *             && archive.WriteString(configName, sizeof(configName));
     *     }
     * 
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         return archive.Read(version)
     *             && archive.Read(difficultyMultiplier)
     *             && archive.ReadString(configName, sizeof(configName));
     *     }
     * 
     *     const char* GetTypeName() const noexcept override {
     *         return "ConfigData";
     *     }
     * };
     * 
     * // ── Enregistrement via factory custom (pas de macro possible) ───
     * void RegisterConfigData() {
     *     // Factory qui fournit les paramètres requis à la construction
     *     auto factory = []() -> NkISerializable* {
     *         // Paramètres par défaut pour la désérialisation
     *         // Les vraies valeurs seront écrasées par Deserialize()
     *         return new ConfigData(
     *             / *version=* /0,
    *             / *mult=* /1.f,
    *             / *name=* /""
    *         );
    *     };
    * 
    *     NkSerializableRegistry::RegisterFactory("ConfigData", factory);
    * }
    * 
    * // ── Factory avec injection de dépendances (cas avancé) ───────────
    * class NetworkConfig : public NkISerializable {
    * public:
    *     char serverAddress[256];
    *     nk_uint16 port;
    *     nk_bool useEncryption;
    * 
    *     // Dépendance externe : un service de validation d'adresse
    *     using AddressValidator = bool(*)(const char*);
    *     AddressValidator validator = nullptr;
    * 
    *     NetworkConfig(AddressValidator v = nullptr) noexcept
    *         : port(8080), useEncryption(true), validator(v) {
    *         serverAddress[0] = '\0';
    *     }
    * 
    *     // ... Serialize/Deserialize/GetTypeName ...
    * };
    * 
    * void RegisterNetworkConfig(AddressValidator globalValidator) {
    *     // Capture de la dépendance dans la lambda de factory
    *     auto factory = [globalValidator]() -> NkISerializable* {
    *         return new NetworkConfig(globalValidator);
    *     };
    * 
    *     NkSerializableRegistry::RegisterFactory("NetworkConfig", factory);
    * }
    * 
    * // ── Usage : création via registry avec factory custom ────────────
    * void Example_CustomFactoryUsage() {
    *     // Enregistrement au démarrage
    *     RegisterConfigData();
    *     RegisterNetworkConfig(&ValidateIpAddress);  // Injection de dépendance
    * 
    *     // Création dynamique plus tard
    *     auto* raw = NkSerializableRegistry::Global().Create("ConfigData");
    *     if (raw) {
    *         // La factory a utilisé les paramètres par défaut
    *         // Deserialize() écrasera avec les vraies valeurs du fichier
    *         NkArchive archive;
    *         if (archive.OpenForRead("config.dat") && raw->Deserialize(archive)) {
    *             if (auto* config = static_cast<ConfigData*>(raw)) {
    *                 logger.Info("Config chargée: %s v%u\n",
    *                             config->configName, config->version);
    *             }
    *         }
    *         delete raw;  // ✅ Responsibility of caller
    *     }
    * }
    * @endcode
    */

    /**
     * @example 04_best_practices_pitfalls.cpp
     * @brief Bonnes pratiques et pièges courants avec NkISerializable.
     * 
     * @code
     * // ✅ BONNES PRATIQUES
     * 
     * // 1. Toujours vérifier le résultat de Create() avant utilisation
     * void SafeCreateExample() {
     *     auto* obj = NkSerializableRegistry::Global().Create("MyType");
     *     if (!obj) {  // ✅ Guard explicite
     *         logger.Error("Type 'MyType' non enregistré ou non créable\n");
     *         return;
     *     }
     *     // Utiliser obj...
     *     delete obj;  // ✅ Cleanup obligatoire
     * }
     * 
     * // 2. Utiliser NkOwned pour RAII quand la durée de vie est locale
     * void Example_WithNkOwned() {
     *     auto owned = NkOwned<NkISerializable>(
     *         NkSerializableRegistry::Global().Create("MyType")
     *     );
     *     if (owned) {
     *         // Utiliser owned.Get()...
     *     }
     *     // delete automatique à la fin du scope — pas de fuite possible
     * }
     * 
     * // 3. Maintenir l'ordre Serialize/Deserialize strictement identique
     * class GoodSerializable : public NkISerializable {
     *     nk_uint32 a; nk_float32 b; char c[32];
     * 
     *     nk_bool Serialize(NkArchive& archive) const override {
     *         // Ordre : a → b → c
     *         return archive.Write(a) && archive.Write(b) && archive.WriteString(c, 32);
     *     }
     *     nk_bool Deserialize(const NkArchive& archive) override {
     *         // MÊME ordre : a → b → c ✅
     *         return archive.Read(a) && archive.Read(b) && archive.ReadString(c, 32);
     *     }
     *     const char* GetTypeName() const noexcept override { return "GoodSerializable"; }
     * };
     * 
     * // 4. Enregistrer tous les types au démarrage, dans un seul thread
     * void RegisterAllTypesAtStartup() {
     *     // Dans main() ou module init, avant tout accès concurrent :
     *     NK_REGISTER_SERIALIZABLE(PlayerStats);
     *     NK_REGISTER_SERIALIZABLE(Enemy);
     *     NK_REGISTER_SERIALIZABLE(NPC);
     *     // ... etc.
     *     // Ensuite : accès concurrents en lecture sont safe
     * }
     * 
     * // 5. Documenter clairement l'ownership transfer dans les APIs
     * /// @return NkISerializable* nouvellement alloué — caller responsable du delete
     * NkISerializable* CreateSerializedObject(const char* typeName);
     * 
     * 
     * // ❌ PIÈGES À ÉVITER
     * 
     * // 1. Oublier de delete le résultat de Create() — fuite mémoire garantie
     * // ❌ auto* obj = registry.Create("MyType");
     * //    // ... utilisation ...
     * //    // OUBLI du delete → fuite mémoire
     * // ✅ Toujours : delete obj; ou utiliser NkOwned pour RAII
     * 
     * // 2. Changer l'ordre des champs entre Serialize et Deserialize
     * // ❌ class BadSerializable {
     * //        nk_bool Serialize(...) { return archive.Write(a) && archive.Write(b); }
     * //        nk_bool Deserialize(...) { return archive.Read(b) && archive.Read(a); }  // ❌ Ordre inversé !
     * //    };
     * // ✅ Toujours maintenir le MÊME ordre dans les deux méthodes
     * 
     * // 3. Enregistrer un type sans constructeur par défaut via Register<T>()
     * // ❌ class NoDefaultCtor { NoDefaultCtor(int x); };  // Pas de ctor()
     * //    NK_REGISTER_SERIALIZABLE(NoDefaultCtor);  // ❌ Compilation OK, runtime crash (new T() échoue)
     * // ✅ Utiliser RegisterFactory() avec une factory qui fournit les paramètres requis
     * 
     * // 4. Appeler Register() depuis plusieurs threads sans synchronisation
     * // ❌ Thread A: Register<TypeA>("A");  // Race condition avec Thread B
     * //    Thread B: Register<TypeB>("B");
     * // ✅ Tous les Register() dans un seul thread au démarrage, avant accès concurrents
     * 
     * // 5. Utiliser un nom de type non-unique ou mal orthographié
     * // ❌ NK_REGISTER_SERIALIZABLE(PlayerStats);  // Enregistré comme "PlayerStats"
     * //    registry.Create("PlayerStat");  // ❌ Orthographe différente → nullptr
     * // ✅ Toujours utiliser #Type via la macro pour éviter les erreurs de frappe :
     * //    NK_REGISTER_SERIALIZABLE(PlayerStats);  // #Type → "PlayerStats" automatiquement
     * 
     * // 6. Conserver un pointeur brut au-delà de sa validité sans documentation
     * // ❌ NkISerializable* cached = registry.Create("MyType");
     * //    // ... plus tard, après un changement de scène ou unload de module ...
     * //    cached->Serialize(archive);  // ❌ Pointeur potentiellement dangling
     * // ✅ Toujours re-créer via Create() quand nécessaire, ou documenter clairement
     * //    la durée de vie attendue et les conditions d'invalidation
     * @endcode
     */

    /** @} */ // end of NkISerializable_Examples group

#endif // NKENTSEU_SERIALIZATION_NKISERIALIZABLE_H