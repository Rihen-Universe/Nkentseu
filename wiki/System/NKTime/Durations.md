# Les durées

> Couche **System** · NKTime · Exprimer **« combien de temps »** : la durée constexpr `NkDuration`
> (entiers de nanosecondes, pour *spécifier*) et l'intervalle calendaire signé `NkTimeSpan` (à la
> .NET, pour *décomposer* en jours/heures/minutes…).

Dès qu'on a besoin de dire **« combien de temps »** — dormir 16 ms, expirer un timeout au bout de
2 secondes, cadencer un tick à 60 Hz, mesurer l'écart entre deux dates — il faut un type *durée*.
La tentation est de tout faire en `float` de secondes : c'est piégeux. Un `float` perd de la
précision au fur et à mesure que les valeurs grandissent, mélange les unités (était-ce des
millisecondes ou des secondes ?), et n'a aucune sémantique de type (rien n'empêche d'additionner
« 3 » à une position). NKTime propose à la place **deux types dédiés**, chacun pour un rôle bien
distinct.

`NkDuration` stocke un **entier 64 bits de nanosecondes** (`int64`) — exact, signé, sans dérive,
**`constexpr`** d'un bout à l'autre. C'est le type qu'on emploie pour **spécifier** une durée : on
l'écrit lisiblement (`NkDuration::FromMilliseconds(16)`), on la combine arithmétiquement, on la
passe à un *sleep* ou à un *timeout*. `NkTimeSpan` répond à un autre besoin : représenter un
**intervalle signé décomposable** en jours / heures / minutes / secondes / millisecondes /
nanosecondes, exactement comme `System.TimeSpan` de .NET — pour de l'affichage calendaire, des
écarts entre dates, des durées « humaines ».

Ce n'est **pas** la même chose que `NkElapsedTime` (un `float64`, *résultat* d'une mesure de chrono,
documenté ailleurs), ni que la classe horaire `NkTime` (un *point* dans la journée, défini dans
`NkTimes.h`). Ici on parle uniquement de **durées** : des quantités, pas des instants.

- **Namespace** : `nkentseu`
- **Headers** : `#include "NKTime/NkDuration.h"`, `#include "NKTime/NkTimeSpan.h"`
- **Header parapluie** : `#include "NKTime/NKTime.h"` (tout le module d'un coup)

---

## La durée à spécifier : `NkDuration`

C'est le type **par défaut** dès qu'une API demande « combien de temps ». Son secret de fabrication
tient en une ligne : à l'intérieur, il n'y a **qu'un seul `int64` de nanosecondes** (qui peut être
négatif). Pas de flottant, pas de couple valeur+unité — un entier exact. La nanoseconde est l'unité
de base parce qu'elle est assez fine pour tout (un tick à 240 Hz fait ~4 166 666 ns) sans jamais
perdre de précision tant qu'on reste dans la plage de l'`int64` (≈ 292 ans).

On ne construit presque **jamais** un `NkDuration` par son constructeur de nanosecondes brut — il est
d'ailleurs `explicit` exprès, pour qu'un `int64` ne se transforme pas en durée par accident. À la
place, on emploie les **fabriques nommées**, qui rendent l'unité explicite à la lecture :

```cpp
auto frame   = NkDuration::FromMilliseconds(16);   // un cadre à ~60 Hz
auto timeout = NkDuration::FromSeconds(2);          // 2 s, lisible sans ambiguïté
auto half    = NkDuration::FromSeconds(2.5);        // surcharge float64 → 2,5 s
```

Toute l'API publique de `NkDuration` est **`constexpr`** et **`noexcept`** — sauf le formatage
texte (`ToString`/`ToStringPrecise`, qui font du `snprintf`). Autrement dit, une durée peut être
calculée **à la compilation** : `constexpr auto kTick = NkDuration::FromSeconds(1) / 60.0;` est une
constante figée dans le binaire, à coût d'exécution nul.

L'arithmétique fait ce qu'on attend. On **additionne / soustrait** deux durées, on **met à
l'échelle** par un scalaire `float64` (`frame * 2.0`), et — détail subtil — `operator/` a **deux
sens** : diviser une durée par un scalaire donne une durée plus courte, mais diviser une durée par
**une autre durée** donne un `float64` **sans unité** (un *ratio*). C'est exactement ce qu'il faut
pour répondre à « combien de cadres tiennent dans une seconde ? » :

