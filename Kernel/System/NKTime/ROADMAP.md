# NKTime — Roadmap

État actuel (mai 2026) : Module stable et complet pour les besoins moteur.
Couvre la mesure haute précision (`NkChrono`/`NkElapsedTime`), les durées
(`NkDuration`), l'orchestration de game loop (`NkClock` avec delta/fps/fixed
timestep/time scale), les calendriers grégoriens (`NkDate`, `NkTimes`,
`NkTimeSpan`) et la gestion de fuseaux (`NkTimeZone` UTC + offset fixe + DST
partiel). Sans STL.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Constantes (NkTimeConstants) | Livré | — | — |
| NkDuration (durée à spécifier, int64 ns) | Livré | — | — |
| NkElapsedTime (résultat de mesure, float64) | Livré | — | — |
| NkChrono (QPC / CLOCK_MONOTONIC / Emscripten, Sleep, Yield, Now) | Livré | — | — |
| NkClock (Tick, delta, fps, fixedDelta, timeScale, pause/resume) | Livré | — | — |
| NkDate (grégorien validé, range 1601..30827) | Livré | — | — |
| NkTimes / NkTime (heure du jour HH:MM:SS.mmm.nnnnnn) | Livré | — | — |
| NkTimeSpan (intervalle signé + décomposition calendaire) | Livré | — | — |
| NkTimeZone (UTC + offset fixe + ToLocal/ToUtc) | Livré | — | — |
| Tests smoke (Duration, Now, TimeZone, Chrono) | Livré | — | — |
| DST réel et règles régionales (TZif / Windows TimeZoneInfo) | Partiel | L | Moyenne |
| Parsing/formatting ISO 8601 / RFC 3339 | TODO | M | Haute |
| Tests étendus (NkClock pause/resume, fixed step, time scale) | TODO | S | Haute |
| Timer/scheduler récurrent (callbacks périodiques) | TODO | M | Moyenne |
| High-resolution profiler scopes (RAII) | TODO | S | Moyenne |
| TAI / Leap seconds awareness | TODO | XL | Basse |
| Sérialisation native NkDate/NkTimeSpan dans NKSerialization | TODO | S | Haute |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Livré

### Constantes et types fondamentaux
- [NkTimeConstants](src/NKTime/NkTimeConstants.h) : NS_PER_SECOND,
  NS_PER_MILLISECOND, etc., source de vérité unique.
- [NkDuration](src/NKTime/NkDuration.h) : durée mutable int64 ns,
  `FromNanoseconds/Microseconds/Milliseconds/Seconds/Minutes/Hours`,
  opérateurs arithmétiques et comparaison.
- [NkElapsedTime](src/NKTime/NkElapsedTime.h) : résultat immuable de mesure,
  float64, expose 4 unités précalculées (ns/µs/ms/s), comparable, soustraction
  retournant un `NkElapsedTime`.

### Mesure haute précision et primitives sleep
- [NkChrono](src/NKTime/NkChrono.h) : chronomètre RAII.
  - Backends : Windows `QueryPerformanceCounter` (~100 ns),
    Linux/macOS `clock_gettime(CLOCK_MONOTONIC)` (~1 ns), Emscripten via
    `performance.now()`.
  - `Elapsed()`, `Reset()`, statiques `Now()`, `Sleep(ns)`,
    `SleepMilliseconds(ms)`, `YieldThread()`.
  - `noexcept`, zero alloc.

### Orchestrateur de game loop
- [NkClock](src/NKTime/NkClock.h) : Tick() → snapshot `NkTime` (delta, total,
  frameCount, fps moyenne glissante, fixedDelta, timeScale).
  - Deux NkChrono internes : frame + total.
  - `Pause()` / `Resume()` préservant le temps total.
  - `SetFixedDelta(dt)` pour stepping déterministe (physique 60 Hz).
  - `SetTimeScale(s)` pour bullet time / fast-forward.

### Calendrier et fuseaux
- [NkDate](src/NKTime/NkDate.h) : date grégorienne validée, opérateurs
  ==/!=/+/-, plage [1601, 30827].
- [NkTimes / NkTime](src/NKTime/NkTimes.h) : heure du jour
  `(hour, minute, second, ms, ns)` validée, sans STL.
- [NkTimeSpan](src/NKTime/NkTimeSpan.h) : intervalle signé en ns avec
  décomposition calendaire (`GetDays`, `GetHours`, ...).
