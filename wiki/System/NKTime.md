# NKTime

> Couche **System** · La gestion du temps du moteur : horloges, chronomètres, temps écoulé,
> durées typées, intervalles, dates calendaires, fuseaux horaires et constantes de conversion.

Dès qu'une chose doit **avancer dans le temps**, **mesurer une durée**, **se déclencher après
un délai** ou **porter une date**, elle passe par NKTime. C'est l'horloge commune du moteur :
la boucle de jeu en tire son delta-time, le profilage mesure ses chronos, les timers spécifient
leurs périodes, les logs et les sauvegardes portent des dates. Tout est **zéro-STL** et taillé
pour la précision (`QueryPerformanceCounter` sous Windows, `CLOCK_MONOTONIC` sous POSIX).

Le module distingue volontairement plusieurs notions de temps, qu'il ne faut pas confondre :
on **mesure** avec un chronomètre (`NkChrono`, `NkClock`), le résultat d'une mesure est un
`NkElapsedTime` (float64), on **spécifie** une durée à venir avec `NkDuration` (int64 ns), on
**décompose** un intervalle calendaire avec `NkTimeSpan`, et on **date** avec `NkDate` /
`NkTimeZone`.

- **Namespace** : `nkentseu` (les constantes de conversion sont dans le sous-namespace `nkentseu::time`)
- **Header parapluie** : `#include "NKTime/NkTimes.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Faire avancer une boucle de jeu, lire le delta/FPS, gérer pause et time scale | [Les horloges](NKTime/Clocks.md) |
| Mesurer une durée écoulée (profilage, chrono), dormir, céder le quantum | [Les horloges](NKTime/Clocks.md) |
| Représenter le résultat d'une mesure et le convertir en ns/us/ms/s | [Les horloges](NKTime/Clocks.md) |
| Spécifier une durée (sleep, timeout, période timer) en valeur typée | [Les durées](NKTime/Durations.md) |
| Décomposer un intervalle en jours/heures/minutes/secondes (style .NET) | [Les durées](NKTime/Durations.md) |
| Manipuler une date calendaire (validation, bissextiles, ISO 8601) | [Les dates](NKTime/Dates.md) |
| Convertir entre UTC et heure locale, gérer les fuseaux et le DST | [Les dates](NKTime/Dates.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat et
ses cas d'usage concrets (boucle de jeu, profilage, timers, journalisation, planification…).

---

## Aperçu des familles

- **Horloges & mesure** (`NkClock.h`, `NkChrono.h`, `NkElapsedTime.h`, `NkTimeConstants.h`) —
  `NkClock` orchestre la boucle de jeu (delta, total, FPS, frameCount, time scale, pause).
  `NkChrono` est le chronomètre haute précision (`Elapsed`/`Reset`, `Now`, `Sleep`,
  `YieldThread`). `NkElapsedTime` est le **résultat** d'une mesure (4 unités float64
  précalculées). Le sous-namespace `nkentseu::time` regroupe les constantes de conversion
  (`NS_PER_SECOND`…), source de vérité unique.
- **Durées & intervalles** (`NkDuration.h`, `NkTimeSpan.h`, `NkTimes.h`) — `NkDuration`
  **spécifie** une durée (int64 ns, mutable, entièrement `constexpr`, fabriques
  `FromSeconds/…`). `NkTimeSpan` est une durée signée **décomposée** en composants calendaires
  (style .NET `TimeSpan`). `NkTimes.h` est le **header parapluie** du module.
- **Dates & fuseaux** (`NkDate.h`, `NkTimeZone.h`) — `NkDate` est une date grégorienne validée
  (bissextiles, ISO 8601, noms de mois FR). `NkTimeZone` est immuable et thread-safe
  (`GetLocal`/`GetUtc`/`FromName`) et convertit entre UTC et local en tenant compte du DST.

---

## Index des 10 headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NkTimes.h` | Parapluie (inclut tout le module). | — |
| `NkTimeApi.h` | Macros d'export (`NKENTSEU_TIME_CLASS_EXPORT`, `NKTIME_NODISCARD`…). | — |
| `NkTimeConstants.h` | Constantes de conversion `nkentseu::time::NS_PER_*`. | [Horloges](NKTime/Clocks.md) |
| `NkElapsedTime.h` | `NkElapsedTime` (résultat de mesure, 4 unités float64). | [Horloges](NKTime/Clocks.md) |
| `NkChrono.h` | `NkChrono` (chronomètre, `Now`, `Sleep`, `YieldThread`). | [Horloges](NKTime/Clocks.md) |
| `NkClock.h` | `NkClock` + `NkClock::NkTime` (boucle de jeu, delta/FPS). | [Horloges](NKTime/Clocks.md) |
| `NkDuration.h` | `NkDuration` (durée typée int64 ns, constexpr). | [Durées](NKTime/Durations.md) |
| `NkTimeSpan.h` | `NkTimeSpan` (intervalle signé décomposé). | [Durées](NKTime/Durations.md) |
| `NkDate.h` | `NkDate` (date grégorienne validée). | [Dates](NKTime/Dates.md) |
| `NkTimeZone.h` | `NkTimeZone` (fuseau, conversions UTC/local, DST). | [Dates](NKTime/Dates.md) |

> La classe horaire `NkTime` (heure du jour) est définie dans `NkTimes.h` aux côtés du
> parapluie ; ne pas la confondre avec le type imbriqué `NkClock::NkTime` (snapshot de frame).

---

[← Couche System](README.md) · [Index du wiki](../README.md)
