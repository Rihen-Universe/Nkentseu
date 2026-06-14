# NKNetwork

> Couche **System** · La pile réseau du moteur : sockets cross-platform, UDP fiable,
> sérialisation bit-packée, connexions, RPC, lobby/matchmaking et client HTTP.

Dès qu'un jeu ou une application doit **parler à une autre machine** — synchroniser
deux joueurs, appeler une fonction sur le serveur, lister les parties d'un lobby,
interroger une API REST — il passe par NKNetwork. Le module est bâti en strates : tout
en bas un **socket** brut (UDP/TCP) portable Windows/POSIX/Emscripten, au-dessus une
couche **UDP fiable** (ACK, retransmission, ordre, fragmentation), puis un **protocole**
bit-packé et des **connexions** à machine à états, et enfin des briques **haut niveau**
(RPC, lobby, HTTP). Chaque strate s'utilise seule : on peut faire du socket pur, ou tout
déléguer au `NkConnectionManager`.

Le module est entièrement **`noexcept`** (pas d'exceptions ; erreurs via `NkNetResult`
ou valeurs sentinelles), **`[[nodiscard]]`** quasi partout, et **zéro allocation heap**
dans le chemin critique (paquets et fenêtres sont des buffers inline). Les enumerators
réels sont préfixés `NK_NET_*` / `NK_HTTP_*` / `NK_SESSION_*` (les formes courtes vues
dans les exemples en commentaire des headers n'existent pas).

- **Namespace** : `nkentseu::net`
- **Header parapluie** : `#include "NKNetwork/NKNetwork.h"` — ⚠️ **CASSÉ** (inclut
  `Replication/NkNetWorld.h`, inexistant). En pratique, **inclure directement les
  sous-headers réels** listés plus bas.

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Ouvrir un socket UDP/TCP, envoyer/recevoir des datagrammes bruts | [Transport](NKNetwork/Transport.md) |
| Constantes, codes de résultat, identifiants de pair/entité, byte-order | [Transport](NKNetwork/Transport.md) |
| Obtenir un canal UDP **fiable** (ACK, retransmit, ordre, fragmentation) | [Transport](NKNetwork/Transport.md) |
| Sérialiser/désérialiser un message compact bit-à-bit (quantification) | [Protocole](NKNetwork/Protocol.md) |
| Gérer une connexion P2P à machine à états (SYN/ACK/FIN, heartbeats) | [Protocole](NKNetwork/Protocol.md) |
| Orchestrer N connexions serveur/client avec thread réseau dédié | [Protocole](NKNetwork/Protocol.md) |
| Appeler une fonction distante (RPC client→serveur / serveur→clients) | [Haut niveau](NKNetwork/HighLevel.md) |
| Créer/rejoindre une partie, lobby, chat, matchmaking, découverte LAN | [Haut niveau](NKNetwork/HighLevel.md) |
| Interroger une API REST / télécharger un fichier (HTTP/HTTPS) | [Haut niveau](NKNetwork/HighLevel.md) |

Chaque page détaille l'API **réelle** des headers (déclarations effectives), avec ses
idiomes et ses pièges. Les blocs d'exemples en commentaire des headers ne reflètent pas
toujours l'API exacte ; les pages s'en tiennent aux déclarations.

---

## Aperçu des familles

- **Transport** (`Core/NkNetDefines.h`, `Transport/NkSocket.h`,
  `Transport/NkReliableUDP.h`) — la fondation. Définitions globales (constantes
  `kNk*`, `NkNetResult`, `NkPeerId`, `NkNetId`, canaux `NkNetChannel`, byte-order),
  socket RAII UDP/TCP cross-platform (`NkSocket`, `NkAddress`, `NkPacket`), et la
  couche UDP fiable (`NkReliableUDP`, `NkReliableChannel`, `NkRTTEstimator`) :
  fenêtre glissante, ACK, retransmission, fragmentation, estimation RTT/jitter.
- **Protocole** (`Protocol/NkBitStream.h`, `Protocol/NkConnection.h`) — la
  sérialisation et la connexion. `NkBitWriter`/`NkBitReader` encodent/décodent
  bit-à-bit avec quantification de flottants, vecteurs et quaternions (« smallest
  three »). `NkConnection` est une connexion P2P à machine à états (handshake,
  heartbeats, file de réception, stats), et `NkConnectionManager` orchestre jusqu'à
  256 connexions sur un thread I/O dédié.
- **Haut niveau** (`RPC/NkRPC.h`, `Lobby/NkLobby.h`, `HTTP/NkHTTPClient.h`) — les
  services gameplay. `NkRPCRouter` enregistre et route des appels de procédure
  distante (jusqu'à 256 RPC). `NkSession`/`NkLobby`/`NkMatchmaker`/`NkDiscovery`
  couvrent partie, lobby+chat, matchmaking asynchrone et découverte LAN.
  `NkHTTPClient`/`NkLeaderboard` fournissent un client HTTP/HTTPS sync+async et un
  client REST de classement.

---

## Index des headers réels

> Le parapluie `NKNetwork.h` est **cassé** (référence un header de réplication
> inexistant) — inclure les sous-headers ci-dessous.

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKNetwork.h` | Parapluie — ⚠️ **cassé** (inclut `Replication/NkNetWorld.h` inexistant). | — |
| `NkNetworkApi.h` | Macros d'export (`NKENTSEU_NETWORK_API`, `NKENTSEU_NETWORK_CLASS_EXPORT`). | — |
| `Core/NkNetDefines.h` | Constantes `kNk*`, `NkNetResult`, `NkPeerId`, `NkNetId`, `NkNetChannel`, config, byte-order, horloge. | [Transport](NKNetwork/Transport.md) |
| `Transport/NkSocket.h` | `NkAddress`, `NkPacket`, `NkSocket` (UDP/TCP RAII, `PlatformInit`, `Select`). | [Transport](NKNetwork/Transport.md) |
| `Transport/NkReliableUDP.h` | `NkReliableUDP`, `NkReliableChannel`, `NkRTTEstimator`, `NkRUDPHeader`, flags. | [Transport](NKNetwork/Transport.md) |
| `Protocol/NkBitStream.h` | `NkBitWriter`, `NkBitReader` (encodage bit-packé + quantifié). | [Protocole](NKNetwork/Protocol.md) |
| `Protocol/NkConnection.h` | `NkConnection`, `NkConnectionManager`, états, stats, `NkReceiveMsg`. | [Protocole](NKNetwork/Protocol.md) |
| `RPC/NkRPC.h` | `NkRPCRouter`, `NkRPCDescriptor`, types/fiabilité RPC, macros `NK_RPC_*`. | [Haut niveau](NKNetwork/HighLevel.md) |
| `Lobby/NkLobby.h` | `NkSession`, `NkLobby`, `NkMatchmaker`, `NkDiscovery`, configs/infos. | [Haut niveau](NKNetwork/HighLevel.md) |
| `HTTP/NkHTTPClient.h` | `NkHTTPClient`, `NkHTTPRequest`/`Response`, `NkLeaderboard`. | [Haut niveau](NKNetwork/HighLevel.md) |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
