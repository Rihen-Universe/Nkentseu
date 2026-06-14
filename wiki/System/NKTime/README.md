# NKTime — documentation détaillée

Le module **NKTime**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKTime.md](../NKTime.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec son contrat et
ses cas d'usage concrets (boucle de jeu, profilage, timers, journalisation, planification…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Clocks.md](Clocks.md) | Horloge de boucle de jeu (`NkClock` : delta, total, FPS, time scale, pause), chronomètre haute précision (`NkChrono` : `Elapsed`/`Reset`, `Now`, `Sleep`, `YieldThread`), résultat de mesure (`NkElapsedTime`, 4 unités float64) et constantes de conversion `nkentseu::time::NS_PER_*`. | `NkClock.h`, `NkChrono.h`, `NkElapsedTime.h`, `NkTimeConstants.h` |
| [Durations.md](Durations.md) | Durée typée pour spécifier un délai (`NkDuration` : int64 ns, `constexpr`, fabriques `FromSeconds/…`, conversions, arithmétique), intervalle signé décomposé en composants calendaires (`NkTimeSpan`, style .NET), et le header parapluie du module. | `NkDuration.h`, `NkTimeSpan.h`, `NkTimes.h` |
| [Dates.md](Dates.md) | Date grégorienne validée (`NkDate` : bissextiles, `DaysInMonth`, ISO 8601, noms de mois FR) et fuseau horaire immuable thread-safe (`NkTimeZone` : `GetLocal`/`GetUtc`/`FromName`, conversions UTC↔local, DST). | `NkDate.h`, `NkTimeZone.h` |

[← Récap NKTime](../NKTime.md) · [← Couche System](../README.md)
