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
| API fluide chaînable (Named/Level/Pattern/Source) | Livré | — | — |
| Capture source auto via macro `logger` (FILE/LINE/FUNC) | Livré | — | — |
| Sanitisation UTF-8 dans LogInternal | Livré | — | — |
| Tests unitaires (smoke, indexed format) | Partiel | S | Haute |
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
- [test_smoke.cpp](tests/test_smoke.cpp) : round-trip des niveaux, formatter de
  base.
- [test_indexed_format.cpp](tests/test_indexed_format.cpp) : formatage
  positionnel `{0}`, `{1:hex}`, etc.
- [benchmark_smoke.cpp](tests/benchmark_smoke.cpp) : micro-bench du chemin
  Info().

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

## Bugs / quirks connus
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
