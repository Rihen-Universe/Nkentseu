# La couche transport

> Couche **System** · NKNetwork · Faire transiter des octets sur le réseau : l'adresse `NkAddress`,
> le socket brut UDP/TCP `NkSocket`, et la fiabilité bâtie par-dessus l'UDP avec `NkReliableUDP`.

Dès qu'un jeu cesse d'être solo, il faut **envoyer des octets d'une machine à l'autre** — la
position d'un joueur, un tir, l'état d'une partie. Le système d'exploitation offre pour cela une
seule primitive vraiment utile : le **socket**. Mais un socket brut est *hostile* au temps réel. En
TCP, il retransmet tout, dans l'ordre, quitte à bloquer toute la partie sur un paquet perdu ; en
UDP, il est rapide mais ne garantit **rien** — paquets perdus, dupliqués, arrivés dans le désordre.
La couche Transport de NKNetwork répond exactement à ce dilemme : un **wrapper de socket** propre et
cross-platform (`NkSocket`), une **adresse** typée (`NkAddress`), et surtout une couche de
**fiabilité sélective sur UDP** (`NkReliableUDP`) qui vous laisse choisir, *canal par canal*, entre
la vitesse de l'UDP et les garanties du TCP.

Tout ce code partage trois principes qu'il faut avoir en tête avant de lire la suite. D'abord, **rien
ne lance d'exception** : chaque appel est `noexcept` et signale ses erreurs par un code de retour
`NkNetResult` ou une valeur sentinelle. Ensuite, **rien n'alloue sur le tas dans le chemin critique** :
paquets et fenêtres sont des buffers fixes posés sur la pile. Enfin, **rien n'est thread-safe par
défaut** (sauf la config et l'horloge) : un socket ou une connexion partagés entre threads doivent
être protégés par un `NkMutex`.

- **Namespace** : `nkentseu::net` (les constantes `kNk*`, le type `NkSocketHandle` et les macros sont au scope global)
- **Headers** : `#include "NKNetwork/Core/NkNetDefines.h"`, `#include "NKNetwork/Transport/NkSocket.h"`, `#include "NKNetwork/Transport/NkReliableUDP.h"`

---

## Désigner un correspondant : `NkAddress`

Avant d'envoyer quoi que ce soit, il faut savoir **à qui**. `NkAddress` est l'identité réseau d'une
machine : une IP (IPv4 complète, IPv6 partiel) plus un port. On la construit de plusieurs façons —
par chaîne (`"192.168.1.10"`), par octets (`192, 168, 1, 10`), ou par les fabriques statiques qui
expriment les cas usuels : `Loopback(port)` pour parler à soi-même, `Any(port)` pour écouter sur
toutes les interfaces (le `0.0.0.0` du serveur), `Broadcast(port)` pour la diffusion locale.

```cpp
NkAddress server{ "127.0.0.1", 7777 };          // par chaîne
NkAddress listen = NkAddress::Any(7777);         // côté serveur : toutes les interfaces
NkAddress me     = NkAddress::Loopback(0);       // port 0 = l'OS en choisit un libre
```

Le piège classique de toute couche réseau est la **résolution de nom (DNS)**. Donner un nom d'hôte
(`"jeu.exemple.com"`) au constructeur, ou appeler `Resolve(host, port)`, déclenche une requête DNS
**bloquante** : le thread s'arrête tant que la réponse n'arrive pas — parfois plusieurs secondes.
Ce n'est **pas** quelque chose à faire dans la boucle de jeu ; on résout au chargement, ou sur un
thread dédié. Un dernier détail défensif : à cause d'une initialisation partielle des membres, ne
vous fiez **jamais** au fait qu'une `NkAddress` par défaut soit « zéro » — interrogez toujours
`IsValid()` avant de l'utiliser.

> **En résumé.** `NkAddress` = IP + port, construite par chaîne / octets / fabriques
> (`Loopback`/`Any`/`Broadcast`). Le DNS (`Resolve`, ctor par nom) est **bloquant** — jamais en game
> loop. Vérifiez toujours `IsValid()` plutôt que de supposer le zéro.

---

## Le tuyau brut : `NkSocket`

`NkSocket` enveloppe le socket de l'OS dans un objet **RAII** : on le crée, on s'en sert, et son
destructeur le ferme tout seul. Il gère les deux familles — UDP (`Type::NK_UDP`, datagrammes,
sans connexion, rapide) et TCP (`Type::NK_TCP`, flux fiable et ordonné, connecté). Comme une
ressource système ne se duplique pas, le socket est **move-only** : on peut le déplacer (transférer
sa propriété), jamais le copier.

La toute première chose à faire dans un programme réseau, **une seule fois**, est d'appeler
`NkSocket::PlatformInit()` — c'est lui qui démarre la pile réseau (le `WSAStartup` de Windows ; un
no-op ailleurs) — et `PlatformShutdown()` à la fin. Ensuite, le cycle de vie d'un socket est limpide :

