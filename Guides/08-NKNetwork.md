# Guide 8 — NKNetwork : réseau, messages et multijoueur

> Module **System** du moteur. Sockets cross-platform (UDP/TCP), sérialisation
> binaire bit-à-bit, gestion de connexions fiables (handshake + heartbeat) et
> briques de lobby/découverte LAN.
> Retour au sommaire : [README.md](README.md).

Pré-requis recommandés : [guide 1 — NKMemory](01-NKMemory.md) (la règle d'or
mémoire vaut aussi ici) et une notion de boucle principale ([guide 2 — NKWindow](02-NKWindow.md)).
Ce guide est **autonome** : aucun rendu n'est nécessaire.

---

## 1. À quoi sert NKNetwork

NKNetwork est la couche réseau de Nkentseu. Elle te donne, du plus bas au plus
haut niveau :

- **Transport** — `NkSocket` : un socket UDP ou TCP unifié pour Windows
  (Winsock2), POSIX (Linux/macOS/Android/iOS) et WebAssembly. Plus `NkAddress`
  (IP + port, résolution DNS) et `NkPacket` (paquet brut MTU-safe).
- **Sérialisation** — `NkBitWriter` / `NkBitReader` : encodage **bit-à-bit** pour
  réduire la bande passante (un booléen = 1 bit, un float quantifié = ~16 bits au
  lieu de 32, etc.).
- **Protocole fiable** — `NkConnection` / `NkConnectionManager` : machine à états
  de connexion (handshake SYN/SYN-ACK/ACK, heartbeat, timeout) au-dessus d'UDP,
  avec 4 canaux de livraison (unreliable, reliable ordered/unordered, sequenced).
- **Découverte LAN** — `NkDiscovery` : broadcast/écoute UDP pour lister les
  serveurs d'un réseau local.

### TCP ou UDP ?

| | **TCP** | **UDP** |
|---|---|---|
| Livraison | garantie, ordonnée | best-effort (perte/désordre possibles) |
| Connexion | flux connecté (`Connect`/`Accept`) | datagrammes non-connectés (`SendTo`/`RecvFrom`) |
| Latence | algorithme de Nagle (désactivable via `SetNoDelay`) | minimale |
| Usage typique | lobby, chat, transfert fiable | positions/inputs de jeu temps réel |

Pour un jeu temps réel, la pratique standard (suivie par NKNetwork) est **UDP +
une couche de fiabilité par canal** (`NkConnection`), plutôt que TCP brut.

---

## 2. Limites actuelles — à lire avant de coder

NKNetwork est un module **en construction**. Son architecture annonce cinq
couches, mais toutes ne sont pas présentes sur disque. État réel constaté
(cf. [ROADMAP.md](../Kernel/System/NKNetwork/ROADMAP.md)) :

**Ce qui est implémenté et utilisable :**

- `Core/NkNetDefines.h` — types, codes de retour, canaux, byte-order.
- `Transport/NkSocket.h` — sockets UDP/TCP cross-platform. **C'est la brique la
  plus solide et la plus directement utilisable.**
- `Transport/NkReliableUDP.h` — couche RUDP (ACK, retransmit, séquençage).
- `Protocol/NkBitStream.h` — sérialisation bit-à-bit.
- `Protocol/NkConnection.h` — `NkConnection` + `NkConnectionManager`.
- `RPC/NkRPC.h`, `Lobby/NkLobby.h`, `HTTP/NkHTTPClient.h` — surfaces déclarées.

**⚠️ Ce qu'il faut éviter pour l'instant :**

