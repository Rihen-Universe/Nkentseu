# NKNetwork — Roadmap

État actuel (mai 2026) : Module en construction avec **architecture posée et
couches basses implémentées**, mais plusieurs couches hautes annoncées dans
l'umbrella header sont absentes du dépôt. Le pitch (`NKNetwork.h`) décrit cinq
couches : système, transport, protocole, ECS replication, application. Sont
réellement présents : Core (NkNetDefines), Transport (NkSocket + NkReliableUDP),
Protocol (NkBitStream + NkConnection), RPC (NkRPC), Lobby (NkLobby) et HTTP
(NkHTTPClient). **La couche Replication ECS (NkNetWorld) est référencée par
`NKNetwork.h` mais n'existe pas sur disque** — include cassé. **Aucun test ni
benchmark**.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Core — NkNetDefines (types, plateforme, IDs, channels, résultats) | Livré | — | — |
| Transport — NkSocket (UDP/TCP, IPv4/IPv6, Winsock2/POSIX, non-blocking) | Livré | — | — |
| Transport — NkReliableUDP (ACK, retransmit, sequence, channels) | Livré | — | — |
| Protocol — NkBitStream (Writer/Reader bit-précis, quantification) | Livré | — | — |
| Protocol — NkConnection + ConnectionManager (handshake 3-way, heartbeat) | Livré | — | — |
| RPC — NkRPCRouter (ServerRPC/ClientRPC/MulticastRPC, garanties) | Livré (déclaratif) | — | — |
| Lobby — NkSession / NkLobby / NkMatchmaker / NkDiscovery (LAN broadcast) | Livré | — | — |
| HTTP — NkHTTPClient (GET/POST, sync/async, TLS mbedTLS/OpenSSL) | Livré | — | — |
| Replication ECS — NkNetWorld / NkNetSystem / NkNetSnapshot / NkNetInterpolator | TODO (include cassé) | XL | Haute |
| Replication — NkNetEntity / NkNetInput / NkNetAuthority | TODO | L | Haute |
| Tests unitaires (aucun dossier `tests/`) | TODO | M | Haute |
| TLS réel câblé (mbedTLS ou OpenSSL) | Partiel | L | Haute |
| WebAssembly — emscripten_fetch pour HTTP | Partiel | M | Moyenne |
| WebSocket / WebRTC pour transport browser | TODO | XL | Basse |
| Compression LZ4/Zstd pour snapshots | TODO | M | Moyenne |
| Encryption end-to-end (DTLS) | TODO | XL | Basse |
| NAT traversal (STUN / TURN / ICE) | TODO | XL | Basse |
| Stats / métriques runtime (RTT, packet loss, bandwidth) | Partiel | S | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Couche Système — NkNetDefines
- [NkNetDefines](src/NKNetwork/Core/NkNetDefines.h) :
  - Détection plateforme (Windows Winsock2 / POSIX socket / Emscripten).
  - Types fondamentaux : `NkPeerId`, `NkNetId`, `NkNetResult`,
    `NkNetChannel`, `NkAddress`, `NkTimestampMs`.
  - Macros cross-platform `WIN32_LEAN_AND_MEAN` guardées (cohabitation avec
    `Pong.jenga` CLI defines).
  - Dépend de NKCore, NKContainers, NKThreading, NKLogger, NKPlatform.

### Couche Transport
- [NkSocket](src/NKNetwork/Transport/NkSocket.h) : abstraction UDP + TCP,
  IPv4/IPv6, DNS, non-blocking. `PlatformInit()` requis avant usage. Handle
  natif typé (`NkNativeSocketHandle`).
- [NkReliableUDP](src/NKNetwork/Transport/NkReliableUDP.h) : couche fiabilité
  inspirée ENet/GameNetworkingSockets/RakNet.
  - Header RUDP avec seqNum + ackNum + ackMask 32-bit (ACK sélectif).
  - Fenêtre d'envoi `NkSendWindow`, retransmit basé sur RTT.
  - 4 canaux : Unreliable / ReliableOrdered / ReliableUnordered / Sequenced.
  - Fragmentation/réassemblage des gros messages.

