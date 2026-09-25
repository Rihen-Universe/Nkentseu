# NKLogger — documentation détaillée

**AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**

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
| [Puits-et-niveaux.md](Puits-et-niveaux.md) | **La table de verite** : quel puits est branche par defaut, dans quelle configuration, ou il ecrit, et ce que l'utilisateur voit. Plus la table des NIVEAUX et les DEUX filtres en serie. Ecrite le 25/09/2026 apres qu'un utilisateur n'ait rien vu sortir de `logger.Debug()`. Contient aussi le cas `std::cout` qui ne sort pas, qui est un defaut A PART. | mesures : `Sandbox/System/NKLoggerSilence/` |

[← Récap NKLogger](../NKLogger.md) · [← Couche System](../README.md)
