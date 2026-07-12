// =============================================================================
// NKNetwork/Replication/NkNetWorld.cpp
// =============================================================================
// Implémentation de la couche de réplication : NkNetInterpolator, NkNetSystem
// (hooks par défaut) et NkNetWorld (registre + snapshots delta/keyframe +
// inputs). Voir NkNetWorld.h pour l'architecture et le protocole filaire.
//
// AUTEUR   : Rihen
// DATE     : 2026-07-12
// LICENCE  : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================

#include "pch.h"
#include "NKNetwork/Replication/NkNetWorld.h"

#include "NKMemory/NkFunction.h"

namespace nkentseu {

	namespace net {

		// =====================================================================
		// NkNetInterpolator
		// =====================================================================

		NkNetInterpolator::NkNetInterpolator(uint32 capacity) noexcept {
			mCapacity = (capacity < 2u) ? 2u : capacity;
		}

		void NkNetInterpolator::Push(uint32 tick, NkTimestampMs receivedAt, const uint8 *state,
									 uint32 size) noexcept {
			if (state == nullptr || size == 0) {
				return;
			}
			// Rejette les doublons et les états qui remontent le temps
			// (canal SEQUENCED : ne devrait pas arriver, on se protège quand même).
			if (!mSnapshots.IsEmpty() && tick <= mSnapshots[mSnapshots.Size() - 1].tick) {
				return;
			}

			NkNetSnapshot snap;
			snap.tick = tick;
			snap.receivedAt = receivedAt;
			snap.state.Resize(size);
			memory::NkCopy(snap.state.Data(), state, size);
			mSnapshots.PushBack(snap);

			// Éviction FIFO au-delà de la capacité.
			while (mSnapshots.Size() > mCapacity) {
				mSnapshots.Erase(mSnapshots.Begin());
			}
		}

		bool NkNetInterpolator::Sample(NkTimestampMs renderTime, const NkNetSnapshot **outA,
									   const NkNetSnapshot **outB, float32 &outAlpha) const noexcept {
			if (outA == nullptr || outB == nullptr) {
				return false;
			}
			*outA = nullptr;
			*outB = nullptr;
			outAlpha = 0.f;

			const uint32 count = static_cast<uint32>(mSnapshots.Size());
			if (count == 0) {
				return false;
			}

			// Avant le premier état : clamp sur le premier.
			if (renderTime <= mSnapshots[0].receivedAt) {
				*outA = &mSnapshots[0];
				*outB = &mSnapshots[0];
				return true;
			}
			// Après le dernier état : clamp sur le dernier.
			if (renderTime >= mSnapshots[count - 1].receivedAt) {
				*outA = &mSnapshots[count - 1];
				*outB = &mSnapshots[count - 1];
				return true;
			}

			// Recherche de la paire encadrante (liste courte : balayage linéaire).
			for (uint32 i = 0; i + 1 < count; ++i) {
				const NkNetSnapshot &a = mSnapshots[i];
				const NkNetSnapshot &b = mSnapshots[i + 1];
				if (renderTime >= a.receivedAt && renderTime <= b.receivedAt) {
					*outA = &a;
					*outB = &b;
					const NkTimestampMs span = b.receivedAt - a.receivedAt;
					if (span > 0) {
						outAlpha = static_cast<float32>(renderTime - a.receivedAt) / static_cast<float32>(span);
					}
					return true;
				}
			}
			return false;
		}

		void NkNetInterpolator::Clear() noexcept {
			mSnapshots.Clear();
		}

		uint32 NkNetInterpolator::Count() const noexcept {
			return static_cast<uint32>(mSnapshots.Size());
		}

		uint32 NkNetInterpolator::LatestTick() const noexcept {
			if (mSnapshots.IsEmpty()) {
				return 0;
			}
			return mSnapshots[mSnapshots.Size() - 1].tick;
		}

		// =====================================================================
		// NkNetSystem — hooks par défaut (no-op)
		// =====================================================================

		void NkNetSystem::OnBeforeSnapshot(uint32 /*tick*/) noexcept {
		}

		void NkNetSystem::OnSnapshotApplied(uint32 /*tick*/) noexcept {
		}

		void NkNetSystem::OnEntityRegistered(const NkNetEntity & /*entity*/) noexcept {
		}

		void NkNetSystem::OnEntityUnregistered(NkNetId /*netId*/) noexcept {
		}

