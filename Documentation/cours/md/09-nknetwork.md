# NKNetwork : faire parler deux machines

Le réseau est le dernier des cinq modules, et le plus abstrait : rien ne
s'affiche, rien ne s'entend, et quand ça ne marche pas, il n'y a souvent aucun
message. C'est aussi celui où le périmètre honnête est le plus important à
poser, parce que le module contient beaucoup de code que *rien* dans le
dépôt n'utilise.

## Où il vit, et ce dont il a besoin

Première singularité : contrairement aux quatre autres, NKNetwork n'est pas dans
`Kernel/Runtime` mais dans `Kernel/System`. C'est un service
système, au même titre que le système de fichiers ou les fils d'exécution.

**`Kernel/System/NKNetwork/ — arborescence`**

```
NKNetwork/
  NKNetwork.jenga · ROADMAP.md
  tests/.jenga/                 <- DOSSIER VIDE (voir plus bas)
  src/NKNetwork/
    NKNetwork.h                  <- en-tete PARAPLUIE + alias de commodite
    Core/NkNetDefines.h/.cpp     <- types, enums, constantes
    Transport/
      NkSocket.h/.cpp            <- socket UDP/TCP brut
      NkReliableUDP.h/.cpp       <- ACK, retransmit, fenetre (usage INTERNE)
    Protocol/
      NkBitStream.h/.cpp         <- NkBitWriter / NkBitReader
      NkConnection.h/.cpp        <- NkConnection + NkConnectionManager
    Replication/NkNetWorld.h/.cpp
    RPC/NkRPC.h/.cpp
    Lobby/NkLobby.h/.cpp
    HTTP/NkHTTPClient.h/.cpp
```

Tout vit dans le *namespace* `nkentseu::net`.

**`Kernel/System/NKNetwork/NKNetwork.jenga:27-42 (abrégé)`**

```cpp
_NET_DEPS = ["NKCore", "NKPlatform", "NKMemory", "NKContainers", "NKLogger", "NKThreading", "NKMath", "NKTime", "NKFileSystem"]
_NET_INCLUDES = ["src", "pch"]
_NET_DEFINES = []
if _WANT_MBEDTLS:
    _NET_DEPS.append("NKMbedTLS")
    _NET_INCLUDES.append("%{wks.location}/Externals/Libs/NKMbedTLS/include")
    _NET_DEFINES.append("NKENTSEU_HTTP_USE_MBEDTLS")
```

> **⚠️ Il faut lier `ws2_32` sous Windows**
>
> Le module utilise les sockets Berkeley, qui s'appellent Winsock sous Windows.
> Votre *application* doit donc ajouter la bibliothèque à son édition de
> liens — le module ne le fait pas pour vous. Les deux consommateurs du dépôt le
> font :
> `Applications/NkNetWorldDemo/NkNetWorldDemo.jenga:35-40` et
> `Sandbox/System/NKNetwork/NKNetworkSandbox.jenga:74-78` ajoutent tous deux
> `"ws2_32"` à leur `links()`.
>
> L'oubli se manifeste par des erreurs d'édition de liens sur des symboles
> `__imp_socket`, `__imp_WSAStartup`… — obscures si on ne
> sait pas d'où elles viennent, évidentes ensuite.

### Deux avertissements sur la documentation du module

> **⚠️ La description du `.jenga` parle d'un autre module**
>
> `Kernel/System/NKNetwork/NKNetwork.jenga:3-15` décrit « un système de
> réflexion runtime » avec les fichiers `NkType`, `NkClass`,
> `NkProperty`, `NkMethod`, `NkRegistry`. C'est la description
> de **NKReflection**, copiée par erreur. Aucun de ces fichiers n'existe dans
> NKNetwork.

> **⚠️ Le dossier `tests/` est vide**
>
> `Kernel/System/NKNetwork/tests/` ne contient qu'un sous-dossier vide. Le
> glob `testfiles(["tests/**.cpp"])` de `NKNetwork.jenga:92-95` ne
> correspond à aucun fichier : le bloc `with test():` est un squelette mort.
>
> De plus, il n'existe **aucune** fonction `SelfTest()` dans tout le
> module — là où NKMedia en compte 52 et NKAudio 2.
>
> Cela ne veut pas dire que le module n'est pas testé : le test existe, mais c'est
> une application séparée, et la feuille de route le dit :
>
> **`Kernel/System/NKNetwork/ROADMAP.md:131-138`**
>
> ```
>  ### Tests (2026-07-12)
>  - `Sandbox/System/NKNetwork` (cible `SandboxNKNetwork`) : **67 checks verts** —
>    NkBitStream round-trip + overflow, NkNetId Pack/Unpack, NkNetInterpolator,
>    réplication client/serveur sur messages forgés, **bout-en-bout loopback réel**
>    (StartServer + Connect 127.0.0.1, handshake, snapshot → spawn → delta →
>    despawn). Mode manuel `--https <url>` pour valider TLS.
> ```
>
> Attention au chemin : ce sandbox est dans `Sandbox/System/NKNetwork/` — à
> la racine du dépôt — et **pas** dans `Applications/Sandbox/`. Ses
> binaires sont bâtis en Debug et en Release.