```cpp
NkSocket::PlatformInit();                                   // une fois, au démarrage

NkSocket sock;
if (sock.Create(NkAddress::Any(7777), NkSocket::Type::NK_UDP) != NK_NET_OK) { /* échec bind */ }
sock.SetNonBlocking(true);                                  // indispensable en game loop

NkAddress to{ "127.0.0.1", 7778 };
sock.SendTo(payload, payloadSize, to);                      // UDP : envoi à une cible

uint32 got = 0; NkAddress from;
sock.RecvFrom(buf, sizeof(buf), got, from);                // got == 0 si rien (non-bloquant)
// ~NkSocket ferme automatiquement
```

Le réglage **non-bloquant** (`SetNonBlocking(true)`) est ce qui rend un socket compatible avec une
boucle de jeu : sans lui, `RecvFrom` *attendrait* qu'un paquet arrive et figerait la frame. En
non-bloquant, `RecvFrom` rend la main immédiatement avec `outSize = 0` quand il n'y a rien — on
appelle donc en boucle, on draine tout ce qui est arrivé, et on passe à autre chose. Pour surveiller
**plusieurs** sockets sans tous les sonder, `NkSocket::Select` indique lesquels ont des données à lire.

Ce n'est **pas** une couche de fiabilité : un `SendTo` UDP qui « réussit » ne garantit pas l'arrivée
du paquet. Si vous avez besoin de garanties, ne réimplémentez pas l'ACK à la main — montez d'un
étage vers `NkReliableUDP`.

> **En résumé.** `NkSocket` = socket OS en RAII, UDP ou TCP, **move-only**. `PlatformInit`/`Shutdown`
> une fois en tout ; `Create` (bind) puis `SetNonBlocking(true)` en game loop ; `SendTo`/`RecvFrom`
> (UDP) ou `Connect`/`Send`/`Recv` (TCP). UDP ne garantit pas la livraison — pour ça, `NkReliableUDP`.

---

## La fiabilité à la carte : `NkReliableUDP`

C'est la pièce maîtresse. `NkReliableUDP` orchestre **une connexion point à point** par-dessus un
`NkSocket` UDP et reconstruit, sélectivement, les garanties que l'UDP n'offre pas : accusés de
réception (ACK), retransmission des paquets perdus, remise en ordre, fragmentation des gros messages,
estimation du temps d'aller-retour (RTT). Le tout sans une seule allocation tas : ses fenêtres
d'envoi/réception sont des tableaux fixes.

L'idée centrale est le **canal** (`NkNetChannel`). Plutôt qu'imposer un compromis unique, on choisit
*par message* le niveau de garantie voulu : `NK_NET_CHANNEL_UNRELIABLE` (UDP pur, le plus rapide —
pour la position d'un joueur, qu'on renvoie de toute façon 30 fois par seconde),
`NK_NET_CHANNEL_RELIABLE_ORDERED` (ACK + retransmit + ordre — pour les commandes critiques),
`NK_NET_CHANNEL_RELIABLE_UNORDERED`, ou `NK_NET_CHANNEL_SEQUENCED` (seul le plus récent compte).

```cpp
NkReliableUDP conn;
conn.Init(&sock, server);                 // socket NON possédé, doit rester valide + non-bloquant

// chaque frame :
conn.Update(dt);                          // retransmissions, ACKs, heartbeats
conn.Send(cmd, cmdSize, NK_NET_CHANNEL_RELIABLE_ORDERED);   // garanti + ordonné
conn.Send(pos, posSize, NK_NET_CHANNEL_UNRELIABLE);         // rapide, jetable

// après avoir lu un datagramme du socket :
conn.OnReceived(raw, rawSize);            // valide l'en-tête, traite les ACKs, range par canal

NkVector<NkRecvEntry> messages;
conn.Drain(messages);                     // récupère ce qui est livrable
```

Le contrat d'usage tient en quatre verbes répétés chaque frame : `Update` (la mécanique interne —
retransmettre ce qui n'a pas été ACKé, envoyer les ACK et heartbeats dus), `OnReceived` (digérer
chaque datagramme brut sorti du socket), `Send` (mettre un message en file sur un canal), `Drain`
(récupérer les messages enfin livrables). C'est `Update` + `OnReceived` qui font tout le travail
invisible ; vous, vous ne voyez que `Send` et `Drain`.

Deux pièges d'**ownership** et de portée à connaître. D'abord, `Init` reçoit un `NkSocket*` qu'il **ne
possède pas** : c'est à vous de garder ce socket vivant et non-bloquant tant que la connexion
existe. Ensuite, `NkReliableUDP` est **non copiable et non déplaçable** (aucun move déclaré) — créez-le
à son emplacement définitif et gardez-en une référence stable.

> **En résumé.** `NkReliableUDP` = fiabilité **par canal** sur un `NkSocket` UDP non possédé.
> Boucle : `Update(dt)` + `OnReceived(raw)` chaque frame, `Send(..., canal)` pour émettre, `Drain`
> pour recevoir. Choisissez le canal selon le besoin (vitesse vs garantie). Ni copie ni move.

---

## Aperçu de l'API

Tout le contenu public des trois headers, en un coup d'œil. Chaque élément est détaillé dans la
« Référence complète ». `[noexcept]` est implicite **partout** ; `[nodiscard]` est signalé quand il
s'applique (et l'absence notable signalée aussi).

