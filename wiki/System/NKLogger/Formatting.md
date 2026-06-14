# Le formatage des messages de log

> Couche **System** · NKLogger · Transformer un `NkLogMessage` brut (niveau, horodatage,
> thread, fichier, message…) en une **ligne de texte lisible**, selon un *pattern* configurable
> façon spdlog : `NkLoggerFormatter`, son token compilé `NkPatternToken`, et l'alias `FormatterPtr`.

Un logger ne produit pas directement du texte : il produit un **`NkLogMessage`**, un petit paquet de
données structurées (le niveau, l'instant exact, l'identifiant du thread, le fichier et la ligne
d'où vient l'appel, le nom du logger, et enfin le message lui-même). Reste à décider *à quoi
ressemble la ligne finale* : faut-il l'horodatage en premier ? avec les millisecondes ? le niveau en
toutes lettres ou en code court ? des couleurs dans la console ? le tout en JSON pour une stack
d'observabilité ? C'est exactement le rôle du **formateur**. On lui donne une fois pour toutes un
**pattern** — une chaîne où des séquences `%…` désignent chaque champ — et il sait dès lors
transformer n'importe quel `NkLogMessage` en la ligne voulue.

Le point de design central tient en une phrase : **le pattern est analysé une seule fois, à la
configuration, en une liste de tokens ; ensuite chaque message ne fait que parcourir ces tokens.**
On ne re-parse pas la chaîne `"[%Y-%m-%d…]"` à chaque ligne de log — ce serait du gâchis en boucle
chaude. On la compile en un `NkVector<NkPatternToken>`, et `Format()` se contente de balayer ce
vecteur. C'est ce qui rend le formatage `O(T)` par message (T = nombre de tokens) au lieu de `O(n)`
sur la longueur du pattern.

Ce n'est **pas** un système de templates à la `printf` où l'on passe des arguments : les valeurs ne
viennent pas de l'appelant, elles sont **extraites du `NkLogMessage`** au moment du formatage. Le
seul token qui porte sa propre donnée est le texte **littéral** (les crochets, les espaces, les
flèches `->`) ; tous les autres vont puiser dans le message.

- **Namespace** : `nkentseu`
- **Header** : `#include "NKLogger/NkLoggerFormatter.h"`

---

## Le pattern : une grammaire de `%…`

Un pattern est une phrase à trous. Le texte ordinaire est recopié tel quel ; les **séquences `%X`**
sont des trous que le formateur remplit à partir du message. `%Y-%m-%d` donne la date, `%H:%M:%S` l'heure,
`%e` les millisecondes, `%L` le niveau en code court (`INF`, `ERR`), `%n` le nom du logger, `%t`
l'identifiant du thread, `%v` enfin le message lui-même. Pour écrire un vrai signe pourcent, on le
double : `%%`.

```cpp
NkLoggerFormatter fmt("[%H:%M:%S.%e] [%L] %v");
// donnera par exemple :  [14:32:07.418] [INF] Scène chargée en 42 ms
```

Deux séquences sont à part : `%^` et `%$`. Elles ne produisent **aucun texte** ; elles **marquent une
zone à colorer**. Entre `%^` et `%$`, si l'on demande les couleurs, le formateur insère les séquences
ANSI correspondant au niveau (rouge pour une erreur, jaune pour un warning…) puis le reset. Sans
couleurs demandées, ces marqueurs sont simplement ignorés. C'est ainsi qu'un **même** pattern sert à
la fois pour le fichier (sans couleur) et pour la console (avec) :

```cpp
NkLoggerFormatter fmt("[%^%L%$] %v");   // [%^…%$] = le niveau sera coloré en console
NkString plain   = fmt.Format(msg);          // [ERR] Texture introuvable
NkString colored = fmt.Format(msg, true);    // \033[31m[ERR]\033[0m Texture introuvable
```

