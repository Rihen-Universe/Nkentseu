# NKNetwork — documentation détaillée

Le module **NKNetwork**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKNetwork.md](../NKNetwork.md).

Chaque page détaille l'API **réelle** des headers (déclarations effectives), avec ses
idiomes et ses pièges concrets (init/shutdown plateforme, `noexcept`, `[[nodiscard]]`,
zéro-heap, thread-safety, symétrie écriture/lecture, callbacks sur thread réseau…).

> Le header parapluie `NKNetwork.h` est **cassé** (inclut `Replication/NkNetWorld.h`,
> inexistant) — inclure directement les sous-headers indiqués ci-dessous.

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Transport.md](Transport.md) | Définitions globales (constantes, `NkNetResult`, `NkPeerId`/`NkNetId`, canaux, byte-order), socket RAII UDP/TCP cross-platform, et la couche UDP fiable (ACK, retransmit, ordre, fragmentation, RTT/jitter). | `Core/NkNetDefines.h`, `Transport/NkSocket.h`, `Transport/NkReliableUDP.h` |
| [Protocol.md](Protocol.md) | Sérialisation bit-packée (`NkBitWriter`/`NkBitReader` : primitifs, quantification, vecteurs/quaternions) et gestion de connexion P2P à machine à états (`NkConnection`, `NkConnectionManager`). | `Protocol/NkBitStream.h`, `Protocol/NkConnection.h` |
| [HighLevel.md](HighLevel.md) | Appels de procédure distante (`NkRPCRouter`), lobby/matchmaking/découverte LAN (`NkSession`, `NkLobby`, `NkMatchmaker`, `NkDiscovery`) et client HTTP/HTTPS + leaderboard REST (`NkHTTPClient`, `NkLeaderboard`). | `RPC/NkRPC.h`, `Lobby/NkLobby.h`, `HTTP/NkHTTPClient.h` |

[← Récap NKNetwork](../NKNetwork.md) · [← Couche System](../README.md)
