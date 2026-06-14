# Les dates et les fuseaux horaires

> Couche **System** · NKTime · Représenter une **date du calendrier** (`NkDate`) et la
> **convertir entre UTC et local** à travers un **fuseau horaire** (`NkTimeZone`).

Manipuler des dates a l'air banal jusqu'au jour où l'on s'aperçoit que « le 30 février » existe
dans un `int`, que février 2024 a 29 jours mais pas février 2023, et que minuit à Tokyo n'est pas
minuit à Paris. Un moteur a besoin de dates **correctes** : un horodatage de sauvegarde, la
date d'expiration d'une licence, le timestamp d'un message réseau, l'affichage « dernière
connexion : 12 mars 2026 ». NKTime sépare proprement deux préoccupations qu'on a tendance à
mélanger : **quel jour du calendrier** (c'est `NkDate`, année/mois/jour validés) et **dans quel
fuseau** (c'est `NkTimeZone`, qui sait passer de l'heure universelle à l'heure locale et inverse).

La règle de fond tient en une phrase : **une date est une valeur du calendrier grégorien validée à
la construction ; un fuseau est un convertisseur immuable, donc partageable sans verrou entre
threads.** Cette page vous apprend à les utiliser ensemble — et à éviter leurs pièges (l'assert
silencieux d'une date invalide, le repli silencieux d'un nom IANA inconnu vers le fuseau local).

Ce ne sont **pas** des types « date + heure » : `NkDate` ne porte aucune heure, aucune
minute, aucune seconde. Pour un instant complet, on combine une `NkDate` (le jour) avec un
`NkTime` / `NkTimeSpan` (l'heure dans la journée) — ces deux derniers vivent dans leurs propres
headers ([Time](Time.md), [Spans](Spans.md)). `NkTimeZone` est précisément le pont entre les deux
mondes : ses surcharges convertissent soit un `NkTime` (l'heure, avec décalage DST), soit une
`NkDate` (le jour, qui peut basculer si le décalage traverse minuit).

- **Namespace** : `nkentseu`
- **Headers** : `#include "NKTime/NkDate.h"` · `#include "NKTime/NkTimeZone.h"`

---

## La date du calendrier : `NkDate`

`NkDate` répond à une seule question — **quel jour ?** — et la garantit **valide**. Trois champs
seulement (`year`, `month`, `day`), mais derrière chaque affectation se cache une **validation
grégorienne** : l'année doit tenir dans `[1601, 30827]`, le mois dans `[1, 12]`, et le jour ne peut
dépasser le nombre réel de jours du mois — qui dépend du mois **et** de l'année (février a 28 ou 29
jours). Construire une date impossible n'échoue pas « gentiment » : ça déclenche un
`NKENTSEU_ASSERT_MSG` en debug (et c'est un comportement indéfini en release, voir plus bas).

```cpp
NkDate today;                  // date système locale courante (appelle GetCurrent())
NkDate launch{ 2026, 3, 12 };  // 12 mars 2026, validé à la construction
int32 d = launch.GetDay();     // 12
NkString iso = launch.ToString();        // "2026-03-12" (ISO 8601)
NkString mois = launch.GetMonthName();   // "Mars" (FR uniquement)
```

Le constructeur par défaut ne donne **pas** une date neutre comme « 1970-01-01 » : il interroge
l'horloge système et renvoie **aujourd'hui** (en heure locale). Si vous voulez une date précise,
donnez-la explicitement. Les **mutateurs** (`SetYear`/`SetMonth`/`SetDay`) re-valident la date
**entière** après modification — ce qui réserve un piège utile : poser `SetYear(2023)` sur une date
qui était au `29/02` (un 29 février valide en 2024, bissextile) déclenche un assert, puisque 2023
n'a pas de 29 février.

`NkDate` n'est **pas** un type d'horodatage : pas d'heure, pas de fuseau intégré, pas
d'arithmétique « +3 jours » dans ce header. C'est une **valeur** triviale à copier, totalement
ordonnée (les six opérateurs de comparaison suivent l'ordre chronologique année→mois→jour), donc
parfaite comme clé ou comme champ sérialisé.

> **En résumé.** `NkDate` = un jour du calendrier grégorien **validé** (année/mois/jour), sans
> heure ni fuseau. Le ctor par défaut = aujourd'hui (local) ; le ctor à 3 arguments et les setters
> **assertent** sur une date invalide. `ToString()` donne l'ISO `"YYYY-MM-DD"`, `GetMonthName()` le
> mois en français. Copiable et comparable → utilisable comme valeur et comme clé.

---

## Le fuseau horaire : `NkTimeZone`

Une date ou une heure n'a de sens que **rapportée à un fuseau**. `NkTimeZone` encapsule cette
notion et, surtout, sait **convertir entre UTC et local**. Sa conception est dictée par un seul
principe : **immuable après construction**. Aucun constructeur public, aucune affectation — on
obtient une instance par les fabriques `GetLocal()`, `GetUtc()` ou `FromName()`, et une fois créée
elle ne change plus. Conséquence directe : **toutes ses méthodes sont `const`, donc l'objet est
thread-safe sans verrou** — on peut partager le même `NkTimeZone` entre tous les threads.

Trois sortes de fuseaux, décrites par l'enum imbriqué `NkKind` : `NK_LOCAL` (le fuseau système,
dont le **changement d'heure été/hiver est géré par l'OS**), `NK_UTC` (l'heure universelle, décalage
0, jamais de DST), et `NK_FIXED_OFFSET` (un décalage fixe en secondes, jamais de DST non plus).

```cpp
NkTimeZone local = NkTimeZone::GetLocal();   // fuseau système, DST automatique
NkTimeZone utc   = NkTimeZone::GetUtc();     // UTC, décalage 0
NkTimeZone tz    = NkTimeZone::FromName("UTC+02:00");  // décalage fixe +2h

NkTime utcInstant = /* ... */;
NkDate  refDate{ 2026, 7, 1 };               // juillet : DST actif en Europe
NkTime  localInstant = local.ToLocal(utcInstant, refDate);
```

Le détail qui compte est le **`refDate` explicite** des conversions de `NkTime`. Le décalage d'un
fuseau local **change selon la date** (heure d'été vs heure d'hiver) ; pour le calculer
correctement il faut savoir *quel jour*. Plutôt que d'appeler en douce `NkDate::GetCurrent()` (un
effet de bord qui rendrait la conversion non reproductible), `NkTimeZone` **vous demande** la date
de référence. C'est volontaire : donnez toujours le bon `refDate` pour un DST exact.

Attention au piège de `FromName` : il n'embarque **pas** une base IANA complète. Il comprend des
noms simples et des décalages (`"UTC"`, `"GMT"`, `"Z"`, `"Etc/UTC"`, `"UTC+02"`, `"GMT-05:30"`…),
mais tout nom inconnu — y compris un vrai nom IANA comme `"Europe/Paris"` — est **silencieusement
redirigé vers le fuseau local**. Il n'y a pas de signal d'échec : si la précision du fuseau compte,
n'attendez pas une erreur, vérifiez vous-même le résultat (`GetKind()`/`GetName()`).

> **En résumé.** `NkTimeZone` = convertisseur UTC↔local **immuable** (donc thread-safe, partageable
> sans verrou), construit **uniquement** par `GetLocal`/`GetUtc`/`FromName`. Trois sortes
> (`NK_LOCAL` avec DST OS, `NK_UTC`, `NK_FIXED_OFFSET`). Donnez toujours un `refDate` correct aux
> conversions de `NkTime` (DST). `FromName` **retombe en silence sur local** pour un nom inconnu —
> pas de gestion d'erreur runtime.

---

## Combiner date et fuseau

`NkTimeZone` surcharge `ToLocal`/`ToUtc` selon ce qu'on lui passe. Avec un `NkTime` (+ `refDate`),
il convertit **l'heure**, décalage DST compris. Avec une `NkDate` seule, il ne gère que le
**changement de jour potentiel** : si le décalage du fuseau traverse minuit, la date peut avancer
ou reculer d'un jour, mais aucune heure n'est portée. Comme il n'existe **aucun `NkDateTime`** dans
ces deux headers, un instant complet « date + heure » se compose à la main : on garde une `NkDate`
pour le jour et un `NkTime` pour l'heure, et on les convertit cohéremment avec le même fuseau et le
même `refDate`.

```cpp
// Horodatage complet d'un événement, converti UTC → local pour l'affichage.
NkTimeZone tz = NkTimeZone::GetLocal();
NkDate utcDay{ 2026, 6, 14 };
NkTime utcHour = /* heure dans la journée, UTC */;

NkDate localDay  = tz.ToLocal(utcDay);              // bascule de jour si l'offset franchit minuit
NkTime localHour = tz.ToLocal(utcHour, utcDay);     // heure locale (DST selon refDate = utcDay)
NkTimeSpan off   = tz.GetUtcOffset(utcDay);         // décalage appliqué ce jour-là
bool ete         = tz.IsDaylightSavingTime(utcDay); // heure d'été active ?
```

> **En résumé.** `ToLocal`/`ToUtc` sont **surchargés** : version `NkTime` (+`refDate`) pour l'heure
> avec DST, version `NkDate` seule pour le seul changement de jour. Pas de `NkDateTime` ici : on
> combine **manuellement** `NkDate` (le jour) et `NkTime` (l'heure), avec le même fuseau et le même
> `refDate`.

---

## Aperçu de l'API

Tous les éléments publics des deux classes, en un coup d'œil. Sauf mention contraire, les
accesseurs et comparateurs sont `const noexcept nodiscard` en `O(1)`. Détails et usages dans la
« Référence complète ».

### `nkentseu::NkDate` — date du calendrier grégorien validée

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkDate()` `[noexcept]`, `NkDate(year, month, day)`, copie/affectation `[noexcept]` | Date courante locale / date validée (peut **asserter**) / copie triviale |
| Accès | `GetYear` `GetMonth` `GetDay` `[O(1)]` | Lire les composantes |
| Mutation | `SetYear` `SetMonth` `SetDay` | Modifier puis **re-valider** la date entière (peut **asserter**) |
| Statics | `GetCurrent()`, `DaysInMonth(year, month)`, `IsLeapYear(year)` `[noexcept]` | Aujourd'hui (thread-safe) / jours du mois / année bissextile ? |
| Formatage | `ToString()`, `GetMonthName()` | ISO `"YYYY-MM-DD"` / nom du mois en **français** (allouent une `NkString`) |
| Comparaison | `==` `!=` `<` `<=` `>` `>=` `[O(1)]` | Ordre **chronologique** année→mois→jour |
| Libre (ADL) | `friend ToString(const NkDate&)` | Délègue à `d.ToString()` (trouvée par ADL dans `nkentseu`) |

### `nkentseu::NkTimeZone` — fuseau horaire et conversions UTC/local

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Sorte | `enum NkKind : uint8` = `NK_LOCAL`(0) `NK_UTC`(1) `NK_FIXED_OFFSET`(2) | Système (DST OS) / UTC fixe / décalage fixe |
| Fabriques | `GetLocal()` `GetUtc()` `[noexcept]`, `FromName(ianaName)` `[noexcept]` | Construction (seul moyen) ; `FromName` **retombe sur local** si inconnu |
| Conversion heure | `ToLocal(NkTime, refDate)`, `ToUtc(NkTime, refDate)` `[noexcept]` | UTC↔local de l'**heure**, DST via `refDate` |
| Conversion date | `ToLocal(NkDate)`, `ToUtc(NkDate)` `[noexcept]` | UTC↔local du **jour** (peut basculer à minuit) |
| Infos | `GetName()`, `GetKind()`, `GetFixedOffsetSeconds()` `[noexcept]` | Nom (réf interne) / sorte / décalage fixe (s) |
| Infos | `GetUtcOffset(date)`, `IsDaylightSavingTime(date)` `[noexcept]` | Décalage UTC pour une date / heure d'été active ? |
| Comparaison | `==` `!=` `[O(1)]` | Égalité = même **kind + name + offset** |

---

## Référence complète

Chaque élément repris en détail : comportement, complexité, et usages dans les différents domaines
du moteur. Les éléments triviaux sont décrits brièvement, les opérations à risque (validation,
conversions, DST) le sont **à fond**.

### `NkDate` — construction et validation

Le constructeur par défaut `NkDate()` est `noexcept` mais **non gratuit** : il interroge l'horloge
système (via `GetCurrent()`) et renvoie la date locale du jour. Le constructeur à trois arguments
`NkDate(year, month, day)` **valide** : il vérifie les plages et la cohérence jour/mois/année (le
fameux 30 février n'existe pas, le 29 février n'existe qu'en année bissextile) et déclenche
`NKENTSEU_ASSERT_MSG` en cas d'erreur. **Il n'y a pas d'exception ni de code d'erreur runtime** : en
debug l'assert vous prévient, en release une date invalide est un **comportement indéfini**. La
copie et l'affectation sont triviales et `noexcept`.

- **IO / réseau / sauvegarde** — date d'un fichier de sauvegarde, d'un snapshot, d'un message :
  toujours construire depuis des valeurs **déjà validées** (issues d'un parseur sûr) pour éviter
  l'assert sur des données externes corrompues.
- **Outils / éditeur** — un champ de saisie de date doit **pré-valider** avant de construire un
  `NkDate`, sinon une frappe utilisateur erronée fait sauter l'assert.
- **Gameplay / IA** — date d'événement de calendrier in-game (saison, anniversaire d'un PNJ) :
  construite à partir de constantes connues, donc sûre.

### `GetYear` / `GetMonth` / `GetDay` — lecture

Trois accesseurs `inline const noexcept nodiscard`, en `O(1)`, qui renvoient les champs bruts. Rien
de plus à dire : triviaux, jamais coûteux, sûrs partout (rendu d'une UI d'horloge, sérialisation
champ par champ, comparaison manuelle).

### `SetYear` / `SetMonth` / `SetDay` — mutation re-validée

Ces mutateurs **ne sont pas `noexcept`** : après modification du champ, ils re-valident la date
**entière** (via `Validate()`). C'est ce qui les rend subtils : changer une seule composante peut
rendre la date globalement incohérente. L'exemple canonique — partir d'un `29/02/2024` (valide) et
appeler `SetYear(2023)` — déclenche un assert, car 2023 n'est pas bissextile. Idiome sûr quand on
change plusieurs champs : ajuster d'abord le **jour** vers une valeur basse, puis le mois, puis
l'année, ou simplement reconstruire un `NkDate` complet d'un coup.

- **Outils / éditeur** — un sélecteur de date (spinners année/mois/jour) doit gérer ce repli :
  réduire le jour avant de changer le mois pour ne pas asserter en passant de mars (31 j) à février
  (28 j).

### `GetCurrent` — aujourd'hui, thread-safe

`static NkDate GetCurrent()` (non `noexcept`) renvoie la date système **locale**. Elle est
**thread-safe** car elle utilise la variante réentrante de la plateforme (`localtime_r` sur
POSIX, `localtime_s` sur Windows) plutôt que le `localtime` global non réentrant. C'est ce que le
ctor par défaut appelle.

- **IO / réseau** — horodater une sauvegarde, un log, un message sortant.
- **Gameplay** — récompense quotidienne, expiration d'un bonus, « connecté aujourd'hui ? » en
  comparant `GetCurrent()` à une date stockée.
- **Threading** — comme elle est réentrante, plusieurs systèmes (logger, sauvegarde auto,
  télémétrie) peuvent l'appeler concurremment sans verrou.

### `DaysInMonth` / `IsLeapYear` — l'arithmétique du calendrier

`static int32 DaysInMonth(year, month)` (non `noexcept`, `O(1)`) renvoie 28 à 31 jours en tenant
compte des bissextiles pour février. `static bool IsLeapYear(year) noexcept` (`O(1)`) applique la
**règle grégorienne exacte** : `(y%4==0 && y%100!=0) || (y%400==0)` — d'où 2000 bissextile mais
1900 non. Ces deux statics sont les briques de toute logique de date sûre.

- **Outils / éditeur** — peupler un calendrier (combien de cases ce mois-ci ?) sans coder en dur.
- **Gameplay** — boucler sur les jours d'un mois (cycle de saison, agenda d'événements) en bornant
  par `DaysInMonth`.
- **IO** — valider une date externe **avant** de construire un `NkDate` (éviter l'assert) : tester
  `day <= DaysInMonth(year, month)`.

### `ToString` / `GetMonthName` — formatage

`ToString() const nodiscard` renvoie l'**ISO 8601** `"YYYY-MM-DD"`, avec zéros de remplissage (donc
triable lexicographiquement). `GetMonthName() const nodiscard` renvoie le nom du mois **en
français** (« Janvier », « Février », … « Juillet », …). **Les deux allouent une `NkString`** : à
éviter dans une boucle chaude, à privilégier pour l'affichage et la persistance.

- **UI / 2D** — afficher une date lisible (« 14 juin 2026 » en composant `GetDay()` +
  `GetMonthName()` + `GetYear()`).
- **IO / sérialisation** — l'ISO `ToString()` est le format de stockage idéal (stable, triable,
  reparsable).
- **Outils / éditeur** — l'aspect FR de `GetMonthName()` est un **piège pour une UI multilingue** :
  pour d'autres langues, formatez vous-même à partir de `GetMonth()`.

### Comparateurs `NkDate` et `friend ToString`

Les six comparateurs (`== != < <= > >=`) sont `const noexcept nodiscard` en `O(1)` et suivent
l'**ordre chronologique** (année, puis mois, puis jour). `NkDate` est donc **totalement ordonnée** :
on peut trier des dates, les utiliser comme clés, tester un intervalle (`d >= debut && d <= fin`).
La fonction amie `ToString(const NkDate&)`, définie inline, délègue à `d.ToString()` et est trouvée
par **ADL** dans `nkentseu` (utile pour un code générique qui appelle `ToString(x)` sans préfixe).

- **Gameplay / IA** — « la licence/quête est-elle expirée ? » via `today > expiration`.
- **Outils / éditeur** — trier une liste de fichiers ou d'événements par date.
- **IO** — filtrer des entrées dans une fenêtre temporelle.

### `NkTimeZone::NkKind` — les trois sortes de fuseau

Enum imbriqué sur `uint8` : `NK_LOCAL = 0` (fuseau système, DST géré par l'OS), `NK_UTC = 1` (UTC
fixe, décalage 0, jamais de DST), `NK_FIXED_OFFSET = 2` (décalage fixe en secondes, jamais de DST).
La sorte conditionne **tout le comportement** des conversions : seul `NK_LOCAL` voit son décalage
varier avec la date (heure d'été), les deux autres sont constants. Lisez-la avec `GetKind()` pour
brancher votre logique (par exemple, ne pas afficher d'indicateur « heure d'été » pour un fuseau
UTC).

### Fabriques `GetLocal` / `GetUtc` / `FromName`

Ce sont les **seuls** moyens de construire un `NkTimeZone` (le constructeur est privé). `GetLocal()`
et `GetUtc()` sont `noexcept nodiscard` et triviaux. `FromName(ianaName) noexcept nodiscard` parse
un nom ou un décalage : il reconnaît `"Local"`, `"UTC"`, `"GMT"`, `"Z"`, `"Etc/UTC"`, et les formes
de décalage `"UTC+HH"`, `"UTC+HH:MM"`, `"GMT-HH"`, etc. **Le piège majeur** : il n'embarque pas la
base IANA — un nom inconnu (y compris un vrai nom comme `"Europe/Paris"`) **retombe silencieusement
sur le fuseau local**, sans signaler d'échec.

- **IO / réseau** — sérialiser/désérialiser un fuseau : préférez une forme **fiable** (`"UTC"`,
  `"UTC+HH:MM"`) plutôt qu'un nom IANA, qui sera mal interprété.
- **Outils / éditeur** — si l'utilisateur choisit un fuseau, **vérifiez** `GetKind()`/`GetName()`
  après `FromName` pour détecter un repli non voulu vers local.
- **Threading** — une fois obtenu, le fuseau est immuable : stockez-le une fois et partagez-le.

### Conversions de `NkTime` : `ToLocal` / `ToUtc` (avec `refDate`)

`ToLocal(NkTime utcTime, NkDate refDate) const noexcept` convertit un instant **UTC → local** ;
`ToUtc(NkTime localTime, NkDate refDate) const noexcept` fait l'inverse (soustrait le décalage). Le
paramètre `refDate` est **central** : le décalage d'un fuseau local dépend de la date (heure d'été),
et le fournir explicitement évite l'effet de bord d'un `GetCurrent()` interne — la conversion
devient **reproductible** et **pure**. Pour `NK_UTC`/`NK_FIXED_OFFSET`, le décalage est constant et
`refDate` n'a pas d'incidence, mais le fournir reste obligatoire (signature commune).

- **IO / réseau** — un serveur stocke et échange tout en **UTC** ; on convertit en local seulement
  au moment de l'affichage côté client (avec la date de l'événement comme `refDate`).
- **Gameplay** — un événement « live » planifié en UTC, affiché à l'heure locale du joueur.
- **UI** — horloge/agenda : convertir l'heure stockée au fuseau de l'utilisateur, avec le bon
  `refDate` pour que l'été/hiver soit juste.

### Conversions de `NkDate` : `ToLocal` / `ToUtc` (jour seul)

Surcharges prenant une `NkDate` seule (`const noexcept`) : elles ne portent **que** le changement de
**jour** potentiel. Si le décalage du fuseau franchit minuit, la date peut avancer ou reculer d'un
jour ; sinon elle est inchangée. Elles **ne transportent aucune heure** — pour un instant complet,
combinez avec la conversion de `NkTime` ci-dessus.

- **IO** — normaliser la date d'un horodatage à l'UTC pour le stockage, ou à local pour
  l'affichage, en sachant qu'elle peut « basculer » d'un jour.
- **Outils** — un agenda local doit tenir compte de ce décalage de jour quand il regroupe des
  événements UTC par journée.

### Informations : `GetName` / `GetKind` / `GetFixedOffsetSeconds`

`GetName() const noexcept` renvoie une **référence** vers le nom interne (« Local », « UTC », ou le
nom fourni) — **aucune copie**, mais la référence n'est valide que tant que l'instance vit.
`GetKind()` renvoie la sorte (`NkKind`). `GetFixedOffsetSeconds()` renvoie le décalage fixe en
secondes (négatif possible ; **0 pour `NK_LOCAL` et `NK_UTC`** — l'offset local réel s'obtient plutôt
par `GetUtcOffset(date)`).

- **UI / outils** — afficher le fuseau choisi (`GetName()`), avec la précaution de la durée de vie
  de la référence (copiez dans une `NkString` si vous la conservez).
- **IO** — sérialiser un fuseau fixe par son `GetFixedOffsetSeconds()`.

### `GetUtcOffset` / `IsDaylightSavingTime` — l'état du fuseau pour une date

`GetUtcOffset(date) const noexcept` renvoie le décalage UTC **pour une date donnée**, sous forme de
`NkTimeSpan` : il **varie** selon le DST pour `NK_LOCAL`, et est **constant** pour
`NK_UTC`/`NK_FIXED_OFFSET`. `IsDaylightSavingTime(date) const noexcept` indique si l'heure d'été est
active ce jour-là : **toujours `false`** pour UTC et offset fixe, et pour `NK_LOCAL` la décision est
**déléguée à l'OS** (`tm_isdst`).

- **UI** — afficher « UTC+2 (heure d'été) » en combinant `GetUtcOffset()` et
  `IsDaylightSavingTime()` pour la date courante.
- **IO / réseau** — calculer le décalage correct à appliquer à un horodatage selon sa date.
- **Outils / éditeur** — vérifier qu'une planification ne tombe pas dans le « trou » d'un changement
  d'heure.

### Comparateurs `NkTimeZone`

`operator==` / `operator!=` sont `const noexcept nodiscard` (`O(1)` plus une comparaison de
`NkString` pour le nom). **Égalité = même kind ET même name ET même offset** : deux fuseaux de même
décalage mais de noms différents sont considérés **distincts**. Utile pour détecter qu'un fuseau a
changé (préférences utilisateur, comparaison avant/après `FromName`).

### Idiomes et pièges (récapitulatif)

- `NkDate(...)` et les setters **assertent** (debug) / sont **UB** (release) sur une date invalide :
  validez les données externes avec `DaysInMonth`/`IsLeapYear` avant de construire.
- `ToString()`/`GetMonthName()` **allouent** une `NkString` ; `GetMonthName()` est **FR uniquement**.
- `NkTimeZone` se construit **uniquement** via les fabriques ; il est **immuable et thread-safe**.
- Fournissez toujours un `refDate` correct aux conversions de `NkTime` (DST exact).
- `FromName` **retombe en silence sur local** pour un nom IANA inconnu — vérifiez le résultat.
- Aucun `NkDateTime` : combinez **manuellement** `NkDate` (jour) + `NkTime` (heure).
- `GetName()` rend une **référence** au membre interne (durée de vie = celle de l'instance).

---

### Exemple récapitulatif

```cpp
#include "NKTime/NkDate.h"
#include "NKTime/NkTimeZone.h"
using namespace nkentseu;

// --- NkDate : valider, formater, comparer ---
NkDate today;                         // aujourd'hui (local)
NkDate launch{ 2026, 3, 12 };         // date validée à la construction

if (today > launch)                   // ordre chronologique
    /* l'événement est passé */;

// Valider une donnée externe AVANT de construire (éviter l'assert) :
int32 y = 2024, m = 2, d = 29;
if (m >= 1 && m <= 12 && d <= NkDate::DaysInMonth(y, m)) {
    NkDate safe{ y, m, d };           // sûr : 2024 est bissextile
    NkString iso  = safe.ToString();      // "2024-02-29"
    NkString mois = safe.GetMonthName();  // "Février" (FR)
}

// --- NkTimeZone : immuable, partageable, conversions UTC <-> local ---
NkTimeZone local = NkTimeZone::GetLocal();   // DST géré par l'OS
NkTimeZone utc   = NkTimeZone::GetUtc();
NkTimeZone fixed = NkTimeZone::FromName("UTC+02:00");

NkDate refDate{ 2026, 7, 1 };                 // pour un DST exact
NkTimeSpan off = local.GetUtcOffset(refDate); // décalage ce jour-là
bool ete       = local.IsDaylightSavingTime(refDate);

// Conversion d'un jour UTC -> local (peut basculer à minuit) :
NkDate localDay = local.ToLocal(refDate);
```

---

[← Index NKTime](README.md) · [Récap NKTime](../NKTime.md) · [Couche System](../README.md)