### `NkNetDefines.h` — socle commun

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handle global | `using NkSocketHandle`, `kNkInvalidSocket` | Type natif du socket + valeur invalide (selon plateforme). |
| Macros socket | `NK_NET_CLOSE_SOCKET(s)`, `NK_NET_SOCKET_ERRORS` | Fermeture + code d'erreur, conditionnels plateforme. |
| Version | `NKNET_VERSION_MAJOR/MINOR/PATCH` | `1` / `0` / `0`. |
| Limites (global) | `kNkMaxConnections` (256), `kNkMaxPacketSize` (1400), `kNkMaxPayloadSize` (1380) | Plafonds connexions / paquet / charge utile. |
| Limites (global) | `kNkSendBufferSize`/`kNkRecvBufferSize` (512 Kio), `kNkMaxChannels` (8) | Tampons socket et nombre de canaux. |
| Limites (global) | `kNkHeartbeatIntervalMs` (250), `kNkTimeoutMs` (10000), `kNkMaxRetransmits` (5) | Cadence heartbeat / délai d'expiration / retransmissions max. |
| Limites (global) | `kNkWindowSize` (64), `kNkMaxFragments` (16), `kNkReplicationHistorySize` (60) | Fenêtre glissante / fragments max / historique réplication. |
| Logging (global) | `NK_NET_LOG_INFO/WARN/ERROR/DEBUG(fmt, …)` | Logs préfixés `[NKNet]` (placeholders `{0}`, suppose un `logger` au site). |
| Assert (global) | `NK_NET_ASSERT(cond, msg)` | **Log seulement**, n'arrête PAS le programme. |
| Config | `struct NkNetworkConfig` | `maxConnections`, `socketBufferSize`, `connectionTimeoutMs`, `heartbeatIntervalMs`. |
| Config | `class NkNetworkConfigManager` | Singleton global : `Configure` / `Get` `[nodiscard]` / `ResetToDefaults` (thread-safe). |
| Résultat | `enum class NkNetResult : uint8` | 14 codes de `NK_NET_OK`(0) à `NK_NET_UNKNOWN`(13). |
| Résultat | `NkNetResultStr(r)` `[nodiscard]` | Libellé statique d'un code (inline, `O(1)`). |
| Identité | `struct NkPeerId` | Id de pair (`value`) : `IsValid`/`IsServer`/`Invalid`/`Server`/`Generate`, `==`/`!=`/`<`. |
| Identité | `struct NkNetId` | Id d'entité répliquée (`id`+`owner`) : `IsValid`/`Invalid`/`Pack`/`Unpack`, `==`/`!=`. |
| Temps | `using NkTimestampMs`, `NkNetNowMs()` `[nodiscard]` | Horloge monotone ms (relative session), thread-safe. |
| Canaux | `enum class NkNetChannel : uint8` | `UNRELIABLE`/`RELIABLE_ORDERED`/`RELIABLE_UNORDERED`/`SEQUENCED`/`SYSTEM`. |
| Byte-order | `NkHToN16/32/64`, `NkNToH16/32/64` | Conversion hôte ↔ réseau (non-inline, body en `.cpp`). |

### `NkSocket.h` — socket et adresse

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Handle global | `using NkNativeSocketHandle`, `kNkNativeInvalidSocket` | Handle natif (jeu de macros **distinct** de NkNetDefines). |
| Macros socket | `NK_NET_CLOSE_SOCKET_IMPL(s)`, `NK_NET_SOCKET_ERROR_CODE` | Fermeture + code d'erreur, conditionnels plateforme. |
| Adresse | `class NkAddress` | IPv4/IPv6 + port. |
| Adresse — ctors | `NkAddress()`, `(host, port)`, `(a,b,c,d, port)` | Invalide / par chaîne (DNS) / par octets. |
| Adresse — fabriques | `Loopback`/`Any`/`Broadcast`/`Resolve` `[nodiscard]` | Boucle locale / toutes interfaces / diffusion / résolution DNS bloquante. |
| Adresse — accès | `Port`/`GetFamily`/`IsIPv4`/`IsIPv6`/`IsValid`/`IsLoopback`/`IsBroadcast`/`IsMulticast` `[nodiscard]` | Inspection. |
| Adresse — texte | `ToString`/`HostString` `[nodiscard]` | `"ip:port"` / hôte seul. |
| Adresse — ops | `operator==`/`!=`/`<` | Égalité et ordre (famille→IP→port). |
| Adresse — enum | `enum class Family : uint8` | `NK_IP_V4`(0) / `NK_IP_V6`(1). |
| Paquet | `struct NkPacket` | Buffer inline 1400 o : `data`, `size`, `from`, `seqNum`, `ackMask`, `channel`, `IsEmpty`, `Clear`. |
| Socket | `class NkSocket` | Socket RAII UDP/TCP, move-only. |
| Socket — enum | `enum class Type : uint8` | `NK_UDP`(0) / `NK_TCP`(1). |
| Socket — vie | `Create` `[nodiscard]`, `Close`, move ctor/assign | Bind / fermeture idempotente / transfert. |
| Socket — config | `SetNonBlocking`/`SetBroadcast`/`SetNoDelay`/`SetSendBufferSize`/`SetRecvBufferSize`/`SetReuseAddr` `[nodiscard]` | Réglages d'options. |
| Socket — UDP | `SendTo`/`RecvFrom` `[nodiscard]` | Envoi/réception datagramme. |
| Socket — TCP | `Listen`/`Connect`/`Accept`/`Send`/`Recv` `[nodiscard]` | Cycle connecté (envois/réceptions possiblement partiels). |
| Socket — inspect | `IsValid`/`GetType`/`GetLocalAddr`/`GetLastError` `[nodiscard]` | État du socket. |
| Socket — statiques | `PlatformInit`/`PlatformShutdown`/`Select` | Init/arrêt pile réseau ; multiplexing lecture (PAS `nodiscard`). |