## Le périmètre de ce chapitre

> **✅ Ce que nous enseignons, et pourquoi**
>
> **Enseigné** — parce que du code du dépôt l'exerce réellement :
>
> - `NkSocket` (initialisation de plateforme, et UDP brut pour la
>   découverte réseau) ;
> - `NkAddress` ;
> - `NkConnectionManager` — la classe centrale ;
> - `NkBitWriter` / `NkBitReader` ;
> - `NkNetWorld` — la réplication d'entités.
>
> **Non enseigné** — parce qu'*aucune* application du dépôt ne s'en
> sert :
>
> - `NkLobby`, `NkSession`, `NkMatchmaker`,
>   `NkDiscovery` — 67 Ko d'en-tête sans un seul consommateur ;
> - `NkRPC` / `NkRPCRouter` — la feuille de route le classe
>   « livré (déclaratif) », ce qui n'est pas la même chose que « prouvé » ;
> - `NkReliableUDP` en usage direct : il n'est employé qu'en interne
>   par `NkConnection`.
>
> Ce n'est pas un jugement sur la qualité de ce code. C'est une règle de prudence :
> un cours ne doit enseigner que ce qu'il peut montrer en train de fonctionner.

Sont également absents, d'après `ROADMAP.md:32-40` : la prédiction client
et la réconciliation serveur, le relais des entités à autorité client, les
transports WebSocket et WebRTC, la compression des instantanés, le chiffrement
DTLS, et la traversée de NAT (STUN/TURN/ICE).

## Le socle : la plateforme

**`Kernel/System/NKNetwork/src/NKNetwork/Transport/NkSocket.h:746-753`**

```cpp
static NkNetResult PlatformInit() noexcept;
static void PlatformShutdown() noexcept;
```

Sous Windows, `PlatformInit` appelle `WSAStartup`. Sous les autres
systèmes, il ne fait presque rien — mais il faut l'appeler quand même, sinon
votre code ne sera pas portable. La feuille de route est catégorique :
« `PlatformInit()` **requis avant usage** » (`ROADMAP.md:52-54`).

**`Applications/Pong/src/Pong/Net/NetworkSession.cpp:28-40`**

```cpp
        void NetworkSession::PlatformInit() {
            const auto r = net::NkSocket::PlatformInit();
            if (r != net::NkNetResult::NK_NET_OK) {
                logger.Error("[Net] PlatformInit failed: {0}", (int)r);
            } else {
                logger.Info("[Net] PlatformInit OK");
            }
        }

        void NetworkSession::PlatformShutdown() {
            net::NkSocket::PlatformShutdown();
            logger.Info("[Net] PlatformShutdown");
        }
```

### Les codes de retour

**`Kernel/System/NKNetwork/src/NKNetwork/Core/NkNetDefines.h:268-291`**

```cpp
enum class NkNetResult : uint8 {
    NK_NET_OK = 0, NK_NET_INVALID_ARG, NK_NET_NOT_CONNECTED, NK_NET_ALREADY_CONNECTED,
    NK_NET_CONNECTION_REFUSED, NK_NET_TIMEOUT, NK_NET_PACKET_TOO_LARGE, NK_NET_SOCKET_ERROR,
    NK_NET_BUFFER_FULL, NK_NET_NOT_INITIALIZED, NK_NET_PLATFORM_UNSUPPORTED,
    NK_NET_AUTH_FAILED, NK_NET_BANNED, NK_NET_UNKNOWN
};
inline const char* NkNetResultStr(NkNetResult r) noexcept;   // -> texte lisible
```

`NkNetResultStr` existe : utilisez-le dans vos journaux plutôt que
d'imprimer un entier. Un « erreur réseau 6 » ne vous dira rien dans six mois.

### Les adresses

**`Kernel/System/NKNetwork/src/NKNetwork/Transport/NkSocket.h:172-353`**

```cpp
class NkAddress {
        NkAddress(const char* ip, uint16 port);
        static NkAddress Loopback(uint16 port, Family f = Family::NK_IP_V4) noexcept;
        static NkAddress Any(uint16 port, Family f = Family::NK_IP_V4) noexcept;
        static NkAddress Broadcast(uint16 port) noexcept;
        bool IsValid() const noexcept;
        NkString ToString() const noexcept;
};
```

Trois fabriques nommées, à comprendre une fois pour toutes :

- `Loopback(port)` — 127.0.0.1, la machine locale. Pour tester ;
- `Any(port)` — « toutes mes interfaces réseau ». C'est ce à quoi un
  serveur se lie. `Any(0)` laisse même le système choisir le port ;
- `Broadcast(port)` — 255.255.255.255, tout le réseau local.

> **⚠️ Toujours valider une adresse saisie**
>
> `NkAddress("192.168.1.20", 7777)` *analyse* la chaîne. Si
> l'utilisateur a tapé n'importe quoi, l'adresse est invalide et
> `Connect` échouera de façon obscure. Pong teste systématiquement avant
> (`Applications/Pong/src/Pong/Net/NetworkSession.cpp:122-129`) et affiche
> un message utile.

## `NkConnectionManager` : la classe du chapitre

