# Le réseau de haut niveau

> Couche **System** · NKNetwork · Au-dessus des sockets bruts : les **appels de procédure
> distants** (`NkRPCRouter`), le **lobby / matchmaking** (`NkLobby`, `NkSession`,
> `NkMatchmaker`, `NkDiscovery`) et le **client HTTP/HTTPS** (`NkHTTPClient`, `NkLeaderboard`).

Une fois qu'on sait envoyer des octets d'une machine à une autre (la couche transport, sockets,
connexions), il reste tout le **gros du travail** : comment appeler une fonction sur le serveur
depuis un client sans tout sérialiser à la main ? comment réunir huit joueurs dans une partie,
gérer les prêts/non-prêts, les expulsions, le passage en jeu ? comment trouver une partie publique
qui correspond à mon niveau et à mon ping ? et comment parler à un service web REST pour soumettre
un score ou télécharger un patch ? Ces trois besoins — **RPC**, **lobby/matchmaking** et **HTTP** —
sont précisément ce que cette famille de haut niveau résout. On ne touche plus aux paquets : on
parle en *fonctions distantes*, en *sessions de joueurs* et en *requêtes web*.

Tout vit dans le même namespace et chaque header tire la base commune `NkNetworkApi.h`. Il n'y a
**pas** de header parapluie : on inclut directement le sous-système voulu.

- **Namespace** : `nkentseu::net`
- **Headers** :
  - `#include "NKNetwork/RPC/NkRPC.h"`
  - `#include "NKNetwork/Lobby/NkLobby.h"`
  - `#include "NKNetwork/HTTP/NkHTTPClient.h"`

Une règle traverse les trois : la quasi-totalité des opérations qui *peuvent échouer* sont
`[[nodiscard]]` et renvoient un `NkNetResult` (`NK_NET_OK`, `NK_NET_AUTH_FAILED`,
`NK_NET_TIMEOUT`…) — **ignorer ce retour est une erreur**. Et tout est **zéro-STL** : tableaux
fixes (256 RPC, 64 joueurs), aucune allocation cachée sur les chemins chauds.

---

## Les appels de procédure distants : `NkRPCRouter`

Le problème que résout le RPC est vieux comme le réseau : sur le client, on aimerait écrire
`server.SpawnProjectile(pos, dir)` comme un appel normal, et que cette fonction s'exécute sur le
serveur. Sans RPC, il faut sérialiser à la main un identifiant de message, empaqueter les
arguments, les envoyer, les recevoir, les désempaqueter, et router vers la bonne fonction. Le
`NkRPCRouter` automatise tout ce circuit : on **enregistre** un handler sous un nom, on **appelle**
par ce nom, et le routeur s'occupe du hash, de la sérialisation et du dispatch.

Trois directions existent, décrites par `NkRPCType` : `NK_SERVER_RPC` (client → serveur, le cas le
plus courant — « je demande au serveur de faire X »), `NK_CLIENT_RPC` (serveur → un client précis,
ciblé par son `NkPeerId`), et `NK_MULTI_CAST_RPC` (serveur → **tous** les pairs, pour diffuser un
événement de jeu). À chaque RPC on attache aussi une **garantie de livraison** (`NkRPCReliability`)
fixée **une fois pour toutes à l'enregistrement** : `NK_RELIABLE` (arrive, mais sans ordre garanti),
`NK_RELIABLE_ORD` (arrive **et** dans l'ordre) ou `NK_UNRELIABLE` (best-effort UDP, pour ce qui
périme vite comme une position).

```cpp
NkRPCRouter router;
router.SetConnectionManager(&conn);   // requis AVANT tout Call*

uint32 id = router.Register(
    "Weapon::Fire", NkRPCType::NK_SERVER_RPC,
    [](NkPeerId caller, NkBitReader& args) {
        math::NkVec3f origin; /* le handler désérialise lui-même */
        // ... lire args, exécuter le tir côté serveur
    },
    NkRPCReliability::NK_RELIABLE_ORD);

// côté client :
NkNetResult r = router.CallServer("Weapon::Fire", origin);   // [[nodiscard]]
```

Le point clé, contre-intuitif : **le handler désérialise lui-même** ses arguments via le
`NkBitReader& args` qu'il reçoit. Le routeur ne connaît pas le type des paramètres ; il ne fait que
transporter un flux d'octets. À l'envoi, en revanche, les arguments variadiques sont sérialisés
automatiquement — mais **seulement** pour un jeu de types fixe : `bool`, `uint8`/`uint16`/`uint32`,
`int32`, `float32`, `math::NkVec3f`, `const char*` et `NkString`. Pas de 64 bits, pas de `double`,
pas d'`int16` : ce ne sont **pas** des arguments sérialisables nativement.

Ce n'est **pas** thread-safe et ce n'est **pas** extensible à l'infini : la table est fixe à
**256 RPC** (au-delà, `Register` renvoie `0`), le lookup est `O(n)` (n ≤ 256), et il faut faire les
enregistrements au démarrage puis les `Call*`/`OnRPCReceived` sur le **thread réseau**.

> **En résumé.** `NkRPCRouter` = appeler une fonction distante par son nom. `Register` au boot
> (fixe `type` + `reliability`), puis `CallServer`/`CallClient`/`Multicast` (tous `[[nodiscard]]`).
> Le handler désérialise lui-même via `NkBitReader`. Types sérialisables limités, table de 256,
> non thread-safe, lookup `O(n)`.

---

## Le lobby et la session : `NkSession`, `NkLobby`

