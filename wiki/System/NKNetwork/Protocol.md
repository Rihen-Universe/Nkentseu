# Sérialisation bit-packée & connexions

> Couche **System** · NKNetwork · Faire tenir un état de jeu dans le **moins de bits possible**
> (`NkBitWriter` / `NkBitReader`) puis le faire voyager de façon fiable d'une machine à l'autre
> (`NkConnection`, `NkConnectionManager`).

Le réseau temps réel pose un problème que rien d'autre dans le moteur ne pose : **chaque octet
compte, et chaque octet peut se perdre**. Envoyer la position d'un joueur soixante fois par seconde
à trente clients, ce n'est pas la même chose que l'écrire dans un fichier. Si l'on sérialise
naïvement — un `float32` par axe, soit douze octets pour une position — on sature la bande passante
bien avant d'avoir un jeu jouable. Et le réseau, lui, ne garantit rien : un paquet UDP peut arriver
en retard, en double, dans le désordre, ou pas du tout. Cette page couvre les deux outils qui
répondent à ces deux problèmes : d'abord **comment empaqueter un état de jeu bit par bit** au lieu
d'octet par octet, ensuite **comment établir et maintenir une connexion fiable** par-dessus UDP.

Tout vit dans le même namespace, et — détail qui simplifie l'écriture — la famille `math` y est
importée, si bien que `NkVec3f` et `NkQuatf` s'écrivent sans qualification.

- **Namespace** : `nkentseu::net` (avec `using namespace math;` interne)
- **Headers** : `#include "NKNetwork/Protocol/NkBitStream.h"` · `#include "NKNetwork/Protocol/NkConnection.h"`
- **Export** : chaque classe/struct est marquée `NKENTSEU_NETWORK_CLASS_EXPORT`.

---

## Écrire bit par bit : `NkBitWriter`

Le cœur de l'économie de bande passante est là. Un `NkBitWriter` écrit dans un buffer d'octets que
**vous** lui fournissez, mais il ne raisonne pas en octets : il raisonne en **bits**. Un booléen
coûte un seul bit, pas un octet. Un entier que vous savez compris entre 0 et 100 coûte sept bits
(`log2(101)` arrondi au-dessus), pas trente-deux. Cette granularité fine est ce qui transforme un
paquet de cent octets en un paquet de vingt.

Le writer ne possède pas son buffer : vous lui passez un pointeur et une capacité à la construction,
et c'est vous qui gérez la durée de vie de cette mémoire (RAII côté appelant, **aucune allocation
interne**). À l'intérieur, un simple curseur `mBitPos` avance à chaque écriture. Toutes les méthodes
sont `noexcept` — rien ne lance, jamais.

```cpp
uint8 buffer[256];
NkBitWriter w(buffer, sizeof(buffer));
w.WriteBool(true);              // 1 bit
w.WriteInt(score, 0, 100);      // 7 bits, borné
w.WriteVec3fQ(pos, -512.f, 512.f, 0.01f);   // position quantifiée
uint32 toSend = w.BytesWritten();           // taille réelle à envoyer
```

La sécurité repose sur un drapeau `mOverflow` **collant** : dès qu'une écriture manque de place, il
passe à `true`, **y reste**, et **toute écriture suivante est silencieusement ignorée**. Ce n'est
**pas** une exception ni un crash : c'est un état qu'on consulte *après coup*, via `IsOverflowed()`.
Le réflexe à acquérir est de vérifier ce drapeau à la fin de chaque séquence d'écriture, et de jeter
le paquet s'il est levé.

> **En résumé.** `NkBitWriter` empaquette des valeurs **bit par bit** dans un buffer **non possédé**
> (durée de vie à votre charge). Toutes les écritures sont `noexcept` ; un débordement lève un flag
> `mOverflow` **collant** au lieu de planter — testez `IsOverflowed()` avant d'envoyer, et lisez la
> taille réelle avec `BytesWritten()`.

---

## La compression qui change tout : quantification

Écrire un `float32` brut coûte trente-deux bits (`WriteF32`, simple copie IEEE 754). Mais la plupart
des flottants d'un jeu n'ont **pas besoin** de cette précision. Une position dans un monde de mille
mètres, précise au centimètre, n'a que cent mille valeurs distinctes par axe : dix-sept bits
suffisent. C'est exactement ce que fait la **quantification**.

`WriteF32Q(v, minV, maxV, prec)` prend une valeur, l'intervalle dans lequel elle vit, et la
précision voulue ; il en déduit le nombre de bits strictement nécessaires (typiquement dix à seize),
normalise, quantifie, et encode. `WriteVec3fQ` applique le procédé aux trois axes. Pour les entiers,
`WriteInt(v, minV, maxV)` fait l'analogue : il encode sur `log2(maxV - minV + 1)` bits exactement —
avec une **assertion en debug** si `v` sort de l'intervalle annoncé.

