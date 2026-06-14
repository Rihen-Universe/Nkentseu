# Les horloges

> Couche **System** · NKTime · Mesurer le **temps qui passe** : la durée mesurée `NkElapsedTime`,
> le chronomètre haute précision `NkChrono`, l'orchestrateur de boucle de jeu `NkClock`, et les
> constantes de conversion du namespace `nkentseu::time`.

Tout programme temps réel se heurte tôt ou tard à la même question : **combien de temps s'est-il
écoulé ?** Combien depuis la dernière frame, pour faire avancer la physique du bon pas ? Combien a
pris cette fonction, pour la profiler ? Combien dois-je dormir avant la prochaine itération ? On
serait tenté d'y répondre avec un simple `float`, mais c'est un piège : un `float` ne dit pas s'il
porte des **secondes**, des **millisecondes** ou des **nanosecondes**, et l'addition de deux durées
exprimées dans des unités différentes donne un résultat silencieusement faux. NKTime règle cela en
**typant le temps** : une durée mesurée est un `NkElapsedTime` qui connaît ses unités, une source
de temps est un `NkChrono`, et une boucle de jeu s'orchestre avec un `NkClock`.

Le compromis central de ce module tient en une phrase : **`NkElapsedTime` est une valeur (un
résultat figé), `NkChrono` est une source (il mesure), et `NkClock` est un chef d'orchestre (il
mesure *et* interprète pour une boucle de jeu).** Cette page vous apprend à les distinguer et à les
combiner. Tout est **zéro-STL**, sans allocation sur les chemins chauds, et adossé à l'horloge
monotonique de l'OS (Windows `QueryPerformanceCounter`, POSIX `clock_gettime(CLOCK_MONOTONIC)`).

- **Namespace** : `nkentseu` (constantes dans `nkentseu::time`)
- **Headers** : `#include "NKTime/NkClock.h"` (tire transitivement `NkChrono.h`, `NkElapsedTime.h`),
  `#include "NKTime/NkChrono.h"`, `#include "NKTime/NkElapsedTime.h"`,
  `#include "NKTime/NkTimeConstants.h"`. **Pas de header parapluie** : on inclut directement celui
  dont on a besoin.

---

## La durée mesurée : `NkElapsedTime`

C'est la **valeur** que tout le module produit : un résultat de mesure, figé. La difficulté qu'il
résout est l'**ambiguïté des unités**. Plutôt que de stocker un seul nombre dont on ne sait plus
s'il vaut des secondes ou des millisecondes, `NkElapsedTime` garde sa source de vérité en
**nanosecondes** (`float64`) et **précalcule une fois pour toutes** les trois autres unités —
`microseconds`, `milliseconds`, `seconds`. Le coût de la conversion est payé **à la construction**,
si bien que `ToSeconds()` ou `ToMilliseconds()` ne sont plus que de simples lectures mémoire, en
`O(1)` et sans la moindre branche.

```cpp
NkElapsedTime frame = NkElapsedTime::FromMilliseconds(16.6);
double s  = frame.ToSeconds();        // 0.0166  — lecture directe, déjà calculée
double us = frame.ToMicroseconds();   // 16600   — idem
```

Le point qui surprend au début : on **ne construit pas** un `NkElapsedTime` directement avec un
nombre. Le constructeur canonique `NkElapsedTime(float64 ns)` est **privé** — c'est lui qui remplit
les quatre champs depuis les nanosecondes, et il est réservé aux opérateurs internes. Pour fabriquer
une durée, on passe **obligatoirement** par une fabrique nommée (`FromSeconds`, `FromMilliseconds`,
`FromMicroseconds`, `FromNanoseconds`), ce qui force l'unité à être **explicite** au site d'appel.
On ne peut plus écrire `NkElapsedTime t = 16` sans dire « 16 quoi ? ».