		// =====================================================================
		// NkNetWorld — cycle de vie
		// =====================================================================

		NkNetWorld::~NkNetWorld() noexcept {
			Shutdown();
		}

		void NkNetWorld::Init(NkConnectionManager *connMgr, bool isServer, const Config &config) noexcept {
			Shutdown();
			mConnMgr = connMgr;
			mIsServer = isServer;
			mConfig = config;
			if (mConfig.tickRate <= 0.f) {
				mConfig.tickRate = 20.f;
			}
			mTickAccumulator = 0.f;
			mTick = 0;
			mNextNetId = 1;
			mInputSequence = 0;
			mForceKeyframe = true;
			mInitialized = (connMgr != nullptr);
		}

		void NkNetWorld::Init(NkConnectionManager *connMgr, bool isServer) noexcept {
			Init(connMgr, isServer, Config());
		}

		void NkNetWorld::Shutdown() noexcept {
			mEntities.Clear();
			mSystems.Clear();
			mInputQueues.Clear();
			mConnMgr = nullptr;
			mInitialized = false;
		}

		bool NkNetWorld::IsInitialized() const noexcept {
			return mInitialized;
		}

		bool NkNetWorld::IsServer() const noexcept {
			return mIsServer;
		}

		// =====================================================================
		// NkNetWorld — registre des entités
		// =====================================================================

		NkNetId NkNetWorld::AllocateNetId() noexcept {
			if (!mIsServer) {
				return NkNetId::Invalid();
			}
			NkNetId id;
			id.id = mNextNetId++;
			id.owner = 0; // 0 = serveur
			return id;
		}

		bool NkNetWorld::RegisterEntity(const NkNetEntity &desc) noexcept {
			if (!desc.netId.IsValid()) {
				return false;
			}
			if (IndexOfEntity(desc.netId) >= 0) {
				return false; // déjà enregistrée
			}
			mEntities.PushBack(desc);
			// Reset de la baseline delta : l'entité doit partir entière.
			NkNetEntity &stored = mEntities[mEntities.Size() - 1];
			stored.everSent = false;
			stored.lastSentSize = 0;

			for (usize i = 0; i < mSystems.Size(); ++i) {
				if (mSystems[i] != nullptr) {
					mSystems[i]->OnEntityRegistered(stored);
				}
			}
			return true;
		}

