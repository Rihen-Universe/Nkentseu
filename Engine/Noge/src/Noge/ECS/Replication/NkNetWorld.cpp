// =============================================================================
// Noge/ECS/Replication/NkNetWorld.cpp — pont ECS -> NKNetwork (réplication)
// =============================================================================
#include "NkNetWorld.h"
#include "NKMemory/NkAllocator.h"

// NOTE : PAS de `using namespace net;` ici -- net::NkNetEntity (descripteur
// de réplication NKNetwork) et ecs::NkNetEntity (composant ECS, cf.
// Components/Network/NkNetComponents.h) partagent le même nom. `using
// namespace ecs;` seul est sans danger (rien d'ambigu côté ecs) ; tous les
// symboles net:: restent qualifiés explicitement pour lever toute ambiguïté.

namespace nkentseu {

	using namespace ecs;

	// -------------------------------------------------------------------------
	NkNetSystem::~NkNetSystem() noexcept {
		Shutdown();
	}

	// -------------------------------------------------------------------------
	// Binding : lien opaque {monde ECS, entité}, adresse stable (NKMemory) --
	// même principe que NkAudioSystem::ClipEntry (NkVector de suivi peut
	// réallouer, l'adresse référencée par net::NkNetWorld ne doit pas bouger).
	// -------------------------------------------------------------------------
	NkNetSystem::Binding *NkNetSystem::MakeBinding(NkEntityId id) noexcept {
		Binding *b = memory::NkGetDefaultAllocator().New<Binding>();
		b->world = mWorld;
		b->entity = id;
		mBindings.PushBack(b);
		return b;
	}

	void NkNetSystem::FreeBinding(Binding *b) noexcept {
		if (!b) {
			return;
		}
		for (nk_usize i = 0; i < mBindings.Size(); ++i) {
			if (mBindings[i] == b) {
				mBindings.RemoveAt(i);
				break;
			}
		}
		memory::NkGetDefaultAllocator().Delete(b);
	}

	NkNetSystem::Binding *NkNetSystem::FindBindingByEntity(NkEntityId id) noexcept {
		for (nk_usize i = 0; i < mBindings.Size(); ++i) {
			if (mBindings[i]->entity == id) {
				return mBindings[i];
			}
		}
		return nullptr;
	}

	// -------------------------------------------------------------------------
	// Sérialisation par défaut : NkTransform (position + rotation).
	// -------------------------------------------------------------------------
	void NkNetSystem::WriteTransform(void *user, net::NkBitWriter &w) {
		const Binding *b = static_cast<const Binding *>(user);
		const NkTransform *tf = b->world->Get<NkTransform>(b->entity);
		if (!tf) {
			return;
		}
		w.WriteF32(tf->localPosition.x);
		w.WriteF32(tf->localPosition.y);
		w.WriteF32(tf->localPosition.z);
		w.WriteF32(tf->localRotation.x);
		w.WriteF32(tf->localRotation.y);
		w.WriteF32(tf->localRotation.z);
		w.WriteF32(tf->localRotation.w);
	}

	void NkNetSystem::ReadTransform(void *user, net::NkBitReader &r) {
		const Binding *b = static_cast<const Binding *>(user);
		NkTransform *tf = b->world->Get<NkTransform>(b->entity);
		const float32 px = r.ReadF32();
		const float32 py = r.ReadF32();
		const float32 pz = r.ReadF32();
		const float32 rx = r.ReadF32();
		const float32 ry = r.ReadF32();
		const float32 rz = r.ReadF32();
		const float32 rw = r.ReadF32();
		if (!tf) {
			return; // octets déjà consommés -- pas de désynchronisation du reader.
		}
		tf->localPosition.x = px;
		tf->localPosition.y = py;
		tf->localPosition.z = pz;
		tf->localRotation.x = rx;
		tf->localRotation.y = ry;
		tf->localRotation.z = rz;
		tf->localRotation.w = rw;
		tf->worldDirty = true;
	}

