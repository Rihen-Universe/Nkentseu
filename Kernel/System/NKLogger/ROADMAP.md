# NKLogger — Roadmap

État actuel (mai 2026) : Module mature et exploitable. API complète multi-style
(positionnel/printf/stream), 7 sinks fournis, formatter à pattern style spdlog,
singleton global avec macro `logger` et capture source automatique. Le sink
asynchrone est livré, mais la couche async transverse (NkAsyncLogger en tant que
classe à part entière) reste exposée via `NkAsyncSink` plutôt que comme logger
dédié.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Cœur logger (NkLogger, NkLog singleton, niveaux, formatter pattern) | Livré | — | — |
| Sinks de base (Console, File, Null, Distributing) | Livré | — | — |
| Sinks fichiers avancés (Rotating par taille, Daily par date) | Livré | — | — |
| Sink asynchrone (NkAsyncSink) | Livré | — | — |
| Battement de journal (NkLogHeartbeat, éteint par défaut) | Livré | — | — |
| Banc de coût par ligne (benchmark_smoke, autonome) | Livré | — | — |
| API fluide chaînable (Named/Level/Pattern/Source) | Livré | — | — |
| Capture source auto via macro `logger` (FILE/LINE/FUNC) | Livré | — | — |
| Sanitisation UTF-8 dans LogInternal | Livré | — | — |
| Tests unitaires (smoke, indexed format) | ⏳ ecrits, NON EXECUTES ici (execution desactivee par politique de workspace) | S | Haute |
| NkAsyncLogger comme classe dédiée (vs sink) | Partiel | M | Moyenne |
| Sink JSON natif (NkJsonSink) | TODO | M | Moyenne |
| Sink réseau (TCP/UDP/syslog) | TODO | L | Basse |
| Couleurs Win32 API fallback (sans ANSI) | TODO | S | Basse |
| Logger structuré (champs typés vs message) | TODO | XL | Basse |
| Métriques de monitoring intégrées (queue size, drop count) | TODO | S | Moyenne |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Cœur — Logger et hiérarchie de classes
- [NkLogger](src/NKLogger/NkLogger.h) : classe de base thread-safe avec multi-sink,
  filtrage par niveau, capture source via `Source(file, line, func)`, API fluide
  héritée et 3 styles de logging (positionnel via `NkFormat`, printf via
  `NkPrintf`, stream-style sans formatage).
