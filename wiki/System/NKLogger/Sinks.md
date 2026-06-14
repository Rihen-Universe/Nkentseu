# Les destinations de log : les *sinks*

> Couche **System** · NKLogger · Décider **où vont les messages** : la console couleur
> `NkConsoleSink`, le fichier `NkFileSink` et ses variantes à rotation `NkRotatingFileSink` /
> `NkDailyFileSink`, le diffuseur `NkDistributingSink`, le trou noir `NkNullSink`, et le logger
> asynchrone `NkAsyncLogger`.

Écrire un log, c'est en réalité deux questions bien distinctes : **quoi** dire (le message, son
niveau, son format) et **où** l'envoyer (l'écran, un fichier, plusieurs fichiers, rien du tout).
NKLogger sépare proprement ces deux responsabilités : le format est l'affaire du *formatter*, la
destination est l'affaire du **sink**. Un sink est simplement un objet qui sait recevoir un
`NkLogMessage` et le faire atterrir quelque part. Toute la famille dérive d'une seule interface,
`NkISink`, et l'on en **branche autant qu'on veut** sur un même logger : la même ligne part à la
fois vers la console *et* vers un fichier journalier, sans que le code appelant ne change d'un iota.

Le compromis central de cette page tient en une phrase : **un sink simple (console, fichier) écrit
de façon synchrone et bloque l'appelant le temps de l'I/O ; pour ne pas payer ce coût dans la
boucle chaude, on enfile les messages dans une file et un thread dédié s'en occupe** — c'est le
rôle, à part, de `NkAsyncLogger`. Le reste n'est qu'une affaire de *où* exactement : un flux
standard, un fichier qui tourne par taille, un fichier qui tourne par jour, ou plusieurs à la fois.

Tous les types partagent la même mécanique de **filtrage par niveau** (`SetLevel`/`ShouldLog`),
d'**activation** (`SetEnabled`) et de **format** (`SetFormatter`/`SetPattern`) héritée de
l'interface. La mémoire passe par NKMemory : on crée un sink avec `memory::MakeShared<TSink>(...)`,
on le configure, puis on l'ajoute au logger.

- **Namespace** : `nkentseu` (les types mémoire et threading sont qualifiés `memory::` /
  `threading::`)
- **Headers** : il n'y a **pas** de header parapluie — on inclut chaque sink directement, par
  exemple `#include "NKLogger/NkSink.h"`, `#include "NKLogger/Sinks/NkConsoleSink.h"`,
  `#include "NKLogger/Sinks/NkFileSink.h"`, `#include "NKLogger/NkAsyncSink.h"`.

---

## L'interface : `NkISink`

Tout commence par le **contrat**. `NkISink` est une interface abstraite (pattern *Strategy*) :
elle ne fait rien par elle-même, mais elle définit ce que **tout** sink doit savoir faire. On ne
l'instancie jamais directement ; on en hérite. Six méthodes sont **purement virtuelles**, donc
obligatoires pour toute destination concrète : `Log` (recevoir un message), `Flush` (forcer
l'écriture), et les quatre qui gèrent le format (`SetFormatter`, `SetPattern`, `GetFormatter`,
`GetPattern`).