```cpp
NkDuration second = NkDuration::FromSeconds(1);
NkDuration frame  = NkDuration::FromMilliseconds(16);
float64 ratio = second / frame;     // ≈ 62.5  (durée / durée → ratio float64)
NkDuration faster = frame / 2.0;    // 8 ms     (durée / scalaire → durée)
```

Deux pièges méritent d'être nommés tout de suite. D'abord, **multiplier ou diviser par un scalaire
tronque** vers l'entier (le résultat repasse en `int64` ns) : `FromNanoseconds(3) / 2.0` donne 1 ns,
pas 1,5. Ensuite, **un littéral `float32` (`2.5f`) est ambigu** entre les surcharges `int64` et
`float64` des fabriques — écrivez `2.5` (float64) ou `2` (int64) sans suffixe `f`.

Pour relire la valeur, on dispose de conversions vers chaque unité, en **deux saveurs** : la version
entière tronque vers zéro (`ToMilliseconds()` → `int64`), la version `…F()` rend un `float64` précis
(`ToMillisecondsF()` → `15.999…`). Et pour l'afficher, `ToString()` choisit **automatiquement**
l'unité la plus lisible (ns/µs/ms/s/min/h/j), tandis que `ToStringPrecise()` reste toujours en
nanosecondes brutes (`"16000000 ns"`).

> **En résumé.** `NkDuration` = un `int64` de nanosecondes, exact, signé, **`constexpr`**, pour
> **spécifier** une durée (sleep, timeout, période). On la crée par les fabriques `From…` (jamais le
> ctor `explicit`), on la combine arithmétiquement. `op/` a deux sens : ÷ scalaire → durée, ÷ durée →
> ratio `float64`. Multiplier/diviser par un scalaire **tronque** ; un littéral `2.5f` est ambigu
> (préférez `2.5`).

---

## L'intervalle décomposable : `NkTimeSpan`

`NkDuration` sait dire « 90 061 secondes » ; il ne sait pas dire « 1 jour, 1 heure, 1 minute et
1 seconde ». C'est le rôle de `NkTimeSpan` : un **intervalle signé** taillé pour la **décomposition
calendaire**, calqué sur `System.TimeSpan` de .NET. À l'intérieur, comme `NkDuration`, il ne stocke
qu'**un seul `int64` de nanosecondes totales** (la source de vérité) — mais son interface est
orientée « composants humains ».

On le construit soit par composants décomposés, soit par fabrique d'unité :

```cpp
NkTimeSpan a(1, 1, 1, 1);              // 1 jour, 1 h, 1 min, 1 s (millisecondes/ns = 0)
NkTimeSpan b = NkTimeSpan::FromHours(25);
```

Le point **crucial** à comprendre — et la source d'erreur la plus fréquente — est la différence
entre les **getters décomposés** et les **totaux**. `GetDays()`, `GetHours()`, `GetMinutes()`…
renvoient le **résidu** après extraction de l'unité supérieure, jamais le total. Sur
`FromHours(25)` :

```cpp
b.GetDays();          // 1    (les heures « débordent » en 1 jour)
b.GetHours();         // 1    (résidu : 25 - 24 = 1, dans [0-23])
b.ToNanoseconds();    // 90 000 000 000 000   (le TOTAL exact, constexpr)
b.ToSeconds();        // 90000.0              (le total en secondes, type double)
```

Pour la **quantité réelle**, prenez toujours `ToNanoseconds()` (exact, `constexpr`) ou `ToSeconds()`
(`double` — noter le type de retour). Les `Get…` ne servent qu'à **afficher** (« 1j 01:01:01 »).

L'arithmétique offre `+`, `-`, `*` et `/` (par un `double`), avec leurs versions composées
in-place, **et** des équivalents nommés chaînables — `Add`, `Subtract`, `Multiply`, `Divide` — qui
renvoient `*this` pour un style fluide. Contrairement à `NkDuration`, il **n'y a pas** d'opérateur
ratio `span / span`, pas de négation unaire, et les scalaires sont des `double` (pas `float64`).

Deux extracteurs font le pont avec le calendrier. `GetTime()` renvoie la **partie horaire** modulo
24 h (un `NkTime`, en normalisant les durées négatives vers `[0, 24h)`) ; `GetDate()` interprète le
span comme un **offset depuis l'époque Unix** (1970-01-01) et renvoie une `NkDate` (algorithme de
Howard Hinnant). Enfin, `ToString()` produit un format calendaire `"[±][Xj ][HH:]MM:SS[.mmm[.nnnnnn]]"`,
et une fonction amie `ToString(const NkTimeSpan&)` (exposée par ADL) délègue à la méthode.

