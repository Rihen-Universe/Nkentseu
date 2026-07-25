#pragma once
// =============================================================================
// Noge/ECS/Replication/NkNetWorld.h — pont ECS -> NKNetwork (réplication)
// =============================================================================
// ADAPTATEUR MINCE, PAS UNE REIMPLEMENTATION. Kernel/System/NKNetwork/src/
// NKNetwork/Replication/NkNetWorld.{h,cpp} (609+634 lignes, RÉEL) implémente
// déjà tout le protocole de réplication server-authoritative : snapshots
// delta/keyframe, NkNetInterpolator, SendInput, sérialisation NkBitStream.
// Son propre en-tête le dit explicitement : le pont vers l'ECS se fait dans
// Noge, exactement comme pour NKPhysics (cf. Noge/ECS/Systems/NkPhysicsSystem
// : POSSÈDE un physics::NkPhysicsWorld, traduit les composants ECS <-> API
// réelle). Ce fichier applique EXACTEMENT le même schéma pour NKNetwork :
//   - NkNetSystem POSSÈDE un net::NkNetWorld réel (non réimplémenté).
//   - RegisterEntity()/UnregisterEntity() enregistrent/désenregistrent des
//     entités ECS auprès de lui (via NkNetEntity, décrit dans
//     Components/Network/NkNetComponents.h).
//   - Execute() appelle net::NkNetWorld::Update(dt) chaque tick et route les
//     messages drainés depuis un net::NkConnectionManager EXISTANT (non
//     possédé, fourni par l'application -- même contrat que
//     net::NkNetWorld::Init()).
//
// CE QUI EST RÉPLIQUÉ PAR DÉFAUT : ecs::NkTransform (position + rotation) de
// l'entité, dans les callbacks writeState/readState (WriteTransform/
// ReadTransform, statiques, référencées via un Binding{world, entity}
// opaque). C'est un choix d'adaptateur "par défaut" volontairement simple
// (même esprit que NkPhysicsSystem qui pousse/tire systématiquement
// NkTransform) -- rien n'empêche une application de fournir ses propres
// writeState/readState via World().RegisterEntity() directement si elle a
// besoin de répliquer un état custom (cf. API net::NkNetEntity).
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Network/NkNetComponents.h"
#include "NKNetwork/Replication/NkNetWorld.h"
#include "NKNetwork/Protocol/NkConnection.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

	// =========================================================================
	// CLASSE : NkNetSystem — pont ECS -> net::NkNetWorld (réplication réelle)
	// =========================================================================
	class NkNetSystem final : public ecs::NkSystem {
		public:
			NkNetSystem() noexcept = default;
			~NkNetSystem() noexcept override;

			NkNetSystem(const NkNetSystem &) = delete;
			NkNetSystem &operator=(const NkNetSystem &) = delete;

			// -------------------------------------------------------------
			// Cycle de vie
			// -------------------------------------------------------------

			/**
			 * @brief Lie l'adaptateur au monde ECS et à un gestionnaire de
			 *        connexions EXISTANT (non possédé, non détruit) --
			 *        même contrat que net::NkNetWorld::Init().
			 * @param world   Monde ECS local (non possédé).
			 * @param connMgr Gestionnaire de connexions déjà démarré
			 *                (StartServer()/Connect() déjà appelé).
			 * @param isServer true = ce pair est l'autorité (émet les snapshots).
			 */
			void Init(ecs::NkWorld *world, net::NkConnectionManager *connMgr, bool isServer,
					  const net::NkNetWorld::Config &config = net::NkNetWorld::Config{}) noexcept;

			/// Désenregistre toutes les entités et coupe le lien (n'affecte
			/// PAS le cycle de vie du connMgr, non possédé).
			void Shutdown() noexcept;

			[[nodiscard]] ecs::NkSystemDesc Describe() const override {
				return ecs::NkSystemDesc{}
					.Writes<ecs::NkTransform>()
					.Writes<ecs::NkNetEntity>()
					.InGroup(ecs::NkSystemGroup::PostUpdate)
					.Sequential()
					.Named("NkNetSystem");
			}

			// -------------------------------------------------------------
			// Registre des entités répliquées (pont ECS <-> net::NkNetWorld)
			// -------------------------------------------------------------

			/**
			 * @brief SERVEUR (ou propriétaire) : enregistre une entité ECS
			 *        existante (NkTransform requis) comme répliquée.
			 * @param id       Entité ECS déjà créée dans le monde lié par Init().
			 * @param prefabId Identifiant applicatif transmis au spawn distant.
			 * @param authority Autorité de simulation (défaut : serveur).
			 * @return true si enregistrée (ajoute le composant NkNetEntity).
			 * @note Client : ne pas appeler directement pour des entités
			 *       distantes -- elles sont créées par le callback interne
			 *       déclenché par net::NkNetWorld::HandleMessage() (SNAPSHOT
			 *       d'une entité inconnue).
			 */
			bool RegisterEntity(ecs::NkEntityId id, uint32 prefabId,
								 net::NkNetAuthority authority = net::NkNetAuthority::NK_NET_AUTHORITY_SERVER) noexcept;

			/**
			 * @brief Désenregistre une entité répliquée (les deux côtés).
			 * @note Ne détruit PAS l'entité ECS elle-même -- même séparation
			 *       stricte que NkPhysicsSystem (le composant décrit l'état,
			 *       le système-pont gère seulement la liaison à la ressource
			 *       externe). L'appelant décide du sort de l'entité ECS.
			 */
			bool UnregisterEntity(ecs::NkEntityId id, bool notifyPeers = true) noexcept;

			// -------------------------------------------------------------
			// Boucle de réplication
			// -------------------------------------------------------------

			/**
			 * @brief Serveur : émet un snapshot si le tick est écoulé (état
			 *        courant poussé depuis NkTransform). Les deux côtés :
			 *        draine le connMgr et route chaque message vers
			 *        net::NkNetWorld::HandleMessage() (snapshots/despawn côté
			 *        client, inputs côté serveur -- les messages non-
			 *        réplication sont ignorés par cet adaptateur minimal).
			 */
			void Execute(ecs::NkWorld &world, float32 dt) noexcept override;

			// -------------------------------------------------------------
			// Accès direct (inputs, force keyframe, stats...)
			// -------------------------------------------------------------

			[[nodiscard]] net::NkNetWorld &World() noexcept {
				return mNetWorld;
			}

			[[nodiscard]] const net::NkNetWorld &World() const noexcept {
				return mNetWorld;
			}

			[[nodiscard]] bool IsServer() const noexcept {
				return mNetWorld.IsServer();
			}

		private:
			// Lien opaque {monde ECS, entité} passé en `void *user` aux
			// callbacks writeState/readState de net::NkNetEntity. Alloué
			// individuellement (NKMemory) pour une adresse stable, même
			// principe que NkAudioSystem::ClipEntry (le NkVector de suivi
			// peut réallouer, l'adresse référencée par net::NkNetWorld ne
			// doit pas bouger).
			struct Binding {
					ecs::NkWorld *world = nullptr;
					ecs::NkEntityId entity;
			};

			[[nodiscard]] Binding *MakeBinding(ecs::NkEntityId id) noexcept;
			void FreeBinding(Binding *b) noexcept;
			[[nodiscard]] Binding *FindBindingByEntity(ecs::NkEntityId id) noexcept;

			// Callbacks de sérialisation par défaut (état = NkTransform).
			// PAS de `noexcept` ici : net::NkNetEntity::WriteStateFn/ReadStateFn
			// (NkFunction<void(void*, NkBitWriter&/NkBitReader&)>) exige un type
			// de pointeur de fonction EXACTEMENT assignable -- `noexcept` change
			// le type du pointeur (C++17) et casse l'affectation par NkFunction.
			static void WriteTransform(void *user, net::NkBitWriter &w);
			static void ReadTransform(void *user, net::NkBitReader &r);

			net::NkNetWorld mNetWorld;					   // réplication réelle (possédée)
			net::NkConnectionManager *mConnMgr = nullptr; // non possédé
			ecs::NkWorld *mWorld = nullptr;			   // non possédé
			NkVector<Binding *> mBindings;				   // adresses stables, libérées explicitement
	};

} // namespace nkentseu
