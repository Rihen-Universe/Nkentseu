# Le cœur du journal

> Couche **System** · NKLogger · Émettre, filtrer et router les messages de journal : les niveaux
> `NkLogLevel`, l'événement `NkLogMessage`, le logger multi-sink `NkLogger`, le singleton fluide
> `NkLog` et le registre global `NkRegistry`.

Dès qu'un programme dépasse le « hello world », il faut **savoir ce qu'il fait sans s'arrêter dans un
débogueur** : quel système a chargé quoi, quelle frame a sauté, pourquoi une texture manque, sur
quel thread la collision a explosé. C'est le rôle d'un journal — non pas un simple `printf`, mais une
chaîne pensée pour la production : on **classe** chaque message par gravité, on **filtre** pour ne
garder que l'utile, on **route** vers plusieurs destinations (console colorée, fichier, réseau) et on
**date/identifie** automatiquement chaque ligne (horodatage, thread, fichier, ligne, fonction). Tout
le compromis tient en une idée : **un message coûte cher à formater, donc on décide le plus tôt
possible s'il doit seulement vivre.** Cette page vous apprend à émettre des journaux propres et à
choisir le bon point d'entrée.

NKLogger s'articule autour de cinq pièces qui montent en abstraction : `NkLogLevel` (le vocabulaire
de gravité), `NkLogMessage` (un événement daté), `NkLogger` (le moteur multi-sink), `NkLog` (un
logger global prêt à l'emploi avec API fluide) et `NkRegistry` (l'annuaire des loggers nommés). On
n'utilise au quotidien que `NkLog` via la macro `logger` ; le reste est là pour structurer un moteur
qui a beaucoup de sous-systèmes.