C'est par elle que tout passe. Elle fait tenir, derrière une API courte, un
protocole UDP fiable complet : poignée de main, accusés de réception,
retransmissions, canaux, déconnexion gracieuse.

**`Kernel/System/NKNetwork/src/NKNetwork/Protocol/NkConnection.h:733-790 (abrégé)`**

```cpp
class NkConnectionManager {
        NkConnectionManager() noexcept = default;
        ~NkConnectionManager() noexcept;                              // (Shutdown auto)
        NkConnectionManager(const NkConnectionManager&) = delete;     // NON COPIABLE

        NkNetResult StartServer(uint16 port, uint32 maxClients = 64) noexcept;
        NkNetResult Connect(const NkAddress& serverAddr, uint16 localPort = 0) noexcept;
        void Shutdown() noexcept;

        NkNetResult SendTo(NkPeerId peer, const uint8* data, uint32 size, NkNetChannel ch) noexcept;
        NkNetResult Broadcast(const uint8* data, uint32 size, NkNetChannel ch) noexcept;
        void DrainAll(NkVector<NkReceiveMsg>& out) noexcept;
        void Disconnect(NkPeerId peer, const char* reason = nullptr) noexcept;
        void DisconnectAll(const char* reason = nullptr) noexcept;
        bool IsServer() const noexcept;   bool IsRunning() const noexcept;
        uint32 ConnectedPeerCount() const noexcept;
        bool GetConnectionStats(NkPeerId peer, NkConnectionStats& outStats) const noexcept;

        /* NkFunction */ onPeerConnected;      ///< (NkPeerId)
        /* NkFunction */ onPeerDisconnected;   ///< (NkPeerId, const char* reason)
        uint32 maxConnections = kNkMaxConnections;
};
```

### Trois invariants de fil d'exécution

Tout le reste du chapitre découle de ces trois faits.

**1. `StartServer` et `Connect` lancent un fil.**

**`Kernel/System/NKNetwork/src/NKNetwork/Protocol/NkConnection.h:762-788`**

```
 @note Crée un socket UDP et le lie à l'adresse Any(port).
 @note Démarre le thread réseau interne pour polling automatique.
 [...]
 @note Envoie une déconnexion gracieuse à tous les pairs connectés.
 @note Attend la fin du thread réseau avant retour (join).
```

Vous n'avez donc pas à interroger le réseau vous-même : un fil le fait en
permanence, et `Shutdown()` l'attend proprement.

**2. Les *callbacks* s'exécutent sur ce fil.**

`onPeerConnected` et `onPeerDisconnected` ne sont **pas**
appelés depuis votre boucle principale. N'y faites rien de lourd : pas de
création d'objets de jeu, pas de manipulation de l'interface, pas d'allocation.
Pong s'y limite à un incrément atomique
(`Applications/Pong/src/Pong/Net/NetworkSession.cpp:60-63`).

**3. La poignée de main est asynchrone.**

`Connect()` renvoie `NK_NET_OK` *avant* que la connexion
soit établie. Ce code de retour signifie « la demande est partie », pas « je suis
connecté ». Le seul test valable est de surveiller
`ConnectedPeerCount()` dans une boucle bornée.

### Le bout-en-bout, tel qu'il est testé

**`Sandbox/System/NKNetwork/src/main.cpp:313-334`**

```cpp
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
```

Cinq cents itérations de dix millisecondes : cinq secondes au maximum. La boucle
est **bornée** — un test qui attend indéfiniment n'est pas un test. Et on
vérifie les *deux* côtés : le serveur voit un pair, le client aussi.

### On ne reçoit pas, on draine

**`Kernel/System/NKNetwork/src/NKNetwork/Protocol/NkConnection.h:229-235`**

```cpp
struct NkReceiveMsg {
        uint8 data[kNkMaxPayloadSize] = {};   ///< buffer INLINE (1380 octets), pas un pointeur
        uint32 size = 0;
        NkPeerId from;
        NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
        NkTimestampMs receivedAt = 0;
};
```

Le fil réseau accumule les messages ; votre boucle les récupère d'un coup :

**`Sandbox/System/NKNetwork/src/main.cpp:397-410`**

```cpp
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
```

> **⚠️ `msgs.Clear()` avant chaque `DrainAll`**
>
> `DrainAll` *ajoute* à votre conteneur. Sans le `Clear()`,
> vous retraiterez les anciens messages à chaque tour, indéfiniment.
>
> Et il y a une seconde raison de vider ce conteneur : `NkReceiveMsg`
> contient un tampon **en ligne** de 1 380 octets, pas un pointeur. Un
> `NkVector<NkReceiveMsg>` de cent éléments pèse 138 Ko. Ne le copiez
> jamais inutilement, et videz-le entre deux images.

Corollaire de ce tampon fixe : **un message ne peut pas dépasser
1 380 octets**. Au-delà, `SendTo` renvoie
`NK_NET_PACKET_TOO_LARGE`.

**`Kernel/System/NKNetwork/src/NKNetwork/Core/NkNetDefines.h:145-175`**

