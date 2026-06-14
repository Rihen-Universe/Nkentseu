# NKLogger

> Couche **System** · Le système de journalisation du moteur : logger central, niveaux de
> sévérité, messages, formatage configurable et sinks multiples (console, fichier, rotation,
> daily, async, distributing, null) avec registre global.

Dès qu'une partie du moteur veut **dire ce qu'elle fait** — une erreur de chargement, l'état
d'un sous-système, une trace de debug — elle passe par NKLogger. C'est l'oreille du moteur :
chaque module y écrit, et le développeur décide où ces messages partent (terminal, fichier
tournant, fichier du jour, file asynchrone) et à quel niveau de détail. Comprendre NKLogger,
c'est savoir filtrer le bruit pendant le développement et garder une trace fiable en
production.

L'usage courant tient en une ligne : la macro `logger` donne le logger par défaut avec capture
automatique du fichier/ligne/fonction, et on l'utilise avec un format positionnel —
`logger.Info("Joueur {0} connecté", nom);`. Le reste de l'API (sinks, formatter, registre)
sert à câbler les destinations et la mise en forme.

- **Namespace** : `nkentseu` (et non `nkentseu::logger` malgré certains commentaires d'en-tête)
- **Header parapluie** : `#include "NKLogger/NkLogger.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Logger un message, choisir un niveau, capturer la source | [Le cœur](NKLogger/Core.md) |
| Comprendre les niveaux de sévérité (TRACE → FATAL → OFF) | [Le cœur](NKLogger/Core.md) |
| Récupérer / créer un logger nommé partagé | [Le cœur](NKLogger/Core.md) |
| Personnaliser l'apparence des lignes (date, niveau, couleurs) | [Le formatage](NKLogger/Formatting.md) |
| Envoyer les logs vers la console, un fichier, plusieurs destinations | [Les sinks](NKLogger/Sinks.md) |
| Faire tourner / dater les fichiers de log, logger en asynchrone | [Les sinks](NKLogger/Sinks.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat, ses
pièges réels et ses cas d'usage concrets (debug, production, multi-thread…).

---

## Aperçu des familles

- **Le cœur** (`NkLogger.h`, `NkLog.h`, `NkLogLevel.h`, `NkLogMessage.h`, `NkRegistry.h`) — la
  classe `NkLogger` (multi-sink, thread-safe), le singleton `NkLog` avec son API fluide et la
  macro `logger`, l'énumération `NkLogLevel` (TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL/FATAL/OFF) et
  ses conversions, le conteneur d'événement `NkLogMessage`, et le `NkRegistry` global des
  loggers nommés. Trois styles d'appel : positionnel `{0}` (recommandé), printf `%s/%d`
  (`*f`), et littéral.
- **Le formatage** (`NkLoggerFormatter.h`) — le `NkLoggerFormatter` qui transforme un
  `NkLogMessage` en ligne texte selon un **pattern** style spdlog (`%Y-%m-%d`, `%L`, `%v`,
  `%^`/`%$` pour la couleur…), avec une dizaine de patterns prédéfinis (`NK_DEFAULT_PATTERN`,
  `NK_DETAILED_PATTERN`, `NK_ISO8601_PATTERN`…) et le token `NkPatternToken`.
- **Les sinks** (`NkSink.h` + `Sinks/*.h`) — l'interface `NkISink` et ses implémentations :
  `NkConsoleSink` (couleur, multiplateforme), `NkFileSink` (persistance fichier),
  `NkRotatingFileSink` (rotation par taille), `NkDailyFileSink` (rotation quotidienne),
  `NkDistributingSink` (broadcast vers plusieurs sinks), `NkNullSink` (no-op), plus le logger
  asynchrone `NkAsyncLogger` (`NkAsyncSink.h`, à thread worker).

---

## Index des 14 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkLogger.h` | Parapluie + classe `NkLogger`, macros `NK_LOG_*`. | [Le cœur](NKLogger/Core.md) |
| `NkLog.h` | Singleton `NkLog`, macro `logger`. | [Le cœur](NKLogger/Core.md) |
| `NkLogLevel.h` | enum `NkLogLevel` + conversions. | [Le cœur](NKLogger/Core.md) |
| `NkLogMessage.h` | struct `NkLogMessage`. | [Le cœur](NKLogger/Core.md) |
| `NkRegistry.h` | `NkRegistry` + free functions de loggers. | [Le cœur](NKLogger/Core.md) |
| `NkLoggerFormatter.h` | `NkLoggerFormatter`, `NkPatternToken`, patterns. | [Le formatage](NKLogger/Formatting.md) |
| `NkSink.h` | interface `NkISink` + alias de pointeurs. | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkConsoleSink.h` | `NkConsoleSink` (console couleur). | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkFileSink.h` | `NkFileSink` (fichier). | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkRotatingFileSink.h` | `NkRotatingFileSink` (rotation par taille). | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkDailyFileSink.h` | `NkDailyFileSink` (rotation quotidienne). | [Les sinks](NKLogger/Sinks.md) |
| `NkAsyncSink.h` | `NkAsyncLogger` (logger asynchrone). | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkDistributingSink.h` | `NkDistributingSink` (broadcast multi-sink). | [Les sinks](NKLogger/Sinks.md) |
| `Sinks/NkNullSink.h` | `NkNullSink` (no-op). | [Les sinks](NKLogger/Sinks.md) |

---

[← Couche System](README.md) · [Index du wiki](../README.md)
