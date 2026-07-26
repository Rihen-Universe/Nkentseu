// =============================================================================
// Applications/NkNetWorldDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour Engine/Noge/src/Noge/ECS/Replication/
// NkNetWorld (pont ECS -> net::NkNetWorld réel de NKNetwork, cf.
// Engine/Noge/ROADMAP.md, Phase I1, item 1). Enregistre 2 entités ECS
// "serveur" via NkNetSystem, boucle Update() en loopback local (127.0.0.1),
// et vérifie PAR ASSERTIONS que l'état des entités "client" converge vers
// l'état "serveur" après un snapshot -- même round-trip déjà validé sans
// ECS par Sandbox/System/NKNetwork/src/main.cpp::TestLoopback(), ici au
// travers de l'adaptateur ECS (NkNetSystem/NkNetEntity) plutôt qu'en pilotant
// net::NkNetWorld directement.
//
// "jenga test" / *_Tests sont bloqués par la politique de workspace (cf.
// Applications/NkEditableMeshDemo, même contournement) : cette petite
// application console n'est PAS une TestSuite, elle exécute le pipeline réel
// et affiche [OK]/[FAIL] par assertion manuelle. Zéro contexte GPU requis
// (sockets + CPU uniquement).
// =============================================================================
// PAS l'umbrella NKNetwork/NKNetwork.h : il injecte des alias de commodité
// dans `nkentseu` (dont `using NkNetSystem = net::NkNetSystem;` et
// `using NkNetEntity = net::NkNetEntity;`) qui entrent en collision directe
// avec l'adaptateur ECS de Noge (`nkentseu::NkNetSystem`, composant
// `nkentseu::ecs::NkNetEntity`). On inclut donc uniquement les en-têtes
// précis nécessaires.
#include "Noge/ECS/Replication/NkNetWorld.h"
#include "NKECS/World/NkWorld.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ecs;
// PAS de `using namespace nkentseu::net;` : net::NkNetEntity (descripteur de
// réplication NKNetwork) et ecs::NkNetEntity (composant ECS) partagent le
// même nom -- même piège que Noge/ECS/Replication/NkNetWorld.cpp. Les
// symboles net:: utilisés ici (peu nombreux) restent qualifiés explicitement.

namespace {

	int gFailCount = 0;
	int gPassCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	bool Near(float32 a, float32 b, float32 eps = 0.001f) noexcept {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= eps;
	}

