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

### Horloge murale — `NkSystemClock` (2026-08-16)

`UnixSeconds()`, `UnixMilliseconds()`, `LocalNow()`, `UtcNow()`,
`StampCompact()` — l'heure **du calendrier**, distincte de `NkChrono` qui reste
**monotone**. Mesurer une durée avec l'horloge murale est un défaut classique :
un ajustement d'horloge en cours de mesure rend une durée négative.

**Pourquoi elle est née** : le module savait mesurer des durées et représenter
des dates, mais **rien ne donnait l'heure qu'il est**. Quatre endroits du dépôt
appelaient donc `std::time`/`localtime` chacun dans leur coin —
`NkStringUtils.cpp`, `NkDirectory.cpp`, `NkUIFileBrowser.cpp`,
`NkCameraSystem.cpp` : le même manque résolu quatre fois localement, avec de la
STL, dans un moteur qui n'en veut pas. *(Les en-têtes du module renvoyaient vers
« NkDateTime, module séparé » — qui n'existe pas : une promesse d'API sans
implémentation.)*

Vérifié : `StampCompact` rend `"20260816_000110"` — 15 caractères, exactement la
forme que produisait `strftime("%Y%m%d_%H%M%S")`. Sur tampon trop petit elle rend
**faux et une chaîne vide**, jamais un horodatage à moitié écrit : un nom de
fichier tronqué se collisionne en silence avec le suivant.

**Premier consommateur** : `NkCameraSystem::GenerateAutoPath`. Les trois autres
sites du dépôt restent à basculer — hors périmètre de ce chantier, notés ici.

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
Relevé (`tests/bench_sleep.cpp`, 40 répétitions par palier, **Release**,
Windows 11 Pro 10.0.26100, 2026-08-15) :

| demandé | réel sans | réel avec `timeBeginPeriod(1)` |
|---|---|---|
| 1 ms | **15,53 ms** | 1,86 ms |
| 4 ms | **15,50 ms** | 5,00 ms |
| 12 ms | **15,39 ms** | 12,45 ms |
| 16 ms | **29,76 ms** | 16,53 ms |

✅ **Rejoué en Debug, et le chiffre ne bouge pas** : 15,19 / 15,52 / 15,65 /
15,38 / 15,51 ms pour 1, 2, 3, 4 et 8 ms demandées. C'est le contrôle qui compte
— il confirme que la quantification est une propriété **du système**, pas de la
construction, donc que cette conclusion-là se transporte d'une configuration à
l'autre. *(À l'inverse du coût d'une ligne de journal, qui varie de ×1,9 à ×30 —
voir `NKLogger/ROADMAP.md`.)*

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

**Preuves d'atterrissage** — `NkCameraDemos --demo=viewer`, **configuration
Release**, Windows 11 Pro 10.0.26100, 2026-08-16, branche `feat/nkxr` :

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

#### L'option écartée, son coût, et pourquoi on ne l'a pas prise

Une décision qui cite l'option écartée se défend dans six mois ; une décision
seule se rediscute. **SFML** place le réglage **dans le sommeil lui-même**, et
l'apparie : `timeBeginPeriod(wPeriodMin)` juste avant le `Sleep`,
`timeEndPeriod` juste après — voir
`Cours/SFML-master/src/SFML/System/Win32/SleepImpl.cpp` (licence zlib, **lu, non
copié**).

| | SFML | Nkentseu |
|---|---|---|
| portée | le temps d'un sommeil | tout le processus |
| coût | **2 appels système par sommeil** | **1 appel au démarrage** |
| intrusivité | nulle hors du sommeil | résolution élevée en permanence |

⚠️ **L'argument par lequel j'avais éliminé cette option ne tenait pas.** J'avais
écrit « effet global et permanent pour un appel local » : c'est vrai d'une
version **non appariée**, et SFML l'apparie. Le vrai compromis n'est donc pas
« correct contre incorrect » mais **moins cher contre moins intrusif**. Rodolf a
tranché pour le nôtre ; la raison de l'écart est ici pour qu'on n'ait pas à la
reconstruire.

**Repris de leur conception** : ne pas coder la période en dur. `timeGetDevCaps`
donne `wPeriodMin`, la résolution réellement supportée par la machine —
**écrire `1` serait une hypothèse non mesurée**, exactement le reproche fait
cette semaine à trois réglages fantômes. La valeur est mémorisée, et `Begin` et
`End` présentent la même : le système apparie par valeur.

*(Contrôlé après ce changement : 62,0 / 62,0 / 61,5 / 62,0 / 61,9 / 61,6 img/s
sur 22 battements — le comportement ne bouge pas.)*

📌 Deux fausses pistes écartées, pour ne pas les refaire : **GLFW** n'utilise
jamais `timeBeginPeriod` (il a `win32_time.c` mais n'expose aucune fonction de
sommeil), et le **SDL2** de la bibliothèque locale est une distribution binaire,
sans source.

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