```cpp
static constexpr uint32 kNkMaxConnections  = 256u;
static constexpr uint32 kNkMaxPacketSize   = 1400u;  // taille MTU-safe
static constexpr uint32 kNkMaxPayloadSize  = 1380u;
static constexpr uint32 kNkMaxChannels     = 8u;
static constexpr uint32 kNkMaxRetransmits  = 5u;
static constexpr uint32 kNkMaxFragments    = 16u;
```

Ces 1 400 octets ne sont pas arbitraires : c'est la taille sous laquelle un
datagramme traverse Internet sans être fragmenté par les routeurs. La
fragmentation IP est la première cause de pertes silencieuses.

> **⚠️ Tester `msg.data` avant de le déréférencer**
>
> Le dépôt contient deux copies presque identiques du même fichier, et leur
> **seule** différence est instructive :
> `Applications/Pong/src/Pong/Net/NetworkSession.cpp:253` déréférence
> `msg.data[0]` sans garde ; la copie corrigée ajoute
> `&& msg.data != nullptr`. Enseignons la version corrigée.

## Choisir son canal

C'est la décision de conception la plus importante du module, et le fichier de
définitions la documente mieux que n'importe quel manuel :

**`Kernel/System/NKNetwork/src/NKNetwork/Core/NkNetDefines.h:505-525`**

```cpp
enum class NkNetChannel : uint8 {
    NK_NET_CHANNEL_UNRELIABLE = 0,       ///< UDP pur — positions, inputs, animations
    NK_NET_CHANNEL_RELIABLE_ORDERED,     ///< TCP-like — chat, événements gameplay
    NK_NET_CHANNEL_RELIABLE_UNORDERED,   ///< livraison garantie, ordre libre
    NK_NET_CHANNEL_SEQUENCED,            ///< seul le plus récent est livré
    NK_NET_CHANNEL_SYSTEM                ///< RÉSERVÉ AU MOTEUR — ne pas utiliser
};
```

Le raisonnement à tenir est le suivant : **une donnée qui sera de toute
façon remplacée dans 16 millisecondes ne mérite pas d'être retransmise**. La
position d'un joueur perdue en route est sans importance — la suivante arrive.
En revanche, un message de chat perdu est perdu pour toujours.

| **Vous envoyez** | **Canal** |
|---|---|
| Position, entrées, animation | `UNRELIABLE` |
| Chat, événements de jeu, commandes critiques | `RELIABLE_ORDERED` |
| Fichiers, gros blocs où l'ordre importe peu | `RELIABLE_UNORDERED` |
| États continus (points de vie, énergie) | `SEQUENCED` |
| Rien du tout | `SYSTEM` |

Le canal `SYSTEM` est réservé au moteur — « usage interne uniquement,
comportement non défini si utilisé ». Ce n'est pas une suggestion.

Pong choisit son canal par un simple booléen, ce qui est le bon niveau
d'abstraction pour une application :

**`Applications/Pong/src/Pong/Net/NetworkSession.cpp:234-243`**

```cpp
        bool NetworkSession::Broadcast(const uint8 *data, uint32 size, uint8 reliable) {
            if (mConnMgr == nullptr)
                return false;
            if (mState.load() != NetworkState::Connected)
                return false;
            const auto ch = reliable ? net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
                                     : net::NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
            const auto r = mConnMgr->Broadcast(data, size, ch);
            return r == net::NkNetResult::NK_NET_OK;
        }
```

Dans le jeu, l'entrée du joueur et l'instantané d'état partent en non fiable
(`GameplayScene.cpp:705` et `:849`), tandis que la pause, le but
marqué et la demande de revanche partent en fiable (`:227`, `:293`,
`:309`).

## `NkBitStream` : ne pas gaspiller le réseau

Avec 1 380 octets par message, chaque bit compte. C'est là qu'intervient la
partie la plus élégante du module.

**`Kernel/System/NKNetwork/src/NKNetwork/Protocol/NkBitStream.h:165-352 (abrégé)`**

```cpp
void WriteBool(bool) ;  WriteU8/U16/U32/U64 ;  WriteI8/I16/I32 ;  WriteF32
void WriteF32Q(float32 v, float32 minV, float32 maxV, float32 prec) noexcept;    // quantifie
void WriteInt(int32 v, int32 minV, int32 maxV) noexcept;                         // borne
void WriteVec3f(const NkVec3f&) ;  WriteVec3fQ(v, minV, maxV, prec) ;  WriteQuatf(const NkQuatf&);
void WriteString(const char* s, uint32 maxLen = 256) noexcept;
void WriteBytes(const uint8* data, uint32 size) noexcept;
void WriteBits(uint32 v, uint32 numBits) noexcept;
void AlignToByte() noexcept;
bool IsOverflowed() const noexcept;
usize BytesWritten() const;
```

Les méthodes de lecture sont exactement symétriques (`ReadBool`,
`ReadU8`, `ReadF32Q`, `ReadInt`…).

### L'argument : la quantification

Un `float` coûte 32 bits. Mais si vous savez qu'un score est entre 0 et
999, il tient sur 10 bits. Et si une position est entre −100 et +100 au
centimètre près, elle tient sur environ 15 bits au lieu de 32.

**`Exemple écrit pour le cours (API : NkBitStream.h:237, 252)`**