À côté de ce contrat, l'interface fournit déjà, **toute faite**, la logique transverse que chaque
sink partage : le **filtrage par niveau** et l'**activation**. `SetLevel(level)` fixe le seuil
minimum (par défaut `NkLogLevel::NK_TRACE`, c'est-à-dire « tout passe ») ; `ShouldLog(level)`
répond `true` si `level >= m_Level` — un test `O(1)` qui doit rester ultra-rapide, car il est
appelé en tête de **chaque** message. `SetEnabled(false)` coupe complètement un sink sans le
retirer du logger, et `SetName` lui donne une étiquette pour s'y retrouver quand on en a plusieurs.

```cpp
class MySink : public nkentseu::NkISink {
public:
    void Log(const NkLogMessage& msg) override {
        if (!ShouldLog(msg.level) || !IsEnabled()) return;   // filtrage en tête, toujours
        // ... écrire le message ...
    }
    void Flush() override { /* ... */ }
    // + les 4 méthodes de formatage
};
```

Côté mémoire, deux alias résument les deux façons de **posséder** un sink :
`NkSinkPtr = memory::NkSharedPtr<NkISink>` (ownership **partagé** — le même sink peut être branché
sur plusieurs loggers) et `NkSinkUniquePtr = memory::NkUniquePtr<NkISink>` (ownership exclusif).
Attention au formatter : `SetFormatter` **consomme** un `NkUniquePtr` (transfert d'ownership par
*move*), tandis que `GetFormatter()` rend un **pointeur brut non-possédé** — qu'il ne faut
**jamais** `delete`.

> **En résumé.** `NkISink` = le contrat de toute destination. Six méthodes à fournir
> obligatoirement (`Log`, `Flush`, les 4 du format) ; le filtrage par niveau (`SetLevel`/
> `ShouldLog` `O(1)`) et l'activation (`SetEnabled`) sont **déjà fournis**. Ce n'est **pas** un
> sink utilisable tel quel, et il **ne garantit aucune thread-safety** : c'est à chaque dérivée de
> filtrer en tête de `Log` et de se protéger si besoin.

---

## La console : `NkConsoleSink`

C'est la destination la plus immédiate, celle qu'on branche en premier pendant le développement :
le texte s'affiche **dans le terminal**, en couleur, et avec la bonne plomberie sur chaque OS —
codes ANSI sous Unix, API Win32 sous Windows, `logcat` sous Android. Les couleurs sont
**auto-détectées** (et le résultat mis en cache) : on n'a pas à savoir si la sortie est un vrai
terminal ou un fichier redirigé. Contrairement à l'interface nue, ce sink est **thread-safe** grâce
à un mutex interne (`m_Mutex`, *mutable*).

Le constructeur par défaut fait le choix raisonnable : sortie `stdout`, couleurs activées, et
**routage des erreurs vers `stderr`**. Ce dernier point compte : par défaut, tout ce qui est
`NK_ERROR`, `NK_CRITICAL` ou `NK_FATAL` part sur `stderr` (et déclenche un *flush* immédiat), pour
que les erreurs survivent même si la sortie standard est bufferisée ou perdue. On peut tout
reconfigurer à chaud — choisir explicitement le flux (`NkConsoleStream::NK_STD_OUT` ou
`NK_STD_ERR`), activer/désactiver la couleur, ou couper la séparation erreurs/`stderr`.

```cpp
auto console = memory::MakeShared<NkConsoleSink>();   // stdout, couleurs, erreurs→stderr
console->SetLevel(NkLogLevel::NK_INFO);               // on tait TRACE/DEBUG
logger.AddSink(console);
```

Ce n'est **pas** une destination persistante : ce qui défile à l'écran ne se retrouve nulle part
une fois le terminal fermé. Pour garder une trace, on lui adjoint un `NkFileSink`.

> **En résumé.** `NkConsoleSink` = sortie terminal **couleur, multiplateforme et thread-safe**.
> Par défaut stdout + erreurs sur stderr + flush auto sur les niveaux `>= NK_ERROR`. Choisissez le
> flux avec `NkConsoleStream`, basculez la couleur et le routage `stderr` à la volée. Idéale en
> dev ; non persistante.

---

## Le fichier : `NkFileSink`

Pour **garder** les logs, on écrit dans un fichier. `NkFileSink` ouvre un `FILE*`, **crée au besoin
les répertoires parents** du chemin, et écrit chaque message suivi d'un retour à la ligne. Par
défaut le *buffering* est **désactivé** (`_IONBF`) : chaque ligne touche le disque tout de suite —
précieux pour qu'un crash ne fasse pas perdre les dernières lignes, au prix d'un débit plus faible.
Comme la console, il est **thread-safe**, mais ici le mutex est **protégé** (`m_Mutex`), donc
accessible aux classes dérivées : c'est ce qui rend possibles les variantes à rotation.

Le constructeur prend le nom du fichier et un drapeau `truncate` : `true` ouvre en écrasement
(mode `"wb"`), `false` (le défaut) ouvre en **ajout** (`"ab"`), pour reprendre un journal existant.
On peut ensuite piloter le cycle de vie du fichier — `Open()`, `Close()` (idempotent), `IsOpen()`,
le changer de nom (`SetFilename`, qui ferme et rouvre), connaître sa taille (`GetFileSize()`, via
`stat()`), ou basculer le mode troncature.

```cpp
auto file = memory::MakeShared<NkFileSink>("logs/session.log");  // ajout ; crée logs/ au besoin
file->SetPattern("[{time}] [{level}] {message}");
logger.AddSink(file);
```

Deux subtilités à connaître. D'abord, les **erreurs d'écriture sont ignorées silencieusement** :
un disque plein ne fera pas planter l'application, mais ne signalera rien non plus. Ensuite,
`NkFileSink` expose un **point d'extension** central, `CheckRotation()` — virtuel, *no-op* par
défaut, appelé après chaque écriture — c'est lui que surchargent les sinks à rotation. Pour les
aider, des méthodes protégées en `…Unlocked` (`OpenUnlocked`, `CloseUnlocked`,
`GetFilenameUnlocked`) font le travail **en supposant le mutex déjà détenu** : on ne les appelle
jamais sans le lock, sous peine de course.

> **En résumé.** `NkFileSink` = persistance fichier `FILE*`, **création des dossiers parents**,
> **buffering off** par défaut (durabilité), thread-safe (mutex **protégé**). `truncate` choisit
> écrasement vs ajout. Erreurs d'écriture **silencieuses**. Le crochet `CheckRotation()` + les
> méthodes `…Unlocked` sont la base des variantes à rotation.

---

## La rotation par taille : `NkRotatingFileSink`

Un fichier de log qui grossit sans limite finit par devenir ingérable. `NkRotatingFileSink` (qui
**hérite** de `NkFileSink`) résout cela en **tournant quand le fichier atteint une taille**. Le
schéma de sauvegarde est un décalage simple : `app.log` devient `app.log.0`, l'ancien `.0` devient
`.1`, et ainsi de suite jusqu'à `m_MaxFiles` sauvegardes — la plus ancienne étant supprimée.
Pour éviter d'interroger le disque à chaque ligne, il **met en cache** la taille courante
(`m_CurrentSize`) plutôt que d'appeler `stat()` sans cesse.

Le constructeur prend le nom, la taille maximale (`maxSize`) et le nombre de sauvegardes à
conserver (`maxFiles`, où `0` désactive toute conservation). Tout est reconfigurable à chaud
(`SetMaxSize`, `SetMaxFiles`), et l'on peut **forcer** une rotation immédiate avec `Rotate()` —
qui remet le compteur de taille à zéro en cas de succès.

```cpp
// fichiers de 5 Mo, on garde app.log + 3 sauvegardes (.0 .1 .2)
auto rot = memory::MakeShared<NkRotatingFileSink>("logs/app.log", 5 * 1024 * 1024, 3);
logger.AddSink(rot);
```

> **En résumé.** `NkRotatingFileSink` = fichier qui **tourne par taille** (`app.log` → `.0` → `.1`
> → … → `.N`), `maxFiles` sauvegardes conservées (`0` = aucune). Taille mise en cache pour éviter
> les `stat()`. `Rotate()` force une rotation. Le bon choix quand le **volume** est le risque.

---

## La rotation quotidienne : `NkDailyFileSink`

Quand on veut **un fichier par jour** plutôt qu'un fichier par tranche de taille, c'est
`NkDailyFileSink` (lui aussi dérivé de `NkFileSink`). À l'heure et à la minute configurées, il
ferme le fichier courant, l'archive avec un suffixe de **date fixe** `.YYYYMMDD` (par exemple
`app.log.20240115`), et rouvre un fichier neuf. Il peut **purger** les archives au-delà de
`m_MaxDays` jours (`0` = illimité). Pour ne pas vérifier la date à chaque ligne, le contrôle est
**limité à une fois par minute** (via `m_LastCheck`).