1. **Ne pas inclure l'umbrella `NKNetwork/NKNetwork.h` tel quel.** Il inclut
   `Replication/NkNetWorld.h`, mais **le dossier `Replication/` n'existe pas**.
   J'ai vérifié : sous `src/NKNetwork/` on trouve `Core HTTP Lobby Protocol RPC
   Transport` — pas de `Replication`. L'umbrella **ne compile donc pas** en
   l'état, et les alias `nkentseu::NkNetWorld`, `NkNetEntity`, `NkNetSnapshot`,
   `NkNetInterpolator`, `NkNetSystem`, `NkNetInput`, `NkNetAuthority` qu'il
   expose pointent vers des types **inexistants**.

   → **Inclure directement les headers dont tu as besoin** (`Transport/NkSocket.h`,
   `Protocol/NkBitStream.h`, `Protocol/NkConnection.h`, …), jamais l'umbrella.

2. **Couche Replication ECS (NkNetWorld) = non disponible.** Toute la partie
   « réplique automatiquement les entités ECS / snapshots / prédiction client /
   réconciliation serveur » est **à venir** (statut TODO, effort XL). Les
   exemples « USAGE RAPIDE » du haut de `NKNetwork.h` qui appellent
   `netWorld.Init(...)` / `netWorld.Update(...)` **ne sont pas exécutables
   aujourd'hui**.

3. **Aucun test ni benchmark** n'accompagne le module. Considère les couches
   hautes (RPC, Lobby, Matchmaker, HTTP/TLS) comme **déclarées mais non
   validées** : utilise-les avec prudence et teste localement.

Ce guide se concentre donc sur **ce qui fonctionne réellement** : sockets,
sérialisation bit-stream, connexions fiables et découverte LAN.

---

## 3. Initialiser le sous-système réseau

Avant tout socket, appelle **une fois** `NkSocket::PlatformInit()` (qui fait
`WSAStartup` sous Windows ; no-op ailleurs), et `PlatformShutdown()` à l'arrêt.

Tous les codes de retour sont des `NkNetResult` (un `enum class`). Le succès est
`NkNetResult::NK_NET_OK`. La fonction `NkNetResultStr(r)` donne une chaîne
lisible pour le log.

```cpp
#include "NKNetwork/Transport/NkSocket.h"
#include "NKLogger/NkLog.h"            // logger.Info / .Error

using namespace nkentseu;
using namespace nkentseu::net;

bool InitNetwork() {
    if (NkSocket::PlatformInit() != NkNetResult::NK_NET_OK) {
        logger.Error("[Net] Echec PlatformInit");
        return false;
    }
    return true;
}

void ShutdownNetwork() {
    NkSocket::PlatformShutdown();
}
```

> Convention de log du module : les macros internes (`NK_NET_LOG_INFO`, etc.)
> interpolent les placeholders `{0} {1} …` de libfmt. Dans ton code applicatif,
> tu peux utiliser directement `logger.Info("...", arg)`.

---

## 4. Ouvrir une socket (le transport)

### 4.1 Les types clés

- `NkAddress` — une adresse réseau (IP + port). Construction :
  - `NkAddress("192.168.1.10", 7777)` (chaîne hôte, **résolution DNS bloquante**
    si ce n'est pas une IP) ;
  - `NkAddress(192, 168, 1, 10, 7777)` (octets IPv4, pas de DNS) ;
  - fabriques : `NkAddress::Loopback(port)` (127.0.0.1), `NkAddress::Any(port)`
    (0.0.0.0, écoute toutes interfaces — `port = 0` = auto-assigné),
    `NkAddress::Broadcast(port)` (255.255.255.255) ;
  - `NkAddress::Resolve("host", port)` → `NkVector<NkAddress>` (**bloquant**, à
    faire hors game loop).
- `NkSocket` — RAII (le destructeur appelle `Close()`), **non copiable** mais
  **déplaçable** (`NkSocket&&`). Type : `NkSocket::Type::NK_UDP` ou `NK_TCP`.

### 4.2 Créer un socket UDP

`Create(localAddr, type)` crée **et lie** le socket. Pense à `SetNonBlocking(true)`
pour une boucle de jeu (en non-bloquant, `RecvFrom` retourne `NK_NET_OK` avec
`outSize == 0` quand il n'y a rien à lire).

```cpp
#include "NKNetwork/Transport/NkSocket.h"
using namespace nkentseu;
using namespace nkentseu::net;

