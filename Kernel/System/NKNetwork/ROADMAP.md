# NKNetwork — Roadmap

État actuel (2026-07-12) : **les cinq couches annoncées par l'umbrella existent
et compilent**. Core (NkNetDefines), Transport (NkSocket UDP/TCP + NkReliableUDP),
Protocol (NkBitStream + NkConnection), **Replication (NkNetWorld — agnostique
ECS, V1 livrée)**, RPC (NkRPC), Lobby (NkLobby) et HTTP/**HTTPS** (NkHTTPClient +
backend TLS mbedTLS opt-in, GET https réel validé status 200). Zero-STL depuis
2026-07-09. **Tests exécutables** : `SandboxNKNetwork` (67 checks verts, dont un
bout-en-bout loopback 127.0.0.1 : handshake + snapshots + delta + despawn).

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
| Replication — NkNetWorld / NkNetSystem / NkNetSnapshot / NkNetInterpolator | Livré (V1, agnostique ECS) | — | — |
| Replication — NkNetEntity / NkNetInput / NkNetAuthority | Livré (V1) | — | — |
| Tests exécutables (`Sandbox/System/NKNetwork`, 67 checks + loopback réel) | Livré | — | — |
| TLS réel câblé (mbedTLS, opt-in `NK_ENABLE_TLS=1`, HTTPS validé 200) | Livré | — | — |
| Replication — prédiction client + réconciliation serveur | TODO | L | Moyenne |
| Replication — relais des entités client-authoritative | TODO | M | Moyenne |
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

### Réplication (2026-07-12) — AGNOSTIQUE du système d'entités
- [NkNetWorld](src/NKNetwork/Replication/NkNetWorld.h) : NKNetwork (couche
  System) ne connaît pas NKECS (couche Runtime) — la réplication fonctionne par
  **enregistrement d'objets** (`NkNetEntity` = NkNetId + prefabId + owner +
  autorité + callbacks `writeState`/`readState` sur NkBitStream), le pont ECS se
  fera dans Noge (même modèle que NKPhysics).
- Serveur : `Update(dt)` → snapshots **delta** à tickRate Hz (état re-sérialisé,
  comparé octet-à-octet à la baseline, seuls les changements partent) +
  **keyframes** périodiques (rattrapage pertes / late-joiners, `ForceKeyframe()`).
  Snapshot trop gros → scindé en messages auto-suffisants (même tick).
- Client : l'app draine `DrainAll` et route via `HandleMessage(msg)` (true =
  consommé) ; entité inconnue → callback `onEntitySpawn` (l'app crée + enregistre,
  l'état est appliqué dans la foulée) ; `DESPAWN` fiable → `onEntityDespawn`.
- Inputs : `SendInput()` (client, séquence croissante) → `DrainInputs(peer)`
  (serveur, déduplication par séquence).
- `NkNetInterpolator` : buffer d'états horodatés + `Sample(renderTime)` →
  paire (A, B) + alpha ; le lerp reste applicatif (sémantique d'état inconnue
  du module). `NkNetSystem` : hooks OnBeforeSnapshot/OnSnapshotApplied/…
- Protocole filaire : magic `0x52 'R'` (distinct transport 0x4E / lobby 0x4C),
  messages SNAPSHOT (canal SEQUENCED) / DESPAWN (RELIABLE_ORDERED) / INPUT.

### HTTPS / TLS (2026-07-12)
- `SendOverTLS` **implémenté** avec mbedTLS (Externals/Libs/NKMbedTLS) :
  RNG CTR-DRBG, connexion, SNI + vérif hostname, handshake, envoi/réception
  avec timeout, erreurs `mbedtls_strerror` lisibles.
- **Opt-in** : `NK_ENABLE_TLS=1 jenga build …` (défaut = HTTP seul, zéro dep).
- Vérification certificat : `Config::verifySSL` + `Config::caCertPath` (bundle
  PEM) → `VERIFY_REQUIRED` ; sans bundle → `VERIFY_NONE` (mbedTLS n'a pas accès
  aux CA système — fournir un bundle en production).
- Validé au runtime : `SandboxNKNetwork --https https://example.com/` → 200.

### Tests (2026-07-12)
- `Sandbox/System/NKNetwork` (cible `SandboxNKNetwork`) : **67 checks verts** —
  NkBitStream round-trip + overflow, NkNetId Pack/Unpack, NkNetInterpolator,
  réplication client/serveur sur messages forgés, **bout-en-bout loopback réel**
  (StartServer + Connect 127.0.0.1, handshake, snapshot → spawn → delta →
  despawn). Mode manuel `--https <url>` pour valider TLS.
  (`jenga test` restant bloqué par la policy workspace `disableunittestexecution`,
  le sandbox est le runner officiel du module.)

### Aliases pratiques
L'umbrella [NKNetwork.h](src/NKNetwork/NKNetwork.h) expose les types principaux
sous `nkentseu::Nk*` (sans le namespace `net::`).

---

## En cours / TODO immédiat

*(Rien de bloquant — l'include Replication est réparé, les tests existent, le
TLS est câblé. Prochaines briques par ordre d'utilité :)*

### Réplication V2
- Prédiction côté client + réconciliation serveur (rejouer les inputs > dernier
  tick acquitté) — les inputs séquencés et `NkNetInterpolator` posent la base.
- Relais des entités `NK_NET_AUTHORITY_CLIENT` (état du pawn propriétaire
  remonté puis rediffusé par le serveur après validation).
- Baseline delta PAR PAIR (aujourd'hui : baseline globale, un late-joiner
  attend la keyframe suivante — ≤ keyframeInterval ticks).

---

## À venir / À ajouter (futur proche)

### Pont ECS de la réplication (dans Noge, pas ici)
La couche Replication V1 est **livrée côté NKNetwork** (voir « Livré ») en mode
agnostique. Reste à construire, dans **Noge** (couche Engine, qui voit NKECS) :
- un composant `NetEntity` ECS + un système qui enregistre automatiquement les
  entités marquées dans `NkNetWorld` (writeState/readState générés — idéalement
  via NKReflection, catégories → NkBitStream) ;
- lag compensation (rewind/replay) au niveau gameplay.
Architecture cible inchangée : tick rate 20-60 Hz, client-side prediction,
server reconciliation (cf. « Réplication V2 » ci-dessus).

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
- ~~`NKNetwork.h` ne compile pas (include Replication manquant)~~ **CORRIGÉ
  2026-07-12** : `Replication/NkNetWorld.{h,cpp}` implémentés, umbrella compilé
  par le sandbox.
- ~~`NkLobbyMessageHeader::kSize = 12` alors que l'en-tête sérialisé fait 16
  octets~~ **CORRIGÉ 2026-07-12** : le payload écrasait `payloadSize` sur le
  fil (kSize → 16). NB : `Deserialize` n'est encore appelé nulle part (la
  réception lobby reste à câbler).
- La réception des messages lobby (dispatch `NkLobbyMessageHeader::Deserialize`
  → callbacks) n'est pas branchée côté client.
- HTTPS sans `Config::caCertPath` = pas de vérification du certificat
  (VERIFY_NONE) — fournir un bundle CA en production.
- État répliqué limité à `kNkNetMaxEntityStateSize` (256 octets) par entité ;
  entité au-delà = ignorée silencieusement du snapshot (à logger ?).

---

## Dépendances
- **Couches en dessous (utilisées)** : NKPlatform (détection OS, Winsock vs
  POSIX), NKCore (Types, Atomic), NKContainers (NkString, NkVector, NkSpan,
  NkFunction), NKThreading (Mutex, Thread pour async + ConnectionManager),
  NKLogger (debug des connexions).
- **Modules au-dessus qui en dépendent** : NKScene (NetEntity components une
  fois implémentés), Runtime (NetWorld dans la game loop), Noge (HTTP pour
  marketplace assets, télémétrie, plugins remote), PV3DE (HTTPClient pour API
  médicale FHIR — cf. ARCHITECTURE.md §5.10 export rapport FHIR/PDF).