### Couche Protocole
- [NkBitStream](src/NKNetwork/Protocol/NkBitStream.h) : sérialisation bit-à-bit.
  - `NkBitWriter` / `NkBitReader` avec `WriteBits(N)`, `WriteBool`,
    `WriteU8/U16/U32/U64`, `WriteF32Q(min, max, precision)` quantifié,
    `WriteString`. `IsOverflowed()` pour détection corruption.
- [NkConnection / NkConnectionManager](src/NKNetwork/Protocol/NkConnection.h) :
  - Machine à 6 états : DISCONNECTED / SYN_SENT / SYN_RECEIVED / ESTABLISHED
    / DISCONNECTING / TIMED_OUT.
  - Handshake 3-way simplifié (SYN / SYN-ACK / ACK).
  - Heartbeat 250 ms, timeout 10 s, mise à jour `mLastActivityAt`.
  - `NkConnectionManager` thread-safe (mutex sur la map peers).
  - Callbacks `onPeerConnected`, `onPeerDisconnected`, `DrainAll(NkVector<NkReceiveMsg>&)`.

### RPC
- [NkRPC](src/NKNetwork/RPC/NkRPC.h) : 3 types (ServerRPC, ClientRPC,
  MulticastRPC) × 3 garanties (Reliable, ReliableOrd, Unreliable).
- Macros `NK_RPC_SERVER / CLIENT / MULTICAST`. Validation côté serveur,
  sérialisation via NkBitStream.

### Lobby & matchmaking
- [NkLobby](src/NKNetwork/Lobby/NkLobby.h) : `NkSession`, `NkLobbyPlayer`,
  `NkSessionConfig`, `NkPlayerInfo`.
- `NkMatchmaker` : recherche async par gameMode + ELO + ping/région.
- `NkDiscovery` : UDP broadcast LAN, port 7778 par défaut.
- Singleton `NkLobby::Global()`.

### HTTP / REST
- [NkHTTPClient](src/NKNetwork/HTTP/NkHTTPClient.h) :
  - Sync : `Get(url)`, `Post(url, body)`, `Send(req)` → `NkHTTPResponse`.
  - Async : `SendAsync(req, callback)`, `Cancel(reqId)`.
  - Helpers : `SetJSON`, `SetBearerToken`, `URLEncode/Decode`,
    `Base64Encode/Decode`.
  - Suivi de redirections 3xx configurable, timeout, vérif SSL configurable.
- `NkLeaderboard` exposé comme alias (couche au-dessus de NkHTTPClient pour
  classements en ligne).

### Aliases pratiques
L'umbrella [NKNetwork.h](src/NKNetwork/NKNetwork.h) expose les types principaux
sous `nkentseu::Nk*` (sans le namespace `net::`).

---

## En cours / TODO immédiat

### Bloquant — Include cassé Replication
`NKNetwork.h` inclut `"Replication/NkNetWorld.h"` mais aucun dossier
`Replication/` n'existe sous `src/NKNetwork/`. Les aliases exposent
`NkNetEntity`, `NkNetInput`, `NkNetAuthority`, `NkNetWorld`, `NkNetSystem`,
`NkNetSnapshot`, `NkNetInterpolator` mais leurs déclarations sont absentes.
**Compilation actuelle de l'umbrella = échec garanti**.

Actions immédiates au choix :
1. Stub minimal `Replication/NkNetWorld.h` avec forward decls vides
   pour débloquer.
2. Implémenter pour de bon (effort XL, cf. ci-dessous).
3. Retirer temporairement l'include et les aliases jusqu'à implémentation.

### Tests
Aucun dossier `tests/` n'existe (contrairement aux autres modules System).
Manque urgent :
- Smoke socket (open/bind/sendto/recvfrom loopback).
- Smoke BitStream (write/read symétrique, overflow detection).
- Smoke handshake (client/server local, ESTABLISHED des deux côtés).
- Smoke HTTP GET sur `httpbin.org/get` ou serveur local (CI).