Le constructeur prend le nom, l'heure et la minute de bascule (par défaut minuit, `0:0`) et le
nombre de jours à conserver. On peut changer l'heure de rotation (`SetRotationTime`), la
rétention (`SetMaxDays`), ou forcer une bascule (`Rotate()` — qui, à la différence du fonctionnement
automatique, ne met pas à jour la date interne `m_CurrentDate`).

```cpp
// bascule chaque jour à 00:00, conserve 7 jours d'historique
auto daily = memory::MakeShared<NkDailyFileSink>("logs/server.log", 0, 0, 7);
logger.AddSink(daily);
```

> **En résumé.** `NkDailyFileSink` = **un fichier par jour**, bascule à `H:M`, suffixe `.YYYYMMDD`,
> rétention `m_MaxDays` (`0` = illimité), contrôle de date **throttlé à 1/min**. À préférer quand le
> découpage **par date** (audit, serveur, sessions) est plus parlant que par taille.

---

## Le diffuseur : `NkDistributingSink`

Souvent on veut la **même ligne à plusieurs endroits** : à l'écran *et* dans un fichier *et* dans
une fenêtre d'éditeur. Plutôt que d'ajouter trois sinks au logger, on peut en regrouper plusieurs
derrière un seul, le `NkDistributingSink` — un **composite** qui, à chaque message, le **rediffuse
(broadcast)** vers tous ses sous-sinks. C'est un `NkISink` comme les autres (donc lui-même branchable
sur un logger), thread-safe via son mutex interne.

Le filtrage `ShouldLog`/`IsEnabled` est appliqué **au niveau du distributeur**, en amont : un
distributeur réglé sur `NK_WARN` ne transmettra jamais un `NK_INFO`, quel que soit le réglage des
sous-sinks. La diffusion est `O(n)` sur le nombre de sous-sinks, sous lock, et les sinks `null`
sont ignorés. La gestion est dynamique : `AddSink`, `RemoveSink` (qui compare **par adresse**, via
`Get()`), `ClearSinks`, plus les requêtes `GetSinks` (copie figée, sûre à itérer hors lock),
`GetSinkCount` et `ContainsSink`.

```cpp
auto multi = memory::MakeShared<NkDistributingSink>();
multi->AddSink(memory::MakeShared<NkConsoleSink>());
multi->AddSink(memory::MakeShared<NkFileSink>("logs/app.log"));
multi->SetLevel(NkLogLevel::NK_INFO);   // filtre une fois, pour tous
logger.AddSink(multi);
```

Le partage du format suit deux logiques différentes : `SetPattern` **propage** le pattern à chaque
sous-sink, tandis que `SetFormatter` le **clone** vers chacun (en repassant par `GetPattern`) tout
en consommant la source ; les *getters* (`GetFormatter`/`GetPattern`) renvoient ceux du **premier**
sous-sink non-null.

> **En résumé.** `NkDistributingSink` = un sink qui **rediffuse** vers N sous-sinks. Filtrage
> appliqué au distributeur (en amont) ; diffusion `O(n)` sous lock ; gestion dynamique par adresse
> (`Add`/`Remove`/`Clear`/`Contains`). Le moyen propre de viser **plusieurs destinations à la fois**.