Pièges à retenir : le ctor « total ns » est `explicit` ; les `Get…` sont **décomposés**, pas
totaux ; il **n'existe pas** de fabrique `float` ni de `FromMicroseconds`, et **pas** de `Zero()`
statique (certains commentaires du header l'utilisent, mais ce symbole n'est **pas** déclaré —
n'en dépendez pas).

> **En résumé.** `NkTimeSpan` = un `int64` ns signé présenté comme un **intervalle calendaire**
> (jours/heures/…), façon .NET. Les `Get…` rendent des **composants décomposés** (résidus), pas des
> totaux : pour la quantité, prenez `ToNanoseconds()`/`ToSeconds()`. Scalaires en `double`, API
> fluide (`Add`/`Subtract`/…), ponts `GetTime()`/`GetDate()`. Pour **spécifier** une durée préférez
> `NkDuration` ; `NkTimeSpan` est pour **décomposer / afficher**.

---

## Aperçu de l'API

La liste de **tous** les éléments publics, en un coup d'œil. Chacun est détaillé dans la
« Référence complète » qui suit. Toute l'API `NkDuration` est `[constexpr noexcept]` sauf
`ToString*` ; l'API `NkTimeSpan` est `[noexcept]` (seuls le ctor ns et `ToNanoseconds()` sont
`constexpr`).

### `NkDuration` — durée à spécifier (`int64` ns, constexpr)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkDuration()`, `explicit NkDuration(int64 ns)`, copie, `operator=` | Durée nulle / depuis ns bruts (`explicit`) / copie. |
| Fabriques (int64) | `FromNanoseconds` `FromMicroseconds` `FromMilliseconds` `FromSeconds` `FromMinutes` `FromHours` `FromDays` | Créer depuis une unité (entier). |
| Fabriques (float64) | `FromMicroseconds` `FromMilliseconds` `FromSeconds` `FromMinutes` `FromHours` `FromDays` | Idem en `float64` (**tronqué** en ns). |
| Conversion (int64) | `ToNanoseconds` `ToMicroseconds` `ToMilliseconds` `ToSeconds` `ToMinutes` `ToHours` `ToDays` | Vers une unité (troncature vers zéro). |
| Conversion (float64) | `ToMicrosecondsF` `ToMillisecondsF` `ToSecondsF` `ToMinutesF` `ToHoursF` `ToDaysF` | Vers une unité (précis). |
| Arithmétique | `a + b` `a - b`, `d * s` `d / s`, `d / d` (→`float64`), `-d` | Somme / diff / échelle / **ratio** / négation. |
| Composés | `+=` `-=` `*=` `/=` | Versions in-place (`*= /=` tronquent). |
| Comparaison | `==` `!=` `<` `<=` `>` `>=` | Comparent `mNanoseconds`. |
| Prédicats | `Abs()` `IsNegative()` `IsZero()` `IsPositive()` | Magnitude / signe / nullité. |
| Formatage | `ToString()` `ToStringPrecise()` | Unité auto-adaptative / toujours en ns (**non constexpr**). |
| Constantes | `Zero()` `Max()` `Min()` | 0 / `INT64_MAX` / `INT64_MIN` ns. |
| Libre | `operator*(float64 s, const NkDuration&)` | Symétrie `scalaire * durée`. |

### `NkTimeSpan` — intervalle signé décomposable (`int64` ns)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkTimeSpan()`, `NkTimeSpan(d, h, m, s, ms=0, ns=0)`, `explicit NkTimeSpan(int64 totalNs)`, copie, `operator=` | Nulle / par composants / depuis total ns (`explicit`) / copie. |
| Fabriques (int64) | `FromDays` `FromHours` `FromMinutes` `FromSeconds` `FromMilliseconds` `FromNanoseconds` | Créer depuis une unité (signée). *Pas* de surcharge float, *pas* de `FromMicroseconds`. |
| Getters décomposés | `GetDays` `GetHours` `GetMinutes` `GetSeconds` `GetMilliseconds` `GetNanoseconds` | Composant **résiduel** (pas le total). |
| Totaux | `ToNanoseconds()` `[constexpr]`, `ToSeconds()` (`double`) | La quantité **réelle**. |
| Arithmétique | `a + b` `a - b`, `s * f` `s / f` (`double`) | Somme / diff / échelle. |
| Composés | `+=` `-=` `*=` `/=` | Versions in-place. |
| Fluide | `Add` `Subtract` `Multiply` `Divide` | Équivalents nommés chaînables (`→ *this`). |
| Comparaison | `==` `!=` `<` `<=` `>` `>=` | Sur le total ns. |
| Calendrier | `GetTime()` (→`NkTime`), `GetDate()` (→`NkDate`) | Partie horaire (mod 24 h) / partie date (époque Unix). |
| Formatage | `ToString()`, `friend ToString(const NkTimeSpan&)` | Format calendaire / variante ADL. |

