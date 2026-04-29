// =============================================================================
// NKNetwork/RPC/NkRPC.h
// =============================================================================
// DESCRIPTION :
//   Système de Remote Procedure Calls (RPC) pour l'appel de fonctions
//   à distance entre clients et serveur dans une architecture réseau.
//
// TYPES DE RPC SUPPORTÉS :
//   • ServerRPC     : Appelé côté client → exécuté côté serveur
//                     (ex: actions joueur, commandes de jeu)
//   • ClientRPC     : Appelé côté serveur → exécuté sur un client spécifique
//                     (ex: feedback visuel, sons personnalisés)
//   • MulticastRPC  : Appelé côté serveur → exécuté sur tous les pairs
//                     (ex: effets globaux, broadcast d'événements)
//
// GARANTIES DE LIVRAISON :
//   • Reliable      : Livraison garantie, ordre non préservé
//   • ReliableOrd   : Livraison garantie + ordre d'exécution préservé
//   • Unreliable    : Meilleur effort, perte possible (optimisation latence)
//
// SÉCURITÉ ET VALIDATION :
//   • Les ServerRPC sont toujours validés côté serveur avant exécution
//   • Le serveur peut rejeter un RPC malformé sans crash ni effet de bord
//   • Les paramètres sont sérialisés via NkBitStream avec vérification de borne
//
// ENREGISTREMENT DES RPC (via macros) :
//   NK_RPC_SERVER(MyClass, OpenDoor, uint32 doorId, float32 force)
//   NK_RPC_CLIENT(MyClass, PlaySound, const char* sfxName)
//   NK_RPC_MULTICAST(MyClass, Explode, NkVec3f pos, float32 radius)
//
// USAGE TYPIQUE :
//   // Client → Serveur : demande d'ouverture de porte
//   mRPCRouter.CallServer("MyClass::OpenDoor", doorId, force);
//
//   // Serveur → Client : feedback sonore pour un joueur
//   mRPCRouter.CallClient(ownerPeerId, "MyClass::PlaySound", "door_open.wav");
//
//   // Serveur → Tous : effet d'explosion visible par tous
//   mRPCRouter.Multicast("MyClass::Explode", bombPos, 5.f);
//
// DÉPENDANCES :
//   • NkConnection.h   : Gestion des connexions peer-to-peer
//   • NkBitStream.h    : Sérialisation binaire compacte des arguments
//   • NkNetDefines.h   : Types fondamentaux et codes de retour réseau
//
// RÈGLES D'UTILISATION :
//   • Enregistrer tous les RPC au démarrage via Register() ou les macros
//   • Utiliser des noms uniques "ClassName::FunctionName" pour éviter les collisions
//   • Valider systématiquement les paramètres des ServerRPC côté serveur
//   • Privilégier ReliableOrd pour les RPC critiques, Unreliable pour les effets
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKRPC_H
#define NKENTSEU_NETWORK_NKRPC_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKNetwork/Protocol/NkBitStream.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKLogger/NkLog.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // ÉNUMÉRATION : NkRPCType — Direction d'exécution du RPC
            // =================================================================
            /**
             * @enum NkRPCType
             * @brief Identifie la direction et la cible d'exécution d'un appel RPC.
             *
             * Cette énumération contrôle le routage des appels de fonctions à distance :
             *   • NK_SERVER_RPC     : Client → Serveur (autorité serveur)
             *   • NK_CLIENT_RPC     : Serveur → Client unique (feedback personnalisé)
             *   • NK_MULTI_CAST_RPC : Serveur → Tous les pairs (broadcast global)
             *
             * @note Le type détermine également les permissions : seul le serveur
             *       peut initier des ClientRPC et MulticastRPC.
             * @see NkRPCRouter::CallServer() / CallClient() / Multicast()
             */
            enum class NkRPCType : uint8
            {
                /// RPC serveur : exécuté sur le serveur après appel depuis un client.
                /// Usage : validation d'actions, mise à jour d'état autorisé.
                NK_SERVER_RPC = 0,

                /// RPC client : exécuté sur un client spécifique après appel serveur.
                /// Usage : effets visuels/sonores personnalisés, UI locale.
                NK_CLIENT_RPC,

                /// RPC multicast : exécuté sur tous les pairs connectés.
                /// Usage : événements globaux, synchronisation d'état partagé.
                NK_MULTI_CAST_RPC
            };

            // =================================================================
            // ÉNUMÉRATION : NkRPCReliability — Garanties de livraison
            // =================================================================
            /**
             * @enum NkRPCReliability
             * @brief Niveau de garantie de livraison pour un appel RPC.
             *
             * Compromis entre fiabilité et latence selon le type de donnée :
             *   • NK_RELIABLE     : Livraison garantie, ordre non préservé
             *   • NK_RELIABLE_ORD : Livraison garantie + ordre d'arrivée préservé
             *   • NK_UNRELIABLE   : Meilleur effort, perte possible (latence minimale)
             *
             * @note NK_RELIABLE_ORD utilise le canal ReliableOrdered de NkNetChannel
             * @note NK_UNRELIABLE est adapté aux effets non-critiques (sons, particules)
             * @see NkNetChannel Pour l'implémentation sous-jacente des garanties
             */
            enum class NkRPCReliability : uint8
            {
                /// Livraison garantie via ACK, réordonnancement possible.
                /// Usage : données importantes où l'ordre n'est pas critique.
                NK_RELIABLE = 0,

                /// Livraison garantie + ordre strict d'exécution préservé.
                /// Usage : commandes de jeu, séquences d'événements dépendantes.
                NK_RELIABLE_ORD,

                /// Envoi UDP pur sans ACK — perte possible mais latence minimale.
                /// Usage : effets visuels/sonores non-critiques, interpolation.
                NK_UNRELIABLE
            };

            // =================================================================
            // STRUCTURE : NkRPCDescriptor — Métadonnées d'un RPC enregistré
            // =================================================================
            /**
             * @struct NkRPCDescriptor
             * @brief Description complète d'un handler RPC pour enregistrement et dispatch.
             *
             * Chaque RPC enregistré dans le système est représenté par un descriptor
             * contenant toutes les métadonnées nécessaires au routage et à l'exécution :
             *   • Nom unique pour identification humaine et debugging
             *   • ID hashé pour lookup rapide dans la table de routage
             *   • Type et fiabilité pour configuration du canal de transport
             *   • Handler fonctionnel pour invocation avec paramètres désérialisés
             *
             * @note Le nom doit suivre le format "ClassName::FunctionName" pour unicité
             * @note L'ID est un hash 32-bit stable : même nom → même ID sur toutes les plateformes
             * @threadsafe Lecture seule thread-safe après enregistrement initial
             *
             * @example
             * @code
             * // Enregistrement manuel d'un RPC serveur
             * NkRPCDescriptor desc;
             * std::strcpy(desc.name, "Player::Jump");
             * desc.id = NkRPCRouter::HashRPCName("Player::Jump");
             * desc.type = NkRPCType::NK_SERVER_RPC;
             * desc.reliability = NkRPCReliability::NK_RELIABLE_ORD;
             * desc.handler = [](NkPeerId caller, NkBitReader& args) {
             *     float32 force;
             *     args.ReadF32(force);
             *     player.Jump(force);
             * };
             * router.Register(desc);
             * @endcode
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkRPCDescriptor
            {
                // -------------------------------------------------------------
                // Constantes de configuration
                // -------------------------------------------------------------

                /// Longueur maximale du nom de RPC (inclut le terminateur nul).
                static constexpr uint32 kMaxNameLen = 128u;

                // -------------------------------------------------------------
                // Métadonnées d'identification
                // -------------------------------------------------------------

                /// Nom lisible du RPC au format "ClassName::FunctionName".
                /// @note Utilisé pour logging, debugging et résolution initiale.
                char name[kMaxNameLen] = {};

                /// ID hashé du nom pour lookup rapide en O(1).
                /// @note Généré via HashRPCName() — stable et déterministe.
                uint32 id = 0;

                /// Direction d'exécution du RPC (Server/Client/Multicast).
                NkRPCType type = NkRPCType::NK_SERVER_RPC;

                /// Niveau de garantie de livraison pour le transport réseau.
                NkRPCReliability reliability = NkRPCReliability::NK_UNRELIABLE;

                // -------------------------------------------------------------
                // Handler fonctionnel — point d'entrée d'exécution
                // -------------------------------------------------------------

                /**
                 * @typedef HandlerFn
                 * @brief Signature des fonctions handlers pour traitement des RPC.
                 * @param caller Identifiant du pair ayant initié l'appel (pour autorisation).
                 * @param args Lecteur de bitstream contenant les paramètres désérialisés.
                 * @note Le handler est responsable de la désérialisation complète des args.
                 * @note Toute exception dans le handler doit être catchée pour éviter un crash.
                 */
                using HandlerFn = NkFunction<void(NkPeerId caller, NkBitReader& args)>;

                /// Fonction appelée lors de la réception et validation du RPC.
                HandlerFn handler;
            };

            // =================================================================
            // CLASSE : NkRPCRouter — Registre et dispatch des appels RPC
            // =================================================================
            /**
             * @class NkRPCRouter
             * @brief Système centralisé d'enregistrement, sérialisation et routage des RPC.
             *
             * Cette classe fournit l'infrastructure complète pour les appels de fonctions
             * à distance dans une architecture client-serveur ou peer-to-peer :
             *   • Registre de handlers avec lookup par hash pour performance
             *   • Sérialisation type-safe des arguments via NkBitStream
             *   • Routage intelligent selon le type de RPC (Server/Client/Multicast)
             *   • Gestion des garanties de livraison via NkRPCReliability
             *
             * CYCLE DE VIE TYPIQUE :
             * @code
             * // 1. Initialisation au démarrage
             * NkRPCRouter router;
             * router.SetConnectionManager(&connectionManager);
             *
             * // 2. Enregistrement des RPC (côté serveur ET clients concernés)
             * router.Register("Player::Jump", NkRPCType::NK_SERVER_RPC,
             *     [](NkPeerId caller, NkBitReader& args) {
             *         float32 force; args.ReadF32(force);
             *         GetPlayer(caller).Jump(force);
             *     }, NkRPCReliability::NK_RELIABLE_ORD);
             *
             * // 3. Appel depuis un client → exécution serveur
             * router.CallServer("Player::Jump", 10.f);
             *
             * // 4. Réception automatique via OnRPCReceived() (appelé par NkConnection)
             * //    Le router dispatch vers le handler approprié
             * @endcode
             *
             * @note Thread-safe en lecture seule après initialisation — protéger les
             *       appels à Register() avec un mutex si accès concurrent au démarrage.
             * @see NkConnectionManager Pour l'intégration avec le système de connexion
             * @see NkBitStream Pour la sérialisation compacte des arguments
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkRPCRouter
            {
            public:

                // -------------------------------------------------------------
                // Constructeur et destructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un routeur vide.
                 * @note La table de descriptors est pré-allouée mais vide (mCount = 0).
                 * @note Appeler SetConnectionManager() avant tout appel à Call*().
                 */
                NkRPCRouter() noexcept = default;

                /**
                 * @brief Destructeur par défaut — libération automatique des ressources.
                 * @note Aucune allocation heap dans cette classe — destruction triviale.
                 */
                ~NkRPCRouter() noexcept = default;

                // -------------------------------------------------------------
                // Enregistrement des handlers RPC
                // -------------------------------------------------------------

                /**
                 * @brief Enregistre un handler RPC dans la table de routage.
                 * @param name Nom unique du RPC au format "ClassName::FunctionName".
                 * @param type Direction d'exécution (ServerRPC/ClientRPC/MulticastRPC).
                 * @param handler Fonction appelée lors de la réception du RPC.
                 * @param rel Niveau de garantie de livraison (défaut: ReliableOrd).
                 * @return ID hashé du RPC (stable, utilisable pour optimisation).
                 * @note Retourne 0 si la table est pleine (kMaxRPCs = 256 max).
                 * @note L'enregistrement doit être fait avant tout appel Call*().
                 * @warning Le nom doit être identique côté émetteur et récepteur.
                 */
                [[nodiscard]] uint32 Register(
                    const char* name,
                    NkRPCType type,
                    NkRPCDescriptor::HandlerFn handler,
                    NkRPCReliability rel = NkRPCReliability::NK_RELIABLE_ORD
                ) noexcept;

                // -------------------------------------------------------------
                // Interface d'appel — templates variadiques pour sérialisation
                // -------------------------------------------------------------

                /**
                 * @brief Appelle un ServerRPC : client → serveur.
                 * @tparam Args Types des arguments à sérialiser (déduits automatiquement).
                 * @param rpcName Nom du RPC à appeler ("ClassName::FunctionName").
                 * @param args Arguments à sérialiser et transmettre au serveur.
                 * @return NkNetResult::OK en cas d'envoi réussi, code d'erreur sinon.
                 * @note Le handler sera exécuté côté serveur à la réception.
                 * @note Les arguments doivent être sérialisables via NetWrite().
                 * @example
                 * @code
                 * // Client demande au serveur d'ouvrir une porte
                 * router.CallServer("Door::Open", doorId, force);
                 * @endcode
                 */
                template<typename... Args>
                [[nodiscard]] NkNetResult CallServer(
                    const char* rpcName,
                    Args&&... args
                ) noexcept;

                /**
                 * @brief Appelle un ClientRPC : serveur → client spécifique.
                 * @tparam Args Types des arguments à sérialiser (déduits automatiquement).
                 * @param target Identifiant du client destinataire (NkPeerId).
                 * @param rpcName Nom du RPC à appeler ("ClassName::FunctionName").
                 * @param args Arguments à sérialiser et transmettre au client.
                 * @return NkNetResult::OK en cas d'envoi réussi, code d'erreur sinon.
                 * @note Uniquement valide côté serveur (ou pair autorisé).
                 * @example
                 * @code
                 * // Serveur envoie un feedback sonore à un joueur
                 * router.CallClient(playerId, "Audio::Play", "jump.wav");
                 * @endcode
                 */
                template<typename... Args>
                [[nodiscard]] NkNetResult CallClient(
                    NkPeerId target,
                    const char* rpcName,
                    Args&&... args
                ) noexcept;

                /**
                 * @brief Appelle un MulticastRPC : serveur → tous les pairs.
                 * @tparam Args Types des arguments à sérialiser (déduits automatiquement).
                 * @param rpcName Nom du RPC à appeler ("ClassName::FunctionName").
                 * @param args Arguments à sérialiser et broadcaster à tous.
                 * @return NkNetResult::OK en cas d'envoi réussi, code d'erreur sinon.
                 * @note Uniquement valide côté serveur (ou pair autorisé).
                 * @example
                 * @code
                 * // Serveur broadcast une explosion visible par tous
                 * router.Multicast("Effect::Explode", position, radius);
                 * @endcode
                 */
                template<typename... Args>
                [[nodiscard]] NkNetResult Multicast(
                    const char* rpcName,
                    Args&&... args
                ) noexcept;

                // -------------------------------------------------------------
                // Interface de réception — dispatch des RPC entrants
                // -------------------------------------------------------------

                /**
                 * @brief Traite un paquet RPC brut reçu du réseau.
                 * @param caller Identifiant du pair ayant envoyé le paquet.
                 * @param data Pointeur vers le buffer contenant le RPC sérialisé.
                 * @param size Taille des données en bytes.
                 * @note Extrait l'ID du RPC, trouve le handler, appelle avec args désérialisés.
                 * @note Gère les erreurs de désérialisation sans crash (logging + skip).
                 * @note Appelée automatiquement par NkConnection::OnReceived().
                 */
                void OnRPCReceived(
                    NkPeerId caller,
                    const uint8* data,
                    uint32 size
                ) noexcept;

                // -------------------------------------------------------------
                // Intégration avec NkConnectionManager
                // -------------------------------------------------------------

                /**
                 * @brief Définit le gestionnaire de connexion pour l'envoi des RPC.
                 * @param cm Pointeur vers le NkConnectionManager à utiliser.
                 * @note Requis avant tout appel à CallServer/CallClient/Multicast.
                 * @note Le pointeur n'est pas possédé — durée de vie gérée par l'appelant.
                 */
                void SetConnectionManager(NkConnectionManager* cm) noexcept;

            private:

                // -------------------------------------------------------------
                // Implémentation interne — sérialisation et envoi
                // -------------------------------------------------------------

                /**
                 * @brief Implémentation commune des méthodes Call*() templates.
                 * @tparam Args Types des arguments à sérialiser.
                 * @param type Direction du RPC pour routage.
                 * @param target Cible de l'envoi (Server/Client/Invalid pour multicast).
                 * @param rpcName Nom du RPC pour résolution de descriptor.
                 * @param args Arguments à sérialiser dans le paquet.
                 * @return NkNetResult de l'opération d'envoi.
                 * @note Non exposé publiquement — appelé par les wrappers templates.
                 */
                template<typename... Args>
                [[nodiscard]] NkNetResult CallInternal(
                    NkRPCType type,
                    NkPeerId target,
                    const char* rpcName,
                    Args&&... args
                ) noexcept;

                /**
                 * @brief Sérialisation récursive des arguments via fold expression.
                 * @tparam T Type de l'argument courant.
                 * @tparam Rest Types des arguments restants.
                 * @param w Writer BitStream pour écriture binaire.
                 * @param arg Argument courant à sérialiser.
                 * @param rest Arguments restants à traiter récursivement.
                 * @note Base case : surcharge vide pour terminer la récursion.
                 */
                template<typename T, typename... Rest>
                void SerializeArgs(
                    NkBitWriter& w,
                    T&& arg,
                    Rest&&... rest
                ) noexcept;

                /// Cas de base : fin de la récursion pour SerializeArgs.
                void SerializeArgs(NkBitWriter& w) noexcept;

                // -------------------------------------------------------------
                // Helpers de sérialisation par type primitif/composé
                // -------------------------------------------------------------

                /// Sérialise un booléen (1 bit).
                void NetWrite(NkBitWriter& w, bool v) noexcept;

                /// Sérialise un uint8 (8 bits).
                void NetWrite(NkBitWriter& w, uint8 v) noexcept;

                /// Sérialise un uint16 (16 bits).
                void NetWrite(NkBitWriter& w, uint16 v) noexcept;

                /// Sérialise un uint32 (32 bits).
                void NetWrite(NkBitWriter& w, uint32 v) noexcept;

                /// Sérialise un int32 (32 bits signés).
                void NetWrite(NkBitWriter& w, int32 v) noexcept;

                /// Sérialise un float32 (IEEE 754, 32 bits).
                void NetWrite(NkBitWriter& w, float32 v) noexcept;

                /// Sérialise un vecteur 3D (3 × float32).
                void NetWrite(NkBitWriter& w, const math::NkVec3f& v) noexcept;

                /// Sérialise une chaîne C-style (longueur + données).
                void NetWrite(NkBitWriter& w, const char* s) noexcept;

                /// Sérialise un NkString (délégation vers CStr()).
                void NetWrite(NkBitWriter& w, const NkString& s) noexcept;

                // -------------------------------------------------------------
                // Utilitaires de lookup et hash
                // -------------------------------------------------------------

                /**
                 * @brief Calcule un hash 32-bit stable pour un nom de RPC.
                 * @param name Chaîne "ClassName::FunctionName" à hasher.
                 * @return Hash 32-bit utilisable comme ID de lookup.
                 * @note Algorithme déterministe : même entrée → même sortie sur toutes les plateformes.
                 * @note Utilisé pour convertir les noms lisibles en IDs compacts pour le réseau.
                 */
                [[nodiscard]] static uint32 HashRPCName(const char* name) noexcept;

                /**
                 * @brief Recherche un descriptor par ID hashé (version const).
                 * @param id Hash du nom de RPC à rechercher.
                 * @return Pointeur vers le descriptor si trouvé, nullptr sinon.
                 * @note Complexité O(n) avec n ≤ kMaxRPCs (256) — acceptable pour usage rare.
                 */
                [[nodiscard]] const NkRPCDescriptor* FindDescriptor(uint32 id) const noexcept;

                /**
                 * @brief Recherche un descriptor par ID hashé (version mutable).
                 * @param id Hash du nom de RPC à rechercher.
                 * @return Pointeur vers le descriptor si trouvé, nullptr sinon.
                 * @note Permet la modification du descriptor trouvé si nécessaire.
                 */
                [[nodiscard]] NkRPCDescriptor* FindDescriptor(uint32 id) noexcept;

                // -------------------------------------------------------------
                // Membres privés — registre et état interne
                // -------------------------------------------------------------

                /// Nombre maximal de RPC enregistrables (limite compile-time).
                static constexpr uint32 kMaxRPCs = 256u;

                /// Table statique des descriptors enregistrés (pré-allouée).
                NkRPCDescriptor mDescriptors[kMaxRPCs];

                /// Compteur du nombre de RPC actuellement enregistrés.
                uint32 mCount = 0;

                /// Pointeur vers le gestionnaire de connexion pour l'envoi réseau.
                NkConnectionManager* mConnMgr = nullptr;
            };

            // =================================================================
            // MACROS D'ENREGISTREMENT — Helpers de déclaration dans les classes
            // =================================================================

            /**
             * @macro NK_RPC_SERVER(Class, Func, ...)
             * @brief Déclare un ServerRPC dans une classe avec signature type-safe.
             *
             * Cette macro génère :
             *   • Une méthode placeholder Func_RPC avec les paramètres spécifiés
             *   • Une constante de nom "Class::Func" pour l'enregistrement
             *
             * USAGE RECOMMANDÉ :
             * @code
             * class PlayerController {
             * public:
             *     // Déclaration du RPC avec macro
             *     NK_RPC_SERVER(PlayerController, Jump, float32 force)
             *
             *     // Implémentation côté serveur (à définir séparément)
             *     void Jump_ServerImpl(NkPeerId caller, float32 force);
             * };
             *
             * // Enregistrement au démarrage
             * router.Register(
             *     PlayerController::kJump_RPCName,
             *     NkRPCType::NK_SERVER_RPC,
             *     [](NkPeerId caller, NkBitReader& args) {
             *         float32 force; args.ReadF32(force);
             *         player.Jump_ServerImpl(caller, force);
             *     }
             * );
             * @endcode
             *
             * @param Class Nom de la classe contenant le RPC.
             * @param Func Nom de la fonction à appeler à distance.
             * @param ... Liste des paramètres de la fonction (types et noms).
             */
            #define NK_RPC_SERVER(Class, Func, ...) \
                void Func##_RPC(__VA_ARGS__); \
                static constexpr const char* k##Func##_RPCName = #Class "::" #Func;

            /**
             * @macro NK_RPC_CLIENT(Class, Func, ...)
             * @brief Déclare un ClientRPC dans une classe avec signature type-safe.
             * @param Class Nom de la classe contenant le RPC.
             * @param Func Nom de la fonction à appeler à distance.
             * @param ... Liste des paramètres de la fonction (types et noms).
             * @see NK_RPC_SERVER Pour la sémantique et l'usage
             */
            #define NK_RPC_CLIENT(Class, Func, ...) \
                void Func##_ClientRPC(__VA_ARGS__); \
                static constexpr const char* k##Func##_ClientRPCName = #Class "::" #Func;

            /**
             * @macro NK_RPC_MULTICAST(Class, Func, ...)
             * @brief Déclare un MulticastRPC dans une classe avec signature type-safe.
             * @param Class Nom de la classe contenant le RPC.
             * @param Func Nom de la fonction à appeler à distance.
             * @param ... Liste des paramètres de la fonction (types et noms).
             * @see NK_RPC_SERVER Pour la sémantique et l'usage
             */
            #define NK_RPC_MULTICAST(Class, Func, ...) \
                void Func##_MulticastRPC(__VA_ARGS__); \
                static constexpr const char* k##Func##_MulticastName = #Class "::" #Func;

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKRPC_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKRPC.H
// =============================================================================
// Ce fichier fournit le système RPC pour appels de fonctions à distance.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Enregistrement et appel basique de ServerRPC
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/RPC/NkRPC.h"
    #include "NKCore/Logger/NkLogger.h"

    class DoorController
    {
    public:
        // Déclaration du RPC via macro
        NK_RPC_SERVER(DoorController, Open, uint32 doorId, float32 force)

        // Implémentation côté serveur (validée)
        void Open_ServerImpl(nkentseu::net::NkPeerId caller, uint32 doorId, float32 force)
        {
            // Validation : le joueur peut-il ouvrir cette porte ?
            if (!CanPlayerOpenDoor(caller, doorId))
            {
                NK_LOG_WARN("Joueur {} ne peut pas ouvrir la porte {}", caller.value, doorId);
                return;
            }

            // Exécution de l'action
            OpenDoor(doorId, force);

            // Feedback optionnel au joueur
            // (via ClientRPC — voir exemple 2)
        }
    };

    // Enregistrement au démarrage de l'application
    void RegisterDoorRPCs(nkentseu::net::NkRPCRouter& router)
    {
        using namespace nkentseu::net;

        router.Register(
            DoorController::kOpen_RPCName,  // "DoorController::Open"
            NkRPCType::NK_SERVER_RPC,
            [](NkPeerId caller, NkBitReader& args) {
                uint32 doorId = 0;
                float32 force = 0.f;

                // Désérialisation dans l'ordre de déclaration
                args.ReadU32(doorId);
                args.ReadF32(force);

                // Dispatch vers l'implémentation
                DoorController controller;
                controller.Open_ServerImpl(caller, doorId, force);
            },
            NkRPCReliability::NK_RELIABLE_ORD  // Ordre important pour les portes
        );
    }

    // Appel depuis un client
    void PlayerInput::RequestOpenDoor(uint32 doorId)
    {
        using namespace nkentseu::net;

        // Le RPC est envoyé au serveur pour validation/exécution
        mRPCRouter.CallServer(
            DoorController::kOpen_RPCName,
            doorId,
            mInteractionForce  // Paramètre supplémentaire
        );
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : ClientRPC pour feedback personnalisé
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/RPC/NkRPC.h"

    class AudioManager
    {
    public:
        // RPC pour jouer un son sur un client spécifique
        NK_RPC_CLIENT(AudioManager, PlaySFX, const char* sfxName, float32 volume)

        // Implémentation côté client (exécution locale)
        void PlaySFX_ClientImpl(const char* sfxName, float32 volume)
        {
            // Jouer le son localement — pas d'impact réseau
            mAudioSystem.Play(sfxName, volume);
        }
    };

    // Usage côté serveur : notifier un joueur d'un événement
    void Server::OnPlayerPickedUpItem(NkPeerId playerId, const Item& item)
    {
        using namespace nkentseu::net;

        // Feedback sonore uniquement pour le joueur concerné
        mRPCRouter.CallClient(
            playerId,
            AudioManager::kPlaySFX_ClientRPCName,
            item.pickupSound.CStr(),  // Conversion NkString → const char*
            1.0f  // Volume normal
        );

        // Note : les autres joueurs n'entendent pas ce son (optimisation bande passante)
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : MulticastRPC pour effets globaux
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/RPC/NkRPC.h"
    #include "NKMath/NkVec3.h"

    class EffectSystem
    {
    public:
        // RPC broadcast pour effets visibles par tous
        NK_RPC_MULTICAST(EffectSystem, SpawnExplosion,
                         nkentseu::math::NkVec3f position,
                         float32 radius,
                         uint32 color)

        // Implémentation exécutée sur tous les pairs
        void SpawnExplosion_Multicast(
            nkentseu::math::NkVec3f position,
            float32 radius,
            uint32 color
        )
        {
            // Création locale de l'effet — synchronisé sur tous les clients
            ParticleSystem::SpawnExplosion(position, radius, color);
        }
    };

    // Usage côté serveur : broadcast d'une explosion
    void Server::TriggerExplosion(const nkentseu::math::NkVec3f& pos, float32 radius)
    {
        using namespace nkentseu::net;

        // Tous les clients reçoivent et affichent l'effet
        mRPCRouter.Multicast(
            EffectSystem::kSpawnExplosion_MulticastName,
            pos,
            radius,
            0xFFFF0000  // Couleur rouge en RGBA
        );
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des erreurs et validation côté serveur
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/RPC/NkRPC.h"
    #include "NKCore/Logger/NkLogger.h"

    void SafeServerRPCHandler(
        nkentseu::net::NkPeerId caller,
        nkentseu::net::NkBitReader& args,
        const char* rpcName,
        std::function<void()> validatedAction
    )
    {
        // Protection contre les exceptions dans le handler
        try
        {
            // Validation basique des paramètres (à adapter selon le RPC)
            if (!args.IsValid())
            {
                NK_LOG_ERROR("RPC {} : BitReader invalide depuis {}", rpcName, caller.value);
                return;
            }

            // Exécution de l'action validée
            validatedAction();
        }
        catch (const std::exception& e)
        {
            NK_LOG_ERROR("RPC {} : Exception depuis {}: {}", rpcName, caller.value, e.what());
            // Ne pas propager — éviter un crash serveur
        }
        catch (...)
        {
            NK_LOG_ERROR("RPC {} : Exception inconnue depuis {}", rpcName, caller.value);
        }
    }

    // Usage dans l'enregistrement d'un RPC
    router.Register(
        "Player::Attack",
        nkentseu::net::NkRPCType::NK_SERVER_RPC,
        [](nkentseu::net::NkPeerId caller, nkentseu::net::NkBitReader& args) {
            SafeServerRPCHandler(caller, args, "Player::Attack", [&]() {
                uint32 targetId = 0;
                args.ReadU32(targetId);

                if (IsValidTarget(caller, targetId)) {
                    PerformAttack(caller, targetId);
                } else {
                    NK_LOG_WARN("Attaque invalide : {} → {}", caller.value, targetId);
                }
            });
        }
    );
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Optimisation avec fiabilité adaptative
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/RPC/NkRPC.h"

    void AdaptiveRPCSend(
        nkentseu::net::NkRPCRouter& router,
        const char* rpcName,
        bool isCritical,
        auto&&... args
    )
    {
        using namespace nkentseu::net;

        // Choix dynamique de la fiabilité selon l'importance
        const NkRPCReliability reliability = isCritical
            ? NkRPCReliability::NK_RELIABLE_ORD  // Critique : garanti + ordonné
            : NkRPCReliability::NK_UNRELIABLE;   // Non-critique : latence minimale

        // Note : la fiabilité est définie à l'enregistrement, pas à l'appel
        // Cet exemple illustre la conception : enregistrer deux versions si besoin

        router.CallServer(rpcName, std::forward<decltype(args)>(args)...);
    }

    // Meilleure pratique : enregistrer des RPC séparés pour cas critiques/non-critiques
    /\*
        // Version critique (garantie)
        router.Register("Player::UpdatePosition_Critical", ..., NK_RELIABLE_ORD);

        // Version non-critique (optimisée)
        router.Register("Player::UpdatePosition_Interp", ..., NK_UNRELIABLE);

        // Appel selon le contexte
        if (isSnapping) {
            router.CallServer("Player::UpdatePosition_Critical", pos);
        } else {
            router.CallServer("Player::UpdatePosition_Interp", pos);
        }
    *\/
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Nommage des RPC :
    ----------------
    - Toujours utiliser le format "ClassName::FunctionName" pour unicité
    - Éviter les noms génériques comme "Update" ou "Action" sans contexte
    - Documenter chaque RPC avec un commentaire décrivant son usage et ses paramètres

    Validation côté serveur :
    ------------------------
    - Jamais faire confiance aux paramètres clients — toujours valider
    - Vérifier les permissions, les bornes, l'état du jeu avant exécution
    - Logger les tentatives suspectes pour détection de triche

    Sérialisation des arguments :
    ----------------------------
    - Ordre de sérialisation = ordre de déclaration dans la macro
    - Utiliser des types simples (uint32, float32) pour compatibilité cross-platform
    - Pour les structures complexes, sérialiser champ par champ dans l'ordre

    Gestion des erreurs :
    --------------------
    - CallServer/CallClient/Multicast peuvent échouer — vérifier le retour NkNetResult
    - Dans les handlers, catcher les exceptions pour éviter un crash serveur
    - Utiliser NK_NET_LOG_* pour tracer les RPC reçus en debug

    Performance :
    ------------
    - Préférer NK_UNRELIABLE pour les RPC fréquents non-critiques (positions, animations)
    - Regrouper les petits RPC en un seul si possible (batching)
    - Éviter les chaînes longues dans les RPC fréquents — utiliser des IDs numériques

    Thread-safety :
    --------------
    - NkRPCRouter n'est PAS thread-safe par défaut
    - Protéger Register() avec un mutex si appelé depuis plusieurs threads au démarrage
    - Les appels Call*() et OnRPCReceived() doivent être faits depuis le thread réseau

    Debugging :
    ----------
    - Activer le logging des RPC en développement pour tracer le flux d'appels
    - Utiliser HashRPCName() pour vérifier la cohérence des IDs entre client/serveur
    - Tester les handlers avec des BitReader corrompus pour robustesse

    Évolution du protocole :
    -----------------------
    - Pour modifier la signature d'un RPC, créer un nouveau nom (v2) plutôt que modifier l'existant
    - Conserver les anciens handlers pour compatibilité avec les clients non mis à jour
    - Documenter les versions de RPC dans un fichier de protocole dédié
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================