---

## Le trou noir : `NkNullSink`

`NkNullSink` est le *Null Object* du module : un sink dont **toutes les méthodes ne font rien**.
`Log` ignore le message, `Flush` est *no-op*, `GetFormatter()` renvoie toujours `nullptr` et
`GetPattern()` toujours une chaîne vide. Comme il n'a **aucun état mutable**, il est thread-safe
**sans la moindre synchronisation** et ne fait **ni allocation ni I/O**.

Ce n'est pas un gadget : c'est l'outil qui évite les `if (logger)` partout. Là où du code attend un
sink, on peut en passer un qui ne coûte rien — pour **désactiver** une catégorie de logs sans
toucher au reste, comme valeur par défaut sûre, ou dans les tests pour absorber les messages sans
les écrire.

```cpp
auto silence = memory::MakeShared<NkNullSink>();
logger.AddSink(silence);   // tout ce qui sort ici est englouti, à coût nul
```

> **En résumé.** `NkNullSink` = destination **no-op**, sans état, thread-safe sans verrou, **zéro
> allocation/I-O**. La valeur par défaut sûre et le moyen propre de « couper » une sortie sans
> conditionnelles dans le code.

---

## Le logger asynchrone : `NkAsyncLogger`

Voici l'exception de la famille — et un piège de nommage à signaler d'emblée. Le header
`NKLogger/NkAsyncSink.h` ne déclare **pas** un sink : il déclare `NkAsyncLogger`, qui hérite de
**`NkLogger`** (pas de `NkISink`). Ce n'est donc pas une destination, mais un **logger** dont le
rôle est de **découpler l'appelant de l'I/O**. Au lieu d'écrire dans les sinks au moment de l'appel
(ce qui bloque l'appelant le temps de l'écriture), il **copie** chaque message dans une file
(`NkQueue<NkLogMessage>`), et un **thread worker dédié** se charge, plus tard, de les distribuer
aux sinks. L'appel de log redevient ainsi quasi gratuit dans la boucle chaude.

Le constructeur prend un nom, une taille de file (`queueSize`, défaut 8192) et un intervalle de
flush en millisecondes (`flushInterval`, défaut 1000 ; `0` = flush sur demande). Point crucial : le
worker **n'est pas démarré** par le constructeur — il faut appeler `Start()` (idempotent) avant de
logger, et `Stop()` pour un arrêt propre (notification, *join*, vidage final de la file). `IsRunning()`
en donne l'état.

```cpp
NkAsyncLogger async("game", 8192, 1000);
async.AddSink(memory::MakeShared<NkFileSink>("logs/game.log"));
async.Start();                              // OBLIGATOIRE avant de logger
// ... la boucle de jeu logue sans bloquer sur le disque ...
async.Stop();                              // flush + vidage + join
```

Reste la question du **débordement** : que faire quand la file est pleine ? La réponse est
configurable via `NkAsyncOverflowPolicy` — `NK_DROP_OLDEST` (défaut : on jette le plus ancien),
`NK_DROP_NEWEST` (on jette le message courant) ou `NK_BLOCK` (on bloque l'appelant jusqu'à ce qu'il
y ait de la place). On règle la file à chaud (`SetMaxQueueSize`, `GetQueueSize` pour les messages en
attente), l'intervalle (`SetFlushInterval`) et la politique (`SetOverflowPolicy`).

Pièges à retenir : il faut **`Start()` avant** de logger, et **après `Stop()` l'enqueue échoue** ;
`NK_BLOCK` peut **bloquer indéfiniment** l'appelant si le worker ne suit pas ; les sinks branchés
**doivent être thread-safe** puisqu'ils sont appelés depuis le worker (potentiellement en
concurrence avec `Flush`) ; et une exception levée dans un sink peut **terminer le worker** (aucun
`try/catch` garanti).