- [NkLog](src/NKLogger/NkLog.h) : singleton thread-safe (Meyer's), API fluide
  `Named().Level().Pattern().Info(...)`, macro `logger` avec capture source auto.
- [NkLogLevel](src/NKLogger/NkLogLevel.h) : 7 niveaux TRACE..FATAL +
  conversions string round-trip.
- [NkLogMessage](src/NKLogger/NkLogMessage.h) : structure de message avec
  metadata (timestamp, logger name, source, level, payload).
- [NkLoggerFormatter](src/NKLogger/NkLoggerFormatter.h) : parser de pattern
  style spdlog (`%Y-%m-%d %H:%M:%S.%e`, `%L`, `%v`, `%n`, `%s:%#`, `%^...%$`
  pour couleurs).
- [NkRegistry](src/NKLogger/NkRegistry.h) : enregistrement de loggers nommés.

### Sinks — destinations livrées
Tous les sinks suivants implémentent `NkISink` ([NkSink](src/NKLogger/NkSink.h))
avec filtrage par niveau, formatter dédié, activation runtime, thread-safety
interne.

- [NkConsoleSink](src/NKLogger/Sinks/NkConsoleSink.h) : stdout/stderr + couleurs
  ANSI + détection `isatty` sur POSIX.
- [NkFileSink](src/NKLogger/Sinks/NkFileSink.h) : append fichier basique.
- [NkRotatingFileSink](src/NKLogger/Sinks/NkRotatingFileSink.h) : rotation
  automatique par taille avec `maxSize` et `maxBackups`.
- [NkDailyFileSink](src/NKLogger/Sinks/NkDailyFileSink.h) : rotation
  quotidienne à `(hour, minute)` configurable, rétention `maxDays`.
- [NkNullSink](src/NKLogger/Sinks/NkNullSink.h) : sink no-op pour désactivation
  et benchmarks.
- [NkDistributingSink](src/NKLogger/Sinks/NkDistributingSink.h) : composite
  broadcast vers N sinks fils.
- [NkAsyncSink](src/NKLogger/Sinks/NkAsyncSink.h) : sink asynchrone avec file
  d'attente, thread worker dédié, politiques de débordement
  `NK_DROP_OLDEST/NK_DROP_NEWEST/NK_BLOCK`, flush périodique configurable.

### Tests

> ⚠️ **CORRIGÉ DEUX FOIS LE 2026-08-15. La première correction était fausse, et
> le récit compte autant que le fait.**
>
> **Ce qui est vrai** : `benchmark_smoke.cpp` était **36 lignes entièrement en
> commentaire**, annoncées ici comme « micro-bench du chemin `Info()` ». Il ne
> mesurait rien — et ne mesurait de toute façon que le formateur, pas ce qu'une
> application paie. Réécrit, voir ci-dessous.
>
> **Ce que j'ai affirmé à tort, et qui est rétabli ici :**
> 1. *« le cadre `Unitest` n'existe pas »* — **faux**. Il est fourni par **Jenga**
>    (`…/Jenga/build/lib/Jenga/Unitest/`), pas par ce dépôt. Chercher dans
>    Nkentseu et conclure sur le monde, c'est mesurer le bon objet dans le
>    mauvais référentiel.
> 2. *« `jenga test` répond SUCCESS 6/6 sur une suite qu'il n'exécute pas »* —
>    **faux, et l'erreur est instructive**. J'avais lancé
>    `jenga test … || jenga build … --tests` : le « SUCCESS 6/6 » venait du
>    **second**, qui construit la bibliothèque et a raison de réussir.
>    `jenga test --project NKLogger` répond en réalité, en rouge :
>    *« Unit-test execution is disabled by workspace policy »*. **L'outillage est
>    honnête ; c'est mon enchaînement `||` qui a attribué la sortie d'une commande
>    à une autre.**
>
> **L'état réel** : les tests ne s'exécutent pas ici parce que Rodolf l'a
> **délibérément désactivé** au niveau des fichiers jenga — choix légitime, et
> annoncé clairement par l'outil. Ils ne sont donc **pas** de la couverture
> aujourd'hui, mais ils ne sont pas cassés pour autant.

- [benchmark_smoke.cpp](tests/benchmark_smoke.cpp) — ✅ **réécrit et
  fonctionnel** : coût par ligne (fichier / sans écriture / filtrée), autonome,
  son propre `main`, aucune dépendance à un cadre de test. Lancement :
  `bash Kernel/System/NKLogger/tests/build_bench.sh` puis
  `/tmp/nklogbench/bench_nklogger.exe`. Il **échoue** si une durée est nulle ou
  si une ligne filtrée ne coûte pas moins qu'une ligne émise.
- `test_smoke.cpp`, `test_indexed_format.cpp` — ⏳ **écrits, non exécutés ici**.
  Ils s'appuient sur `Unitest`, fourni par **Jenga**, et l'exécution des tests
  est **désactivée par politique de workspace** (`disableunittestexecution`) :
  décision de Rodolf, annoncée clairement par `jenga test`. Les réactiver
  relancerait aussi les projets, ce qui est le point à résoudre avant.
  **Ne pas les compter comme couverture tant qu'ils ne tournent pas** — mais ils
  ne sont ni cassés ni orphelins.

---

## En cours / TODO immédiat

### Tests
- Étendre la couverture : tests Rotating/Daily (rollover réel, multi-jours),
  AsyncSink (overflow policies, ordre de messages), Distributing (broadcast
  + filtrage indépendant par sink).
- Tester l'API fluide chaînée : `Named().Level().Source().Info(...)` doit
  consommer puis réinitialiser les metadata correctement.
- Tests concurrents : valider thread-safety avec TSan / helgrind sur
  scénarios multi-producteurs vers AsyncSink.

### NkAsyncLogger en tant que classe dédiée
Le README mentionne un `NkAsyncLogger` first-class avec `Start()/Stop()`,
`SetMaxQueueSize()`, `SetFlushInterval()`. Actuellement seul `NkAsyncSink`
existe. Soit le README est à corriger pour pointer vers `NkAsyncSink`, soit
créer une classe `NkAsyncLogger : public NkLogger` qui encapsule un AsyncSink
et expose l'API documentée.

### Métriques de monitoring
- `GetQueueSize()`, `GetMaxQueueSize()`, `GetDropCount()` sur `NkAsyncSink`.
- Compteur d'erreurs I/O par sink (fwrite failed, socket closed, etc.).
- Hook optionnel pour publier ces métriques vers un consommateur externe.

---

## À venir / À ajouter (futur proche)

### Sinks additionnels
- **NkJsonSink** : sortie structurée JSON ligne-par-ligne pour ingestion par
  ELK, Loki, Datadog, etc. Fields = level/timestamp/logger/file/line/msg.
- **NkSyslogSink** : intégration syslog POSIX (`syslog(3)`) et Event Log
  Windows. Mapping niveaux NK → priorités syslog.
- **NkNetworkSink** : transport UDP/TCP des logs vers un collecteur (cf.
  exemple esquissé dans Readme.md). Reconnect + backoff. Dépend de NKNetwork.
- **NkMemorySink** : capture in-memory pour tests (mentionné dans Readme, pas
  implémenté).
- **NkAndroidSink** : redirection `__android_log_print` vers logcat (Readme
  documente le besoin, pas livré).

### Plateforme
- Fallback couleurs Win32 API (`SetConsoleTextAttribute`) si
  `ENABLE_VIRTUAL_TERMINAL_PROCESSING` indisponible.
- Hyperliens cliquables (OSC 8) dans les terminaux modernes pour les sources
  `file:line`.

### Performance et features avancées
- Logger asynchrone first-class avec API documentée dans Readme (`Start`,
  `Stop`, `SetOverflowPolicy`, `GetQueueSize`).
- Logging structuré : `logger.Info().Field("user", id).Field("action", a).Emit()`
  au lieu du format string.
- Compression mémoire/disque des logs (LZ4 / Zstd dans les fichiers archivés).
- Hot-reload de configuration depuis un fichier `.nkconfig` ou des variables
  d'environnement (`NKLOG_LEVEL`, `NKLOG_PATTERN`, etc. — documentés dans
  Readme mais non câblés).