Le cas le plus spectaculaire est `WriteQuatf`. Un quaternion, c'est quatre flottants, soit cent
vingt-huit bits bruts. Mais un quaternion **normalisé** a une contrainte (`x²+y²+z²+w²=1`) : on peut
donc omettre sa plus grande composante et la reconstruire. C'est la compression *« smallest three »*
— deux bits pour dire quelle composante est omise, puis trois composantes quantifiées sur environ
dix bits — soit **~32 bits au lieu de 128**. La contrepartie est une règle stricte : **le quaternion
doit être normalisé avant l'appel**, sinon la reconstruction côté lecture sera fausse.

```cpp
w.WriteF32Q(health, 0.f, 100.f, 0.5f);   // ~8 bits au lieu de 32
w.WriteQuatf(orientation.Normalized());   // ~32 bits au lieu de 128
```

Toute la magie a un prix unique : **la lecture doit utiliser exactement les mêmes paramètres**
`minV`, `maxV`, `prec`. Le buffer ne les transporte pas — ils sont implicites, partagés par
convention entre l'émetteur et le récepteur. Une seule divergence et c'est la désynchronisation
client/serveur garantie.

> **En résumé.** La quantification troque la précision *inutile* contre des bits : `WriteF32Q`
> /`WriteVec3fQ` pour les flottants bornés, `WriteInt` pour les entiers bornés (assert si hors
> bornes), `WriteQuatf` (*smallest three*, ~32 bits, quaternion **normalisé obligatoire**). Les
> paramètres `minV/maxV/prec` doivent être **identiques** à l'écriture et à la lecture.

---

## Relire dans le même ordre : `NkBitReader`

Le lecteur est le miroir exact du writer. Il prend un buffer en **lecture seule non possédé**, et
expose un `Read*` pour chaque `Write*`. La règle d'or tient en une phrase : **on lit dans le même
ordre, avec les mêmes paramètres, que ce qui a été écrit**. Le flux n'est pas auto-descriptif — il
n'y a pas de balises, pas de noms de champs, juste une suite de bits dont le sens vient entièrement
de l'ordre d'appel.

```cpp
NkBitReader r(packet, packetSize);
bool   alive = r.ReadBool();
int32  score = r.ReadInt(0, 100);
NkVec3f pos  = r.ReadVec3fQ(-512.f, 512.f, 0.01f);
if (r.IsOverflowed()) return;   // paquet tronqué / mal formé
```

La gestion d'erreur suit la même philosophie que le writer, mais inversée : si le buffer est épuisé
ou la lecture invalide, le reader **ne plante pas** — il renvoie une **valeur par défaut** (0,
`false`, `minV`, le quaternion identité) et lève son `mOverflow` collant. `ReadQuatf` reconstruit la
quatrième composante via `w = ±√(1 − x² − y² − z²)` et peut nécessiter une renormalisation. Un détail
pratique sur le bilan : pour un paquet bien formé, `BitsLeft()` doit valoir **zéro** une fois tout
relu — c'est une vérification de cohérence simple et fiable.

> **En résumé.** `NkBitReader` décode symétriquement : même ordre, mêmes paramètres de
> quantification. En cas d'épuisement il renvoie une valeur par défaut et lève `mOverflow` (collant)
> au lieu de planter. Un paquet sain se termine sur `BitsLeft() == 0` ; vérifiez `IsOverflowed()`
> après chaque séquence.

---

## La machine à états : `NkConnection`

Empaqueter des bits ne sert à rien si l'on n'a personne à qui parler de façon fiable. `NkConnection`
représente **une** connexion point à point construite par-dessus UDP, avec tout ce qu'UDP n'offre
pas : une poignée de main (*handshake*), une couche de fiabilité (`NkReliableUDP` intégré), des
heartbeats, des timeouts, une file de réception, des statistiques.

Le cycle de vie est une **machine à états** explicite (`NkConnectionState`) : on part de
`DISCONNECTED`, le client envoie un SYN et passe `SYN_SENT`, le serveur répond et l'on converge vers
`ESTABLISHED`. Côté serveur, le manager initialise déjà la connexion en `SYN_RECEIVED` après
l'*accept*. La fermeture suit le chemin inverse via `DISCONNECTING`, et un silence trop long mène à
`TIMED_OUT`.

```cpp
NkConnection conn;
conn.InitAsClient(&socket, serverAddr, myId);
conn.onConnected    = [](NkPeerId p){ /* prêt à jouer */ };
conn.onDisconnected = [](NkPeerId p, const char* why){ /* nettoyer */ };
if (conn.Connect() == NkNetResult::NK_NET_OK) { /* SYN envoyé */ }
```

Le point capital, à graver : **`NkConnection` n'est pas thread-safe** (sauf `Send`, et seulement si
le moteur est compilé avec `NKNET_THREAD_SAFE`). Toutes ses méthodes se pilotent depuis **le thread
réseau** — et ses callbacks `onConnected` / `onDisconnected` sont *invoqués depuis ce thread*. Si
vous voulez réagir côté gameplay, ne touchez pas directement à la scène dans le callback : différez
l'événement vers le thread jeu (un *event bus*). La connexion n'est pas copiable, et son destructeur
appelle `ForceDisconnect()` si nécessaire — pas de fuite.