> **En résumé.** `NkAsyncLogger` (header `NkAsyncSink.h`, mais c'est un **`NkLogger`**, pas un
> sink) = file + **thread worker** pour sortir l'I/O de la boucle chaude. `Start()` obligatoire,
> `Stop()` pour finir proprement. Débordement réglable (`NK_DROP_OLDEST`/`NK_DROP_NEWEST`/
> `NK_BLOCK`). Les sinks utilisés doivent être thread-safe.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé (comportement, complexité, cas
d'usage) dans la « Référence complète » qui suit. Complexités/notes entre crochets.

### `NkISink` — interface des sinks (`NKLogger/NkSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Contrat (pur) | `Log`, `Flush` | Écrire un message / forcer l'écriture des buffers. |
| Contrat (pur) | `SetFormatter`, `SetPattern`, `GetFormatter`, `GetPattern` | Format : adopter un formatter (move) / depuis un pattern / lire le formatter brut / lire le pattern. |
| Filtrage | `SetLevel`, `GetLevel`, `ShouldLog` `[O(1)]` | Seuil minimum (défaut `NK_TRACE`) / lire / `level >= m_Level` ? |
| Activation | `SetEnabled`, `IsEnabled` | Activer/désactiver (défaut true). |
| Identité | `SetName`, `GetName` | Étiquette du sink (défaut vide). |
| Cycle de vie | `virtual ~NkISink()` | Destructeur virtuel. |
| Alias | `NkSinkPtr`, `NkSinkUniquePtr` | `NkSharedPtr<NkISink>` (partagé) / `NkUniquePtr<NkISink>` (exclusif). |

### `NkConsoleSink` — console couleur (`NKLogger/Sinks/NkConsoleSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkConsoleStream` : `NK_STD_OUT`(0), `NK_STD_ERR`(1) | Choix du flux standard. |
| Construction | `NkConsoleSink()`, `NkConsoleSink(stream, useColors=true)` | Défaut (stdout, couleurs, erreurs→stderr) / configuré. |
| Override | `Log`, `Flush`, `SetFormatter`, `SetPattern`, `GetFormatter`, `GetPattern` | Contrat `NkISink` (route stdout/stderr/logcat, flush auto `>= NK_ERROR`). |
| Config | `SetColorEnabled`, `IsColorEnabled` | Activer/désactiver la couleur. |
| Config | `SetStream`, `GetStream` | Choisir/lire le flux. |
| Config | `SetUseStderrForErrors`, `IsUsingStderrForErrors` | Router `NK_ERROR`/`CRITICAL`/`FATAL` vers stderr. |

### `NkFileSink` — fichier (`NKLogger/Sinks/NkFileSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkFileSink(filename, truncate=false)` | Ouvre en ajout (`"ab"`) ou écrasement (`"wb"`) ; crée les dossiers. |
| Override | `Log`, `Flush`, `SetFormatter`, `SetPattern`, `GetFormatter`, `GetPattern` | Contrat `NkISink` (écrit + newline, appelle `CheckRotation()`). |
| Fichier | `Open`, `Close`, `IsOpen` | Cycle de vie du fichier (`Close` idempotent). |
| Fichier | `GetFilename`, `SetFilename` | Lire / changer (ferme-rouvre). |
| Fichier | `GetFileSize` | Taille via `stat()`. |
| Fichier | `SetTruncate`, `GetTruncate` | Mode écrasement/ajout. |
| Extension | `virtual CheckRotation()` | Crochet de rotation (no-op par défaut). |
| Protégé | `OpenUnlocked`, `CloseUnlocked`, `GetFilenameUnlocked` | Variantes **sans verrou** (mutex déjà détenu). |
| Protégé | `m_Mutex` | Mutex partagé avec les dérivées. |

### `NkRotatingFileSink` — rotation par taille (`NKLogger/Sinks/NkRotatingFileSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkRotatingFileSink(filename, maxSize, maxFiles)` | `maxFiles`=0 désactive la conservation. |
| Override | `Log` | `NkFileSink::Log()` + maj taille + rotation. |
| Config | `SetMaxSize`, `GetMaxSize` | Seuil de rotation (octets). |
| Config | `SetMaxFiles`, `GetMaxFiles` | Nombre de sauvegardes conservées. |
| Action | `Rotate` | Rotation forcée (remet la taille à 0 si succès). |

### `NkDailyFileSink` — rotation quotidienne (`NKLogger/Sinks/NkDailyFileSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkDailyFileSink(filename, hour=0, minute=0, maxDays=0)` | Bascule à `H:M` ; `maxDays`=0 = illimité. |
| Override | `Log` | Écriture + `CheckRotation()`. |
| Config | `SetRotationTime`, `GetRotationHour`, `GetRotationMinute` | Heure de bascule. |
| Config | `SetMaxDays`, `GetMaxDays` | Rétention en jours. |
| Action | `Rotate` | Bascule forcée (ne met pas à jour `m_CurrentDate`). |

### `NkDistributingSink` — diffuseur composite (`NKLogger/Sinks/NkDistributingSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkDistributingSink()`, `NkDistributingSink(sinks)` | Vide / copie initiale d'une collection. |
| Override | `Log` `[O(n)]`, `Flush` | Filtre puis diffuse à chaque sous-sink non-null. |
| Override | `SetFormatter` (clone), `SetPattern` (propage), `GetFormatter`/`GetPattern` (1er non-null) | Partage du format vers les sous-sinks. |
| Gestion | `AddSink`, `RemoveSink` (par adresse), `ClearSinks` | Composition dynamique. |
| Requête | `GetSinks` (copie figée), `GetSinkCount`, `ContainsSink` `[O(n)]` | Inspection (par adresse). |

### `NkNullSink` — destination no-op (`NKLogger/Sinks/NkNullSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkNullSink()` | Aucun état. |
| Override | `Log`, `Flush`, `SetFormatter`, `SetPattern` | Tous **no-op** (formatter passé détruit par RAII). |
| Override | `GetFormatter` → `nullptr`, `GetPattern` → `""` | Toujours vide. |

### `NkAsyncLogger` — logger asynchrone (`NKLogger/NkAsyncSink.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enum | `NkAsyncOverflowPolicy` : `NK_DROP_OLDEST`(0), `NK_DROP_NEWEST`(1), `NK_BLOCK`(2) | Politique de débordement de file. |
| Construction | `NkAsyncLogger(name, queueSize=8192, flushInterval=1000)` | Worker **non** démarré (flushInterval en ms, `0` = sur demande). |
| Override | `Flush` | Vide la file puis flush les sinks. |
| Worker | `Start`, `Stop`, `IsRunning` | Démarrer (idempotent) / arrêter (flush+join) / état atomique. |
| File | `GetQueueSize`, `SetMaxQueueSize`, `GetMaxQueueSize` | Messages en attente / capacité. |
| File | `SetFlushInterval`, `GetFlushInterval` | Intervalle de flush (ms). |
| File | `SetOverflowPolicy`, `GetOverflowPolicy` | Politique de débordement. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, puis cas d'usage par domaine.
Les éléments triviaux (constructeurs, accesseurs) sont décrits brièvement ; le filtrage, la
rotation et l'asynchrone le sont **à fond**.

### Choisir : quel sink pour quel besoin

Le critère n'est pas « lequel marche » mais « où dois-je envoyer les logs, et à quel coût » :

- **Voir tout de suite, en dev** → `NkConsoleSink`.
- **Garder une trace simple** → `NkFileSink`.
- **Le volume risque d'exploser** → `NkRotatingFileSink` (par taille).
- **Le découpage par date a du sens** (serveur, audit, sessions) → `NkDailyFileSink`.
- **Plusieurs destinations à la fois** → `NkDistributingSink`.
- **Désactiver proprement une sortie** → `NkNullSink`.
- **Ne pas bloquer la boucle chaude sur l'I/O** → `NkAsyncLogger` (par-dessus les sinks ci-dessus).

### `NkISink` à fond — le contrat et le filtrage

L'interface fait deux choses essentielles **pour** vous, que toute dérivée hérite : le **filtrage
par niveau** et l'**activation**. `ShouldLog(level)` est un simple `level >= m_Level`, garanti
`O(1)` — il est appelé en tête de chaque `Log`, sur le chemin le plus chaud du module, donc il doit
rester trivial. C'est ce mécanisme qui permet de **monter le seuil en production** (`NK_WARN`) sans
toucher aux innombrables appels `NK_TRACE`/`NK_DEBUG` disséminés dans le code : ils sont émis,
filtrés, et coûtent presque rien.

Le reste — `Log`, `Flush`, les quatre méthodes de format — est **purement virtuel** : c'est ce que
vous écrivez quand vous créez votre propre sink. Idiomes par domaine :

- **Outils / éditeur** — un sink custom qui pousse les messages dans un panneau console de l'éditeur
  (filtré sur place via `ShouldLog`), idéal pour un widget de logs en jeu.
- **IO / réseau** — un sink qui envoie les logs sur une socket vers un agrégateur distant ; le
  filtrage en amont évite de saturer le réseau de `TRACE`.
- **Threading** — l'interface **ne garantit aucune thread-safety** : un sink custom appelé depuis
  `NkAsyncLogger` doit se protéger lui-même.

L'ownership se résume aux alias `NkSinkPtr` (partagé, pour brancher un même sink sur plusieurs
loggers) et `NkSinkUniquePtr` (exclusif). Côté format, `SetFormatter` **consomme** un `NkUniquePtr`
(move) — on lui passe la propriété — tandis que `GetFormatter()` rend un pointeur **non possédé**,
à ne jamais `delete`.

