# NKLogger — documentation détaillée

Le module **NKLogger**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKLogger.md](../NKLogger.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat, ses
pièges réels et ses cas d'usage concrets (debug, production, multi-thread…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Core.md](Core.md) | Logger central `NkLogger` (multi-sink, thread-safe), singleton `NkLog` + macro `logger`, niveaux `NkLogLevel`, message `NkLogMessage`, registre `NkRegistry`. Styles positionnel `{0}` / printf `%s` / littéral. | `NkLogger.h`, `NkLog.h`, `NkLogLevel.h`, `NkLogMessage.h`, `NkRegistry.h` |
| [Formatting.md](Formatting.md) | Mise en forme des lignes via patterns style spdlog (`%Y`, `%L`, `%v`, `%^`/`%$`…), tokens `NkPatternToken`, patterns prédéfinis. | `NkLoggerFormatter.h` |
| [Sinks.md](Sinks.md) | Destinations des logs : interface `NkISink` puis console, fichier, rotation par taille, daily, distributing (multi), null, et le logger asynchrone `NkAsyncLogger`. | `NkSink.h`, `Sinks/NkConsoleSink.h`, `Sinks/NkFileSink.h`, `Sinks/NkRotatingFileSink.h`, `Sinks/NkDailyFileSink.h`, `NkAsyncSink.h`, `Sinks/NkDistributingSink.h`, `Sinks/NkNullSink.h` |

[← Récap NKLogger](../NKLogger.md) · [← Couche System](../README.md)