Réunir des joueurs avant une partie est un **cycle de vie** : on ouvre une session, des joueurs
arrivent, se déclarent prêts, l'hôte lance, on charge, on joue, on termine. `NkSession` modélise
exactement cette machine à états (`NkSessionState` : `IDLE → LOBBY → LOADING → IN_PROGRESS →
ENDED`), et range ses joueurs dans un **tableau fixe** (`kMaxPlayers = 64`) — aucune allocation
dynamique par joueur. Chaque joueur est un `NkPlayerInfo` (pseudo, ELO, ping, équipe, prêt/non,
hôte/non), et la partie est décrite par un `NkSessionConfig` (nom, carte, mode, min/max joueurs,
privé/ranked, mot de passe, plage d'ELO, ping max, région).

`NkSession::Create(cfg)` fait de vous l'**hôte** et passe en `LOBBY` ; `Join(addr, password)`
rejoint en client (et renvoie `NK_NET_AUTH_FAILED` si le mot de passe est faux). Les actions de
l'hôte — `Start()` (vérifie `minPlayers` et que tout le monde est prêt), `End()` (calcule l'ELO si
ranked), `Kick(peer)` — sont **silencieusement ignorées** chez un non-hôte : on teste toujours
`IsHost()` d'abord. Côté client, `SetReady(bool)` se diffuse aux autres, et `Leave()` quitte
proprement (retour à `IDLE`, thread-safe). Trois callbacks publics, assignables directement,
préviennent des changements : `onPlayerJoined`, `onPlayerLeft`, `onStateChanged`.

```cpp
NkSession& s = NkLobby::Global().GetSession();
s.onPlayerJoined  = [](const NkPlayerInfo& p) { /* afficher le slot */ };
s.onStateChanged  = [](NkSessionState st) { /* MAJ écran : NkSessionStateStr(st) */ };

NkSessionConfig cfg;
// cfg.maxPlayers, cfg.mapName, cfg.isRanked, ...
NkNetResult r = NkLobby::Global().CreateSession(cfg);   // [[nodiscard]]
```

`NkLobby` est la **façade** au-dessus de `NkSession` : un singleton (`NkLobby::Global()`) qui
possède une session, y ajoute le **chat** (`SendChatMessage`, callback `onChatMessage`, message
tronqué à 256 caractères) et les **réglages d'hôte** `SetMap` / `SetGameMode` / `SetTeam`. Détail
qui surprend : ces setters de configuration sont des méthodes de **`NkLobby`**, **pas** de
`NkSession` — `s.SetMap(...)` n'existe pas. Et `NkSession` **contient** son `NkConnectionManager`
par valeur (objet possédé, pas un pointeur emprunté comme dans le routeur RPC).

Une incohérence à connaître : le tableau de joueurs est borné à **64** alors que
`NkSessionConfig::maxPlayers` peut monter jusqu'à 256 — ne configurez pas au-delà de 64.

> **En résumé.** `NkSession` = la machine à états d'une partie (IDLE→LOBBY→…→ENDED), 64 joueurs
> max, callbacks `onPlayer*`/`onStateChanged`. `NkLobby::Global()` = la façade singleton qui ajoute
> chat et **réglages d'hôte** (`SetMap`/`SetGameMode`/`SetTeam`, host-only). Vérifiez `IsHost()`
> avant toute action d'hôte ; callbacks reçus sur le thread réseau.

---

## Trouver une partie : `NkMatchmaker`, `NkDiscovery`

Reste à **remplir** ce lobby. Deux mondes : Internet et le réseau local. `NkMatchmaker` cherche en
ligne, de façon **asynchrone** : `SearchAsync` part avec un `SearchParams` (mode de jeu, mon ELO,
ping max, région, nombre de joueurs, ranked ou non), ne bloque pas, et rappelle `onFound` pour
**chaque** candidat trouvé (un `SearchResult` scoré sur `[0..1]`), ou `onError` en cas d'échec.
`CancelSearch()` interrompt (et déclenche `onError(NK_NET_TIMEOUT)` si une recherche tournait).
Côté serveur, `RegisterServer(cfg, url)` s'annonce au matchmaker avec un heartbeat périodique, et
`UnregisterServer()` retire l'annonce.

`NkDiscovery` couvre le **LAN** par broadcast UDP, et c'est une API **entièrement statique** :
`Broadcast(info, port)` envoie un `ServerInfo` à `255.255.255.255` (à répéter ~toutes les 2 s), et
`Listen(timeoutMs, out, port)` — **bloquant** pendant `timeoutMs` — remplit un `NkVector<ServerInfo>`
dédupliqué par IP+port. C'est le « parties détectées sur votre réseau » des jeux en LAN.

```cpp
// LAN : côté serveur on s'annonce, côté client on écoute.
NkDiscovery::ServerInfo me;            // me.name, me.maxPlayers, ...
(void)NkDiscovery::Broadcast(me);      // à appeler périodiquement

NkVector<NkDiscovery::ServerInfo> found;
(void)NkDiscovery::Listen(1500, found);   // bloque 1,5 s, puis 'found' est rempli
```

> **En résumé.** `NkMatchmaker` = recherche **en ligne** asynchrone (`SearchAsync` rappelle
> `onFound` plusieurs fois, `RegisterServer` côté hôte). `NkDiscovery` = découverte **LAN**
> statique (`Broadcast` à répéter, `Listen` **bloquant**). Les deux renvoient des `NkNetResult`
> `[[nodiscard]]` sur les opérations qui échouent.

---

## Parler au web : `NkHTTPClient`, `NkLeaderboard`

