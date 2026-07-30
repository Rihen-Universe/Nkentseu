#pragma once
// =============================================================================
// Noge/ECS/Components/Network/NkNetComponents.h — composant ECS de réplication
// =============================================================================
// Composant DE DONNÉES PURES marquant une entité ECS comme répliquée par
// NkNetSystem (Noge/ECS/Replication/NkNetWorld.h). Même philosophie que
// NkRigidbody3D/NkCollider3D pour NkPhysicsSystem : le composant ne fait rien
// par lui-même, c'est le système-pont qui l'interprète.
//
// Aucune réimplémentation des types réseau : net::NkNetId / net::NkPeerId /
// net::NkNetAuthority viennent tels quels de NKNetwork (couche System), seule
// source de vérité pour l'identité et l'autorité réseau (cf. découverte
// ROADMAP.md Phase I1 : NKNetwork a déjà une implémentation réelle à adapter).
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/Core/NkTypeRegistry.h"
#include "NKNetwork/Core/NkNetDefines.h"
#include "NKNetwork/Replication/NkNetWorld.h" // net::NkNetAuthority

namespace nkentseu {
	namespace ecs {

		// =====================================================================
		// NkNetEntity — composant ECS marquant une entité répliquée
		// =====================================================================
		/**
		 * @struct NkNetEntity
		 * @brief Ajouté par NkNetSystem::RegisterEntity() (serveur) ou par le
		 *        callback interne de spawn distant (client). La présence de ce
		 *        composant = l'entité est suivie par net::NkNetWorld.
		 */
		struct NkNetEntity {
				/// Identité réseau stable, cohérente sur tous les pairs.
				net::NkNetId netId;

				/// Pair propriétaire (Invalid() = serveur).
				net::NkPeerId ownerPeer;

				/// Identifiant de type applicatif (prefab), sémantique libre —
				/// transmis tel quel au spawn distant.
				uint32 prefabId = 0;

				/// Autorité de simulation (type réel net::NkNetAuthority).
				net::NkNetAuthority authority = net::NkNetAuthority::NK_NET_AUTHORITY_SERVER;

				/// true = entité créée par le callback onEntitySpawn (côté
				/// distant qui reçoit) ; false = enregistrée localement par
				/// RegisterEntity() (côté qui possède l'entité).
				bool remoteSpawned = false;
		};
		NK_COMPONENT(NkNetEntity)

	} // namespace ecs
} // namespace nkentseu