NkSocket sock;
// Bind sur le port 7777, toutes interfaces.
if (sock.Create(NkAddress::Any(7777), NkSocket::Type::NK_UDP) != NkNetResult::NK_NET_OK) {
    logger.Error("[Net] Create UDP a echoue: {0}", NkNetResultStr(sock.GetLastError()));
    return;
}
(void)sock.SetNonBlocking(true);
(void)sock.SetRecvBufferSize(1024 * 1024);   // absorber les bursts (optionnel)
(void)sock.SetSendBufferSize(1024 * 1024);
```

Options disponibles (toutes renvoient un `NkNetResult`) :
`SetNonBlocking`, `SetBroadcast` (UDP IPv4, pour la découverte LAN),
`SetNoDelay` (TCP, désactive Nagle), `SetSendBufferSize` / `SetRecvBufferSize`,
`SetReuseAddr` (redémarrage rapide d'un serveur).

### 4.3 TCP : écouter, se connecter, accepter

```cpp
// --- Serveur TCP ---
NkSocket server;
server.Create(NkAddress::Any(8080), NkSocket::Type::NK_TCP);
(void)server.SetReuseAddr(true);
(void)server.SetNonBlocking(true);
(void)server.Listen(32);            // backlog

// Dans la boucle : accepter les nouveaux clients.
NkSocket client;
NkAddress clientAddr;
if (server.Accept(client, clientAddr) == NkNetResult::NK_NET_OK && client.IsValid()) {
    (void)client.SetNoDelay(true);  // latence minimale
    logger.Info("[Net] Client TCP: {0}", clientAddr.ToString().CStr());
}

// --- Client TCP ---
NkSocket toServer;
toServer.Create(NkAddress::Any(0), NkSocket::Type::NK_TCP);
(void)toServer.SetNonBlocking(true);
(void)toServer.Connect(NkAddress("127.0.0.1", 8080)); // non-bloquant : connexion "en cours"
```

> En mode non-bloquant, `Connect`/`Accept` n'attendent pas. `Accept` peut
> retourner `NK_NET_OK` avec un `client` invalide quand il n'y a personne en
> attente — teste toujours `client.IsValid()`.

### 4.4 Surveiller plusieurs sockets : `Select`

`NkSocket::Select(readSockets, timeoutMs, outReady)` est un multiplexeur
portable (alternative légère à epoll/IOCP). Il remplit `outReady` avec les
**indices** des sockets prêts en lecture.

```cpp
NkVector<NkSocket*> watch = { &sock1, &sock2 };
NkVector<uint32>    ready;
if (NkSocket::Select(NkSpan<NkSocket*>(watch.Data(), watch.Size()), 16, ready)
        == NkNetResult::NK_NET_OK) {
    for (uint32 i : ready) { /* watch[i] a des données à lire */ }
}
```

---

## 5. Envoyer et recevoir des données

### 5.1 UDP brut : `SendTo` / `RecvFrom`

```cpp
// Envoi : destination = NkAddress du pair.
const char* msg = "PING";
(void)sock.SendTo(msg, 4, NkAddress("127.0.0.1", 7777));