### `NkConsoleSink` à fond — la sortie terminal

Sa valeur tient à trois choix par défaut : **stdout**, **couleurs auto-détectées** (et cachées pour
ne pas re-tester à chaque ligne), et **erreurs routées vers stderr** avec flush immédiat. Ce
routage (`NK_ERROR`/`NK_CRITICAL`/`NK_FATAL` → stderr) garantit que les pépins remontent même quand
la sortie standard est redirigée ou perdue. La plomberie est multiplateforme : ANSI sous Unix,
API Win32 sous Windows, `logcat` sous Android (où `Flush` devient un *no-op*). Il est thread-safe
(mutex interne *mutable*), donc utilisable depuis plusieurs threads.

- **Gameplay / IA** — la console de dev où défilent états et décisions pendant qu'on joue.
- **Rendu / GPU** — afficher en couleur les warnings de validation, les erreurs de pipeline ; le
  rouge sur `stderr` saute aux yeux dans le flot.
- **Outils** — sortie immédiate d'un utilitaire en ligne de commande.

Réglages : `SetColorEnabled` (couper la couleur pour un fichier redirigé), `SetStream` /
`NkConsoleStream` (forcer stdout ou stderr), `SetUseStderrForErrors` (désactiver la séparation si
l'on veut tout sur un seul flux).

### `NkFileSink` à fond — la persistance et son point d'extension

C'est la **brique** de toute persistance. Il crée les répertoires parents (on peut donner
`logs/sub/app.log` sans préparer l'arborescence) et désactive le *buffering* (`_IONBF`) par
défaut : chaque ligne touche le disque tout de suite, donc un crash ne perd pas les dernières
lignes — au prix d'un débit moindre. Le mode (`truncate`) choisit entre repartir de zéro (`"wb"`)
et **reprendre** un journal (`"ab"`, défaut).

Le cœur de son extensibilité est triple : le crochet `CheckRotation()` (virtuel, *no-op*, appelé
après chaque écriture), le **mutex protégé** `m_Mutex` (donc visible des dérivées) et les méthodes
`…Unlocked` (`OpenUnlocked`, `CloseUnlocked`, `GetFilenameUnlocked`) qui opèrent **sans reprendre le
verrou** — c'est exactement ce dont une rotation a besoin pour fermer/renommer/rouvrir de façon
atomique sous un lock déjà tenu.

- **IO** — journal d'application ou de service qui doit survivre au processus.
- **Outils / éditeur** — fichier de log d'une session d'édition, relu après coup.
- **Tests** — capturer la sortie d'un test dans un fichier dédié pour analyse post-mortem.

Deux pièges : les **erreurs d'écriture sont silencieuses** (disque plein = pas de crash, mais pas
d'alerte non plus), et `GetFileSize()` (via `stat()`) peut **surévaluer** la taille si des buffers
ne sont pas flushés. Et l'on n'appelle **jamais** une méthode `…Unlocked` sans détenir le verrou.

### `NkRotatingFileSink` à fond — borner par la taille

Hérite de `NkFileSink` et surcharge `CheckRotation()` pour comparer la taille **cachée**
(`m_CurrentSize`, tenue à jour à chaque `Log`, pour éviter les `stat()` répétés) au seuil
`m_MaxSize`. La rotation décale les sauvegardes — `app.log` → `.0`, `.0` → `.1`, … — supprime la
plus ancienne au-delà de `m_MaxFiles` (où `0` veut dire « n'en garde aucune »), renomme le courant
en `.0`, rouvre un fichier neuf et remet le compteur à zéro. La séquence se fait **sous lock**, via
les `…Unlocked` du parent.

- **IO / serveur** — un service qui tourne des semaines : on plafonne l'espace disque tout en
  gardant les N derniers fichiers.
- **Rendu / GPU** — capture d'un *trace* GPU verbeux borné en taille (utile au profilage).
- **Gameplay** — log de session de jeu plafonné, pour ne pas remplir le disque d'un joueur.

`SetMaxSize`/`SetMaxFiles` ajustent à chaud ; `Rotate()` force une rotation immédiate (et remet la
taille courante à 0 si elle réussit).

### `NkDailyFileSink` à fond — borner par le temps

L'autre stratégie de rotation : non plus par octets, mais par **jour**. À l'heure/minute fixée, il
archive le fichier courant avec un suffixe de date `.YYYYMMDD` et rouvre un fichier neuf, puis
**purge** ce qui dépasse `m_MaxDays` (`0` = illimité). Pour ne pas comparer la date à chaque ligne,
le contrôle est **throttlé à une fois par minute** (`m_LastCheck` en nanosecondes). La rotation
réelle passe par `PerformRotation` (Close → backup `.YYYYMMDD` → rename → nettoyage des vieux
fichiers → réouverture), toujours sous lock.

- **IO / serveur** — journaux quotidiens d'un serveur, faciles à archiver et à corréler par date.
- **Outils / audit** — traçabilité où « le log du 15 janvier » doit être identifiable d'un coup
  d'œil.
- **Gameplay** — séparer les sessions par jour dans une bêta longue durée.

`SetRotationTime(h, m)` change l'heure de bascule, `SetMaxDays` la rétention, et `Rotate()` force
une bascule — en notant qu'à la différence du mode automatique, elle **ne met pas à jour**
`m_CurrentDate`.

### `NkDistributingSink` à fond — diffuser à plusieurs

Le composite qui transforme « un message » en « le même message partout ». Il filtre **au niveau du
distributeur** (`ShouldLog`/`IsEnabled`) avant de **rediffuser** vers chaque sous-sink non-null,
en `O(n)` sous lock. La gestion est dynamique et **par adresse** : `RemoveSink`/`ContainsSink`
comparent les pointeurs sous-jacents (via `Get()`), pas le contenu. `GetSinks()` renvoie une **copie
figée**, sûre à parcourir hors lock même si la collection change par ailleurs.

- **Tout domaine** — console + fichier en même temps : on voit en direct *et* on garde la trace.
- **Outils / éditeur** — fichier sur disque + panneau de logs in-app, alimentés par le même flux.
- **IO / réseau** — fichier local + envoi distant pour la télémétrie.

Le partage du format a deux comportements distincts à connaître : `SetPattern` **propage** le même
pattern à tous, alors que `SetFormatter` le **clone** (en repassant par `GetPattern`) tout en
consommant la source ; les getters renvoient ceux du **premier** sous-sink non-null.

### `NkNullSink` à fond — l'absence assumée

Le *Null Object* : tout est *no-op*, `GetFormatter()` renvoie toujours `nullptr` et `GetPattern()`
une chaîne vide. Sans état mutable, il est thread-safe **sans verrou** et ne fait **ni allocation
ni I/O** — son coût est, à toutes fins utiles, nul.

- **Tout domaine** — valeur par défaut sûre là où un sink est requis, pour éviter les
  `if (sink != nullptr)`.
- **Tests** — absorber des logs sans polluer la sortie.
- **Configuration** — couper une catégorie (réseau, GPU…) en remplaçant son sink par un null.

### `NkAsyncLogger` à fond — sortir l'I/O de la boucle chaude

L'unique membre de cette page qui n'est **pas** un sink : il hérite de `NkLogger` (le header
trompeur s'appelle `NkAsyncSink.h`). Son principe : l'appel de log se contente d'**enfiler une
copie** du message dans une `NkQueue<NkLogMessage>` (côté producteur, rapide) ; un **thread worker**
dédié, réveillé par une condition variable, **dépile** et distribue aux sinks (côté consommateur).
On déplace ainsi le coût de l'écriture **hors** du thread principal — capital quand l'I/O est lente
(fichier, réseau) et que la boucle est temps réel.

Cycle de vie : le constructeur **ne démarre pas** le worker ; il faut `Start()` (idempotent, vérif
atomique) avant de logger, et `Stop()` (notify → join → flush final) pour terminer ; `IsRunning()`
lit l'état atomique. Le débordement est piloté par `NkAsyncOverflowPolicy` :

- `NK_DROP_OLDEST` (défaut) — on perd les plus anciens : convient quand seuls les logs **récents**
  comptent.
- `NK_DROP_NEWEST` — on jette le courant : préserve l'historique au prix des nouveaux.
- `NK_BLOCK` — on bloque l'appelant jusqu'à libération : aucune perte, mais peut **figer** la boucle
  si le worker ne suit pas.

Cas d'usage par domaine :

- **Rendu / GPU** — logger abondamment depuis la frame sans payer l'écriture disque dans la boucle.
- **Threading** — un logger central alimenté par de nombreux threads producteurs, drainé par un
  seul worker.
- **Audio** — depuis un thread audio temps réel, où une I/O bloquante provoquerait des *glitches* :
  l'enqueue rapide est essentiel.
- **IO / réseau** — un sink réseau lent ne ralentit plus l'application : il travaille côté worker.

Réglages : `SetMaxQueueSize`/`GetQueueSize` (capacité et messages en attente), `SetFlushInterval`
(cadence du worker, ms ; `0` = sur demande), `SetOverflowPolicy`. Pièges : **`Start()` avant** de
logger ; après `Stop()` l'enqueue **échoue** ; `NK_BLOCK` peut **bloquer indéfiniment** ; les sinks
branchés **doivent être thread-safe** (appelés depuis le worker, potentiellement concurrents avec
`Flush`) ; une exception dans un sink peut **terminer le worker** (pas de `try/catch` garanti).

### Le socle commun

- **Allocation NKMemory.** On crée les sinks via `memory::MakeShared<TSink>(...)`, jamais avec
  `new` ; on les détient en `NkSinkPtr` (partagé) ou `NkSinkUniquePtr` (exclusif). Voir
  [NKMemory](../../Foundation/NKMemory.md).
- **Filtrage hérité.** `SetLevel`/`ShouldLog`/`SetEnabled` viennent de `NkISink` : on configure le
  niveau **avant** d'ajouter le sink au logger.
- **Format découplé.** `SetPattern` (depuis une chaîne) ou `SetFormatter` (objet, par move) ;
  `GetFormatter()` est **non possédé** — jamais `delete`.
- **Thread-safety variable.** Garantie par `NkConsoleSink`, `NkFileSink` (+ rotations),
  `NkDistributingSink`, `NkAsyncLogger` (mutex/atomics internes) et `NkNullSink` (par absence
  d'état) ; **pas** par l'interface `NkISink` elle-même.

---

### Exemple

```cpp
#include "NKLogger/NkSink.h"
#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/Sinks/NkRotatingFileSink.h"
#include "NKLogger/Sinks/NkDistributingSink.h"
#include "NKLogger/NkAsyncSink.h"
using namespace nkentseu;

// Console couleur + fichier à rotation, regroupés derrière un diffuseur.
auto console = memory::MakeShared<NkConsoleSink>();          // stdout, erreurs→stderr
auto file    = memory::MakeShared<NkRotatingFileSink>(       // 5 Mo, 3 sauvegardes
                   "logs/app.log", 5 * 1024 * 1024, 3);

auto fanout = memory::MakeShared<NkDistributingSink>();
fanout->AddSink(console);
fanout->AddSink(file);
fanout->SetLevel(NkLogLevel::NK_INFO);                       // filtre une fois, pour les deux

// Pour ne pas bloquer la boucle chaude sur l'I/O : un logger asynchrone.
NkAsyncLogger logger("game", 8192, 1000);
logger.AddSink(fanout);
logger.SetOverflowPolicy(NkAsyncOverflowPolicy::NK_DROP_OLDEST);
logger.Start();                                             // OBLIGATOIRE avant de logger
// ... la frame logue sans payer l'écriture disque ...
logger.Stop();                                              // flush + vidage + join
```

---

[← Index NKLogger](README.md) · [Récap NKLogger](../NKLogger.md) · [Couche System](../README.md)