Un jeu moderne dialogue avec des **services web** : authentification, achats, classements,
téléchargement de patches. `NkHTTPClient` est un client HTTP/HTTPS complet, **synchrone et
asynchrone** (pool de threads), thread-safe. On construit une requête `NkHTTPRequest` (URL, méthode
`NkHTTPMethod`, en-têtes, corps, timeout, redirections) et on lit une réponse `NkHTTPResponse`
(code, texte, en-têtes, corps, erreur, durée).

La règle **critique** est dans la lecture de la réponse : il faut tester `HasError()` **avant**
d'interpréter `statusCode`. Pourquoi ? Parce qu'une panne réseau (DNS, TLS, coupure) ne produit
**pas** de code HTTP — `statusCode` reste à `0` et c'est `error` qui est renseigné. Les helpers
`IsOK()` / `IsRedirect()` / `IsClientError()` / `IsServerError()` classent ensuite le code par
tranche, et `GetHeader(key)` lit un en-tête (insensible à la casse).

```cpp
NkHTTPClient http;

// synchrone (BLOQUANT — jamais sur le thread UI) :
NkHTTPResponse res = http.Get("https://api.exemple.com/status");
if (res.HasError())      { /* panne réseau : lire res.error */ }
else if (res.IsOK())     { /* res.body est exploitable */ }

// asynchrone — renvoie un ID pour Cancel, callback sur le thread réseau :
uint32 reqId = http.SendAsync(req, [](const NkHTTPResponse& r) { /* ... */ });
```

Les méthodes synchrones (`Send`, `Get`, `Post`) sont **bloquantes et `[[nodiscard]]`** : à ne
jamais appeler sur le thread d'interface. L'asynchrone (`SendAsync`, `DownloadFile`) renvoie un
**ID** `[[nodiscard]]` pour `Cancel`, et invoque son callback depuis le **thread réseau** (à
ramener manuellement vers le thread jeu). Côté requête, `SetJSON` **écrase le corps** — donc on
l'appelle en dernier — et l'on n'active **jamais** `verifySSL=false` en production.

`NkLeaderboard` est une commodité bâtie **sur** `NkHTTPClient` : un client REST de classement.
`Configure(baseUrl, apiKey)` (la clé part en en-tête `X-API-Key`), `SubmitScore(...)` (POST JSON
vers `/submit`), `FetchTop(count, cb)` (GET `/top?count=N`, liste triée par rang croissant —
attention, le callback reçoit une **référence non-const** au vecteur), et
`FetchPlayerRank(name, cb)` (rang `0` = non classé).

> **En résumé.** `NkHTTPClient` = HTTP/HTTPS sync **et** async thread-safe. Toujours `HasError()`
> **avant** `statusCode` (`0` = pas de réponse). Sync = bloquant `[[nodiscard]]`, jamais sur l'UI ;
> async = ID pour `Cancel`, callback sur thread réseau. `SetJSON` écrase le corps (en dernier),
> `verifySSL` reste `true` en prod. `NkLeaderboard` = client de classement REST clé en main.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit. Les opérations `[[nodiscard]]` renvoyant `NkNetResult` sont
notées `[NkNetResult]`.

### RPC — `NkRPC.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkRPCType` (`NK_SERVER_RPC`, `NK_CLIENT_RPC`, `NK_MULTI_CAST_RPC`) | Direction/cible d'exécution. |
| Enum | `NkRPCReliability` (`NK_RELIABLE`, `NK_RELIABLE_ORD`, `NK_UNRELIABLE`) | Garantie de livraison. |
| Données | `NkRPCDescriptor` (`name`, `id`, `type`, `reliability`, `handler`, `HandlerFn`) | Métadonnées d'un handler enregistré. |
| Construction | `NkRPCRouter()` · `~NkRPCRouter()` | Routeur (table statique de 256, rien à libérer). |
| Setup | `SetConnectionManager(cm)` | Brancher le gestionnaire de connexions (**requis** avant `Call*`). |
| Enregistrement | `Register(name, type, handler, rel)` `[uint32]` | Enregistre un RPC, renvoie l'ID hashé (**0 = table pleine**). |
| Appel | `CallServer(name, args…)` `[NkNetResult]` | Client → serveur. |
| Appel | `CallClient(target, name, args…)` `[NkNetResult]` | Serveur → un client. |
| Appel | `Multicast(name, args…)` `[NkNetResult]` | Serveur → tous les pairs. |
| Réception | `OnRPCReceived(caller, data, size)` | Dispatch d'un paquet RPC brut entrant. |
| Macros | `NK_RPC_SERVER/CLIENT/MULTICAST(Class, Func, …)` | Déclarent une méthode + une constante de nom. |