- Intégration `ConsolePanel` Noge : afficher les logs en temps réel avec
  filtres par niveau, recherche, et clic sur source.

---

## Livré — le BATTEMENT, et le vidage sur WARN (2026-08-15)

### `NkLogHeartbeat.h` — faire parler le journal PENDANT que l'application vit

`NkHeartbeat` : une porte temporelle, **éteinte par défaut** (`intervalMs = 0`),
sans fil ni minuterie — interrogée depuis la boucle qui tourne déjà. Elle ne
journalise rien elle-même : seule l'application sait ce qui vaut d'être dit.

**Pourquoi ce n'est pas un vidage plus fréquent**, et le diagnostic a été faux
deux fois avant d'être compris : le journal écrit une salve au démarrage puis
plus rien (mesuré 47 lignes puis **zéro** sur 15 s, Galaxy S22+). Le fichier
paraît alors *retenu* alors qu'il est *fini* — et la fermeture, qui ajoute ses
lignes d'extinction, imite à s'y méprendre un vidage de tampon. Or `NkFileSink`
appelle `setvbuf(_IONBF)` : **rien n'est en attente**, et vider plus souvent une
file vide ne produit aucune ligne.

**Mesuré sur `NkCameraDemos --demo=viewer`, processus vivant vérifié :**