- **Namespace** : `nkentseu` (et **pas** `nkentseu::logger` — ce sous-namespace n'existe pas, malgré
  ce que suggèrent certains commentaires d'en-tête)
- **Headers** : `#include "NKLogger/NkLogger.h"` (le hub), `NkLog.h`, `NkRegistry.h`,
  `NkLogLevel.h`, `NkLogMessage.h`

---

## Les niveaux de gravité : `NkLogLevel`

Avant de logger quoi que ce soit, il faut un **vocabulaire de gravité** partagé. `NkLogLevel` est une
énumération fortement typée (`enum class … : uint8`) qui ordonne huit niveaux du plus bavard au plus
grave : `NK_TRACE` (0), `NK_DEBUG` (1), `NK_INFO` (2), `NK_WARN` (3), `NK_ERROR` (4), `NK_CRITICAL`
(5), `NK_FATAL` (6), et enfin la sentinelle `NK_OFF` (7) qui **coupe tout**. L'ordre n'est pas
décoratif : c'est lui qui rend le **filtrage** possible. Régler le seuil d'un logger à `NK_WARN`
revient à dire « ne me montre que ce qui est ≥ 3 » — `NK_WARN`, `NK_ERROR`, `NK_CRITICAL`, `NK_FATAL`
passent, tout le reste est jeté avant même d'être formaté.

Autour de l'enum, un jeu de fonctions libres de conversion, toutes sans état (donc thread-safe) et
renvoyant des chaînes en mémoire **statique** (qu'on ne libère jamais). `NkLogLevelToString` donne le
nom long lisible (« info », « error »), `NkLogLevelToShortString` le code à trois lettres majuscules
(« INF », « ERR », « WRN », « CRT ») pour des colonnes alignées. Les conversions inverses
`NkStringToLogLevel` (insensible à la casse, accepte l'alias « warning ») et `NkShortStringToLogLevel`
(sensible à la casse) lisent une configuration texte. Deux fonctions servent à la couleur :
`NkLogLevelToANSIColor` (séquence `\033[…m` pour un terminal) et `NkLogLevelToWindowsColor`
(attribut `WORD` pour `SetConsoleTextAttribute`). Enfin, `NkLogLevelIsEnabled(level, threshold)` est
le test de filtrage lui-même, défini **inline** dans le header (`level >= threshold`).

```cpp
NkLogLevel seuil = NkStringToLogLevel("warn");   // NK_WARN, depuis un fichier de config
if (NkLogLevelIsEnabled(NkLogLevel::NK_ERROR, seuil)) {
    // ERROR (4) >= WARN (3) → vrai : ce message passerait le filtre
}
```

Ce n'est **pas** une enum « plate » sans garde-fou : il faut écrire `NkLogLevel::NK_ERROR`, jamais
`NK_ERROR` tout court (sauf à l'intérieur des macros, qui qualifient déjà). Et attention au piège des
parsers : `NkStringToLogLevel` et `NkShortStringToLogLevel` retournent **silencieusement** `NK_INFO`
quand l'entrée est invalide ou `nullptr` — impossible de distinguer un vrai « info » d'une coquille.

> **En résumé.** `NkLogLevel` ordonne huit niveaux de `NK_TRACE` (verbeux) à `NK_FATAL`, plus
> `NK_OFF` qui coupe tout. L'ordre **est** le mécanisme de filtrage (`niveau >= seuil`). Les
> conversions texte/couleur sont stateless et statiques ; les parsers retombent silencieusement sur
> `NK_INFO` en cas d'erreur.

---

## L'événement daté : `NkLogMessage`

Quand un message franchit le filtre, il devient un **événement** : une structure `NkLogMessage` qui
empaquette tout ce qu'on voudra afficher ou archiver. C'est un agrégat copyable et déplaçable, **pas
thread-safe** (à synchroniser soi-même si on le partage entre threads). Ses champs sont rangés
scalaires d'abord pour l'alignement : `timestamp` (nanosecondes depuis l'epoch Unix, UTC), `threadId`
(identifiant numérique), `threadName` (nom lisible, optionnel), `level`, puis le `message` lui-même,
le `loggerName` émetteur, et les métadonnées de source `sourceFile` / `sourceLine` / `functionName`.

On le construit rarement à la main — le logger le fait pour nous — mais trois constructeurs existent :
le défaut (qui capture l'horodatage et le thread courants, et met `level = NK_INFO`), un constructeur
niveau + message + nom de logger optionnel, et un constructeur complet avec fichier/ligne/fonction.
Côté lecture, `NkLogMessage` offre une vraie boîte à outils temporelle : `GetLocalTime()` et
`GetUTCTime()` renvoient un `tm` (via les variantes thread-safe `localtime_r`/`gmtime_r`),
`GetMillis()` / `GetMicros()` / `GetSeconds()` convertissent l'horodatage nanoseconde dans l'unité
voulue. `IsValid()` dit si l'événement est exploitable (message non vide **et** horodatage > 0), et
`Reset()` le réinitialise pour le **recycler** dans un pool plutôt que d'en réallouer un.

```cpp
NkLogMessage ev(NkLogLevel::NK_WARN, "GPU timeout sur la frame", "render");
double t = ev.GetSeconds();        // horodatage en secondes fractionnaires
if (ev.IsValid()) { /* router vers les sinks */ }
ev.Reset();                        // prêt à resservir, sans réallocation
```

Ce n'est **pas** un simple `NkString` : c'est l'unité d'information complète que les *sinks* (console,
fichier…) et le *formatter* reçoivent et mettent en forme. Note ABI : la structure est exportée, ne
réordonnez jamais ses champs.

> **En résumé.** `NkLogMessage` = un événement de journal daté et identifié (horodatage ns, thread,
> niveau, message, source). Copyable/déplaçable mais **pas** thread-safe. `Get*Time/Millis/Micros/
> Seconds` pour le lire, `IsValid` pour le valider, `Reset` pour le recycler. C'est l'unité que voient
> les sinks et le formatter.

---

## Le moteur multi-sink : `NkLogger`

`NkLogger` est la pièce centrale : un logger **nommé**, **thread-safe** (mutex interne), qui prend un
message, le filtre, le formate et le **diffuse à plusieurs destinations** (les *sinks*). On le
construit avec un nom (`explicit NkLogger("render")`) ; son destructeur appelle `Flush()` puis
`ClearSinks()`, garantissant qu'aucune ligne tamponnée n'est perdue. Trois familles de réglages le
configurent : les **sinks** (où écrire), le **formatter** (comment mettre en forme) et le **niveau**
(quoi laisser passer).

Côté sinks, `AddSink` ajoute une destination (un `NkSharedPtr<NkISink>`, donc partageable entre
plusieurs loggers), `ClearSinks` les retire, `GetSinkCount` les compte. Côté mise en forme,
`SetFormatter` transfère l'ownership d'un `NkUniquePtr<NkLoggerFormatter>` et `SetPattern` crée ou met
à jour le formatter depuis un motif de style spdlog — les deux se propagent automatiquement aux sinks.
Côté filtrage, `SetLevel` / `GetLevel` règlent le seuil (défaut `NK_INFO`) et `ShouldLog(level)`
renvoie vrai si le message passerait — **c'est l'appel qu'on fait à la main avant un calcul coûteux**,
puisque le logger l'invoque déjà en tête de chaque méthode de log.

L'émission proprement dite se décline en **trois styles**. Le style **positionnel** (recommandé)
passe par `NkFormat` et des accolades indexées : `Log(level, format, args…)` et ses sept raccourcis
`Trace` / `Debug` / `Info` / `Warn` / `Error` / `Critical` / `Fatal`. Le style **printf** (legacy)
passe par `NkPrintf` et les `%s`/`%d` : `Logf(level, format, args…)`, une surcharge avec source
explicite `Logf(level, file, line, func, format, args…)`, et les raccourcis `Tracef` … `Fatalf`. Le
troisième style, dit **stream**, correspond aux surcharges sans argument. Toutes ces méthodes filtrent
d'abord via `ShouldLog`, formatent, puis convergent vers le point d'entrée privé `LogInternal`, qui
**assainit l'UTF-8** et diffuse le message à tous les sinks.

```cpp
NkLogger log("physics");
log.SetLevel(NkLogLevel::NK_DEBUG);
log.Info("Solver: {0} contacts en {1} ms", count, ms);   // positionnel (recommandé)
log.Errorf("échec collision objet %d", id);              // printf (legacy)
```

Pour attacher automatiquement le fichier/ligne/fonction, la méthode fluide
`Source(file, line, func)` mémorise ces métadonnées le temps du **prochain** log, puis les efface —
c'est ce que font les macros. Enfin `Flush()` vide tous les sinks, `GetName()` rend le nom,
`IsEnabled()` / `SetEnabled(bool)` permettent de couper un logger à chaud (les `Log*` deviennent des
no-op). Pour un usage rapide, sept macros prennent un **pointeur** de logger et capturent la source
automatiquement : `NK_LOG_TRACE(logger, …)` … `NK_LOG_FATAL(logger, …)`, plus `NK_LOG_FLUSH(logger)`.

> **En résumé.** `NkLogger` = un logger nommé, thread-safe, multi-sink. On configure sinks
> (`AddSink`), formatter (`SetPattern`/`SetFormatter`) et seuil (`SetLevel`). On émet en positionnel
> (`Info("…{0}…", x)`, recommandé), en printf (`Infof("…%d…", x)`) ou via les macros `NK_LOG_*` (qui
> attendent un **pointeur**). `ShouldLog` filtre avant tout calcul coûteux.

---

## Le logger global prêt à l'emploi : `NkLog`

Dans 90 % du code on ne veut pas instancier de logger : on veut juste écrire une ligne. `NkLog` est ce
raccourci — un **singleton** (Meyer's) qui hérite de tout `NkLogger`, déjà équipé d'un sink console
**et** d'un sink fichier à la première utilisation, avec une **API fluide** pour se reconfigurer en
une ligne. On y accède par `NkLog::Instance()`, on le configure une fois au démarrage avec
`Initialize(name, pattern, level)` (idempotent, thread-safe) et on le ferme avec `Shutdown()` (Flush
+ ClearSinks). Les méthodes fluides `Named(name)`, `Level(level)`, `Pattern(pattern)` et `Source(…)`
renvoient toutes `NkLog&`, donc se chaînent.

Mais l'usage idiomatique passe par la **macro** `logger`, qui développe en
`NkLog::Instance().Source(__FILE__, __LINE__, __func__)` : un accès au singleton **plus** la capture
automatique de la source, le tout chaînable. On écrit donc simplement :

```cpp
logger.Info("Joueur {0} connecté", name);
logger.Warnf("VRAM faible : %d Mo restants", mb);
NkLog::Instance().Level(NkLogLevel::NK_DEBUG);   // monter la verbosité à chaud
```

Attention à deux subtilités. D'abord, `logger` ici est une **référence** (on écrit `logger.Info(…)`,
avec un point), à ne pas confondre avec les macros `NK_LOG_*(logger, …)` qui attendent un **pointeur**
(`->`). Ensuite, comme `Source()` ne garde ses métadonnées que jusqu'au prochain `Log*`, il ne faut
**pas** enchaîner deux `Source()` (ou deux `logger`) sans un appel de log entre les deux — sinon la
première source est consommée par le second. L'alias `logger_src` est strictement équivalent à
`logger`.

> **En résumé.** `NkLog` = un logger global tout prêt (console + fichier), singleton, à configurer une
> fois via `Initialize`. La macro `logger` est l'entrée quotidienne : `logger.Info("…{0}…", x)`,
> source capturée automatiquement. C'est une **référence** (`.`), pas un pointeur — ne pas la
> confondre avec `NK_LOG_*`.

---

## L'annuaire des loggers : `NkRegistry`

Un moteur sérieux a beaucoup de sous-systèmes, et chacun veut **son** logger nommé (« render »,
« audio », « net »…), récupérable de partout sans le passer en paramètre. C'est le rôle de
`NkRegistry`, un singleton thread-safe qui **annuaire** les loggers par nom. On y accède par
`Instance()`, on l'initialise/ferme par `Initialize()` / `Shutdown()`. La gestion des entrées est
classique : `Register(logger)` (faux si le nom existe déjà), `Unregister(name)`, `Get(name)` (nullptr
si absent), `GetOrCreate(name)` (crée à la volée), `Exists(name)`, `Clear()`, plus l'introspection
`GetLoggerNames()` et `GetLoggerCount()`.

À cela s'ajoute une **configuration globale** appliquée transversalement : `SetGlobalLevel` /
`GetGlobalLevel`, `SetGlobalPattern` / `GetGlobalPattern`, `FlushAll()` pour vider tous les loggers
d'un coup, et la notion de logger par défaut (`SetDefaultLogger`, `GetDefaultLogger`,
`CreateDefaultLogger`). Pour éviter de taper `NkRegistry::Instance().…` partout, cinq **fonctions
libres** font le pont : `GetLogger(name)`, `GetDefaultLogger()`, `CreateLogger(name)`, `DropAll()` et
`Drop(name)`.

```cpp
auto render = nkentseu::GetLogger("render");      // ou CreateLogger si absent
render->Info("Pipeline initialisé");
NkRegistry::Instance().SetGlobalLevel(NkLogLevel::NK_WARN);  // tout le moteur passe en WARN
```

Deux points de vigilance. Le stockage interne est un **vecteur de paires** (nom → logger), donc chaque
recherche par nom est en **O(n)** : ce n'est pas une vraie table de hachage, on n'enregistre pas des
milliers de loggers dans une boucle chaude. Et méfiez-vous des exemples en commentaire qui appellent
`NkRegistry::CreateLogger("core")` comme une **méthode statique** : ce symbole n'existe pas ; seule la
**fonction libre** `CreateLogger` est réelle.

> **En résumé.** `NkRegistry` = l'annuaire global des loggers nommés, thread-safe, avec config globale
> (niveau/pattern/flush). On l'utilise via les fonctions libres `GetLogger` / `CreateLogger` /
> `GetDefaultLogger` / `Drop` / `DropAll`. Recherche **O(n)** (vecteur de paires, pas une hashmap) :
> annuaire de sous-systèmes, pas de millions d'entrées.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (comportement, cas
d'usage) dans la « Référence complète ». Complexités/inline entre crochets quand c'est utile.

### `enum class NkLogLevel : uint8` + fonctions libres

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Valeurs | `NK_TRACE` `NK_DEBUG` `NK_INFO` `NK_WARN` `NK_ERROR` `NK_CRITICAL` `NK_FATAL` | Gravité croissante (0 → 6). |
| Sentinelle | `NK_OFF` (7) | Désactive tout logging. |
| Vers texte | `NkLogLevelToString` `[O(1)]`, `NkLogLevelToShortString` `[O(1)]` | Nom long / code 3 lettres (mémoire statique). |
| Depuis texte | `NkStringToLogLevel` `[O(n)]`, `NkShortStringToLogLevel` | Parse (casse insensible / sensible) ; fallback `NK_INFO`. |
| Couleur | `NkLogLevelToANSIColor` `[O(1)]`, `NkLogLevelToWindowsColor` `[O(1)]` | Séquence ANSI / attribut Win32. |
| Filtrage | `NkLogLevelIsEnabled(level, threshold)` `[inline, O(1)]` | `level >= threshold`. |

### `struct NkLogMessage`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champs | `timestamp` `threadId` `threadName` `level` `message` `loggerName` `sourceFile` `sourceLine` `functionName` | Événement complet (ne pas réordonner — ABI). |
| Construction | `NkLogMessage()`, `(lvl, msg, logger="")`, `(lvl, msg, file, line, func, logger="")` | Défaut (capture horodatage/thread) / niveau+msg / complet. |
| État | `Reset()`, `IsValid()` | Recycler / valider (message non vide ET timestamp > 0). |
| Temps | `GetLocalTime()` `GetUTCTime()` (→ `tm`), `GetMillis()` `GetMicros()` `GetSeconds()` | Heure locale/UTC thread-safe ; horodatage en ms/µs/s. |

### `class NkLogger`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `explicit NkLogger(name)`, `~NkLogger()` | Construit nommé ; détruit avec Flush + ClearSinks. |
| Sinks | `AddSink` `ClearSinks` `GetSinkCount` | Ajouter / vider / compter les destinations. |
| Formatage | `SetFormatter` `SetPattern` | Formatter (ownership) / motif spdlog ; propagés aux sinks. |
| Niveau | `SetLevel` `GetLevel` `ShouldLog` | Seuil min / lecture / test de filtrage précoce. |
| Log positionnel | `Log(level,…)` · `Trace/Debug/Info/Warn/Error/Critical/Fatal(…)` | Via `NkFormat`, accolades `{0}` (recommandé). |
| Log printf | `Logf(level,…)` (+ source explicite) · `Tracef…Fatalf` | Via `NkPrintf`, `%s`/`%d` (legacy). |
| Log stream | `Log(level)` · `Trace()…Fatal()` · `Logf(level)` · `Tracef()…Fatalf()` | Surcharges sans argument. |
| Source | `virtual Source(file, line, func)` | Capture métadonnées du prochain log (fluide). |
| Contrôle | `Flush()`, `GetName()`, `IsEnabled()`, `SetEnabled(bool)` | Vider / nom / activer-désactiver. |
| Macros | `NK_LOG_TRACE…NK_LOG_FATAL(logger,…)`, `NK_LOG_FLUSH(logger)` | Log + source auto ; `logger` = **pointeur**. |

### `class NkLog : public NkLogger`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Singleton | `static Instance()`, `static Initialize(name, pattern, level)`, `static Shutdown()` | Accès unique / config / fermeture. |
| Fluide | `Named(name)` `Level(level)` `Pattern(pattern)` `Source(…) override` | Reconfiguration chaînable (`NkLog&`). |
| Logging | (hérité de `NkLogger`) | Tous les styles via l'infra parente. |
| Macros | `logger`, `logger_src` | `Instance().Source(…)` ; `logger` = **référence** chaînable. |

### `class NkRegistry` + fonctions libres

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Singleton | `static Instance()`, `static Initialize()`, `static Shutdown()` | Accès / init / fermeture. |
| Loggers | `Register` `Unregister` `Get` `GetOrCreate` `Exists` `Clear` `GetLoggerNames` `GetLoggerCount` | Gestion par nom `[O(n)]`. |
| Config globale | `Set/GetGlobalLevel` `Set/GetGlobalPattern` `FlushAll` `Set/GetDefaultLogger` `CreateDefaultLogger` | Réglages transversaux. |
| Fonctions libres | `GetLogger` `GetDefaultLogger` `CreateLogger` `DropAll` `Drop` | Raccourcis namespace `nkentseu`. |

---

## Référence complète

Chaque élément repris en détail. Les triviaux (accesseurs, conversions) sont brefs ; le filtrage,
l'émission et le routage sont traités **à fond**, avec leurs usages dans les domaines du moteur.

### `NkLogLevel` et ses conversions à fond

Le point clé est que l'**ordre numérique est le filtre**. Un sous-système qui fixe son seuil règle du
même coup tout ce qu'il laissera passer, et la décision est un simple `>=` (`NkLogLevelIsEnabled`,
inline) — c'est ce qui rend le filtrage assez bon marché pour le mettre en tête de chaque log. Les
conversions servent surtout aux **outils** :

- **Outils / éditeur** — une console de log filtrable par niveau (slider TRACE→FATAL) lit le seuil
  via `NkStringToLogLevel` depuis l'UI et l'affiche via `NkLogLevelToString`.
- **UI / 2D** — colorer chaque ligne d'une console in-game : `NkLogLevelToANSIColor` pour un terminal,
  `NkLogLevelToWindowsColor` pour une console Win32 native.
- **IO / config** — un fichier `.ini` qui dit `log_level = warn` se relit en `NK_WARN` ; les codes
  courts (`NkLogLevelToShortString`) alignent joliment les colonnes d'un fichier de log.
- **Threading** — comme les fonctions sont stateless et renvoient de la mémoire statique, plusieurs
  threads peuvent les appeler sans verrou.

Piège récurrent : les deux parsers retombent sur `NK_INFO` en silence si l'entrée est invalide —
validez la chaîne en amont si la distinction compte.

### `NkLogMessage` à fond

La structure est l'**unité d'information** qui circule entre le logger et ses sinks. Le constructeur
par défaut paie un petit coût (un appel d'horloge, ~20-50 cycles) pour capturer l'horodatage
nanoseconde et le thread courant ; les deux autres constructeurs délèguent à celui-ci puis remplissent
les champs fournis. Ses usages typiques, par domaine :

- **Outils / éditeur** — un panneau de log retient les `NkLogMessage` dans un tampon circulaire ;
  `GetLocalTime()` formate l'heure pour l'humain, `level` colorie la ligne, `sourceFile/Line` ouvrent
  le fichier au double-clic.
- **IO / réseau** — un sink réseau sérialise `timestamp` + `level` + `message` pour un agrégateur
  distant ; `GetMicros()` donne une précision suffisante pour corréler des événements de plusieurs
  machines.
- **Threading** — `threadId` / `threadName` permettent de démêler des logs entrelacés venant de la
  pool de threads (jobs de rendu, chargement asynchrone). La structure elle-même n'étant **pas**
  thread-safe, on en passe une copie par message, jamais une instance partagée.
- **Profilage / outils** — `Reset()` autorise un **pool** d'événements réutilisés pour éviter les
  allocations dans une boucle chaude qui logge beaucoup.

`IsValid()` (message non vide **et** horodatage > 0) sert de garde avant de router un événement
reconstruit ou désérialisé.

### `NkLogger` : émettre, filtrer, router à fond

Le logger fait trois choses, et il faut comprendre **dans quel ordre**. (1) Il **filtre tôt** :
chaque méthode de log appelle `ShouldLog` en première ligne, donc un message sous le seuil ne coûte
quasiment rien — c'est tout l'intérêt de ne pas formater avant de savoir. (2) Il **formate** selon le
style choisi. (3) Il **route** via `LogInternal`, qui assainit l'UTF-8 puis pousse le message à chaque
sink, le tout sous le mutex interne (thread-safe).

**Le filtrage précoce** est une habitude de performance à prendre quand l'argument coûte cher à
construire :

```cpp
if (log.ShouldLog(NkLogLevel::NK_DEBUG))
    log.Debug("Dump scène : {0}", scene.ExpensiveSnapshot());  // snapshot évité si DEBUG filtré
```

**Les trois styles** ne s'équivalent pas. Le **positionnel** (`Info("…{0}…", x)`, via `NkFormat`) est
recommandé : type-safe, lisible, avec mini-spécificateurs (`{0:props}`). Le **printf** (`Infof("…%d…",
x)`, via `NkPrintf`) est legacy mais pratique pour du code porté. Le **stream** correspond aux
surcharges sans argument. Subtilité réelle à connaître : les méthodes nommées sont à la fois des
templates variadiques **et** des surcharges sans paramètre, si bien que `log.Info("msg")` résout vers
la version **template** (pack vide), traitant la chaîne telle quelle — pas vers `Info()`.

Usages du logger nommé, par domaine :

- **Rendu / GPU** — un logger « render » route ses messages vers un fichier *et* la console ; les
  callbacks de validation Vulkan/DX12 y déversent leurs warnings, filtrés à `NK_WARN` en release pour
  ne pas noyer la sortie.
- **ECS / gameplay** — chaque système (collision, IA, animation) prend `GetOrCreate("ai")` etc., ce
  qui permet de monter la verbosité d'**un seul** sous-système sans toucher aux autres.
- **Audio** — un sink dédié horodaté aide à corréler un glitch sonore avec l'événement gameplay qui
  l'a déclenché (`SetEnabled(false)` coupe le bruit une fois le bug compris).
- **IO / réseau** — `Flush()` est crucial avant un crash volontaire (`NK_FATAL` puis `abort`) pour
  s'assurer que la dernière ligne touche le disque ; le destructeur le fait aussi automatiquement.
- **Threading** — le mutex interne sérialise les écritures concurrentes ; les sinks partagés
  (`NkSharedPtr`) peuvent être attachés à plusieurs loggers sans duplication.
- **Outils / éditeur** — `SetPattern` reconfigure à chaud le format affiché (timestamp court vs
  détaillé) sans recompiler.

Les **macros** `NK_LOG_*(logger, …)` capturent `__FILE__/__LINE__/__func__` automatiquement et
n'évaluent les arguments que si `ShouldLog` passe. Deux pièges : elles attendent un **pointeur**
(elles déréférencent par `->`), et comme elles développent en `if` sans accolades, méfiez-vous d'un
`else` qui suivrait (dangling-else) — entourez-les d'un bloc en cas de doute.

### `NkLog` et la macro `logger` à fond

`NkLog` est le chemin court vers un journal : un singleton qui s'auto-équipe d'un sink console et d'un
sink fichier dès sa première utilisation. On le règle une fois au boot (`Initialize` ou les fluides
`Named/Level/Pattern`) et on logge partout via la macro `logger`. C'est l'outil de **tous les jours** :

- **Gameplay / prototypage** — `logger.Info("Score : {0}", score);` sans aucune plomberie, source
  capturée toute seule.
- **Démarrage moteur** — `NkLog::Initialize("nkentseu", pattern, NkLogLevel::NK_DEBUG)` en début de
  `main`, `NkLog::Shutdown()` à la fin pour flusher proprement.
- **Débogage ponctuel** — `NkLog::Instance().Level(NkLogLevel::NK_TRACE)` monte la verbosité globale
  le temps d'une session, puis on la redescend.

Deux règles d'or, déjà signalées : `logger` est une **référence** (`logger.Info`), à ne pas confondre
avec le **pointeur** attendu par `NK_LOG_*` ; et `Source()` ne survit qu'jusqu'au prochain `Log*`,
donc jamais deux `logger`/`Source()` d'affilée sans log entre eux (la première source serait
consommée). `logger_src` est un simple alias.

### `NkRegistry` à fond

Le registre résout le problème du « comment retrouver le logger du sous-système X depuis n'importe
où ». C'est un annuaire global thread-safe, plus une couche de **configuration transversale** :

- **Architecture moteur** — au boot, chaque module enregistre son logger (`Register`) ou le crée
  paresseusement (`GetOrCreate("render")`) ; ailleurs, `GetLogger("render")` le récupère sans
  l'injecter en paramètre.
- **Outils / éditeur** — `GetLoggerNames()` alimente une liste déroulante « par catégorie » dans la
  console de log ; `SetGlobalLevel` agit comme un master-volume de verbosité sur tout le moteur.
- **IO / shutdown** — `FlushAll()` avant fermeture garantit que tous les fichiers de log sont à jour ;
  `DropAll()` / `Drop(name)` nettoient à la fin d'une scène ou d'un test.
- **Tests** — `Clear()` repart d'un registre vierge entre deux cas de test.

Limites à garder en tête : la recherche est **O(n)** (vecteur de paires, pas une hashmap) — parfait
pour une poignée de sous-systèmes, à éviter dans une boucle serrée. Et la « méthode statique »
`NkRegistry::CreateLogger` des commentaires n'existe pas : utilisez la **fonction libre**
`nkentseu::CreateLogger`.

### Pièges transverses

- **Namespace** — tout est dans `nkentseu`. Les `using namespace nkentseu::logger;` qu'on croise dans
  de vieux exemples ne compilent pas : ce sous-namespace n'existe pas.
- **Pointeur vs référence** — `NK_LOG_*(logger, …)` veut un pointeur (`->`) ; la macro `logger` produit
  une référence (`.`). Ne pas mélanger.
- **Source consommée** — les métadonnées de `Source()` sont vidées au premier `Log*` ; ne pas chaîner
  deux captures sans log entre.
- **Parsers silencieux** — `NkStringToLogLevel` / `NkShortStringToLogLevel` retombent sur `NK_INFO`
  sans signaler l'erreur.
- **Ownership** — sinks en `NkSharedPtr` (partagés entre loggers), formatter en `NkUniquePtr`
  (transfert via `SetFormatter`).

---

### Exemple

```cpp
#include "NKLogger/NkLog.h"
#include "NKLogger/NkRegistry.h"
using namespace nkentseu;

// Boot : configurer le logger global une fois.
NkLog::Initialize("nkentseu", NkLoggerFormatter::NK_DETAILED_PATTERN, NkLogLevel::NK_INFO);

// Usage quotidien via la macro `logger` (référence + source auto).
logger.Info("Joueur {0} connecté depuis {1}", name, ip);   // positionnel (recommandé)
logger.Warnf("VRAM faible : %d Mo", mb);                    // printf (legacy)

// Filtrage précoce avant un calcul coûteux.
if (NkLog::Instance().ShouldLog(NkLogLevel::NK_DEBUG))
    logger.Debug("Snapshot scène : {0}", scene.ExpensiveSnapshot());

// Un logger par sous-système, retrouvé via le registre.
auto render = CreateLogger("render");        // fonction libre
render->Error("Échec compilation shader {0}", path);

// Couper toute la verbosité du moteur le temps d'un benchmark.
NkRegistry::Instance().SetGlobalLevel(NkLogLevel::NK_OFF);

// Fermeture propre : flush garanti.
NkRegistry::Instance().FlushAll();
NkLog::Shutdown();
```

---

[← Index NKLogger](README.md) · [Récap NKLogger](../NKLogger.md) · [Couche System](../README.md)