- [NkTimeZone](src/NKTime/NkTimeZone.h) :
  - `GetUtc()`, `GetLocal()`, `FromName("UTC+02:30")` (offset fixe parsé).
  - `ToLocal(date|time)`, `ToUtc(date|time)`, `GetUtcOffset(date)`.
  - Mention DST dans la doc, mais résolution réelle des règles régionales =
    voir section TODO.

### Tests
- [test_smoke.cpp](tests/test_smoke.cpp) : conversions NkDuration,
  monotonicité de `NkChrono::Now()`, round-trip UTC + offset fixe via
  `NkTimeZone`.
- [benchmark_smoke.cpp](tests/benchmark_smoke.cpp) : micro-bench
  Now()/Elapsed().

---

## En cours / TODO immédiat

### DST réel et règles régionales
- Aujourd'hui `NkTimeZone` supporte UTC + offset fixe via `FromName("UTC±HH:MM")`.
- Manque : lecture de la base IANA / TZif (POSIX) et appel à
  `GetTimeZoneInformationForYear` (Windows) pour résoudre "Europe/Paris" avec
  ses règles DST historiques.
- Conséquence actuelle : pas de bascule été/hiver automatique sur les zones
  nommées.

### Parsing / formatting ISO 8601
- `NkDate::FromString("2026-05-26")`, `NkTime::FromString("14:30:00.123")`,
  format combiné `2026-05-26T14:30:00+02:00`.
- `ToString()` symétrique pour sérialisation textuelle (consommé par
  NKSerialization JSON/XML/YAML).

### Tests à étendre
- `NkClock` : Pause/Resume préserve bien `total` ; `fixedDelta` accumulateur
  ; `timeScale=0.5` halve correctement le snapshot scaled.
- `NkDate` : opérations sur frontière 28/29 février, leap years extrêmes.
- `NkTimeSpan` : ajout sur frontière mois/année.

---

## À venir / À ajouter (futur proche)

### Sérialisation native
- Adapter `NkSerialize<NkDate>` / `NkSerialize<NkTimeSpan>` /
  `NkSerialize<NkTimeZone>` dans NKSerialization (JSON, XML, YAML, Binary).
  Aujourd'hui ces types ne sont pas câblés dans le registre de
  sérialiseurs.

### Timer / Scheduler récurrent
- `NkTimer::Every(NkDuration::FromMilliseconds(100), []() { ... })` avec
  callback géré par un thread interne ou via le `NkThreadPool` de
  NKThreading. Cas d'usage : ticks UI, auto-save, garbage collection.

### Scopes de profilage
- `NkProfileScope scope("MyFunction");` RAII qui mesure et publie via un
  consommateur configurable (NKLogger, futur `NkProfiler`). Marker
  Tracy/Optick optionnel.

### NkClock — extensions
- Time-step interpolé / interpolation alpha pour le rendu entre deux
  steps physiques.
- Frame budget tracker : warn si `delta` > `targetFrameTime`.
- Capture de timeline complète (N derniers deltas pour graph).

### Précisions avancées
- TAI clock (sans leap seconds) en plus du monotonic existant.
- `NkSteadyClock::TimeSinceEpoch()` cohérent multi-process pour traces
  distribuées.

### Plateformes
- Validation Android (`clock_gettime`) et iOS (`mach_absolute_time` legacy
  vs `clock_gettime` moderne).

---

## Bugs / quirks connus

### ⚠️ MESURÉ — `SleepMilliseconds` dort ~15,5 ms quoi qu'on lui demande (2026-08-15)

Sur Windows, **sans `timeBeginPeriod(1)`**, toute demande de 1 à 12 ms dort en
réalité ~15,5 ms, et `Sleep(16)` dort **29,8 ms** — deux tics de 15,6.
Relevé (`tests/bench_sleep.cpp`, 40 répétitions par palier) :

| demandé | réel sans | réel avec `timeBeginPeriod(1)` |
|---|---|---|
| 1 ms | **15,53 ms** | 1,86 ms |
| 4 ms | **15,50 ms** | 5,00 ms |
| 12 ms | **15,39 ms** | 12,45 ms |
| 16 ms | **29,76 ms** | 16,53 ms |

**Ce n'est pas un défaut de NKTime** — c'est la résolution de minuterie du
système, et `NkChrono.cpp:277` la documente déjà. Le défaut est **où l'appel se
trouve** : `timeBeginPeriod(1)` n'existe qu'à **un seul endroit du moteur**,
`NkRendererImpl.cpp:116`. Toute application qui n'initialise pas NKRenderer —
une démo caméra en OpenGL direct, un outil, un banc d'essai — hérite donc de la
granularité par défaut **sans que rien ne le lui dise**.

