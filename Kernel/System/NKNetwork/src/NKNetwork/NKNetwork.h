#pragma once
// =============================================================================
// NKNetwork/NKNetwork.h — Point d'entrée unique du module réseau
// =============================================================================
//
// ARCHITECTURE EN COUCHES (bas → haut) :
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │  COUCHE APPLICATION                                              │
//   │  NkLobby  •  NkSession  •  NkMatchmaker  •  NkDiscovery        │
//   │  NkHTTPClient  •  NkLeaderboard                                 │
//   ├─────────────────────────────────────────────────────────────────┤
//   │  COUCHE JEUX (ECS)                                               │
//   │  NkNetWorld  •  NkNetSystem  •  NkNetInterpolator               │
//   │  NkRPCRouter  •  NkNetEntity  •  NkNetInput  •  NkNetSnapshot  │
//   ├─────────────────────────────────────────────────────────────────┤
//   │  COUCHE PROTOCOLE                                                │
//   │  NkConnection  •  NkConnectionManager  •  NkBitStream           │
//   ├─────────────────────────────────────────────────────────────────┤
//   │  COUCHE TRANSPORT                                                │
//   │  NkReliableUDP  •  NkSocket  •  NkAddress  •  NkPacket         │
//   ├─────────────────────────────────────────────────────────────────┤
//   │  COUCHE SYSTÈME                                                  │
//   │  NkNetDefines  •  NkPeerId  •  NkNetId  •  NkTimestampMs       │
//   └─────────────────────────────────────────────────────────────────┘
//
// USAGE RAPIDE — Serveur :
//   NkSocket::PlatformInit();
//   NkConnectionManager server;
//   server.onPeerConnected    = [](NkPeerId p) { printf("Joueur connecté: %llu\n", p.value); };
//   server.onPeerDisconnected = [](NkPeerId p, const char* r) { ... };
//   server.StartServer(7777, 64);  // Port 7777, max 64 clients
//
//   NkNetWorld netWorld;
//   netWorld.Init(&ecsWorld, &server, true);  // isServer=true
//
//   // Game loop :
//   while (running) {
//       float dt = timer.DeltaTime();
//       netWorld.Update(dt);  // Réplique les entités
//
//       NkVector<NkReceiveMsg> msgs;
//       server.DrainAll(msgs);
//       for (auto& msg : msgs) { /* traiter messages */ }
//   }
//
// USAGE RAPIDE — Client :
//   NkSocket::PlatformInit();
//   NkConnectionManager client;
//   client.onPeerConnected = [](NkPeerId) { printf("Connecté au serveur!\n"); };
//   client.Connect(NkAddress("192.168.1.100", 7777));
//
//   NkNetWorld netWorld;
//   netWorld.Init(&ecsWorld, &client, false);  // isServer=false
//
//   // Game loop :
//   while (running) {
//       netWorld.Update(dt);  // Prediction + réception serveur
//   }
//
// USAGE RAPIDE — Lobby + Matchmaking :
//   NkLobby& lobby = NkLobby::Global();
//   lobby.CreateSession({"Ma Partie", "de_dust2", "Deathmatch", 10});
//   lobby.GetSession().SetReady(true);
//   // ou :
//   NkMatchmaker mm;
//   mm.SearchAsync({.gameMode="Deathmatch", .myELO=1200},
//       [](const NkMatchmaker::SearchResult& r) { client.Connect(r.serverAddr); },
//       [](NkNetResult err) { printf("Matchmaking échoué: %s\n", NkNetResultStr(err)); });
//
// USAGE RAPIDE — HTTP :
//   NkHTTPClient http;
//   auto r = http.Get("https://api.myjeu.com/version");
//   if (r.IsOK()) printf("Version: %s\n", r.body.CStr());
//
// =============================================================================

// ── Système ───────────────────────────────────────────────────────────────────
#include "Core/NkNetDefines.h"

// ── Transport ─────────────────────────────────────────────────────────────────
#include "Transport/NkSocket.h"
#include "Transport/NkReliableUDP.h"

// ── Protocole ─────────────────────────────────────────────────────────────────
#include "Protocol/NkBitStream.h"
#include "Protocol/NkConnection.h"

// ── Réplication ECS ───────────────────────────────────────────────────────────
#include "Replication/NkNetWorld.h"

// ── RPC ───────────────────────────────────────────────────────────────────────
#include "RPC/NkRPC.h"

// ── Lobby & Sessions ──────────────────────────────────────────────────────────
#include "Lobby/NkLobby.h"

// ── HTTP / REST ───────────────────────────────────────────────────────────────
#include "HTTP/NkHTTPClient.h"

// ── Alias de commodité ────────────────────────────────────────────────────────
namespace nkentseu {
    // Types fondamentaux
    using NkPeerId     = net::NkPeerId;
    using NkNetId      = net::NkNetId;
    using NkNetResult  = net::NkNetResult;
    using NkNetChannel = net::NkNetChannel;
    using NkAddress    = net::NkAddress;

    // Transport
    using NkSocket         = net::NkSocket;
    using NkPacket         = net::NkPacket;
    using NkReliableUDP    = net::NkReliableUDP;

    // Protocole
    using NkBitWriter          = net::NkBitWriter;
    using NkBitReader          = net::NkBitReader;
    using NkConnection         = net::NkConnection;
    using NkConnectionManager  = net::NkConnectionManager;
    using NkReceiveMsg         = net::NkReceiveMsg;

    // ECS
    using NkNetEntity      = net::NkNetEntity;
    using NkNetInput       = net::NkNetInput;
    using NkNetAuthority   = net::NkNetAuthority;
    using NkNetWorld       = net::NkNetWorld;
    using NkNetSystem      = net::NkNetSystem;
    using NkNetSnapshot    = net::NkNetSnapshot;
    using NkNetInterpolator = net::NkNetInterpolator;

    // RPC
    using NkRPCRouter  = net::NkRPCRouter;

    // Lobby
    using NkSession        = net::NkSession;
    using NkSessionConfig  = net::NkSessionConfig;
    using NkLobby          = net::NkLobby;
    using NkMatchmaker     = net::NkMatchmaker;
    using NkDiscovery      = net::NkDiscovery;
    using NkPlayerInfo     = net::NkPlayerInfo;

    // HTTP
    using NkHTTPClient   = net::NkHTTPClient;
    using NkHTTPRequest  = net::NkHTTPRequest;
    using NkHTTPResponse = net::NkHTTPResponse;
    using NkLeaderboard  = net::NkLeaderboard;
}