```cpp
#include "NKNetwork/Protocol/NkBitStream.h"

uint8 buf[256];
net::NkBitWriter w(buf, sizeof(buf));
w.WriteU8(kMonTypeDeMessage);
w.WriteInt(scoreJoueur, 0, 999);              // 10 bits au lieu de 32
w.WriteF32Q(posX, -100.f, 100.f, 0.01f);      // quantifie, ~15 bits
w.AlignToByte();
if (!w.IsOverflowed())
    client.SendTo(peer, buf, (uint32)w.BytesWritten(),
                  net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
```

> **⚠️ Écriture et lecture doivent être des miroirs exacts**
>
> **`Exemple écrit pour le cours (API : NkBitStream.h:455-635)`**
>
> ```cpp
> net::NkBitReader r(m.data, m.size);
> const uint8  type  = r.ReadU8();
> const int32  score = r.ReadInt(0, 999);                    // MEMES bornes
> const float32 x    = r.ReadF32Q(-100.f, 100.f, 0.01f);     // MEMES bornes, MEME precision
> if (r.IsOverflowed()) { /* message tronque : jeter */ }
> ```
>
> Un flux de bits n'est pas auto-descriptif : il n'y a ni noms de champs, ni
> balises. Si le lecteur écrit `ReadInt(0, 9999)` là où l'écrivain a écrit
> `WriteInt(0, 999)`, il ne lira pas le bon nombre de bits, et
> **tout ce qui suit sera décalé**. Les symptômes sont absurdes : une position
> correcte suivie d'un identifiant aberrant.
>
> Deux disciplines s'imposent. D'abord, écrire les deux fonctions
> *côte à côte*, dans le même fichier, immédiatement l'une après l'autre.
> Ensuite, tester systématiquement `IsOverflowed()` des deux côtés : c'est
> la seule détection d'erreur dont vous disposez.

Le *round-trip* de test est le meilleur exercice du module, parce qu'il ne
demande aucun réseau :

**`Sandbox/System/NKNetwork/src/main.cpp:47-89 (abrégé)`**

```cpp
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
        /* ... symetrique ... */
        NK_CHECK(!reader.IsOverflowed());

        // Overflow en ecriture comme en lecture.
        uint8 tiny[2];
        NkBitWriter tinyWriter(tiny, sizeof(tiny));
        tinyWriter.WriteU32(0xFFFFFFFFu);
        NK_CHECK(tinyWriter.IsOverflowed());
    }
```

## `NkNetWorld` : répliquer des entités

Envoyer des octets, c'est bien. Synchroniser l'état d'un monde, c'est le vrai
sujet. `NkNetWorld` fournit la mécanique.

**`Kernel/System/NKNetwork/src/NKNetwork/Replication/NkNetWorld.h:128-515 (abrégé)`**

```cpp
struct NkNetEntity {
        NkNetId netId;  uint32 prefabId;  void* user;
        WriteStateFn writeState;   ///< serialise l'etat — appele cote AUTORITE
        ReadStateFn  readState;    ///< applique un etat recu — cote replique
};

class NkNetWorld {
        struct Config { float32 tickRate; uint32 keyframeInterval; /* ... */ };
        void Init(NkConnectionManager* mgr, bool isServer, Config cfg = {}) noexcept;
        NkNetId AllocateNetId() noexcept;
        bool RegisterEntity(const NkNetEntity& desc) noexcept;
        bool UnregisterEntity(NkNetId netId, bool notifyPeers = true) noexcept;
        NkNetEntity* FindEntity(NkNetId netId) noexcept;
        uint32 EntityCount() const noexcept;
        void Update(float32 dt) noexcept;
        bool HandleMessage(const NkReceiveMsg& msg) noexcept;
        uint32 CurrentTick() const noexcept;
        void DrainInputs(NkPeerId peer, NkVector<NkNetInput>& out) noexcept;
        SpawnCb   onEntitySpawn;      ///< (NkNetId, uint32 prefabId, NkPeerId owner)
        DespawnCb onEntityDespawn;    ///< (NkNetId)
};
```

Le modèle tient en trois phrases :

**`Kernel/System/NKNetwork/src/NKNetwork/Replication/NkNetWorld.h:20-22`**

```
 HandleMessage(). Entité inconnue → callback onEntitySpawn (l'application
 crée son objet et l'enregistre) ; état reçu → readState ; destruction →
 onEntityDespawn. Les inputs locaux remontent via SendInput().
```

Côté serveur — l'autorité — on décrit comment sérialiser :

**`Sandbox/System/NKNetwork/src/main.cpp:349-361`**

```cpp
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
```

Côté client — la réplique — on réagit à l'apparition d'une entité inconnue :

**`Sandbox/System/NKNetwork/src/main.cpp:171-203 (abrégé)`**

```cpp
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
```

Le `prefabId` est votre code de type : « ceci est un joueur », « ceci est
un projectile ». C'est à vous de décider ce que chaque numéro signifie et de
créer l'objet correspondant.