### Lobby / Matchmaking — `NkLobby.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkSessionState` (`IDLE`/`LOBBY`/`LOADING`/`IN_PROGRESS`/`ENDED`) | État du cycle de vie d'une partie. |
| Libre | `NkSessionStateStr(s)` `[O(1)]` | Nom lisible d'un état. |
| Données | `NkPlayerInfo` (`peerId`, `displayName`, `elo`, `ping`, `isHost`, `isReady`, `team`, `slot`) | Fiche d'un joueur. |
| Données | `NkSessionConfig` (`name`, `mapName`, `gameMode`, `min/maxPlayers`, `isPrivate`, `isRanked`, `password`, `port`, `min/maxELO`, `maxPingMs`, `region`) | Paramètres d'une partie. |
| Session | `NkSession::Create(cfg)` `[NkNetResult]` | Devenir hôte (→ LOBBY). |
| Session | `Join(addr, password)` `[NkNetResult]` | Rejoindre en client. |
| Session | `Leave()`, `Start()` `[NkNetResult]`, `End()`, `Kick(peer, reason)` | Quitter / lancer / terminer / expulser. |
| Session | `SetReady(ready)` | Se déclarer prêt (diffusé). |
| Session (lecture) | `GetState`, `IsHost`, `IsInProgress`, `GetPlayerCount`, `GetConfig`, `FindPlayer(peer)` `[O(n)]`, `GetConnMgr` | Inspecter la session. |
| Session (callbacks) | `onPlayerJoined`, `onPlayerLeft`, `onStateChanged` | Notifications (thread réseau). |
| Lobby | `NkLobby::Global()` | Singleton de façade. |
| Lobby | `CreateSession`/`JoinSession` `[NkNetResult]`, `LeaveSession`, `GetSession` | Délèguent à la session interne. |
| Lobby | `SendChatMessage(msg)`, `onChatMessage`, `ChatMessage` | Chat de lobby (256 chars max). |
| Lobby | `SetMap`, `SetGameMode`, `SetTeam(peer, team)` | Réglages d'hôte (**sur `NkLobby`**, host-only). |
| Matchmaking | `NkMatchmaker::SearchAsync(params, onFound, onError, timeout)` | Recherche en ligne **non bloquante**. |
| Matchmaking | `CancelSearch`, `IsSearching`, `RegisterServer` `[NkNetResult]`, `UnregisterServer` `[NkNetResult]` | Annuler / état / s'annoncer / se retirer. |
| Matchmaking | `SearchParams`, `SearchResult` | Critères de recherche / candidat trouvé. |
| Découverte LAN | `NkDiscovery::Broadcast(info, port)` `[NkNetResult]`, `Listen(timeoutMs, out, port)` `[NkNetResult]` | API **statique** : annoncer / écouter (Listen **bloquant**). |
| Découverte LAN | `ServerInfo` | Description d'un serveur LAN. |

### HTTP — `NkHTTPClient.h`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkHTTPMethod` (`GET`/`POST`/`PUT`/`PATCH`/`DELETE`/`HEAD`) | Verbe HTTP. |
| Données | `NkHTTPHeader` (`key`, `value`) | Un en-tête. |
| Requête | `NkHTTPRequest` (`url`, `method`, `headers`, `body`, `timeoutMs`, `followRedirects`, `maxRedirects`) | Requête à envoyer. |
| Requête (helpers) | `SetJSON`, `SetFormData`, `AddHeader`, `SetBearerToken`, `SetBasicAuth` | Renseigner corps + en-têtes d'auth. |
| Réponse | `NkHTTPResponse` (`statusCode`, `statusText`, `headers`, `body`, `error`, `timeMs`) | Réponse reçue. |
| Réponse (inspection) | `IsOK`, `IsRedirect`, `IsClientError`, `IsServerError`, `HasError`, `GetHeader(key)` | Classer le code / détecter une panne / lire un en-tête. |
| Client | `NkHTTPClient()` · `~NkHTTPClient()` | Client (le destructeur annule l'async). |
| Client (config) | `Configure(cfg)`, `AddDefaultHeader`, `SetDefaultBearerToken`, `Config` | Réglages globaux (UA, SSL, threads…). |
| Client (sync) | `Send(req)` `[NkHTTPResponse]`, `Get(url)`, `Post(url, json)` | Envoi **bloquant** `[[nodiscard]]`. |
| Client (async) | `SendAsync(req, cb)` `[uint32]`, `Cancel(id)`, `CancelAll`, `WaitAll(timeout)`, `PendingCount` | Envoi non bloquant + gestion (ID `[[nodiscard]]`). |
| Client (fichier) | `DownloadFile(url, dest, onProgress, onComplete)` `[uint32]` | Téléchargement async (écriture atomique). |
| Client (statics) | `URLEncode`, `URLDecode`, `Base64Encode` | Utilitaires d'encodage. |
| Leaderboard | `NkLeaderboardEntry` (`rank`, `score`, `playerName`, `extraData`) | Une ligne de classement. |
| Leaderboard | `NkLeaderboard::Configure`, `SubmitScore`, `FetchTop`, `FetchPlayerRank` | Client REST de classement. |

---

## Référence complète

Chaque élément est repris ici en détail — complexité, comportement, et usages dans les différents
domaines du moteur (rendu, ECS, physique, animation, gameplay/IA, audio, UI/2D, IO/réseau, GPU,
threading, outils). Les éléments triviaux sont décrits brièvement ; les structurants, à fond.

### `NkRPCType` et `NkRPCReliability` — le contrat d'un RPC

Avant toute autre chose, un RPC se définit par **deux décisions** prises une fois pour toutes à
l'enregistrement. `NkRPCType` dit *qui exécute* : `NK_SERVER_RPC` (l'autorité fait foi — le client
demande, le serveur valide et applique), `NK_CLIENT_RPC` (le serveur pilote un client précis), et
`NK_MULTI_CAST_RPC` (le serveur informe tout le monde). `NkRPCReliability` dit *avec quelle
garantie* : `NK_RELIABLE` (arrive, ordre indifférent), `NK_RELIABLE_ORD` (arrive et en ordre),
`NK_UNRELIABLE` (UDP best-effort).

- **Gameplay / IA** : un input joueur (`NK_SERVER_RPC` + `NK_RELIABLE_ORD`) doit arriver dans
  l'ordre ; un événement ponctuel (mort, ramassage) se diffuse en `NK_MULTI_CAST_RPC`.
- **Rendu / animation** : un déclencheur d'effet (explosion, *muzzle flash*) tolère `NK_UNRELIABLE`
  — s'il rate, le suivant arrive ; inutile de le retransmettre.