### `NkReliableUDP.h` — fiabilité sur UDP

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| En-tête fil | `struct NkRUDPHeader` | En-tête fixe 20 o big-endian : `Serialize`/`Deserialize` `[nodiscard]`, `kMagic`/`kSize`. |
| Drapeaux | `enum NkRUDPFlags : uint8` | Bits combinables : `Reliable`/`Ordered`/`Ack`/`Handshake`/`Disconnect`/`Ping`/`Fragment`/`Encrypted`. |
| Entrée envoi | `struct NkSendEntry` | Paquet fiable en attente d'ACK (fenêtre d'envoi). |
| Entrée recv | `struct NkRecvEntry` | Paquet validé en attente de livraison. |
| RTT | `class NkRTTEstimator` | Jacobson-Karels : `Update`, `GetRTT`/`GetJitter`/`GetRTO` `[nodiscard]`. |
| Canal | `class NkReliableChannel` | Un canal unidirectionnel à fenêtre glissante (64). |
| Canal — émission | `PrepareReliable` `[nodiscard]`, `GatherPendingSends`, `ProcessACK` | Assigner un seqNum / collecter à (ré)émettre / traiter les ACK. |
| Canal — réception | `InsertReceived` `[nodiscard]`, `DrainDeliverable` | Ranger un reçu / extraire le livrable. |
| Canal — stats | `GetOutgoingAckNum`/`GetOutgoingAckMask`/`GetPendingCount`/`GetPacketLoss` `[nodiscard]` | ACK sortant / en vol / perte. |
| Connexion | `class NkReliableUDP` | Orchestration RUDP d'une connexion point à point (move/copie interdits). |
| Connexion — vie | `Init`, `Update` | Lier un socket non possédé + cible / tick par frame. |
| Connexion — I/O | `Send` `[nodiscard]`, `SendACK` `[nodiscard]`, `OnReceived`, `Drain` | Émettre (fragmente) / ACK forcé / digérer un datagramme / extraire le livré. |
| Connexion — métriques | `GetRTT`/`GetJitter`/`GetPacketLoss`/`GetPingMs`/`GetRemote`/`GetLastRecv` `[nodiscard]`, `UpdateRTT` | Statistiques de lien (`UpdateRTT` NON const/nodiscard). |

---

## Référence complète

Chaque élément repris en détail. Les structures triviales (buffers, accesseurs) sont décrites
brièvement ; les pièces vivantes (socket, fiabilité, RTT) le sont **à fond**, avec leurs usages dans
les différents domaines — gameplay/IA, ECS, physique, animation, rendu, audio, UI/2D, outils,
threading — car le réseau touche *tout* ce qui doit être synchronisé entre machines.

### Le socle : constantes, version, logging

Les `kNk*` (au scope global) sont les **constantes de dimensionnement** de toute la pile, et elles
ne sont pas décoratives : `kNkMaxPacketSize` (1400) est calé sous la **MTU Ethernet** typique (~1500)
pour qu'un datagramme tienne en un seul paquet IP sans fragmentation réseau ; `kNkMaxPayloadSize`
(1380) en retranche les 20 octets d'en-tête RUDP ; `kNkWindowSize` (64) fixe le nombre de paquets en
vol ; `kNkMaxFragments` (16) borne la taille d'un message reconstituable (16 × 1380 ≈ 22 Kio).
`NK_NET_LOG_*` préfixe `[NKNet]` et **suppose une variable `logger` visible au site d'appel** — avec
des placeholders **indexés** `{0} {1}` (style libfmt), pas du `printf`. Détail critique :
`NK_NET_ASSERT` **ne stoppe rien** ; il se contente de logguer l'échec. Ne comptez donc pas dessus
pour interrompre une exécution invalide — c'est un filet de log, pas un garde-fou.

### `NkNetResult` et `NkNetResultStr` — le code d'erreur universel

Comme toute l'API est `noexcept`, **tout** passe ses erreurs par cet `enum class` de 14 valeurs
(de `NK_NET_OK`=0 à `NK_NET_UNKNOWN`=13). C'est la convention de retour de presque chaque fonction de
la couche. On l'utilise dans tous les domaines dès qu'un échange peut échouer :

- **Gameplay / IO réseau** — `Create` renvoie `NK_NET_SOCKET_ERROR` si le port est déjà pris,
  `NK_NET_CONNECTION_REFUSED` quand le serveur dit non, `NK_NET_TIMEOUT` quand un correspondant ne
  répond plus : autant de branches d'UI (« serveur injoignable », « partie pleine »).