Les durées s'**additionnent** et se **soustraient** entre elles (`a + b`, `a - b`), se mettent à
l'échelle par un scalaire (`d * 2.0`, `d / 4.0`), et se **comparent** (`<`, `<=`, `==`…). Toutes ces
opérations recalculent proprement les quatre unités. Deux pièges à connaître : `operator/` n'a
**aucune garde contre le diviseur nul**, et `operator==` compare les nanosecondes `float64` à
l'**égalité exacte** — sensible aux imprécisions, comme toute comparaison de flottants.

Enfin, `ToString()` produit un formatage **adaptatif** : il choisit l'unité la plus lisible (ns, us,
ms ou s) selon l'ordre de grandeur. Notez qu'il écrit `us` en ASCII (pas `µs`), et qu'il **alloue**
une `NkString` — donc, contrairement au reste de la structure, il n'est ni `noexcept` ni `constexpr`.

> **En résumé.** `NkElapsedTime` est une **valeur immuable** qui connaît ses unités : source de
> vérité en nanosecondes, trois autres unités précalculées (accès `O(1)`). On la fabrique
> **uniquement** par les fabriques nommées (`FromSeconds`…), le constructeur par valeur est privé.
> Arithmétique et comparaisons disponibles ; attention au `operator/` sans garde et au `==` exact
> sur flottants.

---

## Le chronomètre : `NkChrono`

Là où `NkElapsedTime` est un résultat, `NkChrono` est l'**instrument** qui le produit. C'est un
chronomètre haute précision : on le crée, il démarre, et on lui demande à tout moment **combien de
temps a passé**. En interne il mémorise un timestamp de départ (`mStartTime`) capturé à la
construction, et toutes ses mesures sont relatives à ce point. Le backend est l'horloge
**monotonique** de l'OS — celle qui **ne recule jamais**, contrairement à l'heure du jour qu'un
ajustement NTP ou un changement d'heure peut faire bondir en arrière.

Deux méthodes en font tout l'intérêt, et la nuance entre elles est capitale :

```cpp
NkChrono chrono;                       // démarre tout de suite
loadAssets();
NkElapsedTime loadTime = chrono.Elapsed();   // lit, NE remet PAS à zéro

NkChrono frameClock;
while (running) {
    NkElapsedTime dt = frameClock.Reset();   // lit PUIS remet à zéro → delta de frame
    update(dt.ToSeconds());
}
```

`Elapsed()` est une **lecture seule** : il renvoie le temps écoulé depuis le départ sans rien
changer — parfait pour profiler un bloc de code. `Reset()`, lui, lit le temps écoulé **puis** remet
le chrono à zéro, et retourne la durée mesurée juste avant la remise à zéro : c'est l'idiome exact du
**delta-time** d'une boucle, où chaque tour mesure l'intervalle depuis le tour précédent. Les deux
sont en `O(1)`.

`NkChrono` expose aussi l'**horloge de base** en statique : `Now()` donne un timestamp absolu
monotonique (origine arbitraire mais stable), et `GetFrequency()` la fréquence de l'horloge
sous-jacente en Hz (la fréquence QPC sous Windows, `1'000'000'000` sous POSIX). Enfin, une famille de
`Sleep` statiques permet de **suspendre le thread** : `Sleep(NkDuration)`, `Sleep(int64 ms)`,
`Sleep(float64 ms)`, et les variantes explicites `SleepMilliseconds` / `SleepMicroseconds` /
`SleepNanoseconds` ; `YieldThread()` cède simplement le quantum au scheduler et revient aussitôt.

Ce n'est **pas** une horloge murale : `NkChrono` ne vous dira jamais « il est 14 h 32 » (c'est le
rôle de `NkTime`, l'heure du jour, hors de cette page). Il mesure des **intervalles**, pas des dates.
Deux pièges : la **copie est autorisée** mais capture l'état au moment de la copie — un `Reset()` sur
une copie n'affecte pas l'originale (états indépendants) ; et la **précision du Sleep dépend du
scheduler** (il peut sur-attendre ou être interrompu ; Windows arrondit un sommeil sous la
milliseconde à 1 ms minimum). Attention aussi à l'ambiguïté `Sleep(int64)` vs `Sleep(float64)` : un
littéral entier choisit la version ms entière, un littéral `.0` la version flottante.

