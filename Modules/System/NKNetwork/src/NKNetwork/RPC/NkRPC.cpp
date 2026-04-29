// =============================================================================
// NKNetwork/RPC/NkRPC.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation du système RPC (Remote Procedure Calls) pour l'appel
//   de fonctions à distance entre clients et serveur.
//
// NOTES D'IMPLÉMENTATION :
//   • Toutes les méthodes publiques retournent NkNetResult pour cohérence API
//   • Gestion d'erreur robuste : logging + retour gracieux sans crash
//   • Sérialisation type-safe via NkBitStream avec vérification de débordement
//   • Lookup par hash pour performance : O(1) en pratique avec table petite
//
// DÉPENDANCES INTERNES :
//   • NkRPC.h : Déclarations des classes implémentées
//   • NkBitStream.h : Sérialisation binaire compacte des arguments
//   • NkNetDefines.h : Types fondamentaux et codes de retour réseau
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour MSVC/Clang avec PCH)
// 2. Header correspondant au fichier .cpp (vérification de cohérence)
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme cible

#include "pch.h"
#include "NKNetwork/RPC/NkRPC.h"

// En-têtes standards C/C++ pour opérations bas-niveau
#include <cstring>
#include <cstdint>

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu::net.

namespace nkentseu {

    namespace net {

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Enregistrement
        // =====================================================================