// Réception (non-bloquant) :
uint8     buf[kNkMaxPacketSize];   // 1400 octets, MTU-safe (NkNetDefines.h)
uint32    received = 0;
NkAddress from;
if (sock.RecvFrom(buf, sizeof(buf), received, from) == NkNetResult::NK_NET_OK
        && received > 0) {
    logger.Info("[Net] {0} octets de {1}", received, from.ToString().CStr());
}
```

Pour TCP, c'est `Send(data, size)` / `Recv(buf, bufSize, outSize)` — attention,
TCP est un **flux** : un `Send` peut être livré en plusieurs `Recv` (et vice
versa). À toi de délimiter tes messages (préfixe de longueur, par exemple).

### 5.2 Sérialiser proprement avec NkBitStream

Envoyer une struct C brute par le réseau est fragile (endianness, padding,
versions). NKNetwork fournit un sérialiseur **bit-à-bit** : `NkBitWriter` écrit
dans un buffer **que tu fournis** (pas d'allocation interne), `NkBitReader` relit
**dans le même ordre**.

Méthodes principales du **writer** :
`WriteBool` (1 bit), `WriteU8/U16/U32/U64`, `WriteI8/I16/I32`, `WriteF32`
(32 bits exacts), `WriteF32Q(v, min, max, prec)` (float quantifié — économie),
`WriteInt(v, min, max)` (entier borné sur le minimum de bits),
`WriteVec3f` / `WriteVec3fQ`, `WriteQuatf` (quaternion compressé « smallest 3 »
→ 32 bits), `WriteString(s, maxLen)`, `WriteBytes`. Et côté **reader**, les
`ReadXxx` symétriques.

Contrôle d'intégrité : après écriture vérifie `IsOverflowed()` (buffer trop
petit) ; après lecture vérifie aussi `IsOverflowed()` (paquet tronqué/corrompu).
La taille à envoyer est `writer.BytesWritten()`.

```cpp
#include "NKNetwork/Protocol/NkBitStream.h"
using namespace nkentseu;
using namespace nkentseu::net;

struct PlayerMove {
    uint32        playerId;
    bool          jumping;
    math::NkVec3f position;     // monde borné [-500, 500] m
    float32       yaw;          // [0, 360[ degrés
};

// --- Émission ---
uint8 buf[256];
NkBitWriter w(buf, sizeof(buf));
w.WriteU32(move.playerId);
w.WriteBool(move.jumping);
w.WriteVec3fQ(move.position, -500.f, 500.f, 0.01f);   // ~16 bits / axe (préc. 1 cm)
w.WriteF32Q(move.yaw, 0.f, 360.f, 0.1f);              // ~12 bits (préc. 0,1°)
if (!w.IsOverflowed()) {
    (void)sock.SendTo(buf, w.BytesWritten(), serverAddr);
}

// --- Réception (mêmes paramètres min/max/prec, MÊME ORDRE) ---
NkBitReader r(buf, received);
PlayerMove m{};
m.playerId = r.ReadU32();
m.jumping  = r.ReadBool();
m.position = r.ReadVec3fQ(-500.f, 500.f, 0.01f);
m.yaw      = r.ReadF32Q(0.f, 360.f, 0.1f);
if (r.IsOverflowed()) {
    logger.Warn("[Net] paquet PlayerMove corrompu/tronque, ignore");
    return;
}
```

> **Règle d'or bit-stream** : l'ordre des `Read*` doit reproduire exactement
> l'ordre des `Write*`, et les paramètres `min/max/prec` des versions quantifiées
> doivent être **identiques** des deux côtés (mieux : partage-les via des
> constantes dans un header commun). Sinon tout le reste du paquet se décale.

Pour la sérialisation côté CPU/fichier (JSON, binaire générique, etc.) voir le
module **NKSerialization** — distinct de NkBitStream, qui est spécialisé pour le
fil réseau compact.

---

## 6. Connexions fiables : `NkConnection` / `NkConnectionManager`

`NkSocket` est du transport brut. Pour un vrai client/serveur (poignée de main,
détection de déconnexion, canaux de fiabilité, routage par `NkPeerId`), la couche
**Protocole** ajoute `NkConnectionManager` au-dessus d'un socket UDP partagé.

### 6.1 Concepts

- **`NkPeerId`** — identifiant opaque d'un pair (`uint64 value`). Convention :
  `value == 1` = serveur (`NkPeerId::Server()`), `value == 0` = invalide
  (`NkPeerId::Invalid()`).
- **Canaux** (`NkNetChannel`) — choisis selon le compromis latence/fiabilité :

  | Canal | Garantie | Usage |
  |---|---|---|
  | `NK_NET_CHANNEL_UNRELIABLE` | aucune | positions, inputs (éphémères) |
  | `NK_NET_CHANNEL_RELIABLE_ORDERED` | livraison + ordre | chat, événements critiques |
  | `NK_NET_CHANNEL_RELIABLE_UNORDERED` | livraison, sans ordre | chunks indépendants |
  | `NK_NET_CHANNEL_SEQUENCED` | seul le plus récent | health/mana (états continus) |
  | `NK_NET_CHANNEL_SYSTEM` | — | **réservé moteur**, ne pas utiliser |

- **`NkReceiveMsg`** — message reçu prêt à traiter : `data[]` + `size` + `from`
  (`NkPeerId`) + `channel` + `receivedAt`.

`NkConnectionManager` lance **son propre thread réseau** (polling + dispatch +
update des connexions). Les callbacks `onPeerConnected` / `onPeerDisconnected`
sont invoqués **depuis ce thread** : si tu touches à l'état du jeu, diffère vers
ton thread principal. Les méthodes `SendTo` / `Broadcast` / `DrainAll` sont
thread-safe.

### 6.2 Squelette serveur

```cpp
#include "NKNetwork/Protocol/NkConnection.h"
using namespace nkentseu;
using namespace nkentseu::net;