- **Boucle d'envoi** — en non-bloquant, `SendTo`/`Send` peuvent rendre `NK_NET_BUFFER_FULL` : le
  tampon OS est saturé, il faut réessayer plus tard (et non considérer ça comme une erreur fatale).
- **Validation** — `NK_NET_PACKET_TOO_LARGE` quand on dépasse `kNkMaxPacketSize`, `NK_NET_INVALID_ARG`
  sur un argument incohérent.
- **Outils / debug** — `NkNetResultStr(r)` (inline, `O(1)`, chaîne statique à ne **pas** libérer)
  donne un libellé lisible pour les logs et les panneaux de diagnostic d'un éditeur réseau.

### `NkNetworkConfig` et `NkNetworkConfigManager` — la configuration globale

`NkNetworkConfig` regroupe les réglages réseau du programme (nombre max de connexions, taille des
tampons socket, délais). `NkNetworkConfigManager` en est le **singleton thread-safe** (protégé par un
mutex global) : on appelle `Configure(cfg)` **avant toute initialisation** réseau, puis `Get()`
partout (qui rend une référence constante), `ResetToDefaults()` pour revenir aux valeurs d'usine.
C'est l'une des **deux seules choses thread-safe** de la couche — pratique pour qu'un thread d'outils
(menu d'options, ligne de commande serveur) ajuste la config pendant que le thread réseau tourne.

### `NkPeerId` et `NkNetId` — les identités du réseau

Deux identifiants opaques à ne pas confondre. `NkPeerId` désigne **un participant** à la session : la
convention veut que `value==0` soit invalide et `value==1` soit **le serveur** (d'où `IsServer()` et
les fabriques `Invalid()`/`Server()`). `Generate()` forge un id unique — mais **par convention, côté
serveur seulement**, car c'est lui qui distribue les identités. `NkPeerId` est ordonnable
(`operator<`), donc utilisable comme clé d'un conteneur trié de joueurs.

`NkNetId` désigne **une entité répliquée** sur le réseau, et il est volontairement **distinct du
`NkEntityId` ECS local** : la même entité a un id local sur chaque machine, mais un seul id *réseau*
partagé. Il encode un `owner` (uint8, la machine qui fait autorité) et un `id` (uint32), qu'on peut
sérialiser en un seul `uint64` via `Pack()` / reconstituer via `Unpack()` — exactement ce qu'on glisse
dans un paquet d'état. Notez qu'il n'a **pas** d'`operator<` (contrairement à `NkPeerId`). Usages :

- **ECS / réplication** — la table de correspondance « id réseau ↔ entité locale » que tient chaque
  client pour appliquer les snapshots du serveur sur les bonnes entités.
- **Gameplay** — référencer un objet distant (« le projectile #42 du joueur 3 ») dans un message RPC.

### `NkNetNowMs` et `NkTimestampMs` — l'horloge réseau

`NkTimestampMs` (un `uint64` en millisecondes, **relatif au début de session** et non à l'epoch) est
l'unité de temps de toute la couche fiabilité : c'est avec lui qu'on date l'envoi d'un paquet pour
savoir quand le retransmettre, ou la dernière réception pour détecter un timeout. `NkNetNowMs()`
fournit cette horloge **monotone** (elle ne recule jamais, contrairement à l'heure murale) et
**thread-safe**. Au-delà du réseau, c'est une horloge monotone utile partout où il faut mesurer un
intervalle sans risquer un ajustement d'heure système (profiling d'outils, timers gameplay).

### `NkNetChannel` — choisir son compromis

L'enum des **canaux** est le concept de design le plus important de la couche : il refuse le faux
choix « tout TCP » ou « tout UDP » et laisse arbitrer *par flux de données*.

- **`NK_NET_CHANNEL_UNRELIABLE`** (UDP pur) — pour ce qui est **renvoyé en continu**, où une perte
  est sans conséquence car la prochaine mise à jour corrige : position/rotation d'un joueur, état
  d'animation, valeurs d'interpolation. C'est le canal des données « jetables et fraîches ».
- **`NK_NET_CHANNEL_RELIABLE_ORDERED`** (ACK + retransmit + ordre) — pour les **commandes critiques**
  qui doivent toutes arriver dans l'ordre : « le joueur a ramassé l'arme », « la porte s'ouvre »,
  messages de chat, transactions d'inventaire.
- **`NK_NET_CHANNEL_RELIABLE_UNORDERED`** — garanti mais **sans contrainte d'ordre** : des événements
  indépendants entre eux (déclencher trois sons, valider trois succès) qu'on veut sûrs sans payer le
  coût d'attente du réordonnancement.
- **`NK_NET_CHANNEL_SEQUENCED`** — seul **le plus récent** est livré, les retards sont jetés : idéal
  quand seule la dernière valeur compte (curseur de visée, dernier état d'une jauge).
- **`NK_NET_CHANNEL_SYSTEM`** — réservé au moteur (handshake, heartbeat, déconnexion).

**Piège d'implémentation** : `NkReliableUDP` n'instancie en réalité que **deux** canaux fiables
(ordered + unordered). `SEQUENCED` et `SYSTEM` existent dans l'enum mais n'ont **pas** de canal membre
dédié dans l'orchestrateur — à savoir si vous comptiez sur un comportement sequenced distinct.

### Les helpers byte-order — parler le « réseau »

`NkHToN16/32/64` (host→network) et `NkNToH16/32/64` (network→host) convertissent les entiers entre
l'ordre d'octets de la machine et le **big-endian** du réseau. Tout entier multi-octets écrit dans un
paquet (un seqNum, une taille, un id) **doit** passer par là à l'émission et au décodage, faute de
quoi deux machines d'**endianness** différente s'incompréhendraient. La version 64 bits est une
implémentation logicielle maison (pas de `htonl64` POSIX). Détail de build à respecter : ces fonctions
sont **non-inline** (corps dans le `.cpp`) — ne pas les redéclarer `inline`, ce qui casserait le link.

### `NkAddress` — l'adresse à fond

Au-delà des fabriques vues plus haut, `NkAddress` expose une batterie de prédicats
(`IsLoopback`/`IsBroadcast`/`IsMulticast`) et deux rendus texte : `ToString()` (`"ip:port"`, ou
`"[::1]:port"` en v6) pour les logs, `HostString()` pour l'hôte seul. L'ordre total (`operator<`,
trié famille→IP byte-à-byte→port) en fait une **clé** valable dans un conteneur trié. Usages :

- **IO réseau** — la cible d'un `SendTo`, la source remplie par `RecvFrom`, l'adresse à laquelle un
  serveur se *bind* (`Any(port)`).
- **Gameplay / lobby** — une liste de serveurs (chacun une `NkAddress` affichée via `ToString`), un
  système d'annuaire, une table « adresse → joueur ».
- **Outils** — un panneau de monitoring réseau d'éditeur qui liste les pairs connectés par adresse.

**Pièges.** Le DNS de `Resolve` (qui rend un `NkVector<NkAddress>`, vide en cas d'échec) et du ctor
par nom est **bloquant** : à isoler hors de la frame. Et l'initialisation des membres étant partielle,
le ctor par défaut ne garantit pas un état « tout zéro » — toujours passer par `IsValid()`.

### `NkPacket` — le datagramme inline

Structure de transport sans la moindre allocation : `data` est un buffer **inline de 1400 octets**
(`kNkMaxPacketSize`), accompagné de `size`, de l'adresse source `from`, et des champs de fiabilité
(`seqNum`, `ackMask`, `channel`). `IsEmpty()` teste `size==0`. **Subtilité** : `Clear()` remet à zéro
`size`/`seqNum`/`ackMask` mais **ne touche pas** au buffer `data` (pas de memset coûteux) — le
contenu résiduel reste, ce qui est sans danger tant qu'on respecte `size`.

### `NkSocket` — le wrapper à fond

Le cœur de la couche transport. RAII (`~NkSocket` appelle `Close()`, idempotent), **move-only** (copie
supprimée ; le move ferme le socket courant avant de transférer et invalide la source). `Create` fait
le *bind* (port 0 = l'OS choisit) ; un socket déjà ouvert est refermé d'abord.

**Configuration.** `SetNonBlocking` est *le* réglage de boucle de jeu (voir plus haut). `SetNoDelay`
désactive l'algorithme de Nagle en TCP (latence minimale, au prix de plus de petits paquets) — utile
pour des commandes interactives. `SetBroadcast` autorise la diffusion UDP (découverte de serveurs sur
le LAN). `SetReuseAddr` (SO_REUSEADDR) facilite le rebind rapide d'un port. Les tailles de tampon sont
**indicatives** : l'OS peut les ajuster.

**Comportement non-bloquant** (à graver) : `RecvFrom` rend `NK_NET_OK` avec `outSize=0` quand rien
n'est arrivé ; `Accept` rend OK avec un client **invalide** s'il n'y a pas de connexion en attente (il
faut donc vérifier `outClient.IsValid()`) ; `SendTo`/`Send` peuvent rendre `NK_NET_BUFFER_FULL` ;
`Send`/`Recv` TCP peuvent être **partiels** (on boucle jusqu'à tout traiter).

Usages par domaine :

- **Gameplay / IO réseau** — la fondation : tout message de jeu finit en `SendTo` (UDP) ou `Send`
  (TCP). Un serveur fait `Create(Any(port))` + `Listen` (TCP) ou boucle `RecvFrom` (UDP).
- **Threading** — un thread réseau dédié possède le socket et draine les paquets ; le socket
  **n'étant pas thread-safe**, on protège l'accès partagé par un `NkMutex`, ou mieux on le confine à
  un seul thread et on échange via une file.
- **Découverte / LAN** — `SetBroadcast(true)` + `Broadcast(port)` pour annoncer/repérer des parties
  locales sans serveur central.
- **Outils / éditeur** — `Select` multiplexe plusieurs sockets (plusieurs clients d'un mini-serveur de
  debug) sans les sonder un à un ; `GetLastError` remonte le code OS brut pour le diagnostic.

Les statiques `PlatformInit`/`PlatformShutdown` encadrent **toute** vie réseau (une fois chacune).
`Select(readSockets, timeoutMs, outReady)` remplit `outReady` des indices des sockets lisibles
(`timeoutMs=0` immédiat, `UINT32_MAX` infini ; rend `NK_NET_TIMEOUT` si rien). Ces deux-là ne sont
**pas** `nodiscard`.

### `NkRUDPHeader` et `NkRUDPFlags` — le format sur le fil

`NkRUDPHeader` est l'**en-tête fixe de 20 octets** préfixant chaque datagramme RUDP, en **big-endian**.
Son layout est figé : `magic` (toujours `0x4E`='N', pour rejeter les paquets étrangers), `flags`,
`channel`, indices de fragment (`fragIdx`/`fragCount`), `dataSize`, et les trois nombres clés de la
fiabilité — `seqNum` (numéro du paquet), `ackNum` (dernier reçu), `ackMask` (bitmask des 32 derniers
reçus, qui acquitte plusieurs paquets d'un coup). `Serialize(buf)` écrit l'en-tête (≥ `kSize` octets,
**sans** le payload) en network order ; `Deserialize(buf, bufSize)` rend faux si le buffer est trop
court ou le magic invalide. C'est le genre de structure qu'on n'écrit qu'une fois mais qui sous-tend
tout — et un excellent modèle pour quiconque conçoit un **format binaire** (outils de capture, replay).

`NkRUDPFlags` est un `enum` **non-class** (donc combinable par OR) de bits : `Reliable`/`Ordered`/`Ack`
décrivent la nature du paquet, `Handshake`/`Disconnect`/`Ping` les messages de contrôle, `Fragment`
signale un morceau de gros message, `Encrypted` un payload chiffré.

### `NkSendEntry` et `NkRecvEntry` — les cases des fenêtres

Deux structures de stockage inline (pas de heap). `NkSendEntry` est une case de la **fenêtre d'envoi**
: un paquet fiable émis mais **pas encore ACKé**, avec son `seqNum`, sa date d'envoi (`sentAt`), son
compteur de retransmissions et le flag `acked`. `NkRecvEntry` est une case de réception : un payload
**validé en attente de livraison** à l'application, avec son `seqNum` et son flag `valid`. C'est le
`NkRecvEntry` qu'on récupère via `Drain`.

### `NkRTTEstimator` — mesurer la latence

Petit objet, rôle capital. Il estime le **temps d'aller-retour** (RTT) et son instabilité (jitter) par
l'algorithme **Jacobson-Karels** (RFC 6298) : à chaque échantillon, `Update(sampleMs)` lisse le RTT
(`RTT ← (1-α)RTT + α·sample`) et la déviation, en **ignorant les valeurs aberrantes** (>10× le RTT). De
là, `GetRTO()` (RTT + 4×jitter) donne le **délai au-delà duquel un paquet est présumé perdu** et doit
être retransmis — trop court, on retransmet inutilement ; trop long, on réagit lentement à une perte.
Tout en `O(1)`, non thread-safe. Usages :

- **IO réseau** — le moteur de retransmission s'en sert pour décider *quand* renvoyer un paquet.
- **Gameplay** — afficher le ping (`GetRTT`), adapter l'agressivité de l'interpolation/prédiction
  côté client selon la latence et le jitter.
- **Outils** — graphes de latence en temps réel dans un panneau de diagnostic réseau.

### `NkReliableChannel` — un canal à fond

C'est la mécanique de fiabilité **unidirectionnelle** d'un seul canal : une **fenêtre glissante** fixe
de 64 cases (`kNkWindowSize`), un `seqNum` monotone depuis 1, et aucune allocation (tout sur la pile).
Son cycle :

- `PrepareReliable(data, size, ch)` assigne un seqNum au message et le range dans la fenêtre (size ≤
  `kNkMaxPayloadSize`) ; rend le seqNum, ou **`UINT32_MAX` si la fenêtre est pleine** (signal de
  saturation à gérer — l'autre bout n'acquitte pas assez vite). Il ne transmet rien lui-même.
- `GatherPendingSends(now, rto, out)` collecte ce qui doit partir : les nouveaux et les non-ACKés dont
  le `rto` est dépassé. Les pointeurs rendus restent valides **jusqu'au prochain `Update`**.
- `ProcessACK(ackNum, ackMask)` libère les cases confirmées (le `ackMask` en acquitte jusqu'à 32 d'un
  coup) et met les stats à jour.
- `InsertReceived(seqNum, data, size)` range un paquet reçu (rend faux si doublon ou hors-fenêtre).
- `DrainDeliverable(out)` extrait le livrable : en **ORDERED**, seulement les paquets consécutifs
  depuis `mExpectedSeq` (on attend les manquants) ; en **UNORDERED**, tout ce qui est là. Les cases
  livrées redeviennent réutilisables.

Les accesseurs `GetOutgoingAckNum/Mask` fournissent l'ACK à renvoyer à l'autre bout, `GetPendingCount`
le nombre de paquets **en vol** (0..64), `GetPacketLoss` le taux de perte (0–1, sur ~128 derniers) —
ce dernier nourrit l'UI de qualité de connexion et toute logique d'adaptation de débit.

### `NkReliableUDP` — l'orchestrateur à fond

La classe que **vous** utilisez. Elle assemble tout le reste pour gérer **une connexion point à point**
: deux `NkReliableChannel` (un ordered, un unordered), un `NkRTTEstimator`, un tampon de réassemblage de
fragments, et une file pour l'unreliable. Non copiable, non déplaçable, sans allocation tas.

- `Init(socket, remote)` lie un `NkSocket*` **non possédé** (qui doit rester valide et non-bloquant)
  et la cible distante.
- `Update(dt)` est le **moteur** appelé chaque frame (`dt` en secondes) : il retransmet les paquets
  non-ACKés dont le RTO est dépassé, envoie les ACK périodiques et les **heartbeats** (pour garder la
  connexion vivante et détecter une coupure).
- `Send(data, size, channel)` met un message sur le canal voulu, en **fragmentant automatiquement** si
  `size > kNkMaxPayloadSize` (jusqu'à `kNkMaxFragments`). `SendACK()` force l'envoi d'un ACK pur
  (normalement géré par `Update`).
- `OnReceived(rawData, rawSize)` digère un **datagramme brut** (en-tête inclus) sorti du socket :
  valide l'en-tête, traite les ACK reçus, range le payload sur le bon canal, met le RTT à jour.
- `Drain(out)` extrait les messages enfin livrables (entrées valides jusqu'au prochain `Drain`).

Les **métriques** (`GetRTT`/`GetJitter`/`GetPacketLoss` — moyenne des canaux fiables —, `GetPingMs`,
`GetRemote`, `GetLastRecv`) alimentent l'affichage de qualité de lien et la logique d'adaptation.
`UpdateRTT(sampleMs)` injecte un échantillon RTT **applicatif** (un PING/PONG de niveau jeu) — c'est la
seule méthode non-`const` et non-`nodiscard` du lot. En interne (privé), `SendFragmented` et
`TryReassemble` gèrent les gros messages, mais le `FragBuffer` n'autorise **qu'un seul réassemblage à
la fois**.

Usages par domaine :

- **Gameplay / IO réseau** — la connexion d'un client à son serveur : commandes en
  `RELIABLE_ORDERED`, états de monde en `UNRELIABLE`, fichiers/gros blobs via la fragmentation auto.
- **ECS / réplication** — transporter les snapshots d'entités (`NkNetId` + composants), les ACK
  permettant au serveur de savoir quelle base le client connaît (delta-compression).
- **Audio** — déclencheurs de sons réseau en `RELIABLE_UNORDERED` (sûrs, mais l'ordre exact importe peu).
- **Animation** — paramètres d'état d'animation en `UNRELIABLE` (la prochaine frame corrige une perte).
- **UI / 2D** — affichage du ping et de la perte (`GetPingMs`, `GetPacketLoss`) dans le HUD multijoueur.
- **Threading** — la connexion **n'est pas thread-safe** (sauf compilation `NKNET_THREAD_SAFE`) ; on la
  confine au thread réseau et on échange avec le gameplay via des files protégées.
- **Outils / éditeur** — un visualiseur de trafic exploitant `GetRTT`/`GetPacketLoss` et la lecture de
  `NkRUDPHeader` pour inspecter chaque paquet.

---

### Exemple récapitulatif

```cpp
#include "NKNetwork/Transport/NkReliableUDP.h"
using namespace nkentseu::net;

NkSocket::PlatformInit();                                  // une seule fois, au démarrage

// --- mise en place ---
NkSocket sock;
sock.Create(NkAddress::Any(0), NkSocket::Type::NK_UDP);    // port auto
sock.SetNonBlocking(true);                                 // obligatoire en game loop

NkReliableUDP conn;
conn.Init(&sock, NkAddress{ "127.0.0.1", 7777 });          // socket NON possédé

// --- chaque frame ---
// 1) drainer ce que l'OS a reçu, le donner à la couche fiabilité
uint8 buf[kNkMaxPacketSize]; uint32 got = 0; NkAddress from;
while (sock.RecvFrom(buf, sizeof(buf), got, from) == NK_NET_OK && got > 0)
    conn.OnReceived(buf, got);

// 2) faire tourner la mécanique (retransmissions, ACKs, heartbeats)
conn.Update(dt);

// 3) émettre : le bon canal pour le bon besoin
conn.Send(positionBytes, posSize, NK_NET_CHANNEL_UNRELIABLE);        // jetable, frais
conn.Send(commandBytes,  cmdSize, NK_NET_CHANNEL_RELIABLE_ORDERED);  // garanti, ordonné

// 4) consommer les messages livrables
NkVector<NkRecvEntry> inbox;
conn.Drain(inbox);
for (const auto& msg : inbox) { /* msg.data, msg.size : traiter */ }

// --- diagnostic ---
uint32 ping = conn.GetPingMs();
float32 loss = conn.GetPacketLoss();

// ~NkSocket ferme le socket ; en fin de programme :
NkSocket::PlatformShutdown();
```

---

[← Index NKNetwork](README.md) · [Récap NKNetwork](../NKNetwork.md) · [Couche System](../README.md)