> **✅ Les deux règles des fonctions d'état**
>
> **`writeState` doit être déterministe** : « deux appels sur le même
> état doivent produire les mêmes octets »
> (`Replication/NkNetWorld.h:124-126`). Sinon le calcul de différences
> envoie des mises à jour perpétuelles pour un état qui n'a pas bougé.
>
> **Écrivez des lambdas, pas des pointeurs de fonction nus.** Le pont ECS du
> moteur le documente
> (`Engine/Noge/src/Noge/ECS/Replication/NkNetWorld.cpp:132-138`) : un
> pointeur de fonction brut résout mal la surcharge du type de rappel.

## Le socket brut : la découverte sur le réseau local

Il reste un cas où l'on descend sous `NkConnectionManager` : annoncer un
serveur pour que les autres machines le trouvent sans qu'on ait à taper une
adresse IP.

**`Kernel/System/NKNetwork/src/NKNetwork/Transport/NkSocket.h:504-715 (abrégé)`**

```cpp
class NkSocket {
        enum class Type : uint8 { NK_UDP, NK_TCP /* ... */ };
        NkNetResult Create(const NkAddress& localAddr, Type type = Type::NK_UDP) noexcept;
        void Close() noexcept;
        NkNetResult SetNonBlocking(bool v) noexcept;
        NkNetResult SetBroadcast(bool v) noexcept;
        NkNetResult SendTo(const void* data, uint32 size, const NkAddress& to) noexcept;
        NkNetResult RecvFrom(void* buf, uint32 bufSize, uint32& outSize, NkAddress& outFrom) noexcept;
        bool IsValid() const noexcept;
};
```

Le socket qui émet la balise :

**`Applications/Pong/src/Pong/Net/NetworkDiscovery.cpp:44-63 (abrégé)`**

```cpp
            mBeaconSock = new net::NkSocket();
            // Bind sur une socket UDP IPv4 quelconque (port 0 = laisse l'OS choisir),
            // on n'a besoin que d'envoyer. SetBroadcast active SO_BROADCAST
            // necessaire pour pouvoir SendTo vers 255.255.255.255.
            const auto rc = mBeaconSock->Create(net::NkAddress::Any(0), net::NkSocket::Type::NK_UDP);
            if (rc != net::NkNetResult::NK_NET_OK) { /* ... */ return false; }
            (void)mBeaconSock->SetNonBlocking(true);
            const auto rb = mBeaconSock->SetBroadcast(true);
            if (rb != net::NkNetResult::NK_NET_OK) { /* ... */ return false; }
```

et celui qui écoute, lié au port de balise sur toutes les interfaces :

**`Applications/Pong/src/Pong/Net/NetworkDiscovery.cpp:84-96 (abrégé)`**

```cpp
            mScanSock = new net::NkSocket();
            // Bind sur kBeaconPort, INADDR_ANY : on reçoit les broadcasts
            // emis vers 255.255.255.255:kBeaconPort par d'autres machines du LAN.
            const auto rc = mScanSock->Create(net::NkAddress::Any(kBeaconPort), net::NkSocket::Type::NK_UDP);
            /* ... */
            (void)mScanSock->SetNonBlocking(true);
            (void)mScanSock->SetBroadcast(true);
```

> **⚠️ Sans `SetBroadcast(true)`, l'émission échoue**
>
> Le système d'exploitation refuse par défaut d'envoyer vers une adresse de
> diffusion — c'est une protection. Il faut l'autoriser explicitement, des deux
> côtés. C'est l'oubli numéro un de la découverte réseau.

Le *tick*, avec ses deux garde-fous :

**`Applications/Pong/src/Pong/Net/NetworkDiscovery.cpp:115-160 (abrégé)`**

```cpp
            if (mBeaconSock != nullptr) {
                mBeaconTimer -= dt;
                if (mBeaconTimer <= 0.0f) {
                    const auto dst = net::NkAddress::Broadcast(kBeaconPort);
                    const auto rs = mBeaconSock->SendTo(&mBeaconPkt, sizeof(mBeaconPkt), dst);
                    mBeaconTimer = kBeaconIntervalSec;
                }
            }
            if (mScanSock != nullptr) {
                // Boucle : on lit tant qu'il y a des datagrammes pendants.
                // En non-bloquant, RecvFrom OK avec outSize=0 = rien a lire.
                constexpr int kMaxPerTick = 32;
                for (int i = 0; i < kMaxPerTick; ++i) {
                    uint8 buf[256];
                    uint32 received = 0;
                    net::NkAddress from;
                    const auto rr = mScanSock->RecvFrom(buf, sizeof(buf), received, from);
                    if (rr != net::NkNetResult::NK_NET_OK) break;
                    if (received == 0) break;
                    /* filtre magic + version, puis UpdateHost(pkt, from) */
                }
            }
```

Deux points à retenir :

1. **en mode non bloquant, un `RecvFrom` qui réussit avec
   `outSize == 0` signifie « rien à lire »**, pas « erreur ». C'est
   la condition d'arrêt normale de la boucle ;
2. **la boucle est bornée** à 32 datagrammes par image. Sur un réseau
   chargé — ou face à un émetteur mal réglé — une boucle non bornée
   monopoliserait la frame.

Notez enfin le filtrage « magic + version » : un port UDP reçoit tout ce que
n'importe qui envoie. Toujours vérifier une signature avant d'interpréter.

## HTTP, et pourquoi HTTPS échoue