- **Audio** : « jouer ce son une fois pour tous » est un multicast best-effort typique.
- **Physique** : une position périodique part en `NK_UNRELIABLE` (elle périme à la frame suivante) ;
  un téléport autoritatif part en `NK_RELIABLE`.

Le piège fondamental : la fiabilité se choisit **à `Register`, jamais à l'appel** — on ne peut pas
rendre un appel donné soudain fiable.

### `NkRPCDescriptor` — la fiche d'un handler

Structure de métadonnées (exportée) : `name` (chaîne fixe de 128, format `"Classe::Fonction"`),
`id` (hash 32 bits du nom, clé de lookup), `type`, `reliability`, et `handler` de type
`HandlerFn = NkFunction<void(NkPeerId caller, NkBitReader& args)>`. Le point capital est la
signature du handler : il reçoit l'**émetteur** (`caller`) et un **flux d'arguments**
(`NkBitReader& args`) qu'il doit **désérialiser lui-même**. Le routeur ne sait rien des types ; il
transporte des octets. C'est pourquoi le `name` doit être **identique** côté émetteur et récepteur
(même hash → même handler). Usage typique : un système ECS expose ses RPC réseau, chacun matérialisé
par un descripteur dans la table du routeur.

### `NkRPCRouter` — le cœur du RPC

Le routeur tient une **table statique fixe** (`kMaxRPCs = 256`, tableau pré-alloué, **zéro
allocation heap**) et un pointeur **non possédé** vers un `NkConnectionManager`. Son cycle est
trivial (`= default` au ctor et au dtor) ; toute la logique est dans ses méthodes.

- `SetConnectionManager(cm)` — **à appeler en premier**, avant tout `Call*` : sans lui, le routeur
  n'a aucun moyen d'émettre. Le pointeur est emprunté, pas possédé (le routeur ne le libère pas).
- `Register(name, type, handler, rel = NK_RELIABLE_ORD)` `[[nodiscard]] uint32` — ajoute une entrée
  et renvoie l'**ID hashé** ; renvoie **`0` si la table est pleine** (256 atteint). Complexité de
  l'ajout linéaire, et **non thread-safe** : on enregistre **tout au démarrage**, avant que le
  thread réseau ne tourne.
- `CallServer` / `CallClient` / `Multicast` `[[nodiscard]] NkNetResult` — les trois portes de
  sortie, variadiques : les arguments sont sérialisés automatiquement (dans la limite des types
  supportés). Toujours vérifier le `NkNetResult` retourné.