		bool NkNetWorld::UnregisterEntity(NkNetId netId, bool notifyPeers) noexcept {
			const int32 index = IndexOfEntity(netId);
			if (index < 0) {
				return false;
			}
			mEntities.Erase(mEntities.Begin() + index);

			for (usize i = 0; i < mSystems.Size(); ++i) {
				if (mSystems[i] != nullptr) {
					mSystems[i]->OnEntityUnregistered(netId);
				}
			}

			// Serveur : informer les clients de la destruction (fiable + ordonné).
			if (mIsServer && notifyPeers && mConnMgr != nullptr) {
				uint8 buffer[64];
				NkBitWriter writer(buffer, sizeof(buffer));
				writer.WriteU8(kNkNetReplMagic);
				writer.WriteU8(kNkNetReplVersion);
				writer.WriteU8(static_cast<uint8>(MsgType::NK_REPL_DESPAWN));
				writer.WriteU16(1u);
				writer.WriteU64(netId.Pack());
				writer.AlignToByte();
				BroadcastBuffer(buffer, writer.BytesWritten(), NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
			}
			return true;
		}

		NkNetEntity *NkNetWorld::FindEntity(NkNetId netId) noexcept {
			const int32 index = IndexOfEntity(netId);
			if (index < 0) {
				return nullptr;
			}
			return &mEntities[index];
		}

		uint32 NkNetWorld::EntityCount() const noexcept {
			return static_cast<uint32>(mEntities.Size());
		}

		int32 NkNetWorld::IndexOfEntity(NkNetId netId) const noexcept {
			for (usize i = 0; i < mEntities.Size(); ++i) {
				if (mEntities[i].netId == netId) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		// =====================================================================
		// NkNetWorld — boucle de réplication
		// =====================================================================

		void NkNetWorld::Update(float32 dt) noexcept {
			if (!mInitialized || !mIsServer) {
				return;
			}

			const float32 tickInterval = 1.f / mConfig.tickRate;
			mTickAccumulator += dt;

			// Garde-fou : après un gros hitch, ne pas rattraper plus de 4 ticks.
			const float32 maxBacklog = tickInterval * 4.f;
			if (mTickAccumulator > maxBacklog) {
				mTickAccumulator = maxBacklog;
			}

			while (mTickAccumulator >= tickInterval) {
				mTickAccumulator -= tickInterval;
				++mTick;

				bool keyframe = mForceKeyframe;
				if (mConfig.keyframeInterval == 0u) {
					keyframe = true;
				} else if ((mTick % mConfig.keyframeInterval) == 0u) {
					keyframe = true;
				}
				mForceKeyframe = false;

				for (usize i = 0; i < mSystems.Size(); ++i) {
					if (mSystems[i] != nullptr) {
						mSystems[i]->OnBeforeSnapshot(mTick);
					}
				}

				BuildAndSendSnapshot(keyframe);
			}
		}

		void NkNetWorld::ForceKeyframe() noexcept {
			mForceKeyframe = true;
		}

		uint32 NkNetWorld::CurrentTick() const noexcept {
			return mTick;
		}

		void NkNetWorld::BuildAndSendSnapshot(bool keyframe) noexcept {
			if (mConnMgr == nullptr) {
				return;
			}

			// Marge de sécurité sous le payload max du transport.
			constexpr uint32 kMessageCapacity = kNkMaxPayloadSize - 32u;
			// En-tête : magic + version + type + flags + tick(u32) + count(u16).
			constexpr uint32 kHeaderSize = 1u + 1u + 1u + 1u + 4u + 2u;

			uint8 message[kNkMaxPayloadSize];
			uint8 scratch[kNkNetMaxEntityStateSize];
			uint8 entry[kNkNetMaxEntityStateSize + 32u];

			NkBitWriter writer(message, sizeof(message));
			uint16 countInMessage = 0;

			// (Ré)ouvre un message SNAPSHOT vide pour le tick courant.
			auto beginMessage = [&]() {
				writer = NkBitWriter(message, sizeof(message));
				writer.WriteU8(kNkNetReplMagic);
				writer.WriteU8(kNkNetReplVersion);
				writer.WriteU8(static_cast<uint8>(MsgType::NK_REPL_SNAPSHOT));
				writer.WriteU8(keyframe ? 1u : 0u);
				writer.WriteU32(mTick);
				writer.WriteU16(0u); // count patché à l'envoi
				countInMessage = 0;
			};

			// Envoie le message courant. Le count u16 occupe les octets 8-9
			// (après magic+version+type+flags aux octets 0-3 et tick u32 aux
			// octets 4-7) ; NkBitWriter écrit MSB-first → big-endian.
			auto flushMessage = [&]() {
				if (countInMessage == 0) {
					return;
				}
				writer.AlignToByte();
				const uint32 total = writer.BytesWritten();
				message[8] = static_cast<uint8>((countInMessage >> 8) & 0xFF);
				message[9] = static_cast<uint8>(countInMessage & 0xFF);
				BroadcastBuffer(message, total, mConfig.snapshotChannel);
			};

			beginMessage();

			for (usize i = 0; i < mEntities.Size(); ++i) {
				NkNetEntity &entity = mEntities[i];
				if (!entity.writeState) {
					continue;
				}

				// 1) Sérialise l'état dans un scratch isolé (framing garanti).
				NkBitWriter stateWriter(scratch, sizeof(scratch));
				entity.writeState(entity.user, stateWriter);
				stateWriter.AlignToByte();
				if (stateWriter.IsOverflowed()) {
					continue; // état > kNkNetMaxEntityStateSize : entité ignorée
				}
				const uint32 stateSize = stateWriter.BytesWritten();

				// 2) Delta : état inchangé depuis le dernier envoi → skip
				//    (sauf keyframe, qui ré-émet tout le monde).
				if (!keyframe && entity.everSent && stateSize == entity.lastSentSize &&
					memory::NkCompare(scratch, entity.lastSent, stateSize) == 0) {
					continue;
				}

				// 3) Encode l'entrée complète dans un tampon dédié pour
				//    connaître sa taille exacte avant insertion.
				NkBitWriter entryWriter(entry, sizeof(entry));
				entryWriter.WriteU64(entity.netId.Pack());
				entryWriter.WriteU32(entity.prefabId);
				entryWriter.WriteU64(entity.ownerPeer.value);
				entryWriter.WriteU16(static_cast<uint16>(stateSize));
				entryWriter.WriteBytes(scratch, stateSize);
				entryWriter.AlignToByte();
				const uint32 entrySize = entryWriter.BytesWritten();

				// 4) Si le message courant est plein : flush + nouveau message
				//    auto-suffisant (même tick).
				if (writer.BytesWritten() + entrySize > kMessageCapacity && countInMessage > 0) {
					flushMessage();
					beginMessage();
				}
				// Entrée plus grosse qu'un message entier : impossible à émettre.
				if (kHeaderSize + entrySize > kMessageCapacity) {
					continue;
				}

				writer.WriteBytes(entry, entrySize);
				++countInMessage;

				// 5) Mémorise la baseline delta.
				memory::NkCopy(entity.lastSent, scratch, stateSize);
				entity.lastSentSize = stateSize;
				entity.everSent = true;
			}

			flushMessage();
		}

		// =====================================================================
		// NkNetWorld — réception
		// =====================================================================

		bool NkNetWorld::HandleMessage(const NkReceiveMsg &msg) noexcept {
			if (!mInitialized || msg.size < 3u) {
				return false;
			}
			if (msg.data[0] != kNkNetReplMagic || msg.data[1] != kNkNetReplVersion) {
				return false;
			}

			NkBitReader reader(msg.data, msg.size);
			(void)reader.ReadU8(); // magic
			(void)reader.ReadU8(); // version
			const MsgType type = static_cast<MsgType>(reader.ReadU8());

			switch (type) {
				case MsgType::NK_REPL_SNAPSHOT:
					if (!mIsServer) {
						ApplySnapshot(reader);
					}
					return true;
				case MsgType::NK_REPL_DESPAWN:
					if (!mIsServer) {
						ApplyDespawn(reader);
					}
					return true;
				case MsgType::NK_REPL_INPUT:
					if (mIsServer) {
						QueueInput(msg, reader);
					}
					return true;
				default:
					// Magic/version corrects mais type inconnu : consommé
					// (évite de polluer l'application avec du bruit).
					return true;
			}
		}

		void NkNetWorld::ApplySnapshot(NkBitReader &reader) noexcept {
			const uint8 flags = reader.ReadU8();
			(void)flags;
			const uint32 tick = reader.ReadU32();
			const uint16 count = reader.ReadU16();

			// Canal SEQUENCED : un tick plus vieux que le dernier appliqué ne
			// devrait pas arriver, mais plusieurs messages PEUVENT porter le
			// même tick (snapshot scindé) — on n'écarte que le passé strict.
			if (tick < mTick) {
				return;
			}
			mTick = tick;

			uint8 stateBytes[kNkNetMaxEntityStateSize];

			for (uint16 e = 0; e < count; ++e) {
				if (reader.IsOverflowed()) {
					return; // message tronqué/corrompu : on abandonne proprement
				}
				const NkNetId netId = NkNetId::Unpack(reader.ReadU64());
				const uint32 prefabId = reader.ReadU32();
				NkPeerId owner;
				owner.value = reader.ReadU64();
				const uint16 stateSize = reader.ReadU16();
				if (stateSize > kNkNetMaxEntityStateSize) {
					return;
				}
				reader.ReadBytes(stateBytes, stateSize);
				reader.AlignToByte();
				if (reader.IsOverflowed()) {
					return;
				}

				NkNetEntity *entity = FindEntity(netId);
				if (entity == nullptr && onEntitySpawn) {
					// L'application crée son objet et appelle RegisterEntity()
					// dans le callback ; on retente le lookup derrière.
					onEntitySpawn(netId, prefabId, owner);
					entity = FindEntity(netId);
				}
				if (entity != nullptr && entity->readState) {
					// Reader isolé sur les octets d'état : un readState qui lit
					// trop/pas assez ne désynchronise pas le reste du message.
					NkBitReader stateReader(stateBytes, stateSize);
					entity->readState(entity->user, stateReader);
				}
			}

			for (usize i = 0; i < mSystems.Size(); ++i) {
				if (mSystems[i] != nullptr) {
					mSystems[i]->OnSnapshotApplied(tick);
				}
			}
		}

		void NkNetWorld::ApplyDespawn(NkBitReader &reader) noexcept {
			const uint16 count = reader.ReadU16();
			for (uint16 i = 0; i < count; ++i) {
				if (reader.IsOverflowed()) {
					return;
				}
				const NkNetId netId = NkNetId::Unpack(reader.ReadU64());
				if (onEntityDespawn) {
					onEntityDespawn(netId);
				}
				(void)UnregisterEntity(netId, false);
			}
		}

		// =====================================================================
		// NkNetWorld — inputs
		// =====================================================================

		NkNetResult NkNetWorld::SendInput(const uint8 *data, uint32 size) noexcept {
			if (!mInitialized || mConnMgr == nullptr) {
				return NkNetResult::NK_NET_NOT_CONNECTED;
			}
			if (mIsServer) {
				return NkNetResult::NK_NET_INVALID_ARG;
			}
			if (data == nullptr || size == 0 || size > kNkNetMaxInputSize) {
				return NkNetResult::NK_NET_INVALID_ARG;
			}

			uint8 buffer[kNkNetMaxInputSize + 32u];
			NkBitWriter writer(buffer, sizeof(buffer));
			writer.WriteU8(kNkNetReplMagic);
			writer.WriteU8(kNkNetReplVersion);
			writer.WriteU8(static_cast<uint8>(MsgType::NK_REPL_INPUT));
			writer.WriteU32(++mInputSequence);
			writer.WriteU32(mTick);
			writer.WriteU16(static_cast<uint16>(size));
			writer.WriteBytes(data, size);
			writer.AlignToByte();

			// Client : un seul pair connecté (le serveur) → broadcast = envoi.
			return mConnMgr->Broadcast(buffer, writer.BytesWritten(), mConfig.inputChannel);
		}

		void NkNetWorld::QueueInput(const NkReceiveMsg &msg, NkBitReader &reader) noexcept {
			NkNetInput input;
			input.sequence = reader.ReadU32();
			input.tick = reader.ReadU32();
			const uint16 size = reader.ReadU16();
			if (size == 0 || size > kNkNetMaxInputSize) {
				return;
			}
			reader.ReadBytes(input.data, size);
			if (reader.IsOverflowed()) {
				return;
			}
			input.size = size;
			input.from = msg.from;

			PeerInputQueue &queue = GetOrCreateInputQueue(msg.from);
			// Déduplication : canal séquencé + numéro croissant → on n'accepte
			// que les séquences strictement plus récentes.
			if (input.sequence <= queue.lastSequence) {
				return;
			}
			queue.lastSequence = input.sequence;
			queue.inputs.PushBack(input);
		}

		void NkNetWorld::DrainInputs(NkPeerId peer, NkVector<NkNetInput> &out) noexcept {
			for (usize i = 0; i < mInputQueues.Size(); ++i) {
				if (mInputQueues[i].peer == peer) {
					for (usize j = 0; j < mInputQueues[i].inputs.Size(); ++j) {
						out.PushBack(mInputQueues[i].inputs[j]);
					}
					mInputQueues[i].inputs.Clear();
					return;
				}
			}
		}

		NkNetWorld::PeerInputQueue &NkNetWorld::GetOrCreateInputQueue(NkPeerId peer) noexcept {
			for (usize i = 0; i < mInputQueues.Size(); ++i) {
				if (mInputQueues[i].peer == peer) {
					return mInputQueues[i];
				}
			}
			PeerInputQueue queue;
			queue.peer = peer;
			mInputQueues.PushBack(queue);
			return mInputQueues[mInputQueues.Size() - 1];
		}

		// =====================================================================
		// NkNetWorld — systèmes + helpers
		// =====================================================================

		void NkNetWorld::AddSystem(NkNetSystem *system) noexcept {
			if (system == nullptr) {
				return;
			}
			for (usize i = 0; i < mSystems.Size(); ++i) {
				if (mSystems[i] == system) {
					return;
				}
			}
			mSystems.PushBack(system);
		}

		void NkNetWorld::RemoveSystem(NkNetSystem *system) noexcept {
			for (usize i = 0; i < mSystems.Size(); ++i) {
				if (mSystems[i] == system) {
					mSystems.Erase(mSystems.Begin() + i);
					return;
				}
			}
		}

		void NkNetWorld::BroadcastBuffer(const uint8 *data, uint32 size, NkNetChannel channel) noexcept {
			if (mConnMgr == nullptr || data == nullptr || size == 0) {
				return;
			}
			(void)mConnMgr->Broadcast(data, size, channel);
		}

	} // namespace net

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// ============================================================