**`Kernel/System/NKNetwork/src/NKNetwork/HTTP/NkHTTPClient.h:383-717 (abrégé)`**

```cpp
struct NkHTTPResponse {
        uint32 statusCode = 0;
        NkString body;
        NkString error;
        bool IsOK() const noexcept;
};
class NkHTTPClient {
        NkHTTPResponse Get(const char* url) noexcept;
        NkHTTPResponse Post(const char* url, const char* json) noexcept;
};
```

> **⚠️ HTTPS est désactivé par défaut**
>
> Le support TLS est optionnel, activé par une variable d'environnement au moment
> de la construction :
>
> **`Kernel/System/NKNetwork/NKNetwork.jenga:21-25`**
>
> ```cpp
> # TLS/HTTPS opt-in : NK_ENABLE_TLS=1 (ou mbedtls) active le backend mbed-TLS
> # (bibliotheque NKMbedTLS, C pur, cross-compilee pour toutes les plateformes y
> # compris iOS). Desactive par defaut -> HTTP simple uniquement, aucune dependance.
> _TLS_OPT = os.getenv("NK_ENABLE_TLS", "").strip().lower()
> _WANT_MBEDTLS = _TLS_OPT in ("1", "true", "on", "yes", "mbedtls")
> ```
>
> Sans cette variable, toute URL en `https://` échoue — proprement, au
> moins :
>
> **`Kernel/System/NKNetwork/src/NKNetwork/HTTP/NkHTTPClient.cpp:1050`**
>
> ```cpp
>             response.error = "HTTPS not compiled in (rebuild with NK_ENABLE_TLS=1)";
> ```
>
> Et si vous activez TLS, votre application doit lier `NKMbedTLS` en plus,
> comme le fait le sandbox. Pour ce cours, tenons-nous-en à `http://`.

Le mode de test manuel du sandbox montre l'usage complet :

**`Sandbox/System/NKNetwork/src/main.cpp:441-451 (abrégé)`**

```cpp
int main(int argc, char **argv) {
    // Mode HTTPS manuel : SandboxNKNetwork --https <url>
    // Nécessite un build avec NK_ENABLE_TLS=1 (backend mbedTLS), sinon le
    // client renvoie proprement "HTTPS not compiled in".
    if (argc >= 3 && NkString(argv[1]) == NkString("--https")) {
        NkHTTPClient http;
        const NkHTTPResponse resp = http.Get(argv[2]);
        /* ... affichage de resp.statusCode, resp.body.Length(), resp.error.CStr() ... */
        return (resp.statusCode >= 200 && resp.statusCode < 400) ? 0 : 1;
    }
```

## Comment structurer une application en réseau

Vous avez maintenant toutes les briques. Reste la question d'architecture, et
Pong y répond de la façon qu'il faut copier.

1. **Une classe de session** possède le `NkConnectionManager` et
   n'expose que des verbes simples :
   `StartHost`, `StartJoin`, `Broadcast`,
   `DrainReceived`, `Shutdown`, plus un état atomique
   (`Idle` / `Hosting` / `Connected`)
   (`Applications/Pong/src/Pong/Net/NetworkSession.h`) ;
2. **un `Tick(dt)`** appelé une fois par image draine le fil
   réseau et met à jour cet état :

   **`Applications/Pong/src/Pong/Game/PongApp.cpp:162-169`**

   ```cpp
               // Tick reseau (drain interne du thread reseau). Aucun cout si
               // la session est Idle.
               mNetwork.Tick(dt);
               // Tick decouverte LAN : emet beacon (host) + drain scan (client).
               mDiscovery.Tick(dt);
   ```

3. **les écrans d'interface lisent l'état, jamais le manager.** Aucune
   scène de Pong ne touche au `NkConnectionManager` : elles appellent
   `DrainReceived` et consultent `NetworkState`.

Le routage des messages se fait dans la session, par type, avec la garde qui
manquait :

**`Applications/Pong/src/Pong/Net/NetworkSession.cpp:245-260 (abrégé)`**

```cpp
        void NetworkSession::DrainInternal() {
            if (mConnMgr == nullptr)
                return;
            // Drain brut depuis NkConnectionManager.
            NkVector<net::NkReceiveMsg> all;
            mConnMgr->DrainAll(all);
            for (uint32 i = 0; i < all.Size(); ++i) {
                const auto &msg = all[i];
                if (msg.size >= sizeof(netproto::PktHello) && msg.data[0] == netproto::kMsgHello) {
                    netproto::PktHello pkt;
                    /* ... recopie des sizeof(pkt) octets de msg.data dans pkt ... */
                    continue;
                }
                // Message non-interne : reserve pour le caller via DrainReceived.
                mPendingForUser.PushBack(msg);
            }
        }
```

Notez le test `msg.size >= sizeof(...)` *avant* de lire le contenu :
un pair malveillant, ou simplement une version différente de votre programme,
peut envoyer n'importe quoi.

> **✅ Le découpage à retenir**
>
> **Le réseau vit sur son fil ; la session fait tampon ; l'interface ne voit
> qu'un état et une file de messages.** Aucune scène, aucun widget ne doit connaître
> `NkConnectionManager`. C'est ce qui rend le jeu testable sans réseau et le
> réseau modifiable sans toucher au jeu.