### `NKTime/NKTime.h` — header parapluie

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Umbrella | `#include "NKTime/NKTime.h"` | Inclut **tout** le module (aucune classe/macro propre). |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité (O(1) partout), et cas d'usage
dans les différents domaines du moteur. Les éléments triviaux sont décrits brièvement ; les points
qui prêtent à confusion le sont **à fond**.

### Choisir : `NkDuration` ou `NkTimeSpan` ?

Le critère est l'**intention**, pas la grandeur :

- **Vous spécifiez une durée à une API** (sleep, timeout, période, accumulateur de temps de jeu) →
  `NkDuration`. Exact, `constexpr`, arithmétique riche, ratio durée/durée.
- **Vous affichez/décomposez une durée pour un humain ou un calendrier** (chrono « MM:SS », écart
  entre deux dates, durée d'une session) → `NkTimeSpan`. Composants jours/heures/…, ponts
  `GetDate()`/`GetTime()`.
- **Vous recevez le résultat d'une mesure de chrono** → ce n'est ni l'un ni l'autre, c'est
  `NkElapsedTime` (un `float64`, documenté à part).

### `NkDuration` — construction et fabriques

Le constructeur par défaut donne une durée nulle ; le constructeur `explicit NkDuration(int64 ns)`
part de nanosecondes brutes. On ne l'utilise quasiment que pour reconstruire une durée depuis une
valeur sérialisée — partout ailleurs, les **fabriques nommées** sont préférables car elles disent
l'unité. Les fabriques existent en deux surcharges : `int64` (exacte) et `float64` (qui **tronque**
en ns). Pas de garde d'overflow : `FromDays(huge)` peut déborder l'`int64` silencieusement.

- **Gameplay / boucle** — `FromMilliseconds(16)` pour un cadre cible, `FromSeconds(0.1)` pour une
  cadence de simulation fixe, `FromMinutes(5)` pour un compte à rebours de manche.
- **Audio** — `FromSeconds(1) / sampleRate` (constexpr) donne la durée d'un échantillon ; planifier
  un *fade* sur `FromMilliseconds(250)`.
- **Animation** — durée d'un clip, d'une transition de *blend*, d'un délai avant déclenchement.
- **IO / réseau** — `FromSeconds(2)` comme timeout de socket, `FromMilliseconds(50)` comme intervalle
  d'envoi de *heartbeat*.
- **Threading** — période d'un timer, durée d'un *sleep* coopératif, fenêtre d'attente d'un *future*.

### `NkDuration` — conversions vers unités

Chaque unité se relit en **entier** (troncature vers zéro : `ToMilliseconds()` → `int64`) ou en
**`float64` précis** (suffixe `…F` : `ToMillisecondsF()`). `ToNanoseconds()` rend la valeur interne
exacte sans perte.

- **Outils / éditeur** — `ToMillisecondsF()` pour afficher un temps de frame avec ses décimales dans
  un profileur.
- **Réseau** — `ToMilliseconds()` (entier) pour remplir un champ de protocole exprimé en ms.
- **GPU / rendu** — `ToSecondsF()` pour pousser un *time uniform* à un shader (effets animés).
- **Gameplay** — `ToSeconds()` (entier) pour un affichage « secondes restantes » qui ne tremble pas.

### `NkDuration` — arithmétique et le double `operator/`

Addition et soustraction composent et comparent des durées ; la multiplication par un scalaire
`float64` met à l'échelle. Le point à retenir absolument est le **double sens de `operator/`** :