> **En résumé.** `NkConnection` = une liaison P2P fiable sur UDP, pilotée par une **machine à états**
> (`DISCONNECTED → SYN → ESTABLISHED → DISCONNECTING/TIMED_OUT`). **Non thread-safe**, à conduire
> depuis le thread réseau ; les callbacks tombent **sur ce thread** → différez-les vers le gameplay.
> `Update(dt)` chaque frame, `DrainReceived` après.

---

## Orchestrer N pairs : `NkConnectionManager`

Un jeu multijoueur, ce n'est presque jamais une seule connexion. `NkConnectionManager` gère un
ensemble de `NkConnection` — en mode **serveur** (N clients) ou en mode **client** (vers un serveur)
— avec son propre **thread d'entrées/sorties**. C'est l'objet de façade qu'on manipule au quotidien.

À l'inverse de `NkConnection`, le manager **est thread-safe** sur son API publique d'envoi/réception
(`SendTo`, `Broadcast`, `DrainAll`, les déconnexions, les compteurs, `GetConnectionStats`) : il
protège ses tables par mutex. On peut donc appeler `DrainAll` depuis le thread jeu pendant que le
thread réseau reçoit. La capacité est **statique** (`kNkMaxConnections`, 256) : pas d'allocation
dynamique de connexions en pleine partie.

```cpp
NkConnectionManager mgr;
mgr.onPeerConnected    = [](NkPeerId p){ /* nouveau joueur */ };
mgr.onPeerDisconnected = [](NkPeerId p, const char* why){ /* parti */ };
mgr.StartServer(/*port*/ 7777, /*maxClients*/ 32);
// ... boucle jeu :
NkVector<NkReceiveMsg> inbox;
mgr.DrainAll(inbox);                       // copie + vide, thread-safe
mgr.Broadcast(snapshot, size);             // à tous
```

Un piège à connaître absolument : **`GetConnection()` renvoie un pointeur volatile**. Le thread
réseau peut l'invalider l'instant d'après (use-after-free possible). Pour lire des stats en sécurité,
n'utilisez **pas** ce pointeur — utilisez `GetConnectionStats(peer, out)`, qui fait une **copie
protégée** par mutex. Comme `NkConnection`, le manager n'est pas copiable et se nettoie tout seul :
son destructeur appelle `Shutdown()` si nécessaire.

