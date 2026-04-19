#pragma once
// =============================================================================
// NKNetwork/RPC/NkRPC.h
// =============================================================================
// Remote Procedure Calls — appel de fonctions à distance.
//
// TYPES DE RPC :
//   ServerRPC  — appelé sur le client, exécuté sur le serveur
//   ClientRPC  — appelé sur le serveur, exécuté sur un ou tous les clients
//   MulticastRPC — exécuté sur tous les pairs (serveur + clients)
//
// SÉCURITÉ :
//   Les ServerRPC sont validés côté serveur (validation des paramètres).
//   Le serveur peut rejeter un RPC invalide sans planter.
//
// ENREGISTREMENT (MACRO) :
//   NK_RPC_SERVER(MyClass, OpenDoor, uint32 doorId, float32 force)
//   NK_RPC_CLIENT(MyClass, PlaySound, const char* sfxName)
//   NK_RPC_MULTICAST(MyClass, Explode, NkVec3f pos, float32 radius)
//
// USAGE :
//   // Côté client — appelle OpenDoor sur le serveur
//   mRPCRouter.CallServer("MyClass::OpenDoor", doorId, force);
//
//   // Côté serveur — appelle PlaySound sur le client propriétaire
//   mRPCRouter.CallClient(ownerPeerId, "MyClass::PlaySound", "door_open.wav");
//
//   // Côté serveur — appelle Explode sur tous
//   mRPCRouter.Multicast("MyClass::Explode", bombPos, 5.f);
// =============================================================================