**Conséquence mesurée, et elle dépassait mon chantier** : une boucle cadencée par
`Sleep(16 − travail)` n'est pas cadencée, elle est **quantifiée**. Sur
`NkCameraDemos --demo=viewer`, intervention à trois exécutions par condition :

| | run 1 | run 2 | run 3 | amplitude |
|---|---|---|---|---|
| sans | 40,7 | 41,2 | 40,1 img/s | 1,1 |
| avec `timeBeginPeriod(1)` | 62,9 | 62,7 | 62,7 img/s | **0,2** |

Forcer la résolution ne rend pas seulement les 60 img/s : **elle supprime la
dispersion**. C'est l'origine du plancher de bruit de ±33 % qui empêchait deux
bancs d'essai distincts de conclure (le mien sur le coût du journal, celui de
NK3DModeler sur les captures).

### ✅ TRANCHÉ ET LIVRÉ — `NkChrono::BeginPreciseTiming()` (Rodolf, 2026-08-16)

**Décision de Rodolf**, catégorie 2 (touche toutes les applications) : une API
nommée, appelée depuis `NkMain`. *C'est la mesure du cantonnement au processus
qui a emporté l'arbitrage, pas une préférence* — l'objection « on impose un effet
à toute la machine » ne tenait plus.

- `NkChrono::BeginPreciseTiming()` / `EndPreciseTiming()` / `IsPreciseTimingActive()` ;
- appelées par `NkWESystem::Initialise` / `Close` — **appariées**, parce que le
  système compte les demandes par processus et qu'une demande sans restitution
  est une fuite, pas un réglage ;
- **active par défaut, refusable** via `NkAppData::enablePreciseTiming`. *Une
  décision par défaut qu'on ne peut pas refuser est une décision irréversible
  déguisée* — et une minuterie fine paie des réveils, donc de l'énergie sur
  portable et sur mobile ;
- déclaration **toujours présente**, corps sous `#if` : hors Windows la fonction
  existe et ne fait rien. Une API qui disparaît sur une plateforme force chaque
  appelant à refaire le `#if`.

**Preuves d'atterrissage** — `NkCameraDemos --demo=viewer`, Windows 11 Pro
10.0.26100, 2026-08-16 :

| condition | 3 exécutions | amplitude |
|---|---|---|
| par défaut, **aucun drapeau** | **61,5 / 61,8 / 61,8 img/s** | 0,3 |
| `enablePreciseTiming = false` | **40,5 / 40,8 / 40,8 img/s** | 0,3 |

Le défaut agit, **et le refus agit aussi** — vérifié en le posant à `false` dans
une application réelle, pas seulement en le lisant.

**Chemin non-Windows : exercé, pas seulement compilé.** `NkCameraDemos` déployé
et lancé sur Galaxy S22+ (Android) : l'application démarre, la caméra diffuse,
aucun effet de bord. Le corps y est vide et `IsPreciseTimingActive()` répond
quand même « oui » — la précision vient de `clock_nanosleep`, sans réglage global
à poser ; répondre « non » ferait croire à un échec.

⚠️ **PORTÉE DE VERSION, à ne pas perdre** : le cantonnement au processus est une
propriété de **Windows 10 version 2004 et postérieur**, pas de Windows en
général. Mesuré le 2026-08-15 sur **Windows 11 Pro 10.0.26100** (15,66 / 15,29 /
15,22 ms avant, pendant, après qu'un autre processus tienne la minuterie fine).
**Sur un Windows antérieur, l'effet est global à la machine** — soit exactement
l'objection que cette mesure a écartée. *Un fait de plateforme a l'air éternel ;
celui-ci porte sa version, sa date et sa machine.*

- L'annonce DST dans `NkTimeZone` n'est pas couverte par les tests actuels :
  seul UTC et offset fixe sont validés.
- Le tableau du Readme indique `float64 ns + 4 unités précalculées` pour
  `NkElapsedTime` : confirmer que les 4 unités (ns/µs/ms/s) sont bien
  publiques et constantes après construction.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types, traits), NKPlatform
  (détection OS pour QPC vs clock_gettime).
- **Modules au-dessus qui en dépendent** : NKLogger (timestamps des messages
  via NkLogMessage), NKThreading (sleep dans spin/backoff), NKFileSystem
  (timestamps fichier), NKNetwork (timeouts), NKRenderer (frame timing,
  `delta` pour animation), Runtime (boucle d'application), Noge
  (profilage, animations UI).