NkConnectionManager server;

server.onPeerConnected = [](NkPeerId p) {
    logger.Info("[Net] joueur connecte: {0}", p.value);
};
server.onPeerDisconnected = [](NkPeerId p, const char* reason) {
    logger.Info("[Net] joueur parti: {0} ({1})", p.value, reason ? reason : "?");
};

if (server.StartServer(7777, /*maxClients*/ 64) != NkNetResult::NK_NET_OK) {
    logger.Error("[Net] StartServer a echoue");
    return;
}

// --- Boucle jeu (thread principal) ---
while (running) {
    // 1) Récupérer les messages arrivés depuis la dernière frame.
    NkVector<NkReceiveMsg> msgs;
    server.DrainAll(msgs);
    for (const NkReceiveMsg& m : msgs) {
        // m.from, m.data, m.size, m.channel — décode avec NkBitReader.
    }

    // 2) Diffuser l'état du jeu (positions = unreliable).
    uint8 buf[1024];
    NkBitWriter w(buf, sizeof(buf));
    /* w.WriteXxx(...) ; */
    if (!w.IsOverflowed()) {
        (void)server.Broadcast(buf, w.BytesWritten(),
                               NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
    }
}

server.DisconnectAll("Server shutting down");
server.Shutdown();
```

`Broadcast(data, size, channel, exclude)` envoie à tous (avec un `exclude`
optionnel) ; `SendTo(peer, data, size, channel)` cible un pair précis.

### 6.3 Squelette client

```cpp
NkConnectionManager client;
bool connected = false;

client.onPeerConnected    = [&](NkPeerId)              { connected = true;  };
client.onPeerDisconnected = [&](NkPeerId, const char*) { connected = false; };

if (client.Connect(NkAddress("127.0.0.1", 7777)) != NkNetResult::NK_NET_OK) {
    logger.Error("[Net] Connect a echoue");
    return;
}

while (running) {
    if (connected) {
        // Envoyer les inputs (unreliable).
        uint8 buf[64];
        NkBitWriter w(buf, sizeof(buf));
        /* w.WriteXxx(input...) ; */
        if (!w.IsOverflowed())
            (void)client.SendTo(NkPeerId::Server(), buf, w.BytesWritten(),
                                NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
    }
    // Recevoir les mises à jour serveur.
    NkVector<NkReceiveMsg> msgs;
    client.DrainAll(msgs);
    for (const NkReceiveMsg& m : msgs) { /* décode avec NkBitReader */ }
}

client.Shutdown();
```

### 6.4 Statistiques de connexion

Pour afficher un ping / un taux de perte (UI, overlay debug), copie les stats de
manière **thread-safe** plutôt que de garder le pointeur de connexion (qui peut
être invalidé par le thread réseau) :

```cpp
NkConnectionStats stats;
if (server.GetConnectionStats(peerId, stats)) {
    logger.Info("[Net] RTT={0}ms perte={1}%", (uint32)stats.rttMs,
                stats.packetLoss * 100.f);
}
```

> Honnêteté : cette couche `NkConnection` est **livrée mais non couverte par des
> tests** dans le dépôt. Le squelette ci-dessus suit fidèlement l'API publique ;
> valide-le sur ta machine (loopback `127.0.0.1`) avant d'en dépendre.

---

## 7. Découverte de serveurs sur le réseau local (LAN)

`NkDiscovery` (dans `Lobby/NkLobby.h`) fournit deux fonctions **statiques** :
le serveur diffuse périodiquement ses infos en UDP broadcast, le client écoute
pendant un délai et récupère la liste.

```cpp
#include "NKNetwork/Lobby/NkLobby.h"
using namespace nkentseu;
using namespace nkentseu::net;

// --- Côté serveur : annoncer sa présence (à répéter ~ toutes les 2 s) ---
NkDiscovery::ServerInfo info{};
std::strcpy(info.name, "Ma partie");
std::strcpy(info.gameMode, "Deathmatch");
info.playerCount = 3;
info.maxPlayers  = 8;
info.addr        = NkAddress::Any(7777);   // port de jeu réel
(void)NkDiscovery::Broadcast(info /*, broadcastPort = 7778 */);

// --- Côté client : scanner le LAN pendant 2 secondes (BLOQUANT) ---
NkVector<NkDiscovery::ServerInfo> servers;
if (NkDiscovery::Listen(2000, servers) == NkNetResult::NK_NET_OK) {
    for (const auto& s : servers) {
        logger.Info("[LAN] {0} {1}/{2} ping {3}ms @ {4}",
            s.name, s.playerCount, s.maxPlayers, s.ping, s.addr.ToString().CStr());
        // Ensuite : client.Connect(s.addr) pour rejoindre.
    }
}
```

`ServerInfo` contient `addr`, `name[128]`, `gameMode[64]`, `playerCount`,
`maxPlayers`, `ping`, `hasPassword`. `Listen` est **bloquant** (durée
`timeoutMs`) et déduplique par adresse — appelle-le hors de la game loop (écran
de browser de serveurs, thread dédié).

> Le reste de `Lobby/NkLobby.h` (`NkSession`, `NkLobby`, `NkMatchmaker`) et les
> modules `RPC/NkRPC.h` et `HTTP/NkHTTPClient.h` sont **déclarés** mais non
> validés (pas de tests, TLS HTTP non câblé d'après la ROADMAP). À traiter comme
> « à venir » tant que tu ne les as pas éprouvés toi-même.

---

## 8. Bonnes pratiques

- **Initialise/nettoie une seule fois** : `NkSocket::PlatformInit()` au démarrage,
  `PlatformShutdown()` à l'arrêt.
- **Toujours non-bloquant** pour une boucle interactive (`SetNonBlocking(true)`).
  En non-bloquant, « rien à lire » = `NK_NET_OK` avec `size == 0`, pas une erreur.
- **Vérifie chaque `NkNetResult`** et logge avec `NkNetResultStr(r)`. Pour le code
  système brut d'un socket, `GetLastError()` aide au diagnostic.
- **Jamais de DNS dans la game loop** : `NkAddress::Resolve` / `NkAddress("host",…)`
  avec un nom DNS est **bloquant** → fais-le au chargement ou dans un thread.
- **Sérialise avec NkBitStream**, pas avec un `memcpy` de struct : tu gagnes en
  bande passante et tu évites les surprises d'endianness/padding. Garde
  `Write*`/`Read*` strictement symétriques et vérifie `IsOverflowed()`.
- **Threading** : `NkConnectionManager` est thread-safe pour `SendTo`/`Broadcast`/
  `DrainAll` ; ses callbacks tournent sur le thread réseau → diffère le travail
  gameplay. Un `NkSocket` seul n'est **pas** thread-safe.
- **Mémoire (règle d'or du moteur)** : n'alloue/libère que via NKMemory. Les
  buffers de `NkBitWriter`/`NkBitReader` et `NkPacket` sont **inline / fournis par
  toi** (pas d'allocation cachée) — pas de piège ici, mais ne mélange jamais
  allocateur custom et `free`/`delete` CRT ([guide 1](01-NKMemory.md)).
- **Ne compte pas sur l'umbrella** `NKNetwork/NKNetwork.h` (include Replication
  cassé) : inclus les headers précis. Et **n'utilise pas** `NkNetWorld` &
  consorts : ils n'existent pas encore.

---

## 9. Récapitulatif

- Le **transport** (`NkSocket`, `NkAddress`, UDP/TCP, `Select`) est la partie la
  plus mûre et directement utilisable, sur Windows/POSIX/Web.
- La **sérialisation bit-à-bit** (`NkBitWriter`/`NkBitReader`) est livrée et te
  permet des paquets compacts ; respecte la symétrie écriture/lecture.
- Les **connexions fiables** (`NkConnectionManager`) offrent handshake, heartbeat,
  canaux et routage par `NkPeerId` — API complète mais **non testée** dans le
  dépôt : valide en loopback.
- La **découverte LAN** (`NkDiscovery::Broadcast`/`Listen`) est utilisable pour un
  browser de serveurs local.
- La **réplication ECS** (`NkNetWorld`, snapshots, prédiction/réconciliation) et
  l'umbrella complet sont **non disponibles** (include cassé, TODO). RPC/Lobby/
  Matchmaker/HTTP sont déclarés mais non validés.

### Dépendances Jenga

Déclare le module `NKNetwork` dans `nkentseudependson`. Jenga propage
transitivement les includes des couches dont NKNetwork dépend (NKCore,
NKPlatform, NKMemory, NKContainers, NKLogger, NKThreading, NKMath).

```python
# MonJeuReseau.jenga (extrait)
with project("MonJeuReseau"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKNetwork",                                  # le module réseau
         "NKMemory", "NKCore", "NKContainers",
         "NKLogger", "NKThreading", "NKMath"],         # déjà tirés transitivement,
        extra_includes=["src"],                        # mais explicites = plus clair
    )
```

Liens système : sous Windows, NKNetwork tire `ws2_32.lib` lui-même
(`#pragma comment(lib, "ws2_32.lib")`) ; sous Linux pense à `pthread` (déjà géré
par le module) ; pas de lib réseau spéciale sur macOS/Android.