- `OnRPCReceived(caller, data, size)` — l'entrée : on lui passe un paquet RPC brut, il extrait l'ID,
  trouve le descripteur (lookup `O(n)`, n ≤ 256), désérialise et invoque le handler. Il **gère les
  erreurs sans crasher** (log + skip d'un paquet malformé). C'est `NkConnection::OnReceived()` qui
  l'appelle, sur le thread réseau.

Usages, par domaine :
- **Gameplay / IA** : la colonne vertébrale du multijoueur — inputs, actions, événements de jeu
  passent tous par le routeur.
- **ECS** : chaque système réseauté enregistre ses RPC au boot ; le routeur fait le pont entre
  paquet et logique de composant.
- **Threading** : modèle simple et sûr **si** on respecte la règle — `Register` au démarrage,
  `Call*`/`OnRPCReceived` sur le **seul** thread réseau (aucun mutex interne).
- **Outils / debug** : le `name` lisible (`"Classe::Fonction"`) facilite le traçage d'un flux RPC.

### Sérialisation native des arguments

Le routeur ne sait sérialiser **que** ces types à l'envoi : `bool`, `uint8`, `uint16`, `uint32`,
`int32`, `float32`, `math::NkVec3f`, `const char*` et `NkString`. Conséquence directe et souvent
oubliée : **pas de 64 bits** (`uint64`/`int64`), **pas de `double`**, pas d'`int16`/`int8`. Pour
transmettre un identifiant 64 bits ou un horodatage, on le découpe en deux `uint32`, ou bien le
handler lit/écrit manuellement via le `NkBitReader`/`NkBitWriter`. En pratique `math::NkVec3f` couvre
positions/directions/vitesses du gameplay et de la physique sans rien faire à la main.

### Macros `NK_RPC_SERVER` / `NK_RPC_CLIENT` / `NK_RPC_MULTICAST`

Pur sucre de déclaration. `NK_RPC_SERVER(Class, Func, ...)` génère une **déclaration de méthode**
(`void Func##_RPC(...)`) **et** une **constante de nom** (`static constexpr const char*
k##Func##_RPCName = "Class::Func"`). Les variantes `_CLIENT` (`_ClientRPC` / `k…_ClientRPCName`) et
`_MULTICAST` (`_MulticastRPC` / `k…_MulticastName`) suivent le même schéma. Important : elles ne
génèrent **ni enregistrement ni corps** — il faut toujours `Register(...)` à la main et écrire
l'implémentation. Leur intérêt : standardiser le nommage et fournir la constante de nom à passer aux
`Call*`, ce qui évite les fautes de frappe dans les chaînes.

### `NkSessionState` et `NkSessionStateStr`

L'énumération décrit le **fil de vie** d'une partie : `NK_SESSION_IDLE` (rien en cours) →
`NK_SESSION_LOBBY` (joueurs en attente) → `NK_SESSION_LOADING` (chargement) →
`NK_SESSION_IN_PROGRESS` (en jeu) → `NK_SESSION_ENDED` (terminée). La fonction libre
`NkSessionStateStr(s)` (définie *inline* dans le header, `O(1)`) en donne le nom lisible
(« Idle »/« Lobby »/« Loading »/« InProgress »/« Ended »/« Unknown »). Usages :
- **UI / 2D** : afficher l'état courant à l'écran (« En attente… », « Chargement… »).
- **Outils / debug** : logguer les transitions d'état lisiblement.
- **Gameplay** : verrouiller des actions selon l'état (le chat n'est ouvert qu'en `LOBBY`).

### `NkPlayerInfo` et `NkSessionConfig`

`NkPlayerInfo` (export) est la **fiche d'un joueur** : `peerId`, `displayName` (64 chars), `elo`
(défaut 1000), `ping`, `isHost`, `isReady`, `team`, `slot`. C'est ce que reçoivent les callbacks et
ce que renvoie `FindPlayer`.
- **UI / 2D** : peupler la liste des joueurs du lobby (pseudo, prêt/non, équipe, ping).
- **Gameplay / IA** : répartir les équipes, équilibrer par ELO, afficher le ping comme indicateur de
  qualité de connexion.

`NkSessionConfig` (export) décrit la **partie** : `name`, `mapName`, `gameMode`, `maxPlayers`
(8) / `minPlayers` (2), `isPrivate`, `isRanked`, `password`, `port` (7777), `minELO`/`maxELO`,
`maxPingMs` (200), `region` (`NkString`). Les champs `min/maxELO`, `maxPingMs` et `region` servent
au **filtrage matchmaking** ; `isRanked` déclenche le calcul d'ELO à la fin. **Attention** :
`maxPlayers` peut valoir jusqu'à 256 alors que la session ne stocke que **64** joueurs — restez ≤ 64.

### `NkSession` — la machine à états d'une partie

`NkSession` (export) **possède** son `NkConnectionManager` (par valeur, pas un pointeur) et range
ses joueurs dans un tableau fixe (`kMaxPlayers = 64`, **aucune allocation par joueur**). Elle est
`friend` de `NkLobby`. Méthodes :

- `Create(cfg)` `[NkNetResult]` — vous devenez **hôte**, l'état passe en `LOBBY`.
- `Join(addr, password = nullptr)` `[NkNetResult]` — rejoint en **client** ; renvoie
  `NK_NET_AUTH_FAILED` si le mot de passe est faux.
- `Leave()` — quitte proprement, retour à `IDLE` ; **thread-safe**.
- `Start()` `[NkNetResult]` — **host-only**, enchaîne `LOBBY → LOADING → IN_PROGRESS` après avoir
  vérifié `minPlayers` et que les joueurs sont prêts.
- `End()` — termine, **calcule l'ELO si ranked**, notifie via `onStateChanged`.
- `Kick(peer, reason = nullptr)` — **host-only**, expulse un joueur.
- `SetReady(bool)` — valide en `LOBBY`, diffusé aux autres.
- Lecture : `GetState`, `IsHost`, `IsInProgress`, `GetPlayerCount`, `GetConfig`, `GetConnMgr`, et
  `FindPlayer(peer)` (renvoie `nullptr` si absent ; pointeur **valide seulement pendant l'appel** ;
  lookup `O(n)`, n ≤ 64).
- Callbacks publics assignables : `onPlayerJoined(const NkPlayerInfo&)`,
  `onPlayerLeft(NkPeerId, const char*)`, `onStateChanged(NkSessionState)`.

Usages :
- **Gameplay / IA** : pilote tout le pré-jeu (composition, prêts, démarrage) et le post-jeu (ELO).
- **UI / 2D** : les trois callbacks alimentent en direct l'écran de lobby (arrivées, départs,
  changements d'état).
- **Threading** : les callbacks sont invoqués depuis le **thread réseau** — il faut **différer** le
  travail UI vers le thread jeu (ne pas toucher au rendu directement dedans).

### `NkLobby` — la façade singleton

`NkLobby` (export) enveloppe une `NkSession` privée et ajoute deux choses qu'une session brute n'a
pas : le **chat** et les **réglages d'hôte**. `Global()` renvoie le singleton (magic-static C++11).

- `CreateSession`/`JoinSession` `[NkNetResult]`, `LeaveSession`, `GetSession()` — délèguent à la
  session interne ; `GetSession()` renvoie une référence (durée de vie = celle du lobby).
- `SendChatMessage(msg)` — valide en `LOBBY`, **tronqué à 256 caractères** silencieusement ; le
  callback public `onChatMessage(const ChatMessage&)` reçoit les messages. `ChatMessage` (struct
  imbriquée, **non exportée**) porte `sender` (64), `text` (256), `at` (`NkTimestampMs`).
- `SetMap`, `SetGameMode`, `SetTeam(peer, team)` — réglages **host-only**, et — point qui surprend —
  **méthodes de `NkLobby`, pas de `NkSession`** (`session.SetMap(...)` n'existe pas).

Usages :
- **UI / 2D** : un seul point d'accès (`NkLobby::Global()`) pour brancher l'écran de lobby complet
  (liste, chat, sélection de carte/mode/équipe).
- **Gameplay** : l'hôte ajuste carte, mode et équipes avant le `Start()`.

### `NkMatchmaker` — le matchmaking en ligne

`NkMatchmaker` (export) cherche des parties **en ligne**, de façon asynchrone.
- `SearchAsync(params, onFound, onError, timeoutSec = 30)` — **non bloquant** ; `onFound` peut être
  invoqué **plusieurs fois** (un par candidat), `onError` une fois en cas d'échec. `SearchParams`
  (imbriquée, non exportée) : `gameMode` (vide = tous), `myELO`, `maxPingMs` (150), `region` (vide =
  toutes), `playerCount`, `ranked`.
- `CancelSearch()` — invoque `onError(NK_NET_TIMEOUT)` si une recherche est active, sinon no-op.
- `IsSearching()` — état courant.
- `RegisterServer(cfg, url)` `[NkNetResult]` — côté hôte/serveur, s'annonce au matchmaker avec un
  **heartbeat périodique** ; `UnregisterServer()` `[NkNetResult]` retire l'annonce (no-op si non
  enregistré).
- `SearchResult` (imbriquée) : `serverAddr`, `sessionName`, `playerCount`, `maxPlayers`, `avgPing`,
  `avgELO`, `score` (normalisé `[0..1]`).

Usages :
- **Gameplay / IA** : « partie rapide » qui filtre par ELO/ping/région et choisit le meilleur
  `score`.
- **UI / 2D** : afficher la liste de résultats au fil de l'eau (chaque `onFound` ajoute une ligne).
- **Threading** : la recherche tourne sur un **thread** dédié ; les callbacks reviennent hors du
  thread jeu — à différer.

### `NkDiscovery` — la découverte LAN

`NkDiscovery` (export) découvre les serveurs du **réseau local** par broadcast UDP, **API
entièrement statique** (aucune instance à créer). `ServerInfo` (imbriquée) : `addr`, `name`,
`gameMode`, `playerCount`, `maxPlayers`, `ping`, `hasPassword`.
- `Broadcast(info, broadcastPort = 7778)` `[NkNetResult]` — envoie à `255.255.255.255:port` ; à
  rappeler **périodiquement** (~2 s) pour rester visible.
- `Listen(timeoutMs, out, broadcastPort = 7778)` `[NkNetResult]` — **bloquant** pendant
  `timeoutMs`, remplit `out` (un `NkVector<ServerInfo>`) avec **déduplication** par IP+port.

Usages :
- **Gameplay** : l'écran « parties LAN » classique des jeux locaux/couch co-op.
- **Outils / éditeur** : repérer rapidement les instances de test sur le réseau de l'atelier.
- **Threading** : `Listen` bloque — l'appeler sur un thread de fond, jamais sur l'UI.

### `NkHTTPMethod`, `NkHTTPHeader`, `NkHTTPRequest`

`NkHTTPMethod` énumère les verbes : `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, `HEAD`. `NkHTTPHeader`
(export) est une simple paire `key`/`value` (`NkString`).

`NkHTTPRequest` (export) décrit la requête : `url`, `method` (défaut `GET`), `headers`, `body`,
`timeoutMs` (10000 ; **0 = infini**), `followRedirects` (true), `maxRedirects` (5). Ses helpers
modifient corps et/ou en-têtes :
- `SetJSON(json)` — pose le corps **et** `Content-Type`/`Accept: application/json`. **Remplace le
  corps** → à appeler **en dernier**.
- `SetFormData(formData)` — corps + `Content-Type: application/x-www-form-urlencoded`.
- `AddHeader(key, value)` — doublons autorisés.
- `SetBearerToken(token)` — ajoute `Authorization: Bearer <token>` (token **sans** préfixe).
- `SetBasicAuth(user, pass)` — ajoute `Authorization: Basic <base64(user:pass)>`.

Usages :
- **IO / réseau** : appeler une API REST (auth, profils, achats).
- **Outils / éditeur** : récupérer des assets ou métadonnées depuis un service distant.

### `NkHTTPResponse` — lire la réponse en sécurité

`NkHTTPResponse` (export) : `statusCode` (**0 = aucune réponse**), `statusText`, `headers`, `body`,
`error` (rempli si panne réseau), `timeMs`. L'inspection (toutes `[[nodiscard]] const noexcept`) :
- `IsOK()` — `200 ≤ code < 300`.
- `IsRedirect()` — `300 ≤ code < 400`.
- `IsClientError()` — `400 ≤ code < 500`.
- `IsServerError()` — `500 ≤ code < 600`.
- `HasError()` — `error` non vide (**priorité sur `statusCode`**).
- `GetHeader(key)` — **insensible à la casse**, `nullptr` si absent ; le pointeur reste valide tant
  que la réponse vit.

La **règle critique** : tester `HasError()` **avant** d'interpréter `statusCode`, car une coupure
réseau ne produit aucun code HTTP. C'est l'erreur la plus fréquente du domaine IO/réseau.

### `NkHTTPClient` — le client HTTP/HTTPS

`NkHTTPClient` (export) est synchrone **et** asynchrone (pool de threads), **thread-safe** (mutex
internes). Son destructeur n'est **pas** trivial : il **annule** les requêtes async, attend avec un
timeout court, puis libère. `Config` (imbriquée, non exportée) : `userAgent` (« NKNetwork/1.0 »),
`defaultTimeoutMs` (10000), `verifySSL` (true), `caCertPath` (vide = CA système), `maxAsyncThreads`
(4), `maxConnections` (8).

- Config : `Configure(cfg)` (s'applique aux requêtes **suivantes**), `AddDefaultHeader`,
  `SetDefaultBearerToken` (remplace le token précédent).
- **Synchrone** (`[[nodiscard]]`, **bloquant**) : `Send(req)`, `Get(url)`, `Post(url, json)`
  (configure le JSON automatiquement). **Jamais sur le thread UI.**
- **Asynchrone** : `SendAsync(req, cb)` `[[nodiscard]] uint32` (renvoie un **ID** pour `Cancel`,
  callback sur le **thread réseau**), `Cancel(id)`, `CancelAll()`,
  `WaitAll(timeoutMs = UINT32_MAX)`, `PendingCount()`.
- **Téléchargement** : `DownloadFile(url, dest, onProgress = nullptr, onComplete = nullptr)`
  `[[nodiscard]] uint32` — async ; `onProgress(bytesReceived, totalBytes)` (`total = 0` si inconnu) ;
  **écriture atomique** (fichier temporaire puis renommage).
- Statics utilitaires (`[[nodiscard]] static NkString`) : `URLEncode(s)` (RFC 3986, préserve
  alphanum + `-._~`), `URLDecode(s)` (décode `%XX`), `Base64Encode(data, size)` (RFC 4648, sans
  retours-ligne).

Usages :
- **IO / réseau** : tout dialogue web — auth, télémétrie, achats, configuration distante.
- **GPU / assets** : `DownloadFile` pour récupérer patches, textures, packs de contenu avec barre de
  progression (`onProgress`).
- **Threading** : préférer l'**async** dans un jeu (le sync bloque) ; toujours ramener le callback
  vers le thread jeu.
- **Outils / éditeur** : `URLEncode`/`Base64Encode` pour bâtir des URL et des en-têtes d'auth
  proprement.

### `NkLeaderboardEntry` et `NkLeaderboard`

`NkLeaderboardEntry` (export) : `rank`, `score` (`uint64`), `playerName` (64), `extraData` (256).

`NkLeaderboard` (export) est un client REST de classement bâti **sur** un `NkHTTPClient` interne :
- `Configure(baseUrl, apiKey)` — `apiKey` envoyée en en-tête `X-API-Key`.
- `SubmitScore(playerName, score, cb = nullptr)` — POST JSON vers `{baseUrl}/submit`.
- `FetchTop(count, cb)` — GET `{baseUrl}/top?count=N` ; liste **triée par rang croissant** ; le
  callback reçoit une **référence non-const** au vecteur.
- `FetchPlayerRank(playerName, cb)` — GET `{baseUrl}/player?name=<URLEncode>` ; **rank = 0 = non
  classé**.

Usages :
- **Gameplay / UI** : soumettre un score de fin de partie et afficher un top 10 ou le rang du
  joueur.
- **IO / réseau** : exemple canonique de client REST clé en main au-dessus de `NkHTTPClient`.

### Pièges transverses

- **`[[nodiscard]]` partout** : presque toute opération faillible renvoie un `NkNetResult` (ou un ID)
  à vérifier — ne l'ignorez pas.
- **Callbacks sur le thread réseau** : RPC, session, matchmaking, HTTP async — tous rappellent hors
  du thread jeu. Différez le travail (UI, rendu) vers le bon thread.
- **`NkRPCRouter` non thread-safe** : `Register` au boot, `Call*`/`OnRPCReceived` sur le thread
  réseau ; table fixe de **256**, lookup `O(n)`.
- **Sérialisation RPC limitée** : `bool`, `uint8/16/32`, `int32`, `float32`, `NkVec3f`,
  `const char*`, `NkString` — ni 64 bits ni `double`.
- **`NkSession` borne à 64 joueurs** bien que `maxPlayers` puisse aller à 256.
- **Réglages d'hôte sur `NkLobby`** (`SetMap`/`SetGameMode`/`SetTeam`), pas sur `NkSession`, et
  **host-only** (sinon ignorés silencieusement — tester `IsHost()`).
- **HTTP : `HasError()` avant `statusCode`**, `SetJSON` écrase le corps (en dernier), `Send`/`Get`/
  `Post` bloquants (pas sur l'UI), et `verifySSL` jamais à `false` en production.
- **Structs imbriquées non exportées** : `NkLobby::ChatMessage`, `NkMatchmaker::SearchParams`/
  `SearchResult`, `NkDiscovery::ServerInfo`, `NkHTTPClient::Config` ne portent pas
  `NKENTSEU_NETWORK_CLASS_EXPORT` (contrairement aux types de premier niveau).

---

### Exemple

```cpp
#include "NKNetwork/RPC/NkRPC.h"
#include "NKNetwork/Lobby/NkLobby.h"
#include "NKNetwork/HTTP/NkHTTPClient.h"
using namespace nkentseu::net;

// 1) RPC : enregistrer au boot, appeler depuis le client.
NkRPCRouter router;
router.SetConnectionManager(&conn);
uint32 fireId = router.Register("Weapon::Fire", NkRPCType::NK_SERVER_RPC,
    [](NkPeerId caller, NkBitReader& args) { /* lire args, exécuter */ },
    NkRPCReliability::NK_RELIABLE_ORD);
NkNetResult sent = router.CallServer("Weapon::Fire", origin);   // [[nodiscard]]

// 2) Lobby : créer une partie via la façade, écouter les arrivées.
NkSession& s = NkLobby::Global().GetSession();
s.onPlayerJoined = [](const NkPlayerInfo& p) { /* MAJ liste */ };
s.onStateChanged = [](NkSessionState st) { /* NkSessionStateStr(st) */ };
NkSessionConfig cfg;                       // cfg.mapName, cfg.maxPlayers (<= 64), ...
NkNetResult created = NkLobby::Global().CreateSession(cfg);
NkLobby::Global().SetMap("Arena01");       // host-only, méthode de NkLobby

// 3) HTTP : soumettre un score, puis lire le top 10.
NkLeaderboard board;
board.Configure("https://api.exemple.com/lb", "ma-cle");
board.SubmitScore("Rihen", 42000, [](bool ok) { /* ... */ });
board.FetchTop(10, [](NkVector<NkLeaderboardEntry>& top) { /* trié par rang */ });
```

---

[← Index NKNetwork](README.md) · [Récap NKNetwork](../NKNetwork.md) · [Couche System](../README.md)