> **En résumé.** `NkChrono` est un chronomètre monotonique : `Elapsed()` lit sans remettre à zéro
> (profilage), `Reset()` lit *puis* remet à zéro (delta-time de boucle). Statiques utiles :
> `Now()`/`GetFrequency()` (horloge brute), `Sleep*`/`YieldThread()` (suspension). Mesure des
> intervalles, pas des dates ; copie = états indépendants ; précision de Sleep tributaire de l'OS.

---

## L'orchestrateur de boucle : `NkClock`

`NkClock` est le niveau au-dessus : il assemble **deux** `NkChrono` pour piloter une boucle de jeu
complète. L'un (`mFrameChrono`) est resetté à chaque frame pour fournir le **delta**, l'autre
(`mTotalChrono`) n'est jamais resetté pour donner le **temps total**. Par-dessus, `NkClock` calcule
le **nombre de frames**, les **FPS** (en moyenne glissante), gère une **échelle de temps**
(*time scale*) et la **pause/reprise**. C'est le seul des trois qui *interprète* le temps pour le
gameplay, et non qui se contente de le mesurer.

Le cœur de l'API est `Tick()` : appelé une fois par frame, il avance d'une frame et renvoie un
**snapshot** — un `NkClock::NkTime` — **par référence**. Ce snapshot rassemble tout ce dont la frame
a besoin :

```cpp
NkClock clock;
while (running) {
    const NkClock::NkTime& t = clock.Tick();   // une frame de plus
    update(t.Scaled());                         // delta × timeScale (logique de jeu)
    render(t.delta);                            // delta brut (interpolation visuelle)
    physics.Step(t.FixedScaled());              // pas fixe déterministe
}
```

