// =============================================================================
// NKSerialization/NkComponentRegistry.h
// Registre des composants ECS sérialisables avec support polymorphique.
//
// Design :
//  - Architecture STL-free : NkVector + pointeurs de fonctions au lieu de std::unordered_map
//  - NkTypeId cohérent avec NkSchemaVersioning (pas de NkComponentId)
//  - Fallback de sérialisation générique via représentation hexadécimale
//  - Intégration transparente avec NkISerializable pour types polymorphiques
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Macros d'enregistrement automatique pour usage simplifié
//
// Responsabilités :
//  - Enregistrement des types de composants par NkTypeId ou nom
//  - Sérialisation/désérialisation génériques via pointeurs de fonctions
//  - Lookup rapide par type ID ou nom de composant
//  - Itération sur tous les composants enregistrés
//
// Thread-safety :
//  - Singleton thread-safe via Meyer's pattern (initialisation C++11 garantie)
//  - Lecture concurrente safe après initialisation complète
//  - Enregistrement (Register/Unregister) doit être fait avant accès concurrent
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKCOMPONENTREGISTRY_H
#define NKENTSEU_SERIALIZATION_NKCOMPONENTREGISTRY_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKSerialization/NkISerializable.h"
    #include "NKSerialization/NkSchemaVersioning.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKCore/NkTraits.h"

    // -------------------------------------------------------------------------
    // EN-TÊTES STANDARDS POUR OPÉRATIONS SUR LES CHAÎNES
    // -------------------------------------------------------------------------
    // Utilisation minimale de cstring pour strcmp (portable, pas de dépendance STL).

    #include <cstring>

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // CLASSE : NkComponentRegistry
        // DESCRIPTION : Registre dédié aux composants ECS sérialisables
        // =============================================================================
        /**
         * @class NkComponentRegistry
         * @brief Registre central pour la sérialisation/désérialisation des composants ECS
         * @ingroup SerializationComponents
         *
         * Rôle principal :
         *  - Gérer l'enregistrement des types de composants par NkTypeId
         *  - Fournir des fonctions de sérialisation/désérialisation génériques
         *  - Permettre le lookup par type ID ou nom de composant
         *  - Supporter l'itération sur tous les composants enregistrés
         *
         * Caractéristiques clés :
         *  - Architecture STL-free : utilise NkVector au lieu de std::unordered_map
         *  - Pointeurs de fonctions au lieu de std::function pour éviter les allocations
         *  - Fallback de sérialisation générique pour les types non-enregistrés
         *  - Intégration transparente avec NkISerializable pour les types polymorphiques
         *  - NkTypeId cohérent avec le système de versionnage (NkSchemaVersioning)
         *
         * Pattern d'utilisation :
         *  1. Enregistrer les composants au démarrage via Register<T>()
         *  2. Utiliser Serialize<T>() / Deserialize<T>() pour la sérialisation
         *  3. Le fallback DefaultSerialize gère les types non-enregistrés
         *
         * Thread-safety :
         *  - Singleton thread-safe via Meyer's pattern (C++11+)
         *  - Lectures concurrentes safe après initialisation
         *  - Enregistrement doit être fait dans un seul thread au démarrage
         *
         * @note Ce registre est indépendant de NkSerializableRegistry qui gère
         *       les objets polymorphiques par nom de type. Ici, on travaille par type ID.
         *
         * @example Enregistrement et utilisation basique
         * @code
         * struct Position {
         *     float x, y, z;
         * };
         *
         * // Enregistrement au démarrage
         * NK_REGISTER_COMPONENT(Position, "Position");
         *
         * // Utilisation
         * Position pos{1.0f, 2.0f, 3.0f};
         * nkentseu::NkArchive archive;
         *
         * if (nkentseu::NkComponentRegistry::Serialize(pos, archive)) {
         *     // archive contient la représentation sérialisée de pos
         * }
         *
         * Position loaded;
         * if (nkentseu::NkComponentRegistry::Deserialize(loaded, archive)) {
         *     // loaded.x == 1.0f, loaded.y == 2.0f, loaded.z == 3.0f
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkComponentRegistry {


            // -----------------------------------------------------------------
            // SECTION 3 : TYPES ET ALIASES
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // TYPE ALIAS : Signatures des fonctions de sérialisation
            // -----------------------------------------------------------------
            /**
             * @typedef SerializeFn
             * @brief Signature des fonctions de sérialisation de composants
             * @ingroup ComponentRegistryTypes
             *
             * Signature : nk_bool(*)(const void*, NkArchive&) noexcept
             *  - const void* : pointeur vers le composant à sérialiser
             *  - NkArchive& : archive de destination pour le stockage
             *  - Retour : true si succès, false en cas d'erreur
             *  - noexcept : garantie de non-levée d'exception
             *
             * @note Utilise un pointeur void* pour permettre le stockage générique
             *       dans le registre sans connaître le type concret à la compilation.
             */
            using SerializeFn = nk_bool(*)(const void*, NkArchive&) noexcept;

            /**
             * @typedef DeserializeFn
             * @brief Signature des fonctions de désérialisation de composants
             * @ingroup ComponentRegistryTypes
             *
             * Signature : nk_bool(*)(void*, const NkArchive&) noexcept
             *  - void* : pointeur vers le composant à peupler
             *  - const NkArchive& : archive source contenant les données
             *  - Retour : true si succès, false en cas d'erreur
             *  - noexcept : garantie de non-levée d'exception
             *
             * @note Utilise un pointeur void* pour permettre le stockage générique
             *       dans le registre sans connaître le type concret à la compilation.
             */
            using DeserializeFn = nk_bool(*)(void*, const NkArchive&) noexcept;


            // -----------------------------------------------------------------
            // STRUCTURE : Entry
            // -----------------------------------------------------------------
            /**
             * @struct Entry
             * @brief Entrée de registre : association type ID → fonctions de sérialisation
             * @ingroup ComponentRegistryTypes
             *
             * Champs :
             *  - typeId : identifiant unique du type (via NkTypeOf<T>())
             *  - name : nom lisible du composant (pour debugging et lookup par nom)
             *  - serialize : pointeur vers la fonction de sérialisation
             *  - deserialize : pointeur vers la fonction de désérialisation
             *
             * @note Toutes les entrées sont stockées dans un NkVector ordonné.
             *       La recherche est linéaire mais optimisée pour <1000 composants.
             */
            struct Entry {

                /// @brief Identifiant unique du type (généré par NkTypeOf<T>())
                NkTypeId typeId = 0u;

                /// @brief Nom lisible du composant (utilisé pour FindByName)
                const char* name = nullptr;

                /// @brief Pointeur vers la fonction de sérialisation
                SerializeFn serialize = nullptr;

                /// @brief Pointeur vers la fonction de désérialisation
                DeserializeFn deserialize = nullptr;

            }; // struct Entry


            // -----------------------------------------------------------------
            // SECTION 4 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : ACCÈS AU SINGLETON
            // -----------------------------------------------------------------
            /**
             * @brief Accès au singleton global du registre de composants
             * @return Référence à l'instance unique de NkComponentRegistry
             * @ingroup ComponentRegistryAPI
             *
             * Thread-safety :
             *  - Initialisation thread-safe garantie par C++11 (Meyer's singleton)
             *  - Accès en lecture concurrente safe après première utilisation
             *  - L'enregistrement (Register) doit être fait avant accès concurrent
             *
             * @note Pattern classique de singleton sans overhead de mutex en lecture.
             */
            [[nodiscard]] static NkComponentRegistry& Global() noexcept {
                static NkComponentRegistry s_instance;
                return s_instance;
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : ENREGISTREMENT DE COMPOSANTS
            // -----------------------------------------------------------------
            /**
             * @brief Enregistre un type de composant dans le registre
             * @tparam T Type du composant à enregistrer
             * @param name Nom lisible du composant (utilisé pour FindByName)
             * @param serFn Pointeur optionnel vers fonction de sérialisation custom
             * @param deserFn Pointeur optionnel vers fonction de désérialisation custom
             * @ingroup ComponentRegistryAPI
             *
             * Comportement :
             *  - Si le type est déjà enregistré : met à jour les fonctions fournies
             *  - Sinon : ajoute une nouvelle entrée avec les fonctions fournies ou fallbacks
             *  - Les fallbacks DefaultSerialize/DefaultDeserialize sont utilisés si null
             *
             * @warning ⚠️ Thread-safety : Register() n'est PAS thread-safe.
             *          Tous les enregistrements doivent être faits avant tout accès
             *          concurrent, typiquement au démarrage dans un seul thread.
             *
             * @note Préférer la macro NK_REGISTER_COMPONENT(T, Name) pour éviter
             *       les erreurs de nommage et centraliser l'enregistrement.
             *
             * @example
             * @code
             * // Enregistrement explicite (dans un .cpp) :
             * nkentseu::NkComponentRegistry::Register<Position>("Position");
             *
             * // Ou via macro (recommandé) :
             * NK_REGISTER_COMPONENT(Position, "Position");
             * @endcode
             */
            template<typename T>
            static void Register(const char* name,
                                SerializeFn serFn = nullptr,
                                DeserializeFn deserFn = nullptr) noexcept {
                auto& reg = Global();
                NkTypeId id = NkTypeOf<T>();

                // Recherche d'une entrée existante pour mise à jour
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (reg.mEntries[i].typeId == id) {
                        if (serFn) {
                            reg.mEntries[i].serialize = serFn;
                        }
                        if (deserFn) {
                            reg.mEntries[i].deserialize = deserFn;
                        }
                        return;
                    }
                }

                // Création d'une nouvelle entrée
                Entry e;
                e.typeId = id;
                e.name = name;
                e.serialize = serFn ? serFn : &DefaultSerialize<T>;
                e.deserialize = deserFn ? deserFn : &DefaultDeserialize<T>;
                reg.mEntries.PushBack(e);
            }

            /**
             * @brief Désenregistre un type de composant du registre
             * @tparam T Type du composant à désenregistrer
             * @ingroup ComponentRegistryAPI
             *
             * Comportement :
             *  - Recherche linéaire par NkTypeId
             *  - Suppression de l'entrée via Erase() si trouvée
             *  - No-op silencieux si le type n'est pas enregistré
             *
             * @warning ⚠️ Thread-safety : Unregister() n'est PAS thread-safe.
             *          À utiliser uniquement pendant l'initialisation ou
             *          dans un contexte mono-thread contrôlé.
             *
             * @note Rarement nécessaire — préférer garder les composants enregistrés
             *       pour toute la durée de vie de l'application.
             */
            template<typename T>
            static void Unregister() noexcept {
                auto& reg = Global();
                NkTypeId id = NkTypeOf<T>();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (reg.mEntries[i].typeId == id) {
                        reg.mEntries.Erase(i);
                        return;
                    }
                }
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : LOOKUP DE COMPOSANTS
            // -----------------------------------------------------------------
            /**
             * @brief Trouve une entrée de registre par NkTypeId
             * @param id Identifiant du type à rechercher
             * @return Pointeur vers l'Entry si trouvée, nullptr sinon
             * @ingroup ComponentRegistryAPI
             *
             * @note Complexité O(n) où n = nombre de composants enregistrés.
             *       Typiquement négligeable car n < 1000.
             * @note Ne vérifie PAS si les fonctions sont non-null — responsabilité de l'appelant.
             */
            [[nodiscard]] static const Entry* Find(NkTypeId id) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (reg.mEntries[i].typeId == id) {
                        return &reg.mEntries[i];
                    }
                }
                return nullptr;
            }

            /**
             * @brief Trouve une entrée de registre par nom de composant
             * @param name Nom du composant à rechercher
             * @return Pointeur vers l'Entry si trouvée, nullptr sinon
             * @ingroup ComponentRegistryAPI
             *
             * @note Complexité O(n × m) où n = nombre de composants, m = longueur moyenne des noms.
             *       Utiliser Find(NkTypeId) pour les performances critiques.
             * @note Comparaison via strcmp — sensible à la casse.
             */
            [[nodiscard]] static const Entry* FindByName(const char* name) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    if (std::strcmp(reg.mEntries[i].name, name) == 0) {
                        return &reg.mEntries[i];
                    }
                }
                return nullptr;
            }

            /**
             * @brief Trouve une entrée de registre par type template
             * @tparam T Type du composant à rechercher
             * @return Pointeur vers l'Entry si trouvée, nullptr sinon
             * @ingroup ComponentRegistryAPI
             *
             * @note Wrapper pratique autour de Find(NkTypeId) avec NkTypeOf<T>().
             * @note Équivalent à Find(NkTypeOf<T>()) mais plus concis.
             */
            template<typename T>
            [[nodiscard]] static const Entry* Find() noexcept {
                return Find(NkTypeOf<T>());
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : SÉRIALISATION/DÉSÉRIALISATION
            // -----------------------------------------------------------------
            /**
             * @brief Sérialise un composant générique via son NkTypeId
             * @param id Identifiant du type du composant
             * @param obj Pointeur const vers le composant à sérialiser
             * @param out Archive de destination pour le stockage
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryAPI
             *
             * @note Utilise Find(id) pour obtenir les fonctions de sérialisation.
             * @note Retourne false si le type n'est pas enregistré ou si la fonction échoue.
             */
            [[nodiscard]] static nk_bool Serialize(NkTypeId id,
                                                 const void* obj,
                                                 NkArchive& out) noexcept {
                const Entry* e = Find(id);
                return e && e->serialize ? e->serialize(obj, out) : false;
            }

            /**
             * @brief Désérialise un composant générique via son NkTypeId
             * @param id Identifiant du type du composant
             * @param obj Pointeur mutable vers le composant à peupler
             * @param arc Archive source contenant les données
             * @return true si la désérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryAPI
             *
             * @note Utilise Find(id) pour obtenir les fonctions de désérialisation.
             * @note Retourne false si le type n'est pas enregistré ou si la fonction échoue.
             */
            [[nodiscard]] static nk_bool Deserialize(NkTypeId id,
                                                   void* obj,
                                                   const NkArchive& arc) noexcept {
                const Entry* e = Find(id);
                return e && e->deserialize ? e->deserialize(obj, arc) : false;
            }

            /**
             * @brief Sérialise un composant template via son type
             * @tparam T Type du composant à sérialiser
             * @param obj Référence const vers le composant à sérialiser
             * @param out Archive de destination pour le stockage
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryAPI
             *
             * @note Wrapper pratique autour de Serialize(NkTypeId, void*, NkArchive&).
             * @note Équivalent à Serialize(NkTypeOf<T>(), &obj, out) mais plus concis.
             */
            template<typename T>
            [[nodiscard]] static nk_bool Serialize(const T& obj, NkArchive& out) noexcept {
                return Serialize(NkTypeOf<T>(), &obj, out);
            }

            /**
             * @brief Désérialise un composant template via son type
             * @tparam T Type du composant à peupler
             * @param obj Référence mutable vers le composant à peupler
             * @param arc Archive source contenant les données
             * @return true si la désérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryAPI
             *
             * @note Wrapper pratique autour de Deserialize(NkTypeId, void*, NkArchive&).
             * @note Équivalent à Deserialize(NkTypeOf<T>(), &obj, arc) mais plus concis.
             */
            template<typename T>
            [[nodiscard]] static nk_bool Deserialize(T& obj, const NkArchive& arc) noexcept {
                return Deserialize(NkTypeOf<T>(), &obj, arc);
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : ITÉRATION ET INTERROGATION
            // -----------------------------------------------------------------
            /**
             * @brief Applique une fonction à chaque entrée du registre
             * @tparam Fn Type de la fonction à appliquer (doit accepter const Entry&)
             * @param fn Fonction à appliquer à chaque entrée
             * @ingroup ComponentRegistryAPI
             *
             * @note Utile pour le debugging, la sauvegarde globale, ou l'inspection.
             * @note La fonction ne doit pas modifier le registre pendant l'itération.
             *
             * @example
             * @code
             * nkentseu::NkComponentRegistry::ForEach([](const auto& entry) {
             *     printf("Composant: %s (ID: %llu)\n", entry.name, entry.typeId);
             * });
             * @endcode
             */
            template<typename Fn>
            static void ForEach(Fn&& fn) noexcept {
                const auto& reg = Global();
                for (nk_size i = 0; i < reg.mEntries.Size(); ++i) {
                    fn(reg.mEntries[i]);
                }
            }

            /**
             * @brief Obtient le nombre de composants enregistrés
             * @return Nombre d'entrées dans le registre
             * @ingroup ComponentRegistryAPI
             *
             * @note Utile pour dimensionner des buffers ou pour le profiling.
             * @note Complexité O(1) — retour direct de la taille du vector.
             */
            [[nodiscard]] static nk_uint32 Count() noexcept {
                return static_cast<nk_uint32>(Global().mEntries.Size());
            }


            // -----------------------------------------------------------------
            // SECTION 5 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
            // -----------------------------------------------------------------
        private:


            // -----------------------------------------------------------------
            // MÉTHODES PRIVÉES : FONCTIONS DE FALLBACK
            // -----------------------------------------------------------------
            /**
             * @brief Fallback de sérialisation générique pour les types non-enregistrés
             * @tparam T Type du composant à sérialiser
             * @param obj Pointeur const vers le composant à sérialiser
             * @param out Archive de destination pour le stockage
             * @return true si la sérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryInternals
             *
             * Stratégie de fallback :
             *  1. Si T dérive de NkISerializable : délégation à obj->Serialize(out)
             *  2. Sinon : sérialisation binaire brute en représentation hexadécimale
             *     - Stockage de sizeof(T) bytes sous forme hexadécimale
             *     - Clés "__raw__" et "__rawSz__" pour identification
             *
             * @note La représentation hexadécimale est portable mais non-lisible.
             *       Pour une sérialisation automatique lisible, utiliser NkReflect.
             * @note Complexité O(sizeof(T)) — acceptable pour des composants petits.
             *
             * @example Sortie pour un float{3.14f} :
             * @code
             * // __raw__: "4048F5C3" (représentation IEEE 754 en hex)
             * // __rawSz__: 4
             * @endcode
             */
            template<typename T>
            static nk_bool DefaultSerialize(const void* obj, NkArchive& out) noexcept {
                // Délégation à NkISerializable si applicable
                if constexpr (traits::NkIsBaseOf<NkISerializable, T>::value) {
                    return static_cast<const T*>(obj)->Serialize(out);
                }
                // Fallback : sérialisation binaire brute en hexadécimal
                const nk_uint8* raw = static_cast<const nk_uint8*>(obj);
                NkString hex;
                hex.Reserve(sizeof(T) * 2u + 8u);
                static const char hx[] = "0123456789ABCDEF";
                for (nk_size i = 0; i < sizeof(T); ++i) {
                    hex.Append(hx[(raw[i] >> 4u) & 0xFu]);
                    hex.Append(hx[raw[i] & 0xFu]);
                }
                out.SetString("__raw__", hex.View());
                out.SetUInt64("__rawSz__", sizeof(T));
                return true;
            }

            /**
             * @brief Fallback de désérialisation générique pour les types non-enregistrés
             * @tparam T Type du composant à peupler
             * @param obj Pointeur mutable vers le composant à peupler
             * @param arc Archive source contenant les données
             * @return true si la désérialisation a réussi, false en cas d'erreur
             * @ingroup ComponentRegistryInternals
             *
             * Stratégie de fallback :
             *  1. Si T dérive de NkISerializable : délégation à obj->Deserialize(arc)
             *  2. Sinon : lecture de la représentation hexadécimale et conversion binaire
             *     - Vérification que __rawSz__ == sizeof(T)
             *     - Conversion hexadécimal → bytes bruts
             *
             * @note La représentation hexadécimale doit correspondre exactement à sizeof(T).
             * @note Échec silencieux si les données sont corrompues ou de mauvaise taille.
             *
             * @example Input pour un float :
             * @code
             * // __raw__: "4048F5C3"
             * // __rawSz__: 4
             * // Résultat : obj pointe vers un float contenant ~3.14f
             * @endcode
             */
            template<typename T>
            static nk_bool DefaultDeserialize(void* obj, const NkArchive& arc) noexcept {
                // Délégation à NkISerializable si applicable
                if constexpr (traits::NkIsBaseOf<NkISerializable, T>::value) {
                    return static_cast<T*>(obj)->Deserialize(arc);
                }
                NkString hex;
                nk_uint64 sz = 0;
                if (!arc.GetString("__raw__", hex) || !arc.GetUInt64("__rawSz__", sz)) {
                    return false;
                }
                if (sz != sizeof(T) || hex.Length() != sizeof(T) * 2u) {
                    return false;
                }
                nk_uint8* raw = static_cast<nk_uint8*>(obj);
                for (nk_size i = 0; i < sizeof(T); ++i) {
                    auto hexDigit = [](char c) -> nk_uint8 {
                        if (c >= '0' && c <= '9') {
                            return static_cast<nk_uint8>(c - '0');
                        }
                        if (c >= 'A' && c <= 'F') {
                            return static_cast<nk_uint8>(c - 'A' + 10);
                        }
                        if (c >= 'a' && c <= 'f') {
                            return static_cast<nk_uint8>(c - 'a' + 10);
                        }
                        return 0u;
                    };
                    raw[i] = static_cast<nk_uint8>(
                        (hexDigit(hex[i * 2u]) << 4u) | hexDigit(hex[i * 2u + 1u]));
                }
                return true;
            }


            // -----------------------------------------------------------------
            // VARIABLES MEMBRES PRIVÉES
            // -----------------------------------------------------------------
            /// @brief Registre interne des composants : vector d'entrées
            /// @note Recherche linéaire : optimisée pour <1000 composants
            NkVector<Entry> mEntries;


        }; // class NkComponentRegistry


    } // namespace nkentseu


    // =============================================================================
    // MACROS : ENREGISTREMENT AUTOMATIQUE DE COMPOSANTS
    // =============================================================================
    /**
     * @defgroup ComponentRegistryMacros Macros d'Enregistrement de Composants
     * @brief Macros pour l'enregistrement automatique des composants ECS
     *
     * Ces macros simplifient l'enregistrement des composants :
     *  - Évitent les erreurs de nommage entre Type et son NkTypeId
     *  - Garantissent l'enregistrement même si le composant n'est pas utilisé ailleurs
     *  - Centralisent l'enregistrement près de la définition du type
     */

    /**
     * @def NK_REGISTER_COMPONENT(Type, Name)
     * @brief Macro pour l'enregistrement automatique d'un composant simple
     * @param Type Type C++ du composant (doit être complet)
     * @param Name Nom lisible du composant (string littéral)
     * @ingroup ComponentRegistryMacros
     *
     * Usage :
     *  - Placer dans un fichier .cpp (jamais dans un header)
     *  - Après la définition complète du type Type
     *  - Utilise les fonctions de fallback DefaultSerialize/DefaultDeserialize
     *
     * Mécanisme :
     *  - Crée un objet static avec un constructeur qui appelle Register<Type>()
     *  - L'objet est construit avant main() (initialisation statique)
     *  - Le nom utilisé est le paramètre Name (pas de stringification automatique)
     *
     * @warning ⚠️ À placer UNIQUEMENT dans un .cpp, jamais dans un header !
     *          Sinon : définitions multiples, ODR violation, comportement indéfini.
     *
     * @example
     * @code
     * // Dans Position.cpp (après la définition de la classe) :
     * #include "ECS/Components/Position.h"
     * #include "NKSerialization/NkComponentRegistry.h"
     *
     * // ... implémentation des méthodes de Position ...
     *
     * // Enregistrement automatique — une seule fois, dans ce .cpp :
     * NK_REGISTER_COMPONENT(Position, "Position");
     * @endcode
     */
    #define NK_REGISTER_COMPONENT(Type, Name)                                    \
    namespace { struct _NkCompReg_##Type {                                       \
        _NkCompReg_##Type() noexcept {                                           \
            ::nkentseu::NkComponentRegistry::Register<Type>(Name);               \
        }                                                                        \
    } _sNkCompReg_##Type; }

    /**
     * @def NK_REGISTER_COMPONENT_CUSTOM(Type, Name, SerFn, DeserFn)
     * @brief Macro pour l'enregistrement avec fonctions de sérialisation custom
     * @param Type Type C++ du composant
     * @param Name Nom lisible du composant
     * @param SerFn Pointeur vers fonction de sérialisation custom
     * @param DeserFn Pointeur vers fonction de désérialisation custom
     * @ingroup ComponentRegistryMacros
     *
     * Signature des fonctions :
     *  - SerFn : nk_bool (*)(const void*, NkArchive&) noexcept
     *  - DeserFn : nk_bool (*)(void*, const NkArchive&) noexcept
     *
     * Usage :
     *  - Placer dans un fichier .cpp (jamais dans un header)
     *  - Après la définition des fonctions SerFn/DeserFn
     *  - Pour les composants nécessitant une logique de sérialisation spécifique
     *
     * @warning ⚠️ À placer UNIQUEMENT dans un .cpp, jamais dans un header !
     *          Sinon : définitions multiples, ODR violation, comportement indéfini.
     *
     * @example
     * @code
     * // Fonctions custom
     * nk_bool SerializePosition(const void* obj, nkentseu::NkArchive& out) noexcept {
     *     const Position* pos = static_cast<const Position*>(obj);
     *     return out.SetFloat32("x", pos->x)
     *         && out.SetFloat32("y", pos->y)
     *         && out.SetFloat32("z", pos->z);
     * }
     *
     * nk_bool DeserializePosition(void* obj, const nkentseu::NkArchive& arc) noexcept {
     *     Position* pos = static_cast<Position*>(obj);
     *     return arc.GetFloat32("x", pos->x)
     *         && arc.GetFloat32("y", pos->y)
     *         && arc.GetFloat32("z", pos->z);
     * }
     *
     * // Enregistrement dans Position.cpp :
     * NK_REGISTER_COMPONENT_CUSTOM(Position, "Position", SerializePosition, DeserializePosition);
     * @endcode
     */
    #define NK_REGISTER_COMPONENT_CUSTOM(Type, Name, SerFn, DeserFn)             \
    namespace { struct _NkCompReg_##Type {                                       \
        _NkCompReg_##Type() noexcept {                                           \
            ::nkentseu::NkComponentRegistry::Register<Type>(Name, SerFn, DeserFn);\
        }                                                                        \
    } _sNkCompReg_##Type; }