	// Pompe les deux côtés pendant au plus `maxSteps` pas de 10ms, jusqu'à ce
	// que `done()` retourne true.
	template <typename DoneFn>
	bool Pump(NkNetSystem &serverNet, NkWorld &serverWorld, NkNetSystem &clientNet, NkWorld &clientWorld,
			  int maxSteps, DoneFn done) noexcept {
		for (int i = 0; i < maxSteps; ++i) {
			serverNet.Execute(serverWorld, 0.02f);
			clientNet.Execute(clientWorld, 0.02f);
			if (done()) {
				return true;
			}
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		return done();
	}

	// Trouve, côté client, l'entité NkNetEntity dont le netId correspond.
	NkEntityId FindClientEntityByNetId(NkWorld &clientWorld, net::NkNetId netId) noexcept {
		NkEntityId found = NkEntityId::Invalid();
		// `ecs::` explicite : l'umbrella NKNetwork.h exporte aussi un alias
		// `nkentseu::NkNetEntity` (= net::NkNetEntity), rendant le nom nu ambigu.
		clientWorld.Query<ecs::NkNetEntity, NkTransform>().ForEach(
			[&](NkEntityId id, ecs::NkNetEntity &netComp, NkTransform & /*tf*/) {
				if (netComp.netId == netId) {
					found = id;
				}
			});
		return found;
	}

} // namespace

int main() {
	logger.Infof("=== NkNetWorldDemo — preuve d'execution Noge/ECS/Replication/NkNetWorld ===\n");

	Check(net::NkSocket::PlatformInit() == net::NkNetResult::NK_NET_OK, "NkSocket::PlatformInit() reussi");

	constexpr uint16 kPort = 48521;

	net::NkConnectionManager server;
	Check(server.StartServer(kPort, 8) == net::NkNetResult::NK_NET_OK, "server.StartServer() reussi");

	net::NkConnectionManager client;
	Check(client.Connect(net::NkAddress::Loopback(kPort)) == net::NkNetResult::NK_NET_OK,
		  "client.Connect(loopback) reussi");

	// Attente du handshake 3-way (thread reseau interne des deux managers).
	bool connected = false;
	for (int i = 0; i < 500; ++i) {
		if (server.ConnectedPeerCount() >= 1 && client.ConnectedPeerCount() >= 1) {
			connected = true;
			break;
		}
		NkChrono::SleepMilliseconds(static_cast<int64>(10));
	}
	Check(connected, "handshake serveur<->client etabli (127.0.0.1)");

	if (!connected) {
		logger.Errorf("[NkNetWorldDemo] connexion non etablie -- suite du test sautee\n");
		client.Shutdown();
		server.Shutdown();
		net::NkSocket::PlatformShutdown();
		logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
		return 1;
	}

	// -------------------------------------------------------------------
	// Mondes ECS + adaptateurs NkNetSystem des deux cotes.
	// -------------------------------------------------------------------
	NkWorld serverWorld;
	NkWorld clientWorld;

	NkNetSystem serverNet;
	NkNetSystem clientNet;

	net::NkNetWorld::Config cfg;
	cfg.tickRate = 60.f;
	cfg.keyframeInterval = 4;

	serverNet.Init(&serverWorld, &server, /*isServer=*/true, cfg);
	clientNet.Init(&clientWorld, &client, /*isServer=*/false);

	Check(serverNet.IsServer(), "serverNet.IsServer() == true");
	Check(!clientNet.IsServer(), "clientNet.IsServer() == false");

	// -------------------------------------------------------------------
	// 1) Enregistre 2 entites ECS "serveur" via NkNetSystem::RegisterEntity.
	// -------------------------------------------------------------------
	const NkEntityId srvA = serverWorld.Create().With<NkTransform>(NkTransform{}).Build();
	serverWorld.GetRef<NkTransform>(srvA).SetLocalPosition(1.f, 2.f, 3.f);
	Check(serverNet.RegisterEntity(srvA, /*prefabId=*/10), "RegisterEntity(srvA, prefab=10) reussi");

	const NkEntityId srvB = serverWorld.Create().With<NkTransform>(NkTransform{}).Build();
	serverWorld.GetRef<NkTransform>(srvB).SetLocalPosition(-5.f, 0.5f, 9.f);
	Check(serverNet.RegisterEntity(srvB, /*prefabId=*/20), "RegisterEntity(srvB, prefab=20) reussi");

	const net::NkNetId netIdA = serverWorld.Get<ecs::NkNetEntity>(srvA)->netId;
	const net::NkNetId netIdB = serverWorld.Get<ecs::NkNetEntity>(srvB)->netId;
	Check(netIdA.IsValid() && netIdB.IsValid() && !(netIdA == netIdB), "netId A/B valides et distincts");

	// -------------------------------------------------------------------
	// 2) Pompe : le serveur emet ses snapshots, le client spawn + applique.
	// -------------------------------------------------------------------
	const bool spawned = Pump(serverNet, serverWorld, clientNet, clientWorld, 500, [&]() {
		return clientWorld.EntityCount() >= 2;
	});
	Check(spawned, "les 2 entites apparaissent cote client (onEntitySpawn)");

	const NkEntityId cliA = FindClientEntityByNetId(clientWorld, netIdA);
	const NkEntityId cliB = FindClientEntityByNetId(clientWorld, netIdB);
	Check(cliA != NkEntityId::Invalid(), "entite client A retrouvee par netId");
	Check(cliB != NkEntityId::Invalid(), "entite client B retrouvee par netId");

	if (cliA != NkEntityId::Invalid() && cliB != NkEntityId::Invalid()) {
		const auto posA = clientWorld.Get<NkTransform>(cliA)->localPosition;
		const auto posB = clientWorld.Get<NkTransform>(cliB)->localPosition;
		Check(Near(posA.x, 1.f) && Near(posA.y, 2.f) && Near(posA.z, 3.f),
			  "convergence client A == serveur (1,2,3) apres snapshot");
		Check(Near(posB.x, -5.f) && Near(posB.y, 0.5f) && Near(posB.z, 9.f),
			  "convergence client B == serveur (-5,0.5,9) apres snapshot");
	}

	// -------------------------------------------------------------------
	// 3) Delta : deplacement serveur -> re-convergence client (nouveau snapshot).
	// -------------------------------------------------------------------
	serverWorld.GetRef<NkTransform>(srvA).SetLocalPosition(42.f, -8.f, 100.f);
	const bool updated = Pump(serverNet, serverWorld, clientNet, clientWorld, 500, [&]() {
		const NkTransform *tf = clientWorld.Get<NkTransform>(cliA);
		return tf && Near(tf->localPosition.x, 42.f) && Near(tf->localPosition.y, -8.f) &&
			   Near(tf->localPosition.z, 100.f);
	});
	Check(updated, "convergence client A == nouvelle position serveur apres delta");

	// -------------------------------------------------------------------
	// 4) Despawn : retrait serveur -> destruction ECS cote client.
	// -------------------------------------------------------------------
	Check(serverNet.UnregisterEntity(srvB), "UnregisterEntity(srvB) reussi cote serveur");
	const bool despawned = Pump(serverNet, serverWorld, clientNet, clientWorld, 500, [&]() {
		return FindClientEntityByNetId(clientWorld, netIdB) == NkEntityId::Invalid();
	});
	Check(despawned, "entite B detruite cote client apres DESPAWN");
	Check(clientWorld.EntityCount() == 1, "clientWorld.EntityCount() == 1 apres despawn (A restante)");

	// -------------------------------------------------------------------
	// Nettoyage.
	// -------------------------------------------------------------------
	serverNet.Shutdown();
	clientNet.Shutdown();
	client.Shutdown();
	server.Shutdown();
	net::NkSocket::PlatformShutdown();

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