	// -------------------------------------------------------------------------
	// Cycle de vie
	// -------------------------------------------------------------------------
	void NkNetSystem::Init(NkWorld *world, net::NkConnectionManager *connMgr, bool isServer,
							const net::NkNetWorld::Config &config) noexcept {
		mWorld = world;
		mConnMgr = connMgr;
		mNetWorld.Init(connMgr, isServer, config);

		// CLIENT : une entité inconnue apparaît dans un snapshot -> crée
		// l'entité ECS correspondante puis l'enregistre AVANT de retourner,
		// exactement comme l'exige le contrat de net::NkNetWorld::SpawnCb
		// (l'état du snapshot est appliqué dans la foulée via readState).
		mNetWorld.onEntitySpawn = [this](net::NkNetId netId, uint32 prefabId, net::NkPeerId owner) {
			if (!mWorld) {
				return;
			}
			const NkEntityId id = mWorld->Create().With<NkTransform>(NkTransform{}).Build();

			NkNetEntity comp;
			comp.netId = netId;
			comp.ownerPeer = owner;
			comp.prefabId = prefabId;
			comp.authority = net::NkNetAuthority::NK_NET_AUTHORITY_SERVER;
			comp.remoteSpawned = true;
			mWorld->Add<NkNetEntity>(id, comp);

			Binding *b = MakeBinding(id);
			net::NkNetEntity desc;
			desc.netId = netId;
			desc.prefabId = prefabId;
			desc.ownerPeer = owner;
			desc.authority = net::NkNetAuthority::NK_NET_AUTHORITY_SERVER;
			desc.user = b;
			// Lambda plutôt que pointeur de fonction brut : un pointeur de
			// fonction libre est un type "T*" que la résolution de surcharge
			// de NkFunction fait matcher par erreur avec le constructeur de
			// binding multi-méthode (template <T> NkFunction(T *obj, ...)),
			// pas avec le constructeur générique F&&. Même convention que
			// Sandbox/System/NKNetwork/src/main.cpp (jamais de pointeur de
			// fonction brut assigné à writeState/readState).
			desc.readState = [](void *user, net::NkBitReader &r) { NkNetSystem::ReadTransform(user, r); };
			(void)mNetWorld.RegisterEntity(desc);
		};

		// Les deux côtés : DESPAWN reçu -> détruit l'entité ECS locale
		// correspondante (l'entrée du registre net::NkNetWorld est retirée
		// APRÈS ce callback par NkNetWorld lui-même, cf. contrat DespawnCb).
		mNetWorld.onEntityDespawn = [this](net::NkNetId netId) {
			if (!mWorld) {
				return;
			}
			net::NkNetEntity *desc = mNetWorld.FindEntity(netId);
			if (!desc || !desc->user) {
				return;
			}
			Binding *b = static_cast<Binding *>(desc->user);
			const NkEntityId id = b->entity;
			mWorld->Destroy(id);
			FreeBinding(b);
		};
	}

	void NkNetSystem::Shutdown() noexcept {
		for (nk_usize i = 0; i < mBindings.Size(); ++i) {
			memory::NkGetDefaultAllocator().Delete(mBindings[i]);
		}
		mBindings.Clear();
		mNetWorld.Shutdown();
		mWorld = nullptr;
		mConnMgr = nullptr;
	}

	// -------------------------------------------------------------------------
	// Registre des entités répliquées
	// -------------------------------------------------------------------------
	bool NkNetSystem::RegisterEntity(NkEntityId id, uint32 prefabId, net::NkNetAuthority authority) noexcept {
		if (!mWorld || !mWorld->IsAlive(id) || mWorld->Has<NkNetEntity>(id)) {
			return false;
		}
		if (!mWorld->Has<NkTransform>(id)) {
			// L'adaptateur par défaut réplique NkTransform : requis.
			return false;
		}

		const net::NkNetId netId = mNetWorld.AllocateNetId();
		if (!netId.IsValid()) {
			return false;
		}

		Binding *b = MakeBinding(id);

		net::NkNetEntity netDesc;
		netDesc.netId = netId;
		netDesc.prefabId = prefabId;
		netDesc.authority = authority;
		netDesc.user = b;
		// Lambda plutôt que pointeur de fonction brut (cf. commentaire
		// analogue dans le callback onEntitySpawn ci-dessus).
		netDesc.writeState = [](void *user, net::NkBitWriter &w) { NkNetSystem::WriteTransform(user, w); };

		if (!mNetWorld.RegisterEntity(netDesc)) {
			FreeBinding(b);
			return false;
		}

		NkNetEntity comp;
		comp.netId = netId;
		comp.prefabId = prefabId;
		comp.authority = authority;
		comp.remoteSpawned = false;
		mWorld->Add<NkNetEntity>(id, comp);
		return true;
	}

	bool NkNetSystem::UnregisterEntity(NkEntityId id, bool notifyPeers) noexcept {
		if (!mWorld) {
			return false;
		}
		const NkNetEntity *comp = mWorld->Get<NkNetEntity>(id);
		if (!comp) {
			return false;
		}
		Binding *b = FindBindingByEntity(id);
		const bool removed = mNetWorld.UnregisterEntity(comp->netId, notifyPeers);
		mWorld->Remove<NkNetEntity>(id);
		FreeBinding(b);
		return removed;
	}

	// -------------------------------------------------------------------------
	// Boucle de réplication
	// -------------------------------------------------------------------------
	void NkNetSystem::Execute(NkWorld &world, float32 dt) noexcept {
		if (!mConnMgr) {
			return; // Init() pas encore appelé -- cohérent avec les autres
					// ponts ECS (ex. NkAudioSystem sans engine initialisé).
		}
		(void)world;

		// Serveur : construit/diffuse le(s) snapshot(s) du tick (writeState
		// -> WriteTransform lit l'état courant depuis NkTransform).
		mNetWorld.Update(dt);

		// Les deux côtés : draine le connMgr et route vers la réplication.
		// Les messages non-réplication (retour false) sont ignorés par cet
		// adaptateur minimal -- une application réelle les routerait vers sa
		// propre couche applicative (lobby/chat/RPC), hors périmètre du pont.
		NkVector<net::NkReceiveMsg> msgs;
		mConnMgr->DrainAll(msgs);
		for (nk_usize i = 0; i < msgs.Size(); ++i) {
			(void)mNetWorld.HandleMessage(msgs[i]);
		}
	}

} // namespace nkentseu