#endif // NKENTSEU_SERIALIZATION_NKCOMPONENTREGISTRY_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKCOMPONENTREGISTRY.H
// =============================================================================
// Ce fichier fournit le registre central pour les composants ECS sérialisables.
// Il combine enregistrement automatique, fallback générique et intégration NkISerializable.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Composant simple avec enregistrement automatique
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkComponentRegistry.h>

    // Définition du composant
    struct Velocity {
        float x, y, z;
    };

    // Enregistrement dans Velocity.cpp :
    // NK_REGISTER_COMPONENT(Velocity, "Velocity");

    void SaveVelocityComponent() {
        Velocity vel{1.0f, 0.0f, 0.5f};
        nkentseu::NkArchive archive;

        // Sérialisation automatique via fallback
        if (nkentseu::NkComponentRegistry::Serialize(vel, archive)) {
            // archive contient : __raw__="3F800000000000003F000000", __rawSz__=12
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Composant avec sérialisation custom lisible
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkComponentRegistry.h>

    struct Transform {
        float position[3];
        float rotation[4];  // quaternion
        float scale;
    };

    // Fonctions de sérialisation custom
    nk_bool SerializeTransform(const void* obj, nkentseu::NkArchive& out) noexcept {
        const Transform* t = static_cast<const Transform*>(obj);
        return out.SetFloat32Array("position", t->position, 3)
            && out.SetFloat32Array("rotation", t->rotation, 4)
            && out.SetFloat32("scale", t->scale);
    }

    nk_bool DeserializeTransform(void* obj, const nkentseu::NkArchive& arc) noexcept {
        Transform* t = static_cast<Transform*>(obj);
        return arc.GetFloat32Array("position", t->position, 3)
            && arc.GetFloat32Array("rotation", t->rotation, 4)
            && arc.GetFloat32("scale", t->scale);
    }

    // Enregistrement dans Transform.cpp :
    // NK_REGISTER_COMPONENT_CUSTOM(Transform, "Transform", SerializeTransform, DeserializeTransform);

    void SaveTransformComponent() {
        Transform t{{1,2,3}, {0,0,0,1}, 1.0f};
        nkentseu::NkArchive archive;

        if (nkentseu::NkComponentRegistry::Serialize(t, archive)) {
            // archive contient des champs lisibles : position=[1,2,3], rotation=[0,0,0,1], scale=1.0
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Composant dérivant de NkISerializable
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkComponentRegistry.h>
    #include <NKSerialization/NkISerializable.h>

    class HealthComponent : public nkentseu::NkISerializable {
    public:
        int current;
        int max;

        nk_bool Serialize(nkentseu::NkArchive& archive) const override {
            return archive.SetInt32("current", current)
                && archive.SetInt32("max", max);
        }

        nk_bool Deserialize(const nkentseu::NkArchive& archive) override {
            return archive.GetInt32("current", current)
                && archive.GetInt32("max", max);
        }

        const char* GetTypeName() const noexcept override {
            return "HealthComponent";
        }
    };

    // Enregistrement dans HealthComponent.cpp :
    // NK_REGISTER_COMPONENT(HealthComponent, "Health");

    void SaveHealthComponent() {
        HealthComponent health{50, 100};
        nkentseu::NkArchive archive;

        // La sérialisation délègue automatiquement à HealthComponent::Serialize()
        if (nkentseu::NkComponentRegistry::Serialize(health, archive)) {
            // archive contient : current=50, max=100
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Chargement dynamique de composants inconnus
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkComponentRegistry.h>

    void LoadUnknownComponent(nkentseu::NkArchive& archive, const char* componentName) {
        // Recherche par nom
        const auto* entry = nkentseu::NkComponentRegistry::FindByName(componentName);
        if (!entry) {
            printf("Composant '%s' non enregistré\n", componentName);
            return;
        }

        // Allocation dynamique (exemple simplifié)
        void* component = malloc(entry->typeId);  // Simplifié - utiliser un allocateur réel

        // Désérialisation générique
        if (nkentseu::NkComponentRegistry::Deserialize(entry->typeId, component, archive)) {
            printf("Composant '%s' chargé avec succès\n", componentName);
            // Utiliser component...
        } else {
            printf("Échec de la désérialisation de '%s'\n", componentName);
        }

        free(component);
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Inspection et debugging du registre
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkComponentRegistry.h>

    void DebugComponentRegistry() {
        printf("Nombre de composants enregistrés : %u\n",
               nkentseu::NkComponentRegistry::Count());

        nkentseu::NkComponentRegistry::ForEach([](const auto& entry) {
            printf("- %s (ID: %llu)\n", entry.name, entry.typeId);
        });
    }
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================