Une propriété rassurante : les tokens sont **sensibles à la casse** (`%L` n'est pas `%l`) et un token
**inconnu est traité comme du littéral**. Si vous tapez `%Z` par erreur, vous obtenez littéralement
`%Z` dans la sortie au lieu d'un plantage — robustesse plutôt que rigueur.

> **En résumé.** Un pattern mêle du texte littéral et des séquences `%X` que le formateur remplit
> depuis le `NkLogMessage`. `%^`/`%$` délimitent une zone colorable (ANSI), sans rien afficher
> eux-mêmes. Casse significative, token inconnu = littéral. `%%` pour un `%` réel.

---

## Le token compilé : `NkPatternToken`

Quand vous appelez `SetPattern(...)`, le formateur ne garde pas la chaîne pour la relire plus tard :
il la **découpe** en une suite de `NkPatternToken`. Un token est une structure minuscule à deux
champs : un `type` (l'énumération qui dit « ceci est l'année », « ceci est le message », « ceci est
du littéral », « ceci ouvre une zone colorée »…) et une `value` — une `NkString` qui ne sert **que**
pour le type `NK_LITERAL` (elle contient le morceau de texte à recopier). Pour tous les autres types,
la valeur est extraite du message au moment voulu, donc `value` reste vide.

C'est cette **pré-analyse** qui fait toute la performance : `Format()` ne fait que parcourir le
vecteur de tokens et, pour chacun, soit recopier son littéral, soit aller chercher le champ
correspondant dans le `NkLogMessage`. Aucune ré-analyse de syntaxe, aucune recherche de `%` à chaud.

> **En résumé.** `NkPatternToken` = le pattern *compilé*, élément par élément : un `type`
> (énumération) + une `value` n'ayant de sens que pour `NK_LITERAL`. Parsé une fois dans
> `SetPattern`, balayé à chaque `Format` — d'où le coût `O(T)` par message.

---

## Patterns prêts à l'emploi et extension

Vous n'êtes pas obligé d'écrire vos patterns à la main : le formateur expose une **bibliothèque de
constantes** couvrant les cas usuels — `NK_DEFAULT_PATTERN` (le complet, par défaut),
`NK_SIMPLE_PATTERN` (juste `%v`), `NK_DETAILED_PATTERN` (avec fichier/ligne/fonction),
`NK_COLOR_PATTERN`, `NK_JSON_PATTERN`, `NK_ISO8601_PATTERN`… On les passe au constructeur ou à
`SetPattern`.

```cpp
NkLoggerFormatter dev(NkLoggerFormatter::NK_DETAILED_PATTERN);  // verbeux, pour déboguer
NkLoggerFormatter prod(NkLoggerFormatter::NK_JSON_PATTERN);     // structuré, pour ingestion
```

Pour les cas où le vocabulaire des `%X` ne suffit pas (formater un champ d'une façon très
particulière), il existe un **point d'extension** protégé, `FormatToken`, appelé pour chaque token.
Attention toutefois : tel que le header le déclare, il n'est **pas marqué `virtual`** — la surcharge
polymorphe par une classe dérivée n'est donc pas garantie par l'API écrite. À considérer comme un
crochet d'implémentation plus que comme une vraie extension publique.

> **En résumé.** Des constantes (`NK_DEFAULT_PATTERN`, `NK_DETAILED_PATTERN`, `NK_JSON_PATTERN`…)
> couvrent les besoins courants. `FormatToken` est un crochet protégé par token — mais non `virtual`
> dans la déclaration, donc à ne pas traiter comme un point d'extension polymorphe fiable.

---

## Aperçu de l'API

Tous les éléments publics de `NkLoggerFormatter.h`. Namespace `nkentseu`, header
`NKLogger/NkLoggerFormatter.h`. Aucun `noexcept`, `nodiscard` ni `constexpr` sur ce header.

### `struct NkPatternToken` — un token compilé

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `enum class Type : uint8` | Nature du token (voir table des valeurs ci-dessous). |
| Données | `Type type` | Type du token, pilote le comportement de formatage. |
| Données | `NkString value` | Texte associé — **utilisé uniquement** pour `NK_LITERAL`. |

Valeurs de `NkPatternToken::Type` (les nombres sont arbitraires, seul le `switch` compte) :

| Valeur | Séquence | Champ |
|--------|----------|-------|
| `NK_LITERAL = 0` | — | Texte littéral recopié tel quel |
| `NK_YEAR = 1` | `%Y` | Année (4 chiffres) |
| `NK_MONTH = 2` | `%m` | Mois |
| `NK_DAY = 3` | `%d` | Jour |
| `NK_HOUR = 4` | `%H` | Heure (24 h) |
| `NK_MINUTE = 5` | `%M` | Minute |
| `NK_SECOND = 6` | `%S` | Seconde |
| `NK_MILLIS = 7` | `%e` | Millisecondes (3 ch.) |
| `NK_MICROS = 8` | `%f` | Microsecondes (cf. piège `%f`/`%F`) |
| `NK_LEVEL = 9` | `%l` | Niveau, texte complet (`info`, `error`) |
| `NK_LEVEL_SHORT = 10` | `%L` | Niveau, code court (`INF`, `ERR`) |
| `NK_THREAD_ID = 11` | `%t` | Identifiant du thread |
| `NK_THREAD_NAME = 12` | `%T` | Nom du thread (ou ID si vide) |
| `NK_SOURCE_FILE = 13` | `%s` | Fichier source (sans chemin) |
| `NK_SOURCE_LINE = 14` | `%#` | Ligne source |
| `NK_FUNCTION = 15` | `%f` / `%F` | Fonction émettrice (cf. piège) |
| `NK_MESSAGE = 16` | `%v` | Message |
| `NK_LOGGER_NAME = 17` | `%n` | Nom hiérarchique du logger |
| `NK_PERCENT = 18` | `%%` | `%` littéral |
| `NK_COLOR_START = 19` | `%^` | Début de zone colorée |
| `NK_COLOR_END = 20` | `%$` | Fin de zone colorée |

### `class NkLoggerFormatter` — le formateur

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkLoggerFormatter()` | Formateur au pattern par défaut (`NK_DEFAULT_PATTERN`), tokens prêts. |
| Construction | `explicit NkLoggerFormatter(const NkString& pattern)` | Pattern personnalisé, parsé immédiatement `[O(n)]`. |
| Construction | `~NkLoggerFormatter() = default` | Libération RAII des `NkVector`/`NkString`. |
| Pattern | `void SetPattern(const NkString&)` | Remplace et reparse le pattern `[O(n)]` — **pas thread-safe**, à faire avant tout log. |
| Pattern | `const NkString& GetPattern() const` | Pattern courant, **par référence** (sans copie). |
| Formatage | `NkString Format(const NkLogMessage&)` | Formate **sans** couleurs `[O(T)]`. **Non `const`**. |
| Formatage | `NkString Format(const NkLogMessage&, bool useColors)` | Formate, couleurs ANSI si `useColors` `[O(T)]`. **Non `const`**. |
| Extension | `void FormatToken(const NkPatternToken&, const NkLogMessage&, bool, NkString&)` | (protégé) Formate un token, l'appende à `result`. **Non `virtual`** dans la déclaration. |
| Patterns | `static const char* NK_DEFAULT_PATTERN` | `"[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v"` |
| Patterns | `static const char* NK_SIMPLE_PATTERN` | `"%v"` — le message seul. |
| Patterns | `static const char* NK_DETAILED_PATTERN` | Complet + `[%s:%# in %f]` (fichier/ligne/fonction). |
| Patterns | `static const char* NK_NKENTSEU_PATTERN` | Style maison, niveau coloré + `[%s:%# in %F]`. |
| Patterns | `static const char* NK_COLOR_PATTERN` | Complet avec niveau coloré (`%^%L%$`). |
| Patterns | `static const char* NK_JSON_PATTERN` | Champs échappés pour JSON valide (valeur exacte non exposée dans le header). |
| Patterns | `static const char* NK_SHORT_PATTERN` | `"%H:%M:%S %L %v"` — heure + niveau + message. |
| Patterns | `static const char* NK_ISO8601_PATTERN` | `"%Y-%m-%dT%H:%M:%S.%fZ [%L] %v"` (horodatage ISO 8601). |
| Patterns | `static const char* NK_SYSLOG_PATTERN` | `"%b %d %H:%M:%S %h %n[%t]: %v"` (cf. piège : `%b`/`%h` hors enum). |

### Alias

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Pointeur | `using FormatterPtr = memory::NkUniquePtr<NkLoggerFormatter>` | Possession unique RAII (move exclusif), allocateur projet. |

---

## Référence complète

Chaque élément repris en détail. Les éléments triviaux (accesseurs, constantes) sont décrits
brièvement ; les opérations qui portent la conception du module — le cycle parse/format, le choix de
pattern, les couleurs — le sont à fond, avec leurs usages dans les domaines du moteur.

### `NkPatternToken` — le pattern compilé, à fond

Un token est un **agrégat simple** : aucun constructeur ni méthode, juste deux champs publics. C'est
volontaire — c'est une donnée plate produite par le parseur et consommée par le formateur, pas un
objet à comportement.

- `type` est un `NkPatternToken::Type` (énumération sur `uint8`, donc un octet) qui dit *quoi*
  émettre. Le commentaire du header est explicite : « les valeurs sont arbitraires, seul le switch
  importe » — ne dépendez jamais des nombres `0..20`, dépendez des noms.
- `value` (une `NkString`) ne porte une donnée que pour `NK_LITERAL` : c'est le morceau de texte fixe
  (les crochets, les espaces, les `->`). Pour tous les autres types, le champ correspondant est lu
  dans le `NkLogMessage` à l'instant du formatage, et `value` reste vide.

L'intérêt par domaine est toujours le même : **payer l'analyse une seule fois**. Que vous logguiez
le rendu, l'ECS, l'audio ou le réseau, le pattern de votre sortie ne change pas d'une frame à
l'autre — le compiler en tokens transforme un coût de parsing récurrent en un simple parcours de
vecteur. C'est la même logique que de précompiler une expression régulière ou un shader plutôt que
de le faire à chaud.

### Construction et destruction de `NkLoggerFormatter`

Trois formes. Le **constructeur par défaut** part de `NK_DEFAULT_PATTERN`
(`"[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v"`) et laisse l'objet immédiatement prêt à formater —
c'est le formateur « qui marche tout de suite » pour démarrer un projet. Le **constructeur à
pattern** (`explicit`, donc pas de conversion implicite depuis une `NkString`) parse le pattern
fourni sur-le-champ, coût `O(n)` sur la longueur du pattern, une fois pour toutes. Le **destructeur**
est celui par défaut : il libère en RAII le `NkVector` de tokens et les `NkString` associées.

Le header ne déclare **ni copie ni déplacement** explicites : le comportement est celui généré par
le compilateur. En pratique, on ne copie pas un formateur — on le partage via l'alias `FormatterPtr`
(voir plus bas), ce qui colle au pattern « un formateur par sink, créé au démarrage ».

- **Outils / éditeur** : instanciez un formateur détaillé (`NK_DETAILED_PATTERN`) au lancement d'un
  outil de build ou de l'éditeur Nogee, pour tracer fichier+ligne+fonction pendant la mise au point.
- **Runtime de jeu** : un formateur court (`NK_SHORT_PATTERN`) suffit en release, où l'on veut des
  lignes denses et lisibles à l'écran.

### `SetPattern` / `GetPattern` — changer et relire le pattern

`SetPattern` remplace le pattern courant : il **invalide** les anciens tokens et **reparse** la
nouvelle chaîne, coût `O(n)`. Deux précautions tirées du header :

- l'opération **n'est pas thread-safe** — il faut la faire **avant** que le moindre thread ne
  commence à logguer, typiquement à l'initialisation. Reparse à chaud pendant que d'autres threads
  appellent `Format()` = course de données.
- c'est une opération de **chemin froid** : changer de pattern à chaque frame annulerait tout le
  bénéfice de la pré-compilation. On le règle une fois, on n'y touche plus.

`GetPattern` renvoie le pattern courant **par référence constante** (pas de copie d'une `NkString`),
pratique pour l'afficher dans un panneau de configuration d'éditeur ou pour sérialiser les réglages
de log d'un projet.

- **Threading** : configurez le pattern dans le thread principal, avant de lancer le pool de
  workers ; chacun pourra ensuite appeler `Format()` en lecture concurrente sans verrou.
- **Outils / éditeur** : un menu « format de la console » qui appelle `SetPattern` *à l'arrêt du
  logging*, puis `GetPattern` pour cocher l'option active.

### `Format` — du message à la ligne

Le cœur du module, en deux surcharges. `Format(message)` produit la ligne **sans couleurs** (les
`%^`/`%$` sont ignorés) ; `Format(message, useColors)` insère les séquences ANSI dans les zones
`%^…%$` quand `useColors` vaut `true`. La première est l'équivalent de la seconde avec `false`.

Le coût est `O(T)`, T = nombre de tokens du pattern (déjà parsés), proportionnel à la taille de la
ligne produite. Deux points à intégrer :

- **`Format` n'est pas `const`** dans la déclaration : ne l'appelez pas sur une
  `const NkLoggerFormatter&`. Le header *documente* une « lecture concurrente thread-safe » (les
  tokens sont immuables après parsing), mais cette garantie est documentaire — elle n'est pas
  exprimée par le qualificateur, donc à confirmer côté implémentation avant de s'y fier sous charge.
- chaque appel **retourne une `NkString` par valeur**, donc une allocation NKMemory. En logging
  haute fréquence, le bon réflexe est de réutiliser le **même** formateur (parsing amorti) et, si
  l'implémentation le permet, d'écrire dans un buffer `NkString` externe réutilisé.

Les domaines où le formatage tombe sur la boucle chaude méritent une attention particulière :

- **Rendu / GPU** : ne formatez pas une ligne par draw call. Logguez par frame ou par évènement
  (changement de pipeline, hot-reload de shader), et préférez un pattern court.
- **Threading** : avec un pattern contenant `%t`/`%T`, chaque ligne identifie son thread — précieux
  pour démêler les logs entrelacés d'un job system.
- **IO / réseau** : un pattern ISO 8601 (`NK_ISO8601_PATTERN`) horodate proprement les paquets émis
  ou reçus, lisible par les outils d'analyse de trace.
- **Audio** : sur le thread audio temps réel, évitez carrément d'allouer — formatez sur un autre
  thread ou différez ; `Format` alloue une `NkString`, donc pas dans le callback temps réel.
- **Gameplay / IA, physique, animation** : pour ces systèmes au pas de simulation, le formatage par
  évènement (transition d'état d'une FSM, collision notable, fin d'animation) est largement sous le
  seuil de coût ; un pattern lisible avec niveau coloré (`NK_COLOR_PATTERN`) aide au débogage en
  direct.

### Les patterns prédéfinis — quoi choisir

Toutes ces constantes sont des `static const char*`. Elles couvrent l'éventail des besoins :

- `NK_DEFAULT_PATTERN` — `"[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v"`. Le complet équilibré :
  horodatage à la milliseconde, niveau court, nom du logger, thread, message. Le bon choix par
  défaut pour un fichier de log de session.
- `NK_SIMPLE_PATTERN` — `"%v"`. Le message **seul**, sans décor. Pour rediriger une sortie vers un
  autre système qui ajoute lui-même les métadonnées, ou pour des tests.
- `NK_DETAILED_PATTERN` — complet + `[%s:%# in %f]` : ajoute fichier, ligne et fonction. Le pattern
  de **débogage** par excellence (rendu, ECS, outils) quand on veut sauter direct au site de l'appel.
- `NK_NKENTSEU_PATTERN` — variante maison avec niveau **coloré** (`%^%L%$`) et `[%s:%# in %F]`. Le
  format console de référence du moteur pendant le développement.
- `NK_COLOR_PATTERN` — `"[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%t] -> %v"`. Le `NK_DEFAULT` avec le
  niveau colorisé : à donner au sink console (avec `Format(msg, true)`), tandis que le sink fichier
  reçoit la version sans couleur.
- `NK_JSON_PATTERN` — champs **échappés pour produire du JSON valide** (la valeur littérale exacte
  n'est pas exposée dans le header). Destiné à l'**ingestion** par une stack d'observabilité
  (recherche, agrégation) plutôt qu'à la lecture humaine.
- `NK_SHORT_PATTERN` — `"%H:%M:%S %L %v"`. Heure, niveau, message : dense et lisible, idéal pour une
  console de jeu en release ou un overlay à l'écran.
- `NK_ISO8601_PATTERN` — `"%Y-%m-%dT%H:%M:%S.%fZ [%L] %v"`. Horodatage **normalisé** (avec
  microsecondes et `Z`), parfait pour corréler des logs IO/réseau avec d'autres sources de trace.
- `NK_SYSLOG_PATTERN` — `"%b %d %H:%M:%S %h %n[%t]: %v"`. Format inspiré de syslog. **Aspirationnel
  en partie** : `%b` (mois abrégé) et `%h` (hostname) n'ont **pas** de `Type` dans l'enum et seraient
  donc traités comme des littéraux (cf. pièges).

### `FormatToken` — le crochet protégé

Méthode **protégée** appelée pour chaque token rencontré pendant `Format`, qui **appende** sa
contribution à `result` (la ligne en construction) selon `token`, le `message` et `useColors`. Le
header la présente comme « méthode virtuelle potentielle pour surcharge » et les exemples l'overrident
via `override` — **mais le mot-clé `virtual` n'apparaît pas dans la déclaration**. Conséquence
pratique : tant que la déclaration reste non-virtuelle, une surcharge dérivée ne sera **pas** appelée
polymorphiquement par la classe de base. À traiter comme un détail d'implémentation, pas comme un
point d'extension public garanti. Si vous avez besoin d'un champ sur mesure (outils/éditeur), la voie
sûre reste de composer un pattern adéquat plutôt que de dériver.

### `FormatterPtr` — possession et partage

`using FormatterPtr = memory::NkUniquePtr<NkLoggerFormatter>` est le pointeur **unique** RAII du
projet : sémantique de déplacement exclusive (un seul propriétaire à la fois), libération
automatique, mémoire via l'allocateur NKMemory (jamais `new`/`delete`). On crée l'objet par
`memory::MakeUnique<NkLoggerFormatter>(...)`. C'est la forme canonique pour stocker le formateur d'un
sink dans la configuration du logger, le transférer sans copie, et garantir sa destruction propre à
la fin de session — cohérent avec la règle NKMemory du projet.

### Les pièges réels du header

À connaître avant de se fier au seul header :

- **Collision `%f`** : la table Doxygen mappe `%f` → microsecondes **et** `%F` → fonction, alors que
  l'enum nomme `NK_MICROS` (`%f`) **et** `NK_FUNCTION` (commentaire `%f`, mais la table dit `%F`). La
  résolution réelle dépend de `ParsePattern()`, invisible dans ce header. Idem pour `%s`. **Ne pas se
  fier au seul header pour `%f`/`%F`/`%s`** — vérifiez le comportement effectif.
- **Tokens hors enum** : `NK_SYSLOG_PATTERN` utilise `%b` et `%h`, qui n'ont aucun `Type`
  correspondant → traités comme littéraux selon la note « token inconnu = littéral ». Le pattern syslog
  est donc partiellement aspirationnel.
- **`FormatToken` non `virtual`** : la surcharge polymorphe n'est pas garantie par l'API écrite
  (voir ci-dessus).
- **`Format()` non `const`** : ne l'appelez pas sur une référence `const` ; le claim « thread-safe en
  lecture concurrente » est documentaire, non porté par le qualificateur.
- **`NK_JSON_PATTERN`** : sa valeur exacte n'est pas dans le header (seulement un exemple de sortie en
  commentaire).
- **Allocation** : `Format()` renvoie une `NkString` par valeur (alloc NKMemory) → réutiliser le
  formateur en logging haute fréquence, idéalement avec un buffer externe.
- **Aucun `noexcept`, `nodiscard` ni `constexpr`** n'apparaît sur ce header.

---

### Exemple récapitulatif

```cpp
#include "NKLogger/NkLoggerFormatter.h"
using namespace nkentseu;

// 1. Un formateur compilé une fois, au démarrage (chemin froid).
NkLoggerFormatter console(NkLoggerFormatter::NK_COLOR_PATTERN);  // niveau coloré
NkLoggerFormatter file   (NkLoggerFormatter::NK_DETAILED_PATTERN); // fichier:ligne:fonction

// 2. Le même message formaté de deux façons, sans re-parser le pattern.
NkLogMessage msg = /* produit par le logger : niveau, horodatage, thread, message… */;

NkString line   = console.Format(msg, /*useColors=*/true);   // \033[…m[ERR]\033[0m -> …
NkString record = file.Format(msg);                          // sans couleurs, verbeux

// 3. Reconfiguration (à l'arrêt du logging, jamais en boucle chaude).
console.SetPattern(NkLoggerFormatter::NK_SHORT_PATTERN);      // "%H:%M:%S %L %v"
const NkString& current = console.GetPattern();              // relecture par référence

// 4. Possession unique RAII pour stocker le formateur d'un sink.
FormatterPtr owned = memory::MakeUnique<NkLoggerFormatter>(NkLoggerFormatter::NK_ISO8601_PATTERN);
NkString iso = owned->Format(msg);                           // 2026-06-14T14:32:07.418000Z [INF] …
```

---

[← Index NKLogger](README.md) · [Récap NKLogger](../NKLogger.md) · [Couche System](../README.md)