```
jenga build --target MonJeuReseau
jenga build --target MonJeuReseau --config Release
```

---

### Niveau de maturité réel (constaté à la lecture du code, juin 2026)

| Brique | Maturité | Remarque |
|---|---|---|
| `NkSocket` / `NkAddress` / `Select` | **Utilisable** | la plus solide, cross-platform |
| `NkBitWriter` / `NkBitReader` | **Utilisable** | sérialisation compacte livrée |
| `NkReliableUDP` (RUDP) | Livré | sous `NkConnection`, non testé isolément |
| `NkConnection` / `NkConnectionManager` | Livré, **non testé** | API complète, valider en loopback |
| `NkDiscovery` (LAN) | Livré, non testé | broadcast/listen statiques |
| RPC / Lobby / Matchmaker / HTTP | **Déclaré, non validé** | TLS non câblé ; prudence |
| Replication ECS (`NkNetWorld`…) | **Indisponible** | dossier absent, umbrella cassé |

**Conclusion** : NKNetwork est un module **bas-niveau exploitable** (sockets +
sérialisation, et une couche connexion à éprouver), mais **pas encore un
framework multijoueur clé-en-main**. Pour un jeu, le chemin réaliste aujourd'hui
est : `NkSocket` (UDP) + `NkBitStream`, éventuellement `NkConnectionManager` pour
la fiabilité — en évitant l'umbrella et la couche Replication.

> Suite du parcours : [guide 9 — Projet 2D complet](09-Projet-2D-complet.md).
> Retour au sommaire : [README.md](README.md).
