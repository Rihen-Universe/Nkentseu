// =============================================================================
// FICHIER  : Sandbox/System/NKNetwork/src/main.cpp
// PROJET   : SandboxNKNetwork
// OBJET    : Validation compile + exécution du module NKNetwork :
//            • NkBitStream : round-trip + détection d'overflow
//            • NkNetId : Pack/Unpack
//            • NkNetInterpolator : clamps aux bornes + alpha au milieu
//            • Réplication client : SNAPSHOT/DESPAWN forgés → spawn/état/retrait
//            • Réplication serveur : INPUT forgé → file par pair + déduplication
//            • Bout-en-bout loopback : serveur + client réels sur 127.0.0.1
//              (handshake, snapshots NkNetWorld, delta, despawn fiable)
//            Inclut l'umbrella NKNetwork.h — valide au passage l'include
//            Replication/NkNetWorld.h (historiquement cassé).
// =============================================================================

#include "NKNetwork/NKNetwork.h"
#include "NKTime/NkChrono.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::net;

namespace {

	int gChecks = 0;
	int gFails = 0;

#define NK_CHECK(cond)                                                                                       \
	do {                                                                                                     \
		++gChecks;                                                                                           \
		if (!(cond)) {                                                                                       \
			++gFails;                                                                                        \
			std::printf("[FAIL] %s:%d : %s\n", __FILE__, __LINE__, #cond);                                   \
		}                                                                                                    \
	} while (0)

	// État répliqué de test : deux floats (x, y).
	struct TestPawn {
			float32 x = 0.f;
			float32 y = 0.f;
	};

	// -----------------------------------------------------------------------
	// NkBitStream
	// -----------------------------------------------------------------------
	void TestBitStream() {
		uint8 buffer[256];
		NkBitWriter writer(buffer, sizeof(buffer));

		writer.WriteBool(true);
		writer.WriteU8(0xAB);
		writer.WriteU16(0x1234);
		writer.WriteU32(0xDEADBEEF);
		writer.WriteU64(0x0123456789ABCDEFull);
		writer.WriteI32(-42);
		writer.WriteF32(3.5f);
		writer.WriteBits(5u, 3);
		writer.AlignToByte();
		const uint8 raw[4] = {1, 2, 3, 4};
		writer.WriteBytes(raw, 4);
		NK_CHECK(!writer.IsOverflowed());

		NkBitReader reader(buffer, writer.BytesWritten());
		NK_CHECK(reader.ReadBool() == true);
		NK_CHECK(reader.ReadU8() == 0xAB);
		NK_CHECK(reader.ReadU16() == 0x1234);
		NK_CHECK(reader.ReadU32() == 0xDEADBEEF);
		NK_CHECK(reader.ReadU64() == 0x0123456789ABCDEFull);
		NK_CHECK(reader.ReadI32() == -42);
		NK_CHECK(reader.ReadF32() == 3.5f);
		NK_CHECK(reader.ReadBits(3) == 5u);
		reader.AlignToByte();
		uint8 back[4] = {};
		reader.ReadBytes(back, 4);
		NK_CHECK(back[0] == 1 && back[1] == 2 && back[2] == 3 && back[3] == 4);
		NK_CHECK(!reader.IsOverflowed());

		// Overflow en écriture comme en lecture.
		uint8 tiny[2];
		NkBitWriter tinyWriter(tiny, sizeof(tiny));
		tinyWriter.WriteU32(0xFFFFFFFFu);
		NK_CHECK(tinyWriter.IsOverflowed());

		NkBitReader tinyReader(tiny, sizeof(tiny));
		(void)tinyReader.ReadU32();
		NK_CHECK(tinyReader.IsOverflowed());

		std::printf("[ OK  ] NkBitStream : round-trip + overflow\n");
	}

	// -----------------------------------------------------------------------
	// NkNetId
	// -----------------------------------------------------------------------
	void TestNetId() {
		NkNetId id;
		id.id = 0xCAFEBABEu;
		id.owner = 7;
		NK_CHECK(id.IsValid());
		NK_CHECK(NkNetId::Unpack(id.Pack()) == id);
		NK_CHECK(!NkNetId::Invalid().IsValid());
		std::printf("[ OK  ] NkNetId : Pack/Unpack\n");
	}

	// -----------------------------------------------------------------------
	// NkNetInterpolator
	// -----------------------------------------------------------------------
	void TestInterpolator() {
		NkNetInterpolator interp(8);
		const uint8 stateA[4] = {1, 0, 0, 0};
		const uint8 stateB[4] = {2, 0, 0, 0};

		NK_CHECK(interp.Count() == 0);
		interp.Push(1, 1000, stateA, 4);
		interp.Push(2, 1100, stateB, 4);
		interp.Push(2, 1150, stateB, 4); // doublon de tick : ignoré
		NK_CHECK(interp.Count() == 2);
		NK_CHECK(interp.LatestTick() == 2);

		const NkNetSnapshot *a = nullptr;
		const NkNetSnapshot *b = nullptr;
		float32 alpha = -1.f;

		NK_CHECK(interp.Sample(1050, &a, &b, alpha));
		NK_CHECK(a != nullptr && b != nullptr);
		NK_CHECK(a->tick == 1 && b->tick == 2);
		NK_CHECK(alpha > 0.49f && alpha < 0.51f);

		NK_CHECK(interp.Sample(500, &a, &b, alpha));
		NK_CHECK(a == b && a->tick == 1); // clamp avant

		NK_CHECK(interp.Sample(9999, &a, &b, alpha));
		NK_CHECK(a == b && a->tick == 2); // clamp après

		interp.Clear();
		NK_CHECK(interp.Count() == 0);
		NK_CHECK(!interp.Sample(1050, &a, &b, alpha));

		std::printf("[ OK  ] NkNetInterpolator : Push/Sample/clamps\n");
	}

	// -----------------------------------------------------------------------
	// Forge un message SNAPSHOT à 1 entité (même format que NkNetWorld).
	// -----------------------------------------------------------------------
	void ForgeSnapshot(NkReceiveMsg &msg, uint32 tick, NkNetId netId, uint32 prefabId, float32 x, float32 y) {
		uint8 state[16];
		NkBitWriter sw(state, sizeof(state));
		sw.WriteF32(x);
		sw.WriteF32(y);
		sw.AlignToByte();

		NkBitWriter w(msg.data, sizeof(msg.data));
		w.WriteU8(kNkNetReplMagic);
		w.WriteU8(kNkNetReplVersion);
		w.WriteU8(1); // NK_REPL_SNAPSHOT
		w.WriteU8(1); // flags : keyframe
		w.WriteU32(tick);
		w.WriteU16(1); // count
		w.WriteU64(netId.Pack());
		w.WriteU32(prefabId);
		w.WriteU64(0); // owner = serveur
		w.WriteU16(static_cast<uint16>(sw.BytesWritten()));
		w.WriteBytes(state, sw.BytesWritten());
		w.AlignToByte();
		msg.size = w.BytesWritten();
	}

	// -----------------------------------------------------------------------
	// Réplication client : SNAPSHOT forgé → spawn + application d'état
	// -----------------------------------------------------------------------
	void TestClientSnapshot() {
		NkConnectionManager mgr; // jamais démarrée : HandleMessage n'envoie rien
		NkNetWorld world;
		world.Init(&mgr, /*isServer=*/false);

		TestPawn pawn;
		uint32 spawnedPrefab = 0;

		world.onEntitySpawn = [&](NkNetId netId, uint32 prefabId, NkPeerId /*owner*/) {
			spawnedPrefab = prefabId;
			NkNetEntity desc;
			desc.netId = netId;
			desc.prefabId = prefabId;
			desc.user = &pawn;
			desc.readState = [](void *user, NkBitReader &r) {
				TestPawn *p = static_cast<TestPawn *>(user);
				p->x = r.ReadF32();
				p->y = r.ReadF32();
			};
			world.RegisterEntity(desc);
		};

		NkNetId netId;
		netId.id = 42;

		NkReceiveMsg msg;
		ForgeSnapshot(msg, 10, netId, 777, 1.5f, -2.5f);
		NK_CHECK(world.HandleMessage(msg));
		NK_CHECK(spawnedPrefab == 777u);
		NK_CHECK(world.EntityCount() == 1);
		NK_CHECK(world.FindEntity(netId) != nullptr);
		NK_CHECK(pawn.x == 1.5f && pawn.y == -2.5f);
		NK_CHECK(world.CurrentTick() == 10u);

		// Deuxième snapshot : pas de re-spawn, état mis à jour.
		spawnedPrefab = 0;
		ForgeSnapshot(msg, 11, netId, 777, 8.f, 9.f);
		NK_CHECK(world.HandleMessage(msg));
		NK_CHECK(spawnedPrefab == 0u);
		NK_CHECK(world.EntityCount() == 1);
		NK_CHECK(pawn.x == 8.f && pawn.y == 9.f);

		// Message non-réplication : refusé (à traiter par l'application).
		NkReceiveMsg other;
		other.data[0] = 0x4C; // magic lobby
		other.size = 16;
		NK_CHECK(!world.HandleMessage(other));

		std::printf("[ OK  ] NkNetWorld client : snapshot -> spawn + etat\n");
	}

	// -----------------------------------------------------------------------
	// Réplication client : DESPAWN forgé
	// -----------------------------------------------------------------------
	void TestClientDespawn() {
		NkConnectionManager mgr;
		NkNetWorld world;
		world.Init(&mgr, false);

		NkNetId netId;
		netId.id = 42;

		TestPawn pawn;
		NkNetEntity desc;
		desc.netId = netId;
		desc.user = &pawn;
		NK_CHECK(world.RegisterEntity(desc));
		NK_CHECK(!world.RegisterEntity(desc)); // doublon refusé
		NK_CHECK(world.EntityCount() == 1);

		bool despawned = false;
		world.onEntityDespawn = [&](NkNetId id) { despawned = (id == netId); };

		NkReceiveMsg msg;
		NkBitWriter w(msg.data, sizeof(msg.data));
		w.WriteU8(kNkNetReplMagic);
		w.WriteU8(kNkNetReplVersion);
		w.WriteU8(2); // NK_REPL_DESPAWN
		w.WriteU16(1);
		w.WriteU64(netId.Pack());
		w.AlignToByte();
		msg.size = w.BytesWritten();

		NK_CHECK(world.HandleMessage(msg));
		NK_CHECK(despawned);
		NK_CHECK(world.EntityCount() == 0);

		std::printf("[ OK  ] NkNetWorld client : despawn\n");
	}

	// -----------------------------------------------------------------------
	// Réplication serveur : INPUT forgé + déduplication
	// -----------------------------------------------------------------------
	void TestServerInputs() {
		NkConnectionManager mgr;
		NkNetWorld world;
		world.Init(&mgr, /*isServer=*/true);

		NkPeerId sender;
		sender.value = 55;

		auto forgeInput = [&](NkReceiveMsg &msg, uint32 seq, uint8 payload) {
			NkBitWriter w(msg.data, sizeof(msg.data));
			w.WriteU8(kNkNetReplMagic);
			w.WriteU8(kNkNetReplVersion);
			w.WriteU8(3); // NK_REPL_INPUT
			w.WriteU32(seq);
			w.WriteU32(100); // tick client
			w.WriteU16(1);
			w.WriteU8(payload);
			w.AlignToByte();
			msg.size = w.BytesWritten();
			msg.from = sender;
		};

		NkReceiveMsg msg;
		forgeInput(msg, 1, 0x11);
		NK_CHECK(world.HandleMessage(msg));
		forgeInput(msg, 2, 0x22);
		NK_CHECK(world.HandleMessage(msg));
		forgeInput(msg, 2, 0x33); // rejoué : jeté
		NK_CHECK(world.HandleMessage(msg));
		forgeInput(msg, 1, 0x44); // en retard : jeté
		NK_CHECK(world.HandleMessage(msg));

		NkVector<NkNetInput> inputs;
		world.DrainInputs(sender, inputs);
		NK_CHECK(inputs.Size() == 2);
		NK_CHECK(inputs[0].sequence == 1 && inputs[0].data[0] == 0x11);
		NK_CHECK(inputs[1].sequence == 2 && inputs[1].data[0] == 0x22);
		NK_CHECK(inputs[1].from == sender);

		inputs.Clear();
		world.DrainInputs(sender, inputs);
		NK_CHECK(inputs.Size() == 0); // file vidée après drain

		std::printf("[ OK  ] NkNetWorld serveur : inputs + deduplication\n");
	}

	// -----------------------------------------------------------------------
	// Bout-en-bout loopback : serveur + client réels sur 127.0.0.1
	// -----------------------------------------------------------------------
	void TestLoopback() {
		NK_CHECK(NkSocket::PlatformInit() == NkNetResult::NK_NET_OK);

		constexpr uint16 kPort = 48213;

		NkConnectionManager server;
		NK_CHECK(server.StartServer(kPort, 8) == NkNetResult::NK_NET_OK);

		NkConnectionManager client;
		NK_CHECK(client.Connect(NkAddress("127.0.0.1", kPort)) == NkNetResult::NK_NET_OK);

		// Attente de l'établissement de la connexion (handshake 3-way).
		bool connected = false;
		for (int i = 0; i < 500; ++i) {
			if (server.ConnectedPeerCount() >= 1 && client.ConnectedPeerCount() >= 1) {
				connected = true;
				break;
			}
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		NK_CHECK(connected);
		if (!connected) {
			std::printf("[FAIL] loopback : connexion non etablie, suite du test sautee\n");
			client.Shutdown();
			server.Shutdown();
			NkSocket::PlatformShutdown();
			return;
		}

		// Monde serveur : une entité répliquée.
		NkNetWorld::Config cfg;
		cfg.tickRate = 60.f;
		cfg.keyframeInterval = 4;

		NkNetWorld serverWorld;
		serverWorld.Init(&server, true, cfg);

		TestPawn serverPawn;
		serverPawn.x = 12.5f;
		serverPawn.y = -7.25f;

		NkNetEntity desc;
		desc.netId = serverWorld.AllocateNetId();
		desc.prefabId = 99;
		desc.user = &serverPawn;
		desc.writeState = [](void *user, NkBitWriter &w) {
			TestPawn *p = static_cast<TestPawn *>(user);
			w.WriteF32(p->x);
			w.WriteF32(p->y);
		};
		NK_CHECK(serverWorld.RegisterEntity(desc));

		// Monde client : spawn à la volée.
		NkNetWorld clientWorld;
		clientWorld.Init(&client, false);

		TestPawn clientPawn;
		bool spawned = false;
		clientWorld.onEntitySpawn = [&](NkNetId netId, uint32 prefabId, NkPeerId /*owner*/) {
			spawned = (prefabId == 99u);
			NkNetEntity d;
			d.netId = netId;
			d.prefabId = prefabId;
			d.user = &clientPawn;
			d.readState = [](void *user, NkBitReader &r) {
				TestPawn *p = static_cast<TestPawn *>(user);
				p->x = r.ReadF32();
				p->y = r.ReadF32();
			};
			clientWorld.RegisterEntity(d);
		};

		// Pompe : le serveur émet ses snapshots, le client draine et route.
		bool applied = false;
		NkVector<NkReceiveMsg> msgs;
		for (int i = 0; i < 500 && !applied; ++i) {
			serverWorld.Update(0.02f);
			msgs.Clear();
			client.DrainAll(msgs);
			for (usize m = 0; m < msgs.Size(); ++m) {
				(void)clientWorld.HandleMessage(msgs[m]);
			}
			applied = spawned && clientPawn.x == 12.5f && clientPawn.y == -7.25f;
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		NK_CHECK(spawned);
		NK_CHECK(applied);
		NK_CHECK(clientWorld.EntityCount() == 1);

		// Delta : changement d'état → nouvel état reçu.
		serverPawn.x = 100.f;
		bool updated = false;
		for (int i = 0; i < 500 && !updated; ++i) {
			serverWorld.Update(0.02f);
			msgs.Clear();
			client.DrainAll(msgs);
			for (usize m = 0; m < msgs.Size(); ++m) {
				(void)clientWorld.HandleMessage(msgs[m]);
			}
			updated = (clientPawn.x == 100.f);
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		NK_CHECK(updated);

		// Despawn répliqué (message fiable).
		bool despawned = false;
		clientWorld.onEntityDespawn = [&](NkNetId) { despawned = true; };
		NK_CHECK(serverWorld.UnregisterEntity(desc.netId));
		for (int i = 0; i < 500 && !despawned; ++i) {
			msgs.Clear();
			client.DrainAll(msgs);
			for (usize m = 0; m < msgs.Size(); ++m) {
				(void)clientWorld.HandleMessage(msgs[m]);
			}
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		NK_CHECK(despawned);
		NK_CHECK(clientWorld.EntityCount() == 0);

		client.Shutdown();
		server.Shutdown();
		NkSocket::PlatformShutdown();

		std::printf("[ OK  ] Loopback 127.0.0.1 : handshake + snapshot + delta + despawn\n");
	}

} // namespace

int main(int argc, char **argv) {
	// Mode HTTPS manuel : SandboxNKNetwork --https <url>
	// Nécessite un build avec NK_ENABLE_TLS=1 (backend mbedTLS), sinon le
	// client renvoie proprement "HTTPS not compiled in".
	if (argc >= 3 && NkString(argv[1]) == NkString("--https")) {
		NkHTTPClient http;
		const NkHTTPResponse resp = http.Get(argv[2]);
		std::printf("[HTTPS] %s -> status=%u bodyLen=%u error='%s'\n", argv[2], resp.statusCode,
					resp.body.Length(), resp.error.CStr());
		return (resp.statusCode >= 200 && resp.statusCode < 400) ? 0 : 1;
	}

	std::printf("=== SandboxNKNetwork : tests du module reseau ===\n");

	TestBitStream();
	TestNetId();
	TestInterpolator();
	TestClientSnapshot();
	TestClientDespawn();
	TestServerInputs();
	TestLoopback();

	std::printf("=== Resultat : %d/%d checks OK (%d echecs) ===\n", gChecks - gFails, gChecks, gFails);
	return (gFails == 0) ? 0 : 1;
}