- **durée ÷ scalaire → durée** : `frame / 2.0` (durée deux fois plus courte). Le résultat **tronque**.
- **durée ÷ durée → `float64`** : un **ratio sans unité**. `total / frame` répond à « combien de
  cadres dans cet intervalle ? ».

La négation unaire `-d` inverse le signe (les durées sont signées). Cas d'usage du ratio :

- **Rendu / temps** — `elapsed / targetFrame` mesure la charge d'une frame (>1 = on dépasse le
  budget).
- **Animation** — `(now - start) / clipDuration` donne le paramètre normalisé `t ∈ [0,1]` d'un clip.
- **Physique** — diviser un pas accumulé par le pas fixe pour savoir **combien** de sous-steps
  exécuter.

Les versions composées (`+= -= *= /=`) modifient en place et renvoient `*this` ; `*=` et `/=`
tronquent comme leurs équivalents binaires.

### `NkDuration` — comparaisons et prédicats

Les six comparateurs ordonnent les durées par leur `mNanoseconds` interne (donc signé). Les
prédicats sont des raccourcis lisibles : `IsZero()` (timer expiré ?), `IsPositive()` /
`IsNegative()` (sens d'un écart : la deadline est-elle passée ?), et `Abs()` qui rend la
**magnitude** positive.

- **Gameplay / IA** — `if (cooldown.IsZero())` autorise une action ; `Abs(a - b) < tolerance` teste
  une quasi-égalité temporelle.
- **Réseau** — `IsNegative()` sur `(deadline - now)` signale un timeout dépassé.
- **Outils** — `Abs()` pour afficher un écart « ±N ms » indépendamment du sens.

### `NkDuration` — formatage et constantes

`ToString()` et `ToStringPrecise()` sont les **seules** méthodes non-`constexpr` (elles allouent une
`NkString` et formatent via `snprintf`). `ToString()` choisit l'unité la plus lisible (ns → j) ;
`ToStringPrecise()` reste en nanosecondes brutes — utile pour des logs exacts ou un diff. Les
constantes `Zero()`, `Max()` (`INT64_MAX` ns) et `Min()` (`INT64_MIN` ns) servent de bornes : `Max()`
fait un excellent « jamais expirer », `Zero()` un état initial.

- **Logs / debug** — `ToString()` pour un message lisible (« frame: 16ms »), `ToStringPrecise()` pour
  une trace exacte.
- **Threading** — `NkDuration::Max()` comme timeout « infini » d'une attente.

La **fonction libre** `operator*(float64 scalar, const NkDuration& d)` permet d'écrire le scalaire à
gauche (`2.5 * frame`), par symétrie avec `frame * 2.5` ; elle délègue simplement à `d * scalar`.

### `NkTimeSpan` — construction et fabriques

Trois entrées : le constructeur par composants `(jours, heures, minutes, secondes, ms=0, ns=0)` —
les composants attendus dans leurs plages naturelles (heures [0-23], minutes/secondes [0-59],
ms [0-999], ns [0-999999], les jours pouvant être négatifs —, le constructeur `explicit` depuis un
total de nanosecondes, et les fabriques `From…` (uniquement **signées int64** : ni surcharge float,
ni `FromMicroseconds`).

- **UI / 2D** — bâtir « 01:30 » via `NkTimeSpan(0, 0, 1, 30)` pour un minuteur affiché.
- **Outils / éditeur** — durée d'une session de travail, longueur d'un segment de timeline.
- **IO / réseau** — durée de validité d'un jeton, fenêtre de rétention d'un cache exprimée en heures.

### `NkTimeSpan` — getters décomposés contre totaux

C'est **le** point à ne pas rater. Les `Get…` renvoient le **composant résiduel**, pas le total :
sur `FromHours(25)`, `GetDays() == 1` et `GetHours() == 1`. Ils servent exclusivement à
**l'affichage** décomposé. Pour la **quantité réelle**, employez `ToNanoseconds()` (exact,
`constexpr`) ou `ToSeconds()` (renvoie un `double`).

- **UI / 2D** — composer un affichage `"{GetHours()}:{GetMinutes():02}:{GetSeconds():02}"`.
- **Gameplay** — `ToSeconds()` pour piloter une barre de progression continue.
- **Outils** — `ToNanoseconds()` pour comparer/sérialiser deux spans sans perte.

### `NkTimeSpan` — arithmétique et API fluide

`+`, `-`, `*` et `/` (scalaire `double`) et leurs versions composées couvrent les besoins courants ;
les méthodes **nommées** `Add` / `Subtract` / `Multiply` / `Divide` font la même chose mais
renvoient `*this`, pour enchaîner : `span.Add(a).Subtract(b)`. À noter par rapport à `NkDuration` :
**pas** de ratio `span / span`, **pas** de négation unaire, **pas** de fonction libre `scalaire * span`.

- **Animation / timeline** — décaler un segment (`Add`), étirer une plage (`Multiply(1.5)`).
- **Gameplay** — accumuler des durées de manches, soustraire le temps écoulé d'un budget.

### `NkTimeSpan` — ponts calendaires `GetTime` / `GetDate`

`GetTime()` extrait la **partie horaire** modulo 24 h (un `NkTime`), en normalisant proprement les
durées négatives vers `[0, 24h)`. `GetDate()` interprète le span comme un **offset depuis l'époque
Unix** (1970-01-01) et reconstruit une `NkDate` via l'algorithme de Howard Hinnant. Ces ponts
relient les durées au reste du module (`NkTime`, `NkDate` dans `NkTimes.h` / `NkDate.h`).

- **IO / réseau** — convertir un horodatage Unix (porté comme un span depuis l'époque) en date
  lisible.
- **Outils / éditeur** — afficher la date+heure d'un fichier ou d'un événement journalisé.

### `NkTimeSpan` — formatage

`ToString()` rend `"[±][Xj ][HH:]MM:SS[.mmm[.nnnnnn]]"` — signe, jours optionnels, puis horloge avec
millisecondes/nanosecondes optionnelles. La **fonction amie** `ToString(const NkTimeSpan&)`
(définie inline dans le header, trouvée par ADL) délègue à la méthode : pratique pour les API
génériques qui appellent un `ToString(x)` non qualifié.

### `NKTime/NKTime.h` — le header parapluie

Ce header ne déclare **aucune** classe, fonction, macro ni énum : il ne contient que des `#include`
(et ses gardes). Il agrège tout le module dans l'ordre de dépendances : `NkTimeApi.h` →
`NkTimeConstants.h` → `NkElapsedTime.h` → `NkDuration.h` → `NkChrono.h` → `NkClock.h` → `NkDate.h` →
`NkTimes.h` → `NkTimeSpan.h` → `NkTimeZone.h`. **Usage** : un consommateur fait
`#include "NKTime/NKTime.h"` pour avoir tout le module d'un coup ; en interne, on inclut plutôt les
headers spécifiques (`NkDuration.h`, `NkTimeSpan.h`…) pour limiter la surface de compilation.

> **Note de cadrage.** La classe horaire `NkTime` (un *point* dans la journée) n'est **pas** dans
> ces headers : elle vit dans `NkTimes.h` (inclus transitivement). De même, `NkElapsedTime`
> (`float64`, résultat de mesure) est défini ailleurs. Cette page couvre strictement les **durées**
> `NkDuration` et `NkTimeSpan`.

---

### Exemple

```cpp
#include "NKTime/NKTime.h"     // ou NkDuration.h + NkTimeSpan.h
using namespace nkentseu;

// --- NkDuration : spécifier et calculer (constexpr) ---
constexpr NkDuration kFrame = NkDuration::FromMilliseconds(16);   // ~60 Hz
constexpr NkDuration kSecond = NkDuration::FromSeconds(1);
constexpr float64 fps = kSecond / kFrame;        // ≈ 62.5  (durée / durée → ratio)

NkDuration timeout = NkDuration::FromSeconds(2);
if ((deadline /* NkDuration */).IsNegative()) {  // deadline dépassée ?
    // ... abandon
}
NkString human = timeout.ToString();             // "2 s" (unité auto)

// --- NkTimeSpan : décomposer / afficher ---
NkTimeSpan span = NkTimeSpan::FromHours(25);
span.GetDays();                                  // 1   (résidu, PAS le total)
span.GetHours();                                 // 1
int64 totalNs = span.ToNanoseconds();            // 90 000 000 000 000  (total exact)
double totalS = span.ToSeconds();                // 90000.0

NkTimeSpan session(0, 1, 30, 0);                 // 1 h 30 min
session.Add(NkTimeSpan::FromMinutes(15));        // fluide → 1 h 45 min
NkString label = session.ToString();             // "01:45:00"
```

---

[← Index NKTime](README.md) · [Récap NKTime](../NKTime.md) · [Couche System](../README.md)
