# Environnement, logging et configuration matérielle

> Couche **Foundation** · NKPlatform · Lire et écrire les **variables d'environnement** de façon
> portable (`NkEnv`), tracer ce qui se passe **sans dépendre de NKLogger** (`NkFoundationLog`), et
> interroger **la plateforme et le matériel** sur lesquels on tourne (`NkPlatformConfig`).

Un moteur doit, très tôt — avant même que les couches hautes ne soient debout —, répondre à trois
questions très terre-à-terre. **Où sont mes données ?** (le chemin d'un fichier de config tient
souvent dans une variable d'environnement). **Qu'est-ce qui vient de se passer ?** (un message de
trace, mais NKLogger n'existe pas encore à ce niveau de la pile). **Sur quoi est-ce que je tourne ?**
(Windows ou Linux ? AVX2 disponible ? combien de cœurs ? petit ou gros boutiste ?). Ces trois
besoins arrivent **au plus bas** de la couche Foundation, là où l'on ne peut s'appuyer ni sur la STL
ni sur les modules supérieurs. NKPlatform y répond avec trois outils volontairement minimalistes,
chacun autonome.

La règle qui les unit : **zéro dépendance vers le haut, zéro STL**. `NkEnv` apporte ses propres
conteneurs (une chaîne, un vecteur, une map) parce qu'il ne peut pas attendre que NKContainers soit
là ; `NkFoundationLog` n'est qu'une poignée de macros et un `printf` ; `NkPlatformConfig` n'est que
de la détection résolue à la compilation plus deux singletons runtime. On les utilise pour
**amorcer** le moteur, pas pour le piloter une fois qu'il tourne.

- **Namespaces** : `nkentseu::env` · `nkentseu::platform` (et `nkentseu::platform::detail`, interne)
- **Headers** : `#include "NKPlatform/NkEnv.h"` · `#include "NKPlatform/NkFoundationLog.h"` ·
  `#include "NKPlatform/NkPlatformConfig.h"`

---

## Lire et écrire l'environnement : `NkEnv`

Les variables d'environnement sont le plus vieux canal de configuration qui soit : `PATH`,
`HOME`/`USERPROFILE`, `VK_ICD_FILENAMES`, une `NKENTSEU_ASSET_DIR` maison… Le problème, c'est que
l'API native diffère par OS (`getenv`/`setenv` POSIX contre `GetEnvironmentVariable` Windows), et que
manipuler des chaînes au plus bas de Foundation **sans STL** impose ses propres conteneurs. `NkEnv`
règle les deux d'un coup : une API portable (`NkGet`, `NkSet`, `NkUnset`, `NkExists`) **et** le strict
nécessaire pour transporter du texte (`NkEnvString`, `NkEnvVector`, `NkEnvUMap`).

On lit une variable avec `NkGet`, qui prend le nom **et** une chaîne de sortie qu'il remplit (la
valeur de retour est la même chose, par commodité). On écrit avec `NkSet` (avec un drapeau
`overwrite` pour ne pas écraser une valeur déjà posée), on efface avec `NkUnset`, on teste avec
`NkExists`. Tout est en `noexcept` : ces appels ne lèvent rien, ils signalent l'échec par un booléen
ou une chaîne vide.

```cpp
using namespace nkentseu::env;

NkEnvString value;
NkGet(NkEnvString("NKENTSEU_ASSET_DIR"), value);   // value rempli, ou vidé si absente
if (value.Empty()) {
    NkSet(NkEnvString("NKENTSEU_ASSET_DIR"), NkEnvString("./assets"), /*overwrite=*/false);
}
```

Le piège à graver dès maintenant : **les constructeurs `const char*` et `char` de `NkEnvString` sont
`explicit`**. Il n'y a donc **aucune** conversion implicite depuis un littéral — on écrit
toujours `NkEnvString("PATH")`, jamais `"PATH"` tout court en argument. Ce n'est pas une coquetterie :
c'est ce qui évite des allocations cachées et rend les appels lisibles.

Pour les chemins, deux helpers savent qu'une variable de type `PATH` est une **liste** séparée par un
caractère qui dépend de l'OS. `NkGetPathSeparator()` renvoie `';'` sous Windows et `':'` ailleurs ;
`NkPrependToPath` et `NkAppendToPath` ajoutent un répertoire en tête ou en queue (et **créent** la
variable si elle n'existe pas). Notez qu'ils ne sont **pas** `noexcept`, contrairement au reste : ils
construisent des chaînes et peuvent donc échouer sur allocation.

```cpp
NkPrependToPath(NkEnvString("C:/MyEngine/bin"));                 // devant PATH
NkAppendToPath(NkEnvString("/opt/vk/icd"),
               NkEnvString("VK_ICD_FILENAMES"));                  // autre variable
```

Ce n'est **pas** un magasin de configuration persistant : `NkSet` ne survit **pas** à la fin du
processus (en particulier sous Windows), il ne touche que l'environnement du process courant et de
ses enfants. Pour un snapshot complet à un instant T, `NkGetAll()` renvoie une `NkEnvUMap` de tout
l'environnement — potentiellement coûteux, et **figé** (il ne suit pas vos `NkSet` ultérieurs).

> **En résumé.** `NkGet` (out-param) / `NkSet` (avec `overwrite`) / `NkUnset` / `NkExists` =
> l'environnement portable, tout en `noexcept`. `NkPrependToPath` / `NkAppendToPath` gèrent les
> variables-listes via `NkGetPathSeparator()` (non `noexcept`). Toujours construire les noms
> **explicitement** (`NkEnvString("…")`). Rien ne persiste après le process.

---

## Les conteneurs de transport : `NkEnvString`, `NkEnvVector`, `NkEnvUMap`

Comme `NkEnv` vit **sous** NKContainers, il ne peut pas emprunter `NkVector` ou `NkString` — il
embarque donc trois conteneurs autonomes, taillés pour le seul besoin de l'environnement. Ce ne sont
**pas** des remplaçants de NKContainers : ils sont délibérément minces, **non thread-safe**, et leurs
accès indexés sont **sans vérification de bornes** (comportement indéfini hors limites). On les
utilise pour porter quelques chaînes, pas pour bâtir une structure de données de gameplay.

`NkEnvString` est une chaîne dynamique : copie/déplacement, concaténation par `operator+`, comparaison
(`==`, `!=`, et `<` lexicographique — mais **pas** `>`/`<=`/`>=`). Son `CStr()` ne renvoie **jamais**
`nullptr` (chaîne vide → `""`), ce qui dispense des gardes paranoïaques quand on la passe à une API C.

`NkEnvVector<T>` est un tableau dynamique à croissance par doublement (`PushBack`, `PopBack`,
`Reserve`, `At`, `operator[]`, itérateurs C `Begin`/`End`). `At` et `operator[]` **ne vérifient
rien** ; `Begin()` peut être `nullptr` quand le vecteur est vide ; et `Reserve` échoue **en silence**
si l'allocation rate (retour anticipé, aucun signal).

`NkEnvUMap<K, V>` est une map non ordonnée à recherche **linéaire** (`O(n)`) — parfaite pour les
petits ensembles, à fuir pour les gros. `Set` insère ou met à jour, `Find` renvoie un **pointeur**
vers la valeur (ou `nullptr`), `Remove` supprime, `Data()` donne le tableau interne en lecture seule.
Le piège central : **le pointeur rendu par `Find` est invalidé par toute modification** de la map
(un `Set` ou `Remove` ultérieur peut le rendre fou).

> **En résumé.** Trois conteneurs minces, **non thread-safe**, accès **non vérifiés**, pensés pour
> transporter quelques chaînes au plus bas de Foundation — pas pour remplacer NKContainers.
> `NkEnvString::CStr()` n'est jamais `nullptr` ; `NkEnvUMap::Find` rend un pointeur **invalidé par
> toute modif** ; `NkEnvVector::Reserve` échoue silencieusement.

---

## Tracer sans NKLogger : `NkFoundationLog`

Au moment où NKPlatform s'initialise, NKLogger — avec ses sept sinks et son mode asynchrone — n'est
pas encore disponible. Il faut pourtant pouvoir **dire ce qui se passe** : une détection CPU qui
échoue, une variable manquante, un chemin introuvable. `NkFoundationLog` est ce filet de sécurité :
quelques macros au-dessus d'un `fprintf(stderr, …)` (ou de `logcat` sur Android), rien de plus. Ce
n'est **pas** un système de log à part entière — c'est le minimum vital pour amorcer.

Le cœur du design est le **filtrage à la compilation**. Cinq niveaux numériques
(`NK_FOUNDATION_LOG_LEVEL_ERROR`=1, `WARN`=2, `INFO`=3, `DEBUG`=4, `TRACE`=5, plus `NONE`=0) sont
comparés à une constante `NK_FOUNDATION_LOG_LEVEL` qui vaut, par défaut, `DEBUG` en build debug et
`WARN` sinon. Les macros au-dessus du niveau actif **ne sont même pas compilées** : leurs arguments
ne sont **jamais évalués** — zéro surcoût runtime. Pour pousser le curseur, on redéfinit la constante
**avant** l'include :

```cpp
#define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_TRACE   // AVANT l'include
#include "NKPlatform/NkFoundationLog.h"

NK_FOUNDATION_LOG_INFO("Backend GPU choisi : %s", apiName);
NK_FOUNDATION_LOG_ERROR("Variable %s introuvable", "VK_ICD_FILENAMES");
```

À côté des macros `printf`-style, une famille `…_VALUE` logge une paire `label=valeur` en formatant la
valeur automatiquement (chaînes, booléens, entiers, flottants, pointeurs — et vos propres types via un
point d'extension décrit plus bas).

```cpp
NK_FOUNDATION_LOG_DEBUG_VALUE("processorCount", caps.logicalProcessorCount);  // "processorCount=16"
NK_FOUNDATION_LOG_TRACE_VALUE(nullptr, hasAVX2);                              // label nul → "true"
```

Deux macros échappent au filtrage par niveau : `NK_FOUNDATION_PRINT(fmt, …)` émet **toujours** (au
niveau `"INF"`) — à réserver aux messages qui doivent sortir quoi qu'il arrive ; et
`NK_FOUNDATION_SPRINT(buffer, size, fmt, …)` n'est **pas** un log du tout, juste un `snprintf`
sécurisé qui renvoie le nombre de caractères.

Enfin, on peut **rediriger** toute la sortie vers son propre puits avec `NkFoundationSetLogSink` : un
simple pointeur de fonction `void(const char* level, const char* file, int line, const char* message)`
où `level` est la chaîne trois lettres (`"ERR"`, `"WRN"`, `"INF"`, `"DBG"`, `"TRC"`). Passer
`nullptr` restaure le backend par défaut ; `NkFoundationGetLogSink` rend le sink courant. C'est
exactement le crochet par lequel, plus tard, on **branche NKLogger** sur ces logs de bas niveau.

> **En résumé.** Logging d'amorçage, filtré **à la compilation** (`NK_FOUNDATION_LOG_LEVEL`, à
> redéfinir **avant** l'include — les logs filtrés n'évaluent même pas leurs args). Familles
> `LOG_<niveau>` (message) et `LOG_<niveau>_VALUE` (`label=valeur`). `PRINT` ignore le filtre,
> `SPRINT` est un `snprintf`. `NkFoundationSetLogSink` redirige tout (et permettra de brancher
> NKLogger). Messages tronqués au-delà de 1024 octets (256 pour les valeurs), sans erreur.

---

## Interroger la plateforme et le matériel : `NkPlatformConfig`

La dernière brique répond à « sur quoi est-ce que je tourne ? », et elle le fait en **deux temps**.
Ce qui est connu **à la compilation** (l'OS, l'architecture, le compilateur, le boutisme, le mode
debug/release) vit dans `PlatformConfig` ; ce qui ne se sait qu'**au lancement** (mémoire physique,
nombre de cœurs, présence de SSE/AVX/NEON, résolution d'écran) vit dans `PlatformCapabilities`. On
n'instancie ni l'une ni l'autre à la main : deux singletons, `GetPlatformConfig()` et
`GetPlatformCapabilities()`, les calculent **une seule fois** (thread-safe en C++11+) et les mettent
en cache.

```cpp
using namespace nkentseu::platform;

const PlatformConfig&       cfg  = GetPlatformConfig();
const PlatformCapabilities& caps = GetPlatformCapabilities();

if (caps.hasAVX2)            renderer.UseSimdPath();
if (!cfg.isLittleEndian)     net.SwapByteOrder();
threadPool.Resize(caps.logicalProcessorCount);
```

Pour les décisions purement compile-time, pas besoin de passer par le singleton : des fonctions
`NKENTSEU_FORCE_INLINE` (`GetPlatformName`, `GetArchName`, `GetCompilerName`, `Is64Bit`,
`IsLittleEndian`) se résolvent **à la compilation** et peuvent même alimenter des `if constexpr`. Et
pour tout ce qui touche aux **chemins** et à l'**alignement**, NKPlatform expose un jeu de macros
(`NKENTSEU_PATH_SEPARATOR`, `NKENTSEU_MAX_PATH`, `NKENTSEU_DYNAMIC_LIB_EXT`,
`NKENTSEU_SIMD_ALIGNMENT`…) qui font le bon choix par OS sans un seul `#ifdef` dans votre code.

Une mise en garde sur ces macros : **seule `NKENTSEU_MAX_PATH` possède un repli** (4096) sur une
plateforme inconnue ; les autres (séparateur, extensions de lib…) restent **indéfinies** hors d'une
branche OS connue. De même, `NKENTSEU_DEBUG_BUILD` et `NKENTSEU_RELEASE_BUILD` sont **exclusives**,
mais `NKENTSEU_OPTIMIZED_BUILD` est **orthogonal** : un release compilé en `-O0` peut être
non-optimisé. Ne confondez pas « release » et « optimisé ».

> **En résumé.** `PlatformConfig` (compile-time) via `GetPlatformConfig()`, `PlatformCapabilities`
> (runtime, hardware) via `GetPlatformCapabilities()` — deux singletons calculés une fois.
> `GetPlatformName/ArchName/CompilerName`, `Is64Bit`, `IsLittleEndian` se résolvent à la compilation.
> Les macros chemins/alignement choisissent par OS ; seule `NKENTSEU_MAX_PATH` a un repli.
> `DEBUG_BUILD`/`RELEASE_BUILD` exclusives, `OPTIMIZED_BUILD` indépendant.

---

## Aperçu de l'API

La liste de **tous** les éléments publics (macros incluses). Chacun est détaillé dans la « Référence
complète ».

### `NkEnv.h` — environnement (`nkentseu::env`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Base | `using NkSize = unsigned int;` | Alias d'entier non signé pour tailles/indices. |
| Base | `NkMove(t)` | `std::move` maison (cast rvalue, sans `<utility>`). |
| Chaîne | `NkEnvString` (ctors `explicit` `const char*`/`char`, copie, move) | Chaîne dynamique sans STL. |
| Chaîne | `CStr`, `Size`, `Empty`, `Clear` | Vue C (jamais `nullptr`), longueur, vide ?, vidage. |
| Chaîne | `operator==`, `!=`, `<` · `operator+` (libre) | Égalité, ordre lexical (pas `>`/`<=`/`>=`), concaténation. |
| Paire | `NkEnvPair<K,V>` (`.key`, `.value`, ctors copie/move) | Paire clé-valeur. |
| Vecteur | `NkEnvVector<T>` : `PushBack`, `PopBack`, `Clear`, `Reserve` | Tableau dynamique (doublement). |
| Vecteur | `At`, `operator[]`, `Size`, `Capacity`, `Empty`, `Begin`/`End` | Accès **non vérifié**, capacité, itérateurs C. |
| Map | `NkEnvUMap<K,V>` : `Set` (copie/move), `Find`, `Remove`, `Clear`, `Size`, `Data` | Map non ordonnée, recherche **`O(n)`**. |
| Env | `NkGet(name, result)` | Lit une variable (out-param). |
| Env | `NkSet(name, value, overwrite=true)` | Écrit (ne persiste pas au-delà du process). |
| Env | `NkUnset(name)`, `NkExists(name)` | Supprime / teste l'existence. |
| Env | `NkGetAll()` | Snapshot complet (figé, coûteux). |
| Chemin | `NkGetPathSeparator()` | `';'` (Windows) / `':'` (POSIX). |
| Chemin | `NkPrependToPath(dir, var=PATH)`, `NkAppendToPath(dir, var=PATH)` | Ajout en tête/queue (non `noexcept`). |

### `NkFoundationLog.h` — logging (`nkentseu::platform`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Niveaux | `NK_FOUNDATION_LOG_LEVEL_NONE/ERROR/WARN/INFO/DEBUG/TRACE` (0..5) | Constantes de sévérité. |
| Niveau actif | `NK_FOUNDATION_LOG_LEVEL` | Seuil compile-time (`DEBUG` en debug, `WARN` sinon). |
| Détection | `NK_FOUNDATION_HAS_ANDROID_LOG` | 1/0 — backend `logcat` si disponible. |
| Sink | `NkFoundationLogSink` (typedef pointeur de fonction) | Callback de redirection. |
| Sink | `gNkFoundationLogSink` | Variable globale du sink (ne pas modifier directement). |
| Sink | `NkFoundationSetLogSink(sink)`, `NkFoundationGetLogSink()` | Définir / lire le sink. |
| Log | `NK_FOUNDATION_LOG_ERROR/WARN/INFO/DEBUG/TRACE(fmt, …)` | Log `printf`-style, filtré par niveau. |
| Log | `NK_FOUNDATION_LOG_ERROR/WARN/INFO/DEBUG/TRACE_VALUE(label, value)` | Log `label=valeur`, formatage auto. |
| Log | `NK_FOUNDATION_PRINT(fmt, …)` | Toujours émis (niveau `"INF"`). |
| Util | `NK_FOUNDATION_SPRINT(buf, size, fmt, …)` | `snprintf` sécurisé (pas un log). |
| Extension | `NKFoundationToString(value, out, outSize)` (ADL) | Formatage d'un type utilisateur. |

### `NkPlatformConfig.h` — config & capacités (`nkentseu::platform`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Compile-time | `PlatformConfig` (struct + ctor) | OS/arch/compilo, debug/release, 64-bit, boutisme, capacités. |
| Compile-time | `GetPlatformConfig()` | Singleton `PlatformConfig`. |
| Runtime | `PlatformCapabilities` (struct + ctor) | Mémoire, cœurs, écran, SSE/AVX/NEON. |
| Runtime | `GetPlatformCapabilities()` | Singleton `PlatformCapabilities`. |
| Inline | `GetPlatformName`, `GetArchName`, `GetCompilerName`, `Is64Bit`, `IsLittleEndian` | Détection résolue à la compilation. |
| Chemins | `NKENTSEU_PATH_SEPARATOR(_STR)`, `NKENTSEU_LINE_ENDING`, `NKENTSEU_MAX_PATH` | Séparateur, fin de ligne, longueur max (repli 4096). |
| Chemins | `NKENTSEU_DYNAMIC_LIB_EXT`, `NKENTSEU_STATIC_LIB_EXT`, `NKENTSEU_EXECUTABLE_EXT` | Extensions de fichiers par OS. |
| Fonctionnalités | `NKENTSEU_HAS_UNICODE/THREADING/FILESYSTEM/NETWORK` | Détection de support (1/0). |
| Build | `NKENTSEU_DEBUG_BUILD`, `NKENTSEU_RELEASE_BUILD`, `NKENTSEU_OPTIMIZED_BUILD` | Mode de build (exclusifs / orthogonal). |
| Alignement | `NKENTSEU_CACHE_ALIGNED`, `NKENTSEU_SIMD_ALIGNMENT`, `NKENTSEU_SIMD_ALIGNED` | Alignement cache / SIMD (16 ou 32). |

---

## Référence complète

Chaque élément est repris ici en détail. Les éléments triviaux sont décrits brièvement ; ce qui porte
un vrai enjeu — boutisme réseau, dispatch SIMD, redirection de sink, persistance de l'environnement —
est détaillé **à fond**, avec ses usages dans les différents domaines.

### `NkSize` et `NkMove` — la mécanique de base

`NkSize` est simplement `unsigned int`, l'alias que tout `NkEnv` emploie pour les tailles et les
indices. `NkMove(t)` est l'équivalent maison de `std::move` : un cast vers une référence rvalue, écrit
ici parce que Foundation ne veut pas tirer `<utility>`. On le croise dans les constructeurs/affectations
par déplacement des conteneurs `NkEnv` — c'est lui qui transforme une copie de chaîne en transfert
`O(1)`. Trivial mais structurant : sans lui, pas de move sémantique sans STL.

### `NkEnvString` — la chaîne de l'environnement

C'est une chaîne dynamique réduite à l'essentiel : construction depuis un `const char*` ou un `char`
(les deux **`explicit`**), copie, déplacement, concaténation via l'`operator+` libre. La comparaison se
limite à `==`, `!=` et `<` (ordre lexicographique) — il n'y a **pas** de `>`, `<=`, `>=`, donc on ne
peut pas trier naïvement dans les deux sens. `CStr()` ne renvoie **jamais** `nullptr` (vide → `""`),
`Size()` donne la longueur hors terminateur, `Empty()` teste, `Clear()` vide et libère.

- **IO / chemins** : transporter le contenu d'une variable `PATH`, d'un dossier d'assets, d'un nom de
  fichier de config avant que NKFileSystem n'entre en jeu — `CStr()` jamais nul simplifie le passage
  aux API C.
- **Réseau** : porter une URL de serveur, un port, un token lus dans l'environnement au démarrage.
- **Gameplay / outils** : un flag de debug (`NKENTSEU_DEBUG=1`) lu une fois et comparé par `==`.

Le réflexe à acquérir : **toujours** construire explicitement (`NkEnvString("PATH")`), y compris pour
chaque argument passé à `NkGet`/`NkSet`/`NkExists`. Ce n'est **pas** un `std::string` interchangeable
avec un littéral — l'`explicit` est délibéré.

### `NkEnvPair`, `NkEnvVector`, `NkEnvUMap` — les conteneurs de transport

`NkEnvPair<K, V>` est une paire publique (`.key`, `.value`) avec construction par copie ou déplacement
(via `NkMove`) — le bloc de base de la map.

`NkEnvVector<T>` est un tableau dynamique à croissance par **doublement** (capacité initiale 4). Il
offre `PushBack`/`PopBack`, `Reserve`, `Clear`, l'accès par `At`/`operator[]`, les compteurs
`Size`/`Capacity`/`Empty`, et des itérateurs **style C** `Begin`/`End`. Deux pièges à intérioriser :
`At` et `operator[]` **ne vérifient pas** les bornes (UB hors limites), et `Reserve` **échoue en
silence** si `malloc` rend `nullptr` (retour anticipé, aucun signal). `Begin()` vaut `nullptr` quand le
vecteur est vide — à tester avant de déréférencer.

`NkEnvUMap<K, V>` stocke ses paires dans un `NkEnvVector` interne et cherche **linéairement** (`O(n)`),
ce qui convient aux **petits** ensembles (quelques dizaines d'entrées max) — exactement la taille d'un
environnement, mais à proscrire pour une structure de gameplay. `Set` insère ou met à jour, `Remove`
supprime (par compaction), `Find` rend un **pointeur** vers la valeur (`nullptr` si absente), `Data()`
le tableau interne en lecture seule. Le piège majeur : **`Find` rend un pointeur invalidé par toute
modification** ultérieure de la map — on l'utilise immédiatement, on ne le conserve pas.

- **Bootstrap moteur** : `NkGetAll()` rend une `NkEnvUMap` ; on y `Find` deux ou trois variables clés
  puis on jette le snapshot.
- **Outils / éditeur** : agréger une poignée de réglages d'environnement pour les afficher.
- **Réseau / IO** : construire la liste de répertoires d'un `PATH` dans un `NkEnvVector` avant de la
  recoller avec `NkGetPathSeparator()`.

Ces trois conteneurs sont **non thread-safe** : un seul thread les touche à la fois.

### `NkGet`, `NkSet`, `NkUnset`, `NkExists`, `NkGetAll` — l'API environnement

`NkGet(name, result)` lit une variable et **remplit le paramètre de sortie** `result` (vidé si la
variable est absente) ; sa valeur de retour est la même chaîne, par commodité. `NkSet(name, value,
overwrite=true)` écrit — avec `overwrite=false`, il **respecte** une valeur déjà posée (idéal pour des
défauts qu'on ne veut pas écraser). `NkUnset` supprime (et renvoie `false` si rien à supprimer),
`NkExists` teste. `NkGetAll()` rend le snapshot complet (coûteux, figé). Toutes sont `noexcept`.

- **Configuration de backend** : lire `VK_ICD_FILENAMES`, `MESA_GL_VERSION_OVERRIDE`, une
  `NKENTSEU_RHI=vulkan` maison pour choisir le backend GPU au lancement.
- **Threading** : honorer un `NKENTSEU_THREADS=8` pour dimensionner le pool sans recompiler.
- **Audio / assets** : pointer un dossier de samples ou de banques via une variable d'environnement.
- **Gameplay / debug** : activer un mode triche/profil avec un simple `NkExists("NKENTSEU_GODMODE")`.

Le point capital : **`NkSet` ne persiste pas** au-delà du process (en particulier Windows). Ce n'est
**pas** un registre de configuration — c'est l'environnement volatil du process courant, hérité par ses
enfants seulement.

### `NkGetPathSeparator`, `NkPrependToPath`, `NkAppendToPath` — les variables-listes

`NkGetPathSeparator()` rend `';'` sous Windows, `':'` ailleurs — le séparateur d'une variable de type
`PATH`. `NkPrependToPath(dir, var=PATH)` et `NkAppendToPath(dir, var=PATH)` insèrent un répertoire en
tête ou en queue de la variable nommée (par défaut `PATH`), **créant** la variable si elle n'existe
pas encore. Contrairement au reste de l'API env, ils ne sont **pas** `noexcept` : ils manipulent des
chaînes et peuvent échouer sur allocation, d'où le booléen de retour.

- **GPU / IO** : ajouter un dossier de DLL (drivers, ICD Vulkan, plugins) au `PATH` au démarrage pour
  que le chargeur dynamique les trouve.
- **Outils** : enrichir une variable de recherche maison (chemins de shaders, de polices) sans toucher
  l'environnement global de la machine.

La nuance tête/queue importe : préfixer (`Prepend`) donne **priorité** à votre répertoire, suffixer
(`Append`) en fait un **dernier recours**.

### Niveaux de log et `NK_FOUNDATION_LOG_LEVEL` — le filtrage compile-time

Les six constantes (`NONE`=0, `ERROR`=1, `WARN`=2, `INFO`=3, `DEBUG`=4, `TRACE`=5) hiérarchisent la
sévérité. `NK_FOUNDATION_LOG_LEVEL` est le **seuil** : par défaut `DEBUG` si un symbole `_DEBUG`/
`DEBUG`/`NKENTSEU_DEBUG` est défini, `WARN` sinon. C'est un filtrage **à la compilation** : une macro
de niveau supérieur au seuil n'émet aucun code, et ses arguments ne sont **jamais évalués** — on peut
donc y mettre un calcul coûteux sans crainte en release. L'idiome obligé : redéfinir
`NK_FOUNDATION_LOG_LEVEL` **avant** l'`#include` pour ouvrir ou fermer le robinet (typiquement `TRACE`
pour déboguer un sous-système, `ERROR` pour un build de prod silencieux).

### `NkFoundationLogSink`, `NkFoundationSetLogSink`, `NkFoundationGetLogSink` — la redirection

`NkFoundationLogSink` est un pointeur de fonction `void(const char* level, const char* file, int line,
const char* message)` ; `level` est la chaîne trois lettres (`"ERR"`, `"WRN"`, `"INF"`, `"DBG"`,
`"TRC"`). `NkFoundationSetLogSink(sink)` l'installe (`nullptr` restaure le backend par défaut —
`stderr` ou `logcat`), `NkFoundationGetLogSink()` le relit. La variable globale `gNkFoundationLogSink`
porte l'état, mais on passe **toujours** par les setters/getters plutôt que de la toucher directement.

- **Bootstrap → NKLogger** : une fois NKLogger debout, on installe un sink qui **réinjecte** ces logs
  de bas niveau dans le système complet — la sortie d'amorçage et la sortie applicative se rejoignent.
- **Tests** : capturer les messages dans un buffer pour les asserter, sans polluer la console.
- **Outils / éditeur** : router les traces vers un panneau de console intégré plutôt que `stderr`.

L'assignation du pointeur est de fait atomique, mais pour un usage avancé (changer le sink pendant que
d'autres threads loggent) il faut synchroniser soi-même.

### Macros de log : `LOG_<niveau>`, `LOG_<niveau>_VALUE`, `PRINT`, `SPRINT`

Les familles `NK_FOUNDATION_LOG_ERROR/WARN/INFO/DEBUG/TRACE(fmt, …)` sont des logs `printf`-style,
gardés par le seuil compile-time, capturant automatiquement `__FILE__` et `__LINE__`. Les variantes
`…_VALUE(label, value)` formatent une paire `label=valeur` en s'appuyant sur le formatage automatique
des types (built-in directement, types utilisateur via le point d'extension ci-dessous) ; un `label`
`nullptr` ne logge que la valeur. Toutes sont enveloppées en `do { … } while (0)`.

`NK_FOUNDATION_PRINT(fmt, …)` **ignore** le seuil et émet toujours (au niveau `"INF"`) — à réserver aux
messages incontournables, sous peine de bruit. `NK_FOUNDATION_SPRINT(buf, size, fmt, …)` n'est **pas**
un log : c'est un `snprintf` sécurisé qui rend le nombre de caractères écrits, pratique pour préparer
une chaîne avant de la logger.

- **Rendu / GPU** : tracer le backend choisi, l'échec d'une création de pipeline, l'absence d'une
  extension — `LOG_ERROR` en prod, `LOG_TRACE` pour suivre chaque étape d'init.
- **Threading** : `LOG_DEBUG_VALUE("workerCount", n)` au démarrage du pool.
- **Physique / animation** : un `LOG_TRACE` conditionnel pour suivre une valeur qui dérive — compilé
  hors en release, donc gratuit.

Le revers du buffer fixe : un message dépassant 1024 octets (256 pour une valeur) est **tronqué
silencieusement**.

### Point d'extension `NKFoundationToString` — formater ses propres types

Pour qu'un type maison apparaisse proprement dans un `…_VALUE`, on définit
`int NKFoundationToString(const T& value, char* out, size_t outSize)` **dans le namespace de `T`** : il
écrit la représentation dans `out` et renvoie le nombre de caractères (`-1` sur erreur). La sélection
se fait par ADL/SFINAE, sans que l'appelant ait à rien préciser.

- **Math / rendu** : afficher un `NkVec3` ou une `NkColor` en `"(x, y, z)"` dans une trace.
- **ECS / gameplay** : formater un identifiant d'entité ou un état de FSM lisiblement.

Sans surcharge, le fallback rend `"<unformattable>"` — un signal clair qu'il manque un
`NKFoundationToString`.

### `PlatformConfig` et `GetPlatformConfig` — ce qui est connu à la compilation

`PlatformConfig` agrège les faits **compile-time** : `platformName`, `archName`, `compilerName` +
`compilerVersion`, les drapeaux `isDebugBuild`/`isReleaseBuild`, `is64Bit`, `isLittleEndian`, les
supports `hasUnicode`/`hasThreading`/`hasFilesystem`/`hasNetwork`, et deux constantes pratiques :
`maxPathLength` (260 Windows, 4096 Linux/Android, 1024 macOS/iOS) et `cacheLineSize` (typiquement 64).
`GetPlatformConfig()` rend un singleton calculé **une seule fois** (statique local, thread-safe
C++11+).

- **Réseau / sérialisation** : `isLittleEndian` décide s'il faut **permuter les octets** vers
  l'ordre réseau (gros-boutiste) avant d'émettre — un bug classique de portabilité qu'on évite ici en
  testant ce drapeau plutôt qu'en supposant l'architecture.
- **IO** : `maxPathLength` dimensionne les buffers de chemins sans coder « 260 » en dur.
- **Build** : `isDebugBuild` active asserts verbeux, overlays de debug, validation GPU.

### `PlatformCapabilities` et `GetPlatformCapabilities` — ce que seul le runtime sait

`PlatformCapabilities` rassemble ce qui se découvre **au lancement** : `totalPhysicalMemory`,
`availablePhysicalMemory`, `pageSize` (typiquement 4096), `processorCount` et
`logicalProcessorCount`, l'affichage (`hasDisplay`, `primaryScreenWidth`/`Height`), et surtout les
jeux d'instructions SIMD : `hasSSE`, `hasSSE2`, `hasAVX`, `hasAVX2`, `hasNEON`.
`GetPlatformCapabilities()` rend un singleton dont la détection hardware ne court qu'**une fois** puis
reste en cache (thread-safe C++11+).

- **SIMD dispatch** : c'est **le** point de bascule. On teste `hasAVX2` au démarrage pour brancher un
  chemin de calcul vectorisé (transformation de sommets, mixage audio, mise à jour de particules) et
  retomber sur SSE2 ou scalaire sinon — la détection runtime permet **un seul binaire** qui exploite
  le meilleur jeu disponible.
- **Threading** : `logicalProcessorCount` dimensionne le ThreadPool ; `processorCount` (cœurs
  physiques) guide l'affinité.
- **Mémoire** : `totalPhysicalMemory` ajuste la taille des caches d'assets, le budget de textures, la
  stratégie de streaming ; `pageSize` aligne les allocations virtuelles.
- **Audio (NEON)** : sur ARM, `hasNEON` choisit le mixeur vectorisé.
- **UI / 2D** : `primaryScreenWidth/Height` cadrent la fenêtre par défaut ; `hasDisplay` distingue un
  contexte headless (serveur, CI) d'un poste graphique.

### Fonctions inline et macros de plateforme

`GetPlatformName`, `GetArchName`, `GetCompilerName`, `Is64Bit`, `IsLittleEndian` sont des helpers
`NKENTSEU_FORCE_INLINE` résolus **à la compilation** — utilisables là où un `if constexpr` ou une
constante est attendu, sans le coût d'un singleton.

Côté macros, le bloc **chemins** (`NKENTSEU_PATH_SEPARATOR`/`_STR`, `NKENTSEU_LINE_ENDING`,
`NKENTSEU_MAX_PATH`, `NKENTSEU_DYNAMIC_LIB_EXT`, `NKENTSEU_STATIC_LIB_EXT`, `NKENTSEU_EXECUTABLE_EXT`)
fait le bon choix par OS : c'est ce qui permet d'écrire du code de fichiers/chargement de DLL portable
**sans `#ifdef`**. Attention : **seule `NKENTSEU_MAX_PATH` a un repli** (4096) hors plateforme connue ;
les autres restent indéfinies. Le bloc **fonctionnalités** (`NKENTSEU_HAS_UNICODE/THREADING/
FILESYSTEM/NETWORK`) vaut 1/0 selon le support détecté. Le bloc **build** distingue
`NKENTSEU_DEBUG_BUILD` et `NKENTSEU_RELEASE_BUILD` (**exclusifs**) de `NKENTSEU_OPTIMIZED_BUILD`
(**orthogonal** : un release `-O0` n'est pas optimisé). Enfin le bloc **alignement** —
`NKENTSEU_CACHE_ALIGNED` (contre le faux partage entre threads), `NKENTSEU_SIMD_ALIGNMENT` (32 si
AVX/AVX2, 16 si SSE/NEON, 16 par défaut) et `NKENTSEU_SIMD_ALIGNED` — décore les structures destinées
aux boucles chaudes SIMD ou partagées entre cœurs.

- **GPU / IO** : `NKENTSEU_DYNAMIC_LIB_EXT` pour charger le bon nom de plugin (`.dll`/`.so`/`.dylib`/
  `.wasm`) sans brancher l'OS à la main.
- **Threading** : `NKENTSEU_CACHE_ALIGNED` sur les compteurs par-thread pour éviter le false-sharing.
- **Rendu / physique** : `NKENTSEU_SIMD_ALIGNED` sur les tableaux de `NkVec4`/matrices traités en SIMD.

---

### Exemple récapitulatif

```cpp
#define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_TRACE   // avant l'include
#include "NKPlatform/NkEnv.h"
#include "NKPlatform/NkFoundationLog.h"
#include "NKPlatform/NkPlatformConfig.h"

using namespace nkentseu::env;
using namespace nkentseu::platform;

// 1) Environnement : choisir le backend GPU, avec un défaut non écrasant.
NkEnvString rhi;
NkGet(NkEnvString("NKENTSEU_RHI"), rhi);
if (rhi.Empty()) NkSet(NkEnvString("NKENTSEU_RHI"), NkEnvString("vulkan"), /*overwrite=*/false);
NkPrependToPath(NkEnvString("C:/MyEngine/plugins"));            // DLL trouvables au load

// 2) Plateforme & matériel : un seul binaire, le meilleur chemin de calcul.
const PlatformConfig&       cfg  = GetPlatformConfig();
const PlatformCapabilities& caps = GetPlatformCapabilities();

NK_FOUNDATION_LOG_INFO("Plateforme : %s / %s / %s", cfg.platformName, cfg.archName, cfg.compilerName);
NK_FOUNDATION_LOG_DEBUG_VALUE("logicalCores", caps.logicalProcessorCount);

if (caps.hasAVX2)        NK_FOUNDATION_LOG_INFO("Chemin SIMD : AVX2");
else if (caps.hasSSE2)   NK_FOUNDATION_LOG_INFO("Chemin SIMD : SSE2");

if (!cfg.isLittleEndian) NK_FOUNDATION_LOG_WARN("Gros-boutiste : permutation réseau requise");

// 3) Rediriger plus tard toute la trace de bas niveau vers NKLogger.
NkFoundationSetLogSink(&MyNKLoggerBridge);   // nullptr restaurerait stderr/logcat
```

---

[← Index NKPlatform](README.md) · [Récap NKPlatform](../NKPlatform.md) · [Détection plateforme →](../NKPlatform.md)