| réglage | T+6 s | T+12 s | croissance |
|---|---|---|---|
| **éteint (défaut)** | 22 lignes | 22 lignes | **0** |
| `--beat=500` | 54 lignes | 65 lignes | 11 |

Cadence tenue : battements espacés de 516, 531, 507 ms pour 500 demandées (la
granularité est celle de la boucle, ~16 ms).

#### Ce que coûte une ligne — mesuré au bon endroit (`tests/benchmark_smoke.cpp`)

**Premier instrument, écarté** : le débit d'images. `--beat=500` contre
`--beat=50` (dix fois plus de lignes) donne 41,4 / 42,1 img/s contre
35,1 / 55,1 / 52,6 — intervalles chevauchants, et le régime le plus bavard
parfois le plus rapide. La bonne formulation n'est pas « indétectable » mais
**« cet instrument ne résout rien sous ±33 % »** : le viewer varie de 35 à
55 img/s sur une boucle plafonnée à 60, et **cette variance EST le plancher de
bruit**. Mesurer un écart sans avoir mesuré le bruit ne conclut rien.

**Second instrument, concluant** — coût par appel, immunisé contre la variance
de la boucle. 20 000 lignes par mesure, Windows 11 Pro 10.0.26100, 2026-08-16,
`feat/nkxr` :

| chemin | **Release** (4 exéc.) | **Debug** | rapport |
|---|---|---|---|
| ligne émise vers un puits **fichier** | **12,4 à 15,0 µs** | **25,8 µs** | ×1,9 |
| formatage + distribution, **sans écriture** | **0,54 à 0,59 µs** | **2,93 µs** | **×5** |
| ligne **filtrée** par le niveau (non émise) | **2 à 5 ns** | **137 ns** | **×30** |

⚠️ **Les deux configurations sont données parce que la conclusion change avec
elles.** « Une trace laissée dans le code est gratuite » est vrai en Release
(2–5 ns) et beaucoup moins en Debug (137 ns, trente fois plus) — encore
négligeable dans l'absolu, mais plus du tout du même ordre. *Un chiffre sans sa
configuration n'est pas faux : il est incomplet, et c'est pire, parce qu'on le
croit général.*

*La toute première exécution donne 26,4 µs / 1,36 µs / 4,9 ns — le fichier est
froid. Les valeurs ci-dessus sont celles du régime établi ; l'amorce d'une seule
ligne ne suffit pas à réchauffer le chemin disque.*

**Ce que ces chiffres disent :**
- **l'écriture pèse 96 %** du coût d'une ligne. C'est le prix de
  `setvbuf(_IONBF)` — chaque ligne part immédiatement, donc aucune n'est perdue
  au plantage. Le compromis est assumé, il est maintenant chiffré ;
- **le battement ne coûte rien** : 2 lignes/s à 500 ms ≈ **26 µs par seconde**,
  soit 0,003 % du temps. À `--beat=50`, 0,03 %. C'est **quatre ordres de
  grandeur** sous le plancher de bruit du premier instrument — voilà pourquoi il
  ne pouvait rien voir ;
- **une trace laissée dans le code coûte 2 à 5 ns** quand son niveau la rejette.
  Le filtrage se fait bien AVANT le travail. C'est le chiffre qui autorise à
  instrumenter sans se demander si ça se paie ;
- ⚠️ **et il corrige une idée reçue de ce module** : journaliser une ligne par
  image coûterait ~13 µs sur 16 667, soit **0,08 %** d'une image à 60 Hz. La
  raison de ne pas le faire n'est donc **pas** la performance — c'est la
  lisibilité. Un journal qui parle à chaque image est illisible, pas lent.