Le snapshot `NkClock::NkTime` porte `delta` (durée de la dernière frame en secondes), `total` (temps
total depuis `Reset()`), `frameCount`, `fps`, `fixedDelta` (le pas fixe déterministe, `1/60` par
défaut) et `timeScale` (le facteur d'échelle, `1.0` par défaut). Deux accesseurs combinent ces
champs : `Scaled()` renvoie `delta * timeScale` (le temps « ressenti » par le gameplay, qu'un
ralenti ou un *bullet-time* peut moduler) et `FixedScaled()` renvoie `fixedDelta * timeScale` (le pas
fixe de la physique, lui aussi soumis à l'échelle). Notez bien : ce `NkClock::NkTime` **imbriqué** est
distinct du type `nkentseu::NkTime` qui, lui, représente l'heure du jour.

Les paramètres de contrôle pilotent le comportement : `SetTimeScale(s)` change l'échelle (un `0.5`
pour un ralenti, un `2.0` pour une accélération — sans toucher `delta` brut), `SetFixedDelta(s)`
règle le pas déterministe, `Pause()` met `delta` et `fps` à zéro au prochain `Tick()` (mais `total`
continue d'avancer et `frameCount` de s'incrémenter), `Resume()` rétablit le calcul normal, et
`IsPaused()` interroge l'état. `GetTime()` relit le dernier snapshot **sans** avancer la frame.
`Reset()` remet chronos, `frameCount` et `total` à zéro **en conservant** `timeScale` et `fixedDelta`
— exactement ce qu'il faut à une transition de niveau, où l'on veut repartir de zéro sans reperdre la
configuration.

Ce n'est **pas** un objet qu'on copie ni qu'on partage entre threads : `NkClock` est **non copiable**
(état mutable interne) et **réservé au thread principal**. Et la référence renvoyée par `Tick()` est
**invalidée au prochain `Tick()`/`Reset()`** — il ne faut pas la conserver au-delà de la frame
courante. Comme `Tick()` n'est volontairement **pas** `[[nodiscard]]`, le compilateur ne vous
préviendra pas si vous oubliez d'en lire le résultat.

> **En résumé.** `NkClock` orchestre une boucle de jeu via deux `NkChrono` (frame + total).
> `Tick()` avance d'une frame et renvoie un snapshot `NkClock::NkTime` **par référence** (delta,
> total, fps, frameCount, fixedDelta, timeScale ; `Scaled()`/`FixedScaled()` appliquent l'échelle).
> Contrôles : `SetTimeScale`, `SetFixedDelta`, `Pause`/`Resume`/`IsPaused`, `Reset` (garde la
> config). Non copiable, thread principal, référence du `Tick` éphémère.

---

## Les constantes de conversion : `nkentseu::time`

Toutes les conversions du moteur partent d'une **source de vérité unique** : le namespace
`nkentseu::time` (`NkTimeConstants.h`), une collection de `static constexpr` typées via
`NKCore/NkTypes.h`. Plutôt que de semer des `1'000'000` magiques dans le code, on écrit
`time::NS_PER_MILLISECOND`. Les constantes se rangent en trois familles : les conversions **vers
nanosecondes** (`NS_PER_MICROSECOND`, `NS_PER_MILLISECOND`, `NS_PER_SECOND`, `NS_PER_MINUTE`,
`NS_PER_HOUR`, `NS_PER_DAY`, en `int64`), les conversions **en secondes** (`SECONDS_PER_MINUTE`,
`SECONDS_PER_HOUR`, `SECONDS_PER_DAY`, en `int64`), et les **bornes des composants** d'une horloge
murale (`HOURS_PER_DAY`, `MINUTES_PER_HOUR`, `SECONDS_PER_MINUTE_I32`, `MILLISECONDS_PER_SECOND`,
`MICROSECONDS_PER_MILLISECOND`, `NANOSECONDS_PER_MILLISECOND`, en `int32`).

Un détail à ne pas confondre : il existe **deux** symboles proches mais distincts —
`SECONDS_PER_MINUTE` (`int64`, pour les calculs de durée) et `SECONDS_PER_MINUTE_I32` (`int32`, pour
décomposer une borne de composant). Aucun header public ne fait `using namespace nkentseu::time`
(règle imposée) : on qualifie toujours `time::…`.

> **En résumé.** `nkentseu::time` centralise toutes les conversions en `static constexpr` typées :
> familles « vers nanosecondes » et « en secondes » en `int64`, « bornes de composants » en `int32`.
> Attention au doublon `SECONDS_PER_MINUTE` (int64) / `SECONDS_PER_MINUTE_I32` (int32) ; toujours
> qualifier `time::`.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé (comportement,
complexité, cas d'usage) dans la « Référence complète ». Complexités / `noexcept` entre crochets
quand c'est utile.

### `struct NkElapsedTime` — durée mesurée (NkElapsedTime.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Champs | `nanoseconds` (source), `microseconds`, `milliseconds`, `seconds` | Quatre `float64` précalculés ; `nanoseconds` est la vérité. |
| Construction | `NkElapsedTime()` `[constexpr, noexcept]` | Durée nulle (4 champs à 0). |
| Fabriques | `FromNanoseconds`, `FromMicroseconds`, `FromMilliseconds`, `FromSeconds` `[static constexpr, noexcept]` | Construire avec **unité explicite**. |
| Accès | `ToNanoseconds`, `ToMicroseconds`, `ToMilliseconds`, `ToSeconds` `[O(1), constexpr, noexcept]` | Lecture directe d'une unité précalculée. |
| Arithmétique | `a + b`, `a - b`, `+=`, `-=`, `a * f`, `a / d` `[constexpr, noexcept]` | Somme/différence de durées, mise à l'échelle (`/` **sans garde**). |
| Comparaison | `==`, `!=`, `<`, `<=`, `>`, `>=` `[constexpr, noexcept]` | Comparent `nanoseconds` (égalité **exacte**). |
| Formatage | `ToString()` (membre), `ToString(e)` (amie ADL) | Format adaptatif ns/us/ms/s ; **alloue**, pas `noexcept`. |

### `class NkChrono` — chronomètre haute précision (NkChrono.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkChrono()` `[noexcept]`, `~NkChrono`, copie/affectation `[default]` | Démarre à la construction ; copie autorisée (états indépendants). |
| Mesure | `Elapsed()` `[O(1), noexcept]` | Temps écoulé, **sans** remise à zéro (profilage). |
| Mesure | `Reset()` `[O(1), noexcept]` | Lit **puis** remet à zéro → delta-time. |
| Horloge brute | `Now()` `[static, noexcept]`, `GetFrequency()` `[static, noexcept]` | Timestamp absolu monotonique ; fréquence en Hz. |
| Sleep | `Sleep(NkDuration)`, `Sleep(int64)`, `Sleep(float64)` `[static, noexcept]` | Suspend le thread (durée typée / ms entières / ms fractionnaires). |
| Sleep | `SleepMilliseconds`, `SleepMicroseconds`, `SleepNanoseconds` `[static, noexcept]` | Suspension par unité explicite. |
| Scheduling | `YieldThread()` `[static, noexcept]` | Cède le quantum, retourne aussitôt. |

### `class NkClock` — orchestrateur de boucle (NkClock.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Snapshot | `NkClock::NkTime` : `delta`, `total`, `frameCount`, `fps`, `fixedDelta`, `timeScale` | Instantané d'une frame (imbriqué, ≠ `nkentseu::NkTime`). |
| Snapshot | `NkTime::Scaled()` `[constexpr, noexcept]`, `NkTime::FixedScaled()` `[constexpr, noexcept]` | `delta×timeScale` / `fixedDelta×timeScale`. |
| Snapshot | `NkTime::From(...)` `[static, noexcept]` | Fabrique depuis valeurs brutes. |
| Cycle de vie | `NkClock()` `[noexcept]`, `~NkClock`, copie/affectation `[deleted]` | **Non copiable** ; aucune alloc à la construction. |
| Cycle de vie | `Reset()` `[noexcept]` | RAZ chronos/total/frameCount ; **garde** timeScale/fixedDelta. |
| Frames | `Tick()` `[noexcept]` | Avance d'une frame, renvoie le snapshot **par référence**. |
| Frames | `GetTime()` `[noexcept]` | Snapshot courant **sans** avancer. |
| Contrôle | `SetTimeScale(f)`, `SetFixedDelta(s)` `[noexcept]` | Échelle de temps ; pas fixe déterministe. |
| Contrôle | `Pause()`, `Resume()`, `IsPaused()` `[noexcept]` | Geler/relancer le delta ; interroger l'état. |
| Sleep / Yield | `Sleep(...)`, `SleepMilliseconds/Microseconds/Nanoseconds`, `YieldThread()` `[static, noexcept]` | Délégation directe à `NkChrono`. |

### `namespace nkentseu::time` — constantes (NkTimeConstants.h)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Vers ns (`int64`) | `NS_PER_MICROSECOND`, `NS_PER_MILLISECOND`, `NS_PER_SECOND`, `NS_PER_MINUTE`, `NS_PER_HOUR`, `NS_PER_DAY` | Facteurs de conversion en nanosecondes. |
| En secondes (`int64`) | `SECONDS_PER_MINUTE`, `SECONDS_PER_HOUR`, `SECONDS_PER_DAY` | Durées exprimées en secondes. |
| Bornes (`int32`) | `HOURS_PER_DAY`, `MINUTES_PER_HOUR`, `SECONDS_PER_MINUTE_I32`, `MILLISECONDS_PER_SECOND`, `MICROSECONDS_PER_MILLISECOND`, `NANOSECONDS_PER_MILLISECOND` | Bornes des composants d'une horloge murale. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, et usages dans les différents
domaines du temps réel. Les éléments triviaux sont décrits brièvement ; les opérations importantes le
sont **à fond**.

### Choisir le bon type

Le critère est **ce que vous voulez faire du temps** :

- **Vous transportez un résultat de mesure** (une durée à passer, stocker, comparer) → `NkElapsedTime`.
- **Vous mesurez un intervalle** (delta-time, profilage d'un bloc, dormir) → `NkChrono`.
- **Vous pilotez une boucle de jeu** (delta + fps + pause + time scale + pas fixe) → `NkClock`.
- **Vous convertissez des unités** (ms↔ns, décomposer une durée) → `nkentseu::time`.

### `NkElapsedTime` à fond

**Valeur, pas instrument.** `NkElapsedTime` ne mesure rien : il *porte* une durée déjà mesurée. Son
intelligence est de stocker la vérité en `nanoseconds` (`float64`) et de **précalculer** à la
construction `microseconds`/`milliseconds`/`seconds`, de sorte que chaque accesseur (`ToSeconds()`…)
soit une lecture mémoire en `O(1)`, sans division ni branche. C'est un POD de 32 octets (4×`float64`)
qu'on copie librement.

**Fabriques nommées obligatoires.** Le constructeur `NkElapsedTime(float64 ns)` qui remplit les
quatre champs est **privé** ; on fabrique donc par `FromSeconds`/`FromMilliseconds`/
`FromMicroseconds`/`FromNanoseconds`, toutes `static constexpr noexcept`. Avantage : l'unité est
**toujours explicite** au site d'appel, ce qui élimine la classe de bugs « j'ai additionné des ms à
des secondes ».

**Arithmétique et comparaisons.** Somme/différence de deux durées, mise à l'échelle par un scalaire,
et les six comparaisons (qui regardent `nanoseconds`). Le `operator/` n'offre **aucune garde** sur le
diviseur nul, et `==` est une égalité **exacte** sur `float64` (sensible aux arrondis).

Cas d'usage, par domaine :
- **Rendu** — transporter le temps d'une frame ou le coût d'une passe (`g-buffer`, ombres) entre le
  code de mesure et l'affichage du profileur, sans jamais perdre l'unité.
- **Animation** — exprimer la durée d'un clip ou le temps courant d'une timeline ; l'arithmétique
  `cursor + dt` reste typée.
- **Audio** — durée d'un buffer, position de lecture, fenêtre de fondu : autant de durées que l'on
  additionne et compare en sécurité.
- **IO / réseau** — un *timeout* ou un RTT exprimés en `NkElapsedTime`, comparés par `<` à un seuil.
- **Outils / éditeur** — afficher proprement un temps mesuré via `ToString()` (format adaptatif,
  `us` en ASCII), pour une console ou un overlay de debug.

### `NkChrono` à fond

**`Elapsed()` vs `Reset()`.** C'est la distinction à graver. `Elapsed()` est une **photographie** :
il renvoie le temps depuis le départ **sans toucher** au chrono — on l'appelle plusieurs fois pour
mesurer plusieurs paliers d'un même bloc. `Reset()` est un **tour de cadran** : il renvoie le temps
écoulé et **redémarre** depuis zéro, ce qui en fait l'outil exact du delta-time, où chaque tour
mesure l'écart avec le précédent. Les deux sont `O(1)`.

**Horloge brute.** `Now()` donne un timestamp absolu monotonique (origine arbitraire mais stable,
qui **ne recule jamais**) ; `GetFrequency()` renvoie la résolution en Hz du compteur sous-jacent
(QPC sous Windows, `1e9` sous POSIX). Utiles pour bâtir ses propres mesures ou horodater des
événements.

**Suspension.** La famille `Sleep` (durée typée, ms entières, ms fractionnaires, ou unités
explicites micro/nano) suspend le thread courant ; `YieldThread()` cède simplement le quantum et
revient aussitôt. La précision réelle dépend du scheduler : un Sleep peut sur-attendre, et Windows
plancher un sommeil sub-milliseconde à 1 ms.

Cas d'usage, par domaine :
- **Boucle / gameplay** — le delta-time de chaque frame via `Reset()` ; toute la simulation s'appuie
  dessus.
- **Outils / éditeur** — profiler un bloc (`Elapsed()` autour d'un import d'asset, d'une compilation
  de shader) sans perturber la mesure.
- **Threading** — un *worker* qui `Sleep` entre deux scrutations, ou `YieldThread()` pour un
  *spin-wait* coopératif qui ne monopolise pas un cœur.
- **IO / réseau** — *backoff* entre tentatives de connexion (Sleep croissant), horodatage de paquets
  via `Now()`.
- **GPU** — encadrer une soumission/présentation pour mesurer le temps CPU d'une frame côté hôte.
- **Audio** — un thread de mixage qui calibre son rythme d'éveil entre deux remplissages de buffer.

### `NkClock` à fond

**Deux chronos, un chef d'orchestre.** `NkClock` combine `mFrameChrono` (resetté chaque frame → le
**delta**) et `mTotalChrono` (jamais resetté → le **temps total**). Autour, il tient `frameCount`,
estime les `fps` en moyenne glissante, applique une échelle de temps et gère la pause. La
construction n'alloue rien et ne touche pas l'OS.

**`Tick()` et le snapshot.** Appelé une fois par frame, `Tick()` avance l'état et renvoie un
`NkClock::NkTime` **par référence**, valide jusqu'au prochain `Tick()`/`Reset()`. Le snapshot porte
`delta`, `total`, `frameCount`, `fps`, `fixedDelta` (défaut `1/60`) et `timeScale` (défaut `1.0`).
Deux helpers le combinent : `Scaled()` (= `delta * timeScale`, le temps que voit la logique de jeu) et
`FixedScaled()` (= `fixedDelta * timeScale`, le pas de la physique). `GetTime()` relit ce même
snapshot sans avancer. `Tick()` n'est **pas** `[[nodiscard]]` : à vous de ne pas oublier d'en lire le
retour.

**Contrôle.** `SetTimeScale` ouvre la porte aux ralentis et accélérations (un `bullet-time` à `0.2`,
un *fast-forward* à `4.0`) **sans toucher** `delta` brut — l'affichage et l'interpolation visuelle
peuvent donc rester en temps réel pendant que la logique ralentit. `SetFixedDelta` règle le pas
déterministe. `Pause()` met `delta` et `fps` à zéro au prochain `Tick()` tout en laissant `total` et
`frameCount` avancer (utile pour figer la simulation sans figer le compteur), `Resume()` reprend,
`IsPaused()` renseigne. `Reset()` repart de zéro **en gardant** `timeScale`/`fixedDelta` — la
transition de niveau idéale.

Cas d'usage, par domaine :
- **Gameplay / IA** — `update(t.Scaled())` pour que tout le monde respecte le ralenti ; `t.frameCount`
  pour cadencer une logique « toutes les N frames ».
- **Physique** — un accumulateur consomme `t.delta` par pas de `t.FixedScaled()` pour une simulation
  **déterministe**, découplée du framerate.
- **Rendu** — `render(t.delta)` et le résidu d'accumulateur pour interpoler les positions entre deux
  pas de physique ; `t.fps` pour l'overlay.
- **Animation** — faire avancer les timelines avec `t.Scaled()`, naturellement gelées par `Pause()`.
- **UI / 2D** — animer transitions et curseurs ; un menu pause met `SetTimeScale(0)` ou `Pause()` sans
  arrêter l'UI elle-même.
- **Outils / éditeur** — un bouton « avance image par image » qui appelle `Tick()` une fois en pause ;
  `Reset()` au rechargement d'une scène.

### `nkentseu::time` à fond

Source unique des facteurs de conversion, toutes `static constexpr` et typées via `NKCore/NkTypes.h`.
Trois familles : les facteurs **vers nanosecondes** (`NS_PER_*`, `int64`), les durées **en secondes**
(`SECONDS_PER_*`, `int64`), et les **bornes de composants** d'une horloge murale (`*_PER_*` en
`int32` : heures/jour, minutes/heure, etc.). Rappel du doublon volontaire : `SECONDS_PER_MINUTE`
(`int64`) pour les calculs de durée et `SECONDS_PER_MINUTE_I32` (`int32`) pour décomposer un
composant. Aucun `using namespace nkentseu::time` dans les headers publics : on qualifie `time::`.

Cas d'usage, par domaine :
- **Tous domaines** — remplacer les nombres magiques (`1'000'000`) par un symbole nommé rend le code
  relisible et garantit la cohérence des conversions.
- **Outils / éditeur** — décomposer une durée en h/min/s pour un affichage `HH:MM:SS` à l'aide des
  bornes `int32`.
- **IO / réseau** — exprimer des timeouts en secondes puis les convertir en ns pour l'API bas niveau.

### Le socle commun

- **Zéro-STL, zéro alloc chaud.** Aucune dépendance STL ; seul `ToString()` alloue une `NkString`.
  Les chemins de mesure (`Tick`, `Elapsed`, `Reset`) sont sans allocation.
- **Monotonique.** `NkChrono`/`NkClock` reposent sur l'horloge monotonique de l'OS : elle **ne
  recule jamais**, à la différence de l'heure du jour (`NkTime`, hors de cette page).
- **`NkDuration` en complément.** Plusieurs `Sleep` acceptent un `nkentseu::NkDuration`
  (`NKTime/NkDuration.h`, hors des quatre headers de cette page) : une durée typée alternative,
  pratique pour passer une durée nommée à `Sleep`.

---

### Exemple récapitulatif

```cpp
#include "NKTime/NkClock.h"        // tire NkChrono + NkElapsedTime
#include "NKTime/NkTimeConstants.h"
using namespace nkentseu;

// 1. Profiler un bloc avec NkChrono (lecture sans reset).
NkChrono prof;
loadLevel();
NkElapsedTime cost = prof.Elapsed();
log("Chargement : " + cost.ToString());        // format adaptatif (ms/s)

// 2. Piloter la boucle de jeu avec NkClock.
NkClock clock;
clock.SetFixedDelta(1.f / 60.f);
double accumulator = 0.0;

while (running) {
    const NkClock::NkTime& t = clock.Tick();   // référence éphémère

    accumulator += t.Scaled();                 // temps « ressenti » (time scale)
    while (accumulator >= t.FixedScaled()) {   // physique déterministe
        physics.Step(t.FixedScaled());
        accumulator -= t.FixedScaled();
    }

    render(t.delta);                           // delta brut pour le visuel
    if (t.frameCount % 30 == 0) ui.SetFps(t.fps);
}

// 3. Convertir via les constantes, sans nombre magique.
int64 budgetNs = 16 * time::NS_PER_MILLISECOND;   // budget de 16 ms en ns

// 4. Composer des durées typées.
NkElapsedTime a = NkElapsedTime::FromMilliseconds(8.0);
NkElapsedTime b = NkElapsedTime::FromMicroseconds(500.0);
NkElapsedTime total = a + b;                    // 8.5 ms, unités cohérentes
NkChrono::Sleep(2.0);                           // dormir 2 ms (version float64)
```

---

[← Index NKTime](README.md) · [Récap NKTime](../NKTime.md) · [Couche System](../README.md)