#include "Protocol/NkConnection.h"
#include "Protocol/NkBitStream.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // Types de RPC
        // =====================================================================
        enum class NkRPCType : uint8 {
            ServerRPC,    ///< Client → Serveur
            ClientRPC,    ///< Serveur → Client spécifique
            MulticastRPC, ///< Serveur → Tous les clients
        };

        enum class NkRPCReliability : uint8 {
            Reliable,     ///< Garanti livré (ordre non garanti)
            ReliableOrd,  ///< Garanti livré + ordonné
            Unreliable,   ///< Meilleur effort (ex: sons non critiques)
        };

        // =====================================================================
        // NkRPCDescriptor — description d'un RPC enregistré
        // =====================================================================
        struct NkRPCDescriptor {
            static constexpr uint32 kMaxNameLen = 128u;
            char              name[kMaxNameLen] = {};  ///< "ClassName::FunctionName"
            uint32            id        = 0;           ///< Hash du nom (stable)
            NkRPCType         type      = NkRPCType::ServerRPC;
            NkRPCReliability  reliability = NkRPCReliability::ReliableOrd;

            // Handler : reçoit le payload désérialisé
            using HandlerFn = NkFunction<void(NkPeerId caller, NkBitReader& args)>;
            HandlerFn handler;
        };

        // =====================================================================
        // NkRPCRouter — registre et dispatch des RPCs
        // =====================================================================
        class NkRPCRouter {
        public:
            NkRPCRouter()  noexcept = default;
            ~NkRPCRouter() noexcept = default;

            // ── Enregistrement ────────────────────────────────────────────────
            /**
             * @brief Enregistre un handler RPC.
             * @param name     "MyClass::FunctionName"
             * @param type     ServerRPC / ClientRPC / MulticastRPC
             * @param handler  Fonction appelée quand le RPC est reçu.
             * @return ID du RPC (hash du nom, stable).
             */
            uint32 Register(const char* name,
                             NkRPCType type,
                             NkRPCDescriptor::HandlerFn handler,
                             NkRPCReliability rel = NkRPCReliability::ReliableOrd) noexcept;

            // ── Envoi ─────────────────────────────────────────────────────────
            /**
             * @brief Appelle un ServerRPC (client → serveur).
             * Le handler est exécuté côté serveur quand il reçoit le paquet.
             */
            template<typename... Args>
            [[nodiscard]] NkNetResult CallServer(const char* rpcName,
                                                  Args&&... args) noexcept {
                return CallInternal(NkRPCType::ServerRPC,
                                     NkPeerId::Server(),
                                     rpcName,
                                     std::forward<Args>(args)...);
            }

            /**
             * @brief Appelle un ClientRPC (serveur → client spécifique).
             */
            template<typename... Args>
            [[nodiscard]] NkNetResult CallClient(NkPeerId target,
                                                  const char* rpcName,
                                                  Args&&... args) noexcept {
                return CallInternal(NkRPCType::ClientRPC,
                                     target, rpcName,
                                     std::forward<Args>(args)...);
            }

            /**
             * @brief Appelle un MulticastRPC (serveur → tous les clients).
             */
            template<typename... Args>
            [[nodiscard]] NkNetResult Multicast(const char* rpcName,
                                                 Args&&... args) noexcept {
                return CallInternal(NkRPCType::MulticastRPC,
                                     NkPeerId::Invalid(),
                                     rpcName,
                                     std::forward<Args>(args)...);
            }

            // ── Réception ────────────────────────────────────────────────────
            /**
             * @brief Traite un paquet RPC reçu.
             * Retrouve le handler par ID, appelle le handler avec les args.
             */
            void OnRPCReceived(NkPeerId caller,
                                const uint8* data, uint32 size) noexcept;

            // ── Liaison au ConnectionManager ─────────────────────────────────
            void SetConnectionManager(NkConnectionManager* cm) noexcept {
                mConnMgr = cm;
            }

        private:
            // Sérialise les arguments et envoie
            template<typename... Args>
            NkNetResult CallInternal(NkRPCType type, NkPeerId target,
                                      const char* rpcName, Args&&... args) noexcept {
                // Trouver le descriptor
                const uint32 id = HashRPCName(rpcName);
                const NkRPCDescriptor* desc = FindDescriptor(id);
                if (!desc) {
                    NK_NET_LOG_ERROR("RPC non enregistré: %s", rpcName);
                    return NkNetResult::InvalidArg;
                }

                // Sérialiser l'en-tête + les args
                uint8 buf[kNkMaxPayloadSize];
                NkBitWriter w(buf, sizeof(buf));
                w.WriteU32(id);  // RPC ID
                // Sérialise les args dans l'ordre
                SerializeArgs(w, std::forward<Args>(args)...);
                if (w.IsOverflowed()) return NkNetResult::PacketTooLarge;

                // Envoi
                const NkNetChannel ch = (desc->reliability == NkRPCReliability::Unreliable)
                    ? NkNetChannel::Unreliable : NkNetChannel::ReliableOrdered;

                if (type == NkRPCType::MulticastRPC) {
                    return mConnMgr ? mConnMgr->Broadcast(buf, w.BytesWritten(), ch)
                                    : NkNetResult::NotInitialized;
                } else {
                    return mConnMgr ? mConnMgr->SendTo(target, buf, w.BytesWritten(), ch)
                                    : NkNetResult::NotInitialized;
                }
            }

            // Sérialisation des arguments (fold expression C++17)
            template<typename T, typename... Rest>
            void SerializeArgs(NkBitWriter& w, T&& arg, Rest&&... rest) noexcept {
                NetWrite(w, std::forward<T>(arg));
                SerializeArgs(w, std::forward<Rest>(rest)...);
            }
            void SerializeArgs(NkBitWriter&) noexcept {}  // Base case

            // Helpers de sérialisation par type
            void NetWrite(NkBitWriter& w, bool v)         noexcept { w.WriteBool(v); }
            void NetWrite(NkBitWriter& w, uint8 v)        noexcept { w.WriteU8(v);   }
            void NetWrite(NkBitWriter& w, uint16 v)       noexcept { w.WriteU16(v);  }
            void NetWrite(NkBitWriter& w, uint32 v)       noexcept { w.WriteU32(v);  }
            void NetWrite(NkBitWriter& w, int32 v)        noexcept { w.WriteI32(v);  }
            void NetWrite(NkBitWriter& w, float32 v)      noexcept { w.WriteF32(v);  }
            void NetWrite(NkBitWriter& w, const math::NkVec3f& v) noexcept { w.WriteVec3f(v); }
            void NetWrite(NkBitWriter& w, const char* s)  noexcept { w.WriteString(s); }
            void NetWrite(NkBitWriter& w, const NkString& s) noexcept { w.WriteString(s.CStr()); }

            // Utilitaires
            [[nodiscard]] static uint32 HashRPCName(const char* name) noexcept;
            [[nodiscard]] const NkRPCDescriptor* FindDescriptor(uint32 id) const noexcept;
            [[nodiscard]] NkRPCDescriptor*       FindDescriptor(uint32 id) noexcept;

            // Registry
            static constexpr uint32 kMaxRPCs = 256u;
            NkRPCDescriptor  mDescriptors[kMaxRPCs] = {};
            uint32           mCount = 0;

            NkConnectionManager* mConnMgr = nullptr;
        };

        // =====================================================================
        // Macros d'enregistrement de RPC (helpers de déclaration)
        // =====================================================================
        /**
         * @macro NK_RPC_SERVER(Class, Func, ...)
         * Déclare un ServerRPC dans une classe.
         *
         * @code
         * class PlayerController {
         *   NK_RPC_SERVER(PlayerController, OpenDoor, uint32 doorId, float32 force)
         *   void OpenDoor_ServerImpl(NkPeerId caller, uint32 doorId, float32 force);
         * };
         * @endcode
         */
        #define NK_RPC_SERVER(Class, Func, ...) \
            void Func##_RPC(__VA_ARGS__);       \
            static constexpr const char* k##Func##_RPCName = #Class "::" #Func;

        #define NK_RPC_CLIENT(Class, Func, ...) \
            void Func##_ClientRPC(__VA_ARGS__); \
            static constexpr const char* k##Func##_ClientRPCName = #Class "::" #Func;

        #define NK_RPC_MULTICAST(Class, Func, ...) \
            void Func##_Multicast(__VA_ARGS__);    \
            static constexpr const char* k##Func##_MulticastName = #Class "::" #Func;

    } // namespace net
} // namespace nkentseu