> **En résumé.** `NkConnectionManager` orchestre jusqu'à 256 connexions (serveur ou client) avec un
> thread I/O dédié. Son API publique d'envoi/réception **est** thread-safe (mutex). `DrainAll` depuis
> le thread jeu, `Broadcast`/`SendTo` pour émettre. **N'utilisez jamais `GetConnection()` pour des
> stats** (pointeur volatile) → `GetConnectionStats()`. `Shutdown` automatique au destructeur.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Les complexités/garanties utiles sont entre crochets.
Le détail (comportement, cas d'usage) suit dans la « Référence complète ».

### `NkBitWriter` — encodeur bit-à-bit (buffer non possédé)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBitWriter(uint8* buf, uint32 capacity)` `[noexcept]` | Buffer destination + capacité (octets), compteurs à zéro. |
| Primitifs | `WriteBool` `[1 bit]`, `WriteU8/16/32/64`, `WriteI8/16/32`, `WriteF32` | Booléen / entiers non signés et signés / float IEEE 754 brut (32 bits). |
| Quantifié | `WriteF32Q(v, minV, maxV, prec)` | Float quantifié sur bits optimaux (~10-16). |
| Quantifié | `WriteInt(v, minV, maxV)` | Entier borné sur `log2(maxV-minV+1)` bits (assert debug si hors bornes). |
| Composites | `WriteVec3f` `[96 bits]`, `WriteVec3fQ`, `WriteQuatf` `[~32 bits]` | Vecteur brut / quantifié / quaternion *smallest three* (normalisé requis). |
| Composites | `WriteString(s, maxLen=256)`, `WriteBytes(data, size)` | Chaîne `u16 len + octets` (tronque si trop long) / blob (aligne d'abord). |
| Bas niveau | `WriteBits(v, numBits)` `[1-32]`, `AlignToByte()` | Brique de base / alignement sur frontière d'octet. |
| Bilan | `BitsWritten()`, `BytesWritten()`, `IsOverflowed()` `[nodiscard const]` | Bits écrits / octets à envoyer / débordement (collant). |

### `NkBitReader` — décodeur bit-à-bit (buffer lecture seule)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkBitReader(const uint8* buf, uint32 size)` `[noexcept]` | Buffer source ; taille stockée en **bits**. |
| Primitifs | `ReadBool`, `ReadU8/16/32/64`, `ReadI8/16/32`, `ReadF32` `[nodiscard]` | Symétriques du writer ; valeur par défaut (0/false) si erreur. |
| Quantifié | `ReadF32Q(minV, maxV, prec)`, `ReadInt(minV, maxV)` `[nodiscard]` | Déquantifie (mêmes params que l'écriture) ; renvoie `minV` si erreur. |
| Composites | `ReadVec3f`, `ReadVec3fQ`, `ReadQuatf` `[nodiscard]` | Vecteur / quaternion reconstruit (4ᵉ composante) ; identité si erreur. |
| Composites | `ReadString(out, maxLen=256)`, `ReadBytes(dst, size)` | Écrit via référence (non `nodiscard`) ; blob (aligne d'abord). |
| Bas niveau | `ReadBits(numBits)` `[nodiscard, 1-32]`, `AlignToByte()` | Brique de base / saut du padding. |
| Bilan | `BitsRead()`, `BytesRead()`, `BitsLeft()`, `IsOverflowed()`, `IsEmpty()` `[nodiscard const]` | Bits lus / octets / bits restants / débordement (collant) / fin atteinte. |

### `NkConnectionState` (enum `uint8`) & `NkConnStateStr`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| États | `NK_CONNECTION_DISCONNECTED(0)`, `SYN_SENT(1)`, `SYN_RECEIVED(2)`, `ESTABLISHED(3)`, `DISCONNECTING(4)`, `TIMED_OUT(5)` | Étapes de la machine à états. |
| Libre | `NkConnStateStr(s)` `[nodiscard, inline, noexcept]` | Libellé statique ("Disconnected"…"Unknown") — ne pas libérer. |

### Structs de données

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| `NkConnectionStats` | `bytesSent/Received`, `packetsSent/Received`, `retransmits`, `rttMs`, `jitterMs`, `packetLoss` `[0..1]`, `connectedSince` | Stats par connexion (lecture seule ; copier via `GetConnectionStats`). |
| `NkReceiveMsg` | `data[kNkMaxPayloadSize]`, `size`, `from`, `channel`, `receivedAt` | Message reçu (buffer **inline**, pas de heap) ; produit par `DrainReceived`/`DrainAll`. |
| `NkNetworkMetrics` | `activeConnections`, `peakConnections`, `bytesSent/Received`, `avgLatencyMs`, `packetLossPercent` `[0-100]`, `connectionErrors`, `timeoutErrors`, `lastUpdate` | Agrégat global (usage **interne** ; pas d'accesseur public exposé). |

### `NkConnection` — une liaison P2P fiable

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Vie | `NkConnection()`, `~NkConnection()`, copie **= delete** | Construit `DISCONNECTED` ; détruit en `ForceDisconnect` ; non copiable. |
| Init | `InitAsServer(socket, remote, localId, remoteId)`, `InitAsClient(socket, remote, localId)` | Socket partagé (non transféré) ; serveur démarre `SYN_RECEIVED`, client `DISCONNECTED`. |
| Cycle | `Connect()` `[nodiscard]`, `Disconnect(reason=nullptr)` `[nodiscard]`, `ForceDisconnect()` | SYN (client) / FIN gracieux / coupure immédiate. |
| Update | `Update(dt)` | `dt` en **secondes** ; retransmissions, heartbeats, timeouts (chaque frame). |
| Envoi | `Send(data, size, ch=RELIABLE_ORDERED)` `[nodiscard]`, `SendObject<T>(obj, ch)` `[nodiscard]` | Octets (fragmentation auto) / objet via `NetSerialize(NkBitWriter&)`. |
| Réception | `OnRawReceived(data, size)`, `DrainReceived(out)` | Traite un datagramme brut / vide la file (ordre de réception). |
| Accès | `GetState`, `IsConnected`, `GetRemotePeerId`, `GetLocalPeerId`, `GetRemoteAddr`, `Stats`, `GetRTT`, `GetPingMs`, `LastActivityAt` `[nodiscard const]` | État, identités, adresse, stats, latence. |
| Callbacks | `onConnected` (`ConnectCb`), `onDisconnected` (`DisconnectCb`) | Appelés **sur le thread réseau** à l'entrée en `ESTABLISHED` / `DISCONNECTED`. |

### `NkConnectionManager` — orchestrateur N pairs (thread I/O dédié)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Vie | `NkConnectionManager()`, `~NkConnectionManager()`, copie **= delete** | Inactif ; détruit en `Shutdown` ; non copiable. |
| Init | `StartServer(port, maxClients=64)` `[nodiscard]`, `Connect(serverAddr, localPort=0)` `[nodiscard]` | Bind serveur / connexion client (`localPort=0` = auto-OS). |
| Arrêt | `Shutdown()` | Déconnecte tous, *join* le thread, libère socket/mémoire. |
| Envoi | `SendTo(peer, data, size, ch)` `[nodiscard]`, `Broadcast(data, size, ch, exclude=Invalid)` `[nodiscard]` | À un pair / à tous (exclusion optionnelle). |
| Réception | `DrainAll(out)` | Copie protégée + vide la file globale (thread jeu). |
| Déco | `Disconnect(peer, reason)`, `DisconnectAll(reason)` | Un pair / tous (à faire avant `Shutdown`). |
| Accès | `IsServer`, `IsRunning`, `ConnectedPeerCount`, `GetLocalPeerId`, `GetConnection`, `GetConnectionStats` `[nodiscard const]` | État, compteurs, accès connexion (**pointeur volatile**), stats (**copie sûre**). |
| Callbacks/Config | `onPeerConnected`, `onPeerDisconnected`, `maxConnections` (≤ 256) | Notifications + plafond configurable. |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages réels. Le trivial est bref,
l'important est traité à fond — avec les domaines du moteur où chaque outil intervient.

### `NkBitWriter` — primitifs et bas niveau

Le constructeur ne fait que mémoriser le pointeur, la capacité et remettre les compteurs à zéro :
**aucune allocation**, le buffer vous appartient. Les écritures primitives sont directes :
`WriteBool` consomme un bit, les `WriteU8/16/32/64` et `WriteI8/16/32` consomment exactement leur
largeur (les signés sont *castés* vers leur équivalent non signé), et `WriteF32` copie les 32 bits
IEEE 754 tels quels via `memcpy` — **sans compression**, à réserver aux cas où la précision pleine
est requise. `WriteU64` se décompose en deux écritures 32 bits, **low puis high** (little-endian).

La brique sous toutes les autres est `WriteBits(v, numBits)` (1 à 32 bits), qui lève `mOverflow` si
la place manque ; `AlignToByte()` ajoute le padding nécessaire pour retomber sur une frontière
d'octet — indispensable avant `WriteBytes`, qui l'appelle d'ailleurs lui-même.

> *Note du header :* les commentaires de `WriteBits` se contredisent sur l'ordre exact des bits
> (LSB d'abord vs bit de poids fort en premier) ; le comportement précis n'est pas déductible du
> header seul. En pratique, peu importe : tant que `NkBitReader` est l'unique lecteur, la symétrie
> est garantie.

Usages par domaine :
- **IO / réseau** — c'est *le* domaine : sérialiser une commande d'entrée, un snapshot, un message
  RPC dans le plus petit paquet possible.
- **Gameplay / IA** — empaqueter un état d'agent (drapeaux booléens d'IA = un bit chacun via
  `WriteBool`) pour réplication.
- **Outils / éditeur** — un format binaire compact (sauvegarde, *replay*) profite de la même
  granularité bit.
- **Threading** — un writer par buffer et par thread : pas de protection interne, l'accès doit être
  exclusif.

### `WriteF32Q`, `WriteVec3fQ`, `WriteInt` — la quantification

C'est ici que se gagne la bande passante. `WriteF32Q(v, minV, maxV, prec)` normalise `v` dans
`[minV, maxV]`, le quantifie au pas `prec`, et l'encode sur le nombre de bits strictement
nécessaires — déterminé par `(maxV - minV) / prec`, en général 10 à 16 bits là où un float brut en
prendrait 32. `WriteVec3fQ` n'est qu'une application par axe. `WriteInt(v, minV, maxV)` est
l'analogue entier : `log2(maxV - minV + 1)` bits, avec une **assertion en debug** si `v` est hors de
l'intervalle promis (un bug de logique, pas une condition réseau).

- **Rendu / animation** — positions et échelles des entités répliquées : un monde de 1 km au cm
  passe de 32 à ~17 bits par axe.
- **Physique** — vitesses bornées (on connaît la vitesse max), facteurs normalisés `[0..1]`.
- **Gameplay** — points de vie `[0..100]`, munitions, index d'arme, compteurs : `WriteInt` les
  ramène à une poignée de bits.
- **Audio** — un volume ou un *pitch* répliqué `[0..1]` ou `[0.5..2]` n'a pas besoin de 32 bits.

La règle non négociable : `minV`, `maxV`, `prec` ne voyagent **pas** dans le buffer ; ils sont une
convention partagée, et la moindre divergence écriture/lecture désynchronise tout.

### `WriteQuatf` — l'orientation comprimée

Un quaternion brut, c'est 128 bits. `WriteQuatf` exploite la contrainte d'unité (`x²+y²+z²+w²=1`)
pour omettre la plus grande composante (encodée par un index sur 2 bits) et quantifier les trois
autres sur ~10 bits — soit **~32 bits**, une division par quatre. C'est l'outil de réplication des
rotations par excellence.

- **Animation / rendu** — orientation d'un personnage, d'une tourelle, d'une caméra distante.
- **Physique** — orientation d'un corps rigide synchronisé.
- **Gameplay** — direction de visée d'un joueur distant.

Contrainte absolue : **le quaternion doit être normalisé avant l'appel**. Un quaternion non unitaire
brise l'hypothèse de reconstruction et produit une rotation fausse côté lecture.

### `WriteString`, `WriteBytes`, `AlignToByte` — données variables

`WriteString` écrit un `uint16` de longueur suivi des octets (`s == nullptr` → longueur 0), et
**tronque silencieusement** au-delà de `maxLen` (256 par défaut) : à utiliser pour des pseudos, du
chat, des identifiants courts, jamais pour du binaire. `WriteBytes` recopie un blob brut, mais
appelle d'abord `AlignToByte()` (un blob d'octets doit commencer sur une frontière d'octet) et lève
`mOverflow` en cas de dépassement.

- **IO / réseau** — noms de joueurs, messages de chat, charges utiles déjà sérialisées ailleurs.
- **Outils / éditeur** — embarquer un sous-bloc binaire (un *chunk* déjà encodé) dans un paquet.

### `NkBitWriter` — bilan : `BitsWritten`, `BytesWritten`, `IsOverflowed`

`BitsWritten()` donne la position du curseur, `BytesWritten()` la taille **réelle à envoyer**
(`(bits + 7) / 8`, padding compris) — c'est elle qu'on passe à la couche socket. `IsOverflowed()`
est le verdict : `true` si une écriture a manqué de place. Comme le flag est **collant**, un seul
test en fin de séquence suffit à savoir si tout le paquet est valide. Dans tous les domaines, le
schéma est le même : écrire, tester `IsOverflowed()`, envoyer `BytesWritten()` octets *seulement* si
le test passe.

### `NkBitReader` — lecture symétrique

Le constructeur stocke la taille **en bits** (`size × 8`). Chaque `Read*` est le miroir strict du
`Write*` correspondant, avec la même largeur ; tous sont `[[nodiscard]]` (sauf `ReadString`, qui
écrit via référence). La différence philosophique avec le writer est dans la gestion d'erreur :
plutôt que d'ignorer, le reader **renvoie une valeur par défaut** quand le buffer est épuisé — 0,
`false` pour `ReadBool`, `minV` pour les versions quantifiées, le quaternion identité pour
`ReadQuatf`, `{0,0,0}` pour `ReadVec3f` — et lève son `mOverflow` collant. `ReadU64` relit low puis
high (little-endian). `ReadF32` refait le `memcpy` IEEE 754.

`ReadQuatf` mérite une note : il reconstruit la composante omise par `w = ±√(1 − x² − y² − z²)`, le
signe étant déduit de la plus grande composante ; le résultat **peut nécessiter une
renormalisation** avant usage. `ReadBits` (1 à 32 bits) est la base de toutes les lectures et lève
`mOverflow` si le buffer est épuisé ou `numBits` invalide ; `AlignToByte` saute le padding, en
miroir du writer (obligatoire avant `ReadBytes`).

- **IO / réseau** — désérialiser snapshots, commandes, RPC reçus.
- **Gameplay** — relire l'état d'entités distantes pour mettre à jour la simulation locale.
- **Threading** — un reader par buffer/thread, accès exclusif (aucune protection interne).

La discipline de symétrie est totale : **même ordre, mêmes `minV/maxV/prec`**. Toute divergence
décale tout le flux et désynchronise client et serveur.

### `NkBitReader` — bilan : `BitsRead`, `BytesRead`, `BitsLeft`, `IsOverflowed`, `IsEmpty`

`BitsRead()`/`BytesRead()` mesurent ce qui a été consommé ; `BitsLeft()` (`mSize − mBitPos`) ce qui
reste — et vaut 0 à la fin. `IsEmpty()` est le raccourci `BitsLeft() == 0`. La vérification de
cohérence canonique d'un paquet bien formé est : tout relire, puis vérifier `BitsLeft() == 0` **et**
`!IsOverflowed()`. Si du rab subsiste ou si le flag est levé, le paquet est rejeté.

### `NkConnectionState` et `NkConnStateStr`

L'enum décrit les six étapes de la connexion (`DISCONNECTED`, `SYN_SENT`, `SYN_RECEIVED`,
`ESTABLISHED`, `DISCONNECTING`, `TIMED_OUT`). `NkConnStateStr` — **défini inline** dans le header —
renvoie un libellé statique correspondant ("Disconnected"… "TimedOut", "Unknown" par défaut). La
chaîne est statique : on l'affiche, on ne la libère jamais.

- **Outils / éditeur** — afficher l'état de chaque pair dans un panneau de debug réseau.
- **IO / réseau** — journaliser les transitions (`NkConnStateStr(old) → NkConnStateStr(new)`).
- **UI / 2D** — un indicateur de connexion dans le HUD ("Connexion…", "En ligne").

### `NkConnectionStats`, `NkReceiveMsg`, `NkNetworkMetrics`

`NkConnectionStats` est un POD de mesures par connexion (octets/paquets envoyés et reçus,
retransmissions, RTT, *jitter*, taux de perte `[0..1]`, instant de connexion). Lecture seule côté
appelant et **sans protection interne** : on en obtient une copie sûre via
`NkConnection::Stats()` ou `NkConnectionManager::GetConnectionStats()`.

- **Outils / éditeur** — graphes de RTT/perte, diagnostic de lag.
- **Gameplay** — adapter le taux d'envoi ou activer une compensation de latence selon le RTT mesuré.

`NkReceiveMsg` est un message reçu : un buffer **inline** (`data[kNkMaxPayloadSize]`, pas
d'allocation heap), sa taille, l'émetteur (`from`), le canal et l'instant de réception. C'est ce que
remplissent `DrainReceived` et `DrainAll` — on l'itère côté jeu pour appliquer les messages.

`NkNetworkMetrics` agrège des statistiques globales (connexions actives/pic, octets, latence
moyenne, perte en %, erreurs, timeouts). **Attention** : c'est un type *interne* — il est bien
membre privé du manager (`mMetrics`), mais les accesseurs publics (`GetMetrics`/`ResetMetrics`) sont
**commentés** dans le header, donc **non disponibles**. Ne comptez pas dessus comme API.

### `NkConnection` — construction, init, cycle de vie

Le constructeur par défaut place la connexion en `DISCONNECTED` ; le destructeur appelle
`ForceDisconnect()` si elle est encore active. La classe **n'est pas copiable** (copie et affectation
`= delete`). On l'amorce par `InitAsServer` (le manager l'appelle après un *accept* : départ en
`SYN_RECEIVED`) ou `InitAsClient` (départ en `DISCONNECTED`, prêt pour `Connect`) — dans les deux cas
le `NkSocket*` est **partagé, pas transféré**.

Le cycle utile : `Connect()` (côté client) envoie le SYN et passe `SYN_SENT` ; `Disconnect(reason)`
envoie un FIN, passe `DISCONNECTING`, et finit en `DISCONNECTED` par timeout ; `ForceDisconnect()`
coupe net, sans paquet de fin. Les trois sont à piloter depuis le thread réseau.

### `NkConnection::Update` — le battement de cœur

`Update(dt)` — `dt` en **secondes** — est le moteur de la connexion : il gère les retransmissions de
la couche fiable, émet les *heartbeats* (PING) qui maintiennent le lien vivant, et déclenche les
timeouts vers `TIMED_OUT` quand le pair se tait trop longtemps. À appeler **chaque frame** depuis le
thread réseau. Sans `Update`, une connexion établie finit par mourir de timeout et rien n'est jamais
retransmis.

### `NkConnection::Send` et `SendObject` — émettre

`Send(data, size, channel)` envoie des octets bruts ; si `size > kNkMaxPayloadSize` (1380 octets), la
**fragmentation est automatique** (jusqu'à `kNkMaxPayloadSize × kNkMaxFragments`). Le canal par
défaut est `NK_NET_CHANNEL_RELIABLE_ORDERED` ; cette méthode est thread-safe *uniquement* si le
moteur est compilé `NKNET_THREAD_SAFE`. `SendObject<T>(obj, ch)` est le confort typé : il sérialise
un objet via sa méthode `NetSerialize(NkBitWriter&)` dans un buffer **pile** de `kNkMaxPayloadSize` —
l'objet doit donc tenir sous cette limite.

- **IO / réseau** — c'est le canal de sortie de toute la logique réseau (snapshots, RPC).
- **Gameplay** — `SendObject` pour répliquer un component sérialisable directement (couplé à
  `NkBitWriter` pour la compression).
- **Audio** — déclencher un son chez les pairs (un petit message d'événement fiable).

### `NkConnection::OnRawReceived` et `DrainReceived` — recevoir

`OnRawReceived(data, size)` traite un datagramme brut : les paquets **système** (SYN/ACK/PING/FIN)
sont consommés en interne pour faire vivre la machine à états, et seules les **données applicatives**
sont mises en file (`mIncomingQueue`). C'est le manager (`DispatchPacket`) qui l'appelle ; on le
touche rarement directement. `DrainReceived(out)` extrait *et vide* cette file dans un vecteur de
`NkReceiveMsg`, dans l'**ordre de réception** (ce qui respecte l'ordre pour les canaux *Ordered*) —
à appeler **après** `Update()`.

### `NkConnection` — accesseurs et callbacks

`GetState`/`IsConnected` (vrai si `ESTABLISHED`), les identités (`GetRemotePeerId`,
`GetLocalPeerId`), l'adresse (`GetRemoteAddr`), les stats (`Stats`), la latence (`GetRTT` en ms
moyen, délégué à `NkReliableUDP` ; `GetPingMs` en est l'arrondi entier) et `LastActivityAt` — tous
`[[nodiscard]] const noexcept`. Les deux callbacks publics, `onConnected(peer)` (`ConnectCb`) et
`onDisconnected(peer, reason)` (`DisconnectCb`), sont déclenchés aux transitions vers `ESTABLISHED`
et `DISCONNECTED` — **depuis le thread réseau**. C'est le point de vigilance : ne manipulez pas la
scène ni l'UI directement dedans ; postez un événement vers le thread jeu.

- **Gameplay** — `onConnected` pour faire apparaître l'avatar du joueur, `onDisconnected` pour le
  retirer proprement.
- **UI / 2D** — alimenter un indicateur d'état réseau (mais via le thread UI, pas dans le callback).

### `NkConnectionManager` — construction, init, arrêt

Le constructeur laisse le manager inactif ; le destructeur appelle `Shutdown()` si le thread tourne ;
la classe est **non copiable**. `StartServer(port, maxClients)` *bind* l'UDP sur `Any(port)` et lance
le thread réseau ; `Connect(serverAddr, localPort)` crée la connexion client et son thread
(`localPort = 0` → port auto-assigné par l'OS). Les deux renvoient un `NkNetResult` et sont
`[[nodiscard]]`. `Shutdown()` déconnecte gracieusement tous les pairs, *join* le thread, et libère
socket et mémoire — à coupler en général avec `DisconnectAll()` juste avant.

### `NkConnectionManager` — envoi, réception, déconnexion

`SendTo(peer, data, size, ch)` cible un pair (renvoie `NK_NET_NOT_CONNECTED` s'il est absent ou non
établi) ; `Broadcast(data, size, ch, exclude)` envoie à tous sauf un éventuel exclu
(`NkPeerId::Invalid()` par défaut = personne d'exclu), et renvoie `NK_NET_OK` si tous réussissent,
sinon le premier code d'erreur. `DrainAll(out)` fait une **copie protégée par mutex** de la file
globale puis la vide — c'est l'unique point de réception côté thread jeu. `Disconnect(peer, reason)`
coupe un pair, `DisconnectAll(reason)` tous (à faire avant `Shutdown`).

- **IO / réseau** — `Broadcast` pour diffuser un snapshot serveur à tous les clients.
- **Gameplay** — `SendTo` pour une réponse RPC ciblée, `DisconnectAll` au retour au menu.

### `NkConnectionManager` — accesseurs, callbacks, config

`IsServer`/`IsRunning` donnent le mode et l'activité ; `ConnectedPeerCount` compte les pairs
`ESTABLISHED` ; `GetLocalPeerId` l'identité locale. Le piège majeur est `GetConnection(peer)` : il
renvoie un **pointeur potentiellement invalidé l'instant d'après** par le thread réseau
(use-after-free). Pour toute lecture de stats, utilisez plutôt `GetConnectionStats(peer, out)`, qui
copie sous mutex et renvoie `true` si le pair existe. Les callbacks `onPeerConnected` /
`onPeerDisconnected` (mêmes types que `NkConnection`) notifient l'arrivée et le départ des pairs ;
`maxConnections` (≤ `kNkMaxConnections` = 256) plafonne le nombre de connexions.

> Rappel : `GetMetrics()` et `ResetMetrics()` sont **commentés** dans le header → **non
> disponibles**, malgré la présence du membre interne `mMetrics`.

### Constantes et pièges transverses

Plusieurs constantes structurent ces API mais sont **déclarées ailleurs** (`NkNetDefines.h` /
`NkReliableUDP.h`) : `kNkMaxPayloadSize` (1380 octets), `kNkMaxFragments`, `kNkMaxConnections`
(256), et — d'après la doc — `kNkHeartbeatIntervalMs` (250 ms) et `kNkTimeoutMs` (10 000 ms). À
retenir absolument :

- **`mOverflow` collant** (writer et reader) : un seul test `IsOverflowed()` en fin de séquence.
- **Symétrie stricte** : même ordre + mêmes `minV/maxV/prec` entre écriture et lecture.
- **Buffers non possédés** : le writer/reader ne libère rien ; un objet par buffer **et** par thread.
- **`NkConnection` non thread-safe**, callbacks **sur le thread réseau** → différer vers le gameplay.
- **`GetConnection()` volatile** → `GetConnectionStats()` pour lire en sécurité.
- Destructeurs auto-nettoyants (`ForceDisconnect` / `Shutdown`) : pas de fuite si l'on oublie.

---

### Exemple récapitulatif

```cpp
#include "NKNetwork/Protocol/NkBitStream.h"
#include "NKNetwork/Protocol/NkConnection.h"
using namespace nkentseu::net;   // NkVec3f / NkQuatf accessibles (using math interne)

// 1) Empaqueter un snapshot d'entité dans le plus petit paquet possible.
uint8 buffer[kNkMaxPayloadSize];
NkBitWriter w(buffer, sizeof(buffer));
w.WriteU16(entityId);
w.WriteVec3fQ(transform.position, -1024.f, 1024.f, 0.01f);   // ~17 bits/axe
w.WriteQuatf(transform.rotation.Normalized());               // ~32 bits
w.WriteInt(health, 0, 100);                                  // 7 bits
w.WriteBool(isAlive);                                        // 1 bit

// 2) Démarrer un serveur et diffuser le snapshot s'il est valide.
NkConnectionManager mgr;
mgr.onPeerConnected = [](NkPeerId p){ /* poster vers le thread jeu */ };
mgr.StartServer(7777, 32);
if (!w.IsOverflowed())
    mgr.Broadcast(buffer, w.BytesWritten());

// 3) Côté jeu : drainer la réception puis désérialiser (même ordre, mêmes params).
NkVector<NkReceiveMsg> inbox;
mgr.DrainAll(inbox);
for (const NkReceiveMsg& msg : inbox) {
    NkBitReader r(msg.data, msg.size);
    uint16  id    = r.ReadU16();
    NkVec3f pos   = r.ReadVec3fQ(-1024.f, 1024.f, 0.01f);
    NkQuatf rot   = r.ReadQuatf();
    int32   hp    = r.ReadInt(0, 100);
    bool    alive = r.ReadBool();
    if (r.IsOverflowed()) continue;          // paquet mal formé → ignoré
    ApplyState(id, pos, rot, hp, alive);
}
```

---

[← Index NKNetwork](README.md) · [Récap NKNetwork](../NKNetwork.md) · [Couche System](../README.md)