### TLS réel
La doc mentionne mbedTLS / OpenSSL, mais le `.cpp` HTTPClient inclut
`<chrono>` / `<thread>` et `winsock2.h` sans backend TLS visible. Choisir un
fournisseur (mbedTLS recommandé pour la portabilité PV3DE) et le câbler. Si
HTTPS n'est pas requis pour le MVP, documenter HTTP-only.

---

## À venir / À ajouter (futur proche)

### Replication ECS — couche prioritaire
Pilier annoncé du module pour PV3DE/Unkeny et tout jeu multijoueur :
- **NkNetEntity** : composant marquant qu'une entité est répliquée + son
  `NkNetId` global.
- **NkNetAuthority** : flag client-authoritative / server-authoritative /
  shared.
- **NkNetWorld** : système ECS qui parcourt les `NkNetEntity` et émet
  snapshots (delta-compression entre frames).
- **NkNetSystem** : interface pour les systèmes répliquants (Transform,
  Animation, Health, ...).
- **NkNetSnapshot** : structure de snapshot avec tick + données compressées.
- **NkNetInterpolator** : interpolation côté client (entity smoothing,
  extrapolation pendant les pertes paquets).
- **NkNetInput** : capture des inputs locaux côté client, send au server pour
  authoritative simulation.

Architecture cible : tick rate configurable (20-60 Hz), client-side prediction,
server reconciliation, lag compensation (rewind/replay).

### Sécurité et fiabilité
- DTLS / handshake chiffré sur RUDP (échange clé Diffie-Hellman ou Ed25519).
- Anti-cheat de base : signed inputs, rate-limit RPC, sanity-check positions.
- Encryption AES-GCM des payloads sensibles.

### NAT traversal
- STUN client pour découvrir public IP/port.
- TURN relay pour les peers derrière NAT symétrique.
- ICE-lite (candidate pairing) si full ICE trop lourd.

### Compression
- LZ4 ou Zstd sur les snapshots > N bytes.
- Delta-compression d'état entre snapshots successifs (RLE / variable-width).

### Métriques runtime
- Per-peer : RTT moyenne / jitter / packet loss / bandwidth in/out.
- Per-channel : drops, retransmits, OoO.
- Exposition via callback ou `NkRenderer` debug overlay (graph).

### Backends supplémentaires
- WebSocket pour clients Web (Emscripten) — remplace UDP par TCP framing.
- WebRTC DataChannel pour P2P browser (DTLS+SCTP intégré).
- KCP-like alternative pour mobile à très haute latence.

### HTTP avancé
- HTTP/2 (multiplexage)
- WebSocket client (au-delà du HTTP REST).
- Upload multipart/form-data, gzip request bodies, gzip/br decoding.

---

## Bugs / quirks connus
- **`NKNetwork.h` ne compile pas tel quel** : include `Replication/NkNetWorld.h`
  pointe vers un fichier inexistant. Bloque toute inclusion depuis un autre
  module.
- Aliases sous `nkentseu::Nk*` dépendent du namespace `net::` (`using NkPeerId
  = net::NkPeerId`), à vérifier que ce namespace est bien défini dans
  `NkNetDefines.h` (la doc dit "net::" mais la convention projet utilise
  `nkentseu::`).
- Aucun test : régressions silencieuses lors des refactors.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKPlatform (détection OS, Winsock vs
  POSIX), NKCore (Types, Atomic), NKContainers (NkString, NkVector, NkSpan,
  NkFunction), NKThreading (Mutex, Thread pour async + ConnectionManager),
  NKLogger (debug des connexions).
- **Modules au-dessus qui en dépendent** : NKScene (NetEntity components une
  fois implémentés), Runtime (NetWorld dans la game loop), Unkeny (HTTP pour
  marketplace assets, télémétrie, plugins remote), PV3DE (HTTPClient pour API
  médicale FHIR — cf. ARCHITECTURE.md §5.10 export rapport FHIR/PDF).