## L'ordre de fermeture

**`Sandbox/System/NKNetwork/src/main.cpp:430-433`**

```cpp
        client.Shutdown();
        server.Shutdown();
        NkSocket::PlatformShutdown();
```

et, avec une couche de réplication au-dessus :

**`Applications/NkNetWorldDemo/src/main.cpp:205-211`**

```cpp
    serverNet.Shutdown();
    clientNet.Shutdown();
    client.Shutdown();
    server.Shutdown();
    net::NkSocket::PlatformShutdown();
```

La règle est celle des poupées russes : **on éteint du plus haut vers le
plus bas**. Les systèmes de réplication d'abord, puis les gestionnaires de
connexion — qui joignent leur fil réseau —, puis la plateforme. Inverser
reviendrait à fermer Winsock pendant qu'un fil s'en sert encore.

> **⚠️ N'incluez pas l'en-tête parapluie dans une application ECS**
>
> Voici le piège le plus coûteux du module, documenté deux fois dans le dépôt :
>
> **`Applications/NkNetWorldDemo/src/main.cpp:20-37`**
>
> ```cpp
> // PAS l'umbrella NKNetwork/NKNetwork.h : il injecte des alias de commodité
> // dans `nkentseu` (dont `using NkNetSystem = net::NkNetSystem;` et
> // `using NkNetEntity = net::NkNetEntity;`) qui entrent en collision directe
> // avec l'adaptateur ECS de Noge (`nkentseu::NkNetSystem`, composant
> // `nkentseu::ecs::NkNetEntity`). On inclut donc uniquement les en-têtes
> // précis nécessaires.
> #include "Noge/ECS/Replication/NkNetWorld.h"
> #include "NKECS/World/NkWorld.h"
> #include "NKNetwork/Transport/NkSocket.h"
> #include "NKTime/NkChrono.h"
> #include "NKLogger/NkLog.h"
>
> using namespace nkentseu;
> using namespace nkentseu::ecs;
> // PAS de `using namespace nkentseu::net;` : net::NkNetEntity (descripteur de
> // réplication NKNetwork) et ecs::NkNetEntity (composant ECS) partagent le
> // même nom -- même piège que Noge/ECS/Replication/NkNetWorld.cpp.
> ```
>
> C'est l'exception à la règle générale du moteur — « incluez toujours l'en-tête
> parapluie » du chapitre 1. Ici, l'en-tête parapluie injecte des alias dans le
> *namespace* global du moteur, et deux types différents portent le même nom.
> Incluez les en-têtes précis, et n'ouvrez pas le *namespace*
> `nkentseu::net` en grand.

## Exercices

> **✏️ 1 — Le flux de bits, sans réseau**
>
> Écrivez un programme console qui sérialise une petite structure de jeu — un
> identifiant, un score entre 0 et 999, deux coordonnées entre −100 et +100 au
> centimètre, un booléen — puis la relit et vérifie l'égalité champ par champ.
>
> Affichez `BytesWritten()`. Comparez avec ce que coûterait la même
> structure en écriture brute (`WriteU32`, `WriteF32`). Puis
> introduisez volontairement une erreur : lisez le score avec les bornes
> `(0, 9999)`. Observez que ce n'est pas seulement le score qui devient
> faux, mais tout ce qui suit.

> **✏️ 2 — Serveur et client dans le même programme**
>
> Reproduisez `TestLoopback` : dans un seul `main`, démarrez un
> serveur sur 127.0.0.1, connectez un client, attendez la poignée de main dans une
> boucle bornée, échangez trois messages sur trois canaux différents, puis fermez
> dans le bon ordre.
>
> Ajoutez de la journalisation dans `onPeerConnected` — et regardez sur quel
> fil elle s'exécute. Puis, volontairement, supprimez la boucle d'attente et
> envoyez immédiatement après `Connect()`. Que se passe-t-il, et pourquoi ?

> **✏️ 3 — Un chat dans votre interface**
>
> Ajoutez à votre application NKGui deux boutons « Héberger » et « Rejoindre », un
> `InputText` pour l'adresse, un second pour le message, et une liste des
> messages reçus.
>
> Respectez l'architecture de Pong : une petite classe de session qui possède le
> manager, un `Tick(dt)` appelé une fois par image, et une interface qui ne
> voit qu'un état et une file. Utilisez `RELIABLE_ORDERED` — un message de
> chat perdu ne se rattrape pas.
>
> Testez avec deux instances de votre programme sur la même machine, puis, si vous
> le pouvez, sur deux machines du même réseau.

> **✏️ 4 — Mesurer ce que coûte la fiabilité**
>
> Reprenez l'exercice 2 et envoyez mille messages, une fois en
> `UNRELIABLE`, une fois en `RELIABLE_ORDERED`, en comptant ceux qui
> arrivent et en chronométrant.
>
> Sur une boucle locale, vous ne perdrez probablement rien : les deux chiffres
> seront identiques. C'est un résultat en soi — **tester le réseau sur
> 127.0.0.1 ne teste pas le réseau**. Consultez ensuite
> `GetConnectionStats` et voyez ce que le module sait vous dire du temps
> d'aller-retour et des pertes.