*Dette au passage, hors périmètre NKLogger* : le débit de `NkCameraDemos --demo=viewer`
varie de 35 à 55 img/s d'une exécution à l'autre sur une boucle plafonnée à 60
(57 % d'amplitude). L'instabilité elle-même n'est pas expliquée, et **tout banc
d'essai monté sur cette boucle hérite de ce plancher**.

### `NkConsoleSink` — vidage à partir de WARN, plus seulement ERROR

Un avertissement est souvent la **dernière chose** qu'une application dit avant
de mal finir. Vers un terminal la libc vide par ligne et personne ne voit la
différence ; **redirigée vers un fichier ou un tube** — ce que fait tout script
de test — la sortie passe en tampon de bloc et un plantage emporte exactement
les lignes qui l'expliquaient. Coût nul là où ça compte : le régime établi
n'émet aucune ligne.

---

## Bugs / quirks connus

### ✅ CATÉGORIE 3 SOLDÉE LE SOIR MÊME — mesurée sur Galaxy S22+ (2026-08-14)

L'appareil est revenu et le code a été **exercé**, pas seulement construit.
`NkCameraDemos` en Release sur SM-S906U1, Adreno 730, OpenGL ES 3.2 :

- le journal **parle** — nos lignes arrivent bien dans `logcat`, de
  `nkmain` jusqu'à `[NkCamera] Streaming started: 1280x720 @30 fps` ;
- **coût mesuré : 47 lignes au démarrage, puis ZÉRO.** Sur une fenêtre de 15
  secondes en régime établi, notre moteur n'écrit aucune ligne (27 lignes
  passent, toutes du système Android). **Rien ne journalise par image.**

Donc le « bruit de journal et coût de performance » annoncé comme effet
**attendu** ne se produit pas : c'est une salve à l'initialisation, puis le
silence. La différence entre attendu et constaté est exactement ce que cette
vérification servait à établir.

Le texte ci-dessous est conservé tel qu'il a été écrit **avant** la mesure : il
dit ce qu'on savait au moment de pousser, et c'est ce qui lui donne sa valeur.

---

### ⚠️ CATÉGORIE 3 — MODIFIÉ, JAMAIS EXERCÉ (état au moment de la poussée)

Le **puits console est actif en Release sur Android** depuis le 2026-08-14
(`NkLog.cpp` : la garde `!defined(NDEBUG)` accepte désormais aussi
`NKENTSEU_PLATFORM_ANDROID` / `__ANDROID__`). Motif : en Release sur téléphone,
le journal était **muet**, et une application qui ne dit rien ne se diagnostique
pas — c'est ce qui a coûté le plus de temps pendant le portage AR.

**Une vingtaine d'applications Android en héritent. Aucune n'a été relancée sur
matériel** : le téléphone a quitté `adb` en cours de session et la reconnexion
sans fil a été refusée. L'APK se construit et se signe — mais **construire n'est
pas exercer**.

Ce n'est **pas une réserve de prudence** : personne n'a vu tourner ce code. Effet
attendu — du bruit de journal et un coût de performance ; pas un plantage, pas
une fuite, réversible en une ligne. **Attendu**, justement : pas constaté.

**À faire dès qu'un appareil est disponible** — lancer une application Android
sans rapport avec l'AR, vérifier le débit du journal et le coût en images par
seconde. Tant que ce n'est pas fait, cette ligne reste.

*(Fait le soir même — voir la section ✅ ci-dessus. Le téléphone est revenu une
heure après la poussée.)*

- Les méthodes stream-style sans message (`Trace()`, `Debug()`, ...) sont
  déclarées dans le header mais leur intérêt est limité (loggent une chaîne
  vide). À documenter ou retirer.
- Les méthodes `Logf(level)` sans format dans la section "stream-style" font
  doublon avec `Log(level)`. Possible héritage d'une refacto.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (Types, Traits, Atomic),
  NKContainers (String, StringView, Vector, Queue, Format, NkFunction),
  NKMemory (SharedPtr, UniquePtr), NKThreading (Mutex, ConditionVariable,
  Thread, ScopedLock), NKPlatform (détection OS).
- **Modules au-dessus qui en dépendent** : tous (Runtime, RHI, Renderer,
  Application, Noge). Le module est consommé via la macro `logger` ou
  `NkLog::Instance()`.