        uint32 NkRPCRouter::Register(
            const char* name,
            NkRPCType type,
            NkRPCDescriptor::HandlerFn handler,
            NkRPCReliability rel
        ) noexcept
        {
            // Validation des paramètres d'entrée
            if (name == nullptr || handler == nullptr)
            {
                NK_NET_LOG_ERROR("RPC Register : paramètres invalides (name ou handler null)");
                return 0u;
            }

            // Vérification de la capacité de la table
            if (mCount >= kMaxRPCs)
            {
                NK_NET_LOG_ERROR("RPC Register : table pleine (max {} RPCs)", kMaxRPCs);
                return 0u;
            }

            // Calcul de l'ID hashé pour lookup rapide
            const uint32 rpcId = HashRPCName(name);

            // Vérification de collision d'ID (deux noms différents → même hash)
            // Probabilité très faible avec hash 32-bit, mais vérification défensive
            if (FindDescriptor(rpcId) != nullptr)
            {
                NK_NET_LOG_WARN("RPC Register : collision d'ID pour '{}', hash={}", name, rpcId);
                // Ne pas bloquer — accepter la collision (rare) ou gérer autrement selon besoin
            }

            // Remplissage du descriptor dans la table
            NkRPCDescriptor& desc = mDescriptors[mCount];

            // Copie sécurisée du nom avec terminateur nul
            const size_t nameLen = std::strlen(name);
            const size_t copyLen = (nameLen < NkRPCDescriptor::kMaxNameLen - 1) ? nameLen : (NkRPCDescriptor::kMaxNameLen - 1);
            std::memcpy(desc.name, name, copyLen);
            desc.name[copyLen] = '\0';  // Terminaison explicite

            // Initialisation des métadonnées
            desc.id = rpcId;
            desc.type = type;
            desc.reliability = rel;
            desc.handler = std::move(handler);

            // Incrémentation du compteur et retour de l'ID
            ++mCount;
            return rpcId;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Templates d'appel (instanciations)
        // =====================================================================

        // Note : Les méthodes templates sont définies inline dans le header.
        // Cette section documente leur comportement pour référence.

        // CallServer<Args...> :
        //   • Résout le descriptor par nom → ID hash
        //   • Sérialise l'ID + arguments dans un buffer temporaire
        //   • Envoie via NkConnectionManager vers le serveur (NkPeerId::Server())
        //   • Utilise le canal ReliableOrdered par défaut (configurable via descriptor)

        // CallClient<Args...> :
        //   • Même logique que CallServer mais cible un NkPeerId spécifique
        //   • Uniquement valide côté serveur ou pair autorisé
        //   • Retourne NkNetResult::NotInitialized si mConnMgr est null

        // Multicast<Args...> :
        //   • Même logique mais utilise Broadcast() au lieu de SendTo()
        //   • Target = NkPeerId::Invalid() indique "tous les pairs"
        //   • Attention : peut générer beaucoup de trafic — utiliser avec modération

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Réception et dispatch
        // =====================================================================

        void NkRPCRouter::OnRPCReceived(
            NkPeerId caller,
            const uint8* data,
            uint32 size
        ) noexcept
        {
            // Validation des paramètres d'entrée
            if (data == nullptr || size == 0)
            {
                NK_NET_LOG_WARN("RPC reçu invalide : buffer null ou vide depuis {}", caller.value);
                return;
            }

            // Création d'un BitReader pour désérialisation
            NkBitReader reader(data, size);

            // Lecture de l'ID du RPC (premier champ : uint32)
            uint32 rpcId = 0u;
            if (!(rpcId = reader.ReadU32()))
            {
                NK_NET_LOG_ERROR("RPC : échec de lecture de l'ID depuis {}", caller.value);
                return;
            }

            // Recherche du descriptor correspondant
            const NkRPCDescriptor* desc = FindDescriptor(rpcId);
            if (desc == nullptr)
            {
                NK_NET_LOG_WARN("RPC inconnu : id={} depuis {}", rpcId, caller.value);
                return;
            }

            // Validation du type de RPC selon la direction attendue
            // (ex: un ServerRPC ne devrait pas être reçu côté client)
            // Cette vérification est optionnelle mais recommandée pour la sécurité
            #if defined(NKNET_DEBUG_RPC)
                if (desc->type == NkRPCType::NK_SERVER_RPC && !caller.IsServer())
                {
                    NK_NET_LOG_DEBUG("RPC Server reçu côté client (normal si client appelle serveur)");
                }
            #endif

            // Exécution du handler avec protection contre les exceptions
            if (desc->handler)
            {
                try
                {
                    // Appel du handler avec le caller et le reader positionné après l'ID
                    desc->handler(caller, reader);
                }
                catch (const std::exception& e)
                {
                    NK_NET_LOG_ERROR("RPC '{}' : exception depuis {}: {}", desc->name, caller.value, e.what());
                }
                catch (...)
                {
                    NK_NET_LOG_ERROR("RPC '{}' : exception inconnue depuis {}", desc->name, caller.value);
                }
            }
            else
            {
                NK_NET_LOG_ERROR("RPC '{}' : handler null pour id={}", desc->name, rpcId);
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Intégration ConnectionManager
        // =====================================================================

        void NkRPCRouter::SetConnectionManager(NkConnectionManager* cm) noexcept
        {
            mConnMgr = cm;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Helpers de sérialisation
        // =====================================================================

        void NkRPCRouter::SerializeArgs(NkBitWriter& w) noexcept
        {
            // Cas de base : fin de la récursion — rien à faire
            (void)w;  // Suppression warning variable unused
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, bool v) noexcept
        {
            w.WriteBool(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, uint8 v) noexcept
        {
            w.WriteU8(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, uint16 v) noexcept
        {
            w.WriteU16(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, uint32 v) noexcept
        {
            w.WriteU32(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, int32 v) noexcept
        {
            w.WriteI32(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, float32 v) noexcept
        {
            w.WriteF32(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, const math::NkVec3f& v) noexcept
        {
            w.WriteVec3f(v);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, const char* s) noexcept
        {
            w.WriteString(s);
        }

        void NkRPCRouter::NetWrite(NkBitWriter& w, const NkString& s) noexcept
        {
            w.WriteString(s.CStr());
        }

        // =====================================================================
        // IMPLÉMENTATION : NkRPCRouter — Utilitaires de hash et lookup
        // =====================================================================

        uint32 NkRPCRouter::HashRPCName(const char* name) noexcept
        {
            // Algorithme de hash simple et déterministe (FNV-1a variant)
            // Choisi pour : rapidité, distribution correcte, portabilité

            if (name == nullptr)
            {
                return 0u;
            }

            // Valeur initiale (prime pour bonne distribution)
            uint32 hash = 2166136261u;

            // Parcours caractère par caractère
            for (const char* p = name; *p != '\0'; ++p)
            {
                // XOR avec le caractère courant
                hash ^= static_cast<uint32>(static_cast<unsigned char>(*p));

                // Multiplication par prime FNV
                hash *= 16777619u;
            }

            return hash;
        }

        const NkRPCDescriptor* NkRPCRouter::FindDescriptor(uint32 id) const noexcept
        {
            // Recherche linéaire dans la table — O(n) avec n ≤ 256
            // Acceptable car : table petite, recherche rare (à la réception)

            for (uint32 i = 0u; i < mCount; ++i)
            {
                if (mDescriptors[i].id == id)
                {
                    return &mDescriptors[i];
                }
            }

            // Non trouvé
            return nullptr;
        }

        NkRPCDescriptor* NkRPCRouter::FindDescriptor(uint32 id) noexcept
        {
            // Version mutable — délègue à la version const pour éviter la duplication
            return const_cast<NkRPCDescriptor*>(
                static_cast<const NkRPCRouter*>(this)->FindDescriptor(id)
            );
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    Algorithme de hash (HashRPCName) :
    ---------------------------------
    - FNV-1a variant : rapide, non-cryptographique, distribution correcte
    - 32-bit output : compromis entre taille réseau et risque de collision
    - Déterministe : même entrée → même sortie sur toutes les plateformes
    - Pour un usage critique, envisager un hash 64-bit ou une table de mapping

    Gestion des collisions de hash :
    -------------------------------
    - Probabilité faible avec 256 RPC max et espace 2^32
    - Stratégie actuelle : accepter la collision (dernier enregistré gagne)
    - Alternative : refuser l'enregistrement ou utiliser une table de hachage avec chaînage

    Sérialisation des arguments :
    ----------------------------
    - Ordre strict : les arguments sont lus dans l'ordre de déclaration
    - Type-safe : chaque type a sa propre surcharge NetWrite()
    - Vérification de débordement : IsOverflowed() après sérialisation complète
    - Extension : ajouter support pour tableaux, structs via templates spécialisés

    Sécurité des handlers :
    ----------------------
    - Try/catch autour de l'appel au handler pour isolation des erreurs
    - Validation des paramètres dans le handler, pas dans le router
    - Logging des appels suspects pour détection de triche/abus
    - Option future : sandboxing ou validation automatique via schema

    Performance :
    ------------
    - Table statique pré-allouée : aucune allocation heap à l'exécution
    - Lookup linéaire : O(256) worst-case = ~256 comparaisons = négligeable
    - Sérialisation inline : pas de copies intermédiaires, écriture directe en buffer
    - Templates instanciés : pas de virtual dispatch, optimisation par le compilateur

    Thread-safety :
    --------------
    - Register() : non thread-safe — protéger avec mutex si appelé depuis plusieurs threads
    - FindDescriptor() : thread-safe en lecture seule après initialisation complète
    - OnRPCReceived() : doit être appelée depuis le thread réseau unique
    - Call*() templates : thread-safe si mConnMgr est thread-safe

    Extensions futures :
    -------------------
    - Support des RPC asynchrones avec callbacks de résultat
    - Compression des payloads pour RPC fréquents avec gros arguments
    - Versioning des RPC pour compatibilité client/serveur différente
    - Profiling intégré : compteur d'appels, temps d'exécution, bande passante
    - Génération automatique de code via script pour réduire les erreurs manuelles
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================